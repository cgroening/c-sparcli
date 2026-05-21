#pragma once

#include "sparcli_core.h"
#include "sparcli_text.h"

typedef enum {
    SC_ALERT_INFO,
    SC_ALERT_WARNING,
    SC_ALERT_ERROR,
    SC_ALERT_SUCCESS,
} ScAlertType;

void sc_alert_str    (ScAlertType type, const char *content);
void sc_alert_text   (ScAlertType type, const ScText *content);
void sc_alert_info   (const char *content);
void sc_alert_warning(const char *content);
void sc_alert_error  (const char *content);
void sc_alert_success(const char *content);
