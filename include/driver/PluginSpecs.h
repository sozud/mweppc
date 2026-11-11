#ifndef DRIVER_PLUGINSPECS_H
#define DRIVER_PLUGINSPECS_H

#include "driver/oslib.h"

// 3.0.1 (src/psp/driver/PluginSpecs.c): file specs in a plugin's format.
// API 1 is the Mac FSSpec, API 2 a path (CWFileSpec); the driver itself uses 2.
struct CWPluginPrivateContext;
extern SInt32 func_00424690(struct CWPluginPrivateContext* context);
extern int func_004246A0(SInt32 api, const void* pluginspec, OSSpec* spec);
extern int func_004246F0(SInt32 api, const OSSpec* spec, void* pluginspec);
extern int func_00415710(
    struct CWPluginPrivateContext* context, const OSSpec* spec, void* pluginspec
);
extern int func_00415730(
    struct CWPluginPrivateContext* context, const void* pluginspec, OSSpec* spec
);

#endif
