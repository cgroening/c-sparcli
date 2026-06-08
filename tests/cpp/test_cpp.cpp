// test_cpp.cpp - assertion suite for the C++ wrapper (include/sparcli.hpp).
//
// Unlike a smoke run, this verifies the wrapper's value-adds: that borrowed
// strings/texts are actually owned (temporaries stay valid), that a Table
// survives a move after rows were added, that List/Tree rich items outlive the
// temporaries they were built from, and that the wrapper renders byte-for-byte
// like the C API. Output is captured into an in-memory stream via ScopedOutput.
//
// Built and run by `make test-cpp`.

#include <sparcli.hpp>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

#include <unistd.h>

using namespace sparcli;

static int g_failures = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) {                                                            \
            std::printf("  \033[32m✔\033[0m %s\n", (msg));                \
        } else {                                                               \
            std::printf("  \033[31m✘\033[0m %s\n", (msg));                \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// Captures everything `fn` prints into a string (off a TTY → width 80).
template <class F>
static std::string render(F&& fn) {
    char*  buf = nullptr;
    size_t len = 0;
    FILE*  fp  = open_memstream(&buf, &len);
    if (!fp) { return {}; }
    { ScopedOutput redirect(fp); fn(); }
    std::fflush(fp);
    std::fclose(fp);
    std::string out(buf, len);
    std::free(buf);
    return out;
}

static bool contains(const std::string& haystack, const std::string& needle) {
    return strip_ansi(haystack).find(needle) != std::string::npos;
}

// A Table built entirely from temporaries must copy them into its arena;
// a borrow bug would leave dangling pointers and garbled/empty output.
static void test_table_owns_temporaries() {
    std::string out = render([] {
        Table t;
        t.add_column("Name");
        t.add_column("Score");
        for (int i = 1; i <= 3; ++i) {
            t.add_row({ "row " + std::to_string(i), std::to_string(i * 10) });
        }
        t.add_row({ cell_markup("[green]OK[/]"), cell("100") });
        t.print({ .header = { .row = true } });
    });
    CHECK(contains(out, "row 3"), "table: temporary cell string survives");
    CHECK(contains(out, "30"),    "table: temporary numeric cell survives");
    CHECK(contains(out, "OK"),    "table: markup cell renders");
}

// A null `const char*` cell must not crash (treated as empty), not std::string
// from nullptr (UB).
static void test_table_null_cell() {
    std::string out = render([] {
        Table t;
        t.add_column("A");
        t.add_column("B");
        t.add_row({ static_cast<const char*>(nullptr), "ok" });
        t.print();
    });
    CHECK(contains(out, "ok"), "table: null const char* cell is safe (empty)");
}

// Moving a Table after rows were added must keep the borrowed cell pointers
// valid (the string arena lives behind a unique_ptr).
static void test_table_survives_move() {
    auto build = [] {
        Table t;
        t.add_column("k");
        t.add_column("v");
        t.add_row({ std::string("alpha"), std::to_string(1) });
        t.add_row({ std::string("beta"),  std::to_string(2) });
        return t;
    };
    Table moved  = build();
    Table moved2 = std::move(moved);
    std::string out = render([&] { moved2.print(); });
    CHECK(contains(out, "alpha") && contains(out, "beta"),
          "table: cell strings survive a move");
}

// The C list borrows ScText; rich items built from temporaries (also in a
// sub-list whose view has already gone) must still render at print time.
static void test_list_text_arena() {
    std::string out = render([] {
        List l({ .marker = SC_LIST_NUMBER });
        l.add("plain");
        {
            auto item = l.add("parent");
            auto sub  = item.sub();
            sub.add_markup("[cyan]" + std::string("nested-rich") + "[/]");
        }
        l.add_markup("[bold]" + std::string("rich-root") + "[/]");
        l.print();
    });
    CHECK(contains(out, "nested-rich"), "list: sub-list rich item survives");
    CHECK(contains(out, "rich-root"),   "list: root rich item survives");
}

// The C tree borrows ScText; a markup node built from a temporary must survive.
static void test_tree_text_arena() {
    std::string out = render([] {
        Tree tr;
        auto root = tr.add("root");
        tr.add_markup("[green]" + std::string("leaf-rich") + "[/]", root);
        tr.print();
    });
    CHECK(contains(out, "leaf-rich"), "tree: rich node survives arena");
}

// A per-column style (ScColOpts.style) must be applied to unstyled data
// cells; per-cell markup styling must still win over it.
static void test_table_column_style() {
    std::string out = render([] {
        Table t;
        t.add_column("Name", { .style = { SC_TEXT_ATTR_BOLD,
                                          SC_ANSI_COLOR_CYAN,
                                          SC_ANSI_COLOR_NONE } });
        t.add_column("Score");
        t.add_row({ "alpha", "10" });
        t.add_row({ cell_markup("[red]beta[/]"), "20" });
        t.print();
    });
    CHECK(contains(out, "alpha") && contains(out, "beta"),
          "table: column-styled cells render");
    CHECK(out.find("\033[36m") != std::string::npos,
          "table: column style emits its color code");
    CHECK(out.find("\033[31m") != std::string::npos,
          "table: per-cell markup still wins over column style");
}

// The wrapper must forward to the C API byte-for-byte.
static void test_wrapper_matches_c() {
    std::string w = render([] {
        panel("hello", { .border = { .type = SC_BORDER_ROUNDED } });
    });
    std::string c = render([] {
        sc_panel_str(
            "hello", ScPanelOpts{ .border = { .type = SC_BORDER_ROUNDED } });
    });
    CHECK(w == c && !w.empty(), "wrapper panel output equals the C API");

    std::string wt = render([] {
        Text t;
        t.append("a");
        t.append_markup("[red]b[/]");
        t.print();
    });
    CHECK(contains(wt, "a") && contains(wt, "b"), "Text builder renders spans");
}

// Backtick inline code must render in magenta with the backticks removed;
// the MarkupOpts::code_style field must override the default via the wrapper.
static void test_markup_inline_code() {
    std::string out = render([] {
        Text::markup("run `make qa` now").print();
    });
    CHECK(out.find("\033[35m") != std::string::npos
              && contains(out, "run make qa now"),
          "backtick code renders magenta without the backticks");

    std::string custom = render([] {
        Text::markup(
            "see `code`",
            MarkupOpts{ .code_style = { .fg = SC_ANSI_COLOR_CYAN } })
            .print();
    });
    CHECK(custom.find("\033[36m") != std::string::npos
              && custom.find("\033[35m") == std::string::npos,
          "MarkupOpts::code_style overrides the default magenta");
}

// Text::append_link must emit an OSC-8 hyperlink wrapping the visible text;
// the escape bytes must not contribute to the visible width.
static void test_text_link() {
    Text t;
    t.append_link("click", "https://example.com");
    CHECK(t.visible_width() == 5, "link escape bytes have zero visible width");
    CHECK(t.span_count() == 1, "link is stored as one span");

    std::string out = render([] {
        Text link;
        link.append_link("click", "https://example.com");
        link.print();
    });
    CHECK(out.find("\033]8;;https://example.com") != std::string::npos
              && contains(out, "click"),
          "Text::append_link emits the OSC-8 hyperlink sequence");
}

// XDG path wrappers must reject invalid names, resolve below $XDG_*, and the
// Pager must be a clean no-op when the output stream is not a terminal.
static void test_paths_and_pager() {
    CHECK(!paths::config("a/b").has_value(),
          "paths: invalid app name yields empty optional");

    char sandbox_template[] = "/tmp/sparcli-cpp-XXXXXX";
    char* sandbox = mkdtemp(sandbox_template);
    if (sandbox) {
        setenv("XDG_CONFIG_HOME", sandbox, 1);
        auto dir = paths::config("cpptest");
        CHECK(dir.has_value() && dir->find(sandbox) == 0,
              "paths: config dir resolves below $XDG_CONFIG_HOME");
        unsetenv("XDG_CONFIG_HOME");
        if (dir) { rmdir(dir->c_str()); }
        rmdir(sandbox);
    }

    int pager_status = -1;
    std::string out = render([&pager_status] {
        Pager pager;   // output stream is a memstream -> no-op session
        println("paged content");
        pager_status = pager.end();
    });
    CHECK(pager_status == 0, "pager: no-op session ends with status 0");
    CHECK(contains(out, "paged content"),
          "pager: no-op session writes to the original stream");
}

// ErrorReport must render message/cause/hint through the alert panel and
// keep its exit code; die() is not testable in-process (it exits).
// Live display: an off-terminal session buffers updates and prints only the
// final frame when it ends; transient sessions print nothing.
static void test_live_display() {
    std::string out = render([] {
        Live live;
        live.update("frame one: starting");
        live.update("frame two: working");
        live.update(capture::str("frame three: done"));
        live.end();
    });
    CHECK(!contains(out, "frame one"),
          "live: intermediate string frames are not printed off-terminal");
    CHECK(!contains(out, "frame two"),
          "live: intermediate frames stay buffered until the end");
    CHECK(contains(out, "frame three: done"),
          "live: the final frame is printed when the session ends");

    std::string transient_out = render([] {
        Live live(LiveOpts{ .transient = true });
        live.update("never shown");
    });   // destructor ends the session
    CHECK(!contains(transient_out, "never shown"),
          "live: transient session prints nothing");
}

// Calculator: the pure expression evaluator and the NumberOpts fields.
static void test_calc_eval() {
    CHECK(calc_eval("1+2*3") == 7.0, "calc: precedence 1+2*3 == 7");
    CHECK(calc_eval("1,5+2*3") == 7.5, "calc: comma separator");
    CHECK(calc_eval("(1+2)*3") == 9.0, "calc: parentheses");
    CHECK(calc_eval("2*-3") == -6.0, "calc: unary minus");
    CHECK(!calc_eval("1/0").has_value(), "calc: division by zero is invalid");
    CHECK(!calc_eval("1++2").has_value(), "calc: '1++2' is invalid");
    CHECK(!calc_eval("garbage").has_value(), "calc: garbage is invalid");
}

// Arrow/special key chords and the fuzzy selection guard (parity with the
// CLI's --arrow-nav navigation).
static void test_special_keys() {
    CHECK(key_name(key_left()) == "\xe2\x86\x90", "key_left renders as left arrow");
    CHECK(key_name(key_right()) == "\xe2\x86\x92", "key_right renders as right arrow");
    CHECK(key_matches(Key{ .type = SC_KEY_LEFT }, key_left()),
          "decoded Left matches key_left()");
    CHECK(!key_matches(Key{ .type = SC_KEY_LEFT }, key_right()),
          "decoded Left does not match key_right()");

    // Modified named keys + case-sensitive char chords.
    CHECK(key_name(key_mod(SC_KEY_UP, SC_MOD_ALT)) == "M-\xe2\x86\x91",
          "key_mod(UP, ALT) renders as M-↑");
    CHECK(key_matches(Key{ .type = SC_KEY_UP, .mods = SC_MOD_SHIFT },
                      key_shift(SC_KEY_UP)),
          "decoded Shift+Up matches key_shift(UP)");
    CHECK(!key_matches(Key{ .type = SC_KEY_CHAR, .codepoint = 'P' },
                       key_char('p')),
          "case-sensitive: decoded 'P' does not match key_char('p')");

    Fuzzy fz({ .prompt = "Find" });
    fz.add("alpha").add("beta");
    CHECK(!fz.has_selection(), "fuzzy: no selection before a run");
}

static void test_error_report() {
    ErrorReport report("something broke");
    report.cause("first reason").hint("try --force").code(3);
    CHECK(report.exit_code() == 3, "errors: builder stores the exit code");

    std::string out = render([&report] { report.print(); });
    CHECK(contains(out, "something broke")
              && contains(out, "caused by: first reason")
              && contains(out, "Hint: try --force"),
          "errors: report renders message, cause and hint");

    // Builder temporaries are copied (the wrapper passes through to C)
    std::string rendered = render([] {
        ErrorReport(std::string("temp ") + "message").print();
    });
    CHECK(contains(rendered, "temp message"),
          "errors: temporary message strings are copied");
}

// Logger must format records, honor sink levels, and treat messages as
// data (a literal "%s"/"%d" in the message is never format-expanded).
static void test_logger() {
    char* buffer = nullptr;
    size_t size = 0;
    std::FILE* sink = open_memstream(&buffer, &size);
    if (!sink) {
        CHECK(false, "log: open_memstream available");
        return;
    }

    {
        Logger logger(LoggerOpts{ .hide_timestamps = true });
        logger.add_terminal(sink, SC_LOG_INFO);
        logger.info("c++ record with literal %d");
        logger.debug("filtered out");
    }
    std::fflush(sink);
    std::fclose(sink);

    std::string out(buffer ? buffer : "", size);
    std::free(buffer);
    CHECK(contains(out, "INFO")
              && contains(out, "c++ record with literal %d"),
          "log: Logger renders the message as data (no format expansion)");
    CHECK(!contains(out, "filtered out"),
          "log: sink level filters DEBUG records");

    // Global logger level roundtrip
    logging::set_level(SC_LOG_ERROR);
    CHECK(logging::level() == SC_LOG_ERROR, "log: global level roundtrip");
    logging::reset();
    CHECK(logging::level() == SC_LOG_INFO, "log: reset restores INFO");
}

// Args must build a tree from C++ temporaries (copies), parse argv with
// typed getters, and report help/error outcomes through status().
static void test_args_parser() {
    Args args(ArgsOpts{
        .prog = "cpptool", .version = "2.0", .about = "C++ wrapper demo" });
    ArgsCmd root = args.root();
    root.flag("verbose", 'v', std::string("Verbose ") + "output");

    ArgsCmd build = root.subcommand("build", "Build the project");
    build.group("Work")
        .opt("jobs", 'j', SC_ARG_INT, "N", "Parallel jobs")
        .opt_default("jobs", "4")
        .opt("mode", 'm', SC_ARG_STR, "MODE", "Build mode")
        .opt_choices("mode", { "debug", "release" })
        .positional("TARGET", SC_ARG_STR, "Build target", true, false);

    const char* argv[] = { "cpptool", "-v", "build", "--mode", "release",
                           "app" };
    auto matched = args.parse(6, const_cast<char**>(argv));
    CHECK(matched.has_value() && matched->name() == "build",
          "args: parse returns the matched subcommand");
    CHECK(args.status() == SC_ARGS_MATCHED && args.exit_code() == 0,
          "args: successful parse reports MATCHED / exit 0");
    CHECK(args.get_flag("verbose") && args.get_int("jobs") == 4
              && args.get_enum("mode") == 1
              && args.get_str("TARGET").value_or("") == "app",
          "args: typed getters return the parsed values");

    // --help is handled (status HANDLED, no match); capture the output
    Args help_args(ArgsOpts{ .prog = "cpptool", .about = "demo" });
    help_args.root().flag("flag", 'f', "A flag");
    std::string help_text = render([&help_args] {
        const char* help_argv[] = { "cpptool", "--help" };
        auto result = help_args.parse(2, const_cast<char**>(help_argv));
        CHECK(!result.has_value()
                  && help_args.status() == SC_ARGS_HANDLED,
              "args: --help reports HANDLED without a match");
    });
    CHECK(contains(help_text, "Usage: cpptool")
              && contains(help_text, "--flag"),
          "args: --help renders the usage screen");
}

// A command's handler is stored in the Args arena, fired by dispatch with the
// right exit code, and survives a move of the owning Args; the raw userdata<T>
// passthrough round-trips a typed pointer.
static void test_args_dispatch() {
    int ran = 0;  // which handler fired (1 = add, 2 = list)

    Args args(ArgsOpts{ .prog = "disp", .about = "dispatch demo" });
    args.root().subcommand("add", "Add an item")
        .positional("TEXT", SC_ARG_STR, "Item text", true, false)
        .handler([&ran](const Args& a) {
            ran = 1;
            return a.get_str("TEXT").value_or("") == "milk" ? 7 : 99;
        });
    ArgsCmd list = args.root().subcommand("list", "List items");
    list.handler([&ran](const Args&) { ran = 2; return 3; });

    const char* argv[] = { "disp", "add", "milk" };
    auto matched = args.parse(3, const_cast<char**>(argv));
    CHECK(matched.has_value() && args.has_handler(*matched),
          "dispatch: the matched command reports a handler");
    CHECK(args.dispatch(*matched) == 7 && ran == 1,
          "dispatch: fires the right handler with the parsed args / exit code");

    // Raw typed passthrough (no arena, pointer is the caller's)
    int sentinel = 42;
    args.root().subcommand("raw", "raw userdata").userdata(&sentinel);
    auto raw = args.parse_line("raw");
    CHECK(raw.has_value() && raw->userdata<int>() == &sentinel
              && !args.has_handler(*raw),
          "userdata: typed void* passthrough round-trips");

    // Move the parser: handlers live on the heap, so the stored void* stays
    // valid and dispatch still works on the moved-to object.
    Args moved = std::move(args);
    const char* argv2[] = { "disp", "list" };
    auto m2 = moved.parse(2, const_cast<char**>(argv2));
    CHECK(m2.has_value() && moved.dispatch(*m2) == 3 && ran == 2,
          "dispatch: handlers survive a move of the owning Args");
}

// The REPL helpers: quote-aware tokenizing (args_split / parse_line) and
// result clearing (reset / implicit reset on re-parse).
static void test_args_repl_helpers() {
    // Standalone tokenizer
    auto tokens = args_split("add \"Buy milk\" --due friday");
    CHECK(tokens.has_value() && tokens->size() == 4
              && (*tokens)[1] == "Buy milk",
          "split: quote-aware tokens (without argv[0])");

    std::string error;
    CHECK(!args_split("add \"oops", &error).has_value()
              && error == "unterminated quote",
          "split: unbalanced quote reports the reason");

    // parse_line: tokenize + parse in one call, reusable per REPL line
    Args args(ArgsOpts{ .prog = "repl", .about = "REPL helper test" });
    ArgsCmd add = args.root().subcommand("add", "Add an item");
    add.opt("due", 'd', SC_ARG_STR, "WHEN", "Due date")
        .positional("TEXT", SC_ARG_STR, "Item text", true, false);
    args.root().subcommand("list", "List items");

    auto first = args.parse_line("add \"Buy milk\" --due friday");
    CHECK(first.has_value() && first->name() == "add"
              && args.get_str("TEXT").value_or("") == "Buy milk"
              && args.get_str("due").value_or("") == "friday",
          "parse_line: quoted line parses into the tree");

    // The second line on the same tree must not see stale values
    auto second = args.parse_line("list");
    CHECK(second.has_value() && second->name() == "list"
              && !args.present("due"),
          "parse_line: re-parse clears the previous results");

    // Explicit reset clears without parsing
    args.reset();
    CHECK(!args.selected_command().has_value(),
          "reset: matched command is cleared");
}

// History must copy entries, dedupe consecutive duplicates, and respect the
// entry cap; the move-only RAII wrapper frees the handle exactly once.
static void test_history() {
    History history;
    history.add("first").add("first").add("second");
    CHECK(history.count() == 2,
          "history: consecutive duplicates collapse");
    CHECK(history.get(0).value_or("") == "first"
              && history.get(1).value_or("") == "second",
          "history: entries are retrievable oldest-first");
    CHECK(!history.get(99).has_value(),
          "history: out-of-range returns an empty optional");

    // Entry cap via opts (C++20 designated initializers on the C struct)
    History capped(HistoryOpts{ .max_entries = 2 });
    capped.add("one").add("two").add("three");
    CHECK(capped.count() == 2 && capped.get(0).value_or("") == "two",
          "history: the cap evicts the oldest entry");

    // apply() wires the handle into a text input's opts
    TextInputOpts opts{};
    capped.apply(opts);
    CHECK(opts.history == capped.get(),
          "history: apply() attaches the handle to the opts");

    // Move semantics: the moved-from handle must not double-free
    History moved = std::move(capped);
    CHECK(moved.count() == 2, "history: handle survives a move");
}

// version(), color_by_name(), the char-filter aliases and the rich-text Live
// overloads added for full C-API parity.
static void test_parity_helpers() {
    // version() mirrors sc_version_string().
    CHECK(version_string() == std::string(sc_version_string()),
          "version: string matches the C accessor");
    CHECK(version().major >= 0, "version: components are populated");

    // color_by_name resolves ANSI + palette names, rejects unknowns/hex.
    auto c_red = color_by_name("red");
    CHECK(c_red.has_value() && c_red->index == sparcli::red().index,
          "color_by_name: 'red' is the ANSI red");
    auto accent = color_by_name("accent");
    CHECK(accent.has_value() && accent->index == -1,
          "color_by_name: 'accent' is a 24-bit palette color");
    CHECK(!color_by_name("#ff0000").has_value(),
          "color_by_name: hex is not a name");
    CHECK(!color_by_name("nope").has_value(),
          "color_by_name: unknown name -> nullopt");

    // The char-filter aliases point at the C built-ins.
    CHECK(filters::digits == sc_filter_digits,
          "filters: digits aliases sc_filter_digits");
    CHECK(filters::decimal == sc_filter_decimal &&
          filters::no_space == sc_filter_no_space,
          "filters: decimal/no_space alias the C built-ins");

    // Live overloads for rich text and tables: only the final frame prints
    // off-terminal.
    std::string out = render([] {
        Live live;
        live.update(Text("text frame"));
        Table t;
        t.add_column("C");
        t.add_row({ "final cell" });
        live.update(t);
        live.end();
    });
    CHECK(!contains(out, "text frame"),
          "live: superseded rich-text frame is dropped");
    CHECK(contains(out, "final cell"),
          "live: final table frame is printed");
}

// ── humanize / diff / multiprogress / process wrappers ──────────────────────

static void test_humanize_wrapper() {
    CHECK(humanize::bytes(1536) == "1.5 KB", "humanize: bytes SI");
    CHECK(humanize::bytes(1536, SC_BYTES_IEC) == "1.5 KiB",
          "humanize: bytes IEC");
    CHECK(humanize::number(1234567) == "1,234,567", "humanize: number");
    CHECK(humanize::compact(12400) == "12.4k", "humanize: compact");
    CHECK(humanize::percent(0.42) == "42%", "humanize: percent");
    CHECK(humanize::duration(93) == "1m 33s", "humanize: duration");
    CHECK(humanize::duration_clock(3725) == "01:02:05",
          "humanize: clock");
    HumanizeOpts de{ .decimal_sep = ',' };
    CHECK(humanize::bytes(1536, SC_BYTES_IEC, de) == "1,5 KiB",
          "humanize: de_DE decimal separator");
}

// Headless coverage of the Fuzzy section / multi / style additions.
static void test_fuzzy_wrapper() {
    Fuzzy f{ FuzzyOpts{ .multi = true } };
    f.add("alpha").add("beta").add("gamma");
    f.set_id(0, 100).set_id(2, 300);
    CHECK(f.id_at(0) == 100 && f.id_at(2) == 300, "fuzzy++: set_id/id_at");
    CHECK(f.label(1) && *f.label(1) == "beta", "fuzzy++: label");
    f.set_label(1, "BETA");
    CHECK(*f.label(1) == "BETA", "fuzzy++: set_label");

    f.set_checked(0).set_checked(2);
    CHECK(f.is_checked(0) && !f.is_checked(1) && f.checked_count() == 2,
          "fuzzy++: checked set");
    f.check_all(true);
    CHECK(f.checked_count() == 3, "fuzzy++: check_all");
    f.set_disabled(1, true);
    f.check_all(true);
    CHECK(!f.is_checked(1), "fuzzy++: check_all skips disabled");

    // Styled / rich section headers construct cleanly (per-section bg/fg).
    Fuzzy s{ FuzzyOpts{ .prompt = "Tasks" } };
    s.add_section_styled("OVERDUE",
        { SC_TEXT_ATTR_BOLD, sparcli::white(), sparcli::red() });
    s.add("pay invoice");
    Text htitle;
    htitle.append("Due ", { SC_TEXT_ATTR_BOLD, sparcli::white(),
                            sparcli::magenta() });
    htitle.append("soon", { SC_TEXT_ATTR_UNDER, sparcli::yellow(),
                            sparcli::magenta() });
    s.add_section_text(htitle, { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE,
                                 sparcli::magenta() });
    s.add("finish slides");
    CHECK(!s.has_selection(), "fuzzy++: styled/rich sections build");

    // add_row_rich flattens span text into the label/match key.
    Fuzzy g{ FuzzyOpts{ .table = true, .n_cols = 1 } };
    std::vector<Text> cells;
    cells.emplace_back();
    cells[0].append("HI").append("GH", { SC_TEXT_ATTR_BOLD,
                    SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
    g.add_row_rich(cells);
    CHECK(g.label(0) && *g.label(0) == "HIGH",
          "fuzzy++: add_row_rich flattens spans");

    // Modal opts (incl. a bare-char clear key) construct cleanly.
    Fuzzy m{ FuzzyOpts{
        .modal = true, .start_in_insert = true,
        .clear_key = key_char('c'),
        .normal_label = "CMD",
        .mode_normal_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                               SC_ANSI_COLOR_BLUE } } };
    m.add("x").add("y");
    CHECK(!m.has_selection(), "fuzzy++: modal opts build");
}

static void test_fullscreen_wrapper() {
    // A pinned header (borrowed); declared first so it outlives the widgets.
    Rendered header = capture::str("== header ==");

    Fuzzy f{ FuzzyOpts{ .prompt = "Find", .fullscreen = true,
                        .valign = SC_VALIGN_MIDDLE, .header = header.get() } };
    f.add("alpha").add("beta");
    CHECK(!f.has_selection(), "fuzzy++: fullscreen opts build");

    Form form{ FormOpts{ .fullscreen = true, .valign = SC_VALIGN_BOTTOM,
                         .header = header.get() } };
    form.row_begin();
    int t = form.add_text("Title", "x");
    CHECK(form.get_string(t) == "x", "form++: fullscreen opts build");

    { AltScreen screen; }   // no-op off a terminal; just exercises begin/drop
    CHECK(true, "altscreen++: scope constructs/destructs");
}

static void test_diff_wrapper() {
    std::string out = render([] {
        diff("a\nb\nc\n", "a\nB\nc\n", DiffOpts{ .context = 0 });
    });
    std::string plain = strip_ansi(out);
    CHECK(contains(plain, "-b") && contains(plain, "+B"),
          "diff: removed/added lines present");
    Text t = diff_text("x\n", "y\n", DiffOpts{ .no_header = true });
    CHECK(t.visible_width() > 0, "diff: diff_text builds rich text");
    Rendered r = capture::diff("x\n", "y\n");
    CHECK(true, "diff: capture::diff returns a Rendered");
    (void)r;
}

static void test_multiprogress_wrapper() {
    // Off a memstream the live display buffers; the final stack is printed.
    std::string out = render([] {
        MultiProgress mp;
        ProgressBarOpts bo{ .left_cap = "[", .right_cap = "]" };
        bo.bar_width = 10;
        bo.show_percent = true;
        int a = mp.add("a", bo);
        int b = mp.add("b", bo);
        mp.update(a, 100, 100);
        mp.update(b, 50, 100);
        mp.end();
    });
    std::string plain = strip_ansi(out);
    CHECK(contains(plain, "a") && contains(plain, "b")
              && contains(plain, "100%"),
          "multiprogress: final stack printed");
}

static void test_process_wrapper() {
    ProcResult r = run({ "echo", "hi there" });
    CHECK(r.ok() && r.exit_code() == 0, "process: echo runs");
    CHECK(contains(std::string(r.out()), "hi there"),
          "process: stdout captured as string_view");

    ProcResult miss = run({ "sparcli-no-such-binary-xyz" });
    CHECK(miss.ok() && miss.exit_code() == 127,
          "process: missing command → 127");

    ProcResult moved = run({ "true" });
    ProcResult sink = std::move(moved);   // move-safety
    CHECK(sink.exit_code() == 0, "process: result survives a move");
}

static void test_edit_file_wrapper() {
    // Force the no-TTY path so the editor refuses to launch and reports -1
    // (the happy path needs a terminal; covered by the PTY suite).
    setenv("SPARCLI_NO_TTY", "1", 1);
    CHECK(edit_file("true", "/tmp/sparcli-cpp-noopen") == -1,
          "edit_file: refuses (-1) with no terminal");
    CHECK(edit_file("") == -1, "edit_file: empty path → -1");
    unsetenv("SPARCLI_NO_TTY");
}

// Headless coverage of the Form wrapper (construction + initial getters).
static void test_form_wrapper() {
    Form f{ FormOpts{ .title = "Contact", .accent = sparcli::cyan() } };
    f.row_begin();
    int name = f.add_text("Name", "Ada",
        FieldOpts{ .width_mode = SC_FWIDTH_PCT, .width = 50 });
    int qty  = f.add_number("Qty", 7);
    int tier = f.add_select("Tier", { "Bronze", "Silver", "Gold" }, 2);
    int tags = f.add_multiselect("Tags", { "a", "b", "c" }, { 0, 2 });
    int ok   = f.add_bool("OK", true);

    CHECK(name == 0 && qty == 1 && tier == 2 && tags == 3 && ok == 4,
          "form++: field indices in add order");
    CHECK(f.get_string(name) == "Ada", "form++: text getter");
    CHECK(f.get_number(qty) == 7.0, "form++: number getter");
    CHECK(f.get_bool(ok), "form++: bool getter");
    CHECK(f.get_choice(tier) == 2, "form++: select getter");
    auto checked = f.get_checked(tags);
    CHECK(checked.size() == 2 && checked[0] == 0 && checked[1] == 2,
          "form++: multiselect getter");

    std::tm seed{};
    seed.tm_year = 2026 - 1900; seed.tm_mon = 4; seed.tm_mday = 15;
    int when = f.add_date("When", seed);
    auto d = f.get_date(when);
    CHECK(d && d->tm_year == 2026 - 1900 && d->tm_mon == 4 && d->tm_mday == 15,
          "form++: date getter");
    CHECK(!f.get_date(name).has_value(), "form++: get_date on non-date = none");

    // A multiline field opening an external editor constructs cleanly.
    Form m{ FormOpts{ .editor = "true" } };
    m.add_text("Notes", "x\ny", FieldOpts{ .multiline = true });
    CHECK(m.get_string(0) == std::string("x\ny"),
          "form++: multiline keeps newlines");

    // autoedit (open the first field's editor at start) is exposed via the alias.
    Form a{ FormOpts{ .autoedit = true } };
    a.add_text("Title", "draft", FieldOpts{});
    CHECK(a.get_string(0) == std::string("draft"), "form++: autoedit opt builds");

    // modified()/modified_marker: a freshly added field is not modified.
    Form md{ FormOpts{ .modified_marker = "[*] " } };
    md.add_text("Title", "x", FieldOpts{});
    CHECK(!md.modified(), "form++: modified() is false before any edit");

    // read_only / not_selectable build via the FieldOpts alias and the value
    // stays readable (display-only, unfocusable field).
    Form ro{ FormOpts{} };
    ro.row_begin();
    int rep = ro.add_text("Repeat", "weekly",
        FieldOpts{ .read_only = true, .not_selectable = true });
    ro.add_text("Title", "t", FieldOpts{});
    CHECK(ro.get_string(rep) == std::string("weekly"),
          "form++: read_only/not_selectable field readable");
}

// Headless coverage of the shortcut help screen wrapper (build + no-TTY show).
static void test_shortcut_help_wrapper() {
    Shortcuts sc;
    sc.section("Actions")
      .on_return(key_ctrl('n'), 1, ShortcutDisplay{ .footer = "new",
                                                     .help = "create an item" })
      .on_callback(key_ctrl('x'), [] { return true; },
                   ShortcutDisplay{ .footer = "delete", .in_footer = false })
      .section("Navigation")
      .help_row("\xe2\x86\x91/\xe2\x86\x93", "move cursor");
    CHECK(sc.fired() == -1, "help++: nothing fired yet");
    CHECK(sc.help_rows().size() == 5, "help++: section+rows recorded in order");
    // show_shortcuts is a no-op without a TTY; it must return cleanly.
    show_shortcuts(sc, ShortcutHelpOpts{ .title = "Keys" });
}

// Strikethrough attribute + runtime palette override (both new this cycle).
static void test_strike_and_palette() {
    // SC_TEXT_ATTR_STRIKE emits the ANSI strike code; check the raw (un-stripped)
    // output, both via the attribute and the [strike] markup tag.
    std::string raw = render([] {
        print("done", style(SC_TEXT_ATTR_STRIKE));
        markup::print("[strike]x[/] [s]y[/]");
    });
    CHECK(raw.find("\x1b[9m") != std::string::npos,
          "strike: SC_TEXT_ATTR_STRIKE / [strike] emit ESC[9m");

    // palette::set overrides a name at runtime; markup [accent] follows it.
    palette::set("accent", sparcli::red());
    std::string acc = render([] { markup::print("[accent]hi[/]"); });
    CHECK(acc.find("\x1b[31m") != std::string::npos,
          "palette: set('accent') recolors [accent] markup");
    palette::reset();
    auto back = color_by_name("accent");
    CHECK(back.has_value() && back->index == -1,
          "palette: reset restores the default accent");
}

int main() {
    std::printf("\nC++ wrapper assertion suite:\n");
    test_form_wrapper();
    test_shortcut_help_wrapper();
    test_table_owns_temporaries();
    test_table_null_cell();
    test_table_survives_move();
    test_list_text_arena();
    test_tree_text_arena();
    test_table_column_style();
    test_wrapper_matches_c();
    test_markup_inline_code();
    test_text_link();
    test_paths_and_pager();
    test_live_display();
    test_calc_eval();
    test_special_keys();
    test_error_report();
    test_logger();
    test_args_parser();
    test_args_dispatch();
    test_args_repl_helpers();
    test_history();
    test_parity_helpers();
    test_strike_and_palette();
    test_humanize_wrapper();
    test_fuzzy_wrapper();
    test_fullscreen_wrapper();
    test_diff_wrapper();
    test_multiprogress_wrapper();
    test_process_wrapper();
    test_edit_file_wrapper();

    if (g_failures > 0) {
        std::printf("\033[31m%d check(s) failed.\033[0m\n", g_failures);
        return 1;
    }
    std::printf("\033[32mAll C++ wrapper checks passed.\033[0m\n");
    return 0;
}
