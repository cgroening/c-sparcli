//! Application-framework helpers: XDG base directories and pager.

use crate::style::cstring;
use sparcli_sys as ffi;
use std::path::PathBuf;

/// XDG base-directory helpers.
///
/// Each function resolves the per-application directory (creating it on
/// first use) and returns `None` when the name is invalid or the
/// directory cannot be created.
///
/// ```no_run
/// let config = sparcli::paths::config("myapp");   // ~/.config/myapp
/// let log = sparcli::paths::file(
///     sparcli::paths::Kind::State, "myapp", "logs/run.log",
/// );
/// ```
pub mod paths {
    use super::*;

    repr_enum!(
        /// The four XDG base-directory kinds.
        Kind {
            Config = ffi::ScPathKind_SC_PATH_CONFIG,
            Data = ffi::ScPathKind_SC_PATH_DATA,
            Cache = ffi::ScPathKind_SC_PATH_CACHE,
            State = ffi::ScPathKind_SC_PATH_STATE,
        } default Config
    );

    /// Takes ownership of a C-heap path string; `None` for null.
    fn take_path(ptr: *mut std::os::raw::c_char) -> Option<PathBuf> {
        if ptr.is_null() {
            return None;
        }
        Some(PathBuf::from(unsafe { crate::take_c_string(ptr) }))
    }

    /// The per-application directory for `kind` (created on first use).
    pub fn dir(kind: Kind, appname: &str) -> Option<PathBuf> {
        take_path(unsafe { ffi::sc_path(kind.raw(), cstring(appname).as_ptr()) })
    }

    /// `~/.config/<app>` (or `$XDG_CONFIG_HOME`).
    pub fn config(appname: &str) -> Option<PathBuf> {
        dir(Kind::Config, appname)
    }

    /// `~/.local/share/<app>` (or `$XDG_DATA_HOME`).
    pub fn data(appname: &str) -> Option<PathBuf> {
        dir(Kind::Data, appname)
    }

    /// `~/.cache/<app>` (or `$XDG_CACHE_HOME`).
    pub fn cache(appname: &str) -> Option<PathBuf> {
        dir(Kind::Cache, appname)
    }

    /// `~/.local/state/<app>` (or `$XDG_STATE_HOME`).
    pub fn state(appname: &str) -> Option<PathBuf> {
        dir(Kind::State, appname)
    }

    /// A file path inside an application directory (parents created).
    /// The file itself is not created.
    pub fn file(kind: Kind, appname: &str, relative: &str) -> Option<PathBuf> {
        take_path(unsafe {
            ffi::sc_path_file(
                kind.raw(),
                cstring(appname).as_ptr(),
                cstring(relative).as_ptr(),
            )
        })
    }
}

/// Options for [`Pager`]. Zero-init (`Default`) pages through `$PAGER` /
/// `less -R`, and only when the output stream is a terminal.
#[derive(Clone, Debug, Default)]
pub struct PagerOpts {
    /// Pager command (split on whitespace, no shell); `None` = `$PAGER`,
    /// then `less -R`.
    pub command: Option<String>,

    /// Page even when the output stream is not a terminal.
    pub always: bool,
}

impl PagerOpts {
    pub fn new() -> Self {
        Self::default()
    }

    /// Sets an explicit pager command.
    pub fn command(mut self, command: &str) -> Self {
        self.command = Some(command.to_string());
        self
    }

    /// Pages even off-terminal.
    pub fn always(mut self) -> Self {
        self.always = true;
        self
    }
}

/// A pager session: output on this thread is piped through the pager until
/// [`end`](Pager::end) is called (or the value is dropped).
///
/// No-op when the output stream is not a terminal (unless
/// [`PagerOpts::always`]), so the same code works in scripts and pipes.
///
/// ```no_run
/// let pager = sparcli::Pager::begin(sparcli::PagerOpts::new());
/// sparcli::markup::println("[bold]lots of output…[/]");
/// let status = pager.end();
/// ```
pub struct Pager {
    ptr: *mut ffi::ScPager,
}

impl Pager {
    /// Starts a pager session (no-op off terminal unless `opts.always`).
    pub fn begin(opts: PagerOpts) -> Pager {
        let command = opts.command.as_deref().map(cstring);
        let raw_opts = ffi::ScPagerOpts {
            command: command
                .as_ref()
                .map_or(std::ptr::null(), |c| c.as_ptr()),
            always: opts.always,
        };
        let ptr = unsafe { ffi::sc_pager_begin(raw_opts) };
        Pager { ptr }
    }

    /// Ends the session, restores the output stream, and returns the
    /// pager's exit status (0 for no-op sessions).
    pub fn end(mut self) -> i32 {
        self.finish()
    }

    /// Shared end path for `end()` and `Drop`.
    fn finish(&mut self) -> i32 {
        if self.ptr.is_null() {
            return 0;
        }
        let status = unsafe { ffi::sc_pager_end(self.ptr) };
        self.ptr = std::ptr::null_mut();
        status
    }
}

impl Drop for Pager {
    fn drop(&mut self) {
        self.finish();
    }
}
