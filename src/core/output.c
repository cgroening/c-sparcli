#include "sparcli.h"
#include "platform/sc_compat.h"

#include <stdbool.h>
#include <stdio.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#  endif

/*
 * Windows 10 1809+ consoles start with ANSI/VT escape processing OFF, so the
 * escape sequences sparcli emits would print literally. Flip the bit once on
 * the real stdout/stderr consoles the first time any output is requested.
 * Idempotent (a static guard) and best-effort: GetConsoleMode fails on a
 * redirected (pipe/file) handle, in which case there is nothing to enable and
 * we leave it untouched - the redirected bytes carry the raw escapes exactly
 * as on POSIX. A benign race on the guard at most flips the (idempotent) bit
 * twice; Windows builds do not run under TSan.
 */
static void sc_enable_vt_output(void) {
    static bool enabled = false;
    if (enabled) { return; }
    enabled = true;
    const DWORD std_ids[2] = { STD_OUTPUT_HANDLE, STD_ERROR_HANDLE };
    for (int i = 0; i < 2; i++) {
        HANDLE handle = GetStdHandle(std_ids[i]);
        DWORD mode;
        if (handle != NULL && handle != INVALID_HANDLE_VALUE
            && GetConsoleMode(handle, &mode)) {
            SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
}
#endif


/*
 * Thread-local so multiple threads can render/capture concurrently to their own
 * streams without racing: each thread keeps its own output target, and the
 * capture API (which swaps this around `sc_output_set_stream`) only ever
 * touches its own thread's value. Zero-initialized TLS is NULL, so every thread
 * still defaults to `stdout`.
 */
static SC_THREAD_LOCAL FILE *current_output = NULL;


FILE *sc_output_stream(void) {
#ifdef _WIN32
    sc_enable_vt_output();
#endif
    return current_output ? current_output : stdout;
}

void sc_output_set_stream(FILE *out) {
    current_output = out;
}
