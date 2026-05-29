# sparcli — C++ Wrapper Reference

A header-only, RAII C++20 layer over the [C API](api-c.md), in the `sparcli::`
namespace:

```cpp
#include <sparcli.hpp>
using namespace sparcli;
```

It is **zero-overhead** (thin inline forwarders), **move-only** owning handles
(no leaks / double-free), owns the strings/texts the C API only borrows (so
temporaries are safe), and returns `std::optional` / `std::vector` from input
prompts. The only exception thrown is `std::bad_alloc`, from a constructor when
the underlying `sc_*_new` returns NULL.

This page documents what the wrapper *adds* (ownership, return semantics,
exceptions). For the per-option meaning of the `*Opts` structs it reuses, see
the [C API reference](api-c.md). Every type/function below is defined in
`include/sparcli.hpp`.

## Build / link

```sh
c++ -std=c++20 app.cpp $(pkg-config --cflags --libs sparcli) -o app
```

Requires C++20 (designated initializers for the `*Opts` structs). Verified by
`tests/cpp/test_cpp.cpp` (`make test-cpp`).

## Why a wrapper? (safety)

Two easy C-API mistakes cannot happen with it:

```cpp
// C API — two footguns:
ScTableData *t = sc_table_new();                 // 1) leaks without sc_table_free
sc_table_add_row(t, (ScCell[]){                  // 2) the table BORROWS the
    sc_cell(std::to_string(n).c_str()) }, 1);    //    string; the temporary dies
sc_table_print(t, (ScTableOpts){0});             //    here → dangling read

// C++ wrapper — RAII frees, and the cell string is copied into the table:
Table t;                                         // frees itself
t.add_row({ std::to_string(n) });                // owned → temporary is safe
t.print();
```

## Types, opts & color helpers

The C structs/enums are reused verbatim via aliases, so options are written with
C++20 designated initializers:

```cpp
panel("Hi", { .border = { .type = SC_BORDER_ROUNDED },
              .title  = { .text = " Title ", .halign = SC_ALIGN_CENTER } });
```

Aliases: `Color`, `TextStyle`, `TextAttribute`, `Title`, `Edges`, `BorderStyle`,
`BorderType`, `HAlign`, `VAlign`, `Position`, and every `*Opts`
(`PanelOpts = ScPanelOpts`, `TableOpts`, `ColOpts`, `RuleOpts`, `ColumnsOpts`,
`ColItem`, `ListOpts`, `TreeOpts`, `KVOpts`, `BadgeOpts`, `ProgressBarOpts`,
`SpinnerOpts`, `PadOpts`, `MarkupOpts`, `ConfirmOpts`, `TextInputOpts`,
`PasswordOpts`, `NumberOpts`, `TextareaOpts`, `SelectOpts`, `FuzzyOpts`,
`DatePickerOpts`, `InputTheme`, `AlertType`, `InputStatus`).

Colors use functions, not the `SC_ANSI_COLOR_*` compound-literal macros (which
are non-standard in C++):

```cpp
Color c1 = red();                 // none/black/red/green/yellow/blue/
Color c2 = rgb(120, 200, 255);    // magenta/cyan/white, plus rgb(r,g,b)
TextStyle s = style(SC_TEXT_ATTR_BOLD, green());   // attr, fg, bg
```

All `ScColor`/`ScTextStyle` enum/macro constants (`SC_TEXT_ATTR_*`,
`SC_BORDER_*`, `SC_ALIGN_*`, …) are plain enums and work as-is.

## RAII handles

All are move-only (non-copyable); the destructor frees the underlying `sc_*`
object. Each exposes `.get()` for the raw C handle (escape hatch). Constructors
throw `std::bad_alloc` on allocation failure.

### Text — rich multi-span text (`ScText`)

```cpp
Text t;                              // empty
Text t2("plain");                    // one unstyled span
Text t3 = Text::markup("[bold]hi[/]");
t.append("x", style(SC_TEXT_ATTR_DIM))
 .append_markup("[red]y[/]")
 .append_badge("v1");
t.print();                           // → current output stream
t.visible_width(); t.span_count();
```

All `append*` calls copy their input.

### Table — `ScTableData` (owns its cell strings)

The C table borrows cell strings, so the wrapper copies them into an internal
arena that lives as long as the table — `std::string` temporaries are safe, and
the table stays valid across a move.

```cpp
Table t;
t.add_column("Name", { .halign = SC_ALIGN_LEFT });
t.add_row({ "Ada", std::to_string(42) });          // string-like cells
t.add_row({ cell_markup("[green]OK[/]"),            // markup cell
            cell("100").align(SC_ALIGN_RIGHT) });   // per-cell options
t.add_row({ "x", "y" }, rgb(40,40,60));             // per-row background
t.add_footer_row({ "total", "142" });
t.print({ .header = { .row = true } });
```

A `Cell` is implicitly constructible from any string-like; chain `.align(h)`,
`.valign(v)`, `.colspan(n)`, `.rowspan(n)`. Free helpers: `cell("…")`,
`cell_markup("…")`. For a fully custom multi-span cell, drop to `t.get()` +
`sc_cell_from_text` (managing the `Text` lifetime yourself).

### List — `ScList` (owns rich-text items)

String items are copied by the C side; rich (`Text`/markup) items are kept alive
in a shared arena that outlives the root list, so sub-list rich items are safe
too.

```cpp
List l({ .marker = SC_LIST_NUMBER });
l.add("first");
auto item = l.add("second");
{ auto sub = item.sub({ .marker = SC_LIST_ALPHA_LC });
  sub.add("nested"); sub.add_markup("[cyan]rich[/]"); }
l.add_markup("[bold]rich root[/]");
l.print();
```

`add(...)` returns a non-owning `Item`; `Item::sub(opts)` nests a list.

### Tree — `ScTree` (owns rich-text nodes)

```cpp
Tree tr;
auto root = tr.add("project");
tr.add("src", root);
tr.add_markup("[dim]README.md[/]", root);
tr.print();
```

`add(...)` / `add_markup(...)` return a non-owning `Node`; pass it as the
`parent` of a child (default `Node{}` = root). String/prefix args are copied;
`Text` nodes are owned by the tree.

### Kv — `ScKV`

```cpp
Kv kv;
kv.add("Version", sc_version_string());   // both strings copied
kv.print();
```

### Columns — `ScColumns` (captures eagerly)

Each `add*` captures the widget's rendering immediately, so the source may be
modified or destroyed afterwards.

```cpp
Columns cols({ .gap = 3 });
cols.add(table, { .header = { .row = true } });
cols.add_panel("side", { .border = { .type = SC_BORDER_SINGLE } });
cols.add(rendered);                       // a captured Rendered
cols.print();
```

Overloads: `add(Table, TableOpts, ColItem)`, `add_panel(string|Text, …)`,
`add(Text|string|List|Tree|Columns|Rendered, ColItem)`.

### ProgressBar / Spinner — `ScProgressBar` / `ScSpinner`

```cpp
ProgressBar bar({ .show_percent = true });
bar.set_label("Installing");
for (int v = 0; v <= 100; ++v) { bar.draw(v, 100); /* sleep */ }
bar.finish(100, 100);

Spinner sp("Loading");
for (int i = 0; i < 30; ++i) { sp.tick(); /* sleep */ }
sp.finish(true, "Done");
```

`draw(value, max)`: `max == 0` → `value` is a 0..1 ratio.

## Stateless output

```cpp
print("x", style(SC_TEXT_ATTR_DIM));   println("y");
panel("body", opts);                   panel(text, opts);
rule("title", opts);  rule(opts);      rule(text, opts);
badge("DONE", { .pad = 1 });

alert::info("…");  alert::debug/warning/error/success("…");
alert::show(SC_ALERT_INFO, "…");

markup::println("[bold red]Error[/]");  markup::print("…");
Text t = markup::parse("[green]ok[/]");
```

## Capture & compose

```cpp
Rendered r  = capture::table(t, opts);   // also: str/text/list/tree/kv/
Rendered r2 = capture::panel("x", opts); //        columns/panel/rule
Rendered s  = vstack({ &r, &r2 }, 1);    // stack into one column (gap lines)
r.pad({ .left = 4 });                     // print with padding
r.align(SC_ALIGN_CENTER);                 // print aligned (0 = terminal width)

pad("text", { .top = 1 });  align("text", SC_ALIGN_RIGHT, 40);

std::string plain = strip_ansi(colored);
std::string cut   = truncate("long…", 12);
clear_line();
```

`ScopedOutput` redirects the (thread-local) output stream for a scope and
restores it on exit:

```cpp
{ ScopedOutput to(fp); println("goes to fp"); }   // restored here
set_output(fp);  set_output(nullptr);  // manual; nullptr = stdout
```

## Input widgets

Each returns `std::optional` / `std::vector`; the result is empty
(`std::nullopt`) when the user cancels (Esc / Ctrl-C) or no interactive terminal
is available. `input_available()` reports whether a prompt can run.

```cpp
if (auto ok   = confirm("Proceed?")) { /* *ok is bool */ }
if (auto name = text_input("Name", { .placeholder = "Ada" })) { /* *name */ }
if (auto pw   = password_input("Password")) { /* *pw */ }
if (auto note = textarea("Notes")) { /* *note (Ctrl-D submits) */ }
if (auto qty  = number_input("Qty", { .min = 0, .max = 100, .step = 5 })) { }
if (auto date = datepicker({}, { .prompt = "Pick a date" })) { /* std::tm */ }

Select sel({ .prompt = "Pick", .multi = true });
sel.add("a").add("b").add("c");
sel.set_checked(0);
if (auto idx = sel.run()) { /* std::vector<size_t> in display order */ }
// single-select convenience: sel.run_one() → std::optional<size_t>

Fuzzy fz({ .prompt = "Find" });
fz.add("Tokyo").add("London");
if (auto i = fz.run()) { /* add-order index */ }

bool m = fuzzy_match("to", "Tokyo");      // pure, no TTY
set_theme({ .accent = magenta() });  reset_theme();   // InputTheme theme();
```

Text/number input constraints reuse the C filter functions via the opts
(`.char_filter = sc_filter_digits`, etc.; see [api-c.md](api-c.md#input-widgets)).

## Escape hatch

Every handle exposes `.get()` returning the raw `Sc*` pointer, so you can always
mix wrapper and C calls or reach functionality the wrapper doesn't surface:

```cpp
Table t;
sc_table_add_row(t.get(), cells, n);   // raw C on a wrapper-owned handle
```

See the [C API reference](api-c.md) for the underlying functions and the exact
option fields.
