#ifndef DRIVER_CLTARG_H
#define DRIVER_CLTARG_H

#include "compiler/common.h"
#include "driver/cmdline.h"
#include "driver/oslib.h"
#include "driver/CLAccessPaths.h"
#include "driver/CLDependencies.h"
#include "driver/CLFiles.h"
#include "driver/CLOverlays.h"
#include "driver/CLSegs.h"
#include "driver/DropInCompilerLinker.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LinkModel_Flat = 0,
    LinkModel_Segmented = 1,
    LinkModel_Overlay1 = 2
};

#if MWCCPSP
// 3.0.1: the shell keeps its own target info (0x820 bytes, Target_New)
typedef struct ShellTargetInfo {
    UInt32 targetCPU;
    UInt32 targetOS;
    short outputType;
    SInt16 linkType;
    UInt8 canRun;
    UInt8 canDebug;
    OSSpec outfile; // +0x0E
    OSSpec symfile; // +0x212
    OSSpec runfile; // +0x416
    OSSpec linkAgainstFile; // +0x61A
} ShellTargetInfo;
#define TARGET_INFO ShellTargetInfo

#endif
struct Target {
    struct BuildInfo {
        UInt32 linesCompiled;
        UInt32 codeSize;
        UInt32 iDataSize;
        UInt32 uDataSize;
    } info;
#if MWCCPSP
    TARGET_INFO* targetinfo;
#else
    CWTargetInfo* targetinfo;
#endif
    struct {
        Segments segs;
        Overlays overlays;
    } linkage;
    SInt32 linkmodel;
    Files files;
    Files pchs;
    Incls incls;
    Paths sysPaths;
    Paths userPaths;
    OSType lang;
    OSType cpu;
    OSType os;
    char targetName[64];
    struct Plugin* preLinker;
    struct Plugin* linker;
    struct Plugin* postLinker;
    UInt32 preLinkerDropinFlags;
    UInt32 linkerDropinFlags;
    UInt32 postLinkerDropinFlags;
    OSPathSpec outputDirectory;
    VFile* virtualFiles;
    struct Target* next;
};
extern Target* Target_New(const char* name, OSType cpu, OSType os, OSType lang);
extern void Targets_Term(Target* list);
extern void Target_Add(Target** list, Target* targ);

#ifdef __cplusplus
}
#endif

#endif
