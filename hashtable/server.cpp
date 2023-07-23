#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <map>
#include <string>
#include <vector>

#include "hashtable.h"

#define get_outer_wrapper_of_hnode(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})

// functions are defined static to limit their scope to this 
// file only to avoid name conflicts (or encaptualtion too)

static const size_t MSG_MAX_LEN = 4096;
static const uint8_t HEADER_LEN = 4;
static std::map<std::string, std::string> map; // later we will update this using hashtable

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

static void msg(const char* msg) {
    fprintf(stderr, "%s\n", msg);
}

/// @brief We are making the fd which is open by socket as non-blocking to make things asycn
/// @param fd : of the socket
static void set_fd_to_non_blocking(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0); // get the flags, initially set the value of flags to 0
    if (errno) {
        die("fcntl error");
        return;
    }
    flags |= O_NONBLOCK; // set this flag 
    errno = 0; 
    (void)fcntl(fd, F_SETFL, flags); // set the flags associated with the fd 
    if (errno) {
        die("fcntl error");
    }
}

enum {
    STATE_REQ = 0,
    STATE_RES = 1, 
    STATE_END = 2,  
};

enum {
    RES_OK = 0,
    RES_ERR = 1, 
    RES_NX = 2,
};

// we are making things async or non-blocking, we may need the container 
// to the hold the results from the defered IO operations 

struct Conn {
    int fd = -1;
    uint32_t state = 0; // setup from the enum 
    // buffer for reading 
    size_t read_buffer_size = 0;
    uint8_t read_buffer[HEADER_LEN + MSG_MAX_LEN]; // fixed size array of integers 
    // buffer for writing 
    size_t write_buffer_size = 0;
    size_t write_buffer_sent = 0;
    uint8_t write_buffer[HEADER_LEN + MSG_MAX_LEN]; // fixed sized
};

// structure for the key and value 
struct Entry {
    struct Hnode node;
    std::string key;
    std::string val;
};


// the datastructure for key spaces 
static struct {
    HashMap db;
} g_data;


/// @brief Whenever a new client join, then this function is called, this is first time connection
/// @param fd_to_conn : Here we will store the connection object mapped by the fd opened for that connection 
/// @param fd : fd of the server, used by accept syscall to accept the connection from client
/// @return success or failure, whether client successfully connect or fails
static int32_t accept_new_connection(std::vector<Conn* >& fd_to_conn, int fd) {
    // accept a new connection 
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);
    int conn_fd = accept(fd, (struct sockaddr*)& client_addr, &socklen);
    if (conn_fd < 0) {
        msg("accept() error");
        return -1;
    }
    // set this new conn_fd to non-blocking
    set_fd_to_non_blocking(conn_fd);
    // create a conn object 
    struct Conn *conn = (struct Conn*) malloc (sizeof(struct Conn));
    if (!conn) {
        close(conn_fd);
        return -1;
    }

    conn->fd = conn_fd;
    conn->state = STATE_REQ;
    conn->read_buffer_size = 0;
    conn->write_buffer_sent = 0;
    conn->write_buffer_size = 0;

    if (fd_to_conn.size() <= (size_t)conn->fd) {
        fd_to_conn.resize(conn->fd + 1);
    }
    fd_to_conn[conn->fd] = conn;
    printf ("Accepted a new connection!\n");
    return 0;
}


static void state_res(Conn *conn);
static void state_req(Conn *conn);

/// @brief used to parse the request, we are using a protocol to send the request
/// @param raw_request : obtained from the client.
/// @param end_pos : The total length of the request.
/// @param parsed_request : Final parsed request which server can understand.
/// @return whether parsable ? success(0) or failure (-1)
static int32_t parse_request(
    const uint8_t *raw_request, 
    size_t end_pos, 
    std::vector<std::string>& parsed_request
) {
    if (end_pos < 4) {
        return -1;
    } 
    uint32_t len_of_request = 0;
    memcpy(&len_of_request, &raw_request[0], 4); // read first 4 bytes, i.e total number of string in request 

    size_t cur_pos = 4; // iterator at which position we have readed from the raw request 

    while (len_of_request--) {
        if (cur_pos + 4 > end_pos) {
            return -1;
        }
        uint32_t size_of_command = 0;
        memcpy(&size_of_command, &raw_request[cur_pos], 4);
        if (cur_pos + 4 + size_of_command > end_pos) {
            return -1; // we are exceeding the final length, and string is still not covered in that length, some issue 
        }
        parsed_request.push_back(std::string((char *)&raw_request[cur_pos + 4], size_of_command));
        cur_pos += 4 + size_of_command;
    }
    if (cur_pos != end_pos) {
        return -1; 
    }
    return 0; // success in parse 
}

static bool is_same(const std::string &word, const char *cmd) {
    return strcasecmp(word.c_str(), cmd) == 0;
}

/// @brief to check if both lhs and rhs are same or different
/// @param lhs type of Hnode
/// @param rhs type of Hnode
/// @return boolean value, true if same, else false 
static bool comparator_function(Hnode *lhs, Hnode *rhs) {
    struct Entry *le = get_outer_wrapper_of_hnode(lhs, struct Entry, node);
    struct Entry *re = get_outer_wrapper_of_hnode(rhs, struct Entry, node);
    return lhs->hcode == rhs->hcode && le->key == re->key;
}

/// @brief 
/// @param data 
/// @param len 
/// @return 
static uint64_t str_hash(const uint8_t *data, size_t len) {
    uint32_t h = 0x811C9DC5; // a random prime ? 
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i]) * 0x01000193;
    }
    return h; // get a unique hash value for a key, so that we can have uniform distribution in hashmap
}


/**
 * @brief get api for the REDIS server, client send the get request with the key
 * Note that we have written a non-generic hashmap and their is no way we can are incorporating
 * the key and value, so what is the way for this ? There is one way, actually we create a 
 * new struct Entry, where first field is the node and thus we first find the node in the 
 * hashmap and once we have that we are asking for the address of the it's outer container
 * Thus we get the access to the wrapper outside of Hnode i.e Entry and now we can have 
 * the val field
 * @param parsed_request : Parsed request which parse_request() returned
 * @param res : response to the client to the get request
 * @param res_len : length of response 
 * @return uint32_t : ENUM type for response (success / failure)
 */
static uint32_t get(std::vector<std::string>& parsed_request, 
    uint8_t *res, 
    uint32_t *res_len
) {

    Entry cur;
    cur.key.swap(parsed_request[1]); // the key which we have passed, get[0] key[1]
    cur.node.hcode = str_hash((uint8_t *)cur.key.data(), cur.key.size());
    printf ("reached\n");
    Hnode *node = hashmap_lookup(&g_data.db, &cur.node, &comparator_function);
    if (!node) {
        return RES_NX;
    }

    const std::string &val = get_outer_wrapper_of_hnode(node, Entry, node)->val;
    assert(val.size() <= MSG_MAX_LEN);
    memcpy(res, val.data(), val.size());
    *res_len = (uint32_t)val.size();
    return RES_OK;
}

/**
 * @brief 
 * 
 * @param parsed_request 
 * @param res 
 * @param res_len 
 * @return uint32_t 
 */
static uint32_t set(std::vector<std::string> &parsed_request, 
    uint8_t *res, 
    uint32_t *res_len 
) {
    (void)res;
    (void)res_len;

    Entry cur; // simply created a structure instance
    cur.key.swap(parsed_request[1]); // set[0], key[1], value[2]
    cur.node.hcode = str_hash((uint8_t *)cur.key.data(), cur.key.size());

    Hnode *node = hashmap_lookup(&g_data.db, &cur.node, &comparator_function);
    if (node) {
        // if this already exists, then update
        get_outer_wrapper_of_hnode(node, Entry, node)->val.swap(parsed_request[2]);
    }
    else {
        // if this is the first entry, then insert
        // Also does this mean the entry is just floating in the memory ? 
        Entry *fresh_entry = new Entry(); // allocated memory on heap
        fresh_entry->node.hcode = cur.node.hcode; // setup the node
        fresh_entry->key.swap(cur.key); // key
        fresh_entry->val.swap(parsed_request[2]); // value 
        hashmap_insert(&g_data.db, &fresh_entry->node);
    }
    return RES_OK;
}

/**
 * @brief delete is used to remove the key from the hashmap, we first remove using hashmap_pop()
 * Note that hashmap is not for handling the garbage cleaning in heap for entry, that is done differently, 
 * neither entry is the owner
 * for the Hnode, instead that is properly handled and cleaned by hashmap
 * @param parsed_request : parsed request by parse_request() function
 * @param res : response code
 * @param res_len : response length
 * @return uint32_t : success of failure 
 */
static uint32_t del(std::vector<std::string> &parsed_request, 
    uint8_t *res, 
    uint32_t *res_len
) {
    (void)res;
    (void)res_len;

    std::string key = parsed_request[1];

    Entry cur;
    cur.key.swap(key);
    cur.node.hcode = str_hash((uint8_t *)cur.key.data(), cur.key.size());
    Hnode *node = hashmap_pop(&g_data.db, &cur.node, &comparator_function);

    if (node) {
        // clean the outer wrapper of Hnode, i.e Entry 
        delete get_outer_wrapper_of_hnode(node, Entry, node);
    }
    return RES_OK;
}

/**
 * @brief 
 * 
 * @param raw_request 
 * @param req_len 
 * @param res_code 
 * @param res 
 * @param res_len 
 * @return int32_t 
 */
static int32_t handle_request(const uint8_t *raw_request, uint32_t req_len, 
        uint32_t* res_code, uint8_t *res, uint32_t *res_len) {
    
    std::vector<std::string> parsed_request;
    
    if (parse_request(raw_request, req_len, parsed_request) != 0) {
        msg ("bad request at during executing handle_request()");
        return -1;
    } 

    if (parsed_request.size() == 2 && is_same(parsed_request.front(), "get")) {
        *res_code = get(parsed_request, res, res_len);
    }
    else if (parsed_request.size() == 2 && is_same(parsed_request.front(), "del")) {
        *res_code = del(parsed_request, res, res_len);
    }
    else if (parsed_request.size() == 3 && is_same(parsed_request.front(), "set")) {
        *res_code = set(parsed_request, res, res_len);
    }
    else {
        *res_code = RES_ERR;
        const char *msg = "Unknown cmd";
        strcpy((char* )res, msg);
        *res_len = strlen(msg);
        return 0;
    }
    return 0;
}

/**
 * @brief 
 * 
 * @param conn 
 * @return true 
 * @return false 
 */
static bool try_one_request(Conn* conn) {
    if (conn->read_buffer_size < 4) {
        // not enough data for this cycle, please try later.
        return false;
    }
    uint32_t len = 0;
    memcpy(&len, &conn->read_buffer[0], 4); // in read_buffer we have the request sent by the client. 
    // too long message 
    if (len > MSG_MAX_LEN) {
        msg("too long message");
        conn->state = STATE_END;
        return false;
    }

    //response for the request 
    uint32_t res_code = 0;
    uint32_t res_len = 0;
    printf ("Calling handle_request\n");
    uint32_t err = handle_request(
        &conn->read_buffer[4], 
        len, 
        &res_code, // to store the response code. 
        &conn->write_buffer[4 + 4], // write response after first four bytes
        &res_len // length of response 
    );

    if (err) {
        conn->state = STATE_END;
        return false;
    }

    res_len += 4; // length of response code and response 
    memcpy(&conn->write_buffer[0], &res_len, 4);
    memcpy(&conn->write_buffer[4], &res_code, 4);
    conn->write_buffer_size = 4 + res_len; // 4 is the header and rest is response code + actual response 

    // remain 
    size_t remain = conn->read_buffer_size - (HEADER_LEN + len);
    if (remain) {
        memmove(conn->read_buffer, &conn->read_buffer[HEADER_LEN + len], remain);
    }
    conn->read_buffer_size = remain;
    // change the state 
    conn->state = STATE_RES;
    state_res(conn); // see if you can flush, call the state_res
    return (conn->state == STATE_REQ);
}

// flush the write buffer to the fd
/**
 * @brief 
 * 
 * @param conn 
 * @return true 
 * @return false 
 */
static bool try_flush_buffer(Conn* conn) {
    ssize_t return_value = 0;
    do {
        size_t remain = conn->write_buffer_size - conn->write_buffer_sent;
        // write the remain data to the fd, at it's buffer from the index(or pointer) write_buffer_sent
        return_value = write(conn->fd, &conn->write_buffer[conn->write_buffer_sent], remain);
    }
    while(return_value < 0 && errno == EINTR); // if asked to return again 
    // EINTR "Interrupted System Call" and indicates that a system call was interrupted 
    // by a signal before it could complete its intended operation.
    if (return_value < 0 && errno == EAGAIN) {
        // got EAGAIN, stop;
        return false;
    }
    if (return_value < 0) {
        msg("write() error");
        conn->state = STATE_END;
        return false;
    }
    conn->write_buffer_sent += (size_t) return_value;
    assert(conn->write_buffer_sent <= conn->write_buffer_size);
    if (conn->write_buffer_sent == conn->write_buffer_size) {
        // response is fully send thus no more in res state 
        conn->state = STATE_REQ; // set the state to open to req state
        conn->write_buffer_sent = 0;
        conn->write_buffer_size = 0;
        return false;
    }
    // if still left with data then we will try again, don't sleep
    return true;
}

static void state_res(Conn* conn) {
    while (try_flush_buffer(conn)) {}
}

// fill the read buffer from the fd, 

/**
 * @brief 
 * 
 * @param conn 
 * @return true 
 * @return false 
 */
static bool try_fill_buffer(Conn* conn) {
    assert(conn->read_buffer_size < sizeof(conn->read_buffer));
    ssize_t return_value = 0;
    do {
        size_t capacity = sizeof(conn->read_buffer) - conn->read_buffer_size; // left capacity in local buffer, to read from the fd
        return_value = read(conn->fd, &conn->read_buffer[conn->read_buffer_size], capacity);
    }while (return_value < 0 && errno == EINTR);

    if (return_value < 0 && errno == EAGAIN) {
        return false; // go to sleep, there is nothing to do
    }
    if (return_value < 0) {
        msg("read() error");
        conn->state = STATE_END;
        return false;
    }
    if (return_value == 0) {
        if (conn->read_buffer_size > 0) {
            msg("unexpected eof"); // there is still to read but we can't as \0 found in between
        }
        else {
            msg ("EOF"); // actual eof
        }
        conn->state = STATE_END;
        return false; // no need to stay awake (i.e while (true));
    }
    conn->read_buffer_size += (size_t)return_value;
    assert(conn->read_buffer_size <= sizeof(conn->read_buffer));
    // EXERCISE: Why there is a loop ? (it was not in the other case ....)
    while (try_one_request(conn)) {} // see if the request can be proceed with 
    return (conn->state == STATE_REQ);
}

static void state_req(Conn* conn) {
    while (try_fill_buffer(conn)) {}
}

// state-machine (see in which state conn object is now)
static void connection_io(Conn* conn) {
    if (conn->state == STATE_REQ) {
        state_req(conn);
    }
    else if (conn->state == STATE_RES) {
        state_res(conn);
    }   
    else {
        assert(0); // not expected. 
    }
}

int main() {
    // AF_INET is for IPv4, and AF_INET6 is for ipv6
    // Sock stream is for TCP
    printf("Server started \n");

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    // this value is used to enable SO_REUSEADDR flag to 1
    int val = 1; 
    // SOL_SOCKET tells at which level this option i.e SO_REUSEADDR is defined 
    // in this case it defined at the level of socket, &val is a pointer to the value
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // bind to the socket 

    struct sockaddr_in addr = {}; // set each value to 0
    addr.sin_family = AF_INET;  // this is an int 
    addr.sin_port = ntohs(3001); // again int 
    addr.sin_addr.s_addr = ntohl(0); // wildcard address 0.0.0.0:3001

    int return_value = bind(fd, (const sockaddr* )& addr, sizeof(addr));
    // if 0, the success, else faliure 
    if (return_value) {
        die("bind()");
    }

    // listen 

    return_value = listen(fd, SOMAXCONN); // SOMAXCONN defines how many connection can stay in queue 
    if (return_value) {
        die("listen()");
    }

    printf("Server started listening on port...\n");

    // a map of all client connection, keyed by fd 

    std::vector<Conn*> fd_to_conn;
    
    // set the listener fd to non-blocking
    set_fd_to_non_blocking(fd);

    // the event loop 

    std::vector<struct pollfd> poll_args; 
    
    while (true) {
        // clear every things whatever you were polling.
        poll_args.clear();
        // put the listener fd at the first position (known position)
        struct pollfd listener_poller_fd = {fd, POLLIN, 0}; // int fd, short events, short revents(event send by kernel and kernel will update this when we report event)
        // POLLIN is an event and it means POLLIN indicates that you want to monitor the file descriptor 
        // for read readiness, i.e., you are interested in reading data from this 
        // file descriptor.
        poll_args.push_back(listener_poller_fd);
        for (Conn* conn : fd_to_conn) {
            if (!conn) {
                continue;
            }
            struct pollfd pfd = {};
            // if we have send req, then we are open for reads i.e POLLIN
            // else we are open for writes, i.e POLLOUT POLLOUT indicates that you want to 
            // monitor the file descriptor for write readiness
            pfd.fd = conn->fd; 
            pfd.events = (conn->state == STATE_REQ) ? POLLIN: POLLOUT; 
            pfd.events = pfd.events | POLLERR; // error conditions with the pollfd 
            poll_args.push_back(pfd);
        }
        // poll for active fds 
        // the timeout arguments doesn't matter here 

        // typedef unsigned long int nfds_t;
        // since poll take array as first element, thus .data() is used thus a pointer can hold this 
        // poll signature: int poll(struct pollfd *fds, nfds_t nfds, int timeout);
        int return_value = poll(poll_args.data(), (nfds_t)poll_args.size(), 1000);
        if (return_value < 0) {
            die("poll");
        }

        // now process all the active connections
        for (size_t i = 1; i < poll_args.size(); ++i) {
            if (poll_args[i].revents) {
                Conn *conn = fd_to_conn[poll_args[i].fd];
                connection_io(conn);
                if (conn->state == STATE_END) {
                    fd_to_conn[conn->fd] = NULL; // set mapping to null
                    (void)close(conn->fd); // close the resource 
                    free(conn); // free the memory allocated by malloc
                }
            }
        }

        // try to accept a new connection if the listening fd is active 
        if (poll_args[0].revents) {
            (void)accept_new_connection(fd_to_conn, fd);
        }
    }


    return 0;
}


