// cpp_demo.cpp – exercises the C++20 wrapper (include/sparcli.hpp).
//
// Build (after `make`):
//   c++ -std=c++20 -Iinclude examples/cpp_demo.cpp libsparcli.a -o cpp_demo
// or:
//   make test-cpp           # compile + run headless (the wrapper gate)
//
// The output widgets run headless; the input widgets are only reached when a
// real terminal is present. This also doubles as the compile gate, so it
// deliberately touches most of the wrapper surface.

#include <sparcli.hpp>

#include <cstdlib>
#include <string>

using namespace sparcli;

static void output_demo() {
    // Markup + styled print.
    markup::println("[bold green]sparcli[/] – C++ wrapper demo");
    println("plain styled line", style(SC_TEXT_ATTR_DIM));

    // Alerts.
    alert::info("Header-only C++20 wrapper over the C API.");

    // Panel built from a rich Text (append / markup / badge).
    Text body;
    body.append("status: ");
    body.append_markup("[bold green]ok[/]  ");
    body.append_badge("v" + std::string(sc_version_string()));
    panel(body, { .border = { .type = SC_BORDER_ROUNDED, .color = cyan() },
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

    rule("Lists, trees & key/value", { .type = SC_BORDER_SINGLE });

    // List with a sub-list.
    List l({ .marker = SC_LIST_NUMBER });
    l.add("First");
    auto item = l.add("Second");
    { auto sub = item.sub({ .marker = SC_LIST_ALPHA_LC });
      sub.add("nested a");
      sub.add_markup("[italic]nested markup[/]"); }   // list owns the Text
    l.add_markup("[cyan]rich item[/]");               // safe: arena-owned
    l.print();

    // Tree (with a markup node – the tree borrows the text, wrapper owns it).
    Tree tree;
    auto root = tree.add("project");
    tree.add("src", root);
    tree.add_markup("[dim]README.md[/]", root);
    tree.print();

    // Key/value.
    Kv kv;
    kv.add("Version", sc_version_string());
    kv.add("Build", "ok");
    kv.print();

    // Columns: compose a captured table next to a panel.
    Rendered captured = capture::table(t, { .header = { .row = true } });
    Columns cols({ .gap = 3 });
    cols.add(captured);
    cols.add_panel("side panel", { .border = { .type = SC_BORDER_SINGLE } });
    cols.print();

    // Capture + vstack + align + pad.
    Rendered a = capture::str("captured A");
    Rendered b = capture::str("captured B");
    Rendered stacked = vstack({ &a, &b }, 1);
    stacked.pad({ .left = 4 });

    // Badge + utilities.
    badge("DONE", { .pad = 1 });
    println("");
    println(truncate("a very long line that will be shortened", 12));
    println(strip_ansi("\033[31mred\033[0m becomes plain"));

    // Progress bar (no sleeps – this is a gate, not a UI).
    ProgressBar bar({ .show_percent = true });
    bar.set_label("Installing");
    for (int v = 0; v <= 100; v += 50) bar.draw(v, 100);
    bar.finish(100, 100);

    // Spinner.
    Spinner sp("Loading");
    for (int i = 0; i < 3; ++i) sp.tick();
    sp.finish(true, "Done");

    // Pure fuzzy matcher (no TTY needed).
    int score = 0;
    if (fuzzy_match("to", "Tokyo", &score))
        markup::println("[dim]fuzzy 'to' matches 'Tokyo' (score " +
                        std::to_string(score) + ")[/]");

    // Scoped output redirection (restored automatically on scope exit).
    { ScopedOutput to(stderr); println("(diagnostics line routed to stderr)"); }
}

static void input_demo() {
    // A process-wide theme; every widget inherits it for zero-init options.
    set_theme({ .accent = magenta() });

    if (auto name = text_input("Your name", { .placeholder = "Ada" }))
        markup::println("[green]Hi[/] " + *name);

    if (auto secret = password_input("Password", { .placeholder = "••••" }))
        println("captured " + std::to_string(secret->size()) + " chars");

    if (auto qty = number_input("Quantity",
                                { .initial = 10, .min = 0, .max = 100, .step = 5 }))
        println("qty = " + std::to_string(static_cast<int>(*qty)));

    if (auto notes = textarea("Notes (Ctrl-D submits)"))
        println("notes: " + std::to_string(notes->size()) + " bytes");

    if (auto yes = confirm("Continue?"); yes && *yes)
        println("continuing…");

    Select sel({ .prompt = "Pick one" });
    sel.add("Alpha").add("Beta").add("Gamma");
    if (auto picked = sel.run_one())
        println("picked index " + std::to_string(*picked));

    Fuzzy fz({ .prompt = "Find a city" });
    fz.add("Tokyo").add("Toronto").add("London").add("Boston");
    if (auto i = fz.run())
        println("fuzzy picked index " + std::to_string(*i));

    if (auto date = datepicker({}, { .prompt = "Pick a date" }))
        println("picked year " + std::to_string(date->tm_year + 1900));

    reset_theme();
}

int main() {
    output_demo();
    // The interactive section opens /dev/tty (so it runs even under a stdin/
    // stdout redirect) and is non-deterministic, so it must not be part of the
    // golden-diffed output: the Makefile sets SPARCLI_DEMO_NONINTERACTIVE for
    // the golden run. Run it only with a terminal and when not suppressed.
    if (input_available() && !std::getenv("SPARCLI_DEMO_NONINTERACTIVE")) {
        rule("Interactive widgets", { .type = SC_BORDER_DOUBLE });
        input_demo();
    }
    return 0;
}
