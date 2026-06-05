//! Interactive input widgets, custom shortcuts and the external-editor flow.
//!
//! Every prompt returns `Result<Option<T>>`: `Ok(Some(v))` = a value,
//! `Ok(None)` = cancelled (Esc / Ctrl-C), `Err(Error::Unavailable)` = no TTY or
//! a read error.

use crate::error::{Error, Result};
use crate::style::{
    cstring, BorderStyle, BoxStyle, Color, HintLayout, HintPos, Style,
};
use crate::text::Text;
use sparcli_sys as ffi;

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

impl Chord {
    /// A short display name, e.g. `"F2"`, `"^E"`, `"M-e"`.
    pub fn name(&self) -> String {
        let mut buf = [0i8; 16];
        unsafe { ffi::sc_key_chord_name(self.0, buf.as_mut_ptr(), buf.len()) };
        unsafe {
            std::ffi::CStr::from_ptr(buf.as_ptr())
                .to_string_lossy()
                .into_owned()
        }
    }
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
    labels: Vec<CString>,
    callbacks: Vec<*mut c_void>,
    fired: Box<Cell<c_int>>,
}

impl Shortcuts {
    pub fn new() -> Self {
        Shortcuts {
            items: Vec::new(),
            labels: Vec::new(),
            callbacks: Vec::new(),
            fired: Box::new(Cell::new(-1)),
        }
    }

    /// Binds a chord that ends the prompt and reports `id` via
    /// [`fired`](Self::fired).
    pub fn on_return(
        mut self,
        chord: Chord,
        id: i32,
        label: Option<&str>,
    ) -> Self {
        let hint = label.map(cstring);
        let hint_ptr = match &hint {
            Some(c) => c.as_ptr(),
            None => std::ptr::null(),
        };
        if let Some(c) = hint {
            self.labels.push(c);
        }
        self.items.push(ffi::ScShortcut {
            chord: chord.0,
            id,
            mode: ffi::ScShortcutMode_SC_SHORTCUT_RETURN,
            on_fire: None,
            user: std::ptr::null_mut(),
            hint_label: hint_ptr,
        });
        self
    }

    /// Binds a chord to a callback (returns `true` to keep the prompt open).
    pub fn on_callback<F>(
        mut self,
        chord: Chord,
        label: Option<&str>,
        f: F,
    ) -> Self
    where
        F: FnMut(i32) -> bool + 'static,
    {
        let hint = label.map(cstring);
        let hint_ptr = match &hint {
            Some(c) => c.as_ptr(),
            None => std::ptr::null(),
        };
        if let Some(c) = hint {
            self.labels.push(c);
        }
        let boxed: Box<ShortcutFn> = Box::new(Box::new(f));
        let user = Box::into_raw(boxed) as *mut c_void;
        self.callbacks.push(user);
        self.items.push(ffi::ScShortcut {
            chord: chord.0,
            id: 0,
            mode: ffi::ScShortcutMode_SC_SHORTCUT_CALLBACK,
            on_fire: Some(shortcut_tramp),
            user,
            hint_label: hint_ptr,
        });
        self
    }

    /// The id of the RETURN-mode shortcut that ended the last run, or `-1`.
    pub fn fired(&self) -> i32 {
        self.fired.get()
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
pub fn text_input(prompt: &str, opts: TextInputOpts) -> Result<Option<String>> {
    let mut o: ffi::ScTextInputOpts = unsafe { mem::zeroed() };
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
}

impl PasswordOpts {
    pub fn new() -> Self {
        PasswordOpts::default()
    }
    pub fn mask(mut self, m: impl Into<String>) -> Self {
        self.mask = Some(m.into());
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
    opts: PasswordOpts,
) -> Result<Option<String>> {
    let mut o: ffi::ScPasswordOpts = unsafe { mem::zeroed() };
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

/// Options for a [`Fuzzy`] finder.
#[derive(Default)]
pub struct FuzzyOpts {
    pub prompt: Option<String>,
    pub max_visible: i32,
    pub no_cycle: bool,
    pub accent: Color,
    pub selected_style: Style,
    pub box_: BoxStyle,
}

impl FuzzyOpts {
    pub fn new() -> Self {
        FuzzyOpts::default()
    }
    pub fn prompt(mut self, s: impl Into<String>) -> Self {
        self.prompt = Some(s.into());
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
}

/// An owning incremental fuzzy finder (list view).
pub struct Fuzzy {
    ptr: *mut ffi::ScFuzzy,
    _prompt: Option<CString>,
}

impl Fuzzy {
    pub fn new(opts: FuzzyOpts) -> Fuzzy {
        let prompt = opts.prompt.as_deref().map(cstring);
        let mut o: ffi::ScFuzzyOpts = unsafe { mem::zeroed() };
        o.prompt = prompt.as_ref().map_or(std::ptr::null(), |c| c.as_ptr());
        o.max_visible = opts.max_visible;
        o.no_cycle = opts.no_cycle;
        o.accent = opts.accent.raw();
        o.selected_style = opts.selected_style.raw();
        o.box_ = opts.box_.raw();
        let ptr = unsafe { ffi::sc_fuzzy_new(o) };
        assert!(!ptr.is_null(), "sc_fuzzy_new: out of memory");
        Fuzzy {
            ptr,
            _prompt: prompt,
        }
    }
    pub fn add(&mut self, label: &str) -> &mut Self {
        unsafe { ffi::sc_fuzzy_add(self.ptr, cstring(label).as_ptr()) };
        self
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
    }
    /// Runs the finder → the chosen item's original add-order index.
    pub fn run(&mut self) -> Result<Option<usize>> {
        let mut out = 0usize;
        let st = unsafe { ffi::sc_fuzzy_run(self.ptr, &mut out) };
        Ok(status(st)?.then_some(out))
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

/// Options for [`datepicker`].
#[derive(Default)]
pub struct DatePickerOpts {
    pub prompt: Option<String>,
    pub week_start: WeekStart,
    pub accent: Color,
    pub box_: BoxStyle,
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
