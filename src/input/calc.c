#include "sparcli.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>


/**
 * @file calc.c
 * @brief Pure arithmetic-expression evaluator behind the number input's
 *        calculator mode (`sc_calc_eval`).
 *
 * Recursive-descent parser over the grammar:
 *
 *     expr    := term (('+' | '-') term)*
 *     term    := factor (('*' | '/') factor)*
 *     factor  := '-' primary | primary
 *     primary := number | '(' expr ')'
 *     number  := digits [sep digits] | sep digits     (sep = '.' or ',')
 *
 * A factor allows at most ONE unary minus and no unary plus - calculator
 * semantics where `1++2` and `--3` are typos, not valid input.
 *
 * No allocation, no globals; hostile input (fuzzing) is bounded by a
 * recursion-depth cap and a fixed numeric-token buffer.
 */

/** Maximum nesting/recursion depth (guards stack usage on hostile input). */
#define CALC_MAX_DEPTH 256

/** Longest accepted numeric literal in characters. */
#define CALC_MAX_NUMBER 64


/** Parser cursor over the expression string. */
typedef struct CalcParser {
    const char *pos;   /**< Next unread character. */
    int depth;         /**< Current factor recursion depth. */
    bool ok;           /**< Cleared on the first syntax/range error. */
} CalcParser;


static double parse_expr(CalcParser *self);
    static double parse_term(CalcParser *self);
        static double parse_factor(CalcParser *self);
            static double parse_primary(CalcParser *self);
                static double parse_number(CalcParser *self);
static void skip_spaces(CalcParser *self);


bool sc_calc_eval(const char *expr, double *result) {
    if (!expr || !result) {
        return false;
    }
    CalcParser parser = { .pos = expr, .depth = 0, .ok = true };
    double value = parse_expr(&parser);

    // The whole string must be consumed and the result must be finite
    // (overflow to inf / 0*inf to nan counts as an error).
    skip_spaces(&parser);
    if (!parser.ok || *parser.pos != '\0' || !isfinite(value)) {
        return false;
    }
    *result = value;
    return true;
}

/** expr := term (('+' | '-') term)* */
static double parse_expr(CalcParser *self) {
    double value = parse_term(self);
    for (;;) {
        skip_spaces(self);
        if (!self->ok) {
            return 0;
        }
        if (*self->pos == '+') {
            self->pos++;
            value += parse_term(self);
        } else if (*self->pos == '-') {
            self->pos++;
            value -= parse_term(self);
        } else {
            return value;
        }
    }
}

/** term := factor (('*' | '/') factor)* */
static double parse_term(CalcParser *self) {
    double value = parse_factor(self);
    for (;;) {
        skip_spaces(self);
        if (!self->ok) {
            return 0;
        }
        if (*self->pos == '*') {
            self->pos++;
            value *= parse_factor(self);
        } else if (*self->pos == '/') {
            self->pos++;
            double divisor = parse_factor(self);
            if (divisor == 0.0) {
                self->ok = false;
                return 0;
            }
            value /= divisor;
        } else {
            return value;
        }
    }
}

/** factor := '-' primary | primary  (at most one unary minus, no unary '+') */
static double parse_factor(CalcParser *self) {
    skip_spaces(self);
    if (!self->ok) {
        return 0;
    }
    if (*self->pos == '-') {
        self->pos++;
        return -parse_primary(self);
    }
    return parse_primary(self);
}

/** primary := number | '(' expr ')' */
static double parse_primary(CalcParser *self) {
    skip_spaces(self);
    if (!self->ok) {
        return 0;
    }
    // Every parenthesis recursion counts against the depth cap, so deeply
    // nested hostile input cannot exhaust the stack.
    if (++self->depth > CALC_MAX_DEPTH) {
        self->ok = false;
        return 0;
    }

    double value = 0;
    if (*self->pos == '(') {
        self->pos++;
        value = parse_expr(self);
        skip_spaces(self);
        if (*self->pos == ')') {
            self->pos++;
        } else {
            self->ok = false;   // unbalanced parenthesis
        }
    } else {
        value = parse_number(self);
    }

    self->depth--;
    return value;
}

/**
 * number := digits [sep digits] | sep digits  (sep = '.' or ',')
 *
 * The token is collected into a bounded local buffer with `,` rewritten
 * to `.`, then parsed with `strtod`. Over-long tokens are rejected rather
 * than silently truncated.
 */
static double parse_number(CalcParser *self) {
    char token[CALC_MAX_NUMBER];
    size_t len = 0;
    bool seen_digit = false;
    bool seen_sep = false;

    const char *p = self->pos;
    while (*p && len + 1 < sizeof token) {
        if (*p >= '0' && *p <= '9') {
            seen_digit = true;
            token[len++] = *p++;
        } else if ((*p == '.' || *p == ',') && !seen_sep) {
            seen_sep = true;
            token[len++] = '.';
            p++;
        } else {
            break;
        }
    }
    token[len] = '\0';

    bool more_numeric_input =
        (*p >= '0' && *p <= '9') || *p == '.' || *p == ',';
    if (!seen_digit || (len + 1 >= sizeof token && more_numeric_input)) {
        self->ok = false;
        return 0;
    }
    self->pos = p;
    return strtod(token, NULL);
}

/** Advances past spaces and tabs. */
static void skip_spaces(CalcParser *self) {
    while (*self->pos == ' ' || *self->pos == '\t') {
        self->pos++;
    }
}
