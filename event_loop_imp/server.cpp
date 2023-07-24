#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>
#include <cstring>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <netinet/in.h>
#include <poll.h>
#include <fcntl.h>
#include <assert.h>

const size_t k_max_msg = 4096;

enum
{
    STATE_REQ = 0,
    STATE_RES = 1,
    STATE_END = 2, // mark the connection for deletion
};

struct Conn
{
    int fd = -1;
    uint32_t state = 0; // either STATE_REQ or STATE_RES
    // buffer for reading
    size_t rbuf_size = 0;
    uint8_t rbuf[4 + k_max_msg];
    // buffer for writing
    size_t wbuf_size = 0;
    size_t wbuf_sent = 0;
    uint8_t wbuf[4 + k_max_msg];
};
static bool try_one_request(Conn *conn);

static void die(int line_number, const char *err_msg)
{
    fprintf(stderr, "line number : %d and the error msg : %s \n", line_number, err_msg);
    exit(1);
}
static void msg(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
}

static void conn_put(std::vector<Conn *> &fd2conn, struct Conn *conn)
{
    if (fd2conn.size() <= (size_t)conn->fd)
    {
        fd2conn.resize(conn->fd + 1);
    }
    fd2conn[conn->fd] = conn;
}

// Saurav, If you read this confirm this.

// what this function does from my understanding is that it sets the
// non blocking mode on fd. It means that reading or write operation
// performed on fd will not consume any time if there is no data available.

static void fd_set_nb(int fd)
{
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    // F_GETFL(void)
    // Return(as the function result) the file access mode and the
    //     file status flags;
    // arg is ignored.

    if (errno)
    {
        die(__LINE__, "fcntl error");
        return;
    }

    flags |= O_NONBLOCK;
    // Under Linux, the O_NONBLOCK flag is sometimes used in cases
    // where one wants to open but does not necessarily have the intention to read or write.
    errno = 0;

    (void)fcntl(fd, F_SETFL, flags);
    // F_SETFL (int)
    // Set the file status flags to the value specified by arg. Which in this case is flags.

    if (errno)
    {
        die(__LINE__, "fcntl error");
    }
}

static bool try_fill_buffer(Conn *conn)
{
    assert(conn->rbuf_size < sizeof(conn->rbuf));
    ssize_t rv = 0;
    do
    {
        size_t cap = sizeof(conn->rbuf) - conn->rbuf_size;
        rv = read(conn->fd, &conn->rbuf[conn->rbuf_size], cap);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0 && errno == EAGAIN)
    {
        // got EAGAIN, stop.
        return false;
    }
    if (rv < 0)
    {
        msg("read() error");
        conn->state = STATE_END;
        return false;
    }
    if (rv == 0)
    {
        if (conn->rbuf_size > 0)
        {
            msg("unexpected EOF");
        }
        else
        {
            msg("EOF");
        }
        conn->state = STATE_END;
        return false;
    }

    conn->rbuf_size += (size_t)rv;
    assert(conn->rbuf_size <= sizeof(conn->rbuf));

    // Try to process requests one by one.
    while (try_one_request(conn))
    {
    }
    return (conn->state == STATE_REQ);
}

static bool try_flush_buffer(Conn *conn)
{
    ssize_t rv = 0;
    do
    {
        size_t remain = conn->wbuf_size - conn->wbuf_sent;
        rv = write(conn->fd, &conn->wbuf[conn->wbuf_sent], remain);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0 && errno == EAGAIN)
    {
        // got EAGAIN, stop.
        return false;
    }
    if (rv < 0)
    {
        msg("write() error");
        conn->state = STATE_END;
        return false;
    }
    conn->wbuf_sent += (size_t)rv;
    assert(conn->wbuf_sent <= conn->wbuf_size);
    if (conn->wbuf_sent == conn->wbuf_size)
    {
        // response was fully sent, change state back
        conn->state = STATE_REQ;
        conn->wbuf_sent = 0;
        conn->wbuf_size = 0;
        return false;
    }
    // still got some data in wbuf, could try to write again
    return true;
}

static void state_res(Conn *conn)
{
    while (try_flush_buffer(conn))
    {
    }
}
// state_req is for reading
static void state_req(Conn *conn)
{
    while (try_fill_buffer(conn))
    {
    }
}

static void connection_io(Conn *conn)
{
    if (conn->state == STATE_REQ)
    {
        state_req(conn);
    }
    else if (conn->state == STATE_RES)
    {
        state_res(conn);
    }
    else
    {
        assert(0);
    }
}

static int32_t accept_new_conn(std::vector<Conn *> &fd2conn, int fd)
{
    // accept
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
    if (connfd < 0)
    {
        msg("accept() error in accept_new_conn");
        return -1;
    }
    fd_set_nb(connfd);

    // creating the struct Conn
    struct Conn *conn = (struct Conn *)malloc(sizeof(struct Conn));
    if (!conn)
    {
        close(connfd);
        return -1;
    }

    conn->fd = connfd;
    conn->state = STATE_REQ;
    conn->rbuf_size = 0;
    conn->wbuf_size = 0;
    conn->wbuf_sent = 0;
    conn_put(fd2conn, conn);
    return 0;
}

static bool try_one_request(Conn *conn)
{
    // try to parse a request from the buffer
    if (conn->rbuf_size < 4)
    {
        // not enough data in the buffer. Will retry in the next iteration
        return false;
    }
    uint32_t len = 0;
    memcpy(&len, &conn->rbuf[0], 4);
    if (len > k_max_msg)
    {
        msg("too long");
        conn->state = STATE_END;
        return false;
    }
    if (4 + len > conn->rbuf_size)
    {
        // not enough data in the buffer. Will retry in the next iteration
        return false;
    }

    // got one request, do something with it
    printf("client says: %.*s\n", len, &conn->rbuf[4]);

    // generating echoing response
    memcpy(&conn->wbuf[0], &len, 4);
    memcpy(&conn->wbuf[4], &conn->rbuf[4], len);
    conn->wbuf_size = 4 + len;

    // remove the request from the buffer.
    // note: frequent memmove is inefficient.
    // note: need better handling for production code.
    size_t remain = conn->rbuf_size - 4 - len;
    if (remain)
    {
        memmove(conn->rbuf, &conn->rbuf[4 + len], remain);
    }
    conn->rbuf_size = remain;

    // change state
    conn->state = STATE_RES;
    state_res(conn);

    // continue the outer loop if the request was fully processed
    return (conn->state == STATE_REQ);
}

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        die(__LINE__, "socket()");
    }

    int val = 1;
    printf("value for sol socket is %d", SOL_SOCKET);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0);

    int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv < 0)
    {
        die(__LINE__, "bind failed");
    }
    rv = listen(fd, SOMAXCONN);

    if (rv)
    {
        die(__LINE__, "listen failed");
    }

    // a map of all client connection, keyed by fd;
    std::vector<Conn *> fd2conn;

    // set the listen fd to nonblocking mode
    //  no idea why , I think this is done for faster read-write operation.
    fd_set_nb(fd);

    // the event loop
    std::vector<struct pollfd> poll_args;
    while (true)
    {
        // prepare the args of the poll();
        poll_args.clear();

        // for convenience, the listening fd is put in the first position
        struct pollfd pfd = {fd, POLLIN, 0};
        // struct pollfd
        // {
        //     int fd;        /* file descriptor */
        //     short events;  /* requested events */
        //     short revents; /* returned events */
        // };
        //            The field events is an input  parameter,  a  bit  mask  specifying  the
        //    events  the  application  is  interested in for the file descriptor fd.
        //    This field may be specified as zero, in which case the only events that
        //    can  be returned in revents are POLLHUP, POLLERR, and POLLNVAL

        poll_args.push_back(pfd);
        // connection fds

        for (Conn *conn : fd2conn)
        {
            if (!conn)
                continue;

            struct pollfd pfd = {};
            pfd.fd = conn->fd;
            pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT;
            // POLLIN = There is data to read
            // POLLOUT = writing is allowed
            pfd.events = pfd.events | POLLERR;

            // POLLERR
            //   Error  condition  (only returned in revents; ignored in events).
            //   This bit is also set for a  file  descriptor  referring  to  the
            //   write end of a pipe when the read end has been closed.
            // I don't think it is really needed in this case though

            poll_args.push_back(pfd);
        }
        // poll for active fds
        // timeout arg does not matter here
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), 1000);
        if (rv < 0)
        {
            die(__LINE__, "poll() error");
        }

        for (int i = 1; i < poll_args.size(); ++i)
        {
            if (poll_args[i].revents)
            {
                Conn *conn = fd2conn[poll_args[i].fd];
                connection_io(conn);
                if (conn->state == STATE_END)
                {
                    // client closed normally, or something bad happened.
                    // destroy this connection
                    fd2conn[conn->fd] = NULL;

                    (void)close(conn->fd);

                    // void is written so that compiler warning can be ignored

                    free(conn);
                }
            }
        }
        // try to accept a new connectopn if listening fd is active
        if (poll_args[0].revents)
        {
            (void)accept_new_conn(fd2conn, fd);
        }
    }

    return 0;
}