#include "l7_common.h"

#define BACKLOG 5
#define MAX_EVENTS 16
#define MAX_PID_SIZE 64

volatile sig_atomic_t do_work = 1;

void sigint_handler(int sig) { do_work = 0; }

int usage(char* name) {
    fprintf(stderr, "%s port\n", name);
    exit(EXIT_FAILURE);
}

uint16_t calculate(int size, char* data) {
    uint16_t calculated = 0;
    for (int i = 0; i < size; i++) {
        if (data[i] == '\0')
            break;
        calculated += data[i] - '0';
    }
    return calculated;
}

uint16_t prepare_data(uint16_t data) {
    return htons(data);
}

void server_work(int tcp_socket) {

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

    char data[MAX_PID_SIZE];
    uint16_t data_to_send;
    int nfds;
    sigset_t mask, oldmask;
    ssize_t size;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    uint16_t best_score = 0;

    while (do_work)
    {
        if ((nfds = epoll_pwait(epoll_descriptor, events, MAX_EVENTS, -1, &oldmask)) > 0)
        {
            for (int n = 0; n < nfds; n++)
            {
                int client_socket = add_new_client(events[n].data.fd);
                if ((size = bulk_read(client_socket, (char *)data, MAX_PID_SIZE)) < 0)
                    ERR("read:");

                if (size == MAX_PID_SIZE) {
                    printf("PID=%d\n", atoi(data));

                    data_to_send = calculate(size, data);
                    if (data_to_send > best_score)
                        best_score = data_to_send;
                    data_to_send = prepare_data(data_to_send);

                    if (bulk_write(client_socket, (char*)&data_to_send, sizeof(int16_t)) < 0 && errno != EPIPE)
                        ERR("write:");
                }
                if (TEMP_FAILURE_RETRY(close(client_socket)) < 0)
                    ERR("close");
            }
        }
        else
        {
            if (errno == EINTR)
                continue;
            ERR("epoll_pwait");
        }
    }
    if (TEMP_FAILURE_RETRY(close(epoll_descriptor)) < 0)
        ERR("close");
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    printf("HIGH SUM=%d\n", best_score);
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

    server_work(tcp_listen_socket);

    if (TEMP_FAILURE_RETRY(close(tcp_listen_socket)) < 0)
        ERR("close");

    fprintf(stderr, "Server has terminated.\n");
    return EXIT_SUCCESS;
}