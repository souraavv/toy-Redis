#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}


int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET; 
    addr.sin_port = ntohs(3000);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK); // 127.0.0.1 or localhost 

    int return_value = connect(fd, (const struct sockaddr *)& addr, sizeof(addr));

    if (return_value < 0) {
        die("connect()");
    }

    char msg[] = "Hi there!";
    write(fd, msg, strlen(msg));

    char read_buffer[64] = {};
    ssize_t n = read(fd, read_buffer, sizeof(read_buffer));
    if (n < 0) {
        die("read");
    }

    printf("Server says %s \n", read_buffer);
    close(fd);
    return 0;
}