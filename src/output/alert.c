#include "sparcli.h"
#include <string.h>
#include <stdio.h>


/**
 * Predefined alert types with associated icons, labels, and colors.
 */
static const struct {
    const char *icon;   /**< UTF-8 icon character */
    const char *label;  /**< title label after icon */
    ScColor     color;  /**< border + title color */
} alert_presets[] = {
    [SC_ALERT_INFO]    = { "\xe2\x84\xb9", "Info",    SC_ANSI_COLOR_BLUE    },
    [SC_ALERT_DEBUG]   = { "\xe2\x9a\x99", "Debug",   SC_ANSI_COLOR_MAGENTA },
    [SC_ALERT_WARNING] = { "\xe2\x9a\xa0", "Warning", SC_ANSI_COLOR_YELLOW  },
    [SC_ALERT_ERROR]   = { "\xe2\x9c\x96", "Error",   SC_ANSI_COLOR_RED     },
    [SC_ALERT_SUCCESS] = { "\xe2\x9c\x94", "Success", SC_ANSI_COLOR_GREEN   },
};


static void build_alert_title  (ScAlertType type, char *title, size_t size);
static ScPanelOpts build_panel_options(
    ScAlertType type, char *title, size_t size
);


void sc_alert_str(ScAlertType type, const char *content) {
    char title[64];
    ScPanelOpts opts = build_panel_options(type, title, sizeof(title));
    sc_panel_str(content, opts);
}

void sc_alert_text(ScAlertType type, const ScText *content) {
    char title[64];
    ScPanelOpts opts = build_panel_options(type, title, sizeof(title));
    sc_panel_text(content, opts);
}

void sc_alert_info   (const char *content) {
    sc_alert_str(SC_ALERT_INFO, content);
}

void sc_alert_debug  (const char *content) {
    sc_alert_str(SC_ALERT_DEBUG, content);
}

void sc_alert_warning(const char *content) {
    sc_alert_str(SC_ALERT_WARNING, content);
}

void sc_alert_error  (const char *content) {
    sc_alert_str(SC_ALERT_ERROR, content);
}

void sc_alert_success(const char *content) {
    sc_alert_str(SC_ALERT_SUCCESS, content);
}


/**
 * Formats the icon and label of an alert preset into `title`.
 *
 * @param alert_type  Alert preset to look up.
 * @param title       Output buffer.
 * @param size        Size of @p title in bytes.
 */
static void build_alert_title(
    ScAlertType alert_type, char *title, size_t size
) {
    snprintf(
        title,
        size,
        "%s %s",
        alert_presets[alert_type].icon,
        alert_presets[alert_type].label
    );
}

/**
 * Builds the panel options for an alert of the given type.
 *
 * @param type   Alert preset that determines color and title text.
 * @param title  Caller-owned buffer that receives the formatted title string.
 * @param size   Size of `title` in bytes.
 * @return       Fully initialised `ScPanelOpts`; `title` must remain valid for
 *               the lifetime of the returned opts.
 */
static ScPanelOpts build_panel_options(
    ScAlertType type, char *title, size_t size
) {
    ScColor color = alert_presets[type].color;
    build_alert_title(type, title, size);

    ScPanelOpts opts = {
        .border  = { .type = SC_BORDER_ROUNDED, .color = color },
        .title   = { .text = title,
            .style = {
                SC_TEXT_ATTR_BOLD, color, SC_ANSI_COLOR_NONE
            },
            .halign = SC_ALIGN_LEFT,
            .pad = 1,
            .pos = SC_POSITION_TOP },
        .padding = {0, 1, 0, 1},
        .full_width = 1,
    };
    return opts;
}
