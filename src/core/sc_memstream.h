#pragma once

/**
 * @file sc_memstream.h
 * @brief Portable in-memory write stream (open_memstream shim).
 *
 * The capture engine renders a widget by pointing `sc_output_set_stream()` at
 * a `FILE *` whose bytes accumulate in a heap buffer, then reads that buffer
 * back. POSIX provides exactly this via `open_memstream(3)`; Windows has no
 * equivalent, so this shim backs the stream with a temporary file there and
 * slurps it into a malloc'd buffer at close.
 *
 * Usage mirrors open_memstream, with sc_memstream_close() replacing fclose():
 *
 *     char *buf = NULL; size_t size = 0;
 *     ScMemStream ms;
 *     FILE *fp = sc_memstream_open(&ms, &buf, &size);
 *     if (!fp) { ... }
 *     fputs("...", fp);                 // or sc_output_set_stream(fp) + render
 *     sc_memstream_close(&ms);          // fills buf/size
 *     // use buf (NUL-terminated, `size` bytes); buf is malloc'd - free() it
 *
 * After a successful close `*buf` is a heap buffer the caller owns and frees;
 * it is NUL-terminated and `*size` excludes that terminator, exactly like
 * open_memstream. On failure `*buf` stays NULL.
 */

#include <stddef.h>
#include <stdio.h>

typedef struct ScMemStream {
    FILE   *fp;        /* write target; valid between open and close */
    char  **buf_out;   /* caller's pointer; receives the heap buffer at close */
    size_t *size_out;  /* caller's pointer; receives the byte count at close */
} ScMemStream;

/**
 * Opens a memory-backed write stream. `buf`/`size` are the caller's output
 * pointers (set to NULL/0 here, filled by sc_memstream_close). Returns the
 * writable `FILE *` on success, or NULL on failure.
 */
FILE *sc_memstream_open(ScMemStream *ms, char **buf, size_t *size);

/**
 * Finalizes the stream: flushes, writes `*buf`/`*size`, releases resources.
 * Returns 0 on success, -1 on failure (with `*buf` left NULL). Safe to call
 * once per successful open; does nothing useful on an already-closed stream.
 */
int sc_memstream_close(ScMemStream *ms);
