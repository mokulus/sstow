.PHONY: all clean install uninstall format

all: sstow

sstow: sstow.c
	$(CC) $(CFLAGS) -O2 -D_DEFAULT_SOURCE -lbsd -g -Wall -Wextra -pedantic -std=c99 -o sstow sstow.c

clean:
	rm -rf sstow

install: sstow
	cp sstow /usr/local/bin/sstow

uninstall:
	rm -f /usr/local/bin/sstow

format: sstow.c
	clang-format -i --style=file $^
