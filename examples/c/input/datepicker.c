/*
 * datepicker.c - calendar month grid for picking a date.
 *
 * Run (after `make`) in a real terminal:
 *   make run-example EX=c/input/datepicker
 *
 * Keys: arrows move by day/week, PageUp/Down (or < / >) change the month,
 * Shift+PageUp/Down change the year, Enter picks.
 */

#include <sparcli.h>

#include <stdio.h>
#include <time.h>


int main(void) {
    if (!sc_input_available()) {
        sc_alert_warning("Run this example in a real terminal "
                         "(not under a pipe).");
        return 0;
    }

    // A zeroed struct tm seeds the picker with today's date.
    struct tm picked = { 0 };
    ScInputStatus status = sc_datepicker(&picked, (ScDatePickerOpts){
        .prompt     = "Delivery date",
        .week_start = SC_WEEK_START_MONDAY,
        .accent     = SC_ANSI_COLOR_CYAN,
    });

    if (status == SC_INPUT_OK) {
        char formatted[64];
        strftime(formatted, sizeof formatted, "%A, %d %B %Y", &picked);
        printf("  -> %s\n", formatted);
    }

    // The struct is in/out: pre-fill it to start at a specific date.
    struct tm new_year = {
        .tm_year = 2027 - 1900,    // years since 1900
        .tm_mon  = 0,              // January
        .tm_mday = 1,
    };
    status = sc_datepicker(&new_year, (ScDatePickerOpts){
        .prompt     = "Starts at 2027-01-01 (Sunday weeks)",
        .week_start = SC_WEEK_START_SUNDAY,
    });

    if (status == SC_INPUT_OK) {
        char formatted[64];
        strftime(formatted, sizeof formatted, "%Y-%m-%d", &new_year);
        printf("  -> %s\n", formatted);
    }

    return 0;
}
