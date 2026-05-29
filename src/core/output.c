#include "sparcli.h"

#include <stdio.h>


/*
 * Thread-local so multiple threads can render/capture concurrently to their own
 * streams without racing: each thread keeps its own output target, and the
 * capture API (which swaps this around `sc_set_output`) only ever touches its
 * own thread's value. Zero-initialized TLS is NULL, so every thread still
 * defaults to `stdout`.
 */
static _Thread_local FILE *current_output = NULL;


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
