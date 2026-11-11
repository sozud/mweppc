#include "driver/ParserHelpers-cc.h"
#include "driver/CWPlugins.h"
#include "driver/oslib.h"
#include "driver/parser.h"
#include "driver/ParserErrors.h"
#include "driver/ParserFace.h"
#include "driver/ParserHelpers.h"
#include "driver/Targets.h"
#include <stdio.h>
#include <string.h>
#if MWCCPSP
#include "driver/option_system.h"
#endif

Handle definesHandle;

int Opt_AddStringToDefines(const char* opt, void* str, const char* param, int flags) {
    AddStringToHandle(&definesHandle, (const char*)str);
    if (param) {
        AddStringToHandle(&definesHandle, param);
    }
    return TRUE;
}

int Opt_DefineSymbol(const char* name, const char* value) {
    char line[1024];

    if (pTool->LANG == Lang_C_CPP || pTool->LANG == 'Asm ' || pTool->LANG == Lang_Rez) {
        sprintf(line, "#define %s %s\n", name, value ? value : "1");
    } else if (pTool->LANG == Lang_Pascal) {
        sprintf(line, "{$definec %s %s}\n", name, value ? value : "1");
#if MWCCPSP
    } else if (pTool->LANG == 'MAsm') {
        // 3.0.1: the MIPS assembler's directives have no '#'
        sprintf(line, "define %s %s\n", name, value ? value : "1");
#endif
    } else {
        sprintf(line, "Option '-D|d' is not supported with this plugin");
        CLPReportError(CLPStr28_WarningText, line);
        return FALSE;
    }

    AddStringToHandle(&definesHandle, line);
    return TRUE;
}

int Opt_UndefineSymbol(const char* opt, void* var, const char* arg, int flags) {
    char line[300];

    if (pTool->LANG == Lang_C_CPP || pTool->LANG == 'Asm ') {
        sprintf(line, "#undef %s\n", arg);
    } else if (pTool->LANG == Lang_Pascal) {
        sprintf(line, "{$undefc %s}\n", arg);
#if MWCCPSP
    } else if (pTool->LANG == 'MAsm') {
        sprintf(line, "undef %s\n", arg);
#endif
    } else {
        sprintf(line, "Option -%s is not supported with this plugin", opt);
        CLPReportError(CLPStr28_WarningText, line);
        return FALSE;
    }

    AddStringToHandle(&definesHandle, line);
    return TRUE;
}

int Opt_AddPrefixFile(const char* opt, void* handle, const char* filename, int flags) {
    char line[300];
    Handle* storage;

    if (!handle) {
        storage = &definesHandle;
    } else {
        storage = (Handle*)handle;
    }

    if (!filename[0]) {
        return TRUE;
    }

    if (pTool->LANG == Lang_C_CPP || pTool->LANG == 'Asm ') {
        if (filename[0] == '<' && filename[strlen(filename) - 1] == '>') {
            snprintf(line, sizeof(line), "#include %s\n", filename);
        } else {
            snprintf(line, sizeof(line), "#include \"%s\"\n", filename);
        }
    } else if (pTool->LANG == Lang_Pascal) {
        snprintf(line, sizeof(line), "{$I+}\n{$I %s}\n{$I-}\n", filename);
#if MWCCPSP
    } else if (pTool->LANG == 'MAsm') {
        snprintf(line, sizeof(line), "include \"%s\"\n", filename);
#endif
    } else {
        sprintf(line, "Option -%s is not supported with this plugin", opt);
        CLPReportError(CLPStr28_WarningText, line);
        return FALSE;
    }

    AddStringToHandle(storage, line);
    return TRUE;
}

#if MWCCPSP
// 3.0.1 (0x411CE9, 0x411D03): an option that sets a pragma variable, and its
// reverse
int Opt_PragmaOnOff(const char* opt, void* var, const char* arg, int flags) {
    if (flags & PARAMPARSEFLAGS_NEGATED) {
        *(char*)var = PR_OFF;
    } else {
        *(char*)var = PR_ON;
    }
    return TRUE;
}

int Opt_PragmaOffOn(const char* opt, void* var, const char* arg, int flags) {
    if (flags & PARAMPARSEFLAGS_NEGATED) {
        *(char*)var = PR_ON;
    } else {
        *(char*)var = PR_OFF;
    }
    return TRUE;
}
#endif
#if MWCCPSP

#endif
int SetupPragmas(const Pragma* pragmas) {
    const char* set;
    char on;
    char off;
    Boolean reverse;
    char line[300];

    while (pragmas->pragma) {
        if (pragmas->flags == PRAGMA_FLAGS_ONOFF || pragmas->flags == PRAGMA_FLAGS_REVERSED) {
            set = NULL;
            reverse = pragmas->flags == PRAGMA_FLAGS_REVERSED;
            on = !reverse ? PR_ON : PR_OFF;
            off = !reverse ? PR_OFF : PR_ON;
            if (*((char*)pragmas->value) == on) {
                set = "on";
            } else if (*((char*)pragmas->value) == off) {
                set = "off";
            } else if (*((char*)pragmas->value) == PR_AUTO) {
                set = "auto";
            } else if (*((char*)pragmas->value) == PR_RESET) {
                set = "reset";
            } else {
#if MWCCPSP
                OS_ASSERT(191, *((char *)pragmas->value) == PR_UNSET);
#else
                OS_ASSERT(186, *((char *)pragmas->value) == PR_UNSET);
#endif
            }

            if (set) {
                snprintf(line, sizeof(line), "#pragma %s %s\n", pragmas->pragma, set);
                AddStringToHandle(&definesHandle, line);
            }
        } else {
#if MWCCPSP
            OS_ASSERT(200, !"Can't handle pragma");
#else
            OS_ASSERT(195, !"Can't handle pragma");
#endif
        }
        pragmas++;
    }

    return TRUE;
}
#if MWCCPSP

#endif
#if MWCCPSP
// 3.0.1 (0x411E32): "name" or "no-name" becomes "#pragma name on|off" in the
// prefix text
int Opt_SetPragma(const char* opt, void* var, const char* arg, int flags) {
    const char* name;
    int on;
    int skip;

    name = arg ? arg : (const char*)var;
    if (!name) {
        return FALSE;
    }
    if (!strncmp(name, "no-", 3)) {
        on = (flags & PARAMPARSEFLAGS_NEGATED) != 0;
        skip = 3;
    } else {
        on = (flags & PARAMPARSEFLAGS_NEGATED) == 0;
        skip = 0;
    }
    if (!Opt_AddStringToDefines(opt, "#pragma ", name + skip, flags)) {
        return FALSE;
    }
    return Opt_AddStringToDefines(opt, on ? " on\n" : " off\n", NULL, flags);
}
#endif
