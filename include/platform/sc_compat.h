#pragma once

/**
 * @file sc_compat.h
 * @brief Cross-toolchain compatibility shims (GCC/Clang/MinGW/MSVC).
 *
 * sparcli builds with three C toolchains: Clang/GCC on macOS and Linux, MinGW
 * (UCRT64) on Windows for the development loop, and MSVC for the PyPI wheels.
 * They disagree on a handful of non-standard spellings, so the library uses the
 * neutral macros defined here instead of writing toolchain-specific keywords at
 * each site. On the GNU compilers every macro expands to exactly what the code
 * used before this header existed, so the POSIX build is byte-for-byte
 * unchanged.
 *
 * Provided here:
 * - `SC_ATTR_FORMAT(fmt, va)` - `printf`-style format-string checking.
 * - `SC_THREAD_LOCAL`         - thread-local storage-class specifier.
 * - `ssize_t`                 - POSIX signed size type (MSVC only).
 *
 * Symbol visibility / DLL linkage lives in <core/sparcli_export.h> (it already
 * has a Windows branch) and is intentionally not duplicated here.
 */


/* ── printf-style format-string checking ───────────────────────────────────
 *
 * GCC/Clang verify format strings against varargs via
 * `__attribute__((format(printf, fmt_idx, va_idx)))`. On MinGW the default
 * `printf` archetype follows the legacy msvcrt conversions and warns on C99
 * specifiers (%zu, %lld, ...) under -Wformat=2; the `gnu_printf` archetype
 * matches the C99/UCRT runtime sparcli actually targets. MSVC has no
 * equivalent attribute, so the macro vanishes there (its own analyzer handles
 * format checking via SAL, which we do not annotate).
 */
#if defined(__GNUC__)
#  if defined(__MINGW32__)
#    define SC_ATTR_FORMAT(fmt_idx, va_idx)                                    \
         __attribute__((format(gnu_printf, fmt_idx, va_idx)))
#  else
#    define SC_ATTR_FORMAT(fmt_idx, va_idx)                                    \
         __attribute__((format(printf, fmt_idx, va_idx)))
#  endif
#else
#  define SC_ATTR_FORMAT(fmt_idx, va_idx)
#endif


/* ── Thread-local storage ───────────────────────────────────────────────────
 *
 * C11 spells this `_Thread_local`; MSVC predates that spelling for C and uses
 * `__declspec(thread)`. (C23's `thread_local` keyword is not yet assumable
 * across all three toolchains.)
 */
#if defined(_MSC_VER)
#  define SC_THREAD_LOCAL __declspec(thread)
#else
#  define SC_THREAD_LOCAL _Thread_local
#endif


/* ── ssize_t ────────────────────────────────────────────────────────────────
 *
 * POSIX/MinGW provide `ssize_t` via <sys/types.h>; MSVC does not. Map it onto
 * the Win32 `SSIZE_T` from <BaseTsd.h>. Guarded against the typedef Microsoft's
 * own headers emit (`_SSIZE_T_DEFINED`) to avoid a redefinition.
 */
#if defined(_MSC_VER) && !defined(_SSIZE_T_DEFINED)
#  include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#  define _SSIZE_T_DEFINED
#endif


/* ── POSIX libc functions absent from the Windows CRT ───────────────────────
 *
 * `strndup` is POSIX.1-2008; neither the UCRT nor msvcrt declare it. The
 * case-insensitive comparators live in <strings.h> on POSIX (no such header on
 * Windows) but exist in the CRT under the `_str(n)icmp` names. The call sites
 * keep their portable spellings; these shims redirect them on Windows only, so
 * the POSIX build is untouched.
 */
#if defined(_WIN32)
#  include <stdlib.h>
#  include <string.h>

static inline char *sc_compat_strndup(const char *s, size_t n) {
    size_t len = 0;
    while (len < n && s[len] != '\0') { len++; }
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, s, len);
        copy[len] = '\0';
    }
    return copy;
}

#  ifndef strndup
#    define strndup sc_compat_strndup
#  endif
#  ifndef strcasecmp
#    define strcasecmp _stricmp
#  endif
#  ifndef strncasecmp
#    define strncasecmp _strnicmp
#  endif
#endif
