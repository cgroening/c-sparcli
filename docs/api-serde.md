# sparcli – Serde (data parsers) API

The **serde** layer adds structured read/write parsers on top of sparcli: JSON,
CSV, TOML, YAML and Markdown. It is a data-only layer – no terminal output, no
input – built around one shared in-memory model, `ScValue`, so the tree formats
(JSON/TOML/YAML) interoperate and converting between them is a parse-then-write
round trip.

It is **opt-in and separate** from the rest of the library:

- Include `<serde/sparcli_serde.h>` (C) or `<serde/sparcli_serde.hpp>` (C++).
  It is **not** pulled in by `<sparcli.h>` / `<sparcli.hpp>`.
- It depends only on the core utilities (and `app/error` for the pretty-error
  bridge); it never touches the output/input stack.
- It ships in the same `libsparcli` as everything else, but has its own test
  gates (`make serde-qa`), kept out of `make test`/`make qa`.
- **C and C++ only.** Rust has `serde`/`toml`/`serde_yaml` and Python its own
  ecosystem, so those bindings deliberately don't wrap this layer.

Per-format headers: `serde/sparcli_value.h`, `serde/sparcli_parse_error.h`,
`serde/sparcli_json.h`, `serde/sparcli_csv.h`, `serde/sparcli_toml.h`,
`serde/sparcli_yaml.h`, `serde/sparcli_markdown.h` (the umbrella pulls them all).

---

## The `ScValue` data model

`ScValue` is a recursive tree of scalars, arrays and objects – the shared model
for JSON, TOML and YAML. CSV (tabular) and Markdown (document outline) have
their own models, described in their sections.

```c
typedef enum ScValueType {
    SC_VALUE_NULL, SC_VALUE_BOOL, SC_VALUE_INT, SC_VALUE_FLOAT,
    SC_VALUE_STRING, SC_VALUE_ARRAY, SC_VALUE_OBJECT, SC_VALUE_DATETIME,
} ScValueType;

typedef struct ScValue ScValue;   // opaque
```

`SC_VALUE_INT` is `int64_t`, `SC_VALUE_FLOAT` is `double`. `SC_VALUE_DATETIME`
holds a date/time as its source string (produced by TOML/YAML, never
interpreted). Numbers are **not** stored losslessly as text – a value that needs
exact decimals (currency) should keep its own string.

### Construction

Each returns a heap node, or `NULL` on allocation failure.

```c
ScValue *sc_value_null(void);
ScValue *sc_value_bool(bool value);
ScValue *sc_value_int(int64_t value);
ScValue *sc_value_float(double value);
ScValue *sc_value_string(const char *value);   // copied; NULL = ""
ScValue *sc_value_array(void);
ScValue *sc_value_object(void);
```

### Inspection

```c
ScValueType sc_value_type(const ScValue *value);          // NULL → SC_VALUE_NULL
size_t      sc_value_len(const ScValue *value);           // array/object length

bool        sc_value_as_bool(const ScValue *value, bool *out);
bool        sc_value_as_int(const ScValue *value, int64_t *out);
bool        sc_value_as_float(const ScValue *value, double *out);   // int promoted
const char *sc_value_as_string(const ScValue *value);     // NULL if not string

ScValue    *sc_value_at(const ScValue *value, size_t index);        // array; NULL OOB
ScValue    *sc_value_get(const ScValue *value, const char *key);    // object; NULL absent
const char *sc_value_key_at(const ScValue *value, size_t index);    // object key by pos
```

The typed accessors return `false`/`NULL` on a type mismatch (never abort), so
navigation is null-tolerant: `sc_value_get` / `sc_value_at` return `NULL` for a
missing key, an out-of-range index, or a wrong container type.

### Mutation – ownership transfers to the container

```c
bool sc_value_push(ScValue *array, ScValue *child);                 // append
bool sc_value_set(ScValue *object, const char *key, ScValue *child);// add/replace
```

On success the container **owns** `child` (no deep copy). On failure (wrong
type, `NULL` arg, or OOM) the call returns `false` and ownership stays with the
caller. `sc_value_set` replaces an existing key in place (freeing the old value)
and keeps its position; objects preserve insertion order.

```c
void sc_value_free(ScValue *value);   // recursively frees the tree; NULL-safe
```

A tree is single-rooted: one `sc_value_free` on the root releases everything.

```c
ScValue *root = sc_value_object();
sc_value_set(root, "name", sc_value_string("sparcli"));
ScValue *tags = sc_value_array();
sc_value_push(tags, sc_value_string("cli"));
sc_value_set(root, "tags", tags);          // root now owns tags
char *json = sc_json_write(root, (ScJsonWriteOpts){ .indent = 2 });
puts(json); free(json);
sc_value_free(root);
```

---

## Parse errors: `ScParseError`

Every `sc_<format>_parse` reports failures through a caller-supplied, stack-only
`ScParseError` (no allocation – like the `sc_args_split` error buffer):

```c
typedef struct ScParseError {
    int  line;     // 1-based; 0 when unset
    int  column;   // 1-based; 0 when unset
    char message[SC_PARSE_ERROR_MESSAGE_MAX];
    char snippet[SC_PARSE_ERROR_SNIPPET_MAX];   // the offending source line
} ScParseError;

ScError *sc_parse_error_to_error(const ScParseError *error);   // → pretty panel
```

`sc_parse_error_to_error` turns it into a renderable `ScError` (exit code `2`)
from `app/sparcli_error.h`, so a CLI can `sc_die(sc_parse_error_to_error(&err))`
to print a red panel with the location and exit.

```c
ScParseError err = { 0 };
ScValue *root = sc_json_parse(text, strlen(text), &err);
if (!root) {
    sc_die(sc_parse_error_to_error(&err));   // renders to stderr + exits
}
```

The `err` pointer is optional everywhere – pass `NULL` if you only need the
`NULL` return.

---

## JSON

Strict RFC 8259 reader (no trailing commas, no comments, no trailing data;
depth-limited) and a round-trippable writer.

```c
typedef struct ScJsonWriteOpts {
    int  indent;            // spaces per level; 0 = compact, single line
    bool sort_keys;         // emit object keys in ascending byte order
    bool trailing_newline;  // append a final newline
} ScJsonWriteOpts;

ScValue *sc_json_parse(const char *src, size_t len, ScParseError *err);
char    *sc_json_write(const ScValue *value, ScJsonWriteOpts opts);  // heap; free
```

`\uXXXX` escapes (including surrogate pairs) decode to UTF-8. The writer emits
round-trippable numbers (a whole float keeps a `.0` so it re-parses as a float)
and escapes control bytes; a non-finite float is written as `null`.

---

## CSV

RFC-4180-style CSV/TSV with quoted fields (embedded delimiters, newlines, `""`
escapes), ragged rows, and an optional header row. Tabular model (`ScCsv`), not
`ScValue`.

```c
typedef struct ScCsvOpts {
    char delim;       // 0 = ','      (use '\t' for TSV)
    char quote;       // 0 = '"'
    bool has_header;  // row 0 is the header (enables sc_csv_header / sc_csv_get)
} ScCsvOpts;

typedef struct ScCsv ScCsv;   // opaque
```

### Reading

```c
ScCsv      *sc_csv_parse(const char *src, size_t len, ScCsvOpts opts,
                         ScParseError *err);
size_t      sc_csv_rows(const ScCsv *csv);                 // total rows
size_t      sc_csv_cols(const ScCsv *csv);                 // widest row
size_t      sc_csv_row_cols(const ScCsv *csv, size_t row);
const char *sc_csv_at(const ScCsv *csv, size_t row, size_t col);  // "" if OOB

bool        sc_csv_has_header(const ScCsv *csv);
const char *sc_csv_header(const ScCsv *csv, size_t col);   // "" if none
size_t      sc_csv_data_rows(const ScCsv *csv);            // rows minus header
const char *sc_csv_get(const ScCsv *csv, size_t data_row, const char *name);
```

Accessors return `""` (never `NULL`) for an out-of-range cell, so ragged rows
need no special-casing. `sc_csv_get(csv, r, "col")` looks a value up by header
name in **data** row `r` (0-based, header excluded).

### Writing

```c
ScCsv *sc_csv_new(ScCsvOpts opts);
bool   sc_csv_add_row(ScCsv *csv, const char *const *fields, size_t count);
char  *sc_csv_write(const ScCsv *csv);   // heap; fields quoted only when needed
void   sc_csv_free(ScCsv *csv);
```

---

## TOML

A near-complete TOML 1.0 reader and a writer, both over `ScValue`.

```c
typedef struct ScTomlWriteOpts {
    bool sort_keys;   // emit keys within each table in ascending order
} ScTomlWriteOpts;

ScValue *sc_toml_parse(const char *src, size_t len, ScParseError *err);
char    *sc_toml_write(const ScValue *value, ScTomlWriteOpts opts);
```

Covered: bare/quoted/dotted keys; `[table]` and `[[array of tables]]`; basic,
literal and multi-line strings (with `\u`/`\U`); integers (decimal with
underscores and sign, plus `0x`/`0o`/`0b`); floats (fraction/exponent,
`inf`/`nan`); booleans; nested/multi-line arrays with trailing commas; inline
tables; and `#` comments. **Date/time values are kept verbatim** as
`SC_VALUE_DATETIME` strings (not parsed into a calendar type). The writer emits
scalar keys first, then `[table]` / `[[array of tables]]` sections, so its
output re-parses.

---

## YAML

A **documented subset** reader and a block-style writer.

```c
typedef struct ScYamlWriteOpts {
    int  indent;      // spaces per level; 0 = 2
    bool sort_keys;   // emit mapping keys ascending
} ScYamlWriteOpts;

ScValue *sc_yaml_parse(const char *src, size_t len, ScParseError *err);
char    *sc_yaml_write(const ScValue *value, ScYamlWriteOpts opts);
```

**Supported:** block mappings and block sequences (including the inline
`- key: value` mapping-item form), flow collections (`[a, b]` / `{k: v}`),
plain / single-quoted / double-quoted scalars, `|` and `>` block scalars
(basic), `#` comments, a leading `---` document marker, and scalar type
inference (`null`/`~`, `true`/`false`, integers, floats incl. `.inf`/`.nan`).

**Not supported (by design):** anchors/aliases (`&`/`*`), tags (`!`), multiple
documents in one stream, and complex/explicit keys (`?`). These are what make
full YAML 1.2 enormous; this reader targets the config / front-matter 95%. The
writer quotes any string that would otherwise re-parse as a non-string (e.g. a
literal `"true"` or `"42"`).

---

## Markdown

Not a full Markdown renderer – two structured operations: **front-matter
splitting** and an **ATX-heading section tree**.

```c
typedef enum ScMdFrontmatter {
    SC_MD_FRONTMATTER_NONE, SC_MD_FRONTMATTER_YAML, SC_MD_FRONTMATTER_TOML,
} ScMdFrontmatter;

typedef struct ScMarkdown  ScMarkdown;    // opaque document
typedef struct ScMdSection ScMdSection;   // opaque heading section
```

### Reading

```c
ScMarkdown     *sc_markdown_parse(const char *src, size_t len, ScParseError *err);

ScMdFrontmatter sc_markdown_frontmatter_format(const ScMarkdown *md);
const char     *sc_markdown_frontmatter_raw(const ScMarkdown *md);   // "" if none
const ScValue  *sc_markdown_frontmatter(const ScMarkdown *md);       // parsed; NULL if none
const char     *sc_markdown_body(const ScMarkdown *md);
```

A leading `---` (YAML) or `+++` (TOML) fenced block is split off: the raw block
is always available, and it is also parsed into an `ScValue` (via the YAML or
TOML reader). The body is parsed into a tree of heading sections, fenced-code
aware (a `#` inside ```` ``` ````/`~~~` is not a heading).

### Section tree

```c
ScMdSection *sc_markdown_root(ScMarkdown *md);   // synthetic level-0 root
ScMdSection *sc_md_section_find(ScMdSection *s, const char *heading_path); // "A/B/C"

int          sc_md_section_level(const ScMdSection *s);   // 1..6 (0 = root)
const char  *sc_md_section_title(const ScMdSection *s);
const char  *sc_md_section_body(const ScMdSection *s);    // text under the heading
size_t       sc_md_section_child_count(const ScMdSection *s);
ScMdSection *sc_md_section_child(const ScMdSection *s, size_t index);
```

The root's body is the document preamble (text before the first heading); each
section owns the text directly beneath its heading and its nested children.

### Building / editing / writing

```c
ScMarkdown  *sc_markdown_new(void);
void         sc_md_section_set_body(ScMdSection *s, const char *text);
ScMdSection *sc_md_section_add(ScMdSection *parent, int level, const char *title);

void sc_markdown_set_frontmatter_raw(ScMarkdown *md, ScMdFrontmatter fmt,
                                     const char *raw);
bool sc_markdown_set_frontmatter(ScMarkdown *md, ScMdFrontmatter fmt,
                                 const ScValue *value);   // serialized via TOML/YAML
char *sc_markdown_write(const ScMarkdown *md);            // heap; front matter + tree
void  sc_markdown_free(ScMarkdown *md);
```

`sc_markdown_set_frontmatter` serializes a value tree with the writer for `fmt`
and stores it as the raw block, so edited front matter is reflected on write
(`sc_markdown_set_frontmatter_raw` sets/clears the raw text directly).

```c
ScMarkdown *md = sc_markdown_parse(doc, strlen(doc), NULL);
ScMdSection *macos = sc_md_section_find(sc_markdown_root(md), "Install/macOS");
printf("%s\n", sc_md_section_body(macos));

ScValue *meta = sc_value_object();
sc_value_set(meta, "title", sc_value_string("Guide"));
sc_markdown_set_frontmatter(md, SC_MD_FRONTMATTER_TOML, meta);
sc_value_free(meta);
char *out = sc_markdown_write(md);   // emits a fresh +++ block + the sections
free(out);
sc_markdown_free(md);
```

---

## Limitations / non-goals

These are deliberate, not bugs:

- **Numbers** are `double`/`int64` only – no lossless decimal lexeme. Keep your
  own string for exact-decimal needs.
- **TOML datetimes** are kept as strings (`SC_VALUE_DATETIME`), not validated or
  converted to a calendar type.
- **YAML** is a subset: no anchors/aliases, tags, multi-document streams, or
  explicit keys (see the YAML section).
- **Markdown** parses structure only (ATX headings + front matter + fenced-code
  awareness), not inline formatting, lists or tables.

---

## C++ wrapper (`sparcli::serde`)

Header-only, in `<serde/sparcli_serde.hpp>`, namespace `sparcli::serde`. RAII
owners, non-owning views, `std::optional` accessors, and a `ParseError`
exception. (Matches the existing C++ wrapper conventions; builds at C++20.)

### `Value` and `View`

`Value` owns an `ScValue` tree (move-only); `View` is a cheap non-owning handle
returned by navigation. Accessors return `std::optional`.

```cpp
using namespace sparcli::serde;

Value root = json::parse(R"({"name":"sparcli","version":[0,1,0]})");
View v = root.view();
std::optional<std::string_view> name = v.get("name").as_string();
std::optional<std::int64_t>     major = v.get("version").at(0).as_int();

Value doc = Value::object();
doc.set("k", Value::string("v"));            // builders: null/boolean/integer/
doc.push(...);                               //   number/string/array/object
```

`Value`: `null/boolean/integer/number/string/array/object` factories, `set`/
`push` (move the child in), `view()`, `type()`, `size()`, `get()` (raw handle),
`release()`. `View`: `type()`, `size()`, `as_bool/as_int/as_double/as_string`
(→ `std::optional`), `at(i)`, `get(key)`, `key_at(i)`, `operator bool`.

### Format namespaces

```cpp
Value j = json::parse(text);   std::string s = json::write(j, { .indent = 2 });
Value t = toml::parse(text);   std::string u = toml::write(t);
Value y = yaml::parse(text);   std::string w = yaml::write(y);
```

Each `parse` throws `serde::ParseError` (derived from `std::runtime_error`,
carrying `.line()` / `.column()`); each `write` returns a `std::string`.

### `Csv`

```cpp
Csv table = Csv::parse("name,age\nAda,30\n", { .has_header = true });
auto age = table.get(0, "age");              // std::string_view
Csv built = Csv::create();
built.add_row({ "a", "b" });                 // std::vector<std::string>
std::string out = built.write();
```

`rows/cols/row_cols/at/has_header/header/data_rows/get` (reading) and
`add_row/write` (building); move-only RAII.

### `Markdown` and `Section`

```cpp
Markdown md = Markdown::parse(text);
if (auto fm = md.frontmatter()) { auto title = fm->get("title").as_string(); }
Section macos = md.root().find("Install/macOS");
if (macos) { std::string_view body = macos.body(); }

Markdown doc = Markdown::create();
Section intro = doc.root().add(1, "Intro");
intro.set_body("Welcome.");
Value meta = Value::object();
meta.set("title", Value::string("Guide"));
doc.set_frontmatter(SC_MD_FRONTMATTER_TOML, meta);
std::string out = doc.write();
```

`Markdown` (move-only RAII): `parse/create`, `frontmatter_format/_raw`,
`frontmatter()` (→ `std::optional<View>`), `body()`, `root()`,
`set_frontmatter_raw/set_frontmatter`, `write()`. `Section` (non-owning handle):
`level/title/body/child_count/child/find` and the editing `set_body/add`.

---

## Build & test

The serde sources compile into `libsparcli`, but the suite is **separate** from
`make test`/`make qa`:

```sh
make test-serde      # data-model + format round-trip logic suite (headless)
make serde-sanitize  # the suite under AddressSanitizer + UBSan
make serde-fuzz      # random-input fuzzing of all five parsers (ASan/UBSan)
make serde-cpp       # the C++ wrapper assertion suite under the sanitizers
make serde-qa        # all of the above – the parser-layer counterpart to `make qa`
```

Run `make serde-qa` after any change under `src/serde/`. See
[`docs/DEVELOPMENT.md`](DEVELOPMENT.md) for the full workflow and
[`docs/examples.md`](examples.md) for the runnable `data/` examples.
