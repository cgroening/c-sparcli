#pragma once

/**
 * @file sparcli_export.h
 * @brief Library-wide visibility, linkage and version macros.
 *
 * Include this header (directly or transitively via `sparcli.h`) when
 * declaring or consuming sparcli's public API. The macros here are:
 *
 * - `SPARCLI_EXPORT` — annotation for symbols that are part of the public
 *   ABI. Resolves to the appropriate visibility / dllexport directive for
 *   the current toolchain, or to nothing on platforms that do not need it.
 * - `SPARCLI_BEGIN_DECLS` / `SPARCLI_END_DECLS` — `extern "C"` brackets so
 *   the headers are usable from C++ callers.
 * - `SPARCLI_VERSION_*` — compile-time version constants. Callers compare
 *   these against the runtime values from `sc_version_*()` to detect ABI
 *   skew between the library they were compiled against and the one
 *   loaded at runtime.
 */


/* ── extern "C" brackets ────────────────────────────────────────────────── */

#ifdef __cplusplus
#  define SPARCLI_BEGIN_DECLS extern "C" {
#  define SPARCLI_END_DECLS   }
#else
#  define SPARCLI_BEGIN_DECLS
#  define SPARCLI_END_DECLS
#endif


/* ── Symbol visibility ──────────────────────────────────────────────────── */

/**
 * `SPARCLI_EXPORT` marks a symbol as part of the public ABI.
 *
 * On Windows it expands to `__declspec(dllexport)` when sparcli itself is
 * being built (when `SPARCLI_BUILDING` is defined by the build system) and
 * to `__declspec(dllimport)` when sparcli is being consumed. On Unix-like
 * systems it expands to `__attribute__((visibility("default")))` so that
 * symbols not so annotated can be hidden via `-fvisibility=hidden`.
 */
#if defined(_WIN32) || defined(__CYGWIN__)
#  ifdef SPARCLI_BUILDING
#    define SPARCLI_EXPORT __declspec(dllexport)
#  else
#    define SPARCLI_EXPORT __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define SPARCLI_EXPORT __attribute__((visibility("default")))
#else
#  define SPARCLI_EXPORT
#endif


/* ── Version ────────────────────────────────────────────────────────────── */

/** Library major version. Bumped on ABI-breaking changes. */
#define SPARCLI_VERSION_MAJOR 0

/** Library minor version. Bumped on additive ABI changes. */
#define SPARCLI_VERSION_MINOR 1

/** Library patch version. Bumped on backwards-compatible fixes. */
#define SPARCLI_VERSION_PATCH 0

/** Encoded version as `MAJOR * 10000 + MINOR * 100 + PATCH`. */
#define SPARCLI_VERSION                                                       \
    (SPARCLI_VERSION_MAJOR * 10000                                            \
     + SPARCLI_VERSION_MINOR * 100                                            \
     + SPARCLI_VERSION_PATCH)


SPARCLI_BEGIN_DECLS

/**
 * Returns the runtime library version components.
 *
 * Each pointer is optional and may be `NULL` if the caller is not
 * interested in that component.
 */
SPARCLI_EXPORT void sc_version(int *major, int *minor, int *patch);

/** Returns a human-readable version string, e.g. `"0.1.0"`. */
SPARCLI_EXPORT const char *sc_version_string(void);

SPARCLI_END_DECLS
