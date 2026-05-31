#!/usr/bin/env python3
"""Output-only gallery: panels, tables, lists/trees/kv, markup, columns.

    python examples/output_gallery.py
"""

from __future__ import annotations

import sparcli as sc


def main() -> None:
    sc.rule("sparcli – output gallery", sc.RuleOpts(type=sc.BorderType.DOUBLE))
    sc.println("")

    sc.markup.println("[bold]Styled[/] text with [italic green]markup[/] and "
                      "[on blue] backgrounds [/].")
    sc.println("")

    sc.panel("Bordered, titled, full-width panel.",
             sc.PanelOpts(title="panel",
                          border=sc.BorderStyle(sc.BorderType.ROUNDED),
                          full_width=True, content_align=sc.Align.CENTER))

    sc.alert.success("Everything compiled.")
    sc.alert.warning("One deprecation.")

    table = sc.Table()
    table.column("City") \
         .column("Pop. (M)", sc.ColOpts(halign=sc.Align.RIGHT)) \
         .column("Note")
    table.row(["Tokyo", "37.4", "largest metro"])
    table.row(["Delhi", "32.9", "fast-growing"])
    table.row(["Oslo", "1.1", "northern"])
    table.print(sc.TableOpts(border=sc.BorderType.ROUNDED, header_row=True,
                             striped=True, title="Cities"))

    lst = sc.List(sc.ListOpts(marker=sc.ListMarker.NUMBER))
    lst.add("Read input")
    proc = lst.add("Process")
    sub = proc.sub(sc.ListOpts(marker=sc.ListMarker.ALPHA_LOWER))
    sub.add("parse")
    sub.add("transform")
    lst.add("Write output")

    kv = sc.Kv(sc.KvOpts(key_style=sc.Style.bold(sc.Color.CYAN)))
    kv.add("Library", "sparcli").add("Binding", "Python (cffi)") \
      .add("Version", sc.version_string())

    cols = sc.Columns(sc.ColumnsOpts(gap=4))
    cols.add_rendered(sc.capture.list(lst)).add_rendered(sc.capture.kv(kv))
    cols.print()


if __name__ == "__main__":
    main()
