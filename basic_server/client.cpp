#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <vector>
#include <string>
#include <netinet/ip.h>

static const size_t MSG_MAX_LEN = 4096;
static const uint8_t HEADER_LEN = 4;

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

static void msg(const char* msg) {
    fprintf(stderr, "%s\n", msg);
}

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


static int32_t send_request(int fd, const std::vector<std::string> &raw_request) {
    msg("sending request");
    uint32_t len = 4;
    for (const std::string &s: raw_request) {
        len += 4 + s.size(); // length of string(int i.e 4) + string size..
    }
    if (len > MSG_MAX_LEN) {
        return -1;
    }
    char write_buffer[4 + MSG_MAX_LEN];
    memcpy(&write_buffer[0], &len, 4); // total length of string (max length of request)
    uint32_t n = raw_request.size(); // total length of request 
    memcpy(&write_buffer[4], &n, 4);
    size_t cur_pos = 8; // iterator on write_buffer 

    for (const std::string &s: raw_request) {
        uint32_t cur_len = (uint32_t) s.size();
        memcpy(&write_buffer[cur_pos], &cur_len, 4);
        memcpy(&write_buffer[cur_pos + 4], s.data(), s.size());
        cur_pos += s.size() + 4; 
    }

    return write_all(fd, write_buffer, 4 + len); // 4 is the length of header

}

static int32_t read_response(int fd) {
    char read_buffer[HEADER_LEN + MSG_MAX_LEN + 1];
    errno = 0;
    int32_t err = read_full(fd, read_buffer, 4); // read first four character from fd to read_buffer
    if (err) {
        if (errno == 0) {
            msg("EOF");
        }
        else {
            msg("read() error");
        }
        return err;
    }
    uint32_t len = 0;
    memcpy(&len, read_buffer, 4); // the first four character were length thus get those to len
    if (len > MSG_MAX_LEN) {
        msg("too long");
        return -1;
    }

    // reply body 
    err = read_full(fd, &read_buffer[4], len); // write data from fd to read_buffer starting @HEADER_LEN 
    if (err) {
        msg("read() err");
        return err;
    }

    // response code

    uint32_t res_code = 0;
    if (len < 4) {
        msg("bad response");
        return -1;
    }

    memcpy(&res_code, &read_buffer[4], 4); // 2nd thing after header is the response code, first is the total length of response i.e response header + actual response 
    printf("server says: [%u] %.*s\n", res_code, len - 4, &read_buffer[4 + 4]);
    return 0;
}

int main(int argc, char **argv) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET; 
    addr.sin_port = ntohs(3001); // port number where to send.
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK); // 127.0.0.1 or localhost (send this packet to self.)

    int return_value = connect(fd, (const struct sockaddr *)& addr, sizeof(addr));

    if (return_value) {
        die("connect()");
    }

    std::vector<std::string> raw_request; 
    for (int i = 1; i < argc; ++i) {
        raw_request.push_back(argv[i]);
    }
    
    int32_t err = send_request(fd, raw_request);
    if (err) {
        goto L_DONE;
    }
    err = read_response(fd);
    if (err) {
        goto L_DONE;
    }

L_DONE:
    close(fd);
    return 0;
}
