//! Error type for fallible operations (currently the interactive prompts).

use std::fmt;

/// Errors returned by sparcli operations.
#[derive(Debug, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum Error {
    /// No interactive terminal is available (output piped, CI, …), or the
    /// prompt could not run (read error / allocation failure). Callers can
    /// fall back to a default.
    Unavailable,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::Unavailable => f.write_str(
                "no interactive terminal available or prompt failed",
            ),
        }
    }
}

impl std::error::Error for Error {}

/// Shorthand for results in this crate.
pub type Result<T> = std::result::Result<T, Error>;
