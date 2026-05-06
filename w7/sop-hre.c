#define _GNU_SOURCE
#include "l4-common.h"

#define BACKLOG 10

volatile sig_atomic_t do_work = 1;

void sigint_handler(int sig) { do_work = 0; }

void usage(char* name)
{
    fprintf(stderr, "USAGE: %s port\n", name);
    exit(EXIT_FAILURE);
}

void server_work(int tcp_socket)
{
    int client_socket = accept(tcp_socket, NULL, NULL);
    if (client_socket < 0)
        ERR("accept");

    printf("Client connected.\n");
    if (TEMP_FAILURE_RETRY(close(client_socket)) < 0)
        ERR("close");
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
