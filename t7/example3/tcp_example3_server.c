#define _GNU_SOURCE

#include "common.h"

#define BACKLOG 10
#define MAX_EVENTS 10
#define MAX_CLIENTS 4

sig_atomic_t run = 1;

void sigint_handler(int signo) {
    (void)signo;
    run = 0;
}

void usage(char* name) {
    fprintf(stderr, "%s port\n", name);
    exit(EXIT_FAILURE);
}

void do_server(int tcp_listen_socket) {
    int epoll_descriptor;
    if ((epoll_descriptor = epoll_create1(0)) < 0)
    {
        ERR("epoll_create:");
    }
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;

    event.data.fd = tcp_listen_socket;
    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, tcp_listen_socket, &event) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    int nfds;
    sigset_t mask, oldmask;
    ssize_t size;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    int client_count = 0;
    char msg[5];

    int client_fds[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_fds[i] = -1;
    }

    char city[21];
    for (int i = 1; i <= 20; i++) {
        city[i] = 'g';
    }

    while (run)
    {
        if ((nfds = epoll_pwait(epoll_descriptor, events, MAX_EVENTS, -1, &oldmask)) > 0)
        {
            for (int n = 0; n < nfds; n++)
            {
                int fd = events[n].data.fd;
                if (fd == tcp_listen_socket) {
                    if (client_count >= MAX_CLIENTS) continue;

                    int client_socket = add_new_client(events[n].data.fd);
                    event.data.fd = client_socket;

                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (client_fds[i] <= 0) {
                            client_fds[i] = client_socket;
                            break;
                        }
                    }
                    client_count++;

                    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, client_socket, &event) == -1)
                    {
                        perror("epoll_ctl: client_sock");
                        exit(EXIT_FAILURE);
                    }
                }
                else {
                    size = read(fd, msg, 4);

                    if (size < 0) ERR("read");
                    if (size == 0) {
                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (client_fds[i] == fd) {
                                client_fds[i] = -1;
                                client_count--;
                            }
                        }
                        if (TEMP_FAILURE_RETRY(close(fd)) < 0)
                            ERR("close");
                    }

                    if (size == 4) {
                        printf(": %s", msg);
                        char c = msg[0];

                        if (c == 'g' || c == 'p') {
                            int num = (msg[1] - '0') * 10 + msg[2] - '0';
                            if (num < 20 && num >= 0) {
                                if (city[num] != c) {
                                    city[num] = c;
                                    for (int i = 0; i < MAX_CLIENTS; i++) {
                                        if (client_fds[i] != fd && client_fds[i] > 0)
                                        {
                                            write(client_fds[i], msg, 4);
                                        }
                                    }
                                }
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

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] > 0) {
            close(client_fds[i]);
        }
    }

    if (TEMP_FAILURE_RETRY(close(epoll_descriptor)) < 0)
        ERR("close");
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    for (int i = 1; i <= 20; i++) {
        char* owner;
        if (city[i] == 'g') {
            owner = "Greeks";
        }
        else {
            owner = "Peruvians";
        }
        printf("%2d: %s\n", i, owner);
    }

}

int main(int argc, char **argv) {
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

    do_server(tcp_listen_socket);

    if (TEMP_FAILURE_RETRY(close(tcp_listen_socket)) < 0)
        ERR("close");

    fprintf(stderr, "Server has terminated.\n");
    return EXIT_SUCCESS;

}