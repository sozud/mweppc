#ifndef DRIVER_CLLOADANDCACHE_H
#define DRIVER_CLLOADANDCACHE_H

#include "compiler/common.h"
#include "driver/oslib.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MWCCPSP
extern short FixTextHandle(DriverHandle txt);
extern int LoadAndCacheFile(OSSpec* spec, DriverHandle* texthandle, Boolean* precomp);
extern void CopyFileText(DriverHandle src, char** text, SInt32* textsize);
#else
extern short FixTextHandle(Handle txt);
extern int LoadAndCacheFile(OSSpec* spec, Handle* texthandle, Boolean* precomp);
extern void CopyFileText(Handle src, char** text, SInt32* textsize);
#endif

#ifdef __cplusplus
}
#endif

#endif
