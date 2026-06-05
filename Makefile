CC      = cc
CXX     = c++
AR      = ar
# EXTRA_CFLAGS is an overridable hook for extra flags (e.g. CI uses
# `make EXTRA_CFLAGS=-Werror` to treat warnings as errors). It is applied to
# both the C and the C++ (wrapper) builds.
# Binary hardening (stack canaries + fortified libc calls). _FORTIFY_SOURCE
# needs optimization; the sanitizer builds undefine it again because fortified
# functions bypass the ASan/TSan interceptors.
# -U before -D: ASan/UBSan builds pre-define _FORTIFY_SOURCE=0 as a compiler
# built-in, so a bare -D would trigger -Wmacro-redefined (-Werror fails).
HARDEN_CFLAGS = -O2 -fstack-protector-strong -U_FORTIFY_SOURCE \
                -D_FORTIFY_SOURCE=2

CFLAGS  = -std=c11 -Wall -Wextra -Wshadow -Wformat=2 -Wnull-dereference \
          -Wcast-align -Wconversion -Wsign-conversion -fPIC $(HARDEN_CFLAGS) \
          -Iinclude -Isrc $(EXTRA_CFLAGS)
CXXFLAGS = -std=c++20 -Wall -Wextra $(HARDEN_CFLAGS) -Iinclude $(EXTRA_CFLAGS)
LDFLAGS = $(HARDEN_LDFLAGS)

# Version. Keep in sync with SPARCLI_VERSION_* in include/sparcli_export.h.
VERSION_MAJOR := 0
VERSION_MINOR := 1
VERSION_PATCH := 0
VERSION       := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

# Install paths (override on the command line, e.g. `make install PREFIX=/opt/sparcli`).
PREFIX        ?= /usr/local
DESTDIR       ?=
LIBDIR        ?= $(PREFIX)/lib
BINDIR        ?= $(PREFIX)/bin
INCLUDEDIR    ?= $(PREFIX)/include
PKGCONFIGDIR  ?= $(LIBDIR)/pkgconfig
ZSHFUNCDIR    ?= $(PREFIX)/share/zsh/site-functions

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
    HARDEN_LDFLAGS :=            # PIE is the default on macOS; RELRO is ELF-only
else
    SHLIB         := libsparcli.so.$(VERSION)
    SHLIB_SONAME  := libsparcli.so.$(VERSION_MAJOR)
    SHLIB_LINK    := libsparcli.so
    SHLIB_LDFLAGS := -shared -Wl,-soname,$(SHLIB_SONAME)
    PTY_LDLIBS    := -lutil      # forkpty
    PTHREAD_LDLIBS := -lpthread
    HARDEN_LDFLAGS := -Wl,-z,relro,-z,now -Wl,-z,noexecstack
endif

# ── Core (foundation: color, text, print, output stream, render) ──────────
SRC     = src/core/output.c src/core/version.c src/core/text_attributes.c \
          src/core/print.c src/core/color.c src/core/text.c src/core/render_wrap.c \
          src/core/sanitize.c \
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
          src/output/pager.c src/output/live.c \
          \
          src/tty/term.c src/tty/key.c src/tty/screen.c \
          \
          src/input/prompt.c src/input/line_editor.c src/input/theme.c \
          src/input/shortcut.c src/input/editor.c src/input/confirm.c \
          src/input/text_input.c src/input/password_input.c src/input/number_input.c \
          src/input/calc.c \
          src/input/textarea.c \
          src/input/select.c src/input/fuzzy.c src/input/datepicker.c \
          src/input/history.c \
          \
          src/app/paths.c src/app/error.c \
          \
          src/log/log.c \
          \
          src/args/args.c src/args/args_value.c src/args/args_suggest.c \
          src/args/args_parse.c src/args/args_help.c src/args/args_complete.c \
          src/args/args_split.c
BUILDDIR          = build.nosync
OBJ               = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SRC))
DEP               = $(OBJ:.o=.d)
LIB               = libsparcli.a
PC_FILE           = sparcli.pc

# ── Command-line tool (cli/) ───────────────────────────────────────────────
# The `sparcli` binary exposes the output and input widgets as shell
# subcommands (see docs/cli.md). It is built from cli/*.c against the static
# library using only the public headers (-Iinclude, deliberately no -Isrc).
# A second, sanitizer-instrumented binary backs the CLI PTY test suite.
CLI_SRC          = cli/main.c cli/cli_common.c cli/cli_parse.c \
                   cli/cli_stdin.c cli/cli_csv.c cli/cmd_output.c \
                   cli/cmd_layout.c cli/cmd_table.c cli/cmd_tree.c \
                   cli/cmd_progress.c cli/cmd_input.c cli/cmd_select.c
CLI_OBJ          = $(patsubst cli/%.c,$(BUILDDIR)/cli/%.o,$(CLI_SRC))
CLI_DEP          = $(CLI_OBJ:.o=.d)
CLI_BIN          = sparcli
CLI_CFLAGS       = -std=c11 -Wall -Wextra -Wshadow -Wformat=2 \
                   -Wnull-dereference -Wcast-align -Wconversion \
                   -Wsign-conversion -Iinclude $(EXTRA_CFLAGS)
CLI_SANITIZE_OBJ = $(patsubst cli/%.c,$(SANITIZE_BUILDDIR)/cli/%.o,$(CLI_SRC))
CLI_SANITIZE_DEP = $(CLI_SANITIZE_OBJ:.o=.d)
CLI_SANITIZE_BIN = sparcli-sanitize

# Sanitizer build artefacts live in a separate tree so plain `make` and
# `make sanitize` never share .o files or a .a - mixing them produces undefined
# ASAN/UBSAN symbols at link time.
SANITIZE_BUILDDIR = build.sanitize.nosync
SANITIZE_OBJ      = $(patsubst src/%.c,$(SANITIZE_BUILDDIR)/%.o,$(SRC))
SANITIZE_DEP      = $(SANITIZE_OBJ:.o=.d)
SANITIZE_LIB      = libsparcli-sanitize.a
SANITIZE_TEST_BIN = tests/output/test_main_sanitize
SANITIZE_FLAGS    = -fsanitize=address,undefined -fno-omit-frame-pointer \
                    -g -O1 -U_FORTIFY_SOURCE

# ThreadSanitizer build artefacts (TSan cannot be combined with ASan, so it
# gets its own tree, .a and flags).
TSAN_BUILDDIR = build.tsan.nosync
TSAN_OBJ      = $(patsubst src/%.c,$(TSAN_BUILDDIR)/%.o,$(SRC))
TSAN_DEP      = $(TSAN_OBJ:.o=.d)
TSAN_LIB      = libsparcli-tsan.a
TSAN_TEST_BIN = tests/input/logic/test_input_main_tsan
TSAN_FLAGS    = -fsanitize=thread -fno-omit-frame-pointer -g -O1 \
                -U_FORTIFY_SOURCE

# Fuzz harnesses: portable random-input fuzzers built under ASan/UBSan (no
# libFuzzer dependency; the harness entry points are libFuzzer-compatible).
FUZZ_SRC   = tests/fuzz/fuzz_markup.c tests/fuzz/fuzz_key.c \
             tests/fuzz/fuzz_sanitize.c tests/fuzz/fuzz_csv.c \
             tests/fuzz/fuzz_args.c tests/fuzz/fuzz_calc.c \
             tests/fuzz/fuzz_split.c
FUZZ_BIN   = $(patsubst tests/fuzz/%.c,$(BUILDDIR)/fuzz/%,$(FUZZ_SRC))
FUZZ_ITERS ?= 200000
FUZZ_SEED  ?= 1

# ── Output test suite (tests/output/) - automatic, non-interactive ────────
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
           tests/output/test_live.c \
           tests/output/test_kv.c \
           tests/output/test_alert.c \
           tests/output/test_badge.c \
           tests/output/test_util.c \
           tests/output/test_pad.c \
           tests/output/test_align.c \
           tests/output/test_markup.c \
           tests/output/test_links.c \
           tests/output/test_errors.c
TEST_BIN = tests/output/test_main

# ── Input logic + widget suite (tests/input/logic/) - interactive ─────────
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
                 tests/input/logic/test_shortcut.c \
                 tests/input/logic/test_select_edit.c \
                 tests/input/logic/test_opts_copy.c \
                 tests/input/logic/test_filters.c \
                 tests/input/logic/test_calc.c \
                 tests/input/logic/test_sanitize.c \
                 tests/input/logic/test_suggest.c \
                 tests/input/logic/test_history.c \
                 tests/input/logic/test_no_tty.c \
                 tests/input/logic/test_threads.c
INPUT_TEST_BIN = tests/input/logic/test_input_main

# ── Input style snapshot suite (tests/input/style/) - non-interactive ─────
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

# ── Application-framework suite (tests/app/) - non-interactive ────────────
# Pure logic tests for the app helpers (XDG paths, pager); safe in CI.
APP_TEST_SRC = tests/app/test_app_main.c \
               tests/app/test_paths.c \
               tests/app/test_pager.c \
               tests/app/test_errors.c \
               tests/app/test_log.c
APP_TEST_BIN = tests/app/test_app_main

# ── Argument-parser suite (tests/args/) - non-interactive ─────────────────
# Parse logic, error reporting, help/completion rendering; safe in CI.
ARGS_TEST_SRC = tests/args/test_args_main.c \
                tests/args/test_args_parse.c \
                tests/args/test_args_errors.c \
                tests/args/test_args_help.c \
                tests/args/test_args_split.c
ARGS_TEST_BIN = tests/args/test_args_main

# Example programs (see docs/examples.md). The tree is grouped by language:
# examples/c/ and examples/cpp/ compile to binaries in EXAMPLES_BUILDDIR
# (mirroring the directory structure), examples/rust/ builds through cargo
# ([[example]] entries in bindings/rust/sparcli/Cargo.toml), examples/python/
# runs against the cffi package. The readme_screenshots_*.c generators stay at
# the examples/ root.
EXAMPLES_BUILDDIR = build.examples.nosync
EXAMPLES_SRC      = $(shell find examples -name '*.c')
EXAMPLES_BIN      = $(patsubst examples/%.c,$(EXAMPLES_BUILDDIR)/%,$(EXAMPLES_SRC))
EXAMPLES_CPP_SRC  = $(shell find examples -name '*.cpp')
EXAMPLES_CPP_BIN  = $(patsubst examples/%.cpp,$(EXAMPLES_BUILDDIR)/%,$(EXAMPLES_CPP_SRC))

# Public headers: the C headers plus the header-only C++ wrapper (sparcli.hpp).
HEADERS = $(shell find include \( -name '*.h' -o -name '*.hpp' \))

.PHONY: all cli test qa test-output test-output-check test-output-golden test-input test-input-style test-input-style-check test-input-style-golden test-input-pty test-app test-args test-cli-check test-cli-golden test-cli-completion test-cli-pty test-cpp test-cpp-golden clean install uninstall sanitize tsan fuzz lint pkgconfig shared examples run-example rust rust-test rust-lint python python-test python-test-debug rebuild-all

# `make` must build the C library + CLI (as documented), not the first target
# that happens to be defined above (the rust/python binding helpers).
.DEFAULT_GOAL := all

# ── Rust binding (bindings/rust/) ─────────────────────────────────────────
# A two-crate cargo workspace (sparcli-sys + sparcli). build.rs compiles the C
# sources via the `cc` crate, so these need only a Rust toolchain (cargo), no
# prior `make`/install. Kept out of `make test` since cargo may be absent.
RUST_MANIFEST = bindings/rust/Cargo.toml

rust:
	cargo build --manifest-path $(RUST_MANIFEST)

# SPARCLI_NO_TTY=1: the smoke tests exercise the "no TTY" error paths. Without
# it, running from an interactive terminal would let prompts open /dev/tty,
# hijack the keyboard and hang the suite (cargo only redirects stdin/stdout;
# the controlling terminal stays reachable).
rust-test:
	SPARCLI_NO_TTY=1 cargo test --manifest-path $(RUST_MANIFEST)

# Rust static analysis: clippy over the whole workspace (lib + tests +
# examples), warnings are errors. Skipped with an install hint when the
# clippy component is missing (same pattern as `make lint` for the C tools).
rust-lint:
	@if cargo clippy --version >/dev/null 2>&1; then \
	    echo "── cargo clippy ──"; \
	    cargo clippy --manifest-path $(RUST_MANIFEST) --all-targets \
	        -- -D warnings; \
	else \
	    echo "clippy not installed: rustup component add clippy"; \
	fi

# ── Python binding (bindings/python/) ─────────────────────────────────────
# A cffi (API-mode) wrapper. build_sparcli.py compiles the vendored C sources
# (reached via the csrc/cinclude symlinks) into src/sparcli/_sparcli_cffi.*, so
# `python build_sparcli.py` builds in place and the tests/examples run with
# PYTHONPATH=src - no install needed. Needs Python + cffi; kept out of
# `make test`. For a real install: `pip install --no-build-isolation bindings/python`.
PY      ?= python3
PY_DIR   = bindings/python

# The previously installed extension is unlinked first: cffi copies the fresh
# build over the old file in place (same inode), which leaves macOS with a
# stale code-signature cache for that inode and gets any process loading it
# SIGKILLed ("Killed: 9"). Unlinking forces a new inode + fresh signature.
python:
	rm -f $(PY_DIR)/src/sparcli/_sparcli_cffi*.so
	cd $(PY_DIR) && $(PY) build_sparcli.py

# SPARCLI_NO_TTY=1 for the same reason as rust-test (also set in conftest.py
# as a fallback for direct `pytest` runs).
python-test: python
	cd $(PY_DIR) && PYTHONPATH=src SPARCLI_NO_TTY=1 $(PY) -m pytest tests -q

# Poisoned-memory run: freed memory is filled with a marker byte, so any
# use-after-free in the C layer breaks assertions instead of silently working
# (a dangling pointer usually still holds the old bytes until the allocator
# reuses them - this makes the corruption deterministic). PYTHONMALLOC=malloc
# routes Python/cffi allocations through libc malloc so the poisoning applies
# to the cffi buffers too. MallocScribble/MallocPreScribble = macOS,
# MALLOC_PERTURB_ = glibc; the foreign one is silently ignored.
python-test-debug: python
	cd $(PY_DIR) && PYTHONPATH=src SPARCLI_NO_TTY=1 PYTHONMALLOC=malloc \
	    MallocScribble=1 MallocPreScribble=1 MALLOC_PERTURB_=85 \
	    $(PY) -m pytest tests -q

# ── Rebuild everything (C lib + install + Rust + Python) in one go ─────────
# The C++ wrapper is header-only (nothing to compile on its own); it is picked
# up by consumers and refreshed through `install`. Needs cargo + a cffi-capable
# Python; override the latter with `make rebuild-all PY=/path/to/python`.
rebuild-all:
	$(MAKE) all
	$(MAKE) install
	$(MAKE) rust
	$(MAKE) python

all: $(LIB) $(SHLIB) $(PC_FILE) $(CLI_BIN)

shared: $(SHLIB)
pkgconfig: $(PC_FILE)
cli: $(CLI_BIN)

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

# ── CLI binary ──────────────────────────────────────────────────────────────
$(CLI_BIN): $(CLI_OBJ) $(LIB)
	$(CC) $(CLI_OBJ) $(LIB) $(LDFLAGS) $(PTHREAD_LDLIBS) -o $@

$(BUILDDIR)/cli/%.o: cli/%.c | $(BUILDDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CLI_CFLAGS) -MMD -MP -c $< -o $@

# Sanitizer-instrumented CLI: same sources against the sanitizer .a, used by
# the CLI PTY test suite so CLI code paths get ASan/UBSan coverage too.
$(CLI_SANITIZE_BIN): $(CLI_SANITIZE_OBJ) $(SANITIZE_LIB)
	$(CC) $(CLI_SANITIZE_OBJ) $(SANITIZE_LIB) $(LDFLAGS) $(SANITIZE_FLAGS) \
	    $(PTHREAD_LDLIBS) -o $@

$(SANITIZE_BUILDDIR)/cli/%.o: cli/%.c | $(SANITIZE_BUILDDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CLI_CFLAGS) $(SANITIZE_FLAGS) -MMD -MP -c $< -o $@

# Full non-interactive test suite: every headless gate. This is the canonical
# "is everything OK?" command (also what CI would run). The interactive widget
# suite (`make test-input`) needs a real TTY and is intentionally NOT included.
# Command-line overrides (e.g. EXTRA_CFLAGS=-Werror) propagate to each sub-make.
test:
	$(MAKE) test-output-check
	$(MAKE) test-input ARGS=--logic
	$(MAKE) test-input-style-check
	$(MAKE) test-input-pty
	$(MAKE) test-app
	$(MAKE) test-args
	$(MAKE) test-cpp
	$(MAKE) test-cli-check
	$(MAKE) test-cli-completion
	$(MAKE) test-cli-pty

# Full QA run: every gate in order, stops at the first failure. This is the
# complete pre-commit validation in one command - the headless test suite
# (warnings as errors), the memory/thread sanitizers, static analysis (C +
# Rust clippy), parser fuzzing, and both language bindings including the
# poisoned-memory FFI gate.
# See docs/DEVELOPMENT.md "After a change - run these".
qa:
	$(MAKE) test EXTRA_CFLAGS=-Werror
	$(MAKE) sanitize
	$(MAKE) tsan
	$(MAKE) lint
	$(MAKE) fuzz
	$(MAKE) rust-test
	$(MAKE) rust-lint
	$(MAKE) python-test
	$(MAKE) python-test-debug
	@echo "\033[32m✔ All QA gates passed.\033[0m"

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

# Application-framework logic suite (XDG paths, pager). Pure logic with real
# child processes (cat/false as pagers); no TTY required, safe in CI.
test-app: $(LIB)
	$(CC) $(CFLAGS) $(APP_TEST_SRC) $(LIB) $(LDFLAGS) -o $(APP_TEST_BIN)
	./$(APP_TEST_BIN)

# Argument-parser logic suite: parse loop, typed values, error reporting,
# did-you-mean, help + completion rendering. No TTY required, safe in CI.
test-args: $(LIB)
	$(CC) $(CFLAGS) $(ARGS_TEST_SRC) $(LIB) $(LDFLAGS) -o $(ARGS_TEST_BIN)
	./$(ARGS_TEST_BIN)

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

# CLI output golden-file gate: runs every non-interactive CLI subcommand with
# fixed arguments/fixtures and diffs the bytes against a committed snapshot.
# Deterministic off a TTY (width falls back to 80) plus explicit --width flags.
CLI_GOLDEN       = tests/cli/expected.txt
CLI_PTY_TEST_SRC = tests/cli/test_cli_pty.c
CLI_PTY_TEST_BIN = tests/cli/test_cli_pty

test-cli-check: $(CLI_BIN) | $(BUILDDIR)
	sh tests/cli/run_output.sh ./$(CLI_BIN) > $(BUILDDIR)/cli.actual.txt
	diff -u $(CLI_GOLDEN) $(BUILDDIR)/cli.actual.txt

# Regenerate the CLI golden file after an intentional rendering change.
test-cli-golden: $(CLI_BIN)
	sh tests/cli/run_output.sh ./$(CLI_BIN) > $(CLI_GOLDEN)
	@echo "Regenerated $(CLI_GOLDEN)"

# Anti-drift: assert the hand-written zsh completion offers every long option
# each `sparcli <cmd> --help` documents (the CLI is not on the args parser yet).
test-cli-completion: $(CLI_BIN)
	sh tests/cli/test_completion.sh ./$(CLI_BIN)

# CLI PTY suite: drives the interactive CLI subcommands (confirm, input,
# select, ...) on a pseudo-terminal and asserts stdout value + exit code.
# Both the harness and the CLI child run under ASan/UBSan.
test-cli-pty: $(CLI_SANITIZE_BIN)
	$(CC) $(CFLAGS) $(SANITIZE_FLAGS) $(CLI_PTY_TEST_SRC) \
	    $(LDFLAGS) $(SANITIZE_FLAGS) $(PTY_LDLIBS) -o $(CLI_PTY_TEST_BIN)
	./$(CLI_PTY_TEST_BIN) ./$(CLI_SANITIZE_BIN) $(ARGS)

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
	$(CXX) $(CXXFLAGS) tests/cpp/cpp_demo.cpp $(LIB) $(LDFLAGS) -o $(CPP_DEMO_BIN)
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
	$(CXX) $(CXXFLAGS) tests/cpp/cpp_demo.cpp $(LIB) $(LDFLAGS) -o $(CPP_DEMO_BIN)
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

# Build & run the input logic suite (incl. test_threads) under ThreadSanitizer.
# Verifies the documented invariant that concurrent rendering/capturing on
# independent streams is race-free (thread-local output target).
tsan: $(TSAN_LIB)
	$(CC) $(CFLAGS) $(TSAN_FLAGS) $(INPUT_TEST_SRC) $(TSAN_LIB) \
	    $(LDFLAGS) $(TSAN_FLAGS) $(PTHREAD_LDLIBS) -o $(TSAN_TEST_BIN)
	./$(TSAN_TEST_BIN) --logic

$(TSAN_LIB): $(TSAN_OBJ)
	$(AR) rcs $@ $^

$(TSAN_BUILDDIR)/%.o: src/%.c | $(TSAN_BUILDDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(TSAN_FLAGS) -MMD -MP -c $< -o $@

$(TSAN_BUILDDIR):
	mkdir -p $(TSAN_BUILDDIR)

# Random-input fuzzing of the external parsers (markup, key decoder, ANSI
# sanitizer, CLI CSV parser) under ASan/UBSan. Deterministic by default (FUZZ_SEED=1); override
# iterations/seed with `make fuzz FUZZ_ITERS=1000000 FUZZ_SEED=42`. The
# harnesses also expose a libFuzzer-compatible LLVMFuzzerTestOneInput for
# toolchains that ship libFuzzer (build with -DSPARCLI_LIBFUZZER
# -fsanitize=fuzzer,address there).
fuzz: $(SANITIZE_LIB) | $(BUILDDIR)
	@mkdir -p $(BUILDDIR)/fuzz
	$(CC) $(CFLAGS) $(SANITIZE_FLAGS) tests/fuzz/fuzz_markup.c $(SANITIZE_LIB) \
	    $(LDFLAGS) $(SANITIZE_FLAGS) -o $(BUILDDIR)/fuzz/fuzz_markup
	$(CC) $(CFLAGS) $(SANITIZE_FLAGS) tests/fuzz/fuzz_key.c $(SANITIZE_LIB) \
	    $(LDFLAGS) $(SANITIZE_FLAGS) -o $(BUILDDIR)/fuzz/fuzz_key
	$(CC) $(CFLAGS) $(SANITIZE_FLAGS) tests/fuzz/fuzz_sanitize.c $(SANITIZE_LIB) \
	    $(LDFLAGS) $(SANITIZE_FLAGS) -o $(BUILDDIR)/fuzz/fuzz_sanitize
	$(CC) $(CFLAGS) $(SANITIZE_FLAGS) -Icli tests/fuzz/fuzz_csv.c \
	    cli/cli_csv.c $(LDFLAGS) $(SANITIZE_FLAGS) -o $(BUILDDIR)/fuzz/fuzz_csv
	$(CC) $(CFLAGS) $(SANITIZE_FLAGS) tests/fuzz/fuzz_args.c $(SANITIZE_LIB) \
	    $(LDFLAGS) $(SANITIZE_FLAGS) -o $(BUILDDIR)/fuzz/fuzz_args
	$(CC) $(CFLAGS) $(SANITIZE_FLAGS) tests/fuzz/fuzz_calc.c $(SANITIZE_LIB) \
	    $(LDFLAGS) $(SANITIZE_FLAGS) -o $(BUILDDIR)/fuzz/fuzz_calc
	$(CC) $(CFLAGS) $(SANITIZE_FLAGS) tests/fuzz/fuzz_split.c $(SANITIZE_LIB) \
	    $(LDFLAGS) $(SANITIZE_FLAGS) -o $(BUILDDIR)/fuzz/fuzz_split
	./$(BUILDDIR)/fuzz/fuzz_markup $(FUZZ_ITERS) $(FUZZ_SEED)
	./$(BUILDDIR)/fuzz/fuzz_key $(FUZZ_ITERS) $(FUZZ_SEED)
	./$(BUILDDIR)/fuzz/fuzz_sanitize $(FUZZ_ITERS) $(FUZZ_SEED)
	./$(BUILDDIR)/fuzz/fuzz_csv $(FUZZ_ITERS) $(FUZZ_SEED)
	./$(BUILDDIR)/fuzz/fuzz_args $(FUZZ_ITERS) $(FUZZ_SEED)
	./$(BUILDDIR)/fuzz/fuzz_calc $(FUZZ_ITERS) $(FUZZ_SEED)
	./$(BUILDDIR)/fuzz/fuzz_split $(FUZZ_ITERS) $(FUZZ_SEED)

# Static analysis. Each tool is optional: the target degrades to an install
# hint when a tool is missing, so it can run on any machine.
# Warnings are errors (--warnings-as-errors='*'), consistent with the C build
# (-Werror in qa) and clippy (-D warnings). If an LLVM upgrade introduces new
# checks that misfire, disable them in .clang-tidy instead of relaxing this.
LLVM_TIDY = $(shell command -v clang-tidy 2>/dev/null \
            || ls /opt/homebrew/opt/llvm/bin/clang-tidy 2>/dev/null)
lint:
	@if command -v cppcheck >/dev/null 2>&1; then \
	    echo "── cppcheck ──"; \
	    cppcheck --enable=warning,performance,portability --std=c11 \
	        --inline-suppr --quiet --error-exitcode=1 \
	        -Iinclude -Isrc src/ cli/; \
	else \
	    echo "cppcheck not installed: brew install cppcheck"; \
	fi
	@if [ -n "$(LLVM_TIDY)" ]; then \
	    echo "── clang-tidy ──"; \
	    $(LLVM_TIDY) --quiet --warnings-as-errors='*' $(SRC) -- $(CFLAGS); \
	    $(LLVM_TIDY) --quiet --warnings-as-errors='*' $(CLI_SRC) \
	        -- $(CLI_CFLAGS); \
	else \
	    echo "clang-tidy not installed: brew install llvm"; \
	fi

# Build all C and C++ example programs into EXAMPLES_BUILDDIR. Linked against
# the static .a, same as the test binary, so they don't depend on the install
# path. Rust/Python examples are built/run on demand via `run-example`.
examples: $(EXAMPLES_BIN) $(EXAMPLES_CPP_BIN)

$(EXAMPLES_BUILDDIR)/%: examples/%.c $(LIB) | $(EXAMPLES_BUILDDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< $(LIB) $(LDFLAGS) -o $@

# The C++ examples are built at C++23 (for std::print) - one step above the
# C++20 the wrapper itself requires, so they show the modern call style.
EXAMPLES_CXXFLAGS = -std=c++23 -Wall -Wextra $(HARDEN_CFLAGS) -Iinclude \
                    $(EXTRA_CFLAGS)
$(EXAMPLES_BUILDDIR)/%: examples/%.cpp $(LIB) | $(EXAMPLES_BUILDDIR)
	@mkdir -p $(dir $@)
	$(CXX) $(EXAMPLES_CXXFLAGS) $< $(LIB) $(LDFLAGS) -o $@

$(EXAMPLES_BUILDDIR):
	mkdir -p $(EXAMPLES_BUILDDIR)

# Build & run a single example: `make run-example EX=<language>/<group>/<name>`
# e.g. EX=c/output/panel_alert, EX=cpp/input/fuzzy, EX=rust/output/table_basic,
# EX=python/app/paths. Dispatches on the language prefix: c/ and cpp/ compile
# against the static library, rust/ goes through cargo (example name = path
# with slashes turned into underscores), python/ runs the script against the
# cffi package (built first when missing).
run-example:
	@case "$(EX)" in \
	    rust/*) \
	        name=$$(echo "$(EX)" | sed -e 's|^rust/||' -e 's|/|_|g'); \
	        cargo run --manifest-path $(RUST_MANIFEST) -p sparcli \
	            --example $$name ;; \
	    python/*) \
	        ls $(PY_DIR)/src/sparcli/_sparcli_cffi*.so >/dev/null 2>&1 \
	            || $(MAKE) python; \
	        PYTHONPATH=$(PY_DIR)/src $(PY) examples/$(EX).py ;; \
	    *) \
	        $(MAKE) $(EXAMPLES_BUILDDIR)/$(EX); \
	        ./$(EXAMPLES_BUILDDIR)/$(EX) ;; \
	esac

install: $(LIB) $(SHLIB) $(PC_FILE) $(CLI_BIN)
	install -d "$(DESTDIR)$(LIBDIR)"
	install -d "$(DESTDIR)$(INCLUDEDIR)"
	install -d "$(DESTDIR)$(PKGCONFIGDIR)"
	install -d "$(DESTDIR)$(BINDIR)"
	install -d "$(DESTDIR)$(ZSHFUNCDIR)"
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
	install -m 755 $(CLI_BIN) "$(DESTDIR)$(BINDIR)/"
	install -m 644 completions/_sparcli "$(DESTDIR)$(ZSHFUNCDIR)/"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/$(LIB)"
	rm -f "$(DESTDIR)$(LIBDIR)/$(SHLIB)"
	rm -f "$(DESTDIR)$(LIBDIR)/$(SHLIB_SONAME)"
	rm -f "$(DESTDIR)$(LIBDIR)/$(SHLIB_LINK)"
	rm -f "$(DESTDIR)$(PKGCONFIGDIR)/$(PC_FILE)"
	rm -f "$(DESTDIR)$(BINDIR)/$(CLI_BIN)"
	rm -f "$(DESTDIR)$(ZSHFUNCDIR)/_sparcli"
	for h in $(HEADERS); do rm -f "$(DESTDIR)$(INCLUDEDIR)/$${h#include/}"; done

clean:
	rm -rf $(BUILDDIR) $(SANITIZE_BUILDDIR) $(TSAN_BUILDDIR) \
	       $(EXAMPLES_BUILDDIR) \
	       $(LIB) $(SANITIZE_LIB) $(TSAN_LIB) \
	       libsparcli.*.dylib libsparcli.so* libsparcli.dylib \
	       $(PC_FILE) $(TEST_BIN) $(INPUT_TEST_BIN) $(STYLE_TEST_BIN) \
	       $(PTY_TEST_BIN) $(APP_TEST_BIN) $(ARGS_TEST_BIN) \
	       $(SANITIZE_TEST_BIN) $(TSAN_TEST_BIN) \
	       $(CLI_BIN) $(CLI_SANITIZE_BIN) $(CLI_PTY_TEST_BIN)

# Auto-generated header dependencies (-MMD -MP). The leading '-' silences the
# "no such file" notice on the first build, before any .d exists.
-include $(DEP) $(SANITIZE_DEP) $(TSAN_DEP) $(CLI_DEP) $(CLI_SANITIZE_DEP)
