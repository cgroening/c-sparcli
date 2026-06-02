#include "sparcli.h"
#include "fuzz_common.h"

#include <string.h>

/**
 * Fuzz target for the arithmetic-expression evaluator (`sc_calc_eval`):
 * random byte sequences must never crash, read out of bounds, recurse
 * without bound, or loop forever. The evaluator either returns `false` or
 * produces a finite double - both are fine; only memory/UB errors (caught
 * by ASan/UBSan) fail the run.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // The evaluator takes a NUL-terminated string; embedded NUL bytes simply
    // truncate the input, which is fine for fuzzing purposes.
    char *expr = malloc(size + 1);
    if (!expr) {
        return 0;
    }
    // Bias the bytes toward the expression alphabet so the parser's deep
    // paths (nesting, precedence, numbers) are exercised - raw random bytes
    // would almost always be rejected at the first character. 25% of the
    // bytes stay raw to keep covering the reject paths.
    static const char alphabet[] = "0123456789+-*/()., \t";
    for (size_t i = 0; i < size; i++) {
        if ((data[i] & 3) != 0) {
            expr[i] = alphabet[data[i] % (sizeof alphabet - 1)];
        } else {
            expr[i] = (char)data[i];
        }
    }
    expr[size] = '\0';

    double result = 0;
    (void)sc_calc_eval(expr, &result);

    free(expr);
    return 0;
}
