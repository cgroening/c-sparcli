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

    /// Resolves a color *name* - the eight ANSI names (`"red"`, `"green"`, …)
    /// or the [`palette`] names (`"accent"`, `"orange"`, `"error"`, the
    /// `*_vivid`/`*_dark` variants, …) - into a [`Color`]. Returns `None` for
    /// an unknown name.
    ///
    /// This is the same name resolver markup (`[accent]`) and the CLI
    /// (`--color accent`) use. Hex strings (`#rrggbb`) are not names; build
    /// those with [`Color::rgb`].
    pub fn by_name(name: &str) -> Option<Color> {
        let mut out: ffi::ScColor = unsafe { std::mem::zeroed() };
        let ok =
            unsafe { ffi::sc_color_by_name(cstring(name).as_ptr(), &mut out) };
        ok.then_some(Color(out))
    }

    pub(crate) fn raw(self) -> ffi::ScColor {
        self.0
    }
}

impl PartialEq for Color {
    fn eq(&self, other: &Self) -> bool {
        self.0.index == other.0.index
            && self.0.r == other.0.r
            && self.0.g == other.0.g
            && self.0.b == other.0.b
    }
}
impl Eq for Color {}

/// Text attribute flags; combine with `|`.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Attr(pub(crate) u32);

impl Attr {
    pub const NONE: Attr = Attr(0);
    pub const BOLD: Attr = Attr(1);
    pub const DIM: Attr = Attr(2);
    pub const ITALIC: Attr = Attr(4);
    pub const UNDERLINE: Attr = Attr(8);
    pub const STRIKE: Attr = Attr(16);
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
        impl $name {
            #[allow(dead_code)]
            pub(crate) fn raw(self) -> u32 { self as u32 }
        }
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
    /// What a full-screen vertical alignment applies to (forms): `All` aligns
    /// the whole header+grid+footer block; `Content` pins the header to the top
    /// and the footer to the bottom, aligning only the grid in between.
    ValignScope {
        All = ffi::ScVAlignScope_SC_VALIGN_SCOPE_ALL,
        Content = ffi::ScVAlignScope_SC_VALIGN_SCOPE_CONTENT,
    } default All
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

repr_enum!(
    /// How a [`BoxStyle`] resolves its width. `Default` is per-widget
    /// (list/choice widgets fit content, text-entry widgets span full width).
    WidthMode {
        Default = ffi::ScWidthMode_SC_WIDTH_DEFAULT,
        Content = ffi::ScWidthMode_SC_WIDTH_CONTENT,
        Fixed = ffi::ScWidthMode_SC_WIDTH_FIXED,
        Full = ffi::ScWidthMode_SC_WIDTH_FULL,
    } default Default
);

repr_enum!(
    /// How far a widget background / row highlight extends.
    BgExtent {
        Widget = ffi::ScBgExtent_SC_BG_EXTENT_WIDGET,
        Text = ffi::ScBgExtent_SC_BG_EXTENT_TEXT,
    } default Widget
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

/// Frames an input widget inside a panel: an optional border, a content
/// background, inner padding and an outer margin. The default is *no frame*
/// (the widget renders inline); set [`BoxStyle::enabled`] (or use
/// [`BoxStyle::new`]) to draw it.
#[derive(Clone, Copy, Debug, Default)]
pub struct BoxStyle {
    /// Render the widget inside a panel frame.
    pub enabled: bool,
    /// Frame border; a [`BorderType::None`] kind defaults to rounded.
    pub border: BorderStyle,
    /// Content-area background color.
    pub bg: Color,
    /// Inner padding; all-zero selects the default of one column left/right.
    pub padding: Edges,
    /// Outer margin around the frame.
    pub margin: Edges,
    /// Width mode; `Default` is per-widget (lists fit content, text = full).
    pub width_mode: WidthMode,
    /// Fixed width in columns (for `Fixed`, or `Default` when non-zero).
    pub width: i32,
    /// Lower width clamp for `Content`; `0` = none.
    pub min_width: i32,
    /// Upper width clamp for `Content`; `0` = none.
    pub max_width: i32,
    /// How far the background / row highlights extend.
    pub bg_extent: BgExtent,
}

impl BoxStyle {
    /// A box with the given border, enabled.
    pub fn new(border: BorderStyle) -> Self {
        BoxStyle {
            enabled: true,
            border,
            ..Default::default()
        }
    }
    /// Enable or disable the frame.
    pub fn enabled(mut self, on: bool) -> Self {
        self.enabled = on;
        self
    }
    /// Set the frame border (and enable the frame).
    pub fn border(mut self, b: BorderStyle) -> Self {
        self.border = b;
        self.enabled = true;
        self
    }
    /// Set the content-area background color.
    pub fn bg(mut self, c: Color) -> Self {
        self.bg = c;
        self
    }
    /// Set the inner padding.
    pub fn padding(mut self, e: Edges) -> Self {
        self.padding = e;
        self
    }
    /// Set the outer margin.
    pub fn margin(mut self, e: Edges) -> Self {
        self.margin = e;
        self
    }
    /// Set a fixed frame/field width in columns.
    pub fn width(mut self, w: i32) -> Self {
        self.width_mode = WidthMode::Fixed;
        self.width = w;
        self
    }
    /// Fit the content, clamped to `[min, max]` (`0` = no clamp).
    pub fn width_content(mut self, min: i32, max: i32) -> Self {
        self.width_mode = WidthMode::Content;
        self.min_width = min;
        self.max_width = max;
        self
    }
    /// Span the full terminal width.
    pub fn width_full(mut self) -> Self {
        self.width_mode = WidthMode::Full;
        self
    }
    /// Set how far the background / row highlights extend.
    pub fn bg_extent(mut self, e: BgExtent) -> Self {
        self.bg_extent = e;
        self
    }
    pub(crate) fn raw(self) -> ffi::ScBoxStyle {
        ffi::ScBoxStyle {
            enabled: self.enabled,
            border: self.border.raw(),
            bg: self.bg.0,
            padding: self.padding.raw(),
            margin: self.margin.raw(),
            width_mode: self.width_mode.raw(),
            width: self.width,
            min_width: self.min_width,
            max_width: self.max_width,
            bg_extent: self.bg_extent.raw(),
        }
    }
}

/// Named RGB palette: the C `SC_COLOR_*` constants as truecolor [`Color`]s,
/// additional to the eight ANSI [`Color`] constants. Use as
/// `sparcli::palette::ACCENT`.
pub mod palette {
    use super::Color;
    // Standard colors
    pub const RED: Color = Color::rgb(243, 139, 139);
    pub const ORANGE: Color = Color::rgb(248, 178, 136);
    pub const YELLOW: Color = Color::rgb(249, 230, 175);
    pub const GREEN: Color = Color::rgb(165, 227, 164);
    pub const CYAN: Color = Color::rgb(148, 225, 239);
    pub const BLUE: Color = Color::rgb(186, 213, 255);
    pub const PURPLE: Color = Color::rgb(207, 173, 247);
    pub const MAGENTA: Color = Color::rgb(245, 159, 224);
    pub const BLACK: Color = Color::rgb(0, 0, 0);
    pub const WHITE: Color = Color::rgb(255, 255, 255);
    // Vivid variants
    pub const RED_VIVID: Color = Color::rgb(255, 69, 87);
    pub const ORANGE_VIVID: Color = Color::rgb(255, 140, 58);
    pub const YELLOW_VIVID: Color = Color::rgb(255, 213, 81);
    pub const GREEN_VIVID: Color = Color::rgb(0, 227, 53);
    pub const CYAN_VIVID: Color = Color::rgb(64, 230, 255);
    pub const BLUE_VIVID: Color = Color::rgb(108, 165, 255);
    pub const PURPLE_VIVID: Color = Color::rgb(175, 77, 255);
    pub const MAGENTA_VIVID: Color = Color::rgb(255, 81, 223);
    // Dark variants
    pub const RED_DARK: Color = Color::rgb(65, 11, 16);
    pub const ORANGE_DARK: Color = Color::rgb(63, 30, 7);
    pub const YELLOW_DARK: Color = Color::rgb(60, 48, 12);
    pub const GREEN_DARK: Color = Color::rgb(0, 49, 5);
    pub const CYAN_DARK: Color = Color::rgb(8, 54, 61);
    pub const BLUE_DARK: Color = Color::rgb(21, 38, 64);
    pub const PURPLE_DARK: Color = Color::rgb(43, 13, 67);
    pub const MAGENTA_DARK: Color = Color::rgb(64, 14, 55);
    // Background / foreground
    pub const BG: Color = Color::rgb(21, 21, 21);
    pub const BG_DARKEN_1: Color = Color::rgb(17, 17, 17);
    pub const BG_DARKEN_2: Color = Color::rgb(12, 12, 12);
    pub const BG_LIGHTEN_1: Color = Color::rgb(26, 26, 26);
    pub const BG_LIGHTEN_2: Color = Color::rgb(37, 37, 37);
    pub const BG_LIGHTEN_3: Color = Color::rgb(43, 43, 43);
    pub const BG_SELECTED: Color = Color::rgb(3, 101, 198);
    pub const FG: Color = Color::rgb(212, 212, 212);
    pub const FG_DARKEN_1: Color = Color::rgb(204, 204, 204);
    pub const FG_DARKEN_2: Color = Color::rgb(187, 187, 187);
    pub const FG_DARKEN_3: Color = Color::rgb(170, 170, 170);
    pub const FG_LIGHTEN_1: Color = Color::rgb(238, 238, 238);
    pub const FG_LIGHTEN_2: Color = Color::rgb(253, 253, 253);
    // Accent colors
    pub const ACCENT: Color = Color::rgb(140, 210, 204);
    pub const ACCENT_DIM: Color = Color::rgb(113, 162, 157);
    pub const ACCENT_DARKER: Color = Color::rgb(79, 114, 111);
    pub const ACCENT_DARK: Color = Color::rgb(49, 73, 71);
    pub const ACCENT_IMPORTANT: Color = Color::rgb(255, 236, 150);
    // Status colors
    pub const ENABLED: Color = Color::rgb(112, 223, 129);
    pub const DISABLED: Color = Color::rgb(224, 108, 117);
    pub const DISABLED_DIM: Color = Color::rgb(128, 128, 128);
    // Diagnostics
    pub const ERROR: Color = Color::rgb(244, 135, 113);
    pub const WARNING: Color = Color::rgb(255, 185, 84);
    pub const SUCCESS: Color = Color::rgb(166, 227, 161);
    pub const INFO: Color = Color::rgb(148, 225, 239);
    pub const HINT: Color = Color::rgb(170, 170, 170);
    pub const UNUSED: Color = Color::rgb(98, 98, 98);

    use sparcli_sys as ffi;
    use super::cstring;

    /// Overrides the color a *name* resolves to at runtime.
    ///
    /// Recolors any name accepted by [`Color::by_name`](super::Color::by_name)
    /// (the eight ANSI names and this palette, e.g. `"accent"`, `"accent_dark"`).
    /// The override is honored by markup (`[accent]`), the CLI (`--color accent`)
    /// and widget defaults that resolve a palette name (e.g. the fuzzy accent).
    /// Pass [`Color::NONE`](super::Color::NONE) to clear a single override.
    /// Set-once before spawning threads. Returns `false` for an unknown name.
    pub fn set(name: &str, color: Color) -> bool {
        unsafe { ffi::sc_palette_set(cstring(name).as_ptr(), color.raw()) }
    }

    /// Current effective value for *name* (override or default), or `None`.
    pub fn get(name: &str) -> Option<Color> {
        super::Color::by_name(name)
    }

    /// Clears every runtime palette override, restoring the defaults.
    pub fn reset() {
        unsafe { ffi::sc_palette_reset() }
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
