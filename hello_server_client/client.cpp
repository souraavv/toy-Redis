#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <cstring>
static void die(int line_number, const char *err_msg)
{
    fprintf(stderr, "line number : %d and the error msg : %s \n", line_number, err_msg);
    exit(1);
}
int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    // int socket(int domain, int type, int protocol);

    // AF_INET = domain , AF_INET is the internet address family for IPv4.
    // SOCK_STREAM = type,
    // sock_stream is a socket type of tcp, the protocol that will be used to transport messages in the network

    if (fd < 0)
    {
        die(__LINE__, "socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK); // 127.0.0.1

    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv)
    {
        die(__LINE__, "connect");
    }

    char msg[] = "hello";
    write(fd, msg, strlen(msg));

    char rbuf[64] = {};
    ssize_t n = read(fd, rbuf, sizeof(rbuf) - 1);
    if (n < 0)
    {
        die(__LINE__, "read");
    }
    printf("server says: %s\n", rbuf);
    close(fd);
}