// cpp_demo.cpp — exercises the C++20 wrapper (include/sparcli.hpp).
//
// Build (after `make`):
//   c++ -std=c++20 -Iinclude examples/cpp_demo.cpp libsparcli.a -o cpp_demo
//
// The output widgets run headless; the input widgets are only reached when a
// real terminal is present.

#include <sparcli.hpp>

#include <string>

using namespace sparcli;

int main() {
    // Markup + styled print.
    markup::println("[bold green]sparcli[/] — C++ wrapper demo");

    // Panel (designated-initializer opts, C++20).
    panel("All systems operational.",
          { .border = { .type = SC_BORDER_ROUNDED, .color = cyan() },
            .title  = { .text = " Status ", .halign = SC_ALIGN_CENTER },
            .padding = { 0, 2, 0, 2 } });

    // Table: strings are OWNED by the wrapper, so temporaries are safe.
    Table t;
    t.add_column("Name", { .halign = SC_ALIGN_LEFT });
    t.add_column("Score", { .halign = SC_ALIGN_RIGHT });
    for (int i = 1; i <= 3; ++i) {
        t.add_row({ "row " + std::to_string(i), std::to_string(i * 10) });
    }
    t.add_row({ cell_markup("[green]✔ ok[/]"), cell("100").align(SC_ALIGN_RIGHT) });
    t.print({ .header = { .row = true } });

    // Rule.
    rule("Lists & trees", { .type = SC_BORDER_SINGLE });

    // List with a sub-list.
    List l({ .marker = SC_LIST_NUMBER });
    l.add("First");
    auto item = l.add("Second");
    { auto sub = item.sub({ .marker = SC_LIST_ALPHA_LC }); sub.add("nested a"); sub.add("nested b"); }
    l.add("Third");
    l.print();

    // Tree.
    Tree tree;
    auto root = tree.add("project");
    tree.add("src", root);
    tree.add("include", root);
    tree.print();

    // Key/value.
    Kv kv;
    kv.add("Version", sc_version_string());
    kv.add("Build", "ok");
    kv.print();

    // Columns composing two panels side by side.
    Columns cols({ .gap = 3 });
    cols.add_panel("left",  { .border = { .type = SC_BORDER_SINGLE } });
    cols.add_panel("right", { .border = { .type = SC_BORDER_SINGLE } });
    cols.print();

    // Capture + vstack + align.
    Rendered a = capture::str("captured A");
    Rendered b = capture::str("captured B");
    Rendered stacked = vstack({ &a, &b }, 1);
    stacked.align(SC_ALIGN_CENTER);

    // Utilities.
    println(truncate("a very long line that will be shortened", 12));

    // Input widgets compile and are reachable behind a TTY check.
    if (input_available()) {
        if (auto name = text_input("Your name", { .placeholder = "Ada" }))
            markup::println("[green]Hi[/] " + *name);

        if (auto yes = confirm("Continue?"); yes && *yes)
            println("continuing…");

        Select sel({ .prompt = "Pick one", .multi = false });
        sel.add("Alpha").add("Beta").add("Gamma");
        if (auto picked = sel.run_one())
            println("picked index " + std::to_string(*picked));
    }
    return 0;
}
