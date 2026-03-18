CC = gcc
CFLAGS = -std=gnu99 -Wall

TARGETS = prog22a

all: $(TARGETS)

prog21a_s: prog21a_s.o
	$(CC) $(CFLAGS) prog21a_s.o -o prog21a_s

prog21a_c: prog21a_c.o
	$(CC) $(CFLAGS) prog21a_c.o -o prog21a_c

prog22a: prog22a.o
	$(CC) $(CFLAGS) prog22a.o -o prog22a

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGETS)

.PHONY: all clean