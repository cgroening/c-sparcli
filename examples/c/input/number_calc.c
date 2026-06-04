/*
 * number_calc.c - numeric input with stepping/clamping and the built-in
 * calculator mode.
 *
 * Run (after `make`) in a real terminal:
 *   make run-example EX=c/input/number_calc
 */

#include <sparcli.h>

#include <stdio.h>
#include <stdlib.h>


static void run_stepper(void);
static void run_calculator(void);
static void run_pure_evaluator(void);


int main(void) {
    if (!sc_input_available()) {
        sc_alert_warning("Run this example in a real terminal "
                         "(not under a pipe).");
        // The expression evaluator is pure and works without a terminal.
        run_pure_evaluator();
        return 0;
    }

    run_stepper();
    run_calculator();
    run_pure_evaluator();
    return 0;
}

/** Up/Down steps the value; the result is clamped to [min, max]. */
static void run_stepper(void) {
    double quantity = 0.0;
    ScInputStatus status = sc_number_input("Quantity", &quantity,
        (ScNumberOpts){
            .initial  = 1,
            .min      = 1,
            .max      = 99,
            .step     = 1,
            .decimals = 0,
            .box.enabled = true, // panel frame; range shown bottom-right
            .box.width   = 28,
        });

    if (status == SC_INPUT_OK) {
        printf("  -> %.0f\n", quantity);
    }
}

/**
 * Calculator mode: type "=" followed by an expression (e.g. =19,99*1,19).
 * Enter shows the result in the field, a second Enter submits. The display
 * is rounded to `decimals`, but `out_text` receives the full precision.
 */
static void run_calculator(void) {
    double amount = 0.0;
    char *exact = NULL;
    ScInputStatus status = sc_number_input("Amount (try =1/3)", &amount,
        (ScNumberOpts){
            .decimals    = 2,
            .decimal_sep = ',',     // comma input/display, '.'-normalized out
            .start_empty = true,    // begin with an empty field
            .calculator  = true,
            .out_text    = &exact,  // exact decimal string, e.g. for Decimal
        });

    if (status == SC_INPUT_OK) {
        printf("  -> displayed: %.2f, exact: %s\n", amount,
               exact ? exact : "?");
        free(exact);
    }
}

/** sc_calc_eval is the pure evaluator behind calculator mode. */
static void run_pure_evaluator(void) {
    const char *expression = "(2,5 + 1,5) * 4 - 1";
    double result = 0.0;
    if (sc_calc_eval(expression, &result)) {
        printf("  %s = %g\n", expression, result);
    } else {
        printf("  invalid expression: %s\n", expression);
    }
}
