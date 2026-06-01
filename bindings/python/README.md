# sparcli (Python)

Safe, Pythonic bindings for the [sparcli](../../README.md) C library – styled terminal output (panels, tables, rules, lists, trees, columns, progress bars, spinners, markup) and interactive input widgets (confirm, text/password/number/ textarea, select, fuzzy finder, date picker).

Built with **cffi** in API mode: a small C extension is compiled from the vendored sparcli C sources, so installing needs only a C compiler – no prior `make` and no system-installed library.

## Quick start

```python
import sparcli as sc

sc.panel("Hello", sc.PanelOpts(title="greeting", full_width=True))

t = sc.Table()
t.column("Name").column("Age", sc.ColOpts(halign=sc.Align.RIGHT))
t.row(["Ada", "36"]).row(["Alan", "41"])
t.print(sc.TableOpts(header_row=True, striped=True))

name = sc.text_input("Your name")    # -> str, or None if cancelled (Esc/Ctrl-C)
```

Interactive widgets return their value on success, `None` on cancel, and raise `sparcli.SparcliInputUnavailable` when there is no terminal.

## Develop & test

From the repo root:

```sh
make python         # build the extension in place (src/sparcli/_sparcli_cffi.*)
make python-test    # build + run the non-interactive pytest suite
```

`make` uses `python3`; point it at an interpreter that has `cffi` with `make python PY=/path/to/python`. To install into an environment instead:

```sh
pip install cffi setuptools wheel
pip install --no-build-isolation -e bindings/python
```

(An **editable** (`-e`) install with build isolation **off**: the C sources live in the repo, reached via the in-tree `csrc`/`cinclude` symlinks, so the build must run in place – an isolated or out-of-tree wheel build would lose them.)

## Examples

```sh
cd bindings/python
PYTHONPATH=src python examples/demo.py            # complete showcase (all widgets)
PYTHONPATH=src python examples/output_gallery.py  # output only
PYTHONPATH=src python examples/input_demo.py      # input only (needs a terminal)
```

See [`docs/api-python.md`](../../docs/api-python.md) for the full reference.
