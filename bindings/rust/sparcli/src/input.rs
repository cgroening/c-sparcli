//! Interactive input widgets, custom shortcuts and the external-editor flow.
//!
//! Every prompt returns `Result<Option<T>>`: `Ok(Some(v))` = a value,
//! `Ok(None)` = cancelled (Esc / Ctrl-C), `Err(Error::Unavailable)` = no TTY or
//! a read error.

use crate::error::{Error, Result};
use crate::style::{
    cstring, BorderStyle, BoxStyle, Color, HintLayout, HintPos, Style, VAlign,
    ValignScope,
};
use crate::output::Rendered;
use crate::text::Text;
use sparcli_sys as ffi;
use std::ptr::NonNull;

use std::cell::Cell;
use std::ffi::CString;
use std::mem;
use std::os::raw::{c_char, c_int, c_void};

repr_enum!(
    /// First weekday column in the date picker.
    WeekStart {
        Default = ffi::ScWeekStart_SC_WEEK_START_DEFAULT,
        Monday = ffi::ScWeekStart_SC_WEEK_START_MONDAY,
        Sunday = ffi::ScWeekStart_SC_WEEK_START_SUNDAY,
    } default Default
);

fn status(s: ffi::ScInputStatus) -> Result<bool> {
    match s {
        ffi::ScInputStatus_SC_INPUT_OK => Ok(true),
        ffi::ScInputStatus_SC_INPUT_CANCELLED => Ok(false),
        _ => Err(Error::Unavailable),
    }
}

/* ── Key chords + shortcuts ──────────────────────────────────────────────── */

/// A key combination for a custom shortcut or the editor key.
#[derive(Clone, Copy)]
pub struct Chord(pub(crate) ffi::ScKeyChord);

/// Ctrl + letter (Ctrl-C/H/I/J/M are reserved and not bindable).
pub fn key_ctrl(letter: char) -> Chord {
    Chord(unsafe { ffi::sc_key_ctrl(letter as c_char) })
}
/// Function key F1..F12.
pub fn key_fn(n: i32) -> Chord {
    Chord(unsafe { ffi::sc_key_fn(n) })
}
/// Alt/Meta + letter.
pub fn key_alt(letter: char) -> Chord {
    Chord(unsafe { ffi::sc_key_alt(letter as c_char) })
}
/// A bare character chord (no modifiers), e.g. `key_char('c')`. Useful for the
/// modal fuzzy finder's `clear_key` or bare-letter shortcuts.
pub fn key_char(letter: char) -> Chord {
    let mut c: ffi::ScKeyChord = unsafe { mem::zeroed() };
    c.key = ffi::ScKeyType_SC_KEY_CHAR;
    c.codepoint = letter as u32;
    c.mods = 0;
    Chord(c)
}

/// Chord for a named (non-character) key - arrows, Enter, Tab. Use these to
/// build e.g. Left = back / Right = forward navigation on any widget.
pub fn key_left() -> Chord {
    Chord(unsafe { ffi::sc_key_special(ffi::ScKeyType_SC_KEY_LEFT) })
}
/// Right-arrow chord. See [`key_left`].
pub fn key_right() -> Chord {
    Chord(unsafe { ffi::sc_key_special(ffi::ScKeyType_SC_KEY_RIGHT) })
}
/// Up-arrow chord. See [`key_left`].
pub fn key_up() -> Chord {
    Chord(unsafe { ffi::sc_key_special(ffi::ScKeyType_SC_KEY_UP) })
}
/// Down-arrow chord. See [`key_left`].
pub fn key_down() -> Chord {
    Chord(unsafe { ffi::sc_key_special(ffi::ScKeyType_SC_KEY_DOWN) })
}
/// Enter-key chord. See [`key_left`].
pub fn key_enter() -> Chord {
    Chord(unsafe { ffi::sc_key_special(ffi::ScKeyType_SC_KEY_ENTER) })
}
/// Tab-key chord. See [`key_left`].
pub fn key_tab() -> Chord {
    Chord(unsafe { ffi::sc_key_special(ffi::ScKeyType_SC_KEY_TAB) })
}
/// Shift-Tab (back-tab) chord. See [`key_left`].
pub fn key_backtab() -> Chord {
    Chord(unsafe { ffi::sc_key_special(ffi::ScKeyType_SC_KEY_BACKTAB) })
}
/// Delete-key chord. See [`key_left`].
pub fn key_delete() -> Chord {
    Chord(unsafe { ffi::sc_key_special(ffi::ScKeyType_SC_KEY_DELETE) })
}
/// Backspace-key chord. See [`key_left`].
pub fn key_backspace() -> Chord {
    Chord(unsafe { ffi::sc_key_special(ffi::ScKeyType_SC_KEY_BACKSPACE) })
}
/// Home-key chord. See [`key_left`].
pub fn key_home() -> Chord {
    Chord(unsafe { ffi::sc_key_special(ffi::ScKeyType_SC_KEY_HOME) })
}
/// End-key chord. See [`key_left`].
pub fn key_end() -> Chord {
    Chord(unsafe { ffi::sc_key_special(ffi::ScKeyType_SC_KEY_END) })
}
/// Page-Up chord. See [`key_left`].
pub fn key_pageup() -> Chord {
    Chord(unsafe { ffi::sc_key_special(ffi::ScKeyType_SC_KEY_PAGEUP) })
}
/// Page-Down chord. See [`key_left`].
pub fn key_pagedown() -> Chord {
    Chord(unsafe { ffi::sc_key_special(ffi::ScKeyType_SC_KEY_PAGEDOWN) })
}
/// Esc-key chord. (Note: Esc also cancels a prompt unless the widget captures
/// it; bindable only where that applies.) See [`key_left`].
pub fn key_esc() -> Chord {
    Chord(unsafe { ffi::sc_key_special(ffi::ScKeyType_SC_KEY_ESC) })
}

impl Chord {
    /// Adds the Shift modifier (for named keys, e.g. `key_up().shift()`).
    /// Shift on a letter folds into the character, so use it with named keys.
    pub fn shift(mut self) -> Chord {
        self.0.mods |= ffi::ScKeyMods_SC_MOD_SHIFT as u8;
        self
    }
    /// Adds the Alt/Meta modifier, e.g. `key_up().alt()` or `key_char('p').alt()`.
    pub fn alt(mut self) -> Chord {
        self.0.mods |= ffi::ScKeyMods_SC_MOD_ALT as u8;
        self
    }
    /// Adds the Ctrl modifier (for named keys, e.g. `key_right().ctrl()`).
    pub fn ctrl(mut self) -> Chord {
        self.0.mods |= ffi::ScKeyMods_SC_MOD_CTRL as u8;
        self
    }

    /// A short display name, e.g. `"F2"`, `"^E"`, `"M-e"`.
    pub fn name(&self) -> String {
        let mut buf = [0 as c_char; 16];
        unsafe { ffi::sc_key_chord_name(self.0, buf.as_mut_ptr(), buf.len()) };
        unsafe {
            std::ffi::CStr::from_ptr(buf.as_ptr())
                .to_string_lossy()
                .into_owned()
        }
    }

    /// Whether `key` matches this chord (raw decoder helper).
    pub fn matches(&self, key: &Key) -> bool {
        unsafe { ffi::sc_key_chord_matches(key.0, self.0) }
    }
}

/* ── Raw key decoding ────────────────────────────────────────────────────── */

repr_enum!(
    /// Logical identity of a decoded [`Key`]. `None` means no key could be
    /// decoded (EOF, or an incomplete escape/UTF-8 prefix needing more bytes).
    KeyType {
        None = ffi::ScKeyType_SC_KEY_NONE,
        Char = ffi::ScKeyType_SC_KEY_CHAR,
        Enter = ffi::ScKeyType_SC_KEY_ENTER,
        Esc = ffi::ScKeyType_SC_KEY_ESC,
        Tab = ffi::ScKeyType_SC_KEY_TAB,
        BackTab = ffi::ScKeyType_SC_KEY_BACKTAB,
        Backspace = ffi::ScKeyType_SC_KEY_BACKSPACE,
        Delete = ffi::ScKeyType_SC_KEY_DELETE,
        Up = ffi::ScKeyType_SC_KEY_UP,
        Down = ffi::ScKeyType_SC_KEY_DOWN,
        Left = ffi::ScKeyType_SC_KEY_LEFT,
        Right = ffi::ScKeyType_SC_KEY_RIGHT,
        Home = ffi::ScKeyType_SC_KEY_HOME,
        End = ffi::ScKeyType_SC_KEY_END,
        PageUp = ffi::ScKeyType_SC_KEY_PAGEUP,
        PageDown = ffi::ScKeyType_SC_KEY_PAGEDOWN,
        ShiftPageUp = ffi::ScKeyType_SC_KEY_SHIFT_PAGEUP,
        ShiftPageDown = ffi::ScKeyType_SC_KEY_SHIFT_PAGEDOWN,
        CtrlA = ffi::ScKeyType_SC_KEY_CTRL_A,
        CtrlC = ffi::ScKeyType_SC_KEY_CTRL_C,
        CtrlD = ffi::ScKeyType_SC_KEY_CTRL_D,
        CtrlE = ffi::ScKeyType_SC_KEY_CTRL_E,
        CtrlK = ffi::ScKeyType_SC_KEY_CTRL_K,
        CtrlU = ffi::ScKeyType_SC_KEY_CTRL_U,
        CtrlW = ffi::ScKeyType_SC_KEY_CTRL_W,
        F1 = ffi::ScKeyType_SC_KEY_F1,
        F2 = ffi::ScKeyType_SC_KEY_F2,
        F3 = ffi::ScKeyType_SC_KEY_F3,
        F4 = ffi::ScKeyType_SC_KEY_F4,
        F5 = ffi::ScKeyType_SC_KEY_F5,
        F6 = ffi::ScKeyType_SC_KEY_F6,
        F7 = ffi::ScKeyType_SC_KEY_F7,
        F8 = ffi::ScKeyType_SC_KEY_F8,
        F9 = ffi::ScKeyType_SC_KEY_F9,
        F10 = ffi::ScKeyType_SC_KEY_F10,
        F11 = ffi::ScKeyType_SC_KEY_F11,
        F12 = ffi::ScKeyType_SC_KEY_F12,
        PasteStart = ffi::ScKeyType_SC_KEY_PASTE_START,
        PasteEnd = ffi::ScKeyType_SC_KEY_PASTE_END,
        Resize = ffi::ScKeyType_SC_KEY_RESIZE,
    } default None
);

impl KeyType {
    fn from_raw(raw: ffi::ScKeyType) -> KeyType {
        // SAFETY: KeyType is `#[repr(u32)]` and covers every ScKeyType value;
        // an out-of-range value cannot occur because the decoder only ever
        // produces these. Fall back to `None` defensively all the same.
        match raw {
            ffi::ScKeyType_SC_KEY_CHAR => KeyType::Char,
            ffi::ScKeyType_SC_KEY_ENTER => KeyType::Enter,
            ffi::ScKeyType_SC_KEY_ESC => KeyType::Esc,
            ffi::ScKeyType_SC_KEY_TAB => KeyType::Tab,
            ffi::ScKeyType_SC_KEY_BACKTAB => KeyType::BackTab,
            ffi::ScKeyType_SC_KEY_BACKSPACE => KeyType::Backspace,
            ffi::ScKeyType_SC_KEY_DELETE => KeyType::Delete,
            ffi::ScKeyType_SC_KEY_UP => KeyType::Up,
            ffi::ScKeyType_SC_KEY_DOWN => KeyType::Down,
            ffi::ScKeyType_SC_KEY_LEFT => KeyType::Left,
            ffi::ScKeyType_SC_KEY_RIGHT => KeyType::Right,
            ffi::ScKeyType_SC_KEY_HOME => KeyType::Home,
            ffi::ScKeyType_SC_KEY_END => KeyType::End,
            ffi::ScKeyType_SC_KEY_PAGEUP => KeyType::PageUp,
            ffi::ScKeyType_SC_KEY_PAGEDOWN => KeyType::PageDown,
            ffi::ScKeyType_SC_KEY_SHIFT_PAGEUP => KeyType::ShiftPageUp,
            ffi::ScKeyType_SC_KEY_SHIFT_PAGEDOWN => KeyType::ShiftPageDown,
            ffi::ScKeyType_SC_KEY_CTRL_A => KeyType::CtrlA,
            ffi::ScKeyType_SC_KEY_CTRL_C => KeyType::CtrlC,
            ffi::ScKeyType_SC_KEY_CTRL_D => KeyType::CtrlD,
            ffi::ScKeyType_SC_KEY_CTRL_E => KeyType::CtrlE,
            ffi::ScKeyType_SC_KEY_CTRL_K => KeyType::CtrlK,
            ffi::ScKeyType_SC_KEY_CTRL_U => KeyType::CtrlU,
            ffi::ScKeyType_SC_KEY_CTRL_W => KeyType::CtrlW,
            ffi::ScKeyType_SC_KEY_F1 => KeyType::F1,
            ffi::ScKeyType_SC_KEY_F2 => KeyType::F2,
            ffi::ScKeyType_SC_KEY_F3 => KeyType::F3,
            ffi::ScKeyType_SC_KEY_F4 => KeyType::F4,
            ffi::ScKeyType_SC_KEY_F5 => KeyType::F5,
            ffi::ScKeyType_SC_KEY_F6 => KeyType::F6,
            ffi::ScKeyType_SC_KEY_F7 => KeyType::F7,
            ffi::ScKeyType_SC_KEY_F8 => KeyType::F8,
            ffi::ScKeyType_SC_KEY_F9 => KeyType::F9,
            ffi::ScKeyType_SC_KEY_F10 => KeyType::F10,
            ffi::ScKeyType_SC_KEY_F11 => KeyType::F11,
            ffi::ScKeyType_SC_KEY_F12 => KeyType::F12,
            ffi::ScKeyType_SC_KEY_PASTE_START => KeyType::PasteStart,
            ffi::ScKeyType_SC_KEY_PASTE_END => KeyType::PasteEnd,
            ffi::ScKeyType_SC_KEY_RESIZE => KeyType::Resize,
            _ => KeyType::None,
        }
    }
}

/// A decoded key event from [`decode_key`]: a logical [`KeyType`], the Unicode
/// codepoint (for `Char`) and a modifier bitmask (Ctrl / Alt / pasted).
#[derive(Clone, Copy)]
pub struct Key(ffi::ScKey);

impl Key {
    /// The logical key identity.
    pub fn kind(&self) -> KeyType {
        KeyType::from_raw(self.0.type_)
    }
    /// The character for a [`KeyType::Char`] key, if any.
    pub fn char_value(&self) -> Option<char> {
        if self.0.type_ == ffi::ScKeyType_SC_KEY_CHAR {
            char::from_u32(self.0.codepoint)
        } else {
            None
        }
    }
    /// The raw modifier bitmask.
    pub fn mods(&self) -> u8 {
        self.0.mods
    }
    /// Ctrl held (a Ctrl-letter not mapped to a named editing key).
    pub fn is_ctrl(&self) -> bool {
        self.0.mods & ffi::ScKeyMods_SC_MOD_CTRL as u8 != 0
    }
    /// Alt/Meta held (an `ESC` prefix preceded the key).
    pub fn is_alt(&self) -> bool {
        self.0.mods & ffi::ScKeyMods_SC_MOD_ALT as u8 != 0
    }
    /// Delivered as part of a bracketed paste.
    pub fn is_pasted(&self) -> bool {
        self.0.mods & ffi::ScKeyMods_SC_MOD_PASTED as u8 != 0
    }
}

/// Decodes a single key event from the front of `buf` (pure, no terminal).
///
/// Returns the decoded [`Key`] and the number of bytes consumed. A consumed
/// count of `0` with [`KeyType::None`] means `buf` holds only the prefix of an
/// incomplete sequence (a lone ESC or partial UTF-8/CSI) - read more and retry.
pub fn decode_key(buf: &[u8]) -> (Key, usize) {
    let mut key: ffi::ScKey = unsafe { mem::zeroed() };
    let n = unsafe {
        ffi::sc_key_decode(buf.as_ptr() as *const c_char, buf.len(), &mut key)
    };
    (Key(key), n)
}

type ShortcutFn = Box<dyn FnMut(i32) -> bool>;

unsafe extern "C" fn shortcut_tramp(id: c_int, user: *mut c_void) -> bool {
    let f = &mut *(user as *mut ShortcutFn);
    f(id)
}

/// A set of custom key shortcuts for a widget. RETURN-mode shortcuts end the
/// prompt and report their id via [`Shortcuts::fired`]; CALLBACK-mode shortcuts
/// run a closure and keep the prompt open unless it returns `false`.
///
/// Build it, attach with `opts.shortcuts(&sc)`, and keep it alive across the
/// `run`/prompt call.
pub struct Shortcuts {
    items: Vec<ffi::ScShortcut>,
    callbacks: Vec<*mut c_void>,
    fired: Box<Cell<c_int>>,
    /// Owns every string referenced by `items`/`help_rows` (footer labels, help
    /// texts, sections, derived key names) so the raw pointers stay valid.
    owned: Vec<CString>,
    help_rows: Vec<ffi::ScShortcutHelpRow>,
    current_section: *const c_char,
}

/// Per-shortcut display metadata for the footer and the help screen.
#[derive(Clone, Copy)]
pub struct ShortcutDisplay<'a> {
    /// Footer text; `None`/empty => not shown in the footer.
    pub footer: Option<&'a str>,
    /// Help-screen description; `None` => falls back to `footer`.
    pub help: Option<&'a str>,
    /// Explicit footer flag (set `false` to keep the binding but hide it).
    pub in_footer: bool,
}

impl Default for ShortcutDisplay<'_> {
    fn default() -> Self {
        ShortcutDisplay { footer: None, help: None, in_footer: true }
    }
}

impl Shortcuts {
    pub fn new() -> Self {
        Shortcuts {
            items: Vec::new(),
            callbacks: Vec::new(),
            fired: Box::new(Cell::new(-1)),
            owned: Vec::new(),
            help_rows: Vec::new(),
            current_section: std::ptr::null(),
        }
    }

    /// Interns a string, returning a stable pointer owned by `self`.
    fn intern(&mut self, s: &str) -> *const c_char {
        let c = cstring(s);
        let p = c.as_ptr();
        self.owned.push(c);
        p
    }

    /// Records the help-screen row for a binding (derived key name + desc).
    fn push_binding_help(&mut self, chord: Chord, desc: *const c_char) {
        let mut buf = [0 as c_char; 32];
        unsafe { ffi::sc_key_chord_name(chord.0, buf.as_mut_ptr(), buf.len()) };
        let name = unsafe { std::ffi::CStr::from_ptr(buf.as_ptr()) }
            .to_string_lossy()
            .into_owned();
        let key = self.intern(&name);
        self.help_rows.push(ffi::ScShortcutHelpRow {
            section: std::ptr::null(),
            key_display: key,
            desc,
        });
    }

    /// Binds a chord that ends the prompt and reports `id` via
    /// [`fired`](Self::fired). `label` (if any) shows in the footer and, unless
    /// a richer help text is set, in the help screen.
    pub fn on_return(
        self,
        chord: Chord,
        id: i32,
        label: Option<&str>,
    ) -> Self {
        self.on_return_d(
            chord,
            id,
            ShortcutDisplay { footer: label, ..Default::default() },
        )
    }

    /// Binds a chord that ends the prompt, with full display metadata
    /// (`footer`/`help`/`in_footer`) for the footer and the help screen.
    pub fn on_return_d(
        mut self,
        chord: Chord,
        id: i32,
        d: ShortcutDisplay,
    ) -> Self {
        let footer = d.footer.map_or(std::ptr::null(), |s| self.intern(s));
        let help = d.help.map_or(std::ptr::null(), |s| self.intern(s));
        self.items.push(ffi::ScShortcut {
            chord: chord.0,
            id,
            mode: ffi::ScShortcutMode_SC_SHORTCUT_RETURN,
            on_fire: None,
            user: std::ptr::null_mut(),
            hint_label: footer,
            help_text: help,
            section: self.current_section,
            hide_in_footer: !d.in_footer,
        });
        let desc = if help.is_null() { footer } else { help };
        self.push_binding_help(chord, desc);
        self
    }

    /// Binds a chord to a callback (returns `true` to keep the prompt open).
    pub fn on_callback<F>(
        self,
        chord: Chord,
        label: Option<&str>,
        f: F,
    ) -> Self
    where
        F: FnMut(i32) -> bool + 'static,
    {
        self.on_callback_d(
            chord,
            ShortcutDisplay { footer: label, ..Default::default() },
            f,
        )
    }

    /// Binds a chord to a callback, with full display metadata.
    pub fn on_callback_d<F>(
        mut self,
        chord: Chord,
        d: ShortcutDisplay,
        f: F,
    ) -> Self
    where
        F: FnMut(i32) -> bool + 'static,
    {
        let footer = d.footer.map_or(std::ptr::null(), |s| self.intern(s));
        let help = d.help.map_or(std::ptr::null(), |s| self.intern(s));
        let boxed: Box<ShortcutFn> = Box::new(Box::new(f));
        let user = Box::into_raw(boxed) as *mut c_void;
        self.callbacks.push(user);
        self.items.push(ffi::ScShortcut {
            chord: chord.0,
            id: 0,
            mode: ffi::ScShortcutMode_SC_SHORTCUT_CALLBACK,
            on_fire: Some(shortcut_tramp),
            user,
            hint_label: footer,
            help_text: help,
            section: self.current_section,
            hide_in_footer: !d.in_footer,
        });
        let desc = if help.is_null() { footer } else { help };
        self.push_binding_help(chord, desc);
        self
    }

    /// Opens a help-screen section: entries added afterwards group under
    /// `title` until the next `section` call.
    pub fn section(mut self, title: &str) -> Self {
        let p = self.intern(title);
        self.current_section = p;
        self.help_rows.push(ffi::ScShortcutHelpRow {
            section: p,
            key_display: std::ptr::null(),
            desc: std::ptr::null(),
        });
        self
    }

    /// Adds a help-screen-only row (no binding), e.g. to document a built-in
    /// widget key such as `↑/↓` "move cursor".
    pub fn help_row(mut self, key_display: &str, description: &str) -> Self {
        let key = self.intern(key_display);
        let desc = self.intern(description);
        self.help_rows.push(ffi::ScShortcutHelpRow {
            section: std::ptr::null(),
            key_display: key,
            desc,
        });
        self
    }

    /// The id of the RETURN-mode shortcut that ended the last run, or `-1`.
    pub fn fired(&self) -> i32 {
        self.fired.get()
    }

    /// The index of the first registered shortcut whose chord matches `key`,
    /// or `None`. Mirrors the prompt engine's own dispatch (`sc_shortcut_find`)
    /// for callers driving the raw [`decode_key`] loop themselves.
    pub fn find(&self, key: &Key) -> Option<usize> {
        let base = self.items.as_ptr();
        let hit =
            unsafe { ffi::sc_shortcut_find(key.0, base, self.items.len()) };
        if hit.is_null() {
            None
        } else {
            Some(unsafe { hit.offset_from(base) } as usize)
        }
    }

    fn apply(
        &self,
        ptr: *mut *const ffi::ScShortcut,
        n: *mut usize,
        out: *mut *mut c_int,
    ) {
        unsafe {
            *ptr = self.items.as_ptr();
            *n = self.items.len();
            *out = self.fired.as_ptr();
        }
    }
}

impl Default for Shortcuts {
    fn default() -> Self {
        Shortcuts::new()
    }
}

impl Drop for Shortcuts {
    fn drop(&mut self) {
        for &u in &self.callbacks {
            unsafe { drop(Box::from_raw(u as *mut ShortcutFn)) };
        }
    }
}

/// Options for [`show_shortcuts`].
pub struct ShortcutHelpOpts<'a> {
    /// Heading / search-field label; `None` => "Keyboard shortcuts".
    pub title: Option<&'a str>,
    /// Accent color; [`Color::NONE`] => yellow (resolved by the C core).
    pub accent: Color,
    /// Footer hint; `None` => "type to filter · esc to close".
    pub footer_hint: Option<&'a str>,
    /// Set when the caller already holds an alternate screen (a long-running
    /// TUI): the help screen renders full-screen without opening a second one.
    /// `false` (default) spans its own alternate screen for the duration.
    pub in_alt_screen: bool,
}

impl Default for ShortcutHelpOpts<'_> {
    fn default() -> Self {
        ShortcutHelpOpts {
            title: None,
            accent: Color::NONE,
            footer_hint: None,
            in_alt_screen: false,
        }
    }
}

/// Shows the modal, scrollable keyboard-shortcut help screen built from a
/// [`Shortcuts`] set (sections, derived key names, descriptions and any
/// [`help_row`](Shortcuts::help_row) lines, in author order). Blocks until
/// Esc/Enter; a no-op without a TTY.
pub fn show_shortcuts(sc: &Shortcuts, opts: ShortcutHelpOpts) {
    let title = opts.title.map(cstring);
    let hint = opts.footer_hint.map(cstring);
    let copts = ffi::ScShortcutHelpOpts {
        title: title.as_ref().map_or(std::ptr::null(), |c| c.as_ptr()),
        accent: opts.accent.raw(),
        footer_hint: hint.as_ref().map_or(std::ptr::null(), |c| c.as_ptr()),
        in_alt_screen: opts.in_alt_screen,
    };
    unsafe {
        ffi::sc_shortcut_help_show(
            sc.help_rows.as_ptr(),
            sc.help_rows.len(),
            &copts,
        );
    }
}

/* ── Theme ───────────────────────────────────────────────────────────────── */

/// Process-wide default styling for every input widget.
///
/// Install a theme once and each widget inherits it for any option the caller
/// leaves at its default; per-call options always win, and the theme in turn
/// wins over the built-in defaults. The `box` framing is merged sub-field by
/// sub-field, so a theme can set just a border and leave the rest per-widget.
///
/// ```no_run
/// use sparcli::{Color, Theme};
/// Theme::new().accent(Color::MAGENTA).marker("➜ ").apply();
/// // … every select/fuzzy/confirm now defaults to a magenta accent …
/// sparcli::reset_theme();   // back to built-in defaults
/// ```
#[derive(Clone, Debug, Default)]
pub struct Theme {
    pub accent: Color,
    /// Default box framing for every widget's `box` field.
    pub box_: BoxStyle,
    pub prompt_style: Style,
    pub selected_style: Style,
    pub cursor_style: Style,
    pub count_style: Style,
    pub summary_style: Style,
    pub error_style: Style,
    pub hint_style: Style,
    pub cursor_marker: Option<String>,
    pub marker: Option<String>,
    pub checkbox_on: Option<String>,
    pub checkbox_off: Option<String>,
    pub hint_layout: HintLayout,
    pub hint_pos: HintPos,
}

impl Theme {
    pub fn new() -> Self {
        Theme::default()
    }
    pub fn accent(mut self, c: Color) -> Self {
        self.accent = c;
        self
    }
    /// Default box framing (border, background, padding, margin, width).
    pub fn box_style(mut self, b: BoxStyle) -> Self {
        self.box_ = b;
        self
    }
    pub fn prompt_style(mut self, s: Style) -> Self {
        self.prompt_style = s;
        self
    }
    pub fn selected_style(mut self, s: Style) -> Self {
        self.selected_style = s;
        self
    }
    pub fn cursor_style(mut self, s: Style) -> Self {
        self.cursor_style = s;
        self
    }
    pub fn count_style(mut self, s: Style) -> Self {
        self.count_style = s;
        self
    }
    pub fn summary_style(mut self, s: Style) -> Self {
        self.summary_style = s;
        self
    }
    pub fn error_style(mut self, s: Style) -> Self {
        self.error_style = s;
        self
    }
    pub fn hint_style(mut self, s: Style) -> Self {
        self.hint_style = s;
        self
    }
    pub fn cursor_marker(mut self, s: impl Into<String>) -> Self {
        self.cursor_marker = Some(s.into());
        self
    }
    pub fn marker(mut self, s: impl Into<String>) -> Self {
        self.marker = Some(s.into());
        self
    }
    pub fn checkbox_on(mut self, s: impl Into<String>) -> Self {
        self.checkbox_on = Some(s.into());
        self
    }
    pub fn checkbox_off(mut self, s: impl Into<String>) -> Self {
        self.checkbox_off = Some(s.into());
        self
    }
    pub fn hint_layout(mut self, l: HintLayout) -> Self {
        self.hint_layout = l;
        self
    }
    pub fn hint_pos(mut self, p: HintPos) -> Self {
        self.hint_pos = p;
        self
    }

    /// Installs this theme as the process-wide default (copies all fields).
    pub fn apply(&self) {
        set_theme(Some(self));
    }
}

/// Installs `theme` as the process-wide input default, or clears it (`None`).
///
/// The struct and its glyph strings are copied by the C side, so the `Theme`
/// need not outlive the call. Not thread-safe: set it once before spawning
/// threads.
pub fn set_theme(theme: Option<&Theme>) {
    let Some(t) = theme else {
        unsafe { ffi::sc_input_set_theme(std::ptr::null()) };
        return;
    };
    let cursor_marker = t.cursor_marker.as_deref().map(cstring);
    let marker = t.marker.as_deref().map(cstring);
    let checkbox_on = t.checkbox_on.as_deref().map(cstring);
    let checkbox_off = t.checkbox_off.as_deref().map(cstring);
    let raw = ffi::ScInputTheme {
        accent: t.accent.raw(),
        box_: t.box_.raw(),
        prompt_style: t.prompt_style.raw(),
        selected_style: t.selected_style.raw(),
        cursor_style: t.cursor_style.raw(),
        count_style: t.count_style.raw(),
        summary_style: t.summary_style.raw(),
        error_style: t.error_style.raw(),
        hint_style: t.hint_style.raw(),
        cursor_marker: cursor_marker
            .as_ref()
            .map_or(std::ptr::null(), |c| c.as_ptr()),
        marker: marker.as_ref().map_or(std::ptr::null(), |c| c.as_ptr()),
        checkbox_on: checkbox_on
            .as_ref()
            .map_or(std::ptr::null(), |c| c.as_ptr()),
        checkbox_off: checkbox_off
            .as_ref()
            .map_or(std::ptr::null(), |c| c.as_ptr()),
        hint_layout: t.hint_layout.raw(),
        hint_pos: t.hint_pos.raw(),
    };
    unsafe { ffi::sc_input_set_theme(&raw) };
}

/// Clears the process-wide input theme, reverting to the built-in defaults.
pub fn reset_theme() {
    unsafe { ffi::sc_input_set_theme(std::ptr::null()) };
}

/* ── Confirm ─────────────────────────────────────────────────────────────── */

/// Options for [`confirm`].
#[derive(Default)]
pub struct ConfirmOpts<'a> {
    pub default_yes: bool,
    pub yes_label: Option<String>,
    pub no_label: Option<String>,
    pub accent: Color,
    pub prompt_style: Style,
    pub box_: BoxStyle,
    pub hide_summary: bool,
    pub hint_layout: HintLayout,
    pub hint_pos: HintPos,
    pub shortcuts: Option<&'a Shortcuts>,
}

impl<'a> ConfirmOpts<'a> {
    pub fn new() -> Self {
        ConfirmOpts::default()
    }
    pub fn default_yes(mut self, on: bool) -> Self {
        self.default_yes = on;
        self
    }
    pub fn accent(mut self, c: Color) -> Self {
        self.accent = c;
        self
    }
    /// Frame the question in a panel.
    pub fn box_style(mut self, b: BoxStyle) -> Self {
        self.box_ = b;
        self
    }
    pub fn shortcuts(mut self, s: &'a Shortcuts) -> Self {
        self.shortcuts = Some(s);
        self
    }
}

/// Yes/No prompt.
pub fn confirm(question: &str, opts: ConfirmOpts) -> Result<Option<bool>> {
    let mut o: ffi::ScConfirmOpts = unsafe { mem::zeroed() };
    let yes = opts.yes_label.as_deref().map(cstring);
    let no = opts.no_label.as_deref().map(cstring);
    o.default_yes = opts.default_yes;
    o.yes_label = yes.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
    o.no_label = no.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
    o.accent = opts.accent.raw();
    o.box_ = opts.box_.raw();
    o.prompt_style = opts.prompt_style.raw();
    o.hide_summary = opts.hide_summary;
    o.hint_layout = opts.hint_layout.raw();
    o.hint_pos = opts.hint_pos.raw();
    if let Some(sc) = opts.shortcuts {
        sc.apply(&mut o.shortcuts, &mut o.n_shortcuts, &mut o.out_shortcut_id);
    }
    let mut out = false;
    let st =
        unsafe { ffi::sc_confirm(cstring(question).as_ptr(), &mut out, o) };
    Ok(status(st)?.then_some(out))
}

/* ── Text / password input ───────────────────────────────────────────────── */

/// Built-in per-character input filters for text/number input.
#[derive(Clone, Copy, Debug)]
pub enum CharFilter {
    Digits,
    Decimal,
    Alpha,
    Alnum,
    NoSpace,
}

impl CharFilter {
    fn raw(self) -> ffi::ScCharFilter {
        Some(match self {
            CharFilter::Digits => ffi::sc_filter_digits as _,
            CharFilter::Decimal => ffi::sc_filter_decimal as _,
            CharFilter::Alpha => ffi::sc_filter_alpha as _,
            CharFilter::Alnum => ffi::sc_filter_alnum as _,
            CharFilter::NoSpace => ffi::sc_filter_no_space as _,
        })
    }
}

/// A validation callback: `Ok(())` accepts the value, `Err(msg)` keeps the
/// prompt open and shows `msg` beneath the field.
type ValidateFn = Box<dyn FnMut(&str) -> std::result::Result<(), String>>;

/// Owns the validator closure for one prompt run and keeps the last error
/// message alive for the C side to read through `err_out`.
struct ValidatorState {
    f: ValidateFn,
    last_err: Option<CString>,
}

unsafe extern "C" fn validate_tramp(
    value: *const c_char,
    ctx: *mut c_void,
    err_out: *mut *const c_char,
) -> bool {
    let state = &mut *(ctx as *mut ValidatorState);
    let v = std::ffi::CStr::from_ptr(value).to_string_lossy();
    match (state.f)(&v) {
        Ok(()) => true,
        Err(msg) => {
            // Keep the message alive past this call (the widget displays it
            // until the next keystroke), then hand the C side a pointer to it.
            state.last_err = Some(cstring(&msg));
            if !err_out.is_null() {
                *err_out = state.last_err.as_ref().unwrap().as_ptr();
            }
            false
        }
    }
}

/// How autocomplete suggestions are presented.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum SuggestMode {
    /// Dim ghost text behind the cursor; Tab accepts (default).
    #[default]
    Ghost,
    /// Dropdown list below the field; arrows navigate, Tab/Enter accept.
    Dropdown,
}

/// How typed text is matched against the suggestion list (dropdown mode).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum SuggestMatch {
    /// Case-insensitive prefix match (default).
    #[default]
    Prefix,
    /// Fuzzy subsequence match, best score first.
    Fuzzy,
}

/// Presentation options for the autocomplete suggestion list.
///
/// `SuggestOpts::default()` keeps the classic ghost-text behavior; use
/// [`SuggestOpts::dropdown`] for a navigable list below the field.
#[derive(Clone, Debug, Default)]
pub struct SuggestOpts {
    pub mode: SuggestMode,
    pub match_mode: SuggestMatch,
    /// Max dropdown rows shown at once; `0` = 5.
    pub max_visible: i32,
    /// Frame around the list; default = plain list without a border.
    pub border: BorderStyle,
    /// Style of unselected rows.
    pub item_style: Style,
    /// Style (incl. background) of the highlighted row; default = bold cyan.
    pub selected_style: Style,
    /// Style of the "… +N more" overflow line; default = dim.
    pub more_style: Style,
    /// Marker before the highlighted row; `None` = "‣ ".
    pub cursor_marker: Option<String>,
    /// Marker before unselected rows; `None` = two spaces.
    pub marker: Option<String>,
}

impl SuggestOpts {
    /// A dropdown suggestion list with default styling.
    pub fn dropdown() -> Self {
        SuggestOpts {
            mode: SuggestMode::Dropdown,
            ..SuggestOpts::default()
        }
    }
    pub fn fuzzy(mut self) -> Self {
        self.match_mode = SuggestMatch::Fuzzy;
        self
    }
    pub fn max_visible(mut self, n: i32) -> Self {
        self.max_visible = n;
        self
    }
    pub fn border(mut self, b: BorderStyle) -> Self {
        self.border = b;
        self
    }
    pub fn item_style(mut self, s: Style) -> Self {
        self.item_style = s;
        self
    }
    pub fn selected_style(mut self, s: Style) -> Self {
        self.selected_style = s;
        self
    }

    /// Converts to the FFI struct; `markers` keeps the C strings alive for
    /// the duration of the call (a `CString`'s heap buffer does not move when
    /// the handle is pushed into the vec).
    fn raw(&self, markers: &mut Vec<CString>) -> ffi::ScSuggestOpts {
        let mut raw: ffi::ScSuggestOpts = unsafe { mem::zeroed() };
        raw.mode = self.mode as ffi::ScSuggestMode;
        raw.match_ = self.match_mode as ffi::ScSuggestMatch;
        raw.max_visible = self.max_visible;
        raw.border = self.border.raw();
        raw.item_style = self.item_style.raw();
        raw.selected_style = self.selected_style.raw();
        raw.more_style = self.more_style.raw();
        if let Some(m) = self.cursor_marker.as_deref() {
            let c = cstring(m);
            raw.cursor_marker = c.as_ptr();
            markers.push(c);
        }
        if let Some(m) = self.marker.as_deref() {
            let c = cstring(m);
            raw.marker = c.as_ptr();
            markers.push(c);
        }
        raw
    }
}

/* ── Input history ───────────────────────────────────────────────────────── */

/// Options for [`History::new`]. All strings are copied by the C side.
#[derive(Clone, Debug, Default)]
pub struct HistoryOpts {
    /// Maximum retained entries; `0` = 500 (oldest evicted past the cap).
    pub max_entries: usize,

    /// Application name for XDG persistence: entries are stored in
    /// `~/.local/state/<app>/history` (created on first use).
    pub app: Option<String>,

    /// Explicit path of the persistence file; overrides `app`.
    pub file: Option<String>,

    /// Do not auto-add submitted lines when attached to a text input.
    pub no_auto_add: bool,

    /// Keep consecutive duplicate lines instead of collapsing them.
    pub keep_duplicates: bool,
}

impl HistoryOpts {
    pub fn new() -> Self {
        Self::default()
    }
    pub fn max_entries(mut self, n: usize) -> Self {
        self.max_entries = n;
        self
    }
    pub fn app(mut self, name: impl Into<String>) -> Self {
        self.app = Some(name.into());
        self
    }
    pub fn file(mut self, path: impl Into<String>) -> Self {
        self.file = Some(path.into());
        self
    }
    pub fn no_auto_add(mut self) -> Self {
        self.no_auto_add = true;
        self
    }
    pub fn keep_duplicates(mut self) -> Self {
        self.keep_duplicates = true;
        self
    }
}

/// Input history for text prompts: Up/Down recall + optional persistence.
///
/// Attach it with [`TextInputOpts::history`]; the prompt then recalls entries
/// with Up/Down and records every submitted line automatically (the C side
/// mutates the history through the attached handle). Dropping the handle
/// saves the persistence file when one is configured.
///
/// ```no_run
/// use sparcli::{text_input, History, HistoryOpts, TextInputOpts};
///
/// let history = History::new(HistoryOpts::new().app("myapp"));
/// loop {
///     match text_input("repl>", TextInputOpts::new().history(&history)) {
///         Ok(Some(line)) => println!("{line}"),
///         _ => break,   // cancelled or no terminal
///     }
/// }
/// // drop(history) saves ~/.local/state/myapp/history
/// ```
pub struct History {
    ptr: *mut ffi::ScHistory,
}

impl History {
    /// Allocates a history (loads the file when one is configured).
    pub fn new(opts: HistoryOpts) -> History {
        let app = opts.app.as_deref().map(cstring);
        let file = opts.file.as_deref().map(cstring);
        let raw = ffi::ScHistoryOpts {
            max_entries: opts.max_entries,
            app: app.as_ref().map_or(std::ptr::null(), |c| c.as_ptr()),
            file: file.as_ref().map_or(std::ptr::null(), |c| c.as_ptr()),
            no_auto_add: opts.no_auto_add,
            keep_duplicates: opts.keep_duplicates,
        };
        let ptr = unsafe { ffi::sc_history_new(raw) };
        assert!(!ptr.is_null(), "sc_history_new: out of memory");
        History { ptr }
    }

    /// Appends a line (empty lines / consecutive duplicates are skipped).
    pub fn add(&mut self, line: &str) -> &mut Self {
        unsafe { ffi::sc_history_add(self.ptr, cstring(line).as_ptr()) };
        self
    }
    /// Number of retained entries.
    pub fn count(&self) -> usize {
        unsafe { ffi::sc_history_count(self.ptr) }
    }
    /// Entry at `index` (`0` = oldest), or `None` when out of range.
    pub fn get(&self, index: usize) -> Option<String> {
        let p = unsafe { ffi::sc_history_get(self.ptr, index) };
        if p.is_null() {
            None
        } else {
            Some(unsafe {
                std::ffi::CStr::from_ptr(p).to_string_lossy().into_owned()
            })
        }
    }
    /// Writes the entries to the configured persistence file.
    pub fn save(&self) -> bool {
        unsafe { ffi::sc_history_save(self.ptr) }
    }
    /// Reloads the entries from the configured persistence file.
    pub fn load(&mut self) -> bool {
        unsafe { ffi::sc_history_load(self.ptr) }
    }

    pub(crate) fn as_mut_ptr(&self) -> *mut ffi::ScHistory {
        self.ptr
    }
}

impl Drop for History {
    fn drop(&mut self) {
        unsafe { ffi::sc_history_free(self.ptr) };
    }
}

/// Options for [`text_input`].
#[derive(Default)]
pub struct TextInputOpts<'a> {
    pub initial: Option<String>,
    pub placeholder: Option<String>,
    pub prompt_style: Style,
    pub value_style: Style,
    pub hide_summary: bool,
    pub max_chars: i32,
    pub hide_char_count: bool,
    pub box_: BoxStyle,
    pub char_filter: Option<CharFilter>,
    /// Validator: `Ok(())` accepts, `Err(msg)` keeps the prompt open and shows
    /// `msg` beneath the field. Set via [`TextInputOpts::validate`].
    pub validate: Option<ValidateFn>,
    pub suggestions: Vec<String>,
    pub suggest: SuggestOpts,
    pub hint_layout: HintLayout,
    pub hint_pos: HintPos,
    pub shortcuts: Option<&'a Shortcuts>,
    pub prompt_text: Option<&'a Text>,
    pub prompt_markup: bool,
    pub external_editor: bool,
    pub editor: Option<String>,
    pub editor_key: Option<Chord>,
    pub history: Option<&'a History>,
    pub no_history_add: bool,
}

impl<'a> TextInputOpts<'a> {
    pub fn new() -> Self {
        TextInputOpts::default()
    }
    pub fn initial(mut self, s: impl Into<String>) -> Self {
        self.initial = Some(s.into());
        self
    }
    pub fn placeholder(mut self, s: impl Into<String>) -> Self {
        self.placeholder = Some(s.into());
        self
    }
    pub fn max_chars(mut self, n: i32) -> Self {
        self.max_chars = n;
        self
    }
    /// Frame the field in a panel with the given width (`0` = terminal width).
    pub fn boxed(mut self, w: i32) -> Self {
        self.box_.enabled = true;
        self.box_.width = w;
        self
    }
    /// Set the full box style (border, background, padding, margin, width).
    pub fn box_style(mut self, b: BoxStyle) -> Self {
        self.box_ = b;
        self
    }
    pub fn char_filter(mut self, f: CharFilter) -> Self {
        self.char_filter = Some(f);
        self
    }
    /// Sets a validator: return `Ok(())` to accept the submitted value, or
    /// `Err(message)` to reject it - the prompt stays open and shows `message`
    /// beneath the field.
    ///
    /// ```no_run
    /// use sparcli::{text_input, TextInputOpts};
    /// let name = text_input("Name", TextInputOpts::new().validate(|v| {
    ///     if v.trim().is_empty() { Err("must not be empty".into()) } else { Ok(()) }
    /// }));
    /// ```
    pub fn validate<F>(mut self, f: F) -> Self
    where
        F: FnMut(&str) -> std::result::Result<(), String> + 'static,
    {
        self.validate = Some(Box::new(f));
        self
    }
    pub fn suggestions(mut self, s: Vec<String>) -> Self {
        self.suggestions = s;
        self
    }
    pub fn suggest(mut self, s: SuggestOpts) -> Self {
        self.suggest = s;
        self
    }
    pub fn shortcuts(mut self, s: &'a Shortcuts) -> Self {
        self.shortcuts = Some(s);
        self
    }
    pub fn prompt_text(mut self, t: &'a Text) -> Self {
        self.prompt_text = Some(t);
        self
    }
    pub fn prompt_markup(mut self, on: bool) -> Self {
        self.prompt_markup = on;
        self
    }
    pub fn external_editor(mut self, on: bool) -> Self {
        self.external_editor = on;
        self
    }
    pub fn editor(mut self, cmd: impl Into<String>) -> Self {
        self.editor = Some(cmd.into());
        self
    }
    /// Attaches an input history: Up/Down recall, auto-add on submit.
    pub fn history(mut self, h: &'a History) -> Self {
        self.history = Some(h);
        self
    }
    /// Do not auto-add the submitted line to the attached history.
    pub fn no_history_add(mut self) -> Self {
        self.no_history_add = true;
        self
    }
}

/// Single-line text entry. `*out` is owned and copied into a `String`.
pub fn text_input(
    prompt: &str,
    mut opts: TextInputOpts,
) -> Result<Option<String>> {
    let mut o: ffi::ScTextInputOpts = unsafe { mem::zeroed() };
    // Validator state must outlive the sc_text_input call (the C side calls
    // back into it); keep it on the stack for the duration of this function.
    let mut validator = opts
        .validate
        .take()
        .map(|f| ValidatorState { f, last_err: None });
    if let Some(state) = validator.as_mut() {
        o.validate = Some(validate_tramp);
        o.validate_ctx = state as *mut ValidatorState as *mut c_void;
    }
    let initial = opts.initial.as_deref().map(cstring);
    let placeholder = opts.placeholder.as_deref().map(cstring);
    let editor = opts.editor.as_deref().map(cstring);
    let sugg: Vec<CString> =
        opts.suggestions.iter().map(|s| cstring(s)).collect();
    let sugg_ptrs: Vec<*const c_char> =
        sugg.iter().map(|c| c.as_ptr()).collect();

    o.initial = initial.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
    o.placeholder = placeholder
        .as_ref()
        .map_or(std::ptr::null(), |c| c.as_ptr());
    o.prompt_style = opts.prompt_style.raw();
    o.value_style = opts.value_style.raw();
    o.hide_summary = opts.hide_summary;
    o.max_chars = opts.max_chars;
    o.hide_char_count = opts.hide_char_count;
    o.box_ = opts.box_.raw();
    o.hint_layout = opts.hint_layout.raw();
    o.hint_pos = opts.hint_pos.raw();
    if let Some(f) = opts.char_filter {
        o.char_filter = f.raw();
    }
    if !sugg_ptrs.is_empty() {
        o.suggestions = sugg_ptrs.as_ptr();
        o.n_suggestions = sugg_ptrs.len();
    }
    let mut suggest_markers: Vec<CString> = Vec::new();
    o.suggest = opts.suggest.raw(&mut suggest_markers);
    if let Some(sc) = opts.shortcuts {
        sc.apply(&mut o.shortcuts, &mut o.n_shortcuts, &mut o.out_shortcut_id);
    }
    if let Some(t) = opts.prompt_text {
        o.prompt_text = t.as_ptr();
    }
    o.prompt_markup = opts.prompt_markup;
    o.external_editor = opts.external_editor;
    o.editor = editor.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
    if let Some(k) = opts.editor_key {
        o.editor_key = k.0;
    }
    if let Some(h) = opts.history {
        o.history = h.as_mut_ptr();
    }
    o.no_history_add = opts.no_history_add;

    let mut out: *mut c_char = std::ptr::null_mut();
    let st =
        unsafe { ffi::sc_text_input(cstring(prompt).as_ptr(), &mut out, o) };
    if status(st)? {
        Ok(Some(unsafe { crate::take_c_string(out) }))
    } else {
        if !out.is_null() {
            unsafe { libc::free(out as *mut c_void) };
        }
        Ok(None)
    }
}

/// Options for [`password_input`].
#[derive(Default)]
pub struct PasswordOpts {
    pub placeholder: Option<String>,
    pub mask: Option<String>,
    pub hide_summary: bool,
    pub max_chars: i32,
    pub hide_char_count: bool,
    pub box_: BoxStyle,
    /// Validator: `Ok(())` accepts, `Err(msg)` keeps the prompt open and shows
    /// `msg` beneath the field. Set via [`PasswordOpts::validate`].
    pub validate: Option<ValidateFn>,
}

impl PasswordOpts {
    pub fn new() -> Self {
        PasswordOpts::default()
    }
    pub fn mask(mut self, m: impl Into<String>) -> Self {
        self.mask = Some(m.into());
        self
    }
    /// Sets a validator: return `Ok(())` to accept, or `Err(message)` to reject
    /// (the prompt stays open and shows `message`). See
    /// [`TextInputOpts::validate`].
    pub fn validate<F>(mut self, f: F) -> Self
    where
        F: FnMut(&str) -> std::result::Result<(), String> + 'static,
    {
        self.validate = Some(Box::new(f));
        self
    }
    /// Frame the field in a panel with the given width (`0` = terminal width).
    pub fn boxed(mut self, w: i32) -> Self {
        self.box_.enabled = true;
        self.box_.width = w;
        self
    }
    /// Set the full box style (border, background, padding, margin, width).
    pub fn box_style(mut self, b: BoxStyle) -> Self {
        self.box_ = b;
        self
    }
}

/// Masked single-line entry.
pub fn password_input(
    prompt: &str,
    mut opts: PasswordOpts,
) -> Result<Option<String>> {
    let mut o: ffi::ScPasswordOpts = unsafe { mem::zeroed() };
    let mut validator = opts
        .validate
        .take()
        .map(|f| ValidatorState { f, last_err: None });
    if let Some(state) = validator.as_mut() {
        o.validate = Some(validate_tramp);
        o.validate_ctx = state as *mut ValidatorState as *mut c_void;
    }
    let placeholder = opts.placeholder.as_deref().map(cstring);
    let mask = opts.mask.as_deref().map(cstring);
    o.placeholder = placeholder
        .as_ref()
        .map_or(std::ptr::null(), |c| c.as_ptr());
    o.mask = mask.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
    o.hide_summary = opts.hide_summary;
    o.max_chars = opts.max_chars;
    o.hide_char_count = opts.hide_char_count;
    o.box_ = opts.box_.raw();
    let mut out: *mut c_char = std::ptr::null_mut();
    let st = unsafe {
        ffi::sc_password_input(cstring(prompt).as_ptr(), &mut out, o)
    };
    if status(st)? {
        Ok(Some(unsafe { crate::take_c_string(out) }))
    } else {
        if !out.is_null() {
            unsafe { libc::free(out as *mut c_void) };
        }
        Ok(None)
    }
}

/* ── Number ──────────────────────────────────────────────────────────────── */

/// Options for [`number_input`] / [`number_input_text`].
#[derive(Default)]
pub struct NumberOpts {
    pub initial: f64,
    /// Start with an empty field instead of the formatted `initial` value;
    /// Enter on an empty field is ignored.
    pub start_empty: bool,
    pub min: f64,
    pub max: f64,
    pub step: f64,
    pub decimals: i32,
    pub hide_summary: bool,
    pub box_: BoxStyle,
    /// Decimal separator shown and accepted while editing; `'\0'` or `'.'`
    /// = period (default), `','` = comma.
    pub decimal_sep: char,

    /// Calculator mode: typing `=` starts an expression (e.g. `=1,5+2*3`);
    /// Enter accepts the result, a second Enter submits it. The field shows
    /// the result rounded to `decimals`, the submitted value keeps full
    /// precision.
    pub calculator: bool,

    /// Submit the displayed (rounded) calculator value instead of the
    /// full-precision result.
    pub calc_store_rounded: bool,

    /// Display calculator results in full precision instead of rounded.
    pub calc_show_precise: bool,
}

impl NumberOpts {
    pub fn new() -> Self {
        NumberOpts::default()
    }
    pub fn range(mut self, min: f64, max: f64) -> Self {
        self.min = min;
        self.max = max;
        self
    }
    pub fn step(mut self, s: f64) -> Self {
        self.step = s;
        self
    }
    pub fn decimals(mut self, d: i32) -> Self {
        self.decimals = d;
        self
    }
    pub fn initial(mut self, v: f64) -> Self {
        self.initial = v;
        self
    }
    pub fn start_empty(mut self, on: bool) -> Self {
        self.start_empty = on;
        self
    }
    pub fn decimal_sep(mut self, sep: char) -> Self {
        self.decimal_sep = sep;
        self
    }
    /// Frame the field in a panel with the given width (`0` = terminal width).
    pub fn boxed(mut self, w: i32) -> Self {
        self.box_.enabled = true;
        self.box_.width = w;
        self
    }
    /// Set the full box style (border, background, padding, margin, width).
    pub fn box_style(mut self, b: BoxStyle) -> Self {
        self.box_ = b;
        self
    }
    pub fn calculator(mut self, on: bool) -> Self {
        self.calculator = on;
        self
    }
    pub fn calc_store_rounded(mut self, on: bool) -> Self {
        self.calc_store_rounded = on;
        self
    }
    pub fn calc_show_precise(mut self, on: bool) -> Self {
        self.calc_show_precise = on;
        self
    }
    fn raw(&self) -> ffi::ScNumberOpts {
        let mut o: ffi::ScNumberOpts = unsafe { mem::zeroed() };
        o.initial = self.initial;
        o.start_empty = self.start_empty;
        o.min = self.min;
        o.max = self.max;
        o.step = self.step;
        o.decimals = self.decimals;
        o.hide_summary = self.hide_summary;
        o.box_ = self.box_.raw();
        o.decimal_sep = if self.decimal_sep == '\0' {
            0
        } else {
            self.decimal_sep as u8 as c_char
        };
        o.calculator = self.calculator;
        o.calc_store_rounded = self.calc_store_rounded;
        o.calc_show_precise = self.calc_show_precise;
        o
    }
}

/// Numeric entry with min/max/step.
pub fn number_input(prompt: &str, opts: NumberOpts) -> Result<Option<f64>> {
    let o = opts.raw();
    let mut out = 0.0f64;
    let st =
        unsafe { ffi::sc_number_input(cstring(prompt).as_ptr(), &mut out, o) };
    Ok(status(st)?.then_some(out))
}

/// Numeric entry returning the exact submitted text instead of an `f64`.
///
/// The string is always `'.'`-normalized and reflects clamping to the
/// configured range, so it can be parsed into an arbitrary-precision decimal
/// type without ever round-tripping through a float.
pub fn number_input_text(
    prompt: &str,
    opts: NumberOpts,
) -> Result<Option<String>> {
    let mut o = opts.raw();
    let mut value = 0.0f64;
    let mut text: *mut c_char = std::ptr::null_mut();
    o.out_text = &mut text;
    let st = unsafe {
        ffi::sc_number_input(cstring(prompt).as_ptr(), &mut value, o)
    };
    if status(st)? {
        Ok(Some(unsafe { crate::take_c_string(text) }))
    } else {
        if !text.is_null() {
            unsafe { libc::free(text as *mut c_void) };
        }
        Ok(None)
    }
}

/* ── Textarea ────────────────────────────────────────────────────────────── */

/// Options for [`textarea`].
#[derive(Default)]
pub struct TextareaOpts {
    pub initial: Option<String>,
    pub placeholder: Option<String>,
    pub box_: BoxStyle,
    pub external_editor: bool,
    pub editor: Option<String>,
    pub editor_key: Option<Chord>,
}

impl TextareaOpts {
    pub fn new() -> Self {
        TextareaOpts::default()
    }
    pub fn initial(mut self, s: impl Into<String>) -> Self {
        self.initial = Some(s.into());
        self
    }
    /// Frame the editor in a panel with the given width (`0` = terminal width).
    pub fn boxed(mut self, w: i32) -> Self {
        self.box_.enabled = true;
        self.box_.width = w;
        self
    }
    /// Set the full box style (border, background, padding, margin, width).
    pub fn box_style(mut self, b: BoxStyle) -> Self {
        self.box_ = b;
        self
    }
    pub fn external_editor(mut self, on: bool) -> Self {
        self.external_editor = on;
        self
    }
    pub fn editor(mut self, cmd: impl Into<String>) -> Self {
        self.editor = Some(cmd.into());
        self
    }
}

/// Multi-line entry (Ctrl-D submits).
pub fn textarea(prompt: &str, opts: TextareaOpts) -> Result<Option<String>> {
    let mut o: ffi::ScTextareaOpts = unsafe { mem::zeroed() };
    let initial = opts.initial.as_deref().map(cstring);
    let placeholder = opts.placeholder.as_deref().map(cstring);
    let editor = opts.editor.as_deref().map(cstring);
    o.initial = initial.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
    o.placeholder = placeholder
        .as_ref()
        .map_or(std::ptr::null(), |c| c.as_ptr());
    o.box_ = opts.box_.raw();
    o.external_editor = opts.external_editor;
    o.editor = editor.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
    if let Some(k) = opts.editor_key {
        o.editor_key = k.0;
    }
    let mut out: *mut c_char = std::ptr::null_mut();
    let st = unsafe { ffi::sc_textarea(cstring(prompt).as_ptr(), &mut out, o) };
    if status(st)? {
        Ok(Some(unsafe { crate::take_c_string(out) }))
    } else {
        if !out.is_null() {
            unsafe { libc::free(out as *mut c_void) };
        }
        Ok(None)
    }
}

/* ── Select ──────────────────────────────────────────────────────────────── */

/// Options for a [`Select`].
#[derive(Default)]
pub struct SelectOpts {
    pub prompt: Option<String>,
    pub multi: bool,
    pub no_cycle: bool,
    pub max_visible: i32,
    pub accent: Color,
    pub selected_style: Style,
    pub box_: BoxStyle,
}

impl SelectOpts {
    pub fn new() -> Self {
        SelectOpts::default()
    }
    pub fn prompt(mut self, s: impl Into<String>) -> Self {
        self.prompt = Some(s.into());
        self
    }
    pub fn multi(mut self, on: bool) -> Self {
        self.multi = on;
        self
    }
    /// Stop the cursor at the list ends instead of cycling (cycling is the
    /// default: Up on the first row jumps to the last, and back).
    pub fn no_cycle(mut self) -> Self {
        self.no_cycle = true;
        self
    }
    pub fn max_visible(mut self, n: i32) -> Self {
        self.max_visible = n;
        self
    }
    pub fn accent(mut self, c: Color) -> Self {
        self.accent = c;
        self
    }
    /// Style of the cursor row. A background fills the full row width as a
    /// highlight bar (see [`BoxStyle::bg_extent`]).
    pub fn selected_style(mut self, s: Style) -> Self {
        self.selected_style = s;
        self
    }
    /// Frame the list in a panel (border, background, padding, margin, width).
    pub fn box_style(mut self, b: BoxStyle) -> Self {
        self.box_ = b;
        self
    }
}

/// An owning single/multi-choice selection.
pub struct Select {
    ptr: *mut ffi::ScSelect,
    count: usize,
    _prompt: Option<CString>,
}

impl Select {
    pub fn new(opts: SelectOpts) -> Select {
        let prompt = opts.prompt.as_deref().map(cstring);
        let mut o: ffi::ScSelectOpts = unsafe { mem::zeroed() };
        o.prompt = prompt.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
        o.multi = opts.multi;
        o.no_cycle = opts.no_cycle;
        o.max_visible = opts.max_visible;
        o.accent = opts.accent.raw();
        o.selected_style = opts.selected_style.raw();
        o.box_ = opts.box_.raw();
        let ptr = unsafe { ffi::sc_select_new(o) };
        assert!(!ptr.is_null(), "sc_select_new: out of memory");
        Select {
            ptr,
            count: 0,
            _prompt: prompt,
        }
    }
    pub fn add(&mut self, label: &str) -> &mut Self {
        unsafe { ffi::sc_select_add(self.ptr, cstring(label).as_ptr()) };
        self.count += 1;
        self
    }
    pub fn set_cursor(&mut self, index: usize) {
        unsafe { ffi::sc_select_set_cursor(self.ptr, index) };
    }
    pub fn set_checked(&mut self, index: usize, on: bool) {
        unsafe { ffi::sc_select_set_checked(self.ptr, index, on) };
    }
    /// Highlighted index (for use from a shortcut callback).
    pub fn cursor(&self) -> usize {
        unsafe { ffi::sc_select_cursor(self.ptr) }
    }
    /// Current label at `index`, or `None` if out of range.
    pub fn label(&self, index: usize) -> Option<String> {
        let p = unsafe { ffi::sc_select_label(self.ptr, index) };
        if p.is_null() {
            None
        } else {
            Some(unsafe {
                std::ffi::CStr::from_ptr(p).to_string_lossy().into_owned()
            })
        }
    }
    pub fn set_label(&mut self, index: usize, label: &str) {
        unsafe {
            ffi::sc_select_set_label(self.ptr, index, cstring(label).as_ptr())
        };
    }
    /// Removes the item at `index` (live; for shortcut callbacks).
    pub fn remove(&mut self, index: usize) {
        unsafe { ffi::sc_select_remove(self.ptr, index) };
    }

    /// Runs the prompt → chosen indices in display order (multi) or one index.
    pub fn run(&mut self) -> Result<Option<Vec<usize>>> {
        let cap = self.count.max(1);
        let mut idx = vec![0usize; cap];
        let mut n = cap;
        let st =
            unsafe { ffi::sc_select_run(self.ptr, idx.as_mut_ptr(), &mut n) };
        if status(st)? {
            idx.truncate(n);
            Ok(Some(idx))
        } else {
            Ok(None)
        }
    }

    /// Single-select convenience.
    pub fn run_one(&mut self) -> Result<Option<usize>> {
        Ok(self.run()?.and_then(|v| v.first().copied()))
    }
}

impl Drop for Select {
    fn drop(&mut self) {
        unsafe { ffi::sc_select_free(self.ptr) };
    }
}

/* ── Fuzzy ───────────────────────────────────────────────────────────────── */

/// Pure subsequence match (no TTY needed); returns the match flag + score.
/// Evaluates a basic arithmetic expression (`+ - * /`, parentheses).
///
/// Both `.` and `,` work as decimal separator. Returns `None` for invalid
/// expressions (syntax error, division by zero, overflow). This is the
/// evaluator behind the number input's calculator mode.
pub fn calc_eval(expr: &str) -> Option<f64> {
    let mut result = 0.0f64;
    let ok =
        unsafe { ffi::sc_calc_eval(cstring(expr).as_ptr(), &mut result) };
    ok.then_some(result)
}

pub fn fuzzy_match(pattern: &str, s: &str) -> (bool, i32) {
    let mut score = 0;
    let ok = unsafe {
        ffi::sc_fuzzy_match(
            cstring(pattern).as_ptr(),
            cstring(s).as_ptr(),
            &mut score,
        )
    };
    (ok, score)
}

/// Result ordering of the filtered list. With section headers the order is
/// applied *within* each section; without sections it is global.
#[derive(Default, Clone, Copy)]
pub enum FuzzyOrder {
    /// Match score, then add order (default).
    #[default]
    Score,
    /// Stable add order (e.g. tasks by time).
    Insertion,
    /// By the given column (case-insensitive).
    Column(usize),
}

/// Options for a [`Fuzzy`] finder.
#[derive(Default)]
pub struct FuzzyOpts {
    pub prompt: Option<String>,
    pub max_visible: i32,
    pub no_cycle: bool,
    pub accent: Color,
    pub selected_style: Style,
    pub box_: BoxStyle,
    /// Render results as a table. Set headers with [`FuzzyOpts::table`] and
    /// add rows with [`Fuzzy::add_row`].
    pub table: bool,
    /// Column headers for the table view.
    pub headers: Vec<String>,
    /// Bitmask of columns the query searches (bit `c` = column `c`); `0`
    /// (default) searches all columns. Table view only.
    pub search_columns: u64,
    /// Bitmask of table columns that stretch to fill the box width when `box`
    /// has a bounded width (bit `c` = column `c`); `0` (default) keeps the table
    /// content-sized. Table view only.
    pub stretch_columns: u64,
    /// Cap the finder's total height in rows so it scrolls instead of
    /// overflowing; `0` (default) auto-fits the terminal. Set to the reserved
    /// rows when running inside a live dashboard.
    pub max_height: i32,
    /// Suppress the right-edge scrollbar (shown by default while the list
    /// scrolls); the `↑ x–y/total ↓` text indicator still shows.
    pub no_scrollbar: bool,
    /// Table-view rendering options (border, header style, padding, …).
    pub table_opts: crate::output::TableOpts,
    /// Multi-select: Space toggles, run via [`Fuzzy::run_multi`].
    pub multi: bool,
    /// Table view: render the checkbox as its own leading column.
    pub checkbox_column: bool,
    /// Append the matched-row count to each section header ("Monday (3)").
    pub section_counts: bool,
    /// Result ordering (within sections when present).
    pub order: FuzzyOrder,
    /// Sort descending for `Column`/`Insertion` order.
    pub order_desc: bool,
    /// Style of section-header rows (zero-init = dim + bold).
    pub section_style: Style,
    /// Style of disabled (greyed) rows (zero-init = dim).
    pub disabled_style: Style,
    /// Text shown when no row matches the query.
    pub empty_text: Option<String>,
    /// Pre-fill the query field on the next run.
    pub initial_query: Option<String>,
    /// Key that toggles all selectable rows (multi only).
    pub toggle_all_key: Option<Chord>,
    /// Key that toggles the cursor's section (multi only).
    pub toggle_section_key: Option<Chord>,
    /// Enable a modal normal/insert mode (vim-style). Off by default: every
    /// key types into the query. When on, normal mode fires bare-letter
    /// shortcuts and navigates with `j`/`k`/`g`/`G`; insert mode edits the
    /// query. Toggle with `i` / `Esc`; the mode shows in the query line.
    pub modal: bool,
    /// Modal: start in insert mode instead of normal mode (the default).
    pub start_in_insert: bool,
    /// Hide the NORMAL/INSERT badge (the field is still tinted per mode).
    pub hide_mode_badge: bool,
    /// Normal-mode badge text (default "NORMAL").
    pub normal_label: Option<String>,
    /// Insert-mode badge text (default "INSERT").
    pub insert_label: Option<String>,
    /// Badge + query-field style in normal mode (zero-init = white on blue).
    pub mode_normal_style: Style,
    /// Badge + query-field style in insert mode (zero-init = black on green).
    pub mode_insert_style: Style,
    /// Chord that enters insert mode (default `i`).
    pub insert_key: Option<Chord>,
    /// Chord that leaves insert mode / cancels in normal mode (default `Esc`).
    pub normal_key: Option<Chord>,
    /// Normal-mode chord that clears the query (default: disabled).
    pub clear_key: Option<Chord>,
    /// Full-screen mode: the finder fills the terminal, its list grows with the
    /// items up to the available height (minus `header`) and only then scrolls;
    /// the leftover space is placed by `valign`. Run inside an [`AltScreen`].
    pub fullscreen: bool,
    /// Vertical alignment of the (header + finder) block (fullscreen only).
    pub valign: VAlign,
    /// Header pinned above the finder (fullscreen only). Borrowed: the
    /// [`Rendered`] must outlive the run. Set with [`FuzzyOpts::header`].
    pub header: Option<NonNull<ffi::ScRendered>>,
}

impl FuzzyOpts {
    pub fn new() -> Self {
        FuzzyOpts::default()
    }
    pub fn prompt(mut self, s: impl Into<String>) -> Self {
        self.prompt = Some(s.into());
        self
    }
    /// Switches to the table view with the given column headers (add rows with
    /// [`Fuzzy::add_row`]).
    pub fn table<I, S>(mut self, headers: I) -> Self
    where
        I: IntoIterator<Item = S>,
        S: Into<String>,
    {
        self.table = true;
        self.headers = headers.into_iter().map(Into::into).collect();
        self
    }
    /// Sets which columns the query searches (bitmask; `0` = all).
    pub fn search_columns(mut self, mask: u64) -> Self {
        self.search_columns = mask;
        self
    }
    /// Sets which table columns stretch to fill the box width (bitmask).
    pub fn stretch_columns(mut self, mask: u64) -> Self {
        self.stretch_columns = mask;
        self
    }
    /// Caps the finder height in rows (scrolls within); 0 = auto-fit terminal.
    pub fn max_height(mut self, rows: i32) -> Self {
        self.max_height = rows;
        self
    }
    /// Suppress the right-edge scrollbar (kept on by default while scrolling).
    /// Enables full-screen mode (grow then scroll, header + valign).
    pub fn fullscreen(mut self) -> Self {
        self.fullscreen = true;
        self
    }
    /// Sets the vertical alignment of the block (fullscreen only).
    pub fn valign(mut self, v: VAlign) -> Self {
        self.valign = v;
        self
    }
    /// Pins a borrowed header above the finder (fullscreen only). The
    /// [`Rendered`] must outlive the run.
    pub fn header(mut self, header: &Rendered) -> Self {
        self.header = NonNull::new(header.as_ptr() as *mut ffi::ScRendered);
        self
    }
    pub fn no_scrollbar(mut self) -> Self {
        self.no_scrollbar = true;
        self
    }
    /// Sets the table-view rendering options.
    pub fn table_opts(mut self, opts: crate::output::TableOpts) -> Self {
        self.table_opts = opts;
        self
    }
    /// Stop the cursor at the result-list ends instead of cycling (cycling is
    /// the default).
    pub fn no_cycle(mut self) -> Self {
        self.no_cycle = true;
        self
    }
    pub fn accent(mut self, c: Color) -> Self {
        self.accent = c;
        self
    }
    /// Style of the cursor row. A background fills the full row width as a
    /// highlight bar (see [`BoxStyle::bg_extent`]); in the table view it
    /// overrides the accent highlight.
    pub fn selected_style(mut self, s: Style) -> Self {
        self.selected_style = s;
        self
    }
    /// Frame the finder in a panel (border, background, padding, margin, width).
    pub fn box_style(mut self, b: BoxStyle) -> Self {
        self.box_ = b;
        self
    }
    /// Enable multi-select (Space toggles; run with [`Fuzzy::run_multi`]).
    pub fn multi(mut self) -> Self {
        self.multi = true;
        self
    }
    /// Render the checkbox as its own leading table column (multi).
    pub fn checkbox_column(mut self) -> Self {
        self.checkbox_column = true;
        self
    }
    /// Show the matched-row count on each section header.
    pub fn section_counts(mut self) -> Self {
        self.section_counts = true;
        self
    }
    /// Set the result ordering (applied within sections when present).
    pub fn order(mut self, order: FuzzyOrder) -> Self {
        self.order = order;
        self
    }
    /// Sort descending for `Column`/`Insertion` ordering.
    pub fn order_desc(mut self) -> Self {
        self.order_desc = true;
        self
    }
    /// Style of section-header rows.
    pub fn section_style(mut self, s: Style) -> Self {
        self.section_style = s;
        self
    }
    /// Style of disabled (greyed) rows.
    pub fn disabled_style(mut self, s: Style) -> Self {
        self.disabled_style = s;
        self
    }
    /// Text shown when the query matches nothing.
    pub fn empty_text(mut self, s: impl Into<String>) -> Self {
        self.empty_text = Some(s.into());
        self
    }
    /// Pre-fill the query field on the next run.
    pub fn initial_query(mut self, s: impl Into<String>) -> Self {
        self.initial_query = Some(s.into());
        self
    }
    /// Key that toggles all selectable rows on/off (multi only).
    pub fn toggle_all_key(mut self, chord: Chord) -> Self {
        self.toggle_all_key = Some(chord);
        self
    }
    /// Key that toggles the cursor's section on/off (multi only).
    pub fn toggle_section_key(mut self, chord: Chord) -> Self {
        self.toggle_section_key = Some(chord);
        self
    }
    /// Enable the modal normal/insert mode (vim-style).
    pub fn modal(mut self) -> Self {
        self.modal = true;
        self
    }
    /// Start in insert mode (requires [`FuzzyOpts::modal`]).
    pub fn start_in_insert(mut self) -> Self {
        self.start_in_insert = true;
        self
    }
    /// Hide the NORMAL/INSERT badge (the field is still tinted).
    pub fn hide_mode_badge(mut self) -> Self {
        self.hide_mode_badge = true;
        self
    }
    /// Set the normal- and insert-mode badge labels.
    pub fn mode_labels(
        mut self,
        normal: impl Into<String>,
        insert: impl Into<String>,
    ) -> Self {
        self.normal_label = Some(normal.into());
        self.insert_label = Some(insert.into());
        self
    }
    /// Set the badge + query-field styles for normal and insert mode.
    pub fn mode_styles(mut self, normal: Style, insert: Style) -> Self {
        self.mode_normal_style = normal;
        self.mode_insert_style = insert;
        self
    }
    /// Override the insert / normal mode-toggle chords (defaults `i` / `Esc`).
    pub fn mode_keys(mut self, insert: Chord, normal: Chord) -> Self {
        self.insert_key = Some(insert);
        self.normal_key = Some(normal);
        self
    }
    /// Normal-mode chord that clears the query field.
    pub fn clear_key(mut self, chord: Chord) -> Self {
        self.clear_key = Some(chord);
        self
    }
}

/// An owning incremental fuzzy finder (list view).
pub struct Fuzzy {
    ptr: *mut ffi::ScFuzzy,
    _prompt: Option<CString>,
    count: usize,
}

impl Fuzzy {
    pub fn new(opts: FuzzyOpts) -> Fuzzy {
        let prompt = opts.prompt.as_deref().map(cstring);
        let headers: Vec<CString> =
            opts.headers.iter().map(|h| cstring(h)).collect();
        let header_ptrs: Vec<*const c_char> =
            headers.iter().map(|c| c.as_ptr()).collect();
        // The C side copies these strings in sc_fuzzy_new, so the locals only
        // need to live until the call returns.
        let empty = opts.empty_text.as_deref().map(cstring);
        let initial = opts.initial_query.as_deref().map(cstring);
        let normal_label = opts.normal_label.as_deref().map(cstring);
        let insert_label = opts.insert_label.as_deref().map(cstring);
        let mut arena = crate::style::Arena::new();
        let mut o: ffi::ScFuzzyOpts = unsafe { mem::zeroed() };
        o.prompt = prompt.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
        o.max_visible = opts.max_visible;
        o.no_cycle = opts.no_cycle;
        o.accent = opts.accent.raw();
        o.selected_style = opts.selected_style.raw();
        o.box_ = opts.box_.raw();
        o.table = opts.table;
        if !header_ptrs.is_empty() {
            o.headers = header_ptrs.as_ptr();
            o.n_cols = header_ptrs.len();
        }
        o.search_columns = opts.search_columns;
        o.stretch_columns = opts.stretch_columns;
        o.max_height = opts.max_height;
        o.no_scrollbar = opts.no_scrollbar;
        o.fullscreen = opts.fullscreen;
        o.valign = opts.valign.raw();
        o.header = opts.header.map_or(std::ptr::null(), |p| p.as_ptr());
        o.table_opts = opts.table_opts.raw(&mut arena);
        o.multi = opts.multi;
        o.checkbox_column = opts.checkbox_column;
        o.section_counts = opts.section_counts;
        o.section_style = opts.section_style.raw();
        o.disabled_style = opts.disabled_style.raw();
        o.order_desc = opts.order_desc;
        match opts.order {
            FuzzyOrder::Score => o.order = ffi::ScFuzzyOrder_SC_FUZZY_ORDER_SCORE,
            FuzzyOrder::Insertion => {
                o.order = ffi::ScFuzzyOrder_SC_FUZZY_ORDER_INSERTION;
            }
            FuzzyOrder::Column(col) => {
                o.order = ffi::ScFuzzyOrder_SC_FUZZY_ORDER_COLUMN;
                o.order_column = col;
            }
        }
        o.empty_text = empty.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
        o.initial_query =
            initial.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
        if let Some(c) = opts.toggle_all_key {
            o.toggle_all_key = c.0;
        }
        if let Some(c) = opts.toggle_section_key {
            o.toggle_section_key = c.0;
        }
        o.modal = opts.modal;
        o.start_in_insert = opts.start_in_insert;
        o.hide_mode_badge = opts.hide_mode_badge;
        o.normal_label =
            normal_label.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
        o.insert_label =
            insert_label.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
        o.mode_normal_style = opts.mode_normal_style.raw();
        o.mode_insert_style = opts.mode_insert_style.raw();
        if let Some(c) = opts.insert_key {
            o.insert_key = c.0;
        }
        if let Some(c) = opts.normal_key {
            o.normal_key = c.0;
        }
        if let Some(c) = opts.clear_key {
            o.clear_key = c.0;
        }
        let ptr = unsafe { ffi::sc_fuzzy_new(o) };
        assert!(!ptr.is_null(), "sc_fuzzy_new: out of memory");
        Fuzzy {
            ptr,
            _prompt: prompt,
            count: 0,
        }
    }
    /// Adds a single-column item (list view).
    pub fn add(&mut self, label: &str) -> &mut Self {
        unsafe { ffi::sc_fuzzy_add(self.ptr, cstring(label).as_ptr()) };
        self.count += 1;
        self
    }
    /// Adds a multi-column row (table view). The query searches the columns
    /// selected by [`FuzzyOpts::search_columns`].
    pub fn add_row<I, S>(&mut self, fields: I) -> &mut Self
    where
        I: IntoIterator<Item = S>,
        S: AsRef<str>,
    {
        let cs: Vec<CString> =
            fields.into_iter().map(|f| cstring(f.as_ref())).collect();
        let ptrs: Vec<*const c_char> = cs.iter().map(|c| c.as_ptr()).collect();
        unsafe { ffi::sc_fuzzy_add_row(self.ptr, ptrs.as_ptr(), ptrs.len()) };
        self.count += 1;
        self
    }
    /// Adds a non-selectable section header (e.g. a day in a todo list).
    pub fn add_section(&mut self, title: &str) -> &mut Self {
        unsafe { ffi::sc_fuzzy_add_section(self.ptr, cstring(title).as_ptr()) };
        self.count += 1;
        self
    }
    /// Adds a section header with its own style; `style.bg` fills the bar
    /// (merged over the global section style).
    pub fn add_section_styled(&mut self, title: &str, style: Style)
        -> &mut Self
    {
        unsafe {
            ffi::sc_fuzzy_add_section_styled(self.ptr, cstring(title).as_ptr(),
                style.raw());
        }
        self.count += 1;
        self
    }
    /// Adds a section header with a rich [`Text`] title (deep-copied); `fill`
    /// paints the full-width bar and styles the optional count suffix.
    pub fn add_section_text(&mut self, title: &Text, fill: Style) -> &mut Self {
        unsafe {
            ffi::sc_fuzzy_add_section_text(self.ptr, title.as_ptr(), fill.raw());
        }
        self.count += 1;
        self
    }
    /// Adds a single item with a base text style (whole-cell color).
    pub fn add_styled(&mut self, label: &str, style: Style) -> &mut Self {
        unsafe {
            ffi::sc_fuzzy_add_styled(self.ptr, cstring(label).as_ptr(),
                style.raw());
        }
        self.count += 1;
        self
    }
    /// Adds a multi-column row with a per-cell base style (padded to the field
    /// count). The match highlight is overlaid on top.
    pub fn add_row_styled<I, S>(&mut self, fields: I, styles: &[Style])
        -> &mut Self
    where
        I: IntoIterator<Item = S>,
        S: AsRef<str>,
    {
        let cs: Vec<CString> =
            fields.into_iter().map(|f| cstring(f.as_ref())).collect();
        let ptrs: Vec<*const c_char> = cs.iter().map(|c| c.as_ptr()).collect();
        let mut sty: Vec<ffi::ScTextStyle> =
            styles.iter().map(|s| s.raw()).collect();
        sty.resize(ptrs.len(), unsafe { mem::zeroed() });
        unsafe {
            ffi::sc_fuzzy_add_row_styled(self.ptr, ptrs.as_ptr(),
                sty.as_ptr(), ptrs.len());
        }
        self.count += 1;
        self
    }
    /// Adds a row of rich [`Text`] cells (deep-copied; the match key is each
    /// cell's flattened text).
    pub fn add_row_rich(&mut self, cells: &[Text]) -> &mut Self {
        // The C side only reads (clones) the cells, so casting away const is
        // sound: it never mutates the borrowed ScText.
        let ptrs: Vec<*mut ffi::ScText> =
            cells.iter().map(|t| t.as_ptr() as *mut ffi::ScText).collect();
        unsafe {
            ffi::sc_fuzzy_add_row_rich(self.ptr, ptrs.as_ptr(), ptrs.len());
        }
        self.count += 1;
        self
    }
    /// Greys out the row at `index` (add order) and makes it unselectable.
    pub fn set_disabled(&mut self, index: usize, on: bool) {
        unsafe { ffi::sc_fuzzy_set_disabled(self.ptr, index, on) };
    }
    /// Sets a stable caller id on the row at `index` (add order).
    pub fn set_id(&mut self, index: usize, id: u64) {
        unsafe { ffi::sc_fuzzy_set_id(self.ptr, index, id) };
    }
    /// Stable id of the row at `index` (add order), or 0.
    pub fn id_at(&self, index: usize) -> u64 {
        unsafe { ffi::sc_fuzzy_id_at(self.ptr, index) }
    }
    /// Stable id of the currently highlighted row, or 0.
    pub fn cursor_id(&self) -> u64 {
        unsafe { ffi::sc_fuzzy_cursor_id(self.ptr) }
    }
    /// Pre-checks/unchecks the row at `index` (multi-select).
    pub fn set_checked(&mut self, index: usize, on: bool) {
        unsafe { ffi::sc_fuzzy_set_checked(self.ptr, index, on) };
    }
    /// Whether the row at `index` (add order) is checked.
    pub fn is_checked(&self, index: usize) -> bool {
        unsafe { ffi::sc_fuzzy_is_checked(self.ptr, index) }
    }
    /// Checks or unchecks every selectable row.
    pub fn check_all(&mut self, on: bool) {
        unsafe { ffi::sc_fuzzy_check_all(self.ptr, on) };
    }
    /// Number of checked rows.
    pub fn checked_count(&self) -> usize {
        unsafe { ffi::sc_fuzzy_checked_count(self.ptr) }
    }
    /// Pre-positions the cursor on `index` (add order, clamped to a selectable).
    pub fn set_cursor(&mut self, index: usize) {
        unsafe { ffi::sc_fuzzy_set_cursor(self.ptr, index) };
    }
    /// First field of the row at `index` (add order), or `None`.
    pub fn label(&self, index: usize) -> Option<String> {
        let p = unsafe { ffi::sc_fuzzy_label(self.ptr, index) };
        if p.is_null() {
            return None;
        }
        Some(
            unsafe { std::ffi::CStr::from_ptr(p) }
                .to_string_lossy()
                .into_owned(),
        )
    }
    /// Replaces the first field of the row at `index` (add order).
    pub fn set_label(&mut self, index: usize, label: &str) {
        unsafe {
            ffi::sc_fuzzy_set_label(self.ptr, index, cstring(label).as_ptr());
        }
    }
    /// Replaces all fields of the row at `index` (add order).
    pub fn set_row<I, S>(&mut self, index: usize, fields: I)
    where
        I: IntoIterator<Item = S>,
        S: AsRef<str>,
    {
        let cs: Vec<CString> =
            fields.into_iter().map(|f| cstring(f.as_ref())).collect();
        let ptrs: Vec<*const c_char> = cs.iter().map(|c| c.as_ptr()).collect();
        unsafe {
            ffi::sc_fuzzy_set_row(self.ptr, index, ptrs.as_ptr(), ptrs.len());
        }
    }
    /// Sets the base style of cell `col` in the row at `index`.
    pub fn set_row_style(&mut self, index: usize, col: usize, style: Style) {
        unsafe {
            ffi::sc_fuzzy_set_row_style(self.ptr, index, col, style.raw());
        }
    }
    pub fn cursor_index(&self) -> usize {
        unsafe { ffi::sc_fuzzy_cursor_index(self.ptr) }
    }
    /// Whether a row matches the current query (the selection is valid).
    /// Unlike [`cursor_index`](Self::cursor_index), this distinguishes "no
    /// match" from "row 0" - useful in a forward/submit shortcut callback.
    pub fn has_selection(&self) -> bool {
        unsafe { ffi::sc_fuzzy_has_selection(self.ptr) }
    }
    pub fn remove(&mut self, index: usize) {
        unsafe { ffi::sc_fuzzy_remove(self.ptr, index) };
        self.count = self.count.saturating_sub(1);
    }
    /// Re-applies the query after appending rows mid-run (e.g. from a refresh
    /// shortcut), so new items appear without closing the finder.
    pub fn refresh(&mut self) {
        unsafe { ffi::sc_fuzzy_refresh(self.ptr) };
    }
    /// Runs the finder → the chosen item's original add-order index.
    pub fn run(&mut self) -> Result<Option<usize>> {
        let mut out = 0usize;
        let st = unsafe { ffi::sc_fuzzy_run(self.ptr, &mut out) };
        Ok(status(st)?.then_some(out))
    }
    /// Runs the finder in multi-select mode → the checked add-order indices
    /// (empty when nothing was checked), or `None` on cancel.
    pub fn run_multi(&mut self) -> Result<Option<Vec<usize>>> {
        let cap = self.count.max(1);
        let mut buf = vec![0usize; cap];
        let mut n = cap;
        let st =
            unsafe { ffi::sc_fuzzy_run_multi(self.ptr, buf.as_mut_ptr(), &mut n) };
        Ok(status(st)?.then(|| {
            buf.truncate(n);
            buf
        }))
    }
}

impl Drop for Fuzzy {
    fn drop(&mut self) {
        unsafe { ffi::sc_fuzzy_free(self.ptr) };
    }
}

/* ── Date picker ─────────────────────────────────────────────────────────── */

/// A calendar date.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Date {
    pub year: i32,
    /// 1-12.
    pub month: u32,
    /// 1-31.
    pub day: u32,
}

impl Date {
    /// True for the "no date" sentinel (`day == 0`), as returned by
    /// [`datepicker`] on a clear with [`DatePickerOpts::allow_clear`].
    pub fn is_empty(&self) -> bool {
        self.day == 0
    }
}

/// Options for [`datepicker`].
#[derive(Default)]
pub struct DatePickerOpts {
    pub prompt: Option<String>,
    pub week_start: WeekStart,
    pub accent: Color,
    pub box_: BoxStyle,
    /// Allow clearing the date with Delete/Backspace; the picker then returns a
    /// present-but-empty [`Date`] (`is_empty()`) meaning "no date".
    pub allow_clear: bool,
}

impl DatePickerOpts {
    pub fn new() -> Self {
        DatePickerOpts::default()
    }
    pub fn prompt(mut self, s: impl Into<String>) -> Self {
        self.prompt = Some(s.into());
        self
    }
    pub fn week_start(mut self, w: WeekStart) -> Self {
        self.week_start = w;
        self
    }
    pub fn accent(mut self, c: Color) -> Self {
        self.accent = c;
        self
    }
    /// Frame the calendar in a panel (border, background, padding, margin).
    pub fn box_style(mut self, b: BoxStyle) -> Self {
        self.box_ = b;
        self
    }
    /// Allow clearing the date to "no date" with Delete/Backspace.
    pub fn allow_clear(mut self) -> Self {
        self.allow_clear = true;
        self
    }
}

/// Month-grid date picker. `seed` defaults to today when `None`.
pub fn datepicker(
    seed: Option<Date>,
    opts: DatePickerOpts,
) -> Result<Option<Date>> {
    let prompt = opts.prompt.as_deref().map(cstring);
    let mut o: ffi::ScDatePickerOpts = unsafe { mem::zeroed() };
    o.prompt = prompt.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
    o.week_start = opts.week_start.raw();
    o.accent = opts.accent.raw();
    o.box_ = opts.box_.raw();
    o.allow_clear = opts.allow_clear;

    let mut tm: ffi::tm = unsafe { mem::zeroed() };
    if let Some(d) = seed {
        tm.tm_year = d.year - 1900;
        tm.tm_mon = d.month as c_int - 1;
        tm.tm_mday = d.day as c_int;
    }
    let st = unsafe { ffi::sc_datepicker(&mut tm, o) };
    if status(st)? {
        Ok(Some(Date {
            year: tm.tm_year + 1900,
            month: (tm.tm_mon + 1) as u32,
            day: tm.tm_mday as u32,
        }))
    } else {
        Ok(None)
    }
}

/* ── Form ────────────────────────────────────────────────────────────────── */

repr_enum!(
    /// How a field's grid-column width is sized.
    FieldWidthMode {
        Auto = ffi::ScFieldWidthMode_SC_FWIDTH_AUTO,
        Pct = ffi::ScFieldWidthMode_SC_FWIDTH_PCT,
        Fixed = ffi::ScFieldWidthMode_SC_FWIDTH_FIXED,
    } default Auto
);

/// Per-field layout and behaviour. All fields are optional (zero-init defaults).
///
/// The C `validate` callback is intentionally not exposed here.
#[derive(Default, Clone)]
pub struct FieldOpts {
    pub width_mode: FieldWidthMode,
    /// Percent (Pct) or column count (Fixed); ignored for Auto.
    pub width: i32,
    /// Columns spanned (0 = 1).
    pub col_span: i32,
    /// Rows spanned (0 = 1).
    pub row_span: i32,
    /// Content lines inside the box (0 = 1).
    pub height: i32,
    /// Full-screen forms only: grow this field's row to consume the remaining
    /// terminal height (e.g. a multiline body that fills the screen). `height`
    /// acts as the minimum; if several fields set it, the first wins.
    pub fill_height: bool,
    pub required: bool,
    /// Text field: multi-line value edited via the external editor.
    pub multiline: bool,
    /// Date field: the date may be absent. The field starts empty when its
    /// `initial` is `None`, Delete/Backspace clears it while editing, and
    /// [`Form::get_date`] returns `None` for an empty field.
    pub date_optional: bool,
    /// One-line help shown in the editor region.
    pub help: Option<String>,
    /// Box border (zero = rounded).
    pub border: BorderStyle,
    /// Display-only: the field is focusable and rendered, but its value can
    /// never change (the editor never opens, a bool does not toggle, value
    /// cycling is blocked). Use for a value edited elsewhere (e.g. a separate
    /// wizard). Default = editable.
    pub read_only: bool,
    /// Skip this field in all focus navigation (arrows, Tab/Shift-Tab, the
    /// initial active field, autoedit) — the cursor can never land on it. A
    /// non-selectable field cannot block submit (its `required` is treated as
    /// satisfied). Combine with [`read_only`](Self::read_only) for a
    /// display-only, unfocusable field. Default = selectable.
    pub not_selectable: bool,
}

/// Builds an `ffi::ScFieldOpts`; the returned `CString` (if any) backs `help`
/// and must outlive the FFI call.
fn field_ffi(o: &FieldOpts) -> (ffi::ScFieldOpts, Option<CString>) {
    let help = o.help.as_deref().map(cstring);
    let mut f: ffi::ScFieldOpts = unsafe { mem::zeroed() };
    f.width_mode = o.width_mode.raw();
    f.width = o.width;
    f.col_span = o.col_span;
    f.row_span = o.row_span;
    f.height = o.height;
    f.fill_height = o.fill_height;
    f.required = o.required;
    f.multiline = o.multiline;
    f.date_optional = o.date_optional;
    f.read_only = o.read_only;
    f.not_selectable = o.not_selectable;
    f.help = help.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
    f.border = o.border.raw();
    (f, help)
}

/// Options for a [`Form`].
#[derive(Default)]
pub struct FormOpts {
    pub title: Option<String>,
    pub title_style: Style,
    pub accent: Color,
    pub hint: Option<String>,
    pub hint_layout: HintLayout,
    pub hint_pos: HintPos,
    pub hint_style: Style,
    pub summary_style: Style,
    pub hide_summary: bool,
    /// Stop at the grid edges instead of wrapping arrow navigation (default:
    /// arrows cycle around the edges; Tab/Shift-Tab always cycle).
    pub no_cycle: bool,
    /// Open the editor for the initial field as soon as the form starts instead
    /// of starting in navigation mode (e.g. type a new record's title right
    /// away). No effect on a bool field; multiline text fields are only focused.
    pub autoedit: bool,
    /// External editor command for multiline fields (None = $VISUAL/$EDITOR).
    pub editor: Option<String>,
    /// Key that opens the editor (None = Ctrl-G).
    pub editor_key: Option<Chord>,
    /// Extension for the editor's temp file (e.g. `".md"`) so the editor detects
    /// the filetype; `None`/empty = no extension.
    pub editor_suffix: Option<String>,
    /// Background of the editor box below the grid (default: a subtle gray).
    pub edit_bg: Color,
    /// Full-screen mode: compose [valign-pad][header][grid] filling the terminal
    /// (consistent shell alongside a fullscreen finder). Run inside an
    /// [`AltScreen`](crate::AltScreen).
    pub fullscreen: bool,
    /// Vertical alignment of the form within the screen (fullscreen only).
    pub valign: VAlign,
    /// What [`valign`](Self::valign) applies to (fullscreen only). `All` aligns
    /// the whole header+grid+footer block; `Content` pins the header to the top
    /// and the footer to the bottom, aligning only the grid in between.
    pub valign_scope: ValignScope,
    /// Header pinned above the grid (fullscreen only). Borrowed: the
    /// [`Rendered`] must outlive the run.
    pub header: Option<NonNull<ffi::ScRendered>>,
    /// Prefix on a modified field's title (e.g. `"[*] "`); `None` = no marker.
    /// Appears when the field differs from its initial value. See
    /// [`Form::modified`].
    pub modified_marker: Option<String>,
}

impl FormOpts {
    pub fn new() -> Self {
        FormOpts::default()
    }
    /// Enables full-screen mode (header + valign over the terminal).
    pub fn fullscreen(mut self) -> Self {
        self.fullscreen = true;
        self
    }
    /// Sets the vertical alignment of the block (fullscreen only).
    pub fn valign(mut self, v: VAlign) -> Self {
        self.valign = v;
        self
    }
    /// Pins a borrowed header above the grid (fullscreen only). The
    /// [`Rendered`] must outlive the run.
    pub fn header(mut self, header: &Rendered) -> Self {
        self.header = NonNull::new(header.as_ptr() as *mut ffi::ScRendered);
        self
    }
}

/// An owning grid-layout form. Add fields row by row, [`run`](Form::run) it,
/// then read values back with the `get_*` methods.
pub struct Form {
    ptr: *mut ffi::ScForm,
    count: usize,
}

impl Form {
    pub fn new(opts: FormOpts) -> Form {
        let title = opts.title.as_deref().map(cstring);
        let hint = opts.hint.as_deref().map(cstring);
        let editor = opts.editor.as_deref().map(cstring);
        let editor_suffix = opts.editor_suffix.as_deref().map(cstring);
        let marker = opts.modified_marker.as_deref().map(cstring);
        let mut o: ffi::ScFormOpts = unsafe { mem::zeroed() };
        o.title = title.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
        o.title_style = opts.title_style.raw();
        o.accent = opts.accent.raw();
        o.hint = hint.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
        o.hint_layout = opts.hint_layout.raw();
        o.hint_pos = opts.hint_pos.raw();
        o.hint_style = opts.hint_style.raw();
        o.summary_style = opts.summary_style.raw();
        o.hide_summary = opts.hide_summary;
        o.no_cycle = opts.no_cycle;
        o.autoedit = opts.autoedit;
        o.editor = editor.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
        if let Some(c) = opts.editor_key {
            o.editor_key = c.0;
        }
        o.editor_suffix =
            editor_suffix.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
        o.edit_bg = opts.edit_bg.raw();
        o.fullscreen = opts.fullscreen;
        o.valign = opts.valign.raw();
        o.valign_scope = opts.valign_scope.raw();
        o.header = opts.header.map_or(std::ptr::null(), |p| p.as_ptr());
        o.modified_marker =
            marker.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
        // Strings are copied by the C side, so the temporaries above suffice.
        let ptr = unsafe { ffi::sc_form_new(o) };
        assert!(!ptr.is_null(), "sc_form_new: out of memory");
        Form { ptr, count: 0 }
    }

    /// Whether any field differs from the value it was added with (e.g. for an
    /// "unsaved changes?" prompt when the user cancels).
    pub fn modified(&self) -> bool {
        unsafe { ffi::sc_form_modified(self.ptr) }
    }

    /// Starts a new grid row.
    pub fn row_begin(&mut self) -> &mut Self {
        unsafe { ffi::sc_form_row_begin(self.ptr) };
        self
    }
    /// Placeholder under a col/row-spanning field.
    pub fn add_skip(&mut self) -> &mut Self {
        unsafe { ffi::sc_form_add_skip(self.ptr) };
        self
    }

    /// Adds a text field; returns its index.
    pub fn add_text(&mut self, label: &str, initial: &str,
                    opts: FieldOpts) -> usize {
        let l = cstring(label);
        let i = cstring(initial);
        let (fo, _h) = field_ffi(&opts);
        let idx = unsafe {
            ffi::sc_form_add_text(self.ptr, l.as_ptr(), i.as_ptr(), fo)
        };
        self.count += 1;
        idx as usize
    }
    /// Adds a number field; returns its index.
    pub fn add_number(&mut self, label: &str, initial: f64,
                      opts: FieldOpts) -> usize {
        let l = cstring(label);
        let (fo, _h) = field_ffi(&opts);
        let idx =
            unsafe { ffi::sc_form_add_number(self.ptr, l.as_ptr(), initial, fo) };
        self.count += 1;
        idx as usize
    }
    /// Adds a bool field; returns its index.
    pub fn add_bool(&mut self, label: &str, initial: bool,
                    opts: FieldOpts) -> usize {
        let l = cstring(label);
        let (fo, _h) = field_ffi(&opts);
        let idx =
            unsafe { ffi::sc_form_add_bool(self.ptr, l.as_ptr(), initial, fo) };
        self.count += 1;
        idx as usize
    }
    /// Adds a single-choice field; returns its index.
    pub fn add_select(&mut self, label: &str, choices: &[&str],
                      initial: usize, opts: FieldOpts) -> usize {
        let l = cstring(label);
        let cs: Vec<CString> = choices.iter().map(|s| cstring(s)).collect();
        let ptrs: Vec<*const c_char> = cs.iter().map(|c| c.as_ptr()).collect();
        let (fo, _h) = field_ffi(&opts);
        let idx = unsafe {
            ffi::sc_form_add_select(self.ptr, l.as_ptr(), ptrs.as_ptr(),
                                    ptrs.len(), initial, fo)
        };
        self.count += 1;
        idx as usize
    }
    /// Adds a multi-choice field; returns its index.
    pub fn add_multiselect(&mut self, label: &str, choices: &[&str],
                           checked: &[usize], opts: FieldOpts) -> usize {
        let l = cstring(label);
        let cs: Vec<CString> = choices.iter().map(|s| cstring(s)).collect();
        let ptrs: Vec<*const c_char> = cs.iter().map(|c| c.as_ptr()).collect();
        let (fo, _h) = field_ffi(&opts);
        let idx = unsafe {
            ffi::sc_form_add_multiselect(self.ptr, l.as_ptr(), ptrs.as_ptr(),
                                         ptrs.len(), checked.as_ptr(),
                                         checked.len(), fo)
        };
        self.count += 1;
        idx as usize
    }
    /// Adds a date field; returns its index. `initial = None` seeds today.
    pub fn add_date(&mut self, label: &str, initial: Option<Date>,
                    opts: FieldOpts) -> usize {
        let l = cstring(label);
        let (fo, _h) = field_ffi(&opts);
        let mut tm: ffi::tm = unsafe { mem::zeroed() };
        if let Some(d) = initial {
            tm.tm_year = d.year - 1900;
            tm.tm_mon = d.month as c_int - 1;
            tm.tm_mday = d.day as c_int;
        }
        let idx =
            unsafe { ffi::sc_form_add_date(self.ptr, l.as_ptr(), tm, fo) };
        self.count += 1;
        idx as usize
    }

    /// Runs the form. `Ok(true)` = submitted, `Ok(false)` = cancelled.
    pub fn run(&mut self) -> Result<bool> {
        status(unsafe { ffi::sc_form_run(self.ptr) })
    }

    /// Current text of `field`, or `None` if out of range.
    pub fn get_string(&self, field: usize) -> Option<String> {
        let p = unsafe { ffi::sc_form_get_string(self.ptr, field as c_int) };
        if p.is_null() {
            None
        } else {
            Some(unsafe {
                std::ffi::CStr::from_ptr(p).to_string_lossy().into_owned()
            })
        }
    }
    pub fn get_number(&self, field: usize) -> f64 {
        unsafe { ffi::sc_form_get_number(self.ptr, field as c_int) }
    }
    pub fn get_bool(&self, field: usize) -> bool {
        unsafe { ffi::sc_form_get_bool(self.ptr, field as c_int) }
    }
    pub fn get_choice(&self, field: usize) -> usize {
        unsafe { ffi::sc_form_get_choice(self.ptr, field as c_int) }
    }
    /// Checked add-order indices of a multiselect field.
    pub fn get_checked(&self, field: usize) -> Vec<usize> {
        let f = field as c_int;
        let n = unsafe {
            ffi::sc_form_get_checked(self.ptr, f, std::ptr::null_mut(), 0)
        };
        let mut out = vec![0usize; n];
        if n > 0 {
            unsafe {
                ffi::sc_form_get_checked(self.ptr, f, out.as_mut_ptr(), n);
            }
        }
        out
    }
    /// Picked date of a date field, or `None` if not a date field.
    pub fn get_date(&self, field: usize) -> Option<Date> {
        let mut tm: ffi::tm = unsafe { mem::zeroed() };
        if unsafe { ffi::sc_form_get_date(self.ptr, field as c_int, &mut tm) } {
            Some(Date {
                year: tm.tm_year + 1900,
                month: (tm.tm_mon + 1) as u32,
                day: tm.tm_mday as u32,
            })
        } else {
            None
        }
    }
}

impl Drop for Form {
    fn drop(&mut self) {
        unsafe { ffi::sc_form_free(self.ptr) };
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::{CStr, CString};

    // The validator trampoline must: accept Ok values (return true, write no
    // error) and, for Err values, return false and expose the message through
    // err_out, keeping it alive past the call.
    #[test]
    fn validator_trampoline_reports_error_message() {
        let mut state = ValidatorState {
            f: Box::new(|v: &str| {
                if v == "ok" {
                    Ok(())
                } else {
                    Err(format!("bad: {v}"))
                }
            }),
            last_err: None,
        };
        let ctx = &mut state as *mut ValidatorState as *mut c_void;

        let ok = CString::new("ok").unwrap();
        let mut err: *const c_char = std::ptr::null();
        let r = unsafe { validate_tramp(ok.as_ptr(), ctx, &mut err) };
        assert!(r);
        assert!(err.is_null());

        let bad = CString::new("xy").unwrap();
        let r = unsafe { validate_tramp(bad.as_ptr(), ctx, &mut err) };
        assert!(!r);
        assert!(!err.is_null());
        let msg = unsafe { CStr::from_ptr(err).to_str().unwrap() };
        assert_eq!(msg, "bad: xy");
    }
}
