CC = gcc
CFLAGS = -std=gnu99 -Wall
LDFLAGS = -pthread -lrt

TARGETS = l7-1_server l7-1_client_local l7-1_client_tcp

.PHONY: all clean

all: $(TARGETS)

l7-1_server: l7-1_server.c l7_common.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

l7-1_client_local: l7-1_client_local.c l7_common.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

l7-1_client_tcp: l7-1_client_tcp.c l7_common.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGETS)