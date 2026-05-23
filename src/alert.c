#include "sparcli.h"
#include <string.h>
#include <stdio.h>

/* ── Preset table ────────────────────────────────────────────────────────── */

static const struct {
    const char *icon;   /* UTF-8 icon character */
    const char *label;  /* title label after icon */
    ScColor     color;  /* border + title color */
} alert_presets[] = {
    [SC_ALERT_INFO]    = { "\xe2\x84\xb9", "Info",    { 4, 0, 0, 0 } }, /* ℹ blue    */
    [SC_ALERT_WARNING] = { "\xe2\x9a\xa0", "Warning", { 3, 0, 0, 0 } }, /* ⚠ yellow  */
    [SC_ALERT_ERROR]   = { "\xe2\x9c\x96", "Error",   { 1, 0, 0, 0 } }, /* ✖ red     */
    [SC_ALERT_SUCCESS] = { "\xe2\x9c\x94", "Success", { 2, 0, 0, 0 } }, /* ✔ green   */
};

/* ── Public API ──────────────────────────────────────────────────────────── */

void sc_alert_str(ScAlertType type, const char *content) {
    ScColor col = alert_presets[type].color;

    char title[64];
    snprintf(title, sizeof(title), "%s %s",
             alert_presets[type].icon, alert_presets[type].label);

    ScPanelOpts opts = {
        .border       = SC_BORDER_SINGLE,
        .border_color = col,
        .title        = title,
        .title_style   = { SC_TEXT_ATTR_BOLD, col, SC_ANSI_COLOR_NONE },
        .title_pos    = SC_TITLE_TOP,
        .title_align  = SC_ALIGN_LEFT,
        .title_pad    = 1,
        .padding      = {0, 1, 0, 1},
        .full_width   = 1,
    };
    sc_panel_str(content, opts);
}

void sc_alert_text(ScAlertType type, const ScText *content) {
    ScColor col = alert_presets[type].color;

    char title[64];
    snprintf(title, sizeof(title), "%s %s",
             alert_presets[type].icon, alert_presets[type].label);

    ScPanelOpts opts = {
        .border       = SC_BORDER_SINGLE,
        .border_color = col,
        .title        = title,
        .title_style   = { SC_TEXT_ATTR_BOLD, col, SC_ANSI_COLOR_NONE },
        .title_pos    = SC_TITLE_TOP,
        .title_align  = SC_ALIGN_LEFT,
        .title_pad    = 1,
        .padding      = {0, 1, 0, 1},
        .full_width   = 1,
    };
    sc_panel_text(content, opts);
}

void sc_alert_info   (const char *content) { sc_alert_str(SC_ALERT_INFO,    content); }
void sc_alert_warning(const char *content) { sc_alert_str(SC_ALERT_WARNING, content); }
void sc_alert_error  (const char *content) { sc_alert_str(SC_ALERT_ERROR,   content); }
void sc_alert_success(const char *content) { sc_alert_str(SC_ALERT_SUCCESS, content); }
