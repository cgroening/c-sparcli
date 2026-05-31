#!/usr/bin/env python3
"""Complete showcase of the Python wrapper: every output component and every
input widget.

    python examples/demo.py        # (with PYTHONPATH=src, or after install)

The output section runs anywhere; the input section is skipped when there is no
interactive terminal (piped / CI).
"""

from __future__ import annotations

import time

import sparcli as sc


def heading(title: str) -> None:
    sc.println("")
    sc.rule(title, sc.RuleOpts(type=sc.BorderType.DOUBLE,
                               title_align=sc.Align.LEFT))
    sc.println("")


# ── Output ────────────────────────────────────────────────────────────────────
def output_section() -> None:
    heading("Text & markup")
    sc.print_("attributes: ")
    sc.print_("bold ", sc.Style.bold())
    sc.print_("dim ", sc.Style.dim())
    sc.print_("italic ", sc.Style.italic())
    sc.print_("under ", sc.Style(attr=sc.Attr.UNDERLINE))
    sc.println("rgb", sc.Style(fg=sc.Color.rgb(120, 200, 255)))
    sc.markup.println(
        "[bold red]Error:[/] not found  [on cyan] 200 OK [/]  "
        "[rgb(255,170,0)]warn[/]")
    t = sc.Text()
    t.append("multi-").append("span ", sc.Style.bold(sc.Color.GREEN)) \
     .append("text", sc.Style.italic(sc.Color.MAGENTA))
    t.print()
    sc.println("")

    heading("Panels & alerts")
    sc.panel("auto-fit content", sc.PanelOpts(title="single",
             border=sc.BorderStyle(sc.BorderType.SINGLE)))
    sc.panel("rounded, full width, centered",
             sc.PanelOpts(title="panel",
                          border=sc.BorderStyle(sc.BorderType.ROUNDED),
                          content_align=sc.Align.CENTER, full_width=True))
    sc.panel("colored border + background",
             sc.PanelOpts(
                 title="styled",
                 border=sc.BorderStyle(
                     sc.BorderType.THICK, color=sc.Color.BLUE),
                 bg=sc.Color.rgb(20, 20, 40)))
    sc.alert.info("informational")
    sc.alert.debug("debug detail")
    sc.alert.warning("careful now")
    sc.alert.error("something failed")
    sc.alert.success("all good")

    heading("Badges & rules")
    sc.badge("NEW", sc.BadgeOpts(
        text_style=sc.Style.bold(sc.Color.BLACK, sc.Color.GREEN)))
    sc.print_("  ")
    sc.badge("v1.2", sc.BadgeOpts(
        left_cap="(", right_cap=")", text_style=sc.Style.dim()))
    sc.println("")
    sc.rule(opts=sc.RuleOpts(type=sc.BorderType.SINGLE))
    sc.rule("centered", sc.RuleOpts(type=sc.BorderType.THICK, width=40,
                                    halign=sc.Align.CENTER))

    heading("Table")
    table = sc.Table()
    table.column("Item") \
         .column("Qty", sc.ColOpts(halign=sc.Align.RIGHT)) \
         .column("Price", sc.ColOpts(halign=sc.Align.RIGHT))
    table.row(["Apples", "12", "3.40"])
    table.row(["Bananas", "5", "1.10"], bg=sc.Color.rgb(30, 30, 30))
    table.row([sc.Cell("Subtotal", colspan=2), sc.Cell.skip(),
               sc.Cell("4.50", halign=sc.Align.RIGHT)])
    table.footer_row(["Total", "17", "4.50"])
    table.print(sc.TableOpts(border=sc.BorderType.ROUNDED, header_row=True,
                             striped=True))

    heading("List, tree & key/value")
    lst = sc.List(sc.ListOpts(marker=sc.ListMarker.NUMBER))
    step = lst.add("prepare")
    sub = step.sub(sc.ListOpts(marker=sc.ListMarker.ALPHA_LOWER))
    sub.add("gather")
    sub.add("measure")
    lst.add("cook")
    lst.add("serve")

    tree = sc.Tree()
    root = tree.add("project", style=sc.Style.bold())
    src = tree.add("src", root)
    tree.add("main.py", src)
    tree.add("lib.py", src)
    tree.add("README", root, style=sc.Style.dim())

    kv = sc.Kv(sc.KvOpts(key_style=sc.Style.bold(sc.Color.CYAN)))
    kv.add("Name", "sparcli").add("Lang", "C + Python")
    kv.add("Version", sc.version_string())

    cols = sc.Columns(sc.ColumnsOpts(
        gap=2,
        sep=sc.BorderStyle(sc.BorderType.SINGLE,
                           color=sc.Color.rgb(80, 80, 100))))
    cols.add_rendered(sc.capture.list(lst)) \
        .add_rendered(sc.capture.tree(tree)) \
        .add_rendered(sc.capture.kv(kv))
    cols.print()

    heading("Capture / compose")
    single = sc.PanelOpts(border=sc.BorderStyle(sc.BorderType.SINGLE))
    a = sc.capture.panel("top", single)
    b = sc.capture.panel("bottom", single)
    stacked = sc.vstack([a, b], gap=1)
    if stacked:
        stacked.align(sc.Align.CENTER)
    sc.pad_str("padded + indented", sc.PadOpts(left=6, top=1, bottom=1))

    heading("Progress & spinner")
    bar = sc.ProgressBar(sc.ProgressBarOpts(
        type=sc.ProgressType.BLOCK, left_cap="[", right_cap="]",
        show_percent=True, fill_color=sc.Color.GREEN, width=50))
    bar.set_label("Installing")
    for v in range(101):
        bar.draw(v, 100)
        time.sleep(0.008)
    bar.finish(100, 100)

    sp = sc.Spinner("Crunching", sc.SpinnerOpts(type=sc.SpinnerType.DOTS,
                                                 color=sc.Color.CYAN))
    for _ in range(24):
        sp.tick()
        time.sleep(0.04)
    sp.finish(True, "Done")


# ── Input ────────────────────────────────────────────────────────────────────
def input_section() -> None:
    heading("Input widgets")

    yes = sc.confirm("Run the input demo?", sc.ConfirmOpts(default_yes=True))
    sc.println(f"  confirm -> {yes}", sc.Style.dim())

    # Text input: boxed, with a custom shortcut and the external editor.
    shortcuts = sc.Shortcuts().on_return(sc.key_fn(2), 1, name="help")
    name = sc.text_input(
        "Name (F2 help · Ctrl-G editor)",
        sc.TextInputOpts(placeholder="Ada Lovelace", boxed=True, width=40,
                         external_editor=True, shortcuts=shortcuts))
    if shortcuts.fired() == 1:
        sc.println("  text -> F2/help requested", sc.Style.dim())
    else:
        sc.println(f"  text -> {name!r}", sc.Style.dim())

    # Rich (partly-styled) prompt via prompt_text.
    prompt = (sc.Text().append("Rename ")
              .append("Apple", sc.Style.italic()).append(" to"))
    renamed = sc.text_input(
        "", sc.TextInputOpts(prompt_text=prompt, initial="Apple"))
    sc.println(f"  rename -> {renamed!r}", sc.Style.dim())

    pw = sc.password_input("Password", sc.PasswordOpts(mask="•"))
    if pw is not None:
        sc.println(f"  password -> {len(pw)} chars", sc.Style.dim())

    n = sc.number_input("Quantity",
                        sc.NumberOpts(min=0, max=100, step=5, initial=10))
    sc.println(f"  number -> {n}", sc.Style.dim())

    notes = sc.textarea(
        "Notes (Ctrl-D submit · Ctrl-G editor)",
        sc.TextareaOpts(boxed=True, width=48, external_editor=True))
    if notes is not None:
        sc.println(f"  textarea -> {len(notes)} bytes", sc.Style.dim())

    sel = sc.Select(sc.SelectOpts(prompt="Pick a fruit"))
    sel.add("Apple").add("Banana").add("Cherry").add("Date")
    sc.println(f"  select -> index {sel.run_one()}", sc.Style.dim())

    multi = sc.Select(sc.SelectOpts(prompt="Pick toppings", multi=True))
    multi.add("Cheese").add("Olives").add("Basil").add("Chili")
    sc.println(f"  multi-select -> {multi.run()}", sc.Style.dim())

    fz = sc.Fuzzy(sc.FuzzyOpts(prompt="Find a city"))
    for c in ("Tokyo", "London", "Berlin", "Paris", "Oslo", "Madrid", "Lisbon"):
        fz.add(c)
    sc.println(f"  fuzzy -> index {fz.run()}", sc.Style.dim())

    d = sc.datepicker(opts=sc.DatePickerOpts(prompt="Pick a date",
                                             week_start=sc.WeekStart.MONDAY))
    sc.println(f"  date -> {d}", sc.Style.dim())


def main() -> None:
    sc.println(f"sparcli {sc.version_string()} — Python wrapper demo",
               sc.Style.bold(sc.Color.CYAN))
    output_section()
    if sc.input_available():
        input_section()
    else:
        heading("Input widgets")
        sc.println("Skipped — run this in a real terminal to try the prompts.",
                   sc.Style.dim())


if __name__ == "__main__":
    main()
