CC = gcc
CFLAGS = -std=gnu99 -Wall
LDFLAGS = -pthread -lrt

TARGETS = example1-napoleon

.PHONY: all clean

all: $(TARGETS)

example1-napoleon: example1-napoleon.c l8_common.h
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGETS)
