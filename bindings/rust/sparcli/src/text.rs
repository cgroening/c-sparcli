//! Owning multi-span rich text (`Text`) and inline markup.

use crate::style::{cstring, AnsiMode, Style};
use sparcli_sys as ffi;

/// An owning, multi-span rich-text object. Every `append` copies its input, so
/// no borrowed string needs to outlive the call. Freed on drop.
pub struct Text {
    ptr: *mut ffi::ScText,
}

impl Text {
    /// An empty text. Panics on allocation failure (as Rust does globally).
    pub fn new() -> Text {
        let ptr = unsafe { ffi::sc_text_new() };
        assert!(!ptr.is_null(), "sc_text_new: out of memory");
        Text { ptr }
    }

    /// A text from one unstyled span.
    pub fn from_text(s: &str) -> Text {
        let mut t = Text::new();
        t.append(s, Style::default());
        t
    }

    /// Parses inline markup (`[bold red]…[/]`) into a `Text`.
    pub fn markup(m: &str) -> Text {
        let ptr = unsafe { ffi::sc_markup_parse(cstring(m).as_ptr()) };
        assert!(!ptr.is_null(), "sc_markup_parse: out of memory");
        Text { ptr }
    }

    /// Appends a styled span (the string is copied).
    pub fn append(&mut self, s: &str, style: Style) -> &mut Self {
        unsafe {
            ffi::sc_text_append(self.ptr, cstring(s).as_ptr(), style.raw())
        };
        self
    }

    /// Appends parsed markup.
    pub fn append_markup(&mut self, m: &str) -> &mut Self {
        unsafe { ffi::sc_markup_append(self.ptr, cstring(m).as_ptr()) };
        self
    }

    /// Prints to the current output stream.
    pub fn print(&self) {
        unsafe { ffi::sc_print_text(self.ptr) };
    }

    /// Maximum visible column width across lines (ANSI/UTF-8 aware).
    pub fn visible_width(&self) -> usize {
        unsafe { ffi::sc_text_visible_width(self.ptr) }
    }

    /// Number of spans.
    pub fn span_count(&self) -> usize {
        unsafe { ffi::sc_text_span_count(self.ptr) }
    }

    pub(crate) fn as_ptr(&self) -> *const ffi::ScText {
        self.ptr
    }
    pub(crate) fn as_mut_ptr(&self) -> *mut ffi::ScText {
        self.ptr
    }
    /// Borrowed raw `ScText*` (escape hatch); not owned.
    pub fn as_raw(&self) -> *mut ffi::ScText {
        self.ptr
    }
}

impl Default for Text {
    fn default() -> Self {
        Text::new()
    }
}

impl Drop for Text {
    fn drop(&mut self) {
        unsafe { ffi::sc_text_free(self.ptr) };
    }
}

/// Options for the markup parser.
#[derive(Clone, Copy, Debug, Default)]
pub struct MarkupOpts {
    /// Silently drop unrecognized tags (keep their inner text) instead of
    /// emitting the brackets verbatim.
    pub strip_unknown: bool,

    /// ANSI passthrough for raw escape bytes in the markup text; the default
    /// inherits the process-wide [`set_allow_ansi`](crate::set_allow_ansi)
    /// setting.
    pub ansi: AnsiMode,
}

impl MarkupOpts {
    pub(crate) fn raw(self) -> ffi::ScMarkupOpts {
        ffi::ScMarkupOpts {
            strip_unknown: self.strip_unknown,
            ansi: self.ansi.raw(),
        }
    }
}

/// Rich-compatible inline markup helpers.
pub mod markup {
    use super::*;

    /// Parses markup into a [`Text`].
    pub fn parse(m: &str) -> Text {
        Text::markup(m)
    }

    /// Parses markup with parser options.
    pub fn parse_opts(m: &str, opts: MarkupOpts) -> Text {
        let ptr = unsafe {
            ffi::sc_markup_parse_opts(cstring(m).as_ptr(), opts.raw())
        };
        assert!(!ptr.is_null(), "sc_markup_parse_opts: out of memory");
        // Wrap without re-exposing the private ctor: reuse from_raw.
        super::from_raw(ptr)
    }

    /// Prints markup to the current output stream (no trailing newline).
    pub fn print(m: &str) {
        unsafe { ffi::sc_markup_print(cstring(m).as_ptr()) };
    }

    /// Prints markup followed by a newline.
    pub fn println(m: &str) {
        unsafe { ffi::sc_markup_println(cstring(m).as_ptr()) };
    }

    /// Prints markup with parser options + newline.
    pub fn println_opts(m: &str, opts: MarkupOpts) {
        unsafe { ffi::sc_markup_println_opts(cstring(m).as_ptr(), opts.raw()) };
    }
}

pub(crate) fn from_raw(ptr: *mut ffi::ScText) -> Text {
    Text { ptr }
}
