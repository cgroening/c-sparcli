# sparcli – Developer Guide

Build, test, and install workflow for working on sparcli itself. For the public API see [`API.md`](API.md) (the hub linking the [C](api-c.md) and [C++](api-cpp.md) references); for the command-line tool see [`cli.md`](cli.md); for architecture and house conventions see [`../CLAUDE.md`](../CLAUDE.md).

**Requirements:** a C11 compiler (`cc`, clang, or gcc), `make`, and a POSIX system. The interactive PTY test suite uses `forkpty`.

---

## Build

```sh
make                 # libsparcli.a + shared lib (.dylib/.so) + sparcli.pc + the sparcli CLI
make shared          # only the shared library
make pkgconfig       # only sparcli.pc
make cli             # only the sparcli command-line tool (see docs/cli.md)
make clean           # remove all build trees, libs, test binaries

make rust            # build the Rust binding   (see "make rust" below)
make python          # build the Python binding (see "make python" below)
make rebuild-all     # C lib + install + Rust + Python in one command
```

Compiler flags are `-std=c11 -Wall -Wextra`. Treat warnings as errors with the `EXTRA_CFLAGS` hook (recommended before committing, and how CI-style checks are run):

```sh
make EXTRA_CFLAGS=-Werror
```

`EXTRA_CFLAGS` takes any extra flags, so it also serves debug builds: `make EXTRA_CFLAGS='-g -O0'`.

**Good to know:**

- **Header-dependency tracking** is automatic (`-MMD -MP`): editing a header rebuilds every dependent object on the next `make` – you do **not** need `make clean` after changing a header.
- The **sanitizer build** uses a separate build tree and a separate `.a`, so it never contaminates the normal artefacts.
- Tests link against the **static** `libsparcli.a`, so running them never depends on the install path or the dynamic-loader search order.
- **Linux and macOS** are both supported; the shared-library naming (`.so`/`.dylib` + soname) and the `-lutil` link for `forkpty` (PTY suite) are selected automatically by the Makefile.

---

## Tests

The suites split along the output/input boundary. Everything except `make test-input` (which needs a real terminal) runs headless.

### `make test` – full non-interactive suite

The canonical "is everything OK?" command. Runs all headless gates in order – `test-output-check`, `test-input ARGS=--logic`, `test-input-style-check`, `test-input-pty`, `test-cpp`, `test-cli-check`, `test-cli-pty` – and fails if any does. Command-line overrides propagate, so `make test EXTRA_CFLAGS=-Werror` builds every gate with warnings as errors. The interactive widget suite (`make test-input`) needs a real TTY and is **not** included. The individual targets it chains are documented below.

```sh
make test                     # run every headless gate
make test EXTRA_CFLAGS=-Werror
```

### `make qa` – every QA gate in one command

The complete pre-commit validation. Runs, in order and stopping at the first failure:

1. `make test EXTRA_CFLAGS=-Werror` – build + every headless test gate, warnings as errors
2. `make sanitize` – output suite under ASan/UBSan
3. `make tsan` – logic suite under ThreadSanitizer
4. `make lint` – cppcheck + clang-tidy
5. `make fuzz` – random-input fuzzing of all external parsers
6. `make rust-test` – Rust binding (rebuilds the C sources via `cc`)
7. `make python-test` – Python binding (rebuilds the cffi extension)
8. `make python-test-debug` – Python suite with poisoned freed memory (FFI lifetime gate)

```sh
make qa                       # the one command to run after any change
```

Run this before every commit; the individual targets are documented below for running a single gate during development.

### `make test-output` – output gallery

The output renderer suite in `tests/output/`, printed to stdout for visual inspection (the golden-file gate below is the automated check). Covers: text attributes, colors, panels, alerts, tables, rules, trees, lists, progress bar, spinner, key-value pairs, badges, utilities, padding, alignment, markup, and columns (basic + advanced).

```sh
make test-output                              # all output tests
make test-output ARGS=--no-animated           # skip the spinner/progress animations
make test-output ARGS=--focus                 # run only the focused subset
make test-output ARGS="--focus --no-animated" # combine flags
```

The two *animated* cases (progress bar, spinner) are skipped by `--no-animated`.

### `make test-output-check` – output golden-file gate

Runs the output suite with `--no-animated` and diffs it against `tests/output/expected.txt`. Fails on any rendering drift. This is the automated correctness gate for the output side.

```sh
make test-output-check     # diff against the committed snapshot
make test-output-golden    # regenerate the snapshot (after an intended change)
```

### `make test-input` – interactive widget suite

Drives every input widget for real in `tests/input/logic/`. **Needs a real TTY** – run it in a terminal, not a pipe.

```sh
make test-input                # interactive: confirm, text, password, number,
                               # textarea, select, fuzzy, datepicker, theme
make test-input ARGS=--logic   # non-interactive logic checks only (CI-safe)
```

`ARGS=--logic` skips the interactive widgets and runs only the pure-logic checks – key decoder, the shortcut chord matcher, the select label-edit API, the line editor, character filters, and the thread-safety test – and exits non-zero if any assertion fails.

### `make test-input-style` – style snapshots

Renders every input widget in many styles via the internal frame builders, with no TTY and no keystrokes. Safe anywhere.

```sh
make test-input-style          # print all style snapshots
make test-input-style-check    # diff against tests/input/style/expected.txt
make test-input-style-golden   # regenerate the snapshot
```

### `make test-input-pty` – self-driving PTY suite (ASan/UBSan)

Forks each interactive widget onto a pseudo-terminal, feeds it a canned keystroke script, and asserts the returned value – no human needed. Built with AddressSanitizer + UndefinedBehaviorSanitizer, so it also gives sanitizer coverage of the interactive code paths. Covers confirm, text, number, select, textarea, and datepicker, plus custom shortcuts (return/callback/remove), Esc-cancel, and the external-editor flow (driven by a stub editor).

```sh
make test-input-pty
```

### `make test-cpp` – C++ wrapper gate

Four checks for the header-only C++20 wrapper (`include/sparcli.hpp`); needs a C++ compiler (`$(CXX)`, default `c++`):

1. **Compile** `examples/cpp_demo.cpp` (so the example never bit-rots).
2. **Golden diff** the demo's output against `tests/cpp/expected.txt`.
3. **Assertion suite** `tests/cpp/test_cpp.cpp` – verifies the wrapper's ownership/lifetime guarantees (table built from temporaries, `Table` survives a move, List/Tree rich-text arenas) and that it renders like the C API. Built under **AddressSanitizer + UBSan** so arena/RAII/move memory bugs are caught.
4. **PTY regression** `tests/cpp/test_cpp_pty.cpp` – drives the interactive string prompts (`text_input` / `password_input` / `textarea`) over a pseudo-terminal and asserts the returned value, so the out-param sequencing those wrappers once got wrong stays fixed. Also under ASan/UBSan.

```sh
make test-cpp                    # all three checks
make test-cpp EXTRA_CFLAGS=-Werror
make test-cpp-golden             # regenerate tests/cpp/expected.txt
```

### `make test-cli-check` – CLI output golden-file gate

Runs `tests/cli/run_output.sh`, which invokes the `sparcli` binary for every non-interactive subcommand (print, panel, rule, table, list, tree, kv, alert, badge, columns, plus help texts and error cases) with fixed arguments and the fixtures in `tests/cli/fixtures/`, and diffs the bytes against `tests/cli/expected.txt`. Deterministic because stdout is a pipe (terminal width falls back to 80) and layout-sensitive cases pass an explicit `--width`. The animated commands (`spin`, `progress`) are excluded here and covered by the PTY gate below.

```sh
make test-cli-check     # diff against the committed snapshot
make test-cli-golden    # regenerate the snapshot (after an intended change)
```

### `make test-cli-pty` – CLI input PTY suite (ASan/UBSan)

Drives the interactive CLI subcommands (confirm, input, password, number, textarea, select, fuzzy, date) on a pseudo-terminal: `tests/cli/test_cli_pty.c` forks a sanitizer-instrumented CLI binary (`sparcli-sanitize`), redirects its stdout onto a pipe (exactly like shell command substitution), feeds canned keystrokes through the PTY, and asserts both the value on stdout and the exit code. Also covers the no-TTY error paths (exit 2) and `spin`'s exit-code propagation. Both the harness and the CLI child run under AddressSanitizer + UBSan.

```sh
make test-cli-pty
```

### `make rust` / `make rust-test` – Rust bindings

The safe Rust crate lives in `bindings/rust/` (a cargo workspace: `sparcli-sys` + `sparcli`). `sparcli-sys/build.rs` compiles the C sources with the `cc` crate, so these targets need only a Rust toolchain (`cargo`) – no prior `make` or install. Kept out of `make test` since cargo may be absent.

```sh
make rust          # cargo build (compiles the C via cc, links the FFI)
make rust-test     # cargo test: non-interactive integration tests + doctests
cargo clippy --manifest-path bindings/rust/Cargo.toml --all-targets -- -D warnings
cargo build --manifest-path bindings/rust/Cargo.toml --features regen  # bindgen path (libclang)

# Examples (the workspace has no bin, so plain `cargo run` fails – pick one).
# From the repo root:
cargo run --manifest-path bindings/rust/Cargo.toml -p sparcli --example demo
# Or from inside bindings/rust/ :
cargo run -p sparcli --example demo
#   `demo` = complete showcase (all output components + all input widgets);
#   `output_gallery` / `input_demo` are the focused variants.
```

FFI bindings are committed (`bindings/rust/sparcli-sys/src/bindings.rs`); the `regen` feature regenerates them from the headers with bindgen. The reference is [`docs/api-rust.md`](api-rust.md).

### `make python` / `make python-test` – Python bindings

The Pythonic `sparcli` package lives in `bindings/python/` (a cffi API-mode wrapper). `build_sparcli.py` compiles the vendored C sources – reached through the in-tree `csrc`/`cinclude` symlinks (→ `../../src`, `../../include`) – into `src/sparcli/_sparcli_cffi.*`, so `make python` builds in place and the tests and examples run with `PYTHONPATH=src`, no install required. Needs Python with `cffi`; kept out of `make test` since neither may be present.

```sh
make python                       # build the extension in place
make python-test                  # build + run the non-interactive pytest suite
make python-test-debug            # same suite with poisoned freed memory (see below)
make python-test PY=/path/to/python   # point make at an interpreter that has cffi

# Examples (after `make python`, from bindings/python/):
PYTHONPATH=src python examples/demo.py            # complete showcase (all widgets)
PYTHONPATH=src python examples/output_gallery.py  # output only
PYTHONPATH=src python examples/input_demo.py      # input only (needs a terminal)
```

To install into an environment instead – an editable (`-e`) install with build isolation off, because the C sources are reached via the in-repo symlinks and the build must run in place:

```sh
pip install cffi setuptools wheel
pip install --no-build-isolation -e bindings/python
```

The `cdef` in `build_sparcli.py` uses cffi's partial-struct (`...;`) syntax, so the compiler fills in exact struct layout from the headers – a real mismatch fails the build. The reference is [`docs/api-python.md`](api-python.md).

### `make python-test-debug` – Python suite with poisoned freed memory

Runs the same pytest suite, but with every freed allocation overwritten by a marker byte (`PYTHONMALLOC=malloc` + macOS `MallocScribble`/glibc `MALLOC_PERTURB_`). A use-after-free in the C layer – e.g. a borrowed string whose cffi buffer was garbage-collected – then shows up as garbled output and a failing assertion instead of silently "working" because the old bytes happen to still be there. **Run this after any change to the bindings or to string-lifetime handling.**

This is the gate that would have caught the Select/Fuzzy opts use-after-free found via numcli: tests with string literals can never dangle, and ASan only reports memory that actually gets freed – poisoning makes the dangling read deterministic.

### `make sanitize` – output suite under ASan/UBSan

Rebuilds and runs the output suite with AddressSanitizer + UBSan in a separate build tree.

```sh
make sanitize ARGS=--no-animated
```

> The style and PTY runners take no `ARGS`. The thread-safety test runs as part of `make test-input ARGS=--logic`.

### `make tsan` – logic suite under ThreadSanitizer

Rebuilds the library with `-fsanitize=thread` (own build tree, since TSan and ASan cannot be combined) and runs the non-interactive logic suite, including the thread-safety test. This verifies the documented invariant that concurrent rendering/capturing on independent streams is race-free, instead of just asserting it.

```sh
make tsan
```

### `make lint` – static analysis (cppcheck + clang-tidy)

Runs cppcheck and clang-tidy (configured by the repo-root `.clang-tidy`; conservative bug-finding checks, noisy style checks disabled with documented reasons) over `src/`. Each tool is optional – the target prints an install hint (`brew install cppcheck` / `brew install llvm`) when one is missing.

```sh
make lint
```

### `make fuzz` – random-input fuzzing of the parsers (ASan/UBSan)

Feeds the four external parsers – the inline-markup parser (`sc_markup_parse`), the key decoder (`sc_key_decode`), the ANSI sanitizer (`sc_sanitize_copy` / `sc_strip_ansi`) and the CLI's CSV parser (`sc_cli_csv_parse`) – with pseudo-random byte sequences under ASan/UBSan. The sanitizer harness additionally asserts its output contract (no ESC/control bytes when disallowed, output never longer than the input). Deterministic by default; tune with `FUZZ_ITERS` / `FUZZ_SEED`. The harnesses in `tests/fuzz/` expose a libFuzzer-compatible `LLVMFuzzerTestOneInput`, so they can also run under a real libFuzzer toolchain (`-DSPARCLI_LIBFUZZER -fsanitize=fuzzer,address`; Apple clang does not ship libFuzzer).

```sh
make fuzz                              # 200k inputs per parser, seed 1
make fuzz FUZZ_ITERS=1000000 FUZZ_SEED=42
```

### Golden-file workflow

`test-output-check`, `test-input-style-check`, `test-cli-check` and the C++ `test-cpp` gate diff the rendered bytes against committed snapshots (deterministic because, off a TTY, the terminal width falls back to 80). When you change rendering **on purpose**:

1. regenerate with `make test-output-golden`, `make test-input-style-golden`, `make test-cli-golden` and/or `make test-cpp-golden`,
2. review the diff to confirm the change is what you intended,
3. commit the updated `expected.txt`.

### After a change – run these

Copy-paste block to validate a change.

```sh
# 1. The full QA run: every gate in order (= the pre-commit validation).
#    Equivalent to running, in order: make test EXTRA_CFLAGS=-Werror,
#    make sanitize, make tsan, make lint, make fuzz, make rust-test,
#    make python-test, make python-test-debug.
make qa

# 2. If you changed rendering on purpose, regenerate + review + commit the golden:
make test-output-golden
make test-input-style-golden
make test-cli-golden                   # only if you changed the CLI's output
make test-cpp-golden                   # only if you changed the C++ wrapper

# 3. Examples still build (and try the input widgets by hand):
make examples                          # compile every examples/*.c
make run-example EX=readme_screenshots_output # output widget gallery (static)
make run-example EX=readme_screenshots_input  # input widget gallery (static)
make run-example EX=input_demo         # interactive walkthrough of all input widgets
make run-example EX=shortcut_demo      # custom shortcuts: F2 rename, Ctrl-X delete
./examples/cli_output_demo.zsh         # CLI output commands demo (zsh)
./examples/cli_input_demo.zsh          # CLI input commands demo (zsh, interactive)

# 4. Interactive widget suite (needs a real terminal):
make test-input
```

### Rebuilding the bindings & consumers after a C-library change

The C++/Rust/Python bindings and any external consumer each carry their **own** copy of the compiled library (the Rust and Python wrappers compile the C sources themselves; pkg-config consumers link the installed `.a`/`.so`). None of them pick up a `src/` or `include/` change automatically – after editing the C core, rebuild each one you use. All commands are relative to the **project root**.

**All at once:** `make rebuild-all` runs steps 1–3 below in order (C library + `install` + Rust + Python); the C++ wrapper is header-only and needs no rebuild. It needs `cargo` and a cffi-capable Python – point at one with `make rebuild-all PY=/path/to/python`. `install` defaults to `/usr/local` (needs `sudo`); pass a prefix for a user-local install, e.g. `make rebuild-all PREFIX=$HOME/.local`. Run the individual steps instead when you only touched one binding.

```sh
# 1. Core C library + (re)install – needed by pkg-config consumers, the C/C++
#    headers, and any external project that links the installed lib.
make                                   # rebuild libsparcli.a + .so + .pc
make install                           # reinstall (user-local; or `sudo make install`)

# 2. Rust binding – build.rs recompiles the C via cc (no install needed).
cargo build --manifest-path bindings/rust/Cargo.toml
cargo test  --manifest-path bindings/rust/Cargo.toml
#    If a struct / signature / enum changed, regenerate the committed bindgen
#    output (needs libclang) and commit it:
cargo build --manifest-path bindings/rust/Cargo.toml --features regen
#    -> commit bindings/rust/sparcli-sys/src/bindings.rs

# 3. Python binding – rebuild the cffi extension (it is NOT rebuilt on import).
make python                            # build in place into src/sparcli/
make python-test                       # build + headless pytest
#    An editable install does not auto-rebuild either – re-run it after a change:
uv pip install --no-build-isolation -e bindings/python
#    If a struct/signature the wrapper uses changed or was added, first update
#    the cdef in bindings/python/build_sparcli.py (and the dataclass `_fill`).

# 4. C++ wrapper – header-only; if the C API changed, update include/sparcli.hpp,
#    then recompile dependents and rerun the gate:
make test-cpp
```

**Easy to forget:**

- **Version bump on ABI changes:** edit `SPARCLI_VERSION_*` in `include/core/sparcli_export.h`; the shared-lib version and `.pc` track it, so re-run `make install` and rebuild every consumer.
- **Golden files** if you changed rendering on purpose: `make test-output-golden`, `make test-input-style-golden`, `make test-cpp-golden` (review + commit).
- **Docs in lockstep:** `CLAUDE.md`, the per-language references `docs/api-c.md` / `api-cpp.md` / `api-rust.md` / `api-python.md`, and `README.md`.
- **External consumers** that link the *installed* library must be rebuilt after `make install`.
- **The cdef is hand-maintained** (Python) and the bindgen output is committed (Rust): a new/changed public function or struct field is invisible to those bindings until you update `build_sparcli.py` / regen `bindings.rs`.

---

## Install / packaging

```sh
make install                       # default PREFIX=/usr/local (may need sudo)
make install PREFIX=$HOME/.local   # user-local install
make install DESTDIR=/tmp/stage    # staged install (packaging)
make uninstall
```

`make install` lays down:

- `libsparcli.a` and the shared library (with the `soname`/link symlinks) in `$(LIBDIR)`,
- the public headers under `$(INCLUDEDIR)/{core,output,input}/` (plus the `sparcli.h` umbrella),
- `sparcli.pc` in `$(PKGCONFIGDIR)`,
- the `sparcli` CLI binary in `$(BINDIR)` (default `$(PREFIX)/bin`),
- the zsh completion `_sparcli` in `$(ZSHFUNCDIR)` (default `$(PREFIX)/share/zsh/site-functions`).

Consumers then build against it with pkg-config:

```sh
cc myapp.c $(pkg-config --cflags --libs sparcli) -o myapp
```

### User-local install (no sudo)

The default `PREFIX=/usr/local` is **not writable without root** on most macOS setups, so `make install` fails there with `Operation not permitted`. Rather than `sudo` (which leaves root-owned files in `/usr/local` – and, if you ever build *as root*, root-owned objects in the build tree that later block a normal `make`), install into your home prefix instead:

```sh
make clean                          # rebuild so the dylib's install_name
make install PREFIX="$HOME/.local"  #   points at ~/.local, not /usr/local
```

> Why `make clean` first: the shared library's `install_name` is baked in **at link time** from `PREFIX`. Reusing a dylib that was linked for `/usr/local` would make the loader look there at run time and fail. A clean rebuild with `PREFIX="$HOME/.local"` links it (and writes `sparcli.pc`) for the right path.

`pkg-config` does not search `~/.local` by default, so point it there once – add to `~/.zshrc` (or `~/.bashrc`):

```sh
# Let pkg-config find libraries installed under ~/.local (e.g. sparcli)
echo 'export PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig:$PKG_CONFIG_PATH"' >> ~/.zshrc
source ~/.zshrc
```

Verify it resolves to your home prefix (not a stale `/usr/local` copy):

```sh
pkg-config --cflags --libs sparcli   # → -I$HOME/.local/include -L$HOME/.local/lib -lsparcli
```

Remove later with `make uninstall PREFIX="$HOME/.local"`.

### Default prefix (`/usr/local`) without sudo

If you want to keep the default `PREFIX=/usr/local` (so consumers need no `PKG_CONFIG_PATH` setup and pkg-config / the linker find sparcli out of the box), the alternative is a **one-time** ownership change of the three install directories:

```sh
sudo install -d -o "$(whoami)" -g admin \
    /usr/local/lib /usr/local/include /usr/local/lib/pkgconfig
```

(`install -d` creates the directories if missing and re-owns them if they already exist – one command covers both cases.)

After that, `make install` / `make uninstall` work without `sudo` – for sparcli and any other project installing there. This is long-standing practice on macOS (Intel-era Homebrew owned `/usr/local` the same way); SIP protects `/usr/local` itself, not its contents. No `make clean` is needed since the prefix does not change.

Trade-off: those directories become writable by your user account, so any process running as you can place libraries there. Prefer the `~/.local` install above if that matters in your environment.

---

## Releasing a new version

The version string lives in **three places that must stay in sync** – the most common release mistake is bumping only some of them:

| Location | What it drives |
|----------|----------------|
| `Makefile` – `VERSION_MAJOR` / `_MINOR` / `_PATCH` | shared-library version + soname, `sparcli.pc` |
| `include/core/sparcli_export.h` – `SPARCLI_VERSION_MAJOR` / `_MINOR` / `_PATCH` | compile-time constants; `sc_version()` / `sc_version_string()` |
| `README.md` – the `![Version: …]` badge | the version shown on the project page |

Checklist:

1. `make test EXTRA_CFLAGS=-Werror` is green; regenerate goldens if rendering changed intentionally.
2. Bump the version in all **three** locations above.
3. `make clean && make` – `sparcli.pc` and the soname pick up the new value; confirm with `./build.examples.nosync/...` or a quick `sc_version_string()` check.
4. Commit, then tag and push: `git tag vX.Y.Z && git push --tags`.
5. *(optional)* Write GitHub release notes / a CHANGELOG entry.

Bumping the **major** changes the soname, so dependents must relink.

---

## Examples

```sh
make examples                                 # build every examples/*.c
make run-example EX=readme_screenshots_output # static gallery of output widgets
make run-example EX=readme_screenshots_input  # static gallery of input widgets
make run-example EX=shortcut_demo             # custom shortcuts + rich prompt (interactive)

./examples/cli_output_demo.zsh                # sparcli CLI: every output command (zsh)
./examples/cli_input_demo.zsh                 # sparcli CLI: every input command (zsh, interactive)
```

Examples are auto-discovered (`$(wildcard examples/*.c)`): dropping a new `examples/foo.c` in makes `make run-example EX=foo` work – no Makefile edit. The two `cli_*.zsh` scripts are plain zsh scripts that run the `./sparcli` binary built by `make` (point them at another binary with `SPARCLI=/path/to/sparcli`).

---

## Project layout & adding code

```
src/core/     color, text, print, output stream, render-wrap, version
src/output/   panel, rule, list, tree, columns, kv, alert, badge,
              progressbar, spinner, markup, pad, util, + table/
src/tty/      raw mode + signal restore, key decoder, in-place redraw
src/input/    prompt engine, line editor, shortcut, external editor, confirm,
              text/password/number, textarea, select, fuzzy, datepicker, theme
include/{core,output,input}/   public C headers (sparcli.h is the umbrella)
include/sparcli.hpp            header-only C++20 wrapper (RAII over the C API)
cli/                           the sparcli command-line tool (subcommand per widget)
completions/                   zsh completion (_sparcli) for the CLI
bindings/rust/                 cargo workspace: sparcli-sys (FFI) + sparcli (safe)
bindings/python/               cffi (API-mode) wrapper: src/sparcli/ + build_sparcli.py
tests/output/                  output suite
tests/input/{logic,style,pty}/ interactive / snapshot / PTY suites
tests/cpp/                     C++ wrapper assertion suite + golden
tests/cli/                     CLI golden-file suite + CLI PTY suite
```

The **C++ wrapper** (`include/sparcli.hpp`, namespace `sparcli`) is a thin, header-only layer: RAII handle classes, owned strings where the C API borrows (tables), `std::optional`/`std::vector` returns for input, and a `.get()` escape hatch to the C API. It is verified by `make test-cpp` and documented in [`api-cpp.md`](api-cpp.md). Keep both in sync when you add or change public C functions.

The **output/input boundary is physical**: `src/core` and `src/output` are stream-oriented and write through the redirectable `sc_output_stream()`; `src/tty` and `src/input` drive a real terminal in raw mode.

To **add a source file**, append its path (e.g. `src/output/foo.c`) to `SRC` in the `Makefile` – the build tree mirrors `src/` automatically. Public headers go under `include/<area>/`, and cross-includes use root-relative paths (`#include "core/sparcli_core.h"`, resolved via `-Iinclude`). The table renderer is split into sub-modules that are `#include`-chained from `table.c`, so only `src/output/table/table.c` appears in `SRC`.

To **add public API**, declare it inside the header's `SPARCLI_BEGIN_DECLS` / `SPARCLI_END_DECLS` block (these provide the `extern "C"` wrapper for C++ consumers) and mark the prototype `SPARCLI_EXPORT` – symbols are hidden by default, so an unmarked function won't be part of the shared-library ABI.

### Adding public API – ownership checklist

Every pointer a public API receives needs an explicit answer to *"who owns this, for how long?"*. Getting this wrong is invisible to normal tests (string literals never dangle) and surfaces as use-after-free only in FFI consumers – so it is a design-time checklist, not something tests catch for free:

1. **Does the function store a caller pointer past its own return?** (handle-based `*_new()`, process-wide setters, deferred rendering)
   - **Default: copy it** (and free the copy in the matching `_free()`). This is the project invariant – see "Opts-string lifetime" in [`../CLAUDE.md`](../CLAUDE.md).
   - Only **borrow** when copying is genuinely impossible/expensive (e.g. `ScText` content, `shortcuts` callbacks) – then document the required lifetime **per field** in the header.
2. **Write a lifetime test** that hands over a temporary buffer and clobbers/frees it right after the call:
   - Logic suite: clobber a stack buffer (pattern: `tests/input/logic/test_opts_copy.c`).
   - PTY suite: `strdup` + `free` before the run, so ASan sees a real dangling read (pattern: cases `fuzzy-heap-prompt`, `theme-heap-strings`).
   - Output suite: clobbered buffer + golden comparison (pattern: "Opts strings are copied" cases in `test_lists.c` / `test_progressbar.c`).
   - Python: construct from a temporary, `gc.collect()`, then use the object (pattern: `test_*_survive_gc` in `bindings/python/tests/test_smoke.py`) – and run it under `make python-test-debug`.
3. **Sanitize at the trust boundary.** Every new public entry point that accepts user text must route it through the ANSI sanitizer exactly once (`sc_sanitize_copy_mode` honoring the widget's `ScAnsiMode ansi` opts field, or `sc_sanitize_copy(str, sc_allow_ansi())` when there are no opts) and use the internal raw paths (`sc_text_append_raw`, `sc_print_raw`) afterwards – never re-sanitize library-rendered content. See "ANSI sanitization / trust boundary" in [`../CLAUDE.md`](../CLAUDE.md).
4. **Update the bindings and docs** (cdef/bindgen/hpp, `api-*.md`, `CLAUDE.md`) – see the rebuild section above.

### Adding a test

- **Output test:** add `tests/output/test_foo.c`, append it to `TEST_SRC` in the `Makefile`, and register it in `get_all_tests()` in `tests/output/test_main.c`. If it renders deterministic output, regenerate the golden file afterwards (`make test-output-golden`).
- **Input logic/widget test:** add the source to `INPUT_TEST_SRC`, declare the entry point in `tests/input/logic/test_input.h`, and add it to the runner table in `tests/input/logic/test_input_main.c` (mark it `interactive` or not).
- **Input style snapshot:** add to `STYLE_TEST_SRC` and the `tests/input/style/` runner, then `make test-input-style-golden`.
- **C++ wrapper check:** add a `CHECK(...)` case to `tests/cpp/test_cpp.cpp` (no Makefile change – `test-cpp` compiles that file directly); if it changes the demo output, run `make test-cpp-golden`.
- **CLI output case:** add the invocation to `tests/cli/run_output.sh` (use the `section`/`expect_fail` helpers and, for layout-sensitive commands, an explicit `--width`), then `make test-cli-golden`.
- **CLI input case:** add an entry to the `CASES[]` array in `tests/cli/test_cli_pty.c` (CLI argv, keystrokes, expected stdout + exit code; `no_tty = true` for non-terminal behavior). No Makefile change needed.

See [`../CLAUDE.md`](../CLAUDE.md) for the full module map and type reference.

---

## Conventions (in brief)

The full reference lives in [`../CLAUDE.md`](../CLAUDE.md); the essentials:

- Doc comments sit **above** the member/field (`/** … */`), with a blank line between members; enum values keep a trailing `/**< … */`.
- `struct`/`enum` typedefs carry a **tag** (`typedef struct Name { … } Name;`).
- Horizontal alignment fields/params are `halign`, vertical `valign` – never a bare `align` (that name is reserved for the `sc_align_*` *operation*).
- Lines stay within **80 columns**.
- Zero-initialized `ScColor` / `ScTextStyle` means "not set / no formatting" – rely on it instead of spelling out defaults.
- String vs rich-text entry points come in `_str` / `_text` pairs; terse inline constructors have FFI-friendly exported variants.

---

## Pre-commit checklist

1. `make qa` – the full QA run passes: build with `-Werror`, all headless test gates, ASan/UBSan, TSan, lint, fuzzing, and both bindings (Rust + Python incl. the poisoned-memory gate). See "`make qa`" above for what it chains.
2. If you changed rendering on purpose, regenerate the affected golden file (`make test-output-golden` / `make test-input-style-golden` / `make test-cli-golden` / `make test-cpp-golden`), review the diff and commit it.
3. If you added public API: walk the **ownership checklist** above (copy vs. documented borrow + lifetime test) and update the bindings (cdef / bindgen / hpp) + docs.
4. If you changed the C API surface: regenerate the Rust bindgen output (`cargo build --manifest-path bindings/rust/Cargo.toml --features regen`) and commit `bindings.rs`.
