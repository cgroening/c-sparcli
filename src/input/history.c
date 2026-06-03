#include "input_internal.h"
#include "core/sanitize_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/**
 * @file history.c
 * @brief Input history: Up/Down recall storage + plain-text persistence.
 *
 * The handle is pure storage - the recall navigation lives in the text
 * input widget. Entries are sanitized when they enter the history (trust
 * boundary), both from `sc_history_add` and from the persistence file.
 */


/** Default entry cap when `ScHistoryOpts.max_entries` is zero. */
#define HISTORY_DEFAULT_MAX_ENTRIES 500

/** Relative file name used for XDG-app persistence. */
#define HISTORY_XDG_FILE_NAME "history"

/** Longest persisted line read back from the history file. */
#define HISTORY_LINE_BUFFER 4096


/** Input-history storage; see sparcli_history.h. */
struct ScHistory {
    char **entries;          /**< Owned strings, oldest first. */
    size_t count;
    size_t cap;
    size_t max_entries;      /**< Resolved cap (never zero). */
    char *file;              /**< Owned persistence path; NULL = in-memory. */
    bool keep_duplicates;
    bool no_auto_add;
};


// Forward declarations indented to reflect call hierarchy
static char *resolve_file_path(const ScHistoryOpts *opts);
static void clear_entries(ScHistory *self);
static bool append_entry(ScHistory *self, char *entry);


ScHistory *sc_history_new(ScHistoryOpts opts) {
    ScHistory *self = calloc(1, sizeof *self);
    if (!self) {
        return NULL;
    }
    self->max_entries = opts.max_entries
        ? opts.max_entries
        : HISTORY_DEFAULT_MAX_ENTRIES;
    self->keep_duplicates = opts.keep_duplicates;
    self->no_auto_add = opts.no_auto_add;
    self->file = resolve_file_path(&opts);

    if (self->file) {
        sc_history_load(self);   // a missing file simply starts empty
    }
    return self;
}

void sc_history_add(ScHistory *self, const char *line) {
    if (!self || !line || line[0] == '\0') {
        return;
    }
    // Entries are single lines: embedded line breaks would corrupt the
    // persistence format and can never round-trip through the line editor.
    if (strchr(line, '\n') || strchr(line, '\r')) {
        return;
    }

    // Trust boundary: the line crosses into the library here
    char *copy = sc_sanitize_copy(line, false);
    if (!copy || copy[0] == '\0') {
        free(copy);
        return;
    }

    // Collapse consecutive duplicates (the readline-style default)
    if (!self->keep_duplicates && self->count > 0
        && strcmp(self->entries[self->count - 1], copy) == 0) {
        free(copy);
        return;
    }

    if (!append_entry(self, copy)) {
        free(copy);
    }
}

size_t sc_history_count(const ScHistory *self) {
    return self ? self->count : 0;
}

const char *sc_history_get(const ScHistory *self, size_t index) {
    if (!self || index >= self->count) {
        return NULL;
    }
    return self->entries[index];
}

bool sc_history_save(const ScHistory *self) {
    if (!self || !self->file) {
        return false;
    }

    FILE *out = fopen(self->file, "w");
    if (!out) {
        return false;
    }
    bool ok = true;
    for (size_t i = 0; i < self->count; i++) {
        if (fprintf(out, "%s\n", self->entries[i]) < 0) {
            ok = false;
            break;
        }
    }
    if (fclose(out) != 0) {
        ok = false;
    }
    return ok;
}

bool sc_history_load(ScHistory *self) {
    if (!self || !self->file) {
        return false;
    }

    FILE *in = fopen(self->file, "r");
    if (!in) {
        return false;
    }

    clear_entries(self);

    char line[HISTORY_LINE_BUFFER];
    while (fgets(line, sizeof line, in)) {
        line[strcspn(line, "\r\n")] = '\0';
        sc_history_add(self, line);   // sanitizes + dedupes + caps
    }
    fclose(in);
    return true;
}

void sc_history_free(ScHistory *self) {
    if (!self) {
        return;
    }
    if (self->file) {
        sc_history_save(self);
    }
    clear_entries(self);
    free(self->entries);
    free(self->file);
    free(self);
}

/** Whether a text input should auto-add submitted lines to this history. */
bool sc_history_auto_add(const ScHistory *self) {
    return self != NULL && !self->no_auto_add;
}


/* ── Internals ──────────────────────────────────────────────────────────── */

/** Resolves the persistence path: explicit file > XDG app dir > none. */
static char *resolve_file_path(const ScHistoryOpts *opts) {
    if (opts->file) {
        return sc_dup_str(opts->file);
    }
    if (opts->app) {
        return sc_path_file(SC_PATH_STATE, opts->app, HISTORY_XDG_FILE_NAME);
    }
    return NULL;
}

/** Frees all entries; keeps the array allocation for reuse. */
static void clear_entries(ScHistory *self) {
    for (size_t i = 0; i < self->count; i++) {
        free(self->entries[i]);
    }
    self->count = 0;
}

/** Appends an owned entry, evicting the oldest when the cap is reached. */
static bool append_entry(ScHistory *self, char *entry) {
    if (self->count == self->max_entries) {
        free(self->entries[0]);
        memmove(
            self->entries, self->entries + 1,
            (self->count - 1) * sizeof *self->entries
        );
        self->count--;
    }

    if (self->count == self->cap) {
        size_t cap = self->cap ? self->cap * 2 : 16;
        if (cap > self->max_entries) {
            cap = self->max_entries;
        }
        char **grown = realloc(self->entries, cap * sizeof *grown);
        if (!grown) {
            return false;
        }
        self->entries = grown;
        self->cap = cap;
    }

    self->entries[self->count++] = entry;
    return true;
}
