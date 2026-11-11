#include "driver/CWPlugins.h"
#include "driver/CWPluginsPrivate.h"
#include "driver/DropInCompilerLinker.h"
#include "driver/Files.h"
#if MWCCPSP
#include <stdlib.h>
#endif
#include <string.h>

#define GetContext DropInCompilerLinkerPrivate_GetContext

extern "C" {

static CWCompilerLinkerContext* GetContext(CWPluginContext context) {
    if (context &&
        (context->pluginType == CWDROPINCOMPILERTYPE ||
         context->pluginType == CWDROPINLINKERTYPE)) {
        return static_cast<CWCompilerLinkerContext*>(context);
    } else {
        return 0;
    }
}

#if MWCCPSP
SInt32 func_00424690(CWPluginContext context);
int func_00415710(CWPluginContext context, const OSSpec* spec, void* pluginspec);
int func_00415730(CWPluginContext context, const void* pluginspec, OSSpec* spec);
// CWObjectData.dependencies entries: the plugin's (API 2 path) and the
// shell's (OSSpec)
typedef struct PluginDependencyInfo {
    SInt32 fileIndex;
    char fileSpec[0x104];
    short dependencyType;
    short dependencyTag;
} PluginDependencyInfo;
typedef struct ShellDependencyInfo {
    SInt32 fileIndex;
    OSSpec fileSpec;
    short dependencyType;
    short dependencyTag;
} ShellDependencyInfo;
static void TargetInfoToPlugin1(CWCompilerLinkerContext* cl, PluginTargetInfo1* ti);
static void TargetInfoToPlugin2(CWCompilerLinkerContext* cl, PluginTargetInfo2* ti);
#endif
#if MWCCPSP

#endif
CW_CALLBACK CWIsPrecompiling(CWPluginContext context, Boolean* isPrecompiling) {
    CWCompilerLinkerContext* cl;
    if (!isPrecompiling) {
        return cwErrInvalidParameter;
    }
#if MWCCPSP
    cl = GetContext(context);
    if (!cl) {
#else
    if (!(cl = GetContext(context))) {
#endif
        return cwErrInvalidCallback;
    }
    *isPrecompiling = cl->precompile;
    return cwNoErr;
}

CW_CALLBACK CWIsAutoPrecompiling(CWPluginContext context, Boolean* isAutoPrecompiling) {
    CWCompilerLinkerContext* cl;
    if (!isAutoPrecompiling) {
        return cwErrInvalidParameter;
    }
#if MWCCPSP
    cl = GetContext(context);
    if (!cl) {
#else
    if (!(cl = GetContext(context))) {
#endif
        return cwErrInvalidCallback;
    }
    *isAutoPrecompiling = cl->autoprecompile;
    return cwNoErr;
}

CW_CALLBACK CWIsPreprocessing(CWPluginContext context, Boolean* isPreprocessing) {
    CWCompilerLinkerContext* cl;
    if (!isPreprocessing) {
        return cwErrInvalidParameter;
    }
#if MWCCPSP
    cl = GetContext(context);
    if (!cl) {
#else
    if (!(cl = GetContext(context))) {
#endif
        return cwErrInvalidCallback;
    }
    *isPreprocessing = cl->preprocess;
    return cwNoErr;
}

CW_CALLBACK CWIsGeneratingDebugInfo(CWPluginContext context, Boolean* isGenerating) {
    CWCompilerLinkerContext* cl;
    if (!isGenerating) {
        return cwErrInvalidParameter;
    }
#if MWCCPSP
    cl = GetContext(context);
    if (!cl) {
#else
    if (!(cl = GetContext(context))) {
#endif
        return cwErrInvalidCallback;
    }
    *isGenerating = cl->debuginfo;
    return cwNoErr;
}

CW_CALLBACK CWIsCachingPrecompiledHeaders(CWPluginContext context, Boolean* isCaching) {
    CWCompilerLinkerContext* cl;
    if (!isCaching) {
        return cwErrInvalidParameter;
    }
#if MWCCPSP
    cl = GetContext(context);
    if (!cl) {
#else
    if (!(cl = GetContext(context))) {
#endif
        return cwErrInvalidCallback;
    }
    *isCaching = cl->cachingPCH;
    return cwNoErr;
}

CW_CALLBACK CWGetBrowseOptions(CWPluginContext context, CWBrowseOptions* browseOptions) {
    CWCompilerLinkerContext* cl;
    if (!browseOptions) {
        return cwErrInvalidParameter;
    }
#if MWCCPSP
    cl = GetContext(context);
    if (!cl) {
#else
    if (!(cl = GetContext(context))) {
#endif
        return cwErrInvalidCallback;
    }
    *browseOptions = cl->browseoptions;
    return cwNoErr;
}

CW_CALLBACK CWGetTargetInfo(CWPluginContext context, CWTargetInfo* targetInfo) {
    CWCompilerLinkerContext* cl;
    if (!targetInfo) {
        return cwErrInvalidParameter;
    }
#if MWCCPSP
    cl = GetContext(context);
    if (!cl) {
#else
    if (!(cl = GetContext(context))) {
#endif
        return cwErrInvalidCallback;
    }
#if MWCCPSP
    // 3.0.1 converts the shell's target info into the plugin's spec format
    if (func_00424690(cl) == 1) {
        TargetInfoToPlugin1(cl, (PluginTargetInfo1*)targetInfo);
    } else {
        TargetInfoToPlugin2(cl, (PluginTargetInfo2*)targetInfo);
    }
#else
    if (cl->apiVersion >= 10) {
        *targetInfo = *cl->targetinfo;
    } else if (cl->apiVersion >= 8) {
        *(CWTargetInfo_V8*)targetInfo = *(CWTargetInfo_V8*)cl->targetinfo;
    } else {
        memset(targetInfo, 0, sizeof(CWTargetInfo));
        if (CWFileSpecNotEmpty(&cl->targetinfo_V7.outfile)) {
            targetInfo->outputType = linkOutputFile;
        } else {
            targetInfo->outputType = linkOutputNone;
        }
        targetInfo->outfile = cl->targetinfo_V7.outfile;
        targetInfo->symfile = cl->targetinfo_V7.symfile;
        targetInfo->runfile = cl->targetinfo_V7.outfile;
        targetInfo->linkType = cl->targetinfo_V7.linkType;
        targetInfo->canRun = cl->targetinfo_V7.canRun;
        targetInfo->canDebug = cl->targetinfo_V7.canDebug;
        targetInfo->debuggerCreator = cl->targetinfo_V7.debuggerCreator;
        targetInfo->runHelperCreator = cl->targetinfo_V7.runHelperCreator;
    }
#endif
    return cwNoErr;
}

CW_CALLBACK CWGetMainFileNumber(CWPluginContext context, SInt32* fileNumber) {
    CWCompilerLinkerContext* cl;
    if (!fileNumber) {
        return cwErrInvalidParameter;
    }
#if MWCCPSP
    cl = GetContext(context);
    if (!cl) {
#else
    if (!(cl = GetContext(context))) {
#endif
        return cwErrInvalidCallback;
    }
    *fileNumber = cl->whichfile;
    return cwNoErr;
}

CW_CALLBACK CWGetMainFileID(CWPluginContext context, short* fileID) {
    CWCompilerLinkerContext* cl;
    if (!fileID) {
        return cwErrInvalidParameter;
    }
#if MWCCPSP
    cl = GetContext(context);
    if (!cl) {
#else
    if (!(cl = GetContext(context))) {
#endif
        return cwErrInvalidCallback;
    }
    *fileID = cl->fileID;
    return cwNoErr;
}

#if MWCCPSP

#endif
CW_CALLBACK CWGetMainFileSpec(CWPluginContext context, CWFileSpec* fileSpec) {
    CWCompilerLinkerContext* cl;
    if (!fileSpec) {
        return cwErrInvalidParameter;
    }
#if MWCCPSP
    cl = GetContext(context);
    if (!cl) {
#else
    if (!(cl = GetContext(context))) {
#endif
        return cwErrInvalidCallback;
    }
#if MWCCPSP
    func_00415710(cl, &cl->sourcefile, fileSpec);
#else
    *fileSpec = cl->sourcefile;
#endif
    return cwNoErr;
}

CW_CALLBACK CWGetMainFileText(CWPluginContext context, const char** text, SInt32* textLength) {
    CWCompilerLinkerContext* cl;
    if (!text) {
        return cwErrInvalidParameter;
    }
    if (!textLength) {
        return cwErrInvalidParameter;
    }
#if MWCCPSP
    cl = GetContext(context);
    if (!cl) {
#else
    if (!(cl = GetContext(context))) {
#endif
        return cwErrInvalidCallback;
    }
    *text = cl->sourcetext;
    *textLength = cl->sourcetextsize;
    return cwNoErr;
}

CW_CALLBACK CWStoreObjectData(CWPluginContext context, SInt32 whichfile, CWObjectData* object) {
    CWCompilerLinkerContext* cl;
    if (!object) {
        return cwErrInvalidParameter;
    }
#if MWCCPSP
    cl = GetContext(context);
    if (!cl) {
#else
    if (!(cl = GetContext(context))) {
#endif
        return cwErrInvalidCallback;
    }
#if MWCCPSP
    // 3.0.1 hands the shell a copy whose specs are converted to OSSpecs
    {
        OSSpec objectfile;
        CWObjectData shellobject;
        ShellDependencyInfo* deps;
        PluginDependencyInfo* plugindeps;
        CWResult result;
        int i;

        deps = NULL;
        memset(&shellobject, 0, sizeof(shellobject));
        shellobject.objectdata = object->objectdata;
        shellobject.browsedata = object->browsedata;
        shellobject.reserved1 = object->reserved1;
        shellobject.codesize = object->codesize;
        shellobject.udatasize = object->udatasize;
        shellobject.idatasize = object->idatasize;
        shellobject.compiledlines = object->compiledlines;
        shellobject.interfaceChanged = object->interfaceChanged;
        shellobject.reserved2 = object->reserved2;
        shellobject.compilecontext = object->compilecontext;
        shellobject.dependencyCount = object->dependencyCount;
        if (object->dependencyCount > 0) {
            deps =
                (ShellDependencyInfo*)calloc(object->dependencyCount, sizeof(ShellDependencyInfo));
            if (!deps) {
                return cwErrOutOfMemory;
            }
            plugindeps = (PluginDependencyInfo*)object->dependencies;
            shellobject.dependencies = (struct CWDependencyInfo*)deps;
            for (i = 0; i < object->dependencyCount; i++) {
                deps[i].fileIndex = plugindeps[i].fileIndex;
                func_00415730(cl, plugindeps[i].fileSpec, &deps[i].fileSpec);
                deps[i].dependencyType = plugindeps[i].dependencyType;
                deps[i].dependencyTag = plugindeps[i].dependencyTag;
            }
        }
        // the original reloads object->objectfile for the call
        if (context->apiVersion >= 10 && object->objectfile) {
            func_00415730(cl, object->objectfile, &objectfile);
            shellobject.objectfile = (CWFileSpec*)&objectfile;
        }
        result = cl->callbacks->cbStoreObjectData(cl, whichfile, &shellobject);
        if (deps) {
            object->dependencies = (struct CWDependencyInfo*)plugindeps;
            free(deps);
        }
        return result;
    }
#else
    return cl->callbacks->cbStoreObjectData(cl, whichfile, object);
#endif
}

CW_CALLBACK CWDisplayLines(CWPluginContext context, SInt32 nlines) {
    CWCompilerLinkerContext* cl;
#if MWCCPSP
    cl = GetContext(context);
    if (!cl) {
#else
    if (!(cl = GetContext(context))) {
#endif
        return cwErrInvalidCallback;
    }
    return cl->callbacks->cbDisplayLines(cl, nlines);
}

CW_CALLBACK CWCachePrecompiledHeader(
    CWPluginContext context, const CWFileSpec* filespec, CWMemHandle pchhandle
) {
    CWCompilerLinkerContext* cl;
    if (!filespec) {
        return cwErrInvalidParameter;
    }
    if (!pchhandle) {
        return cwErrInvalidParameter;
    }
#if MWCCPSP
    cl = GetContext(context);
    if (!cl) {
#else
    if (!(cl = GetContext(context))) {
#endif
        return cwErrInvalidCallback;
    }
#if MWCCPSP
    {
        OSSpec spec;
        func_00415730(cl, filespec, &spec);
        return cl->callbacks->cbCachePrecompiledHeader(cl, (const CWFileSpec*)&spec, pchhandle);
    }
#else
    return cl->callbacks->cbCachePrecompiledHeader(cl, filespec, pchhandle);
#endif
}

CW_CALLBACK CWGetPrecompiledHeaderSpec(
    CWPluginContext context, CWFileSpec* pchspec, const char* target
) {
    CWCompilerLinkerContext* cl;
    if (!pchspec) {
        return cwErrInvalidParameter;
    }
#if MWCCPSP
    cl = GetContext(context);
    if (!cl) {
#else
    if (!(cl = GetContext(context))) {
#endif
        return cwErrInvalidCallback;
    }
#if MWCCPSP
    {
        OSSpec spec;
        CWResult result;
        func_00415730(cl, pchspec, &spec);
        result = cl->callbacks->cbGetPrecompiledHeaderSpec(cl, (CWFileSpec*)&spec, target);
        func_00415710(cl, &spec, pchspec);
        return result;
    }
#else
    return cl->callbacks->cbGetPrecompiledHeaderSpec(cl, pchspec, target);
#endif
}
#if MWCCPSP

#endif
#if MWCCPSP
#define TARGET_INFO_TO_PLUGIN(name, type) \
    static void name(CWCompilerLinkerContext* cl, type* ti) { \
        ti->outputType = cl->targetinfo->outputType; \
        func_00415710(cl, &cl->targetinfo->outfile, ti->outfile); \
        func_00415710(cl, &cl->targetinfo->symfile, ti->symfile); \
        func_00415710(cl, &cl->targetinfo->runfile, ti->runfile); \
        ti->linkType = cl->targetinfo->linkType; \
        ti->canRun = cl->targetinfo->canRun; \
        ti->canDebug = cl->targetinfo->canDebug; \
        ti->targetCPU = cl->targetinfo->targetCPU; \
        ti->targetOS = cl->targetinfo->targetOS; \
        ti->runHelperIsRegKey = cl->targetinfo->runHelperIsRegKey; \
        ti->debugHelperIsRegKey = cl->targetinfo->debugHelperIsRegKey; \
        func_00415710(cl, &cl->targetinfo->linkAgainstFile, ti->linkAgainstFile); \
    }
// 0x4466A0, 0x4467B0
TARGET_INFO_TO_PLUGIN(TargetInfoToPlugin2, PluginTargetInfo2)
TARGET_INFO_TO_PLUGIN(TargetInfoToPlugin1, PluginTargetInfo1)
#endif
}
