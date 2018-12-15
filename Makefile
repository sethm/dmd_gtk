SRCDIR = src
LIBDIR = lib
CC = gcc
CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
LDLIBS = $(shell pkg-config --libs gtk+-3.0)
EXE = dmd

CSRC = $(wildcard src/*.c)
OBJ = $(CSRC:.c=.o)

LDFLAGS = $(LDLIBS) -lm

.PHONY: all clean

all: $(EXE)

clean:
	@rm -f $(EXE) $(OBJ)

$(EXE): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)
