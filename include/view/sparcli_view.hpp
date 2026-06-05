#pragma once

/**
 * @file sparcli_view.hpp
 * @brief C++ wrapper for the view layer (`view/sparcli_view.h`): render serde
 *        data models to the terminal.
 *
 * Opt-in like serde (depends on the `ScValue`/`ScMarkdown` models), so it is
 * not part of `<sparcli.hpp>`. Include it explicitly. Requires C++20.
 */

#include "view/sparcli_view.h"
#include "serde/sparcli_serde.hpp"
#include <sparcli.hpp>            // sparcli::Text, sparcli::Rendered

#include <string>
#include <string_view>


namespace sparcli::view {

using ValueRenderOpts    = ScValueRenderOpts;
using MarkdownRenderOpts = ScMarkdownRenderOpts;

/* ── ScValue pretty-print ─────────────────────────────────────────────────── */

/** jq-style colored rendering of a value as rich text. @see sc_value_render_text */
inline Text value_render_text(const serde::Value& v, ValueRenderOpts o = {}) {
    return Text::adopt(sc_value_render_text(v.get(), o));
}
inline Text value_render_text(const serde::View& v, ValueRenderOpts o = {}) {
    return Text::adopt(sc_value_render_text(v.get(), o));
}
/** Prints a value to the current output stream. @see sc_value_render */
inline void value_render(const serde::Value& v, ValueRenderOpts o = {}) {
    sc_value_render(v.get(), o);
}
inline void value_render(const serde::View& v, ValueRenderOpts o = {}) {
    sc_value_render(v.get(), o);
}
/** Captures a value rendering. @see sc_capture_value */
inline Rendered capture_value(const serde::Value& v, ValueRenderOpts o = {}) {
    return Rendered::adopt(sc_capture_value(v.get(), o));
}
inline Rendered capture_value(const serde::View& v, ValueRenderOpts o = {}) {
    return Rendered::adopt(sc_capture_value(v.get(), o));
}

/* ── Markdown render ──────────────────────────────────────────────────────── */

/** Renders a parsed Markdown document. @see sc_markdown_render */
inline void markdown_render(const serde::Markdown& md,
                            MarkdownRenderOpts o = {}) {
    sc_markdown_render(md.get(), o);
}
/** Captures a Markdown rendering. @see sc_capture_markdown */
inline Rendered capture_markdown(const serde::Markdown& md,
                                 MarkdownRenderOpts o = {}) {
    return Rendered::adopt(sc_capture_markdown(md.get(), o));
}
/** Parses + renders a Markdown string in one call. @see sc_markdown_render_str */
inline void markdown_render_str(std::string_view src,
                                MarkdownRenderOpts o = {}) {
    sc_markdown_render_str(std::string(src).c_str(), o);
}

}  // namespace sparcli::view
