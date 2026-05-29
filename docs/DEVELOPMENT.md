# sparcli — Developer Guide

Build, test, and install workflow for working on sparcli itself. For the public
API see [`API.md`](API.md); for architecture and house conventions see
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

**Good to know:**

- **Header-dependency tracking** is automatic (`-MMD -MP`): editing a header
  rebuilds every dependent object on the next `make` — you do **not** need
  `make clean` after changing a header.
- The **sanitizer build** uses a separate build tree and a separate `.a`, so it
  never contaminates the normal artefacts.
- Tests link against the **static** `libsparcli.a`, so running them never
  depends on the install path or the dynamic-loader search order.

---

## Tests

The suites split along the output/input boundary. Everything except
`make test-input` (which needs a real terminal) runs headless.

### `make test` — output suite

Non-interactive renderer tests in `tests/output/`. Prints to stdout for visual
inspection; also gated by a golden file (below).

```sh
make test                              # all output tests
make test ARGS=--no-animated           # skip the spinner/progress animations
make test ARGS=--focus                 # run only the focused subset
make test ARGS="--focus --no-animated" # combine flags
```

Covers: text attributes, colors, panels, alerts, tables, rules, trees, lists,
progress bar, spinner, key-value pairs, badges, utilities, padding, alignment,
markup, and columns (basic + advanced). The two *animated* cases (progress bar,
spinner) are skipped by `--no-animated`.

### `make test-output-check` — output golden-file gate

Runs the output suite with `--no-animated` and diffs it against
`tests/output/expected.txt`. Fails on any rendering drift. This is the automated
correctness gate for the output side.

```sh
make test-output-check     # diff against the committed snapshot
make test-output-golden    # regenerate the snapshot (after an intended change)
```

### `make test-input` — interactive widget suite

Drives every input widget for real in `tests/input/logic/`. **Needs a real
TTY** — run it in a terminal, not a pipe.

```sh
make test-input                # interactive: confirm, text, password, number,
                               # textarea, select, fuzzy, datepicker, theme
make test-input ARGS=--logic   # non-interactive logic checks only (CI-safe)
```

`ARGS=--logic` skips the interactive widgets and runs only the pure-logic
checks — key decoder, line editor, character filters, and the thread-safety
test — and exits non-zero if any assertion fails.

### `make test-input-style` — style snapshots

Renders every input widget in many styles via the internal frame builders, with
no TTY and no keystrokes. Safe anywhere.

```sh
make test-input-style          # print all style snapshots
make test-input-style-check    # diff against tests/input/style/expected.txt
make test-input-style-golden   # regenerate the snapshot
```

### `make test-input-pty` — self-driving PTY suite (ASan/UBSan)

Forks each interactive widget onto a pseudo-terminal, feeds it a canned
keystroke script, and asserts the returned value — no human needed. Built with
AddressSanitizer + UndefinedBehaviorSanitizer, so it also gives sanitizer
coverage of the interactive code paths. Covers confirm, text, number, select,
textarea, and datepicker.

```sh
make test-input-pty
```

### `make sanitize` — output suite under ASan/UBSan

Rebuilds and runs the output suite with AddressSanitizer + UBSan in a separate
build tree.

```sh
make sanitize ARGS=--no-animated
```

> The style and PTY runners take no `ARGS`. The thread-safety test runs as part
> of `make test-input ARGS=--logic`.

### Golden-file workflow

`test-output-check` and `test-input-style-check` diff the rendered bytes against
committed snapshots (deterministic because, off a TTY, the terminal width falls
back to 80). When you change rendering **on purpose**:

1. regenerate with `make test-output-golden` and/or `make test-input-style-golden`,
2. review the diff to confirm the change is what you intended,
3. commit the updated `expected.txt`.

### After a change — run these

Copy-paste block to validate a change. The first five are headless (safe to run
anywhere, including over a pipe); the rest need a real terminal.

```sh
# 1. Build clean, warnings as errors.
make EXTRA_CFLAGS=-Werror

# 2. Headless gates — all must pass.
make test-output-check       EXTRA_CFLAGS=-Werror   # output golden-file diff
make test-input ARGS=--logic EXTRA_CFLAGS=-Werror   # input logic + thread safety
make test-input-style-check  EXTRA_CFLAGS=-Werror   # style snapshot diff
make test-input-pty          EXTRA_CFLAGS=-Werror   # interactive paths under ASan/UBSan

# 3. If you changed rendering on purpose, regenerate + review + commit the golden:
make test-output-golden
make test-input-style-golden

# 4. Examples still build (and try the input widgets by hand):
make examples                          # compile every examples/*.c
make run-example EX=readme_screenshots # output gallery
make run-example EX=input_demo         # interactive walkthrough of all input widgets

# 5. Interactive widget suite (needs a real terminal):
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

---

## Examples

```sh
make examples                            # build every examples/*.c
make run-example EX=readme_screenshots   # build & run one example
```

---

## Project layout & adding code

```
src/core/     color, text, print, output stream, render-wrap, version
src/output/   panel, rule, list, tree, columns, kv, alert, badge,
              progressbar, spinner, markup, pad, util, + table/
src/tty/      raw mode + signal restore, key decoder, in-place redraw
src/input/    prompt engine, line editor, confirm, text/password/number,
              textarea, select, fuzzy, datepicker, theme
include/{core,output,input}/   public headers (sparcli.h is the umbrella)
tests/output/                  output suite
tests/input/{logic,style,pty}/ interactive / snapshot / PTY suites
```

The **output/input boundary is physical**: `src/core` and `src/output` are
stream-oriented and write through the redirectable `sc_output_stream()`;
`src/tty` and `src/input` drive a real terminal in raw mode.

To **add a source file**, append its path (e.g. `src/output/foo.c`) to `SRC` in
the `Makefile` — the build tree mirrors `src/` automatically. Public headers go
under `include/<area>/`, and cross-includes use root-relative paths
(`#include "core/sparcli_core.h"`, resolved via `-Iinclude`). The table renderer
is split into sub-modules that are `#include`-chained from `table.c`, so only
`src/output/table/table.c` appears in `SRC`.

See [`../CLAUDE.md`](../CLAUDE.md) for the full module map and type reference.

---

## Conventions (in brief)

The full reference lives in [`../CLAUDE.md`](../CLAUDE.md); the essentials:

- Doc comments sit **above** the member/field (`/** … */`), with a blank line
  between members; enum values keep a trailing `/**< … */`.
- `struct`/`enum` typedefs carry a **tag** (`typedef struct Name { … } Name;`).
- Horizontal alignment fields/params are `halign`, vertical `valign` — never a
  bare `align` (that name is reserved for the `sc_align_*` *operation*).
- Lines stay within **80 columns**.
- Zero-initialized `ScColor` / `ScTextStyle` means "not set / no formatting" —
  rely on it instead of spelling out defaults.
- String vs rich-text entry points come in `_str` / `_text` pairs; terse inline
  constructors have FFI-friendly exported variants.

---

## Pre-commit checklist

1. `make EXTRA_CFLAGS=-Werror` — builds clean, no warnings.
2. The headless gates are green:
   - `make test-output-check`
   - `make test-input ARGS=--logic`
   - `make test-input-style-check`
   - `make test-input-pty`
3. If you changed rendering on purpose, regenerate the affected golden file and
   commit it.
