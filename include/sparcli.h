#ifndef SPARCLI_H
#define SPARCLI_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ── ANSI escape codes (internal use) ──────────────────────────────────── */
#define SC_RESET   "\033[0m"
#define SC_BOLD    "\033[1m"
#define SC_DIM     "\033[2m"
#define SC_ITALIC  "\033[3m"
#define SC_UNDER   "\033[4m"

/* ── ScStyle ────────────────────────────────────────────────────────────── */

/* Each flag occupies exactly one bit so styles can be combined with |:
   SC_STYLE_BOLD | SC_STYLE_ITALIC = 0b0001 | 0b0010 = 0b0011 (unambiguous) */
typedef enum {
    SC_STYLE_NONE   = 0,
    SC_STYLE_BOLD   = 1 << 0,
    SC_STYLE_DIM    = 1 << 1,
    SC_STYLE_ITALIC = 1 << 2,
    SC_STYLE_UNDER  = 1 << 3,
} ScStyle;

/* ── ScColor ────────────────────────────────────────────────────────────── */

/* Named ANSI color (0-7) or 24-bit RGB. index -2 = not set, -1 = RGB mode. */
typedef struct {
    int     index;
    uint8_t r, g, b;
} ScColor;

#define SC_COLOR_NONE    ((ScColor){ -2, 0, 0, 0 })
#define SC_COLOR_BLACK   ((ScColor){  0, 0, 0, 0 })
#define SC_COLOR_RED     ((ScColor){  1, 0, 0, 0 })
#define SC_COLOR_GREEN   ((ScColor){  2, 0, 0, 0 })
#define SC_COLOR_YELLOW  ((ScColor){  3, 0, 0, 0 })
#define SC_COLOR_BLUE    ((ScColor){  4, 0, 0, 0 })
#define SC_COLOR_MAGENTA ((ScColor){  5, 0, 0, 0 })
#define SC_COLOR_CYAN    ((ScColor){  6, 0, 0, 0 })
#define SC_COLOR_WHITE   ((ScColor){  7, 0, 0, 0 })

ScColor sc_rgb(uint8_t r, uint8_t g, uint8_t b);

/* ── ScOptions ──────────────────────────────────────────────────────────── */

typedef struct {
    ScStyle style;
    ScColor fg;
    ScColor bg;
} ScOptions;

void sc_print(const char *text, ScOptions opts);
void sc_println(const char *text, ScOptions opts);

/* ── ScText ─────────────────────────────────────────────────────────────── */

typedef struct {
    char     *text;
    ScOptions opts;
} ScSpan;

typedef struct {
    ScSpan *spans;
    size_t  count;
    size_t  cap;
} ScText;

ScText *sc_text_new(void);
void    sc_text_append(ScText *t, const char *text, ScOptions opts);
void    sc_text_free(ScText *t);
void    sc_print_text(const ScText *t);
size_t  sc_text_visible_width(const ScText *t);

/* ── Panels ─────────────────────────────────────────────────────────────── */

typedef enum {
    SC_BORDER_NONE,
    SC_BORDER_ASCII,
    SC_BORDER_SINGLE,
    SC_BORDER_DOUBLE,
    SC_BORDER_ROUNDED,
    SC_BORDER_THICK,
} ScBorderStyle;

typedef enum { SC_TITLE_TOP, SC_TITLE_BOTTOM } ScTitlePos;
typedef enum { SC_ALIGN_LEFT, SC_ALIGN_CENTER, SC_ALIGN_RIGHT } ScAlign;

typedef struct {
    ScBorderStyle border;
    ScColor       border_color;
    const char   *title;       /* NULL = kein Titel */
    ScOptions     title_opts;  /* empfohlen: SC_STYLE_BOLD */
    ScTitlePos    title_pos;
    ScAlign       title_align;
    int           pad_x;
    int           pad_y;
    int           width;         /* 0 = auto */
    int           title_pad;     /* spaces on each side of title text, default 1 */
    ScAlign       content_align; /* horizontal alignment of content lines */
    int           full_width;    /* 1 = stretch to terminal width */
} ScPanelOpts;

void sc_panel_str(const char *content, ScPanelOpts opts);
void sc_panel_text(const ScText *content, ScPanelOpts opts);

/* ── Tables ─────────────────────────────────────────────────────────────── */

typedef enum { SC_VALIGN_TOP, SC_VALIGN_MIDDLE, SC_VALIGN_BOTTOM } ScValign;

typedef enum { SC_CELL_STR, SC_CELL_TEXT } ScCellKind;

typedef struct {
    ScCellKind  kind;
    const char *str;         /* not owned; used when kind == SC_CELL_STR */
    ScText     *text;        /* not owned; used when kind == SC_CELL_TEXT */
    int         align_set;   /* 1 = overrides column alignment */
    ScAlign     align;
    int         valign_set;  /* 1 = overrides row valignment */
    ScValign    valign;
    int         colspan;     /* 0/1=normal, >1=spans N cols, -1=skip (covered by colspan) */
    int         rowspan;     /* 0/1=normal, >1=spans N rows, -1=skip (covered by rowspan) */
} ScCell;

#define SC_CELL(s)           ((ScCell){ SC_CELL_STR,  (s), NULL, 0, 0, 0, 0, 0,     0 })
#define SC_CELL_A(s,ha,va)   ((ScCell){ SC_CELL_STR,  (s), NULL, 1,(ha), 1,(va), 0, 0 })
#define SC_CELL_T(t)         ((ScCell){ SC_CELL_TEXT, NULL, (t), 0, 0, 0, 0, 0,     0 })
#define SC_CELL_TA(t,ha,va)  ((ScCell){ SC_CELL_TEXT, NULL, (t), 1,(ha), 1,(va), 0, 0 })
#define SC_CELL_CS(s,cs)     ((ScCell){ SC_CELL_STR,  (s), NULL, 0, 0, 0, 0, (cs),  0 })
#define SC_CELL_CSA(s,cs,ha) ((ScCell){ SC_CELL_STR,  (s), NULL, 1,(ha), 0, 0, (cs),  0 })
#define SC_CELL_TCS(t,cs)    ((ScCell){ SC_CELL_TEXT, NULL, (t), 0, 0, 0, 0, (cs),  0 })
#define SC_CELL_TCSA(t,cs,ha)((ScCell){ SC_CELL_TEXT, NULL, (t), 1,(ha), 0, 0, (cs),  0 })
#define SC_CELL_SKIP         ((ScCell){ SC_CELL_STR,  "",  NULL, 0, 0, 0, 0, -1,    0 })
#define SC_CELL_RS(s,rs)     ((ScCell){ SC_CELL_STR,  (s), NULL, 0, 0, 0, 0, 0,  (rs) })
#define SC_CELL_TRS(t,rs)    ((ScCell){ SC_CELL_TEXT, NULL,(t),  0, 0, 0, 0, 0,  (rs) })
#define SC_ROW_SKIP          ((ScCell){ SC_CELL_STR,  "",  NULL, 0, 0, 0, 0, 0,    -1 })

typedef struct {
    int      min_w;    /* minimum column width, 0 = none  */
    int      max_w;    /* maximum column width, 0 = none  */
    int      fixed_w;  /* fixed column width,   0 = auto  */
    ScAlign  align;    /* default horizontal alignment    */
    ScValign valign;   /* default vertical alignment      */
    int      wrap;     /* 1 = word-wrap instead of truncate */
    ScColor  bg;       /* per-column background, SC_COLOR_NONE = not set */
} ScColOpts;

typedef struct {
    ScBorderStyle style;
    ScColor       outer_color;
    ScColor       inner_color;
    ScColor       header_row_sep_color;
    ScColor       header_col_sep_color;
    int           no_outer;    /* 1 = suppress outer frame */
    int           no_inner_h;  /* 1 = suppress inner row separators */
    int           no_inner_v;  /* 1 = suppress inner col separators (except header col) */
} ScTableBorders;

typedef struct {
    ScTableBorders borders;
    int            header_row;       /* 1 = first added row is header */
    int            header_col;       /* 1 = first column is header    */
    ScColor        header_row_bg;
    ScColor        header_col_bg;
    ScOptions      header_opts;      /* style applied to header cells */
    int            striped;          /* 1 = alternating row bg colors */
    ScColor        stripe_bg;        /* bg for odd data rows (0-indexed) */
    ScColor        footer_row_bg;    /* bg for footer rows */
    ScColor        footer_col_bg;    /* bg for footer col (col 0 when header_col) */
    ScOptions      footer_opts;      /* style applied to footer cells */
    const char    *title;
    ScOptions      title_opts;
    ScTitlePos     title_pos;
    ScAlign        title_align;
    int            title_pad;        /* spaces around title text, default 1 */
    int            cell_pad_x;
    int            cell_pad_y;
    int            total_width;      /* 0 = auto; >0 = scale cols to fit */
    int            max_rows;         /* 0 = unlimited; >0 = limit + indicator */
    int            rtl;              /* 1 = right-to-left column order */
} ScTableOpts;

typedef struct ScTable ScTable;

ScTable *sc_table_new(ScTableOpts opts);
void     sc_table_add_col(ScTable *t, const char *header, ScColOpts col);
void     sc_table_add_row(ScTable *t, ScCell *cells, size_t n);
void     sc_table_add_row_bg(ScTable *t, ScCell *cells, size_t n, ScColor bg);
void     sc_table_add_footer_row(ScTable *t, ScCell *cells, size_t n);
void     sc_table_print(const ScTable *t);
void     sc_table_free(ScTable *t);

/* ── Tree ───────────────────────────────────────────────────────────────────── */

typedef struct {
    ScBorderStyle style;           /* connector characters */
    ScColor       connector_color; /* SC_COLOR_NONE = no color */
    int           indent;          /* extra spaces after connector, default 1 (→ 4 chars/level) */
    int           no_guide;        /* 1 = suppress vertical continuation lines */
} ScTreeOpts;

typedef struct ScTreeNode ScTreeNode;
typedef struct ScTree     ScTree;

ScTree     *sc_tree_new     (ScTreeOpts opts);
ScTreeNode *sc_tree_add_str (ScTree *t, ScTreeNode *parent,
                              const char   *str,    ScOptions opts,
                              const char   *prefix, ScOptions prefix_opts);
ScTreeNode *sc_tree_add_text(ScTree *t, ScTreeNode *parent,
                              const ScText *text,
                              const char   *prefix, ScOptions prefix_opts);
void        sc_tree_print   (const ScTree *t);
void        sc_tree_free    (ScTree *t);

/* ── Rule ───────────────────────────────────────────────────────────────────── */

typedef struct {
    ScBorderStyle style;       /* line character, default SC_BORDER_SINGLE */
    ScColor       color;       /* line color, SC_COLOR_NONE = no color     */
    ScOptions     title_opts;  /* style/color for the title text           */
    ScAlign       title_align; /* LEFT/CENTER/RIGHT placement of title     */
    int           title_pad;   /* spaces on each side of title, default 1 */
    int           width;       /* 0 = full terminal width                  */
    ScAlign       align;       /* placement of rule when width > 0         */
    int           margin;      /* symmetric left+right indent in chars     */
    int           pad_y;       /* blank lines above and below the rule     */
} ScRuleOpts;

void sc_rule_str (const char *title, ScRuleOpts opts);
void sc_rule_text(const ScText *title, ScRuleOpts opts);

/* ── Columns ────────────────────────────────────────────────────────────────── */

typedef struct {
    char  **lines;      /* heap-alloc strings, ANSI codes included */
    int    *vis_widths; /* visible character width per line */
    size_t  count;
    int     max_vis_w;  /* max vis_width across all lines */
} ScRendered;

void sc_rendered_free(ScRendered *r);

typedef struct {
    int     min_w;   /* 0 = none */
    int     max_w;   /* 0 = none */
    int     fixed_w; /* 0 = auto */
    ScAlign align;   /* placement within col when col_w > content_w */
} ScColItem;

typedef struct {
    int           gap;         /* spaces between columns, default 3 */
    ScBorderStyle sep_style;   /* SC_BORDER_NONE = no separator line */
    ScColor       sep_color;
    ScValign      valign;      /* vertical alignment when heights differ */
    int           total_width; /* 0 = auto; >0 = scale flex cols to fit */
} ScColumnsOpts;

typedef struct ScColumns ScColumns;

ScColumns *sc_columns_new(ScColumnsOpts opts);
void sc_columns_add_table     (ScColumns *cl, const ScTable   *t,       ScColItem item);
void sc_columns_add_panel_str (ScColumns *cl, const char      *content, ScPanelOpts opts, ScColItem item);
void sc_columns_add_panel_text(ScColumns *cl, const ScText    *content, ScPanelOpts opts, ScColItem item);
void sc_columns_add_text      (ScColumns *cl, const ScText    *t,       ScColItem item);
void sc_columns_add_str       (ScColumns *cl, const char      *s,       ScColItem item);
void sc_columns_add_columns   (ScColumns *cl, const ScColumns *nested,  ScColItem item);
void sc_columns_add_tree      (ScColumns *cl, const ScTree    *tree,    ScColItem item);
void sc_columns_print(const ScColumns *cl);
void sc_columns_free(ScColumns *cl);

#endif /* SPARCLI_H */
