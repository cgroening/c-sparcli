# sparcli – Developer Guide

Build, test, and install workflow for working on sparcli itself. For the public
API see [`API.md`](API.md) (the hub linking the [C](api-c.md) and
[C++](api-cpp.md) references); for architecture and house conventions see
[`../CLAUDE.md`](../CLAUDE.md).

**Requirements:** a C11 compiler (`cc`, clang, or gcc), `make`, and a POSIX
system. The interactive PTY test suite uses `forkpty`.

---

## Build

```sh
make                 # libsparcli.a + shared lib (.dylib/.so) + sparcli.pc
make shared          # only the shared library
make pkgconfig       # only sparcli.pc
make clean           # remove all build trees, libs, test binaries
```

Compiler flags are `-std=c11 -Wall -Wextra`. Treat warnings as errors with the
`EXTRA_CFLAGS` hook (recommended before committing, and how CI-style checks are
run):

```sh
make EXTRA_CFLAGS=-Werror
```

`EXTRA_CFLAGS` takes any extra flags, so it also serves debug builds:
`make EXTRA_CFLAGS='-g -O0'`.

**Good to know:**

- **Header-dependency tracking** is automatic (`-MMD -MP`): editing a header
  rebuilds every dependent object on the next `make` – you do **not** need
  `make clean` after changing a header.
- The **sanitizer build** uses a separate build tree and a separate `.a`, so it
  never contaminates the normal artefacts.
- Tests link against the **static** `libsparcli.a`, so running them never
  depends on the install path or the dynamic-loader search order.
- **Linux and macOS** are both supported; the shared-library naming
  (`.so`/`.dylib` + soname) and the `-lutil` link for `forkpty` (PTY suite) are
  selected automatically by the Makefile.

---

## Tests

The suites split along the output/input boundary. Everything except
`make test-input` (which needs a real terminal) runs headless.

### `make test` – full non-interactive suite

The canonical "is everything OK?" command. Runs all headless gates in order –
`test-output-check`, `test-input ARGS=--logic`, `test-input-style-check`,
`test-input-pty`, `test-cpp` – and fails if any does. Command-line overrides
propagate, so `make test EXTRA_CFLAGS=-Werror` builds every gate with warnings
as errors. The interactive widget suite (`make test-input`) needs a real TTY
and is **not** included. The individual targets it chains are documented below.

```sh
make test                     # run every headless gate
make test EXTRA_CFLAGS=-Werror
```

### `make test-output` – output gallery

The output renderer suite in `tests/output/`, printed to stdout for visual
inspection (the golden-file gate below is the automated check). Covers: text
attributes, colors, panels, alerts, tables, rules, trees, lists, progress bar,
spinner, key-value pairs, badges, utilities, padding, alignment, markup, and
columns (basic + advanced).

```sh
make test-output                              # all output tests
make test-output ARGS=--no-animated           # skip the spinner/progress animations
make test-output ARGS=--focus                 # run only the focused subset
make test-output ARGS="--focus --no-animated" # combine flags
```

The two *animated* cases (progress bar, spinner) are skipped by `--no-animated`.

### `make test-output-check` – output golden-file gate

Runs the output suite with `--no-animated` and diffs it against
`tests/output/expected.txt`. Fails on any rendering drift. This is the automated
correctness gate for the output side.

```sh
make test-output-check     # diff against the committed snapshot
make test-output-golden    # regenerate the snapshot (after an intended change)
```

### `make test-input` – interactive widget suite

Drives every input widget for real in `tests/input/logic/`. **Needs a real
TTY** – run it in a terminal, not a pipe.

```sh
make test-input                # interactive: confirm, text, password, number,
                               # textarea, select, fuzzy, datepicker, theme
make test-input ARGS=--logic   # non-interactive logic checks only (CI-safe)
```

`ARGS=--logic` skips the interactive widgets and runs only the pure-logic
checks – key decoder, line editor, character filters, and the thread-safety
test – and exits non-zero if any assertion fails.

### `make test-input-style` – style snapshots

Renders every input widget in many styles via the internal frame builders, with
no TTY and no keystrokes. Safe anywhere.

```sh
make test-input-style          # print all style snapshots
make test-input-style-check    # diff against tests/input/style/expected.txt
make test-input-style-golden   # regenerate the snapshot
```

### `make test-input-pty` – self-driving PTY suite (ASan/UBSan)

Forks each interactive widget onto a pseudo-terminal, feeds it a canned
keystroke script, and asserts the returned value – no human needed. Built with
AddressSanitizer + UndefinedBehaviorSanitizer, so it also gives sanitizer
coverage of the interactive code paths. Covers confirm, text, number, select,
textarea, and datepicker.

```sh
make test-input-pty
```

### `make test-cpp` – C++ wrapper gate

Four checks for the header-only C++20 wrapper (`include/sparcli.hpp`); needs a
C++ compiler (`$(CXX)`, default `c++`):

1. **Compile** `examples/cpp_demo.cpp` (so the example never bit-rots).
2. **Golden diff** the demo's output against `tests/cpp/expected.txt`.
3. **Assertion suite** `tests/cpp/test_cpp.cpp` – verifies the wrapper's
   ownership/lifetime guarantees (table built from temporaries, `Table` survives
   a move, List/Tree rich-text arenas) and that it renders like the C API. Built
   under **AddressSanitizer + UBSan** so arena/RAII/move memory bugs are caught.
4. **PTY regression** `tests/cpp/test_cpp_pty.cpp` – drives the interactive
   string prompts (`text_input` / `password_input` / `textarea`) over a
   pseudo-terminal and asserts the returned value, so the out-param sequencing
   those wrappers once got wrong stays fixed. Also under ASan/UBSan.

```sh
make test-cpp                    # all three checks
make test-cpp EXTRA_CFLAGS=-Werror
make test-cpp-golden             # regenerate tests/cpp/expected.txt
```

### `make sanitize` – output suite under ASan/UBSan

Rebuilds and runs the output suite with AddressSanitizer + UBSan in a separate
build tree.

```sh
make sanitize ARGS=--no-animated
```

> The style and PTY runners take no `ARGS`. The thread-safety test runs as part
> of `make test-input ARGS=--logic`.

### Golden-file workflow

`test-output-check`, `test-input-style-check` and the C++ `test-cpp` gate diff
the rendered bytes against committed snapshots (deterministic because, off a
TTY, the terminal width falls back to 80). When you change rendering **on
purpose**:

1. regenerate with `make test-output-golden`, `make test-input-style-golden`
   and/or `make test-cpp-golden`,
2. review the diff to confirm the change is what you intended,
3. commit the updated `expected.txt`.

### After a change – run these

Copy-paste block to validate a change.

```sh
# 1. Build + every headless gate, warnings as errors (the main check).
make test EXTRA_CFLAGS=-Werror

# 2. If you changed rendering on purpose, regenerate + review + commit the golden:
make test-output-golden
make test-input-style-golden
make test-cpp-golden                   # only if you changed the C++ wrapper

# 3. Examples still build (and try the input widgets by hand):
make examples                          # compile every examples/*.c
make run-example EX=readme_screenshots_output # output widget gallery (static)
make run-example EX=readme_screenshots_input  # input widget gallery (static)
make run-example EX=input_demo         # interactive walkthrough of all input widgets

# 4. Interactive widget suite (needs a real terminal):
make test-input
```

---

## Install / packaging

```sh
make install                       # default PREFIX=/usr/local (may need sudo)
make install PREFIX=$HOME/.local   # user-local install
make install DESTDIR=/tmp/stage    # staged install (packaging)
make uninstall
```

`make install` lays down:

- `libsparcli.a` and the shared library (with the `soname`/link symlinks) in
  `$(LIBDIR)`,
- the public headers under `$(INCLUDEDIR)/{core,output,input}/` (plus the
  `sparcli.h` umbrella),
- `sparcli.pc` in `$(PKGCONFIGDIR)`.

Consumers then build against it with pkg-config:

```sh
cc myapp.c $(pkg-config --cflags --libs sparcli) -o myapp
```

### User-local install (no sudo)

The default `PREFIX=/usr/local` is **not writable without root** on most macOS
setups, so `make install` fails there with `Operation not permitted`. Rather
than `sudo` (which leaves root-owned files in `/usr/local` — and, if you ever
build *as root*, root-owned objects in the build tree that later block a normal
`make`), install into your home prefix instead:

```sh
make clean                          # rebuild so the dylib's install_name
make install PREFIX="$HOME/.local"  #   points at ~/.local, not /usr/local
```

> Why `make clean` first: the shared library's `install_name` is baked in **at
> link time** from `PREFIX`. Reusing a dylib that was linked for `/usr/local`
> would make the loader look there at run time and fail. A clean rebuild with
> `PREFIX="$HOME/.local"` links it (and writes `sparcli.pc`) for the right path.

`pkg-config` does not search `~/.local` by default, so point it there once — add
to `~/.zshrc` (or `~/.bashrc`):

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

---

## Releasing a new version

The version string lives in **three places that must stay in sync** – the most
common release mistake is bumping only some of them:

| Location | What it drives |
|----------|----------------|
| `Makefile` – `VERSION_MAJOR` / `_MINOR` / `_PATCH` | shared-library version + soname, `sparcli.pc` |
| `include/core/sparcli_export.h` – `SPARCLI_VERSION_MAJOR` / `_MINOR` / `_PATCH` | compile-time constants; `sc_version()` / `sc_version_string()` |
| `README.md` – the `![Version: …]` badge | the version shown on the project page |

Checklist:

1. `make test EXTRA_CFLAGS=-Werror` is green; regenerate goldens if rendering
   changed intentionally.
2. Bump the version in all **three** locations above.
3. `make clean && make` – `sparcli.pc` and the soname pick up the new value;
   confirm with `./build.examples.nosync/...` or a quick `sc_version_string()`
   check.
4. Commit, then tag and push: `git tag vX.Y.Z && git push --tags`.
5. *(optional)* Write GitHub release notes / a CHANGELOG entry.

Bumping the **major** changes the soname, so dependents must relink.

---

## Examples

```sh
make examples                                 # build every examples/*.c
make run-example EX=readme_screenshots_output # static gallery of output widgets
make run-example EX=readme_screenshots_input  # static gallery of input widgets
```

Examples are auto-discovered (`$(wildcard examples/*.c)`): dropping a new
`examples/foo.c` in makes `make run-example EX=foo` work – no Makefile edit.

---

## Project layout & adding code

```
src/core/     color, text, print, output stream, render-wrap, version
src/output/   panel, rule, list, tree, columns, kv, alert, badge,
              progressbar, spinner, markup, pad, util, + table/
src/tty/      raw mode + signal restore, key decoder, in-place redraw
src/input/    prompt engine, line editor, confirm, text/password/number,
              textarea, select, fuzzy, datepicker, theme
include/{core,output,input}/   public C headers (sparcli.h is the umbrella)
include/sparcli.hpp            header-only C++20 wrapper (RAII over the C API)
tests/output/                  output suite
tests/input/{logic,style,pty}/ interactive / snapshot / PTY suites
tests/cpp/                     C++ wrapper assertion suite + golden
```

The **C++ wrapper** (`include/sparcli.hpp`, namespace `sparcli`) is a thin,
header-only layer: RAII handle classes, owned strings where the C API borrows
(tables), `std::optional`/`std::vector` returns for input, and a `.get()`
escape hatch to the C API. It is verified by `make test-cpp` and documented in
[`api-cpp.md`](api-cpp.md). Keep both in sync when you add or change public
C functions.

The **output/input boundary is physical**: `src/core` and `src/output` are
stream-oriented and write through the redirectable `sc_output_stream()`;
`src/tty` and `src/input` drive a real terminal in raw mode.

To **add a source file**, append its path (e.g. `src/output/foo.c`) to `SRC` in
the `Makefile` – the build tree mirrors `src/` automatically. Public headers go
under `include/<area>/`, and cross-includes use root-relative paths
(`#include "core/sparcli_core.h"`, resolved via `-Iinclude`). The table renderer
is split into sub-modules that are `#include`-chained from `table.c`, so only
`src/output/table/table.c` appears in `SRC`.

To **add public API**, declare it inside the header's
`SPARCLI_BEGIN_DECLS` / `SPARCLI_END_DECLS` block (these provide the `extern "C"`
wrapper for C++ consumers) and mark the prototype `SPARCLI_EXPORT` – symbols are
hidden by default, so an unmarked function won't be part of the shared-library
ABI.

### Adding a test

- **Output test:** add `tests/output/test_foo.c`, append it to `TEST_SRC` in the
  `Makefile`, and register it in `get_all_tests()` in `tests/output/test_main.c`.
  If it renders deterministic output, regenerate the golden file afterwards
  (`make test-output-golden`).
- **Input logic/widget test:** add the source to `INPUT_TEST_SRC`, declare the
  entry point in `tests/input/logic/test_input.h`, and add it to the runner
  table in `tests/input/logic/test_input_main.c` (mark it `interactive` or not).
- **Input style snapshot:** add to `STYLE_TEST_SRC` and the `tests/input/style/`
  runner, then `make test-input-style-golden`.
- **C++ wrapper check:** add a `CHECK(...)` case to `tests/cpp/test_cpp.cpp`
  (no Makefile change – `test-cpp` compiles that file directly); if it changes
  the demo output, run `make test-cpp-golden`.

See [`../CLAUDE.md`](../CLAUDE.md) for the full module map and type reference.

---

## Conventions (in brief)

The full reference lives in [`../CLAUDE.md`](../CLAUDE.md); the essentials:

- Doc comments sit **above** the member/field (`/** … */`), with a blank line
  between members; enum values keep a trailing `/**< … */`.
- `struct`/`enum` typedefs carry a **tag** (`typedef struct Name { … } Name;`).
- Horizontal alignment fields/params are `halign`, vertical `valign` – never a
  bare `align` (that name is reserved for the `sc_align_*` *operation*).
- Lines stay within **80 columns**.
- Zero-initialized `ScColor` / `ScTextStyle` means "not set / no formatting" –
  rely on it instead of spelling out defaults.
- String vs rich-text entry points come in `_str` / `_text` pairs; terse inline
  constructors have FFI-friendly exported variants.

---

## Pre-commit checklist

1. `make test EXTRA_CFLAGS=-Werror` – builds clean (no warnings) and all five
   headless gates pass.
2. If you changed rendering on purpose, regenerate the affected golden file
   (`make test-output-golden` / `make test-input-style-golden` /
   `make test-cpp-golden`) and commit it.
