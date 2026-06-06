/*
 * form_demo.c
 *
 * A grid-layout form: framed fields arranged in a raster with per-field
 * width (%/fixed/auto), spans (a tall field spanning two rows), 2D arrow
 * navigation, and in-region editing below the grid.
 *
 *   - Arrow keys move the highlighted box (2D); Tab/Shift-Tab cycle.
 *   - Enter opens the editor below the grid; a second Enter saves; Esc aborts
 *     the edit. Bool fields toggle directly with Space/Enter.
 *   - The multiline "Notes" field opens in $EDITOR/nvim with Enter or Ctrl-G.
 *   - Ctrl-D submits the whole form; Esc (in navigation) cancels.
 *
 * Build (after `make`):
 *   make run-example EX=c/apps/form_demo
 */

#include "sparcli.h"

#include <stdio.h>


int main(void) {
    if (!sc_input_available()) {
        sc_alert_warning("Run this in a real terminal (not under a pipe).");
        return 0;
    }

    ScForm *form = sc_form_new((ScFormOpts){
        .title = "Edit contact",
        .accent = SC_COLOR_ACCENT,
        .editor = "nvim",   /* multiline fields open here via Ctrl-G */
    });

    /* Row 1: three equal-width fields. */
    sc_form_row_begin(form);
    int first = sc_form_add_text(form, "First name", "Apple",
        (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 33 });
    int last = sc_form_add_text(form, "Last name", "Inc.",
        (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 33 });
    int role = sc_form_add_text(form, "Role", "Vendor",
        (ScFieldOpts){ .width_mode = SC_FWIDTH_AUTO });

    /* Row 2: a 60/40 split with a select field. */
    static const char *const tiers[] = { "Bronze", "Silver", "Gold" };
    sc_form_row_begin(form);
    int email = sc_form_add_text(form, "Email", "info@apple.example",
        (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 60 });
    int tier = sc_form_add_select(form, "Tier", tiers, 3, 1,
        (ScFieldOpts){ .width_mode = SC_FWIDTH_AUTO, .col_span = 2 });

    /* A multiselect + date row. */
    static const char *const tags[] = {
        "vip", "wholesale", "tax-exempt", "net-30" };
    sc_form_row_begin(form);
    int qty = sc_form_add_number(form, "Qty", 42,
        (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 30 });
    int flags = sc_form_add_multiselect(form, "Flags", tags, 4,
        (const size_t[]){ 0 }, 1, (ScFieldOpts){ .width_mode = SC_FWIDTH_AUTO });
    int since = sc_form_add_date(form, "Since", (struct tm){ 0 },
        (ScFieldOpts){ .width_mode = SC_FWIDTH_AUTO });

    /* Row 3+4: a tall notes field (rowspan 2) in the MIDDLE column, flanked by
       stacked fields (Phone/City on the left, Active/ZIP on the right). */
    sc_form_row_begin(form);
    int phone = sc_form_add_text(form, "Phone", "+1 555 0100",
        (ScFieldOpts){ .width_mode = SC_FWIDTH_AUTO });
    int notes = sc_form_add_text(form, "Notes", "preferred supplier",
        (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 50,
                       .row_span = 2, .height = 3, .multiline = true,
                       .help = "ctrl-g opens $EDITOR" });
    int active = sc_form_add_bool(form, "Active", true,
        (ScFieldOpts){ .width_mode = SC_FWIDTH_AUTO });

    sc_form_row_begin(form);
    int city = sc_form_add_text(form, "City", "Cupertino",
        (ScFieldOpts){ .width_mode = SC_FWIDTH_AUTO });
    sc_form_add_skip(form);   // covered by the rowspan "Notes" field
    int zip = sc_form_add_text(form, "ZIP", "95014",
        (ScFieldOpts){ .width_mode = SC_FWIDTH_AUTO });

    ScInputStatus st = sc_form_run(form);
    if (st == SC_INPUT_OK) {
        sc_markup_println("\n[green]\xe2\x9c\x94[/] Saved:");
        printf("  %s %s (%s)\n", sc_form_get_string(form, first),
               sc_form_get_string(form, last), sc_form_get_string(form, role));
        static const char *const tiers2[] = { "Bronze", "Silver", "Gold" };
        printf("  email=%s  tier=%s\n", sc_form_get_string(form, email),
               tiers2[sc_form_get_choice(form, tier)]);
        size_t fl[4];
        size_t nfl = sc_form_get_checked(form, flags, fl, 4);
        struct tm since_tm = { 0 };
        char since_buf[32] = "?";
        if (sc_form_get_date(form, since, &since_tm)) {
            strftime(since_buf, sizeof since_buf, "%Y-%m-%d", &since_tm);
        }
        printf("  qty=%g  flags=%zu set  since=%s\n",
               sc_form_get_number(form, qty), nfl, since_buf);
        printf("  notes=%s\n", sc_form_get_string(form, notes));
        printf("  phone=%s  active=%s\n", sc_form_get_string(form, phone),
               sc_form_get_bool(form, active) ? "yes" : "no");
        printf("  city=%s  zip=%s\n", sc_form_get_string(form, city),
               sc_form_get_string(form, zip));
    } else if (st == SC_INPUT_CANCELLED) {
        sc_markup_println("\n[yellow]cancelled[/]");
    } else {
        sc_alert_warning("No TTY available.");
    }

    sc_form_free(form);
    return 0;
}
