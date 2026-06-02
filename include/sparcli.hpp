#pragma once
/**
 * @file sparcli.hpp
 * @brief Header-only C++20 wrapper around the sparcli C API.
 *
 * A thin, zero-overhead layer that adds:
 * - **RAII** owning handles (no leaks / double-free; move-only),
 * - **owned strings/texts** where the C API only borrows (no dangling),
 * - `std::optional` / `std::vector` returns for input prompts,
 * - a `.get()` escape hatch back to the C API.
 *
 * This header documents only what the wrapper adds on top of the C API
 * (ownership/lifetime, return semantics, exceptions); for the per-option and
 * per-function reference see the C headers (`@see sc_*`) or `docs/api-c.md`.
 * A readable C++ reference is in `docs/api-cpp.md`.
 *
 * Requires C++20 (designated initializers for the `*Opts` structs). The only
 * exception thrown is `std::bad_alloc`, from a constructor when the underlying
 * `sc_*_new` returns NULL; everything else is exception-free.
 *
 * Why it exists - two C-API footguns it removes:
 * @code
 * // C API: forgetting sc_table_free leaks; and the table BORROWS cell
 * // strings, so a temporary dangles and is read later at print time:
 * sc_table_add_row(t, (ScCell[]){
 *     sc_cell(std::to_string(n).c_str()) }, 1);   // dangling after this stmt
 *
 * // Wrapper: RAII frees, and the cell string is copied into a table-owned
 * // arena, so passing a temporary is safe:
 * Table t;
 * t.add_row({ std::to_string(n) });               // owned → no dangling
 * @endcode
 * (Verified by tests/cpp/test_cpp.cpp.)
 *
 * @code
 * #include <sparcli.hpp>
 * using namespace sparcli;
 * panel("Hi", { .border = { .type = SC_BORDER_ROUNDED } });
 * @endcode
 */
#include <sparcli.h>

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <new>

namespace sparcli {

// ── Value-type aliases (reuse the C structs/enums verbatim) ──────────────────
using Color           = ScColor;
using TextStyle       = ScTextStyle;
using TextAttribute   = ScTextAttribute;
using Title           = ScTitle;
using Edges           = ScEdges;
using BorderStyle     = ScBorderStyle;
using BorderType      = ScBorderType;
using HAlign          = ScHAlign;
using VAlign          = ScVAlign;
using Position        = ScPosition;
using AnsiMode        = ScAnsiMode;

using PanelOpts       = ScPanelOpts;
using TableOpts       = ScTableOpts;
using ColOpts         = ScColOpts;
using RuleOpts        = ScRuleOpts;
using ColumnsOpts     = ScColumnsOpts;
using ColItem         = ScColItem;
using ListOpts        = ScListOpts;
using TreeOpts        = ScTreeOpts;
using KVOpts          = ScKVOpts;
using BadgeOpts       = ScBadgeOpts;
using ProgressBarOpts = ScProgressBarOpts;
using SpinnerOpts     = ScSpinnerOpts;
using PadOpts         = ScPadOpts;
using MarkupOpts      = ScMarkupOpts;

using AlertType       = ScAlertType;
using InputStatus     = ScInputStatus;
using HintLayout      = ScHintLayout;
using HintPosition    = ScHintPosition;
using ConfirmOpts     = ScConfirmOpts;
using TextInputOpts   = ScTextInputOpts;
using PasswordOpts    = ScPasswordOpts;
using NumberOpts      = ScNumberOpts;
using TextareaOpts    = ScTextareaOpts;
using SelectOpts      = ScSelectOpts;
using FuzzyOpts       = ScFuzzyOpts;
using DatePickerOpts  = ScDatePickerOpts;
using InputTheme      = ScInputTheme;

// ── Color helpers (avoid the C SC_ANSI_COLOR_* compound-literal macros, which
//    are non-standard in C++; the functional accessors below are equivalent) ──
inline Color none()    { return sc_color_none();    }
inline Color black()   { return sc_color_black();   }
inline Color red()     { return sc_color_red();     }
inline Color green()   { return sc_color_green();   }
inline Color yellow()  { return sc_color_yellow();  }
inline Color blue()    { return sc_color_blue();    }
inline Color magenta() { return sc_color_magenta(); }
inline Color cyan()    { return sc_color_cyan();    }
inline Color white()   { return sc_color_white();   }
inline Color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    return sc_color_from_rgb(r, g, b);
}
inline TextStyle style(TextAttribute attr, Color fg = {}, Color bg = {}) {
    return sc_text_style(attr, fg, bg);
}

namespace detail {
// A NUL-terminated copy of a string_view, kept alive for a full expression.
// (string_view is not guaranteed NUL-terminated, so we materialize.)
inline std::string z(std::string_view s) { return std::string(s); }

// Debug guard against using a moved-from handle: asserts the wrapped pointer is
// non-null, then returns it. Wraps each C-call site so misuse aborts clearly in
// debug builds; compiles to nothing under -DNDEBUG. (Destructors and move
// assignment must still tolerate nullptr, so they do NOT use this.)
template <class T>
inline T* live(T* p) {
    assert(p && "use of a moved-from sparcli handle");
    return p;
}
}  // namespace detail

/**
 * Owning, move-only multi-span rich-text object (wraps `ScText`).
 *
 * The destructor frees the underlying text. All `append*` calls copy their
 * input, so no caller string needs to outlive the call. @see sc_text_new,
 * sc_text_append, sc_markup_parse.
 */
class Text {
public:
    /** Allocates an empty text. @throws std::bad_alloc on OOM. */
    Text() : p_(sc_text_new()) { require(); }
    /** Allocates a text from `s` (one unstyled span). @throws std::bad_alloc */
    explicit Text(std::string_view s)
        : p_(sc_text_from_str(detail::z(s).c_str())) { require(); }

    /** Parses inline markup into a Text. @throws std::bad_alloc */
    static Text markup(std::string_view m) {
        return Text(sc_markup_parse(detail::z(m).c_str()));
    }
    /** Markup overload with parser options. @see ScMarkupOpts */
    static Text markup(std::string_view m, MarkupOpts opts) {
        return Text(sc_markup_parse_opts(detail::z(m).c_str(), opts));
    }

    ~Text() { sc_text_free(p_); }
    Text(Text&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    Text& operator=(Text&& o) noexcept {
        if (this != &o) { sc_text_free(p_); p_ = o.p_; o.p_ = nullptr; }
        return *this;
    }
    Text(const Text&) = delete;
    Text& operator=(const Text&) = delete;

    /** Appends a styled span (string copied). @see sc_text_append */
    Text& append(std::string_view s, TextStyle st = {}) {
        sc_text_append(detail::live(p_), detail::z(s).c_str(), st);
        return *this;
    }
    /** Appends a span wrapped in an OSC-8 terminal hyperlink (clickable in
     *  supporting terminals; plain text elsewhere). @see sc_text_append_link */
    Text& append_link(std::string_view s, std::string_view url,
                      TextStyle st = {}) {
        sc_text_append_link(detail::live(p_), detail::z(s).c_str(),
                            detail::z(url).c_str(), st);
        return *this;
    }
    /** Appends parsed markup. @see sc_markup_append */
    Text& append_markup(std::string_view m) {
        sc_markup_append(detail::live(p_), detail::z(m).c_str());
        return *this;
    }
    /** Markup append with parser options. @see sc_markup_append_opts */
    Text& append_markup(std::string_view m, MarkupOpts opts) {
        sc_markup_append_opts(detail::live(p_), detail::z(m).c_str(), opts);
        return *this;
    }
    /** Appends a styled badge token. @see sc_text_append_badge */
    Text& append_badge(std::string_view s, BadgeOpts opts = {}) {
        sc_text_append_badge(detail::live(p_), detail::z(s).c_str(), opts);
        return *this;
    }

    /** Prints to the current output stream. @see sc_print_text */
    void        print()          const { sc_print_text(detail::live(p_)); }
    /** Max visible column width across lines. @see sc_text_visible_width */
    std::size_t visible_width()  const {
        return sc_text_visible_width(detail::live(p_));
    }
    /** Number of spans. @see sc_text_span_count */
    std::size_t span_count()     const {
        return sc_text_span_count(detail::live(p_));
    }

    /** Borrowed underlying `ScText*` (escape hatch to the C API); not owned. */
    ScText*       get()       { return p_; }
    const ScText* get() const { return p_; }

private:
    explicit Text(ScText* adopted) : p_(adopted) { require(); }
    void require() const { if (!p_) throw std::bad_alloc(); }
    ScText* p_;
};

/**
 * Owning, move-only captured rendering of a widget (wraps `ScRendered`).
 *
 * Produced by the `capture::*` helpers and `vstack`; feed it to `pad`/`align`
 * or `Columns::add`. @see sc_capture_str, sc_rendered_free.
 */
class Rendered {
public:
    ~Rendered() { sc_rendered_free(p_); }
    Rendered(Rendered&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    Rendered& operator=(Rendered&& o) noexcept {
        if (this != &o) { sc_rendered_free(p_); p_ = o.p_; o.p_ = nullptr; }
        return *this;
    }
    Rendered(const Rendered&) = delete;
    Rendered& operator=(const Rendered&) = delete;

    /** Prints with padding. @see sc_pad_print */
    void pad(PadOpts opts) const { sc_pad_print(detail::live(p_), opts); }
    /** Prints aligned within `w` columns. @see sc_align_print */
    void align(HAlign a, int w = 0) const {
        sc_align_print(detail::live(p_), a, w);
    }

    /** Borrowed underlying `ScRendered*` (escape hatch); not owned. */
    ScRendered*       get()       { return p_; }
    const ScRendered* get() const { return p_; }

    /** Adopts ownership of a raw `ScRendered*` (e.g. from `sc_capture_*`). */
    static Rendered adopt(ScRendered* r) { return Rendered(r); }

private:
    explicit Rendered(ScRendered* adopted) : p_(adopted) {
        if (!p_) throw std::bad_alloc();
    }
    ScRendered* p_;
};

/**
 * Owning, move-only table (wraps `ScTableData`).
 *
 * The C table only *borrows* cell strings, so this wrapper copies every cell
 * string into an internal arena that lives as long as the table - passing
 * `std::string` temporaries to `add_row` is therefore safe. Markup cells are
 * owned by the C table itself. @see sc_table_new, sc_table_add_row.
 */
class Table {
public:
    /**
     * A single table cell. Implicitly constructible from any string-like, so
     * `t.add_row({ "a", "b" })` just works; chain `.align()/.colspan()/…` for
     * options, or use the `cell` / `cell_markup` free helpers.
     */
    class Cell {
    public:
        Cell(const char* s)        : str_(s ? s : "") {}  // tolerate nullptr
        Cell(std::string s)        : str_(std::move(s)) {}
        Cell(std::string_view s)   : str_(s) {}

        /** Override horizontal alignment for this cell. */
        Cell& align(HAlign h) {
            halign_set_ = true; halign_ = h; return *this;
        }
        /** Override vertical alignment for this cell. */
        Cell& valign(VAlign v) {
            valign_set_ = true; valign_ = v; return *this;
        }
        /** Span `n` columns (fill the covered positions with skip cells). */
        Cell& colspan(int n)   { col_span_ = n; return *this; }
        /** Span `n` rows. */
        Cell& rowspan(int n)   { row_span_ = n; return *this; }

        /** Markup cell (prefer the `cell_markup` free helper). */
        static Cell from_markup(std::string m) {
            Cell c(std::move(m)); c.kind_ = Kind::Markup; return c;
        }

    private:
        friend class Table;
        enum class Kind { Str, Markup };
        Kind        kind_ = Kind::Str;
        std::string str_;
        bool        halign_set_ = false; HAlign halign_{};
        bool        valign_set_ = false; VAlign valign_{};
        int         col_span_ = 0;
        int         row_span_ = 0;
    };

    /** Allocates an empty table. @throws std::bad_alloc on OOM. */
    Table()
        : p_(sc_table_new()),
          strings_(std::make_unique<std::deque<std::string>>()) {
        if (!p_) throw std::bad_alloc();
    }
    ~Table() { sc_table_free(p_); }
    Table(Table&& o) noexcept
        : p_(o.p_), strings_(std::move(o.strings_)) { o.p_ = nullptr; }
    Table& operator=(Table&& o) noexcept {
        if (this != &o) {
            sc_table_free(p_);
            p_ = o.p_; strings_ = std::move(o.strings_); o.p_ = nullptr;
        }
        return *this;
    }
    Table(const Table&) = delete;
    Table& operator=(const Table&) = delete;

    /** Adds a column. The C side copies `header`. @see sc_table_add_column */
    Table& add_column(std::string_view header, ColOpts opts = {}) {
        // The C side copies the header, so a transient c_str() is fine here;
        // only cell strings (borrowed until print) go through the arena.
        sc_table_add_column(detail::live(p_), detail::z(header).c_str(), opts);
        return *this;
    }
    /** Adds a data row; cell strings are copied into the table-owned arena. */
    Table& add_row(std::vector<Cell> cells) {
        emit(cells, Row::Data);
        return *this;
    }
    /** Adds a data row with a background color. @see sc_table_add_row_bg */
    Table& add_row(std::vector<Cell> cells, Color bg) {
        auto sc = build(cells);
        sc_table_add_row_bg(detail::live(p_), sc.data(), sc.size(), bg);
        return *this;
    }
    /** Adds a footer row. @see sc_table_add_footer_row */
    Table& add_footer_row(std::vector<Cell> cells) {
        emit(cells, Row::Footer);
        return *this;
    }

    /** Renders the table. @see sc_table_print, ScTableOpts */
    void print(TableOpts opts = {}) const {
        sc_table_print(detail::live(p_), opts);
    }

    /** Borrowed underlying `ScTableData*` (escape hatch); not owned. */
    ScTableData*       get()       { return p_; }
    const ScTableData* get() const { return p_; }

private:
    enum class Row { Data, Footer };

    const char* own(std::string_view s) {
        // The deque lives behind a unique_ptr, so its node addresses are stable
        // even across a Table move - the borrowed c_str() pointers stay valid.
        strings_->emplace_back(s);
        return strings_->back().c_str();
    }
    std::vector<ScCell> build(std::vector<Cell>& cells) {
        std::vector<ScCell> out;
        out.reserve(cells.size());
        for (auto& c : cells) {
            ScCell sc = (c.kind_ == Cell::Kind::Markup)
                ? sc_cell_from_markup(c.str_.c_str())  // table owns the ScText
                : sc_cell_from_str(own(c.str_));  // owned by the wrapper
            sc.halign_set = c.halign_set_; sc.halign = c.halign_;
            sc.valign_set = c.valign_set_; sc.valign = c.valign_;
            sc.col_span   = c.col_span_;   sc.row_span = c.row_span_;
            out.push_back(sc);
        }
        return out;
    }
    void emit(std::vector<Cell>& cells, Row which) {
        auto sc = build(cells);
        if (which == Row::Footer) {
            sc_table_add_footer_row(detail::live(p_), sc.data(), sc.size());
        } else {
            sc_table_add_row(detail::live(p_), sc.data(), sc.size());
        }
    }

    ScTableData* p_;
    // Owns every borrowed cell/header string. Behind a unique_ptr so the deque
    // object never relocates → stored c_str() pointers survive a Table move.
    std::unique_ptr<std::deque<std::string>> strings_;
};

/** A plain-string table cell (chain `.align()/.colspan()/…` for options). */
inline Table::Cell cell(std::string_view s)        { return Table::Cell(s); }
/** A table cell whose content is parsed as inline markup. */
inline Table::Cell cell_markup(std::string_view s)  {
    return Table::Cell::from_markup(std::string(s));
}

/**
 * Owning, move-only bulleted/numbered list (wraps `ScList`).
 *
 * String items are copied by the C side. The C list *borrows* any `ScText`, so
 * the wrapper keeps rich-text items alive in a **shared text arena** that lives
 * as long as the root list - sub-lists share that arena, so a rich item added
 * to a sub-list stays valid until the whole list is destroyed.
 * @see sc_list_new, sc_list_add_str, sc_list_add_text, sc_list_add_sub.
 */
class List {
public:
    /** Handle to an added item; non-owning. Use `sub()` to nest a list. */
    class Item {
    public:
        /** Attaches a sub-list (owned by the parent; shares its text arena). */
        List sub(ListOpts opts = {}) {
            return List(sc_list_add_sub(detail::live(p_), opts),
                        /*owns=*/false, arena_);
        }
        /** Borrowed underlying `ScListItem*` (escape hatch); not owned. */
        ScListItem* get() const { return p_; }
    private:
        friend class List;
        using Arena = std::shared_ptr<std::deque<Text>>;
        Item(ScListItem* p, Arena a) : p_(p), arena_(std::move(a)) {}
        ScListItem* p_;
        Arena       arena_;
    };

    /** Allocates an empty list. @throws std::bad_alloc on OOM. */
    explicit List(ListOpts opts = {})
        : p_(sc_list_new(opts)), owns_(true),
          arena_(std::make_shared<std::deque<Text>>()) {
        if (!p_) throw std::bad_alloc();
    }
    ~List() { if (owns_) sc_list_free(p_); }
    List(List&& o) noexcept
        : p_(o.p_), owns_(o.owns_), arena_(std::move(o.arena_)) {
        o.p_ = nullptr; o.owns_ = false;
    }
    List& operator=(List&& o) noexcept {
        if (this != &o) {
            if (owns_) sc_list_free(p_);
            p_ = o.p_; owns_ = o.owns_; arena_ = std::move(o.arena_);
            o.p_ = nullptr; o.owns_ = false;
        }
        return *this;
    }
    List(const List&) = delete;
    List& operator=(const List&) = delete;

    /** Adds a string item (copied by the C side). @see sc_list_add_str */
    Item add(std::string_view s, TextStyle st = {}) {
        return Item(sc_list_add_str(detail::live(p_), detail::z(s).c_str(), st),
                    arena_);
    }
    /** Adds a rich item; the list takes ownership of `t`. */
    Item add(Text t) {
        arena_->push_back(std::move(t));
        return Item(sc_list_add_text(detail::live(p_), arena_->back().get()),
                    arena_);
    }
    /** Adds a rich item from markup (owned by the list). */
    Item add_markup(std::string_view m) { return add(Text::markup(m)); }

    /** Renders the list. @see sc_list_print */
    void print() const { sc_list_print(detail::live(p_)); }

    /** Borrowed underlying `ScList*` (escape hatch); not owned. */
    ScList*       get()       { return p_; }
    const ScList* get() const { return p_; }

private:
    List(ScList* p, bool owns, Item::Arena arena)
        : p_(p), owns_(owns), arena_(std::move(arena)) {
        if (!p_) throw std::bad_alloc();
    }
    ScList*      p_;
    bool         owns_;
    Item::Arena  arena_;
};

/**
 * Owning, move-only tree view (wraps `ScTree`).
 *
 * String nodes are copied by the C side. The C tree *borrows* any `ScText`, so
 * the wrapper owns rich-text nodes in an internal arena (the stored C text
 * pointer stays valid across a `Tree` move). Nodes are returned as non-owning
 * handles. @see sc_tree_new, sc_tree_add_str, sc_tree_add_text.
 */
class Tree {
public:
    /** Non-owning handle to a node; pass as the `parent` of a child. */
    class Node {
    public:
        Node() : p_(nullptr) {}
        explicit Node(ScTreeNode* p) : p_(p) {}
        ScTreeNode* get() const { return p_; }
    private:
        ScTreeNode* p_;
    };

    /** Allocates an empty tree. @throws std::bad_alloc on OOM. */
    explicit Tree(TreeOpts opts = {}) : p_(sc_tree_new(opts)) {
        if (!p_) throw std::bad_alloc();
    }
    ~Tree() { sc_tree_free(p_); }
    Tree(Tree&& o) noexcept : p_(o.p_), texts_(std::move(o.texts_)) {
        o.p_ = nullptr;
    }
    Tree& operator=(Tree&& o) noexcept {
        if (this != &o) {
            sc_tree_free(p_);
            p_ = o.p_; texts_ = std::move(o.texts_); o.p_ = nullptr;
        }
        return *this;
    }
    Tree(const Tree&) = delete;
    Tree& operator=(const Tree&) = delete;

    /**
     * Adds a string node under `parent` (a default `Node{}` adds a root).
     * String and prefix are copied by the C side. @see sc_tree_add_str
     */
    Node add(std::string_view s, Node parent = {}, TextStyle st = {},
             std::string_view prefix = {}, TextStyle prefix_st = {}) {
        return Node(sc_tree_add_str(
            detail::live(p_), parent.get(), detail::z(s).c_str(), st,
            prefix.empty() ? nullptr : detail::z(prefix).c_str(), prefix_st));
    }
    /** Adds a rich node; the tree owns `t`. @see sc_tree_add_text */
    Node add(Text t, Node parent = {},
             std::string_view prefix = {}, TextStyle prefix_st = {}) {
        texts_.push_back(std::move(t));
        return Node(sc_tree_add_text(
            detail::live(p_), parent.get(), texts_.back().get(),
            prefix.empty() ? nullptr : detail::z(prefix).c_str(), prefix_st));
    }
    /** Adds a rich node from markup (owned by the tree). */
    Node add_markup(std::string_view m, Node parent = {},
                    std::string_view prefix = {}, TextStyle prefix_st = {}) {
        return add(Text::markup(m), parent, prefix, prefix_st);
    }
    /** Renders the tree. @see sc_tree_print */
    void print() const { sc_tree_print(detail::live(p_)); }

    /** Borrowed underlying `ScTree*` (escape hatch); not owned. */
    ScTree*       get()       { return p_; }
    const ScTree* get() const { return p_; }

private:
    ScTree*               p_;
    std::deque<Text>      texts_;   // owns ScText borrowed by the C tree
};

/**
 * Owning, move-only key/value list (wraps `ScKV`). The C side copies both key
 * and value, so no lifetime management is needed. @see sc_kv_new, sc_kv_add.
 */
class Kv {
public:
    /** Allocates an empty key/value list. @throws std::bad_alloc */
    explicit Kv(KVOpts opts = {}) : p_(sc_kv_new(opts)) {
        if (!p_) throw std::bad_alloc();
    }
    ~Kv() { sc_kv_free(p_); }
    Kv(Kv&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    Kv& operator=(Kv&& o) noexcept {
        if (this != &o) { sc_kv_free(p_); p_ = o.p_; o.p_ = nullptr; }
        return *this;
    }
    Kv(const Kv&) = delete;
    Kv& operator=(const Kv&) = delete;

    /** Adds a key/value pair (both copied). @see sc_kv_add */
    Kv& add(std::string_view key, std::string_view value) {
        sc_kv_add(detail::live(p_), detail::z(key).c_str(),
                  detail::z(value).c_str());
        return *this;
    }
    /** Renders the list. @see sc_kv_print */
    void print() const { sc_kv_print(detail::live(p_)); }

    /** Borrowed underlying `ScKV*` (escape hatch); not owned. */
    ScKV*       get()       { return p_; }
    const ScKV* get() const { return p_; }

private:
    ScKV* p_;
};

/**
 * Owning, move-only side-by-side layout (wraps `ScColumns`).
 *
 * Each `add*` call captures the widget's rendering **eagerly**, so the source
 * widget need not outlive the columns and may be modified or destroyed after.
 * @see sc_columns_new, sc_columns_add_table.
 */
class Columns {
public:
    /** Allocates an empty columns layout. @throws std::bad_alloc */
    explicit Columns(ColumnsOpts opts = {}) : p_(sc_columns_new(opts)) {
        if (!p_) throw std::bad_alloc();
    }
    ~Columns() { sc_columns_free(p_); }
    Columns(Columns&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    Columns& operator=(Columns&& o) noexcept {
        if (this != &o) { sc_columns_free(p_); p_ = o.p_; o.p_ = nullptr; }
        return *this;
    }
    Columns(const Columns&) = delete;
    Columns& operator=(const Columns&) = delete;

    /// @name Add the next column (the widget's rendering is captured eagerly).
    /// @{
    Columns& add(const Table& t, TableOpts o = {}, ColItem it = {}) {
        sc_columns_add_table(detail::live(p_), t.get(), o, it); return *this;
    }
    Columns& add_panel(std::string_view content, PanelOpts o = {},
                       ColItem it = {}) {
        sc_columns_add_panel_str(detail::live(p_), detail::z(content).c_str(),
                                 o, it);
        return *this;
    }
    Columns& add_panel(const Text& content, PanelOpts o = {}, ColItem it = {}) {
        sc_columns_add_panel_text(detail::live(p_), content.get(), o, it);
        return *this;
    }
    Columns& add(const Text& t, ColItem it = {}) {
        sc_columns_add_text(detail::live(p_), t.get(), it); return *this;
    }
    Columns& add(std::string_view s, ColItem it = {}) {
        sc_columns_add_str(detail::live(p_), detail::z(s).c_str(), it);
        return *this;
    }
    Columns& add(const List& l, ColItem it = {}) {
        sc_columns_add_list(detail::live(p_), l.get(), it); return *this;
    }
    Columns& add(const Tree& t, ColItem it = {}) {
        sc_columns_add_tree(detail::live(p_), t.get(), it); return *this;
    }
    Columns& add(const Columns& nested, ColItem it = {}) {
        sc_columns_add_columns(detail::live(p_), nested.get(), it);
        return *this;
    }
    Columns& add(const Rendered& r, ColItem it = {}) {
        sc_columns_add_rendered(detail::live(p_), r.get(), it); return *this;
    }
    /// @}

    /** Renders the columns. @see sc_columns_print */
    void print() const { sc_columns_print(detail::live(p_)); }

    /** Borrowed underlying `ScColumns*` (escape hatch); not owned. */
    ScColumns*       get()       { return p_; }
    const ScColumns* get() const { return p_; }

private:
    ScColumns* p_;
};

/**
 * Owning, move-only animated progress bar (wraps `ScProgressBar`). Allocate
 * once, call `draw` in a loop, `finish` at the end. @see sc_progressbar_new.
 */
class ProgressBar {
public:
    /** Allocates a progress bar. @throws std::bad_alloc */
    explicit ProgressBar(ProgressBarOpts opts = {})
        : p_(sc_progressbar_new(opts)) {
        if (!p_) throw std::bad_alloc();
    }
    ~ProgressBar() { sc_progressbar_free(p_); }
    ProgressBar(ProgressBar&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    ProgressBar& operator=(ProgressBar&& o) noexcept {
        if (this != &o) { sc_progressbar_free(p_); p_ = o.p_; o.p_ = nullptr; }
        return *this;
    }
    ProgressBar(const ProgressBar&) = delete;
    ProgressBar& operator=(const ProgressBar&) = delete;

    /** Sets the label (copied). @see sc_progressbar_set_label */
    void set_label(std::string_view l) {
        sc_progressbar_set_label(detail::live(p_), detail::z(l).c_str());
    }
    /** Redraws in place. `max == 0` → `value` is a 0..1 ratio. */
    void draw(double value, double max = 0.0) {
        sc_progressbar_draw(detail::live(p_), value, max);
    }
    /** Finalizes the bar (ends the line). @see sc_progressbar_finish */
    void finish(double value, double max = 0.0) {
        sc_progressbar_finish(detail::live(p_), value, max);
    }

    /** Borrowed underlying `ScProgressBar*` (escape hatch); not owned. */
    ScProgressBar* get() { return p_; }

private:
    ScProgressBar* p_;
};

/**
 * Owning, move-only animated spinner (wraps `ScSpinner`). Call `tick` in a
 * loop, `finish` to end with a success/failure mark. @see sc_spinner_new.
 */
class Spinner {
public:
    /** Allocates a spinner with an initial label. @throws std::bad_alloc */
    explicit Spinner(std::string_view label, SpinnerOpts opts = {})
        : p_(sc_spinner_new(detail::z(label).c_str(), opts)) {
        if (!p_) throw std::bad_alloc();
    }
    ~Spinner() { sc_spinner_free(p_); }
    Spinner(Spinner&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    Spinner& operator=(Spinner&& o) noexcept {
        if (this != &o) { sc_spinner_free(p_); p_ = o.p_; o.p_ = nullptr; }
        return *this;
    }
    Spinner(const Spinner&) = delete;
    Spinner& operator=(const Spinner&) = delete;

    /** Updates the label mid-animation (copied). @see sc_spinner_set_label */
    void set_label(std::string_view l) {
        sc_spinner_set_label(detail::live(p_), detail::z(l).c_str());
    }
    /** Advances to the next frame. @see sc_spinner_tick */
    void tick() { sc_spinner_tick(detail::live(p_)); }
    /** Ends with ✔/✖ and a final label. @see sc_spinner_finish */
    void finish(bool success, std::string_view label) {
        sc_spinner_finish(detail::live(p_), success, detail::z(label).c_str());
    }

    /** Borrowed underlying `ScSpinner*` (escape hatch); not owned. */
    ScSpinner* get() { return p_; }

private:
    ScSpinner* p_;
};

// ── Stateless output free functions ──────────────────────────────────────────
// Thin wrappers over the C renderers; each takes a string_view (or Text) plus
// the C `*Opts` struct (use C++20 designated initializers). @see sc_print etc.

/** Prints styled text. @see sc_print */
inline void print(std::string_view s, TextStyle st = {}) {
    sc_print(detail::z(s).c_str(), st);
}
/** Prints styled text + newline. @see sc_println */
inline void println(std::string_view s, TextStyle st = {}) {
    sc_println(detail::z(s).c_str(), st);
}

/** Renders a bordered panel. @see sc_panel_str, ScPanelOpts */
inline void panel(std::string_view content, PanelOpts opts = {}) {
    sc_panel_str(detail::z(content).c_str(), opts);
}
/** Panel from rich text. @see sc_panel_text */
inline void panel(const Text& content, PanelOpts opts = {}) {
    sc_panel_text(content.get(), opts);
}
/** Horizontal rule with a title. @see sc_rule_str, ScRuleOpts */
inline void rule(std::string_view title, RuleOpts opts = {}) {
    sc_rule_str(detail::z(title).c_str(), opts);
}
/** Title-less rule. @see sc_rule_str */
inline void rule(RuleOpts opts = {})           { sc_rule_str(nullptr, opts); }
/** Rule with a rich-text title. @see sc_rule_text */
inline void rule(const Text& title, RuleOpts opts = {}) {
    sc_rule_text(title.get(), opts);
}
/** Prints an inline badge token (no newline). @see sc_print_badge */
inline void badge(std::string_view text, BadgeOpts opts = {}) {
    sc_print_badge(detail::z(text).c_str(), opts);
}

/** Preset alert boxes (icon + color + border). @see sc_alert_info etc. */
namespace alert {
inline void info(std::string_view c) { sc_alert_info(detail::z(c).c_str()); }
inline void debug(std::string_view c) { sc_alert_debug(detail::z(c).c_str()); }
inline void warning(std::string_view c) {
    sc_alert_warning(detail::z(c).c_str());
}
inline void error(std::string_view c) { sc_alert_error(detail::z(c).c_str()); }
inline void success(std::string_view c) {
    sc_alert_success(detail::z(c).c_str());
}
inline void show(AlertType t, std::string_view c) {
    sc_alert_str(t, detail::z(c).c_str());
}
}  // namespace alert

/** Rich-compatible inline markup (`[bold red]…[/]`). @see sc_markup_parse. */
namespace markup {
inline Text parse(std::string_view m) { return Text::markup(m); }
inline Text parse(std::string_view m, MarkupOpts o) {
    return Text::markup(m, o);
}
inline void print(std::string_view m) { sc_markup_print(detail::z(m).c_str()); }
inline void println(std::string_view m) {
    sc_markup_println(detail::z(m).c_str());
}
inline void println(std::string_view m, MarkupOpts o) {
    sc_markup_println_opts(detail::z(m).c_str(), o);
}
}  // namespace markup

// ── Capture / compose ────────────────────────────────────────────────────────
/** Renders a widget into an owning Rendered for reuse. @see sc_capture_str. */
namespace capture {
inline Rendered str(std::string_view s) {
    return Rendered::adopt(sc_capture_str(detail::z(s).c_str()));
}
inline Rendered text(const Text& t) {
    return Rendered::adopt(sc_capture_text(t.get()));
}
inline Rendered table(const Table& t, TableOpts o = {}) {
    return Rendered::adopt(sc_capture_table(t.get(), o));
}
inline Rendered list(const List& l) {
    return Rendered::adopt(sc_capture_list(l.get()));
}
inline Rendered tree(const Tree& t) {
    return Rendered::adopt(sc_capture_tree(t.get()));
}
inline Rendered kv(const Kv& k) {
    return Rendered::adopt(sc_capture_kv(k.get()));
}
inline Rendered columns(const Columns& c) {
    return Rendered::adopt(sc_capture_columns(c.get()));
}
inline Rendered panel(std::string_view content, PanelOpts o = {}) {
    return Rendered::adopt(sc_capture_panel_str(detail::z(content).c_str(), o));
}
inline Rendered panel(const Text& content, PanelOpts o = {}) {
    return Rendered::adopt(sc_capture_panel_text(content.get(), o));
}
inline Rendered rule(std::string_view title, RuleOpts o = {}) {
    return Rendered::adopt(sc_capture_rule_str(detail::z(title).c_str(), o));
}
}  // namespace capture

/** Stacks captured renderings top-to-bottom into one column (`gap` blank lines
 *  between). Inputs are not consumed. @see sc_vstack */
inline Rendered vstack(const std::vector<const Rendered*>& parts, int gap = 0) {
    std::vector<const ScRendered*> raw;
    raw.reserve(parts.size());
    for (const Rendered* r : parts) raw.push_back(r->get());
    return Rendered::adopt(sc_vstack(raw.data(), raw.size(), gap));
}

/** Prints a string with padding. @see sc_pad_str */
inline void pad(std::string_view s, PadOpts opts) {
    sc_pad_str(detail::z(s).c_str(), opts);
}
/** Prints a string aligned within `w` columns. @see sc_align_str */
inline void align(std::string_view s, HAlign a, int w = 0) {
    sc_align_str(detail::z(s).c_str(), a, w);
}

// ── Utilities ────────────────────────────────────────────────────────────────
/**
 * Sets the process-wide ANSI passthrough for user strings (default: off,
 * i.e. escape sequences are stripped at the API boundary).
 * @see sc_set_allow_ansi
 */
inline void set_allow_ansi(bool allow) { sc_set_allow_ansi(allow); }
/** Returns the current process-wide ANSI passthrough. @see sc_allow_ansi */
inline bool allow_ansi()               { return sc_allow_ansi(); }
/** Returns `s` with all ANSI escapes removed. @see sc_strip_ansi */
inline std::string strip_ansi(std::string_view s) {
    char* c = sc_strip_ansi(detail::z(s).c_str());
    std::string r(c ? c : "");
    std::free(c);
    return r;
}
/** Truncates `s` to `max_cols` columns + `ellipsis`. @see sc_truncate */
inline std::string truncate(std::string_view s, int max_cols,
                            std::string_view ellipsis = "…") {
    char* c = sc_truncate(detail::z(s).c_str(), max_cols,
                          detail::z(ellipsis).c_str());
    std::string r(c ? c : "");
    std::free(c);
    return r;
}
/** Overwrites the current terminal line in place. @see sc_clear_line */
inline void clear_line() { sc_clear_line(); }

/** Redirects the thread-local output stream; `nullptr` restores stdout. */
inline void set_output(std::FILE* out) { sc_output_set_stream(out); }
/** Current output stream (never NULL). @see sc_output_stream */
inline std::FILE* output_stream()       { return sc_output_stream(); }

/**
 * RAII output redirection: routes sparcli output to `out` for the lifetime of
 * the object and restores the previous stream on scope exit. The stream target
 * is thread-local. @see sc_output_set_stream.
 */
class ScopedOutput {
public:
    explicit ScopedOutput(std::FILE* out) : prev_(sc_output_stream()) {
        sc_output_set_stream(out);
    }
    ~ScopedOutput() { sc_output_set_stream(prev_); }
    ScopedOutput(const ScopedOutput&) = delete;
    ScopedOutput& operator=(const ScopedOutput&) = delete;
private:
    std::FILE* prev_;
};

// ── Application helpers ──────────────────────────────────────────────────────
// XDG base directories and pager integration. @see sparcli_paths.h,
// sparcli_pager.h

/** XDG base-directory helpers; directories are created on first use. */
namespace paths {

using Kind = ScPathKind;

namespace detail {
/** Wraps a C-heap path into an optional string (empty optional on NULL). */
inline std::optional<std::string> take_path(char* c) {
    if (!c) { return std::nullopt; }
    std::string r(c);
    std::free(c);
    return r;
}
}  // namespace detail

/** Per-app base directory for `kind` (created). @see sc_path */
inline std::optional<std::string> dir(Kind kind, std::string_view appname) {
    return detail::take_path(
        sc_path(kind, sparcli::detail::z(appname).c_str()));
}
/** `~/.config/<app>` (or `$XDG_CONFIG_HOME`). @see sc_path_config */
inline std::optional<std::string> config(std::string_view appname) {
    return dir(SC_PATH_CONFIG, appname);
}
/** `~/.local/share/<app>` (or `$XDG_DATA_HOME`). @see sc_path_data */
inline std::optional<std::string> data(std::string_view appname) {
    return dir(SC_PATH_DATA, appname);
}
/** `~/.cache/<app>` (or `$XDG_CACHE_HOME`). @see sc_path_cache */
inline std::optional<std::string> cache(std::string_view appname) {
    return dir(SC_PATH_CACHE, appname);
}
/** `~/.local/state/<app>` (or `$XDG_STATE_HOME`). @see sc_path_state */
inline std::optional<std::string> state(std::string_view appname) {
    return dir(SC_PATH_STATE, appname);
}
/** File path inside an app dir (parents created). @see sc_path_file */
inline std::optional<std::string> file(Kind kind, std::string_view appname,
                                       std::string_view relative) {
    return detail::take_path(
        sc_path_file(kind, sparcli::detail::z(appname).c_str(),
                     sparcli::detail::z(relative).c_str()));
}

}  // namespace paths

using PagerOpts = ScPagerOpts;

/**
 * RAII pager session: between construction and `end()`/destruction, output on
 * this thread is piped through `$PAGER` / `less -R`. No-op when the output
 * stream is not a terminal (unless `opts.always`). @see sc_pager_begin
 */
class Pager {
public:
    explicit Pager(PagerOpts opts = {}) : p_(sc_pager_begin(opts)) {}
    /** Ends the session early and returns the pager's exit status. */
    int end() {
        int status = p_ ? sc_pager_end(p_) : 0;
        p_ = nullptr;
        return status;
    }
    ~Pager() { if (p_) { sc_pager_end(p_); } }
    Pager(Pager&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    Pager& operator=(Pager&& o) noexcept {
        if (this != &o) { end(); p_ = o.p_; o.p_ = nullptr; }
        return *this;
    }
    Pager(const Pager&) = delete;
    Pager& operator=(const Pager&) = delete;
private:
    ScPager* p_;
};

using LiveOpts = ScLiveOpts;

/**
 * RAII live-display session: re-renders a composed frame in place, so
 * multiple widgets form a continuously updating dashboard. Off-terminal,
 * updates are buffered and only the final frame is printed when the session
 * ends. @see sc_live_begin
 */
class Live {
public:
    explicit Live(LiveOpts opts = {}) : l_(sc_live_begin(opts)) {}
    /** Redraws the live region with a captured frame. @see sc_live_update */
    void update(const Rendered& frame) {
        if (l_) { sc_live_update(l_, frame.get()); }
    }
    /** Redraws the live region with a plain (multi-line) string. */
    void update(std::string_view s) {
        if (l_) { sc_live_update_str(l_, detail::z(s).c_str()); }
    }
    /** Ends the session early: restores the terminal and, off-terminal,
        prints the buffered final frame. */
    void end() {
        if (l_) { sc_live_end(l_); l_ = nullptr; }
    }
    ~Live() { if (l_) { sc_live_end(l_); } }
    Live(Live&& o) noexcept : l_(o.l_) { o.l_ = nullptr; }
    Live& operator=(Live&& o) noexcept {
        if (this != &o) { end(); l_ = o.l_; o.l_ = nullptr; }
        return *this;
    }
    Live(const Live&) = delete;
    Live& operator=(const Live&) = delete;
private:
    ScLive* l_;
};

/**
 * Structured error report rendered as a red alert panel: message, cause
 * chain, hint and exit code. The pretty replacement for
 * `std::cerr << …; std::exit(1);` in CLI applications. @see sparcli_error.h
 */
class ErrorReport {
public:
    explicit ErrorReport(std::string_view message)
        : p_(sc_error_new(detail::z(message).c_str())) {}
    ~ErrorReport() { sc_error_free(p_); }
    ErrorReport(ErrorReport&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    ErrorReport& operator=(ErrorReport&& o) noexcept {
        if (this != &o) { sc_error_free(p_); p_ = o.p_; o.p_ = nullptr; }
        return *this;
    }
    ErrorReport(const ErrorReport&) = delete;
    ErrorReport& operator=(const ErrorReport&) = delete;

    /** Appends one `caused by:` line. @see sc_error_add_cause */
    ErrorReport& cause(std::string_view c) {
        sc_error_add_cause(p_, detail::z(c).c_str());
        return *this;
    }
    /** Sets the `Hint:` line. @see sc_error_set_hint */
    ErrorReport& hint(std::string_view h) {
        sc_error_set_hint(p_, detail::z(h).c_str());
        return *this;
    }
    /** Sets the exit code used by die() (default 1). @see sc_error_set_code */
    ErrorReport& code(int exit_code) {
        sc_error_set_code(p_, exit_code);
        return *this;
    }
    /** The configured exit code. @see sc_error_code */
    int exit_code() const { return sc_error_code(p_); }

    /** Renders to the current output stream (no exit). @see sc_error_print */
    void print() const { sc_error_print(p_); }
    /** Renders to stderr (no exit). @see sc_error_print_stderr */
    void print_stderr() const { sc_error_print_stderr(p_); }
    /** Renders to stderr and exits with the configured code. @see sc_die */
    [[noreturn]] void die() {
        ScError* consumed = p_;
        p_ = nullptr;
        sc_die(consumed);
    }
private:
    ScError* p_;
};

/** One-shot error exit: render `message` (+ `hint`) and exit. @see sc_die_msg */
[[noreturn]] inline void die(int exit_code, std::string_view message,
                             std::string_view hint = {}) {
    sc_die_msg(exit_code, detail::z(message).c_str(),
               hint.empty() ? nullptr : detail::z(hint).c_str());
}

// ── Logging ──────────────────────────────────────────────────────────────────
// Leveled, colored terminal output + plain-text file sinks. @see sparcli_log.h

using LogLevel = ScLogLevel;
using LoggerOpts = ScLoggerOpts;

/**
 * Handle-based logger (RAII): independent sinks and options, e.g. for a
 * library/subsystem. Messages are passed as data (never as a printf format
 * string). For the process-wide logger use the `logging::` functions.
 */
class Logger {
public:
    explicit Logger(LoggerOpts opts = {}) : p_(sc_logger_new(opts)) {
        if (!p_) { throw std::bad_alloc(); }
    }
    ~Logger() { sc_logger_free(p_); }
    Logger(Logger&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    Logger& operator=(Logger&& o) noexcept {
        if (this != &o) { sc_logger_free(p_); p_ = o.p_; o.p_ = nullptr; }
        return *this;
    }
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /** Adds a colored terminal sink (stream is borrowed, not closed). */
    Logger& add_terminal(std::FILE* stream, LogLevel min = SC_LOG_DEBUG) {
        sc_logger_add_terminal(p_, stream, min);
        return *this;
    }
    /** Adds a plain-text file sink (append mode; closed on destruction). */
    Logger& add_file(std::string_view path, LogLevel min = SC_LOG_DEBUG) {
        sc_logger_add_file(p_, detail::z(path).c_str(), min);
        return *this;
    }
    /** Sets the logger-wide minimum level. */
    Logger& level(LogLevel min) {
        sc_logger_set_level(p_, min);
        return *this;
    }

    /** Emits one record (message is data, not a format string). */
    void log(LogLevel lvl, std::string_view msg) const {
        sc_logger_log(p_, lvl, nullptr, 0, "%s", detail::z(msg).c_str());
    }
    void debug(std::string_view msg) const { log(SC_LOG_DEBUG, msg); }
    void info(std::string_view msg)  const { log(SC_LOG_INFO, msg); }
    void warn(std::string_view msg)  const { log(SC_LOG_WARN, msg); }
    void error(std::string_view msg) const { log(SC_LOG_ERROR, msg); }

private:
    ScLogger* p_;
};

/** The process-wide global logger (built-in stderr sink at INFO). */
namespace logging {

/** Terminal-sink level (default INFO). @see sc_log_set_level */
inline void set_level(LogLevel lvl)      { sc_log_set_level(lvl); }
/** Current terminal-sink level. @see sc_log_level */
inline LogLevel level()                  { return sc_log_level(); }
/** Format options (timestamps, source, markup). @see sc_log_set_opts */
inline void set_opts(LoggerOpts opts)    { sc_log_set_opts(opts); }
/** Adds a plain-text file sink. @see sc_log_add_file */
inline bool add_file(std::string_view path, LogLevel min = SC_LOG_DEBUG) {
    return sc_log_add_file(detail::z(path).c_str(), min);
}
/** Closes file sinks + restores defaults. @see sc_log_reset */
inline void reset()                      { sc_log_reset(); }

/** Emits one record (message is data, not a format string). */
inline void log(LogLevel lvl, std::string_view msg) {
    sc_log_log(lvl, nullptr, 0, "%s", detail::z(msg).c_str());
}
inline void debug(std::string_view msg) { log(SC_LOG_DEBUG, msg); }
inline void info(std::string_view msg)  { log(SC_LOG_INFO, msg); }
inline void warn(std::string_view msg)  { log(SC_LOG_WARN, msg); }
inline void error(std::string_view msg) { log(SC_LOG_ERROR, msg); }

}  // namespace logging

// ── Argument parser ──────────────────────────────────────────────────────────
// Declarative subcommands, typed options, auto --help, did-you-mean and zsh
// completion. @see sparcli_args.h

using ArgsOpts = ScArgsOpts;
using ArgsStatus = ScArgsStatus;
using ArgType = ScArgType;

/**
 * A borrowed command node of an `Args` tree. Builder methods chain; the node
 * is owned by the `Args` parser, never by this wrapper.
 */
class ArgsCmd {
public:
    explicit ArgsCmd(ScArgsCmd* cmd) : c_(cmd) {}

    /** Adds (and returns) a nested subcommand. @see sc_args_subcommand */
    ArgsCmd subcommand(std::string_view name, std::string_view about) {
        return ArgsCmd(sc_args_subcommand(c_, detail::z(name).c_str(),
                                          detail::z(about).c_str()));
    }
    /** Help section heading for this command. @see sc_args_cmd_group */
    ArgsCmd& group(std::string_view heading) {
        sc_args_cmd_group(c_, detail::z(heading).c_str());
        return *this;
    }
    /** Adds a boolean flag. @see sc_args_flag */
    ArgsCmd& flag(std::string_view long_name, char short_name,
                  std::string_view help) {
        sc_args_flag(c_, detail::z(long_name).c_str(), short_name,
                     detail::z(help).c_str());
        return *this;
    }
    /** Adds a typed value option. @see sc_args_opt */
    ArgsCmd& opt(std::string_view long_name, char short_name, ArgType type,
                 std::string_view metavar, std::string_view help) {
        sc_args_opt(c_, detail::z(long_name).c_str(), short_name, type,
                    detail::z(metavar).c_str(), detail::z(help).c_str());
        return *this;
    }
    /** Sets an option default. @see sc_args_opt_default */
    ArgsCmd& opt_default(std::string_view long_name, std::string_view value) {
        sc_args_opt_default(c_, detail::z(long_name).c_str(),
                            detail::z(value).c_str());
        return *this;
    }
    /** Restricts an option to fixed choices. @see sc_args_opt_choices */
    ArgsCmd& opt_choices(std::string_view long_name,
                         const std::vector<std::string>& choices) {
        std::vector<const char*> pointers;
        pointers.reserve(choices.size() + 1);
        for (const auto& choice : choices) { pointers.push_back(choice.c_str()); }
        pointers.push_back(nullptr);
        sc_args_opt_choices(c_, detail::z(long_name).c_str(), pointers.data());
        return *this;
    }
    /** Marks an option as required. @see sc_args_opt_required */
    ArgsCmd& opt_required(std::string_view long_name) {
        sc_args_opt_required(c_, detail::z(long_name).c_str());
        return *this;
    }
    /** Adds a positional argument slot. @see sc_args_positional */
    ArgsCmd& positional(std::string_view name, ArgType type,
                        std::string_view help, bool required = false,
                        bool variadic = false) {
        sc_args_positional(c_, detail::z(name).c_str(), type,
                           detail::z(help).c_str(), required, variadic);
        return *this;
    }

    /** Command name. @see sc_args_cmd_name */
    std::string name() const {
        const char* n = sc_args_cmd_name(c_);
        return n ? n : "";
    }
    /** Borrowed underlying node (escape hatch). */
    ScArgsCmd* get() { return c_; }
    bool operator==(const ArgsCmd& other) const { return c_ == other.c_; }

private:
    ScArgsCmd* c_;
};

/**
 * Owning, move-only argument parser (wraps `ScArgs`).
 *
 * Build the tree via `root()`, then `parse(argc, argv)`. On success the
 * matched command is returned; on `--help`/`--version` or errors the
 * optional is empty and `status()` tells which (exit 0 vs 2).
 */
class Args {
public:
    explicit Args(ArgsOpts opts = {}) : p_(sc_args_new(opts)) {
        if (!p_) { throw std::bad_alloc(); }
    }
    ~Args() { sc_args_free(p_); }
    Args(Args&& o) noexcept : p_(o.p_), status_(o.status_) { o.p_ = nullptr; }
    Args& operator=(Args&& o) noexcept {
        if (this != &o) {
            sc_args_free(p_);
            p_ = o.p_;
            status_ = o.status_;
            o.p_ = nullptr;
        }
        return *this;
    }
    Args(const Args&) = delete;
    Args& operator=(const Args&) = delete;

    /** The root command node (attach options/subcommands here). */
    ArgsCmd root() { return ArgsCmd(sc_args_root(p_)); }

    /** Parses argv; empty optional = help/version handled or parse error. */
    std::optional<ArgsCmd> parse(int argc, char** argv) {
        const ScArgsCmd* matched = sc_args_parse(p_, argc, argv, &status_);
        if (!matched) { return std::nullopt; }
        return ArgsCmd(const_cast<ScArgsCmd*>(matched));
    }
    /** Outcome of the last parse. */
    ArgsStatus status() const { return status_; }
    /** Suggested process exit code for the last parse outcome. */
    int exit_code() const { return status_ == SC_ARGS_ERROR ? 2 : 0; }

    /** String value (empty optional = never supplied, no default). */
    std::optional<std::string> get_str(std::string_view name) const {
        const char* value = sc_args_get_str(p_, detail::z(name).c_str());
        if (!value) { return std::nullopt; }
        return std::string(value);
    }
    long get_int(std::string_view name) const {
        return sc_args_get_int(p_, detail::z(name).c_str());
    }
    double get_double(std::string_view name) const {
        return sc_args_get_double(p_, detail::z(name).c_str());
    }
    bool get_flag(std::string_view name) const {
        return sc_args_get_flag(p_, detail::z(name).c_str());
    }
    int get_enum(std::string_view name) const {
        return sc_args_get_enum(p_, detail::z(name).c_str());
    }
    Color get_color(std::string_view name) const {
        return sc_args_get_color(p_, detail::z(name).c_str());
    }
    std::vector<std::string> get_many(std::string_view name) const {
        std::size_t count = 0;
        const char* const* values =
            sc_args_get_many(p_, detail::z(name).c_str(), &count);
        std::vector<std::string> result;
        result.reserve(count);
        for (std::size_t i = 0; i < count; i++) { result.emplace_back(values[i]); }
        return result;
    }
    bool present(std::string_view name) const {
        return sc_args_present(p_, detail::z(name).c_str());
    }
    /** Name of the matched command (empty optional before parsing). */
    std::optional<std::string> selected_command() const {
        const char* name = sc_args_selected_command(p_);
        if (!name) { return std::nullopt; }
        return std::string(name);
    }

    /** Renders the help screen for `cmd` (root by default). */
    void print_help(ScArgsCmd* cmd = nullptr) const {
        sc_args_print_help(p_, cmd);
    }
    /** Emits the zsh completion script. */
    void print_zsh_completion() const { sc_args_print_zsh_completion(p_); }

    /** Borrowed underlying `ScArgs*` (escape hatch to the C API). */
    ScArgs* get() { return p_; }

private:
    ScArgs* p_;
    ArgsStatus status_ = SC_ARGS_MATCHED;
};

// ── Custom shortcuts ─────────────────────────────────────────────────────────
// Bind extra keys (Ctrl-letter / F-key / Alt-letter) to actions on any widget.
// @see sparcli_shortcut.h

using KeyChord = ScKeyChord;
using Shortcut = ScShortcut;
using Key      = ScKey;

/** Ctrl + letter chord, e.g. `key_ctrl('e')`. @see sc_key_ctrl */
inline KeyChord key_ctrl(char letter) { return sc_key_ctrl(letter); }
/** Function-key chord; `n` in 1..12. @see sc_key_fn */
inline KeyChord key_fn(int n)         { return sc_key_fn(n); }
/** Alt/Meta + letter chord, e.g. `key_alt('e')`. @see sc_key_alt */
inline KeyChord key_alt(char letter)  { return sc_key_alt(letter); }

/** Short display name for a chord, e.g. "F2", "^E", "M-e".
    @see sc_key_chord_name */
inline std::string key_name(KeyChord chord) {
    char buf[16];
    sc_key_chord_name(chord, buf, sizeof buf);
    return std::string(buf);
}
/** True when a decoded key matches a chord. @see sc_key_chord_matches */
inline bool key_matches(Key key, KeyChord chord) {
    return sc_key_chord_matches(key, chord);
}
/** First shortcut whose chord matches `key`, or `nullptr`.
    @see sc_shortcut_find */
inline const Shortcut* shortcut_find(Key key,
                                     const std::vector<Shortcut>& items) {
    return sc_shortcut_find(key, items.data(), items.size());
}

/**
 * Owning builder for a set of custom shortcuts, plus the fired-id slot.
 *
 * Build it, then attach it to any widget's opts with `apply(opts)`; after the
 * call, `fired()` is the id of a RETURN-mode shortcut that ended the prompt
 * (or `-1`). Callback bindings take a `std::function<bool()>` (return `true`
 * to keep the prompt open) kept alive in an internal arena.
 *
 * @code
 * sparcli::Shortcuts sc;
 * sc.on_return(sparcli::key_fn(2), 1)
 *   .on_callback(sparcli::key_ctrl('r'), []{ reload(); return true; });
 * sparcli::ConfirmOpts opts{};
 * sc.apply(opts);
 * auto yes = sparcli::confirm("Proceed?", opts);
 * if (sc.fired() == 1) { open_help(); }
 * @endcode
 */
class Shortcuts {
public:
    /**
     * Binds a chord that ends the prompt and reports `id` via `fired()`.
     * `label` (if given) shows in the widget's key-hint footer; it must outlive
     * the run (a string literal is fine).
     */
    Shortcuts& on_return(KeyChord chord, int id, const char* label = nullptr) {
        items_.push_back(Shortcut{
            chord, id, SC_SHORTCUT_RETURN, nullptr, nullptr, label
        });
        return *this;
    }
    /** Binds a chord to a callback (returns `true` to keep the prompt open). */
    Shortcuts& on_callback(KeyChord chord, std::function<bool()> fn,
                           const char* label = nullptr) {
        fns_.push_back(std::move(fn));   // deque: addresses stay stable
        items_.push_back(Shortcut{
            chord, static_cast<int>(items_.size()), SC_SHORTCUT_CALLBACK,
            &trampoline, &fns_.back(), label
        });
        return *this;
    }

    /** Wires this set into any widget opts struct (sets the three fields). */
    template <class Opts>
    Opts& apply(Opts& opts) {
        opts.shortcuts = items_.data();
        opts.n_shortcuts = items_.size();
        opts.out_shortcut_id = &fired_id_;
        return opts;
    }

    /** Id of the RETURN-mode shortcut that ended the last run, or `-1`. */
    int fired() const { return fired_id_; }

private:
    static bool trampoline(int /*id*/, void *user) {
        return (*static_cast<std::function<bool()> *>(user))();
    }
    std::vector<Shortcut>             items_;
    std::deque<std::function<bool()>> fns_;   // stable addresses for `user`
    int                               fired_id_ = -1;
};

// ── Input widgets ────────────────────────────────────────────────────────────
// Each returns std::optional / std::vector; the result is empty (std::nullopt)
// when the user cancels (Esc / Ctrl-C) or no interactive terminal is available.
// @see sc_confirm, sc_text_input, etc.

/** True when an interactive prompt can run (a TTY is present). */
inline bool input_available() { return sc_input_available(); }

/** Yes/No prompt. @return the choice, or `std::nullopt` on cancel/no-TTY. */
inline std::optional<bool> confirm(std::string_view question,
                                   ConfirmOpts opts = {}) {
    bool out = false;
    if (sc_confirm(detail::z(question).c_str(), &out, opts) == SC_INPUT_OK)
        return out;
    return std::nullopt;
}

namespace detail {
inline std::optional<std::string> take(char* out, InputStatus st) {
    if (st != SC_INPUT_OK) { std::free(out); return std::nullopt; }
    std::string r(out ? out : "");
    std::free(out);
    return r;
}
}  // namespace detail

/** Single-line text entry. @return the string, or nullopt on cancel/no-TTY. */
inline std::optional<std::string> text_input(std::string_view prompt,
                                             TextInputOpts opts = {}) {
    char* out = nullptr;
    // Sequence the call before reading `out`: the evaluation order of function
    // arguments is unspecified, so `take(out, sc_text_input(..., &out, ...))`
    // could read `out` while it is still nullptr (losing the value and leaking
    // the string the C call allocates).
    const InputStatus st = sc_text_input(detail::z(prompt).c_str(), &out, opts);
    return detail::take(out, st);
}
/** Masked entry. @return the secret, or nullopt. @see sc_password_input */
inline std::optional<std::string> password_input(std::string_view prompt,
                                                 PasswordOpts opts = {}) {
    char* out = nullptr;
    const InputStatus st =
        sc_password_input(detail::z(prompt).c_str(), &out, opts);
    return detail::take(out, st);
}
/** Multi-line entry (Ctrl-D submits). @return the text, or nullopt. */
inline std::optional<std::string> textarea(std::string_view prompt,
                                           TextareaOpts opts = {}) {
    char* out = nullptr;
    const InputStatus st = sc_textarea(detail::z(prompt).c_str(), &out, opts);
    return detail::take(out, st);
}
/** Numeric entry. @return the value, or nullopt. @see sc_number_input */
inline std::optional<double> number_input(std::string_view prompt,
                                          NumberOpts opts = {}) {
    double out = 0.0;
    if (sc_number_input(detail::z(prompt).c_str(), &out, opts) == SC_INPUT_OK)
        return out;
    return std::nullopt;
}
/** Numeric entry returning the exact submitted text ('.'-normalized, never
 *  round-tripped through `double`), so the caller can construct an arbitrary-
 *  precision decimal type. @return the string, or nullopt.
 *  @see ScNumberOpts.out_text */
inline std::optional<std::string> number_input_text(std::string_view prompt,
                                                    NumberOpts opts = {}) {
    double value = 0.0;
    char* out = nullptr;
    opts.out_text = &out;
    const InputStatus st =
        sc_number_input(detail::z(prompt).c_str(), &value, opts);
    return detail::take(out, st);
}
/** Month-grid date picker (a zeroed `seed` starts at today). @return the picked
 *  date, or nullopt. @see sc_datepicker */
inline std::optional<std::tm> datepicker(std::tm seed = {},
                                         DatePickerOpts opts = {}) {
    if (sc_datepicker(&seed, opts) == SC_INPUT_OK) return seed;
    return std::nullopt;
}

/** Pure subsequence match (no TTY needed). @see sc_fuzzy_match */
inline bool fuzzy_match(std::string_view pattern, std::string_view str,
                        int* score = nullptr) {
    return sc_fuzzy_match(detail::z(pattern).c_str(),
                          detail::z(str).c_str(), score);
}

/** Sets process-wide input defaults inherited by every widget. */
inline void set_theme(const InputTheme& theme) { sc_input_set_theme(&theme); }
/** Clears the process-wide theme (back to built-in defaults). */
inline void reset_theme()                       { sc_input_set_theme(nullptr); }
/** Current process-wide theme. @see sc_input_theme */
inline InputTheme theme()                        { return sc_input_theme(); }

/**
 * Owning, move-only single/multi-choice selection (wraps `ScSelect`). Labels
 * are copied by the C side. Set `SelectOpts.multi` for checkboxes.
 * @see sc_select_new, sc_select_run.
 */
class Select {
public:
    /** Allocates a selection prompt. @throws std::bad_alloc */
    explicit Select(SelectOpts opts = {}) : p_(sc_select_new(opts)) {
        if (!p_) throw std::bad_alloc();
    }
    ~Select() { sc_select_free(p_); }
    Select(Select&& o) noexcept : p_(o.p_), count_(o.count_) { o.p_ = nullptr; }
    Select& operator=(Select&& o) noexcept {
        if (this != &o) {
            sc_select_free(p_); p_ = o.p_; count_ = o.count_; o.p_ = nullptr;
        }
        return *this;
    }
    Select(const Select&) = delete;
    Select& operator=(const Select&) = delete;

    /** Appends a selectable item (label copied). @see sc_select_add */
    Select& add(std::string_view label) {
        sc_select_add(detail::live(p_), detail::z(label).c_str());
        ++count_; return *this;
    }
    /** Pre-positions the cursor. @see sc_select_set_cursor */
    void set_cursor(std::size_t index) {
        sc_select_set_cursor(detail::live(p_), index);
    }
    /** Pre-checks an item (multi-select). @see sc_select_set_checked */
    void set_checked(std::size_t index, bool on = true) {
        sc_select_set_checked(detail::live(p_), index, on);
    }
    /** Highlighted index (use from a shortcut callback).
        @see sc_select_cursor */
    std::size_t cursor() const { return sc_select_cursor(detail::live(p_)); }
    /** Removes the item at `index` (live, from a callback).
        @see sc_select_remove */
    void remove(std::size_t index) {
        sc_select_remove(detail::live(p_), index);
    }
    /** Current label at `index` (empty if out of range).
        @see sc_select_label */
    std::string_view label(std::size_t index) const {
        const char* s = sc_select_label(detail::live(p_), index);
        return s ? std::string_view(s) : std::string_view();
    }
    /** Replaces the label at `index` (copied). @see sc_select_set_label */
    void set_label(std::size_t index, std::string_view text) {
        sc_select_set_label(detail::live(p_), index, detail::z(text).c_str());
    }

    /**
     * Runs the prompt. @return chosen indices in display order (multi-select)
     * or a single index (single-select), or `std::nullopt` on cancel/no-TTY.
     */
    std::optional<std::vector<std::size_t>> run() {
        std::vector<std::size_t> idx(count_ ? count_ : 1);
        std::size_t n = idx.size();
        if (sc_select_run(detail::live(p_), idx.data(), &n) != SC_INPUT_OK)
            return std::nullopt;
        idx.resize(n);
        return idx;
    }
    /** Single-select convenience. @return the index, or `std::nullopt`. */
    std::optional<std::size_t> run_one() {
        auto r = run();
        if (r && !r->empty()) return (*r)[0];
        return std::nullopt;
    }

    /** Borrowed underlying `ScSelect*` (escape hatch); not owned. */
    ScSelect* get() { return p_; }

private:
    ScSelect*   p_;
    std::size_t count_ = 0;
};

/**
 * Owning, move-only incremental fuzzy finder (wraps `ScFuzzy`). Labels/rows are
 * copied by the C side; set `FuzzyOpts.table` for a table view.
 * @see sc_fuzzy_new, sc_fuzzy_run.
 */
class Fuzzy {
public:
    /** Allocates a fuzzy finder. @throws std::bad_alloc */
    explicit Fuzzy(FuzzyOpts opts = {}) : p_(sc_fuzzy_new(opts)) {
        if (!p_) throw std::bad_alloc();
    }
    ~Fuzzy() { sc_fuzzy_free(p_); }
    Fuzzy(Fuzzy&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    Fuzzy& operator=(Fuzzy&& o) noexcept {
        if (this != &o) { sc_fuzzy_free(p_); p_ = o.p_; o.p_ = nullptr; }
        return *this;
    }
    Fuzzy(const Fuzzy&) = delete;
    Fuzzy& operator=(const Fuzzy&) = delete;

    /** Adds a single-field item (copied). @see sc_fuzzy_add */
    Fuzzy& add(std::string_view label) {
        sc_fuzzy_add(detail::live(p_), detail::z(label).c_str()); return *this;
    }
    /** Adds a multi-field row; `fields[0]` is the match key. */
    Fuzzy& add_row(const std::vector<std::string>& fields) {
        std::vector<const char*> ptrs;
        ptrs.reserve(fields.size());
        for (const auto& f : fields) ptrs.push_back(f.c_str());
        sc_fuzzy_add_row(detail::live(p_), ptrs.data(), ptrs.size());
        return *this;
    }
    /** Runs the finder. @return the add-order index, or nullopt. */
    std::optional<std::size_t> run() {
        std::size_t out = 0;
        if (sc_fuzzy_run(detail::live(p_), &out) != SC_INPUT_OK)
            return std::nullopt;
        return out;
    }
    /** Highlighted row's index (from a callback). @see sc_fuzzy_cursor_index */
    std::size_t cursor_index() const {
        return sc_fuzzy_cursor_index(detail::live(p_));
    }
    /** Removes the row at `index` (live, from a callback).
        @see sc_fuzzy_remove */
    void remove(std::size_t index) { sc_fuzzy_remove(detail::live(p_), index); }

    /** Borrowed underlying `ScFuzzy*` (escape hatch); not owned. */
    ScFuzzy* get() { return p_; }

private:
    ScFuzzy* p_;
};

}  // namespace sparcli
