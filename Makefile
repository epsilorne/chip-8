# Set compiler to gcc
CC = gcc

# Set flags
CFLAGS = -Wall -g

PROGS = chip8
OBJS = $(addsuffix .o, $(FILES))

all: $(PROGS)

$(PROGS): % : src/%.o
	"$(CC)" $(CFLAGS) -o $@ $<

$(OBJS): % : src/%.c
	"$(CC)" $(CFLAGS) -c -o $@ src/$@.c

.PHONY: clean
clean:
	rm -rf *.o
