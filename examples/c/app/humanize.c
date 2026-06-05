/*
 * humanize.c - human-readable sizes, durations, relative time and numbers.
 *
 * Run (after `make`):
 *   make run-example EX=c/app/humanize
 */

#include <sparcli.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>


/** Prints "label  value" and frees the heap string. */
static void show(const char *label, char *value) {
    printf("  %-16s %s\n", label, value);
    free(value);
}


int main(void) {
    sc_rule_str("Sizes", (ScRuleOpts){ .type = SC_BORDER_SINGLE });
    show("1536 (SI):",  sc_humanize_bytes(1536, SC_BYTES_SI));
    show("1536 (IEC):", sc_humanize_bytes(1536, SC_BYTES_IEC));
    show("5 GB:",       sc_humanize_bytes(5000000000ULL, SC_BYTES_SI));

    sc_rule_str("Numbers", (ScRuleOpts){ .type = SC_BORDER_SINGLE });
    show("grouped:", sc_humanize_number(1234567, (ScHumanizeOpts){ 0 }));
    show("compact:", sc_humanize_compact(12400, (ScHumanizeOpts){ 0 }));
    show("percent:", sc_humanize_percent(0.42, (ScHumanizeOpts){ 0 }));
    show("de_DE:",   sc_humanize_number(1234567.89,
                        (ScHumanizeOpts){ .decimals = 2,
                                          .decimal_sep = ',',
                                          .group_sep = '.' }));

    sc_rule_str("Durations & time", (ScRuleOpts){ .type = SC_BORDER_SINGLE });
    show("93s:",     sc_humanize_duration(93));
    show("8054s:",   sc_humanize_duration(8054));
    show("clock:",   sc_humanize_duration_clock(3725));

    time_t now = time(NULL);
    show("3h ago:",  sc_humanize_relative(now - 3L * 3600, now));
    show("in 2d:",   sc_humanize_relative(now + 2L * 86400, now));
    return 0;
}
