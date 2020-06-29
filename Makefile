sstow: sstow.c
	$(CC) -g -Wall -Wextra -pedantic -std=c99 -o sstow sstow.c

clean:
	rm -rf sstow
