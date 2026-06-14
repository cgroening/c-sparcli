# Windows support

sparcli is fully Windows-native: the C library, the `sparcli` CLI and the
shared library build and run on Windows 10 1809+ / Windows Terminal. The
platform differences live behind `#ifdef _WIN32` branches inside the existing
files (the pattern `src/input/editor_file.c` established), plus a small
compatibility header `include/platform/sc_compat.h`, so the three source lists
(the Makefile, `bindings/rust/sparcli-sys/build.rs`, and
`bindings/python/build_sparcli.py`) stay in lock-step.

The supported development toolchain is **MinGW-w64 UCRT64** (via MSYS2). MSVC is
used for the PyPI wheels (see *Bindings* below).

## What is ported

| Area | POSIX | Windows |
|---|---|---|
| Terminal size | `ioctl(TIOCGWINSZ)` | `GetConsoleScreenBufferInfo` |
| ANSI output | always on | `ENABLE_VIRTUAL_TERMINAL_PROCESSING` + UTF-8 codepage, enabled lazily on first output |
| Raw mode | `termios` + `/dev/tty` | `SetConsoleMode` on `CONIN$`/`CONOUT$` (VT input) |
| Key reading | `read()` + `select()` | `ReadConsoleInputW` + `WaitForSingleObject`; key events translated to the VT byte sequences the shared decoder already understands |
| Resize | `SIGWINCH` | `WINDOW_BUFFER_SIZE_EVENT` → `SC_KEY_RESIZE` |
| Cleanup on exit/interrupt | `sigaction` + `atexit` | `SetConsoleCtrlHandler` + `atexit` |
| Capture stream | `open_memstream` | temp file under `%TEMP%` (`GetTempFileNameW` + `_wfopen("w+bTD")`) — see `src/core/memstream.c` |
| Subprocess (`sc_run`) | `fork`/`exec`/`poll` | `CreatePipe` + `CreateProcessW` + I/O threads |
| Editor / pager | `fork`/`execvp` | `CreateProcessW` (editor, inherited console) / `_popen` (pager, default `more`) |
| CLI `spin` | `fork` + `waitpid` | `_spawnvp(_P_NOWAIT)` + `WaitForSingleObject` tick loop |

`sc_compat.h` shims: `SC_ATTR_FORMAT`, `SC_THREAD_LOCAL`, `ssize_t` (MSVC),
`strndup`, `strcasecmp`/`strncasecmp`, `localtime_r`.

## Prerequisites (MSYS2 / UCRT64)

```sh
# In an MSYS2 UCRT64 shell:
pacman -S mingw-w64-ucrt-x86_64-gcc make \
          mingw-w64-ucrt-x86_64-clang-tools-extra \
          mingw-w64-ucrt-x86_64-cppcheck
```

The repo must be a real local clone, not a network share, and should be checked
out with LF line endings (a `.gitattributes` enforces this).

## Building

From the **UCRT64** shell at the repo root:

```sh
make cli        # builds libsparcli.a + the sparcli.exe CLI
make shared     # builds libsparcli.dll (+ libsparcli.dll.a import library)
make qa-win     # the Windows gate: builds cli + shared with -Werror
```

`make qa` (macOS/Linux) remains the full gate (it adds TSan/UBSan and a real
PTY); those sanitizer/PTY gates stay Unix-only. `qa-win` is the Windows subset.

### Cross-sanity from macOS/Linux

The non-interactive core cross-compiles with a MinGW cross toolchain, e.g.:

```sh
brew install mingw-w64
make CC=x86_64-w64-mingw32-gcc qa-win-compile   # compile-only smoke check
```

## Bindings

- **Rust** (`bindings/rust/`): `build.rs` compiles the C sources via the `cc`
  crate and defines `SPARCLI_STATIC`. The crate builds with a MinGW (`-gnu`)
  target. Note: on an **ARM64** Windows host, `cargo` also needs a host
  toolchain for build scripts — install the **ARM64 MSVC build tools** (native
  `aarch64-pc-windows-msvc` host), since the `x86_64-pc-windows-gnu` *cross*
  path conflicts with MSYS2's `link` shadowing the MSVC `link.exe`.

- **Python** (`bindings/python/`): `build_sparcli.py` compiles the sources into
  a cffi extension. On Windows it references the real `src`/`include` by
  absolute path (the `csrc`/`cinclude` symlinks check out as text files when
  git `core.symlinks` is off) and selects MSVC flags (`/std:c11`). CPython on
  Windows builds extensions with **MSVC**, which is the same compiler the PyPI
  wheels use (built via `cibuildwheel --platform windows`, `win_amd64`).

### MSVC notes

MSVC's C compiler is stricter than GCC/Clang in two ways the port accounts for:
`__attribute__`/`_Thread_local`/`ssize_t` (shimmed in `sc_compat.h`), and
constant initializers — MSVC rejects a compound-literal color macro
(`SC_ANSI_COLOR_*`) as a static initializer (C2099), so static style tables use
designated initializers instead. Completing the MSVC build (and thus the
wheels) requires this designated-initializer treatment across the colour
registry (`src/core/color.c`) and any remaining static colour tables, plus
confirming `<stdatomic.h>` under the installed MSVC version.
