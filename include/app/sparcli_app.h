#pragma once

/**
 * @file sparcli_app.h
 * @brief Sub-umbrella for sparcli's application-framework helpers.
 *
 * Unlike the output widgets (terminal rendering) and the input widgets
 * (interactive prompts), the headers reached through this file help with
 * the surrounding CLI application: where its files live (XDG paths) and
 * how it reports fatal errors. `sparcli.h` includes it transitively.
 */

#include "app/sparcli_paths.h"          // IWYU pragma: export
#include "app/sparcli_error.h"          // IWYU pragma: export
