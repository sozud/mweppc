// mwccps2 b210/b198's CompilerSetupOptions.c: the GCC-compatible command line's parse
// steps. Only MidParse is placed so far (PreParse 0x412800 and PostParse 0x4129E0 are
// not yet decompiled).
#include "compiler/mwcc_core.h"
#include "compiler/common.h"
#include "driver/ParserHelpers.h"
#include "driver/ParserHelpers-cc.h"
#include "driver/ToolHelpers.h"
#include "driver/pref_structs.h"

#if MWCCPS2 && !MWCC_CORE
Boolean ppDumpFlag; // b198 0x637366: -E with preprocessor output kept
Boolean cppPS2Includes = TRUE; // 0x5DDBAC: C++ searches MWCppPS2Includes
extern Boolean useDefaultIncludes; // ParserGlue-psp.c (its compilerPS2Includes)

// b198 0x4128B0: after the options are parsed, the preprocess stage when only
// preprocessing, and the include paths from the environment
int func_004128B0(void) {
    const char* match1;
    const char* match2;
    char* value;

    if (ppDumpFlag) {
        if (!(pCmdLine.stages & 1)) {
            pCmdLine.state = 2;
            pCmdLine.toDisk |= 1;
            if (!Opt_SetStage("-P", "Pp", NULL, 0)) {
                return 0;
            }
        } else {
            if (!Opt_AddStringToDefines("-P", "#pragma simple_prepdump on\n", NULL, 0)) {
                return 0;
            }
        }
    }

    if ((int)pCmdLine.state == 0) {
        pCmdLine.state = 3;
    }

    if (cppPS2Includes) {
        value = GetEnvVar("MWCppPS2Includes\0MWCppIncludes\0", 0, &match1);
        if (value && !AddAccessPathList(value, ';', ':', 0, NULL, 1, -1, 0)) {
            return 0;
        }
    }

    if (useDefaultIncludes) {
        value = GetEnvVar("MWCPS2Includes\0MWCIncludes\0", 0, &match2);
        if (value && !AddAccessPathList(value, ';', ':', 0, NULL, 1, -1, 0)) {
            return 0;
        }
    }
    return 1;
}
#endif
