SRCDIR = src
LIBDIR = dmd_core
CC = gcc
GIT = git
CARGO = cargo
CFLAGS = $(shell pkg-config --cflags gtk+-3.0) -Wall -std=gnu99
GTKLIBS = $(shell pkg-config --libs gtk+-3.0)
EXE = dmd5620
CSRC = $(wildcard src/*.c)
OBJ = $(CSRC:.c=.o)
LDFLAGS = $(GTKLIBS) -lm -lpthread -lc -ldl -lutil
CORELIB = $(LIBDIR)/target/release/libdmd_core.a

ifeq ($(PREFIX),)
PREFIX := /usr/local
endif

ifdef DEBUG
	CFLAGS+ = -g -O0
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		CFLAGS += -O3 -Os
	else
		CFLAGS += -O3 -Os -s
	endif
endif

.PHONY: all clean

all: $(EXE)

clean:
	@rm -f $(EXE) $(OBJ)
	@cd $(LIBDIR) && $(CARGO) clean

$(CORELIB):
	$(if $(wildcard ./dmd_core/Cargo.toml),,$(error The submodule dmd_core is not checked out.))
	@cd $(LIBDIR) && $(CARGO) build --release

$(EXE): $(CORELIB) $(OBJ)
	@$(CC) $(CFLAGS) -o $@ $^ $(CORELIB) $(LDFLAGS)

install: $(EXE)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(EXE) $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/man/man1
	install -m 644 dmd5620.man $(DESTDIR)$(PREFIX)/man/man1/dmd5620.1
	install -d $(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps
	install -d $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps
	install -m 644 assets/dmd5620.png $(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps
	install -m 644 assets/dmd5620.svg $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(EXE)
	rm -f $(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps/dmd5620.png
	rm -f $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/dmd5620.svg
	rm -f $(DESTDIR)$(PREFIX)/man/man1/dmd5620.1
