//! Logging: leveled, colored terminal output + plain-text file sinks.

use crate::style::{cstring, AnsiMode};
use sparcli_sys as ffi;

repr_enum!(
    /// Log severity, ordered from most to least verbose.
    LogLevel {
        Debug = ffi::ScLogLevel_SC_LOG_DEBUG,
        Info = ffi::ScLogLevel_SC_LOG_INFO,
        Warn = ffi::ScLogLevel_SC_LOG_WARN,
        Error = ffi::ScLogLevel_SC_LOG_ERROR,
        Off = ffi::ScLogLevel_SC_LOG_OFF,
    } default Debug
);

/// Format options for a [`Logger`] / the global logger.
#[derive(Clone, Copy, Debug, Default)]
pub struct LoggerOpts {
    /// Minimum level processed at all (default: everything).
    pub level: LogLevel,

    /// Hide the dim `[HH:MM:SS]` timestamp prefix.
    pub hide_timestamps: bool,

    /// Render the dim `file:line` suffix (only useful from C macros).
    pub show_source: bool,

    /// Parse `[tag]` markup in messages.
    pub markup: bool,

    /// ANSI passthrough for message text.
    pub ansi: AnsiMode,
}

impl LoggerOpts {
    pub fn new() -> Self {
        Self::default()
    }

    /// Sets the minimum level.
    pub fn level(mut self, level: LogLevel) -> Self {
        self.level = level;
        self
    }

    /// Hides the timestamp prefix.
    pub fn hide_timestamps(mut self) -> Self {
        self.hide_timestamps = true;
        self
    }

    /// Parses `[tag]` markup in messages.
    pub fn markup(mut self) -> Self {
        self.markup = true;
        self
    }

    pub(crate) fn raw(self) -> ffi::ScLoggerOpts {
        ffi::ScLoggerOpts {
            level: self.level.raw(),
            hide_timestamps: self.hide_timestamps,
            show_source: self.show_source,
            markup: self.markup,
            ansi: self.ansi.raw(),
        }
    }
}

/// A handle-based logger with its own file sinks - for a subsystem that
/// must not touch the process-wide logger.
///
/// Terminal output is the global logger's job (see [`log`]); handle-based
/// loggers write to plain-text files. Messages are always passed as data,
/// never as a printf format string.
///
/// ```no_run
/// use sparcli::{Logger, LoggerOpts, LogLevel};
/// let logger = Logger::new(LoggerOpts::new().hide_timestamps())
///     .add_file("subsystem.log", LogLevel::Debug);
/// logger.info("subsystem started");
/// ```
pub struct Logger {
    ptr: *mut ffi::ScLogger,
}

impl Logger {
    /// A new logger with no sinks. Panics on allocation failure.
    pub fn new(opts: LoggerOpts) -> Logger {
        let ptr = unsafe { ffi::sc_logger_new(opts.raw()) };
        assert!(!ptr.is_null(), "sc_logger_new: out of memory");
        Logger { ptr }
    }

    /// Adds a plain-text file sink (append mode; closed on drop).
    pub fn add_file(self, path: &str, min_level: LogLevel) -> Self {
        unsafe {
            ffi::sc_logger_add_file(
                self.ptr,
                cstring(path).as_ptr(),
                min_level.raw(),
            )
        };
        self
    }

    /// Sets the logger-wide minimum level.
    pub fn set_level(&mut self, level: LogLevel) {
        unsafe { ffi::sc_logger_set_level(self.ptr, level.raw()) };
    }

    /// Emits one record (the message is data, not a format string).
    pub fn log(&self, level: LogLevel, message: &str) {
        let format = cstring("%s");
        let text = cstring(message);
        unsafe {
            ffi::sc_logger_log(
                self.ptr,
                level.raw(),
                std::ptr::null(),
                0,
                format.as_ptr(),
                text.as_ptr(),
            )
        };
    }

    /// Emits a [`LogLevel::Debug`] record.
    pub fn debug(&self, message: &str) {
        self.log(LogLevel::Debug, message);
    }

    /// Emits a [`LogLevel::Info`] record.
    pub fn info(&self, message: &str) {
        self.log(LogLevel::Info, message);
    }

    /// Emits a [`LogLevel::Warn`] record.
    pub fn warn(&self, message: &str) {
        self.log(LogLevel::Warn, message);
    }

    /// Emits a [`LogLevel::Error`] record.
    pub fn error(&self, message: &str) {
        self.log(LogLevel::Error, message);
    }
}

impl Drop for Logger {
    fn drop(&mut self) {
        unsafe { ffi::sc_logger_free(self.ptr) };
    }
}

/// The process-wide global logger: a colored stderr sink (level
/// [`LogLevel::Info`] by default) plus optional file sinks.
///
/// Configure it once at startup (configuration is not thread-safe), then
/// log from any thread.
///
/// ```no_run
/// use sparcli::log;
/// log::set_level(log::LogLevel::Debug);
/// log::add_file("app.log", log::LogLevel::Debug);
/// log::info("started");
/// ```
pub mod log {
    pub use super::{Logger, LoggerOpts};
    use super::{cstring, ffi, LogLevel as Level};

    /// Re-export so `log::LogLevel` works.
    pub use super::LogLevel;

    /// Sets the stderr-sink level (default [`LogLevel::Info`]).
    pub fn set_level(level: Level) {
        unsafe { ffi::sc_log_set_level(level.raw()) };
    }

    /// The current stderr-sink level.
    pub fn level() -> Level {
        let raw = unsafe { ffi::sc_log_level() };
        match raw {
            ffi::ScLogLevel_SC_LOG_DEBUG => Level::Debug,
            ffi::ScLogLevel_SC_LOG_INFO => Level::Info,
            ffi::ScLogLevel_SC_LOG_WARN => Level::Warn,
            ffi::ScLogLevel_SC_LOG_ERROR => Level::Error,
            _ => Level::Off,
        }
    }

    /// Sets the global format options (timestamps, source, markup).
    pub fn set_opts(opts: LoggerOpts) {
        unsafe { ffi::sc_log_set_opts(opts.raw()) };
    }

    /// Adds a plain-text file sink to the global logger.
    pub fn add_file(path: &str, min_level: Level) -> bool {
        unsafe {
            ffi::sc_log_add_file(cstring(path).as_ptr(), min_level.raw())
        }
    }

    /// Closes file sinks and restores the default configuration.
    pub fn reset() {
        unsafe { ffi::sc_log_reset() };
    }

    /// Emits one record (the message is data, not a format string).
    pub fn log(level: Level, message: &str) {
        let format = cstring("%s");
        let text = cstring(message);
        unsafe {
            ffi::sc_log_log(
                level.raw(),
                std::ptr::null(),
                0,
                format.as_ptr(),
                text.as_ptr(),
            )
        };
    }

    /// Emits a [`LogLevel::Debug`] record on the global logger.
    pub fn debug(message: &str) {
        log(Level::Debug, message);
    }

    /// Emits a [`LogLevel::Info`] record on the global logger.
    pub fn info(message: &str) {
        log(Level::Info, message);
    }

    /// Emits a [`LogLevel::Warn`] record on the global logger.
    pub fn warn(message: &str) {
        log(Level::Warn, message);
    }

    /// Emits a [`LogLevel::Error`] record on the global logger.
    pub fn error(message: &str) {
        log(Level::Error, message);
    }
}
