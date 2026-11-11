#include "driver/Projects.h"
#if MWCCPSP
#include "driver/PluginSpecs.h"
#endif
#include "driver/CWParserPluginsPrivate.h"
#include "driver/CWPlugins.h"
#include "driver/CWPluginsPrivate.h"
#include "driver/CWSecretPluginCallbacks.h"
#include "driver/Files.h"
#include "driver/Generic.h"
#include "driver/MacSpecs.h"
#include "driver/Memory.h"
#include "driver/MsDos.h"
#include "driver/oslib.h"
#include "driver/ParserErrors.h"
#include "driver/ParserFace.h"
#include <setjmp.h>
#include <string.h>

int GetFileCount(void) {
    SInt32 num;
    CWGetProjectFileCount(parseopts.context, &num);
    return num;
}

void SetFileOutputName(SInt32 position, SInt16 which, char* outfilename) {
    CWResult result;
    if (outfilename && outfilename[0]) {
        result =
            CWParserSetFileOutputName(parseopts.context, position, which ? which : 1, outfilename);
        if (result) {
            failedCallback = "CWParserSetFileOutputName";
            longjmp(exit_plugin, result);
        }
    }
}
#if MWCCPSP

#endif
#if MWCCPSP
#endif

int AddFileToProject(
    OSSpec* oss, SInt16 which, char* outfilename, Boolean exists, SInt32 position
) {
#if MWCCPSP
    OSSpec link;
    CWFileSpec cws;
    OSSpec spec;
    CWNewProjectEntryInfo pei;
    CWResult result;
    int err;

    // 3.0.1 follows links when the file must exist, and converts with
    // func_004246F0, which only reports an error in that case
    spec = *oss;
    err = 0;
    if (exists) {
        while (err < 50 && !OS_ResolveLink(&spec, &link)) {
            spec = link;
            err++;
        }
        err = func_004246F0(2, &spec, &cws);
        if (err) {
            CLPOSAlert(CLPStr44, err, OS_SpecToStringRelative(&spec, 0, STSbuf, sizeof(STSbuf)));
            return FALSE;
        }
    } else {
        func_004246F0(2, &spec, &cws);
    }
#else
    CWFileSpec cws;
    CWNewProjectEntryInfo pei;
    CWResult result;
    int err;

    err = OS_OSSpec_To_FSSpec(oss, &cws);
    if (err) {
        CLPOSAlert(CLPStr44, err, OS_SpecToStringRelative(oss, 0, STSbuf, sizeof(STSbuf)));
        return FALSE;
    }
#endif

    if (position == -2) {
        pei.position = 0;
    } else if (position == -1) {
        CWGetProjectFileCount(parseopts.context, &pei.position);
    } else if (position == 0) {
        pei.position = -1;
    } else {
        pei.position = position;
    }

    pei.segment = parseopts.currentSegment;
    pei.overlayGroup = parseopts.currentOverlayGroup;
    pei.overlay = parseopts.currentOverlay;
    pei.groupPath = NULL;
    pei.mergeintooutput = parseopts.mergeIntoOutput;
    pei.weakimport = parseopts.weakImport;
    pei.initbefore = parseopts.initBefore;
    result = CWAddProjectEntry(parseopts.context, &cws, !exists, &pei, &position);
    if (result) {
        failedCallback = "CWAddProjectEntry";
        longjmp(exit_plugin, result);
    }
    parseopts.mergeIntoOutput = parseopts.weakImport = parseopts.initBefore = FALSE;
    SetFileOutputName(position, which, outfilename);
    return TRUE;
}

Boolean GetFileInfo(SInt32 position, OSSpec* spec, char* plugin) {
    CWProjectFileInfo pfi;
    if (CWGetFileInfo(parseopts.context, position, TRUE, &pfi)) {
        return FALSE;
    }

#if MWCCPSP
#if CW_PLUGIN_PATHSPEC
    func_004246A0(2, &pfi.filespec, spec);
#else
    OS_FSSpec_To_OSSpec(&pfi.filespec, spec);
#endif
#else
    OS_FSSpec_To_OSSpec(&pfi.filespec, spec);
#endif
    strncpy(plugin, pfi.dropinname, 32);
    plugin[31] = 0;
    return TRUE;
}

int AddAccessPath(OSPathSpec* oss, SInt16 type, SInt32 position, Boolean recursive) {
    CWNewAccessPathInfo api;
    CWResult result;
    OSSpec spec;
    int err;
    CWAccessPathListInfo apli;

    if ((err = OS_MakeSpecWithPath(oss, NULL, FALSE, &spec)) ||
#if MWCCPSP
#if CW_PLUGIN_PATHSPEC
        (err = func_004246F0(2, &spec, &api.pathSpec))) {
#else
        (err = OS_OSSpec_To_FSSpec(&spec, &api.pathSpec))) {
#endif
#else
        (err = OS_OSSpec_To_FSSpec(&spec, &api.pathSpec))) {
#endif
        CLPOSAlert(CLPStr45, err, OS_PathSpecToString(&spec.path, STSbuf, sizeof(STSbuf)));
        return FALSE;
    }

    if (position == -2) {
        api.position = 0;
    } else if (position == -1 || position == 0) {
        api.position = -1;
    } else if (position == -1) {
        result = CWGetAccessPathListInfo(parseopts.context, &apli);
        if (result) {
            failedCallback = "CWGetAccessPathListInfo";
            longjmp(exit_plugin, result);
        }
        if ((type & 1) == 1) {
            api.position = apli.systemPathCount;
        } else {
            api.position = apli.userPathCount;
        }
    } else {
        api.position = position;
    }

    api.type = ((type & 1) == 0) ? cwUserPath : cwSystemPath;
    api.type |= (type & 2) ? cwAccessPathFlagKeepSystem : 0;
    api.recursive = recursive;

    result = CWParserAddAccessPath(parseopts.context, &api);
    if (result) {
        failedCallback = "CWParserAddAccessPath";
        longjmp(exit_plugin, result);
    }

    return TRUE;
}

int MoveSystemPathsIntoUserList(void) {
    CWResult result = CWParserSwapAccessPaths(parseopts.context);
    return result == cwNoErr;
}

void AddVirtualFile(const char* filename, Handle* text) {
    CWResult result;
    CWMemHandle mh;

    if (*text) {
        CWSecretAttachHandle(parseopts.context, *text, &mh);
        result = CWParserCreateVirtualFile(parseopts.context, filename, mh);
        if (result) {
            failedCallback = "CWParserCreateVirtualFile";
            longjmp(exit_plugin, result);
        }
        DisposeHandle(*text);
        *text = NULL;
    }
}

void GetOutputFileDirectory(OSPathSpec* dir) {
    CWResult result;
    CWFileSpec idefss;
    OSSpec spec;

    result = CWGetOutputFileDirectory(parseopts.context, &idefss);
    if (result) {
        failedCallback = "CWGetOutputFileDirectory";
        longjmp(exit_plugin, result);
    }

#if MWCCPSP
#if CW_PLUGIN_PATHSPEC
    func_004246A0(2, &idefss, &spec);
#else
    OS_FSSpec_To_OSSpec(&idefss, &spec);
#endif
#else
    OS_FSSpec_To_OSSpec(&idefss, &spec);
#endif
    *dir = spec.path;
}

void SetOutputFileDirectory(OSPathSpec* dir) {
    CWResult result;
    CWFileSpec idefss;
    OSSpec spec;

    OS_MakeSpecWithPath(dir, NULL, FALSE, &spec);
#if MWCCPSP
#if CW_PLUGIN_PATHSPEC
    func_004246F0(2, &spec, &idefss);
#else
    OS_OSSpec_To_FSSpec(&spec, &idefss);
#endif
#else
    OS_OSSpec_To_FSSpec(&spec, &idefss);
#endif

    result = CWParserSetOutputFileDirectory(parseopts.context, &idefss);
    if (result) {
        failedCallback = "CWParserSetOutputFileDirectory";
        longjmp(exit_plugin, result);
    }
}

void AddOverlayGroup(const char* name, CWAddr64* addr, SInt32* groupnum, SInt32* overlaynum) {
    CWResult result;

    *overlaynum = -1;
    result = CWParserAddOverlay1Group(parseopts.context, name, addr, groupnum);
    if (result) {
        if (result == cwErrInvalidCallback) {
            CLPReportError(CLPStr72);
        } else {
            failedCallback = "CWParserAddOverlay1Group";
            longjmp(exit_plugin, result);
        }
    }
}

void AddOverlay(SInt32 groupnum, const char* name, SInt32* overlaynum) {
    CWResult result;

    result = CWParserAddOverlay1(parseopts.context, name, groupnum, overlaynum);
    if (result) {
        if (result == cwErrInvalidCallback) {
            CLPReportError(CLPStr72);
        } else {
            failedCallback = "CWParserAddOverlay1";
            longjmp(exit_plugin, result);
        }
    }
}

void AddSegment(const char* name, SInt16 attrs, SInt32* segmentnum) {
    CWResult result;

    result = CWParserAddSegment(parseopts.context, name, attrs, segmentnum);
    if (result) {
        if (result == cwErrInvalidCallback) {
            CLPReportError(CLPStr73);
        } else {
            failedCallback = "CWParserAddSegment";
            longjmp(exit_plugin, result);
        }
    }
}

void ChangeSegment(SInt32 segmentnum, const char* name, SInt16 attrs) {
    CWResult result;

    result = CWParserSetSegment(parseopts.context, segmentnum, name, attrs);
    if (result) {
        if (result == cwErrInvalidCallback) {
            CLPReportError(CLPStr73);
        } else {
            failedCallback = "CWParserSetSegment";
            longjmp(exit_plugin, result);
        }
    }
}
