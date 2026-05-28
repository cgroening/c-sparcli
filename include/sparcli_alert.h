#pragma once

#include "sparcli_core.h"  // IWYU pragma: export
#include "sparcli_text.h"  // IWYU pragma: export

SPARCLI_BEGIN_DECLS


/**
 * ScAlertType – Visual style preset for an alert panel.
 *
 * Each variant selects a distinct icon, label, and accent color.
 */
typedef enum {
    SC_ALERT_INFO,     /**< ℹ  Blue    - informational message */
    SC_ALERT_DEBUG,    /**< ⚙  Magenta - debugging output      */
    SC_ALERT_WARNING,  /**< ⚠  Yellow  - non-fatal warning     */
    SC_ALERT_ERROR,    /**< ✖  Red     - error or failure      */
    SC_ALERT_SUCCESS,  /**< ✔  Green   - successful outcome    */
} ScAlertType;


/**
 * Renders a full-width alert panel with a plain-text body.
 *
 * @param type    Visual preset (icon, label, color).
 * @param content Body text; may contain `\n` for multi-line output.
 */
SPARCLI_EXPORT void sc_alert_str(ScAlertType type, const char *content);

/**
 * Renders a full-width alert panel with a rich-text body.
 *
 * @param type    Visual preset (icon, label, color).
 * @param content Rich text body; not freed by this function.
 */
SPARCLI_EXPORT void sc_alert_text(ScAlertType type, const ScText *content);

/** Renders an info alert (ℹ, blue) with a plain-text body. */
SPARCLI_EXPORT void sc_alert_info   (const char *content);

/** Renders a debug alert (⚙, magenta) with a plain-text body. */
SPARCLI_EXPORT void sc_alert_debug  (const char *content);

/** Renders a warning alert (⚠, yellow) with a plain-text body. */
SPARCLI_EXPORT void sc_alert_warning(const char *content);

/** Renders an error alert (✖, red) with a plain-text body. */
SPARCLI_EXPORT void sc_alert_error  (const char *content);

/** Renders a success alert (✔, green) with a plain-text body. */
SPARCLI_EXPORT void sc_alert_success(const char *content);

SPARCLI_END_DECLS
