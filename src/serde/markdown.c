#include "serde_internal.h"
#include "serde/sparcli_markdown.h"
#include "serde/sparcli_toml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file markdown.c
 * @brief Front-matter splitting and an ATX-heading section tree for Markdown.
 *
 * Not a full Markdown renderer: it separates a leading `---`/`+++` block from
 * the body and parses the body into a tree of heading sections (each owning
 * the text beneath it and its nested children), fenced-code-block aware.
 */


/** Maximum heading level; also bounds the build stack (root + levels 1..6). */
#define MD_MAX_LEVEL 6

/** Leading spaces tolerated before a heading / fence marker (CommonMark). */
#define MD_MAX_INDENT 3


/** One heading section: title, the body beneath it, and nested children. */
struct ScMdSection {
    int           level;       /**< 0 for the synthetic root, else 1..6. */
    char         *title;       /**< Owned heading text ("" for root). */
    char         *body;        /**< Owned body text ("" when none). */
    ScMdSection **children;    /**< Owned child sections. */
    size_t        child_count;
    size_t        child_cap;
};

struct ScMarkdown {
    ScMdFrontmatter fm_format;
    char           *frontmatter_raw; /**< Owned raw block ("" when none). */
    ScValue        *frontmatter;     /**< Parsed front matter (may be NULL). */
    char           *body;            /**< Owned body after the front matter. */
    ScMdSection    *root;            /**< Synthetic root (level 0). */
};


// Forward declarations indented to reflect the call hierarchy.
static const char *extract_frontmatter(
    const char *text, const char *end, ScMdFrontmatter *format, char **raw
);
static bool build_sections(ScMdSection *root, const char *body);
    static bool parse_heading(
        const char *line, size_t len, int *level, const char **title,
        size_t *title_len
    );
    static char fence_marker(const char *line, size_t len);
    static bool finalize_body(ScMdSection *section, ScSerdeBuf *buf);
static ScMdSection *section_new(int level, const char *title, size_t title_len);
static bool section_attach(ScMdSection *parent, ScMdSection *child);
static void section_free(ScMdSection *section);
static void write_section(
    ScSerdeBuf *buf, const ScMdSection *section, bool is_root
);
static char *dup_n(const char *text, size_t len);
static char *trim_dup(const char *text);
static bool is_blank_char(char ch);


/* ── Reading ───────────────────────────────────────────────────────────── */

ScMarkdown *sc_markdown_parse(const char *src, size_t len, ScParseError *err) {
    if (err) {
        *err = (ScParseError){ 0 };
    }
    if (!src) {
        len = 0;
    }

    char *copy = malloc(len + 1);
    ScMarkdown *md = calloc(1, sizeof *md);
    if (!copy || !md) {
        free(copy);
        free(md);
        if (err) {
            snprintf(err->message, sizeof err->message, "out of memory");
        }
        return NULL;
    }
    if (len > 0) {
        memcpy(copy, src, len);
    }
    copy[len] = '\0';

    md->root = section_new(0, "", 0);
    const char *body_start = extract_frontmatter(
        copy, copy + len, &md->fm_format, &md->frontmatter_raw
    );
    if (!md->frontmatter_raw) {
        md->frontmatter_raw = dup_n("", 0);
    }
    if (md->fm_format == SC_MD_FRONTMATTER_TOML) {
        md->frontmatter = sc_toml_parse(
            md->frontmatter_raw, strlen(md->frontmatter_raw), NULL
        );
    }

    md->body = dup_n(body_start, (size_t)(copy + len - body_start));
    bool ok = md->root && md->frontmatter_raw && md->body
        && build_sections(md->root, md->body);
    free(copy);

    if (!ok) {
        sc_markdown_free(md);
        if (err) {
            snprintf(err->message, sizeof err->message, "out of memory");
        }
        return NULL;
    }
    return md;
}

/**
 * Splits a leading `---`/`+++` block off the front. Returns a pointer to the
 * body start; sets `*format`/`*raw` (raw owned) when a block is present.
 */
static const char *extract_frontmatter(
    const char *text, const char *end, ScMdFrontmatter *format, char **raw
) {
    *format = SC_MD_FRONTMATTER_NONE;
    *raw = NULL;
    if (end - text < 3) {
        return text;
    }

    ScMdFrontmatter fmt = SC_MD_FRONTMATTER_NONE;
    char marker = 0;
    if (text[0] == '-' && text[1] == '-' && text[2] == '-') {
        fmt = SC_MD_FRONTMATTER_YAML;
        marker = '-';
    } else if (text[0] == '+' && text[1] == '+' && text[2] == '+') {
        fmt = SC_MD_FRONTMATTER_TOML;
        marker = '+';
    } else {
        return text;
    }

    // The opening fence line must be exactly the three marker characters.
    const char *after = text + 3;
    if (after < end && *after == '\r') {
        after++;
    }
    if (!(after < end && *after == '\n')) {
        return text;
    }
    const char *content = after + 1;

    // Find a closing fence line equal to the marker.
    const char *line = content;
    while (line < end) {
        const char *line_end = line;
        while (line_end < end && *line_end != '\n') {
            line_end++;
        }
        size_t line_len = (size_t)(line_end - line);
        if (line_len > 0 && line[line_len - 1] == '\r') {
            line_len--;
        }
        if (line_len == 3 && line[0] == marker && line[1] == marker
            && line[2] == marker) {
            const char *raw_end = line;
            if (raw_end > content && raw_end[-1] == '\n') {
                raw_end--;
            }
            if (raw_end > content && raw_end[-1] == '\r') {
                raw_end--;
            }
            *raw = dup_n(content, (size_t)(raw_end - content));
            *format = fmt;
            const char *body = (line_end < end) ? line_end + 1 : end;
            return body;
        }
        line = (line_end < end) ? line_end + 1 : end;
    }

    // No closing fence: the leading marker was not front matter after all.
    return text;
}

/** Builds the section tree from the body text. Returns false on allocation
 *  failure (the partial tree is freed by the caller via `sc_markdown_free`). */
static bool build_sections(ScMdSection *root, const char *body) {
    ScMdSection *stack[MD_MAX_LEVEL + 1];
    size_t depth = 0;
    stack[depth++] = root;
    ScMdSection *current = root;

    ScSerdeBuf buf = { 0 };
    char fence = 0; // active fence marker, or 0
    const char *end = body + strlen(body);
    const char *p = body;

    while (p < end) {
        const char *line_end = p;
        while (line_end < end && *line_end != '\n') {
            line_end++;
        }
        size_t line_len = (size_t)(line_end - p);
        if (line_len > 0 && p[line_len - 1] == '\r') {
            line_len--;
        }

        char marker = fence_marker(p, line_len);
        int level = 0;
        const char *title = NULL;
        size_t title_len = 0;

        if (marker != 0 && (fence == 0 || fence == marker)) {
            fence = (fence == 0) ? marker : 0;
            sc_serde_buf_append(&buf, p, line_len);
            sc_serde_buf_append_char(&buf, '\n');
        } else if (fence == 0
                   && parse_heading(p, line_len, &level, &title, &title_len)) {
            if (!finalize_body(current, &buf)) {
                sc_serde_buf_free(&buf);
                return false;
            }
            // Pop to the parent; never pop the root (index 0, level 0), so
            // `depth >= 1` always holds and stack[depth - 1] is valid.
            while (depth > 1 && stack[depth - 1]->level >= level) {
                depth--;
            }
            ScMdSection *parent = stack[depth - 1];
            ScMdSection *section = section_new(level, title, title_len);
            if (!section || !section_attach(parent, section)) {
                section_free(section);
                sc_serde_buf_free(&buf);
                return false;
            }
            if (depth <= MD_MAX_LEVEL) {
                stack[depth++] = section;
            } else {
                stack[depth - 1] = section;
            }
            current = section;
        } else {
            sc_serde_buf_append(&buf, p, line_len);
            sc_serde_buf_append_char(&buf, '\n');
        }

        p = (line_end < end) ? line_end + 1 : end;
    }

    return finalize_body(current, &buf);
}

/** Parses an ATX heading line; fills level/title on success. */
static bool parse_heading(
    const char *line, size_t len, int *level, const char **title,
    size_t *title_len
) {
    size_t i = 0;
    size_t indent = 0;
    while (i < len && line[i] == ' ' && indent < MD_MAX_INDENT) {
        i++;
        indent++;
    }
    size_t hashes = 0;
    while (i < len && line[i] == '#') {
        i++;
        hashes++;
    }
    if (hashes < 1 || hashes > MD_MAX_LEVEL) {
        return false;
    }
    if (i < len && line[i] != ' ' && line[i] != '\t') {
        return false; // a heading needs a space after the # run
    }
    while (i < len && (line[i] == ' ' || line[i] == '\t')) {
        i++;
    }

    size_t e = len;
    while (e > i && (line[e - 1] == ' ' || line[e - 1] == '\t')) {
        e--;
    }
    // Strip a trailing closing `#` sequence (only if space-separated).
    size_t te = e;
    while (te > i && line[te - 1] == '#') {
        te--;
    }
    if (te < e && (te == i || line[te - 1] == ' ' || line[te - 1] == '\t')) {
        e = te;
        while (e > i && (line[e - 1] == ' ' || line[e - 1] == '\t')) {
            e--;
        }
    }

    *level = (int)hashes;
    *title = line + i;
    *title_len = e - i;
    return true;
}

/** Returns the fence marker (`` ` `` or `~`) of a fence line, else `0`. */
static char fence_marker(const char *line, size_t len) {
    size_t i = 0;
    size_t indent = 0;
    while (i < len && line[i] == ' ' && indent < MD_MAX_INDENT) {
        i++;
        indent++;
    }
    if (i + 3 <= len) {
        if (line[i] == '`' && line[i + 1] == '`' && line[i + 2] == '`') {
            return '`';
        }
        if (line[i] == '~' && line[i + 1] == '~' && line[i + 2] == '~') {
            return '~';
        }
    }
    return 0;
}

/** Trims the accumulated buffer and stores it as the section's body. */
static bool finalize_body(ScMdSection *section, ScSerdeBuf *buf) {
    char *raw = sc_serde_buf_finish(buf);
    if (!raw) {
        return false;
    }
    char *trimmed = trim_dup(raw);
    free(raw);
    if (!trimmed) {
        return false;
    }
    free(section->body);
    section->body = trimmed;
    return true;
}


/* ── Accessors ─────────────────────────────────────────────────────────── */

ScMdFrontmatter sc_markdown_frontmatter_format(const ScMarkdown *md) {
    return md ? md->fm_format : SC_MD_FRONTMATTER_NONE;
}

const char *sc_markdown_frontmatter_raw(const ScMarkdown *md) {
    return (md && md->frontmatter_raw) ? md->frontmatter_raw : "";
}

const ScValue *sc_markdown_frontmatter(const ScMarkdown *md) {
    return md ? md->frontmatter : NULL;
}

const char *sc_markdown_body(const ScMarkdown *md) {
    return (md && md->body) ? md->body : "";
}

ScMdSection *sc_markdown_root(ScMarkdown *md) {
    return md ? md->root : NULL;
}

ScMdSection *sc_md_section_find(
    ScMdSection *section, const char *heading_path
) {
    if (!section || !heading_path) {
        return NULL;
    }

    const char *cursor = heading_path;
    ScMdSection *node = section;
    while (*cursor != '\0') {
        const char *slash = strchr(cursor, '/');
        size_t seg_len = slash ? (size_t)(slash - cursor) : strlen(cursor);

        ScMdSection *match = NULL;
        for (size_t i = 0; i < node->child_count; i++) {
            ScMdSection *child = node->children[i];
            if (strlen(child->title) == seg_len
                && strncmp(child->title, cursor, seg_len) == 0) {
                match = child;
                break;
            }
        }
        if (!match) {
            return NULL;
        }
        node = match;
        cursor = slash ? slash + 1 : cursor + seg_len;
    }
    return node;
}

int sc_md_section_level(const ScMdSection *section) {
    return section ? section->level : 0;
}

const char *sc_md_section_title(const ScMdSection *section) {
    return (section && section->title) ? section->title : "";
}

const char *sc_md_section_body(const ScMdSection *section) {
    return (section && section->body) ? section->body : "";
}

size_t sc_md_section_child_count(const ScMdSection *section) {
    return section ? section->child_count : 0;
}

ScMdSection *sc_md_section_child(const ScMdSection *section, size_t index) {
    if (!section || index >= section->child_count) {
        return NULL;
    }
    return section->children[index];
}


/* ── Building / editing ────────────────────────────────────────────────── */

ScMarkdown *sc_markdown_new(void) {
    ScMarkdown *md = calloc(1, sizeof *md);
    if (!md) {
        return NULL;
    }
    md->root = section_new(0, "", 0);
    md->frontmatter_raw = dup_n("", 0);
    md->body = dup_n("", 0);
    if (!md->root || !md->frontmatter_raw || !md->body) {
        sc_markdown_free(md);
        return NULL;
    }
    return md;
}

void sc_markdown_set_frontmatter_raw(
    ScMarkdown *md, ScMdFrontmatter format, const char *raw
) {
    if (!md) {
        return;
    }
    char *copy = dup_n(raw ? raw : "", raw ? strlen(raw) : 0);
    if (!copy) {
        return;
    }
    free(md->frontmatter_raw);
    md->frontmatter_raw = copy;
    md->fm_format = raw ? format : SC_MD_FRONTMATTER_NONE;

    sc_value_free(md->frontmatter);
    md->frontmatter = NULL;
    if (md->fm_format == SC_MD_FRONTMATTER_TOML) {
        md->frontmatter = sc_toml_parse(copy, strlen(copy), NULL);
    }
}

void sc_md_section_set_body(ScMdSection *section, const char *text) {
    if (!section) {
        return;
    }
    char *copy = dup_n(text ? text : "", text ? strlen(text) : 0);
    if (!copy) {
        return;
    }
    free(section->body);
    section->body = copy;
}

ScMdSection *sc_md_section_add(
    ScMdSection *parent, int level, const char *title
) {
    if (!parent || level < 1 || level > MD_MAX_LEVEL) {
        return NULL;
    }
    ScMdSection *section = section_new(
        level, title ? title : "", title ? strlen(title) : 0
    );
    if (!section || !section_attach(parent, section)) {
        section_free(section);
        return NULL;
    }
    return section;
}

char *sc_markdown_write(const ScMarkdown *md) {
    ScSerdeBuf buf = { 0 };
    if (md) {
        if (md->fm_format != SC_MD_FRONTMATTER_NONE
            && md->frontmatter_raw[0] != '\0') {
            const char *delim =
                (md->fm_format == SC_MD_FRONTMATTER_TOML) ? "+++" : "---";
            sc_serde_buf_append_str(&buf, delim);
            sc_serde_buf_append_char(&buf, '\n');
            sc_serde_buf_append_str(&buf, md->frontmatter_raw);
            size_t raw_len = strlen(md->frontmatter_raw);
            if (raw_len > 0 && md->frontmatter_raw[raw_len - 1] != '\n') {
                sc_serde_buf_append_char(&buf, '\n');
            }
            sc_serde_buf_append_str(&buf, delim);
            sc_serde_buf_append_char(&buf, '\n');
        }
        if (md->root) {
            write_section(&buf, md->root, true);
        }
    }
    return sc_serde_buf_finish(&buf);
}

void sc_markdown_free(ScMarkdown *md) {
    if (!md) {
        return;
    }
    free(md->frontmatter_raw);
    sc_value_free(md->frontmatter);
    free(md->body);
    section_free(md->root);
    free(md);
}


/* ── Writer / tree internals ───────────────────────────────────────────── */

/** Emits a section (heading + body) and recurses into its children. */
static void write_section(
    ScSerdeBuf *buf, const ScMdSection *section, bool is_root
) {
    if (!is_root) {
        if (buf->len > 0) {
            sc_serde_buf_append_char(buf, '\n'); // blank line before heading
        }
        for (int i = 0; i < section->level; i++) {
            sc_serde_buf_append_char(buf, '#');
        }
        sc_serde_buf_append_char(buf, ' ');
        sc_serde_buf_append_str(buf, section->title);
        sc_serde_buf_append_char(buf, '\n');
        if (section->body[0] != '\0') {
            sc_serde_buf_append_char(buf, '\n');
            sc_serde_buf_append_str(buf, section->body);
            sc_serde_buf_append_char(buf, '\n');
        }
    } else if (section->body[0] != '\0') {
        sc_serde_buf_append_str(buf, section->body);
        sc_serde_buf_append_char(buf, '\n');
    }

    for (size_t i = 0; i < section->child_count; i++) {
        write_section(buf, section->children[i], false);
    }
}

/** Allocates a section, copying `title_len` bytes of `title`. */
static ScMdSection *section_new(
    int level, const char *title, size_t title_len
) {
    ScMdSection *section = calloc(1, sizeof *section);
    if (!section) {
        return NULL;
    }
    section->level = level;
    section->title = dup_n(title, title_len);
    section->body = dup_n("", 0);
    if (!section->title || !section->body) {
        section_free(section);
        return NULL;
    }
    return section;
}

/** Appends `child` to `parent`, growing the child array. */
static bool section_attach(ScMdSection *parent, ScMdSection *child) {
    if (parent->child_count == parent->child_cap) {
        if (parent->child_cap > SIZE_MAX / 2) {
            return false;
        }
        size_t new_cap = parent->child_cap ? parent->child_cap * 2 : 4;
        if (new_cap > SIZE_MAX / sizeof *parent->children) {
            return false;
        }
        ScMdSection **grown = realloc(
            parent->children, new_cap * sizeof *parent->children
        );
        if (!grown) {
            return false;
        }
        parent->children = grown;
        parent->child_cap = new_cap;
    }
    parent->children[parent->child_count] = child;
    parent->child_count++;
    return true;
}

/** Recursively frees a section and its children. */
static void section_free(ScMdSection *section) {
    if (!section) {
        return;
    }
    for (size_t i = 0; i < section->child_count; i++) {
        section_free(section->children[i]);
    }
    free(section->children);
    free(section->title);
    free(section->body);
    free(section);
}


/* ── Small string helpers ──────────────────────────────────────────────── */

/** Copies `len` bytes of `text` into a fresh NUL-terminated string. */
static char *dup_n(const char *text, size_t len) {
    // calloc zero-initializes, so the buffer is fully defined even before the
    // memcpy (and the trailing NUL is already in place).
    char *copy = calloc(len + 1, 1);
    if (!copy) {
        return NULL;
    }
    if (len > 0) {
        memcpy(copy, text, len);
    }
    return copy;
}

/** Returns a copy of `text` with leading/trailing whitespace removed. */
static char *trim_dup(const char *text) {
    const char *start = text;
    while (*start != '\0' && is_blank_char(*start)) {
        start++;
    }
    const char *finish = start + strlen(start);
    while (finish > start && is_blank_char(finish[-1])) {
        finish--;
    }
    return dup_n(start, (size_t)(finish - start));
}

/** True for ASCII whitespace (space, tab, CR, LF). */
static bool is_blank_char(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}
