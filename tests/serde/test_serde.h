#pragma once

#include "sparcli.h"
#include "serde/sparcli_serde.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file test_serde.h
 * @brief Shared harness for the serde test suite (`tests/serde/`).
 *
 * Pure logic tests over the data model and the format readers/writers - no
 * TTY required, deterministic, safe in CI. Kept out of `make test`/`qa`.
 */

extern int g_serde_failures;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) {                                                            \
            printf("  \033[32m✔\033[0m %s\n", (msg));                          \
        } else {                                                               \
            printf("  \033[31m✘\033[0m %s\n", (msg));                          \
            g_serde_failures++;                                                \
        }                                                                      \
    } while (0)

/* Test entry points (one per file). */
void test_serde_value(void);
void test_serde_json(void);
void test_serde_csv(void);
void test_serde_toml(void);
void test_serde_yaml(void);
void test_serde_markdown(void);
