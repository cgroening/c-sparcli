#include "sparcli.h"

#include <stdio.h>


static FILE *current_output = NULL;


FILE *sc_output_stream(void) {
    return current_output ? current_output : stdout;
}

void sc_set_output(FILE *out) {
    current_output = out;
}

void sc_with_output(FILE *out, void (*fn)(void *ctx), void *ctx) {
    if (!fn) { return; }
    FILE *saved = current_output;
    current_output = out;
    fn(ctx);
    current_output = saved;
}
