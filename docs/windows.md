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
| Capture stream | `open_memstream` | temp file under `%TEMP%` (`GetTempFileNameW` + `_wfopen("w+bTD")`) – see `src/core/memstream.c` |
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
  crate (`SPARCLI_STATIC`, `/experimental:c11atomics`, and it strips the `\\?\`
  prefix `canonicalize()` adds so MSVC's `cl` accepts the paths). The C compiles
  cleanly under MSVC. Building `cargo test` needs an MSVC environment, e.g. on an
  ARM64 host via the x64-emulated toolchain:

  ```bat
  call vcvarsall.bat arm64_x64
  cargo +stable-x86_64-pc-windows-msvc test --manifest-path bindings\rust\Cargo.toml
  ```

  **Open item:** the committed `sparcli-sys/src/bindings.rs` was generated on
  macOS and hard-codes that platform's struct layouts (e.g. `struct tm` = 56
  bytes); its bindgen layout assertions fail to compile on Windows, where
  `struct tm` is smaller. Fix by regenerating bindings per target (the `regen`
  feature needs libclang) or by gating the layout tests per `target_os` rather
  than committing a single platform's bindings.

- **Python** (`bindings/python/`): `build_sparcli.py` compiles the sources into
  a cffi extension with **MSVC** (the compiler CPython-on-Windows uses, and the
  same one the PyPI wheels use). It references the real `src`/`include` by
  absolute path (the `csrc`/`cinclude` symlinks check out as text files when git
  `core.symlinks` is off) and passes `/std:c11 /experimental:c11atomics`. The
  full suite passes:

  ```powershell
  python bindings\python\build_sparcli.py
  $env:PYTHONPATH="src"; $env:SPARCLI_NO_TTY="1"; $env:PYTHONUTF8="1"
  python -m pytest bindings\python\tests -q     # 64 passed
  ```

  `PYTHONUTF8=1` makes Python's text I/O UTF-8 (matching POSIX), so tests that
  read back rendered box-drawing output decode correctly. Wheels:
  `cibuildwheel --platform windows` (`win_amd64`).

### MSVC notes

MSVC's C compiler is stricter than GCC/Clang; the port accounts for it:
`__attribute__` / `_Thread_local` / `ssize_t` are shimmed in `sc_compat.h`;
`<stdatomic.h>` needs `/experimental:c11atomics`; and a compound-literal color
macro is not a constant initializer (C2099), so every colour is defined once as
a brace-initializer `SC_*_INIT` (used by the static colour/style tables) with
the value macro `SC_*` derived as `((ScColor)SC_*_INIT)` for expressions
(`include/core/sparcli_core.h`, `sparcli_palette.h`).
