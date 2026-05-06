#define _GNU_SOURCE
#include "l4-common.h"

#define BACKLOG 10
#define MAX_EVENTS 10
#define WELCOME_MESSAGE "Welcome, elector!\n"
#define MAX_MESSAGE_SIZE 1024
#define ELECTORS_NUM 7

typedef struct Electors {
    int connected;
    int vote;
    int fd;
}elector_t;

volatile sig_atomic_t do_work = 1;

void sigint_handler(int sig) { do_work = 0; }

void usage(char* name)
{
    fprintf(stderr, "USAGE: %s port\n", name);
    exit(EXIT_FAILURE);
}

void server_work(int tcp_socket)
{
    int epoll_descriptor;
    if ((epoll_descriptor = epoll_create1(0)) < 0)
    {
        ERR("epoll_create:");
    }
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;

    event.data.fd = tcp_socket;
    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, tcp_socket, &event) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    int nfds;
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    elector_t electors[8];

    while (do_work)
    {
        if ((nfds = epoll_pwait(epoll_descriptor, events, MAX_EVENTS, -1, &oldmask)) > 0)
        {
            for (int n = 0; n < nfds; n++)
            {
                int curr_fd = events[n].data.fd;

                if (curr_fd == tcp_socket) {
                    int client_socket = add_new_client(events[n].data.fd);
                    if (bulk_write(client_socket, WELCOME_MESSAGE, sizeof(WELCOME_MESSAGE)) < 0 && errno != EPIPE)
                        ERR("write:");

                    event.data.fd = client_socket;
                    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, client_socket, &event) == -1)
                    {
                        perror("epoll_ctl: client_sock");
                        exit(EXIT_FAILURE);
                    }
                }
                else {
                    char msg;
                    int bytes_received = recv(curr_fd, &msg, 1, 0);

                    if (bytes_received <= 0) {
                        if (bytes_received == 0) {
                            for (int j = 1; j <= ELECTORS_NUM; j++) {
                                if (electors[j].fd == curr_fd) {
                                    electors[j].connected = 0;
                                    electors[j].fd = -1;
                                }
                            }
                            if (TEMP_FAILURE_RETRY(close(curr_fd)) < 0)
                                ERR("close");
                        }
                        else {
                            ERR("recv()");
                        }
                    } else {
                        int registered_elector = 0;
                        for (int j = 1; j <= ELECTORS_NUM; j++) {
                            if (electors[j].fd == curr_fd) {
                                if (msg >= '1' && msg <= '3') {
                                    electors[j].vote = msg - '0';
                                    printf("Elector %d voted %d\n", j, electors[j].vote);
                                }
                                registered_elector = 1;
                                break;
                            }
                        }

                        if (!registered_elector) {
                            if (msg >= '1' && msg <= '7') {
                                int index = msg - '0';
                                if (!electors[index].connected) {
                                    electors[index].connected = 1;
                                    electors[index].fd = curr_fd;
                                    event.events = EPOLLIN;
                                    event.data.fd = curr_fd;
                                    char welcome_message[1024];

                                    int size = sprintf(welcome_message, "Welcome, elector of %d!\n", index);

                                    if (write(curr_fd, welcome_message, size) < 0)
                                        ERR("write");
                                }
                                else {
                                    close(curr_fd);
                                }
                            }
                            else {
                                close(curr_fd);
                            }
                        }
                    }
                }
            }
        }
        else
        {
            if (errno == EINTR)
                continue;
            ERR("epoll_pwait");
        }
    }

    for (int i = 1; i <= ELECTORS_NUM; i++) {
        if (electors[i].connected) {
            close(electors[i].fd);
        }
    }
    if (TEMP_FAILURE_RETRY(close(epoll_descriptor)) < 0)
        ERR("close");
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

}

int main(int argc, char** argv) {
    if (argc != 2)
        usage(argv[0]);

    int tcp_listen_socket;
    int new_flags;

    if (sethandler(SIG_IGN, SIGPIPE))
        ERR("Setting SIGPIPE:");
    if (sethandler(sigint_handler, SIGINT))
        ERR("Setting SIGINT:");


    tcp_listen_socket = bind_tcp_socket(atoi(argv[1]), BACKLOG);
    new_flags = fcntl(tcp_listen_socket, F_GETFL) | O_NONBLOCK;
    fcntl(tcp_listen_socket, F_SETFL, new_flags);

    server_work(tcp_listen_socket);

    if (TEMP_FAILURE_RETRY(close(tcp_listen_socket)) < 0)
        ERR("close");

    fprintf(stderr, "Server has terminated.\n");
    return EXIT_SUCCESS;
}
