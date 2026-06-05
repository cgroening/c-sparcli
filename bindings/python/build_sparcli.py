"""cffi build script for the sparcli Python binding (API / out-of-line mode).

``set_source`` ``#include``s the real ``sparcli.h`` and compiles the vendored C
sources straight into the extension module, so a plain ``pip install`` needs
only a C compiler - no prior ``make`` and no system-installed library (the same
self-contained story as the Rust ``cc`` build).

The ``cdef`` below uses cffi's *partial struct* syntax (``...;`` at the end of a
struct body): only the fields the Python layer actually touches are listed, and
the C compiler fills in the true size, layout and remaining fields. That keeps
the declaration small and robust to additive field changes in the C headers - a
genuine layout mismatch fails the build instead of corrupting memory at runtime.
"""

from __future__ import annotations

import os

from cffi import FFI

# ── Repo paths ──────────────────────────────────────────────────────────────
# build_sparcli.py lives at bindings/python/ → repo root is two levels up.
_HERE = os.path.dirname(os.path.abspath(__file__))

# The repo's C tree is exposed *inside* this directory via the ``csrc`` and
# ``cinclude`` symlinks (-> ../../src, ../../include). setuptools requires
# extension source/include paths to be relative to the setup.py directory and
# refuses paths that escape it ("..") or are absolute, so we reference the C
# files through these in-package links - a single source of truth, no copy.
_SRC = "csrc"
_INCLUDE = "cinclude"

# C sources, mirroring the `SRC` list in the repo Makefile (and the Rust
# build.rs `SOURCES` array). Only `table/table.c` is compiled directly; it
# `#include`s its siblings.
_SOURCES = [
    "core/output.c",
    "core/version.c",
    "core/text_attributes.c",
    "core/print.c",
    "core/color.c",
    "core/text.c",
    "core/render_wrap.c",
    "core/sanitize.c",
    "output/panel.c",
    "output/table/table.c",
    "output/table/table_print.c",
    "output/table/table_print_init.c",
    "output/table/table_print_render.c",
    "output/table/table_print_render_cell.c",
    "output/table/table_print_render_border.c",
    "output/table/table_print_render_row.c",
    "output/columns.c",
    "output/rule.c",
    "output/tree.c",
    "output/list.c",
    "output/progressbar.c",
    "output/spinner.c",
    "output/kv.c",
    "output/alert.c",
    "output/badge.c",
    "output/util.c",
    "output/pad.c",
    "output/markup.c",
    "tty/term.c",
    "tty/key.c",
    "tty/screen.c",
    "input/prompt.c",
    "input/line_editor.c",
    "input/theme.c",
    "input/shortcut.c",
    "input/editor.c",
    "input/confirm.c",
    "input/text_input.c",
    "input/password_input.c",
    "input/number_input.c",
    "input/calc.c",
    "input/textarea.c",
    "input/select.c",
    "input/fuzzy.c",
    "input/datepicker.c",
    "input/history.c",
    "output/pager.c",
    "output/live.c",
    "app/paths.c",
    "app/error.c",
    "log/log.c",
    "args/args.c",
    "args/args_value.c",
    "args/args_suggest.c",
    "args/args_parse.c",
    "args/args_help.c",
    "args/args_complete.c",
    "args/args_split.c",
]

ffibuilder = FFI()

ffibuilder.cdef(
    r"""
/* ── libc ─────────────────────────────────────────────────────────────── */
void free(void *ptr);
FILE *fdopen(int fd, const char *mode);
int fflush(FILE *stream);
int fclose(FILE *stream);

/* ── Enums (verified against the headers) ─────────────────────────────── */
typedef enum { SC_TEXT_ATTR_NONE = 0, SC_TEXT_ATTR_BOLD = 1, SC_TEXT_ATTR_DIM = 2,
               SC_TEXT_ATTR_ITALIC = 4, SC_TEXT_ATTR_UNDER = 8 } ScTextAttribute;
typedef enum { SC_BORDER_NONE, SC_BORDER_ASCII, SC_BORDER_SINGLE,
               SC_BORDER_DOUBLE, SC_BORDER_ROUNDED, SC_BORDER_THICK } ScBorderType;
typedef enum { SC_POSITION_TOP, SC_POSITION_BOTTOM } ScPosition;
typedef enum { SC_ALIGN_LEFT, SC_ALIGN_CENTER, SC_ALIGN_RIGHT } ScHAlign;
typedef enum { SC_VALIGN_TOP, SC_VALIGN_MIDDLE, SC_VALIGN_BOTTOM } ScVAlign;
typedef enum { SC_ANSI_MODE_DEFAULT = 0, SC_ANSI_MODE_ALLOW = 1,
               SC_ANSI_MODE_SANITIZE = 2 } ScAnsiMode;
typedef enum { SC_CELL_STR, SC_CELL_TEXT, SC_CELL_MARKUP } ScCellKind;
typedef enum { SC_LIST_BULLET, SC_LIST_NUMBER, SC_LIST_ALPHA_LC, SC_LIST_ALPHA_UC,
               SC_LIST_ROMAN_LC, SC_LIST_ROMAN_UC } ScListMarker;
typedef enum { SC_PROGRESS_BLOCK, SC_PROGRESS_ASCII, SC_PROGRESS_LINE,
               SC_PROGRESS_SHADED } ScProgressType;
typedef enum { SC_SPINNER_BRAILLE, SC_SPINNER_PIPE, SC_SPINNER_DOTS,
               SC_SPINNER_ARROW } ScSpinnerType;
typedef enum { SC_ALERT_INFO, SC_ALERT_DEBUG, SC_ALERT_WARNING, SC_ALERT_ERROR,
               SC_ALERT_SUCCESS } ScAlertType;
typedef enum { SC_INPUT_OK = 0, SC_INPUT_CANCELLED, SC_INPUT_ERROR } ScInputStatus;
typedef enum { SC_HINT_LAYOUT_DEFAULT = 0, SC_HINT_INLINE, SC_HINT_STACKED,
               SC_HINT_HIDDEN } ScHintLayout;
typedef enum { SC_HINT_POS_DEFAULT = 0, SC_HINT_POS_TOP, SC_HINT_POS_BOTTOM,
               SC_HINT_POS_LEFT, SC_HINT_POS_RIGHT } ScHintPosition;
typedef enum { SC_WEEK_START_DEFAULT = 0, SC_WEEK_START_MONDAY = 1,
               SC_WEEK_START_SUNDAY = 2 } ScWeekStart;
typedef enum { SC_WIDTH_DEFAULT = 0, SC_WIDTH_CONTENT, SC_WIDTH_FIXED,
               SC_WIDTH_FULL } ScWidthMode;
typedef enum { SC_BG_EXTENT_WIDGET = 0, SC_BG_EXTENT_TEXT } ScBgExtent;
typedef enum { SC_SUGGEST_GHOST = 0, SC_SUGGEST_DROPDOWN } ScSuggestMode;
typedef enum { SC_SUGGEST_MATCH_PREFIX = 0,
               SC_SUGGEST_MATCH_FUZZY } ScSuggestMatch;
typedef enum { SC_SHORTCUT_RETURN = 0, SC_SHORTCUT_CALLBACK } ScShortcutMode;
typedef enum { SC_MOD_NONE = 0, SC_MOD_CTRL = 1, SC_MOD_ALT = 2,
               SC_MOD_PASTED = 4 } ScKeyMods;
/* ScKeyType: only the members the binding names; ... pulls in the rest. */
typedef enum { SC_KEY_NONE = 0, SC_KEY_CHAR, SC_KEY_ESC, SC_KEY_ENTER,
               SC_KEY_TAB, SC_KEY_UP, SC_KEY_DOWN, SC_KEY_LEFT, SC_KEY_RIGHT,
               SC_KEY_F1, ... } ScKeyType;

/* ── Core value types (fully declared: small and stable) ───────────────── */
typedef struct { int index; uint8_t r, g, b; } ScColor;
typedef struct { ScTextAttribute attr; ScColor fg; ScColor bg; } ScTextStyle;
typedef struct { int top; int right; int bottom; int left; } ScEdges;
typedef struct { ScBorderType type; ScColor color; ScColor bg; } ScBorderStyle;
typedef struct {
    bool enabled;
    ScBorderStyle border;
    ScColor bg;
    ScEdges padding;
    ScEdges margin;
    ScWidthMode width_mode;
    int width;
    int min_width;
    int max_width;
    ScBgExtent bg_extent;
} ScBoxStyle;

typedef struct ScText ScText;
typedef struct {
    const char *text;
    const ScText *rich_text;
    ScTextStyle style;
    ScHAlign halign;
    int pad;
    ScPosition pos;
    ...;
} ScTitle;

ScColor sc_color_from_rgb(uint8_t r, uint8_t g, uint8_t b);
ScColor sc_color_none(void);
void sc_print(const char *raw_str, ScTextStyle style);
void sc_println(const char *raw_str, ScTextStyle style);
void sc_version(int *major, int *minor, int *patch);
const char *sc_version_string(void);

/* ── ScText / ScSpan ───────────────────────────────────────────────────── */
typedef struct { char *raw_str; ScTextStyle style; } ScSpan;
ScText *sc_text_new(void);
ScText *sc_text_from_str(const char *str);
void sc_text_append(ScText *text, const char *raw_str, ScTextStyle style);
void sc_text_append_link(ScText *text, const char *raw_str, const char *url,
                         ScTextStyle style);
void sc_text_free(ScText *text);
void sc_print_text(const ScText *text);
size_t sc_text_visible_width(const ScText *text);
size_t sc_text_span_count(const ScText *text);
ScSpan sc_text_span(const ScText *text, size_t index);

/* ── Rendered / output stream ──────────────────────────────────────────── */
typedef struct {
    char **lines;
    int *column_widths;
    size_t line_count;
    int max_column_width;
} ScRendered;
void sc_rendered_free(ScRendered *r);
FILE *sc_output_stream(void);
void sc_output_set_stream(FILE *out);

/* ── Markup ────────────────────────────────────────────────────────────── */
typedef struct { bool strip_unknown; ScAnsiMode ansi; ScTextStyle code_style; ...; } ScMarkupOpts;
ScText *sc_markup_parse(const char *markup);
ScText *sc_markup_parse_opts(const char *markup, ScMarkupOpts opts);
void sc_markup_append(ScText *text, const char *markup);
void sc_markup_append_opts(ScText *text, const char *markup, ScMarkupOpts opts);
void sc_markup_print(const char *markup);
void sc_markup_print_opts(const char *markup, ScMarkupOpts opts);
void sc_markup_println(const char *markup);
void sc_markup_println_opts(const char *markup, ScMarkupOpts opts);

/* ── Panel ─────────────────────────────────────────────────────────────── */
typedef struct {
    ScBorderStyle border;
    ScColor bg;
    ScTitle title;
    ScTitle subtitle;
    bool full_width;
    int width;
    ScHAlign content_align;
    ScEdges padding;
    ScEdges margin;
    ScAnsiMode ansi;
    ...;
} ScPanelOpts;
void sc_panel_str(const char *content, ScPanelOpts opts);
void sc_panel_text(const ScText *content, ScPanelOpts opts);

/* ── Rule ──────────────────────────────────────────────────────────────── */
typedef struct {
    ScBorderType type;
    ScColor color;
    ScTitle title;
    int width;
    ScHAlign halign;
    ScEdges margin;
    ScAnsiMode ansi;
    ...;
} ScRuleOpts;
void sc_rule_str(const char *title, ScRuleOpts opts);
void sc_rule_text(const ScText *title, ScRuleOpts opts);

/* ── Badge ─────────────────────────────────────────────────────────────── */
typedef struct {
    const char *left_cap;
    const char *right_cap;
    ScTextStyle text_style;
    int pad;
    ScAnsiMode ansi;
    ...;
} ScBadgeOpts;
void sc_print_badge(const char *text, ScBadgeOpts opts);
void sc_text_append_badge(ScText *text_obj, const char *text, ScBadgeOpts opts);

/* ── Alert ─────────────────────────────────────────────────────────────── */
void sc_alert_str(ScAlertType type, const char *content);
void sc_alert_text(ScAlertType type, const ScText *content);
void sc_alert_info(const char *content);
void sc_alert_debug(const char *content);
void sc_alert_warning(const char *content);
void sc_alert_error(const char *content);
void sc_alert_success(const char *content);

/* ── Table ─────────────────────────────────────────────────────────────── */
typedef struct {
    ScCellKind kind;
    const char *str;
    ScText *text;
    bool halign_set;
    ScHAlign halign;
    bool valign_set;
    ScVAlign valign;
    int col_span;
    int row_span;
    ...;
} ScCell;
ScCell sc_cell_from_str(const char *str);
ScCell sc_cell_str_aligned(const char *str, ScHAlign halign, ScVAlign valign);
ScCell sc_cell_from_text(ScText *text);
ScCell sc_cell_text_aligned(ScText *text, ScHAlign halign, ScVAlign valign);
ScCell sc_cell_colspan(const char *str, int col_span);
ScCell sc_cell_rowspan(const char *str, int row_span);
ScCell sc_cell_skip_placeholder(void);
ScCell sc_row_skip_placeholder(void);
ScCell sc_cell_from_markup(const char *markup);

typedef struct {
    int min_width;
    int max_width;
    int fixed_width;
    ScHAlign halign;
    ScVAlign valign;
    bool word_wrap;
    ScColor bg;
    ScTextStyle style;
    ...;
} ScColOpts;
typedef struct {
    ScBorderType type;
    ScColor outer_color;
    ScColor inner_color;
    ScColor header_row_sep_color;
    ScColor header_col_sep_color;
    bool no_outer;
    bool no_inner_h;
    bool no_inner_v;
    ...;
} ScTableBorder;
typedef struct {
    bool row;
    bool col;
    ScColor row_bg;
    ScColor col_bg;
    ScTextStyle style;
    ...;
} ScTableHeader;
typedef struct {
    ScColor row_bg;
    ScColor col_bg;
    ScTextStyle style;
    ...;
} ScTableFooter;
typedef struct {
    ScTableBorder border;
    ScTableHeader header;
    bool striped;
    ScColor stripe_bg;
    ScTableFooter footer;
    ScTitle title;
    ScEdges cell_pad;
    ScEdges margin;
    int total_width;
    int max_rows;
    bool right_to_left;
    ScAnsiMode ansi;
    ...;
} ScTableOpts;
typedef struct ScTableData ScTableData;
ScTableData *sc_table_new(void);
void sc_table_add_column(ScTableData *table, const char *header, ScColOpts col_opts);
void sc_table_add_row(ScTableData *table, ScCell *cells, size_t cell_count);
void sc_table_add_row_bg(ScTableData *table, ScCell *cells, size_t cell_count, ScColor bg);
void sc_table_add_footer_row(ScTableData *table, ScCell *cells, size_t cell_count);
void sc_table_print(const ScTableData *table, ScTableOpts opts);
void sc_table_free(ScTableData *table);

/* ── List ──────────────────────────────────────────────────────────────── */
typedef struct {
    ScListMarker marker;
    const char *bullet;
    const char *marker_prefix;
    const char *marker_suffix;
    ScTextStyle marker_style;
    int indent;
    int item_gap;
    int width;
    ScEdges margin;
    ScAnsiMode ansi;
    ...;
} ScListOpts;
typedef struct ScList ScList;
typedef struct ScListItem ScListItem;
ScList *sc_list_new(ScListOpts opts);
ScListItem *sc_list_add_str(ScList *list, const char *str, ScTextStyle style);
ScListItem *sc_list_add_text(ScList *list, const ScText *text);
ScList *sc_list_add_sub(ScListItem *parent, ScListOpts opts);
void sc_list_print(const ScList *list);
void sc_list_free(ScList *list);

/* ── Tree ──────────────────────────────────────────────────────────────── */
typedef struct {
    ScBorderType type;
    ScColor connector_color;
    int indent;
    bool no_guide;
    ScAnsiMode ansi;
    ...;
} ScTreeOpts;
typedef struct ScTree ScTree;
typedef struct ScTreeNode ScTreeNode;
ScTree *sc_tree_new(ScTreeOpts opts);
ScTreeNode *sc_tree_add_str(ScTree *tree, ScTreeNode *parent, const char *str,
                            ScTextStyle style, const char *prefix,
                            ScTextStyle prefix_style);
ScTreeNode *sc_tree_add_text(ScTree *tree, ScTreeNode *parent, const ScText *text,
                             const char *prefix, ScTextStyle prefix_style);
void sc_tree_print(const ScTree *tree);
void sc_tree_free(ScTree *tree);

/* ── Key/Value ─────────────────────────────────────────────────────────── */
typedef struct {
    const char *sep;
    int key_width;
    int width;
    ScEdges margin;
    int item_gap;
    bool wrap_val;
    ScTextStyle key_style;
    ScTextStyle val_style;
    ScAnsiMode ansi;
    ...;
} ScKVOpts;
typedef struct ScKV ScKV;
ScKV *sc_kv_new(ScKVOpts opts);
void sc_kv_add(ScKV *kv, const char *key, const char *value);
void sc_kv_print(const ScKV *kv);
void sc_kv_free(ScKV *kv);

/* ── Columns ───────────────────────────────────────────────────────────── */
typedef struct {
    int min_w;
    int max_w;
    int fixed_w;
    ScHAlign halign;
    bool valign_set;
    ScVAlign valign;
    ScColor bg;
    bool stretch;
    ...;
} ScColItem;
typedef struct {
    int gap;
    ScBorderStyle sep;
    ScVAlign valign;
    int total_width;
    ScEdges margin;
    ...;
} ScColumnsOpts;
typedef struct ScColumns ScColumns;
ScColumns *sc_columns_new(ScColumnsOpts opts);
void sc_columns_add_table(ScColumns *columns, const ScTableData *table,
                          ScTableOpts opts, ScColItem item);
void sc_columns_add_panel_str(ScColumns *columns, const char *content,
                              ScPanelOpts opts, ScColItem item);
void sc_columns_add_panel_text(ScColumns *columns, const ScText *content,
                               ScPanelOpts opts, ScColItem item);
void sc_columns_add_text(ScColumns *columns, const ScText *text, ScColItem item);
void sc_columns_add_str(ScColumns *columns, const char *str, ScColItem item);
void sc_columns_add_columns(ScColumns *columns, const ScColumns *nested, ScColItem item);
void sc_columns_add_tree(ScColumns *columns, const ScTree *tree, ScColItem item);
void sc_columns_add_list(ScColumns *columns, const ScList *list, ScColItem item);
void sc_columns_add_rendered(ScColumns *columns, const ScRendered *rendered, ScColItem item);
void sc_columns_print(const ScColumns *columns);
void sc_columns_free(ScColumns *columns);

/* ── Capture / compose ─────────────────────────────────────────────────── */
ScRendered *sc_capture_str(const char *str);
ScRendered *sc_capture_text(const ScText *text);
ScRendered *sc_capture_table(const ScTableData *table, ScTableOpts opts);
ScRendered *sc_capture_list(const ScList *list);
ScRendered *sc_capture_tree(const ScTree *tree);
ScRendered *sc_capture_kv(const ScKV *kv);
ScRendered *sc_capture_columns(const ScColumns *columns);
ScRendered *sc_capture_panel_str(const char *content, ScPanelOpts opts);
ScRendered *sc_capture_panel_text(const ScText *content, ScPanelOpts opts);
ScRendered *sc_capture_rule_str(const char *title, ScRuleOpts opts);
ScRendered *sc_capture_rule_text(const ScText *title, ScRuleOpts opts);
ScRendered *sc_vstack(const ScRendered *const *parts, size_t n, int gap);

/* ── Pad / align ───────────────────────────────────────────────────────── */
typedef struct { int top; int right; int bottom; int left; } ScPadOpts;
void sc_pad_print(const ScRendered *rendered, ScPadOpts opts);
void sc_pad_str(const char *str, ScPadOpts opts);
void sc_pad_text(const ScText *text, ScPadOpts opts);
void sc_align_print(const ScRendered *rendered, ScHAlign halign, int width);
void sc_align_str(const char *str, ScHAlign halign, int width);
void sc_align_text(const ScText *text, ScHAlign halign, int width);

/* ── Progress / spinner ────────────────────────────────────────────────── */
typedef struct {
    bool enabled;
    double mid;
    double high;
    ScColor color_low;
    ScColor color_mid;
    ScColor color_high;
    ...;
} ScProgressThresholds;
typedef struct {
    ScProgressType type;
    const char *left_cap;
    const char *right_cap;
    ScColor fill_color;
    ScColor empty_color;
    ScProgressThresholds thresholds;
    bool show_percent;
    bool show_value;
    int bar_width;
    int width;
    int label_width;
    ScTextStyle label_style;
    ScAnsiMode ansi;
    ...;
} ScProgressBarOpts;
typedef struct ScProgressBar ScProgressBar;
ScProgressBar *sc_progressbar_new(ScProgressBarOpts opts);
void sc_progressbar_set_label(ScProgressBar *bar, const char *label);
void sc_progressbar_draw(ScProgressBar *bar, double value, double max);
void sc_progressbar_finish(ScProgressBar *bar, double value, double max);
void sc_progressbar_free(ScProgressBar *bar);

typedef struct {
    ScSpinnerType type;
    ScColor color;
    ScTextStyle label_style;
    ScAnsiMode ansi;
    ...;
} ScSpinnerOpts;
typedef struct ScSpinner ScSpinner;
ScSpinner *sc_spinner_new(const char *label, ScSpinnerOpts opts);
void sc_spinner_set_label(ScSpinner *spinner, const char *label);
void sc_spinner_tick(ScSpinner *spinner);
void sc_spinner_finish(ScSpinner *spinner, bool success, const char *label);
void sc_spinner_free(ScSpinner *spinner);

/* ── Utilities ─────────────────────────────────────────────────────────── */
void sc_set_allow_ansi(bool allow);
bool sc_allow_ansi(void);
char *sc_strip_ansi(const char *str);
char *sc_truncate(const char *str, int max_cols, const char *ellipsis);
void sc_clear_line(void);

/* ── App: XDG paths + pager ────────────────────────────────────────────── */
typedef enum {
    SC_PATH_CONFIG = 0, SC_PATH_DATA, SC_PATH_CACHE, SC_PATH_STATE
} ScPathKind;
char *sc_path(ScPathKind kind, const char *appname);
char *sc_path_config(const char *appname);
char *sc_path_data(const char *appname);
char *sc_path_cache(const char *appname);
char *sc_path_state(const char *appname);
char *sc_path_file(ScPathKind kind, const char *appname, const char *relative);

typedef struct ScPager ScPager;
typedef struct {
    const char *command;
    bool always;
    ...;
} ScPagerOpts;
ScPager *sc_pager_begin(ScPagerOpts opts);
int sc_pager_end(ScPager *pager);

/* ── Live display ──────────────────────────────────────────────────────── */
typedef struct ScLive ScLive;
typedef struct {
    bool alt_screen;
    bool show_cursor;
    bool transient;
    bool always;
    ...;
} ScLiveOpts;
ScLive *sc_live_begin(ScLiveOpts opts);
void sc_live_update(ScLive *live, const ScRendered *frame);
void sc_live_update_str(ScLive *live, const char *str);
void sc_live_end(ScLive *live);

typedef enum {
    SC_LOG_DEBUG = 0, SC_LOG_INFO, SC_LOG_WARN, SC_LOG_ERROR, SC_LOG_OFF
} ScLogLevel;
typedef struct {
    ScLogLevel level;
    bool hide_timestamps;
    bool show_source;
    bool markup;
    ScAnsiMode ansi;
    ...;
} ScLoggerOpts;
typedef struct ScLogger ScLogger;
ScLogger *sc_logger_new(ScLoggerOpts opts);
void sc_logger_add_terminal(ScLogger *logger, FILE *stream, ScLogLevel min_level);
bool sc_logger_add_file(ScLogger *logger, const char *path, ScLogLevel min_level);
void sc_logger_set_level(ScLogger *logger, ScLogLevel level);
void sc_logger_log(ScLogger *logger, ScLogLevel level, const char *file,
                   int line, const char *format, ...);
void sc_logger_free(ScLogger *logger);
void sc_log_set_level(ScLogLevel level);
ScLogLevel sc_log_level(void);
void sc_log_set_opts(ScLoggerOpts opts);
bool sc_log_add_file(const char *path, ScLogLevel min_level);
void sc_log_reset(void);
void sc_log_log(ScLogLevel level, const char *file, int line,
                const char *format, ...);

typedef struct ScError ScError;
ScError *sc_error_new(const char *message);
void sc_error_add_cause(ScError *error, const char *cause);
void sc_error_set_hint(ScError *error, const char *hint);
void sc_error_set_code(ScError *error, int exit_code);
int sc_error_code(const ScError *error);
void sc_error_print(const ScError *error);
void sc_error_print_stderr(const ScError *error);
void sc_error_free(ScError *error);

/* ── Input: shared key/shortcut machinery ──────────────────────────────── */
typedef struct {
    ScKeyType type;
    uint32_t codepoint;
    char bytes[8];
    uint8_t mods;
    ...;
} ScKey;
typedef struct {
    ScKeyType key;
    uint32_t codepoint;
    uint8_t mods;
    ...;
} ScKeyChord;
typedef struct {
    ScKeyChord chord;
    int id;
    ScShortcutMode mode;
    bool (*on_fire)(int id, void *user);
    void *user;
    const char *hint_label;
    ...;
} ScShortcut;
ScKeyChord sc_key_ctrl(char letter);
ScKeyChord sc_key_fn(int n);
ScKeyChord sc_key_alt(char letter);
ScKeyChord sc_key_special(ScKeyType key);
bool sc_key_chord_matches(ScKey key, ScKeyChord chord);
const ScShortcut *sc_shortcut_find(ScKey key, const ScShortcut *items, size_t n);
void sc_key_chord_name(ScKeyChord chord, char *buf, size_t cap);
size_t sc_key_decode(const char *buf, size_t len, ScKey *out);
bool sc_input_available(void);

typedef bool (*ScValidateFn)(const char *value, void *ctx, const char **err_out);
typedef bool (*ScCharFilter)(uint32_t codepoint, void *ctx);
bool sc_filter_digits(uint32_t codepoint, void *ctx);
bool sc_filter_decimal(uint32_t codepoint, void *ctx);
bool sc_filter_alpha(uint32_t codepoint, void *ctx);
bool sc_filter_alnum(uint32_t codepoint, void *ctx);
bool sc_filter_no_space(uint32_t codepoint, void *ctx);

/* ── Theme ─────────────────────────────────────────────────────────────── */
typedef struct {
    ScColor accent;
    ScBoxStyle box;
    ScTextStyle prompt_style;
    ScTextStyle selected_style;
    ScTextStyle cursor_style;
    ScTextStyle count_style;
    ScTextStyle summary_style;
    ScTextStyle error_style;
    ScTextStyle hint_style;
    const char *cursor_marker;
    const char *marker;
    const char *checkbox_on;
    const char *checkbox_off;
    ScHintLayout hint_layout;
    ScHintPosition hint_pos;
    ...;
} ScInputTheme;
void sc_input_set_theme(const ScInputTheme *theme);
ScInputTheme sc_input_theme(void);

/* ── Confirm ───────────────────────────────────────────────────────────── */
typedef struct {
    bool default_yes;
    const char *yes_label;
    const char *no_label;
    ScTextStyle prompt_style;
    ScColor accent;
    ScTextStyle selected_style;
    ScTextStyle unselected_style;
    ScBoxStyle box;
    ScTextStyle summary_style;
    bool hide_summary;
    const char *hint;
    ScHintLayout hint_layout;
    ScHintPosition hint_pos;
    ScTextStyle hint_style;
    const ScShortcut *shortcuts;
    size_t n_shortcuts;
    int *out_shortcut_id;
    const ScText *prompt_text;
    bool prompt_markup;
    ...;
} ScConfirmOpts;
ScInputStatus sc_confirm(const char *question, bool *out, ScConfirmOpts opts);

/* ── Input history ─────────────────────────────────────────────────────── */
typedef struct {
    size_t max_entries;
    const char *app;
    const char *file;
    bool no_auto_add;
    bool keep_duplicates;
    ...;
} ScHistoryOpts;
typedef struct ScHistory ScHistory;
ScHistory *sc_history_new(ScHistoryOpts opts);
void sc_history_add(ScHistory *history, const char *line);
size_t sc_history_count(const ScHistory *history);
const char *sc_history_get(const ScHistory *history, size_t index);
bool sc_history_save(const ScHistory *history);
bool sc_history_load(ScHistory *history);
void sc_history_free(ScHistory *history);

/* ── Text input ────────────────────────────────────────────────────────── */
typedef struct {
    ScSuggestMode mode;
    ScSuggestMatch match;
    int max_visible;
    ScBorderStyle border;
    ScTextStyle item_style;
    ScTextStyle selected_style;
    ScTextStyle more_style;
    const char *cursor_marker;
    const char *marker;
    ...;
} ScSuggestOpts;

typedef struct {
    const char *initial;
    const char *placeholder;
    ScTextStyle prompt_style;
    ScTextStyle value_style;
    ScTextStyle cursor_style;
    ScTextStyle error_style;
    ScTextStyle summary_style;
    bool hide_summary;
    int max_chars;
    bool hide_char_count;
    ScTextStyle count_style;
    ScBoxStyle box;
    const char *hint;
    ScHintLayout hint_layout;
    ScHintPosition hint_pos;
    ScTextStyle hint_style;
    ScCharFilter char_filter;
    void *char_filter_ctx;
    const char *const *suggestions;
    size_t n_suggestions;
    ScSuggestOpts suggest;
    ScValidateFn validate;
    void *validate_ctx;
    const ScShortcut *shortcuts;
    size_t n_shortcuts;
    int *out_shortcut_id;
    const ScText *prompt_text;
    bool prompt_markup;
    bool external_editor;
    const char *editor;
    ScKeyChord editor_key;
    ScHistory *history;
    bool no_history_add;
    ...;
} ScTextInputOpts;
ScInputStatus sc_text_input(const char *prompt, char **out, ScTextInputOpts opts);

typedef struct {
    const char *placeholder;
    const char *mask;
    ScTextStyle prompt_style;
    ScTextStyle cursor_style;
    ScTextStyle error_style;
    ScTextStyle summary_style;
    bool hide_summary;
    int max_chars;
    bool hide_char_count;
    ScTextStyle count_style;
    ScBoxStyle box;
    const char *hint;
    ScHintLayout hint_layout;
    ScHintPosition hint_pos;
    ScTextStyle hint_style;
    ScCharFilter char_filter;
    void *char_filter_ctx;
    ScValidateFn validate;
    void *validate_ctx;
    const ScShortcut *shortcuts;
    size_t n_shortcuts;
    int *out_shortcut_id;
    const ScText *prompt_text;
    bool prompt_markup;
    ...;
} ScPasswordOpts;
ScInputStatus sc_password_input(const char *prompt, char **out, ScPasswordOpts opts);

/* ── Number ────────────────────────────────────────────────────────────── */
typedef struct {
    double initial;
    bool start_empty;
    double min;
    double max;
    double step;
    int decimals;
    ScTextStyle prompt_style;
    ScTextStyle value_style;
    ScTextStyle cursor_style;
    ScTextStyle summary_style;
    bool hide_summary;
    const char *hint;
    ScHintLayout hint_layout;
    ScHintPosition hint_pos;
    ScTextStyle hint_style;
    ScBoxStyle box;
    const ScShortcut *shortcuts;
    size_t n_shortcuts;
    int *out_shortcut_id;
    const ScText *prompt_text;
    bool prompt_markup;
    char decimal_sep;
    char **out_text;
    bool calculator;
    bool calc_store_rounded;
    bool calc_show_precise;
    ScTextStyle error_style;
    const char *calc_warn_text;
    ScTextStyle calc_warn_style;
    ...;
} ScNumberOpts;
ScInputStatus sc_number_input(const char *prompt, double *out, ScNumberOpts opts);
bool sc_calc_eval(const char *expr, double *result);

/* ── Textarea ──────────────────────────────────────────────────────────── */
typedef struct {
    const char *initial;
    const char *placeholder;
    ScTextStyle prompt_style;
    ScTextStyle value_style;
    ScTextStyle cursor_style;
    ScTextStyle summary_style;
    bool hide_summary;
    const char *hint;
    ScHintLayout hint_layout;
    ScHintPosition hint_pos;
    ScTextStyle hint_style;
    ScBoxStyle box;
    const ScShortcut *shortcuts;
    size_t n_shortcuts;
    int *out_shortcut_id;
    const ScText *prompt_text;
    bool prompt_markup;
    bool external_editor;
    const char *editor;
    ScKeyChord editor_key;
    ...;
} ScTextareaOpts;
ScInputStatus sc_textarea(const char *prompt, char **out, ScTextareaOpts opts);

/* ── Select ────────────────────────────────────────────────────────────── */
typedef struct {
    const char *prompt;
    bool multi;
    bool no_cycle;
    int max_visible;
    ScTextStyle prompt_style;
    ScColor accent;
    ScTextStyle selected_style;
    const char *cursor_marker;
    const char *marker;
    const char *checkbox_on;
    const char *checkbox_off;
    ScBoxStyle box;
    ScTextStyle summary_style;
    bool hide_summary;
    const char *hint;
    ScHintLayout hint_layout;
    ScHintPosition hint_pos;
    ScTextStyle hint_style;
    const ScShortcut *shortcuts;
    size_t n_shortcuts;
    int *out_shortcut_id;
    const ScText *prompt_text;
    bool prompt_markup;
    ...;
} ScSelectOpts;
typedef struct ScSelect ScSelect;
ScSelect *sc_select_new(ScSelectOpts opts);
void sc_select_add(ScSelect *select, const char *label);
void sc_select_set_cursor(ScSelect *select, size_t index);
void sc_select_set_checked(ScSelect *select, size_t index, bool on);
ScInputStatus sc_select_run(ScSelect *select, size_t *indices, size_t *count_io);
size_t sc_select_cursor(const ScSelect *select);
const char *sc_select_label(const ScSelect *select, size_t index);
void sc_select_set_label(ScSelect *select, size_t index, const char *label);
void sc_select_remove(ScSelect *select, size_t index);
void sc_select_free(ScSelect *select);

/* ── Fuzzy ─────────────────────────────────────────────────────────────── */
typedef struct {
    const char *prompt;
    int max_visible;
    bool no_cycle;
    ScColor accent;
    bool table;
    const char *const *headers;
    size_t n_cols;
    uint64_t search_columns;
    ScTextStyle prompt_style;
    ScTextStyle selected_style;
    ScTextStyle cursor_style;
    ScTextStyle counter_style;
    const char *cursor_marker;
    const char *marker;
    ScBoxStyle box;
    ScTableOpts table_opts;
    ScTextStyle summary_style;
    bool hide_summary;
    const char *hint;
    ScHintLayout hint_layout;
    ScHintPosition hint_pos;
    ScTextStyle hint_style;
    const ScShortcut *shortcuts;
    size_t n_shortcuts;
    int *out_shortcut_id;
    const ScText *prompt_text;
    bool prompt_markup;
    ...;
} ScFuzzyOpts;
typedef struct ScFuzzy ScFuzzy;
bool sc_fuzzy_match(const char *pattern, const char *str, int *score);
ScFuzzy *sc_fuzzy_new(ScFuzzyOpts opts);
void sc_fuzzy_add(ScFuzzy *fuzzy, const char *label);
void sc_fuzzy_add_row(ScFuzzy *fuzzy, const char *const *fields, size_t n);
ScInputStatus sc_fuzzy_run(ScFuzzy *fuzzy, size_t *out_index);
size_t sc_fuzzy_cursor_index(const ScFuzzy *fuzzy);
bool sc_fuzzy_has_selection(const ScFuzzy *fuzzy);
void sc_fuzzy_remove(ScFuzzy *fuzzy, size_t index);
void sc_fuzzy_free(ScFuzzy *fuzzy);

/* ── Date picker ───────────────────────────────────────────────────────── */
struct tm {
    int tm_sec; int tm_min; int tm_hour; int tm_mday; int tm_mon;
    int tm_year; int tm_wday; int tm_yday; int tm_isdst; ...;
};
typedef struct {
    const char *prompt;
    ScWeekStart week_start;
    ScColor accent;
    ScTextStyle prompt_style;
    ScTextStyle header_style;
    ScTextStyle weekday_style;
    ScTextStyle selected_style;
    const char *header_prev;
    const char *header_next;
    ScBoxStyle box;
    ScTextStyle summary_style;
    bool hide_summary;
    const char *hint;
    ScHintLayout hint_layout;
    ScHintPosition hint_pos;
    ScTextStyle hint_style;
    const ScShortcut *shortcuts;
    size_t n_shortcuts;
    int *out_shortcut_id;
    const ScText *prompt_text;
    bool prompt_markup;
    ...;
} ScDatePickerOpts;
ScInputStatus sc_datepicker(struct tm *io, ScDatePickerOpts opts);
"""
)

ffibuilder.set_source(
    "sparcli._sparcli_cffi",
    r"""
    #include <stdlib.h>
    #include <sparcli.h>
    """,
    sources=[f"{_SRC}/{s}" for s in _SOURCES],
    include_dirs=[_INCLUDE, _SRC],
    # Stack canaries for the embedded C code (matches the Makefile hardening).
    extra_compile_args=['-std=c11', '-fstack-protector-strong'],
)


if __name__ == "__main__":
    # Build the extension and drop the compiled module next to the package
    # source (src/sparcli/_sparcli_cffi.*), so `PYTHONPATH=src` imports work
    # without an install. (`pip install` goes through setup.py's cffi_modules.)
    # We compile from this directory (where the csrc/cinclude symlinks
    # resolve), then copy the shared object into src/sparcli and clean up.
    import shutil

    built = ffibuilder.compile(verbose=True)
    dst_dir = os.path.join(_HERE, "src", "sparcli")
    shutil.copyfile(built, os.path.join(dst_dir, os.path.basename(built)))
    shutil.rmtree(os.path.join(_HERE, "sparcli"), ignore_errors=True)
    print("installed", os.path.basename(built),
          "->", os.path.relpath(dst_dir, _HERE))
