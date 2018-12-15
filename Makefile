SRCDIR = src
LIBDIR = lib
CC = gcc
CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTKLIBS = $(shell pkg-config --libs gtk+-3.0)
EXE = dmd
CSRC = $(wildcard src/*.c)
OBJ = $(CSRC:.c=.o)
RUSTLIB = $(LIBDIR)/target/release/libdmd_bindings.a
LDFLAGS = $(GTKLIBS) -lm

.PHONY: all clean

all: $(EXE)

clean:
	@rm -f $(EXE) $(OBJ)
	cd lib; cargo clean

$(EXE): $(RUSTLIB) $(OBJ)
	$(CC) -o $@ $^ $(RUSTLIB) $(LDFLAGS)

$(RUSTLIB):
	cd lib; cargo build --release
