#pragma once

/**
 * @file sparcli_view.h
 * @brief Umbrella for the view layer: render serde data models to the
 *        terminal.
 *
 * The view layer is the bridge between the data layer (`serde`, `ScValue` /
 * `ScMarkdown`) and the output widgets. It depends on **both**, which is why
 * it lives above them and - like serde - is **not** part of the `sparcli.h`
 * umbrella. Include this header explicitly.
 *
 * It provides:
 *  - `sc_value_render*`    - colored, jq-style pretty-printing of `ScValue`.
 *  - `sc_markdown_render*` - Markdown rendered through the widget stack
 *                            (headings, lists, code blocks, tables, …).
 */

#include "view/sparcli_value_render.h"
#include "view/sparcli_markdown_render.h"
