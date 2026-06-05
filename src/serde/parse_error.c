#include "serde/sparcli_parse_error.h"

#include <stdio.h>

/**
 * @file parse_error.c
 * @brief Bridge from the lightweight `ScParseError` to a renderable
 *        `ScError` (red alert panel) from the app layer.
 */


/** Exit code applied to errors produced from a parse failure. */
#define PARSE_ERROR_EXIT_CODE 2

/** Buffer for the composed message line (location + reason). */
#define PARSE_ERROR_COMPOSED_MAX (SC_PARSE_ERROR_MESSAGE_MAX + 64)


ScError *sc_parse_error_to_error(const ScParseError *error) {
    if (!error) {
        return NULL;
    }

    char composed[PARSE_ERROR_COMPOSED_MAX];
    if (error->line > 0) {
        snprintf(
            composed, sizeof composed,
            "Parse error at line %d, column %d: %s",
            error->line, error->column, error->message
        );
    } else {
        snprintf(composed, sizeof composed, "Parse error: %s", error->message);
    }

    ScError *result = sc_error_new(composed);
    if (!result) {
        return NULL;
    }
    if (error->snippet[0] != '\0') {
        sc_error_add_cause(result, error->snippet);
    }
    sc_error_set_code(result, PARSE_ERROR_EXIT_CODE);
    return result;
}
