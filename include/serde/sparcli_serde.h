#pragma once

/**
 * @file sparcli_serde.h
 * @brief Umbrella header for the serde layer: structured read/write parsers.
 *
 * The serde layer is intentionally NOT part of the main `sparcli.h` umbrella
 * (which stays a terminal-output/input library). Include this header to pull
 * in the data model and the format readers/writers.
 *
 * As of the first stage it exposes the `ScValue` model, the parse-error type
 * and JSON; further formats (CSV, TOML, YAML, Markdown) are added here as
 * they land.
 */

#include "serde/sparcli_value.h"
#include "serde/sparcli_parse_error.h"
#include "serde/sparcli_json.h"
