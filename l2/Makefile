CC = gcc
CFLAGS = -Wall -Wextra -pedantic

SRCS = prog13a.c prog13b.c prog14.c prog15.c prog16a.c prog16b.c example1.c example2.c

BINS = $(SRCS:.c=)

all: $(BINS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(BINS)

.PHONY: all clean