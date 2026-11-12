// mwccpsp 3.0.1: the PSP command-line parser tool (0x406A90..0x406E50),
// mweppc's ParserGlue-eabi-ppc-cc.c for the PSP. 3.0.1 has two option sets:
// the Metrowerks one and "GCC compatible options", picked by a first
// argument of --gcc-compat/--mw-compat or the MWCL_GCC_COMPAT variable.
//
// The tables (preference panels with their defaults, the option lists and the
// ParserTool records) are in ParserGlue-psp.opt.c, generated from the exe by
// tools/mwccpsp/gen_parser_psp.py.
#include "driver/Arguments.h"
#include "driver/IO.h"
#include "driver/OptimizerHelpers.h"
#include "driver/option_system.h"
#include "driver/parser.h"
#include "driver/ParserFace.h"
#include "driver/cmdline.h"
#include "driver/CWParserPluginsPrivate.h"
#include "driver/ParserHelpers.h"
#include "driver/ParserHelpers-cc.h"
#include "driver/pref_structs.h"
#include "driver/Projects.h"
#include "driver/StdTargetWarningHelpers-cc.h"
#include "driver/StringUtils.h"
#include "driver/Targets.h"
#include "driver/ToolHelpers.h"
#include "driver/ToolHelpers-cc.h"
#include <stdlib.h>
#include <string.h>

extern PCmdLine pCmdLine;
extern PFrontEndC pFrontEndC;
extern Handle definesHandle;
extern ParserTool* pTool;
extern ParserTool parser_mw; // 0x5BDA04
extern ParserTool parser_gcc; // 0x5BDA8C
extern ParserTool parser_dispatch; // 0x5BDB04

extern int func_00412800(void); // the GCC compatible tool's PreParse
extern int func_004128B0(void); // its MidParse
extern int func_004129E0(void); // its PostParse
extern int func_004148F0(const char* a, const char* b); // a case-insensitive compare

#if MWCCPSP_NATIVE
// the native build's option tables (src/native/parser_tables.c) point at it
Boolean useDefaultIncludes = TRUE;
// the panels those tables define, at their 3.0.1 sizes
typedef char check_pCmdLine[sizeof(PCmdLine) == 28 ? 1 : -1];
typedef char check_pCmdLineLinker[sizeof(PCmdLineLinker) == 6 ? 1 : -1];
typedef char check_pFrontEndC[sizeof(PFrontEndC) == 320 ? 1 : -1];
typedef char check_pWarningC[sizeof(PWarningC) == 14 ? 1 : -1];
typedef char check_pGlobalOptimizer[sizeof(PGlobalOptimizer) == 12 ? 1 : -1];
#else
static Boolean useDefaultIncludes = TRUE; // 0x5B0C2C
#endif
static int gcc_compat; // 0x624B5C

// the tool records point at the callbacks; natively those records are in
// another unit
#if MWCCPSP_NATIVE
#define TOOL_CALLBACK
#else
#define TOOL_CALLBACK static
#endif

TOOL_CALLBACK int PreParse(void) {
    if (!gcc_compat) {
        useDefaultIncludes = TRUE;
        setLinkerOutputFilename = FALSE;
        linkerOutputFilename[0] = 0;
        definesHandle = NULL;
    } else if (!func_00412800()) {
        return FALSE;
    }
    return TRUE;
}

TOOL_CALLBACK int MidParse(void) {
    const char* match;
    const char* env;

    if (gcc_compat) {
        if (!func_004128B0()) {
            return FALSE;
        }
    } else {
        if (pCmdLine.state == OptsCmdLineState_Unset) {
            pCmdLine.state = OptsCmdLineState_CompileAndLink;
        }
        if (parseopts.possibleFiles > 0 && useDefaultIncludes) {
#if MWCCPS2
            if ((env = GetEnvVar("MWCPSX2MIPSIncludes\0MWCIncludes\0", TRUE, &match))) {
#else
            if ((env = GetEnvVar("MWCPSPMIPSIncludes\0MWCIncludes\0", TRUE, &match))) {
#endif
                if (!AddAccessPathList(env, ';', ':', 1, (char*)match, TRUE, -1, FALSE)) {
                    return FALSE;
                }
            }
        }
    }
    return TRUE;
}

#if MWCCPS2
extern Pragma ps2_targetPragmas[]; // defined at the end of this file
#endif

TOOL_CALLBACK int PostParse(void) {
    static const char* name = "(command-line defines)";

    if (gcc_compat) {
        if (!func_004129E0()) {
            return FALSE;
        }
    } else {
#if MWCCPS2
        // the table at b210 0x5C9C44 (PSP 0x5B0C44, misnamed irPragmas in
        // tools/mwccpsp/config/PSP_3_0_1/symbols.txt) is the target's: far_call, mips16.
        // irPragmas itself is at 0x5DD984 (the GCC tool's PostParse).
        if (!SetupPragmas(ps2_targetPragmas) || !SetupPragmas(warningPragmas)) {
#else
        if (!SetupPragmas(irPragmas) || !SetupPragmas(warningPragmas)) {
#endif
            return FALSE;
        }
        if (definesHandle) {
            if (pCmdLine.verbose) {
                ShowTextHandle(name, definesHandle);
            }
            AddVirtualFile(name, &definesHandle);
            c2pstrcpy((StringPtr)pFrontEndC.prefixname, name);
        } else {
            pFrontEndC.prefixname[0] = 0;
        }
        FinishCompilerTool();
        if (!ValidateToolState(TRUE)) {
            return FALSE;
        }
        if (linkerOutputFilename[0]) {
            Arg_AddToToolArgs(&linkargs, ATK_ARG_END, NULL);
            Arg_AddToToolArgs(&linkargs, ATK_ARG, "-o");
            Arg_AddToToolArgs(&linkargs, ATK_ARG_END, NULL);
            Arg_AddToToolArgs(&linkargs, ATK_ARG, linkerOutputFilename);
            Arg_AddToToolArgs(&linkargs, ATK_ARG_END, NULL);
        }
    }
    return TRUE;
}

// the dispatching tool: its PreParse installs the real one
TOOL_CALLBACK int Dispatch_PreParse(void) {
    CWCommandLineArgs* args = parseopts.args;
    char* env;

    if (args->argc > 1 && !strcmp(args->argv[1], "--mw-compat")) {
        gcc_compat = FALSE;
        memmove(&args->argv[1], &args->argv[2], (args->argc - 1) * sizeof(char*));
        args->argc--;
        Arg_AddToToolArgs(&linkargs, ATK_ARG, "--mw-compat");
    } else if (args->argc > 1 && !strcmp(args->argv[1], "--gcc-compat")) {
        gcc_compat = TRUE;
        memmove(&args->argv[1], &args->argv[2], (args->argc - 1) * sizeof(char*));
        args->argc--;
        Arg_AddToToolArgs(&linkargs, ATK_ARG, "--gcc-compat");
    } else {
        env = getenv("MWCL_GCC_COMPAT");
        if (env && func_004148F0(env, "off") && func_004148F0(env, "no")) {
            gcc_compat = TRUE;
        } else {
            gcc_compat = FALSE;
        }
    }

    if (!gcc_compat) {
        if (!SetParserToolInfo(&parser_mw) || !SetupParserToolOptions()) {
            return FALSE;
        }
    } else {
        if (!SetParserToolInfo(&parser_gcc) || !SetupParserToolOptions()) {
            return FALSE;
        }
    }
    return pTool->PreParse();
}

TOOL_CALLBACK int Dispatch_MidParse(void) {
    return FALSE;
}

TOOL_CALLBACK int Dispatch_PostParse(void) {
    return FALSE;
}

int RegisterStaticParserToolInfo(void) {
    return SetParserToolInfo(&parser_dispatch);
}

#if MWCCPS2
// b210 0x5C9C44: the target pragmas the command line can set
extern UInt8 data_00625B1A;
extern UInt8 data_00625B1B;
Pragma ps2_targetPragmas[] = {
    {&data_00625B1A, "far_call", 0}, {&data_00625B1B, "mips16", 0}, {0, 0, 0}
};
#endif
