CC      = cc
AR      = ar
CFLAGS  = -std=c11 -Wall -Wextra -fPIC -Iinclude -Isrc
LDFLAGS =

# Version. Keep in sync with SPARCLI_VERSION_* in include/sparcli_export.h.
VERSION_MAJOR := 0
VERSION_MINOR := 1
VERSION_PATCH := 0
VERSION       := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

# Install paths (override on the command line, e.g. `make install PREFIX=/opt/sparcli`).
PREFIX        ?= /usr/local
DESTDIR       ?=
LIBDIR        ?= $(PREFIX)/lib
INCLUDEDIR    ?= $(PREFIX)/include
PKGCONFIGDIR  ?= $(LIBDIR)/pkgconfig

# Platform-specific shared library naming.
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    SHLIB         := libsparcli.$(VERSION).dylib
    SHLIB_SONAME  := libsparcli.$(VERSION_MAJOR).dylib
    SHLIB_LINK    := libsparcli.dylib
    SHLIB_LDFLAGS := -dynamiclib \
                     -install_name $(LIBDIR)/$(SHLIB_SONAME) \
                     -compatibility_version $(VERSION_MAJOR) \
                     -current_version $(VERSION)
else
    SHLIB         := libsparcli.so.$(VERSION)
    SHLIB_SONAME  := libsparcli.so.$(VERSION_MAJOR)
    SHLIB_LINK    := libsparcli.so
    SHLIB_LDFLAGS := -shared -Wl,-soname,$(SHLIB_SONAME)
endif

# ── Core (foundation: color, text, print, output stream, render) ──────────
SRC     = src/core/output.c src/core/version.c src/core/text_attributes.c \
          src/core/print.c src/core/color.c src/core/text.c src/core/render_wrap.c \
          \
          src/output/panel.c \
          src/output/table/table.c \
          src/output/table/table_print.c \
          src/output/table/table_print_init.c \
          src/output/table/table_print_render.c \
          src/output/table/table_print_render_cell.c \
          src/output/table/table_print_render_border.c \
          src/output/table/table_print_render_row.c \
          src/output/columns.c src/output/rule.c src/output/tree.c src/output/list.c \
          src/output/progressbar.c src/output/spinner.c src/output/kv.c src/output/alert.c \
          src/output/badge.c src/output/util.c src/output/pad.c src/output/markup.c \
          \
          src/tty/term.c src/tty/key.c src/tty/screen.c \
          \
          src/input/prompt.c src/input/line_editor.c src/input/confirm.c \
          src/input/text_input.c src/input/password_input.c src/input/select.c \
          src/input/fuzzy.c src/input/datepicker.c
BUILDDIR          = build.nosync
OBJ               = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SRC))
LIB               = libsparcli.a
PC_FILE           = sparcli.pc

# Sanitizer build artefacts live in a separate tree so plain `make` and
# `make sanitize` never share .o files or a .a — mixing them produces undefined
# ASAN/UBSAN symbols at link time.
SANITIZE_BUILDDIR = build.sanitize.nosync
SANITIZE_OBJ      = $(patsubst src/%.c,$(SANITIZE_BUILDDIR)/%.o,$(SRC))
SANITIZE_LIB      = libsparcli-sanitize.a
SANITIZE_TEST_BIN = tests/test_main_sanitize
SANITIZE_FLAGS    = -fsanitize=address,undefined -fno-omit-frame-pointer -g -O1

# ── Output test suite (tests/output/) — automatic, non-interactive ────────
TEST_SRC = tests/output/test_main.c \
           tests/output/test_text_attributes.c \
           tests/output/test_colors.c \
           tests/output/test_columns_basic.c \
           tests/output/test_panels.c \
           tests/output/test_tables.c \
           tests/output/test_columns.c \
           tests/output/test_rules.c \
           tests/output/test_trees.c \
           tests/output/test_lists.c \
           tests/output/test_progressbar.c \
           tests/output/test_spinner.c \
           tests/output/test_kv.c \
           tests/output/test_alert.c \
           tests/output/test_badge.c \
           tests/output/test_util.c \
           tests/output/test_pad.c \
           tests/output/test_align.c \
           tests/output/test_markup.c
TEST_BIN = tests/output/test_main

# ── Input test suite (tests/input/) — interactive, needs a real TTY ───────
INPUT_TEST_SRC = tests/input/test_input_main.c \
                 tests/input/test_confirm.c \
                 tests/input/test_text_input.c \
                 tests/input/test_password_input.c \
                 tests/input/test_select.c \
                 tests/input/test_fuzzy.c \
                 tests/input/test_datepicker.c \
                 tests/input/test_line_editor.c \
                 tests/input/test_key_decode.c
INPUT_TEST_BIN = tests/input/test_input_main

# Example programs: each examples/*.c compiles to a binary in EXAMPLES_BUILDDIR.
EXAMPLES_BUILDDIR = build.examples.nosync
EXAMPLES_SRC      = $(wildcard examples/*.c)
EXAMPLES_BIN      = $(patsubst examples/%.c,$(EXAMPLES_BUILDDIR)/%,$(EXAMPLES_SRC))

HEADERS = $(shell find include -name '*.h')

.PHONY: all test test-input clean install uninstall sanitize pkgconfig shared examples run-example

all: $(LIB) $(SHLIB) $(PC_FILE)

shared: $(SHLIB)
pkgconfig: $(PC_FILE)

$(LIB): $(OBJ)
	$(AR) rcs $@ $^

$(SHLIB): $(OBJ)
	$(CC) $(SHLIB_LDFLAGS) $(LDFLAGS) -o $@ $^

$(PC_FILE): sparcli.pc.in Makefile
	sed -e 's|@PREFIX@|$(PREFIX)|g' \
	    -e 's|@LIBDIR@|$(LIBDIR)|g' \
	    -e 's|@INCLUDEDIR@|$(INCLUDEDIR)|g' \
	    -e 's|@VERSION@|$(VERSION)|g' \
	    $< > $@

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Test executable is linked against the static .a so it never depends on the
# install path / dyld search order of the shared library.
test: $(LIB)
	$(CC) $(CFLAGS) $(TEST_SRC) $(LIB) $(LDFLAGS) -o $(TEST_BIN)
	./$(TEST_BIN) $(ARGS)

# Interactive input-widget test runner. Needs a real TTY, so it is a separate
# target and is NOT part of `make test`. Run it directly in a terminal.
test-input: $(LIB)
	$(CC) $(CFLAGS) $(INPUT_TEST_SRC) $(LIB) $(LDFLAGS) -o $(INPUT_TEST_BIN)
	./$(INPUT_TEST_BIN) $(ARGS)

# Build & run the test suite with AddressSanitizer + UndefinedBehaviorSanitizer.
# Uses a separate build tree and a separate .a so it never contaminates the
# normal build artefacts.
sanitize: $(SANITIZE_LIB)
	$(CC) $(CFLAGS) $(SANITIZE_FLAGS) $(TEST_SRC) $(SANITIZE_LIB) $(LDFLAGS) $(SANITIZE_FLAGS) -o $(SANITIZE_TEST_BIN)
	./$(SANITIZE_TEST_BIN) $(ARGS)

$(SANITIZE_LIB): $(SANITIZE_OBJ)
	$(AR) rcs $@ $^

$(SANITIZE_BUILDDIR)/%.o: src/%.c | $(SANITIZE_BUILDDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SANITIZE_FLAGS) -c $< -o $@

$(SANITIZE_BUILDDIR):
	mkdir -p $(SANITIZE_BUILDDIR)

# Build all example programs into EXAMPLES_BUILDDIR. Linked against the static
# .a, same as the test binary, so they don't depend on the install path.
examples: $(EXAMPLES_BIN)

$(EXAMPLES_BUILDDIR)/%: examples/%.c $(LIB) | $(EXAMPLES_BUILDDIR)
	$(CC) $(CFLAGS) $< $(LIB) $(LDFLAGS) -o $@

$(EXAMPLES_BUILDDIR):
	mkdir -p $(EXAMPLES_BUILDDIR)

# Build & run a single example: `make run-example EX=readme_screenshots`.
run-example: $(EXAMPLES_BUILDDIR)/$(EX)
	./$(EXAMPLES_BUILDDIR)/$(EX)

install: $(LIB) $(SHLIB) $(PC_FILE)
	install -d "$(DESTDIR)$(LIBDIR)"
	install -d "$(DESTDIR)$(INCLUDEDIR)"
	install -d "$(DESTDIR)$(PKGCONFIGDIR)"
	install -m 644 $(LIB) "$(DESTDIR)$(LIBDIR)/"
	install -m 755 $(SHLIB) "$(DESTDIR)$(LIBDIR)/"
	cd "$(DESTDIR)$(LIBDIR)" && \
	    ln -sf $(SHLIB) $(SHLIB_SONAME) && \
	    ln -sf $(SHLIB_SONAME) $(SHLIB_LINK)
	for h in $(HEADERS); do \
	    rel=$${h#include/}; \
	    install -d "$(DESTDIR)$(INCLUDEDIR)/$$(dirname $$rel)"; \
	    install -m 644 $$h "$(DESTDIR)$(INCLUDEDIR)/$$rel"; \
	done
	install -m 644 $(PC_FILE) "$(DESTDIR)$(PKGCONFIGDIR)/"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/$(LIB)"
	rm -f "$(DESTDIR)$(LIBDIR)/$(SHLIB)"
	rm -f "$(DESTDIR)$(LIBDIR)/$(SHLIB_SONAME)"
	rm -f "$(DESTDIR)$(LIBDIR)/$(SHLIB_LINK)"
	rm -f "$(DESTDIR)$(PKGCONFIGDIR)/$(PC_FILE)"
	for h in $(HEADERS); do rm -f "$(DESTDIR)$(INCLUDEDIR)/$${h#include/}"; done

clean:
	rm -rf $(BUILDDIR) $(SANITIZE_BUILDDIR) $(EXAMPLES_BUILDDIR) \
	       $(LIB) $(SANITIZE_LIB) \
	       libsparcli.*.dylib libsparcli.so* libsparcli.dylib \
	       $(PC_FILE) $(TEST_BIN) $(INPUT_TEST_BIN) $(SANITIZE_TEST_BIN)
