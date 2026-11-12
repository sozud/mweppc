#include "driver/CLPluginRequests.h"
#include "driver/CLDropinCallbacks_V10.h"
#include "driver/CLErrors.h"
#include "driver/CLFiles.h"
#include "driver/CLLicenses.h"
#include "driver/CLMain.h"
#include "driver/CLOverlays.h"
#include "driver/CLPlugins.h"
#include "driver/CLTarg.h"
#include "driver/cmdline.h"
#include "driver/CWPlugins.h"
#include "driver/CWPluginsPrivate.h"
#include "driver/DropInCompilerLinker.h"
#include "driver/Files.h"
#include "driver/Generic.h"
#include "driver/MacFileTypes.h"
#include "driver/MacSpecs.h"
#if MWCCPSP
#include "driver/MemUtils.h"
#include "driver/Memory.h"
#endif
#include "driver/MsDos.h"
#include "driver/oslib.h"
#include "driver/pref_structs.h"
#include <string.h>

extern "C" {
#if MWCCPSP
void func_0041AED0(Plugin* plugin); // after a plugin request
#define SHELL_HANDLE_BUCKETS 0x101

// these units build with -inline off; 3.0.1 has the hash written out
#define ShellHandleHash(ptr, isHandle) \
    ((((UInt32)(ptr) >> 24) ^ ((UInt32)(ptr) >> 16) ^ ((UInt32)(ptr) >> 8) ^ \
      ((UInt32)(ptr) + ((isHandle) != 0))) % \
     SHELL_HANDLE_BUCKETS)

// 0x41AA50
ShellHandleEntry* ShellHandles_Add(
    CWPluginContext context,
    void* ptr,
    Boolean keep2,
    Boolean isHandle,
    Boolean keep,
    Boolean hasMacHandle
) {
    shellContextType* sc = static_cast<shellContextType*>(context->shellContext);
    ShellHandleEntry* entry;
    UInt32 hash;

    if (!ptr) {
        return NULL;
    }
    entry = static_cast<ShellHandleEntry*>(xmalloc(NULL, sizeof(ShellHandleEntry)));
    entry->isHandle = isHandle;
    entry->keep = keep;
    entry->hasMacHandle = hasMacHandle;
    entry->keep2 = keep2;
    entry->ptr = ptr;
    hash = ShellHandleHash(ptr, isHandle);
    entry->next = sc->handles[hash];
    sc->handles[hash] = entry;
    return entry;
}

// 0x41AAF0
ShellHandleEntry* ShellHandles_Find(CWPluginContext context, void* ptr, Boolean isHandle) {
    shellContextType* sc = static_cast<shellContextType*>(context->shellContext);
    ShellHandleEntry* entry;

    if (!ptr) {
        return NULL;
    }
    for (entry = sc->handles[ShellHandleHash(ptr, isHandle)]; entry; entry = entry->next) {
        if (entry->isHandle == isHandle && ptr == entry->ptr) {
            return entry;
        }
    }
    return NULL;
}

// 0x41AB70
void ShellHandles_Remove(CWPluginContext context, void* ptr, Boolean isHandle) {
    shellContextType* sc = static_cast<shellContextType*>(context->shellContext);
    ShellHandleEntry* entry;
    ShellHandleEntry* prev;
    UInt32 hash;

    if (!ptr) {
        return;
    }
    hash = ShellHandleHash(ptr, isHandle);
    prev = NULL;
    for (entry = sc->handles[hash]; entry; entry = entry->next) {
        if (entry->isHandle == isHandle && ptr == entry->ptr) {
            if (prev) {
                prev->next = entry->next;
            } else {
                sc->handles[hash] = entry->next;
            }
            xfree(entry);
            return;
        }
        prev = entry;
    }
}

// 0x41A8A0: the plugin's target info back into the shell's
static void TargetInfo_FromPlugin(Plugin* plugin, PluginTargetInfo* src, ShellTargetInfo* dst) {
    dst->outputType = src->outputType;
    dst->linkType = src->linkType;
    dst->canRun = src->canRun;
    dst->canDebug = src->canDebug;
    dst->targetCPU = src->targetCPU;
    dst->targetOS = src->targetOS;
    dst->outfile = src->outfile;
    dst->symfile = src->symfile;
    dst->runfile = src->runfile;
    dst->linkAgainstFile = src->linkAgainstFile;
}

// 0x41AC00
void ShellHandles_Init(CWPluginContext context) {
    shellContextType* sc = static_cast<shellContextType*>(context->shellContext);
    sc->handles = static_cast<ShellHandleEntry**>(
        xcalloc(NULL, SHELL_HANDLE_BUCKETS * sizeof(ShellHandleEntry*))
    );
}

// 0x41AC20: frees what the plugin did not keep, then the table
void ShellHandles_Free(CWPluginContext context) {
    shellContextType* sc = static_cast<shellContextType*>(context->shellContext);
    ShellHandleEntry* entry;
    ShellHandleEntry* next;
    int i;

    if (!sc) {
        return;
    }
    for (i = 0; i < SHELL_HANDLE_BUCKETS; i++) {
        entry = sc->handles[i];
        if (entry) {
            for (; entry; entry = next) {
                next = entry->next;
                if (!entry->keep && !entry->keep2) {
                    if (entry->isHandle) {
                        if (entry->hasMacHandle) {
                            OS_DestroyMacHandle(
                                OS_MacHandleFromOSHandle(static_cast<OSHandle*>(entry->ptr)),
                                static_cast<OSHandle*>(entry->ptr)
                            );
                        }
                        OS_FreeHandle(static_cast<OSHandle*>(entry->ptr));
                        xfree(entry->ptr);
                    } else {
                        xfree(entry->ptr);
                    }
                    xfree(entry);
                }
            }
        }
        sc->handles[i] = NULL;
    }
    xfree(sc->handles);
}
#endif
#if MWCCPSP

// 0x41AED0: after a request, what the plugin did not keep is freed
void func_0041AED0(Plugin* plugin) {
    if (!plugin->context) {
        return;
    }
    ShellHandles_Free(plugin->context);
}

#endif
static CW_CALLBACK CallPlugin(Plugin* plugin) {
    return Plugin_Call(plugin, plugin->context);
}

#if MWCCPSP
// 0x41AD00
static void SetupPluginContext(Plugin* plugin, CWResult request) {
    OSSpec spec;
    shellContextType* sc;
    OSPathSpec cwd;

    sc = static_cast<shellContextType*>(plugin->context->shellContext);
    sc->plugin = plugin;
    sc->userAccessPathsChanged = sc->systemAccessPathsChanged = TRUE;
    ShellHandles_Init(plugin->context);
    plugin->context->request = request;
    plugin->context->apiVersion = 13;
    plugin->context->pluginStorage = NULL;

    OS_MakeFileSpec("Project.mcp", &spec);
    plugin->context->projectFile = spec;
    if (gTarg) {
        OS_MakeSpecWithPath(&gTarg->outputDirectory, NULL, FALSE, &spec);
        plugin->context->outputFileDirectory = spec;
    } else {
        OS_GetCWD(&cwd);
        OS_MakeSpecWithPath(&cwd, NULL, FALSE, &spec);
        plugin->context->outputFileDirectory = spec;
    }

    plugin->context->shellSignature = 'CWIE';
    plugin->context->pluginType = Plugin_GetDropInFlags(plugin)->dropintype;

    if (gTarg) {
        plugin->context->numFiles = Files_Count(&gTarg->files);
    } else {
        plugin->context->numFiles = 0;
    }

    if (gTarg) {
        plugin->context->numOverlayGroups = Overlays_CountGroups(&gTarg->linkage.overlays);
    } else {
        plugin->context->numOverlayGroups = 0;
    }

    plugin->context->callbackOSError = plugin->context->pluginOSError = noErr;
    plugin->context->accessPathList = NULL;
    plugin->context->targetDataDirectorySpec = (CWFileSpec*)&plugin->context->outputFileDirectory;
}
#else
static void SetupPluginContext(Plugin* plugin, CWResult request) {
    OSSpec spec;
    shellContextType* sc;
    OSPathSpec cwd;

    sc = static_cast<shellContextType*>(plugin->context->shellContext);
    sc->plugin = plugin;
    sc->userAccessPathsChanged = sc->systemAccessPathsChanged = TRUE;
    plugin->context->request = request;
    plugin->context->apiVersion = 12;
    plugin->context->pluginStorage = NULL;

    OS_MakeFileSpec("Project.mcp", &spec);
    OS_OSSpec_To_FSSpec(&spec, &plugin->context->projectFile);
    if (gTarg) {
        OS_MakeSpecWithPath(&gTarg->outputDirectory, NULL, FALSE, &spec);
        OS_OSSpec_To_FSSpec(&spec, &plugin->context->outputFileDirectory);
    } else {
        OS_GetCWD(&cwd);
        OS_MakeSpecWithPath(&cwd, NULL, FALSE, &spec);
        OS_OSSpec_To_FSSpec(&spec, &plugin->context->outputFileDirectory);
    }

    plugin->context->shellSignature = 'CWIE';
    plugin->context->pluginType = Plugin_GetDropInFlags(plugin)->dropintype;

    if (gTarg) {
        plugin->context->numFiles = Files_Count(&gTarg->files);
    } else {
        plugin->context->numFiles = 0;
    }

    if (gTarg) {
        plugin->context->numOverlayGroups = Overlays_CountGroups(&gTarg->linkage.overlays);
    } else {
        plugin->context->numOverlayGroups = 0;
    }

    plugin->context->callbackOSError = plugin->context->pluginOSError = noErr;
    plugin->context->accessPathList = NULL;
}
#endif
}

Boolean SendParserRequest(
    Plugin* plugin,
    Target* target,
    CWCommandLineArgs* args,
    OSType cpu,
    OSType os,
    int numPlugins,
    const CLPluginInfo* pluginInfo,
    int numPanels,
    const char** panelNames,
    CWCommandLineArgs* plugin_args,
    CWCommandLineArgs* panel_args,
    const char* build_date,
    const char* build_time,
    const ToolVersionInfo* build_tool
) {
    int result;
    CWParserContext* pc;

    SetupPluginContext(plugin, 1);

    pc = static_cast<CWParserContext*>(plugin->context);
    pc->build_date = build_date;
    pc->build_time = build_time;
    pc->build_tool = build_tool;
    pc->args = args;
    pc->cpu = cpu;
    pc->os = os;
    pc->numPlugins = numPlugins;
    pc->plugins = pluginInfo;
    pc->numPanels = numPanels;
    pc->panelNames = panelNames;
    pc->plugin_args = plugin_args;
    pc->panel_args = panel_args;

#if MWCCPSP
    result = CallPlugin(plugin);
    func_0041AED0(plugin);
    if (result != cwNoErr) {
        return FALSE;
    }
#else
    if (CallPlugin(plugin) != cwNoErr) {
        return FALSE;
    }
#endif
    if (!SendTargetInfoRequest(target, target->linker, target->linkerDropinFlags)) {
        return FALSE;
    }

    target->linkmodel = (target->targetinfo->linkType == exelinkageSegmented) ? LinkModel_Segmented
        : (target->targetinfo->linkType == exelinkageOverlay1)                ? LinkModel_Overlay1
                                                                              : LinkModel_Flat;

    if (!Plugin_VerifyPanels(plugin)) {
        return FALSE;
    }

    SetupPluginContext(plugin, 2);
    pc->args = args;
    pc->cpu = cpu;
    pc->os = os;
    pc->numPlugins = numPlugins;
    pc->plugins = pluginInfo;
    pc->numPanels = numPanels;
    pc->panelNames = panelNames;
    pc->plugin_args = plugin_args;
    pc->panel_args = panel_args;

    result = CallPlugin(plugin);
#if MWCCPSP
    func_0041AED0(plugin);
#endif
    return (result == cwNoErr);
}

#if MWCCPSP
extern "C" {
// 0x41C0E0: the object file is up to date (compile-and-link skips the compile)
extern Boolean func_0041C0E0(File* file);
// 0x41A9C0: copies the target info in the plugin's API version
// 0x41A9C0: the shell's target info into the plugin's
static void TargetInfo_ToPlugin(Plugin* plugin, ShellTargetInfo* src, PluginTargetInfo* dst) {
    dst->outputType = src->outputType;
    dst->linkType = src->linkType;
    dst->canRun = src->canRun;
    dst->canDebug = src->canDebug;
    dst->targetCPU = src->targetCPU;
    dst->targetOS = src->targetOS;
    dst->outfile = src->outfile;
    dst->symfile = src->symfile;
    dst->runfile = src->runfile;
    dst->linkAgainstFile = src->linkAgainstFile;
}
#define func_0041A9C0 TargetInfo_ToPlugin
}

// 0x41B080
Boolean SendCompilerRequest(Plugin* plugin, File* file, SInt16 stage) {
    CWResult result;
    CWCompilerLinkerContext* cc;
    Boolean precompiling;
    SInt16 type;
    OSType mactype;
    const char* pn;
    int detailed;

    if (stage & CmdLineStageMask_Cg) {
        if (optsCompiler.forcePrecompile == 1) {
            precompiling = TRUE;
        } else if (optsCompiler.forcePrecompile == 2) {
            precompiling = FALSE;
        } else {
            precompiling = (file->mappingflags & kPrecompile) != 0;
        }
    } else {
        precompiling = FALSE;
    }

    if (optsCmdLine.verbose) {
        pn = Plugin_GetDropInName(plugin);
        detailed = optsCmdLine.verbose > 1;
        if (stage & CmdLineStageMask_Pp) {
            CLReport(CLStr34 + detailed, file->srcfilename, pn);
        } else if (stage & CmdLineStageMask_Cg) {
            if (precompiling) {
                CLReport(CLStr32 + detailed, file->srcfilename, pn);
            } else {
                CLReport(CLStr30 + detailed, file->srcfilename, pn);
            }
        } else if (stage & CmdLineStageMask_Dp) {
            CLReport(CLStr36 + detailed, file->srcfilename, pn);
        } else if (stage & CmdLineStageMask_Ds) {
            CLReport(CLStr46 + detailed, file->srcfilename, pn);
        } else if (!stage) {
            CLReport(CLStr48 + detailed, file->srcfilename, pn);
        }
    }

    if (optsCmdLine.state == OptsCmdLineState_CompileAndLink &&
        (stage & CmdLineStageMask_Cg) &&
        !precompiling &&
        (Plugin_CL_GetObjectFlags(plugin)->flags & 0x40000000) &&
        func_0041C0E0(file)) {
        file->codesize = file->udatasize = file->idatasize = file->compiledlines = 0;
        file->objectOnDisk = TRUE;
        return TRUE;
    }

    if (!Plugin_VerifyPanels(plugin)) {
        return FALSE;
    }

    SetupPluginContext(plugin, reqCompile);
    cc = static_cast<CWCompilerLinkerContext*>(plugin->context);
    func_0041A9C0(plugin, gTarg->targetinfo, cc->targetinfo);

    cc->sourcefile = file->srcfss;
    if (!OS_GetMacFileType(&file->srcfss, &mactype) && mactype == 'TEXT') {
        // the shell spec: UCBGetFileText takes it in 3.0.1
        if (UCBGetFileText(
                cc, (const CWFileSpec*)&cc->sourcefile, &cc->sourcetext, &cc->sourcetextsize, &type
            ) != cwNoErr) {
            return FALSE;
        }
    } else {
        cc->sourcetext = NULL;
        cc->sourcetextsize = 0;
    }

    cc->whichfile = file->filenum;
    cc->debuginfo = optsCmdLine.debugInfo;
    cc->recordbrowseinfo = file->recordbrowseinfo;
    cc->fileID = file->browseFileID;
    memcpy(&cc->browseoptions, &file->browseoptions, sizeof(cc->browseoptions));

    if (stage & (CmdLineStageMask_Pp | CmdLineStageMask_Dp)) {
        if (stage & CmdLineStageMask_Pp) {
            cc->preprocess = TRUE;
        } else {
            cc->preprocess = 2;
        }
    } else {
        cc->preprocess = FALSE;
    }

    if ((stage & CmdLineStageMask_Cg) && precompiling) {
        cc->precompile = TRUE;
        cc->autoprecompile = TRUE;
    } else {
        cc->precompile = FALSE;
        cc->autoprecompile = FALSE;
    }

    if (!(stage & (CmdLineStageMask_Pp | CmdLineStageMask_Cg | CmdLineStageMask_Dp))) {
        cc->preprocess = 2;
        cc->precompile = 2;
    }

    result = CallPlugin(plugin);

    if (cc->sourcetext) {
        UCBReleaseFileText(cc, cc->sourcetext);
        cc->sourcetext = NULL;
        cc->sourcetextsize = 0;
    }

    func_0041AED0(plugin);
    return (result == cwNoErr);
}
#else
Boolean SendCompilerRequest(Plugin* plugin, File* file, SInt16 stage) {
    CWResult result;
    CWCompilerLinkerContext* cc;
    Boolean precompiling;
    SInt16 type;
    OSType mactype;
    const char* pn;
    int detailed;

    if (stage & CmdLineStageMask_Cg) {
        if (optsCompiler.forcePrecompile == 1) {
            precompiling = TRUE;
        } else if (optsCompiler.forcePrecompile == 2) {
            precompiling = FALSE;
        } else {
            precompiling = (file->mappingflags & kPrecompile) != 0;
        }
    } else {
        precompiling = FALSE;
    }

    if (optsCmdLine.verbose) {
        pn = Plugin_GetDropInName(plugin);
        detailed = optsCmdLine.verbose > 1;
        if (stage & CmdLineStageMask_Pp) {
            CLReport(CLStr34 + detailed, file->srcfilename, pn);
        } else if (stage & CmdLineStageMask_Cg) {
            if (precompiling) {
                CLReport(CLStr32 + detailed, file->srcfilename, pn);
            } else {
                CLReport(CLStr30 + detailed, file->srcfilename, pn);
            }
        } else if (stage & CmdLineStageMask_Dp) {
            CLReport(CLStr36 + detailed, file->srcfilename, pn);
        } else if (stage & CmdLineStageMask_Ds) {
            CLReport(CLStr46 + detailed, file->srcfilename, pn);
        } else if (!stage) {
            CLReport(CLStr48 + detailed, file->srcfilename, pn);
        }
    }

    if (!Plugin_VerifyPanels(plugin)) {
        return FALSE;
    }

    SetupPluginContext(plugin, reqCompile);
    cc = static_cast<CWCompilerLinkerContext*>(plugin->context);
    *cc->targetinfo = *gTarg->targetinfo;

    OS_OSSpec_To_FSSpec(&file->srcfss, &cc->sourcefile);
    if (!OS_GetMacFileType(&file->srcfss, &mactype) && mactype == 'TEXT') {
        if (UCBGetFileText(cc, &cc->sourcefile, &cc->sourcetext, &cc->sourcetextsize, &type) !=
            cwNoErr) {
            return FALSE;
        }
    } else {
        cc->sourcetext = NULL;
        cc->sourcetextsize = 0;
    }

    cc->whichfile = file->filenum;
    cc->debuginfo = optsCmdLine.debugInfo;
    cc->recordbrowseinfo = file->recordbrowseinfo;
    cc->fileID = file->browseFileID;
    memcpy(&cc->browseoptions, &file->browseoptions, sizeof(cc->browseoptions));

    if (stage & (CmdLineStageMask_Pp | CmdLineStageMask_Dp)) {
        if (stage & CmdLineStageMask_Pp) {
            cc->preprocess = TRUE;
        } else {
            cc->preprocess = 2;
        }
    } else {
        cc->preprocess = FALSE;
    }

    if ((stage & CmdLineStageMask_Cg) && precompiling) {
        cc->precompile = TRUE;
        cc->autoprecompile = TRUE;
    } else {
        cc->precompile = FALSE;
        cc->autoprecompile = FALSE;
    }

    if (!(stage & (CmdLineStageMask_Pp | CmdLineStageMask_Cg | CmdLineStageMask_Dp))) {
        cc->preprocess = 2;
        cc->precompile = 2;
    }

    result = CallPlugin(plugin);

    if (cc->sourcetext) {
        UCBReleaseFileText(cc, cc->sourcetext);
        cc->sourcetext = NULL;
        cc->sourcetextsize = 0;
    }

    return (result == cwNoErr);
}
#endif

#if MWCCPSP
// 0x41B450
Boolean SendTargetInfoRequest(Target* targ, Plugin* linker, SInt32 dropinflags) {
    CWResult result;
    CWCompilerLinkerContext* cc;
    OSSpec spec;

    if (linker && !(dropinflags & isExecutableTool)) {
        if (optsCmdLine.verbose > 1) {
            CLReport(CLStr50, Plugin_GetDropInName(linker));
        }
        OS_ASSERT(474, Plugin_GetPluginType(linker) == CWDROPINLINKERTYPE);

        if (!Plugin_VerifyPanels(linker)) {
            return FALSE;
        }

        SetupPluginContext(linker, reqTargetInfo);
        cc = static_cast<CWCompilerLinkerContext*>(linker->context);
        TargetInfo_ToPlugin(linker, gTarg->targetinfo, cc->targetinfo);
        result = CallPlugin(linker);
        TargetInfo_FromPlugin(linker, cc->targetinfo, gTarg->targetinfo);
        func_0041AED0(linker);
    } else {
        OS_MakeFileSpec("(unknown file)", &spec);
        targ->targetinfo->outputType = linkOutputFile;
        targ->targetinfo->outfile = spec;
        targ->targetinfo->symfile = spec;
        targ->targetinfo->runfile = spec;
        targ->targetinfo->linkType = exelinkageFlat;
        targ->targetinfo->canRun = FALSE;
        targ->targetinfo->canDebug = FALSE;
        targ->targetinfo->targetCPU = targ->cpu;
        targ->targetinfo->targetOS = targ->os;
        result = cwNoErr;
    }

    return (result == cwNoErr);
}

// 0x41B5C0
Boolean SendLinkerRequest(Plugin* plugin, SInt32 dropinflags, ShellTargetInfo* targetInfo) {
    CWResult result;
    CWCompilerLinkerContext* cc;
    const char* nm;
    int detailed;

    if (optsCmdLine.verbose) {
        nm = Plugin_GetDropInName(plugin);
        detailed = optsCmdLine.verbose > 1;
        if (dropinflags & isPostLinker) {
            CLReport(CLStr44 + detailed, nm);
        } else if (dropinflags & isPreLinker) {
            CLReport(CLStr42 + detailed, nm);
        } else {
            CLReport(CLStr40 + detailed, nm);
        }
    }

    if (!Plugin_VerifyPanels(plugin)) {
        return FALSE;
    }

    SetupPluginContext(plugin, reqLink);
    cc = static_cast<CWCompilerLinkerContext*>(plugin->context);
    TargetInfo_ToPlugin(plugin, targetInfo, cc->targetinfo);
    result = CallPlugin(plugin);
    TargetInfo_FromPlugin(plugin, cc->targetinfo, targetInfo);
    func_0041AED0(plugin);

    return (result == cwNoErr);
}
#else
Boolean SendTargetInfoRequest(Target* targ, Plugin* linker, SInt32 dropinflags) {
    CWResult result;
    CWCompilerLinkerContext* cc;
    OSSpec spec;
    CWFileSpec fss;

    if (linker && !(dropinflags & isExecutableTool)) {
        if (optsCmdLine.verbose > 1) {
            CLReport(CLStr50, Plugin_GetDropInName(linker));
        }
        OS_ASSERT(298, Plugin_GetPluginType(linker) == CWDROPINLINKERTYPE);

        if (!Plugin_VerifyPanels(linker)) {
            return FALSE;
        }

        SetupPluginContext(linker, reqTargetInfo);
        cc = static_cast<CWCompilerLinkerContext*>(linker->context);
        *cc->targetinfo = *targ->targetinfo;
        result = CallPlugin(linker);
        *targ->targetinfo = *cc->targetinfo;
    } else {
        OS_MakeFileSpec("(unknown file)", &spec);
        OS_OSSpec_To_FSSpec(&spec, &fss);
        targ->targetinfo->outputType = linkOutputFile;
        targ->targetinfo->outfile = fss;
        targ->targetinfo->symfile = fss;
        targ->targetinfo->runfile = fss;
        targ->targetinfo->linkType = exelinkageFlat;
        targ->targetinfo->canRun = FALSE;
        targ->targetinfo->canDebug = FALSE;
        targ->targetinfo->targetCPU = targ->cpu;
        targ->targetinfo->targetOS = targ->os;
        targ->targetinfo->outfileCreator = 'CWIE';
        targ->targetinfo->outfileType = 'APPL';
        targ->targetinfo->debuggerCreator = 'MWDB';
        targ->targetinfo->runHelperCreator = '?\?\?\?';
        result = cwNoErr;
    }

    return (result == cwNoErr);
}

Boolean SendLinkerRequest(Plugin* plugin, SInt32 dropinflags, CWTargetInfo* targetInfo) {
    CWResult result;
    CWCompilerLinkerContext* cc;
    const char* nm;
    int detailed;

    if (optsCmdLine.verbose) {
        nm = Plugin_GetDropInName(plugin);
        detailed = optsCmdLine.verbose > 1;
        if (dropinflags & isPostLinker) {
            CLReport(CLStr44 + detailed, nm);
        } else if (dropinflags & isPreLinker) {
            CLReport(CLStr42 + detailed, nm);
        } else {
            CLReport(CLStr40 + detailed, nm);
        }
    }

    if (!Plugin_VerifyPanels(plugin)) {
        return FALSE;
    }

    SetupPluginContext(plugin, reqLink);
    cc = static_cast<CWCompilerLinkerContext*>(plugin->context);
    *cc->targetinfo = *targetInfo;
    result = CallPlugin(plugin);
    *targetInfo = *cc->targetinfo;

    return (result == cwNoErr);
}
#endif

Boolean SendDisassemblerRequest(Plugin* linker, File* file) {
    CWResult result;
    Plugin* plugin;

    if (linker == NULL) {
        plugin = file->compiler;
    } else {
        plugin = linker;
    }

    if (optsCmdLine.verbose) {
        CLReport(
            (optsCmdLine.verbose > 1) + CLStr46, file->srcfilename, Plugin_GetDropInName(linker)
        );
    }

    if (!Plugin_VerifyPanels(plugin)) {
        return FALSE;
    }

    if (!linker) {
        SetupPluginContext(plugin, reqCompDisassemble);
        static_cast<CWCompilerLinkerContext*>(plugin->context)->whichfile = file->filenum;
        result = CallPlugin(plugin);
    } else {
        SetupPluginContext(plugin, reqDisassemble);
        static_cast<CWCompilerLinkerContext*>(plugin->context)->whichfile = file->filenum;
        result = CallPlugin(plugin);
    }

    return (result == cwNoErr);
}

#if MWCCPSP
inline void* operator new(size_t, void* where) {
    return where;
}

// 3.0.1: the context is allocated with xmalloc and constructed in place; at
// termination it is destroyed and freed. A plugin of an unknown kind gets no
// context (and the shell context is then stored through it, as in the
// original).
Boolean SendInitOrTermRequest(Plugin* plugin, Boolean reqIsInitialize) {
    CWResult result;
    OSType ty;
    shellContextType* sc;

    result = cwErrRequestFailed;
    if (plugin) {
        if (clState.pluginDebug) {
            CLReport(reqIsInitialize ? CLStr51 : CLStr52, Plugin_GetDropInName(plugin));
        }

        ty = Plugin_GetDropInFlags(plugin)->dropintype;
        if (reqIsInitialize) {
            if (ty == CWDROPINDRIVERTYPE) {
                plugin->context = new (xmalloc(NULL, sizeof(CWPluginPrivateContext)))
                    CWPluginPrivateContext(ty, 0);
            } else if (ty == CWDROPINPARSERTYPE) {
                plugin->context = new (xmalloc(NULL, sizeof(CWParserContext))) CWParserContext;
            } else if (ty == CWDROPINCOMPILERTYPE || ty == CWDROPINLINKERTYPE) {
                plugin->context =
                    new (xmalloc(NULL, sizeof(CWCompilerLinkerContext))) CWCompilerLinkerContext;
            } else {
                plugin->context = NULL;
            }

            sc = static_cast<shellContextType*>(xmalloc(NULL, sizeof(shellContextType)));
            plugin->context->shellContext = sc;
            sc = static_cast<shellContextType*>(plugin->context->shellContext);
            sc->plugin = NULL;
            sc->pluginAPI = 0;
            sc->userAccessPathsChanged = FALSE;
            sc->systemAccessPathsChanged = FALSE;
            sc->handles = NULL;
            static_cast<shellContextType*>(plugin->context->shellContext)->pluginAPI = 2;
        }

        if (plugin->context) {
            SetupPluginContext(plugin, reqIsInitialize ? reqInitialize : reqTerminate);
            result = CallPlugin(plugin);
            func_0041AED0(plugin);
        } else {
            result = cwNoErr;
        }

        if (!reqIsInitialize) {
            License_AutoCheckin();
            if (plugin->context) {
                if (ty == CWDROPINDRIVERTYPE) {
                    plugin->context->~CWPluginPrivateContext();
                } else if (ty == CWDROPINPARSERTYPE) {
                    static_cast<CWParserContext*>(plugin->context)->~CWParserContext();
                } else if (ty == CWDROPINCOMPILERTYPE || ty == CWDROPINLINKERTYPE) {
                    static_cast<CWCompilerLinkerContext*>(plugin->context)
                        ->~CWCompilerLinkerContext();
                }
                xfree(plugin->context);
            }
        }
    }

    return (result == cwNoErr);
}
#else
Boolean SendInitOrTermRequest(Plugin* plugin, Boolean reqIsInitialize) {
    CWResult result;
    OSType ty;

    result = cwErrRequestFailed;

    if (plugin) {
        if (clState.pluginDebug) {
            CLReport(reqIsInitialize ? CLStr51 : CLStr52, Plugin_GetDropInName(plugin));
        }

        if (reqIsInitialize) {
            ty = Plugin_GetDropInFlags(plugin)->dropintype;
            if (ty == CWDROPINDRIVERTYPE) {
                plugin->context = new CWPluginPrivateContext(ty);
            } else if (ty == CWDROPINPARSERTYPE) {
                plugin->context = new CWParserContext;
            } else if (ty == CWDROPINCOMPILERTYPE || ty == CWDROPINLINKERTYPE) {
                plugin->context = new CWCompilerLinkerContext;
            } else {
                plugin->context = NULL;
            }

            plugin->context->shellContext = new shellContextType;
        }

        if (plugin->context) {
            SetupPluginContext(plugin, reqIsInitialize ? reqInitialize : reqTerminate);
            result = CallPlugin(plugin);
        } else {
            result = cwNoErr;
        }

        if (!reqIsInitialize) {
            License_AutoCheckin();
        }
    }

    return (result == cwNoErr);
}
#endif

void AddFileTypeMappingList(OSFileTypeMappings** list, const OSFileTypeMappingList* entry) {
    OS_AddFileTypeMappingList(list, entry);
}
