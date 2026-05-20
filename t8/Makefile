CC = gcc
CFLAGS = -std=gnu99 -Wall
LDFLAGS = -pthread -lrt

TARGETS = l8-1_server l8-1_client

.PHONY: all clean

all: $(TARGETS)

l7-1_server: l8-1_server.c l8_common.h
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

l7-1_client_local: l8-1_client.c l8_common.h
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
clean:
	rm -f $(TARGETS)
