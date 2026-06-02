/**
 * @file cli_parse.h
 * String-to-type parsers shared by all CLI subcommands: enum names, colors,
 * edge specs and numbers. All parsers return `false` on invalid input and
 * leave `*out` untouched in that case (fail fast at the CLI boundary).
 */
#pragma once

#include "sparcli.h"

#include <stdbool.h>

/** Parses a border name (`none|ascii|single|double|rounded|thick`). */
bool sc_cli_parse_border(const char *name, ScBorderType *out);

/** Parses a horizontal alignment name (`left|center|right`). */
bool sc_cli_parse_halign(const char *name, ScHAlign *out);

/** Parses an alert level name (`info|debug|warning|error|success`). */
bool sc_cli_parse_alert(const char *name, ScAlertType *out);

/**
 * Parses a list marker name
 * (`bullet|number|alpha|alpha-upper|roman|roman-upper`).
 */
bool sc_cli_parse_list_marker(const char *name, ScListMarker *out);

/** Parses a spinner type name (`braille|pipe|dots|arrow`). */
bool sc_cli_parse_spinner(const char *name, ScSpinnerType *out);

/** Parses a progress bar type name (`block|ascii|line|shaded`). */
bool sc_cli_parse_progress(const char *name, ScProgressType *out);

/** Parses a week start name (`monday|sunday`). */
bool sc_cli_parse_week_start(const char *name, ScWeekStart *out);

/**
 * Parses an input character filter name
 * (`digits|decimal|alpha|alnum|no-space`).
 */
bool sc_cli_parse_filter(const char *name, ScCharFilter *out);

/**
 * Parses a color spec into `*out`.
 *
 * Accepted forms: a named ANSI color (`black`, `red`, `green`, `yellow`,
 * `blue`, `magenta`, `cyan`, `white`), `#RRGGBB` hex, or `R,G,B` decimal
 * components (0-255 each).
 */
bool sc_cli_parse_color(const char *spec, ScColor *out);

/**
 * Parses an edge spec (padding/margin) into `*out`.
 *
 * Accepted forms: `N` (all four sides), `V,H` (top/bottom, left/right),
 * or `T,R,B,L`.
 */
bool sc_cli_parse_edges(const char *spec, ScEdges *out);

/** Parses a non-negative integer. */
bool sc_cli_parse_int(const char *str, int *out);

/** Parses a floating-point number. */
bool sc_cli_parse_double(const char *str, double *out);
