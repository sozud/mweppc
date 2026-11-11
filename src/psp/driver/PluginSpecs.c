#include "driver/CLDropinCallbacks_V10.h"
#include "driver/CLErrors.h"
#include "driver/CWPlugins.h"
#include "driver/CWPluginsPrivate.h"
#include "driver/MacSpecs.h"
#include "driver/MsDos.h"
#include "driver/oslib.h"

#pragma auto_inline off

// 3.0.1: plugins name files in the format of their plugin API version
// (CWPLUGIN_API 1: Mac FSSpec, 2: a Windows path); the shell keeps OSSpecs
// and converts at the plugin boundary (0x424690..0x424750, 0x415710).

// 0x424690
SInt32 func_00424690(CWPluginContext context) {
    return ((shellContextType*)context->shellContext)->pluginAPI;
}

// 0x4246A0: a plugin's spec as an OSSpec
int func_004246A0(SInt32 api, const void* pluginspec, OSSpec* spec) {
    switch (api) {
        case 1:
            return OS_FSSpec_To_OSSpec((const FSSpec*)pluginspec, spec);
        case 2:
            return OS_MakeSpec((const char*)pluginspec, spec, NULL);
        default:
            CLFatalError("Unsupported CWPLUGIN_API %d\n", api);
            return -1;
    }
}

// 0x4246F0: an OSSpec in a plugin's format
int func_004246F0(SInt32 api, const OSSpec* spec, void* pluginspec) {
    switch (api) {
        case 1:
            return OS_OSSpec_To_FSSpec(spec, (FSSpec*)pluginspec);
        case 2:
            if (OS_SpecToString(spec, (char*)pluginspec, 260)) {
                return 0;
            }
            return 111;
        default:
            CLFatalError("Unsupported CWPLUGIN_API %d\n", api);
            return -1;
    }
}

// 0x415710: a shell spec for this plugin
int func_00415710(CWPluginContext context, const OSSpec* spec, void* pluginspec) {
    return func_004246F0(func_00424690(context), spec, pluginspec);
}

// 0x415730: a plugin's spec for the shell
int func_00415730(CWPluginContext context, const void* pluginspec, OSSpec* spec) {
    return func_004246A0(func_00424690(context), pluginspec, spec);
}
