/**
 * @file cli_stdin.h
 * Helpers for reading command input from a file argument or from stdin.
 */
#pragma once

#include <stddef.h>

/**
 * Reads the entire contents of `path` into a NUL-terminated heap buffer.
 *
 * `NULL` or `"-"` reads from stdin instead of a file.
 *
 * Input larger than 64 MiB is rejected (returns `NULL`), so untrusted
 * data cannot exhaust memory.
 *
 * @param path  File path, `"-"`, or `NULL` for stdin.
 * @return      Heap buffer (caller frees), or `NULL` on open/read/alloc
 *              failure or oversized input.
 */
char *sc_cli_read_source(const char *path);

/**
 * Splits `data` into lines in place (each `\n` is replaced with NUL).
 *
 * Trailing `\r` characters (CRLF input) are stripped from each line. A
 * trailing final newline does not produce an extra empty line.
 *
 * @param data   Buffer to split; modified in place and must outlive the
 *               returned array.
 * @param count  Receives the number of lines.
 * @return       Heap array of pointers into `data` (caller frees the array
 *               only), or `NULL` on allocation failure.
 */
char **sc_cli_split_lines(char *data, size_t *count);
