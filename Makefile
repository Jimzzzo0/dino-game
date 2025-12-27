CC = gcc
CFLAGS = -O2 -Wall -Wextra -Iinclude

NASM = nasm
NASMFLAGS = -f elf64

TARGET = dino

all: $(TARGET)

# Link: C object + ASM object
$(TARGET): src/main.o src/asm_funcs.o
	$(CC) $(CFLAGS) -o $@ $^

# Compile C -> .o
src/main.o: src/main.c
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble NASM -> .o
src/asm_funcs.o: src/asm_funcs.asm
	$(NASM) $(NASMFLAGS) $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f src/*.o $(TARGET)
