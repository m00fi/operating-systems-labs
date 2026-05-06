#include "l7_common.h"

#define MAX_PID_SIZE 64

void usage(char* name) {
    fprintf(stderr, "%s name port\n", name);
    exit(EXIT_FAILURE);
}

void print_answer(uint16_t data) {
    uint16_t data_to_print = ntohs(data);
    printf("SUM=%d\n", data_to_print);
}

int main(int argc, char **argv) {
    int fd;
    pid_t pid = getpid();
    if (argc != 3)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    //printf("My pid is: %d\n", pid);
    fd = connect_tcp_socket(argv[1], argv[2]);

    char data[MAX_PID_SIZE];
    uint16_t data_to_read;

    memset(data, 0, MAX_PID_SIZE);

    sprintf(data, "%d", pid);

    if (bulk_write(fd, data, MAX_PID_SIZE) < 0)
        ERR("write:");

    if (bulk_read(fd, (char *)&data_to_read, sizeof(uint16_t)) < sizeof(uint16_t))
        ERR("read:");
    print_answer(data_to_read);

    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");
    return EXIT_SUCCESS;
}