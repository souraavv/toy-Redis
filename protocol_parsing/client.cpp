#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

const size_t k_max_msg = 4096;
const uint8_t header_length = 4;

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


static int32_t query(int fd, const char* text) {
    uint32_t text_length = (uint32_t)strlen(text);
    if (text_length > k_max_msg) {
        return -1;
    }

    char write_buffer[header_length + k_max_msg];
    memcpy(write_buffer, &text_length, header_length);
    memcpy(&write_buffer[header_length], text, text_length);
    if (int32_t err = write_all(fd, write_buffer, header_length + text_length)) {
        return err;
    }

    // four byte header 

    char read_buffer[header_length + k_max_msg];
    errno = 0;
    int32_t err = read_full(fd, read_buffer, header_length);
    if (err) {
        if (errno == 0) {
            msg("EOF");
        }
        else {
            msg("read() error");
        }
        return err;
    }

    uint32_t reply_length = 0;
    memcpy(&reply_length, read_buffer, header_length); // read the length send by the server ;
    if (reply_length > k_max_msg) {
        msg("too long");
        return -1;
    }
    // reply body 
    err = read_full(fd, &read_buffer[header_length], reply_length);
    if (err) {
        msg("read() error");
        return err;
    }

    read_buffer[header_length + reply_length] = '\0';
    printf("Server is saying: %s\n", &read_buffer[header_length]);
    return 0;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET; 
    addr.sin_port = ntohs(3000); // port number where to send.
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK); // 127.0.0.1 or localhost (send this packet to self.)

    int return_value = connect(fd, (const struct sockaddr *)& addr, sizeof(addr));

    if (return_value < 0) {
        die("connect()");
    }

    int32_t err = query(fd, "hello1");
    if (err) {
        goto L_DONE;
    }
    err = query(fd, "hello2");
    if (err) {
        goto L_DONE;
    }
    err = query(fd, "hello3");
    if (err) {
        goto L_DONE;
    }

L_DONE:
    close(fd);
    return 0;
}