SRCDIR = src
LIBDIR = lib
CC = gcc
CFLAGS = $(shell pkg-config --cflags gtk+-3.0) -Wall
GTKLIBS = $(shell pkg-config --libs gtk+-3.0)
EXE = dmd5620
CSRC = $(wildcard src/*.c)
OBJ = $(CSRC:.c=.o)
LDFLAGS = $(GTKLIBS) -lm -lpthread -lc -ldl
CORELIB = $(LIBDIR)/libdmd_core.a

ifeq ($(PREFIX),)
PREFIX := /usr/local
endif

ifdef DEBUG
CFLAGS+ = -g -O0
else
CFLAGS += -O3 -Os -s
endif

.PHONY: all clean

all: $(EXE)

clean:
	@rm -f $(EXE) $(OBJ)

$(EXE): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(CORELIB) $(LDFLAGS)

install: $(EXE)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(EXE) $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps
	install -d $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps
	install -m 644 assets/dmd5620.png $(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps
	install -m 644 assets/dmd5620.svg $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(EXE)
	rm -f $(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps/dmd5620.png
	rm -f $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/dmd5620.svg
