// form_demo.cpp - grid-layout form (C++ wrapper).
//
// A raster of framed fields with per-field width (%/fixed/auto) and col/row
// spans (a "Tier" select spanning two columns, a tall multiline "Notes" field
// spanning two rows), 2D arrow navigation, and in-place editing below the grid.
//
//   - Arrow keys move the highlighted box (2D); Tab/Shift-Tab cycle.
//   - Enter opens the editor below the grid; a second Enter saves; Esc aborts.
//     Bool fields toggle directly with Space/Enter.
//   - The multiline "Notes" field opens in $EDITOR/nvim with Enter or Ctrl-G.
//   - Ctrl-D submits the whole form; Esc (in navigation) cancels.
//
// Run (after `make`) in a real terminal:
//   make run-example EX=cpp/apps/form_demo

#include <sparcli.hpp>

#include <ctime>
#include <print>
#include <string>
#include <vector>

using namespace sparcli;


int main() {
    if (!input_available()) {
        alert::warning("Run this in a real terminal (not under a pipe).");
        return 0;
    }

    Form form({ .title = "Edit contact",
                .accent = palette::accent(),
                .editor = "nvim" });   // multiline fields open here (Enter/^G)

    // Row 1: three equal-width fields.
    form.row_begin();
    int first = form.add_text("First name", "Apple",
        { .width_mode = SC_FWIDTH_PCT, .width = 33 });
    int last = form.add_text("Last name", "Inc.",
        { .width_mode = SC_FWIDTH_PCT, .width = 33 });
    int role = form.add_text("Role", "Vendor", { .width_mode = SC_FWIDTH_AUTO });

    // Row 2: a 60/40 split; the Tier select spans the other two columns.
    const std::vector<std::string> tiers = { "Bronze", "Silver", "Gold" };
    form.row_begin();
    int email = form.add_text("Email", "info@apple.example",
        { .width_mode = SC_FWIDTH_PCT, .width = 60 });
    int tier = form.add_select("Tier", tiers, 1,
        { .width_mode = SC_FWIDTH_AUTO, .col_span = 2 });

    // A number + multiselect + date row.
    const std::vector<std::string> tags = {
        "vip", "wholesale", "tax-exempt", "net-30" };
    form.row_begin();
    int qty = form.add_number("Qty", 42, { .width_mode = SC_FWIDTH_PCT,
                                           .width = 30 });
    int flags = form.add_multiselect("Flags", tags, { 0 },
        { .width_mode = SC_FWIDTH_AUTO });
    int since = form.add_date("Since", {}, { .width_mode = SC_FWIDTH_AUTO });

    // Rows 4+5: a tall multiline notes field (rowspan 2) beside stacked fields.
    form.row_begin();
    int notes = form.add_text("Notes", "preferred supplier",
        { .width_mode = SC_FWIDTH_PCT, .width = 50, .row_span = 2, .height = 3,
          .multiline = true, .help = "ctrl-g opens $EDITOR" });
    int phone = form.add_text("Phone", "+1 555 0100",
        { .width_mode = SC_FWIDTH_AUTO });
    int active = form.add_bool("Active", true, { .width_mode = SC_FWIDTH_AUTO });

    form.row_begin();
    form.add_skip();   // covered by the rowspan "Notes" field
    int city = form.add_text("City", "Cupertino",
        { .width_mode = SC_FWIDTH_AUTO });
    int zip = form.add_text("ZIP", "95014", { .width_mode = SC_FWIDTH_AUTO });

    if (form.run()) {
        markup::println("\n[green]✔[/] Saved:");
        std::println("  {} {} ({})", std::string(form.get_string(first)),
                     std::string(form.get_string(last)),
                     std::string(form.get_string(role)));
        std::println("  email={}  tier={}", std::string(form.get_string(email)),
                     tiers[form.get_choice(tier)]);

        std::string when = "?";
        if (auto d = form.get_date(since)) {
            char buf[32];
            std::tm tm = *d;
            std::strftime(buf, sizeof buf, "%Y-%m-%d", &tm);
            when = buf;
        }
        std::println("  qty={}  flags={} set  since={}",
                     form.get_number(qty), form.get_checked(flags).size(), when);
        std::println("  notes={}", std::string(form.get_string(notes)));
        std::println("  phone={}  active={}", std::string(form.get_string(phone)),
                     form.get_bool(active) ? "yes" : "no");
        std::println("  city={}  zip={}", std::string(form.get_string(city)),
                     std::string(form.get_string(zip)));
    } else {
        markup::println("\n[yellow]cancelled[/]");
    }
    return 0;
}
