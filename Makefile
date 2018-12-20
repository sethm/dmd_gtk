SRCDIR = src
LIBDIR = lib
CC = gcc
CFLAGS = $(shell pkg-config --cflags gtk+-3.0) -Wall
GTKLIBS = $(shell pkg-config --libs gtk+-3.0)
EXE = dmd5620
CSRC = $(wildcard src/*.c)
OBJ = $(CSRC:.c=.o)
LDFLAGS = $(GTKLIBS) -lm -lpthread -lc -ldl
CARGOFLAGS =

ifdef DEBUG
CFLAGS += -g -O0
RUSTLIB = $(LIBDIR)/target/debug/libdmd_bindings.a
else
CFLAGS += -O3 -Os -s
CARGOFLAGS += --release
RUSTLIB = $(LIBDIR)/target/release/libdmd_bindings.a
endif

.PHONY: all clean

all: $(EXE)

clean:
	@rm -f $(EXE) $(OBJ)
	cd lib; cargo clean

$(EXE): $(RUSTLIB) $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(RUSTLIB) $(LDFLAGS)

$(RUSTLIB):
	cd lib; cargo build $(CARGOFLAGS)
