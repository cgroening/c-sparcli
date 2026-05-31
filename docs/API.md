# sparcli – API Reference

sparcli has one reference page per language interface:

- **[C API](api-c.md)** – every public type, function, option struct and macro
  of the core C library (`#include <sparcli.h>`).
- **[C++ wrapper](api-cpp.md)** – the header-only, RAII C++20 layer in the
  `sparcli::` namespace (`#include <sparcli.hpp>`), built on top of the C API.
- **[Rust bindings](api-rust.md)** – the safe, idiomatic `sparcli` crate
  (`bindings/rust/`): RAII handles, builder option structs, `Result<Option<T>>`
  prompts, closures for callbacks.

For installation, linking and quick-start examples (C and C++), see the
[main README](../README.md). For the build/test/contributor workflow, see
[`DEVELOPMENT.md`](DEVELOPMENT.md).

> Future language bindings (e.g. Python) will get their own `api-*.md` page here.
