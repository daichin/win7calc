# Native (C + GTK3) build of the Windows 7 Calculator replica.
#   make            -> ./win7calc
#   make check      -> run the core unit tests
#   make install   -> ~/.local/bin/win7calc + app-menu entry (PREFIX=/usr/local for all users)
# Needs: gcc, pkg-config, GTK 3 and libmpdec headers
#   Debian/Ubuntu: libgtk-3-dev libmpdec-dev    Arch/Omarchy: gtk3 mpdecimal
PKG_CONFIG ?= pkg-config
CC ?= cc
CFLAGS ?= -O2 -g
CFLAGS += -std=gnu11 -Wall -Wextra -Wno-unused-parameter
MPDEC_CFLAGS ?=
# Link libmpdec statically when libmpdec.a exists (Debian), else dynamically (Arch).
ifeq ($(shell $(CC) -print-file-name=libmpdec.a),libmpdec.a)
MPDEC_LIBS ?= -lmpdec
else
MPDEC_LIBS ?= -l:libmpdec.a
endif
PREFIX ?= $(HOME)/.local
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk+-3.0)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk+-3.0)

CORE_SRC := src/mathfn.c src/numfmt.c src/engine.c src/units.c src/datecalc.c src/worksheets.c
UI_SRC := src/main.c src/app.c src/layout.c src/render.c src/view.c src/panels.c
CORE_OBJ := $(CORE_SRC:src/%.c=build/%.o)
UI_OBJ := $(UI_SRC:src/%.c=build/ui_%.o)

all: win7calc

win7calc: $(CORE_OBJ) $(UI_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(GTK_LIBS) $(MPDEC_LIBS) -lm

build/%.o: src/%.c $(wildcard src/*.h) | build
	$(CC) $(CFLAGS) $(MPDEC_CFLAGS) -c -o $@ $<

build/ui_%.o: src/%.c $(wildcard src/*.h) | build
	$(CC) $(CFLAGS) $(MPDEC_CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

build/test_core: tests/test_core.c $(CORE_OBJ)
	$(CC) $(CFLAGS) $(MPDEC_CFLAGS) -Isrc $(LDFLAGS) -o $@ $^ $(MPDEC_LIBS) -lm

check: build/test_core
	./build/test_core

build:
	mkdir -p build

install: win7calc
	install -Dm755 win7calc $(DESTDIR)$(PREFIX)/bin/win7calc
	sed 's|^Exec=.*|Exec=$(PREFIX)/bin/win7calc|' ../win7calc.desktop > win7calc.desktop.tmp
	install -Dm644 win7calc.desktop.tmp $(DESTDIR)$(PREFIX)/share/applications/win7calc.desktop
	rm -f win7calc.desktop.tmp

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/win7calc $(DESTDIR)$(PREFIX)/share/applications/win7calc.desktop

clean:
	rm -rf build win7calc

.PHONY: all check clean install uninstall
