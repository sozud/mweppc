#include "driver/CLDependencies.h"
#include "driver/CLDropinCallbacks_V10.h"
#include "driver/CLErrors.h"
#include "driver/CLFileOps.h"
#include "driver/CLFiles.h"
#include "driver/CLIncludeFileCache.h"
#include "driver/CLIO.h"
#include "driver/CLMain.h"
#include "driver/CLPlugins.h"
#include "driver/cmdline.h"
#include "driver/CWPlugins.h"
#include "driver/CWPluginsPrivate.h"
#include "driver/DropInCompilerLinker.h"
#include "driver/ErrMgr.h"
#include "driver/FileHandles.h"
#include "driver/Files.h"
#include "driver/Generic.h"
#include "driver/MacSpecs.h"
#include "driver/Memory.h"
#include "driver/MemUtils.h"
#include "driver/MsDos.h"
#include "driver/oslib.h"
#include "driver/pref_structs.h"
#if MWCCPSP
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>

extern "C" {

#if MWCCPSP
extern "C" {
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
// 0x41C280: reads an object file the plugin wrote back into memory
extern Boolean func_0041C280(File* file);
}

// 0x42B390: the shell's spec; the handle goes to the include cache
CW_CALLBACK UCBCachePrecompiledHeader(
    CWPluginContext context, const CWFileSpec* filespec, CWMemHandle pchhandle
) {
    OSSpec spec;

    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBCachePrecompiledHeader");
    }

#if MWCCPS2
    if (ShellHandles_Find(context, pchhandle, TRUE) != NULL) {
#else
    if (ShellHandles_Find(context, pchhandle, TRUE)) {
#endif
        spec = *reinterpret_cast<const OSSpec*>(filespec);
        CacheIncludeFile(&spec, reinterpret_cast<OSHandle*>(pchhandle), TRUE);
        ShellHandles_Remove(context, pchhandle, TRUE);
        return cwNoErr;
    }
    return cwErrInvalidParameter;
}

// 0x42B420
CW_CALLBACK UCBLoadObjectData(CWPluginContext context, SInt32 whichfile, CWMemHandle* objectdata) {
    File* file;

    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBLoadObjectData");
    }

    *objectdata = NULL;
    if (whichfile == -1) {
        whichfile = static_cast<CWCompilerLinkerContext*>(context)->whichfile;
    }
    file = Files_GetFile(&gTarg->files, whichfile);
    if (!file) {
        return cwErrUnknownFile;
    }
    if (!file->objectdata && file->objectOnDisk && !func_0041C280(file)) {
        return cwErrRequestFailed;
    }
#if MWCCPS2
    if (file->objectdata) {
        *objectdata = reinterpret_cast<CWMemHandle>(file->objectdata);
        if (!ShellHandles_Find(context, file->objectdata, TRUE)) {
            ShellHandles_Add(context, file->objectdata, TRUE, TRUE, FALSE, FALSE);
        }
        return cwNoErr;
    }
    return cwErrRequestFailed;
#else
    if (!file->objectdata) {
        return cwErrRequestFailed;
    }
    *objectdata = reinterpret_cast<CWMemHandle>(file->objectdata);
    if (!ShellHandles_Find(context, file->objectdata, TRUE)) {
        ShellHandles_Add(context, file->objectdata, TRUE, TRUE, FALSE, FALSE);
    }
    return cwNoErr;
#endif
}
#else
CW_CALLBACK UCBCachePrecompiledHeader(
    CWPluginContext context, const CWFileSpec* filespec, CWMemHandle pchhandle
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBCachePrecompiledHeader");
    }

    Handle handle;
    OSSpec spec;
    OS_FSSpec_To_OSSpec(filespec, &spec);
    UCBSecretDetachHandle(context, pchhandle, &handle);
    CacheIncludeFile(&spec, handle, TRUE);
    return cwNoErr;
}

CW_CALLBACK UCBLoadObjectData(CWPluginContext context, SInt32 whichfile, CWMemHandle* objectdata) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBLoadObjectData");
    }

    File* file = Files_GetFile(&gTarg->files, whichfile);
    if (!file) {
        return cwErrUnknownFile;
    }

    if (file->objectdata) {
        UCBSecretAttachHandle(context, file->objectdata, objectdata);
        return cwNoErr;
    } else {
        return cwErrRequestFailed;
    }
}
#endif

#if MWCCPSP
// 0x428470: UCBFindAndLoadFile with the shell's file info
extern "C" CW_CALLBACK func_00428470(
    CWPluginContext context, const char* filename, ShellFileInfo* fileinfo
);

// 0x42B510
CW_CALLBACK UCBStoreObjectData(CWPluginContext context, SInt32 whichfile, CWObjectData* object) {
    File* filedata;
    ShellFileInfo fi;

    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBStoreObjectData");
    }

    if (CheckForUserBreak()) {
        return cwErrUserCanceled;
    }

    if (whichfile == -1) {
        whichfile = static_cast<CWCompilerLinkerContext*>(context)->whichfile;
    }
    filedata = Files_GetFile(&gTarg->files, whichfile);
    if (!filedata) {
        return cwErrUnknownFile;
    }

    if (object->objectfile) {
        if (filedata->outfileowner && filedata->outfileowner != CmdLineStageMask_Cg) {
            DO_INTERNAL_ERROR(
                278, "Cannot store object file spec for '%s'\n", filedata->srcfilename
            );
        }
        filedata->outfss = *reinterpret_cast<const OSSpec*>(object->objectfile);
        filedata->wroteToDisk |= CmdLineStageMask_Cg;
        if (optsCmdLine.verbose) {
            CLReport(
                CLStr14,
                filedata->srcfilename,
                OS_SpecToStringRelative(&filedata->outfss, NULL, STSbuf, sizeof(STSbuf))
            );
        }
        if (!filedata->objectdata) {
            filedata->objectOnDisk = TRUE;
        }
    } else {
        if (filedata->objectdata &&
            filedata->objectdata != reinterpret_cast<OSHandle*>(object->objectdata)) {
            OS_FreeHandle(filedata->objectdata);
        }
        if (filedata->browsedata &&
            filedata->browsedata != reinterpret_cast<OSHandle*>(object->browsedata)) {
            OS_FreeHandle(filedata->browsedata);
        }
        filedata->objectdata = reinterpret_cast<OSHandle*>(object->objectdata);
        filedata->browsedata = reinterpret_cast<OSHandle*>(object->browsedata);
        ShellHandles_Remove(context, filedata->objectdata, TRUE);
        ShellHandles_Remove(context, filedata->browsedata, TRUE);
        filedata->objectOnDisk = FALSE;
    }

    filedata->codesize = object->codesize;
    filedata->udatasize = object->udatasize;
    filedata->idatasize = object->idatasize;
    filedata->compiledlines = object->compiledlines;
    filedata->recompileDependents = object->dependencyCount && object->interfaceChanged;
    OS_GetTime(&filedata->outmoddate);
    if (object->reserved1) {
        filedata->filetype = object->reserved1;
    }

    if (object->reserved2) {
        char* newname = reinterpret_cast<char*>(object->reserved2);
        if (OS_MakeSpec(newname, &filedata->srcfss, NULL) || OS_Status(&filedata->srcfss)) {
            fi.fullsearch = TRUE;
            fi.dependencyType = cwNoDependency;
            fi.isdependentoffile = -1;
            fi.suppressload = TRUE;
            if (func_00428470(context, newname, &fi) == cwNoErr) {
                filedata->srcfss = fi.filespec;
            } else {
                char* fnptr = OS_GetFileNamePtr(newname);
                if (fnptr != newname) {
                    fi.fullsearch = TRUE;
                    fi.dependencyType = cwNoDependency;
                    fi.isdependentoffile = -1;
                    fi.suppressload = TRUE;
                    if (func_00428470(context, fnptr, &fi) == cwNoErr) {
                        filedata->srcfss = fi.filespec;
                    }
                }
            }
        }
    }

    return cwNoErr;
}
#else
CW_CALLBACK UCBStoreObjectData(CWPluginContext context, SInt32 whichfile, CWObjectData* object) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBStoreObjectData");
    }

    if (CheckForUserBreak()) {
        return cwErrUserCanceled;
    }

    File* filedata = Files_GetFile(&gTarg->files, whichfile);
    if (!filedata) {
        return cwErrUnknownFile;
    }

    if (!object->objectfile) {
        Handle objecthand;
        Handle browsehand;
        UCBSecretDetachHandle(context, object->objectdata, &objecthand);
        UCBSecretDetachHandle(context, object->browsedata, &browsehand);
        filedata->objectdata = objecthand;
        filedata->browsedata = browsehand;
        UCBSecretAttachHandle(context, objecthand, &object->objectdata);
        UCBSecretAttachHandle(context, browsehand, &object->browsedata);
    } else {
        if (filedata->outfileowner && filedata->outfileowner != CmdLineStageMask_Cg) {
            DO_INTERNAL_ERROR(
                240, "Cannot store object file spec for '%s'\n", filedata->srcfilename
            );
        }
        OS_FSSpec_To_OSSpec(object->objectfile, &filedata->outfss);
        filedata->wroteToDisk |= CmdLineStageMask_Cg;
    }

    filedata->codesize = object->codesize;
    filedata->udatasize = object->udatasize;
    filedata->idatasize = object->idatasize;
    filedata->compiledlines = object->compiledlines;
    if (filedata->dropinflags & kGeneratesrsrcs) {
        filedata->hasresources = TRUE;
    } else {
        filedata->hasobjectcode = TRUE;
    }
    filedata->recompileDependents = object->dependencyCount && object->interfaceChanged;
    OS_GetTime(&filedata->outmoddate);
    if (object->reserved1) {
        filedata->filetype = object->reserved1;
    }

    if (object->reserved2) {
        char* newname = reinterpret_cast<char*>(object->reserved2);
        CWFileInfo fi;
        if (OS_MakeSpec(newname, &filedata->srcfss, NULL) || OS_Status(&filedata->srcfss)) {
            fi.fullsearch = TRUE;
            fi.dependencyType = cwNoDependency;
            fi.isdependentoffile = -1;
            fi.suppressload = TRUE;
            if (UCBFindAndLoadFile(context, newname, &fi) == cwNoErr) {
                OS_FSSpec_To_OSSpec(&fi.filespec, &filedata->srcfss);
            } else {
                char* fnptr = OS_GetFileNamePtr(newname);
                if (fnptr != newname) {
                    fi.fullsearch = TRUE;
                    fi.dependencyType = cwNoDependency;
                    fi.isdependentoffile = -1;
                    fi.suppressload = TRUE;
                    if (UCBFindAndLoadFile(context, fnptr, &fi) == cwNoErr) {
                        OS_FSSpec_To_OSSpec(&fi.filespec, &filedata->srcfss);
                    }
                }
            }
        }
    }

    return cwNoErr;
}
#endif

#if MWCCPSP
// 0x41C200: releases a file's object data
extern "C" void func_0041C200(File* file);

// 0x42B7D0
CW_CALLBACK UCBFreeObjectData(CWPluginContext context, SInt32 whichfile, CWMemHandle objectdata) {
    File* file;

    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBFreeObjectData");
    }

    if (whichfile == -1) {
        whichfile = static_cast<CWCompilerLinkerContext*>(context)->whichfile;
    }
    file = Files_GetFile(&gTarg->files, whichfile);
    if (!file) {
        return cwErrUnknownFile;
    }
    if (file->objectdata && file->objectdata == reinterpret_cast<OSHandle*>(objectdata)) {
        ShellHandles_Remove(context, objectdata, TRUE);
        func_0041C200(file);
        return cwNoErr;
    }
    return cwErrInvalidParameter;
}
#else
CW_CALLBACK UCBFreeObjectData(CWPluginContext context, SInt32 whichfile, CWMemHandle objectdata) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBFreeObjectData");
    }

    File* file = Files_GetFile(&gTarg->files, whichfile);
    if (!file) {
        return cwErrUnknownFile;
    }

    if (file->objectdata) {
        DisposeHandle(file->objectdata);
        file->objectdata = NULL;
        return cwNoErr;
    } else {
        return cwErrInvalidParameter;
    }
}
#endif

CW_CALLBACK UCBDisplayLines(CWPluginContext context, SInt32 nlines) {
    ShowWorking(12);

#if MWCCPSP
    if (optsCmdLine.verbose > 5) {
#else
    if (optsCmdLine.verbose > 3) {
#endif
        CLPrint("Callback: %s\n", "UCBDisplayLines");
    }

    if (CheckForUserBreak()) {
        return cwErrUserCanceled;
    } else {
        return cwNoErr;
    }
}

CW_CALLBACK UCBBeginSubCompile(
    CWPluginContext context, SInt32 whichfile, CWPluginContext* subContext
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBBeginSubCompile");
    }

#if MWCCPSP
    DO_INTERNAL_ERROR(427, "UCBBeginSubCompile not implemented");
#else
    DO_INTERNAL_ERROR(372, "UCBBeginSubCompile not implemented");
#endif
    return cwErrRequestFailed;
}

CW_CALLBACK UCBEndSubCompile(CWPluginContext subContext) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBEndSubCompile");
    }

#if MWCCPSP
    DO_INTERNAL_ERROR(439, "UCBEndSubCompile not implemented");
#else
    DO_INTERNAL_ERROR(384, "UCBEndSubCompile not implemented");
#endif
    return cwErrRequestFailed;
}

CW_CALLBACK UCBGetPrecompiledHeaderSpec(
    CWPluginContext context, CWFileSpec* pchspec, const char* intarget
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBGetPrecompiledHeaderSpec");
    }

    CWCompilerLinkerContext* c;
    int err;
    OSSpec outfss;
    File* file;
    const CWObjectFlags* cof;
    char target[260];

    if (context->pluginType == CWDROPINCOMPILERTYPE || context->pluginType == CWDROPINLINKERTYPE) {
        c = static_cast<CWCompilerLinkerContext*>(context);
    } else {
        return cwErrInvalidCallback;
    }

    file = Files_GetFile(&gTarg->files, c->whichfile);
#if MWCCPSP
    OS_ASSERT(475, file != NULL);
#else
    OS_ASSERT(420, file != NULL);
#endif

    cof = Plugin_CL_GetObjectFlags(file->compiler);
    if (!file->outfilename[0]) {
        if (intarget) {
            if (optsCompiler.canonicalIncludes) {
                strcpy(target, intarget);
            } else if (OS_CanonPath(intarget, target) != noErr) {
                return cwErrInvalidParameter;
            }

            err = OS_MakeSpecWithPath(&file->srcfss.path, target, FALSE, &outfss);
            if (err) {
                CLReportOSError(CLStr97, err, target);
                return cwErrRequestFailed;
            }

            if (!file->outfileowner || file->outfileowner == CmdLineStageMask_Cg) {
                file->outfileowner = CmdLineStageMask_Cg;
                OS_SpecToStringRelative(&outfss, NULL, file->outfilename, sizeof(target));
            }

            if (optsCmdLine.verbose) {
                CLReport(
                    CLStr16,
                    "precompiled ",
                    OS_SpecToStringRelative(&outfss, NULL, STSbuf, sizeof(STSbuf))
                );
            }
        } else {
            OS_MakeSpecWithPath(
                &gTarg->outputDirectory,
                file->srcfilename,
                !optsCompiler.relPathInOutputDir,
                &outfss
            );
            OS_NameSpecSetExtension(
                &outfss.name, optsCompiler.pchFileExt[0] ? optsCompiler.pchFileExt : cof->pchFileExt
            );

            if (!file->outfileowner || file->outfileowner == CmdLineStageMask_Cg) {
                file->outfileowner = CmdLineStageMask_Cg;
                OS_NameSpecToString(&outfss.name, file->outfilename, sizeof(target));
            }

            CLReport(CLStr59, OS_SpecToStringRelative(&outfss, NULL, STSbuf, sizeof(STSbuf)));
        }

#if MWCCPSP
        SHELL_SPEC_FROM_OSSPEC(&outfss, pchspec);
#else
        OS_OSSpec_To_FSSpec(&outfss, pchspec);
#endif
    } else {
        err = OS_MakeSpecWithPath(&gTarg->outputDirectory, file->outfilename, FALSE, &outfss);
        if (err) {
            CLReportOSError(CLStr97, err, file->outfilename);
            return cwErrRequestFailed;
        }

#if MWCCPSP
        SHELL_SPEC_FROM_OSSPEC(&outfss, pchspec);
#else
        OS_OSSpec_To_FSSpec(&outfss, pchspec);
#endif

        if (intarget) {
            CLReport(
                CLStr60, OS_SpecToStringRelative(&outfss, NULL, STSbuf, sizeof(STSbuf)), intarget
            );
        }
    }

    return cwNoErr;
}

CW_CALLBACK UCBGetResourceFile(CWPluginContext context, CWFileSpec* filespec) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBGetResourceFile");
    }

#if MWCCPSP
    DO_INTERNAL_ERROR(569, "UCBGetResourceFile not implemented");
#else
    DO_INTERNAL_ERROR(514, "UCBGetResourceFile not implemented");
#endif
    return cwErrRequestFailed;
}

CW_CALLBACK UCBPutResourceFile(
    CWPluginContext context, const char* prompt, const char* name, CWFileSpec* filespec
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBPutResourceFile");
    }

#if MWCCPSP
    DO_INTERNAL_ERROR(584, "UCBPutResourceFile not implemented");
#else
    DO_INTERNAL_ERROR(529, "UCBPutResourceFile not implemented");
#endif
    return cwErrRequestFailed;
}

static int UnitNameToSBMName(OSSpec* spec, File* srcfile) {
    const CWObjectFlags* cof;
    int err;

    cof = Plugin_CL_GetObjectFlags(srcfile->compiler);
    err = OS_NameSpecChangeExtension(
        &spec->name,
        cof->pchFileExt ? cof->pchFileExt : ".sbm",
        cof->pchFileExt ? (cof->pchFileExt[0] == '.') : FALSE
    );
    return err;
}

CW_CALLBACK UCBLookUpUnit(
    CWPluginContext context,
    const char* name,
    Boolean isdependency,
    const void** unitdata,
    SInt32* unitdatalength
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBLookUpUnit");
    }

    CWFileInfo includeinfo;
    OSSpec unitspec;
    OSSpec sbmspec;
    OSTime unittime;
    UInt32 unitmactime;
    char sbmpath[260];
    int err;
    CWCompilerLinkerContext* c;
    File* srcfile;
    File* sbmfile;

    if (context->pluginType == CWDROPINCOMPILERTYPE || context->pluginType == CWDROPINLINKERTYPE) {
        c = static_cast<CWCompilerLinkerContext*>(context);
    } else {
        return cwErrInvalidCallback;
    }

    srcfile = Files_GetFile(&gTarg->files, c->whichfile);
#if MWCCPSP
    OS_ASSERT(646, srcfile != NULL);
#else
    OS_ASSERT(591, srcfile != NULL);
#endif

    *unitdata = NULL;
    *unitdatalength = 0;

    if (optsCompiler.sbmState == OptsCompilerSbmState_Rebuild ||
        optsCompiler.sbmState == OptsCompilerSbmState_Clean) {
        if (clState.pluginDebug) {
            CLPrint("UCBLookUpUnit:  sbmState == sbmRebuild or sbmClean; failing\n");
        }
        return cwErrSBMNotFound;
    }

    includeinfo.fullsearch = TRUE;
    includeinfo.dependencyType = isdependency ? cwNormalDependency : cwNoDependency;
    includeinfo.isdependentoffile = -1;
    includeinfo.suppressload = TRUE;
    if (UCBFindAndLoadFile(context, name, &includeinfo) != cwNoErr) {
        if (clState.pluginDebug) {
            CLPrint("UCBLookUpUnit:  could not find source '%s'; failing\n", name);
        }
        return cwErrFileNotFound;
    }

#if MWCCPSP
    SHELL_SPEC_TO_OSSPEC(&includeinfo.filespec, &unitspec);
#else
    OS_FSSpec_To_OSSpec(&includeinfo.filespec, &unitspec);
#endif
    sbmspec = unitspec;
    err = UnitNameToSBMName(&sbmspec, srcfile);
    if (err) {
        if (clState.pluginDebug) {
            CLPrint(
                "UCBLookUpUnit:  could not make precompiled unit spec for '%s' (%s)\n",
                name,
                OS_GetErrText(err)
            );
        }
        return cwErrRequestFailed;
    }

    if (optsCompiler.sbmPath[0]) {
        sbmspec.path = clState.sbmPathSpec;
        OS_SpecToString(&sbmspec, sbmpath, sizeof(sbmpath));
        if (clState.pluginDebug) {
            CLPrint("UCBLookUpUnit:  Only looking at '%s'\n", sbmpath);
        }
    } else {
        OS_NameSpecToString(&sbmspec.name, sbmpath, sizeof(sbmpath));
        if (clState.pluginDebug) {
            CLPrint("UCBLookUpUnit:  searching paths for '%s'\n", sbmpath);
        }
    }

    includeinfo.fullsearch = TRUE;
    includeinfo.dependencyType = isdependency ? cwNormalDependency : cwNoDependency;
    includeinfo.isdependentoffile = -1;
    includeinfo.suppressload = FALSE;
    if (UCBFindAndLoadFile(context, sbmpath, &includeinfo) != cwNoErr || !includeinfo.filedata) {
        if (clState.pluginDebug) {
            CLPrint(
                "UCBLookUpUnit:  could not find or load precompiled unit file '%s'; failing\n",
                sbmpath
            );
        }
        return cwErrFileNotFound;
    }

#if MWCCPSP
#if CW_SHELL_OSSPEC
    sbmspec = includeinfo.filespec;
#else
    SHELL_SPEC_TO_OSSPEC(&includeinfo.filespec, &sbmspec);
#endif
#else
    OS_FSSpec_To_OSSpec(&includeinfo.filespec, &sbmspec);
#endif
    if (includeinfo.filedatatype != cwFileTypePrecompiledHeader) {
        CLPrint(
            "UCBLookUpUnit:  file '%s' does not appear to be precompiled\n",
            OS_SpecToString(&sbmspec, STSbuf, sizeof(STSbuf))
        );
        UCBReleaseFileText(context, includeinfo.filedata);
        return cwErrRequestFailed;
    }

    OS_GetFileTime(&unitspec, NULL, &unittime);
    OS_TimeToMac(unittime, &unitmactime);
    if (unitmactime != *reinterpret_cast<const UInt32*>(includeinfo.filedata)) {
        if (clState.pluginDebug) {
            CLPrint(
                "UCBLookUpUnit:  file '%s' does not have internal timestamp that matches source unit's timestamp (0x%8x != 0x%8x)\n",
                OS_SpecToString(&sbmspec, STSbuf, sizeof(STSbuf)),
                unitmactime,
                *reinterpret_cast<const UInt32*>(includeinfo.filedata)
            );
        }
        UCBReleaseFileText(context, includeinfo.filedata);
        return cwErrRequestFailed;
    }

    sbmfile = Files_FindFile(&gTarg->pchs, &sbmspec);
    if (!sbmfile) {
        sbmfile = File_New();
        sbmfile->srcfss = unitspec;
        OS_SpecToString(&sbmfile->srcfss, sbmfile->srcfilename, sizeof(sbmpath));
        sbmfile->outfss = sbmspec;
        OS_SpecToString(&sbmfile->outfss, sbmfile->outfilename, sizeof(sbmpath));
        sbmfile->compiler = srcfile->compiler;
        sbmfile->outfileowner = CmdLineStageMask_Cg;
        sbmfile->writeToDisk = srcfile->writeToDisk;
        sbmfile->dropinflags = srcfile->dropinflags;
        sbmfile->objectflags = srcfile->objectflags;
        sbmfile->mappingflags = srcfile->mappingflags;
        Deps_Initialize(&sbmfile->deps, &gTarg->incls);
        if (!Files_AddFile(&gTarg->pchs, sbmfile)) {
            return cwErrRequestFailed;
        }
    }

    if (clState.pluginDebug) {
        CLPrint("UCBLookUpUnit:  success for '%s'\n", name);
    }

    if (optsCmdLine.verbose) {
        CLReport(
            CLStr62,
            "unit symbol table",
            OS_SpecToStringRelative(&sbmspec, NULL, STSbuf, sizeof(STSbuf))
        );
    }

    *unitdata = includeinfo.filedata;
    *unitdatalength = includeinfo.filedatalength;
    return cwNoErr;
}

CW_CALLBACK UCBSBMfiles(CWPluginContext context, short libref) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBSBMfiles");
    }
    return cwNoErr;
}

CW_CALLBACK UCBStoreUnit(
    CWPluginContext context,
    const char* inunitname,
    CWMemHandle unitdata,
    CWDependencyTag dependencytag
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBStoreUnit");
    }

    char unitname[260];
    OSSpec sbmspec;
    int err;
#if !MWCCPSP
    Handle h;
    OSHandle unithand;
#endif
    OSFileHandle fhand;
    CWCompilerLinkerContext* c;
    File* srcfile;

    if (optsCompiler.canonicalIncludes) {
        strcpy(unitname, inunitname);
    } else if (OS_CanonPath(inunitname, unitname)) {
        return cwErrInvalidParameter;
    }

    if (optsCompiler.sbmState == OptsCompilerSbmState_Normal ||
        optsCompiler.sbmState == OptsCompilerSbmState_Rebuild) {
        if (context->pluginType == CWDROPINCOMPILERTYPE ||
            context->pluginType == CWDROPINLINKERTYPE) {
            c = static_cast<CWCompilerLinkerContext*>(context);
        } else {
            return cwErrInvalidCallback;
        }

        srcfile = Files_GetFile(&gTarg->files, c->whichfile);
#if MWCCPSP
        OS_ASSERT(845, srcfile != NULL);
#else
        OS_ASSERT(791, srcfile != NULL);
#endif

        if (optsCompiler.sbmPath[0]) {
            err = OS_MakeSpecWithPath(&clState.sbmPathSpec, unitname, TRUE, &sbmspec);
            if (err) {
                if (clState.pluginDebug) {
                    CLPrint(
                        "UCBStoreUnit:  '%s' is a bad unit name (%s)\n",
                        unitname,
                        OS_GetErrText(err)
                    );
                }
                return cwErrInvalidParameter;
            }
        } else {
            err = OS_MakeFileSpec(unitname, &sbmspec);
            if (err) {
                if (clState.pluginDebug) {
                    CLPrint(
                        "UCBStoreUnit:  '%s' is a bad filename (%s)\n", unitname, OS_GetErrText(err)
                    );
                }
                return cwErrInvalidParameter;
            }
        }

        err = UnitNameToSBMName(&sbmspec, srcfile);
        if (err) {
            if (clState.pluginDebug) {
                CLPrint(
                    "UCBStoreUnit:  could not make precompiled unit form of '%s' (%s)\n",
                    unitname,
                    OS_GetErrText(err)
                );
            }
            return cwErrInvalidParameter;
        }

        if (optsCmdLine.verbose) {
            CLReport(
                CLStr61,
                "unit symbol table",
                OS_SpecToStringRelative(&sbmspec, NULL, STSbuf, sizeof(STSbuf))
            );
        }

#if MWCCPSP
        // 3.0.1: the unit data is an OSHandle; the shell forgets it
        ShellHandles_Remove(context, unitdata, TRUE);
        err = OS_NewFileHandle(&sbmspec, (OSHandle*)unitdata, TRUE, &fhand);
#else
        UCBSecretDetachHandle(context, unitdata, &h);
        OS_DestroyMacHandle(h, &unithand);
        err = OS_NewFileHandle(&sbmspec, &unithand, TRUE, &fhand);
#endif
        if (err || (err = OS_FreeFileHandle(&fhand))) {
            CLReportOSError(
                CLStr18,
                err,
                "precompiled unit",
                OS_SpecToStringRelative(&sbmspec, NULL, STSbuf, sizeof(STSbuf))
            );
            return cwErrRequestFailed;
        }
    }

    return cwNoErr;
}

CW_CALLBACK UCBReleaseUnit(CWPluginContext context, void* unitdata) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBReleaseUnit");
    }

    if (!unitdata) {
        return cwErrRequestFailed;
    }
    return UCBReleaseFileText(context, static_cast<const char*>(unitdata));
}

CW_CALLBACK UCBUnitNameToFileName(CWPluginContext context, const char* unitname, char* filename) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBUnitNameToFileName");
    }

    strcpy(filename, unitname);
    if (!OS_EqualPath(filename + strlen(filename) - 2, ".p")) {
        strcat(filename, ".p");
    }
    return cwNoErr;
}

CW_CALLBACK UCBOSAlert(CWPluginContext context, const char* message, OSErr errorcode) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBOSAlert");
    }
    if (CheckForUserBreak()) {
        return cwErrUserCanceled;
    }

    char errormessage[256];
    GetSysErrText(errorcode, errormessage);
    CLStyledMessageDispatch(
        static_cast<shellContextType*>(context->shellContext)->plugin,
        NULL,
        errorcode,
        Msg_Alert,
        "%\n(%)\n",
        message ? message : "",
        errormessage
    );
    return cwNoErr;
}

CW_CALLBACK UCBOSErrorMessage(CWPluginContext context, const char* message, OSErr errorcode) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBOSErrorMessage");
    }
#if MWCCPSP
    if (errorcode == cwNoErr) {
#else
    if (!errorcode) {
#endif
        return cwNoErr;
    }
    if (CheckForUserBreak()) {
        return cwErrUserCanceled;
    }

    char errormessage[256];
    GetSysErrText(errorcode, errormessage);
    CLStyledMessageDispatch(
        static_cast<shellContextType*>(context->shellContext)->plugin,
        NULL,
        errorcode,
        Msg_Alert,
        "%\n%\n",
        message ? message : "",
        errormessage
    );
    return cwNoErr;
}

CW_CALLBACK UCBGetModifiedFiles(
    CWPluginContext context, SInt32* modifiedFileCount, const SInt32** modifiedFiles
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBGetModifiedFiles");
    }

    *modifiedFileCount = 0;
#if MWCCPSP
    DO_INTERNAL_ERROR(1002, "CWGetModifiedFiles not implemented!\n");
#else
    DO_INTERNAL_ERROR(949, "CWGetModifiedFiles not implemented!\n");
#endif
    return cwNoErr;
}

CW_CALLBACK UCBGetSuggestedObjectFileSpec(
    CWPluginContext context, SInt32 whichfile, CWFileSpec* fileSpec
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBGetSuggestedObjectFileSpec");
    }

    File* file;
#if MWCCPS2
    if ((file = Files_GetFile(&gTarg->files, whichfile)) == NULL) {
#else
    if (!(file = Files_GetFile(&gTarg->files, whichfile))) {
#endif
        return cwErrUnknownFile;
    }

    if (!GetOutputFile(file, CmdLineStageMask_Cg)) {
        return cwErrRequestFailed;
    }

#if MWCCPSP
#if CW_SHELL_OSSPEC
    *fileSpec = file->outfss;
#else
    SHELL_SPEC_FROM_OSSPEC(&file->outfss, fileSpec);
#endif
#else
    OS_OSSpec_To_FSSpec(&file->outfss, fileSpec);
#endif
    return cwNoErr;
}

CW_CALLBACK UCBGetStoredObjectFileSpec(
    CWPluginContext context, SInt32 whichfile, CWFileSpec* fileSpec
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBGetStoredObjectFileSpec");
    }

    File* file;
    if (!(file = Files_GetFile(&gTarg->files, whichfile))) {
        return cwErrUnknownFile;
    }
    if (file->outfileowner != CmdLineStageMask_Cg) {
#if MWCCPSP
        DO_INTERNAL_ERROR(1046, "Lost stored object file spec for '%s'\n", file->srcfilename);
#else
        DO_INTERNAL_ERROR(993, "Lost stored object file spec for '%s'\n", file->srcfilename);
#endif
        return cwErrRequestFailed;
    } else {
#if MWCCPSP
#if CW_SHELL_OSSPEC
        *fileSpec = file->outfss;
#else
        SHELL_SPEC_FROM_OSSPEC(&file->outfss, fileSpec);
#endif
#else
        OS_OSSpec_To_FSSpec(&file->outfss, fileSpec);
#endif
        return cwNoErr;
    }
}
}

CWCompilerLinkerCallbacks sCompilerLinkerCallbacks = {
    UCBCachePrecompiledHeader,
    UCBLoadObjectData,
    UCBStoreObjectData,
    UCBFreeObjectData,
    UCBDisplayLines,
    UCBBeginSubCompile,
    UCBEndSubCompile,
    UCBGetPrecompiledHeaderSpec,
    UCBGetResourceFile,
    UCBPutResourceFile,
    UCBLookUpUnit,
    UCBSBMfiles,
    UCBStoreUnit,
    UCBReleaseUnit,
    UCBUnitNameToFileName,
    UCBOSErrorMessage,
    UCBOSAlert,
    UCBGetModifiedFiles,
    UCBGetSuggestedObjectFileSpec,
    UCBGetStoredObjectFileSpec,
    NULL
};

CWCompilerLinkerContext::CWCompilerLinkerContext()
    : CWPluginPrivateContext(clState.plugintype, sizeof(CWCompilerLinkerContext)) {
#if MWCCPSP
    targetinfo = static_cast<PluginTargetInfo*>(xmalloc("context", sizeof(PluginTargetInfo)));
    memset(targetinfo, 0, sizeof(PluginTargetInfo));
#else
    targetinfo = static_cast<CWTargetInfo*>(xmalloc("context", sizeof(CWTargetInfo)));
    memset(targetinfo, 0, sizeof(CWTargetInfo));
#endif

    if (gTarg) {
        targetinfo->targetCPU = gTarg->cpu;
        targetinfo->targetOS = gTarg->os;
    } else {
        targetinfo->targetCPU = targetinfo->targetOS = targetCPUAny;
    }

    whichfile = 0;
    memset(&sourcefile, 0, sizeof(sourcefile));
    sourcetext = NULL;
    sourcetextsize = 0;

    precompile = autoprecompile = preprocess = FALSE;
    cachingPCH = FALSE;
    debuginfo = FALSE;
    fileID = 0;
    memset(&browseoptions, 0, sizeof(browseoptions));
    reserved = NULL;
    sequenceID = 0;
    parentPB = NULL;
    targetStorage = NULL;
    texthandle = NULL;
    memset(&targetinfo_V7, 0, sizeof(targetinfo_V7));

    callbacks = &sCompilerLinkerCallbacks;
}

CWCompilerLinkerContext::~CWCompilerLinkerContext() {
#if MWCCPSP
    // 3.0.1 also frees what the linker request filled in
    SInt32 i;

    if (commandLineArgs) {
        xfree((void*)commandLineArgs);
    }
    if (workingDirectorySpec) {
        xfree(workingDirectorySpec);
    }
    if (environmentVariables) {
        for (i = 0; i < numEnvironmentVariables; i++) {
            xfree((void*)environmentVariables[i].name);
            xfree((void*)environmentVariables[i].value);
        }
        xfree(environmentVariables);
    }
#endif
    xfree(targetinfo);
}
