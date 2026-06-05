#pragma once

#include "core/sparcli_export.h"
#include "serde/sparcli_value.h"
#include "serde/sparcli_parse_error.h"

#include <stdbool.h>
#include <stddef.h>

SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_markdown.h
 * @brief Structured Markdown: front-matter split plus a heading/section tree.
 *
 * This is not a full Markdown renderer. It does two things:
 *
 * 1. **Front matter** - a leading `---` (YAML) or `+++` (TOML) fenced block is
 *    separated from the body. The raw block is always available; a TOML block
 *    is also parsed into an `ScValue`. (YAML parsing is wired in once the YAML
 *    reader lands; until then a `---` block exposes its raw text only.)
 *
 * 2. **Sections** - the body is parsed into a tree of ATX-heading sections
 *    (`#`..`######`). Each section owns the text directly beneath its heading
 *    (before the first sub-heading) and its nested child sections, so sections
 *    can be read, edited (body text) and re-serialized. Fenced code blocks
 *    (```` ``` ```` / `~~~`) are respected, so a `#` inside code is not a
 *    heading.
 */


/** Which front-matter syntax a document uses. */
typedef enum ScMdFrontmatter {
    SC_MD_FRONTMATTER_NONE, /**< No front matter. */
    SC_MD_FRONTMATTER_YAML, /**< `---` fenced YAML. */
    SC_MD_FRONTMATTER_TOML, /**< `+++` fenced TOML. */
} ScMdFrontmatter;

/** Opaque Markdown document; create with `sc_markdown_parse`/`sc_markdown_new`. */
typedef struct ScMarkdown ScMarkdown;

/** Opaque heading section in the document outline. */
typedef struct ScMdSection ScMdSection;


/* ── Reading ───────────────────────────────────────────────────────────── */

/**
 * Parses a Markdown document: splits front matter and builds the section tree.
 *
 * @param src  Source bytes (need not be NUL-terminated).
 * @param len  Length of `src` in bytes.
 * @param err  Optional; filled on an allocation failure.
 * @return     Heap document (free with `sc_markdown_free`), or `NULL` on
 *             allocation failure.
 */
SPARCLI_EXPORT ScMarkdown *sc_markdown_parse(
    const char *src, size_t len, ScParseError *err
);

/** Returns the front-matter syntax of the document. */
SPARCLI_EXPORT ScMdFrontmatter sc_markdown_frontmatter_format(
    const ScMarkdown *md
);

/** Returns the raw front-matter block text (`""` when there is none). */
SPARCLI_EXPORT const char *sc_markdown_frontmatter_raw(const ScMarkdown *md);

/**
 * Returns the parsed front matter as an `ScValue` (borrowed, owned by the
 * document), or `NULL` when there is none or it was not parsed.
 */
SPARCLI_EXPORT const ScValue *sc_markdown_frontmatter(const ScMarkdown *md);

/** Returns the body text (everything after the front matter); `""` if empty. */
SPARCLI_EXPORT const char *sc_markdown_body(const ScMarkdown *md);


/* ── Section tree ──────────────────────────────────────────────────────── */

/**
 * Returns the synthetic root section (level `0`). Its body is the document
 * preamble (text before the first heading); its children are the top-level
 * sections.
 */
SPARCLI_EXPORT ScMdSection *sc_markdown_root(ScMarkdown *md);

/**
 * Finds a descendant section by a `/`-separated heading path, e.g.
 * `"Install/macOS"`. Matching is exact on heading text.
 *
 * @return  The section, or `NULL` when the path does not resolve.
 */
SPARCLI_EXPORT ScMdSection *sc_md_section_find(
    ScMdSection *section, const char *heading_path
);

/** Returns the heading level (`1`..`6`; `0` for the synthetic root). */
SPARCLI_EXPORT int sc_md_section_level(const ScMdSection *section);

/** Returns the heading text (`""` for the root). */
SPARCLI_EXPORT const char *sc_md_section_title(const ScMdSection *section);

/** Returns the body text directly under the heading (`""` when none). */
SPARCLI_EXPORT const char *sc_md_section_body(const ScMdSection *section);

/** Returns the number of direct child sections. */
SPARCLI_EXPORT size_t sc_md_section_child_count(const ScMdSection *section);

/** Returns the child section at `index` (`NULL` when out of range). */
SPARCLI_EXPORT ScMdSection *sc_md_section_child(
    const ScMdSection *section, size_t index
);


/* ── Building / editing ────────────────────────────────────────────────── */

/** Allocates an empty document (no front matter, empty root). */
SPARCLI_EXPORT ScMarkdown *sc_markdown_new(void);

/**
 * Sets the raw front-matter block and its syntax (the block is copied; pass
 * `SC_MD_FRONTMATTER_NONE` / `NULL` to remove it).
 */
SPARCLI_EXPORT void sc_markdown_set_frontmatter_raw(
    ScMarkdown *md, ScMdFrontmatter format, const char *raw
);

/**
 * Sets the front matter from an `ScValue`, serializing it with the writer for
 * `format` (TOML or YAML) and storing the result as the raw block, so edited
 * front matter is reflected by `sc_markdown_write`.
 *
 * @param md      Target document.
 * @param format  `SC_MD_FRONTMATTER_TOML` or `SC_MD_FRONTMATTER_YAML`.
 * @param value   Value tree to serialize (typically an object); borrowed.
 * @return        `true` on success; `false` for `SC_MD_FRONTMATTER_NONE`, a
 *                `NULL` value, or on a serialization/allocation failure. To
 *                clear the front matter use `sc_markdown_set_frontmatter_raw`.
 */
SPARCLI_EXPORT bool sc_markdown_set_frontmatter(
    ScMarkdown *md, ScMdFrontmatter format, const ScValue *value
);

/** Replaces a section's body text (copied; `NULL` clears it). */
SPARCLI_EXPORT void sc_md_section_set_body(
    ScMdSection *section, const char *text
);

/**
 * Appends a new child section.
 *
 * @param parent  Section to append under.
 * @param level   Heading level (`1`..`6`).
 * @param title   Heading text; copied.
 * @return        The new section (owned by the tree), or `NULL` on failure.
 */
SPARCLI_EXPORT ScMdSection *sc_md_section_add(
    ScMdSection *parent, int level, const char *title
);

/**
 * Serializes the document back to Markdown (front matter as its raw block,
 * then the section tree).
 *
 * @return  Heap-allocated, NUL-terminated Markdown (caller `free`s it), or
 *          `NULL` on allocation failure.
 */
SPARCLI_EXPORT char *sc_markdown_write(const ScMarkdown *md);

/** Reads and parses a Markdown file; `NULL` on a read error (`err` filled).
 *  Free the result with `sc_markdown_free`. */
SPARCLI_EXPORT ScMarkdown *sc_markdown_parse_file(
    const char *path, ScParseError *err
);

/** Serializes `md` and writes it to `path`; `false` on a write error. */
SPARCLI_EXPORT bool sc_markdown_write_file(
    const ScMarkdown *md, const char *path
);

/** Frees a document and its whole section tree; safe to pass `NULL`. */
SPARCLI_EXPORT void sc_markdown_free(ScMarkdown *md);

SPARCLI_END_DECLS
