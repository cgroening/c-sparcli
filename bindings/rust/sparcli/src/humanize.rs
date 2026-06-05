//! Human-readable formatting: byte sizes, durations, relative time and
//! grouped/compact numbers. Each function returns an owned `String`.

use crate::take_c_string;
use sparcli_sys as ffi;

repr_enum!(
    /// Unit system for [`bytes`]: `Si` (1000-based `KB`), `Iec` (1024-based
    /// `KiB`), `IecShort` (1024-based short `K`/`M`/`G`).
    ByteUnit {
        Si = ffi::ScByteUnit_SC_BYTES_SI,
        Iec = ffi::ScByteUnit_SC_BYTES_IEC,
        IecShort = ffi::ScByteUnit_SC_BYTES_IEC_SHORT,
    } default Si
);

/// Formatting options (zero-init = sensible defaults).
#[derive(Clone, Debug, Default)]
pub struct HumanizeOpts {
    /// Fixed number of fractional digits; `0` = per-function default.
    pub decimals: i32,
    /// Decimal separator; `None` = `'.'` (use `','` for de_DE).
    pub decimal_sep: Option<char>,
    /// Thousands separator for [`number`]; `None` = `','` (use `'.'` for de_DE).
    pub group_sep: Option<char>,
    /// Suppress the space between number and unit.
    pub no_space: bool,
}

impl HumanizeOpts {
    pub fn new() -> Self {
        HumanizeOpts::default()
    }
    pub fn decimals(mut self, n: i32) -> Self {
        self.decimals = n;
        self
    }
    pub fn decimal_sep(mut self, c: char) -> Self {
        self.decimal_sep = Some(c);
        self
    }
    pub fn group_sep(mut self, c: char) -> Self {
        self.group_sep = Some(c);
        self
    }
    pub fn no_space(mut self, on: bool) -> Self {
        self.no_space = on;
        self
    }
    fn raw(&self) -> ffi::ScHumanizeOpts {
        ffi::ScHumanizeOpts {
            decimals: self.decimals,
            decimal_sep: sep_byte(self.decimal_sep),
            group_sep: sep_byte(self.group_sep),
            no_space: self.no_space,
        }
    }
}

/// Maps an optional ASCII separator char to the C `char` field (`0` = unset).
fn sep_byte(c: Option<char>) -> std::os::raw::c_char {
    match c {
        Some(ch) => ch as u8 as std::os::raw::c_char,
        None => 0,
    }
}

/// Formats a byte count, e.g. `bytes(1536, ByteUnit::Si)` → `"1.5 KB"`.
pub fn bytes(n: u64, unit: ByteUnit) -> String {
    bytes_opts(n, unit, HumanizeOpts::default())
}

/// Options-taking variant of [`bytes`].
pub fn bytes_opts(n: u64, unit: ByteUnit, opts: HumanizeOpts) -> String {
    unsafe { take_c_string(ffi::sc_humanize_bytes_opts(n, unit.raw(), opts.raw())) }
}

/// Formats a number with thousands grouping, e.g. `1234567` → `"1,234,567"`.
pub fn number(value: f64, opts: HumanizeOpts) -> String {
    unsafe { take_c_string(ffi::sc_humanize_number(value, opts.raw())) }
}

/// Formats a number compactly, e.g. `12400` → `"12.4k"`.
pub fn compact(value: f64, opts: HumanizeOpts) -> String {
    unsafe { take_c_string(ffi::sc_humanize_compact(value, opts.raw())) }
}

/// Formats a ratio as a percentage, e.g. `0.42` → `"42%"`.
pub fn percent(ratio: f64, opts: HumanizeOpts) -> String {
    unsafe { take_c_string(ffi::sc_humanize_percent(ratio, opts.raw())) }
}

/// Formats a duration as two units, e.g. `93` → `"1m 33s"`.
pub fn duration(seconds: f64) -> String {
    unsafe { take_c_string(ffi::sc_humanize_duration(seconds)) }
}

/// Formats a duration as a clock value, e.g. `3725` → `"01:02:05"`.
pub fn duration_clock(seconds: f64) -> String {
    unsafe { take_c_string(ffi::sc_humanize_duration_clock(seconds)) }
}

/// Formats `when` relative to `now` (Unix seconds), e.g. `"3 hours ago"`.
pub fn relative(when: i64, now: i64) -> String {
    unsafe {
        take_c_string(ffi::sc_humanize_relative(
            when as ffi::time_t,
            now as ffi::time_t,
        ))
    }
}
