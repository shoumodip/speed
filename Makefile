CFLAGS=-Wall -Wextra -std=c11 -pedantic
LIBS=-lncurses

speed: $(wildcard src/*)
	$(CC) $(CFLAGS) -o $@ src/*.c $(LIBS)
