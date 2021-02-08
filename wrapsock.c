#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>


int
Accept(int fd, struct sockaddr *sa, socklen_t *salenptr)
{
    int  n;

    if ( (n = accept(fd, sa, salenptr)) < 0) {
        perror("accept error");
        exit(1);
    }
    return(n);
}

void
Bind(int fd, const struct sockaddr *sa, socklen_t salen)
{
    if (bind(fd, sa, salen) < 0){
        perror("bind error");
        exit(1);
    }
}

/* This connect wrapper function doesn't quite do what we want */
/* The fix is below */
/*void
Connect(int fd, const struct sockaddr *sa, socklen_t salen)
{
    if (connect(fd, sa, salen) < 0) {
        perror("connect error");
        perror(1);
    }
}
*/

int
Connect(int fd, const struct sockaddr *sa, socklen_t salen)
{
    int result;
    if ((result = connect(fd, sa, salen)) < 0) {
        perror("connect error");
    }
    return(result);
}

void
Listen(int fd, int backlog)
{
    if (listen(fd, backlog) < 0) {
        perror("listen error");
        exit(1);
    }
}

int
Select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
       struct timeval *timeout)
{
    int n;

    if ( (n = select(nfds, readfds, writefds, exceptfds, timeout)) < 0) {
        perror("select error");
        exit(1);
    }
    return(n);              /* can return 0 on timeout */
}


int
Socket(int family, int type, int protocol)
{
    int n;

    if ( (n = socket(family, type, protocol)) < 0) {
        perror("socket error");
        exit(1);
    }
    return(n);
}

void
Close(int fd)
{
    if (close(fd) == -1) {
        perror("close error");
        exit(1);
    }
}
