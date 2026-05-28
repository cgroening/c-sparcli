#include "sparcli.h"

#include <stdio.h>


static FILE *current_output = NULL;


FILE *sc_output_stream(void) {
    return current_output ? current_output : stdout;
}

void sc_set_output(FILE *out) {
    current_output = out;
}
