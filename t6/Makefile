CC = gcc
CFLAGS = -std=gnu99 -Wall

TARGETS = l6-1 l6-2_server l6-2_client shm-example1

all: $(TARGETS)


l6-1: l6-1.o
	$(CC) $(CFLAGS) l6-1.o -o l6-1
l6-2_server: l6-2_server.o
	$(CC) $(CFLAGS) l6-2_server.o -o l6-2_server
l6-2_client: l6-2_client.o
	$(CC) $(CFLAGS) l6-2_client.o -o l6-2_client
shm-example1: shm-example1.o
	$(CC) $(CFLAGS) shm-example1.o -o shm-example1
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGETS)

.PHONY: all clean
