# Set compiler to gcc
CC = gcc

# Set flags
CFLAGS = -Wall -g

PROGS = chip8
OBJS = $(addsuffix .o, $(FILES))
LDLIBS = -lSDL2 -lm

all: $(PROGS)

$(PROGS): % : src/%.o
	"$(CC)" $(CFLAGS) -o $@ $< $(LDLIBS)

$(OBJS): % : src/%.c
	"$(CC)" $(CFLAGS) -c -o $@ src/$@.c

.PHONY: clean
clean:
	rm -rf *.o
