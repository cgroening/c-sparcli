// fullscreen_app.cpp - a full-screen app shell with the C++ wrapper.
//
// Run (after `make`) in a real terminal:
//   make run-example EX=cpp/apps/fullscreen_app
//
// Mirrors examples/c/apps/fullscreen_app.c:
//   - AltScreen owns the alternate screen for the whole loop (no flicker when
//     switching widgets).
//   - The widgets run with fullscreen = true + a pinned header (a captured
//     panel) + valign: each frame they compose [valign-pad][header][body] and
//     fill the terminal. The Fuzzy finder grows with its items up to the screen
//     height, then scrolls; the leftover space is placed by VALIGN (below the
//     finder for TOP, above the header for BOTTOM, both for MIDDLE).
//   - Enter (or ^F) on an item opens the Form to edit it; ^R adds an item live.

#include <sparcli.hpp>

#include <optional>
#include <string>

using namespace sparcli;


// ── Knobs you can change ──────────────────────────────────────
constexpr ScVAlign VALIGN = SC_VALIGN_TOP;  // TOP / MIDDLE / BOTTOM
constexpr bool SUBTITLE_BELOW = true;          // legend below vs on the border
// ──────────────────────────────────────────────────────────────

enum { ACT_FORM = 1, ACT_REFRESH = 2 };   // shortcut ids


// The "Fullscreen Example - XXX" content line, in mixed colors/styles.
static Text title_line(bool finder, int count, std::string_view selected) {
    Text t;
    t.append("Fullscreen Example", { SC_TEXT_ATTR_BOLD, palette::accent(),
                                     none() })
     .append(" - ", { SC_TEXT_ATTR_DIM, none(), none() });
    if (finder) {
        t.append("finder", { SC_TEXT_ATTR_BOLD, green(), none() });
        t.append("  " + std::to_string(count) + " tasks",
                 { SC_TEXT_ATTR_NONE, cyan(), none() });
    } else {
        t.append("editing ", { SC_TEXT_ATTR_BOLD, yellow(), none() });
        t.append(selected, { SC_TEXT_ATTR_BOLD, palette::accent(), none() });
    }
    return t;
}

// The YYY legend line, in mixed colors/styles.
static Text legend_line(bool finder) {
    const TextStyle dim{ SC_TEXT_ATTR_DIM, none(), none() };
    const TextStyle key{ SC_TEXT_ATTR_BOLD, palette::accent(), none() };
    const TextStyle red_{ SC_TEXT_ATTR_BOLD, red(), none() };
    Text t;
    if (finder) {
        t.append("enter ", key).append("edit", dim).append("  ·  ", dim)
         .append("^R ", key).append("refresh", dim).append("  ·  ", dim)
         .append("esc ", red_).append("quit", dim);
    } else {
        t.append("^D ", key).append("save", dim).append("  ·  ", dim)
         .append("esc ", red_).append("back", dim);
    }
    return t;
}

// Builds the pinned header: a rounded full-width panel whose content is the
// "Fullscreen Example - XXX" line, plus the YYY legend (below / on border).
static Rendered build_header(bool finder, int count, std::string_view sel) {
    Text title = title_line(finder, count, sel);
    Text legend = legend_line(finder);

    PanelOpts po{};
    po.border = { .type = SC_BORDER_ROUNDED, .color = palette::accent() };
    po.full_width = true;
    po.padding = { .right = 1, .left = 1 };
    if (!SUBTITLE_BELOW) {
        po.subtitle = ScTitle{ .rich_text = legend.get(),
                               .halign = SC_ALIGN_RIGHT,
                               .pad = 1, .pos = SC_POSITION_BOTTOM };
    }
    Rendered panel = capture::panel(title, po);
    if (!SUBTITLE_BELOW) {
        return panel;
    }
    Rendered sub = capture::text(legend);
    return vstack({ &panel, &sub }, 0);
}

// Runs the fuzzy finder full-screen under `hdr`. Returns false on cancel (Esc);
// otherwise sets `selected` to the chosen label and `action` to the fired id.
static bool run_finder(const Rendered& hdr, std::string& selected,
                       int& action) {
    Fuzzy* fp = nullptr;
    int next = 6;
    Shortcuts sc;
    sc.on_return(key_ctrl('f'), ACT_FORM, "edit (form)")
      .on_callback(key_ctrl('r'), [&fp, &next] {
          if (fp) {
              fp->add("Task " + std::to_string(next++) + " (added)");
              fp->refresh();
          }
          return true;   // keep the finder open
      }, "refresh");

    FuzzyOpts opts{};
    opts.prompt = "Find";
    opts.fullscreen = true;        // grow to fill, then scroll
    opts.valign = VALIGN;
    opts.header = hdr.get();
    opts.hide_summary = true;
    opts.box = BoxStyle{ .enabled = true,
                         .border = { .type = SC_BORDER_ROUNDED },
                         .width_mode = SC_WIDTH_FULL };
    sc.apply(opts);

    Fuzzy f(opts);
    fp = &f;
    for (int i = 1; i <= 5; ++i) {
        f.add("Task " + std::to_string(i));
    }
    std::optional<std::size_t> idx = f.run();
    action = sc.fired();
    if (!idx) {
        return false;
    }
    if (auto lbl = f.label(*idx)) {
        selected = *lbl;
    }
    return true;
}

// Runs the edit form full-screen under `hdr`, prefilled with `task`.
static void run_form(std::string_view task, const Rendered& hdr) {
    FormOpts fo{};
    fo.accent = palette::accent();
    fo.hide_summary = true;
    fo.fullscreen = true;
    fo.valign = VALIGN;
    fo.header = hdr.get();
    Form form(fo);
    FieldOpts full{};
    full.width_mode = SC_FWIDTH_PCT;
    full.width = 100;
    form.row_begin().add_text("Title", task, full);
    form.row_begin().add_text("Note", "", full);
    form.run();   // Ctrl-D saves, Esc cancels
}

int main() {
    if (!input_available()) {
        alert::warning("Run this in a real terminal (not under a pipe).");
        return 0;
    }

    AltScreen screen;   // owns the alt screen for the whole loop
    bool finder = true;
    std::string selected = "Task 1";
    for (;;) {
        Rendered hdr = build_header(finder, 5, selected);
        if (finder) {
            int action = -1;
            if (!run_finder(hdr, selected, action)) {
                break;            // esc quits
            }
            finder = false;       // Enter or ^F: edit the selected item
        } else {
            run_form(selected, hdr);
            finder = true;
        }
    }
    return 0;
}
