// test_cpp.cpp – assertion suite for the C++ wrapper (include/sparcli.hpp).
//
// Unlike a smoke run, this verifies the wrapper's value-adds: that borrowed
// strings/texts are actually owned (temporaries stay valid), that a Table
// survives a move after rows were added, that List/Tree rich items outlive the
// temporaries they were built from, and that the wrapper renders byte-for-byte
// like the C API. Output is captured into an in-memory stream via ScopedOutput.
//
// Built and run by `make test-cpp`.

#include <sparcli.hpp>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

using namespace sparcli;

static int g_failures = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) {                                                            \
            std::printf("  \033[32m✔\033[0m %s\n", (msg));                \
        } else {                                                               \
            std::printf("  \033[31m✘\033[0m %s\n", (msg));                \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// Captures everything `fn` prints into a string (off a TTY → width 80).
template <class F>
static std::string render(F&& fn) {
    char*  buf = nullptr;
    size_t len = 0;
    FILE*  fp  = open_memstream(&buf, &len);
    if (!fp) { return {}; }
    { ScopedOutput redirect(fp); fn(); }
    std::fflush(fp);
    std::fclose(fp);
    std::string out(buf, len);
    std::free(buf);
    return out;
}

static bool contains(const std::string& haystack, const std::string& needle) {
    return strip_ansi(haystack).find(needle) != std::string::npos;
}

// A Table built entirely from temporaries must copy them into its arena;
// a borrow bug would leave dangling pointers and garbled/empty output.
static void test_table_owns_temporaries() {
    std::string out = render([] {
        Table t;
        t.add_column("Name");
        t.add_column("Score");
        for (int i = 1; i <= 3; ++i) {
            t.add_row({ "row " + std::to_string(i), std::to_string(i * 10) });
        }
        t.add_row({ cell_markup("[green]OK[/]"), cell("100") });
        t.print({ .header = { .row = true } });
    });
    CHECK(contains(out, "row 3"), "table: temporary cell string survives");
    CHECK(contains(out, "30"),    "table: temporary numeric cell survives");
    CHECK(contains(out, "OK"),    "table: markup cell renders");
}

// A null `const char*` cell must not crash (treated as empty), not std::string
// from nullptr (UB).
static void test_table_null_cell() {
    std::string out = render([] {
        Table t;
        t.add_column("A");
        t.add_column("B");
        t.add_row({ static_cast<const char*>(nullptr), "ok" });
        t.print();
    });
    CHECK(contains(out, "ok"), "table: null const char* cell is safe (empty)");
}

// Moving a Table after rows were added must keep the borrowed cell pointers
// valid (the string arena lives behind a unique_ptr).
static void test_table_survives_move() {
    auto build = [] {
        Table t;
        t.add_column("k");
        t.add_column("v");
        t.add_row({ std::string("alpha"), std::to_string(1) });
        t.add_row({ std::string("beta"),  std::to_string(2) });
        return t;
    };
    Table moved  = build();
    Table moved2 = std::move(moved);
    std::string out = render([&] { moved2.print(); });
    CHECK(contains(out, "alpha") && contains(out, "beta"),
          "table: cell strings survive a move");
}

// The C list borrows ScText; rich items built from temporaries (also in a
// sub-list whose view has already gone) must still render at print time.
static void test_list_text_arena() {
    std::string out = render([] {
        List l({ .marker = SC_LIST_NUMBER });
        l.add("plain");
        {
            auto item = l.add("parent");
            auto sub  = item.sub();
            sub.add_markup("[cyan]" + std::string("nested-rich") + "[/]");
        }
        l.add_markup("[bold]" + std::string("rich-root") + "[/]");
        l.print();
    });
    CHECK(contains(out, "nested-rich"), "list: sub-list rich item survives");
    CHECK(contains(out, "rich-root"),   "list: root rich item survives");
}

// The C tree borrows ScText; a markup node built from a temporary must survive.
static void test_tree_text_arena() {
    std::string out = render([] {
        Tree tr;
        auto root = tr.add("root");
        tr.add_markup("[green]" + std::string("leaf-rich") + "[/]", root);
        tr.print();
    });
    CHECK(contains(out, "leaf-rich"), "tree: rich node survives arena");
}

// The wrapper must forward to the C API byte-for-byte.
static void test_wrapper_matches_c() {
    std::string w = render([] {
        panel("hello", { .border = { .type = SC_BORDER_ROUNDED } });
    });
    std::string c = render([] {
        sc_panel_str(
            "hello", ScPanelOpts{ .border = { .type = SC_BORDER_ROUNDED } });
    });
    CHECK(w == c && !w.empty(), "wrapper panel output equals the C API");

    std::string wt = render([] {
        Text t;
        t.append("a");
        t.append_markup("[red]b[/]");
        t.print();
    });
    CHECK(contains(wt, "a") && contains(wt, "b"), "Text builder renders spans");
}

int main() {
    std::printf("\nC++ wrapper assertion suite:\n");
    test_table_owns_temporaries();
    test_table_null_cell();
    test_table_survives_move();
    test_list_text_arena();
    test_tree_text_arena();
    test_wrapper_matches_c();

    if (g_failures > 0) {
        std::printf("\033[31m%d check(s) failed.\033[0m\n", g_failures);
        return 1;
    }
    std::printf("\033[32mAll C++ wrapper checks passed.\033[0m\n");
    return 0;
}
