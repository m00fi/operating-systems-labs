#ifndef L7_COMMON_H
#define L7_COMMON_H

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)             \
(__extension__({                               \
long int __result;                         \
do                                         \
__result = (long int)(expression);     \
while (__result == -1L && errno == EINTR); \
__result;                                  \
}))
#endif

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// DEKLARACJE FUNKCJI:
int sethandler(void (*f)(int), int sigNo);
int make_local_socket(char *name, struct sockaddr_un *addr);
int connect_local_socket(char *name);
int bind_local_socket(char *name, int backlog_size);
int make_tcp_socket(void);
struct sockaddr_in make_address(char *address, char *port);
int connect_tcp_socket(char *name, char *port);
int bind_tcp_socket(uint16_t port, int backlog_size);
int add_new_client(int sfd);
ssize_t bulk_read(int fd, char *buf, size_t count);
ssize_t bulk_write(int fd, char *buf, size_t count);

#endif // L7_COMMON_H