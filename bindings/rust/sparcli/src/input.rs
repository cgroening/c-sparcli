//! Interactive input widgets, custom shortcuts and the external-editor flow.
//!
//! Every prompt returns `Result<Option<T>>`: `Ok(Some(v))` = a value,
//! `Ok(None)` = cancelled (Esc / Ctrl-C), `Err(Error::Unavailable)` = no TTY or
//! a read error.

use crate::error::{Error, Result};
use crate::style::{cstring, BorderStyle, Color, HintLayout, HintPos, Style};
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
    pub boxed: bool,
    pub border: BorderStyle,
    pub width: i32,
    pub char_filter: Option<CharFilter>,
    pub suggestions: Vec<String>,
    pub hint_layout: HintLayout,
    pub hint_pos: HintPos,
    pub shortcuts: Option<&'a Shortcuts>,
    pub prompt_text: Option<&'a Text>,
    pub prompt_markup: bool,
    pub external_editor: bool,
    pub editor: Option<String>,
    pub editor_key: Option<Chord>,
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
    pub fn boxed(mut self, w: i32) -> Self {
        self.boxed = true;
        self.width = w;
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
    o.boxed = opts.boxed;
    o.border = opts.border.raw();
    o.width = opts.width;
    o.hint_layout = opts.hint_layout.raw();
    o.hint_pos = opts.hint_pos.raw();
    if let Some(f) = opts.char_filter {
        o.char_filter = f.raw();
    }
    if !sugg_ptrs.is_empty() {
        o.suggestions = sugg_ptrs.as_ptr();
        o.n_suggestions = sugg_ptrs.len();
    }
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
    pub boxed: bool,
    pub width: i32,
}

impl PasswordOpts {
    pub fn new() -> Self {
        PasswordOpts::default()
    }
    pub fn mask(mut self, m: impl Into<String>) -> Self {
        self.mask = Some(m.into());
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
    o.boxed = opts.boxed;
    o.width = opts.width;
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
    pub boxed: bool,
    pub width: i32,
    /// Decimal separator shown and accepted while editing; `'\0'` or `'.'`
    /// = period (default), `','` = comma.
    pub decimal_sep: char,
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
    fn raw(&self) -> ffi::ScNumberOpts {
        let mut o: ffi::ScNumberOpts = unsafe { mem::zeroed() };
        o.initial = self.initial;
        o.start_empty = self.start_empty;
        o.min = self.min;
        o.max = self.max;
        o.step = self.step;
        o.decimals = self.decimals;
        o.hide_summary = self.hide_summary;
        o.boxed = self.boxed;
        o.width = self.width;
        o.decimal_sep = if self.decimal_sep == '\0' {
            0
        } else {
            self.decimal_sep as u8 as c_char
        };
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
    pub boxed: bool,
    pub width: i32,
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
    pub fn boxed(mut self, w: i32) -> Self {
        self.boxed = true;
        self.width = w;
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
    o.boxed = opts.boxed;
    o.width = opts.width;
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
    pub max_visible: i32,
    pub accent: Color,
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
    pub fn max_visible(mut self, n: i32) -> Self {
        self.max_visible = n;
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
        o.max_visible = opts.max_visible;
        o.accent = opts.accent.raw();
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
    pub accent: Color,
}

impl FuzzyOpts {
    pub fn new() -> Self {
        FuzzyOpts::default()
    }
    pub fn prompt(mut self, s: impl Into<String>) -> Self {
        self.prompt = Some(s.into());
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
        o.accent = opts.accent.raw();
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
