// mwccpsp 3.0.1: the ELF library importer plugin's remaining callbacks and
// its registration (0x4104B0..0x410510; data at 0x5C425C..0x5C43B0). The
// rest of the plugin is in ELFImportGlue.c.
#include "driver/ClientGlue.h"
#include "driver/CWPlugins.h"
#include "driver/DropInCompilerLinker.h"

extern CWPLUGIN_ENTRY(ELFImport_main)(CWPluginContext context);
extern CWPLUGIN_ENTRY(ELFImport_GetDropInFlags)(const DropInFlags** flags, SInt32* flagsSize);
extern CWPLUGIN_ENTRY(ELFImport_GetDropInName)(const char** dropinName);
extern CWPLUGIN_ENTRY(ELFImport_GetDisplayName)(const char** displayName);
extern CWPLUGIN_ENTRY(ELFImport_GetTargetList)(const CWTargetList** targetList);
extern CWPLUGIN_ENTRY(ELFImport_GetDefaultMappingList)(const CWExtMapList** defaultMappingList);
extern CWPLUGIN_ENTRY(ELFImport_GetPanelList)(const CWPanelList** panelList);

static CWPLUGIN_ENTRY(LibImport_GetVersionInfo)(const VersionInfo** versionInfo) {
    static const VersionInfo vi = {2, 3, 3, 0};

    *versionInfo = &vi;
    return cwNoErr;
}

static CWPLUGIN_ENTRY(LibImport_GetFileTypeMappings)(const OSFileTypeMappingList** mappings) {
    // clang-format off
    static const OSFileTypeMapping ftmes[2] = {
        {'MPLF', "!<arch>\n", 8, 0, NULL},
        {'ELF ', "\x7F" "ELF", 4, 0, NULL}
    };
    // clang-format on
    static const OSFileTypeMappingList ftml = {2, ftmes};

    *mappings = &ftml;
    return cwNoErr;
}

static CWPLUGIN_ENTRY(LibImport_GetObjectFlags)(const CWObjectFlags** objectFlags) {
    // clang-format off
    static const CWObjectFlags myFlags = {2, 0x40000000, NULL, NULL, NULL, "s", NULL, NULL, 0, 0, 0, 0, 0, 0,
                                          'CWIE', 'TEXT', 0, 0};
    // clang-format on

    *objectFlags = &myFlags;
    return cwNoErr;
}

// clang-format off
static BasePluginCallbacks libimport_cb = {
    ELFImport_main,
    ELFImport_GetDropInFlags,
    ELFImport_GetDisplayName,
    ELFImport_GetDropInName,
    ELFImport_GetPanelList,
    NULL,
    NULL,
    LibImport_GetVersionInfo,
    LibImport_GetFileTypeMappings
};
// clang-format on

// clang-format off
static CompilerLinkerPluginCallbacks libimport_cl_cb = {
    ELFImport_GetTargetList,
    ELFImport_GetDefaultMappingList,
    NULL,
    NULL,
    LibImport_GetObjectFlags,
    NULL
};
// clang-format on

int RegisterStaticLibImporterPlugin(void) {
    return RegisterStaticCompilerLinkerPlugin(&libimport_cb, &libimport_cl_cb);
}

// clang-format off
static const char* STR911[] = {
    "Not enough memory to read ELF object file",
    "Can't open ELF object file",
    "Can't read ELF object file",
    "Invalid ELF object file",
    NULL
};
// clang-format on

int RegisterLibImporterResources(void) {
    return RegisterResource("ELF Importer Errors", 911, STR911);
}
