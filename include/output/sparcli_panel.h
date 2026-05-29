#pragma once

#include "core/sparcli_core.h"
#include "core/sparcli_text.h"

SPARCLI_BEGIN_DECLS


/**
 * ScPanelOpts Struct - Layout and visual options for a bordered content panel.
 *
 * Zero-initialize to get sensible defaults: no border, no background, no title,
 * auto width, no padding, and no margin.
 *
 * @note `full_width` overrides `width` when both are set; the panel width is
 * then `terminal_width - 2` (one column margin each side).
 *
 * @note `bg` uses the same zero-init sentinel as `ScProgressBarOpts.fill_color`:
 * `{0,0,0,0}` (index==0 AND r==g==b==0 together) means "not set".
 *
 * @code
 * ScPanelOpts opts = {
 *     .border  = { SC_BORDER_ROUNDED, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE },
 *     .title   = {
 *         .text = "Status", .align = SC_ALIGN_CENTER, .pad = 1,
 *         .pos  = SC_POSITION_TOP
 *     },
 *     .padding = { .top = 1, .right = 2, .bottom = 1, .left = 2 },
 *     .full_width    = true,
 *     .content_align = SC_ALIGN_LEFT,
 * };
 * sc_panel_str("All systems operational.", opts);
 * @endcode
 */
typedef struct {
    ScBorderStyle border; /**< Border tyoe, foreground and background color */
    ScColor  bg;          /**< Content area background;
                               zero-init ({0,0,0,0}) = no color */
    ScTitle  title;       /**< Title bar: text, style, alignment, padding and
                               position */
    bool     full_width;  /**< When true, stretches to terminal width (overrides
                               width); computed as terminal_width - 2 */
    int      width;          /**< Panel width; 0 = auto-fit to content */
    ScHAlign content_align;  /**< Horizontal alignment of content lines */
    ScEdges  padding;        /**< Inner spacing (top/right/bottom/left) */
    ScEdges  margin;         /**< Outer spacing (top/right/bottom/left) */
} ScPanelOpts;


/**
 * Renders a null-terminated string inside a styled, bordered panel.
 *
 * The string may contain `\n` to produce multi-line content. Each line is
 * aligned according to `opts.content_align` and padded by `opts.padding`.
 *
 * @param content  Null-terminated string to display inside the panel.
 *                 Must not be `NULL`.
 * @param opts     Layout and visual options; see `ScPanelOpts`.
 */
SPARCLI_EXPORT void sc_panel_str (const char *content, ScPanelOpts opts);

/**
 * Renders a multi-span `ScText` object inside a styled, bordered panel.
 *
 * Use this variant when the content requires mixed styling (colors, bold, etc.)
 * across different text spans. Each line of the `ScText` is aligned and padded
 * according to the opts.
 *
 * @param content  Rich-text content to display. Must not be `NULL`.
 * @param opts     Layout and visual options; see `ScPanelOpts`.
 */
SPARCLI_EXPORT void sc_panel_text(const ScText *content, ScPanelOpts opts);

SPARCLI_END_DECLS
