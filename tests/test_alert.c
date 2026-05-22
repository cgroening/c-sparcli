#include "sparcli.h"
#include <stdio.h>


void test_alert(void) {
    printf("\n");

    sc_alert_info   ("This is an informational message.");
    sc_alert_warning("Disk usage is above 85%. Consider cleaning up old files.");
    sc_alert_error  ("Failed to connect to database: connection timeout.");
    sc_alert_success("Deployment completed successfully in 3.2 seconds.");

    printf("\n");

    sc_alert_success("Build passed: 147 tests, 0 failures\nCoverage: 94.2%");

    {
        ScText *t = sc_text_new();
        sc_text_append(t, "Loaded ",   (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
        sc_text_append(t, "42",        (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLUE, SC_ANSI_COLOR_NONE });
        sc_text_append(t, " modules",  (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
        sc_alert_text(SC_ALERT_INFO, t);
        sc_text_free(t);
    }
}
