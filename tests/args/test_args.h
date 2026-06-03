#pragma once

#include "sparcli.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file test_args.h
 * @brief Shared harness for the argument-parser test suite (`tests/args/`).
 *
 * Pure logic tests - no TTY required, safe in CI.
 */

extern int g_args_failures;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) {                                                            \
            printf("  \033[32m✔\033[0m %s\n", (msg));                          \
        } else {                                                               \
            printf("  \033[31m✘\033[0m %s\n", (msg));                          \
            g_args_failures++;                                                 \
        }                                                                      \
    } while (0)

/* Test entry points (one per file). */
void test_args_parse(void);
void test_args_errors(void);
void test_args_help(void);
void test_args_split(void);

/* Shared helpers (test_args_main.c). */

/** Builds the standard demo parser tree used across the test files. */
ScArgs *test_args_build_demo(void);

/** Runs `sc_args_parse` while capturing stderr; returns the captured text
 *  (heap; caller frees) and writes the status/matched results. */
char *test_args_parse_capture_stderr(
    ScArgs *args, int argc, char **argv,
    ScArgsStatus *status, const ScArgsCmd **matched
);

/** Captures everything a callback prints to the output stream. */
char *test_args_capture_output(void (*render)(ScArgs *args), ScArgs *args);

/** NULL-safe substring check. */
bool test_args_contains(const char *haystack, const char *needle);
