override CFLAGS=-Wall -Wextra -Wshadow -g -O0 -fsanitize=address,undefined

ifdef CI
override CFLAGS=-Wall -Wextra -Werror
endif

.PHONY: clean all

all: tcp_example3_server

tcp_example3_server: tcp_example3_server.c
	gcc $(CFLAGS) -o tcp_example3_server tcp_example3_server.c

clean:
	rm -f tcp_example3_server