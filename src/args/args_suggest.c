#include "sparcli.h"
#include "args/args_internal.h"

#include <stdlib.h>
#include <string.h>


/** Maximum edit distance for a did-you-mean suggestion. */
#define SUGGEST_MAX_DISTANCE 2

/** Names longer than this still allow `SUGGEST_MAX_DISTANCE` typos. */
#define SUGGEST_MIN_LENGTH_FOR_TWO 5


// Forward declarations indented to reflect call hierarchy
static bool is_close_enough(const char *candidate, const char *name);
    static size_t min3(size_t a, size_t b, size_t c);


size_t sc_args_levenshtein(const char *a, const char *b) {
    size_t a_length = strlen(a);
    size_t b_length = strlen(b);

    // Single-row dynamic programming (O(min) memory)
    size_t *row = malloc((b_length + 1) * sizeof(size_t));
    if (!row) { return a_length > b_length ? a_length : b_length; }

    for (size_t j = 0; j <= b_length; j++) { row[j] = j; }

    for (size_t i = 1; i <= a_length; i++) {
        size_t diagonal = row[0];
        row[0] = i;
        for (size_t j = 1; j <= b_length; j++) {
            size_t previous = row[j];
            size_t substitution_cost = a[i - 1] == b[j - 1] ? 0 : 1;
            row[j] = min3(
                row[j] + 1,                    /* deletion */
                row[j - 1] + 1,                /* insertion */
                diagonal + substitution_cost   /* substitution */
            );
            diagonal = previous;
        }
    }

    size_t distance = row[b_length];
    free(row);
    return distance;
}

const char *sc_args_suggest_option(const ScArgsCmd *leaf, const char *name) {
    if (!leaf || !name) { return NULL; }

    const char *best = NULL;
    size_t best_distance = SUGGEST_MAX_DISTANCE + 1;

    for (const ScArgsCmd *cmd = leaf; cmd; cmd = cmd->parent) {
        for (size_t i = 0; i < cmd->option_count; i++) {
            const char *candidate = cmd->options[i].long_name;
            if (!is_close_enough(candidate, name)) { continue; }
            size_t distance = sc_args_levenshtein(candidate, name);
            if (distance < best_distance) {
                best_distance = distance;
                best = candidate;
            }
        }
    }
    return best;
}

const char *sc_args_suggest_subcommand(const ScArgsCmd *cmd, const char *name) {
    if (!cmd || !name) { return NULL; }

    const char *best = NULL;
    size_t best_distance = SUGGEST_MAX_DISTANCE + 1;

    for (size_t i = 0; i < cmd->subcommand_count; i++) {
        const char *candidate = cmd->subcommands[i]->name;
        if (!is_close_enough(candidate, name)) { continue; }
        size_t distance = sc_args_levenshtein(candidate, name);
        if (distance < best_distance) {
            best_distance = distance;
            best = candidate;
        }
    }
    return best;
}

/**
 * Pre-filter + distance check: short names only allow one typo,
 * longer names up to `SUGGEST_MAX_DISTANCE`.
 */
static bool is_close_enough(const char *candidate, const char *name) {
    size_t allowed = strlen(name) >= SUGGEST_MIN_LENGTH_FOR_TWO
        ? SUGGEST_MAX_DISTANCE
        : 1;
    return sc_args_levenshtein(candidate, name) <= allowed;
}

/** Minimum of three sizes. */
static size_t min3(size_t a, size_t b, size_t c) {
    size_t smaller = a < b ? a : b;
    return smaller < c ? smaller : c;
}
