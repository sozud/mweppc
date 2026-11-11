#include "compiler/common.h"
#include "psp/BE.h"
#include "compiler/CError.h"
#include "compiler/CFunc.h"
#include "compiler/CompilerTools.h"
#include "compiler/CParser.h"
#include "compiler/enode.h"
#include "compiler/objects.h"
#include "compiler/types.h"
#include "compiler/CInt64.h"
#include "compiler/CMachine.h"
#include "compiler/Exceptions.h"

#line 1 "FunctionCalls.c"
// mwccpsp 3.0.1 FunctionCalls.c (named by its assertions): passing
// arguments and calling functions.

// one argument of a call
typedef struct ArgInfo {
    struct ArgInfo* next;
    ENode* expr;
    VOperand opnd; // +0x08
    SInt32 offset; // +0x2C: stack offset, or -1
    SInt16 gpr; // +0x30: first general register, or -1
    SInt16 fpr; // +0x32: float register, or -1
    SInt16 unk34;
    SInt16 flags; // +0x36: ArgInfoFlag_*
    SInt16 cost; // +0x38: the expression's cost
} ArgInfo;

enum {
    ArgInfoFlag_GPR = 1,
    ArgInfoFlag_FPR = 2,
    ArgInfoFlag_Stack = 4,
    ArgInfoFlag_Promote = 8, // an unprototyped small integer
    ArgInfoFlag_FloatStack = 0x10
};

#define IS_SINGLE(t) \
    (TYPE_INTEGRAL(t)->integral == IT_FLOAT || ((t)->type == TYPEFLOAT && (t)->size == 4))
#define IS_DOUBLE(t) \
    (TYPE_INTEGRAL(t)->integral == IT_DOUBLE || ((t)->type == TYPEFLOAT && (t)->size == 8))
#define IS_SOFTFLOAT(t) \
    ((IS_SINGLE(t) && !(copts.fp_library & 1)) || (IS_DOUBLE(t) && !(copts.fp_library & 2)))

extern SInt32 data_00624788; // register size in bytes
extern SInt32 data_0062421C; // stack alignment
extern SInt32 data_006242B4; // outgoing argument area
extern UInt8 data_00625EA7; // some argument is passed on the stack
extern Type* func_00464B40(Type* type); // the struct's single floating-point member

static inline ArgInfo* new_arginfo(ENode* expr) {
    ArgInfo* info = lalloc(sizeof(ArgInfo));

    info->next = NULL;
    info->expr = expr;
    info->offset = -1;
    info->gpr = -1;
    info->fpr = -1;
    info->unk34 = 0;
    info->flags = 0;
    info->cost = expr->cost;
    return info;
}

// EABI: up to eight general argument registers ($a0-$t3) and eight float
// registers ($f12-$f19); a pair is even-aligned; the rest goes on the stack.
// skip is an argument that is not passed (the caller handles it).
ArgInfo* func_004A8C00(ENode* skip, ENodeList* args, FuncArg* argtypes) {
    ArgInfo* list = NULL;
    int gpr = 4;
    int fpr = 12;
    SInt32 offset = 0;
    ArgInfo* tail;
    ENode* expr;
    Type* type;

    while (args) {
        expr = args->node;
        if (expr == skip) {
            args = args->next;
            argtypes = argtypes->next;
            continue;
        }
        type = expr->rtype;
        if (list) {
            tail->next = new_arginfo(expr);
            tail = tail->next;
        } else {
            tail = new_arginfo(expr);
            list = tail;
        }

        if (type->type == TYPEFLOAT || func_00464B40(type)) {
            if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
                if (!(type->type == TYPEFLOAT && IS_DOUBLE(type))) {
                    if (gpr > 11) {
                        tail->flags |= ArgInfoFlag_Stack;
                        data_00625EA7 = 1;
                        tail->offset = offset;
                        offset += data_00624788;
                    } else {
                        tail->flags |= ArgInfoFlag_GPR;
                        tail->gpr = gpr;
                    }
                    gpr++;
                } else {
                    if (gpr > 11) {
                        tail->flags |= ArgInfoFlag_Stack;
                        offset = (offset + 7) & ~7;
                        data_00625EA7 = 1;
                        tail->offset = offset;
                        offset += data_00624788 * 2;
                    } else {
                        if (gpr & 1) {
                            gpr++;
                        }
                        tail->flags |= ArgInfoFlag_GPR;
                        tail->gpr = gpr;
                        if (gpr + 1 > 11) {
                            tail->flags |= ArgInfoFlag_Stack;
                            offset = (offset + 7) & ~7;
                            data_00625EA7 = 1;
                            tail->offset = offset;
                            offset += data_00624788 * 2;
                        }
                        gpr += 2;
                    }
                }
            } else if (!argtypes || argtypes == &oldstyle || argtypes == &elipsis) {
                if (fpr > 19) {
                    tail->flags |= ArgInfoFlag_Stack | ArgInfoFlag_FloatStack;
                    data_00625EA7 = 1;
                    tail->offset = offset;
                    offset += data_00624788;
                } else {
                    tail->flags |= ArgInfoFlag_FPR;
                    tail->fpr = fpr;
                    fpr += 2;
                }
            } else {
                if (fpr > 19) {
                    tail->flags |= ArgInfoFlag_Stack;
                    data_00625EA7 = 1;
                    tail->offset = offset;
                    offset += data_00624788;
                } else {
                    tail->flags |= ArgInfoFlag_FPR;
                    tail->fpr = fpr;
                    fpr++;
                }
            }
        } else if (type->type == TYPEINT ||
                   type->type == TYPEENUM ||
                   type->type == TYPEPOINTER ||
                   (type->type == TYPEMEMBERPOINTER && type->size == 4)) {
            if (!argtypes || argtypes == &elipsis || argtypes == &oldstyle) {
                if (type->size < 4) {
                    tail->flags |= ArgInfoFlag_Promote;
                }
            }
            if ((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8) {
                offset = (offset + 7) & ~7;
            }
            if ((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8) {
                if (gpr & 1) {
                    gpr++;
                }
                if (gpr > 11) {
                    tail->flags |= ArgInfoFlag_Stack;
                    data_00625EA7 = 1;
                    tail->offset = offset;
                    offset += data_00624788 * 2;
                } else {
                    tail->flags |= ArgInfoFlag_GPR;
                    tail->gpr = gpr;
                    if (gpr + 1 > 11) {
                        tail->flags |= ArgInfoFlag_Stack;
                        data_00625EA7 = 1;
                    }
                    gpr += 2;
                }
            } else {
                if (gpr > 11) {
                    tail->flags |= ArgInfoFlag_Stack;
                    data_00625EA7 = 1;
                    tail->offset = offset;
                    offset += data_00624788;
                } else {
                    tail->flags |= ArgInfoFlag_GPR;
                    tail->gpr = gpr;
                    gpr++;
                }
            }
        } else {
            if (type->size > data_00624788) {
                CError_FATAL(594);
            }
            if (gpr > 11) {
                tail->flags |= ArgInfoFlag_Stack;
                data_00625EA7 = 1;
                tail->offset = offset;
                offset += data_00624788;
            } else {
                tail->flags |= ArgInfoFlag_GPR;
                tail->gpr = gpr;
                gpr++;
            }
        }

        args = args->next;
        if (argtypes && argtypes != &elipsis && argtypes != &oldstyle) {
            argtypes = argtypes->next;
        }
    }

    offset = offset > data_006242B4 ? offset : data_006242B4;
    data_006242B4 = (offset + (data_0062421C - 1)) & ~(data_0062421C - 1);

    return list;
}

typedef struct UsedRegs3 {
    SInt32 codewords;
    UInt32 intregs;
    UInt32 fpregs;
} UsedRegs3;

extern SInt16 data_00625E48; // move
extern SInt32 data_006241E0[7]; // caller-saved registers of each class: count
extern SInt32 data_0060E038[7][128]; // ... and their numbers
extern UsedRegs3* data_006241D8; // registers the function uses
extern Statement* data_00625070; // current_statement
extern struct Alias* worst_case_func;
extern Object* rt_MIPS16_prtglue16;
extern Object* rt_MIPS16_prtglue32;
extern MPCode* func_00480420(int op, SInt16 nargs);
extern VOperand func_004949B0(Object* obj); // the object as an operand
extern void func_004A72B0(VOperand* op, ENode* expr);
extern void func_004A64C0(VOperand* op, Type* type, short copy);
extern MOperand func_004C3E70(UInt16 reg, UInt8 rclass, UInt16 flags);
extern Object* func_00438DC0(Object* func); // what a far call goes through
extern SInt32 func_004B8990(void);
extern SInt32 func_00461F50(void);
extern void func_00461D60(Object* func, UInt32* masks); // registers the callee clobbers, per class
extern int func_0049E1C0(ExceptionAction* actions); // countexceptionactionregisters
extern void func_0049DE40(ExceptionAction* actions, MOperand* args); // noteexceptionactionregisters
extern void func_0049DD90(MPCode* pc, ExceptionAction* actions); // recordexceptionactions
extern void func_0046FA50(void); // makepcblock

// branch_subroutine: call func (or through register reg, in $t9 when both
// are given), listing the registers the call uses (masks: arguments, per
// class) and clobbers as operands.
void func_004A8310(Object* func, int reg, UInt32* masks) {
    VOperand v;
    UInt32 clobbered[7];
    MOperand* arg;
    MPCode* pc;
    int saved = 0;
    int extra;
    int n;
    int i;
    int r;
    RegClass301 rclass;
    Boolean t9 = FALSE;
    UInt16 op = 0x3C; // jal
    UInt16 op2 = 0x3D; // jalr
    SInt16 move = data_00625E48;

    if (func && reg) {
        saved = reg;
        reg = 0;
    }
    if (copts.far_call) {
        if (func) {
            v = func_004949B0(func_00438DC0(func));
            func_004A72B0(&v, NULL);
            if (v.kind) {
                func_004A64C0(&v, TYPE(&stsignedint), 0);
            }
            reg = v.reg;
            func = NULL;
        }
    }

    extra = 0;
    if (copts.cplusplus && copts.exceptions && data_00625070) {
        extra = func_0049E1C0(data_00625070->dobjstack);
    }
    n = func_00461F50() - func_004B8990();
    if (reg) {
        if (saved) {
            emitpcode(move, 0x19, saved);
            masks[0] |= 0x2000000;
            pc = func_00480420(op2, n + extra + 3);
        } else {
            pc = func_00480420(op2, n + extra + 2);
        }
    } else if (saved) {
        emitpcode(move, 0x19, saved);
        masks[0] |= 0x2000000;
        t9 = FALSE;
        pc = func_00480420(op, extra + n + 3);
    } else {
        pc = func_00480420(op, extra + n + 2);
    }

    data_006241D8->intregs |= 0x80000000;
    if (func && func != rt_MIPS16_prtglue16 && func != rt_MIPS16_prtglue32) {
        func_00461D60(func, clobbered);
    } else {
        for (rclass = 0; rclass < 7; rclass++) {
            clobbered[rclass] = 0xFFFFFFFF;
        }
    }

    arg = &pc->args[2];
    for (rclass = 0; rclass < 7; rclass++) {
        if (rclass == 5) {
            break;
        }
        for (i = 0; i < data_006241E0[rclass]; i++) {
            r = data_0060E038[rclass][i];
            arg->kind = 0;
            arg->rclass = rclass;
            arg->reg = r;
            arg->flags = 0;
            if (clobbered[rclass] & (BIT32(r))) {
                arg->flags |= 2;
            }
            if (masks[rclass] & (BIT32(r))) {
                arg->flags |= 1;
            }

            arg++;
        }
    }
    if (t9) {
        arg->kind = 0;
        arg->rclass = 0;
        arg->reg = 0x19;
        arg->flags = 1;
        arg++;
    }
    if (copts.cplusplus && copts.exceptions && data_00625070) {
        func_0049DE40(data_00625070->dobjstack, arg);
    }

    if (copts.mips_option08 && !reg) {
        pc->op = 0x38;
        pc->args[0].kind = 4;
        pc->args[0].value = 0;
        MOP_SETOBJ(pc->args[0], func);
        pc->unk38 = 10;
        pc->relobj = func;
        pc->alias = worst_case_func;
    } else if (reg) {
        pc->args[0] = func_004C3E70(0x1F, 0, 2);
        pc->args[1] = func_004C3E70(reg, 0, 1);
    } else {
        pc->args[0].kind = 4;
        pc->args[0].value = 0;
        MOP_SETOBJ(pc->args[0], func);
        pc->args[1] = func_004C3E70(0x1F, 0, 2);
        pc->unk38 = 4;
        pc->alias = worst_case_func;
        pc->relobj = func;
    }
    appendpcode(data_006246B4, pc);
    func_0046FA50();
    if (copts.cplusplus && copts.exceptions && data_00625070) {
        func_0049DD90(pc, data_00625070->dobjstack);
    }
}

#define IS_LONGLONG(t) (((t)->type == TYPEINT || (t)->type == TYPEENUM) && (t)->size == 8)

extern SInt16 data_00625E4C; // store word
extern void (*data_005D0140[])(ENode* expr, void* target, VOperand* out);
extern MPCode* func_004808E0(
    SInt16 op, SInt16 reg, SInt16 base, Object* obj, SInt32 offset
); // store a GPR
extern MPCode* func_004807C0(
    SInt16 op, SInt16 reg, SInt16 base, Object* obj, SInt32 offset
); // store an FPR
extern void func_0052DE30(VOperand* dst, VOperand* src, SInt32 size, SInt32 align); // copy a block
extern SInt16 CMach_GetTypeAlign(Type* type); // alignment

// Store an argument that is passed on the stack to its slot at $sp+offset.
void func_004A8840(ArgInfo* info) {
    Type* type = info->expr->rtype;
    ENode* expr = info->expr;
    VOperand dst;

    if (type->type == TYPEINT ||
        type->type == TYPEENUM ||
        type->type == TYPEPOINTER ||
        (type->type == TYPEMEMBERPOINTER && type->size == 4)) {
        if (!info->unk34) {
            data_005D0140[expr->type](expr, NULL, &info->opnd);
        }
        if (info->opnd.kind) {
            func_004A64C0(&info->opnd, type, 0);
        }
        if (IS_LONGLONG(type)) {
            if (copts.littleendian) {
                func_004808E0(data_00625E4C, info->opnd.reg, 29, NULL, info->offset);
                func_004808E0(
                    data_00625E4C, info->opnd.regHi, 29, NULL, info->offset + data_00624788
                );
            } else {
                func_004808E0(data_00625E4C, info->opnd.regHi, 29, NULL, info->offset);
                func_004808E0(
                    data_00625E4C, info->opnd.reg, 29, NULL, info->offset + data_00624788
                );
            }
        } else {
            func_004808E0(data_00625E4C, info->opnd.reg, 29, NULL, info->offset);
        }
    } else if (type->type == TYPEFLOAT) {
        if (!info->unk34) {
            data_005D0140[expr->type](expr, NULL, &info->opnd);
        }
        if (info->opnd.kind) {
            func_004A64C0(&info->opnd, type, 0);
        }
        if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
            if (IS_SINGLE(type) && !(info->flags & ArgInfoFlag_FloatStack)) {
                func_004808E0(0x58F, info->opnd.reg, 29, NULL, info->offset);
            } else if (copts.littleendian) {
                func_004808E0(0x58F, info->opnd.reg, 29, NULL, info->offset);
                func_004808E0(0x58F, info->opnd.regHi, 29, NULL, info->offset + data_00624788);
            } else {
                func_004808E0(0x58F, info->opnd.regHi, 29, NULL, info->offset);
                func_004808E0(0x58F, info->opnd.reg, 29, NULL, info->offset + data_00624788);
            }
        } else if (IS_SINGLE(type) && !(info->flags & ArgInfoFlag_FloatStack)) {
            func_004807C0(0x591, info->opnd.reg, 29, NULL, info->offset);
        } else {
            func_004807C0(
                (copts.mips_isa >= 2 && (copts.fp_library & 2)) ? 0x588 : 0x584,
                info->opnd.reg,
                29,
                NULL,
                info->offset
            );
        }
    } else {
        dst.kind = 2;
        dst.reg = 29;
        dst.object = 0;
        dst.unk18 = 0;
        dst.offset = info->offset;
        if (!info->unk34) {
            if (ENODE_IS(info->expr, EINDIRECT)) {
                data_005D0140[info->expr->data.monadic->type](
                    info->expr->data.monadic, NULL, &info->opnd
                );
            } else {
                data_005D0140[info->expr->type](info->expr, NULL, &info->opnd);
            }
        } else if (ENODE_IS(info->expr, EINDIRECT) && info->opnd.kind == 4) {
            info->opnd.kind = 2;
        }
        func_0052DE30(&dst, &info->opnd, type->size, CMach_GetTypeAlign(type));
    }
}

extern SInt16 data_00625E74; // 16-byte register move
extern SInt32 data_00625010; // word size - 1
extern SInt32 data_0062504C; // log2 of the word size
extern ArgInfo* func_004A9100(
    ENode* skip, ENodeList* args, FuncArg* argtypes
); // the 64-bit long ABI
extern void func_00480B40(SInt16 op, SInt16 reg, SInt16 base, Object* obj, SInt32 offset); // load
extern void func_00480230(SInt16 align, void* unk);
extern VOperand func_0047FAA0(
    UInt16 op, int fpr, Boolean pair
); // the result registers after a call
extern UInt8 func_00436B30(Type* type); // how a value of the type is returned
extern Type* func_00464BE0(Type* type); // the type of the struct's only member
extern void setpcodeflags(UInt32 flags);

// 3.0.1 TypeClass fields (the header has 2.4.7's layout)
#define TCLASS_VTABLE(c) (*(VTable**)&((TypeClass*)(c))->vtable) // +0x1C
#define TCLASS_FLAGS(c) (*(UInt16*)&((TypeClass*)(c))->flags) // +0x24
#define TFUNC_CLASS(t) (*(TypeClass**)&((TypeMemberFunc*)(t))->theclass) // +0x1C

static inline int is_aggregate(Type* type) {
    UInt8 t = type->type;
    UInt8 d;
    int structlike = 0;

    d = t - TYPESTRUCT;
    if (d <= 8 && ((BIT32(d)) & 0x103)) {
        structlike = 1;
    }
    return structlike || (t == TYPEMEMBERPOINTER && type->size == 12);
}

// call_function: pass the arguments, call, and give the result's registers.
void func_004A7400(ENode* expr, VOperand* out) {
    UInt32 masks[7];
    VOperand fop;
    TypeClass* tclass;
    SInt32 vindex;
    ENode* funcref;
    Type* rettype;
    ENode* thisexpr;
    ArgInfo* list;
    ArgInfo* info;
    Type* type;
    SInt16 load;
    int kind;
    int r;
    int off;
    int words;
    int i;
    SInt16 op;
    Type* t;
    int thisreg;
    UInt8 rc;

    funcref = expr->data.funccall.funcref;
    rettype = expr->data.funccall.functype->functype;
    thisexpr = NULL;
    load = 0x56D;
    for (i = 0; i < 7; i++) {
        masks[i] = 0;
    }

    if (expr->data.funccall.functype->flags & 0x80) {
        if (rettype && is_aggregate(rettype)) {
            thisexpr = expr->data.funccall.args->next->node;
        } else {
            thisexpr = expr->data.funccall.args->node;
        }
    }
    if (copts.mips_long64) {
        list =
            func_004A9100(thisexpr, expr->data.funccall.args, expr->data.funccall.functype->args);
    } else {
        list =
            func_004A8C00(thisexpr, expr->data.funccall.args, expr->data.funccall.functype->args);
    }

    // arguments that call functions first
    for (info = list; info; info = info->next) {
        if (info->expr->hascall) {
            data_005D0140[info->expr->type](info->expr, NULL, &info->opnd);
            info->unk34 = 1;
        }
    }
    if (funcref->hascall) {
        data_005D0140[funcref->type](funcref, NULL, &fop);
        if (fop.kind) {
            func_004A64C0(&fop, TYPE(&void_ptr), 0);
        }
    } else if (thisexpr && thisexpr->hascall) {
        data_005D0140[thisexpr->type](thisexpr, NULL, &fop);
        if (fop.kind) {
            func_004A64C0(&fop, TYPE(&void_ptr), 0);
        }
    }

    for (info = list; info; info = info->next) {
        if (info->flags & ArgInfoFlag_Stack) {
            func_004A8840(info);
        }
    }
    for (info = list; info; info = info->next) {
        if ((info->flags & 7) == (ArgInfoFlag_GPR | ArgInfoFlag_Stack)) {
            for (r = info->gpr, off = 0; r <= 11; r++, off += data_00624788) {
                func_00480B40(load, r, 29, NULL, info->offset + off);
            }
        }
    }
    for (info = list; info; info = info->next) {
        if ((UInt32)((info->flags & 7) - 1) <= 1 && !info->unk34) {
            data_005D0140[info->expr->type](info->expr, NULL, &info->opnd);
        }
    }

    for (info = list; info; info = info->next) {
        kind = info->flags & 7;
        type = info->expr->rtype;
        if (kind == (ArgInfoFlag_GPR | ArgInfoFlag_Stack)) {
            for (r = info->gpr; r <= 11; r++) {
                func_00480B40(load, r, 29, NULL, (r - info->gpr) * data_00624788 + info->offset);
                masks[0] |= BIT32(r);
            }
        } else if (kind == ArgInfoFlag_GPR &&
                   (type->type == TYPECLASS ||
                    type->type == TYPESTRUCT ||
                    (type->type == TYPEMEMBERPOINTER && type->size == 12)) &&
                   info->opnd.kind != 0) {
            // a struct in general registers, a word at a time
            if (info->opnd.kind == 2) {
                info->opnd.kind = 4;
            }
            if (info->opnd.kind != 4) {
                CError_FATAL(1235);
            } else {
                words = (type->size >> data_0062504C) + ((type->size & data_00625010) != 0);
                if (type->size > 2) {
                    op = 0x56D;
                } else if (type->size > 1) {
                    op = 0x569;
                } else {
                    op = 0x568;
                }
                for (i = 0; i < words; i++) {
                    func_00480B40(
                        op,
                        info->gpr + i,
                        info->opnd.reg,
                        (Object*)info->opnd.object,
                        data_00624788 * i + info->opnd.offset
                    );
                    func_00480230(CMach_GetTypeAlign(type), (void*)info->opnd.object);
                    masks[0] |= BIT32((info->gpr + i));
                }
            }
        } else if (kind == ArgInfoFlag_GPR) {
            if (info->opnd.kind) {
                func_004A64C0(&info->opnd, type, 0);
            }
            if (info->opnd.kind == 2) {
                info->opnd.kind = 4;
            }
            switch (info->opnd.kind) {
                case 0:
                    t = info->expr->rtype;
                    if (IS_LONGLONG(t) || (t->type == TYPEFLOAT && IS_DOUBLE(t))) {
                        if (copts.littleendian) {
                            emitpcode(data_00625E48, info->gpr, info->opnd.reg);
                            if (info->gpr < 11) {
                                emitpcode(data_00625E48, info->gpr + 1, info->opnd.regHi);
                            }
                        } else {
                            if (info->gpr < 11) {
                                emitpcode(data_00625E48, info->gpr + 1, info->opnd.reg);
                            }
                            emitpcode(data_00625E48, info->gpr, info->opnd.regHi);
                        }
                        masks[0] |= 3 << info->gpr;
                    } else {
                        emitpcode(
                            type->size > 8 ? data_00625E74 : data_00625E48,
                            info->gpr,
                            info->opnd.reg
                        );
                        masks[0] |= BIT32(info->gpr);
                    }
                    break;
                case 4:
                    t = info->expr->rtype;
                    if (t->size > data_00624788) {
                        CError_FATAL(1271);
                    } else if (info->opnd.kind == 4) {
                        if (info->opnd.kind) {
                            func_004A64C0(&info->opnd, t, 0);
                        }
                        emitpcode(data_00625E48, info->gpr, info->opnd.reg);
                        masks[0] |= BIT32(info->gpr);
                    }
                    break;
                case 1:
                    t = info->expr->rtype;
                    if (IS_SINGLE(t)) {
                        emitpcode(0xA5, info->gpr, info->opnd.reg);
                        setpcodeflags(0x4000);
                        masks[0] |= BIT32(info->gpr);
                    } else {
                        if (copts.littleendian) {
                            emitpcode(0xA5, info->gpr, info->opnd.reg);
                            data_006246B4->last->args[1].flags |= 0x8000;
                            setpcodeflags(0x100);
                            emitpcode(0xA5, info->gpr + 1, info->opnd.reg);
                            data_006246B4->last->args[1].flags |= 0xC000;
                        } else {
                            emitpcode(0xA5, info->gpr + 1, info->opnd.reg);
                            data_006246B4->last->args[1].flags |= 0x8000;
                            setpcodeflags(0x100);
                            emitpcode(0xA5, info->gpr, info->opnd.reg);
                            data_006246B4->last->args[1].flags |= 0xC000;
                        }
                        setpcodeflags(0x100);
                        masks[0] |= 3 << info->gpr;
                    }
                    break;
                default:
                    CError_FATAL(1314);
            }
        } else if (kind == ArgInfoFlag_FPR) {
            if (!(t = func_00464B40(type))) {
                t = info->expr->rtype;
            }
            if (info->opnd.kind) {
                func_004A64C0(&info->opnd, t, 0);
            }
            emitpcode(IS_SINGLE(t) ? 0x5B1 : 0x5B2, info->fpr, info->opnd.reg);
            masks[1] |= BIT32(info->fpr);
        }
    }

    if (ENODE_IS(funcref, EOBJREF)) {
        if (!(funcref->flags & 0x80) &&
            CParser_IsVirtualFunction(funcref->data.objref, &tclass, &vindex)) {
            thisreg = (rettype && is_aggregate(rettype)) ? 5 : 4;
            tclass = TFUNC_CLASS(funcref->data.objref->type);
            if (TCLASS_FLAGS(tclass) & 1) {
                func_00480B40(load, 0x19, thisreg, NULL, 0);
                func_00480B40(load, 0x19, 0x19, NULL, TCLASS_VTABLE(tclass)->offset);
            } else {
                func_00480B40(load, 0x19, thisreg, NULL, TCLASS_VTABLE(tclass)->offset);
            }
            func_00480B40(load, 0x19, 0x19, NULL, vindex);
            masks[0] |= 0x2000000;
            fop.kind = 0;
            fop.reg = 0x19;
            masks[0] |= 1 << ((rettype && is_aggregate(rettype)) ? 5 : 4);
            func_004A8310(NULL, fop.reg, masks);
        } else if (!thisexpr) {
            func_004A8310(funcref->data.objref, 0, masks);
        } else {
            if (!thisexpr->hascall) {
                data_005D0140[thisexpr->type](thisexpr, NULL, &fop);
                if (fop.kind) {
                    func_004A64C0(&fop, TYPE(&void_ptr), 0);
                }
            }
            func_004A8310(funcref->data.objref, fop.reg, masks);
        }
    } else {
        if (!funcref->hascall) {
            data_005D0140[funcref->type](funcref, NULL, &fop);
            if (fop.kind) {
                func_004A64C0(&fop, TYPE(&void_ptr), 0);
            }
        }
        func_004A8310(NULL, fop.reg, masks);
    }

    if (out) {
        rc = func_00436B30(rettype);
        if ((UInt8)(rc - 4) <= 1) {
            t = rettype;
            if (t->type == TYPESTRUCT || t->type == TYPECLASS) {
                t = func_00464BE0(t);
            }
            *out = func_0047FAA0(IS_SINGLE(t) ? 0x5B1 : 0x5B2, 1, 0);
        } else if ((UInt8)(rc - 2) <= 1) {
            if (copts.function_align16) {
                *out = func_0047FAA0(
                    rettype->size > 8 ? data_00625E74 : data_00625E48,
                    0,
                    IS_LONGLONG(rettype) || (rettype->type == TYPEFLOAT && IS_DOUBLE(rettype))
                );
            } else {
                *out = func_0047FAA0(
                    data_00625E48,
                    0,
                    IS_LONGLONG(rettype) || (rettype->type == TYPEFLOAT && IS_DOUBLE(rettype))
                );
            }
        } else if (rc == 1) {
            out->kind = 3;
            out->imm = cint64_zero;
        } else {
            CError_FATAL(1392);
        }
    }
}
