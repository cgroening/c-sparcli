#pragma once

#include "sparcli.h"

#include <stddef.h>


/**
 * @file text_internal.h
 * @brief Private struct layout for `ScText`.
 *
 * The public API treats `ScText` as opaque (only the forward declaration
 * is visible in `sparcli_text.h`). sparcli's own source files include
 * this header to access the fields directly and avoid accessor-call
 * overhead in the hot rendering paths. External consumers use the
 * accessor functions in `sparcli_text.h`.
 */

struct ScText {
    ScSpan *spans;
    size_t  count;
    size_t  capacity;
};
