#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <string.h>
static void msg(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
}

static void die(int line_number, const char *err_msg)
{
    fprintf(stderr, "line number : %d and the error msg : %s \n", line_number, err_msg);
    exit(1);
}

static void do_something(int connfd)
{
    char rbuf[64] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0)
    {
        msg("read() error");
        return;
    }
    printf("client says: %s\n", rbuf);

    char wbuf[] = "world";
    write(connfd, wbuf, strlen(wbuf));
}

//        int socket(int domain, int type, int protocol);
//
// socket -- socket()  creates  an endpoint for communication and returns a file de‐
//        scriptor that refers to that endpoint.  The file descriptor returned by
//        a  successful call will be the lowest-numbered file descriptor not cur‐
//        rently open for the process.

//        The domain argument specifies a communication domain; this selects  the
//        protocol  family  which will be used for communication.  These families
//        are defined in <sys/socket.h>.

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    //       int setsockopt(int sockfd, int level, int optname,
    //                  const void *optval, socklen_t optlen);
    // The SO_REUSEADDR socket option allows a socket to forcibly bind to a port in use by another socket.

    // TO DO -- Why SO_Reuseaddr is needed for this?

    // BIND
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    // the port to which application(TCP) is binded. Not needed for our usecase
    addr.sin_addr.s_addr = ntohl(0); // wild card address 0.0.0.0
    int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));

    if (rv)
    {
        die(__LINE__, "bind failed");
    }

    // listen
    //   int listen(int sockfd, int backlog);

    rv = listen(fd, SOMAXCONN);
    // SOMAXCONN = 4096
    //  The backlog argument defines the maximum length to which the  queue  of
    //   pending  connections  for sockfd may grow.
    if (rv)
    {
        die(__LINE__, "listen ()");
    }

    while (1)
    {
        // accept
        struct sockaddr_in client_addr = {};
        socklen_t socklen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);

        if (connfd < 0)
        {
            continue;
        }
        do_something(connfd);
        close(connfd);
    }
}