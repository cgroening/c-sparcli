#pragma once

#include <stddef.h>

static inline int    min_i (int a,    int b)    { return a < b ? a : b; }
static inline int    max_i (int a,    int b)    { return a > b ? a : b; }
static inline size_t min_sz(size_t a, size_t b) { return a < b ? a : b; }
static inline size_t max_sz(size_t a, size_t b) { return a > b ? a : b; }

static inline int min_i_n(const int *arr, size_t n) {
    int r = arr[0];
    for (size_t i = 1; i < n; i++) r = min_i(r, arr[i]);
    return r;
}

static inline int max_i_n(const int *arr, size_t n) {
    int r = arr[0];
    for (size_t i = 1; i < n; i++) r = max_i(r, arr[i]);
    return r;
}

static inline size_t min_sz_n(const size_t *arr, size_t n) {
    size_t r = arr[0];
    for (size_t i = 1; i < n; i++) r = min_sz(r, arr[i]);
    return r;
}

static inline size_t max_sz_n(const size_t *arr, size_t n) {
    size_t r = arr[0];
    for (size_t i = 1; i < n; i++) r = max_sz(r, arr[i]);
    return r;
}
