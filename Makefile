CC=gcc
CFLAGS=-O2 -Wall -Wextra -Iinclude

OBJS=src/main.o

all: dino

dino: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: dino
	./dino

clean:
	rm -f dino $(OBJS)
