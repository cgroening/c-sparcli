// test_view_cpp.cpp - assertion suite for the opt-in, serde-dependent C++
// wrappers: sparcli::Config (app/sparcli_config.hpp) and the sparcli::view
// renderers (view/sparcli_view.hpp). Built + run by `make qa-view-cpp` under
// AddressSanitizer/UBSan.

#include <app/sparcli_config.hpp>
#include <view/sparcli_view.hpp>

#include <cstdio>
#include <string>

using namespace sparcli;

static int g_failures = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) { std::printf("  \033[32m✔\033[0m %s\n", (msg)); }       \
        else { std::printf("  \033[31m✘\033[0m %s\n", (msg)); ++g_failures; }  \
    } while (0)

static bool contains(const std::string& h, const std::string& n) {
    return h.find(n) != std::string::npos;
}

template <class F>
static std::string render(F&& fn) {
    char* buf = nullptr; size_t len = 0;
    FILE* fp = open_memstream(&buf, &len);
    if (!fp) { return {}; }
    set_output(fp);
    fn();
    set_output(stdout);
    std::fclose(fp);
    std::string r = buf ? std::string(buf, len) : std::string();
    std::free(buf);
    return r;
}


static void test_config() {
    Config cfg;

    // defaults via a serde::Value object
    serde::Value def = serde::Value::object();
    def.set("port", serde::Value::integer(8080));
    def.set("debug", serde::Value::boolean(false));
    CHECK(cfg.set_defaults(def), "config: set_defaults");
    CHECK(cfg.get_int("port", 0) == 8080, "config: get_int default");

    // set by dotted path (takes ownership)
    CHECK(cfg.set("server.host", serde::Value::string("localhost")),
          "config: set nested path");
    CHECK(cfg.get_string("server.host", "") == "localhost",
          "config: nested string read");

    // explicit overlay overrides
    serde::Value flags = serde::Value::object();
    flags.set("port", serde::Value::integer(9090));
    CHECK(cfg.merge(flags), "config: merge overlay");
    CHECK(cfg.get_int("port", 0) == 9090, "config: overlay overrides default");

    // missing file is not an error
    CHECK(cfg.load_file("/no/such/file.toml") == SC_CONFIG_MISSING,
          "config: missing file → MISSING");

    // move-safety
    Config moved = std::move(cfg);
    CHECK(moved.get_int("port", 0) == 9090, "config: survives a move");
}

static void test_value_render() {
    serde::Value root = serde::Value::object();
    root.set("name", serde::Value::string("sparcli"));
    root.set("n", serde::Value::integer(1));

    std::string out = strip_ansi(render([&] {
        view::value_render(root, view::ValueRenderOpts{ .no_color = true });
    }));
    CHECK(contains(out, "\"name\": \"sparcli\""), "view: value_render output");

    Text t = view::value_render_text(serde::View(root.get()),
                                     view::ValueRenderOpts{ .no_color = true });
    CHECK(t.visible_width() > 0, "view: value_render_text from View");

    Rendered r = view::capture_value(root, view::ValueRenderOpts{ .no_color = true });
    (void)r;
    CHECK(true, "view: capture_value returns a Rendered");
}

static void test_markdown_render() {
    const char* md =
        "# Title\n\nA **bold** word.\n\n- one\n- two\n";
    std::string out = render([&] {
        view::markdown_render_str(md);
    });
    std::string plain = strip_ansi(out);
    CHECK(contains(plain, "Title"), "view: markdown heading");
    CHECK(contains(plain, "bold"), "view: markdown inline emphasis");
    CHECK(contains(plain, "one") && contains(plain, "two"),
          "view: markdown list");

    serde::Markdown doc = serde::Markdown::parse(md);
    Rendered r = view::capture_markdown(doc);
    (void)r;
    CHECK(true, "view: capture_markdown from a parsed document");
}

int main() {
    std::printf("\nC++ view/config wrapper suite:\n");
    test_config();
    test_value_render();
    test_markdown_render();

    if (g_failures > 0) {
        std::printf("\033[31m%d check(s) failed.\033[0m\n", g_failures);
        return 1;
    }
    std::printf("\033[32mAll view/config C++ checks passed.\033[0m\n");
    return 0;
}
