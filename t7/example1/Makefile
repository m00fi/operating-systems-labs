CC = gcc
CFLAGS = -std=gnu99 -Wall
LDFLAGS = -pthread -lrt

TARGETS = tcp_example1_server tcp_example1_client

.PHONY: all clean

all: $(TARGETS)

tcp_example1_server: tcp_example1_server.c l7_common.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tcp_example1_client: tcp_example1_client.c l7_common.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGETS)