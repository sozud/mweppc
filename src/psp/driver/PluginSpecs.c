#include "driver/CLDropinCallbacks_V10.h"
#include "driver/CLErrors.h"
#include "driver/CWPlugins.h"
#include "driver/CWPluginsPrivate.h"
#include "driver/MacSpecs.h"
#include "driver/MsDos.h"
#include "driver/oslib.h"
#include <string.h>

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

extern void func_004247F0(SInt32 api, const OSTime* time, void* filetime);

// 0x415750: a shell time for this plugin
void func_00415750(CWPluginContext context, OSTime time, void* filetime) {
    func_004247F0(func_00424690(context), &time, filetime);
}

extern void func_00424750(SInt32 api, const void* filetime, OSTime* time);

// 0x415770: a plugin's time for the shell
void func_00415770(CWPluginContext context, const void* filetime, OSTime* time) {
    func_00424750(func_00424690(context), filetime, time);
}

// 0x424750: a plugin's file time as an OSTime (none or zero: now)
void func_00424750(SInt32 api, const void* filetime, OSTime* time) {
    switch (api) {
        case 1:
            if (!filetime || !memcmp(filetime, "\0\0\0\0\0\0\0\0", 8)) {
                OS_GetTime(time);
            } else {
                OS_MacToTime(*(const UInt32*)filetime, time);
            }
            return;
        case 2:
            if (!filetime || !memcmp(filetime, "\0\0\0\0\0\0\0\0", 8)) {
                OS_GetTime(time);
            } else {
                time->dwLowDateTime = ((const OSTime*)filetime)->dwLowDateTime;
                time->dwHighDateTime = ((const OSTime*)filetime)->dwHighDateTime;
            }
            return;
        default:
            CLFatalError("Unsupported CWPLUGIN_API %d\n", api);
            return;
    }
}

// 0x4247F0: an OSTime as a plugin's file time
void func_004247F0(SInt32 api, const OSTime* time, void* filetime) {
    switch (api) {
        case 1:
            OS_TimeToMac(*time, (UInt32*)filetime);
            return;
        case 2:
            ((OSTime*)filetime)->dwLowDateTime = time->dwLowDateTime;
            ((OSTime*)filetime)->dwHighDateTime = time->dwHighDateTime;
            return;
        default:
            CLFatalError("Unsupported CWPLUGIN_API %d\n", api);
            return;
    }
}
