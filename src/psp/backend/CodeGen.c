#include "compiler/common.h"
#include <stdio.h>
#include <stdlib.h>
#include "psp/BE.h"
#include "compiler/CError.h"
#include "compiler/CParser.h"
#include "compiler/CompilerTools.h"
#include "compiler/CDecl.h"
#include "driver/COSToolsCLT.h"
#include "compiler/objects.h"
#include "compiler/types.h"
#include <string.h>
#include "compiler/CPrep.h"
#include "compiler/enode.h"
#include "compiler/CInt64.h"
#include "compiler/CFunc.h"
#include "compiler/CMangler.h"
#include "compiler/CodeGen.h"
#include "compiler/RegisterInfo.h"

// mwccpsp 3.0.1 CodeGen.c: the per-function backend driver (0x435BF0). It
// runs after the function's statements have been turned into pcode: it adds
// the return instruction, then the backend passes in order, each followed by
// a dump when #pragma debug_listing is on, and finally hands the encoded
// words to the object writer. The pass names are those of the dump labels.

// Nonzero when the target has a floating-point unit (an int, as the exe tests it).
inline int has_fpu(void) {
    return copts.fp_library;
}

typedef struct UsedRegs {
    SInt32 codewords; // +0x00: function size in words
    UInt32 intregs; // +0x04: integer registers written
    UInt32 fpregs; // +0x08: floating-point registers written
} UsedRegs;

extern HashNameNode* func_0049F6E0(Object* obj); // CMangler_GetLinkName
extern void func_00436860(Object* func); // set up for a function
extern void func_0046FA50(void); // start a labelled block
extern MPCode* func_00480420(int op, SInt16 nargs);
extern void BE_DumpPCode(char* name, char* label);
extern void func_004AF700(Object* func);
extern void func_0048AEF0(Object* func); // macro expansion B
extern void BE_PeepholeMerge(Object* func, int flag);
extern void BE_PeepholeOptimizePCode(Object* func);
extern void BE_EarlyConditionalMoves(void);
extern void BE_RemoveCommonSubexpressions(int postcoloring);
#if MWCCPS2 && MWCCPS2_BUILD < 210
// b145's removecommonsubexpressions takes no argument
#define BE_CSE(arg) ((void (*)(void))BE_RemoveCommonSubexpressions)()
#else
#define BE_CSE(arg) BE_RemoveCommonSubexpressions(arg)
#endif
extern void BE_LoopInvariants(Object* func);
extern void func_004B35D0(Object* func);
extern void func_004B3650(void);
extern void func_004B5250(void* model); // scheduler setup
extern void func_004B5210(void); // schedule
extern void BE_ColorRegisters(Object* func);
void BE_TailCalls(void);
extern void func_0048F260(Object* func); // macro expansion
extern void BE_LateConditionalMoves(void);
extern void BE_EncodeAll(void);
extern void BE_OptimizeBlocks(void);
extern void BE_InsertNops(Object* func, int scheduled, Boolean special);
extern void BE_LayoutAndEncode(Object* func);
extern void func_00461DF0(Object* func);
void func_00436D00(Object* func);
extern void ObjGen_EmitCode(Object* func, void* words, SInt32 size, ObjCodeReloc* relocs);
extern void func_0049D9B0(Object* func, SInt32 size); // exception tables
extern void ObjGen_GenerateMWCats(Object* func, SInt32 size);

extern void* data_00625070; // current_statement
extern SInt32 data_00624DEC;
extern MPCodeBlock* data_00624C18; // the function's first labelled block
extern SInt32 data_00625548;
extern SInt32 data_0062525C;
extern SInt32 data_006250A8;
extern SInt32 data_006245C0;
extern UsedRegs* data_006241D8;
extern SInt32 data_006241E0[7]; // registers per class: [1] the saved floating-point ones ...
extern SInt32 data_0060E038[7][BE_MAX_CLASS_REGS]; // ... and their numbers (PSP 0x60E238: class 1)
extern char data_00625EA4;
extern char data_00625EA9;
extern char data_005D084C[]; // scheduler models
extern char data_005D082C[];
extern ObjCodeReloc* data_005E8B8C; // the function's relocations
extern ObjCodeReloc* data_005E8B88; // ... and the last one
extern struct Alias* data_00625358;
extern SInt16 data_00625E3E;

#define IS_SINGLE(t) \
    (TYPE_INTEGRAL(t)->integral == IT_FLOAT || ((t)->type == TYPEFLOAT && (t)->size == 4))
#define IS_DOUBLE(t) \
    (TYPE_INTEGRAL(t)->integral == IT_DOUBLE || ((t)->type == TYPEFLOAT && (t)->size == 8))

// b210: with 32-bit longs only a0-a3 / $f12-$f15 carry arguments
#if MWCCPS2
#define LAST_GPR (copts.mips_long64 ? (UInt8)11 : (UInt8)7)
#define LAST_FPR (copts.mips_long64 ? (UInt8)19 : (UInt8)15)
#else
#define LAST_GPR 11
#define LAST_FPR 19
#endif

#define DUMP(label) \
    do { \
        if (copts.debuglisting) { \
            BE_DumpPCode(name, (label)); \
        } \
    } while (0)

static void note_written_regs(MPCode* pc) {
    MOperand* op;
    int n;

    for (n = pc->argcount, op = pc->args; n; n--, op++) {
        if (op->kind == 0 && (op->flags & 2)) {
            if (op->rclass == 0) {
                data_006241D8->intregs |= BIT32(op->reg);
            } else if ((copts.fp_library & 1) && op->rclass == 1) {
                data_006241D8->fpregs |= BIT32(op->reg);
            }
        }
    }
}

#if MWCCPS2
extern void ps2_dividefix_4C4910(Object* func); // #pragma dividefix pass
extern void ps2_padloop_4C2950(Object* func); // #pragma padloop pass
extern SInt32 ps2_padloop_636D5C; // cleared before the padloop loop search
extern struct Loop301* func_0054AB90(Boolean blocks); // findloopsinflowgraph
#endif

#undef CError_B145_SHIFT
#define CError_B145_SHIFT -10 // b145 asserts
void CodeGen_GenerateFunction(Object* func, Boolean emitreturn, Boolean optimize, Boolean special) {
    Type* rettype;
    char* name;
    UInt16 op;
    MPCode* pc;
    MPCodeBlock* block;
    MPCode* last;
    UInt32* words;
    SInt32 size;
    SInt32 n;
    int i;
    Boolean opt;

    opt = FALSE;
    rettype = TYPE_FUNC(func->type)->functype;
    name = func_0049F6E0(func)->name;
    op = special ? MACRO_OP(0xA5) : MACRO_OP(0xA4);
    func_00436860(func);
    data_00625070 = NULL;
    if (copts.optimizationlevel > 0 && !data_00624DEC && optimize) {
        opt = TRUE;
    }

    if (emitreturn || special) {
        if (!rettype) {
            pc = func_00480420(op, 0);
#if !MWCCPS2 // the R5900's 128-bit $v0 holds a long long
        } else if (((rettype->type == TYPEINT || rettype->type == TYPEENUM) &&
                    rettype->size == 8) ||
                   (rettype->type == TYPEFLOAT &&
                    !((IS_SINGLE(rettype) && (copts.fp_library & 1)) ||
                      (IS_DOUBLE(rettype) && (copts.fp_library & 2))) &&
                    IS_DOUBLE(rettype))) {
            // a 64-bit value in $v0/$v1
            pc = func_00480420(op, 2);
            pc->args[0].kind = 0;
            pc->args[0].rclass = 0;
            pc->args[0].reg = 2;
            pc->args[0].flags = 1;
            pc->args[1].kind = 0;
            pc->args[0].rclass = 0; // sic: args[1].rclass is left as made
            pc->args[1].reg = 3;
            pc->args[1].flags = 1;
#endif
        } else if (rettype->type == TYPEFLOAT &&
                   ((IS_SINGLE(rettype) && (copts.fp_library & 1)) ||
                    (IS_DOUBLE(rettype) && (copts.fp_library & 2)))) {
            // in $f0
            pc = func_00480420(op, 1);
            pc->args[1].kind = 0; // sic: one past the only operand
            pc->args[0].rclass = 1;
            pc->args[0].reg = 0;
            pc->args[0].flags = 1;
        } else {
            // in $v0
            pc = func_00480420(op, 1);
            pc->args[0].kind = 0;
            pc->args[0].rclass = 0;
            pc->args[0].reg = 2;
            pc->args[0].flags = 1;
        }
        appendpcode(data_006246B4, pc);
    }

    func_0046FA50();
    data_00624C18 = data_006246B4;
    data_006246B4->unk20 = data_00625548;
    if (copts.debuglisting) {
        BE_DumpPCode(func_0049F6E0(func)->name, "AFTER CodeGen");
    }
#if MWCCPS2 && MWCCPS2_BUILD < 210 // b145 always deletes unreachable blocks
    deleteunreachableblocks();
#else
    if (!data_0062525C) {
        deleteunreachableblocks();
    }
#endif
    if (opt) {
        func_004AF700(func);
    }
    func_0048AEF0(func);
    DUMP("Macro Expansion B");

    if (opt && copts.peephole) {
        BE_PeepholeMerge(func, 0);
        DUMP("Peephole Merge [Post BEMacroExpandB]");
        BE_PeepholeOptimizePCode(func);
        DUMP("After peephole [Post BEMacroExpandB]");
    }
#if !MWCCPS2 // b210 has no early conditional moves
    if (opt && copts.conditional_move) {
        BE_EarlyConditionalMoves();
        DUMP("After early conditional move optimization");
    }
#endif
    if (opt && copts.commonsubs) {
        BE_CSE(0);
        DUMP("After removecommonsubexpressions [Post BEMacroExpandB]");
        if (copts.loopinvariants) {
            BE_LoopInvariants(func);
            if (data_006250A8) {
                BE_CSE(0);
                DUMP("After Loop Invariant [Post BEMacroExpandB]");
            }
        }
        func_004B35D0(func);
        func_004B3650();
    }

    if (!special) {
        if (copts.schedule && optimize) {
            func_004B5250(data_005D084C);
            func_004B5210();
            DUMP("After schedule");
            for (block = data_00624FD0; block; block = block->next) {
                block->flags2E &= ~8;
            }
            if (opt && copts.peephole) {
                BE_PeepholeMerge(func, 0);
                DUMP("Peephole Merge [POST SCHEDULE]");
                BE_PeepholeOptimizePCode(func);
                DUMP("After peepholeoptimizepcode [POST SCHEDULE]");
            }
            if (opt && copts.commonsubs) {
                BE_CSE(0);
                if (data_006245C0) {
                    DUMP("After removecommonsubexpressions [POST SCHEDULE]");
                }
            }
        }
        BE_ColorRegisters(func);
        DUMP("After register coloring");
        if (opt && copts.commonsubs) {
            BE_CSE(1);
            if (data_006245C0) {
                DUMP("After removecommonsubexpressions [POST COLORING]");
            }
        }
        if (opt && copts.peephole) {
            BE_PeepholeMerge(func, 0);
            DUMP("Peephole Merge [POST COLORING]");
            BE_PeepholeOptimizePCode(func);
            DUMP("After peepholeoptimizepcode [POST COLORING]");
        }
        for (block = data_00624FD0; block; block = block->next) {
            for (pc = block->first; pc; pc = pc->next) {
                note_written_regs(pc);
            }
        }
    } else {
        for (block = data_00624FD0; block; block = block->next) {
            for (pc = block->first; pc; pc = pc->next) {
                if (pc->flags & 8) {
                    // a call: everything callee-saved may be written
                    for (i = 0; i < data_006241E0[0]; i++) {
                        data_006241D8->intregs |= BIT32(data_0060E038[0][i]);
                    }
                    if (copts.fp_library & 1) {
                        for (i = 0; i < data_006241E0[1]; i++) {
                            data_006241D8->fpregs |= BIT32(data_0060E038[1][i]);
                        }
                    }
                    goto regs_done;
                }
                note_written_regs(pc);
            }
        }
    }
regs_done:

    data_00625EA4 = 0;
    if (copts.tailcall && (!copts.cplusplus || !copts.exceptions) && !data_00625EA9) {
        BE_TailCalls();
        DUMP("After tail call");
    }
    func_0048F260(func);
    DUMP("After macro expansion");
#if MWCCPS2 && MWCCPS2_BUILD < 210 // b145: conditional_move, any ISA
    if (opt && copts.peephole && copts.conditional_move) {
#else
    if (opt && copts.peephole && COPTS_FIELD(late_conditional_move, 0) && copts.mips_isa >= 4) {
#endif
        BE_LateConditionalMoves();
        DUMP("After late conditional move optimization");
    }
#if MWCCPS2
    if (copts.dividefix) {
        ps2_dividefix_4C4910(func);
        DUMP("After dividefix operation");
    }
    if (copts.padloop) {
        ps2_padloop_636D5C = 0;
        func_0054AB90(0);
    }
    if (copts.padloop && !special) {
        ps2_padloop_4C2950(func);
    }
#endif
    BE_EncodeAll();
    DUMP("After binary1");
    if (copts.schedule && optimize) {
        BE_OptimizeBlocks();
        DUMP("After block optimization");
        func_004B5250(data_005D082C);
        func_004B5210();
        DUMP("After schedule after");
    }
    BE_InsertNops(func, copts.schedule && optimize, special);
    DUMP("After NOP");
    BE_LayoutAndEncode(func);
    func_00461DF0(func);

    size = data_006241D8->codewords << 2;
    words = lalloc(size);
    n = 0;
    for (block = data_00624FD0; block; block = block->next) {
        last = NULL;
        for (pc = block->first; pc; pc = pc->next) {
            if (pc->flags & 0x40000000) {
                // emitted after everything else in the block
                if (last) {
                    CError_Internal("CodeGen.c", CError_B145_LINE(2798));
                }
                last = pc;
            } else if (pc->op < BE_NUM_OPCODES) {
                words[n++] = pc->word;
            }
        }
        if (last && last->op < BE_NUM_OPCODES) {
            words[n++] = last->word;
        }
    }
    func_00436D00(func);
    ObjGen_EmitCode(func, words, size, data_005E8B8C);
    if (copts.cplusplus && copts.exceptions && !data_00625EA9 && !special) {
        func_0049D9B0(func, data_006241D8->codewords * 4);
    }
    if (copts.cats) {
        ObjGen_GenerateMWCats(func, data_006241D8->codewords * 4);
    }
    data_00625E3E = 0;
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

extern SInt32 data_006247A8; // #pragma pack/align value in effect
extern UInt8 data_00625094; // current section
extern SInt32 data_006246C0;
extern void func_004621F0(void);

// Called by initialize_compiler_options; empty in 3.0.1.
void func_00435A80(void) {}

void func_00435A90(void) {
    data_006246C0 = 1;
    func_004621F0();
}

// The optimization level turns passes on and off (#pragma optimization_level).
void CodeGen_UpdateOptimizerOptions(void) {
    copts.globaloptimizer = copts.optimizationlevel >= 1;
    copts.peephole = copts.optimizationlevel >= 1;
    copts.schedule = copts.optimizationlevel >= 3;
    copts.tailcall = copts.optimizationlevel >= 3;
    copts.unknown36[0] = copts.optimizationlevel >= 3;
    copts.optimizewithasm = copts.optimizationlevel >= 1;
}

#if MWCCPS2
// b210 0x433340: the back end's pragmas (the handlers are b210's own,
// bound in config/PSP_3_0_1/symbols.manual.txt); an unknown or failed one is skipped to
// the end of the line
extern SInt32 data_00624FD4; // maximum alignment, if any (#pragma pack)

// 0x433830: #pragma pack [(n)]: n is 0, 1, 2, 4, 8 or 16; the alignment cap
// is cleared on anything else, and on a bare #pragma pack
int ps2_pragma_pack(void) {
    tk = plex();
    if (tk == TK_EOL) {
        data_00624FD4 = 0;
        return 1;
    }
    if (tk == '(') {
        tk = plex();
        if (tk == TK_INTCONST) {
            data_00624FD4 = tkintconst.lo;
            if (data_00624FD4 == 0 ||
                data_00624FD4 == 1 ||
                data_00624FD4 == 2 ||
                data_00624FD4 == 4 ||
                data_00624FD4 == 8 ||
                data_00624FD4 == 16) {
                tk = plex();
                if (tk == ')') {
                    return 1;
                }
            }
        }
    }
    data_00624FD4 = 0;
    return 0;
}
extern int ps2_pragma_align(void); // 0x4337A0
extern int ps2_pragma_processor(void); // 0x433A30
extern int ps2_pragma_maxtempforblkmv(void); // 0x433D30
extern int ps2_pragma_vu0regnumber(void); // 0x4336D0
extern int ps2_pragma_vu0maxusednvreg(void); // 0x433660
extern int ps2_pragma_section(void); // 0x433DC0
extern int ps2_pragma_definesection(void); // 0x4338D0
extern UInt8 ps2_cgswitch[16]; // 0x622580

void CodeGen_ParsePragma(HashNameNode* name) {
    int ok;
    int i;
    short t;

    if (!strcmp(name->name, "pack")) {
        if (ps2_pragma_pack()) {
            return;
        }
    } else if (!strcmp(name->name, "cgswitch")) {
        tk = plex();
        if (tk == TK_EOL) {
            for (i = 0; i < 16; i++) {
                ps2_cgswitch[i] = 0;
            }
            ok = 1;
        } else {
            if (tk == '(') {
                tk = plex();
                if (tk == TK_INTCONST && CInt64_GetULong(&tkintconst) < 16) {
                    ps2_cgswitch[CInt64_GetULong(&tkintconst)] = 1;
                }
            }
            ok = 0;
        }
        if (ok) {
            return;
        }
    } else if (!strcmp(name->name, "align")) {
        if (ps2_pragma_align()) {
            return;
        }
    } else if (!strcmp(name->name, "processor")) {
        if (ps2_pragma_processor()) {
            return;
        }
    } else if (!strcmp(name->name, "maxtempforblkmv")) {
        if (ps2_pragma_maxtempforblkmv()) {
            return;
        }
    } else if (!strcmp(name->name, "vu0_register_number")) {
        if (ps2_pragma_vu0regnumber()) {
            return;
        }
    } else if (!strcmp(name->name, "vu0_mmi_reg_binding")) {
        if (notendofline() && (t = plex()) == TK_IDENTIFIER) {
            if (!strcmp(tkidentifier->name, "on")) {
                copts.mips_option43 = 1;
            } else if (!strcmp(tkidentifier->name, "off")) {
                copts.mips_option43 = 0;
            }
        }
        return;
    } else if (!strcmp(name->name, "vu0_max_used_nvreg")) {
        if (ps2_pragma_vu0maxusednvreg()) {
            return;
        }
    } else if (!strcmp(name->name, "section")) {
        if (ps2_pragma_section()) {
            return;
        }
    } else if (!strcmp(name->name, "define_section")) {
        if (ps2_pragma_definesection()) {
            return;
        }
    }
    if (copts.warn_illpragma) {
        CPrep_Warning(0x57);
    }
    do {
        t = plex();
    } while (t != TK_EOL && t != 0);
}
#endif

#if MWCCPS2
typedef struct PS2RegVarNode {
    struct PS2RegVarNode* next;
    Object* object;
    SInt32 unk8;
    VarInfo* info; // requested class at +0x2E (unk24[2]), register at +0x32
} PS2RegVarNode;
extern PS2RegVarNode* ps2_objgen_636288; // data objects declared in a register
extern Boolean func_004B8980(Object* obj);
#endif
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -28 // b145 asserts
void CodeGen_UpdateObject(Object* obj) {
#if MWCCPS2
    // b210 (0x433EF0): a data object in a register takes over the entry
    // reserved for its register class and number
    PS2RegVarNode* node;

    if (copts.mips_option43 && func_004B8980(obj)) {
        VarInfo* vi;
        VarInfo* other;

        node = ps2_objgen_636288;
        vi = obj->u.data.info;
        for (; node; node = node->next) {
            other = node->info;
            if (vi->unk24[2] == other->unk24[2]) {
                if (vi->reg == other->reg) {
                    node->object = obj;
                }
                break;
            }
        }
    }
#endif
    if (obj->type && obj->type->type == TYPEFUNC) {
        if (obj->name && !strcmp(obj->name->name, "__builtin_next_arg")) {
            obj->u.func.u.intrinsicid = 7;
        }
        return;
    }
    if (!obj->section) {
        obj->section = data_00625094;
    }
    if (data_006247A8 && !(obj->qual & Q_ALIGNED_MASK)) {
        switch (data_006247A8) {
            case 1:
                obj->qual |= Q_ALIGNED_1;
                break;
            case 2:
                obj->qual |= Q_ALIGNED_2;
                break;
            case 4:
                obj->qual |= Q_ALIGNED_4;
                break;
            case 8:
                obj->qual |= Q_ALIGNED_8;
                break;
            case 0x10:
                obj->qual |= Q_ALIGNED_16;
                break;
            case 0x20:
                obj->qual |= Q_ALIGNED_32;
                break;
            case 0x40:
                obj->qual |= Q_ALIGNED_64;
                break;
            case 0x80:
                obj->qual |= Q_ALIGNED_128;
                break;
            case 0x100:
                obj->qual |= Q_ALIGNED_256;
                break;
            case 0x200:
                obj->qual |= Q_ALIGNED_512;
                break;
            case 0x400:
                obj->qual |= Q_ALIGNED_1024;
                break;
            case 0x800:
                obj->qual |= Q_ALIGNED_2048;
                break;
            case 0x1000:
                obj->qual |= Q_ALIGNED_4096;
                break;
            case 0x2000:
                obj->qual |= Q_ALIGNED_8192;
                break;
            default:
#if MWCCPS2
                CError_Internal("CodeGen.c", CError_B145_LINE(4203));
#else
                CError_Internal("CodeGen.c", CError_B145_LINE(4220));
#endif
        }
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// The target's predefined macros; their values are computed when expanded
// (CPrep). The Macro records are this unit's statics.
static Macro mMIPSM;
static Macro mMIPS_ISA2M;
static Macro mMIPS_ISA3M;
static Macro mMIPS_ISA4M;
static Macro mpc_relativeM;
static Macro mgp_relativeM;
static Macro mfpuM;
static Macro mptr64M;
static Macro mfpu32M;
static Macro mfpu64M;
static Macro mcpu32M;
static Macro mcpu64M;
static Macro mcpu128M;
static Macro mn64abiM;
static Macro mMIPS_16M;
static Macro mMIPS_processorM;
static Macro mMIPS_single_fpuM;
static Macro mMIPS_double_fpuM;
static Macro mMIPS_PSPM;
static Macro mmips_eabiM;
static Macro mmips_single_floatM;
static Macro mmipsM;
static Macro mMIPSELM;
#if MWCCPS2
static Macro mmips64M;
#endif
static Macro mneed_size_tM;
static Macro mneed_ptrdiff_tM;
static Macro mPTRDIFF_TYPEM;
static Macro mSIZE_TYPEM;
static Macro mATTRIBUTE_IMPURE_PTRM;

void CodeGen_InsertSpecialMacros(void) {
    CPrep_InsertSpecialMacro(&mMIPSM, "__MIPS__");
    CPrep_InsertSpecialMacro(&mMIPS_ISA2M, "__MIPS_ISA2__");
    CPrep_InsertSpecialMacro(&mMIPS_ISA3M, "__MIPS_ISA3__");
    CPrep_InsertSpecialMacro(&mMIPS_ISA4M, "__MIPS_ISA4__");
    CPrep_InsertSpecialMacro(&mpc_relativeM, "__pc_relative__");
    CPrep_InsertSpecialMacro(&mgp_relativeM, "__gp_relative__");
    CPrep_InsertSpecialMacro(&mfpuM, "__fpu__");
    CPrep_InsertSpecialMacro(&mptr64M, "__ptr64__");
    CPrep_InsertSpecialMacro(&mfpu32M, "__fpu32__");
    CPrep_InsertSpecialMacro(&mfpu64M, "__fpu64__");
    CPrep_InsertSpecialMacro(&mcpu32M, "__cpu32__");
    CPrep_InsertSpecialMacro(&mcpu64M, "__cpu64__");
    CPrep_InsertSpecialMacro(&mcpu128M, "__cpu128__");
    CPrep_InsertSpecialMacro(&mn64abiM, "__n64abi__");
    CPrep_InsertSpecialMacro(&mMIPS_16M, "__MIPS_16__");
    CPrep_InsertSpecialMacro(&mMIPS_processorM, "__MIPS_processor__");
    CPrep_InsertSpecialMacro(&mMIPS_single_fpuM, "__MIPS_single_fpu__");
    CPrep_InsertSpecialMacro(&mMIPS_double_fpuM, "__MIPS_double_fpu__");
#if MWCCPS2
    CPrep_InsertSpecialMacro(&mMIPS_PSPM, "__MIPS_PSX2__");
#else
    CPrep_InsertSpecialMacro(&mMIPS_PSPM, "__MIPS_PSP__");
#endif
    CPrep_InsertSpecialMacro(&mmips_eabiM, "__mips_eabi");
    CPrep_InsertSpecialMacro(&mmips_single_floatM, "__mips_single_float");
    CPrep_InsertSpecialMacro(&mmipsM, "__mips__");
#if MWCCPS2
    CPrep_InsertSpecialMacro(&mmips64M, "__mips64");
#endif
    CPrep_InsertSpecialMacro(&mMIPSELM, "__MIPSEL__");
    CPrep_InsertSpecialMacro(&mneed_size_tM, "__need_size_t");
    CPrep_InsertSpecialMacro(&mneed_ptrdiff_tM, "__need_ptrdiff_t");
    CPrep_InsertSpecialMacro(&mPTRDIFF_TYPEM, "__PTRDIFF_TYPE__");
    CPrep_InsertSpecialMacro(&mSIZE_TYPEM, "__SIZE_TYPE__");
    CPrep_InsertSpecialMacro(&mATTRIBUTE_IMPURE_PTRM, "__ATTRIBUTE_IMPURE_PTR__");
}

static void func_00436DA0(Object* func, MPCode* pc, ObjCodeReloc** head, ObjCodeReloc** tail) {
    ObjCodeReloc* r;
    ObjCodeReloc* scan;

    if (!pc->relobj) {
#if MWCCPS2 && MWCCPS2_BUILD < 210
        if (pc->unk38 == 4) {
#else
        if (pc->unk38 == 4 || pc->unk38 == 2) {
#endif
            pc->relobj = func;
            pc->alias = data_00625358;
        } else {
            CError_Internal("CodeGen.c", CError_B145_LINE(2053));
        }
    }
    if (pc->unk38 == 6) {
        // a %lo goes right after the %hi of the same object
        for (scan = *head; scan; scan = scan->next) {
            if (scan->type == 5 && scan->target == pc->relobj) {
                r = lalloc(sizeof(ObjCodeReloc));
                r->type = pc->unk38;
                r->offset = pc->offset;
                r->unk0C = 0;
                r->target = pc->relobj;
                r->next = scan->next;
                scan->next = r;
                return;
            }
        }
        CError_Internal("CodeGen.c", CError_B145_LINE(2073));
    }
    r = lalloc(sizeof(ObjCodeReloc));
    if (*tail) {
        (*tail)->next = r;
    } else {
        *head = r;
    }
    r->next = NULL;
    *tail = r;
    r->type = pc->unk38;
    r->offset = pc->offset;
    r->unk0C = 0;
    r->target = pc->relobj;
    if (pc->unk38 == 5) {
        r = lalloc(sizeof(ObjCodeReloc));
        (*tail)->next = r;
        r->next = NULL;
        *tail = r;
        r->type = pc->pair->unk38;
        r->offset = pc->pair->offset;
        r->unk0C = 0;
        r->target = pc->relobj;
        pc->pair->unk38 = 0;
    }
}

// Collect the function's relocations: the %lo ones last, each after its %hi.
void func_00436D00(Object* func) {
    MPCodeBlock* block;
    MPCode* pc;

    data_005E8B8C = data_005E8B88 = NULL;
    for (block = data_00624FD0; block; block = block->next) {
        for (pc = block->first; pc; pc = pc->next) {
            if (pc->unk38 != 6 && pc->unk38 != 0) {
                func_00436DA0(func, pc, &data_005E8B8C, &data_005E8B88);
            }
        }
    }
    for (block = data_00624FD0; block; block = block->next) {
        for (pc = block->first; pc; pc = pc->next) {
            if (pc->unk38 == 6) {
                func_00436DA0(func, pc, &data_005E8B8C, &data_005E8B88);
            }
        }
    }
}

// True when the constant needs more than one instruction to load: it fits
// neither a signed 16-bit immediate nor a lui (low half zero).
Boolean func_004323D0(CInt64 value) {
    SInt32 v;
    if (CInt64_IsInRange(value, 4)) {
        v = value.lo;
        if (v <= 0x7FFF && v >= -0x8000) {
            return 0;
        }
        if (!(v & 0xFFFF)) {
            return 0;
        }
        return 1;
    }
    return 1;
    CError_FATAL(5045);
    return 0;
}

// Switch between the 32-bit and 64-bit long long macro variants.
extern SInt16 data_00625AD0;
extern SInt16 data_00625B58;
extern SInt16 data_00625E48;
extern SInt32 data_00624778;

void func_00434E50(Boolean on) {
    if (on) {
        data_00625AD0 = 0x3E5;
        data_00625B58 = 0x3E4;
        data_00625E48 = 0x3DF;
        data_00624778 = 1;
    } else {
        data_00625AD0 = MACRO_OP(0x82);
        data_00625B58 = MACRO_OP(0x81);
        data_00624778 = 0;
    }
}

// 128-bit integers can only appear as constants.
ENode* CodeGen_CheckExpr(ENode* expr) {
    if (expr) {
#if MWCCPS2
        if (expr->type == EASS || expr->type == ETYPCON || expr->type == EINTCONST) {
#else
        if (expr->type == EINTCONST) {
#endif
            return expr;
        }
        if (expr->rtype->type == TYPEINT && expr->rtype->size == 16) {
            CError_Error(0x39);
        }
    }
    return expr;
}

// Preference panels, as the IDE plugin stores them. Only the fields read
// here are named; the panel defaults live in the exe's data.
typedef struct PSPCodeGenPrefs {
    SInt32 unk00;
    SInt16 unk04;
    SInt16 fpu; // 0x06 0 none, 1 single, 2 double, 3 both (fp_library 0/1/3/7)
    SInt16 isa; // 0x08 MIPS ISA level - 1
    UInt8 readonly_strings; // 0x0a
    UInt8 unk0B[2];
    UInt8 inline_intrinsics; // 0x0d
    UInt8 unk0E;
    UInt8 unk0F; // 0x0f
    UInt8 unk10[4];
} PSPCodeGenPrefs; // 0x14

typedef struct PSPLinkerPrefs {
    SInt32 unk00;
    UInt8 unk04;
    UInt8 symfullpath; // 0x05
    UInt8 unk06[0x14E];
} PSPLinkerPrefs; // 0x154

typedef struct PSPProjectPrefs {
    SInt32 unk00;
    SInt32 option04; // 0x04
    SInt32 unk08[13];
} PSPProjectPrefs; // 0x3c

extern PSPCodeGenPrefs data_005B0A78;
extern PSPLinkerPrefs data_005B0A8C;
extern UInt8 data_0060B920[16];
extern UInt8 data_00625EAB;
extern SInt32 data_00625318;
void func_00435300(void);
extern SInt16 data_00625CE0;
extern SInt16 data_00625E4A;
extern SInt16 data_00625E4E;
extern SInt16 data_00625E72;
extern SInt16 data_00625E42;
extern SInt16 data_00625E54;
extern SInt16 data_00625B6E;
extern SInt16 data_00625E46;
extern CompilerLinkerParamBlk cparams;
extern int __stdcall func_00415F00(
    CWPluginContext context, const char* name, CWMemHandle* handle
); // CWGetNamedPreferences
extern int __stdcall func_00416250(
    CWPluginContext context, CWMemHandle handle, Boolean moveHi, void** ptr
); // CWLockMemHandle
extern int __stdcall func_004162B0(
    CWPluginContext context, CWMemHandle handle
); // CWUnlockMemHandle
extern int __stdcall func_004161D0(CWPluginContext context, CWMemHandle handle); // CWFreeMemHandle

#if MWCCPS2
// b210 (0x435110): the PS2 panels ("PS2 MIPS CodeGen", "MIPS Bare Linker
// Panel", "MIPS Bare Project") and the PS2 fixed options
void func_00434F30(void) {
    PSPProjectPrefs project;
    void* ptr;
    CWMemHandle handle;
    int i;

    for (i = 0; i < 16; i++) {
        data_0060B920[i] = 0;
    }

    if (func_00415F00(cparams.context, "PS2 MIPS CodeGen", &handle) == 0) {
        if (func_00416250(cparams.context, handle, 0, &ptr) == 0) {
            data_005B0A78 = *(PSPCodeGenPrefs*)ptr;
            func_004162B0(cparams.context, handle);
        }
        func_004161D0(cparams.context, handle);
    }

    copts.stdc_fenv_access = 1;
    if (!copts.optimizesize) {
        copts.mips_option3A = 1;
    }
    data_00625EAB = 0;
    copts.readonly_strings = data_005B0A78.readonly_strings;
    copts.inline_intrinsics = data_005B0A78.inline_intrinsics;
    copts.structalignment = 2;
    copts.fp_library = 0;
    copts.mips_isa = 1;
    copts.mips_option01 = 0;
    copts.mips_option0F = 0;
    copts.mips_option10 = 1;
    copts.mips_long64 = 0;
    copts.mips_option17 = 0;
    data_00625318 = 0;
    copts.mips_option33 = 4;
    copts.mips_option39 = 2;
    copts.warn_inlineasm = 0;
    copts.cse_hard_reg_gpr = 1;
    copts.cse_hard_reg_fpr = 1;
    copts.cse_hard_reg_special = 0;
    copts.mips_option132 = 1;
    copts.mips_flags02 = 0x1D;
    copts.mips_option19 = 1;
    copts.littleendian = 1;
    copts.mips_option11 = 1;
    copts.mips_long64 = 1;
    copts.mips_option10 = 0;
    copts.mips_option0F = 1;
    copts.gcc_extensions = 1;
    copts.padloop = 1;
    copts.dividefix = 1;
    copts.fastmath = 1;
    copts.enable_vu0_registers = 1;
    copts.vu0_register_number = 0x20;
    copts.function_align16 = 0x19; // copts+0x48
    copts.mips_option43 = 0; // copts+0x47
    copts.cse_hard_reg_vu0 = 0;
    copts.cse_vector_type = 0;
    copts.volatileasm = 0;

    if (func_00415F00(cparams.context, "MIPS Bare Linker Panel", &handle) == 0) {
        if (func_00416250(cparams.context, handle, 0, &ptr) == 0) {
            data_005B0A8C = *(PSPLinkerPrefs*)ptr;
            func_004162B0(cparams.context, handle);
        }
        func_004161D0(cparams.context, handle);
    }
    copts.absolutepath = data_005B0A8C.symfullpath;

    if (func_00415F00(cparams.context, "MIPS Bare Project", &handle) == 0) {
        if (func_00416250(cparams.context, handle, 0, &ptr) == 0) {
            project = *(PSPProjectPrefs*)ptr;
            func_004162B0(cparams.context, handle);
        }
        func_004161D0(cparams.context, handle);
    }
    copts.mips_option04 = project.option04;
    if (copts.mips_option04 != 0) {
        copts.mips_option17 = 1;
    }
    copts.cats = 1;
    data_00625CE0 = MACRO_OP(0x5F);
    data_00625E4A = MACRO_OP(0x5F);
    data_00625E4E = MACRO_OP(0x3A);
    data_00625E72 = MACRO_OP(0x3A);
    data_00625E42 = MACRO_OP(0x35);
    data_00625E54 = MACRO_OP(0x31);
    data_00625AD0 = MACRO_OP(0x82);
    data_00625B58 = MACRO_OP(0x81);
    data_00625B6E = MACRO_OP(0x55);
    data_00625E46 = 0x9D;
    func_00435300();
}
#else
// Read the PSP preference panels into copts and set the fixed options.
void func_00434F30(void) {
    PSPProjectPrefs project;
    void* ptr;
    CWMemHandle handle;
    int i;

    for (i = 0; i < 16; i++) {
        data_0060B920[i] = 0;
    }

    if (func_00415F00(cparams.context, "PSP MIPS CodeGen", &handle) == 0) {
        if (func_00416250(cparams.context, handle, 0, &ptr) == 0) {
            data_005B0A78 = *(PSPCodeGenPrefs*)ptr;
            func_004162B0(cparams.context, handle);
        }
        func_004161D0(cparams.context, handle);
    }

    copts.stdc_fenv_access = 1;
    if (!copts.optimizesize) {
        copts.mips_option3A = 1;
    }
    data_00625EAB = 0;
    copts.readonly_strings = data_005B0A78.readonly_strings;
    copts.inline_intrinsics = data_005B0A78.inline_intrinsics;
    copts.structalignment = 2;
    copts.fp_library = 0;
    copts.mips_isa = 1;
    copts.mips_option01 = 0;
    copts.mips_option0F = 0;
    copts.mips_option10 = 1;
    copts.mips_long64 = 0;
    copts.mips_option17 = 0;
    data_00625318 = 0;
    copts.mips_option33 = 4;
    copts.mips_option39 = 2;
    copts.warn_inlineasm = 0;
    copts.mips_option132 = 1;
    copts.mips_flags02 = 0x1E;
    copts.mips_option19 = 1;
    copts.littleendian = 1;
    copts.mips_option11 = 1;
    copts.mips_long64 = 0;
    copts.mips_option10 = 1;
    copts.mips_option0F = 1;
    copts.gcc_extensions = 1;
    copts.mips_option3D = 1;
    copts.mips_option3B = 1;
    copts.fastmath = 1;
    copts.mips_option41 = 0;
    copts.mips_option43 = 0x20;
    copts.volatileasm = 1;

    if (func_00415F00(cparams.context, "PSP Linker Panel", &handle) == 0) {
        if (func_00416250(cparams.context, handle, 0, &ptr) == 0) {
            data_005B0A8C = *(PSPLinkerPrefs*)ptr;
            func_004162B0(cparams.context, handle);
        }
        func_004161D0(cparams.context, handle);
    }
    copts.absolutepath = data_005B0A8C.symfullpath;

    if (func_00415F00(cparams.context, "PSP Project", &handle) == 0) {
        if (func_00416250(cparams.context, handle, 0, &ptr) == 0) {
            project = *(PSPProjectPrefs*)ptr;
            func_004162B0(cparams.context, handle);
        }
        func_004161D0(cparams.context, handle);
    }
    copts.mips_option04 = project.option04;
    if (copts.mips_option04 != 0) {
        copts.mips_option17 = 1;
    }
    copts.cats = 1;
    data_00625CE0 = MACRO_OP(0x5F);
    data_00625E4A = MACRO_OP(0x5F);
    data_00625E4E = MACRO_OP(0x3A);
    data_00625E72 = MACRO_OP(0x3A);
    data_00625E42 = MACRO_OP(0x35);
    data_00625E54 = MACRO_OP(0x31);
    data_00625AD0 = MACRO_OP(0x82);
    data_00625B58 = MACRO_OP(0x81);
    data_00625B6E = MACRO_OP(0x55);
    data_00625E46 = 0x9D;
    func_00435300();
}
#endif

extern SInt32 data_0062421C;
extern SInt32 data_00624788;
extern SInt32 data_00625010;
extern SInt32 data_0062504C;
extern SInt16 data_00625E40;
extern SInt16 data_00625E4C;
extern SInt16 data_00625E52;
extern SInt16 data_00625E5C;
extern SInt16 data_00625E74;
extern void func_0046FFB0(void);
extern void CMach_Configure(void);

// fp_library for the CodeGen panel's FPU setting.
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -10 // b145 asserts
inline char panel_fp_library(void) {
    switch (data_005B0A78.fpu) {
        case 0:
            return 0;
        case 1:
            return 1;
        case 2:
            return 3;
        case 3:
            return 7;
        default:
            CError_FATAL(2914);
            return 0;
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// Set the ISA, FPU and code generation options for the selected processor.
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -21 // b145 asserts
void func_00435300(void) {
    SInt16 processor;

    data_00625E48 = 0;
    data_00625E74 = 0;
    processor = copts.mips_flags02;
    switch (processor & ~0xF000) {
#if !MWCCPS2 // b210 has no processor 30
        case 30:
            data_0062421C = 0x10;
            copts.mips_isa = 2;
            copts.fp_library = 1;
            data_005B0A78.fpu = 1;
            copts.mips_option10 = 1;
            copts.prefetch = 1;
            copts.COPTS_LATE_CONDITIONAL_MOVE = 1;
            copts.mips_option47 = 1;
            copts.mips_option31 = 1;
            data_00625CE0 = MACRO_OP(0x5F);
            if (has_fpu()) {
                copts.mips_option0F = 1;
                copts.sqrt = 1;
                copts.trunc = 1;
                copts.floatsp32 = 1;
                data_005B0A78.unk0F = 1;
                copts.mips_option2F = 1;
                copts.mips_option30 = 1;
                copts.mips_option1A = 1;
            }
            break;
#endif
        case 1:
            copts.no_branch_likely = 1;
            copts.no_load_delay_nops = 1;
            copts.floatsp32 = 1;
            copts.prefetch = 1;
            copts.mips_option2F = 1;
            copts.mips_option30 = 1;
            copts.fp_library = 7;
            copts.sqrt = 1;
            copts.COPTS_LATE_CONDITIONAL_MOVE = 1;
            copts.mips_option31 = 1;
            copts.mips_option1A = 1;
            break;
        case 2:
            copts.no_branch_likely = 1;
            copts.no_load_delay_nops = 1;
            copts.prefetch = 1;
            copts.mips_option2F = 1;
            copts.mips_option30 = 1;
            copts.fp_library = 7;
            copts.sqrt = 1;
            copts.mips_option23 = 1;
            copts.COPTS_LATE_CONDITIONAL_MOVE = 1;
            copts.mips_isa = 5;
            copts.mips_option31 = 1;
            break;
        case 17:
            copts.mips_flags02 |= 0x2000;
            copts.fp_library = 0;
            copts.mips_isa = 2;
            copts.no_load_delay_nops = 1;
            break;
        case 18:
            copts.mips_flags02 |= 0x2000;
            copts.fp_library = 0;
            copts.mips_isa = 3;
            copts.no_load_delay_nops = 1;
            break;
        case 19:
            copts.mips_flags02 |= 0x2000;
            copts.fp_library = 0;
            copts.no_load_delay_nops = 1;
            copts.mips_isa = 3;
            break;
        case 20:
        case 21:
            copts.mips_flags02 |= 0x2000;
            copts.fp_library = 7;
            copts.trunc = 1;
            copts.no_load_delay_nops = 1;
            copts.mips_isa = 3;
            break;
        case 22:
            copts.mips_flags02 |= 0x4000;
            copts.fp_library = 7;
            copts.trunc = 1;
            copts.no_load_delay_nops = 1;
            copts.mips_isa = 3;
            copts.gencvtcheck = 1;
            break;
        case 23:
            copts.mips_flags02 |= 0x4000;
            copts.fp_library = 7;
            copts.trunc = 1;
            copts.no_load_delay_nops = 1;
            copts.mips_isa = 4;
            copts.gencvtcheck = 1;
            copts.mips_option2F = 1;
            data_00625CE0 = 0x187;
            data_00625E4A = 0x188;
            break;
        case 4:
        case 5:
        case 6:
        case 7:
        case 9:
            copts.mips_flags02 |= 0x1000;
            copts.fp_library = 0;
            copts.mips_isa = 1;
            copts.no_branch_likely = 1;
            break;
        case 8:
            copts.mips_flags02 |= 0x1000;
            copts.fp_library = 3;
            copts.mips_isa = 1;
            copts.no_branch_likely = 1;
            break;
        case 12:
        case 13:
            copts.mips_flags02 |= 0x2000;
            copts.fp_library = 1;
            data_00625CE0 = 0x176;
            copts.no_load_delay_nops = 1;
            copts.mips_isa = 3;
            copts.floatsp32 = 1;
            break;
        case 10:
        case 11:
        case 15:
            copts.mips_flags02 |= 0x2000;
            copts.fp_library = 7;
            copts.trunc = 1;
            copts.no_load_delay_nops = 1;
            copts.mips_isa = 3;
            break;
        case 14:
            copts.mips_flags02 |= 0x2000;
            data_00625CE0 = 0x176;
            copts.mips_isa = 2;
            copts.no_load_delay_nops = 1;
            copts.fp_library = 0;
            copts.COPTS_LATE_CONDITIONAL_MOVE = 1;
            copts.prefetch = 1;
            copts.mips_option31 = 1;
            break;
        case 16:
            copts.mips_flags02 |= 0x4000;
            copts.fp_library = 7;
            copts.trunc = 1;
            copts.no_load_delay_nops = 1;
            copts.mips_isa = 4;
            copts.mips_option2F = 1;
            break;
        case 3:
            copts.mips_flags02 |= 0x2000;
            copts.fp_library = 0;
            copts.no_load_delay_nops = 1;
            data_00625CE0 = 0x17E;
            data_00625E4A = 0x17F;
            data_00625E4E = 0x180;
            data_00625E72 = 0x181;
            data_00625E42 = 0x184;
            data_00625E54 = 0x182;
            copts.mips_isa = 3;
            break;
        case 27:
            copts.mips_flags02 |= 0x4000;
            copts.fp_library = 7;
            copts.trunc = 1;
            copts.no_load_delay_nops = 1;
            data_00625CE0 = 0x2EC;
            copts.mips_isa = 3;
            break;
        case 28:
            copts.mips_option26 = 0;
            copts.mips_isa = 2;
            copts.no_load_delay_nops = 1;
            copts.mips_flags02 |= 0x2000;
            data_00625318 = 1;
            copts.fp_library = 0;
            copts.mips_option16 = 1;
            data_00625AD0 = 0x3E5;
            data_00625B58 = 0x3E4;
#if MWCCPS2
            data_00625E48 = 0x3DF;
#endif
            break;
        case 29:
            data_0062421C = 0x10;
            copts.mips_flags02 |= 0x4000;
            copts.fp_library = 1;
            copts.mips_option39 = 3;
            copts.no_load_delay_nops = 1;
            copts.mips_isa = 3;
#if MWCCPS2
            copts.mips_option41 = 1;
#else
            copts.function_align16 = 1;
#endif
            copts.conditional_move = 0;
            copts.COPTS_LATE_CONDITIONAL_MOVE = 1;
            copts.prefetch = 1;
            copts.sqrt = 1;
            copts.mips_option23 = 1;
            copts.floatsp32 = 1;
            copts.mips_option25 = 1;
#if MWCCPS2
            copts.usefloatacc = 1;
#else
            copts.mips_option3C = 1;
#endif
            data_00625CE0 = 0x1DB;
            data_00625E4A = 0x1DC;
            data_00625E74 = 0x1CC;
#if MWCCPS2
            data_00625E48 = 0xAB;
#endif
            break;
        case 24:
        case 25:
        case 26:
            copts.mips_flags02 |= 0x4000;
            copts.fp_library = 7;
            copts.COPTS_LATE_CONDITIONAL_MOVE = 1;
            copts.trunc = 1;
            copts.no_load_delay_nops = 1;
            copts.mips_isa = 4;
            copts.mips_option2F = 1;
            copts.sqrt = 1;
            copts.mips_option23 = 1;
            data_00625CE0 = 0x17B;
            break;
        default:
            if (processor == 0x1000) {
                copts.mips_isa = 1;
                copts.fp_library = panel_fp_library();
                copts.no_branch_likely = 1;
            } else if (processor == 0x2000) {
                copts.mips_isa = data_005B0A78.isa + 1;
                copts.fp_library = panel_fp_library();
                copts.no_load_delay_nops = 1;
            } else if (processor == 0x4000) {
                copts.mips_isa = data_005B0A78.isa + 1;
                copts.fp_library = panel_fp_library();
                copts.no_load_delay_nops = 1;
            } else {
                CError_FATAL(3241);
            }
    }

    if (copts.mips_isa >= 2) {
        copts.no_load_delay_nops = 1;
        if (copts.fp_library) {
            copts.sqrt = 1;
        }
    }
    if (copts.mips_isa >= 4) {
        copts.COPTS_LATE_CONDITIONAL_MOVE = 1;
        copts.prefetch = 1;
        copts.mips_option23 = 1;
        copts.mips_option2F = 1;
    }
    if (copts.mips_isa >= 5) {
        copts.mips_option30 = 1;
    }
#if MWCCPS2
    // b210: 64-bit registers (8-byte saves, ld/sd moves)
    copts.has8bytebitfields = 1;
    copts.mips_option39 = 3;
    data_00624788 = 8;
    data_00625010 = 7;
    data_0062504C = 3;
    data_00625E40 = MACRO_OP(0x4A);
    data_00625E4C = MACRO_OP(0x6F);
    data_00625E52 = 0x93;
    data_00625E5C = 0xC4;
    if (data_00625E48 == 0) {
        data_00625E48 = 0xAB;
    }
#else
    data_00624788 = 4;
    data_00625010 = 3;
    data_0062504C = 2;
    data_00625E40 = MACRO_OP(0x55);
    data_00625E4C = MACRO_OP(0x77);
    data_00625E52 = 0x9D;
    data_00625E5C = 0xCB;
    data_00625E48 = 0xAA;
#endif
    func_0046FFB0();
    CMach_Configure();
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// Runtime helper functions the code generator calls.
Object* rt_divdi3;
Object* rt_udivdi3;
Object* rt_moddi3;
Object* rt_umoddi3;
Object* rt_muldi3;
Object* rt_muldi3_2;
Object* rt_ashrdi3;
Object* rt_lshrdi3;
Object* rt_ashldi3;
Object* rt_l_muls;
Object* rt_l_mulu;
Object* rt_l_divs;
Object* rt_l_divu;
Object* rt_l_mod;
Object* rt_l_modu;
Object* rt_fixdfdi;
Object* rt_fixunsdfdi;
Object* rt_floatdidf;
Object* rt_d_ulltod;
Object* rt_fixsfdi;
Object* rt_fixunssfdi;
Object* rt_floatdisf;
Object* rt_f_ulltof;
Object* rt_fixdfsi;
Object* rt_fixunsdfsi;
Object* rt_floatsidf;
Object* rt_d_utod;
Object* rt_f_ftoi;
Object* rt_f_ftou;
Object* rt_f_itof;
Object* rt_f_utof;
Object* rt_fp_round;
Object* rt_fp_get_fpscr;
Object* rt_fp_set_fpscr;
Object* rt_adddf3;
Object* rt_subdf3;
Object* rt_muldf3;
Object* rt_divdf3;
Object* rt_dpcmp;
Object* rt_dpcmpe;
Object* rt_dpfeq;
Object* rt_dpfne;
Object* rt_dpflt;
Object* rt_dpfle;
Object* rt_dpfgt;
Object* rt_dpfge;
Object* rt_dpfor;
Object* rt_dofun;
Object* rt_f_add;
Object* rt_f_sub;
Object* rt_f_mul;
Object* rt_f_div;
Object* rt_f_cmp;
Object* rt_f_cmpe;
Object* rt_f_feq;
Object* rt_f_fne;
Object* rt_f_flt;
Object* rt_f_fle;
Object* rt_f_fgt;
Object* rt_f_fge;
Object* rt_f_for;
Object* rt_f_fun;
Object* rt_truncdfsf2;
Object* rt_extendsfdf2;
Object* rt_profile_s;
Object* rt_profile_e;
Object* rt_strcpy;
Object* rt_memcpy;
Object* rt_MIPS16_prtglue32;
Object* rt_MIPS16_prtglue16;

// Fills the rt_ objects' names; soft-float helpers have a _dflt_ variant
// used when hardware floating point is available (ISA III and up).
#define RT_SOFTFP(name) \
    ((copts.fp_library && copts.stdc_fenv_access) || copts.mips_isa < 3 ? name : "_dflt" name)
extern void func_0045C740(void); // CMach_ReInitRuntimeObjects
extern Boolean func_00458560(Boolean is_precompiler); // Intrinsics_ReInitRuntimeObjects
extern void func_0045B910(void);
extern void func_0045C780(void);

void CodeGen_SetupRuntimeObjects(void) {
    rt_divdi3 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_udivdi3 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_moddi3 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_umoddi3 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_muldi3 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_muldi3_2 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_ashrdi3 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_lshrdi3 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_ashldi3 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_l_muls = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_l_mulu = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_l_divs = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_l_divu = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_l_mod = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_l_modu = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_fixdfdi = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_fixunsdfdi = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_floatdidf = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_d_ulltod = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_fixsfdi = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_fixunssfdi = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_floatdisf = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_ulltof = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_fixdfsi = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_fixunsdfsi = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_floatsidf = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_d_utod = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_ftoi = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_ftou = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_itof = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_utof = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_fp_round = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_fp_get_fpscr = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_fp_set_fpscr = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_adddf3 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_subdf3 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_muldf3 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_divdf3 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_dpcmp = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_dpcmpe = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_dpfeq = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_dpfne = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_dpflt = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_dpfle = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_dpfgt = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_dpfge = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_dpfor = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_dofun = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_add = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_sub = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_mul = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_div = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_cmp = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_cmpe = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_feq = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_fne = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_flt = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_fle = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_fgt = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_fge = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_for = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_f_fun = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_truncdfsf2 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_extendsfdf2 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_profile_s = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_profile_e = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_strcpy = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_memcpy = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_MIPS16_prtglue32 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    rt_MIPS16_prtglue16 = CParser_NewRTFunc(&stvoid, NULL, 2, 0);
    func_0045B910();
    func_0045C780();
}

// b210 names its double/float conversion and arithmetic helpers after the
// older PS2 runtime (dptoli, dpadd, ...)
#if MWCCPS2
#define RT_NAME(psp, ps2) ps2
#else
#define RT_NAME(psp, ps2) psp
#endif

Boolean CodeGen_ReInitRuntimeObjects(Boolean is_precompiler) {
    rt_divdi3->name = GetHashNameNodeExport("__divdi3");
    rt_udivdi3->name = GetHashNameNodeExport("__udivdi3");
    rt_moddi3->name = GetHashNameNodeExport("__moddi3");
    rt_umoddi3->name = GetHashNameNodeExport("__umoddi3");
    rt_muldi3->name = GetHashNameNodeExport("__muldi3");
    rt_muldi3_2->name = GetHashNameNodeExport("__muldi3");
    rt_ashrdi3->name = GetHashNameNodeExport("__ashrdi3");
    rt_lshrdi3->name = GetHashNameNodeExport("__lshrdi3");
    rt_ashldi3->name = GetHashNameNodeExport("__ashldi3");
    rt_l_muls->name = GetHashNameNodeExport("_l_muls");
    rt_l_mulu->name = GetHashNameNodeExport("_l_mulu");
    rt_l_divs->name = GetHashNameNodeExport("_l_divs");
    rt_l_divu->name = GetHashNameNodeExport("_l_divu");
    rt_l_mod->name = GetHashNameNodeExport("_l_mod");
    rt_l_modu->name = GetHashNameNodeExport("_l_modu");
    rt_fixdfdi->name = GetHashNameNodeExport("__fixdfdi");
    rt_fixunsdfdi->name = GetHashNameNodeExport("__fixunsdfdi");
    rt_floatdidf->name = GetHashNameNodeExport("__floatdidf");
    rt_d_ulltod->name = GetHashNameNodeExport(RT_SOFTFP("_d_ulltod"));
    rt_fixsfdi->name = GetHashNameNodeExport("__fixsfdi");
    rt_fixunssfdi->name = GetHashNameNodeExport("__fixunssfdi");
    rt_floatdisf->name = GetHashNameNodeExport("__floatdisf");
    rt_f_ulltof->name = GetHashNameNodeExport(RT_SOFTFP("_f_ulltof"));
    rt_fixdfsi->name = GetHashNameNodeExport(RT_NAME("__fixdfsi", "dptoli"));
    rt_fixunsdfsi->name = GetHashNameNodeExport(RT_NAME("__fixunsdfsi", "dptoul"));
    rt_floatsidf->name = GetHashNameNodeExport(RT_NAME("__floatsidf", "litodp"));
    rt_d_utod->name = GetHashNameNodeExport(RT_SOFTFP("_d_utod"));
    rt_f_ftoi->name = GetHashNameNodeExport(RT_NAME("_f_ftoi", "fptosi"));
    rt_f_ftou->name = GetHashNameNodeExport(RT_NAME("_f_ftou", "fptoui"));
    rt_f_itof->name = GetHashNameNodeExport(RT_SOFTFP("_f_itof"));
    rt_f_utof->name = GetHashNameNodeExport(RT_SOFTFP("_f_utof"));
    rt_fp_round->name = GetHashNameNodeExport("_fp_round");
    rt_fp_get_fpscr->name = GetHashNameNodeExport("_fp_get_fpscr");
    rt_fp_set_fpscr->name = GetHashNameNodeExport("_fp_set_fpscr");
    rt_adddf3->name = GetHashNameNodeExport(RT_NAME("__adddf3", "dpadd"));
    rt_subdf3->name = GetHashNameNodeExport(RT_NAME("__subdf3", "dpsub"));
    rt_muldf3->name = GetHashNameNodeExport(RT_NAME("__muldf3", "dpmul"));
    rt_divdf3->name = GetHashNameNodeExport(RT_NAME("__divdf3", "dpdiv"));
    rt_dpcmp->name = GetHashNameNodeExport("_dpcmp");
    rt_dpcmpe->name = GetHashNameNodeExport("_dpcmpe");
    rt_dpfeq->name = GetHashNameNodeExport("_dpfeq");
    rt_dpfne->name = GetHashNameNodeExport("_dpfne");
    rt_dpflt->name = GetHashNameNodeExport("_dpflt");
    rt_dpfle->name = GetHashNameNodeExport("_dpfle");
    rt_dpfgt->name = GetHashNameNodeExport("_dpfgt");
    rt_dpfge->name = GetHashNameNodeExport("_dpfge");
    rt_dpfor->name = GetHashNameNodeExport("_dpfor");
    rt_dofun->name = GetHashNameNodeExport(RT_NAME("_dofun", "_dpfun"));
    rt_f_add->name = GetHashNameNodeExport(RT_SOFTFP("_f_add"));
    rt_f_sub->name = GetHashNameNodeExport(RT_SOFTFP("_f_sub"));
    rt_f_mul->name = GetHashNameNodeExport(RT_SOFTFP("_f_mul"));
    rt_f_div->name = GetHashNameNodeExport(RT_SOFTFP("_f_div"));
    rt_f_cmp->name = GetHashNameNodeExport(RT_SOFTFP("_f_cmp"));
    rt_f_cmpe->name = GetHashNameNodeExport(RT_SOFTFP("_f_cmpe"));
    rt_f_feq->name = GetHashNameNodeExport(RT_SOFTFP("_f_feq"));
    rt_f_fne->name = GetHashNameNodeExport(RT_SOFTFP("_f_fne"));
    rt_f_flt->name = GetHashNameNodeExport(RT_SOFTFP("_f_flt"));
    rt_f_fle->name = GetHashNameNodeExport(RT_SOFTFP("_f_fle"));
    rt_f_fgt->name = GetHashNameNodeExport(RT_SOFTFP("_f_fgt"));
    rt_f_fge->name = GetHashNameNodeExport(RT_SOFTFP("_f_fge"));
    rt_f_for->name = GetHashNameNodeExport(RT_SOFTFP("_f_for"));
    rt_f_fun->name = GetHashNameNodeExport(RT_SOFTFP("_f_fun"));
    rt_truncdfsf2->name = GetHashNameNodeExport(RT_NAME("__truncdfsf2", "dptofp"));
    rt_extendsfdf2->name = GetHashNameNodeExport(RT_NAME("__extendsfdf2", "fptodp"));
    rt_profile_s->name = GetHashNameNodeExport("_profile_s");
    rt_profile_e->name = GetHashNameNodeExport("_profile_e");
    rt_strcpy->name = GetHashNameNodeExport("strcpy");
    rt_memcpy->name = GetHashNameNodeExport("memcpy");
    rt_MIPS16_prtglue32->name = GetHashNameNodeExport("__MIPS16_prtglue32");
    rt_MIPS16_prtglue16->name = GetHashNameNodeExport("__MIPS16_prtglue16");
    func_0045C740();
    return func_00458560(is_precompiler);
}

// --- CodeGen_Generator -------------------------------------------------

// The expression generators' result is a VOperand (2.4.7's VOperand); only the
// register is read here.

typedef struct RegTableEntry {
    UInt8 used;
    UInt8 unk01[15];
} RegTableEntry;

typedef struct ObjectList_ {
    struct ObjectList_* next;
    Object* object;
} ObjectList_;

#ifndef OBJECT_FRAMEOFFSET
#define OBJECT_FRAMEOFFSET(obj) (*(SInt32*)&((Object*)(obj))->u.var.offset) // +0x30
#endif

extern SInt16 data_00625E56;
extern SInt32 data_00624D84; // source position for new instructions
extern RegTableEntry data_00610438[128];
extern RegTableEntry data_0060A020[128];
extern RegTableEntry data_00609820[128];
extern SInt32 data_006258B8; // the function's first source offset
extern UInt8 data_00625EA7;
extern Type* data_005E8B98; // the type being returned
extern ObjectList_* data_005E8B94; // locals whose frame offset a label invalidates
extern void* data_0062468C;
extern SInt32 data_00624BA0;
extern SInt32 data_006246CC;
extern SInt32 data_00624810;
extern CLabel* data_006242A4; // returnlabel
extern UInt8 data_00625E83; // something jumps to returnlabel
extern Object* data_00625078; // gFunction
extern void* data_00624D48;
extern UInt8 data_00625EAD;
extern void* data_00625008;
extern void* data_006242D4;
extern SInt32 data_00625350;
extern UInt8 data_00625EB6; // the function has varargs
extern SInt32 data_005E8B9C;
extern SInt32 data_00624FCC; // locals start
extern ObjectList_* data_005E8B90; // locals to place last
extern SInt32 data_00624DCC;
extern void* data_00624C3C;
extern UInt8 data_00625E7E;
extern UInt8 data_00625FCF;
extern Object* data_006258DC;
extern void (*data_005D0140[])(ENode* expr, void* target, VOperand* op);

Object* func_004366D0(void); // createstaticinitobject
extern void func_0049E310(void); // initializeexceptiontables
extern void func_004A16C0(Statement* stmts, char* name, char* title); // DumpIR
extern Statement* func_004A16F0(Object* func, Statement* stmts); // COpt_Optimizer
extern void func_004A25E0(Statement* stmts); // expandIR32
extern void func_0046FEC0(void); // initpcode
extern void func_004A46A0(void);
extern void func_004621F0(void);
void func_004388C0(
    void (*fn)(Object*, SInt16, Boolean), int varargs
); // process_arguments (64-bit long)
void func_00438A90(void (*fn)(Object*, SInt16, Boolean), int varargs); // process_arguments
void func_00437C90(Object* obj, SInt16 reg, Boolean isint);
void func_00437C20(Object* obj, SInt16 reg, Boolean isint);
void func_00437330(Object* obj, SInt16 reg, Boolean isint);
void func_00437D80(void);
extern void func_004A4A20(Object* rtfunc); // call the profiler
extern void func_00462050(void); // open_temp_registers
extern void func_00461FC0(void); // check_temp_registers
extern void func_00461F80(void); // close_temp_registers
extern void func_00482000(Statement* stmt); // InlineAsm_TranslateIRtoPCode
extern void func_0049C250(ENode* cond, CLabel* label, Boolean truth); // conditionalstatement
extern void func_00480710(MPCodeLabel* label); // branch_always
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 5 // b145 asserts
void func_00436F90(ENode* expr, Boolean islast); // returnstatement
extern void func_004A4A40(ENode* expr, void* info); // switchstatement
extern SInt16 func_004A5360(Object* obj); // a local's base register
extern void func_004808E0(
    SInt16 op, SInt16 reg, SInt16 base, Object* obj, SInt32 offset
); // store a register to a local
extern void func_00480B40(
    SInt16 op, SInt16 reg, SInt16 base, Object* obj, SInt32 offset
); // load a local into a register
extern void func_00494960(ENode* expr, void* target, VOperand* op);
extern void func_0047F2D0(VOperand* dst, VOperand* src);
extern void func_004A8310(Object* func, int unk, UInt32* masks); // branch_subroutine
extern void func_0046FA90(MPCodeLabel* label);
extern void func_00490370(void);

void newstatement(SInt32 sourceoffset, SInt32 value) {
    MPCodeBlock* block = data_006246B4;
    data_0062478C = value;
    if (block && !block->pcodecount) {
        block->weight = value;
    }
    if (sourceoffset > 0 && sourceoffset > data_00624D84) {
        data_00624D84 = sourceoffset;
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// Consecutive labels share one pcode label.
void assign_labels(Statement* stmt) {
    Statement* prev = NULL;
    for (; stmt; stmt = stmt->next) {
        if (stmt->type == ST_LABEL && !stmt->label->pclabel) {
            if (prev && prev->type == ST_LABEL) {
                stmt->label->pclabel = prev->label->pclabel;
            } else {
                stmt->label->pclabel = (struct PCodeLabel*)makepclabel();
            }
        }
        prev = stmt;
    }
}

// Start the function's code: its used-register record and, when asked, the
// entry pseudo-instruction.
void func_00436F00(Object* func, int entry) {
    data_006241D8 = galloc(sizeof(UsedRegs));
    memclrw(data_006241D8, sizeof(UsedRegs));
    *(UsedRegs**)&func->toc = data_006241D8;
    data_006242D4 = 0;
    if (entry) {
        emitpcode(MACRO_OP(0xA3));
        data_006246B4->last->unk20 = data_006258B8;
    }
    newstatement(data_006258B8, 0);
}

void CodeGen_Generator(
    Statement* statements, Object* func, UInt8 oldStyleParameters, Boolean callOnModuleBind
) {
    Statement* stmt;
    Statement* prev;
    CLabel* label;
    ObjectList_* list;
    MPCodeBlock* block;
    FuncArg* arg;
    char* name;
    Boolean islast;
    int skip;
    int i;
    VOperand op;
    VOperand op2;

    data_00625E56 = 0;
    for (i = 0; i < BE_MAX_CLASS_REGS; i++) {
        data_00610438[i].used = 0;
    }
    for (i = 0; i < BE_MAX_CLASS_REGS; i++) {
        data_0060A020[i].used = 0;
    }
    for (i = 0; i < BE_MAX_CLASS_REGS; i++) {
        data_00609820[i].used = 0;
    }
    data_00624D84 = data_006258B8;

    if (!func) {
        func = func_004366D0();
        skip = 0;
    } else {
        skip = 0;
    }
    if (skip) {
#if MWCCPS2
        goto skipped;
#else
        return;
#endif
    }

    data_00625AD0 = MACRO_OP(0x82);
    data_00625B58 = MACRO_OP(0x81);
    data_00624778 = 0;
    data_00625EA7 = 0;
    data_005E8B98 = NULL;
    data_005E8B94 = NULL;
    needs_cleanup = 0;
    data_00624DEC = 0;
    data_0062468C = 0;
    data_00625070 = NULL;
    data_00624BA0 = 0;
    data_006246CC = 0;
    data_00624810 = 0;
#if !MWCCPS2 || MWCCPS2_BUILD >= 210
    data_0062525C = 0;
#endif
    if (copts.cplusplus && copts.exceptions) {
        func_0049E310();
    }
    data_006242A4 = cleanreturnlabel = newlabel();
    data_00625E83 = 0;

    data_00625078 = func;
    if (func) {
        name = CMangler_GetLinkName(func)->name;
    } else {
        name = "(init)";
    }

    if (copts.dumpir) {
        func_004A16C0(statements, name, "Original IR");
    }
    statements = func_004A16F0(func, statements);
    if (copts.dumpir) {
        func_004A16C0(statements, name, "After IR Optimization");
    }
    data_00624D48 = 0;
    data_00625EAD = 0;
    data_00625008 = 0;
    func_004A25E0(statements);
    if (copts.dumpir) {
        func_004A16C0(statements, name, "After expandIR32");
    }

    func_0046FEC0();
#if MWCCPSP_NATIVE // 3.0.1 evaluates arguments right to left
    {
        MPCodeLabel* l = makepclabel();
        pclabel(block = makepcblock(), l);
    }
#else
    pclabel(block = makepcblock(), makepclabel());
#endif
    data_00624FD0 = block;
    func_00436F00(func, 1);

    func_004A46A0();
    func_004621F0();
    data_00625350 = copts.optimizationlevel && !data_00624DEC;
    if (data_006246CC && data_00624810) {
        data_00625350 = 1;
    }
#if !MWCCPS2 || MWCCPS2_BUILD >= 210
    if (data_0062525C) {
        data_00625350 = 0;
        data_00624DEC |= 1;
    }
#endif

    for (arg = TYPE_FUNC(func->type)->args; arg && arg != &elipsis; arg = arg->next) {}
    data_00625EB6 = arg == &elipsis;

    data_00624FCC = data_005E8B9C = 0;
    data_005E8B90 = NULL;
#if MWCCPS2
    // b210 only supports the 64-bit-long ABI here; the varargs register
    // count is 11 (a0-a7, t0-t3 aliases) or 7
    if (copts.mips_long64) {
        func_004388C0(func_00437C90, 0);
        data_005E8B9C = data_00624FCC;
        func_004388C0(func_00437C20, 0);
    } else {
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -28 // b145 asserts
        CError_FATAL(3823);
    }
    func_00437D80();
    if (copts.mips_long64) {
        func_004388C0(func_00437330, data_00625EB6);
        if (data_00625EB6 && data_00624DCC <= LAST_GPR) {
            data_005E8B9C = data_00624FCC;
            func_004388C0(func_00437C20, 0);
        }
    } else {
        func_00438A90(func_00437330, data_00625EB6);
        if (data_00625EB6 && data_00624DCC <= LAST_GPR) {
            data_005E8B9C = data_00624FCC;
            func_00438A90(func_00437C20, 0);
        }
    }
#else
    if (copts.mips_long64) {
        func_004388C0(func_00437C90, 0);
        data_005E8B9C = data_00624FCC;
        func_004388C0(func_00437C20, 0);
    } else {
        func_00438A90(func_00437C90, 0);
        data_005E8B9C = data_00624FCC;
        func_00438A90(func_00437C20, 0);
    }
    func_00437D80();
    if (copts.mips_long64) {
        func_004388C0(func_00437330, data_00625EB6);
        if (data_00625EB6 && data_00624DCC <= 11) {
            data_005E8B9C = data_00624FCC;
            func_004388C0(func_00437C20, 0);
        }
    } else {
        func_00438A90(func_00437330, data_00625EB6);
        if (data_00625EB6 && data_00624DCC <= 11) {
            data_005E8B9C = data_00624FCC;
            func_00438A90(func_00437C20, 0);
        }
    }
#endif

    if (copts.mips_option01) {
        func_004A4A20(rt_profile_s);
    }

    assign_labels(statements->next);
    data_006242A4->pclabel = (struct PCodeLabel*)makepclabel();

    data_00624C3C = 0;
    data_00625E7E = 0;
    data_00625FCF = 0;
    func_00462050();

    for (stmt = statements->next; stmt; stmt = stmt->next) {
        data_00625070 = stmt;
        switch (stmt->type) {
            case ST_NOP:
                break;
            case 16: // ST_ASM
                newstatement(stmt->sourceoffset, stmt->value);
                func_00482000(stmt);
                if (!stmt->next || (stmt->next && stmt->next->type != 16)) {
                    data_00625FCF = 0;
                }
                if (stmt->next && stmt->next->type != 16 && stmt->next->type != ST_LABEL) {
                    data_006246B4->flags2E = 0;
                    data_00625E56 = 0;
                }
                break;
            case ST_EXPRESSION:
                newstatement(stmt->sourceoffset, stmt->value);
                data_005D0140[stmt->expr->type](stmt->expr, NULL, &op);
                break;
            case ST_LABEL:
                label = stmt->label;
                if (!label->pclabel) {
                    label->pclabel = (struct PCodeLabel*)makepclabel();
                }
                if (!((MPCodeLabel*)label->pclabel)->resolved) {
                    func_0046FA90((MPCodeLabel*)label->pclabel);
                    func_00490370();
                    data_00625E7E = 1;
                }
                for (list = data_005E8B94; list; list = list->next) {
                    OBJECT_FRAMEOFFSET(list->object) = 0;
                }
                if (data_00625E56 &&
                    stmt->next &&
                    stmt->next->type != 16 &&
                    stmt->next->type != ST_LABEL) {
                    data_00625E56 = 0;
                }
                break;
            case ST_IFGOTO:
                newstatement(stmt->sourceoffset, stmt->value);
                func_0049C250(stmt->expr, stmt->label, 1);
                break;
            case ST_IFNGOTO:
                newstatement(stmt->sourceoffset, stmt->value);
                func_0049C250(stmt->expr, stmt->label, 0);
                break;
            case ST_GOTO:
                newstatement(stmt->sourceoffset, stmt->value);
                label = stmt->label;
                if (data_006242A4 == label) {
                    data_00625E83 = 1;
                }
                if (!label->pclabel) {
                    label->pclabel = (struct PCodeLabel*)makepclabel();
                }
                func_00480710((MPCodeLabel*)label->pclabel);
                for (list = data_005E8B94; list; list = list->next) {
                    OBJECT_FRAMEOFFSET(list->object) = 0;
                }
                break;
            case 15: // ST_GOTOEXPR
                newstatement(stmt->sourceoffset, stmt->value);
#if MWCCPS2
                CError_FATAL(3921);
#else
                CError_FATAL(3938);
#endif
            case ST_RETURN:
                newstatement(stmt->sourceoffset, stmt->value);
                if (data_006246B4->flags2E & 0x4000) {
                    data_006246B4->flags2E &= ~0x4000;
                }
                for (prev = stmt->next; prev; prev = prev->next) {
                    if (prev->type > ST_LABEL) {
                        islast = 0;
                        goto gotlast;
                    }
                }
                islast = 1;
gotlast:
                func_00436F90(stmt->expr, islast);
                break;
            case ST_SWITCH:
                newstatement(stmt->sourceoffset, stmt->value);
                func_004A4A40(stmt->expr, stmt->label);
                break;
            case 12: // ST_BEGINCATCH
            {
                Object* obj = stmt->expr->data.objref;
                func_004808E0(
                    MACRO_OP(0x77), 29, func_004A5360(obj), obj, OBJECT_FRAMEOFFSET(obj) + 0x14
                );
            } break;
            case 14: // ST_ENDCATCHDTOR
            {
                UInt32 masks[2] = {0x10, 0};
                func_00494960(stmt->expr, NULL, &op2);
                func_0047F2D0(&op2, &op2);
                emitpcode(data_00625E48, 4, op2.reg);
                func_004A8310(data_006258DC, 0, masks);
            }
            case 13: // ST_ENDCATCH
            {
                Object* obj = stmt->expr->data.objref;
                func_00480B40(
                    MACRO_OP(0x55), 29, func_004A5360(obj), obj, OBJECT_FRAMEOFFSET(obj) + 0x14
                );
            } break;
            default:
#if MWCCPS2
                CError_FATAL(3952);
#else
                CError_FATAL(3969);
#endif
        }
        func_00461FC0();
    }

    func_00461F80();
    data_00625E7E = 0;
    if (data_00625E83) {
        label = data_006242A4;
        if (!label->pclabel) {
            label->pclabel = (struct PCodeLabel*)makepclabel();
        }
        if (!((MPCodeLabel*)label->pclabel)->resolved) {
            func_0046FA90((MPCodeLabel*)label->pclabel);
            func_00490370();
            data_00625E7E = 1;
        }
        for (list = data_005E8B94; list; list = list->next) {
            OBJECT_FRAMEOFFSET(list->object) = 0;
        }
    }
    newstatement(data_00625548, 0);
    CodeGen_GenerateFunction(func, 1, 1, 0);
#if MWCCPS2
skipped:;
#endif
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// --- return values -------------------------------------------------------

#define IS_SOFTFLOAT(t) \
    ((IS_SINGLE(t) && !(copts.fp_library & 1)) || (IS_DOUBLE(t) && !(copts.fp_library & 2)))
#define IS_HARDFLOAT(t) \
    ((IS_SINGLE(t) && (copts.fp_library & 1)) || (IS_DOUBLE(t) && (copts.fp_library & 2)))

extern Boolean func_00464EE0(Type* type);
extern Type* func_00464B40(Type* type); // the struct's single floating-point member
extern Type* func_00464BE0(Type* type); // the type of the struct's only member

UInt8 func_00436B30(Type* type);

typedef struct RegList {
    SInt32 count;
    struct {
        UInt8 rclass;
        UInt8 pad;
        SInt16 reg;
    } regs[0x81];
} RegList;

extern RegList data_005E8740; // registers live at the function's exit
extern void func_004C3B00(RegList* list);

inline void addexitreg(UInt8 rclass, SInt16 reg) {
    SInt32 n = data_005E8740.count;
    if (n > 0x80) {
        CError_FATAL(2302);
    }
    data_005E8740.regs[n].rclass = rclass;
    data_005E8740.regs[n].reg = reg;
    data_005E8740.count = n + 1;
}

extern SInt32 data_00624B4C; // init section
extern SInt32 data_00624BE8; // pinit section

// The static initializer function __sinit_<file> (3.0.1 createstaticinitobject):
// it goes in .init, and a pointer to it, .p__sinit_<file>, goes in .ctor.
Object* func_004366D0(void) {
    unsigned char fname[256];
    char* buf;
    char* pname;
    TypeFunc* tfunc;
    Object* func;
    Object* ptr;
    ObjDataReloc* reloc;
    SInt32 data;

    COS_FileGetFSSpecInfo(&cparamblkptr->sourcefile, NULL, 0x104, (char*)fname, sizeof(fname));
    buf = lalloc(fname[0] + 10);
    sprintf(buf, "__sinit_%*.*s", -fname[0], fname[0], fname);

    tfunc = galloc(sizeof(TypeFunc));
    memclrw(tfunc, sizeof(TypeFunc));
    tfunc->type = TYPEFUNC;
    tfunc->functype = &stvoid;
    tfunc->args = NULL;
    tfunc->flags = FUNC_DEFINED;

    func = galloc(sizeof(Object));
    memclrw(func, sizeof(Object));
    func->type = (Type*)tfunc;
    func->name = GetHashNameNodeExport(buf);
    func->sclass = TK_STATIC;
    func->datatype = DFUNC;
    func->section = data_00624B4C;

    ptr = galloc(sizeof(Object));
    memclrw(ptr, sizeof(Object));
    pname = lalloc(strlen(buf) + 4);
    sprintf(pname, ".p%s", buf);
    ptr->type = CDecl_NewPointerType((Type*)tfunc);
    ptr->name = GetHashNameNodeExport(pname);
    ptr->sclass = TK_STATIC;
    ptr->datatype = 1;
    ptr->section = data_00624BE8;

    reloc = galloc(sizeof(ObjDataReloc));
    memclrw(reloc, sizeof(ObjDataReloc));
    reloc->target = func;
    data = 0;
    ObjGen_EmitData(ptr, &data, reloc, ptr->type->size);
    return func;
}

extern struct Alias* worst_case_func;

// 0x4342E0: load a nonzero constant into a register
static void func_004342E0(SInt32 value, SInt16 reg) {
    if (value) {
        if (value >= -0x8000 && value < 0x8000) {
            emitpcode(3, reg, 0, value); // addiu
        } else if (value >= 0 && value < 0x10000) {
            emitpcode(0xBF, reg, 0, value); // ori
        } else if (value & 0xFFFF) {
            emitpcode(0x90, reg, (UInt16)(value >> 16)); // lui
            emitpcode(0xBF, reg, reg, value & 0xFFFF);
        } else {
            emitpcode(0x90, reg, (UInt16)(value >> 16));
        }
    } else {
        CError_FATAL(3990);
    }
}

// 0x433FB0: a thunk that adjusts this (through the vtable when ctoroffset
// is not negative) and jumps to func; the adjustment goes in the delay slot
void CodeGen_GenVDispatchThunk(
    Object* thunkobj, Object* func, SInt32 this_delta, SInt32 return_delta, SInt32 ctoroffset
) {
    MPCode* slot;
    MPCode* jump;
    MPCodeBlock* block;
    Type* rettype;
    SInt16 reg;
    Boolean memret;

    slot = NULL;
    arguments = NULL;
    locals = NULL;
    data_00624778 = 0;
    CError_ASSERT(4042, !return_delta);
    data_00625EA7 = 0;
    data_00625008 = 0;
    func_0046FEC0();
#if MWCCPSP_NATIVE // 3.0.1 evaluates arguments right to left
    {
        MPCodeLabel* l = makepclabel();
        pclabel(block = makepcblock(), l);
    }
#else
    pclabel(block = makepcblock(), makepclabel());
#endif
    data_00624FD0 = block;
    func_00436F00(thunkobj, 0);

    // this is in $a0, or $a1 when the result goes through a hidden pointer
    memret = FALSE;
    rettype = TYPE_FUNC(func->type)->functype;
    if (rettype &&
        (IS_TYPE_STRUCT(rettype) ||
         IS_TYPE_CLASS(rettype) ||
         IS_TYPE_ARRAY(rettype) ||
         (IS_TYPE_MEMBERPOINTER(rettype) && rettype->size == 12))) {
        memret = TRUE;
    }
    reg = memret ? 5 : 4;

    if (ctoroffset >= 0) {
        emitpcode(0x9D, 25, 0, ctoroffset, reg); // lw $t9, ctoroffset(this)
        if (this_delta) {
            if (this_delta <= 0x8000 && this_delta >= -0x8000) {
                emitpcode(3, reg, reg, this_delta);
            } else {
                func_004342E0(this_delta, 24);
                emitpcode(4, reg, reg, 24); // addu
            }
        }
        slot = makepcode(4, reg, reg, 25);
    } else if (this_delta) {
        if (this_delta <= 0x8000 && this_delta >= -0x8000) {
            slot = makepcode(3, reg, reg, this_delta);
        } else {
            func_004342E0(this_delta, 24);
            emitpcode(4, reg, reg, 24);
        }
    }

    CError_ASSERT(4098, !return_delta);
    if (copts.mips_option08) {
        jump = func_00480420(0x39, 2);
        jump->unk38 = 10;
    } else {
        jump = func_00480420(0x3A, 2);
        jump->unk38 = 4;
    }
    jump->flags |= 0x80000;
    jump->relobj = func;
    jump->alias = worst_case_func;
    jump->args[0].kind = 4;
    jump->args[0].value = 0;
    MOP_SETOBJ(jump->args[0], func);
    jump->args[1].kind = 0;
    jump->args[1].rclass = 0;
    jump->args[1].reg = reg;
    jump->args[1].flags |= 1;

    if (!slot) {
        slot = makepcode(0xBA); // nop
    }
    appendpcode(data_006246B4, slot);
    data_006246B4->last->flags |= 0x40000000;
    appendpcode(data_006246B4, jump);
    CodeGen_GenerateFunction(thunkobj, 0, 0, 1);
}

// Note the registers that carry the function's result, and $sp.
void func_00436860(Object* func) {
    data_005E8740.count = 0;
    switch (func_00436B30(TYPE_FUNC(func->type)->functype)) {
        case 0:
        case 1:
            break;
        case 2:
            addexitreg(0, 2);
            break;
        case 3:
            addexitreg(0, 2);
            addexitreg(0, 3);
            break;
        case 4:
            addexitreg(1, 0);
            break;
        case 5:
            addexitreg(1, 0);
            addexitreg(1, 1);
            break;
        case 6:
            addexitreg(1, 0);
            addexitreg(1, 2);
            break;
        default:
            CError_FATAL(2364);
    }
    addexitreg(0, 29);
#if MWCCPS2
    {
        // b210: data objects living in a register stay live at the exit
        PS2RegVarNode* node;

        for (node = ps2_objgen_636288; node; node = node->next) {
            if (node->object) {
                addexitreg(node->info->unk24[2], node->info->reg);
            }
        }
    }
#endif
    func_004C3B00(&data_005E8740);
}

// How a value of this type is returned: 0 nothing, 1 in memory, 2 in $v0,
// 3 in $v0/$v1, 4 in $f0.
UInt8 func_00436B30(Type* type) {
    if (!type) {
        return 0;
    }
#if !MWCCPS2 // b210 (0x436CC0): a long long or soft double is one 64-bit register
    if (((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8) ||
        (type->type == TYPEFLOAT &&
         IS_SOFTFLOAT(type) &&
         type->type == TYPEFLOAT &&
         IS_DOUBLE(type))) {
        return 3;
    }
#endif
    if (type->type == TYPEFLOAT && IS_HARDFLOAT(type)) {
        return 4;
    }
    if (type->type == TYPEMEMBERPOINTER && type->size == 4) {
        if (func_00464EE0(type)) {
            return 1;
        }
        return 2;
    }
    if (copts.mips_long64 && (type->type == TYPESTRUCT || type->type == TYPECLASS)) {
        if (func_00464EE0(type)) {
            return 1;
        }
        if (func_00464B40(type)) {
            return 4;
        }
        if (func_00464BE0(type)) {
            return 2;
        }
    }
    if (type->type == TYPECLASS) {
        return 1;
    }
    if (type &&
        (type->type == TYPEARRAY ||
         type->type == TYPESTRUCT ||
         type->type == TYPECLASS ||
         (type->type == TYPEMEMBERPOINTER && type->size == 12))) {
        return 1;
    }
    return 2;
}

extern void func_004A4A20(Object* rtfunc);
extern void func_004A64C0(VOperand* op, Type* type, int flags); // make the operand a value
extern void func_0047FC80(
    VOperand op, UInt16 opcode, int unk, Boolean pair
); // move to the result register

#undef CError_B145_SHIFT
#define CError_B145_SHIFT 5 // b145 asserts
void func_00436F90(ENode* expr, Boolean islast) {
    Type* type;
    UInt16 opcode;
    Boolean pair;
    UInt8 kind;
    ObjectList_* list;
    VOperand op;

    if (expr) {
        data_005E8B98 = expr->rtype;
        type = expr->rtype;
        kind = func_00436B30(type);
        data_005D0140[expr->type](expr, NULL, &op);
        if (copts.mips_option01) {
            func_004A4A20(rt_profile_e);
        }
        if (kind == 4) {
            if (op.kind) {
                func_004A64C0(
                    &op,
                    (type->type == TYPESTRUCT || type->type == TYPECLASS) ? func_00464BE0(type)
                                                                          : type,
                    0
                );
            }
            opcode = IS_SINGLE(type) ? MACRO_OP(0x99) : MACRO_OP(0x9A);
            func_0047FC80(op, opcode, 1, 0);
        } else if (kind == 5) {
            if (op.kind) {
                func_004A64C0(&op, type, 0);
            }
            opcode = IS_SINGLE(type) ? MACRO_OP(0x99) : MACRO_OP(0x9A);
            func_0047FC80(op, opcode, 1, 0);
        } else if (kind == 2 || kind == 3) {
            if (op.kind) {
                func_004A64C0(&op, type, 0);
            }
#if MWCCPS2 // b210 (0x437090): one 64-bit register, never a pair
            pair = FALSE;
#else
            pair = ((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8) ||
                (type->type == TYPEFLOAT && IS_DOUBLE(type));
#endif
            opcode = ((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 16)
                ? data_00625E74
                : data_00625E48;
            func_0047FC80(op, opcode, 0, pair);
        } else if (kind != 1) {
            CError_FATAL(1960);
        }
    } else if (copts.mips_option01) {
        func_004A4A20(rt_profile_e);
    }

    if (!islast) {
        if (!data_006242A4->pclabel) {
            data_006242A4->pclabel = (struct PCodeLabel*)makepclabel();
        }
        func_00480710((MPCodeLabel*)data_006242A4->pclabel);
        data_00625E83 = 1;
        for (list = data_005E8B94; list; list = list->next) {
            OBJECT_FRAMEOFFSET(list->object) = 0;
        }
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// --- locals --------------------------------------------------------------

extern SInt32 data_00625350; // registers are assigned by coloring (optimizing)
extern SInt32 data_00624650; // locals' frame size
extern SInt32 data_00624C1C[7]; // free saved registers per class ([1] 0x624C20)
void func_00438160(void);
extern void func_004621A0(Object* obj, int unk); // give the local a saved register
extern void func_004A4660(Object* obj); // give a local a stack frame slot
extern SInt16 func_00464E20(Object* obj); // a local's alignment

#define LOCAL_REG(obj) ((obj)->datatype == DLOCAL ? (obj)->u.var.info->reg : 0)

// Without the optimizer: the most-used integer locals get saved registers.
void func_00437FE0(void) {
    ObjectList* list;
    Object* obj;
    Object* best;
    SInt32 bestusage;
    VarInfo* vi;
    Type* type;
    SInt32 usage;

    while (data_00624C1C[0]) {
        best = NULL;
        bestusage = -1;
        if (!(data_00624DEC & 2)) {
            for (list = locals; list; list = list->next) {
                obj = list->object;
                vi = Registers_GetVarInfo(obj);
                type = obj->type;
                if (!vi->reg && vi->used && !vi->noregister && !is_volatile_object(obj)) {
                    usage = vi->usage;
                    if (usage >= bestusage &&
                        usage >= 1 &&
                        (type->type == TYPEINT ||
                         type->type == TYPEENUM ||
                         type->type == TYPEPOINTER ||
                         (type->type == TYPEMEMBERPOINTER && type->size == 4)) &&
#if !MWCCPS2 // b210: a long long fits one 64-bit GPR
                        !((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8) &&
#endif
                        !(type->type == TYPEFLOAT && IS_SOFTFLOAT(type))) {
                        best = obj;
                        bestusage = usage;
                    }
                }
            }
        }
        if (!best) {
            break;
        }
        func_004621A0(best, 0);
        data_006241D8->intregs |= BIT32(Registers_GetVarInfo(best)->reg);
    }
}

// The floating-point counterpart; it only checks the candidates.
void func_00437F40(void) {
    ObjectList* list;
    Object* obj;
    VarInfo* vi;

    if (data_00624C1C[1]) {
        if (!(data_00624DEC & 2)) {
            for (list = arguments; list; list = list->next) {
                obj = list->object;
                vi = Registers_GetVarInfo(obj);
                if (!vi->reg && vi->used && !vi->noregister) {
                    is_volatile_object(obj);
                }
            }
        }
        if (!(data_00624DEC & 2)) {
            for (list = locals; list; list = list->next) {
                obj = list->object;
                vi = Registers_GetVarInfo(obj);
                if (!vi->reg && vi->used && !vi->noregister) {
                    is_volatile_object(obj);
                }
            }
        }
    }
}

// allocate_locals: registers first, then frame slots by alignment (big
// objects first when the frame would not fit a 16-bit offset).
#if MWCCPS2
extern void ps2_argcb_437DA0(
    Object* obj, SInt16 reg, Boolean b
); // b210-only process_arguments callback
#endif
void func_00437D80(void) {
    ObjectList* list;
    Object* obj;
    SInt32 align;

#if MWCCPS2
    // b210 (0x437BB0): first a pass over the arguments with its own callback
    if (data_00625EA9 && !data_00625350 && !copts.mips_long64) {
        func_00438A90(ps2_argcb_437DA0, 0);
    }
#endif
    if (data_00625350) {
        func_00438160();
    } else {
        func_00437FE0();
        if (has_fpu()) {
            func_00437F40();
        }
    }

    data_00624650 = 0;
    if (locals) {
        for (align = 1; align <= 0x2000; align += align) {
            for (list = locals; list; list = list->next) {
                obj = list->object;
                if (!LOCAL_REG(obj) &&
                    Registers_GetVarInfo(obj)->used &&
                    func_00464E20(obj) == align) {
                    func_004A4660(obj);
                }
            }
        }
    }

    if (data_00624650 > 0x7FFF) {
        data_00624650 = 0;
        for (list = locals; list; list = list->next) {
            obj = list->object;
            if (!LOCAL_REG(obj) &&
                Registers_GetVarInfo(obj)->used &&
                list->object->type->size > 8) {
                func_004A4660(obj);
            }
        }
        for (align = 1; align <= 0x2000; align += align) {
            for (list = locals; list; list = list->next) {
                obj = list->object;
                if (!LOCAL_REG(obj) &&
                    Registers_GetVarInfo(obj)->used &&
                    obj->type->size <= 8 &&
                    func_00464E20(obj) == align) {
                    func_004A4660(obj);
                }
            }
        }
    }

    for (; data_005E8B90; data_005E8B90 = data_005E8B90->next) {
        func_004A4660(data_005E8B90->object);
    }
}

// --- arguments -----------------------------------------------------------

extern SInt32 data_00624DC8; // next floating-point argument register
extern SInt32 data_00624788; // argument slot size
extern SInt16 CMach_GetTypeAlign(Type* type); // alignment
extern MOperand func_004C3E70(SInt16 reg, UInt8 rclass, UInt16 flags);

// Hand each argument its register ($a0-$t3 or $f12-$f19, 0 when it goes
// on the stack); a varargs function stores the remaining registers.
void func_00438A90(void (*fn)(Object*, SInt16, Boolean), int varargs) {
    ObjectList* list;
    Object* obj;
    Type* type;
    MPCode* pc;
    SInt16 gpr;
    SInt16 fpr;

    gpr = 4;
    fpr = 12;
    for (list = arguments; list; list = list->next) {
        obj = list->object;
        type = obj->type;
        if ((type->type == TYPEFLOAT && IS_HARDFLOAT(type)) || func_00464B40(type)) {
            fn(obj, fpr > 19 ? 0 : fpr, 0);
            fpr++;
        } else if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
            if (type->type == TYPEFLOAT && IS_DOUBLE(type)) {
                if (gpr & 1) {
                    gpr++;
                }
                fn(obj, gpr + 1 > 11 ? 0 : gpr, 1);
                gpr += 2;
            } else {
                fn(obj, gpr > 11 ? 0 : gpr, 1);
                gpr++;
            }
        } else {
            if (CMach_GetTypeAlign(type) == 8 && (gpr & 1)) {
                gpr++;
            }
            if ((type->type == TYPEINT ||
                 type->type == TYPEENUM ||
                 type->type == TYPEPOINTER ||
                 (type->type == TYPEMEMBERPOINTER && type->size == 4)) &&
                (type->type == TYPEINT || type->type == TYPEENUM) &&
                type->size == 8) {
                if (gpr & 1) {
                    gpr++;
                }
                fn(obj, gpr + 1 > 11 ? 0 : gpr, 1);
                gpr += 2;
            } else {
                fn(obj, gpr > 11 ? 0 : gpr, 1);
                gpr++;
            }
        }
    }

    data_00624DCC = gpr;
    data_00624DC8 = fpr;
    if (varargs) {
        data_00625EA7 = 1;
        for (; gpr <= 11; gpr++) {
            pc = func_00480420(MACRO_OP(0x77), 3);
            pc->args[0].kind = 0;
            pc->args[0].reg = gpr;
            pc->args[0].rclass = 0;
            pc->args[0].flags = 1;
            pc->args[1].kind = 4;
            pc->args[1].value = data_00624FCC;
            MOP_SETOBJ(pc->args[1], galloc(sizeof(Object)));
            memset((void*)MOP_OBJ(pc->args[1]), 0, sizeof(Object));
            ((Object*)MOP_OBJ(pc->args[1]))->sclass = 0x102;
            ((Object*)MOP_OBJ(pc->args[1]))->datatype = DLOCAL;
            pc->args[2] = func_004C3E70(29, 0, 1);
            appendpcode(data_006246B4, pc);
            data_00624FCC += data_00624788;
        }
    }
}

// --- misc ----------------------------------------------------------------

extern Boolean func_004AF320(void); // VarInfos must outlive the function

VarInfo* CodeGen_GetNewVarInfo(void) {
    VarInfo* vi;

    if (func_004AF320()) {
        vi = galloc(sizeof(VarInfo));
    } else {
        vi = lalloc(sizeof(VarInfo));
    }
    memclrw(vi, sizeof(VarInfo));
    vi->deftoken = *CPrep_CurStreamElement();
    vi->varnumber = localcount++;
    return vi;
}

// data_00625EA7 (UInt8) is declared above
extern char data_00625EAE;

// 0x46EF90: turns the function's one call into a jump when it ends the
// function (or precedes a lone return block) and no callee-saved register
// is written.
void BE_TailCalls(void) {
    UsedRegs* regs;
    MPCodeBlock* block;
    MPCodeBlock* prev;
    MPCodeBlock* b;
    MPBlockLink* pred;
    MPCode* call;
    MPCode* pc;
    MOperand* op;
    UInt16 origop;
    int count;
    int i;
    Boolean hasarg;
    Boolean hassp;

    regs = data_006241D8;
    if ((regs->intregs & 0x40FF0000) ||
        (regs->fpregs & 0xFFF00000) ||
        data_00625EA7 ||
        data_00625EAE) {
        return;
    }
    for (block = data_00624FD0; block->next; block = block->next) {}
    if (!block) {
        return;
    }
    if (!block->first) {
        pred = block->predecessors;
        if (!pred || pred->nextpred || !(block = pred->from)) {
            return;
        }
    }
    call = block->last;
    if (!call) {
        return;
    }
    if (call->op == MACRO_OP(0xA4) && !call->prev) {
        pred = block->predecessors;
        if (!pred->from) {
            return;
        }
        prev = block->prev;
        if (pred->from->number != prev->number) {
            if (!pred->nextpred || pred->nextpred->from->number != prev->number) {
                return;
            }
        }
        call = prev->last;
        if (!call) {
            return;
        }
    } else if (call->op == MACRO_OP(0xA4)) {
        call = call->prev;
    }
    origop = call->op;
    if (origop != 0x3C && origop != 0x3D && origop != 0x38) {
        return;
    }
    for (b = data_00624FD0; b; b = b->next) {
        for (pc = b->first; pc; pc = pc->next) {
            if ((pc->flags & 8) && pc != call) {
                return;
            }
        }
    }
    // no argument register set up with the stack pointer just before
    for (count = 0, pc = call; (pc = pc->prev);) {
        if (pc->op == MACRO_OP(0x44)) {
            hasarg = 0;
            hassp = 0;
            for (i = 0, op = pc->args; i < pc->argcount; i++, op++) {
                if (op->kind == 0 && op->rclass == 0) {
                    if (op->reg >= 4 && op->reg <= 7) {
                        hasarg = 1;
                    } else if (op->reg == 29) {
                        hassp = 1;
                    }
                }
            }
            if (hasarg == 1 && hassp == 1) {
                return;
            }
        }
        if (++count > 10) {
            break;
        }
    }
    if (origop == 0x3C) {
        call->op = 0x3A;
    }
    if (call->op == 0x38) {
        call->op = 0x39;
    } else if (call->op == 0x3D) {
        MOperand tmp;

        call->op = 0x3B;
        tmp = call->args[0];
        call->args[0] = call->args[1];
        call->args[1] = tmp;
    }
    call->flags |= 0x80000;
    data_00625EA4 = 1;
    data_006241D8->intregs &= 0x7FFFFFFF;
}

// 0x435AA0: a register-allocated local "name'reg" standing for obj in a
// register (the store-load optimization).
#if MWCCPS2
// b210 (0x435C40) passes the register number to sprintf as a whole int
void func_00435AA0(Object* obj, char rclass, int reg) {
#else
void func_00435AA0(Object* obj, char rclass, short reg) {
#endif
    Type* type;
    Object* local;
    char* name;
    VarInfo* vi;
    ObjectList* list;

    type = obj->type;
    local = lalloc(sizeof(Object));
    memclrw(local, sizeof(Object));
    name = lalloc(strlen(obj->name->name) + 0x16);
    sprintf(name, "%s'%d", obj->name->name, reg);
    local->otype = OT_OBJECT;
    local->access = 0;
    local->datatype = DLOCAL;
    local->type = type;
    local->name = GetHashNameNodeExport(name);
    vi = CodeGen_GetNewVarInfo();
    local->u.var.info = vi;
    vi->func = data_00625078;
    vi->usage = 1;
    vi->used = 1;
    vi->flags |= VarInfoFlag_InReg;
    vi->rclass = rclass;
    vi->reg = reg;
    list = lalloc(sizeof(ObjectList));
    memclrw(list, sizeof(ObjectList));
    list->next = locals;
    list->object = local;
    locals = list;
}

// The PSP Project panel's option (0 turns on .mwcats).
int func_00438FC0(void) {
    return copts.mips_option04;
}

// --- argument homes --------------------------------------------------------

#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define IS_AGGREGATE(t) \
    ((t) && \
     ((t)->type == TYPEARRAY || \
      (t)->type == TYPESTRUCT || \
      (t)->type == TYPECLASS || \
      ((t)->type == TYPEMEMBERPOINTER && (t)->size == 12)))

// An argument passed on the stack lives in the caller's argument area.
void func_00437C20(Object* obj, SInt16 reg, Boolean isint) {
    SInt32 align;
    SInt32 offset;

    if (reg) {
        return;
    }
    align = CMach_GetTypeAlign(obj->type);
    data_005E8B9C = ALIGN_UP(data_005E8B9C, align);
    offset = data_005E8B9C;
    obj->datatype = DLOCAL;
    Registers_GetVarInfo(obj)->flags |= 1;
    OBJECT_FRAMEOFFSET(obj) = offset;
    data_005E8B9C += ALIGN_UP(obj->type->size, data_00624788);
}

// An argument passed in a register: the optimizer keeps it there unless its
// address is needed; otherwise it gets a frame slot.
void func_00437C90(Object* obj, SInt16 reg, Boolean isint) {
    VarInfo* vi;
    ObjectList_* list;
    SInt32 align;
    SInt32 offset;

    if (!reg) {
        return;
    }
    if (!data_00625EB6 && data_00625350) {
        vi = Registers_GetVarInfo(obj);
        if (is_volatile_object(obj) || IS_AGGREGATE(obj->type) || vi->noregister) {
            list = lalloc(sizeof(ObjectList_));
            memclrw(list, sizeof(ObjectList_));
            list->object = obj;
            list->next = data_005E8B90;
            data_005E8B90 = list;
        }
    } else {
        align = CMach_GetTypeAlign(obj->type);
        data_00624FCC = ALIGN_UP(data_00624FCC, align);
        offset = data_00624FCC;
        obj->datatype = DLOCAL;
        Registers_GetVarInfo(obj)->flags |= 1;
        OBJECT_FRAMEOFFSET(obj) = offset;
        data_00624FCC += ALIGN_UP(obj->type->size, 16);
    }
}

// --- move_assigned_argument ---------------------------------------------

#define IS_TYPE_8BYTE(t) \
    ((((t)->type == TYPEINT || (t)->type == TYPEENUM) && (t)->size == 8) || \
     ((t)->type == TYPEFLOAT && IS_DOUBLE(t)))

extern SInt16 data_00625E40; // load word (soft-float halves)
extern SInt16 data_00625E4C; // store word (soft-float halves)
extern SInt32 data_00625010; // word size - 1
extern SInt32 data_0062504C; // log2 of the word size
extern void func_00480A00(
    SInt16 op, SInt16 reg, SInt16 base, Object* obj, SInt32 offset
); // load into an FPR
extern void func_004807C0(
    SInt16 op, SInt16 reg, SInt16 base, Object* obj, SInt32 offset
); // store an FPR
extern SInt16 func_0047FFD0(Type* type); // store opcode for the type
extern SInt16 func_0047FD20(Type* type, SInt32* unk); // load opcode for the type
extern struct Alias* make_distinct_virtual_alias(Object* obj); // 0x481DA0

// Move an argument from where it arrives (register reg, or the stack when
// reg is 0) to where the function keeps it (its register, or its slot).
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 5 // b145 asserts
void func_00437330(Object* obj, SInt16 reg, Boolean isint) {
    SInt32 home;
    SInt32 unk;
    VarInfo* vi;
    SInt16 base;
    Type* type;
    Type* t;
    SInt32 n;
    SInt32 offset;
    SInt16 op;

    vi = Registers_GetVarInfo(obj);
    type = obj->type;
    base = func_004A5360(obj);
    if (!vi) {
        CError_FATAL(1554);
    }
    if (!vi->used) {
        return;
    }
    if ((t = func_00464B40(type))) {
        type = t;
    }

    if (reg) {
        if ((home = vi->reg)) {
            if (vi->rclass == 1) {
                emitpcode(IS_SINGLE(type) ? MACRO_OP(0x99) : MACRO_OP(0x9A), (SInt16)home, reg);
            } else if (vi->rclass == 1) {
                CError_FATAL(1588);
#if !MWCCPS2 // b210 (0x4373F0): a long long/double is one 64-bit register, one move
            } else if (IS_TYPE_8BYTE(type)) {
                if (copts.littleendian) {
                    emitpcode(0xAA, (SInt16)home, reg);
                    emitpcode(0xAA, vi->regHi, reg + 1);
                } else {
                    emitpcode(0xAA, vi->regHi, reg);
                    emitpcode(0xAA, vi->reg, reg + 1);
                }
#endif
            } else {
                emitpcode(
                    ((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 16)
                        ? data_00625E74
                        : data_00625E48,
                    (SInt16)home,
                    reg
                );
            }
            if (type->type == TYPEPOINTER) {
                data_006246B4->last->alias = make_distinct_virtual_alias(obj);
            }
        } else {
            data_00625EA7 = 1;
            home = type->type;
            if (home == TYPEINT ||
                home == TYPEENUM ||
                home == TYPEPOINTER ||
                (home == TYPEMEMBERPOINTER && type->size == 4)) {
#if MWCCPS2 // b210: one store of the 64-bit register
                if (0) {
#else
                if (IS_TYPE_8BYTE(type)) {
#endif
                    if (!vi->noregister) {
                        vi->reg = 0;
                        vi->noregister = 1;
                    }
                    func_004808E0(data_00625E4C, reg, base, obj, OBJECT_FRAMEOFFSET(obj));
                    func_004808E0(
                        data_00625E4C, reg + 1, base, obj, OBJECT_FRAMEOFFSET(obj) + data_00624788
                    );
                } else if (home == TYPEFLOAT) {
                    func_004808E0(
                        IS_SINGLE(type) ? MACRO_OP(0x77) : MACRO_OP(0x6F),
                        reg,
                        base,
                        obj,
                        OBJECT_FRAMEOFFSET(obj)
                    );
                } else {
                    func_004808E0(func_0047FFD0(type), reg, base, obj, OBJECT_FRAMEOFFSET(obj));
                }
            } else if (home == TYPEFLOAT) {
                if (home == TYPEFLOAT && IS_SOFTFLOAT(type)) {
                    if (IS_SINGLE(type)) {
                        func_004808E0(MACRO_OP(0x77), reg, base, obj, OBJECT_FRAMEOFFSET(obj));
#if MWCCPS2 // b210: a soft double is one 64-bit register
                    } else if (IS_DOUBLE(type)) {
                        func_004808E0(data_00625E4C, reg, base, obj, OBJECT_FRAMEOFFSET(obj));
#endif
                    } else {
                        if (!vi->noregister) {
                            vi->reg = 0;
                            vi->noregister = 1;
                        }
                        func_004808E0(data_00625E4C, reg, base, obj, OBJECT_FRAMEOFFSET(obj));
                        func_004808E0(
                            data_00625E4C,
                            reg + 1,
                            base,
                            obj,
                            OBJECT_FRAMEOFFSET(obj) + data_00624788
                        );
                    }
                } else {
                    if (IS_SINGLE(type)) {
                        op = MACRO_OP(0x79);
                    } else if (copts.mips_isa >= 2 && (copts.fp_library & 2)) {
                        op = MACRO_OP(0x70);
                    } else {
                        op = MACRO_OP(0x6C);
                    }
                    func_004807C0(op, reg, base, obj, OBJECT_FRAMEOFFSET(obj));
                }
            } else {
                // an aggregate in argument registers: store word by word
                n = (obj->type->size >> data_0062504C) + ((data_00625010 & obj->type->size) != 0);
#if MWCCPS2 // b210: doublewords (sd) for an aggregate wider than a word
                if (type->size > 4) {
                    home = MACRO_OP(0x6F);
                } else
#endif
                    if (type->size > 2) {
                    home = MACRO_OP(0x77);
                } else if (type->size > 1) {
                    home = MACRO_OP(0x76);
                } else {
                    home = MACRO_OP(0x75);
                }
                offset = OBJECT_FRAMEOFFSET(obj);
#if MWCCPS2 // b210: $a0-$t3 with 64-bit longs (copts+0x15), else $a0-$a3
                while (n > 0 && reg <= (copts.mips_long64 ? 11 : 7)) {
#else
                while (n > 0 && reg <= 11) {
#endif
                    func_004808E0(home, reg, base, obj, offset);
                    reg++;
                    offset += data_00624788;
                    n--;
                }
            }
        }
    } else {
        if (!(home = vi->reg)) {
            return;
        }
        data_00625EA7 = 1;
        Registers_GetVarInfo(obj)->reg = 0;
        if (type->type == TYPEPOINTER || (type->type == TYPEMEMBERPOINTER && type->size == 4)) {
            func_00480B40(0x9D, home, base, obj, OBJECT_FRAMEOFFSET(obj));
            data_006246B4->last->flags |= 0x8000;
#if MWCCPS2 // b210: one load of the 64-bit register (below)
        } else if (0) {
#else
        } else if (((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8) ||
                   (type->type == TYPEFLOAT &&
                    IS_DOUBLE(type) &&
                    type->type == TYPEFLOAT &&
                    IS_SOFTFLOAT(type))) {
#endif
            if (copts.littleendian) {
                func_00480B40(data_00625E40, home, base, obj, OBJECT_FRAMEOFFSET(obj));
                data_006246B4->last->flags |= 0x8000;
                func_00480B40(
                    data_00625E40, vi->regHi, base, obj, OBJECT_FRAMEOFFSET(obj) + data_00624788
                );
                data_006246B4->last->flags |= 0x8000;
            } else {
                func_00480B40(data_00625E40, vi->regHi, base, obj, OBJECT_FRAMEOFFSET(obj));
                data_006246B4->last->flags |= 0x8000;
                func_00480B40(
                    data_00625E40, home, base, obj, OBJECT_FRAMEOFFSET(obj) + data_00624788
                );
                data_006246B4->last->flags |= 0x8000;
            }
        } else if (type->type == TYPEINT ||
                   type->type == TYPEENUM ||
                   type->type == TYPEPOINTER ||
                   (type->type == TYPEMEMBERPOINTER && type->size == 4)) {
            func_00480B40(func_0047FD20(type, &unk), home, base, obj, OBJECT_FRAMEOFFSET(obj));
            data_006246B4->last->flags |= 0x8000;
        } else if (type->type == TYPEFLOAT) {
            if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
                func_00480B40(data_00625E40, home, base, obj, OBJECT_FRAMEOFFSET(obj));
            } else {
                if (IS_SINGLE(type)) {
                    op = MACRO_OP(0x56);
                } else if (copts.mips_isa >= 2 && (copts.fp_library & 2)) {
                    op = MACRO_OP(0x4B);
                } else {
                    op = MACRO_OP(0x48);
                }
                func_00480A00(op, home, base, obj, OBJECT_FRAMEOFFSET(obj));
            }
            data_006246B4->last->flags |= 0x8000;
        } else {
            CError_FATAL(1715);
        }
        Registers_GetVarInfo(obj)->reg = home;
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// --- virtual registers for the optimizer ----------------------------------

extern SInt32 data_00625A98[7]; // next virtual register of each class

#define IS_INTLIKE(t) \
    ((t)->type == TYPEINT || \
     (t)->type == TYPEENUM || \
     (t)->type == TYPEPOINTER || \
     ((t)->type == TYPEMEMBERPOINTER && (t)->size == 4))

// An argument gets a virtual register unless its address is taken; one
// passed on the stack only when it is used often enough.
void func_004383C0(Object* obj, SInt16 reg, Boolean isint) {
    VarInfo* vi;
    Type* type;

    vi = obj->u.var.info;
    type = obj->type;
    if (!vi->used) {
        return;
    }
    if (vi->noregister || is_volatile_object(obj)) {
        data_00625EA7 = 1;
        return;
    }
    if (!IS_INTLIKE(type) && type->type != TYPEFLOAT && !func_00464B40(type)) {
        data_00625EA7 = 1;
        return;
    }
    if (!reg && vi->usage < 4) {
        vi->noregister = 1;
        data_00625EA7 = 1;
        return;
    }
    if (IS_INTLIKE(type)) {
        vi->rclass = 0;
        vi->reg = data_00625A98[0]++;
#if !MWCCPS2 // b210 (0x4381D0): one 64-bit register
        if ((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8) {
            vi->regHi = data_00625A98[0]++;
        }
#endif
    } else if (type->type == TYPEFLOAT) {
        if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
            vi->rclass = 0;
            vi->reg = data_00625A98[0]++;
#if !MWCCPS2
            if (type->type == TYPEFLOAT && IS_DOUBLE(type)) {
                vi->regHi = data_00625A98[0]++;
            }
#endif
        } else {
            vi->rclass = 1;
            vi->reg = data_00625A98[1]++;
        }
    } else if (func_00464B40(type)) {
        vi->rclass = 1;
        vi->reg = data_00625A98[1]++;
    }
}

// With the optimizer, every eligible argument and local gets a virtual register.
void func_00438160(void) {
    ObjectList* list;
    Object* obj;
    VarInfo* vi;
    Type* type;

    if (copts.mips_long64) {
        func_004388C0(func_004383C0, 0);
    } else {
        func_00438A90(func_004383C0, 0);
    }
    for (list = locals; list; list = list->next) {
        obj = list->object;
        vi = Registers_GetVarInfo(obj);
        type = obj->type;
        if (!vi->used) {
            continue;
        }
        if (vi->noregister || is_volatile_object(obj)) {
            data_00625EA7 = 1;
            continue;
        }
#if MWCCPS2
        // b210 (0x438000): a 128-bit integer local flagged at VarInfo +0x2D
        // goes to a VU0 register; no register pairs
        if (vi->unk24[1] && (type->type == TYPEINT || type->type == TYPEENUM) && type->size == 16) {
            vi->rclass = 5;
            vi->reg = data_00625A98[5]++;
        } else if (!IS_INTLIKE(type) && type->type != TYPEFLOAT) {
            data_00625EA7 = 1;
        } else if (IS_INTLIKE(type)) {
            vi->rclass = 0;
            vi->reg = data_00625A98[0]++;
        } else if (type->type == TYPEFLOAT) {
            if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
                vi->rclass = 0;
                vi->reg = data_00625A98[0]++;
            } else {
                vi->rclass = 1;
                vi->reg = data_00625A98[1]++;
            }
        }
#else
        if (type->type == TYPEFLOAT && TYPE_INTEGRAL(type)->integral == IT_17) {
            vi->rclass = 5;
            vi->reg = data_00625A98[5]++;
        } else if (!IS_INTLIKE(type) && type->type != TYPEFLOAT) {
            data_00625EA7 = 1;
        } else if (IS_INTLIKE(type)) {
            vi->rclass = 0;
            if ((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8) {
                vi->reg = data_00625A98[0]++;
                vi->regHi = data_00625A98[0]++;
            } else {
                vi->reg = data_00625A98[0]++;
            }
        } else if (type->type == TYPEFLOAT) {
            if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
                vi->rclass = 0;
                if (type->type == TYPEFLOAT && IS_DOUBLE(type)) {
                    vi->reg = data_00625A98[0]++;
                    vi->regHi = data_00625A98[0]++;
                } else {
                    vi->reg = data_00625A98[0]++;
                }
            } else {
                vi->rclass = 1;
                vi->reg = data_00625A98[1]++;
            }
        }
#endif
    }
}

// process_arguments with 64-bit registers: every integer argument takes one
// register, and varargs registers are stored as doublewords.
void func_004388C0(void (*fn)(Object*, SInt16, Boolean), int varargs) {
    ObjectList* list;
    Object* obj;
    Type* type;
    MPCode* pc;
    SInt16 gpr;
    SInt16 fpr;

    gpr = 4;
    fpr = 12;
    for (list = arguments; list; list = list->next) {
        obj = list->object;
        type = obj->type;
        if ((type->type == TYPEFLOAT && IS_HARDFLOAT(type)) || func_00464B40(type)) {
            fn(obj, fpr > LAST_FPR ? 0 : fpr, 0);
            fpr++;
        } else {
            fn(obj, gpr > LAST_GPR ? 0 : gpr, 1);
            gpr++;
        }
    }

    data_00624DCC = gpr;
    data_00624DC8 = fpr;
    if (varargs) {
        data_00625EA7 = 1;
        for (; gpr <= LAST_GPR; gpr++) {
            pc = func_00480420(MACRO_OP(0x6F), 3);
            pc->args[0].kind = 0;
            pc->args[0].reg = gpr;
            pc->args[0].rclass = 0;
            pc->args[0].flags = 1;
            pc->args[1].kind = 4;
            pc->args[1].value = data_00624FCC;
            MOP_SETOBJ(pc->args[1], galloc(sizeof(Object)));
            memset((void*)MOP_OBJ(pc->args[1]), 0, sizeof(Object));
            ((Object*)MOP_OBJ(pc->args[1]))->sclass = 0x102;
            ((Object*)MOP_OBJ(pc->args[1]))->datatype = DLOCAL;
            pc->args[2] = func_004C3E70(29, 0, 1);
            appendpcode(data_006246B4, pc);
            data_00624FCC += data_00624788;
        }
    }
}

extern VOperand func_0047F660(SInt32 value); // a constant in a new register

// __builtin_va_info-style query on a constant operand: 2 gives the integer
// argument registers used, 3 the floating-point ones.
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 5 // b145 asserts
VOperand func_00438680(VOperand* which) {
    if (which->kind != 3) {
        CError_FATAL(891);
    }
    if (which->imm.lo == 2) {
        return func_0047F660(data_00624DCC - 4);
    } else if (which->imm.lo == 3) {
        return func_0047F660(data_00624DC8 - 12);
    } else {
        VOperand op = {0};
        CError_FATAL(902);
        return op;
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

extern SInt32 func_00464B20(Object* obj); // size in whole words
extern SInt32 data_00624FCC; // locals start

// 3.0.1 (0x4385E0): the stack slot past an argument (the intrinsics' va_arg
// helper): the operand moved past the object, never into the locals' area
VOperand func_004385E0(VOperand* arg) {
    VOperand next;

    next = *arg;
    next.offset += func_00464B20((Object*)arg->object);
    if (next.offset < data_00624FCC) {
        next.offset = data_00624FCC;
    }
    return next;
}

extern Boolean func_00458530(Object* obj);

// 0x432440
Boolean CodeGen_IsPublicRuntimeObject(Object* obj) {
    return func_00458530(obj);
}

#if !MWCCPS2 // b210: CodeGen_ParsePragma and its own handlers above
#include "compiler/CPrepTokenizer.h"
#include "compiler/tokens.h"

extern SInt32 data_00624FD4; // #pragma pack: the maximum alignment
extern SInt32 func_00464210(char* name);
extern SInt32 ObjGen_DefineSection(
    char* name, char* elfname, char* bssname, char* addressing, char* access
);

// skip to the end of the pragma's line
#define PRAGMA_SKIP_LINE() \
    do { \
        tk = plex(); \
    } while (tk != TK_EOL && tk != TK_EOF)

// 0x433580: #pragma vu0_register_number n | reset
static Boolean CodeGen_PragmaVU0Register(void) {
    short t;

    if (!notendofline()) {
        if (copts.warn_illpragma) {
            CPrep_Warning(ERR_ILLEGAL_PRAGMA);
        }
        skipendofline();
        return FALSE;
    }
    t = plex();
    if (t == TK_INTCONST) {
        if ((SInt32)tkintconst.lo < 0 || (SInt32)tkintconst.lo > 32) {
            CPrep_Error(55);
            skipendofline();
            return FALSE;
        }
        copts.mips_option43 = tkintconst.lo;
        copts.mips_option41 = 1;
        func_004621F0();
        return TRUE;
    }
    if (t == TK_IDENTIFIER && !strcmp(tkidentifier->name, "reset")) {
        copts.mips_option43 = 32;
        copts.mips_option41 = 1;
        return TRUE;
    }
    CPrep_Error(6);
    skipendofline();
    return FALSE;
}

// 0x433650: #pragma align [(n)], n a power of two up to 0x2000
static Boolean CodeGen_PragmaAlign(void) {
    SInt32 a;

    tk = plex();
    if (tk == TK_EOL) {
        data_006247A8 = 0;
        return TRUE;
    }
    if (tk == '(') {
        tk = plex();
        if (tk == TK_INTCONST) {
            data_006247A8 = tkintconst.lo;
            for (a = 0x2000; a; a >>= 1) {
                if (tkintconst.lo == a) {
                    tk = plex();
                    if (tk == ')') {
                        return TRUE;
                    }
                    break;
                }
            }
        }
    }
    data_006247A8 = 0;
    return FALSE;
}

// 0x4336E0: #pragma pack [(n)], n 0, 1, 2, 4, 8 or 16
static Boolean CodeGen_PragmaPack(void) {
    tk = plex();
    if (tk == TK_EOL) {
        data_00624FD4 = 0;
        return TRUE;
    }
    if (tk == '(') {
        tk = plex();
        if (tk == TK_INTCONST) {
            data_00624FD4 = tkintconst.lo;
            switch (data_00624FD4) {
                case 0:
                case 1:
                case 2:
                case 4:
                case 8:
                case 16:
                    tk = plex();
                    if (tk == ')') {
                        return TRUE;
                    }
                    break;
            }
        }
    }
    data_00624FD4 = 0;
    return FALSE;
}

// 0x433780: #pragma define_section name "elfname" ["bssname"] [addressing [access]]
static Boolean CodeGen_PragmaDefineSection(void) {
    char* name;
    char* elfname;
    char* bssname;
    char* addressing;
    char* access;

    bssname = NULL;
    addressing = NULL;
    access = NULL;
    tk = plex();
    if (tk != TK_IDENTIFIER) {
        return FALSE;
    }
    name = tkidentifier->name;
    tk = plex();
    if (tk != TK_STRING) {
        return FALSE;
    }
    elfname = tkstring;
    tk = plex();
    if (tk == TK_STRING) {
        bssname = tkstring;
        tk = plex();
    }
    if (tk == TK_IDENTIFIER) {
        addressing = tkidentifier->name;
        tk = plex();
    }
    if (tk == TK_IDENTIFIER) {
        access = tkidentifier->name;
        tk = plex();
    }
    if (!ObjGen_DefineSection(name, elfname, bssname, addressing, access)) {
        return FALSE;
    }
    if (tk == TK_EOL || tk == TK_EOF) {
        return TRUE;
    }
    PRAGMA_SKIP_LINE();
    return FALSE;
}

// #pragma processor names and the processor they select
static const struct {
    const char* name;
    SInt16 processor;
} codegen_processors[] = {
    {"VR4100", 0x11},
    {"VR4111", 0x12},
    {"VR4121", 0x13},
    {"VR4200", 0x14},
    {"VR4300", 0x15},
    {"VR5000", 0x16},
    {"VR5400", 0x17},
    {"RM7000", 0x1B},
    {"R5900", 0x1D},
    {"PSP", 0x1E},
    {"IDT3081", 8},
    {"IDT4640", 0xC},
    {"IDT4650", 0xD},
    {"IDT4700", 0xB},
    {"IDT32364", 0xE},
    {"SONY4500", 3},
    {"TR4101", 0x1C},
};

// 0x4338E0: #pragma processor name
static Boolean CodeGen_PragmaProcessor(void) {
    unsigned int i;

    tk = plex();
    if (tk != TK_IDENTIFIER) {
        return FALSE;
    }
    for (i = 0; i < sizeof(codegen_processors) / sizeof(codegen_processors[0]); i++) {
        if (!strcmp(tkidentifier->name, codegen_processors[i].name)) {
            copts.mips_flags02 = codegen_processors[i].processor;
            func_00435300();
            PRAGMA_SKIP_LINE();
            return TRUE;
        }
    }
    return FALSE;
}

// 0x433BE0: #pragma maxtempforblkmv [(n)], n 1..6 (4 by default)
static Boolean CodeGen_PragmaMaxTempForBlkMv(void) {
    tk = plex();
    if (tk == TK_EOL) {
        copts.mips_option33 = 4;
        return TRUE;
    }
    if (tk == '(') {
        tk = plex();
        if (tk == TK_INTCONST && (SInt32)tkintconst.lo >= 1 && (SInt32)tkintconst.lo <= 6) {
            copts.mips_option33 = tkintconst.lo;
            tk = plex();
            if (tk == ')') {
                return TRUE;
            }
        }
    }
    copts.mips_option33 = 4;
    return FALSE;
}

// 0x433C70: #pragma section name begin | end
static Boolean CodeGen_PragmaSection(void) {
    SInt32 sec;
    SInt32* cur;

    cur = (SInt32*)&data_00625094; // a word in the original
    tk = plex();
    if (tk != TK_IDENTIFIER) {
        return FALSE;
    }
    sec = func_00464210(tkidentifier->name);
    tk = plex();
    if (tk != TK_IDENTIFIER) {
        return FALSE;
    }
    if (!strcmp(tkidentifier->name, "begin")) {
        if (!sec || *cur) {
            return FALSE;
        }
        *cur = sec;
    } else if (!strcmp(tkidentifier->name, "end")) {
        if (!sec || *cur != sec) {
            return FALSE;
        }
        *cur = 0;
    } else {
        return FALSE;
    }
    PRAGMA_SKIP_LINE();
    return TRUE;
}

// 0x433330: the back end's pragmas; others are warned about and skipped
void CodeGen_ParsePragma(HashNameNode* name) {
    short i;
    Boolean ok;

    if (!strcmp(name->name, "pack")) {
        ok = CodeGen_PragmaPack();
    } else if (!strcmp(name->name, "cgswitch")) {
        // #pragma cgswitch [(n)]: n < 16 sets that code generator switch;
        // alone, it clears them all
        tk = plex();
        if (tk == TK_EOL) {
            for (i = 0; i < 16; i++) {
                data_0060B920[i] = 0;
            }
            ok = TRUE;
        } else {
            if (tk == '(') {
                tk = plex();
                if (tk == TK_INTCONST && tkintconst.lo < 16) {
                    data_0060B920[tkintconst.lo] = 1;
                }
            }
            ok = FALSE;
        }
    } else if (!strcmp(name->name, "align")) {
        ok = CodeGen_PragmaAlign();
    } else if (!strcmp(name->name, "processor")) {
        ok = CodeGen_PragmaProcessor();
    } else if (!strcmp(name->name, "maxtempforblkmv")) {
        ok = CodeGen_PragmaMaxTempForBlkMv();
    } else if (!strcmp(name->name, "vu0_register_number")) {
        ok = CodeGen_PragmaVU0Register();
    } else if (!strcmp(name->name, "section")) {
        ok = CodeGen_PragmaSection();
    } else if (!strcmp(name->name, "define_section")) {
        ok = CodeGen_PragmaDefineSection();
    } else {
        ok = FALSE;
    }
    if (ok) {
        return;
    }
    if (copts.warn_illpragma) {
        CPrep_Warning(ERR_ILLEGAL_PRAGMA);
    }
    do {
        i = plex();
    } while (i != TK_EOL && i != TK_EOF);
}
#endif

// 0x4A16C0: the IR listing (empty in the release compiler)
void func_004A16C0(Statement* stmts, char* name, char* title) {}

// 0x4AFFF0: the pcode listing (empty in the release compiler)
void BE_DumpPCode(char* name, char* title) {}
