#include "test_serde.h"

#include <unistd.h>


// Forward declarations indented to reflect the call hierarchy.
static void check_json_file(void);
static void check_toml_file(void);
static void check_yaml_file(void);
static void check_csv_file(void);
static void check_markdown_file(void);
static void check_missing_file(void);
    static bool temp_path(char *buf, size_t size);


/** Exercises the per-format `*_parse_file` / `*_write_file` wrappers. */
void test_serde_file(void) {
    check_json_file();
    check_toml_file();
    check_yaml_file();
    check_csv_file();
    check_markdown_file();
    check_missing_file();
}


/* ── One round trip per format ───────────────────────────────────────────── */

static void check_json_file(void) {
    char path[64];
    if (!temp_path(path, sizeof path)) {
        CHECK(false, "json file: mkstemp failed");
        return;
    }
    ScValue *root = sc_json_parse("{\"k\":42}", 8, NULL);
    bool wrote = sc_json_write_file(root, path, (ScJsonWriteOpts){ 0 });
    sc_value_free(root);

    ScValue *back = sc_json_parse_file(path, NULL);
    int64_t value = 0;
    CHECK(wrote && sc_value_as_int(sc_value_get(back, "k"), &value)
              && value == 42,
          "json file: write_file -> parse_file round-trips");
    sc_value_free(back);
    unlink(path);
}

static void check_toml_file(void) {
    char path[64];
    if (!temp_path(path, sizeof path)) {
        CHECK(false, "toml file: mkstemp failed");
        return;
    }
    const char *toml = "[server]\nport = 8080\n";
    ScValue *root = sc_toml_parse(toml, strlen(toml), NULL);
    bool wrote = sc_toml_write_file(root, path, (ScTomlWriteOpts){ 0 });
    sc_value_free(root);

    ScValue *back = sc_toml_parse_file(path, NULL);
    int64_t port = 0;
    CHECK(wrote && sc_value_as_int(sc_value_path(back, "server.port"), &port)
              && port == 8080,
          "toml file: write_file -> parse_file round-trips");
    sc_value_free(back);
    unlink(path);
}

static void check_yaml_file(void) {
    char path[64];
    if (!temp_path(path, sizeof path)) {
        CHECK(false, "yaml file: mkstemp failed");
        return;
    }
    const char *yaml = "name: sparcli\ntags:\n  - a\n  - b\n";
    ScValue *root = sc_yaml_parse(yaml, strlen(yaml), NULL);
    bool wrote = sc_yaml_write_file(root, path, (ScYamlWriteOpts){ 0 });
    sc_value_free(root);

    ScValue *back = sc_yaml_parse_file(path, NULL);
    CHECK(wrote && back != NULL
              && strcmp(sc_value_as_string(sc_value_path(back, "tags.1")), "b")
                     == 0,
          "yaml file: write_file -> parse_file round-trips");
    sc_value_free(back);
    unlink(path);
}

static void check_csv_file(void) {
    char path[64];
    if (!temp_path(path, sizeof path)) {
        CHECK(false, "csv file: mkstemp failed");
        return;
    }
    const char *csv = "name,role\nAda,author\n";
    ScCsv *doc = sc_csv_parse(
        csv, strlen(csv), (ScCsvOpts){ .has_header = true }, NULL
    );
    bool wrote = sc_csv_write_file(doc, path);
    sc_csv_free(doc);

    ScCsv *back = sc_csv_parse_file(
        path, (ScCsvOpts){ .has_header = true }, NULL
    );
    CHECK(wrote && back != NULL
              && strcmp(sc_csv_get(back, 0, "role"), "author") == 0,
          "csv file: write_file -> parse_file round-trips");
    sc_csv_free(back);
    unlink(path);
}

static void check_markdown_file(void) {
    char path[64];
    if (!temp_path(path, sizeof path)) {
        CHECK(false, "markdown file: mkstemp failed");
        return;
    }
    const char *doc = "# Title\nBody.\n## Sub\nMore.\n";
    ScMarkdown *md = sc_markdown_parse(doc, strlen(doc), NULL);
    bool wrote = sc_markdown_write_file(md, path);
    sc_markdown_free(md);

    ScMarkdown *back = sc_markdown_parse_file(path, NULL);
    ScMdSection *sub = back
        ? sc_md_section_find(sc_markdown_root(back), "Title/Sub")
        : NULL;
    CHECK(wrote && sub != NULL
              && strcmp(sc_md_section_body(sub), "More.") == 0,
          "markdown file: write_file -> parse_file round-trips");
    sc_markdown_free(back);
    unlink(path);
}

static void check_missing_file(void) {
    ScParseError err = { 0 };
    ScValue *missing = sc_json_parse_file("/nonexistent/sparcli/zz.json", &err);
    CHECK(missing == NULL && err.message[0] != '\0',
          "file: a missing file reports an error (with the path)");
}


/* ── Helpers ─────────────────────────────────────────────────────────────── */

/** Creates a unique temp file and returns its path (the file is left empty). */
static bool temp_path(char *buf, size_t size) {
    snprintf(buf, size, "/tmp/sparcli_serde_file_XXXXXX");
    int fd = mkstemp(buf);
    if (fd < 0) {
        return false;
    }
    close(fd);
    return true;
}
