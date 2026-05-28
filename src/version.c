#include "sparcli.h"


/** Stringified version literal; updated together with the version macros. */
static const char *VERSION_STRING = "0.1.0";


void sc_version(int *major, int *minor, int *patch) {
    if (major) { *major = SPARCLI_VERSION_MAJOR; }
    if (minor) { *minor = SPARCLI_VERSION_MINOR; }
    if (patch) { *patch = SPARCLI_VERSION_PATCH; }
}

const char *sc_version_string(void) {
    return VERSION_STRING;
}
