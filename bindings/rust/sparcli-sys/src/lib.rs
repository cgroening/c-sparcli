//! Raw, unsafe FFI bindings to the sparcli C library.
//!
//! Use the safe [`sparcli`](https://docs.rs/sparcli) crate instead unless you
//! need direct C access. The C sources are compiled from the repo by
//! `build.rs` (via the `cc` crate). Bindings are committed in `bindings.rs`;
//! build with `--features regen` to regenerate them from the headers (needs
//! libclang).
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]
#![allow(clippy::all)]

#[cfg(feature = "regen")]
include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

#[cfg(not(feature = "regen"))]
include!("bindings.rs");
