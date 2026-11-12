#include "driver/CLDropinCallbacks_V10.h"
#include "driver/CLAccessPaths.h"
#include "driver/CLBrowser.h"
#include "driver/CLDependencies.h"
#include "driver/CLErrors.h"
#include "driver/CLFiles.h"
#include "driver/CLIncludeFileCache.h"
#include "driver/CLIO.h"
#include "driver/CLLicenses.h"
#include "driver/CLLoadAndCache.h"
#include "driver/CLMain.h"
#include "driver/CLOverlays.h"
#include "driver/CLPlugins.h"
#include "driver/CLPrefs.h"
#include "driver/CLSegs.h"
#include "driver/CLTarg.h"
#include "driver/cmdline.h"
#include "driver/CWPlugins.h"
#include "driver/CWPluginsPrivate.h"
#include "driver/Files.h"
#include "driver/Generic.h"
#include "driver/MacFileTypes.h"
#include "driver/MacSpecs.h"
#include "driver/Memory.h"
#include "driver/MemUtils.h"
#include "driver/MsDos.h"
#include "driver/oslib.h"
#include "driver/pref_structs.h"
#include "driver/StringUtils.h"
#include <stdio.h>
#if MWCCPSP_NATIVE
extern "C" FILE* win32_fopen(const char* path, const char* mode);
#define fopen win32_fopen
#endif
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern "C" {

static void ANSI_To_Mac_GMT_Time(time_t sectm, UInt32* secs) {
    struct tm* tmrec;
    int years;
    int ydays;

    tmrec = localtime(&sectm);
    years = tmrec->tm_year - 4;

    ydays = (years * 365) + ((years + 3) / 4) - ((years + 4) / 100) + ((years - 296) / 400);

    *secs = ((ydays + tmrec->tm_yday) * 86400) + (tmrec->tm_hour * 3600) + (tmrec->tm_min * 60) +
        (tmrec->tm_sec);
}

static void Mac_To_ANSI_Time(UInt32 secs, time_t* sectm) {
    static int monthdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    struct tm tmrec;
    int month;

    memset(&tmrec, 0, sizeof(tmrec));
    tmrec.tm_sec = secs % 60;
    tmrec.tm_min = (secs / 60) % 60;
    tmrec.tm_hour = (secs / 3600) % 24;
    tmrec.tm_yday = (secs / 86400) % 365;
    tmrec.tm_year = (secs / 31536000) + 4;
    tmrec.tm_yday -=
        ((((secs / 31536000) + 3) / 4) - (((secs / 31536000) + 4) / 100) +
         ((SInt32)((secs / 31536000) - 296) / 400));

    if ((tmrec.tm_year % 4) && ((tmrec.tm_year % 100) || !(tmrec.tm_year % 400))) {
        monthdays[1] = 28;
    } else {
        monthdays[1] = 29;
    }

    for (month = 0; tmrec.tm_yday >= monthdays[month]; month++) {
        tmrec.tm_yday -= monthdays[month];
    }
    tmrec.tm_mon = month;
    tmrec.tm_mday = tmrec.tm_yday + 1;

    *sectm = mktime(&tmrec);
    if (month >= 4 && month < 10) {
        *sectm += 3600;
    }
}

CW_CALLBACK UCBGetFileInfo(
    CWPluginContext context,
    SInt32 whichfile,
    Boolean checkFileLocation,
    CWProjectFileInfo* fileinfo
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBGetFileInfo");
    }

    File* file;
    time_t tmptime;
    SInt16 id;
    int ret;
    char fullpath[260];

    file = Files_GetFile(&gTarg->files, whichfile);
    if (!file) {
        return cwErrUnknownFile;
    }

    memset(fileinfo, 0, sizeof(CWProjectFileInfo));
#if MWCCPSP
    SHELL_SPEC_FROM_OSSPEC(&file->srcfss, &fileinfo->filespec);
#else
    OS_OSSpec_To_FSSpec(&file->srcfss, &fileinfo->filespec);
#endif
    OS_TimeToMac(file->srcmoddate, &fileinfo->moddate);
    Mac_To_ANSI_Time(fileinfo->moddate, &tmptime);
    ANSI_To_Mac_GMT_Time(tmptime, &fileinfo->moddate);

    fileinfo->segment = file->segnum;
    fileinfo->hasobjectcode = file->hasobjectcode;
    fileinfo->hasresources = file->hasresources;
    fileinfo->isresourcefile = file->isresourcefile;
    fileinfo->weakimport = file->weakimport;
    fileinfo->initbefore = file->initbefore;
    fileinfo->mergeintooutput = file->mergeintooutput;
    fileinfo->reserved = FALSE;
    fileinfo->gendebug = file->gendebug;
    OS_TimeToMac(file->outmoddate, &fileinfo->objmoddate);

    if (file->compiler) {
        strncpy(fileinfo->dropinname, Plugin_GetDropInName(file->compiler), 31);
        fileinfo->dropinname[31] = 0;
    } else {
        fileinfo->dropinname[0] = 0;
    }

    fileinfo->fileID = file->browseFileID;
    fileinfo->recordbrowseinfo = file->recordbrowseinfo;
    fileinfo->filetype = file->filetype;
    fileinfo->filecreator = file->filecreator;

    if (optsCompiler.browserEnabled) {
        ret = Browser_SearchFile(
            &clState.browseTableHandle,
            OS_SpecToString(&file->srcfss, fullpath, sizeof(fullpath)),
            &id
        );
        if (ret) {
            fileinfo->recordbrowseinfo = TRUE;
            fileinfo->fileID = id;
        } else {
            fileinfo->recordbrowseinfo = FALSE;
            fileinfo->fileID = id;
        }
    } else {
        fileinfo->recordbrowseinfo = FALSE;
        fileinfo->fileID = 0;
    }
    fileinfo->unitdatadependencytag = 0;
    fileinfo->hasunitdata = FALSE;
    return cwNoErr;
}

#if MWCCPSP
// 0x428100: a spec for a file name with no directory
extern "C" void func_00428100(OSSpec* spec, const char* name) {
    spec->path.s[0] = 0;
    OS_MakeNameSpec(name, &spec->name);
}

// 0x428120
static Boolean FindAndLoad_VirtualFile(
    CWPluginContext context,
    const char* filename,
    ShellFileInfo* fileinfo,
    OSSpec* spec,
    Boolean* loaded
) {
    VFile* vf;
    char* fnameptr;

    fnameptr = OS_GetFileNamePtr(const_cast<char*>(filename));
    vf = VFiles_Find(gTarg->virtualFiles, fnameptr);
    if (vf) {
        if (!fileinfo->suppressload) {
            CopyFileText(
                &vf->data, const_cast<char**>(&fileinfo->filedata), &fileinfo->filedatalength
            );
            *loaded = TRUE;
        }
        fileinfo->filedatatype = cwFileTypeText;
        fileinfo->fileID = 0;
        fileinfo->alreadyincluded = FALSE;
        fileinfo->recordbrowseinfo = FALSE;
        func_00428100(&fileinfo->filespec, fnameptr);
        OS_MakeFileSpec(fnameptr, spec);
        if (optsCompiler.printHeaderNames) {
            CLPrint("%s\n", fnameptr);
        }
        return TRUE;
    }
    return FALSE;
}

// 0x4281D0
static Boolean FindAndLoad_AccessPathFile(
    CWPluginContext context,
    const char* filename,
    ShellFileInfo* fileinfo,
    OSSpec* spec,
    Boolean* loaded
) {
    SInt32 incl;
    File* depfile;
    SInt16 id;
    int ret;
    char fullpath[260];

    if (Incls_FindFileInPaths(
            static_cast<shellContextType*>(context->shellContext)->pluginAPI,
            &gTarg->incls,
            const_cast<char*>(filename),
            fileinfo->fullsearch || optsCompiler.noSysPath,
            spec,
            &incl
        )) {
        if (context->pluginType == CWDROPINCOMPILERTYPE) {
            depfile = Files_GetFile(
                &gTarg->files,
                (fileinfo->isdependentoffile < 0)
                    ? static_cast<CWCompilerLinkerContext*>(context)->whichfile
                    : fileinfo->isdependentoffile
            );
            if (depfile) {
                Deps_AddDependency(
                    &depfile->deps,
                    incl,
                    NULL,
                    FALSE,
                    fileinfo->dependencyType,
                    &fileinfo->alreadyincluded
                );
            }

            if (optsCompiler.browserEnabled) {
                ret = Browser_SearchAndAddFile(
                    &clState.browseTableHandle,
                    OS_SpecToString(spec, fullpath, sizeof(fullpath)),
                    &id
                );
                if (ret == 0) {
                    return 2;
                } else if (ret < 0) {
                    fileinfo->recordbrowseinfo = TRUE;
                    fileinfo->fileID = id;
                } else {
                    fileinfo->recordbrowseinfo = FALSE;
                    fileinfo->fileID = id;
                }
            } else {
                fileinfo->recordbrowseinfo = FALSE;
                fileinfo->fileID = 0;
            }
        }

        fileinfo->filespec = *spec;
        return TRUE;
    }

    return FALSE;
}

// 0x428340
static Boolean FindAndLoad_MissingFile(
    CWPluginContext context,
    const char* filename,
    ShellFileInfo* fileinfo,
    OSSpec* spec,
    Boolean* loaded
) {
    File* file;
    CWCompilerLinkerContext* c;

    if (optsCompiler.ignoreMissingFiles) {
        if (context->pluginType == CWDROPINCOMPILERTYPE ||
            context->pluginType == CWDROPINLINKERTYPE) {
            c = static_cast<CWCompilerLinkerContext*>(context);
        } else {
            return FALSE;
        }

        file = Files_GetFile(&gTarg->files, c->whichfile);
        OS_ASSERT(504, file != NULL);

        OS_MakeSpecWithPath(&file->srcfss.path, filename, TRUE, spec);
        Deps_AddDependency(
            &file->deps,
            -1,
            spec,
            !(fileinfo->fullsearch || optsCompiler.noSysPath),
            fileinfo->dependencyType,
            &fileinfo->alreadyincluded
        );

        fileinfo->fileID = 0;
        fileinfo->recordbrowseinfo = FALSE;
        if (!fileinfo->suppressload) {
            fileinfo->filedata = xstrdup("");
            fileinfo->filedatalength = 0;
            fileinfo->filedatatype = cwFileTypeText;
            *loaded = TRUE;
        } else {
            fileinfo->filedata = NULL;
            fileinfo->filedatalength = 0;
            fileinfo->filedatatype = cwFileTypeUnknown;
        }

        fileinfo->filespec = *spec;
        return TRUE;
    }

    return FALSE;
}

// 0x428470: the shell's file info; the found file is passed on as an OSSpec
CW_CALLBACK UCBFindAndLoadFile(
    CWPluginContext context, const char* infilename, CWFileInfo* cwfileinfo
) {
    ShellFileInfo* fileinfo = reinterpret_cast<ShellFileInfo*>(cwfileinfo);
    OSSpec spec;
    Boolean loaded;
    char filename[260];
    char browseinfo[64];

    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBFindAndLoadFile");
    }

    ShowWorking(8);
    if (CheckForUserBreak()) {
        return cwErrUserCanceled;
    }

    if (optsCompiler.canonicalIncludes) {
        strcpy(filename, infilename);
    } else if (OS_CanonPath(infilename, filename)) {
        return cwErrInvalidParameter;
    }

    loaded = FALSE;
    if (!FindAndLoad_VirtualFile(context, filename, fileinfo, &spec, &loaded) &&
        !FindAndLoad_AccessPathFile(context, filename, fileinfo, &spec, &loaded) &&
        !FindAndLoad_MissingFile(context, filename, fileinfo, &spec, &loaded)) {
        return cwErrFileNotFound;
    }

    if (optsCompiler.printHeaderNames) {
        CLPrint("%s\n", OS_SpecToStringRelative(&spec, NULL, STSbuf, sizeof(STSbuf)));
    }

    if (context->pluginType == CWDROPINCOMPILERTYPE) {
        if (optsCmdLine.verbose > 2) {
            sprintf(browseinfo, " (browse fileID %d)", fileinfo->fileID);
            CLReport(
                CLStr21,
                fileinfo->alreadyincluded ? "Included" : "Including",
                OS_SpecToString(&spec, STSbuf, sizeof(STSbuf)),
                fileinfo->recordbrowseinfo ? browseinfo : ""
            );
        } else if (optsCmdLine.verbose > 1 && !fileinfo->alreadyincluded) {
            CLReport(CLStr21, "Including", OS_SpecToString(&spec, STSbuf, sizeof(STSbuf)), "");
        }
    } else {
        fileinfo->fileID = 0;
        fileinfo->recordbrowseinfo = FALSE;
        fileinfo->alreadyincluded = FALSE;
    }

    if (!loaded) {
        if (!fileinfo->suppressload) {
            // the shell spec: UCBGetFileText takes it in 3.0.1
            return UCBGetFileText(
                context,
                reinterpret_cast<const CWFileSpec*>(&fileinfo->filespec),
                &fileinfo->filedata,
                &fileinfo->filedatalength,
                &fileinfo->filedatatype
            );
        } else {
            fileinfo->filedata = NULL;
            fileinfo->filedatalength = 0;
            fileinfo->filedatatype = cwFileTypeUnknown;
            return cwNoErr;
        }
    } else {
        return cwNoErr;
    }
}
#else
static Boolean FindAndLoad_VirtualFile(
    CWPluginContext context,
    const char* filename,
    CWFileInfo* fileinfo,
    OSSpec* spec,
    Boolean* loaded
) {
    VFile* vf;
    char* fnameptr;

    fnameptr = OS_GetFileNamePtr(const_cast<char*>(filename));
    vf = VFiles_Find(gTarg->virtualFiles, fnameptr);
    if (vf) {
        if (!fileinfo->suppressload) {
            CopyFileText(
                vf->data, const_cast<char**>(&fileinfo->filedata), &fileinfo->filedatalength
            );
            *loaded = TRUE;
        }
        fileinfo->filedatatype = cwFileTypeText;
        fileinfo->fileID = 0;
        fileinfo->alreadyincluded = FALSE;
        fileinfo->recordbrowseinfo = FALSE;
        fileinfo->filespec.vRefNum = fileinfo->filespec.parID = 0;
        c2pstrcpy(fileinfo->filespec.name, fnameptr);
        OS_MakeFileSpec(fnameptr, spec);
        if (optsCompiler.printHeaderNames) {
            CLPrint("%s\n", fnameptr);
        }
        return TRUE;
    }

    return FALSE;
}

static Boolean FindAndLoad_AccessPathFile(
    CWPluginContext context,
    const char* filename,
    CWFileInfo* fileinfo,
    OSSpec* spec,
    Boolean* loaded
) {
    SInt32 incl;
    shellContextType* sc;
    File* depfile;
    SInt16 id;
    int ret;
    char fullpath[260];

    if (Incls_FindFileInPaths(
            &gTarg->incls,
            const_cast<char*>(filename),
            fileinfo->fullsearch || optsCompiler.noSysPath,
            spec,
            &incl
        )) {
        if (context->pluginType == CWDROPINCOMPILERTYPE) {
            depfile = Files_GetFile(
                &gTarg->files,
                (fileinfo->isdependentoffile < 0)
                    ? static_cast<CWCompilerLinkerContext*>(context)->whichfile
                    : fileinfo->isdependentoffile
            );
            if (depfile) {
                Deps_AddDependency(
                    &depfile->deps,
                    incl,
                    NULL,
                    FALSE,
                    fileinfo->dependencyType,
                    &fileinfo->alreadyincluded
                );
            }

            if (optsCompiler.browserEnabled) {
                ret = Browser_SearchAndAddFile(
                    &clState.browseTableHandle,
                    OS_SpecToString(spec, fullpath, sizeof(fullpath)),
                    &id
                );
                if (ret == 0) {
                    return 2;
                } else if (ret < 0) {
                    fileinfo->recordbrowseinfo = TRUE;
                    fileinfo->fileID = id;
                } else {
                    fileinfo->recordbrowseinfo = FALSE;
                    fileinfo->fileID = id;
                }
            } else {
                fileinfo->recordbrowseinfo = FALSE;
                fileinfo->fileID = 0;
            }
        }

        OS_OSSpec_To_FSSpec(spec, &fileinfo->filespec);
        return TRUE;
    }

    return FALSE;
}

static Boolean FindAndLoad_MissingFile(
    CWPluginContext context,
    const char* filename,
    CWFileInfo* fileinfo,
    OSSpec* spec,
    Boolean* loaded
) {
    File* file;
    CWCompilerLinkerContext* c;

    if (optsCompiler.ignoreMissingFiles) {
        if (context->pluginType == CWDROPINCOMPILERTYPE ||
            context->pluginType == CWDROPINLINKERTYPE) {
            c = static_cast<CWCompilerLinkerContext*>(context);
        } else {
            return FALSE;
        }

        file = Files_GetFile(&gTarg->files, c->whichfile);
        OS_ASSERT(486, file != NULL);

        OS_MakeSpecWithPath(&file->srcfss.path, filename, TRUE, spec);
        Deps_AddDependency(
            &file->deps,
            -1,
            spec,
            !(fileinfo->fullsearch || optsCompiler.noSysPath),
            fileinfo->dependencyType,
            &fileinfo->alreadyincluded
        );

        fileinfo->fileID = 0;
        fileinfo->recordbrowseinfo = FALSE;
        if (!fileinfo->suppressload) {
            fileinfo->filedata = xstrdup("");
            fileinfo->filedatalength = 0;
            fileinfo->filedatatype = cwFileTypeText;
            *loaded = TRUE;
        } else {
            fileinfo->filedata = NULL;
            fileinfo->filedatalength = 0;
            fileinfo->filedatatype = cwFileTypeUnknown;
        }

        OS_OSSpec_To_FSSpec(spec, &fileinfo->filespec);
        return TRUE;
    }

    return FALSE;
}

CW_CALLBACK UCBFindAndLoadFile(
    CWPluginContext context, const char* infilename, CWFileInfo* fileinfo
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBFindAndLoadFile");
    }

    OSSpec spec;
    Boolean loaded;
    char filename[260];
    char browseinfo[64];

    ShowWorking(8);
    if (CheckForUserBreak()) {
        return cwErrUserCanceled;
    }

    if (optsCompiler.canonicalIncludes) {
        strcpy(filename, infilename);
    } else if (OS_CanonPath(infilename, filename)) {
        return cwErrInvalidParameter;
    }

    loaded = FALSE;
    if (!FindAndLoad_VirtualFile(context, filename, fileinfo, &spec, &loaded) &&
        !FindAndLoad_AccessPathFile(context, filename, fileinfo, &spec, &loaded) &&
        !FindAndLoad_MissingFile(context, filename, fileinfo, &spec, &loaded)) {
        return cwErrFileNotFound;
    }

    if (optsCompiler.printHeaderNames) {
        CLPrint("%s\n", OS_SpecToStringRelative(&spec, NULL, STSbuf, sizeof(STSbuf)));
    }

    if (context->pluginType == CWDROPINCOMPILERTYPE) {
        if (optsCmdLine.verbose > 2) {
            sprintf(browseinfo, " (browse fileID %d)", fileinfo->fileID);
            CLReport(
                CLStr21,
                fileinfo->alreadyincluded ? "Included" : "Including",
                OS_SpecToString(&spec, STSbuf, sizeof(STSbuf)),
                fileinfo->recordbrowseinfo ? browseinfo : ""
            );
        } else if (optsCmdLine.verbose > 1 && !fileinfo->alreadyincluded) {
            CLReport(CLStr21, "Including", OS_SpecToString(&spec, STSbuf, sizeof(STSbuf)), "");
        }
    } else {
        fileinfo->fileID = 0;
        fileinfo->recordbrowseinfo = FALSE;
        fileinfo->alreadyincluded = FALSE;
    }

    if (!loaded) {
        if (!fileinfo->suppressload) {
            return UCBGetFileText(
                context,
                &fileinfo->filespec,
                &fileinfo->filedata,
                &fileinfo->filedatalength,
                &fileinfo->filedatatype
            );
        } else {
            fileinfo->filedata = NULL;
            fileinfo->filedatalength = 0;
            fileinfo->filedatatype = cwFileTypeUnknown;
            return cwNoErr;
        }
    } else {
        return cwNoErr;
    }
}
#endif

#if MWCCPSP
// 0x428720: the shell spec (an OSSpec); unknown files may be virtual files
CW_CALLBACK UCBGetFileText(
    CWPluginContext context,
    const CWFileSpec* filespec,
    const char** text,
    SInt32* textLength,
    short* filedatatype
) {
    OSHandle* texthandle;
    Boolean precomp;
    OSSpec spec;
    VFile* vf;
    char filename[260];

    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBGetFileText");
    }

    spec = *reinterpret_cast<const OSSpec*>(filespec);
    if (LoadAndCacheFile(&spec, &texthandle, &precomp)) {
        OS_NameSpecToString(
            &reinterpret_cast<const OSSpec*>(filespec)->name, filename, sizeof(filename)
        );
        vf = VFiles_Find(gTarg->virtualFiles, filename);
        if (!vf) {
            return cwErrFileNotFound;
        }
        CopyFileText(&vf->data, const_cast<char**>(text), textLength);
        *filedatatype = cwFileTypeText;
        OS_MakeFileSpec(filename, &spec);
        return cwNoErr;
    }

    if (texthandle) {
        if (!precomp) {
            CopyFileText(texthandle, const_cast<char**>(text), textLength);
            FreeIncludeFile(texthandle);
        } else {
            *text = static_cast<char*>(OS_LockHandle(texthandle));
            if (OS_GetHandleSize(texthandle, reinterpret_cast<UInt32*>(textLength))) {
                return cwErrRequestFailed;
            }
        }
    }

    if (texthandle) {
        if (!precomp) {
            *filedatatype = cwFileTypeText;
        } else {
            *filedatatype = cwFileTypePrecompiledHeader;
        }
    } else {
        *filedatatype = cwFileTypeUnknown;
    }
    return cwNoErr;
}
#else
CW_CALLBACK UCBGetFileText(
    CWPluginContext context,
    const CWFileSpec* filespec,
    const char** text,
    SInt32* textLength,
    short* filedatatype
) {
    int err;
    Handle texthandle = NULL;
    Boolean precomp;
    OSSpec spec;
    VFile* vf;
    char filename[256];

    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBGetFileText");
    }

    OS_FSSpec_To_OSSpec(filespec, &spec);
    err = LoadAndCacheFile(&spec, &texthandle, &precomp);
    if (err) {
        p2cstrcpy(filename, filespec->name);
        if (filespec->vRefNum ||
            filespec->parID ||
            !(vf = VFiles_Find(gTarg->virtualFiles, filename))) {
            CLReportOSError(CLStr93, err, OS_SpecToString(&spec, STSbuf, sizeof(STSbuf)));
            return cwErrFileNotFound;
        }
        CopyFileText(vf->data, const_cast<char**>(text), textLength);
        *filedatatype = cwFileTypeText;
        int err = OS_MakeFileSpec(filename, &spec);
    } else {
        if (texthandle) {
            if (!precomp) {
                CopyFileText(texthandle, const_cast<char**>(text), textLength);
                FreeIncludeFile(texthandle);
            } else {
                HLock(texthandle);
                *text = *texthandle;
                *textLength = GetHandleSize(texthandle);
            }
        }

        if (texthandle) {
            if (!precomp) {
                *filedatatype = cwFileTypeText;
            } else {
                *filedatatype = cwFileTypePrecompiledHeader;
            }
        } else {
            *filedatatype = cwFileTypeUnknown;
        }
    }

    return cwNoErr;
}
#endif

CW_CALLBACK UCBReleaseFileText(CWPluginContext context, const char* text) {
    short callbackResult = 0;
    CWCompilerLinkerContext* cc = static_cast<CWCompilerLinkerContext*>(context);

    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBReleaseFileText");
    }

    if (text) {
        if (text != cc->sourcetext) {
            xfree((void*)text);
        }
    } else {
        callbackResult = cwErrInvalidParameter;
    }

    return callbackResult;
}

CW_CALLBACK UCBGetSegmentInfo(
    CWPluginContext context, SInt32 whichsegment, CWProjectSegmentInfo* segmentinfo
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBGetSegmentInfo");
    }

    if (gTarg->linkmodel != LinkModel_Segmented) {
        return cwErrInvalidCallback;
    }

    Segment* seg = Segments_GetSegment(&gTarg->linkage.segs, whichsegment);
    if (!seg) {
        return cwErrUnknownSegment;
    }

    strcpy(segmentinfo->name, seg->name);
    segmentinfo->attributes = seg->attrs;
    return cwNoErr;
}

CW_CALLBACK UCBGetOverlay1GroupInfo(
    CWPluginContext context, SInt32 whichgroup, CWOverlay1GroupInfo* groupinfo
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBGetOverlay1GroupInfo");
    }

    OvlGroup* grp = Overlays_GetOvlGroup(&gTarg->linkage.overlays, whichgroup);
    if (gTarg->linkmodel != LinkModel_Overlay1) {
        return cwErrInvalidCallback;
    }

    if (grp) {
        strcpy(groupinfo->name, grp->name);
        groupinfo->address.lo = grp->addr.lo;
        groupinfo->address.hi = grp->addr.hi;
        groupinfo->numoverlays = OvlGroup_CountOverlays(grp);
        return cwNoErr;
    } else {
        return cwErrInvalidParameter;
    }
}

CW_CALLBACK UCBGetOverlay1FileInfo(
    CWPluginContext context,
    SInt32 whichgroup,
    SInt32 whichoverlay,
    SInt32 whichoverlayfile,
    CWOverlay1FileInfo* fileinfo
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBGetOverlay1FileInfo");
    }

    if (gTarg->linkmodel != LinkModel_Overlay1) {
        return cwErrInvalidCallback;
    }

    SInt32 filenum = Overlays_GetFileInOverlay(
        &gTarg->linkage.overlays, whichgroup, whichoverlay, whichoverlayfile
    );
    if (filenum >= 0) {
        fileinfo->whichfile = filenum;
        return cwNoErr;
    } else {
        return cwErrInvalidParameter;
    }
}

CW_CALLBACK UCBGetOverlay1Info(
    CWPluginContext context, SInt32 whichgroup, SInt32 whichoverlay, CWOverlay1Info* overlayinfo
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBGetOverlay1Info");
    }

    if (gTarg->linkmodel != LinkModel_Overlay1) {
        return cwErrInvalidCallback;
    }

    Overlay* oly = Overlays_GetOverlayInGroup(&gTarg->linkage.overlays, whichgroup, whichoverlay);
    if (oly) {
        strcpy(overlayinfo->name, oly->name);
        overlayinfo->numfiles = Overlay_CountFiles(oly);
        return cwNoErr;
    } else {
        return cwErrInvalidParameter;
    }
}

#if MWCCPSP
// UCBReportMessage's formats by [has line2][line2 ends a line][line1 ends a line]
static const char* fmts[2][2][2] = {"%\n", "%", "%\n", "%", "%\n%\n", "%%\n", "%\n%", "%%"};

struct ShellMessageRef {
    OSSpec sourcefile;
    SInt32 linenumber;
    short tokenoffset;
    short tokenlength;
    SInt32 selectionoffset;
    SInt32 selectionlength;
};
#define SHELLREF reinterpret_cast<const ShellMessageRef*>(msgRef)

#endif
CW_CALLBACK UCBReportMessage(
    CWPluginContext context,
    const CWMessageRef* msgRef,
    const char* line1,
    const char* line2,
    short errorlevel,
    SInt32 errorNumber
) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBReportMessage");
    }
#if MWCCPSP
    if (optsCmdLine.verbose > 4) {
        CLPrint("Message type: %d, number: %d\n", errorlevel, errorNumber);
    }
#endif

    short msgType;
    int eeol;
    char* tptr;
    MessageRef ref;
#if !MWCCPSP

    static const char* fmts[2][2][2] = {"%\n", "%", "%\n", "%", "%\n%\n", "%%\n", "%\n%", "%%"};
#endif

    if (line1 == NULL) {
        line1 = "";
        eeol = 0;
    } else {
        const char* eeptr = line1 + strlen(line1) - 1;
        eeol = *eeptr == '\r' || *eeptr == '\n';
    }

    if (CheckForUserBreak()) {
        return cwErrUserCanceled;
    }

    if (errorlevel == messagetypeError) {
        msgType = Msg_Error;
    } else if (errorlevel == messagetypeWarning) {
        msgType = Msg_Warning;
    } else {
        msgType = Msg_Note;
    }

    if (!msgRef) {
        if (line2 && line2[0]) {
            const char* eeptr = line2 + strlen(line2) - 1;
            int eeol2 = *eeptr == '\r' || *eeptr == '\n';
            CLStyledMessageDispatch(
                static_cast<shellContextType*>(context->shellContext)->plugin,
                NULL,
                errorNumber,
                msgType,
                fmts[1][eeol2][eeol],
                line1,
                line2
            );
        } else {
            CLStyledMessageDispatch(
                static_cast<shellContextType*>(context->shellContext)->plugin,
                NULL,
                errorNumber,
                msgType,
                fmts[0][0][eeol],
                line1
            );
        }
    } else {
#if MWCCPSP
        // 3.0.1 hands the shell a message ref holding an OSSpec
        ref.sourcefile = static_cast<CWCompilerLinkerContext*>(context)->sourcefile;
        ref.errorfile = SHELLREF->sourcefile;
        ref.sourceline = const_cast<char*>(line2);
        ref.linenumber = SHELLREF->linenumber;

        ref.tokenoffset = SHELLREF->tokenoffset;
        ref.tokenlength = SHELLREF->tokenlength;
        ref.selectionoffset = SHELLREF->selectionoffset;
        ref.selectionlength = SHELLREF->selectionlength;
#else
        OS_FSSpec_To_OSSpec(
            &static_cast<CWCompilerLinkerContext*>(context)->sourcefile, &ref.sourcefile
        );
        OS_FSSpec_To_OSSpec(&msgRef->sourcefile, &ref.errorfile);
        ref.sourceline = const_cast<char*>(line2);
        ref.linenumber = msgRef->linenumber;

        ref.tokenoffset = msgRef->tokenoffset;
        ref.tokenlength = msgRef->tokenlength;
        ref.selectionoffset = msgRef->selectionoffset;
        ref.selectionlength = msgRef->selectionlength;
#endif

        if (ref.tokenoffset < 0) {
            ref.tokenoffset = 0;
        }
        if (ref.tokenlength < 0) {
            ref.tokenlength = 0;
        }
        if (ref.selectionoffset < 0) {
            ref.selectionoffset = 0;
        }
        if (ref.selectionlength < 0) {
            ref.selectionlength = 0;
        }

        CLStyledMessageDispatch(
            static_cast<shellContextType*>(context->shellContext)->plugin,
            &ref,
            errorNumber,
            msgType,
            fmts[0][0][eeol],
            line1
        );
    }

    return cwNoErr;
}

CW_CALLBACK UCBAlert(
    CWPluginContext context, const char* msg1, const char* msg2, const char* msg3, const char* msg4
) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBAlert");
    }

    if (CheckForUserBreak()) {
        return cwErrUserCanceled;
    }

    if (msg4) {
        CLStyledMessageDispatch(
            static_cast<shellContextType*>(context->shellContext)->plugin,
            NULL,
            0,
            Msg_Alert,
            "%\n%\n%\n%\n",
            msg1 ? msg1 : "",
            msg2 ? msg2 : "",
            msg3 ? msg3 : "",
            msg4
        );
    } else if (msg3) {
        CLStyledMessageDispatch(
            static_cast<shellContextType*>(context->shellContext)->plugin,
            NULL,
            0,
            Msg_Alert,
            "%\n%\n%\n",
            msg1 ? msg1 : "",
            msg2 ? msg2 : "",
            msg3
        );
    } else if (msg2) {
        CLStyledMessageDispatch(
            static_cast<shellContextType*>(context->shellContext)->plugin,
            NULL,
            0,
            Msg_Alert,
            "%\n%\n",
            msg1 ? msg1 : "",
            msg2
        );
    } else {
        CLStyledMessageDispatch(
            static_cast<shellContextType*>(context->shellContext)->plugin,
            NULL,
            0,
            Msg_Alert,
            "%\n",
            msg1 ? msg1 : ""
        );
    }

    return cwNoErr;
}

CW_CALLBACK UCBShowStatus(CWPluginContext context, const char* line1, const char* line2) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBShowStatus");
    }

    if (CheckForUserBreak()) {
        return cwErrUserCanceled;
    }

    if (optsCmdLine.verbose > 1) {
        if ((!line1 || !*line1) && (!line2 || !*line2)) {
            return cwNoErr;
        }

        if (line2 && *line2) {
            CLStyledMessageDispatch(
                static_cast<shellContextType*>(context->shellContext)->plugin,
                NULL,
                0,
                Msg_Status,
                "%\n%\n",
                line1 ? line1 : "",
                line2 ? line2 : ""
            );
        } else {
            CLStyledMessageDispatch(
                static_cast<shellContextType*>(context->shellContext)->plugin,
                NULL,
                0,
                Msg_Status,
                "%\n",
                line1 ? line1 : ""
            );
        }
    }

    return cwNoErr;
}

CW_CALLBACK UCBUserBreak(CWPluginContext context) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBUserBreak");
    }

    ShowWorking(8);
    if (CheckForUserBreak()) {
        return cwErrUserCanceled;
    } else {
        return cwNoErr;
    }
}

#if MWCCPSP
extern "C" ShellHandleEntry* ShellHandles_Add(
    CWPluginContext context,
    void* ptr,
    Boolean keep2,
    Boolean isHandle,
    Boolean keep,
    Boolean hasMacHandle
);
#endif
#if MWCCPSP

#endif
CW_CALLBACK UCBGetNamedPreferences(
    CWPluginContext context, const char* prefsname, CWMemHandle* prefsdata
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBGetNamedPreferences");
    }

#if MWCCPSP
    // 0x429070: a copy of the panel in an OS handle the request owns
    PrefPanel* pnl = Prefs_FindPanel(prefsname);
    if (pnl) {
        if (optsCmdLine.verbose > 2) {
            CLReport(CLStr83, prefsname);
        }
        OSHandle* h = static_cast<OSHandle*>(xmalloc(NULL, sizeof(OSHandle)));
        if (!PrefPanel_GetHandle(pnl, h)) {
            xfree(h);
            return cwErrRequestFailed;
        }
        *prefsdata = reinterpret_cast<CWMemHandle>(h);
        ShellHandles_Add(context, h, FALSE, TRUE, FALSE, FALSE);
        return cwNoErr;
    } else {
        CLReportError(CLStr91, prefsname);
        *prefsdata = NULL;
        return cwErrRequestFailed;
    }
#else
    PrefPanel* pnl = Prefs_FindPanel(prefsname);
    if (pnl) {
        if (optsCmdLine.verbose > 2) {
            CLReport(CLStr83, prefsname);
        }
        Handle h = PrefPanel_GetHandle(pnl);
        UCBSecretAttachHandle(context, h, prefsdata);
        return cwNoErr;
    } else {
        CLReportError(CLStr91, prefsname);
        *prefsdata = NULL;
        return cwErrRequestFailed;
    }
#endif
}

CW_CALLBACK UCBStorePluginData(
    CWPluginContext context, SInt32 whichfile, CWDataType type, CWMemHandle prefsdata
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBStorePluginData");
    }

    DO_INTERNAL_ERROR(1267, "UCBStorePluginData not implemented");
    return cwErrRequestFailed;
}

CW_CALLBACK UCBGetPluginData(
    CWPluginContext context, SInt32 whichfile, CWDataType type, CWMemHandle* prefsdata
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBGetPluginData");
    }

    DO_INTERNAL_ERROR(1286, "UCBGetPluginData not implemented");
    return cwErrRequestFailed;
}

CW_CALLBACK UCBSetModDate(
    CWPluginContext context, const CWFileSpec* filespec, CWFileTime* moddate, Boolean isGenerated
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBSetModDate");
    }

    OSSpec spec;
    OSTime mdtm;
    int err;
    SInt32 idx;
    File* file;

#if MWCCPSP
    SHELL_SPEC_TO_OSSPEC(filespec, &spec);
#else
    OS_FSSpec_To_OSSpec(filespec, &spec);
#endif
    if (!moddate || !*moddate) {
        OS_GetTime(&mdtm);
    } else {
        OS_MacToTime(*moddate, &mdtm);
    }

    for (idx = 0; idx < Files_Count(&gTarg->files); idx++) {
        file = Files_GetFile(&gTarg->files, idx);
        if (OS_EqualSpec(&file->srcfss, &spec)) {
            file->srcmoddate = mdtm;
        } else if (OS_EqualSpec(&file->outfss, &spec)) {
            file->outmoddate = mdtm;
        }
    }

    err = OS_SetFileTime(&spec, NULL, &mdtm);
    if (err == 0) {
        return cwNoErr;
    } else {
        return cwErrRequestFailed;
    }
}

CW_CALLBACK UCBAddProjectEntry(
    CWPluginContext context,
    const CWFileSpec* fileSpec,
    Boolean isGenerated,
    const CWNewProjectEntryInfo* entry,
    SInt32* whichfile
) {
    File* file;
    OSSpec fss;
    UInt32 filetype;
    Plugin* plugin;
    const char* extptr;
    char ext[16];
    UInt32 flags;
    char filename[260];
    const DropInFlags* df;
    OvlGroup* ovg;
    Overlay* oly;
    SInt32 idx;
    SInt32 failed;
    SInt32 index;

    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBAddProjectEntry");
    }

#if MWCCPSP
    fss = *reinterpret_cast<const OSSpec*>(fileSpec); // 3.0.1: the shell's spec
#else
    OS_FSSpec_To_OSSpec(fileSpec, &fss);
#endif
    if (OS_IsDir(&fss)) {
        return cwErrInvalidParameter;
    }

    OS_NameSpecToString(&fss.name, filename, sizeof(filename));
    if (gTarg->linker && !(gTarg->linkerDropinFlags & (linkAllowDupFileNames | isExecutableTool))) {
        if (Files_FindFile(&gTarg->files, &fss)) {
            if (optsCmdLine.verbose > 1) {
                CLReportWarning(CLStr106, filename);
            }
            return cwNoErr;
        }
    }

    extptr = filename + strlen(filename) - 1;
    while (extptr > filename && *extptr != '.') {
        --extptr;
    }
    if (extptr <= filename) {
        extptr = ".";
    }
    strncpy(ext, extptr, sizeof(ext) - 1);
    ext[sizeof(ext) - 1] = 0;

    OS_SpecToStringRelative(&fss, NULL, filename, sizeof(filename));
    if (optsCmdLine.verbose > 2) {
        CLReport(CLStr77, " to project", filename);
    }

    if (OS_GetMacFileType(&fss, &filetype)) {
        filetype = CWFOURCHAR('T', 'E', 'X', 'T');
    }

    plugin = Plugins_GetPluginForFile(
        NULL, CWDROPINANYTYPE, targetCPUAny, targetOSAny, filetype, ext, Lang_Any
    );
    if (plugin && gTarg->linker && !Plugin_CL_MatchesTarget(plugin, gTarg->cpu, gTarg->os, FALSE)) {
        CLReportError(CLStr76, filename);
    }

    if (!plugin) {
        plugin = Plugins_CL_MatchTarget(
            NULL, gTarg->cpu, gTarg->os, clState.plugintype, clState.language
        );
    }

    if (plugin) {
        if (!Plugin_CL_GetCompilerMapping(plugin, filetype, ext, &flags)) {
            flags = 0;
            if (!isGenerated && !optsCompiler.compileIgnored && optsCompiler.forcePrecompile != 1) {
                CLReportWarning(
                    CLStr73,
                    "file",
                    filename,
                    (clState.plugintype == CWDROPINCOMPILERTYPE) ? "treating as source text"
                                                                 : "passing unknown file to linker"
                );
            }
            if (isGenerated) {
                flags = kIgnored;
            }
        } else if (!isGenerated && (flags & kIgnored) && !optsCompiler.compileIgnored) {
            if ((!optsCmdLine.stages ||
                 (optsCmdLine.stages & (CmdLineStageMask_Cg | CmdLineStageMask_Ds))) &&
                (optsCompiler.forcePrecompile != 1)) {
                CLReportWarning(CLStr28, filename);
            } else {
                flags &= ~kIgnored;
            }
        }

        if (optsCompiler.compileIgnored && !isGenerated) {
            flags &= ~kIgnored;
        }

        if (clState.pluginDebug) {
            CLPrint("Using plugin '%s' for '%s'\n", Plugin_GetDropInName(plugin), filename);
            CLPrint(
                "[flags: %s, %s, %s, %s]\n",
                (flags & kPrecompile) ? "precompile" : "don't precompile",
                (flags & kLaunchable) ? "launchable" : "not launchable",
                (flags & kRsrcfile) ? "resource file" : "not resource file",
                (flags & kIgnored) ? "ignored" : "used"
            );
        }
    } else {
        CLReportError(CLStr74, filename);
        flags = kIgnored;
    }

    file = File_New();
    if (!file) {
        return cwErrOutOfMemory;
    }

    Deps_Initialize(&file->deps, &gTarg->incls);

    index = entry->segment;
    if (index == -1) {
        index = Segments_Count(&gTarg->linkage.segs) - 1;
    }
    file->segnum = index;

    strcpy(file->srcfilename, filename);
    file->srcfss = fss;

    OS_GetFileTime(&file->srcfss, NULL, &file->srcmoddate);
    OS_GetTime(&file->outmoddate);

    file->outfilename[0] = 0;
    memset(&file->outfss, 0, sizeof(file->outfss));

    file->outfileowner = 0;
    file->writeToDisk = file->wroteToDisk = file->tempOnDisk = 0;

    if (plugin && Plugin_GetPluginType(plugin) != CWDROPINCOMPILERTYPE) {
        file->compiler = NULL;
    } else {
        file->compiler = plugin;
    }

    file->filetype = filetype;
    file->mappingflags = flags;

    if (file->compiler) {
        df = Plugin_GetDropInFlags(file->compiler);
        file->dropinflags = df->dropinflags;
    } else {
        file->dropinflags = 0;
    }

    file->weakimport = entry->weakimport;
    file->initbefore = entry->initbefore;
    file->mergeintooutput = entry->mergeintooutput;
    file->isresourcefile = (flags & kRsrcfile) != 0;

    if (file->compiler) {
        file->objectflags = Plugin_CL_GetObjectFlags(file->compiler)->flags;
    } else {
        file->objectflags = 0;
    }

    file->sourceUsage = 0;
    if (!(file->mappingflags & kIgnored)) {
        if (file->compiler) {
            file->sourceUsage |= CmdLineStageMask_Pp;
            if (!(file->objectflags & 0x80000000)) {
                file->sourceUsage |= CmdLineStageMask_Cg;
            }
        } else if (plugin) {
            file->sourceUsage |= CmdLineStageMask_Cg;
        }
    }

    if (file->isresourcefile) {
        file->sourceUsage |= CmdLineStageMask_Cg;
    }

    file->objectUsage = file->sourceUsage & CmdLineStageMask_Pp;
    if ((file->sourceUsage & CmdLineStageMask_Pp) &&
        !(file->mappingflags & kPrecompile) &&
        (optsCompiler.forcePrecompile != 1) &&
        (file->objectflags & 0x80000000)) {
        file->objectUsage |= CmdLineStageMask_Cg;
    }

    *whichfile = 0;
    index = entry->position;
    if (index != -1) {
        failed = !Files_AddFile(&gTarg->files, file);
    } else {
        failed = !Files_InsertFile(&gTarg->files, file, index);
    }
    if (failed) {
        return cwErrRequestFailed;
    }
    *whichfile = file->filenum;
    context->numFiles++;

    if (gTarg->linkmodel == LinkModel_Overlay1) {
        index = entry->overlayGroup;
        if (index == -1) {
            index = Overlays_CountGroups(&gTarg->linkage.overlays) - 1;
        }
        ovg = Overlays_GetOvlGroup(&gTarg->linkage.overlays, index);
        if (ovg) {
            index = entry->overlay;
            if (index == -1) {
                index = OvlGroup_CountOverlays(ovg) - 1;
            }
            oly = OvlGroup_GetOverlay(ovg, index);
            if (oly) {
                if (!Overlay_AddFile(oly, file->filenum, &idx)) {
                    return cwErrRequestFailed;
                }
            } else {
                return cwErrRequestFailed;
            }
        } else {
            return cwErrRequestFailed;
        }
    }

    return cwNoErr;
}

#if MWCCPSP
extern "C" {
// 0x428100: a shell spec for a file name
extern "C" void func_00428100(OSSpec* spec, const char* name);
// 0x4292F0: UCBAddProjectEntry with a shell spec
extern CW_CALLBACK func_004292F0(
    CWPluginContext context,
    const OSSpec* spec,
    Boolean isSource,
    const CWNewProjectEntryInfo* entry,
    SInt32* pos
);
}

// 0x429B20: the main output of a compile (preprocessing, requests 1 and 2)
// becomes the file's text; other documents go to stdout, to a file named
// after stdout_base, or become virtual files for the parser
CW_CALLBACK UCBCreateNewTextDocument(
    CWPluginContext context, const CWNewTextDocumentInfo* docinfo
) {
    OSSpec fss;
    char filename[256];
    CWNewProjectEntryInfo entry;
    SInt32 pos;
    UInt32 size;
    OSHandle* texth;

    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBCreateNewTextDocument");
    }

    if (context->pluginType == CWDROPINCOMPILERTYPE || context->pluginType == CWDROPINLINKERTYPE) {
        CWCompilerLinkerContext* clcontext = static_cast<CWCompilerLinkerContext*>(context);
        Boolean isMain = (clcontext->request == reqCompile && clcontext->preprocess) ||
            clcontext->request == 2 ||
            clcontext->request == 1;
        File* file = Files_GetFile(&gTarg->files, clcontext->whichfile);
        ShellHandles_Remove(context, docinfo->text, TRUE);
        if (isMain) {
            if (!file) {
                return cwErrUnknownFile;
            }
            if (file->textdata) {
                if (!ShowHandle(file->textdata, TRUE)) {
                    return cwErrRequestFailed;
                }
                OS_FreeHandle(file->textdata);
            }
            file->textdata = reinterpret_cast<OSHandle*>(docinfo->text);
            OS_GetTime(&file->outmoddate);
            return cwNoErr;
        } else {
            int len;
            int i;
            FILE* stdfile;

            texth = reinterpret_cast<OSHandle*>(docinfo->text);
            OS_GetHandleSize(texth, &size);
            if (docinfo->documentname || clState.stdout_base) {
                len = 0;
                if (clState.stdout_base) {
                    strcpy(filename, clState.stdout_base);
                    len = strlen(clState.stdout_base);
                    if (docinfo->documentname) {
                        filename[len++] = '_';
                    }
                }
                for (i = 0; docinfo->documentname[i]; i++) {
                    filename[len++] =
                        docinfo->documentname[i] == ' ' ? '_' : docinfo->documentname[i];
                }
                filename[len] = 0;

                if (!docinfo->documentname) {
                    stdfile = fopen(filename, !clState.stdout_written ? "wt" : "at");
                    clState.stdout_written = TRUE;
                } else {
                    stdfile = fopen(filename, "wt");
                }
                if (stdfile) {
                    fwrite(OS_LockHandle(texth), size, 1, stdfile);
                    fclose(stdfile);
                    OS_UnlockHandle(texth);
                    return cwNoErr;
                }
            }

            if (!ShowHandle(texth, FALSE)) {
                return cwErrRequestFailed;
            }
            return cwNoErr;
        }
    } else if (context->pluginType == CWDROPINPARSERTYPE) {
        VFile* vf;

        texth = reinterpret_cast<OSHandle*>(docinfo->text);
        ShellHandles_Remove(context, texth, TRUE);
        if (docinfo->markDirty) {
            if (!docinfo->documentname) {
                return cwErrInvalidParameter;
            }

            vf = VFile_New(docinfo->documentname, texth);
            if (!vf) {
                return cwErrOutOfMemory;
            }
            if (!VFiles_Add(&gTarg->virtualFiles, vf)) {
                return cwErrRequestFailed;
            }

            entry.position = entry.segment = entry.overlayGroup = entry.overlay = -1;
            entry.groupPath = NULL;
            entry.mergeintooutput = entry.weakimport = entry.initbefore = FALSE;
            func_00428100(&fss, docinfo->documentname);
            return func_004292F0(context, &fss, TRUE, &entry, &pos);
        } else {
            if (!ShowHandle(texth, docinfo->documentname != NULL)) {
                return cwErrRequestFailed;
            }
            return cwNoErr;
        }
    } else {
        DO_INTERNAL_ERROR(1840, "Cannot deal with unexpected document");
        return cwErrInvalidCallback;
    }
}
#else
CW_CALLBACK UCBCreateNewTextDocument(
    CWPluginContext context, const CWNewTextDocumentInfo* docinfo
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBCreateNewTextDocument");
    }

    if (context->pluginType == CWDROPINCOMPILERTYPE || context->pluginType == CWDROPINLINKERTYPE) {
        if (!docinfo->documentname) {
            CWCompilerLinkerContext* clcontext = static_cast<CWCompilerLinkerContext*>(context);
            File* file = Files_GetFile(&gTarg->files, clcontext->whichfile);
            if (!file) {
                return cwErrUnknownFile;
            }

            if (file->textdata) {
                UInt32 size = GetHandleSize(file->textdata);
                if (!ShowHandle(file->textdata, size, TRUE)) {
                    return cwErrRequestFailed;
                }
                DisposeHandle(file->textdata);
            }

            UCBSecretDetachHandle(context, docinfo->text, &file->textdata);
            OS_GetTime(&file->outmoddate);
            return cwNoErr;
        } else {
            Handle texth;
            UCBSecretDetachHandle(context, docinfo->text, &texth);
            UInt32 size = GetHandleSize(texth);
            if (strcmp(docinfo->documentname, ">stdout") || clState.stdout_base) {
                FILE* stdfile;
                if (!strcmp(docinfo->documentname, ">stdout")) {
                    if (!clState.stdout_written) {
                        stdfile = fopen(clState.stdout_base, "wt");
                    } else {
                        stdfile = fopen(clState.stdout_base, "at");
                    }
                    clState.stdout_written = TRUE;
                } else {
                    stdfile = fopen(docinfo->documentname, "wt");
                }

                if (stdfile) {
                    HLock(texth);
                    fwrite(*texth, size, 1, stdfile);
                    fclose(stdfile);
                    HUnlock(texth);
                    return cwNoErr;
                }
            }

            if (!ShowHandle(texth, size, FALSE)) {
                return cwErrRequestFailed;
            } else {
                return cwNoErr;
            }
        }
    } else if (context->pluginType == CWDROPINPARSERTYPE) {
        Handle texth;
        FSSpec fss;
        CWNewProjectEntryInfo entry;
        SInt32 pos;
        Str255 filename;
        VFile* vf;

        UCBSecretDetachHandle(context, docinfo->text, &texth);

        if (docinfo->markDirty) {
            if (!docinfo->documentname) {
                return cwErrInvalidParameter;
            }

            vf = VFile_New(docinfo->documentname, texth);
            if (!vf) {
                return cwErrOutOfMemory;
            }
            if (!VFiles_Add(&gTarg->virtualFiles, vf)) {
                return cwErrRequestFailed;
            }

            entry.position = entry.segment = entry.overlayGroup = entry.overlay = -1;
            entry.groupPath = NULL;
            entry.mergeintooutput = entry.weakimport = entry.initbefore = FALSE;
            c2pstrcpy(filename, docinfo->documentname);
            FSMakeFSSpec(0, 0, filename, &fss);
            return UCBAddProjectEntry(context, &fss, TRUE, &entry, &pos);
        } else {
            UInt32 size = GetHandleSize(texth);
            if (!ShowHandle(texth, size, docinfo->documentname != NULL)) {
                return cwErrRequestFailed;
            } else {
                return cwNoErr;
            }
        }
    } else {
        DO_INTERNAL_ERROR(1755, "Cannot deal with unexpected document");
        return cwErrInvalidCallback;
    }
}
#endif

#if MWCCPSP
// 3.0.1: what a plugin allocates is recorded for the request (ShellHandles_*)
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
extern CW_CALLBACK CWSetPluginOSError(CWPluginContext context, SInt32 err);
}

// 0x429F40
CW_CALLBACK UCBAllocateMemory(
    CWPluginContext context, SInt32 size, Boolean isPermanent, void** ptr
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBAllocateMemory");
    }

    if (!size) {
        size = 1;
    }
    *ptr = malloc(size);
    if (!*ptr) {
        return cwErrOutOfMemory;
    }
    ShellHandles_Add(context, *ptr, FALSE, FALSE, isPermanent, FALSE);
    return cwNoErr;
}

// 0x429FC0
CW_CALLBACK UCBFreeMemory(CWPluginContext context, void* ptr, Boolean isPermanent) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBFreeMemory");
    }

#if MWCCPS2
    if (ShellHandles_Find(context, ptr, FALSE) != NULL) {
#else
    if (ShellHandles_Find(context, ptr, FALSE)) {
#endif
        xfree(ptr);
        ShellHandles_Remove(context, ptr, FALSE);
        return cwNoErr;
    }
    return cwErrInvalidParameter;
}

// 0x42A020
CW_CALLBACK UCBAllocMemHandle(
    CWPluginContext context, SInt32 size, Boolean useTempMemory, CWMemHandle* memhandle
) {
    OSHandle* handle;
    int err;

    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBAllocMemHandle");
    }

    *memhandle = NULL;
    handle = static_cast<OSHandle*>(malloc(sizeof(OSHandle)));
    if (!handle) {
        return cwErrOutOfMemory;
    }
    if ((err = OS_NewHandle(size, handle))) {
        CWSetPluginOSError(context, err);
        free(handle);
        return cwErrOutOfMemory;
    }
    *memhandle = reinterpret_cast<CWMemHandle>(handle);
    ShellHandles_Add(context, handle, FALSE, TRUE, FALSE, FALSE);
    return cwNoErr;
}
#else
CW_CALLBACK UCBAllocateMemory(
    CWPluginContext context, SInt32 size, Boolean isPermanent, void** ptr
) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBAllocateMemory");
    }

    *ptr = xmalloc(NULL, size);
    return (*ptr == NULL) ? cwErrRequestFailed : cwNoErr;
}

CW_CALLBACK UCBFreeMemory(CWPluginContext context, void* ptr, Boolean isPermanent) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBFreeMemory");
    }

    if (ptr) {
        xfree(ptr);
        return cwNoErr;
    } else {
        return cwErrRequestFailed;
    }
}

CW_CALLBACK UCBAllocMemHandle(
    CWPluginContext context, SInt32 size, Boolean useTempMemory, CWMemHandle* memhandle
) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBAllocMemHandle");
    }

    Handle handle = NewHandle(size);
    if (!handle) {
        return cwErrOutOfMemory;
    }

    UCBSecretAttachHandle(context, handle, memhandle);
    return cwNoErr;
}
#endif

#if MWCCPSP
// 0x42A0B0: the handles are OSHandles recorded in the request's table
CW_CALLBACK UCBFreeMemHandle(CWPluginContext context, CWMemHandle memhandle) {
#if MWCCPS2
    ShellHandleEntry* entry;
    int err;
    void* handle = memhandle;

    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBFreeMemHandle");
    }

    if ((entry = ShellHandles_Find(context, handle, TRUE)) != NULL) {
        err = OS_FreeHandle(static_cast<OSHandle*>(entry->ptr));
        ShellHandles_Remove(context, memhandle, TRUE);
        CWSetPluginOSError(context, err);
        return err == 0 ? cwNoErr : cwErrRequestFailed;
    }
    return cwErrInvalidParameter;
#else
    ShellHandleEntry* entry;
    int err;

    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBFreeMemHandle");
    }

    if ((entry = ShellHandles_Find(context, memhandle, TRUE))) {
        err = OS_FreeHandle(static_cast<OSHandle*>(entry->ptr));
        ShellHandles_Remove(context, memhandle, TRUE);
        CWSetPluginOSError(context, err);
        return err ? cwErrRequestFailed : cwNoErr;
    }
    return cwErrInvalidParameter;
#endif
}

// 0x42A130
CW_CALLBACK UCBGetMemHandleSize(CWPluginContext context, CWMemHandle memhandle, SInt32* size) {
#if MWCCPS2
    ShellHandleEntry* entry;
    int err;
    void* handle = memhandle;

    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBGetMemHandleSize");
    }

    if ((entry = ShellHandles_Find(context, handle, TRUE)) != NULL) {
        err = OS_GetHandleSize(static_cast<OSHandle*>(entry->ptr), reinterpret_cast<UInt32*>(size));
        CWSetPluginOSError(context, err);
        return err == 0 ? cwNoErr : cwErrRequestFailed;
    }
#else
    ShellHandleEntry* entry;
    int err;

    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBGetMemHandleSize");
    }

    if ((entry = ShellHandles_Find(context, memhandle, TRUE))) {
        err = OS_GetHandleSize(static_cast<OSHandle*>(entry->ptr), reinterpret_cast<UInt32*>(size));
        CWSetPluginOSError(context, err);
        return err ? cwErrRequestFailed : cwNoErr;
    }
#endif
    return cwErrInvalidParameter;
}

// 0x42A1A0
CW_CALLBACK UCBResizeMemHandle(CWPluginContext context, CWMemHandle memhandle, SInt32 newSize) {
#if MWCCPS2
    ShellHandleEntry* entry;
    int err;
    void* handle = memhandle;

    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBResizeMemHandle");
    }

    if ((entry = ShellHandles_Find(context, handle, TRUE)) != NULL) {
        err = OS_ResizeHandle(static_cast<OSHandle*>(entry->ptr), newSize);
        CWSetPluginOSError(context, err);
        return err == 0 ? cwNoErr : cwErrOutOfMemory;
    }
    return cwErrInvalidParameter;
#else
    ShellHandleEntry* entry;
    int err;

    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBResizeMemHandle");
    }

    if ((entry = ShellHandles_Find(context, memhandle, TRUE))) {
        err = OS_ResizeHandle(static_cast<OSHandle*>(entry->ptr), newSize);
        CWSetPluginOSError(context, err);
        return err ? cwErrOutOfMemory : cwNoErr;
    }
    return cwErrInvalidParameter;
#endif
}

// 0x42A210
CW_CALLBACK UCBLockMemHandle(
    CWPluginContext context, CWMemHandle handle, Boolean moveHi, void** ptr
) {
#if MWCCPS2
    ShellHandleEntry* entry;
    void* h = handle;

    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBLockMemHandle");
    }

    if ((entry = ShellHandles_Find(context, h, TRUE)) != NULL) {
        *ptr = OS_LockHandle(static_cast<OSHandle*>(entry->ptr));
        CWSetPluginOSError(context, *ptr ? 0 : 8);
        return *ptr ? cwNoErr : cwErrOutOfMemory;
    }
    return cwErrInvalidParameter;
#else
    ShellHandleEntry* entry;

    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBLockMemHandle");
    }

    if ((entry = ShellHandles_Find(context, handle, TRUE))) {
        *ptr = OS_LockHandle(static_cast<OSHandle*>(entry->ptr));
        CWSetPluginOSError(context, *ptr ? 0 : 8);
        return *ptr ? cwNoErr : cwErrOutOfMemory;
    }
    return cwErrInvalidParameter;
#endif
}

// 0x42A290
CW_CALLBACK UCBUnlockMemHandle(CWPluginContext context, CWMemHandle handle) {
#if MWCCPS2
    ShellHandleEntry* entry;
    void* h = handle;

    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBUnlockMemHandle");
    }

    if ((entry = ShellHandles_Find(context, h, TRUE)) != NULL) {
        OS_UnlockHandle(static_cast<OSHandle*>(entry->ptr));
        return cwNoErr;
    }
    return cwErrInvalidParameter;
#else
    ShellHandleEntry* entry;

    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBUnlockMemHandle");
    }

    if ((entry = ShellHandles_Find(context, handle, TRUE))) {
        OS_UnlockHandle(static_cast<OSHandle*>(entry->ptr));
        return cwNoErr;
    }
    return cwErrInvalidParameter;
#endif
}
#else
CW_CALLBACK UCBFreeMemHandle(CWPluginContext context, CWMemHandle memhandle) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBFreeMemHandle");
    }

    Handle handle;
    UCBSecretDetachHandle(context, memhandle, &handle);
    DisposeHandle(handle);
    return cwNoErr;
}

CW_CALLBACK UCBGetMemHandleSize(CWPluginContext context, CWMemHandle memhandle, SInt32* size) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBGetMemHandleSize");
    }

    Handle handle;
    UCBSecretDetachHandle(context, memhandle, &handle);
    *size = GetHandleSize(handle);
    return cwNoErr;
}

CW_CALLBACK UCBResizeMemHandle(CWPluginContext context, CWMemHandle memhandle, SInt32 newSize) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBResizeMemHandle");
    }

    Handle handle;
    UCBSecretDetachHandle(context, memhandle, &handle);
    SetHandleSize(handle, newSize);
    return (MemError() == noErr) ? cwNoErr : cwErrOutOfMemory;
}

CW_CALLBACK UCBLockMemHandle(
    CWPluginContext context, CWMemHandle handle, Boolean moveHi, void** ptr
) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBLockMemHandle");
    }

    HLock(reinterpret_cast<Handle>(handle));
    *ptr = *reinterpret_cast<Handle>(handle);
    return cwNoErr;
}

CW_CALLBACK UCBUnlockMemHandle(CWPluginContext context, CWMemHandle handle) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBUnlockMemHandle");
    }

    HUnlock(reinterpret_cast<Handle>(handle));
    return cwNoErr;
}
#endif

CW_CALLBACK UCBGetTargetName(CWPluginContext context, char* name, short maxLength) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBGetTargetName");
    }

    strncpy(name, "command-line target", maxLength);
    return cwNoErr;
}

CW_CALLBACK UCBPreDialog(CWPluginContext context) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBPreDialog");
    }

    return cwNoErr;
}

CW_CALLBACK UCBPostDialog(CWPluginContext context) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBPostDialog");
    }

    return cwNoErr;
}

CW_CALLBACK UCBPreFileAction(CWPluginContext context, const CWFileSpec* theFile) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBPreFileAction");
    }

    DO_INTERNAL_ERROR(1963, "UCBPreFileAction not implemented");
    return cwErrRequestFailed;
}

CW_CALLBACK UCBPostFileAction(CWPluginContext context, const CWFileSpec* theFile) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBPostFileAction");
    }

    DO_INTERNAL_ERROR(1977, "UCBPostFileAction not implemented");
    return cwErrRequestFailed;
}

CW_CALLBACK UCBCacheAccessPathList(CWPluginContext context) {
    IDEAccessPathList* apl;
    shellContextType* sc;
    int x;

    sc = static_cast<shellContextType*>(context->shellContext);
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBCacheAccessPathList");
    }

    apl = context->accessPathList;
    if (!apl) {
        apl = context->accessPathList =
            static_cast<IDEAccessPathList*>(calloc(sizeof(IDEAccessPathList), 1));
    }

    if (sc->userAccessPathsChanged || !apl->userPaths) {
        OSSpec spec;
        Path* path;
        IDEAccessPath* ap;

        apl->userPathCount = Paths_Count(&gTarg->userPaths);
        apl->userPaths = static_cast<IDEAccessPath*>(
            xrealloc("access paths", apl->userPaths, sizeof(IDEAccessPath) * apl->userPathCount)
        );

        for (x = 0; x < apl->userPathCount; x++) {
            ap = &apl->userPaths[x];
            path = Paths_GetPath(&gTarg->userPaths, x);
            OS_ASSERT(2010, path);

            OS_MakeSpecWithPath(path->spec, NULL, FALSE, &spec);
#if MWCCPSP
#if CW_SHELL_OSSPEC
            ap->pathSpec = spec;
#else
            SHELL_SPEC_FROM_OSSPEC(&spec, &ap->pathSpec);
#endif
#else
            OS_OSSpec_To_FSSpec(&spec, &ap->pathSpec);
#endif
            ap->recursive = path->recursive != 0;
            if (path->recursive) {
                ap->subdirectoryCount = Paths_CountRecurse(path->recursive);
                if (ap->subdirectories) {
                    xfree(ap->subdirectories);
                }
#if MWCCPSP
#if CW_SHELL_OSSPEC
                ap->subdirectories = static_cast<CWFileSpec*>(
                    xmalloc(NULL, sizeof(CWFileSpec) * ap->subdirectoryCount)
                );
                Paths_CopyRecurseFSS(
                    context->shellContext,
                    ap->subdirectories,
                    path->recursive,
                    ap->subdirectoryCount
                );
#else
                ap->subdirectories =
                    static_cast<FSSpec*>(xmalloc(NULL, sizeof(FSSpec) * ap->subdirectoryCount));
                Paths_CopyRecurseFSS(ap->subdirectories, path->recursive, ap->subdirectoryCount);
#endif
#else
                ap->subdirectories =
                    static_cast<FSSpec*>(xmalloc(NULL, sizeof(FSSpec) * ap->subdirectoryCount));
                Paths_CopyRecurseFSS(ap->subdirectories, path->recursive, ap->subdirectoryCount);
#endif
            } else {
                ap->subdirectoryCount = 0;
                ap->subdirectories = NULL;
            }
        }

        sc->userAccessPathsChanged = FALSE;
    }

    if (sc->systemAccessPathsChanged || !apl->systemPaths) {
        OSSpec spec;
        Path* path;
        IDEAccessPath* ap;

        apl->systemPathCount = Paths_Count(&gTarg->sysPaths);
        apl->systemPaths = static_cast<IDEAccessPath*>(
            xrealloc("access paths", apl->systemPaths, sizeof(IDEAccessPath) * apl->systemPathCount)
        );

        for (x = 0; x < apl->systemPathCount; x++) {
            ap = &apl->systemPaths[x];
            path = Paths_GetPath(&gTarg->sysPaths, x);
            OS_ASSERT(2044, path);

            OS_MakeSpecWithPath(path->spec, NULL, FALSE, &spec);
#if MWCCPSP
#if CW_SHELL_OSSPEC
            ap->pathSpec = spec;
#else
            SHELL_SPEC_FROM_OSSPEC(&spec, &ap->pathSpec);
#endif
#else
            OS_OSSpec_To_FSSpec(&spec, &ap->pathSpec);
#endif
            ap->recursive = path->recursive != 0;
            if (path->recursive) {
                ap->subdirectoryCount = Paths_CountRecurse(path->recursive);
                if (ap->subdirectories) {
                    xfree(ap->subdirectories);
                }
#if MWCCPSP
#if CW_SHELL_OSSPEC
                ap->subdirectories = static_cast<CWFileSpec*>(
                    xmalloc(NULL, sizeof(CWFileSpec) * ap->subdirectoryCount)
                );
                Paths_CopyRecurseFSS(
                    context->shellContext,
                    ap->subdirectories,
                    path->recursive,
                    ap->subdirectoryCount
                );
#else
                ap->subdirectories =
                    static_cast<FSSpec*>(xmalloc(NULL, sizeof(FSSpec) * ap->subdirectoryCount));
                Paths_CopyRecurseFSS(ap->subdirectories, path->recursive, ap->subdirectoryCount);
#endif
#else
                ap->subdirectories =
                    static_cast<FSSpec*>(xmalloc(NULL, sizeof(FSSpec) * ap->subdirectoryCount));
                Paths_CopyRecurseFSS(ap->subdirectories, path->recursive, ap->subdirectoryCount);
#endif
            } else {
                ap->subdirectoryCount = 0;
                ap->subdirectories = NULL;
            }
        }

        sc->systemAccessPathsChanged = FALSE;
    }

    return cwNoErr;
}

#if MWCCPSP
// 0x42A7C0: records the OS handle behind a Mac handle for the request
CW_CALLBACK UCBSecretAttachHandle(CWPluginContext context, Handle handle, CWMemHandle* memHandle) {
    OSHandle* oh;
    ShellHandleEntry* entry;

    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBSecretAttachHandle");
    }

    oh = OS_PeekMacHandle(handle);
    if ((entry = ShellHandles_Find(context, oh, TRUE))) {
        if (entry->ptr != oh && !entry->keep2) {
            OS_FreeHandle(static_cast<OSHandle*>(entry->ptr));
        }
        entry->ptr = oh;
    } else {
        ShellHandles_Add(context, oh, FALSE, TRUE, FALSE, TRUE);
    }
    *memHandle = reinterpret_cast<CWMemHandle>(oh);
    return cwNoErr;
}

// 0x42A840: a Mac handle for an OS handle the request knows
CW_CALLBACK UCBSecretPeekHandle(CWPluginContext context, CWMemHandle memHandle, Handle* handle) {
    OSHandle* oh = reinterpret_cast<OSHandle*>(memHandle);
    ShellHandleEntry* entry;

    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBSecretPeekHandle");
    }

    entry = ShellHandles_Find(context, oh, TRUE);
    if (!memHandle || !handle || !entry) {
        *handle = NULL;
        return cwErrInvalidParameter;
    }
    if (entry->hasMacHandle) {
        *handle = OS_MacHandleFromOSHandle(oh);
        if (!*handle) {
            CLFatalError(__FILE__, 2317, "UCBSecretDetachHandle failed");
            return cwErrRequestFailed;
        }
    } else {
        *handle = OS_CreateMacHandleDup(oh);
        entry->hasMacHandle = TRUE;
    }
    return cwNoErr;
}

// 0x42A900
CW_CALLBACK UCBSecretDetachHandle(CWPluginContext context, CWMemHandle memHandle, Handle* handle) {
    CWResult err;

    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBSecretDetachHandle");
    }

    err = UCBSecretPeekHandle(context, memHandle, handle);
    if (!err) {
        ShellHandles_Remove(context, memHandle, TRUE);
        OS_FreeHandle(reinterpret_cast<OSHandle*>(memHandle));
    }
    return err;
}
#else
CW_CALLBACK UCBSecretAttachHandle(CWPluginContext context, Handle handle, CWMemHandle* memHandle) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBSecretAttachHandle");
    }

    *reinterpret_cast<Handle*>(memHandle) = handle;
    return cwNoErr;
}

CW_CALLBACK UCBSecretDetachHandle(CWPluginContext context, CWMemHandle memHandle, Handle* handle) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBSecretDetachHandle");
    }

    if (!memHandle || !handle) {
        *handle = NULL;
        return cwErrInvalidParameter;
    } else {
        *handle = reinterpret_cast<Handle>(memHandle);
        return cwNoErr;
    }
}

CW_CALLBACK UCBSecretPeekHandle(CWPluginContext context, CWMemHandle memHandle, Handle* handle) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBSecretPeekHandle");
    }

    if (!memHandle || !handle) {
        *handle = NULL;
        return cwErrInvalidParameter;
    } else {
        *handle = reinterpret_cast<Handle>(memHandle);
        return cwNoErr;
    }
}
#endif

CW_CALLBACK UCBCheckoutLicense(
    CWPluginContext context,
    const char* featureName,
    const char* licenseVersion,
    UInt32 flags,
    void* reserved,
    SInt32* cookie
) {
    char errorBuf[4096];
    SInt32 license;
    UInt32 autoCheckin;

    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBCheckoutLicense");
    }

    autoCheckin = flags & 1;
    if (!autoCheckin && !cookie) {
        return cwErrInvalidParameter;
    }
    if (flags & ~3) {
#if MWCCPSP
        DO_INTERNAL_ERROR(2385, "Unknown license flags");
#else
        DO_INTERNAL_ERROR(2160, "Unknown license flags");
#endif
    }
    if (cookie) {
        *cookie = 0;
    }
    license = License_Checkout(featureName, licenseVersion, autoCheckin, errorBuf);
    if (cookie) {
        *cookie = license;
    }
    if (!license) {
        if (!(flags & 2)) {
            CLReportError(CLStr87, errorBuf);
        }
        return cwErrLicenseCheckFailed;
    }
    return cwNoErr;
}

CW_CALLBACK UCBCheckinLicense(CWPluginContext context, SInt32 cookie) {
    if (optsCmdLine.verbose > 3) {
        CLPrint("Callback: %s\n", "UCBCheckinLicense");
    }

    if (cookie) {
        License_Checkin(cookie);
    }
    return cwNoErr;
}

CW_CALLBACK UCBResolveRelativePath(
    CWPluginContext context,
    const CWRelativePath* relativePath,
    CWFileSpec* fileSpec,
    Boolean create
) {
    DO_INTERNAL_ERROR(2255, "UCBResolveRelativePath not implemented");
    return cwErrRequestFailed;
}

CW_CALLBACK UCBMacOSErrToCWResult(CWPluginContext context, OSErr err) {
    if (optsCmdLine.verbose > 4) {
        CLPrint("Callback: %s\n", "UCBMacOSErrToCWResult");
    }

    return OSErrtoCWResult(err);
}
}

// clang-format off
CW_BasePluginCallbacks sBasePluginCallbacks = {
    UCBGetFileInfo,
    UCBFindAndLoadFile,
    UCBGetFileText,
    UCBReleaseFileText,
    UCBGetSegmentInfo,
    UCBGetOverlay1GroupInfo,
    UCBGetOverlay1Info,
    UCBGetOverlay1FileInfo,
    UCBReportMessage,
    UCBAlert,
    UCBShowStatus,
    UCBUserBreak,
    UCBGetNamedPreferences,
    UCBStorePluginData,
    UCBGetPluginData,
    UCBSetModDate,
    UCBAddProjectEntry,
    UCBCreateNewTextDocument,
    UCBAllocateMemory,
    UCBFreeMemory,
    UCBAllocMemHandle,
    UCBFreeMemHandle,
    UCBGetMemHandleSize,
    UCBResizeMemHandle,
    UCBLockMemHandle,
    UCBUnlockMemHandle,
    {(void*)UCBSecretAttachHandle, (void*)UCBSecretDetachHandle, (void*)UCBSecretPeekHandle, NULL, NULL, NULL, NULL, NULL},
    UCBGetTargetName,
    UCBCacheAccessPathList,
    UCBPreDialog,
    UCBPostDialog,
    UCBPreFileAction,
    UCBPostFileAction,
    UCBCheckoutLicense,
    UCBCheckinLicense,
    UCBResolveRelativePath
};
// clang-format on

#if MWCCPS2
static CWIDEInfo sIDEInfo = {4, 2, 0, 0, 13}; // b210 0x5DFE24
#else
static CWIDEInfo sIDEInfo = {3, 3, 0, 0, 10};
#endif

CWPluginPrivateContext::CWPluginPrivateContext(SInt32 thePluginType, SInt32 totalSize) {
    if (totalSize <= 0) {
        totalSize = sizeof(CWPluginPrivateContext);
    }
    memset((void*)this, 0, totalSize);
    shellSignature = 'CWIE';
    pluginType = thePluginType;
    shellInfo = &sIDEInfo;
    callbacks = &sBasePluginCallbacks;
}

CWPluginPrivateContext::~CWPluginPrivateContext() {
    int x;

    if (accessPathList) {
        if (accessPathList->userPathCount) {
            for (x = 0; x < accessPathList->userPathCount; x++) {
                if (accessPathList->userPaths[x].subdirectories) {
                    xfree(accessPathList->userPaths[x].subdirectories);
                }
            }
        }
        if (accessPathList->systemPathCount) {
            for (x = 0; x < accessPathList->systemPathCount; x++) {
                if (accessPathList->systemPaths[x].subdirectories) {
                    xfree(accessPathList->systemPaths[x].subdirectories);
                }
            }
        }
        xfree(accessPathList->userPaths);
        xfree(accessPathList->systemPaths);
        xfree(accessPathList);
        accessPathList = NULL;
    }
}
