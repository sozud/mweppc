#ifndef COMPILER_UB_PROBE_H
#define COMPILER_UB_PROBE_H

// Native builds only: the places where the original compiler reads memory it
// never wrote (docs: mwccps2-ub) take a fixed value in the native, the one the
// original usually sees. MWCC_UB_FILL overrides that value per kind, so a unit
// built twice with different fills shows whether its output depends on the
// original's leftover bytes (tools/mwccpsp/ub_probe.py):
//
//   MWCC_UB_FILL=operand=0xffff        memory/immediate operand class+flags (UB 2)
//   MWCC_UB_FILL=hascall=0xff          constant node hascall byte (UB 1)
//   MWCC_UB_FILL=all=0xff              every kind
//
// Several kinds are separated by commas. Unset, every site keeps its default.

#if MWCCPSP_NATIVE
#include <stdlib.h>
#include <string.h>

static inline int mwcc_ub_fill(const char* kind, int dflt) {
    const char* s = getenv("MWCC_UB_FILL");
    size_t n = strlen(kind);

    while (s && *s) {
        if ((!strncmp(s, kind, n) && s[n] == '=') || (!strncmp(s, "all", 3) && s[3] == '=')) {
            return (int)strtol(strchr(s, '=') + 1, NULL, 0);
        }
        s = strchr(s, ',');
        if (s) {
            s++;
        }
    }
    return dflt;
}
#endif

#endif
