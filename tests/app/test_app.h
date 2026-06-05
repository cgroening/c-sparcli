#pragma once

#include <stdio.h>

/**
 * @file test_app.h
 * @brief Shared harness for the application-framework test suite
 *        (`tests/app/`): XDG paths, pager, pretty errors, logging.
 *
 * Pure logic tests - no TTY required, safe in CI.
 */

extern int g_app_failures;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) {                                                            \
            printf("  \033[32m✔\033[0m %s\n", (msg));                          \
        } else {                                                               \
            printf("  \033[31m✘\033[0m %s\n", (msg));                          \
            g_app_failures++;                                                  \
        }                                                                      \
    } while (0)

/* Test entry points (one per file). */
void test_paths(void);
void test_pager(void);
void test_errors(void);
void test_log(void);
void test_process(void);
void test_config(void);
