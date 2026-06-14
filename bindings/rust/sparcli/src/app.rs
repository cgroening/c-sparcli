//! Application-framework helpers: XDG base directories, pager, and
//! pretty errors.

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

/// Opens an external editor on an existing file, inheriting the controlling
/// terminal, and waits for it to exit. The file is edited in place (nothing is
/// captured) - re-read it yourself afterwards. Call only when no prompt session
/// is active.
///
/// `cmd` is the editor command (split on whitespace, no shell); `None` resolves
/// `$VISUAL`, then `$EDITOR`, then a platform default (`nvim`/`vi` on POSIX,
/// `notepad` on Windows). Returns the editor's exit code (`0` = clean), `127`
/// when the command was not found, or `-1` on a spawn failure or when no
/// controlling terminal is available.
///
/// ```no_run
/// let rc = sparcli::edit_file(None, "/tmp/note.md");
/// ```
pub fn edit_file(cmd: Option<&str>, path: &str) -> i32 {
    let c_cmd = cmd.map(cstring);
    let cmd_ptr = c_cmd
        .as_ref()
        .map_or(std::ptr::null(), |c| c.as_ptr());
    unsafe { ffi::sc_edit_file(cmd_ptr, cstring(path).as_ptr()) }
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

/// Renders a one-shot error (message + optional hint) to stderr and exits the
/// process with `code`. Convenience for the common single-message case; use
/// [`ErrorReport`] when you need a cause chain.
///
/// ```no_run
/// sparcli::die_msg(2, "config not found", Some("run 'app init'"));
/// ```
pub fn die_msg(code: i32, message: &str, hint: Option<&str>) -> ! {
    let msg = cstring(message);
    let hint = hint.map(cstring);
    let hint_ptr = hint.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
    unsafe { ffi::sc_die_msg(code, msg.as_ptr(), hint_ptr) };
    unreachable!("sc_die_msg never returns");
}

/// A structured error report rendered as a red alert panel: message,
/// cause chain, hint, and exit code.
///
/// The pretty replacement for `eprintln!` + `std::process::exit` in CLI
/// binaries.
///
/// ```no_run
/// sparcli::ErrorReport::new("Config could not be loaded")
///     .cause("file not found: ~/.config/app/config.toml")
///     .hint("Run 'app init' to create a default config")
///     .code(2)
///     .die();
/// ```
pub struct ErrorReport {
    ptr: *mut ffi::ScError,
}

impl ErrorReport {
    /// A new report with the given message. Panics on allocation failure.
    pub fn new(message: &str) -> ErrorReport {
        let ptr = unsafe { ffi::sc_error_new(cstring(message).as_ptr()) };
        assert!(!ptr.is_null(), "sc_error_new: out of memory");
        ErrorReport { ptr }
    }

    /// Appends one entry to the cause chain (rendered as `caused by:`).
    pub fn cause(self, cause: &str) -> Self {
        unsafe { ffi::sc_error_add_cause(self.ptr, cstring(cause).as_ptr()) };
        self
    }

    /// Sets the hint line (rendered as `Hint:`).
    pub fn hint(self, hint: &str) -> Self {
        unsafe { ffi::sc_error_set_hint(self.ptr, cstring(hint).as_ptr()) };
        self
    }

    /// Sets the process exit code used by [`die`](ErrorReport::die)
    /// (default 1).
    pub fn code(self, exit_code: i32) -> Self {
        unsafe { ffi::sc_error_set_code(self.ptr, exit_code) };
        self
    }

    /// The configured exit code.
    pub fn exit_code(&self) -> i32 {
        unsafe { ffi::sc_error_code(self.ptr) }
    }

    /// Renders the report to the current output stream (does not exit).
    pub fn print(&self) {
        unsafe { ffi::sc_error_print(self.ptr) };
    }

    /// Renders the report to stderr (does not exit).
    pub fn print_stderr(&self) {
        unsafe { ffi::sc_error_print_stderr(self.ptr) };
    }

    /// Renders the report to stderr and exits the process with the
    /// configured exit code.
    pub fn die(self) -> ! {
        let ptr = self.ptr;
        std::mem::forget(self); // sc_die consumes (frees) the error
        unsafe { ffi::sc_die(ptr) };
        unreachable!("sc_die never returns");
    }
}

impl Drop for ErrorReport {
    fn drop(&mut self) {
        unsafe { ffi::sc_error_free(self.ptr) };
    }
}
