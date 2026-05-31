#include "sparcli.h"
#include <stdio.h>


void test_with_str(void);
void test_with_text(void);


void test_alert(void) {
    test_with_str();
    test_with_text();
}

void test_with_str() {
    sc_alert_info   ("This is an informational message.");
    sc_alert_debug  ("These are some debugging values: 1, 2, 3.");
    sc_alert_warning(
        "Disk usage is above 85%. Consider cleaning up old files.");
    sc_alert_error  ("Failed to connect to database: connection timeout.");
    sc_alert_success("Deployment completed successfully in 3.2 seconds.");
}

void test_with_text() {
    ScTextStyle plain = (ScTextStyle){
        SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
    };
    ScTextStyle bold_blue = (ScTextStyle){
        SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLUE, SC_ANSI_COLOR_NONE
    };

    ScText *t = sc_text_new();
    sc_text_append(t, "Loaded ",   plain);
    sc_text_append(t, "42",        bold_blue);
    sc_text_append(t, " modules.", plain);
    sc_alert_text(SC_ALERT_INFO, t);
    sc_text_free(t);


}
