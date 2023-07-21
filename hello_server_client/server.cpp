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
static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

static void msg(const char* msg) {
    fprintf(stderr, "%s\n", msg);
}

static void do_something(int conn_fd) {
    char read_buffer[64] = {};
    // read returns the number of read or written bytes 
    ssize_t n = read(conn_fd, read_buffer, sizeof(read_buffer) - 1);

    if (n < 0) {
        msg("read() error");
        return;
    }
    printf("client is saying: %s\n", read_buffer);

    char write_buffer[64] = "hello world!";
    write(conn_fd, write_buffer, strlen(write_buffer));
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
        // accept 
        struct sockaddr_in client_addr = {};
        socklen_t socklen = sizeof(client_addr);
        int conn_fd = accept(fd, (struct sockaddr* )&client_addr, &socklen);
        if (conn_fd < 0) {
            continue; // error 
        }
        do_something(conn_fd);
        close(conn_fd);
    }
    return 0;
}

