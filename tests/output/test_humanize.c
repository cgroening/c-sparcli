#include "sparcli.h"
#include <stdio.h>
#include <stdlib.h>


/** Prints one labeled humanize result and frees it. */
static void show(const char *label, char *value) {
    printf("%-22s %s\n", label, value ? value : "(null)");
    free(value);
}


void test_humanize(void) {
    printf("\n");

    /* ── 1. Byte sizes ── */
    printf("--- Humanize 1. sizes ---\n");
    show("0 (SI):",       sc_humanize_bytes(0, SC_BYTES_SI));
    show("512 (SI):",     sc_humanize_bytes(512, SC_BYTES_SI));
    show("1536 (SI):",    sc_humanize_bytes(1536, SC_BYTES_SI));
    show("1536 (IEC):",   sc_humanize_bytes(1536, SC_BYTES_IEC));
    show("5242880 (IEC):", sc_humanize_bytes(5242880, SC_BYTES_IEC));
    show("1500000 (SI):", sc_humanize_bytes(1500000, SC_BYTES_SI));
    show("3221225472 SHORT:",
         sc_humanize_bytes(3221225472ULL, SC_BYTES_IEC_SHORT));
    show("de_DE 1536 IEC:",
         sc_humanize_bytes_opts(1536, SC_BYTES_IEC,
                                (ScHumanizeOpts){ .decimal_sep = ',' }));

    printf("\n--- Humanize 2. numbers ---\n");
    show("1234567:",  sc_humanize_number(1234567, (ScHumanizeOpts){ 0 }));
    show("1234567.5/2:",
         sc_humanize_number(1234567.5, (ScHumanizeOpts){ .decimals = 2 }));
    show("de_DE 1234567,89:",
         sc_humanize_number(1234567.89,
                            (ScHumanizeOpts){ .decimals = 2,
                                              .decimal_sep = ',',
                                              .group_sep = '.' }));
    show("-9876543:", sc_humanize_number(-9876543, (ScHumanizeOpts){ 0 }));

    printf("\n--- Humanize 3. compact ---\n");
    show("999:",     sc_humanize_compact(999, (ScHumanizeOpts){ 0 }));
    show("12400:",   sc_humanize_compact(12400, (ScHumanizeOpts){ 0 }));
    show("1200000:", sc_humanize_compact(1200000, (ScHumanizeOpts){ 0 }));
    show("7500000000:",
         sc_humanize_compact(7500000000.0, (ScHumanizeOpts){ 0 }));

    printf("\n--- Humanize 4. percent ---\n");
    show("0.42:",    sc_humanize_percent(0.42, (ScHumanizeOpts){ 0 }));
    show("0.1234/1:",
         sc_humanize_percent(0.1234, (ScHumanizeOpts){ .decimals = 1 }));

    printf("\n--- Humanize 5. durations ---\n");
    show("45:",     sc_humanize_duration(45));
    show("93:",     sc_humanize_duration(93));
    show("8054:",   sc_humanize_duration(8054));
    show("90061:",  sc_humanize_duration(90061));
    show("7200:",   sc_humanize_duration(7200));
    show("clock 93:",   sc_humanize_duration_clock(93));
    show("clock 3725:", sc_humanize_duration_clock(3725));

    printf("\n--- Humanize 6. relative ---\n");
    time_t now = 1000000000;  /* fixed reference for determinism */
    show("now-10s:",  sc_humanize_relative(now - 10, now));
    show("now-200s:", sc_humanize_relative(now - 200, now));
    show("now-3h:",   sc_humanize_relative(now - 3L * 3600, now));
    show("now-2d:",   sc_humanize_relative(now - 2L * 86400, now));
    show("in 5h:",    sc_humanize_relative(now + 5L * 3600, now));
    show("in 3mo:",   sc_humanize_relative(now + 90L * 86400, now));
}
