# sparcli – API Reference

sparcli has one reference page per language interface:

- **[C API](api-c.md)** – every public type, function, option struct and macro of the core C library (`#include <sparcli.h>`): widgets, markup, capture, input prompts, pager.
- **[Framework API (C)](api-framework.md)** – the application-framework helpers of the C library: XDG paths, pretty errors, logging, and the argument parser (subcommands, typed options, auto --help, zsh completion).
- **[Serde / data parsers (C + C++)](api-serde.md)** – the opt-in `serde/` layer: the shared `ScValue` model and JSON/CSV/TOML/YAML/Markdown readers and writers, plus the `sparcli::serde` C++ wrapper. Not part of the `sparcli.h` umbrella.
- **[C++ wrapper](api-cpp.md)** – the header-only, RAII C++20 layer in the `sparcli::` namespace (`#include <sparcli.hpp>`), built on top of the C API.
- **[Rust bindings](api-rust.md)** – the safe, idiomatic `sparcli` crate (`bindings/rust/`): RAII handles, builder option structs, `Result<Option<T>>` prompts, closures for callbacks.
- **[Python bindings](api-python.md)** – the Pythonic `sparcli` package (`bindings/python/`): a cffi (API-mode) wrapper with RAII handles, `@dataclass` options, `value | None` prompts and `SparcliInputUnavailable`.
- **[Command-line tool](cli.md)** – the `sparcli` binary for zsh/bash: every output and input widget as a shell subcommand, with markup, stdin/stdout conventions, exit codes and zsh completion.

For installation, linking and quick-start examples (C, C++, Rust and Python), see the [main README](../README.md). For the build/test/contributor workflow, see [`DEVELOPMENT.md`](DEVELOPMENT.md).
