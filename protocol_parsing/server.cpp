#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h> 
#include <unistd.h> 
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

// functions are defined static to limit their scope to this 
// file only to avoid name conflicts (or encaptualtion too)


const size_t k_max_msg = 4096;


static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

static void msg(const char* msg) {
    fprintf(stderr, "%s\n", msg);
}

/// @brief IO Helper function for read the buffer until n bytes.
/// @return the success(zero) of failure(non-zero) of this operation 

static int32_t read_full(int fd, char* buffer, size_t n) {
    while (n > 0) {
        // read syscall just return whatever data is available in the kernel 
        // or block if there is none. It's upto application to handle 
        // insufficient data
        ssize_t return_value = read(fd, buffer, n); // return_value represent how many character we read.
        if (return_value <= 0) {
            return -1; // error, or unexpected EOF
        }

        assert((size_t)return_value <= n);
        n -= (size_t)return_value;
        buffer += return_value; // advance the pointer 
    }
    return 0;
}

static int32_t write_all(int fd, const char* buffer, size_t n) {
    while (n > 0) {
        // syscall write may return success, even when partial data is written 
        // because of buffer overflow. Thus we have while(n > 0), to keep 
        // retrying until we are done with writting all the n bytes from the buffer
        ssize_t return_value = write(fd, buffer, n); // return_value represents how many characters we have write
        if (return_value <= 0) {
            return -1; // error 
        }
        assert((size_t)return_value <= n);
        n -= (size_t)return_value;
        buffer += return_value; // here we are advancing the pointer so that we don't write what we already did
    }
    return 0;
}


static int32_t one_request(int conn_fd) {
    // four byte header 
    char read_buffer[4 + k_max_msg + 1];
    const uint8_t header_size = 4;
    errno = 0;
    int32_t err = read_full(conn_fd, read_buffer, header_size);
    if (err) {
        if (errno == 0) { // if nothing is send...
            msg ("EOF");
        }
        else {
            msg("read() error");
        }
        return err;
    }

    uint32_t client_message_length = 0;
    memcpy(&client_message_length, read_buffer, 4); // assuming little endian 
    if (client_message_length > k_max_msg) {
        msg("too long");
        return -1;
    }

    // request body 

    err = read_full(conn_fd, &read_buffer[4], client_message_length);
    if (err) {
        msg("read() error");
        return err;
    }

    read_buffer[header_size + client_message_length] = '\0'; // Mark this point as end 
    printf("Client says: %s\n", &(read_buffer[header_size]));

    // reply to the client using same protocol 

    const char reply[] = "hello world!";
    char write_buffer[header_size + sizeof(reply)];
    uint32_t reply_length = (uint32_t)strlen(reply);
    // paste the server message length in the first header_size bytes to the buffer
    memcpy(write_buffer, &reply_length, header_size);
    // and rest of reply from write_buffer + header_size 
    memcpy(&write_buffer[header_size], reply, reply_length);
    return write_all(conn_fd, write_buffer, header_size + reply_length);
}


int main() {
    // AF_INET is for IPv4, and AF_INET6 is for ipv6
    // Sock stream is for TCP
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
    addr.sin_port = ntohs(3000); // again int 
    addr.sin_addr.s_addr = ntohs(0); // wildcard address 0.0.0.0:3000

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

    // loop for each connection 

    while (true) {
        // accept the connection from the client...
        struct sockaddr_in client_addr = {};
        socklen_t socklen = sizeof(client_addr);
        // conn_fd stores the file descriptor representing the newly accepted connection
        // accept() is used to accept incoming connection requests on a listening socket 
        // It also take the container i.e sockaddr_in type which is used to hold the 
        // info about client addr
        int conn_fd = accept(fd, (struct sockaddr* )&client_addr, &socklen);
        if (conn_fd < 0) {
            continue; // error 
        }

        // do_something(conn_fd);
        // only serves one client connection at once 

        while (true) {
            // one_request will parse only one request and respond, until something bad happen
            int32_t err = one_request(conn_fd);
            if (err) {
                break;
            }
        }
        close(conn_fd);
    }
    return 0;
}

