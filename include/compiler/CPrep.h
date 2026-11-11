#ifndef COMPILER_CPREP_H
#define COMPILER_CPREP_H

#include "compiler/common.h"
#include "compiler/tokens.h"
#include "driver/CWPlugins.h"
#include "driver/DropInCompilerLinker.h"
#include "driver/Files.h"
#if MWCCPSP
#include "driver/COSToolsCLT.h"
#endif
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !MWCCPSP // 3.0.1: natural alignment (is_nextchar, poptokenseq)
#pragma pack(push, 2)
#endif
extern SInt16 tk;
extern GList pplist;
#define OPT_OFFSET(optname) ((short)offsetof(CompilerLinkerOptions, optname))
#define OPT_OFFSET_MASK 0x1FFF
enum {
    TK_MACRO_ARG_EXPANDED = 1,
    TK_MACRO_ARG_PASTED = 2,
    TK_MACRO_ARG_HASH = 3,
    TK_MACRO_NOEXPAND = 4,
    TK_MACRO_BOUNDARY = 5
};
#define OPT_FLAG_LIST 0x2000
#define OPT_FLAG_NOPRAGMA 0x4000
#define OPT_FLAG_RECONFIGURE 0x8000
struct CompilerLinkerParamBlk {
    struct CWPluginPrivateContext* context;
    CWObjectData object;
    Handle objectdata;
    Handle browsedata;
    SInt32 request;
    SInt32 version;
    CWFileSpec targetfile;
    SInt32 numfiles;
    SInt32 whichfile;
    CWFileSpec sourcefile;
    const char* sourcetext;
    SInt32 sourcetextsize;
    Boolean precompile;
    Boolean autoprecompile;
    Boolean preprocess;
    Boolean SYMinfo;
    Boolean caching_includes;
    CWBrowseOptions browseoptions;
    Boolean recordbrowseinfo;
    SInt16 browserfileID;
    SInt32 targetOS;
    SInt32 targetCPU;
    char* idetargetname;
};
struct Macro {
    struct Macro* next;
    struct HashNameNode* name;
    char* text;
    UInt16 nargs;
#if MWCCPSP
    // 3.0.1 swaps these (CPrep_InsertSpecialMacro sets +0xF, insertmacro
    // clears +0xE)
    UInt8 isExpanding;
    Boolean is_special;
#else
    Boolean is_special;
    UInt8 isExpanding;
#endif
    struct HashNameNode* names[1];
};
struct TokenStack {
    char* pos;
    char* macrostart;
    struct Macro* macro;
    Boolean macrocheck;
};
struct CPrepFileInfo {
    CWFileSpec textfile;
#if MWCCPSP
    char* nameNode; // 3.0.1: the path (CTool_GetPathName)
#else
    struct HashNameNode* nameNode;
#endif
#if MWCCPSP
    struct COSFileTime fileModDate; // 3.0.1: no linedelta
#else
    SInt32 fileModDate;
    SInt32 linedelta;
#endif
    char* textbuffer;
    SInt32 textlength;
    SInt32 linenumber;
    SInt32 pos;
    Boolean hasprepline;
    SInt16 fileID;
    Boolean recordbrowseinfo;
    Boolean unkfield123;
    Boolean unkfield124;
    Boolean unkfield125;
    Boolean isSystemInclude;
};
#if !MWCCPSP
#pragma pack(pop)
#endif
extern CompilerLinkerParamBlk* cparamblkptr;
extern CInt64 tkintconst;
extern struct Float tkfloatconst;
extern char* tkstring;
extern struct HashNameNode* tkidentifier;
extern SInt32 tksize;
extern short ispascalstring;
extern SInt16 nlflag;
extern SInt32 lines;
extern UInt8 spaceskip;
extern struct Macro** macrohashtable;
extern UInt8 cprep_nomem_exit;
extern UInt8 cprep_nostring;
extern UInt8 cprep_eoltokens;
#if MWCCPSP
extern TokenStack tokenstack[256]; // 3.0.1 nests macros 256 deep
#else
extern TokenStack tokenstack[128];
#endif
extern SInt16 tokenstacklevel;
extern char* pos;
extern char* macropos;
extern char* nextcharpos;
extern unsigned char CPrep_SkipNewCommentChar;
extern UInt8 preprocessing_only;
extern Handle stringmem;
extern SInt32 maxstringsize;
extern UInt8 cprep_idarray[256];
extern UInt8 was_escchar;
extern UInt8 macrocheck;
extern UInt8 widestring;
extern unsigned char at_linestart;
extern char* prep_file_start;
extern char* prep_file_end;
extern UInt8 cprep_strconcat;
extern struct CPrepFileInfo* prep_file;
extern short filesp;
extern SInt32 linenumber;
extern struct TStreamElement* ts_last;
extern struct TStreamElement* ts_current;
extern SInt32 ts_preread_elements;

extern void CPrep_InsertSpecialMacro(Macro* macro, const char* name);
extern void CPrep_InsertSpecialMacros(void);
extern void CPrep_RemoveSpecialMacros(void);
extern void CPrep_TSBufferGrow(SInt32 amount);
extern void CPrep_TokenStreamGetState(SInt32* state);
extern void CPrep_TokenStreamSetState(SInt32* state);
extern void CPrep_UnLex(void);
extern void CPrep_TokenStreamSetCurState(SInt32* state);
typedef void (*CPrepStreamFuncPtr)(TStreamElement*);
extern void CPrep_StreamGetBlock(TokenStream* stream, CPrepStreamFuncPtr func, int unused);
extern void CPrep_StreamGetSemicolon(TokenStream* stream, CPrepStreamFuncPtr func);
extern void CPrep_StreamInsert(TokenStream* stream, SInt32* state);
extern void CPrep_StreamRemove(TokenStream* stream, SInt32* state);
extern void CPrep_TokenStreamFlush(void);
extern TStreamElement* CPrep_CurStreamElement(void);
extern void CPrep_Error(short code);
extern void CPrep_ErrorName(short code, const char* name);
extern void CPrep_Warning(short code);
extern void popfile(void);
extern void CPrep_SetSourceFile(FileOffsetInfo* foi);
extern void CPrep_NewFileOffsetInfo(FileOffsetInfo* foi, TStreamElement* ts);
extern SInt32 CPrep_GetFileOffsetInfo(FileOffsetInfo* foi);
extern void CPrep_GetFileOffsetInfo2(FileOffsetInfo* foi, SInt32* pLine, HashNameNode** pName);
extern void CPrep_ResetFileInfo(CPrepFileInfo* file);
extern void CPrep_GetPrepPos(CPrepFileInfo** file, SInt32* ppos);
extern UInt8 C_Compiler(CompilerLinkerParamBlk* param);
extern void poptokenseq(void);
extern void foundnl(void);
extern void newline(void);
extern short notendofline(void);
extern char* CPrep_GetFileName(char* buffer, Boolean mainFile, Boolean quoted);
extern Boolean macrotest(void);
extern short CPrep_PragmaLex(Boolean no_err);
extern void CPrep_PushOption(SInt32 id, char value);
extern void CPrep_PopOption(SInt32 id);
extern void preprocessor(void);
extern SInt32 CPrep_BrowserTokenOffset(FileOffsetInfo* foi);
extern SInt32 CPrep_BrowserFileOffset(void);
extern void CPrep_BrowserFilePosition(CPrepFileInfo** fileinfo, SInt32* offset);
extern CPrepFileInfo* CPrep_BrowserCurrentFile(void);
extern void CPrep_GetTokenContext(
    TStreamElement* token,
    CPrepFileInfo** file,
    SInt32* position,
    short* column,
    SInt32* line,
    char* text,
    short* textpos,
    short* textcol,
    char* context,
    short* contextpos
);

#ifdef __cplusplus
}
#endif

#endif
