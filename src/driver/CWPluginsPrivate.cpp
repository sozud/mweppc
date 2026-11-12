#include "driver/CWPluginsPrivate.h"
#include "driver/CLDropinCallbacks_V10.h"
#include "driver/CWPlugins.h"
#include "driver/Files.h"
#if MWCCPSP
#include <string.h>
#endif

extern "C" {
static Boolean ValidateContext(CWPluginContext context) {
    return (context && context->shellSignature == 'CWIE') &&
        !(context->request == reqInitialize ||
          context->request == reqTerminate ||
          context->request == reqIdle);
}

static Boolean ValidateInitTermContext(CWPluginContext context) {
    return context &&
        (context->shellSignature == 'CWIE') &&
        ((context->request == reqInitialize) ||
         (context->request == reqTerminate) ||
         (context->request == reqIdle));
}

static Boolean IsVCSContext(CWPluginContext context) {
    return context && (context->pluginType == CWDROPINVCSTYPE);
}

CW_CALLBACK CWGetPluginRequest(CWPluginContext context, SInt32* request) {
    if (!ValidateContext(context) && !ValidateInitTermContext(context)) {
        return cwErrInvalidParameter;
    }
    if (request == NULL) {
        return cwErrInvalidParameter;
    }

    *request = context->request;
    return cwNoErr;
}

CW_CALLBACK CWGetAPIVersion(CWPluginContext context, SInt32* version) {
    if (!ValidateContext(context) && !ValidateInitTermContext(context)) {
        return cwErrInvalidParameter;
    }
    if (version == NULL) {
        return cwErrInvalidParameter;
    }

    *version = context->apiVersion;
    return cwNoErr;
}
#if MWCCPSP

#endif
#if MWCCPSP
// 0x415710: converts a shell spec for the plugin's API version
extern "C" void func_00415710(CWPluginContext context, const OSSpec* spec, CWFileSpec* out);
#endif

CW_CALLBACK CWGetProjectFile(CWPluginContext context, CWFileSpec* projectSpec) {
    if (IsVCSContext(context) || !ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    if (projectSpec == NULL) {
        return cwErrInvalidParameter;
    }

#if MWCCPSP
    func_00415710(context, &context->projectFile, projectSpec);
#else
    *projectSpec = context->projectFile;
#endif
    return cwNoErr;
}

CW_CALLBACK CWGetTargetName(CWPluginContext context, char* name, short maxLength) {
    if (IsVCSContext(context) || !ValidateContext(context)) {
        return cwErrInvalidParameter;
    }

    if (context->apiVersion >= 8) {
        return context->callbacks->cbGetTargetName(context, name, maxLength);
    } else {
        return cwErrRequestFailed;
    }
}

CW_CALLBACK CWGetProjectFileCount(CWPluginContext context, SInt32* count) {
    if (IsVCSContext(context) || !ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    if (count == NULL) {
        return cwErrInvalidParameter;
    }

    *count = context->numFiles;
    return cwNoErr;
}

CW_CALLBACK CWGetFileInfo(
    CWPluginContext context,
    SInt32 whichfile,
    Boolean checkFileLocation,
    CWProjectFileInfo* fileinfo
) {
    if (IsVCSContext(context) || !ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    if (fileinfo == NULL) {
        return cwErrInvalidParameter;
    }

    return context->callbacks->cbGetFileInfo(context, whichfile, checkFileLocation, fileinfo);
}

#if MWCCPSP
// a CWPLUGIN_API 2 plugin's file info: the file is a Windows path
#pragma pack(push, 2)
typedef struct PluginFileInfo2 {
    Boolean fullsearch;
    char dependencyType;
    SInt32 isdependentoffile;
    Boolean suppressload;
    Boolean padding;
    const char* filedata;
    SInt32 filedatalength;
    short filedatatype;
    short fileID;
    char filespec[260]; // +0x14
    Boolean alreadyincluded; // +0x118
    Boolean recordbrowseinfo; // +0x119
} PluginFileInfo2;

// a CWPLUGIN_API 2 plugin's message ref
typedef struct PluginMessageRef2 {
    char sourcefile[260];
    SInt32 linenumber; // +0x104
    short tokenoffset;
    short tokenlength;
    SInt32 selectionoffset;
    SInt32 selectionlength;
} PluginMessageRef2;
#pragma pack(pop)

extern "C" {
extern SInt32 func_00424690(CWPluginContext context);
extern int func_00415730(CWPluginContext context, const void* pluginspec, OSSpec* spec);
// 0x4164A0: the shell's file info for a CWPLUGIN_API 1 plugin
extern void func_004164A0(CWPluginContext context, const ShellFileInfo* in, CWFileInfo* out);
}

// 0x416440: the shell's file info for a CWPLUGIN_API 2 plugin
extern "C" void func_00416440(
    CWPluginContext context, const ShellFileInfo* in, PluginFileInfo2* out
) {
    out->filedata = in->filedata;
    out->filedatalength = in->filedatalength;
    out->filedatatype = in->filedatatype;
    out->fileID = in->fileID;
    func_00415710(context, &in->filespec, reinterpret_cast<CWFileSpec*>(out->filespec));
    out->alreadyincluded = in->alreadyincluded;
    out->recordbrowseinfo = in->recordbrowseinfo;
}

// 0x415AB0: the shell looks the file up with its own file info
CW_CALLBACK CWFindAndLoadFile(CWPluginContext context, const char* filename, CWFileInfo* fileinfo) {
    ShellFileInfo info;
    CWResult result;

    if (IsVCSContext(context) || !ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    if (filename == NULL) {
        return cwErrInvalidParameter;
    }
    if (fileinfo == NULL) {
        return cwErrInvalidParameter;
    }

    memset(&info, 0, sizeof(info));
    info.fullsearch = fileinfo->fullsearch;
    info.dependencyType = fileinfo->dependencyType;
    info.isdependentoffile = fileinfo->isdependentoffile;
    info.suppressload = fileinfo->suppressload;
    result = context->callbacks->cbFindAndLoadFile(
        context, filename, reinterpret_cast<CWFileInfo*>(&info)
    );
    if (!result) {
        if (func_00424690(context) == 1) {
            func_004164A0(context, &info, fileinfo);
        } else {
            func_00416440(context, &info, reinterpret_cast<PluginFileInfo2*>(fileinfo));
        }
    }
    return result;
}
#else
CW_CALLBACK CWFindAndLoadFile(CWPluginContext context, const char* filename, CWFileInfo* fileinfo) {
    if (IsVCSContext(context) || !ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    if (filename == NULL) {
        return cwErrInvalidParameter;
    }
    if (fileinfo == NULL) {
        return cwErrInvalidParameter;
    }

    return context->callbacks->cbFindAndLoadFile(context, filename, fileinfo);
}
#endif

static CWResult EnsureCachedAccessPaths(CWPluginContext context) {
    if (!context->accessPathList) {
        CWResult res = context->callbacks->cbCacheAccessPathList(context);
        if (res) {
            return res;
        }
        if (!context->accessPathList) {
            return cwErrRequestFailed;
        }
    }
    return cwNoErr;
}

CW_CALLBACK CWGetAccessPathListInfo(CWPluginContext context, CWAccessPathListInfo* pathListInfo) {
    if (!ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    if (pathListInfo == NULL) {
        return cwErrInvalidParameter;
    }
    if (context->apiVersion < 10) {
        return cwErrInvalidCallback;
    }
    CWResult res = EnsureCachedAccessPaths(context);
    if (res) {
        return res;
    }
    pathListInfo->systemPathCount = context->accessPathList->systemPathCount;
    pathListInfo->userPathCount = context->accessPathList->userPathCount;
    pathListInfo->alwaysSearchUserPaths = context->accessPathList->alwaysSearchUserPaths;
    pathListInfo->convertPaths = context->accessPathList->convertPaths;
    return cwNoErr;
}

CW_CALLBACK CWGetFileText(
    CWPluginContext context,
    const CWFileSpec* filespec,
    const char** text,
    SInt32* textLength,
    short* filedatatype
) {
    if (!ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    if (!filespec) {
        return cwErrInvalidParameter;
    }
    if (!text) {
        return cwErrInvalidParameter;
    }
    if (!textLength) {
        return cwErrInvalidParameter;
    }
    if (!filedatatype) {
        return cwErrInvalidParameter;
    }
    return context->callbacks->cbGetFileText(context, filespec, text, textLength, filedatatype);
}

CW_CALLBACK CWReleaseFileText(CWPluginContext context, const char* text) {
    if (!ValidateContext(context)) {
        return cwErrInvalidParameter;
    }

    if (!text) {
        return cwNoErr;
    }
    return context->callbacks->cbReleaseFileText(context, text);
}
#if MWCCPSP

#endif
#if MWCCPSP
typedef struct ShellMessageRef {
    OSSpec sourcefile;
    SInt32 linenumber;
    short tokenoffset;
    short tokenlength;
    SInt32 selectionoffset;
    SInt32 selectionlength;
} ShellMessageRef;

// 0x4163E0: converts a plugin message ref for the shell
extern "C" void func_004163E0(
    CWPluginContext context, ShellMessageRef* out, const CWMessageRef* in
);
#endif

CW_CALLBACK CWReportMessage(
    CWPluginContext context,
    const CWMessageRef* msgRef,
    const char* line1,
    const char* line2,
    short errorlevel,
    SInt32 errorNumber
) {
#if MWCCPSP
    // 0x415DC0: the shell's callbacks take a message ref with its own spec
    ShellMessageRef ref;

    if (!ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    if (msgRef) {
        func_004163E0(context, &ref, msgRef);
    }
    return context->callbacks->cbReportMessage(
        context, msgRef ? (const CWMessageRef*)&ref : NULL, line1, line2, errorlevel, errorNumber
    );
#else
    if (!ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    return context->callbacks->cbReportMessage(
        context, msgRef, line1, line2, errorlevel, errorNumber
    );
#endif
}

CW_CALLBACK CWAlert(
    CWPluginContext context, const char* msg1, const char* msg2, const char* msg3, const char* msg4
) {
    if (!ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    return context->callbacks->cbAlert(context, msg1, msg2, msg3, msg4);
}

CW_CALLBACK CWShowStatus(CWPluginContext context, const char* line1, const char* line2) {
    if (!ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    return context->callbacks->cbShowStatus(context, line1, line2);
}

CW_CALLBACK CWUserBreak(CWPluginContext context) {
    if (!ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    return context->callbacks->cbUserBreak(context);
}

CW_CALLBACK CWGetNamedPreferences(
    CWPluginContext context, const char* prefsname, CWMemHandle* prefsdata
) {
    if (!ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    if (!prefsname) {
        return cwErrInvalidParameter;
    }
    if (!prefsdata) {
        return cwErrInvalidParameter;
    }
    return context->callbacks->cbGetNamedPreferences(context, prefsname, prefsdata);
}

CW_CALLBACK CWSetModDate(
    CWPluginContext context, const CWFileSpec* filespec, CWFileTime* moddate, Boolean isGenerated
) {
    if (IsVCSContext(context) || !ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    if (!filespec) {
        return cwErrInvalidParameter;
    }
    return context->callbacks->cbSetModDate(context, filespec, moddate, isGenerated);
}

#if MWCCPSP
extern "C" int func_00415730(CWPluginContext context, const void* pluginspec, OSSpec* spec);
#endif
CW_CALLBACK CWAddProjectEntry(
    CWPluginContext context,
    const CWFileSpec* fileSpec,
    Boolean isGenerated,
    const CWNewProjectEntryInfo* projectEntryInfo,
    SInt32* whichfile
) {
    if (IsVCSContext(context) || !ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    if (!fileSpec) {
        return cwErrInvalidParameter;
    }
    if (context->apiVersion < 8) {
        return cwErrRequestFailed;
    }
#if MWCCPSP
    // 0x416010: the shell takes an OSSpec
    {
        OSSpec spec;
        func_00415730(context, fileSpec, &spec);
        return context->callbacks->cbAddProjectEntry(
            context,
            reinterpret_cast<const CWFileSpec*>(&spec),
            isGenerated,
            projectEntryInfo,
            whichfile
        );
    }
#else
    return context->callbacks->cbAddProjectEntry(
        context, fileSpec, isGenerated, projectEntryInfo, whichfile
    );
#endif
}

CW_CALLBACK CWCreateNewTextDocument(CWPluginContext context, const CWNewTextDocumentInfo* docinfo) {
    if (!ValidateContext(context)) {
        return cwErrInvalidParameter;
    }
    if (!docinfo) {
        return cwErrInvalidParameter;
    }
    return context->callbacks->cbCreateNewTextDocument(context, docinfo);
}

CW_CALLBACK CWAllocMemHandle(
    CWPluginContext context, SInt32 size, Boolean useTempMemory, CWMemHandle* handle
) {
    if (!ValidateContext(context) && !ValidateInitTermContext(context)) {
        return cwErrInvalidParameter;
    }
    if (!handle) {
        return cwErrInvalidParameter;
    }
    return context->callbacks->cbAllocMemHandle(context, size, useTempMemory, handle);
}

CW_CALLBACK CWFreeMemHandle(CWPluginContext context, CWMemHandle handle) {
    if (!ValidateContext(context) && !ValidateInitTermContext(context)) {
        return cwErrInvalidParameter;
    }
    return context->callbacks->cbFreeMemHandle(context, handle);
}

CW_CALLBACK CWLockMemHandle(
    CWPluginContext context, CWMemHandle handle, Boolean moveHi, void** ptr
) {
    if (!ValidateContext(context) && !ValidateInitTermContext(context)) {
        return cwErrInvalidParameter;
    }
    if (!ptr) {
        return cwErrInvalidParameter;
    }
    return context->callbacks->cbLockMemHandle(context, handle, moveHi, ptr);
}

CW_CALLBACK CWUnlockMemHandle(CWPluginContext context, CWMemHandle handle) {
    if (!ValidateContext(context) && !ValidateInitTermContext(context)) {
        return cwErrInvalidParameter;
    }
    return context->callbacks->cbUnlockMemHandle(context, handle);
}

CW_CALLBACK CWDonePluginRequest(CWPluginContext, CWResult resultCode) {
    return resultCode;
}

#if MWCCPSP
// 0x415890
CW_CALLBACK CWSetPluginOSError(CWPluginContext context, SInt32 err) {
    if (!ValidateContext(context) && !ValidateInitTermContext(context)) {
        return cwErrInvalidParameter;
    }
    if (func_00424690(context) == 1) {
        context->pluginOSError = (SInt16)err;
    } else {
        context->pluginOSError = err;
    }
    return cwNoErr;
}

// 0x416210
CW_CALLBACK CWResizeMemHandle(CWPluginContext context, CWMemHandle handle, SInt32 newSize) {
    if (!ValidateContext(context) && !ValidateInitTermContext(context)) {
        return cwErrInvalidParameter;
    }
    return context->callbacks->cbResizeMemHandle(context, handle, newSize);
}

// 0x416390: a plugin tells which CWPLUGIN_API its file specs follow
CW_CALLBACK CWSetPluginAPI(CWPluginContext context, SInt32 api) {
    if (!ValidateContext(context) && !ValidateInitTermContext(context)) {
        return cwErrInvalidParameter;
    }
#if MWCCPS2
    shellContextType* sc = static_cast<shellContextType*>(context->shellContext);
    if (api == 1) {
        goto set;
    }
    if (api == 2) {
        goto set;
    }
    return cwErrInvalidParameter;
set:
    sc->pluginAPI = api;
    return cwNoErr;
#else
    if (api == 1 || api == 2) {
        static_cast<shellContextType*>(context->shellContext)->pluginAPI = api;
        return cwNoErr;
    }
    return cwErrInvalidParameter;
#endif
}

// 0x4163E0
extern "C" void func_004163E0(
    CWPluginContext context, ShellMessageRef* out, const CWMessageRef* cwin
) {
    const PluginMessageRef2* in = reinterpret_cast<const PluginMessageRef2*>(cwin);
    func_00415730(context, in->sourcefile, &out->sourcefile);
    out->linenumber = in->linenumber;
    out->tokenoffset = in->tokenoffset;
    out->tokenlength = in->tokenlength;
    out->selectionoffset = in->selectionoffset;
    out->selectionlength = in->selectionlength;
}
#endif
#if MWCCPSP

#endif
CW_CALLBACK CWCheckoutLicense(
    CWPluginContext context,
    const char* featureName,
    const char* licenseVersion,
    SInt32 flags,
    void* reserved,
    SInt32* cookie
) {
    if (!ValidateContext(context) && !ValidateInitTermContext(context)) {
        return cwErrInvalidParameter;
    }
    return context->callbacks->cbCheckoutLicense(
        context, featureName, licenseVersion, flags, reserved, cookie
    );
}

CW_CALLBACK CWCheckinLicense(CWPluginContext context, SInt32 cookie) {
    if (!ValidateContext(context) && !ValidateInitTermContext(context)) {
        return cwErrInvalidParameter;
    }
    return context->callbacks->cbCheckinLicense(context, cookie);
}
}
