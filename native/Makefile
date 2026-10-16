# Native (C + GTK3) build of the Windows 7 Calculator replica.
#   make            -> ./win7calc
#   make check      -> run the core unit tests
# Needs: gcc, pkg-config, libgtk-3-dev, libmpdec-dev (linked statically).
PKG_CONFIG ?= pkg-config
CC ?= cc
CFLAGS ?= -O2 -g
CFLAGS += -std=gnu11 -Wall -Wextra -Wno-unused-parameter
MPDEC_CFLAGS ?=
MPDEC_LIBS ?= -l:libmpdec.a
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

clean:
	rm -rf build win7calc

.PHONY: all check clean
