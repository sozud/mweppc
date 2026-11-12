#include "driver/ClientGlue.h"
#include "driver/CLErrors.h"
#include "driver/CLMain.h"
#include "driver/CLPlugins.h"
#include "driver/CWPlugins.h"
#include "driver/oslib.h"
#include "driver/Resources.h"
#include "driver/ResourceStrings.h"
#include <string.h>
#if MWCCPS2
#include "compiler/win32.h"
// declared here, not in win32.h: new declarations there renumber other units' statics
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(
    HMODULE hModule, LPSTR lpFilename, DWORD nSize
);
__declspec(dllimport) HMODULE __stdcall LoadLibraryA(LPCSTR lpLibFileName);
__declspec(dllimport) BOOL __stdcall FreeLibrary(HMODULE hLibModule);
#endif

#if MWCCPS2
// b210 first looks for the string table in the exe's own Win32 resources
int __stdcall RegisterResource(const char* name, SInt16 rsrcid, const char** list) {
    char str[256];
    char path[260];
    HMODULE module;
    int len;
    Handle h;

    if (list) {
        return Res_AddResource(name, rsrcid, list);
    }
    if (!GetModuleFileNameA(GetModuleHandleA(NULL), path, 259)) {
        OS_SpecToString(&clState.programSpec, path, 260);
    }
    module = LoadLibraryA(path);
    if (module) {
        len = LoadStringA(module, 101, str, 256);
        FreeLibrary(module);
        if (len) {
            return TRUE;
        }
    }
    h = GetResource('STR#', rsrcid);
    if (h) {
        ReleaseResource(h);
        return TRUE;
    }
    CLFatalError("Resource ('STR#',%d) '%s' not found in executable\n", rsrcid, name);
    return FALSE;
}
#else
int __stdcall RegisterResource(const char* name, SInt16 rsrcid, const char** list) {
    Handle h;

    if (list == NULL) {
        h = GetResource('STR#', rsrcid);
        if (h == NULL) {
            CLFatalError("Resource ('STR#',%d) '%s' not found in executable\n", rsrcid, name);
            return FALSE;
        }
        ReleaseResource(h);
        return TRUE;
    } else {
        return Res_AddResource(name, rsrcid, list);
    }
}
#endif

int __stdcall RegisterStaticPlugin(const BasePluginCallbacks* callbacks) {
    return Plugins_Add(Plugin_New(callbacks, NULL, NULL));
}

int __stdcall RegisterStaticCompilerLinkerPlugin(
    const BasePluginCallbacks* callbacks, const CompilerLinkerPluginCallbacks* cl_callbacks
) {
    return Plugins_Add(Plugin_New(callbacks, cl_callbacks, NULL));
}

int __stdcall RegisterStaticParserPlugin(
    const BasePluginCallbacks* callbacks, const ParserPluginCallbacks* pr_callbacks
) {
    return Plugins_Add(Plugin_New(callbacks, NULL, pr_callbacks));
}

void __stdcall SetBuildTarget(OSType cpu, OSType os) {
    clState.cpu = cpu;
    clState.os = os;
}

void __stdcall SetParserType(OSType plang) {
    clState.parserstyle = plang;
}

void __stdcall SetPluginType(OSType lang, OSType type) {
    clState.language = lang;
    clState.plugintype = type;
}

int __stdcall CmdLine_Initialize(
    int argc, char** argv, const char* builddate, const char* buildtime
) {
    strncpy(cmdline_build_date, builddate, sizeof(cmdline_build_date));
    strncpy(cmdline_build_time, buildtime, sizeof(cmdline_build_time));
    return Main_Initialize(argc, argv);
}

int __stdcall CmdLine_Driver(void) {
    return Main_Driver();
}

int __stdcall CmdLine_Terminate(int exitcode) {
    return Main_Terminate(exitcode);
}
