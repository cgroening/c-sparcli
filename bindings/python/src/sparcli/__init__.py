"""sparcli - styled terminal output and interactive input widgets for Python.

A safe, Pythonic wrapper over the sparcli C library. Output widgets write to the
current output stream; input widgets drive a real terminal in raw mode.

    import sparcli as sc

    sc.panel("Hello", sc.PanelOpts(title="greeting", full_width=True))

    t = sc.Table()
    t.column("Name").column("Age", sc.ColOpts(halign=sc.Align.RIGHT))
    t.row(["Ada", "36"])
    t.print(sc.TableOpts(header_row=True, striped=True))

    name = sc.text_input("Your name")   # str, or None if cancelled

The raw cffi objects are available as ``sparcli.sys`` (``ffi``/``lib``) for
anything the wrapper does not surface.
"""

from __future__ import annotations

from . import _sparcli_cffi as sys  # raw FFI escape hatch (ffi, lib)
from . import capture, markup
from .app import (ErrorReport, Pager, app_dir, app_file, cache_dir,
                  config_dir, data_dir, state_dir)
from .capture import (PadOpts, Rendered, align_str, align_text, capture,
                      pad_str, pad_text, vstack)
from .color import Color
from .columns import ColItem, Columns, ColumnsOpts
from .enums import (Align, AnsiMode, Attr, AlertType, BorderType, HintLayout,
                    HintPos, ListMarker, LogLevel, PathKind, Position,
                    ProgressType, SpinnerType, SuggestMatch, SuggestMode,
                    VAlign, WeekStart)
from .log import (Logger, log_add_file, log_debug, log_error,
                  log_hide_timestamps, log_info, log_level, log_reset,
                  log_set_level, log_warning)
from .errors import SparcliError, SparcliInputUnavailable
from .input import (ConfirmOpts, DatePickerOpts, NumberOpts, PasswordOpts,
                    SuggestOpts, TextInputOpts, TextareaOpts, confirm,
                    datepicker, decimal_input, filter_alnum, filter_alpha,
                    filter_decimal, filter_digits, filter_no_space,
                    input_available, number_input, password_input, text_input,
                    textarea)
from .keys import KeyChord, Shortcuts, key_alt, key_ctrl, key_fn
from .output import (BadgeOpts, PanelOpts, RuleOpts, ScopedOutput, alert,
                     allow_ansi, badge, clear_line, panel, print_, println,
                     rule, set_allow_ansi, strip_ansi, truncate, version,
                     version_string)
from .progress import (ProgressBar, ProgressBarOpts, Spinner, SpinnerOpts,
                       Thresholds)
from .select import (Fuzzy, FuzzyOpts, Select, SelectOpts, fuzzy_match)
from .structures import (Kv, KvOpts, List, ListItem, ListOpts, Tree, TreeNode,
                         TreeOpts)
from .style import BorderStyle, Edges, Style, Title
from .table import (Cell, ColOpts, Table, TableBorder, TableOpts)
from .text import Text
from .theme import Theme, set_theme

__version__ = version_string()

__all__ = [
    # raw FFI
    "sys",
    # core values
    "Color", "Style", "Attr", "BorderStyle", "Edges", "Title", "Text",
    # enums
    "Align", "VAlign", "BorderType", "Position", "ListMarker", "ProgressType",
    "SpinnerType", "AlertType", "HintLayout", "HintPos", "WeekStart",
    "SuggestMode", "SuggestMatch", "AnsiMode", "PathKind",
    # errors
    "SparcliError", "SparcliInputUnavailable",
    # app helpers (XDG paths, pager, pretty errors)
    "app_dir", "config_dir", "data_dir", "cache_dir", "state_dir", "app_file",
    "Pager", "ErrorReport",
    # logging
    "Logger", "LogLevel", "log_set_level", "log_level", "log_add_file",
    "log_hide_timestamps", "log_reset", "log_debug", "log_info", "log_warning",
    "log_error",
    # simple output
    "print_", "println", "panel", "PanelOpts", "rule", "RuleOpts", "badge",
    "BadgeOpts", "alert", "version", "version_string", "strip_ansi", "truncate",
    "clear_line", "ScopedOutput", "set_allow_ansi", "allow_ansi",
    # markup
    "markup",
    # tables
    "Table", "Cell", "ColOpts", "TableOpts", "TableBorder",
    # structures
    "List", "ListItem", "ListOpts", "Tree", "TreeNode", "TreeOpts", "Kv",
    "KvOpts",
    # columns
    "Columns", "ColumnsOpts", "ColItem",
    # capture / compose
    "capture", "Rendered", "PadOpts", "vstack", "pad_str", "pad_text",
    "align_str", "align_text",
    # progress
    "ProgressBar", "ProgressBarOpts", "Thresholds", "Spinner", "SpinnerOpts",
    # input
    "input_available", "confirm", "ConfirmOpts", "text_input", "TextInputOpts",
    "SuggestOpts",
    "password_input", "PasswordOpts", "number_input", "decimal_input",
    "NumberOpts", "textarea", "TextareaOpts", "datepicker", "DatePickerOpts",
    "Select", "SelectOpts", "Fuzzy", "FuzzyOpts", "fuzzy_match",
    "filter_digits", "filter_decimal", "filter_alpha", "filter_alnum",
    "filter_no_space",
    # shortcuts / keys / theme
    "Shortcuts", "KeyChord", "key_ctrl", "key_fn", "key_alt", "Theme",
    "set_theme",
]
