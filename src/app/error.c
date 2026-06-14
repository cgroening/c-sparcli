#include "sparcli.h"
#include "core/sanitize_internal.h"
#include "core/text_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Default process exit code when none is set. */
#define ERROR_DEFAULT_EXIT_CODE 1

/** Initial capacity of the cause array. */
#define ERROR_INITIAL_CAUSE_CAPACITY 4

/** Indented prefix of every cause line. */
#define ERROR_CAUSE_PREFIX "\n  caused by: "

/** Prefix of the hint block (blank line + label). */
#define ERROR_HINT_LABEL "Hint: "


/** Span styles used by the error body. */
/* Designated initializers (fg/bg zero = no color): a compound-literal color
 * macro is not a constant initializer under MSVC's C compiler. */
static const ScTextStyle MESSAGE_STYLE    = { .attr = SC_TEXT_ATTR_BOLD };
static const ScTextStyle CAUSE_STYLE      = { .attr = SC_TEXT_ATTR_DIM };
static const ScTextStyle HINT_LABEL_STYLE = { .attr = SC_TEXT_ATTR_BOLD };
static const ScTextStyle PLAIN_STYLE      = { 0 };


/** Structured error report (opaque to callers). */
struct ScError {
    char  *message;         /**< Sanitized message; never `NULL`. */
    char  *hint;            /**< Sanitized hint; `NULL` = none. */
    char **causes;          /**< Sanitized cause chain (oldest first). */
    size_t cause_count;     /**< Number of causes. */
    size_t cause_capacity;  /**< Allocated cause slots. */
    int    exit_code;       /**< Exit code used by `sc_die`. */
};


// Forward declarations indented to reflect call hierarchy
static ScText *build_error_body(const ScError *error);
static char *sanitize_or_empty(const char *str);


ScError *sc_error_new(const char *message) {
    ScError *error = calloc(1, sizeof(ScError));
    if (!error) { return NULL; }

    error->message = sanitize_or_empty(message);
    if (!error->message) {
        free(error);
        return NULL;
    }
    error->exit_code = ERROR_DEFAULT_EXIT_CODE;
    return error;
}

void sc_error_add_cause(ScError *error, const char *cause) {
    if (!error || !cause) { return; }

    if (error->cause_count == error->cause_capacity) {
        size_t new_capacity = error->cause_capacity
            ? error->cause_capacity * 2
            : ERROR_INITIAL_CAUSE_CAPACITY;
        char **grown = realloc(
            error->causes, new_capacity * sizeof(char *)
        );
        if (!grown) { return; }
        error->causes = grown;
        error->cause_capacity = new_capacity;
    }

    // User string crosses the trust boundary here
    char *copy = sc_sanitize_copy(cause, sc_allow_ansi());
    if (!copy) { return; }
    error->causes[error->cause_count++] = copy;
}

void sc_error_set_hint(ScError *error, const char *hint) {
    if (!error) { return; }
    free(error->hint);
    error->hint = hint ? sc_sanitize_copy(hint, sc_allow_ansi()) : NULL;
}

void sc_error_set_code(ScError *error, int exit_code) {
    if (!error) { return; }
    error->exit_code = exit_code;
}

int sc_error_code(const ScError *error) {
    return error ? error->exit_code : ERROR_DEFAULT_EXIT_CODE;
}

void sc_error_print(const ScError *error) {
    if (!error) { return; }

    ScText *body = build_error_body(error);
    if (!body) { return; }
    sc_alert_text(SC_ALERT_ERROR, body);
    sc_text_free(body);
}

void sc_error_print_stderr(const ScError *error) {
    if (!error) { return; }

    // Render to stderr regardless of the current (thread-local) target
    FILE *previous = sc_output_stream();
    sc_output_set_stream(stderr);
    sc_error_print(error);
    sc_output_set_stream(previous);
}

void sc_error_free(ScError *error) {
    if (!error) { return; }
    free(error->message);
    free(error->hint);
    for (size_t i = 0; i < error->cause_count; i++) {
        free(error->causes[i]);
    }
    free(error->causes);
    free(error);
}

_Noreturn void sc_die(ScError *error) {
    int exit_code = ERROR_DEFAULT_EXIT_CODE;
    if (error) {
        exit_code = error->exit_code;
        sc_error_print_stderr(error);
        sc_error_free(error);
    }
    exit(exit_code);
}

_Noreturn void sc_die_msg(int code, const char *message, const char *hint) {
    ScError *error = sc_error_new(message);
    if (!error) {
        exit(code);   // allocation failure: still honor the exit code
    }
    sc_error_set_hint(error, hint);
    sc_error_set_code(error, code);
    sc_die(error);
}

/**
 * Builds the alert-panel body: bold message, dim cause chain, hint
 * block. All stored strings are already sanitized, so they are appended
 * through the raw (trusted) path.
 */
static ScText *build_error_body(const ScError *error) {
    ScText *body = sc_text_new();
    if (!body) { return NULL; }

    sc_text_append_raw(body, error->message, MESSAGE_STYLE);

    for (size_t i = 0; i < error->cause_count; i++) {
        sc_text_append_raw(body, ERROR_CAUSE_PREFIX, CAUSE_STYLE);
        sc_text_append_raw(body, error->causes[i], CAUSE_STYLE);
    }

    if (error->hint) {
        sc_text_append_raw(body, "\n\n", PLAIN_STYLE);
        sc_text_append_raw(body, ERROR_HINT_LABEL, HINT_LABEL_STYLE);
        sc_text_append_raw(body, error->hint, PLAIN_STYLE);
    }
    return body;
}

/** Sanitized copy of `str`; an empty heap string for `NULL` input. */
static char *sanitize_or_empty(const char *str) {
    if (!str) { return strdup(""); }
    return sc_sanitize_copy(str, sc_allow_ansi());
}
