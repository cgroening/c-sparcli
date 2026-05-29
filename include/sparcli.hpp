#pragma once
//
// sparcli.hpp — header-only C++20 wrapper around the sparcli C API.
//
// Goals: RAII (no leaks / no double-free), owned strings where the C API only
// borrows (no dangling pointers), clean call sites with little boilerplate,
// and a `.get()` escape hatch so you can always drop down to the C API.
//
// Requires C++20 (designated initializers for the *Opts structs). The only
// exception thrown is std::bad_alloc, from a constructor when the underlying
// `sc_*_new` returns NULL; everything else is exception-free.
//
//   #include <sparcli.hpp>
//   using namespace sparcli;
//   panel("Hi", { .border = { .type = SC_BORDER_ROUNDED } });
//
#include <sparcli.h>

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <deque>
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
}  // namespace detail

// ── Rich text (owns ScText*) ─────────────────────────────────────────────────
class Text {
public:
    Text() : p_(sc_text_new()) { require(); }
    explicit Text(std::string_view s)
        : p_(sc_text_from_str(detail::z(s).c_str())) { require(); }

    // Parse inline markup into a Text (`[bold red]…[/]`).
    static Text markup(std::string_view m) {
        return Text(sc_markup_parse(detail::z(m).c_str()));
    }
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

    Text& append(std::string_view s, TextStyle st = {}) {
        sc_text_append(p_, detail::z(s).c_str(), st);
        return *this;
    }
    Text& append_markup(std::string_view m) {
        sc_markup_append(p_, detail::z(m).c_str());
        return *this;
    }
    Text& append_markup(std::string_view m, MarkupOpts opts) {
        sc_markup_append_opts(p_, detail::z(m).c_str(), opts);
        return *this;
    }
    Text& append_badge(std::string_view s, BadgeOpts opts = {}) {
        sc_text_append_badge(p_, detail::z(s).c_str(), opts);
        return *this;
    }

    void        print()          const { sc_print_text(p_); }
    std::size_t visible_width()  const { return sc_text_visible_width(p_); }
    std::size_t span_count()     const { return sc_text_span_count(p_); }

    ScText*       get()       { return p_; }
    const ScText* get() const { return p_; }

private:
    explicit Text(ScText* adopted) : p_(adopted) { require(); }
    void require() const { if (!p_) throw std::bad_alloc(); }
    ScText* p_;
};

// ── Captured rendering (owns ScRendered*) ────────────────────────────────────
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

    void pad(PadOpts opts)            const { sc_pad_print(p_, opts); }
    void align(HAlign a, int w = 0)   const { sc_align_print(p_, a, w); }

    ScRendered*       get()       { return p_; }
    const ScRendered* get() const { return p_; }

    // Adopt a raw ScRendered* (from the sc_capture_* family).
    static Rendered adopt(ScRendered* r) { return Rendered(r); }

private:
    explicit Rendered(ScRendered* adopted) : p_(adopted) {
        if (!p_) throw std::bad_alloc();
    }
    ScRendered* p_;
};

// ── Table (owns ScTableData* and the strings/headers the C API borrows) ──────
class Table {
public:
    // A single cell. Implicitly constructible from any string-like, so
    // `t.add_row({ "a", "b" })` just works; richer cells via the helpers below.
    class Cell {
    public:
        Cell(const char* s)        : str_(s) {}
        Cell(std::string s)        : str_(std::move(s)) {}
        Cell(std::string_view s)   : str_(s) {}

        Cell& align(HAlign h)  { halign_set_ = true; halign_ = h; return *this; }
        Cell& valign(VAlign v) { valign_set_ = true; valign_ = v; return *this; }
        Cell& colspan(int n)   { col_span_ = n; return *this; }
        Cell& rowspan(int n)   { row_span_ = n; return *this; }

        // Cell whose content is parsed as inline markup (prefer `cell_markup`).
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

    Table& add_column(std::string_view header, ColOpts opts = {}) {
        // The C side copies the header, so a transient c_str() is fine here;
        // only cell strings (borrowed until print) go through the arena.
        sc_table_add_column(p_, detail::z(header).c_str(), opts);
        return *this;
    }
    Table& add_row(std::vector<Cell> cells) {
        emit(cells, Row::Data);
        return *this;
    }
    Table& add_row(std::vector<Cell> cells, Color bg) {
        auto sc = build(cells);
        sc_table_add_row_bg(p_, sc.data(), sc.size(), bg);
        return *this;
    }
    Table& add_footer_row(std::vector<Cell> cells) {
        emit(cells, Row::Footer);
        return *this;
    }

    void print(TableOpts opts = {}) const { sc_table_print(p_, opts); }

    ScTableData*       get()       { return p_; }
    const ScTableData* get() const { return p_; }

private:
    enum class Row { Data, Footer };

    const char* own(std::string_view s) {
        // The deque lives behind a unique_ptr, so its node addresses are stable
        // even across a Table move — the borrowed c_str() pointers stay valid.
        strings_->emplace_back(s);
        return strings_->back().c_str();
    }
    std::vector<ScCell> build(std::vector<Cell>& cells) {
        std::vector<ScCell> out;
        out.reserve(cells.size());
        for (auto& c : cells) {
            ScCell sc = (c.kind_ == Cell::Kind::Markup)
                ? sc_cell_from_markup(c.str_.c_str())  // table owns the ScText
                : sc_cell_from_str(own(c.str_));        // wrapper owns the string
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
            sc_table_add_footer_row(p_, sc.data(), sc.size());
        } else {
            sc_table_add_row(p_, sc.data(), sc.size());
        }
    }

    ScTableData* p_;
    // Owns every borrowed cell/header string. Behind a unique_ptr so the deque
    // object never relocates → stored c_str() pointers survive a Table move.
    std::unique_ptr<std::deque<std::string>> strings_;
};

// Cell helpers (free functions so call sites read `cell_markup("…")`).
inline Table::Cell cell(std::string_view s)        { return Table::Cell(s); }
inline Table::Cell cell_markup(std::string_view s)  {
    return Table::Cell::from_markup(std::string(s));
}

// ── List (owns ScList*; sub-lists and rich-text items are kept alive safely) ─
//
// The C list borrows any ScText passed to it, so the wrapper owns those Text
// objects in a shared arena that lives as long as the root list (sub-lists
// share the same arena). String items are copied by the C side, so they need
// no special handling.
class List {
public:
    class Item {
    public:
        // Attach a sub-list (owned by the parent list; shares its text arena).
        List sub(ListOpts opts = {}) {
            return List(sc_list_add_sub(p_, opts), /*owns=*/false, arena_);
        }
        ScListItem* get() const { return p_; }
    private:
        friend class List;
        using Arena = std::shared_ptr<std::deque<Text>>;
        Item(ScListItem* p, Arena a) : p_(p), arena_(std::move(a)) {}
        ScListItem* p_;
        Arena       arena_;
    };

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

    Item add(std::string_view s, TextStyle st = {}) {
        return Item(sc_list_add_str(p_, detail::z(s).c_str(), st), arena_);
    }
    // Rich item: the list takes ownership of the Text (moved into the arena).
    Item add(Text t) {
        arena_->push_back(std::move(t));
        return Item(sc_list_add_text(p_, arena_->back().get()), arena_);
    }
    Item add_markup(std::string_view m) { return add(Text::markup(m)); }

    void print() const { sc_list_print(p_); }

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

// ── Tree (owns ScTree*; nodes are non-owning handles) ────────────────────────
class Tree {
public:
    class Node {
    public:
        Node() : p_(nullptr) {}
        explicit Node(ScTreeNode* p) : p_(p) {}
        ScTreeNode* get() const { return p_; }
    private:
        ScTreeNode* p_;
    };

    explicit Tree(TreeOpts opts = {}) : p_(sc_tree_new(opts)) {
        if (!p_) throw std::bad_alloc();
    }
    ~Tree() { sc_tree_free(p_); }
    Tree(Tree&& o) noexcept : p_(o.p_), texts_(std::move(o.texts_)) { o.p_ = nullptr; }
    Tree& operator=(Tree&& o) noexcept {
        if (this != &o) {
            sc_tree_free(p_);
            p_ = o.p_; texts_ = std::move(o.texts_); o.p_ = nullptr;
        }
        return *this;
    }
    Tree(const Tree&) = delete;
    Tree& operator=(const Tree&) = delete;

    Node add(std::string_view s, Node parent = {}, TextStyle st = {},
             std::string_view prefix = {}, TextStyle prefix_st = {}) {
        return Node(sc_tree_add_str(
            p_, parent.get(), detail::z(s).c_str(), st,
            prefix.empty() ? nullptr : detail::z(prefix).c_str(), prefix_st));
    }
    // Rich node: the tree borrows the ScText, so the wrapper owns it (the C
    // text pointer stays valid across a Tree move).
    Node add(Text t, Node parent = {},
             std::string_view prefix = {}, TextStyle prefix_st = {}) {
        texts_.push_back(std::move(t));
        return Node(sc_tree_add_text(
            p_, parent.get(), texts_.back().get(),
            prefix.empty() ? nullptr : detail::z(prefix).c_str(), prefix_st));
    }
    Node add_markup(std::string_view m, Node parent = {},
                    std::string_view prefix = {}, TextStyle prefix_st = {}) {
        return add(Text::markup(m), parent, prefix, prefix_st);
    }
    void print() const { sc_tree_print(p_); }

    ScTree*       get()       { return p_; }
    const ScTree* get() const { return p_; }

private:
    ScTree*               p_;
    std::deque<Text>      texts_;   // owns ScText borrowed by the C tree
};

// ── Key/Value list (owns ScKV*; copies its strings → no lifetime worries) ────
class Kv {
public:
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

    Kv& add(std::string_view key, std::string_view value) {
        sc_kv_add(p_, detail::z(key).c_str(), detail::z(value).c_str());
        return *this;
    }
    void print() const { sc_kv_print(p_); }

    ScKV*       get()       { return p_; }
    const ScKV* get() const { return p_; }

private:
    ScKV* p_;
};

// ── Columns (owns ScColumns*; captures each added widget eagerly) ────────────
class Columns {
public:
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

    Columns& add(const Table& t, TableOpts o = {}, ColItem it = {}) {
        sc_columns_add_table(p_, t.get(), o, it); return *this;
    }
    Columns& add_panel(std::string_view content, PanelOpts o = {}, ColItem it = {}) {
        sc_columns_add_panel_str(p_, detail::z(content).c_str(), o, it); return *this;
    }
    Columns& add_panel(const Text& content, PanelOpts o = {}, ColItem it = {}) {
        sc_columns_add_panel_text(p_, content.get(), o, it); return *this;
    }
    Columns& add(const Text& t, ColItem it = {}) {
        sc_columns_add_text(p_, t.get(), it); return *this;
    }
    Columns& add(std::string_view s, ColItem it = {}) {
        sc_columns_add_str(p_, detail::z(s).c_str(), it); return *this;
    }
    Columns& add(const List& l, ColItem it = {}) {
        sc_columns_add_list(p_, l.get(), it); return *this;
    }
    Columns& add(const Tree& t, ColItem it = {}) {
        sc_columns_add_tree(p_, t.get(), it); return *this;
    }
    Columns& add(const Columns& nested, ColItem it = {}) {
        sc_columns_add_columns(p_, nested.get(), it); return *this;
    }
    Columns& add(const Rendered& r, ColItem it = {}) {
        sc_columns_add_rendered(p_, r.get(), it); return *this;
    }

    void print() const { sc_columns_print(p_); }

    ScColumns*       get()       { return p_; }
    const ScColumns* get() const { return p_; }

private:
    ScColumns* p_;
};

// ── Progress bar (owns ScProgressBar*) ───────────────────────────────────────
class ProgressBar {
public:
    explicit ProgressBar(ProgressBarOpts opts = {}) : p_(sc_progressbar_new(opts)) {
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

    void set_label(std::string_view l) { sc_progressbar_set_label(p_, detail::z(l).c_str()); }
    void draw(double value, double max = 0.0)   { sc_progressbar_draw(p_, value, max); }
    void finish(double value, double max = 0.0) { sc_progressbar_finish(p_, value, max); }

    ScProgressBar* get() { return p_; }

private:
    ScProgressBar* p_;
};

// ── Spinner (owns ScSpinner*) ────────────────────────────────────────────────
class Spinner {
public:
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

    void set_label(std::string_view l) { sc_spinner_set_label(p_, detail::z(l).c_str()); }
    void tick() { sc_spinner_tick(p_); }
    void finish(bool success, std::string_view label) {
        sc_spinner_finish(p_, success, detail::z(label).c_str());
    }

    ScSpinner* get() { return p_; }

private:
    ScSpinner* p_;
};

// ── Stateless output free functions ──────────────────────────────────────────
inline void print(std::string_view s, TextStyle st = {})   { sc_print(detail::z(s).c_str(), st); }
inline void println(std::string_view s, TextStyle st = {}) { sc_println(detail::z(s).c_str(), st); }

inline void panel(std::string_view content, PanelOpts opts = {}) {
    sc_panel_str(detail::z(content).c_str(), opts);
}
inline void panel(const Text& content, PanelOpts opts = {}) {
    sc_panel_text(content.get(), opts);
}
inline void rule(std::string_view title, RuleOpts opts = {}) {
    sc_rule_str(detail::z(title).c_str(), opts);
}
inline void rule(RuleOpts opts = {})           { sc_rule_str(nullptr, opts); }
inline void rule(const Text& title, RuleOpts opts = {}) {
    sc_rule_text(title.get(), opts);
}
inline void badge(std::string_view text, BadgeOpts opts = {}) {
    sc_print_badge(detail::z(text).c_str(), opts);
}

namespace alert {
inline void info   (std::string_view c) { sc_alert_info(detail::z(c).c_str()); }
inline void debug  (std::string_view c) { sc_alert_debug(detail::z(c).c_str()); }
inline void warning(std::string_view c) { sc_alert_warning(detail::z(c).c_str()); }
inline void error  (std::string_view c) { sc_alert_error(detail::z(c).c_str()); }
inline void success(std::string_view c) { sc_alert_success(detail::z(c).c_str()); }
inline void show(AlertType t, std::string_view c) { sc_alert_str(t, detail::z(c).c_str()); }
}  // namespace alert

namespace markup {
inline Text parse(std::string_view m)              { return Text::markup(m); }
inline Text parse(std::string_view m, MarkupOpts o){ return Text::markup(m, o); }
inline void print(std::string_view m)   { sc_markup_print(detail::z(m).c_str()); }
inline void println(std::string_view m) { sc_markup_println(detail::z(m).c_str()); }
inline void println(std::string_view m, MarkupOpts o) {
    sc_markup_println_opts(detail::z(m).c_str(), o);
}
}  // namespace markup

// ── Capture / compose ────────────────────────────────────────────────────────
namespace capture {
inline Rendered str(std::string_view s)   { return Rendered::adopt(sc_capture_str(detail::z(s).c_str())); }
inline Rendered text(const Text& t)       { return Rendered::adopt(sc_capture_text(t.get())); }
inline Rendered table(const Table& t, TableOpts o = {}) { return Rendered::adopt(sc_capture_table(t.get(), o)); }
inline Rendered list(const List& l)       { return Rendered::adopt(sc_capture_list(l.get())); }
inline Rendered tree(const Tree& t)       { return Rendered::adopt(sc_capture_tree(t.get())); }
inline Rendered kv(const Kv& k)           { return Rendered::adopt(sc_capture_kv(k.get())); }
inline Rendered columns(const Columns& c) { return Rendered::adopt(sc_capture_columns(c.get())); }
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

// Stack several captured renderings into one column (gap blank lines between).
inline Rendered vstack(const std::vector<const Rendered*>& parts, int gap = 0) {
    std::vector<const ScRendered*> raw;
    raw.reserve(parts.size());
    for (const Rendered* r : parts) raw.push_back(r->get());
    return Rendered::adopt(sc_vstack(raw.data(), raw.size(), gap));
}

// pad / align convenience over a string.
inline void pad(std::string_view s, PadOpts opts)        { sc_pad_str(detail::z(s).c_str(), opts); }
inline void align(std::string_view s, HAlign a, int w = 0) { sc_align_str(detail::z(s).c_str(), a, w); }

// ── Utilities (return owned std::string) ─────────────────────────────────────
inline std::string strip_ansi(std::string_view s) {
    char* c = sc_strip_ansi(detail::z(s).c_str());
    std::string r(c ? c : "");
    std::free(c);
    return r;
}
inline std::string truncate(std::string_view s, int max_cols,
                            std::string_view ellipsis = "…") {
    char* c = sc_truncate(detail::z(s).c_str(), max_cols, detail::z(ellipsis).c_str());
    std::string r(c ? c : "");
    std::free(c);
    return r;
}
inline void clear_line() { sc_clear_line(); }

// Redirect the (thread-local) output stream. `set_output(nullptr)` restores
// stdout. Prefer ScopedOutput, which restores the previous stream on scope exit.
inline void set_output(std::FILE* out) { sc_output_set_stream(out); }
inline std::FILE* output_stream()       { return sc_output_stream(); }

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

// ── Input widgets (return std::optional; nullopt = cancelled or no TTY) ──────
inline bool input_available() { return sc_input_available(); }

inline std::optional<bool> confirm(std::string_view question, ConfirmOpts opts = {}) {
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

inline std::optional<std::string> text_input(std::string_view prompt, TextInputOpts opts = {}) {
    char* out = nullptr;
    return detail::take(out, sc_text_input(detail::z(prompt).c_str(), &out, opts));
}
inline std::optional<std::string> password_input(std::string_view prompt, PasswordOpts opts = {}) {
    char* out = nullptr;
    return detail::take(out, sc_password_input(detail::z(prompt).c_str(), &out, opts));
}
inline std::optional<std::string> textarea(std::string_view prompt, TextareaOpts opts = {}) {
    char* out = nullptr;
    return detail::take(out, sc_textarea(detail::z(prompt).c_str(), &out, opts));
}
inline std::optional<double> number_input(std::string_view prompt, NumberOpts opts = {}) {
    double out = 0.0;
    if (sc_number_input(detail::z(prompt).c_str(), &out, opts) == SC_INPUT_OK)
        return out;
    return std::nullopt;
}
inline std::optional<std::tm> datepicker(std::tm seed = {}, DatePickerOpts opts = {}) {
    if (sc_datepicker(&seed, opts) == SC_INPUT_OK) return seed;
    return std::nullopt;
}

inline bool fuzzy_match(std::string_view pattern, std::string_view str, int* score = nullptr) {
    return sc_fuzzy_match(detail::z(pattern).c_str(), detail::z(str).c_str(), score);
}

inline void set_theme(const InputTheme& theme) { sc_input_set_theme(&theme); }
inline void reset_theme()                       { sc_input_set_theme(nullptr); }
inline InputTheme theme()                        { return sc_input_theme(); }

// Single/multi selection (owns ScSelect*).
class Select {
public:
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

    Select& add(std::string_view label) {
        sc_select_add(p_, detail::z(label).c_str()); ++count_; return *this;
    }
    void set_cursor(std::size_t index)               { sc_select_set_cursor(p_, index); }
    void set_checked(std::size_t index, bool on = true) { sc_select_set_checked(p_, index, on); }

    // Multi-select: chosen indices in display order. Single-select: one index.
    std::optional<std::vector<std::size_t>> run() {
        std::vector<std::size_t> idx(count_ ? count_ : 1);
        std::size_t n = idx.size();
        if (sc_select_run(p_, idx.data(), &n) != SC_INPUT_OK) return std::nullopt;
        idx.resize(n);
        return idx;
    }
    // Convenience for single-select.
    std::optional<std::size_t> run_one() {
        auto r = run();
        if (r && !r->empty()) return (*r)[0];
        return std::nullopt;
    }

    ScSelect* get() { return p_; }

private:
    ScSelect*   p_;
    std::size_t count_ = 0;
};

// Fuzzy finder (owns ScFuzzy*).
class Fuzzy {
public:
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

    Fuzzy& add(std::string_view label) {
        sc_fuzzy_add(p_, detail::z(label).c_str()); return *this;
    }
    Fuzzy& add_row(const std::vector<std::string>& fields) {
        std::vector<const char*> ptrs;
        ptrs.reserve(fields.size());
        for (const auto& f : fields) ptrs.push_back(f.c_str());
        sc_fuzzy_add_row(p_, ptrs.data(), ptrs.size());
        return *this;
    }
    std::optional<std::size_t> run() {
        std::size_t out = 0;
        if (sc_fuzzy_run(p_, &out) != SC_INPUT_OK) return std::nullopt;
        return out;
    }

    ScFuzzy* get() { return p_; }

private:
    ScFuzzy* p_;
};

}  // namespace sparcli
