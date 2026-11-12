#include "driver/CLFileOps.h"
#include "driver/CLAccessPaths.h"
#include "driver/CLBrowser.h"
#include "driver/CLDependencies.h"
#include "driver/CLErrors.h"
#include "driver/CLFiles.h"
#include "driver/CLIncludeFileCache.h"
#include "driver/CLIO.h"
#include "driver/CLMain.h"
#include "driver/CLPluginRequests.h"
#include "driver/CLPlugins.h"
#include "driver/CLTarg.h"
#include "driver/CLToolExec.h"
#include "driver/CLWriteObjectFile.h"
#include "driver/cmdline.h"
#include "driver/CWPlugins.h"
#include "driver/DropInCompilerLinker.h"
#if MWCCPSP
#include "driver/FileHandles.h"
#endif
#include "driver/Files.h"
#include "driver/Generic.h"
#include "driver/MacSpecs.h"
#include "driver/Memory.h"
#if MWCCPSP
#include "driver/MemUtils.h"
#endif
#include "driver/MsDos.h"
#include "driver/oslib.h"
#include "driver/pref_structs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_bytes(UInt32 size, int* amount, const char** unit);
#if MWCCPSP

static UInt32 TEXTDATA_SIZE(OSHandle* h) {
    UInt32 size;
    OS_GetHandleSize(h, &size);
    return size;
}
#endif

#if MWCCPSP
extern int __stdcall func_00409690(const OSSpec* from, const OSSpec* to); // rename
extern Boolean func_00416B10(const OSSpec* spec, Boolean decorate); // show a text file

// 0x41C480: text output comes in a handle or in the temporary file
// file->tempfss. Make-style dependency output (-M without writing to disk)
// reads the file into a handle; otherwise the file is renamed to the output
// or shown, then deleted.
static int OutputTextData(File* file, SInt16 stage, OSType maccreator, OSType mactype) {
    int ret;
    Boolean fromTemp;
    OSFileHandle fh;
    int err;

    ret = 0;
    fromTemp = FALSE;

    if (!file->textdata && !OS_Status(&file->tempfss)) {
        if (stage == CmdLineStageMask_Dp && !(optsCmdLine.toDisk & CmdLineStageMask_Dp)) {
            fromTemp = TRUE;
        } else {
            fromTemp = FALSE;
        }
        if (fromTemp) {
            err = OS_NewFileHandle(&file->tempfss, NULL, FALSE, &fh);
            if (err) {
                CLReportOSError(
                    CLStr93, err, OS_SpecToString(&file->tempfss, STSbuf, sizeof(STSbuf))
                );
                return 0;
            }
            file->textdata = &fh.hand;
        }
    }

    if (file->textdata) {
        if (stage != CmdLineStageMask_Dp || optsCmdLine.toDisk & CmdLineStageMask_Dp) {
            if (!(file->writeToDisk & stage)) {
                ret = ShowHandle(file->textdata, FALSE);
            } else {
                if (optsCmdLine.verbose) {
                    CLReport(
                        CLStr15,
                        OS_SpecToStringRelative(&file->outfss, NULL, STSbuf, sizeof(STSbuf))
                    );
                }
                ret = WriteHandleToFile(&file->outfss, file->textdata, maccreator, mactype);
                file->wroteToDisk |= stage;
            }
        } else {
            if (optsCompiler.outMakefile[0]) {
                ret =
                    AppendHandleToFile(&clState.makefileSpec, file->textdata, maccreator, mactype);
            } else {
                ret = ShowHandle(file->textdata, FALSE);
            }
        }

        if (fromTemp) {
            OS_FreeFileHandle(&fh);
            OS_Delete(&file->tempfss);
        } else {
            OS_FreeHandle(file->textdata);
            file->textdata = NULL;
        }
    } else if (!OS_Status(&file->tempfss)) {
        if (file->writeToDisk & stage) {
            if (optsCmdLine.verbose) {
                CLReport(
                    CLStr15, OS_SpecToStringRelative(&file->outfss, NULL, STSbuf, sizeof(STSbuf))
                );
            }
            OS_Delete(&file->outfss);
            err = func_00409690(&file->tempfss, &file->outfss);
            if (err) {
                CLReportOSError(
                    CLStr8, err, OS_SpecToString(&file->outfss, STSbuf, sizeof(STSbuf))
                );
            } else {
                OS_SetMacFileCreatorAndType(&file->outfss, maccreator, mactype);
                ret = 1;
            }
            file->wroteToDisk |= stage;
        } else {
            ret = func_00416B10(&file->tempfss, FALSE);
            OS_Delete(&file->tempfss);
        }
    } else {
        const char* doing;
        switch (stage) {
            case CmdLineStageMask_Pp:
                doing = "preprocessing";
                break;
            case CmdLineStageMask_Ds:
                doing = "disassembling";
                break;
            case CmdLineStageMask_Dp:
                doing = "dependency-mapping";
                break;
            case CmdLineStageMask_Cg:
                doing = "compiling";
                break;
            default:
                doing = "processing";
                break;
        }
        CLReportError(CLStr96, doing, file->srcfilename);
        ret = 0;
    }

    return ret;
}
#else
static int OutputTextData(File* file, SInt16 stage, OSType maccreator, OSType mactype) {
    int ret = 0;
    SInt32 size;

    if (file->textdata) {
        size = GetHandleSize(file->textdata);
        if (stage != CmdLineStageMask_Dp || optsCmdLine.toDisk & CmdLineStageMask_Dp) {
            if (!(file->writeToDisk & stage)) {
                ret = ShowHandle(file->textdata, size, FALSE);
            } else {
                if (optsCmdLine.verbose) {
                    CLReport(
                        CLStr15,
                        OS_SpecToStringRelative(&file->outfss, NULL, STSbuf, sizeof(STSbuf))
                    );
                }
                ret = WriteHandleToFile(&file->outfss, file->textdata, size, maccreator, mactype);
                file->wroteToDisk |= stage;
            }
        } else {
            if (optsCompiler.outMakefile[0]) {
                ret = AppendHandleToFile(
                    &clState.makefileSpec, file->textdata, size, maccreator, mactype
                );
            } else {
                ret = ShowHandle(file->textdata, size, FALSE);
            }
        }

        DisposeHandle(file->textdata);
        file->textdata = NULL;
    }

    return ret;
}
#endif

#if MWCCPSP
// 3.0.1: with -gccdep the dependency file is named after the object and goes
// in its directory; a plugin without an extension for an output gets a
// default one
int GetOutputFile(File* file, SInt16 stage) {
    int err;
    char tempoutfilename[260];
    char* targetoutfilename;
    Boolean specifiedName;
    const char* ext;
    const CWObjectFlags* cof;
    char tmpname[260];
    char* env;
    char* extptr;
    char* fnptr;
    char text[256];
    char* inname;
    char* basename;
    OSPathSpec tmppath;
    OSPathSpec* outdir;

    outdir = &gTarg->outputDirectory;
    if (optsCmdLine.toDisk & stage) {
        file->writeToDisk |= stage;
    }

    if (file->outfileowner == stage) {
        targetoutfilename = file->outfilename;
        specifiedName = file->outfilename[0] && (file->tempOnDisk & stage) == 0;
        if (specifiedName) {
            file->writeToDisk |= stage;
            file->tempOnDisk &= ~stage;
        }
    } else {
        targetoutfilename = tempoutfilename;
        specifiedName = FALSE;
    }

    if (stage == CmdLineStageMask_Dp && optsCompiler.gccDepends) {
        basename = file->outfilename[0] ? file->outfilename : file->srcfilename;
    } else {
        basename = file->srcfilename;
    }

    if (!(stage & (optsCmdLine.toDisk | file->writeToDisk | file->tempOnDisk))) {
        if ((stage == CmdLineStageMask_Cg) &&
            (optsCmdLine.stages & CmdLineStageMask_Ds) &&
            (optsCmdLine.state == OptsCmdLineState_Compile) &&
            optsCompiler.keepObjects) {
            file->writeToDisk |= stage;
        } else {
            file->tempOnDisk |= stage;
        }
    }

    if (!specifiedName && (stage & (optsCmdLine.toDisk | file->writeToDisk | file->tempOnDisk))) {
        cof = Plugin_CL_GetObjectFlags(file->compiler);
        if (stage == CmdLineStageMask_Pp) {
            ext = optsCompiler.ppFileExt[0] ? optsCompiler.ppFileExt : cof->ppFileExt;
            if (!ext) {
                ext = ".pp";
            }
        } else if (stage == CmdLineStageMask_Ds) {
            ext = optsCompiler.disFileExt[0] ? optsCompiler.disFileExt : cof->disFileExt;
            if (!ext) {
                ext = ".dis";
            }
        } else if (stage == CmdLineStageMask_Dp) {
            ext = optsCompiler.depFileExt[0] ? optsCompiler.depFileExt : cof->depFileExt;
            if (!ext) {
                ext = ".dep";
            }
        } else if (stage == CmdLineStageMask_Cg) {
            ext = optsCompiler.objFileExt[0] ? optsCompiler.objFileExt : cof->objFileExt;
            if (!ext) {
                ext = ".cod";
            }
            if (!(file->objectflags & 0x80000000) && !specifiedName) {
                ext = NULL;
            }
        } else {
            ext = NULL;
        }

        if (ext && ext[0]) {
            if (ext[0] != '.') {
                snprintf(text, sizeof(text), ".%s", ext);
            } else {
                strncpy(text, ext, sizeof(text) - 1);
                text[sizeof(text) - 1] = 0;
            }
            if ((file->tempOnDisk & stage) && !(optsCmdLine.stages & CmdLineStageMask_Dp)) {
                env = getenv("TEMP");
                if (!env || !env[0] || OS_MakePathSpec(NULL, env, &tmppath)) {
                    env = getenv("TMP");
                }
                if (!env || !env[0] || OS_MakePathSpec(NULL, env, &tmppath)) {
                    env = getenv("TMPDIR");
                }
                if (!env || !env[0] || OS_MakePathSpec(NULL, env, &tmppath)) {
                    env = ".";
                }
                snprintf(
                    tmpname, sizeof(tmpname), "%s%c%s", env, OS_PATHSEP, OS_GetFileNamePtr(basename)
                );
                inname = tmpname;
            } else if (!optsCompiler.relPathInOutputDir) {
                inname = OS_GetFileNamePtr(basename);
            } else {
                inname = basename;
            }

            fnptr = OS_GetFileNamePtr(inname);
            extptr = strrchr(fnptr, '.');
            if (!extptr || ext[0] == '.') {
                extptr = fnptr + strlen(fnptr);
            }

            if (extptr - fnptr + strlen(text) >= sizeof(text)) {
                extptr = &fnptr[sizeof(text) - 1] - strlen(text);
            }

            snprintf(targetoutfilename, 260, "%.*s%s", extptr - inname, inname, text);
        } else {
            *targetoutfilename = 0;
            file->writeToDisk &= ~stage;
            file->tempOnDisk &= ~stage;
        }
    }

    if (stage & (file->writeToDisk | file->tempOnDisk)) {
        if (stage == CmdLineStageMask_Dp && optsCompiler.gccDepends) {
            outdir = &file->outfss.path;
        }
        err = OS_MakeSpecWithPath(
            outdir, targetoutfilename, !optsCompiler.relPathInOutputDir, &file->outfss
        );
        if (!err) {
            if (OS_EqualSpec(&file->srcfss, &file->outfss)) {
                if (specifiedName) {
                    CLReportError(CLStr102, targetoutfilename);
                    file->writeToDisk &= ~stage;
                    file->tempOnDisk &= ~stage;
                    return FALSE;
                }
                file->writeToDisk &= ~stage;
                file->tempOnDisk &= ~stage;
            }
        } else {
            CLReportOSError(
                CLStr9,
                err,
                targetoutfilename,
                OS_PathSpecToString(&gTarg->outputDirectory, STSbuf, sizeof(STSbuf))
            );
        }
        return err == 0;
    }

    return TRUE;
}
#else
int GetOutputFile(File* file, SInt16 stage) {
    int err;
    char tempoutfilename[260];
    char* targetoutfilename;
    Boolean specifiedName;
    const char* ext;
    const CWObjectFlags* cof;
    char tmpname[260];
    char* env;
    char* extptr;
    char* fnptr;
    char text[256];
    char* inname;
    OSPathSpec tmppath;

    if (optsCmdLine.toDisk & stage) {
        file->writeToDisk |= stage;
    }

    if (file->outfileowner == stage) {
        targetoutfilename = file->outfilename;
        specifiedName = file->outfilename[0] && (file->tempOnDisk & stage) == 0;
        if (specifiedName) {
            file->writeToDisk |= stage;
            file->tempOnDisk &= ~stage;
        }
    } else {
        targetoutfilename = tempoutfilename;
        specifiedName = FALSE;
    }

    if (!(stage & (optsCmdLine.toDisk | file->writeToDisk | file->tempOnDisk))) {
        if ((stage == CmdLineStageMask_Cg) &&
            (optsCmdLine.stages & CmdLineStageMask_Ds) &&
            (optsCmdLine.state == OptsCmdLineState_Compile) &&
            optsCompiler.keepObjects) {
            file->writeToDisk |= stage;
        } else {
            file->tempOnDisk |= stage;
        }
    }

    if (!specifiedName && (stage & (optsCmdLine.toDisk | file->writeToDisk | file->tempOnDisk))) {
        cof = Plugin_CL_GetObjectFlags(file->compiler);
        if (stage == CmdLineStageMask_Pp) {
            ext = optsCompiler.ppFileExt[0] ? optsCompiler.ppFileExt : cof->ppFileExt;
        } else if (stage == CmdLineStageMask_Ds) {
            ext = optsCompiler.disFileExt[0] ? optsCompiler.disFileExt : cof->disFileExt;
        } else if (stage == CmdLineStageMask_Dp) {
            ext = optsCompiler.depFileExt[0] ? optsCompiler.depFileExt : cof->depFileExt;
        } else if (stage == CmdLineStageMask_Cg) {
            ext = optsCompiler.objFileExt[0] ? optsCompiler.objFileExt : cof->objFileExt;
            if (!(file->objectflags & 0x80000000) && !specifiedName) {
                ext = NULL;
            }
        } else {
            ext = NULL;
        }

        if (ext && ext[0]) {
            if (ext[0] != '.') {
                snprintf(text, sizeof(text), ".%s", ext);
            } else {
                strncpy(text, ext, sizeof(text) - 1);
                text[sizeof(text) - 1] = 0;
            }
            if ((file->tempOnDisk & stage) && !(optsCmdLine.stages & CmdLineStageMask_Dp)) {
                env = getenv("TEMP");
                if (!env || !env[0] || OS_MakePathSpec(NULL, env, &tmppath)) {
                    env = getenv("TMP");
                }
                if (!env || !env[0] || OS_MakePathSpec(NULL, env, &tmppath)) {
                    env = getenv("TMPDIR");
                }
                if (!env || !env[0] || OS_MakePathSpec(NULL, env, &tmppath)) {
                    env = ".";
                }
                snprintf(
                    tmpname,
                    sizeof(tmpname),
                    "%s%c%s",
                    env,
                    OS_PATHSEP,
                    OS_GetFileNamePtr(file->srcfilename)
                );
                inname = tmpname;
            } else if (!optsCompiler.relPathInOutputDir) {
                inname = OS_GetFileNamePtr(file->srcfilename);
            } else {
                inname = file->srcfilename;
            }

            fnptr = OS_GetFileNamePtr(inname);
            extptr = strrchr(fnptr, '.');
            if (!extptr || ext[0] == '.') {
                extptr = fnptr + strlen(fnptr);
            }

            if (extptr - fnptr + strlen(text) >= sizeof(text)) {
                extptr = &fnptr[sizeof(text) - 1] - strlen(text);
            }

            snprintf(targetoutfilename, 260, "%.*s%s", extptr - inname, inname, text);
        } else {
            *targetoutfilename = 0;
            file->writeToDisk &= ~stage;
            file->tempOnDisk &= ~stage;
        }
    }

    if (stage & (file->writeToDisk | file->tempOnDisk)) {
        err = OS_MakeSpecWithPath(
            &gTarg->outputDirectory,
            targetoutfilename,
            !optsCompiler.relPathInOutputDir,
            &file->outfss
        );
        if (!err) {
            if (OS_EqualSpec(&file->srcfss, &file->outfss)) {
                if (specifiedName) {
                    CLReportError(CLStr102, targetoutfilename);
                    file->writeToDisk &= ~stage;
                    file->tempOnDisk &= ~stage;
                    return FALSE;
                }
                file->writeToDisk &= ~stage;
                file->tempOnDisk &= ~stage;
            }
        } else {
            CLReportOSError(
                CLStr9,
                err,
                targetoutfilename,
                OS_PathSpecToString(&gTarg->outputDirectory, STSbuf, sizeof(STSbuf))
            );
        }
        return err == 0;
    }

    return TRUE;
}
#endif

static int SyntaxCheckFile(File* file) {
    if (!SendCompilerRequest(file->compiler, file, 0)) {
        return FALSE;
    }
    return TRUE;
}

#if MWCCPSP
// 0x41CE80: OutputTextData handles the temporary file and the error
static int PreprocessFile(File* file) {
    const CWObjectFlags* cof;

    cof = Plugin_CL_GetObjectFlags(file->compiler);
    if (!(file->dropinflags & kCanpreprocess)) {
        CLReportWarning(CLStr53, Plugin_GetDropInName(file->compiler), file->srcfilename);
        return TRUE;
    }

    if (!SendCompilerRequest(file->compiler, file, CmdLineStageMask_Pp)) {
        return FALSE;
    }

    if (!GetOutputFile(file, CmdLineStageMask_Pp)) {
        return FALSE;
    }

    return OutputTextData(
        file,
        CmdLineStageMask_Pp,
        optsCompiler.ppFileCreator ? optsCompiler.ppFileCreator : cof->ppFileCreator,
        optsCompiler.ppFileType ? optsCompiler.ppFileType : cof->ppFileType
    );
}
#else
static int PreprocessFile(File* file) {
    const CWObjectFlags* cof;

    if (!(file->dropinflags & kCanpreprocess)) {
        CLReportWarning(CLStr53, Plugin_GetDropInName(file->compiler), file->srcfilename);
        return TRUE;
    }

    if (!SendCompilerRequest(file->compiler, file, CmdLineStageMask_Pp)) {
        return FALSE;
    }

    if (!GetOutputFile(file, CmdLineStageMask_Pp)) {
        return FALSE;
    }

    if (file->textdata) {
        cof = Plugin_CL_GetObjectFlags(file->compiler);
        return OutputTextData(
            file,
            CmdLineStageMask_Pp,
            optsCompiler.ppFileCreator ? optsCompiler.ppFileCreator : cof->ppFileCreator,
            optsCompiler.ppFileType ? optsCompiler.ppFileType : cof->ppFileType
        );
    }

    CLReportError(CLStr96, "preprocessing", file->srcfilename);
    return FALSE;
}
#endif

static int DependencyMapFile(File* file, Boolean compileifnecessary) {
    OSHandle mf;
    const CWObjectFlags* cof;

    cof = Plugin_CL_GetObjectFlags(file->compiler);
    if (!(optsCmdLine.stages & (CmdLineStageMask_Pp | CmdLineStageMask_Cg)) && compileifnecessary) {
        if (!SendCompilerRequest(file->compiler, file, CmdLineStageMask_Dp)) {
            return FALSE;
        }
    }

    if (!GetOutputFile(file, CmdLineStageMask_Cg)) {
        return FALSE;
    }

#if MWCCPSP
    {
        OSHandle* h = xmalloc(NULL, sizeof(OSHandle));
        Deps_ListDependencies(&gTarg->incls, file, h);
        file->textdata = h;
    }
#else
    Deps_ListDependencies(&gTarg->incls, file, &mf);
    file->textdata = OS_CreateMacHandle(&mf);
#endif

    if (!GetOutputFile(file, CmdLineStageMask_Dp)) {
        return FALSE;
    }

    if (OutputTextData(
            file,
            CmdLineStageMask_Dp,
            optsCompiler.depFileCreator ? optsCompiler.depFileCreator : cof->depFileCreator,
            optsCompiler.depFileType ? optsCompiler.depFileType : cof->depFileType
        )) {
        return TRUE;
    } else {
        return FALSE;
    }
}

static int RecordBrowseInfo(File* file) {
    const CWObjectFlags* cof;

    if (!file->recordbrowseinfo) {
        return TRUE;
    }

    if (!file->browsedata) {
        CLReportError(
            CLStr101,
            Plugin_GetDropInName(file->compiler),
            OS_SpecToStringRelative(&file->srcfss, NULL, STSbuf, sizeof(STSbuf))
        );
        return FALSE;
    }

    if ((optsCmdLine.toDisk & CmdLineStageMask_Cg) && optsCompiler.browserEnabled) {
        cof = Plugin_CL_GetObjectFlags(file->compiler);
        if (!WriteBrowseData(
                file,
                optsCompiler.brsFileCreator ? optsCompiler.brsFileCreator : cof->brsFileCreator,
                optsCompiler.brsFileType ? optsCompiler.brsFileType : cof->brsFileType
            )) {
            return FALSE;
        }
    }

    return TRUE;
}

static int RecordObjectData(File* file) {
    const CWObjectFlags* cof = Plugin_CL_GetObjectFlags(file->compiler);

#if MWCCPS2
    if (file->objectOnDisk) {
        DO_INTERNAL_ERROR(798, "Object data is flushed");
    }
    if (file->objectdata == NULL && !file->objectOnDisk) {
        return TRUE;
    }
#else
    if (!file->objectdata) {
        return TRUE;
    }
#endif

    if (!WriteObjectFile(
            file,
            optsCompiler.objFileCreator ? optsCompiler.objFileCreator : cof->objFileCreator,
            optsCompiler.objFileType ? optsCompiler.objFileType : cof->objFileType
        )) {
        return FALSE;
    }

    return TRUE;
}

int StoreObjectFile(File* file) {
    if (file->wroteToDisk & CmdLineStageMask_Cg) {
        return TRUE;
    }

#if MWCCPS2
    if (file->objectOnDisk) {
        DO_INTERNAL_ERROR(820, "Object data flushed before storing?\n");
    }
    if (file->objectdata == NULL && file->browsedata == NULL) {
        return TRUE;
    }
#else
    if (!file->objectdata && !file->browsedata) {
        return TRUE;
    }
#endif

    if (GetOutputFile(file, CmdLineStageMask_Cg)) {
        if (!((file->writeToDisk | file->tempOnDisk) & CmdLineStageMask_Cg)) {
            return TRUE;
        }

        if (!RecordBrowseInfo(file) || !RecordObjectData(file)) {
            return FALSE;
        }

        file->wroteToDisk |= CmdLineStageMask_Cg;
        return TRUE;
    }

    return FALSE;
}

#if MWCCPSP
extern void func_0041C200(File* file); // frees the object data
#endif
#if MWCCPSP

#endif
static int CompileFile(File* file) {
    int ret = TRUE;

    if (!(file->dropinflags & kCanprecompile) &&
        ((optsCompiler.forcePrecompile == 1) || (file->mappingflags & kPrecompile))) {
        CLReportWarning(CLStr54, Plugin_GetDropInName(file->compiler), file->srcfilename);
        return TRUE;
    }

    if (optsCompiler.browserEnabled) {
        SInt16 id;
        int ret;
        char fullpath[260];
        memset(file->browseoptions, 1, sizeof(file->browseoptions));
        OS_SpecToString(&file->srcfss, fullpath, sizeof(fullpath));
        ret = Browser_SearchAndAddFile(&clState.browseTableHandle, fullpath, &id);
        if (!ret) {
            return FALSE;
        }
        file->recordbrowseinfo = ret < 0;
        file->browseFileID = id;
        if (optsCmdLine.verbose > 1) {
            CLReport(CLStr22, file->srcfilename, file->browseFileID);
        }
    } else {
        memset(file->browseoptions, 0, sizeof(file->browseoptions));
        file->recordbrowseinfo = FALSE;
        file->recordbrowseinfo = FALSE;
        file->browseFileID = 0;
    }

#if MWCCPSP
    // 3.0.1: the plugin's kind says what it made; an object it wrote itself
    // (or one already written) counts; outside -c the object data is freed
#if MWCCPS2
    if (!SendCompilerRequest(file->compiler, file, CmdLineStageMask_Cg)) {
        ret = FALSE;
    } else {
        if (file->dropinflags & kGeneratesrsrcs) {
            file->hasresources = TRUE;
        } else if (file->dropinflags & kGeneratescode) {
            file->hasobjectcode = TRUE;
        }
        if ((!file->hasobjectcode ||
             (!file->objectdata &&
              !file->objectOnDisk &&
              !(file->wroteToDisk & CmdLineStageMask_Cg))) &&
            (file->dropinflags & kGeneratescode) &&
            (file->objectUsage & CmdLineStageMask_Cg)) {
            CLReportError(CLStr96, "compiling", file->srcfilename);
            ret = FALSE;
        } else if (optsCmdLine.state == OptsCmdLineState_Compile) {
            if (!StoreObjectFile(file)) {
                ret = FALSE;
            }
        } else {
            func_0041C200(file);
        }
    }
    return ret;
#else
    if (!SendCompilerRequest(file->compiler, file, CmdLineStageMask_Cg)) {
        return FALSE;
    }
    if (file->dropinflags & kGeneratesrsrcs) {
        file->hasresources = TRUE;
    } else if (file->dropinflags & kGeneratescode) {
        file->hasobjectcode = TRUE;
    }
    if ((
            !file->hasobjectcode ||
            (!file->objectdata && !file->objectOnDisk && !(file->wroteToDisk & CmdLineStageMask_Cg))
        ) &&
        (file->dropinflags & kGeneratescode) &&
        (file->objectUsage & CmdLineStageMask_Cg)) {
        CLReportError(CLStr96, "compiling", file->srcfilename);
        ret = FALSE;
    } else if (optsCmdLine.state == OptsCmdLineState_Compile) {
        if (!StoreObjectFile(file)) {
            ret = FALSE;
        }
    } else {
        func_0041C200(file);
    }
    return ret;
#endif
#endif
    if (!SendCompilerRequest(file->compiler, file, CmdLineStageMask_Cg)) {
        ret = FALSE;
    } else {
        if ((!file->hasobjectcode || !file->objectdata) &&
            (file->dropinflags & kGeneratescode) &&
            (file->objectUsage & CmdLineStageMask_Cg)) {
            CLReportError(CLStr96, "compiling", file->srcfilename);
            ret = FALSE;
        } else if (optsCmdLine.state == OptsCmdLineState_Compile && !StoreObjectFile(file)) {
            ret = FALSE;
        }
    }

    return ret;
}

static int DisassembleWithLinker(File* file, Plugin* linker, SInt32 linkerDropinFlags) {
    char filename[260];
    int ret;

    if (file->writeToDisk & CmdLineStageMask_Ds) {
        OS_SpecToString(&file->outfss, filename, sizeof(filename));
        ret = ExecuteLinker(
            linker,
            linkerDropinFlags,
            file,
            (optsCmdLine.toDisk & CmdLineStageMask_Ds) ? NULL : filename,
            NULL
        );
        file->wroteToDisk |= CmdLineStageMask_Ds;
        return ret;
    }

    return ExecuteLinker(linker, linkerDropinFlags, file, NULL, NULL);
}

static int DisassembleFile(File* file) {
    if (!file->hasobjectcode && !file->hasresources) {
        CLReportError(CLStr56, file->srcfilename);
        return FALSE;
    }

    if (!GetOutputFile(file, CmdLineStageMask_Ds)) {
        return FALSE;
    }

    if (file->dropinflags & kCandisassemble) {
        if (!SendDisassemblerRequest(file->compiler, file)) {
            return FALSE;
        }

        if (file->textdata) {
            const CWObjectFlags* cof = Plugin_CL_GetObjectFlags(file->compiler);
            return OutputTextData(
                file,
                CmdLineStageMask_Ds,
                optsCompiler.disFileCreator ? optsCompiler.disFileCreator : cof->disFileCreator,
                optsCompiler.disFileType ? optsCompiler.disFileType : cof->disFileType
            );
        }
        CLReportError(CLStr96, "disassembling", file->srcfilename);
        return FALSE;

    } else {
        if ((gTarg->linkerDropinFlags & isExecutableTool) &&
            !(gTarg->linkerDropinFlags & cantDisassemble)) {
            return DisassembleWithLinker(file, gTarg->linker, gTarg->linkerDropinFlags);
        }
        if ((gTarg->preLinkerDropinFlags & isExecutableTool) &&
            !(gTarg->preLinkerDropinFlags & cantDisassemble)) {
            return DisassembleWithLinker(file, gTarg->preLinker, gTarg->preLinkerDropinFlags);
        }

        if (gTarg->linker && !(gTarg->linkerDropinFlags & cantDisassemble)) {
            if (SendDisassemblerRequest(gTarg->linker, file)) {
#if MWCCPSP
                if (file->textdata && TEXTDATA_SIZE(file->textdata) > 0) {
#else
                if (file->textdata && GetHandleSize(file->textdata) > 0) {
#endif
                    const CWObjectFlags* cof = Plugin_CL_GetObjectFlags(file->compiler);
                    return OutputTextData(
                        file,
                        CmdLineStageMask_Ds,
                        optsCompiler.disFileCreator ? optsCompiler.disFileCreator
                                                    : cof->disFileCreator,
                        optsCompiler.disFileType ? optsCompiler.disFileType : cof->disFileType
                    );
                }
                CLReportError(CLStr96, "disassembling", file->srcfilename);
            }
            return FALSE;
        }
        if (gTarg->preLinker && !(gTarg->preLinkerDropinFlags & cantDisassemble)) {
            if (SendDisassemblerRequest(gTarg->preLinker, file)) {
#if MWCCPSP
                if (file->textdata && TEXTDATA_SIZE(file->textdata) > 0) {
#else
                if (file->textdata && GetHandleSize(file->textdata) > 0) {
#endif
                    const CWObjectFlags* cof = Plugin_CL_GetObjectFlags(file->compiler);
                    return OutputTextData(
                        file,
                        CmdLineStageMask_Ds,
                        optsCompiler.disFileCreator ? optsCompiler.disFileCreator
                                                    : cof->disFileCreator,
                        optsCompiler.disFileType ? optsCompiler.disFileType : cof->disFileType
                    );
                }
                CLReportError(CLStr96, "disassembling", file->srcfilename);
            }
            return FALSE;
        }

        if (gTarg->linker) {
            CLReportError(
                CLStr58,
                Plugin_GetDropInName(gTarg->linker),
                Plugin_GetDropInName(file->compiler),
                file->srcfilename
            );
        } else {
            CLReportError(CLStr57, Plugin_GetDropInName(file->compiler), file->srcfilename);
        }

        return FALSE;
    }
}

static int CompileEntry(File* file, Boolean* compiled) {
    UInt32 beginWork;
    UInt32 endWork;
    struct BuildInfo* tinfo = &gTarg->info;

    *compiled = FALSE;

    if (optsCmdLine.dryRun) {
        *compiled = TRUE;
        return TRUE;
    }

    if (!(file->sourceUsage & CmdLineStageMask_Pp)) {
        if (file->mappingflags & kRsrcfile) {
            *compiled = TRUE;
#if MWCCPS2
            // b210: a resource file is its own output
            file->outfss = file->srcfss;
            if (!file->objectdata) {
                file->objectOnDisk = TRUE;
            }
#endif
        }
        return TRUE;
    }

#if MWCCPS2
    OS_ASSERT(1169, file->compiler);
#else
    OS_ASSERT(835, file->compiler);
#endif

    *compiled = TRUE;
    beginWork = OS_GetMilliseconds();
    Browser_Initialize(&clState.browseTableHandle);
    Deps_ChangeSpecialAccessPath(&file->srcfss, TRUE);

    if ((optsCmdLine.stages == 0) && !SyntaxCheckFile(file)) {
        return FALSE;
    }
    if ((optsCmdLine.stages & CmdLineStageMask_Pp) && !PreprocessFile(file)) {
        return FALSE;
    }
    if ((optsCmdLine.stages & (CmdLineStageMask_Cg | CmdLineStageMask_Ds)) && !CompileFile(file)) {
        return FALSE;
    }
    if ((optsCmdLine.stages & CmdLineStageMask_Dp) && !DependencyMapFile(file, TRUE)) {
        return FALSE;
    }
    if ((optsCmdLine.stages & CmdLineStageMask_Ds) && !DisassembleFile(file)) {
        return FALSE;
    }

    if (optsCmdLine.verbose && (optsCmdLine.stages & CmdLineStageMask_Cg)) {
        int cn;
        int dn;
        int un;
        const char* cun;
        const char* dun;
        const char* uun;
        CLReport(CLStr25, file->compiledlines);
        tinfo->linesCompiled += file->compiledlines;
        set_bytes(file->codesize, &cn, &cun);
        set_bytes(file->idatasize, &dn, &dun);
        set_bytes(file->udatasize, &un, &uun);
        CLReport(CLStr26, cn, cun, dn, dun, un, uun);
    }
    tinfo->codeSize += file->codesize;
    tinfo->iDataSize += file->idatasize;
    tinfo->uDataSize += file->udatasize;

    Browser_Terminate(&clState.browseTableHandle);
    endWork = OS_GetMilliseconds();

    if (optsCmdLine.timeWorking) {
        if (optsCmdLine.verbose) {
            CLReport(CLStr24, (endWork - beginWork) / 1000.0, "compile", "", "", "");
        } else {
            CLReport(
                CLStr24, (endWork - beginWork) / 1000.0, "compile", "'", file->srcfilename, "'"
            );
        }
    }

    return TRUE;
}

static void set_bytes(UInt32 size, int* amount, const char** unit) {
    *amount = size;
    *unit = "bytes";
}

static void DumpFileAndPathInfo(void) {
    int i;

    if (clState.plugintype == CWDROPINCOMPILERTYPE) {
        CLReport(CLStr84, "User include search paths ([d] = default, [r] = recursive)");
    } else {
        CLReport(CLStr84, "Library search paths ([d] = default, [r] = recursive)");
    }

    if (!Paths_Count(&gTarg->userPaths) && clState.plugintype == CWDROPINCOMPILERTYPE) {
        CLReport(CLStr85, "(none)", "");
    } else {
        for (i = 0; i < Paths_Count(&gTarg->userPaths); i++) {
            Path* path = Paths_GetPath(&gTarg->userPaths, i);
#if MWCCPSP
            OS_ASSERT(1358, path != NULL);
#else
            OS_ASSERT(1024, path != NULL);
#endif

            CLReport(
                CLStr85,
                OS_PathSpecToString(path->spec, STSbuf, sizeof(STSbuf)),
                ((path->flags & 2) && path->recursive) ? " [d] [r]"
                    : (path->flags & 2)                ? " [d]"
                    : path->recursive                  ? " [r]"
                                                       : ""
            );

            if (path->recursive && optsCmdLine.verbose > 2) {
                UInt16 j;
                for (j = 0; j < Paths_Count(path->recursive); j++) {
                    Path* sub = Paths_GetPath(path->recursive, j);
#if MWCCPSP
                    OS_ASSERT(1373, sub);
#else
                    OS_ASSERT(1039, sub);
#endif
                    CLReport(CLStr85, "\t", OS_PathSpecToString(sub->spec, STSbuf, sizeof(STSbuf)));
                }
            }
        }
    }

    if (clState.plugintype == CWDROPINCOMPILERTYPE) {
        CLReport(CLStr84, "System include search paths ([d] = default, [r] = recursive)");
    }

    if (!Paths_Count(&gTarg->sysPaths)) {
        CLReport(CLStr85, "(none)", "");
    }

    for (i = 0; i < Paths_Count(&gTarg->sysPaths); i++) {
        Path* path = Paths_GetPath(&gTarg->sysPaths, i);
#if MWCCPSP
        OS_ASSERT(1388, path != NULL);
#else
        OS_ASSERT(1054, path != NULL);
#endif

        CLReport(
            CLStr85,
            OS_PathSpecToString(path->spec, STSbuf, sizeof(STSbuf)),
            ((path->flags & 2) && path->recursive) ? " [d] [r]"
                : (path->flags & 2)                ? " [d]"
                : path->recursive                  ? " [r]"
                                                   : ""
        );

        if (path->recursive && optsCmdLine.verbose > 2) {
            UInt16 j;
            for (j = 0; j < Paths_Count(path->recursive); j++) {
                Path* sub = Paths_GetPath(path->recursive, j);
#if MWCCPSP
                OS_ASSERT(1403, sub);
#else
                OS_ASSERT(1069, sub);
#endif
                CLReport(CLStr85, "\t", OS_PathSpecToString(sub->spec, STSbuf, sizeof(STSbuf)));
            }
        }
    }

    if (clState.plugintype == CWDROPINCOMPILERTYPE) {
        CLReport(CLStr84, "Files in project (relative to CWD)");
    } else {
        CLReport(CLStr84, "Link order for project (relative to CWD)");
    }

    for (i = 0; i < Files_Count(&gTarg->files); i++) {
        File* file = Files_GetFile(&gTarg->files, i);
        if (!VFiles_Find(gTarg->virtualFiles, file->srcfilename)) {
            CLReport(
                CLStr85, OS_SpecToStringRelative(&file->srcfss, NULL, STSbuf, sizeof(STSbuf)), ""
            );
        } else {
            CLReport(CLStr85, file->srcfilename, "\t(virtual file)");
        }
    }
}

int CompileFilesInProject(void) {
    struct BuildInfo* tinfo;
    SInt32 index;
    SInt32 startTime;
    SInt32 endTime;
    int ignored;
    int compiled;
    Boolean failed;

    failed = FALSE;
    startTime = OS_GetMilliseconds();
    InitializeIncludeCache();
    if (optsCmdLine.verbose > 1) {
        DumpFileAndPathInfo();
    }

    if (!SendTargetInfoRequest(gTarg, gTarg->linker, gTarg->linkerDropinFlags)) {
        return Result_Failed;
    }

    ignored = 0;
    compiled = 0;
    for (index = 0; index < Files_Count(&gTarg->files); index++) {
        int ret;
        File* file;
        Boolean wascompiled;

        file = Files_GetFile(&gTarg->files, index);
        if (!file->outfileowner) {
            file->outfileowner = CmdLineStageMask_Cg;
        }
        ret = CompileEntry(file, &wascompiled);
        if (ret) {
            if (wascompiled) {
                compiled++;
            } else {
                ignored++;
            }
            if (CheckForUserBreak()) {
                return Result_Cancelled;
            }
        }

        if (!ret) {
            if (optsCompiler.noFail) {
                failed = TRUE;
                clState.countWarnings = 0;
                clState.countErrors = 0;
                clState.withholdWarnings = FALSE;
                clState.withholdErrors = FALSE;
            } else {
                return CheckForUserBreak() ? Result_Cancelled : Result_Failed;
            }
        }
    }

    if (optsCmdLine.stages & CmdLineStageMask_Dp) {
        for (index = 0; index < Files_Count(&gTarg->pchs); index++) {
            int ret;
            File* file;

            file = Files_GetFile(&gTarg->pchs, index);
            ret = DependencyMapFile(file, FALSE);
            if (!ret) {
                if (optsCompiler.noFail) {
                    failed = TRUE;
                    clState.countWarnings = 0;
                    clState.countErrors = 0;
                    clState.withholdWarnings = FALSE;
                    clState.withholdErrors = FALSE;
                } else {
                    return CheckForUserBreak() ? Result_Cancelled : Result_Failed;
                }
            }
        }
    }

    CleanupIncludeCache();

    tinfo = &gTarg->info;
    if (optsCmdLine.verbose && compiled > 1) {
        int cn;
        int dn;
        int un;
        const char* cun;
        const char* dun;
        const char* uun;
        set_bytes(tinfo->codeSize, &cn, &cun);
        set_bytes(tinfo->iDataSize, &dn, &dun);
        set_bytes(tinfo->uDataSize, &un, &uun);
        CLReport(CLStr27, cn, cun, dn, dun, un, uun);
    }

    endTime = OS_GetMilliseconds();

    if (optsCmdLine.timeWorking) {
        CLReport(CLStr24, (endTime - startTime) / 1000.0, "compile", "", "", "project");
    }

    if (ignored > 0 && compiled == 0 && (gTarg->preLinker || gTarg->linker)) {
#if MWCCPS2
        CLReportWarning(CLStr29); // b210 (0x41DF50 +0x1BB): a warning
#else
        CLReportError(CLStr29);
#endif
    }

    if (failed) {
        return Result_Failed;
    }
    return CheckForUserBreak() ? Result_Cancelled : Result_Success;
}

int LinkProject(void) {
    SInt32 startTime;
    SInt32 endTime;

    startTime = OS_GetMilliseconds();
    if (!SendTargetInfoRequest(gTarg, gTarg->linker, gTarg->linkerDropinFlags)) {
        return Result_Failed;
    }
    if (!SetupTemporaries()) {
        return Result_Failed;
    }

    if (gTarg->preLinker &&
        optsLinker.callPreLinker &&
        optsCmdLine.state == OptsCmdLineState_CompileAndLink) {
        SInt32 subStart;
        SInt32 subEnd;

        subStart = OS_GetMilliseconds();
        if (gTarg->preLinkerDropinFlags & isExecutableTool) {
            if (!ExecuteLinker(gTarg->preLinker, gTarg->preLinkerDropinFlags, NULL, NULL, NULL)) {
                return Result_Failed;
            }
        } else if (!optsCmdLine.dryRun) {
            if (!SendLinkerRequest(
                    gTarg->preLinker, gTarg->preLinkerDropinFlags, gTarg->targetinfo
                )) {
                return CheckForUserBreak() ? Result_Cancelled : Result_Failed;
            }
        }
        subEnd = OS_GetMilliseconds();

        if (optsCmdLine.timeWorking) {
            CLReport(CLStr24, (subEnd - subStart) / 1000.0, "prelink project", "", "", "");
        }
    }

    if (gTarg->linker &&
        optsLinker.callLinker &&
        optsCmdLine.state == OptsCmdLineState_CompileAndLink) {
        SInt32 subStart;
        SInt32 subEnd;

        subStart = OS_GetMilliseconds();
        if (gTarg->linkerDropinFlags & isExecutableTool) {
            if (!ExecuteLinker(gTarg->linker, gTarg->linkerDropinFlags, NULL, NULL, NULL)) {
                return Result_Failed;
            }
        } else if (!optsCmdLine.dryRun) {
            if (!SendLinkerRequest(gTarg->linker, gTarg->linkerDropinFlags, gTarg->targetinfo)) {
                return CheckForUserBreak() ? Result_Cancelled : Result_Failed;
            }
        }
        subEnd = OS_GetMilliseconds();

        if (optsCmdLine.timeWorking) {
            CLReport(CLStr24, (subEnd - subStart) / 1000.0, "link project", "", "", "");
        }
    }

    if (gTarg->postLinker &&
        optsLinker.callPostLinker &&
        optsCmdLine.state == OptsCmdLineState_CompileAndLink) {
        SInt32 subStart;
        SInt32 subEnd;
        FSSpec fss;
        OSSpec outfile;

        subStart = OS_GetMilliseconds();
        if (gTarg->postLinkerDropinFlags & isExecutableTool) {
            if (!ExecuteLinker(gTarg->postLinker, gTarg->postLinkerDropinFlags, NULL, NULL, NULL)) {
                return Result_Failed;
            }
        } else if (!optsCmdLine.dryRun) {
            if (!SendTargetInfoRequest(gTarg, gTarg->linker, gTarg->linkerDropinFlags)) {
                return Result_Failed;
            }
            if (gTarg->targetinfo->outputType == linkOutputFile) {
#if MWCCPSP
                outfile = gTarg->targetinfo->outfile;
#else
                fss = gTarg->targetinfo->outfile;
#endif
            }
            if (gTarg->targetinfo->outputType != linkOutputNone) {
                if (!SendLinkerRequest(
                        gTarg->postLinker, gTarg->postLinkerDropinFlags, gTarg->targetinfo
                    )) {
                    return CheckForUserBreak() ? Result_Cancelled : Result_Failed;
                }
                if (!SendTargetInfoRequest(
                        gTarg, gTarg->postLinker, gTarg->postLinkerDropinFlags
                    )) {
                    return Result_Failed;
                }
            }
        }
        subEnd = OS_GetMilliseconds();

        if (optsCmdLine.timeWorking) {
            CLReport(CLStr24, (subEnd - subStart) / 1000.0, "postlink project", "", "", "");
        }

        if (!optsLinker.keepLinkerOutput && gTarg->targetinfo->outputType == linkOutputFile) {
#if MWCCPSP
            if (optsCmdLine.verbose > 1) {
                CLReport(CLStr19, OS_SpecToString(&outfile, STSbuf, sizeof(STSbuf)));
            }
            OS_Delete(&outfile);
#else
            if (optsCmdLine.verbose > 1) {
                OS_FSSpec_To_OSSpec(&fss, &outfile);
                CLReport(CLStr19, OS_SpecToString(&outfile, STSbuf, sizeof(STSbuf)));
            }
            FSpDelete(&fss);
#endif
        }
    }

    if (!DeleteTemporaries()) {
        return Result_Failed;
    }

    endTime = OS_GetMilliseconds();

    if (optsCmdLine.timeWorking) {
        CLReport(CLStr24, (endTime - startTime) / 1000.0, "finish link stage", "", "", "");
    }

    return Result_Success;
}
