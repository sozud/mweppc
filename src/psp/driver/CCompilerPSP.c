#include <stdio.h>
#include <stdlib.h>
#include "compiler/CodeGen.h"
#include "compiler/CompilerTools.h"
#include "compiler/CParser.h"
#include "compiler/CPrep.h"
#include "driver/CWPlugins.h"
#include "driver/CWPluginsPrivate.h"
#include "driver/DropInCompilerLinker.h"
#include <ctype.h>
#include <string.h>

// 3.0.1's C/C++ compiler plugin glue (2.4.7: src/compiler/driver/CCompiler.c).
// The license calls and the option updates moved to other units.

// The 3.0.1 "C/C++ Compiler" panel: the 2.4.7 record plus C99 and bottom-up
// inlining, and a C string prefix file name
typedef struct PFrontEndC {
    SInt16 version;
    Boolean cplusplus;
    Boolean checkprotos;
    Boolean arm;
    Boolean trigraphs;
    Boolean onlystdkeywords;
    Boolean enumsalwaysint;
    Boolean mpwpointerstyle;
    unsigned char oldprefixname[32];
    Boolean ansistrict;
    Boolean mpwcnewline;
    Boolean wchar_type;
    Boolean enableexceptions;
    Boolean dontreusestrings;
    Boolean poolstrings;
    Boolean dontinline;
    Boolean useRTTI;
    Boolean multibyteaware;
    Boolean unsignedchars;
    Boolean autoinline;
    Boolean booltruefalse;
    Boolean direct_to_som;
    Boolean som_env_check;
    Boolean alwaysinline;
    SInt16 inlinelevel;
    Boolean ecplusplus;
    Boolean objective_c;
    Boolean defer_codegen;
    Boolean templateparser;
    Boolean c99;
    Boolean bottomupinline;
    char prefixname[256];
} PFrontEndC;

typedef struct PWarningC {
    SInt16 version;
    Boolean warn_illpragma;
    Boolean warn_emptydecl;
    Boolean warn_possunwant;
    Boolean warn_unusedvar;
    Boolean warn_unusedarg;
    Boolean warn_extracomma;
    Boolean pedantic;
    Boolean warningerrors;
    Boolean warn_hidevirtual;
    Boolean warn_implicitconv;
    Boolean warn_notinlined;
    Boolean warn_structclass;
} PWarningC;

typedef struct PGlobalOptimizer {
    SInt16 version;
    UInt8 optimizationlevel;
    UInt8 optfor;
    UInt8 reserved[8];
} PGlobalOptimizer;

extern void func_00435A90(void); // CodeGen: compiler init
extern void func_00435A80(void); // CodeGen: compiler term
extern void func_00434F30(void); // CodeGen: back end options
extern void CodeGen_UpdateOptimizerOptions(void);
extern void func_00439EA0(void); // optimizer options
extern void COS_SetPluginContext(CWPluginContext context);
extern void COS_ClearPluginContext(void);
extern char* func_004393A0(const CWFileSpec* spec); // the spec's file name
extern const char* COS_GetFileExtension(const char* path);
extern CW_CALLBACK CWSetPluginAPI(CWPluginContext context, SInt32 api);

SInt32 license_cookie; // 0x6258A0
static Boolean using_license_manager; // 0x5E8BA0
extern Boolean crippled;
extern CompilerLinkerParamBlk cparams;

static char extension[256];
static char target_name[128];
static PFrontEndC pFrontEnd;

static void SetupDefaultFrontEndPrefs(PFrontEndC* prefs);
static Boolean setup_param_block(CWPluginContext context);
static void initialize_compiler_options(CompilerLinkerParamBlk* params);

#pragma auto_inline off

// 0x438FD0 (placed with the code generator in 3.0.1)
void ReleaseLicense(CWPluginContext context) {
    if (license_cookie && using_license_manager) {
        CWCheckinLicense(context, license_cookie);
    }
    using_license_manager = FALSE;
    license_cookie = 0;
}

// 0x439010
#if MWCCPS2
__declspec(dllimport) int __stdcall GetSystemDefaultLangID(void);

// b210: feature "1" unless the system is Japanese; feature "3" for Japanese
// systems or when "1" is not licensed
void GetLicense(CWPluginContext context) {
    if (!license_cookie) {
#if MWCCPS2
        if ((unsigned short)GetSystemDefaultLangID() != 0x411) { // b210 compares the LANGID word
#else
        if (GetSystemDefaultLangID() != 0x411) {
#endif
            CWCheckoutLicense(context, "Win32_Plugins_MIPS_PS2", "1", 0, NULL, &license_cookie);
            if (license_cookie) {
                goto done;
            }
        }
        CWCheckoutLicense(context, "Win32_Plugins_MIPS_PS2", "3", 0, NULL, &license_cookie);
done:
        using_license_manager = TRUE;
    }
}
#else
void GetLicense(CWPluginContext context) {
    if (!license_cookie) {
        CWCheckoutLicense(context, "Win32_Plugins_MIPS_PSP", "1", 0, NULL, &license_cookie);
        using_license_manager = TRUE;
    }
}
#endif

#pragma auto_inline on

// inlined into MWC_main
static short store_compile_results(Boolean compiled) {
    CWResult result = cwErrRequestFailed;

    if (cparams.preprocess) {
        if (cparams.object.objectdata) {
            CWNewTextDocumentInfo docinfo;
            memset(&docinfo, 0, sizeof(CWNewTextDocumentInfo));
            docinfo.text = cparams.object.objectdata;
            cparams.object.objectdata = NULL;
            result = CWCreateNewTextDocument(cparams.context, &docinfo);
        }
    } else if (compiled) {
        result = CWStoreObjectData(cparams.context, cparams.whichfile, &cparams.object);
    }
    return result;
}

#pragma auto_inline off

// 0x4233A0
CWPLUGIN_ENTRY(MWC_main)(CWPluginContext context) {
    CWResult result;
    SInt32 request;

    result = cwNoErr;
    CWGetPluginRequest(context, &request);
    switch (request) {
        case reqInitialize:
            CWSetPluginAPI(context, 2);
            cparams.context = context;
            license_cookie = 0;
            func_00435A90();
            break;
        case reqTerminate:
            cparams.context = context;
            func_00435A80();
            ReleaseLicense(context);
            break;
        case reqCompile:
            COS_SetPluginContext(context);
            setup_param_block(context);
            GetLicense(context);
            if (license_cookie) {
                initialize_compiler_options(&cparams);
                func_00439EA0();
                func_00434F30();
                func_00439EA0();
                CodeGen_UpdateOptimizerOptions();
                result = C_Compiler(&cparams);
                result = store_compile_results(result);
            } else {
                result = cwErrSilent;
            }
            COS_ClearPluginContext();
            break;
    }
    return CWDonePluginRequest(context, result);
}

#pragma auto_inline on

// 0x423B10; also inlined into initialize_compiler_options
static void ConvertFrontEndPrefs(const PFrontEndC* src, PFrontEndC* dst) {
    int err = 0;

    if (src) {
        if (src->version > 0x10) {
            *dst = *src;
        } else {
            *dst = *src;
            err = 0;
        }
    }
    if (!src || err) {
        SetupDefaultFrontEndPrefs(dst);
    }
}

// the panel readers, inlined into initialize_compiler_options
static void GetFrontEndPrefs(PFrontEndC* prefs) {
    CWMemHandle prefshand;
    void* prefsptr;

    if (CWGetNamedPreferences(cparams.context, "C/C++ Compiler", &prefshand) == cwNoErr &&
        CWLockMemHandle(cparams.context, prefshand, FALSE, &prefsptr) == cwNoErr) {
        if (ConvertFrontEndPrefs) {
            ConvertFrontEndPrefs((const PFrontEndC*)prefsptr, prefs);
        } else {
            *prefs = *(const PFrontEndC*)prefsptr;
        }
        CWUnlockMemHandle(cparams.context, prefshand);
        CWFreeMemHandle(cparams.context, prefshand);
    } else if (ConvertFrontEndPrefs) {
        SetupDefaultFrontEndPrefs(prefs);
    } else {
        memset(prefs, 0, sizeof(PFrontEndC));
    }
}

static void GetWarningPrefs(PWarningC* prefs) {
    CWMemHandle prefshand;
    void* prefsptr;

    if (CWGetNamedPreferences(cparams.context, "C/C++ Warnings", &prefshand) == cwNoErr &&
        CWLockMemHandle(cparams.context, prefshand, FALSE, &prefsptr) == cwNoErr) {
        *prefs = *(const PWarningC*)prefsptr;
        CWUnlockMemHandle(cparams.context, prefshand);
        CWFreeMemHandle(cparams.context, prefshand);
    } else {
        memset(prefs, 0, sizeof(PWarningC));
    }
}

static void GetGlobalOptimizerPrefs(PGlobalOptimizer* prefs) {
    CWMemHandle prefshand;
    void* prefsptr;

#if MWCCPS2
    if (CWGetNamedPreferences(cparams.context, "PS2 Global Optimizer", &prefshand) == cwNoErr &&
#else
    if (CWGetNamedPreferences(cparams.context, "PSP Global Optimizer", &prefshand) == cwNoErr &&
#endif
        CWLockMemHandle(cparams.context, prefshand, FALSE, &prefsptr) == cwNoErr) {
        *prefs = *(const PGlobalOptimizer*)prefsptr;
        CWUnlockMemHandle(cparams.context, prefshand);
        CWFreeMemHandle(cparams.context, prefshand);
    } else {
        memset(prefs, 0, sizeof(PGlobalOptimizer));
    }
}

#pragma auto_inline off

// 0x423500
static void initialize_compiler_options(CompilerLinkerParamBlk* params) {
    PWarningC pWarningC;
    PGlobalOptimizer pGlobalOptimizer;
    const char* p;
    char* q;
    char* ext;

    memclrw(&copts, sizeof(CompilerLinkerOptions));

    GetFrontEndPrefs(&pFrontEnd);

    copts.cplusplus = TRUE;
    p = COS_GetFileExtension(func_004393A0(&params->sourcefile));
    q = extension;
    while (*p) {
        *q++ = tolower(*p++);
    }
    *q = 0;
    ext = extension;
    if (!strcmp(ext, ".c") || !strcmp(ext, ".h") || !strcmp(ext, ".pch")) {
        copts.cplusplus = pFrontEnd.cplusplus;
    }

    copts.checkprotos = pFrontEnd.checkprotos;
    copts.ARMscoping = pFrontEnd.arm;
    copts.trigraphs = pFrontEnd.trigraphs;
    copts.onlystdkeywords = pFrontEnd.onlystdkeywords;
    copts.enumsalwaysint = pFrontEnd.enumsalwaysint;
    copts.mpwc_relax = pFrontEnd.mpwpointerstyle;
    copts.ANSIstrict = pFrontEnd.ansistrict;
    copts.mpwc_newline = pFrontEnd.mpwcnewline;
    copts.exceptions = pFrontEnd.enableexceptions;
    copts.dont_reuse_strings = pFrontEnd.dontreusestrings;
    copts.poolstrings = pFrontEnd.poolstrings;
    copts.dontinline = pFrontEnd.dontinline;
    copts.RTTI = pFrontEnd.useRTTI;
    copts.oldprefixname = (StringPtr)pFrontEnd.prefixname;
    copts.multibyteaware = pFrontEnd.multibyteaware;
    copts.unsigned_char = pFrontEnd.unsignedchars;
    copts.auto_inline = pFrontEnd.autoinline;
    copts.direct_to_som = pFrontEnd.direct_to_som;
    copts.som_env_check = pFrontEnd.som_env_check;
    copts.booltruefalse = pFrontEnd.booltruefalse;
    copts.inlinelevel = pFrontEnd.inlinelevel;
    copts.wchar_type = pFrontEnd.wchar_type;
    copts.ecplusplus = pFrontEnd.ecplusplus;
    copts.defer_codegen = pFrontEnd.defer_codegen;
    if (pFrontEnd.templateparser) {
        copts.parse_func_templ = TRUE;
        copts.parse_mfunc_templ = TRUE;
        copts.warn_no_typename = TRUE;
    }
    copts.c9x = pFrontEnd.c99;
    copts.inline_bottom_up = pFrontEnd.bottomupinline;
    copts.unknown52 = 15;
    copts.inline_bottom_up_once = TRUE;
    copts.inlinemaxsize = 256;
    copts.inlinemaxtotalsize = 10000;

    GetWarningPrefs(&pWarningC);

    copts.warn_illpragma = pWarningC.warn_illpragma;
    copts.warn_emptydecl = pWarningC.warn_emptydecl;
    copts.warn_possunwant = pWarningC.warn_possunwant;
    copts.warn_unusedvar = pWarningC.warn_unusedvar;
    copts.warn_unusedarg = pWarningC.warn_unusedarg;
    copts.warn_extracomma = pWarningC.warn_extracomma;
    copts.pedantic = pWarningC.pedantic;
    copts.warningerrors = pWarningC.warningerrors;
    copts.warn_hidevirtual = pWarningC.warn_hidevirtual;
    copts.warn_implicitconv = pWarningC.warn_implicitconv;
    copts.warn_notinlined = pWarningC.warn_notinlined;
    copts.warn_structclass = pWarningC.warn_structclass;

    GetGlobalOptimizerPrefs(&pGlobalOptimizer);

    copts.optimizationlevel = pGlobalOptimizer.optimizationlevel;
    copts.optimizesize = (Boolean)(pGlobalOptimizer.optfor == 1);
    copts.crippled = crippled;
    copts.unrollfactor = 8;
    copts.unknownF6 = 8;
    copts.unrollinstrfactor = 100;
    copts.filesyminfo = params->SYMinfo;
    copts.appltype = 'CWIE';
    copts.headtype = 'MMCH';
    copts.texttype = 'TEXT';
}

// 0x423BA0
static Boolean setup_param_block(CWPluginContext context) {
    PluginTargetInfo2 tinfo;

    memset(&cparams, 0, sizeof(CompilerLinkerParamBlk));
    cparams.context = context;

    if (CWGetPluginRequest(context, &cparams.request) != cwNoErr) {
        return FALSE;
    }
    if (CWGetAPIVersion(context, &cparams.version) != cwNoErr) {
        return FALSE;
    }
    if (CWGetProjectFile(context, &cparams.targetfile) != cwNoErr) {
        return FALSE;
    }
    if (CWGetProjectFileCount(context, &cparams.numfiles) != cwNoErr) {
        return FALSE;
    }
    if (CWGetMainFileNumber(context, &cparams.whichfile) != cwNoErr) {
        return FALSE;
    }
    if (CWGetMainFileSpec(context, &cparams.sourcefile) != cwNoErr) {
        return FALSE;
    }
    if (CWGetMainFileText(context, &cparams.sourcetext, &cparams.sourcetextsize) != cwNoErr) {
        return FALSE;
    }
    if (CWIsPrecompiling(context, &cparams.precompile) != cwNoErr) {
        return FALSE;
    }
    if (CWIsAutoPrecompiling(context, &cparams.autoprecompile) != cwNoErr) {
        return FALSE;
    }
    if (CWIsPreprocessing(context, &cparams.preprocess) != cwNoErr) {
        return FALSE;
    }
    if (CWIsGeneratingDebugInfo(context, &cparams.SYMinfo) != cwNoErr) {
        return FALSE;
    }
    if (CWIsCachingPrecompiledHeaders(context, &cparams.caching_includes) != cwNoErr) {
        return FALSE;
    }
    if (CWGetBrowseOptions(context, &cparams.browseoptions) != cwNoErr) {
        return FALSE;
    }
    cparams.recordbrowseinfo = !cparams.preprocess;
    if (CWGetMainFileID(context, &cparams.browserfileID) != cwNoErr) {
        return FALSE;
    }
    if (CWGetTargetInfo(context, (CWTargetInfo*)&tinfo) != cwNoErr) {
        return FALSE;
    }

    cparams.targetOS = tinfo.targetOS;
    cparams.targetCPU = tinfo.targetCPU;
    cparams.idetargetname = target_name;
    if (CWGetTargetName(context, cparams.idetargetname, sizeof(target_name)) != cwNoErr) {
        return FALSE;
    }
    return TRUE;
}

// 0x423E00
static void SetupDefaultFrontEndPrefs(PFrontEndC* prefs) {
    memset(prefs, 0, sizeof(PFrontEndC));
    prefs->version = 0x10;
    prefs->cplusplus = FALSE;
    prefs->checkprotos = FALSE;
    prefs->arm = FALSE;
    prefs->trigraphs = FALSE;
    prefs->onlystdkeywords = FALSE;
    prefs->enumsalwaysint = FALSE;
    prefs->mpwpointerstyle = FALSE;
    prefs->prefixname[0] = 0;
    prefs->oldprefixname[0] = 0;
    prefs->ansistrict = FALSE;
    prefs->mpwcnewline = FALSE;
    prefs->wchar_type = TRUE;
    prefs->enableexceptions = TRUE;
    prefs->dontreusestrings = FALSE;
    prefs->poolstrings = FALSE;
    prefs->dontinline = FALSE;
    prefs->useRTTI = TRUE;
    prefs->multibyteaware = FALSE;
    prefs->unsignedchars = FALSE;
    prefs->autoinline = FALSE;
    prefs->booltruefalse = TRUE;
    prefs->inlinelevel = 0;
    prefs->ecplusplus = FALSE;
    prefs->defer_codegen = FALSE;
    prefs->templateparser = FALSE;
    prefs->c99 = FALSE;
    prefs->bottomupinline = TRUE;
}
