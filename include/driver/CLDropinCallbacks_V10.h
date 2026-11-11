#ifndef DRIVER_CLDROPINCALLBACKS_V10_H
#define DRIVER_CLDROPINCALLBACKS_V10_H

#include "compiler/common.h"
#include "driver/cmdline.h"
#include "driver/CWPlugins.h"
#include "driver/Files.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 2)
#if MWCCPSP
// 3.0.1: 14 bytes; the handles a plugin got during one request are kept in
// a 0x101-bucket table (CLPluginRequests.cpp, 0x41AA50..0x41AC20)
typedef struct ShellHandleEntry {
    struct ShellHandleEntry* next;
    void* ptr;
    Boolean isHandle; // +8: an OSHandle (else a plain allocation)
    Boolean keep; // +9: not freed after the request
    Boolean hasMacHandle; // +A: a Mac handle wraps it
    Boolean keep2; // +B: not freed after the request
} ShellHandleEntry;

typedef struct shellContextType {
    struct Plugin* plugin;
    SInt32 pluginAPI; // +4: CWPLUGIN_API (file spec format), 2 at init
    Boolean userAccessPathsChanged;
    Boolean systemAccessPathsChanged;
    ShellHandleEntry** handles;
} shellContextType;

// the shell's view of CWFileInfo: the found file is an OSSpec (+0x14)
typedef struct ShellFileInfo {
    Boolean fullsearch;
    char dependencyType;
    SInt32 isdependentoffile;
    Boolean suppressload;
    Boolean padding;
    const char* filedata;
    SInt32 filedatalength;
    short filedatatype;
    short fileID;
    OSSpec filespec;
    Boolean alreadyincluded; // +0x218
    Boolean recordbrowseinfo; // +0x219
} ShellFileInfo;

// CLPluginRequests.cpp
#ifdef __cplusplus
extern "C" {
#endif
extern ShellHandleEntry* ShellHandles_Add(
    CWPluginContext context,
    void* ptr,
    Boolean keep2,
    Boolean isHandle,
    Boolean keep,
    Boolean hasMacHandle
);
extern ShellHandleEntry* ShellHandles_Find(CWPluginContext context, void* ptr, Boolean isHandle);
extern void ShellHandles_Remove(CWPluginContext context, void* ptr, Boolean isHandle);
extern void ShellHandles_Init(CWPluginContext context);
extern void ShellHandles_Free(CWPluginContext context);
#ifdef __cplusplus
}
#endif
#else
typedef struct shellContextType {
    struct Plugin* plugin;
    Boolean userAccessPathsChanged;
    Boolean systemAccessPathsChanged;
} shellContextType;
#endif
#pragma pack(pop)

#pragma pack(push, 2)
struct IDEAccessPathList {
    SInt32 userPathCount;
    struct IDEAccessPath* userPaths;
    SInt32 systemPathCount;
    struct IDEAccessPath* systemPaths;
    Boolean alwaysSearchUserPaths;
    Boolean convertPaths;
};
struct IDEAccessPath {
    CWFileSpec pathSpec;
    Boolean recursive;
    SInt32 subdirectoryCount;
    CWFileSpec* subdirectories;
};
#pragma pack(pop)

extern CW_CALLBACK UCBFindAndLoadFile(
    CWPluginContext context, const char* infilename, CWFileInfo* fileinfo
);
extern CW_CALLBACK UCBGetFileText(
    CWPluginContext context,
    const CWFileSpec* filespec,
    const char** text,
    SInt32* textLength,
    short* filedatatype
);
extern CW_CALLBACK UCBReleaseFileText(CWPluginContext context, const char* text);
extern CW_CALLBACK UCBSecretAttachHandle(
    CWPluginContext context, Handle handle, CWMemHandle* memHandle
);
extern CW_CALLBACK UCBSecretDetachHandle(
    CWPluginContext context, CWMemHandle memHandle, Handle* handle
);

#ifdef __cplusplus
}
#endif

#endif
