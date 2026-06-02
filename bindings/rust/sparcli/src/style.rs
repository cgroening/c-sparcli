//! Colors, text styles, alignment and the small enums shared across widgets.

use sparcli_sys as ffi;
use std::ffi::CString;
use std::os::raw::c_char;

/// A color: unset (the default), one of the 8 named ANSI colors, or 24-bit RGB.
///
/// The zero value [`Color::NONE`] emits no escape codes, matching the C
/// library's zero-init sentinel.
#[derive(Clone, Copy, Debug)]
pub struct Color(pub(crate) ffi::ScColor);

impl Default for Color {
    fn default() -> Self {
        Color::NONE
    }
}

macro_rules! named_color {
    ($name:ident, $idx:expr) => {
        pub const $name: Color = Color(ffi::ScColor {
            index: $idx,
            r: 0,
            g: 0,
            b: 0,
        });
    };
}

impl Color {
    named_color!(NONE, 0);
    named_color!(BLACK, 1);
    named_color!(RED, 2);
    named_color!(GREEN, 3);
    named_color!(YELLOW, 4);
    named_color!(BLUE, 5);
    named_color!(MAGENTA, 6);
    named_color!(CYAN, 7);
    named_color!(WHITE, 8);

    /// A 24-bit truecolor.
    pub const fn rgb(r: u8, g: u8, b: u8) -> Color {
        Color(ffi::ScColor { index: -1, r, g, b })
    }

    pub(crate) fn raw(self) -> ffi::ScColor {
        self.0
    }
}

/// Text attribute flags; combine with `|`.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Attr(pub(crate) u32);

impl Attr {
    pub const NONE: Attr = Attr(0);
    pub const BOLD: Attr = Attr(1);
    pub const DIM: Attr = Attr(2);
    pub const ITALIC: Attr = Attr(4);
    pub const UNDERLINE: Attr = Attr(8);
}

impl std::ops::BitOr for Attr {
    type Output = Attr;
    fn bitor(self, rhs: Attr) -> Attr {
        Attr(self.0 | rhs.0)
    }
}

/// A styled text span: attributes + foreground + background.
#[derive(Clone, Copy, Debug, Default)]
pub struct Style {
    pub attr: Attr,
    pub fg: Color,
    pub bg: Color,
}

impl Style {
    pub fn new() -> Self {
        Style::default()
    }
    pub fn attr(mut self, a: Attr) -> Self {
        self.attr = a;
        self
    }
    pub fn fg(mut self, c: Color) -> Self {
        self.fg = c;
        self
    }
    pub fn bg(mut self, c: Color) -> Self {
        self.bg = c;
        self
    }
    /// Convenience: bold.
    pub fn bold() -> Self {
        Style::new().attr(Attr::BOLD)
    }
    /// Convenience: dim.
    pub fn dim() -> Self {
        Style::new().attr(Attr::DIM)
    }
    /// Convenience: italic.
    pub fn italic() -> Self {
        Style::new().attr(Attr::ITALIC)
    }

    pub(crate) fn raw(self) -> ffi::ScTextStyle {
        ffi::ScTextStyle {
            attr: self.attr.0,
            fg: self.fg.0,
            bg: self.bg.0,
        }
    }
}

macro_rules! repr_enum {
    ($(#[$m:meta])* $name:ident
     { $($variant:ident = $val:path),+ $(,)? } default $def:ident) => {
        $(#[$m])*
        #[derive(Clone, Copy, Debug, PartialEq, Eq)]
        #[repr(u32)]
        pub enum $name { $($variant = $val as u32),+ }
        impl Default for $name { fn default() -> Self { $name::$def } }
        impl $name { pub(crate) fn raw(self) -> u32 { self as u32 } }
    };
}

repr_enum!(
    /// Horizontal alignment.
    Align {
        Left = ffi::ScHAlign_SC_ALIGN_LEFT,
        Center = ffi::ScHAlign_SC_ALIGN_CENTER,
        Right = ffi::ScHAlign_SC_ALIGN_RIGHT,
    } default Left
);

repr_enum!(
    /// Vertical alignment.
    VAlign {
        Top = ffi::ScVAlign_SC_VALIGN_TOP,
        Middle = ffi::ScVAlign_SC_VALIGN_MIDDLE,
        Bottom = ffi::ScVAlign_SC_VALIGN_BOTTOM,
    } default Top
);

repr_enum!(
    /// Border / line character set.
    BorderType {
        None = ffi::ScBorderType_SC_BORDER_NONE,
        Ascii = ffi::ScBorderType_SC_BORDER_ASCII,
        Single = ffi::ScBorderType_SC_BORDER_SINGLE,
        Double = ffi::ScBorderType_SC_BORDER_DOUBLE,
        Rounded = ffi::ScBorderType_SC_BORDER_ROUNDED,
        Thick = ffi::ScBorderType_SC_BORDER_THICK,
    } default None
);

repr_enum!(
    /// Title placement edge (panels/tables).
    Position {
        Top = ffi::ScPosition_SC_POSITION_TOP,
        Bottom = ffi::ScPosition_SC_POSITION_BOTTOM,
    } default Top
);

repr_enum!(
    /// Per-widget ANSI passthrough for user strings; `Default` inherits the
    /// process-wide [`set_allow_ansi`](crate::set_allow_ansi) setting.
    AnsiMode {
        Default = ffi::ScAnsiMode_SC_ANSI_MODE_DEFAULT,
        Allow = ffi::ScAnsiMode_SC_ANSI_MODE_ALLOW,
        Sanitize = ffi::ScAnsiMode_SC_ANSI_MODE_SANITIZE,
    } default Default
);

repr_enum!(
    /// Key-hint footer layout.
    HintLayout {
        Default = ffi::ScHintLayout_SC_HINT_LAYOUT_DEFAULT,
        Inline = ffi::ScHintLayout_SC_HINT_INLINE,
        Stacked = ffi::ScHintLayout_SC_HINT_STACKED,
        Hidden = ffi::ScHintLayout_SC_HINT_HIDDEN,
    } default Default
);

repr_enum!(
    /// Key-hint footer placement.
    HintPos {
        Default = ffi::ScHintPosition_SC_HINT_POS_DEFAULT,
        Top = ffi::ScHintPosition_SC_HINT_POS_TOP,
        Bottom = ffi::ScHintPosition_SC_HINT_POS_BOTTOM,
        Left = ffi::ScHintPosition_SC_HINT_POS_LEFT,
        Right = ffi::ScHintPosition_SC_HINT_POS_RIGHT,
    } default Default
);

/// Inner/outer spacing: top, right, bottom, left (in columns/lines).
#[derive(Clone, Copy, Debug, Default)]
pub struct Edges {
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
    pub left: i32,
}

impl Edges {
    /// Same value on all four edges.
    pub fn all(v: i32) -> Self {
        Edges {
            top: v,
            right: v,
            bottom: v,
            left: v,
        }
    }
    /// Vertical (top/bottom) and horizontal (left/right).
    pub fn symmetric(v: i32, h: i32) -> Self {
        Edges {
            top: v,
            right: h,
            bottom: v,
            left: h,
        }
    }
    pub(crate) fn raw(self) -> ffi::ScEdges {
        ffi::ScEdges {
            top: self.top,
            right: self.right,
            bottom: self.bottom,
            left: self.left,
        }
    }
}

/// A border bundle: character set + foreground + background colors.
#[derive(Clone, Copy, Debug, Default)]
pub struct BorderStyle {
    pub kind: BorderType,
    pub color: Color,
    pub bg: Color,
}

impl BorderStyle {
    pub fn new(kind: BorderType) -> Self {
        BorderStyle {
            kind,
            color: Color::NONE,
            bg: Color::NONE,
        }
    }
    pub fn color(mut self, c: Color) -> Self {
        self.color = c;
        self
    }
    pub fn bg(mut self, c: Color) -> Self {
        self.bg = c;
        self
    }
    pub(crate) fn raw(self) -> ffi::ScBorderStyle {
        ffi::ScBorderStyle {
            type_: self.kind.raw(),
            color: self.color.0,
            bg: self.bg.0,
        }
    }
}

/// A growable arena of `CString`s that backs the borrowed `*const c_char`
/// pointers handed to the C API for the duration of one call. Keep it alive
/// until the C call returns.
#[derive(Default)]
pub(crate) struct Arena {
    strings: Vec<CString>,
}

impl Arena {
    pub(crate) fn new() -> Self {
        Arena::default()
    }
    /// Interns `s` (interior NUL bytes are stripped) and returns a borrowed
    /// pointer valid until the arena is dropped.
    pub(crate) fn cstr(&mut self, s: &str) -> *const c_char {
        let cleaned: CString = match CString::new(s) {
            Ok(c) => c,
            Err(_) => CString::new(s.replace('\0', "")).unwrap(),
        };
        self.strings.push(cleaned);
        self.strings.last().unwrap().as_ptr()
    }
    /// Like [`cstr`](Self::cstr) but returns NULL for `None`.
    pub(crate) fn opt(&mut self, s: Option<&str>) -> *const c_char {
        match s {
            Some(s) => self.cstr(s),
            None => std::ptr::null(),
        }
    }
}

/// Converts a borrowed string into a temporary `CString`, stripping interior
/// NULs. Used for one-shot calls where no arena is needed.
pub(crate) fn cstring(s: &str) -> CString {
    CString::new(s)
        .unwrap_or_else(|_| CString::new(s.replace('\0', "")).unwrap())
}
