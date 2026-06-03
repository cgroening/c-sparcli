// list_tree.cpp - nested lists and tree views (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/output/list_tree

#include <sparcli.hpp>

using namespace sparcli;


static void print_lists();
static void print_tree();


int main() {
    print_lists();
    println("");
    print_tree();
    return 0;
}

// Markers, nesting and rich (markup) items.
static void print_lists() {
    List steps({ .marker = SC_LIST_NUMBER,
                 .marker_style = style(SC_TEXT_ATTR_BOLD, cyan()) });
    steps.add("Check out the repository");
    auto build = steps.add("Build the project");
    steps.add("Run the test suite");

    // A sub-list is owned by the parent item.
    {
        auto sub = build.sub({ .marker = SC_LIST_ALPHA_LC, .indent = 2 });
        sub.add("make");
        sub.add("make qa");
    }
    steps.print();
    println("");

    List notes({ .marker = SC_LIST_BULLET, .bullet = "→", .width = 46 });
    notes.add("Long items are word-wrapped and continuation lines are "
              "indented under the first word of the item.");
    notes.add_markup("Items can be [bold green]rich text[/].");
    notes.print();
}

// A file-system-like tree; add() returns a Node usable as a parent.
static void print_tree() {
    Tree tree({ .type = SC_BORDER_SINGLE, .connector_color = cyan() });

    auto root = tree.add("sparcli/");
    auto src  = tree.add("src/", root);
    tree.add("core/", src);
    tree.add("output/", src);

    auto docs = tree.add("docs/", root);
    tree.add_markup("[green]examples.md[/]", docs);

    tree.add("Makefile", root);
    tree.print();
}
