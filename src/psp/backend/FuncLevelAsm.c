// mwccpsp 3.0.1 FuncLevelAsm.c (0x4F8610..0x4F8800): asm functions.
#include "compiler/common.h"
#include "compiler/CError.h"
#include "compiler/CParser.h"
#include "compiler/CFunc.h"
#include "compiler/CPrep.h"
#include "compiler/CMangler.h"
#include "compiler/objects.h"
#include "psp/BE.h"

// 0x4F8610 / 0x4F8620: nothing to do for MIPS function-level assembly
void CleanupAssembler(void) {}

void SetupAssembler(void) {}

extern SInt32 data_00624D84; // source position for new instructions
extern void* data_00625008;
extern SInt16 data_00625E56;
extern void* data_00624C3C;
extern UInt8 data_00625E7E;
extern char data_00625EA9;
extern char data_00625EAE; // the frame pointer is used
extern SInt32 data_00624DEC;
extern Object* data_00625078; // the function being generated
extern SInt32 functionbodyoffset;
extern SInt32 symdeclend;
extern void func_004621F0(void);
extern void func_00434E50(Boolean on);
extern void func_004A46A0(void);
extern void func_00436F00(Object* func, int entry);
extern void func_0046FA90(MPCodeLabel* label);
extern void func_00482000(Statement* stmt); // assemble an asm statement
extern void ScanStatements(volatile short endToken); // InlineAsm.c
extern void newstatement(SInt32 sourceoffset, SInt32 value);
extern void CodeGen_GenerateFunction(
    Object* func, Boolean emitreturn, Boolean optimize, Boolean special
);

// 0x4F8630: an asm function body: its statements are assembled one by one,
// labels placed, and the function generated without optimization
void Assembler(Object* func) {
    Boolean save_cplusplus;
    Boolean save_asmpoundcomment;
    Statement* stmt;
    Statement* scan;
    MPCodeBlock* block;

#if !MWCCPS2 // b210 (0x506800) does not save these
    save_cplusplus = copts.cplusplus;
    save_asmpoundcomment = copts.asmpoundcomment;
#endif
    data_00624D84 = functionbodyoffset;
    data_00625008 = 0;
    func_004621F0();
    func_00434E50(0);
    data_00625E56 = 0x4088;
    data_00624C3C = 0;
    stmt = curstmt;

    cprep_eoltokens = TRUE;
    in_assembler = TRUE;
    ScanStatements('}');
    in_assembler = FALSE;
    cprep_eoltokens = FALSE;
#if !MWCCPS2
    copts.cplusplus = save_cplusplus;
    copts.asmpoundcomment = save_asmpoundcomment;
#endif

    CheckCLabels();
    if (fatalerrors) {
        return;
    }

    if (copts.filesyminfo) {
        symdeclend = CPrep_GetFileOffsetInfo(&cparser_fileoffset);
        CPrep_SetSourceFile(&cparser_fileoffset);
    }

    data_00625EA9 = 1;
    CMangler_GetLinkName(func);
    func->flags |= OBJECT_DEFINED;
    data_00625EAE = 0;
    func_004A46A0();
    data_00624DEC = 1;
    if (fatalerrors) {
        return;
    }

    initpcode();
#if MWCCPSP_NATIVE // 3.0.1 evaluates arguments right to left
    {
        MPCodeLabel* l = makepclabel();
        pclabel(block = makepcblock(), l);
    }
#else
    pclabel(block = makepcblock(), makepclabel());
#endif
    data_00624FD0 = block;
    func_00436F00(func, 0);
    assign_labels(stmt->next);

    for (scan = stmt->next; scan; scan = scan->next) {
        switch (scan->type) {
            case 16: // ST_ASM
                newstatement(scan->sourceoffset, 0);
                func_00482000(scan);
                break;
            case ST_LABEL:
                if (!((MPCodeLabel*)scan->label->pclabel)->resolved) {
                    func_0046FA90((MPCodeLabel*)scan->label->pclabel);
                    data_006246B4->flags2E = data_00625E56;
                    data_00625E7E = 1;
                }
                break;
            default:
#if MWCCPS2
                CError_Internal("FuncLevelAsm.c", CError_B145_LINE(169)); // mwccps2: 0xA9
#else
                CError_FATAL(175);
#endif
        }
    }

    if (fatalerrors) {
        return;
    }
    data_00625078 = func;
    CodeGen_GenerateFunction(func, 0, 1, 1);
}
