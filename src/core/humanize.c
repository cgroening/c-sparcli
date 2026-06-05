#include "sparcli.h"

#include <stdlib.h>
#include <string.h>


/* ── small helpers ───────────────────────────────────────────────────────── */

/** Heap-duplicates a NUL-terminated string (NULL on allocation failure). */
static char *dup_str(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) { memcpy(p, s, n); }
    return p;
}

/**
 * Appends `suffix` (optionally separated by a space) to the heap string
 * `owned`, taking ownership of `owned` (freed here). Returns a new heap
 * string; `NULL` on allocation failure (and frees `owned`).
 */
static char *join2(char *owned, const char *suffix, bool space) {
    if (!owned) { return NULL; }
    size_t la = strlen(owned);
    size_t ls = strlen(suffix);
    size_t sp = space ? 1 : 0;
    char *out = malloc(la + sp + ls + 1);
    if (!out) { free(owned); return NULL; }
    memcpy(out, owned, la);
    if (space) { out[la] = ' '; }
    memcpy(out + la + sp, suffix, ls + 1);
    free(owned);
    return out;
}

/**
 * Formats a (possibly negative) magnitude with a fixed number of fractional
 * digits, custom decimal/grouping separators and optional trailing-zero
 * trimming. Returns a heap string; `NULL` on allocation failure.
 *
 * @param gsep  Thousands separator, or `0` to disable grouping.
 */
static char *fmt_number(double value, int decimals,
                        char dsep, char gsep, bool trim_zeros) {
    if (decimals < 0) { decimals = 0; }
    if (dsep == 0)    { dsep = '.'; }

    bool neg = value < 0;
    if (neg) { value = -value; }

    char raw[512];
    int n = snprintf(raw, sizeof raw, "%.*f", decimals, value);
    if (n < 0 || (size_t)n >= sizeof raw) { return NULL; }

    char *dot = strchr(raw, '.');
    size_t int_len  = dot ? (size_t)(dot - raw) : (size_t)n;
    char  *frac     = dot ? dot + 1 : NULL;
    size_t frac_len = frac ? strlen(frac) : 0;

    if (trim_zeros && frac_len > 0) {
        while (frac_len > 0 && frac[frac_len - 1] == '0') { frac_len--; }
    }

    size_t group_seps = (gsep && int_len > 0) ? (int_len - 1) / 3 : 0;
    size_t out_size = (neg ? 1u : 0u) + int_len + group_seps
                    + (frac_len > 0 ? 1 + frac_len : 0) + 1;
    char *out = malloc(out_size);
    if (!out) { return NULL; }

    size_t w = 0;
    if (neg) { out[w++] = '-'; }

    if (gsep) {
        for (size_t i = 0; i < int_len; i++) {
            if (i > 0 && (int_len - i) % 3 == 0) { out[w++] = gsep; }
            out[w++] = raw[i];
        }
    } else {
        memcpy(out + w, raw, int_len);
        w += int_len;
    }

    if (frac_len > 0) {
        out[w++] = dsep;
        memcpy(out + w, frac, frac_len);
        w += frac_len;
    }
    out[w] = '\0';
    return out;
}


/* ── sizes ───────────────────────────────────────────────────────────────── */

char *sc_humanize_bytes_opts(uint64_t bytes, ScByteUnit unit,
                             ScHumanizeOpts opts) {
    static const char *si[]   = {"B","KB","MB","GB","TB","PB","EB"};
    static const char *iec[]  = {"B","KiB","MiB","GiB","TiB","PiB","EiB"};
    static const char *iecs[] = {"B","K","M","G","T","P","E"};

    double base = (unit == SC_BYTES_SI) ? 1000.0 : 1024.0;
    const char *const *labels =
        (unit == SC_BYTES_SI) ? si : (unit == SC_BYTES_IEC ? iec : iecs);

    double v = (double)bytes;
    int i = 0;
    while (v >= base && i < 6) { v /= base; i++; }

    int dec = (i == 0) ? 0 : (opts.decimals > 0 ? opts.decimals : 1);
    char *num = fmt_number(v, dec, opts.decimal_sep, 0, true);
    return join2(num, labels[i], !opts.no_space);
}

char *sc_humanize_bytes(uint64_t bytes, ScByteUnit unit) {
    return sc_humanize_bytes_opts(bytes, unit, (ScHumanizeOpts){ 0 });
}


/* ── numbers ─────────────────────────────────────────────────────────────── */

char *sc_humanize_number(double value, ScHumanizeOpts opts) {
    char gsep = opts.group_sep ? opts.group_sep : ',';
    return fmt_number(value, opts.decimals, opts.decimal_sep, gsep, false);
}

char *sc_humanize_compact(double value, ScHumanizeOpts opts) {
    static const char *suf[] = {"", "k", "M", "B", "T"};
    bool neg = value < 0;
    double v = neg ? -value : value;
    int i = 0;
    while (v >= 1000.0 && i < 4) { v /= 1000.0; i++; }

    int dec = (i == 0) ? 0 : (opts.decimals > 0 ? opts.decimals : 1);
    char *num = fmt_number(neg ? -v : v, dec, opts.decimal_sep, 0, true);
    return join2(num, suf[i], false);
}

char *sc_humanize_percent(double ratio, ScHumanizeOpts opts) {
    char *num = fmt_number(ratio * 100.0, opts.decimals, opts.decimal_sep,
                           0, false);
    return join2(num, "%", false);
}


/* ── durations ───────────────────────────────────────────────────────────── */

char *sc_humanize_duration(double seconds) {
    double s = seconds < 0 ? -seconds : seconds;
    long total = (long)(s + 0.5);
    long days  = total / 86400;
    long hours = (total % 86400) / 3600;
    long mins  = (total % 3600) / 60;
    long secs  = total % 60;

    char buf[64];
    if (days > 0) {
        if (hours > 0) { snprintf(buf, sizeof buf, "%ldd %ldh", days, hours); }
        else           { snprintf(buf, sizeof buf, "%ldd", days); }
    } else if (hours > 0) {
        if (mins > 0) { snprintf(buf, sizeof buf, "%ldh %ldm", hours, mins); }
        else          { snprintf(buf, sizeof buf, "%ldh", hours); }
    } else if (mins > 0) {
        if (secs > 0) { snprintf(buf, sizeof buf, "%ldm %lds", mins, secs); }
        else          { snprintf(buf, sizeof buf, "%ldm", mins); }
    } else {
        snprintf(buf, sizeof buf, "%lds", secs);
    }
    return dup_str(buf);
}

char *sc_humanize_duration_clock(double seconds) {
    double s = seconds < 0 ? -seconds : seconds;
    long total = (long)(s + 0.5);
    long hours = total / 3600;
    long mins  = (total % 3600) / 60;
    long secs  = total % 60;

    char buf[32];
    if (hours > 0) {
        snprintf(buf, sizeof buf, "%02ld:%02ld:%02ld", hours, mins, secs);
    } else {
        snprintf(buf, sizeof buf, "%02ld:%02ld", mins, secs);
    }
    return dup_str(buf);
}


/* ── relative time ───────────────────────────────────────────────────────── */

char *sc_humanize_relative(time_t when, time_t now) {
    double diff = difftime(now, when);
    bool past = diff >= 0;
    double a = diff < 0 ? -diff : diff;
    long sec = (long)(a + 0.5);

    char phrase[32];
    if (sec < 45) {
        return dup_str("just now");
    } else if (sec < 90) {
        snprintf(phrase, sizeof phrase, "1 minute");
    } else if (sec < 45L * 60) {
        snprintf(phrase, sizeof phrase, "%ld minutes", (sec + 30) / 60);
    } else if (sec < 90L * 60) {
        snprintf(phrase, sizeof phrase, "1 hour");
    } else if (sec < 22L * 3600) {
        snprintf(phrase, sizeof phrase, "%ld hours", (sec + 1800) / 3600);
    } else if (sec < 36L * 3600) {
        snprintf(phrase, sizeof phrase, "1 day");
    } else if (sec < 25L * 86400) {
        snprintf(phrase, sizeof phrase, "%ld days", (sec + 43200) / 86400);
    } else if (sec < 45L * 86400) {
        snprintf(phrase, sizeof phrase, "1 month");
    } else if (sec < 320L * 86400) {
        snprintf(phrase, sizeof phrase, "%ld months",
                 (sec + 1296000) / 2592000);
    } else if (sec < 548L * 86400) {
        snprintf(phrase, sizeof phrase, "1 year");
    } else {
        snprintf(phrase, sizeof phrase, "%ld years",
                 (sec + 15768000) / 31536000);
    }

    char out[64];
    if (past) { snprintf(out, sizeof out, "%s ago", phrase); }
    else      { snprintf(out, sizeof out, "in %s", phrase); }
    return dup_str(out);
}
