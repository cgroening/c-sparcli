#include "sparcli.h"


/* Build the version string from the compile-time macros so it cannot drift. */
#define SPARCLI_STR_(x) #x
#define SPARCLI_STR(x)  SPARCLI_STR_(x)

static const char *VERSION_STRING =
    SPARCLI_STR(SPARCLI_VERSION_MAJOR) "."
    SPARCLI_STR(SPARCLI_VERSION_MINOR) "."
    SPARCLI_STR(SPARCLI_VERSION_PATCH);


void sc_version(int *major, int *minor, int *patch) {
    if (major) { *major = SPARCLI_VERSION_MAJOR; }
    if (minor) { *minor = SPARCLI_VERSION_MINOR; }
    if (patch) { *patch = SPARCLI_VERSION_PATCH; }
}

const char *sc_version_string(void) {
    return VERSION_STRING;
}
