#include "test_input.h"
#include "sparcli.h"
#include "input/input_internal.h"   /* sc_number_frame_calc */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


/** Strips ANSI codes from `line` and checks it contains `needle`. */
static bool line_contains(const char *line, const char *needle) {
    char *plain = sc_strip_ansi(line);
    bool found = plain && strstr(plain, needle) != NULL;
    free(plain);
    return found;
}

/** True when any rendered line of `frame` contains `needle` (ANSI-stripped). */
static bool frame_contains(const ScRendered *frame, const char *needle) {
    if (!frame) {
        return false;
    }
    for (size_t i = 0; i < frame->line_count; i++) {
        if (line_contains(frame->lines[i], needle)) {
            return true;
        }
    }
    return false;
}

/** Evaluates `expr` and checks the result equals `expected` (exact). */
static bool eval_equals(const char *expr, double expected) {
    double result = 0;
    return sc_calc_eval(expr, &result) && result == expected;
}


static void check_eval_basics(void);
static void check_eval_separators(void);
static void check_eval_errors(void);
static void check_preview_frames(void);


/**
 * Non-interactive: the pure expression evaluator (`sc_calc_eval`) and the
 * calculator-mode rendering (live preview, error line) via snapshot frames.
 */
void test_calc(void) {
    check_eval_basics();
    check_eval_separators();
    check_eval_errors();
    check_preview_frames();
}

/** Operator precedence, parentheses, unary signs, whitespace. */
static void check_eval_basics(void) {
    CHECK(eval_equals("1+2*3", 7.0), "eval: precedence 1+2*3 == 7");
    CHECK(eval_equals("(1+2)*3", 9.0), "eval: parentheses (1+2)*3 == 9");
    CHECK(eval_equals("2*(3+4)/7", 2.0), "eval: nested 2*(3+4)/7 == 2");
    CHECK(eval_equals("-3", -3.0), "eval: unary minus");
    CHECK(eval_equals("2*-3", -6.0), "eval: unary minus after operator");
    CHECK(eval_equals("-(2+3)", -5.0), "eval: negated parentheses");
    CHECK(eval_equals(" 1 + 2 ", 3.0), "eval: whitespace is ignored");
    CHECK(eval_equals("10-4-3", 3.0), "eval: subtraction is left-associative");
    CHECK(eval_equals("24/4/2", 3.0), "eval: division is left-associative");
    CHECK(eval_equals("6/4", 1.5), "eval: division yields fractions");
}

/** Both '.' and ',' work as decimal separator, also mixed. */
static void check_eval_separators(void) {
    CHECK(eval_equals("1,5+2*3", 7.5), "eval: comma separator 1,5+2*3 == 7,5");
    CHECK(eval_equals("1.5+2*3", 7.5), "eval: period separator");
    CHECK(eval_equals("0,5*0.5", 0.25), "eval: mixed separators");
    CHECK(eval_equals(",5", 0.5), "eval: leading separator ,5 == 0,5");
    CHECK(eval_equals("1,", 1.0), "eval: trailing separator 1, == 1");
}

/** Invalid expressions are rejected (false), never crash. */
static void check_eval_errors(void) {
    double result = 0;
    CHECK(!sc_calc_eval("", &result), "eval: empty string is invalid");
    CHECK(!sc_calc_eval("   ", &result), "eval: whitespace-only is invalid");
    CHECK(!sc_calc_eval("1+", &result), "eval: missing operand is invalid");
    CHECK(!sc_calc_eval("1++2", &result), "eval: '1++2' (typo) is invalid");
    CHECK(!sc_calc_eval("--3", &result), "eval: double negation is invalid");
    CHECK(!sc_calc_eval("+5", &result), "eval: unary plus is invalid");
    CHECK(!sc_calc_eval("(1+2", &result), "eval: unbalanced '(' is invalid");
    CHECK(!sc_calc_eval("1+2)", &result), "eval: unbalanced ')' is invalid");
    CHECK(!sc_calc_eval("1/0", &result), "eval: division by zero is invalid");
    CHECK(!sc_calc_eval("5/(3-3)", &result),
          "eval: division by computed zero is invalid");
    CHECK(!sc_calc_eval("abc", &result), "eval: letters are invalid");
    CHECK(!sc_calc_eval("1+2x", &result), "eval: trailing garbage is invalid");
    CHECK(!sc_calc_eval("1e10", &result),
          "eval: scientific notation is not supported");
    CHECK(!sc_calc_eval(NULL, &result), "eval: NULL expression is invalid");
    CHECK(!sc_calc_eval("1+1", NULL), "eval: NULL result pointer is invalid");

    // Overflow to infinity is rejected: six 60-digit numbers multiplied
    // exceed the double range (~1e308).
    char big[61];
    memset(big, '9', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    char overflow[512];
    snprintf(overflow, sizeof overflow, "%s*%s*%s*%s*%s*%s",
             big, big, big, big, big, big);
    CHECK(!sc_calc_eval(overflow, &result),
          "eval: overflow to infinity is invalid");

    // The result pointer is untouched on failure.
    result = 42.0;
    (void)sc_calc_eval("garbage", &result);
    CHECK(result == 42.0, "eval: *result untouched on failure");
}

/** Calculator-mode frames: live preview, invalid marker, error line. */
static void check_preview_frames(void) {
    ScNumberOpts opts = { .decimals = 1, .decimal_sep = ',' };

    // Valid expression: the preview shows the result with the configured
    // separator, rounded to `decimals`.
    ScRendered *preview = sc_number_frame_calc("Amount",
        (ScNumberCalcFrame){ .expr = "=1,5+2*3" }, opts);
    CHECK(frame_contains(preview, "= 7,5"),
          "frame: live preview shows '= 7,5'");
    CHECK(frame_contains(preview, "=1,5+2*3"),
          "frame: the expression stays visible while typing");
    sc_rendered_free(preview);

    // Invalid/incomplete expression: the preview shows '= ?'.
    ScRendered *invalid = sc_number_frame_calc("Amount",
        (ScNumberCalcFrame){ .expr = "=1,5+" }, opts);
    CHECK(frame_contains(invalid, "= ?"),
          "frame: invalid expression previews '= ?'");
    sc_rendered_free(invalid);

    // Error line after Enter on an invalid expression.
    ScRendered *error = sc_number_frame_calc("Amount",
        (ScNumberCalcFrame){ .expr = "=1,5+", .error = true }, opts);
    CHECK(frame_contains(error, "invalid expression"),
          "frame: error line shows 'invalid expression'");
    sc_rendered_free(error);

    // Full-precision display (calc_show_precise).
    ScNumberOpts precise_opts = { .decimals = 2, .calc_show_precise = true };
    ScRendered *precise = sc_number_frame_calc("Amount",
        (ScNumberCalcFrame){ .expr = "=10/4" }, precise_opts);
    CHECK(frame_contains(precise, "= 2.5"),
          "frame: calc_show_precise previews the unrounded result");
    sc_rendered_free(precise);

    // Boxed mode: preview renders inside the panel.
    ScNumberOpts boxed_opts = { .decimals = 1, .boxed = true, .width = 40 };
    ScRendered *boxed = sc_number_frame_calc("Amount",
        (ScNumberCalcFrame){ .expr = "=2+3" }, boxed_opts);
    CHECK(frame_contains(boxed, "= 5"),
          "frame: boxed mode shows the preview inside the panel");
    sc_rendered_free(boxed);

    // The calc-mode hint replaces the default footer.
    ScRendered *hint = sc_number_frame_calc("Amount",
        (ScNumberCalcFrame){ .expr = "=1+1" }, opts);
    CHECK(frame_contains(hint, "accept result"),
          "frame: calc-mode hint advertises Enter = accept");
    sc_rendered_free(hint);

    // Calculator enabled but not in calc mode: the hint advertises '='.
    ScNumberOpts calc_opts = { .calculator = true };
    ScRendered *normal = sc_number_frame("Amount", 5, calc_opts);
    CHECK(frame_contains(normal, "= calc"),
          "frame: enabled calculator advertises '= calc' in the hint");
    sc_rendered_free(normal);
}
