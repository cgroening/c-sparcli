//! Safe, idiomatic Rust bindings for
//! [sparcli](https://github.com/cgroening/sparcli): styled terminal output
//! (panels, tables, rules, columns, lists, trees, key/value blocks, alerts,
//! badges, progress bars, spinners, markup) and interactive prompts (confirm,
//! text/password, number, textarea, select, fuzzy finder, date picker) with
//! custom shortcuts and an external-editor flow.
//!
//! The crate wraps the C library (compiled from source by `sparcli-sys`) with
//! RAII handles, builder-style option structs, `Result<Option<T>>` returns for
//! prompts, and closures for callbacks.
//!
//! ```no_run
//! use sparcli::{panel, PanelOpts, BorderType, markup};
//! panel("Hello", PanelOpts::new().rounded().title("greeting"));
//! markup::println("[bold green]done[/]");
//! ```
//!
//! # Thread-safety
//! The C output target is *thread-local* and the interactive input session is
//! *process-global* (one prompt at a time). The handle types hold raw pointers
//! and are therefore neither `Send` nor `Sync`: build and use them on one
//! thread (each thread may build its own).

#![doc(html_root_url = "https://docs.rs/sparcli/0.1.0")]

use sparcli_sys as ffi;
use std::ffi::CStr;
use std::os::raw::c_void;

#[macro_use]
mod style;
mod app;
mod error;
mod input;
mod logging;
mod output;
mod text;

pub use app::{paths, ErrorReport, Pager, PagerOpts};
pub use error::{Error, Result};
pub use input::*;
pub use logging::{log, LogLevel, Logger, LoggerOpts};
pub use output::*;
pub use style::{
    Align, AnsiMode, Attr, BorderStyle, BorderType, Color, Edges, HintLayout,
    HintPos, Position, Style, VAlign,
};
pub use text::{markup, MarkupOpts, Text};

use style::cstring;

/// Prints styled text (no trailing newline).
pub fn print(s: &str, style: Style) {
    unsafe { ffi::sc_print(cstring(s).as_ptr(), style.raw()) };
}

/// Prints styled text followed by a newline.
pub fn println(s: &str, style: Style) {
    unsafe { ffi::sc_println(cstring(s).as_ptr(), style.raw()) };
}

/// The compiled library version as `(major, minor, patch)`.
pub fn version() -> (i32, i32, i32) {
    let (mut a, mut b, mut c) = (0, 0, 0);
    unsafe { ffi::sc_version(&mut a, &mut b, &mut c) };
    (a, b, c)
}

/// The library version string (e.g. `"0.1.0"`).
pub fn version_string() -> &'static str {
    unsafe {
        CStr::from_ptr(ffi::sc_version_string())
            .to_str()
            .unwrap_or("")
    }
}

/// Sets the process-wide ANSI passthrough for user strings.
///
/// Default is `false`: escape sequences and control bytes are stripped from
/// every string entering the library, so untrusted data cannot inject
/// terminal escape codes. Per-widget opts override this via their `ansi`
/// field ([`AnsiMode`]).
pub fn set_allow_ansi(allow: bool) {
    unsafe { ffi::sc_set_allow_ansi(allow) };
}

/// The current process-wide ANSI passthrough setting.
pub fn allow_ansi() -> bool {
    unsafe { ffi::sc_allow_ansi() }
}

/// Returns a copy of `s` with all ANSI escape sequences removed (CSI, OSC,
/// DCS, two-character ESC and lone ESC bytes).
pub fn strip_ansi(s: &str) -> String {
    unsafe { take_c_string(ffi::sc_strip_ansi(cstring(s).as_ptr())) }
}

/// Truncates `s` to `max_cols` visible columns, appending `ellipsis` when it
/// did not fit.
pub fn truncate(s: &str, max_cols: i32, ellipsis: &str) -> String {
    let ell = cstring(ellipsis);
    unsafe {
        take_c_string(ffi::sc_truncate(
            cstring(s).as_ptr(),
            max_cols,
            ell.as_ptr(),
        ))
    }
}

/// Overwrites the current terminal line in place.
pub fn clear_line() {
    unsafe { ffi::sc_clear_line() };
}

/// True when an interactive prompt can run (a TTY is present).
pub fn input_available() -> bool {
    unsafe { ffi::sc_input_available() }
}

/// Redirects the thread-local output stream to `out` (a raw libc `FILE*`); pass
/// `null` to restore stdout. Escape hatch for capturing to a custom stream;
/// most callers use [`capture`](crate::capture) instead.
///
/// # Safety
/// `out` must be a valid `FILE*` (or null) that outlives its use as the target.
pub unsafe fn set_output_raw(out: *mut ffi::FILE) {
    ffi::sc_output_set_stream(out);
}

/// The current raw output stream (never null).
pub fn output_stream_raw() -> *mut ffi::FILE {
    unsafe { ffi::sc_output_stream() }
}

/// Copies a C-heap string into an owned `String` and frees the C allocation.
/// Returns an empty string for a null pointer.
pub(crate) unsafe fn take_c_string(ptr: *mut std::os::raw::c_char) -> String {
    if ptr.is_null() {
        return String::new();
    }
    let s = CStr::from_ptr(ptr).to_string_lossy().into_owned();
    libc::free(ptr as *mut c_void);
    s
}

/// Re-export of the raw FFI crate for advanced/escape-hatch use.
pub use sparcli_sys as sys;
