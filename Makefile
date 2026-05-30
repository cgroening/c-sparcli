CC      = cc
CXX     = c++
AR      = ar
# EXTRA_CFLAGS is an overridable hook for extra flags (e.g. CI uses
# `make EXTRA_CFLAGS=-Werror` to treat warnings as errors). It is applied to
# both the C and the C++ (wrapper) builds.
CFLAGS  = -std=c11 -Wall -Wextra -fPIC -Iinclude -Isrc $(EXTRA_CFLAGS)
CXXFLAGS = -std=c++20 -Wall -Wextra -Iinclude $(EXTRA_CFLAGS)
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
    PTY_LDLIBS    :=             # forkpty lives in libSystem on macOS
    PTHREAD_LDLIBS :=            # pthreads in libSystem on macOS
else
    SHLIB         := libsparcli.so.$(VERSION)
    SHLIB_SONAME  := libsparcli.so.$(VERSION_MAJOR)
    SHLIB_LINK    := libsparcli.so
    SHLIB_LDFLAGS := -shared -Wl,-soname,$(SHLIB_SONAME)
    PTY_LDLIBS    := -lutil      # forkpty
    PTHREAD_LDLIBS := -lpthread
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
          src/input/prompt.c src/input/line_editor.c src/input/theme.c src/input/confirm.c \
          src/input/text_input.c src/input/password_input.c src/input/number_input.c \
          src/input/textarea.c \
          src/input/select.c src/input/fuzzy.c src/input/datepicker.c
BUILDDIR          = build.nosync
OBJ               = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SRC))
DEP               = $(OBJ:.o=.d)
LIB               = libsparcli.a
PC_FILE           = sparcli.pc

# Sanitizer build artefacts live in a separate tree so plain `make` and
# `make sanitize` never share .o files or a .a – mixing them produces undefined
# ASAN/UBSAN symbols at link time.
SANITIZE_BUILDDIR = build.sanitize.nosync
SANITIZE_OBJ      = $(patsubst src/%.c,$(SANITIZE_BUILDDIR)/%.o,$(SRC))
SANITIZE_DEP      = $(SANITIZE_OBJ:.o=.d)
SANITIZE_LIB      = libsparcli-sanitize.a
SANITIZE_TEST_BIN = tests/output/test_main_sanitize
SANITIZE_FLAGS    = -fsanitize=address,undefined -fno-omit-frame-pointer -g -O1

# ── Output test suite (tests/output/) – automatic, non-interactive ────────
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

# ── Input logic + widget suite (tests/input/logic/) – interactive ─────────
# (`ARGS=--logic` runs only the non-interactive logic tests, suitable for CI.)
INPUT_TEST_SRC = tests/input/logic/test_input_main.c \
                 tests/input/logic/test_confirm.c \
                 tests/input/logic/test_text_input.c \
                 tests/input/logic/test_password_input.c \
                 tests/input/logic/test_number.c \
                 tests/input/logic/test_textarea.c \
                 tests/input/logic/test_select.c \
                 tests/input/logic/test_fuzzy.c \
                 tests/input/logic/test_datepicker.c \
                 tests/input/logic/test_theme.c \
                 tests/input/logic/test_line_editor.c \
                 tests/input/logic/test_key_decode.c \
                 tests/input/logic/test_filters.c \
                 tests/input/logic/test_threads.c
INPUT_TEST_BIN = tests/input/logic/test_input_main

# ── Input style snapshot suite (tests/input/style/) – non-interactive ─────
STYLE_TEST_SRC = tests/input/style/test_style_main.c \
                 tests/input/style/test_style_confirm.c \
                 tests/input/style/test_style_text.c \
                 tests/input/style/test_style_number.c \
                 tests/input/style/test_style_textarea.c \
                 tests/input/style/test_style_select.c \
                 tests/input/style/test_style_fuzzy.c \
                 tests/input/style/test_style_datepicker.c
STYLE_TEST_BIN = tests/input/style/test_style_main

# ── Self-driving PTY suite: runs the interactive widgets under a pseudo-
# terminal (no human needed). Built with the sanitizers so it also gives
# AddressSanitizer/UBSan coverage of the interactive code paths.
PTY_TEST_SRC = tests/input/pty/test_pty.c
PTY_TEST_BIN = tests/input/pty/test_pty

# Example programs: each examples/*.c compiles to a binary in EXAMPLES_BUILDDIR.
EXAMPLES_BUILDDIR = build.examples.nosync
EXAMPLES_SRC      = $(wildcard examples/*.c)
EXAMPLES_BIN      = $(patsubst examples/%.c,$(EXAMPLES_BUILDDIR)/%,$(EXAMPLES_SRC))

# Public headers: the C headers plus the header-only C++ wrapper (sparcli.hpp).
HEADERS = $(shell find include \( -name '*.h' -o -name '*.hpp' \))

.PHONY: all test test-output test-output-check test-output-golden test-input test-input-style test-input-style-check test-input-style-golden test-input-pty test-cpp test-cpp-golden clean install uninstall sanitize pkgconfig shared examples run-example

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
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Full non-interactive test suite: every headless gate. This is the canonical
# "is everything OK?" command (also what CI would run). The interactive widget
# suite (`make test-input`) needs a real TTY and is intentionally NOT included.
# Command-line overrides (e.g. EXTRA_CFLAGS=-Werror) propagate to each sub-make.
test:
	$(MAKE) test-output-check
	$(MAKE) test-input ARGS=--logic
	$(MAKE) test-input-style-check
	$(MAKE) test-input-pty
	$(MAKE) test-cpp

# Visual output gallery: builds and runs the output suite for eyeballing.
# ARGS: --no-animated (skip animations), --focus (focused subset), or both.
# Linked against the static .a so it never depends on the install path / dyld
# search order of the shared library.
test-output: $(LIB)
	$(CC) $(CFLAGS) $(TEST_SRC) $(LIB) $(LDFLAGS) -o $(TEST_BIN)
	./$(TEST_BIN) $(ARGS)

# Golden-file regression test for the output suite. The output is deterministic
# off a TTY (sc_terminal_width() falls back to 80) with --no-animated, so the
# rendered bytes can be diffed against a committed snapshot.
OUTPUT_GOLDEN = tests/output/expected.txt

test-output-check: $(LIB) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(TEST_SRC) $(LIB) $(LDFLAGS) -o $(TEST_BIN)
	./$(TEST_BIN) --no-animated > $(BUILDDIR)/output.actual.txt
	diff -u $(OUTPUT_GOLDEN) $(BUILDDIR)/output.actual.txt

# Regenerate the output golden file after an intentional rendering change.
test-output-golden: $(LIB)
	$(CC) $(CFLAGS) $(TEST_SRC) $(LIB) $(LDFLAGS) -o $(TEST_BIN)
	./$(TEST_BIN) --no-animated > $(OUTPUT_GOLDEN)
	@echo "Regenerated $(OUTPUT_GOLDEN)"

# Interactive input-widget test runner. Needs a real TTY, so it is a separate
# target and is NOT part of `make test`. Run it directly in a terminal.
test-input: $(LIB)
	$(CC) $(CFLAGS) $(INPUT_TEST_SRC) $(LIB) $(LDFLAGS) $(PTHREAD_LDLIBS) -o $(INPUT_TEST_BIN)
	./$(INPUT_TEST_BIN) $(ARGS)

# Non-interactive style snapshot runner: renders every widget in many styles.
# Safe to run anywhere (no TTY required).
test-input-style: $(LIB)
	$(CC) $(CFLAGS) $(STYLE_TEST_SRC) $(LIB) $(LDFLAGS) -o $(STYLE_TEST_BIN)
	./$(STYLE_TEST_BIN) $(ARGS)

# Golden-file regression test for the input style snapshots. Non-interactive
# (the frame builders render without a TTY), so the bytes are deterministic.
STYLE_GOLDEN = tests/input/style/expected.txt

test-input-style-check: $(LIB) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(STYLE_TEST_SRC) $(LIB) $(LDFLAGS) -o $(STYLE_TEST_BIN)
	./$(STYLE_TEST_BIN) > $(BUILDDIR)/style.actual.txt
	diff -u $(STYLE_GOLDEN) $(BUILDDIR)/style.actual.txt

# Regenerate the style golden file after an intentional rendering change.
test-input-style-golden: $(LIB)
	$(CC) $(CFLAGS) $(STYLE_TEST_SRC) $(LIB) $(LDFLAGS) -o $(STYLE_TEST_BIN)
	./$(STYLE_TEST_BIN) > $(STYLE_GOLDEN)
	@echo "Regenerated $(STYLE_GOLDEN)"

# Self-driving PTY suite under the sanitizers: exercises the interactive widgets
# end-to-end (raw mode, key decoding, redraw) with no human and reports leaks /
# UB. Linked against the sanitizer .a like `make sanitize`.
test-input-pty: $(SANITIZE_LIB)
	$(CC) $(CFLAGS) $(SANITIZE_FLAGS) $(PTY_TEST_SRC) $(SANITIZE_LIB) \
	    $(LDFLAGS) $(SANITIZE_FLAGS) $(PTY_LDLIBS) -o $(PTY_TEST_BIN)
	./$(PTY_TEST_BIN) $(ARGS)

# C++ wrapper gate (needs a C++ compiler). Three checks:
#  1. the example compiles cleanly and its output matches a golden snapshot;
#  2. the assertion suite (ownership / move-safety / C parity) passes, built
#     under AddressSanitizer + UBSan so arena / RAII memory bugs are caught;
#  3. a self-driving PTY test exercises the interactive string prompts
#     (text_input / password_input / textarea) and asserts the returned value,
#     guarding against the out-param sequencing bug those wrappers once had.
CPP_DEMO_BIN     = $(EXAMPLES_BUILDDIR)/cpp_demo
CPP_TEST_BIN     = $(EXAMPLES_BUILDDIR)/test_cpp
CPP_PTY_TEST_BIN = $(EXAMPLES_BUILDDIR)/test_cpp_pty
CPP_GOLDEN   = tests/cpp/expected.txt
test-cpp: $(LIB) $(SANITIZE_LIB) | $(EXAMPLES_BUILDDIR)
	$(CXX) $(CXXFLAGS) examples/cpp_demo.cpp $(LIB) $(LDFLAGS) -o $(CPP_DEMO_BIN)
	SPARCLI_DEMO_NONINTERACTIVE=1 ./$(CPP_DEMO_BIN) </dev/null 2>/dev/null \
	    > $(EXAMPLES_BUILDDIR)/cpp.actual.txt
	diff -u $(CPP_GOLDEN) $(EXAMPLES_BUILDDIR)/cpp.actual.txt
	$(CXX) $(CXXFLAGS) $(SANITIZE_FLAGS) tests/cpp/test_cpp.cpp $(SANITIZE_LIB) \
	    $(LDFLAGS) $(SANITIZE_FLAGS) -o $(CPP_TEST_BIN)
	./$(CPP_TEST_BIN)
	$(CXX) $(CXXFLAGS) $(SANITIZE_FLAGS) tests/cpp/test_cpp_pty.cpp \
	    $(SANITIZE_LIB) $(LDFLAGS) $(SANITIZE_FLAGS) $(PTY_LDLIBS) \
	    -o $(CPP_PTY_TEST_BIN)
	./$(CPP_PTY_TEST_BIN)

# Regenerate the C++ demo golden after an intentional rendering change.
test-cpp-golden: $(LIB) | $(EXAMPLES_BUILDDIR)
	$(CXX) $(CXXFLAGS) examples/cpp_demo.cpp $(LIB) $(LDFLAGS) -o $(CPP_DEMO_BIN)
	SPARCLI_DEMO_NONINTERACTIVE=1 ./$(CPP_DEMO_BIN) </dev/null 2>/dev/null \
	    > $(CPP_GOLDEN)
	@echo "Regenerated $(CPP_GOLDEN)"

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
	$(CC) $(CFLAGS) $(SANITIZE_FLAGS) -MMD -MP -c $< -o $@

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
	       $(PC_FILE) $(TEST_BIN) $(INPUT_TEST_BIN) $(STYLE_TEST_BIN) \
	       $(PTY_TEST_BIN) $(SANITIZE_TEST_BIN)

# Auto-generated header dependencies (-MMD -MP). The leading '-' silences the
# "no such file" notice on the first build, before any .d exists.
-include $(DEP) $(SANITIZE_DEP)
