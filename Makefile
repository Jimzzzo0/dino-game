CC=gcc
CFLAGS=-O2 -Wall -Wextra -Iinclude

NASM=nasm
NASMFLAGS=-f elf64

all: dino

dino: src/main.o src/asm_funcs.o
	$(CC) -o dino src/main.o src/asm_funcs.o

src/main.o: src/main.c
	$(CC) $(CFLAGS) -c src/main.c -o src/main.o

src/asm_funcs.o: src/asm_funcs.asm
	$(NASM) $(NASMFLAGS) src/asm_funcs.asm -o src/asm_funcs.o

run: dino
	./dino

clean:
	rm -f src/*.o dino
