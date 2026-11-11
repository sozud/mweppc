#ifndef DRIVER_CWPLUGINSPRIVATE_H
#define DRIVER_CWPLUGINSPRIVATE_H

#include "compiler/common.h"
#include "driver/cmdline.h"
#include "driver/CWPlugins.h"
#include "driver/DropInCompilerLinker.h"
#include "driver/Files.h"
#include "driver/oslib.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 2)
#if MWCCPSP
// 3.0.1: the plugin's target info with shell specs (0xE22 bytes; the
// CWCompilerLinkerContext constructor, 0x41A8A0/0x41A9C0)
typedef struct PluginTargetInfo {
    short outputType;
    OSSpec outfile; // +2
    OSSpec symfile; // +0x206
    OSSpec runfile; // +0x40A
    SInt16 linkType; // +0x60E
    UInt8 canRun;
    UInt8 canDebug;
    UInt32 targetCPU; // +0x612
    UInt32 targetOS; // +0x616
    Boolean runHelperIsRegKey; // +0x61A
    Boolean debugHelperIsRegKey; // +0x61B
    char helpers[0x602]; // args/runHelperName/debugHelperName (not passed on)
    OSSpec linkAgainstFile; // +0xC1E
} PluginTargetInfo;

// The CWTargetInfo a plugin sees: specs in its API's format (API 1 FSSpec,
// API 2 260-byte path), filled by CWGetTargetInfo.
#define PLUGIN_TARGET_INFO(name, specsize) \
    typedef struct name { \
        short outputType; \
        char outfile[specsize]; \
        char symfile[specsize]; \
        char runfile[specsize]; \
        SInt16 linkType; \
        UInt8 canRun; \
        UInt8 canDebug; \
        UInt32 targetCPU; \
        UInt32 targetOS; \
        Boolean runHelperIsRegKey; \
        Boolean debugHelperIsRegKey; \
        char helpers[0x602]; \
        char linkAgainstFile[specsize]; \
    } name;
PLUGIN_TARGET_INFO(PluginTargetInfo1, 0x46)
PLUGIN_TARGET_INFO(PluginTargetInfo2, 0x104)
#undef PLUGIN_TARGET_INFO
#endif
#if MWCCPSP

#endif
struct CWPluginPrivateContext {
#ifdef __cplusplus
    CWPluginPrivateContext(SInt32 thePluginType, SInt32 totalSize = 0);
    ~CWPluginPrivateContext();
#endif

    SInt32 request;
    SInt32 apiVersion;
    void* shellContext;
    void* pluginStorage;
#if MWCCPSP
    // 3.0.1 (CWPluginsPrivate.cpp): the shell keeps its own path specs and
    // 32-bit OS errors; shellSignature is at +0x418, callbacks at +0x484
    OSSpec projectFile;
    OSSpec outputFileDirectory;
#else
    CWFileSpec projectFile;
    CWFileSpec outputFileDirectory;
#endif
    SInt32 shellSignature;
    SInt32 pluginType;
    SInt32 numFiles;
    SInt32 numOverlayGroups;
#if MWCCPSP
    SInt32 callbackOSError;
    SInt32 pluginOSError;
#else
    OSErr callbackOSError;
    OSErr pluginOSError;
#endif
    CWIDEInfo* shellInfo;
    struct IDEAccessPathList* accessPathList;
    SInt32 dontEatEvents;
    CWFileSpec* targetDataDirectorySpec;
    SInt32 reserved[17];
    struct CW_BasePluginCallbacks* callbacks;
};

#ifdef __cplusplus
struct CWCompilerLinkerContext : CWPluginPrivateContext {
    CWCompilerLinkerContext();
    ~CWCompilerLinkerContext();

    SInt32 whichfile;
#if MWCCPSP
    OSSpec sourcefile; // +0x48C (CWCompilerLinkerContext::CWCompilerLinkerContext 0x42CB..)
#else
    CWFileSpec sourcefile;
#endif
    const char* sourcetext;
    SInt32 sourcetextsize;
    Boolean precompile;
    Boolean autoprecompile;
    Boolean preprocess;
    Boolean cachingPCH;
    Boolean debuginfo;
    SInt16 fileID;
    CWBrowseOptions browseoptions;
    Boolean recordbrowseinfo;
    void* reserved;
    SInt32 sequenceID;
    CWCompilerLinkerContext* parentPB;
    void* targetStorage;
    SInt32 reservedcompiler[7];
    CWMemHandle texthandle;
#if MWCCPSP
    UInt8 targetinfo_V7[0x720]; // +0x6E2; its 3.0.1 layout is not known yet
#else
    CWTargetInfoV7 targetinfo_V7;
#endif
#if MWCCPSP
    struct PluginTargetInfo* targetinfo; // 0xE22 bytes
#else
    CWTargetInfo* targetinfo;
#endif
    const char* commandLineArgs;
    CWFileSpec* workingDirectorySpec;
    SInt32 numEnvironmentVariables;
    CWEnvVarInfo* environmentVariables;
    SInt32 workingDirectoryError;
    SInt32 reservedlinker[2];
    CWCompilerLinkerCallbacks* callbacks;
};

struct CWParserContext : CWPluginPrivateContext {
    CWParserContext();
    ~CWParserContext();

    CWCommandLineArgs* args;
    CWDataType cpu;
    CWDataType os;
    const char* build_date;
    const char* build_time;
    const struct ToolVersionInfo* build_tool;
    int numPlugins;
    const CLPluginInfo* plugins;
    int numPanels;
    const char** panelNames;
    CWCommandLineArgs* plugin_args;
    CWCommandLineArgs* panel_args;
    struct CWParserCallbacks* callbacks;
};
#endif
#pragma pack(pop)

typedef struct CW_BasePluginCallbacks {
    CW_CALLBACK_PTR (*cbGetFileInfo)(CWPluginContext, SInt32, Boolean, CWProjectFileInfo*);
    CW_CALLBACK_PTR (*cbFindAndLoadFile)(CWPluginContext, const char*, CWFileInfo*);
    CW_CALLBACK_PTR (*cbGetFileText)(
        CWPluginContext, const CWFileSpec*, const char**, SInt32*, short*
    );
    CW_CALLBACK_PTR (*cbReleaseFileText)(CWPluginContext, const char*);
    CW_CALLBACK_PTR (*cbGetSegmentInfo)(CWPluginContext, SInt32, CWProjectSegmentInfo*);
    CW_CALLBACK_PTR (*cbGetOverlay1GroupInfo)(CWPluginContext, SInt32, CWOverlay1GroupInfo*);
    CW_CALLBACK_PTR (*cbGetOverlay1Info)(CWPluginContext, SInt32, SInt32, CWOverlay1Info*);
    CW_CALLBACK_PTR (*cbGetOverlay1FileInfo)(
        CWPluginContext, SInt32, SInt32, SInt32, CWOverlay1FileInfo*
    );
    CW_CALLBACK_PTR (*cbReportMessage)(
        CWPluginContext, const CWMessageRef*, const char*, const char*, short, SInt32
    );
    CW_CALLBACK_PTR (*cbAlert)(CWPluginContext, const char*, const char*, const char*, const char*);
    CW_CALLBACK_PTR (*cbShowStatus)(CWPluginContext, const char*, const char*);
    CW_CALLBACK_PTR (*cbUserBreak)(CWPluginContext);
    CW_CALLBACK_PTR (*cbGetNamedPreferences)(CWPluginContext, const char*, CWMemHandle*);
    CW_CALLBACK_PTR (*cbStorePluginData)(CWPluginContext, SInt32, CWDataType, CWMemHandle);
    CW_CALLBACK_PTR (*cbGetPluginData)(CWPluginContext, SInt32, CWDataType, CWMemHandle*);
    CW_CALLBACK_PTR (*cbSetModDate)(CWPluginContext, const CWFileSpec*, CWFileTime*, Boolean);
    CW_CALLBACK_PTR (*cbAddProjectEntry)(
        CWPluginContext, const CWFileSpec*, Boolean, const CWNewProjectEntryInfo*, SInt32*
    );
    CW_CALLBACK_PTR (*cbCreateNewTextDocument)(CWPluginContext, const CWNewTextDocumentInfo*);
    CW_CALLBACK_PTR (*cbAllocateMemory)(CWPluginContext, SInt32, Boolean, void**);
    CW_CALLBACK_PTR (*cbFreeMemory)(CWPluginContext, void*, Boolean);
    CW_CALLBACK_PTR (*cbAllocMemHandle)(CWPluginContext, SInt32, Boolean, CWMemHandle*);
    CW_CALLBACK_PTR (*cbFreeMemHandle)(CWPluginContext, CWMemHandle);
    CW_CALLBACK_PTR (*cbGetMemHandleSize)(CWPluginContext, CWMemHandle, SInt32*);
    CW_CALLBACK_PTR (*cbResizeMemHandle)(CWPluginContext, CWMemHandle, SInt32);
    CW_CALLBACK_PTR (*cbLockMemHandle)(CWPluginContext, CWMemHandle, Boolean, void**);
    CW_CALLBACK_PTR (*cbUnlockMemHandle)(CWPluginContext, CWMemHandle);
    void* cbInternal[8];
    CW_CALLBACK_PTR (*cbGetTargetName)(CWPluginContext, char*, short);
    CW_CALLBACK_PTR (*cbCacheAccessPathList)(CWPluginContext);
    CW_CALLBACK_PTR (*cbPreDialog)(CWPluginContext);
    CW_CALLBACK_PTR (*cbPostDialog)(CWPluginContext);
    CW_CALLBACK_PTR (*cbPreFileAction)(CWPluginContext, const CWFileSpec*);
    CW_CALLBACK_PTR (*cbPostFileAction)(CWPluginContext, const CWFileSpec*);
    CW_CALLBACK_PTR (*cbCheckoutLicense)(
        CWPluginContext, const char*, const char*, UInt32, void*, SInt32*
    );
    CW_CALLBACK_PTR (*cbCheckinLicense)(CWPluginContext, SInt32);
    CW_CALLBACK_PTR (*cbResolveRelativePath)(
        CWPluginContext, const CWRelativePath*, CWFileSpec*, Boolean
    );
} CW_BasePluginCallbacks;

extern CW_CALLBACK CWGetPluginRequest(CWPluginContext context, SInt32* request);
extern CW_CALLBACK CWGetAPIVersion(CWPluginContext context, SInt32* version);
extern CW_CALLBACK CWGetProjectFile(CWPluginContext context, CWFileSpec* projectSpec);
extern CW_CALLBACK CWGetOutputFileDirectory(
    CWPluginContext context, CWFileSpec* outputFileDirectory
);
extern CWResult OSErrtoCWResult(OSErr err);
extern CW_CALLBACK CWGetTargetName(CWPluginContext context, char* name, short maxLength);
extern CW_CALLBACK CWGetProjectFileCount(CWPluginContext context, SInt32* count);
extern CW_CALLBACK CWGetFileInfo(
    CWPluginContext context,
    SInt32 whichfile,
    Boolean checkFileLocation,
    CWProjectFileInfo* fileinfo
);
extern CW_CALLBACK CWFindAndLoadFile(
    CWPluginContext context, const char* filename, CWFileInfo* fileinfo
);
extern CW_CALLBACK CWGetAccessPathListInfo(
    CWPluginContext context, CWAccessPathListInfo* pathListInfo
);
extern CW_CALLBACK CWGetFileText(
    CWPluginContext context,
    const CWFileSpec* filespec,
    const char** text,
    SInt32* textLength,
    short* filedatatype
);
extern CW_CALLBACK CWReleaseFileText(CWPluginContext context, const char* text);
extern CW_CALLBACK CWReportMessage(
    CWPluginContext context,
    const CWMessageRef* msgRef,
    const char* line1,
    const char* line2,
    short errorlevel,
    SInt32 errorNumber
);
extern CW_CALLBACK CWAlert(
    CWPluginContext context, const char* msg1, const char* msg2, const char* msg3, const char* msg4
);
extern CW_CALLBACK CWShowStatus(CWPluginContext context, const char* line1, const char* line2);
extern CW_CALLBACK CWUserBreak(CWPluginContext context);
extern CW_CALLBACK CWGetNamedPreferences(
    CWPluginContext context, const char* prefsname, CWMemHandle* prefsdata
);
extern CW_CALLBACK CWSetModDate(
    CWPluginContext context, const CWFileSpec* filespec, CWFileTime* moddate, Boolean isGenerated
);
extern CW_CALLBACK CWAddProjectEntry(
    CWPluginContext context,
    const CWFileSpec* fileSpec,
    Boolean isGenerated,
    const CWNewProjectEntryInfo* projectEntryInfo,
    SInt32* whichfile
);
extern CW_CALLBACK CWCreateNewTextDocument(
    CWPluginContext context, const CWNewTextDocumentInfo* docinfo
);
extern CW_CALLBACK CWAllocMemHandle(
    CWPluginContext context, SInt32 size, Boolean useTempMemory, CWMemHandle* handle
);
extern CW_CALLBACK CWFreeMemHandle(CWPluginContext context, CWMemHandle handle);
#if MWCCPSP
extern CW_CALLBACK CWAllocateMemory(
    CWPluginContext context, SInt32 size, Boolean isPermanent, void** ptr
);
extern CW_CALLBACK CWResizeMemHandle(CWPluginContext context, CWMemHandle handle, SInt32 newSize);
#endif
extern CW_CALLBACK CWLockMemHandle(
    CWPluginContext context, CWMemHandle handle, Boolean moveHi, void** ptr
);
extern CW_CALLBACK CWUnlockMemHandle(CWPluginContext context, CWMemHandle handle);
extern CW_CALLBACK CWDonePluginRequest(CWPluginContext context, CWResult resultCode);
extern CW_CALLBACK CWCheckoutLicense(
    CWPluginContext context,
    const char* featureName,
    const char* licenseVersion,
    SInt32 flags,
    void* reserved,
    SInt32* cookie
);
extern CW_CALLBACK CWCheckinLicense(CWPluginContext context, SInt32 cookie);

#ifdef __cplusplus
}
#endif

#endif
