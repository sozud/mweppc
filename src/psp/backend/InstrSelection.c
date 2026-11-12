#include "compiler/common.h"
#include "psp/BE.h"
#include "compiler/CError.h"
#include "compiler/CInt64.h"
#include "compiler/CMangler.h"
#include "compiler/CompilerTools.h"
#include "compiler/enode.h"
#include "compiler/objects.h"
#include "compiler/types.h"
#include <string.h>

// mwccpsp 3.0.1 InstrSelection.c: expression generators. Each takes the
// node, a target (unused here) and fills a VOperand.

#define OBJECT_FRAMEOFFSET(obj) (*(SInt32*)&((Object*)(obj))->u.var.offset) // +0x30

typedef struct ObjectList_ {
    struct ObjectList_* next;
    Object* object;
} ObjectList_;

extern ObjectList_* data_006242D4; // objects whose names need a $ fixup
extern SInt16 func_004A5360(Object* obj); // a local's base register
extern Boolean func_004B8980(Object* obj);
extern void func_00531730(Object* obj);
extern VOperand func_004C3BA0(SInt32 reg, UInt8 kind);
extern VOperand func_004C3B10(MOperand op);

// An object as a memory operand: base register and offset. Objects reached
// through $gp, $s7 or an absolute address get their offset from a relocation.
VOperand func_004949B0(Object* obj) {
    VOperand v;
    ObjectList_* list;

    memclrw(&v, sizeof(VOperand));
    v.kind = 2;
    v.reg = func_004A5360(obj);
    v.object = (PtrInt)obj;
#if MWCCPS2
    if ((int)v.reg == 28 || (int)v.reg == 23 || v.reg == 0 || func_004B8980((Object*)v.object)) {
#else
    if (v.reg == 28 || v.reg == 23 || v.reg == 0 || func_004B8980((Object*)v.object)) {
#endif
        v.offset = 0;
        if (obj->datatype != DLOCAL && strchr(CMangler_GetLinkName(obj)->name, '$')) {
            list = lalloc(sizeof(ObjectList_));
            list->object = obj;
            list->next = data_006242D4;
            data_006242D4 = list;
        } else {
            func_00531730(obj);
        }
    } else {
        v.offset = OBJECT_FRAMEOFFSET(obj);
    }
    v.unk18 = 0;
    return v;
}

// EOBJREF
void func_00494960(ENode* expr, void* target, VOperand* out) {
    *out = func_004949B0(expr->data.objref);
}

// EINTCONST: zero is $zero (a pair for long long).
void func_00495410(ENode* expr, void* target, VOperand* out) {
    if (!CInt64_IsZero(&expr->data.intval)) {
        out->kind = 3;
        out->imm = expr->data.intval;
        out->unk18 = 0;
    } else {
        *out = func_004C3BA0(0, 0);
        if ((expr->rtype->type == TYPEINT || expr->rtype->type == TYPEENUM) &&
            expr->rtype->size == 8) {
            out->regHi = 0;
            out->kind = 0;
        }
    }
}

// A node that caches its operand: the subexpression in its first operand
// slot (+0x20), the result in its second (+0x24).
#define CACHED_SUB(n) ((n)->data.diadic.left)
#define CACHED_OP(n) (*(VOperand**)&(n)->data.diadic.right)

extern void (*data_005D0140[])(ENode* expr, void* target, VOperand* out);
extern SInt32 data_00624788; // word size
extern void func_004A72B0(VOperand* op, ENode* expr);
extern void func_004A64C0(VOperand* op, Type* type, int flags); // make the operand a value
extern SInt16 CMach_GetTypeAlign(Type* type); // alignment
extern void func_00480230(SInt16 align, void* unk);
void func_0049B930(ENode* expr, VOperand* old, VOperand* updated);

#define IS_AGGREGATE(t) \
    ((t) && \
     ((t)->type == TYPEARRAY || \
      (t)->type == TYPESTRUCT || \
      (t)->type == TYPECLASS || \
      ((t)->type == TYPEMEMBERPOINTER && (t)->size == 12)))

// The operand of the node, loaded when it has to be.
void func_0049B770(ENode* expr, void* target, VOperand* out) {
    ENode* sub;
    Type* type;

    sub = expr->data.monadic;
    data_005D0140[sub->type](sub, NULL, out);
    func_004A72B0(out, expr);
    if (!(out->flags & 0x80)) {
        return;
    }
    type = expr->rtype;
    if (type->type == TYPEINT ||
        type->type == TYPEENUM ||
        type->type == TYPEPOINTER ||
        (type->type == TYPEMEMBERPOINTER && type->size == 4) ||
        type->type == TYPEFLOAT ||
        (IS_AGGREGATE(type) && type->size <= data_00624788)) {
        if (out->kind) {
            func_004A64C0(out, type, 0);
        }
        if (IS_AGGREGATE(expr->rtype) && sub->type == 0x3A) {
            func_00480230(CMach_GetTypeAlign(sub->rtype), sub->data.monadic);
        }
    }
}

void func_0049B8D0(ENode* expr, void* target, VOperand* out) {
    VOperand a;
    VOperand b;

    func_0049B930(expr, &a, &b);
    *out = a;
}

// The subexpression of the node's cached operand, generated once.
void func_0049C160(ENode* expr, void* target, VOperand* out) {
    ENode* node = expr->data.monadic;

    if (!CACHED_OP(node)) {
        CACHED_OP(node) = lalloc(sizeof(VOperand));
        data_005D0140[CACHED_SUB(node)->type](CACHED_SUB(node), NULL, CACHED_OP(node));
    }
    *out = *CACHED_OP(node);
}

void func_0049C1E0(ENode* expr, void* target, VOperand* out) {
    ENode* node = expr;

    if (!CACHED_OP(node)) {
        CACHED_OP(node) = lalloc(sizeof(VOperand));
        data_005D0140[CACHED_SUB(node)->type](CACHED_SUB(node), NULL, CACHED_OP(node));
    }
    *out = *CACHED_OP(node);
}

extern VOperand func_0047ECC0(VOperand* src, CInt64* value, Type* type);
extern VOperand func_0047EF00(VOperand* src, SInt32 value, Type* type);
extern VOperand func_0047F740(UInt16 op, VOperand* a, VOperand* b, char rclass);
extern VOperand func_004A7390(VOperand* src); // the high word of a pair

// src + value; a long long adds the halves with a carry.
VOperand func_0049C060(Type* type, VOperand* src, CInt64 value) {
    VOperand lo;
    VOperand hi;
    VOperand c;

#if MWCCPS2
    // b210: a long long adds its constant in one 64-bit register
    return func_0047ECC0(src, &value, type);
#endif
    if ((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8) {
        hi = func_004A7390(src);
        lo = func_0047EF00(src, value.lo, NULL);
        c = func_0047F740(0xD7, &lo, src, 0);
        c = func_0047F740(4, &c, &hi, 0);
        c = func_0047EF00(&c, value.hi, NULL);
        lo.regHi = c.reg;
        lo.kind = 0;
        return lo;
    }
    return func_0047ECC0(src, &value, type);
}

#include "compiler/CFunc.h"
#include "compiler/CodeGen.h"

extern SInt32 data_00625A98[7]; // next virtual register, per class
extern SInt16 data_00625E48; // move
extern VOperand func_0047EA50(VOperand* src, SInt32 shift, Type* type); // src >> shift
extern void func_00479540(UInt16 op, VOperand* src, CLabel* label); // compare with zero and branch
extern VOperand func_00479D70(UInt16 op, VOperand* src, SInt32 imm);
extern VOperand func_0047F9A0(VOperand* src);
extern VOperand func_0047F900(UInt16 op, VOperand* a, char rclass);
extern void func_0046FA90(MPCodeLabel* label);
extern void func_00480710(MPCodeLabel* label); // branch_always

// Signed division by 2^shift, rounding toward zero; negated when asked.
// Not for long long.
Boolean func_00494170(VOperand* src, SInt32 shift, int negate, VOperand* out, Type* type) {
    Boolean ll;
    SInt32 reg;
    CLabel* label;
    VOperand t;
    VOperand v;

#if MWCCPS2 // b210 divides a long long in one 64-bit GPR too
    ll = (Boolean)(((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8) != 0);
#else
    ll = (type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8;
    if (ll) {
        return 0;
    }
#endif
    reg = data_00625A98[0]++;
    label = newlabel();
    t = func_0047EA50(src, shift, type);
    emitpcode(data_00625E48, reg, t.reg);
    func_00479540(0x1C, src, label);
    t = func_00479D70(ll ? 0x78 : 3, src, (BIT32(shift)) - 1);
    t = func_0047EA50(&t, shift, type);
    emitpcode(data_00625E48, reg, t.reg);
    func_0046FA90((MPCodeLabel*)label->pclabel);
    t = func_004C3BA0(reg, 0);
    *out = func_0047F9A0(&t);
    if (negate) {
        v = func_0047F900(ll ? 0x83 : 0xB9, out, 0);
        *out = v;
    }
    return 1;
}

#include "compiler/CParser.h"
#include "compiler/CMachine.h"

// out = src >> shift; a long long shifts the pair.
#if MWCCPS2
// b210 (0x49EEB0): 64-bit registers shift a long long like any integer
// (a call in b210's EDIV, not inlined)
__declspec(noinline) void func_00494360(VOperand* src, Type* type, SInt16 shift, VOperand* out) {
    *out = func_0047EA50(src, (SInt16)(shift & 0x3F), type);
}
#else
void func_00494360(VOperand* src, Type* type, SInt16 shift, VOperand* out) {
    SInt16 n;
    SInt32 t1;
    SInt32 t2;

    if (!((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8)) {
        *out = func_0047EA50(src, (SInt16)(shift & 0x3F), type);
        return;
    }
    n = shift & 0x3F;
    if (n >= 32) {
        if (n == 32) {
            out->reg = src->regHi;
            emitpcode(
                is_unsigned(type) ? 0xDA : 0xD8, out->regHi = data_00625A98[0]++, out->reg, 31
            );
            out->kind = 0;
        } else {
            emitpcode(
                is_unsigned(type) ? 0xDA : 0xD8, out->regHi = data_00625A98[0]++, src->regHi, 31
            );
            emitpcode(
                is_unsigned(type) ? 0xDA : 0xD8, out->reg = data_00625A98[0]++, src->regHi, n - 32
            );
            out->kind = 0;
        }
    } else {
        out->kind = 0;
        n = shift & 0x3F;
        emitpcode(is_unsigned(type) ? 0xDA : 0xD8, out->regHi = data_00625A98[0]++, src->regHi, n);
        emitpcode(0xDA, t1 = data_00625A98[0]++, src->reg, n);
        emitpcode(0xD2, t2 = data_00625A98[0]++, src->regHi, 32 - n);
        emitpcode(0xBE, out->reg = data_00625A98[0]++, t2, t1);
    }
}
#endif

typedef struct UsedRegs_ {
    SInt32 codewords;
    UInt32 intregs;
    UInt32 fpregs;
} UsedRegs_;

extern UsedRegs_* data_006241D8;
extern int func_0045B8E0(ENode* expr); // an intrinsic call
extern void func_004585B0(ENode* expr, SInt16 target, VOperand* out); // expand the intrinsic
extern void func_004A7400(ENode* expr, VOperand* out); // an ordinary call

// EFUNCCALL
void func_00494AA0(ENode* expr, SInt16 target, VOperand* out) {
    if (func_0045B8E0(expr)) {
        func_004585B0(expr, target, out);
    } else {
        data_006241D8->intregs |= 0x80000000;
        func_004A7400(expr, out);
    }
}

#define IS_SINGLE(t) \
    (TYPE_INTEGRAL(t)->integral == IT_FLOAT || ((t)->type == TYPEFLOAT && (t)->size == 4))
#define IS_DOUBLE(t) \
    (TYPE_INTEGRAL(t)->integral == IT_DOUBLE || ((t)->type == TYPEFLOAT && (t)->size == 8))
#define IS_HARDFLOAT(t) \
    ((IS_SINGLE(t) && (copts.fp_library & 1)) || (IS_DOUBLE(t) && (copts.fp_library & 2)))
#define IS_SOFTFLOAT(t) \
    ((IS_SINGLE(t) && !(copts.fp_library & 1)) || (IS_DOUBLE(t) && !(copts.fp_library & 2)))

void func_004950C0(
    ENode* cond, CLabel* iftrue, CLabel* iffalse, Boolean truenext, Boolean falsenext
); // branch on a condition
extern VOperand func_004A7050(VOperand* src, Type* type); // into a new register

// ECOND: a ? b : c.
void func_00494AE0(ENode* expr, void* target, VOperand* out) {
    Type* type;
    ENode* a;
    ENode* b;
    ENode* cond;
    CLabel* l1;
    CLabel* l2;
    CLabel* l3;
    SInt32 op;
    Type* t;
    VOperand va;
    VOperand vb;
    VOperand c;

    type = expr->rtype;
    a = expr->data.cond.expr1;
    b = expr->data.cond.expr2;
    if (copts.peephole &&
        copts.conditional_move &&
        a->type == EINTCONST &&
        b->type == EINTCONST &&
        ((cond = expr->data.cond.cond)->type == EEQU || cond->type == ENOTEQU) &&
        cond->data.diadic.right->type == EINTCONST &&
        CInt64_Equal(cond->data.diadic.right->data.intval, cint64_zero)) {
        // a conditional move on the value compared with zero
        op = expr->data.cond.cond->type == ENOTEQU ? 0x111 : 0x110;
        data_005D0140[expr->data.cond.cond->data.diadic.left->type](
            expr->data.cond.cond->data.diadic.left, NULL, &c
        );
        if (c.kind) {
            func_004A64C0(&c, expr->data.cond.cond->data.diadic.left->rtype, 1);
        }
        data_005D0140[a->type](a, NULL, &va);
        if (va.kind) {
            func_004A64C0(&va, a->rtype, 1);
        }
        if (!va.reg) {
            va = func_004A7050(&va, a->rtype);
        }
        data_005D0140[b->type](b, NULL, &vb);
        if (vb.kind) {
            func_004A64C0(&vb, b->rtype, 1);
        }
        emitpcode(op, va.reg, vb.reg, c.reg);
        *out = func_004C3B10(data_006246B4->last->args[0]);
        return;
    }

    l1 = newlabel();
    l2 = newlabel();
    l3 = newlabel();
    l1->pclabel = (struct PCodeLabel*)makepclabel();
    l2->pclabel = (struct PCodeLabel*)makepclabel();
    l3->pclabel = (struct PCodeLabel*)makepclabel();
    func_004950C0(expr->data.cond.cond, l1, l2, 1, 0);

    func_0046FA90((MPCodeLabel*)l1->pclabel);
    data_005D0140[a->type](a, NULL, &va);
    if (va.kind <= 1) {
        va = func_004A7050(&va, type);
    } else if (IS_AGGREGATE(a->rtype) && a->rtype->size > data_00624788) {
        if (va.kind == 4) {
            func_004A72B0(&va, a);
            va.kind = 2;
        }
        if (va.kind) {
            func_004A64C0(&va, a->rtype, 1);
        }
    } else if (va.kind) {
        func_004A64C0(&va, a->rtype, 1);
    }
    func_00480710((MPCodeLabel*)l3->pclabel);

    func_0046FA90((MPCodeLabel*)l2->pclabel);
    data_005D0140[b->type](b, NULL, &vb);
    if (IS_AGGREGATE(b->rtype) && b->rtype->size > data_00624788) {
        if (vb.kind == 4) {
            func_004A72B0(&vb, b);
            vb.kind = 2;
        }
        if (vb.kind) {
            func_004A64C0(&vb, b->rtype, 1);
        }
    } else if (vb.kind) {
        func_004A64C0(&vb, b->rtype, 1);
    }
    t = b->rtype;
    if (t->type == TYPEFLOAT && IS_HARDFLOAT(t)) {
        emitpcode(IS_SINGLE(t) ? MACRO_OP(0x99) : MACRO_OP(0x9A), va.reg, vb.reg);
    } else {
        emitpcode(data_00625E48, va.reg, vb.reg);
#if !MWCCPS2 // b210: one 64-bit register, no high half
        if (((t->type == TYPEINT || t->type == TYPEENUM) && t->size == 8) ||
            (t->type == TYPEFLOAT && IS_DOUBLE(t))) {
            emitpcode(data_00625E48, va.regHi, vb.regHi);
        }
#endif
    }
    func_0046FA90((MPCodeLabel*)l3->pclabel);

    if (type->type != TYPEVOID && !expr->ignored) {
        *out = va;
    } else {
        out->kind = 3;
        out->imm = cint64_zero;
    }
}

extern VOperand func_0047EBB0(VOperand* src, SInt32 shift, Type* type); // src << shift
extern MPCode* makepcode(VarArgShort op, ...);

#define IS_TYPE_LL(t) (((t)->type == TYPEINT || (t)->type == TYPEENUM) && (t)->size == 8)

// out = src << shift (negated when asked); a long long shifts the pair.
#if MWCCPS2
// b210 (0x49EF20): one 64-bit register, so no pair shift (called, not inlined)
__declspec(noinline) void func_004945B0(
    VOperand* src, Type* type, SInt16 shift, SInt16 negate, VOperand* out
) {
    VOperand v;

    *out = func_0047EBB0(src, (SInt16)(shift & 0x3F), type);
    if (negate) {
        v = func_0047F900(IS_TYPE_LL(type) ? 0x83 : 0xB9, out, 0);
        *out = v;
    }
}
#else
void func_004945B0(VOperand* src, Type* type, SInt16 shift, SInt16 negate, VOperand* out) {
    SInt16 n;
    Boolean ll;
    MPCode* pc;
    VOperand hi;
    VOperand t;
    VOperand v;

    if (!IS_TYPE_LL(type)) {
        *out = func_0047EBB0(src, (SInt16)(shift & 0x3F), type);
    } else {
        n = shift & 0x3F;
        if (n >= 32) {
            if (n == 32) {
                *out = func_004C3BA0(0, 0);
                out->regHi = src->reg;
            } else {
                *out = func_004C3BA0(0, 0);
                t = func_0047EBB0(src, n - 32, type);
                out->regHi = t.reg;
            }
        } else {
            hi = func_004A7390(src);
            n = shift & 0x3F;
            hi = func_0047EBB0(&hi, n, type);
            *out = func_0047EBB0(src, n, type);
            pc = makepcode(0xDA, data_00625A98[0]++, src->reg, 32 - n);
            appendpcode(data_006246B4, pc);
            t = func_004C3B10(pc->args[0]);
            hi = func_0047F740(0xBE, &hi, &t, 0);
            out->kind = 0;
            out->regHi = hi.reg;
        }
    }
    if (negate) {
        if (IS_TYPE_LL(type)) {
            CError_FATAL(1018);
        }
        ll = IS_TYPE_LL(type);
        v = func_0047F900(ll ? 0x83 : 0xB9, out, 0);
        *out = v;
    }
}
#endif

extern void func_0049C250(ENode* cond, CLabel* label, Boolean truth); // conditionalstatement

// Branch to iftrue or iffalse on the condition; truenext/falsenext say
// which label directly follows, so its branch can be left out.
#undef CError_PS2_SHIFT
#define CError_PS2_SHIFT -15 // mwccps2: its assert is at line 0xE53
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -34 // b145 asserts
void func_004950C0(
    ENode* cond, CLabel* iftrue, CLabel* iffalse, Boolean truenext, Boolean falsenext
) {
    CLabel* label;

    label = newlabel();
    switch (cond->type) {
        case ELAND:
            label->pclabel = (struct PCodeLabel*)makepclabel();
            func_004950C0(cond->data.diadic.left, label, iffalse, 1, 0);
            func_0046FA90((MPCodeLabel*)label->pclabel);
            func_004950C0(cond->data.diadic.right, iftrue, iffalse, 0, 0);
            break;
        case ELOR:
            label->pclabel = (struct PCodeLabel*)makepclabel();
            func_004950C0(cond->data.diadic.left, iftrue, label, 0, 1);
            func_0046FA90((MPCodeLabel*)label->pclabel);
            func_004950C0(cond->data.diadic.right, iftrue, iffalse, 0, 0);
            break;
        case ELOGNOT:
            func_004950C0(cond->data.monadic, iffalse, iftrue, falsenext, truenext);
            break;
        case ELESS:
        case EGREATER:
        case ELESSEQU:
        case EGREATEREQU:
        case EEQU:
        case ENOTEQU:
            if (truenext) {
                func_0049C250(cond, iffalse, 0);
            } else if (falsenext) {
                func_0049C250(cond, iftrue, 1);
            } else {
                func_0049C250(cond, iftrue, 1);
                func_00480710((MPCodeLabel*)iffalse->pclabel);
            }
            break;
        default:
            CError_FATAL(3682);
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#undef CError_PS2_SHIFT
#define CError_PS2_SHIFT 0

extern VOperand func_00497480(
    Type* type, ENode* left, ENode* right, UInt16 op, UInt16 op64
); // a logical operation

// EOR
void func_00497360(ENode* expr, void* target, VOperand* out) {
    *out = func_00497480(
        expr->rtype, expr->data.diadic.left, expr->data.diadic.right, 0xBE, MACRO_OP(0x69)
    );
}

// EXOR
void func_004973C0(ENode* expr, void* target, VOperand* out) {
    *out = func_00497480(
        expr->rtype, expr->data.diadic.left, expr->data.diadic.right, 0xE4, MACRO_OP(0x97)
    );
}

// EAND
void func_00497420(ENode* expr, void* target, VOperand* out) {
    *out = func_00497480(
        expr->rtype, expr->data.diadic.left, expr->data.diadic.right, 6, MACRO_OP(0x02)
    );
}

void func_0049D8D0(
    ENode* left, ENode* right, VOperand* a, VOperand* b, int load
); // generate both operands
extern VOperand func_0047E650(VOperand* src, UInt32 mask); // src & mask
extern VOperand func_0047E580(VOperand* src, UInt32 mask); // src | mask
extern VOperand func_0047E3D0(VOperand* src, SInt32 value); // src ^ value
extern VOperand func_0047F9F0(CInt64 value, Type* type);

// A bitwise operation; op64 (0x51A and, 0x581 or, 0x5AF xor) names it.
VOperand func_00497480(Type* type, ENode* left, ENode* right, UInt16 op, UInt16 op64) {
    VOperand a;
    VOperand b;
    VOperand tmp;
    VOperand r;
    VOperand hi;
    VOperand t;

#if !MWCCPS2 // b210 does not clear them first
    memclrw(&a, sizeof(VOperand));
    memclrw(&b, sizeof(VOperand));
#endif
    func_0049D8D0(left, right, &a, &b, 0);
    if (a.kind == 3 && b.kind == 3) {
        switch (op64) {
            case MACRO_OP(0x02):
                a.imm = CInt64_And(a.imm, b.imm);
                return a;
            case MACRO_OP(0x69):
                a.imm = CInt64_Or(a.imm, b.imm);
                return a;
            case MACRO_OP(0x97):
                a.imm = CInt64_Xor(a.imm, b.imm);
                return a;
        }
    }
    if (a.kind == 3) {
        tmp = a;
        a = b;
        b = tmp;
    }
    if (a.kind) {
        func_004A64C0(&a, left->rtype, 0);
    }
    if (b.kind != 3 && b.kind) {
        func_004A64C0(&b, right->rtype, 0);
    }

#if !MWCCPS2 // b210: a long long is one 64-bit register
    if (IS_TYPE_LL(type)) {
        hi = func_004A7390(&a);
        if (b.kind == 3) {
            switch (op64) {
                case MACRO_OP(0x02):
                    r = func_0047E650(&a, b.imm.lo);
                    hi = func_0047E650(&hi, b.imm.hi);
                    break;
                case MACRO_OP(0x69):
                    r = func_0047E580(&a, b.imm.lo);
                    hi = func_0047E580(&hi, b.imm.hi);
                    break;
                case MACRO_OP(0x97):
                    r = func_0047E3D0(&a, b.imm.lo);
                    hi = func_0047E3D0(&hi, b.imm.hi);
                    break;
            }
        } else {
            r = func_0047F740(op, &a, &b, 0);
            t = func_004A7390(&b);
            hi = func_0047F740(op, &hi, &t, 0);
        }
        r.kind = 0;
        r.regHi = hi.reg;
        return r;
    }
#endif
    if (IS_TYPE_LL(type) && !CInt64_IsInRange(b.imm, 4)) {
        t = func_0047F9F0(b.imm, type);
        return func_0047F740(op, &a, &t, 0);
    }
    if (b.kind == 3) {
        switch (op64) {
            case MACRO_OP(0x02):
                r = func_0047E650(&a, b.imm.lo);
                break;
            case MACRO_OP(0x69):
                r = func_0047E580(&a, b.imm.lo);
                break;
            case MACRO_OP(0x97):
                r = func_0047E3D0(&a, b.imm.lo);
                break;
        }
    } else {
        r = func_0047F740(op, &a, &b, 0);
    }
    return r;
}

extern SInt16 func_00464E20(Object* obj); // an object's alignment
extern void func_0052DE30(VOperand* dst, VOperand* src, SInt32 size, SInt32 align); // copy a block

// Struct assignment: copy the block, as aligned as both objects allow.
void func_00496CB0(ENode* expr, void* target, VOperand* out) {
#if MWCCPS2
    ENode* left;
    ENode* right;
#else
    ENode* right;
    ENode* left;
#endif
    Type* type;
    SInt32 align;
    VOperand src;

    right = expr->data.diadic.right;
    left = expr->data.diadic.left;
    type = expr->rtype;
    data_005D0140[right->type](right, NULL, &src);
    data_005D0140[left->type](left, NULL, out);
    align = CMach_GetTypeAlign(type);
#if MWCCPS2
    if (left->type == EOBJREF && right->type == EOBJREF) {
        if (func_00464E20(left->data.objref) < func_00464E20(right->data.objref)) {
            align = func_00464E20(left->data.objref);
        } else {
            align = func_00464E20(right->data.objref);
        }
    }
#else
    if (left->type == EOBJREF && right->type == EOBJREF) {
        align = func_00464E20(
            func_00464E20(left->data.objref) < func_00464E20(right->data.objref)
                ? left->data.objref
                : right->data.objref
        );
    }
#endif
    func_0052DE30(out, &src, type->size, align);
}

extern Object* rt_ashldi3;
extern void func_004A47E0(VOperand* op, int unk, int argno); // pass as a call argument
extern void func_004A4A20(Object* rtfunc); // call
extern VOperand func_004A46E0(int pair); // the call's result

// ESHL
void func_00497E50(ENode* expr, void* target, VOperand* out) {
    Type* type;
    ENode* left;
    ENode* right;
    SInt16 op;
    VOperand a;
    VOperand b;

    left = expr->data.diadic.left;
    right = expr->data.diadic.right;
    type = expr->rtype;
    if (right->type == EINTCONST) {
        data_005D0140[left->type](left, NULL, &a);
        if ((a.kind >= 4 || a.kind == 2) && a.kind) {
            func_004A64C0(&a, left->rtype, 0);
        }
        func_004945B0(&a, type, (SInt16)right->data.intval.lo, 0, out);
        return;
    }
#if !MWCCPS2 // b210 shifts a long long with dsllv
    if (IS_TYPE_LL(type)) {
        func_0049D8D0(left, right, &a, &b, 1);
        func_004A47E0(&a, 1, 1);
        func_004A47E0(&b, 1, 2);
        func_004A4A20(rt_ashldi3);
        *out = func_004A46E0(1);
        return;
    }
#endif
    op = IS_TYPE_LL(type) ? 0x86 : 0xD3;
    func_0049D8D0(left, right, &a, &b, 0);
    if (a.kind) {
        func_004A64C0(&a, left->rtype, 0);
    }
    if (b.kind) {
        func_004A64C0(&b, right->rtype, 0);
    }
    emitpcode(op, data_00625A98[0]++, a.reg, b.reg);
    *out = func_004C3B10(data_006246B4->last->args[0]);
}

extern Object* rt_lshrdi3;
extern Object* rt_ashrdi3;

// ESHR
void func_00497A90(ENode* expr, void* target, VOperand* out) {
    Type* type;
    ENode* left;
    ENode* right;
    SInt16 op;
    VOperand a;
    VOperand b;

    left = expr->data.diadic.left;
    right = expr->data.diadic.right;
    type = left->rtype;
    if (right->type == EINTCONST) {
        data_005D0140[left->type](left, NULL, &a);
        if ((a.kind >= 4 || a.kind == 2) && a.kind) {
            func_004A64C0(&a, left->rtype, 0);
        }
        func_00494360(&a, type, (SInt16)right->data.intval.lo, out);
        return;
    }
#if MWCCPS2 // b210 shifts a long long with dsrlv/dsrav
    if (IS_TYPE_LL(type)) {
#else
    if (IS_TYPE_LL(type)) {
        func_0049D8D0(left, right, &a, &b, 1);
        func_004A47E0(&a, 1, 1);
        func_004A47E0(&b, 1, 2);
        func_004A4A20(is_unsigned(type) ? rt_lshrdi3 : rt_ashrdi3);
        *out = func_004A46E0(1);
    } else if (IS_TYPE_LL(type)) {
#endif
        op = is_unsigned(type) ? 0x8C : 0x89;
        func_0049D8D0(left, right, &a, &b, 0);
        if (a.kind) {
            func_004A64C0(&a, left->rtype, 0);
        }
        if (b.kind) {
            func_004A64C0(&b, right->rtype, 0);
        }
        emitpcode(op, data_00625A98[0]++, a.reg, b.reg);
        *out = func_004C3B10(data_006246B4->last->args[0]);
    } else {
        op = is_unsigned(type) ? 0xDB : 0xD9;
        func_0049D8D0(left, right, &a, &b, 0);
        if (a.kind) {
            func_004A64C0(&a, left->rtype, 0);
        }
        if (b.kind) {
            func_004A64C0(&b, right->rtype, 0);
        }
        emitpcode(op, data_00625A98[0]++, a.reg, b.reg);
        *out = func_004C3B10(data_006246B4->last->args[0]);
    }
}

// Generate both operands, the costlier first; with load, into registers.
void func_0049D8D0(ENode* left, ENode* right, VOperand* a, VOperand* b, int load) {
    if (right->cost > left->cost + 2) {
        data_005D0140[right->type](right, NULL, b);
        if (load && b->kind) {
            func_004A64C0(b, right->rtype, 0);
        }
        data_005D0140[left->type](left, NULL, a);
        if (load && a->kind) {
            func_004A64C0(a, left->rtype, 0);
        }
    } else {
        data_005D0140[left->type](left, NULL, a);
        if (load && a->kind) {
            func_004A64C0(a, left->rtype, 0);
        }
        data_005D0140[right->type](right, NULL, b);
        if (load && b->kind) {
            func_004A64C0(b, right->rtype, 0);
        }
    }
}

extern void func_004A5A90(VOperand src, VOperand* dst, Type* type); // store
extern void func_004A5380(VOperand src, VOperand* dst, PtrInt bitfield); // merge into a bitfield
extern void func_004A5730(VOperand src, PtrInt bitfield, VOperand* out); // a bitfield's new value
extern VOperand func_00479C30(UInt16 op, VOperand* src, SInt32 bits); // sign or zero extend

// EASS
void func_00496D50(ENode* expr, void* target, VOperand* out) {
    ENode* right;
    ENode* left;
    Type* type;
    int indirect;
    PtrInt bitfield;
    VOperand src;
    VOperand dst;
    VOperand tmp;

    right = expr->data.diadic.right;
    left = expr->data.diadic.left;
    type = expr->rtype;
    memset(out, 0, sizeof(VOperand));
    memset(&dst, 0, sizeof(VOperand));
    memset(&src, 0, sizeof(VOperand));
    indirect = right->type == EINDIRECT ? 1 : 0;
    data_005D0140[right->type](right, NULL, &src);

    if (type->type == TYPEFLOAT) {
        if (src.kind) {
            func_004A64C0(&src, right->rtype, 0);
        }
        if (left->type == EINDIRECT) {
            data_005D0140[left->data.monadic->type](left->data.monadic, NULL, &dst);
        } else {
            data_005D0140[left->type](left, NULL, &dst);
        }
        func_004A72B0(&dst, NULL);
        func_004A5A90(src, &dst, type);
        out->kind = src.kind;
        out->reg = src.reg;
        out->regHi = src.regHi;
        out->unk18 = 0;
        return;
    }

    if (src.kind) {
        func_004A64C0(&src, right->rtype, 0);
    }
    if (left->type == EINDIRECT) {
        data_005D0140[left->data.monadic->type](left->data.monadic, NULL, &dst);
    } else {
        data_005D0140[left->type](left, NULL, &dst);
    }
    if ((bitfield = dst.unk18)) {
        dst.unk18 = 0;
        func_004A72B0(&dst, NULL);
        tmp = dst;
        if (tmp.kind) {
            func_004A64C0(&tmp, type, 0);
        }
        func_004A5380(src, &tmp, bitfield);
        func_004A5A90(tmp, &dst, type);
        if (!expr->ignored) {
            func_004A5730(tmp, bitfield, out);
            return;
        }
    } else {
        func_004A72B0(&dst, expr);
        func_004A5A90(src, &dst, type);
    }

    if (!indirect && !expr->ignored && (type->type == TYPEINT || type->type == TYPEENUM)) {
        // the value of the assignment, narrowed to the type
        switch (TYPE_INTEGRAL(type)->integral) {
            case IT_BOOL:
            case IT_CHAR:
            case IT_SCHAR:
            case IT_UCHAR:
                *out = func_00479C30(is_unsigned(type) ? MACRO_OP(0x82) : MACRO_OP(0x81), &src, 8);
                return;
            case IT_SHORT:
            case IT_USHORT:
#if MWCCPS2 // b210 narrows wchar_t to 16 bits without testing the option
            case IT_WCHAR_T:
#endif
                *out = func_00479C30(is_unsigned(type) ? MACRO_OP(0x82) : MACRO_OP(0x81), &src, 16);
                return;
#if !MWCCPS2
            case IT_WCHAR_T:
                if (copts.ushort_wchar_t) {
                    *out = func_00479C30(
                        is_unsigned(type) ? MACRO_OP(0x82) : MACRO_OP(0x81), &src, 16
                    );
                    return;
                }
#endif
            default:
                out->kind = src.kind;
                out->reg = src.reg;
                out->regHi = src.regHi;
                return;
        }
    }
    out->kind = src.kind;
    out->reg = src.reg;
    out->regHi = src.regHi;
    out->unk18 = 0;
}

extern Object* rt_f_sub;
extern Object* rt_subdf3;
void func_00499010(
    UInt16 op, ENode* x, ENode* y, ENode* z, SInt16 target, VOperand* out
); // fused multiply-add family
extern void func_004A4890(VOperand* op, Type* type, int argno); // pass a float argument
extern VOperand func_004A4710(Type* type); // a float call's result

#if MWCCPS2
extern VOperand ps2_const128_48E3D0(CInt64 value, Type* type); // load a constant (128-bit CInt64)
#endif
// madd/msub on the float accumulator: b210 tests #pragma usefloatacc (copts+0x3F)
#if MWCCPS2
#define FLOAT_ACC_ON copts.usefloatacc
#else
#define FLOAT_ACC_ON copts.mips_option3C
#endif
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -34 // b145 asserts (ESUB, EADD)
// ESUB
void func_004980C0(ENode* expr, SInt16 target, VOperand* out) {
    Type* type;
    ENode* left;
    ENode* right;
    VOperand a;
    VOperand b;
    VOperand borrow;
    VOperand ahi;
    VOperand bhi;

    type = expr->rtype;
    left = expr->data.diadic.left;
    right = expr->data.diadic.right;
    if (type->type == TYPEFLOAT) {
        if (right->type == EMUL &&
            IS_SINGLE(left->rtype) &&
            IS_SINGLE(right->rtype) &&
            IS_SINGLE(right->data.diadic.left->rtype) &&
            IS_SINGLE(right->data.diadic.right->rtype) &&
            FLOAT_ACC_ON) {
            CError_ASSERT(2412, right->rtype->type == TYPEFLOAT);
            func_00499010(
                0x1E2, right->data.diadic.left, right->data.diadic.right, left, target, out
            );
            return;
        }
        func_0049D8D0(left, right, &a, &b, 1);
        if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
            func_004A4890(&a, type, 1);
            func_004A4890(&b, type, 2);
            func_004A4A20(IS_SINGLE(type) ? rt_f_sub : rt_subdf3);
            *out = func_004A4710(type);
        } else {
            emitpcode(IS_SINGLE(type) ? 0x103 : 0x102, data_00625A98[1]++, a.reg, b.reg);
            *out = func_004C3B10(data_006246B4->last->args[0]);
        }
        return;
    }

    func_0049D8D0(left, right, &a, &b, 1);
    if (a.kind == 3 && b.kind == 3) {
        CError_FATAL(2449);
    }
    if (b.kind == 3) {
        CError_FATAL(2452);
    }
#if MWCCPS2
    // b210 (0x4A3200): one 64-bit register, dsubu for a long long
    if (a.kind == 3) {
        a = ps2_const128_48E3D0(a.imm, type);
    }
    *out = func_0047F740(IS_TYPE_LL(type) ? 0x8E : 0xDD, &a, &b, 0);
    return;
#endif
    if (a.kind == 3) {
        a = func_0047F9F0(a.imm, type);
    }
    if (IS_TYPE_LL(type)) {
        borrow = func_0047F740(0xD7, &a, &b, 0);
        *out = func_0047F740(0xDD, &a, &b, 0);
        ahi = func_004A7390(&a);
        bhi = func_004A7390(&b);
        ahi = func_0047F740(0xDD, &ahi, &bhi, 0);
        ahi = func_0047F740(0xDD, &ahi, &borrow, 0);
        out->kind = 0;
        out->regHi = ahi.reg;
    } else {
        *out = func_0047F740(0xDD, &a, &b, 0);
    }
}

extern Object* rt_f_add;
extern Object* rt_adddf3;
extern SInt32 data_00624744[7]; // first virtual register of each class
extern SInt16 data_00625E3E; // frame register
extern VOperand func_0047F2D0(VOperand* src); // the address in a register

#define ALL_SINGLE4(a, b, c, d) (IS_SINGLE(a) && IS_SINGLE(b) && IS_SINGLE(c) && IS_SINGLE(d))

// EADD
void func_00498590(ENode* expr, SInt16 target, VOperand* out) {
    Type* type;
    ENode* left;
    ENode* right;
    SInt32 reg;
    VOperand a;
    VOperand b;
    VOperand tmp;
    VOperand c;
    VOperand hi;

    type = expr->rtype;
    left = expr->data.diadic.left;
    right = expr->data.diadic.right;
    if (type->type == TYPEFLOAT) {
        if (left->type == EMUL &&
            ALL_SINGLE4(
                left->rtype,
                right->rtype,
                left->data.diadic.left->rtype,
                left->data.diadic.right->rtype
            ) &&
            FLOAT_ACC_ON) {
            CError_ASSERT(2246, left->rtype->type == TYPEFLOAT);
            func_00499010(
                0x1DE, left->data.diadic.left, left->data.diadic.right, right, target, out
            );
            return;
        }
        if (right->type == EMUL &&
            ALL_SINGLE4(
                left->rtype,
                right->rtype,
                right->data.diadic.left->rtype,
                right->data.diadic.right->rtype
            ) &&
            FLOAT_ACC_ON) {
            CError_ASSERT(2258, right->rtype->type == TYPEFLOAT);
            func_00499010(
                0x1DE, right->data.diadic.left, right->data.diadic.right, left, target, out
            );
            return;
        }
        func_0049D8D0(left, right, &a, &b, 1);
        if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
            func_004A4890(&a, type, 1);
            func_004A4890(&b, type, 2);
            func_004A4A20(IS_SINGLE(type) ? rt_f_add : rt_adddf3);
            *out = func_004A4710(type);
        } else {
            if (a.kind) {
                func_004A64C0(&a, left->rtype, 0);
            }
            if (b.kind) {
                func_004A64C0(&b, right->rtype, 0);
            }
            emitpcode(IS_SINGLE(type) ? 0xED : 0xEC, data_00625A98[1]++, a.reg, b.reg);
            *out = func_004C3B10(data_006246B4->last->args[0]);
        }
        return;
    }

    func_0049D8D0(left, right, &a, &b, 0);
    if (b.kind == 2) {
        tmp = a;
        a = b;
        b = tmp;
    }
    if (a.kind == 2) {
        // a memory operand plus an offset or a register
        if (b.kind == 2) {
            CError_FATAL(2303);
        }
        if (b.kind == 3) {
            *out = a;
            out->offset += b.imm.lo;
            return;
        }
        if (b.kind) {
            func_004A64C0(&b, right->rtype, 0);
        }
        reg = a.reg;
        if (reg < data_00624744[0] && reg != data_00625E3E) {
            tmp = func_004C3BA0(a.reg, 0);
            c = func_0047F2D0(&a);
            c = func_0047F740(4, &c, &b, 0);
            *out = a;
            out->offset = 0;
            out->object = 0;
            out->unk18 = 0;
            out->reg = c.reg;
            return;
        }
        emitpcode(4, reg = data_00625A98[0]++, b.reg, a.reg);
        *out = a;
        out->reg = reg;
        return;
    }
    if (a.kind == 3) {
        tmp = a;
        a = b;
        b = tmp;
    }
    if (a.kind) {
        func_004A64C0(&a, left->rtype, 0);
    }
    if (b.kind == 3) {
        if (expr->rtype->type == TYPEPOINTER) {
            memset(out, 0, sizeof(VOperand));
            out->kind = 2;
            out->offset = b.imm.lo;
            out->object = 0;
            out->reg = a.reg;
        } else {
            *out = func_0049C060(type, &a, b.imm);
        }
        return;
    }
    if (b.kind) {
        func_004A64C0(&b, right->rtype, 0);
    }
#if MWCCPS2
    // b210 (0x4A3610): one 64-bit register, daddu for a long long
    *out = func_0047F740(IS_TYPE_LL(type) ? 0x79 : 4, &a, &b, 0);
    return;
#endif
    if (IS_TYPE_LL(type)) {
        *out = func_0047F740(4, &a, &b, 0);
        c = func_0047F740(0xD7, out, &a, 0);
        hi = func_004A7390(&a);
        c = func_0047F740(4, &c, &hi, 0);
        hi = func_004A7390(&b);
        c = func_0047F740(4, &c, &hi, 0);
        out->regHi = c.reg;
        out->kind = 0;
        return;
    }
    *out = func_0047F740(IS_TYPE_LL(type) ? 0x79 : 4, &a, &b, 0);
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// x * y combined with z in the accumulator; the result in target or a new FPR.
void func_00499010(UInt16 op, ENode* x, ENode* y, ENode* z, SInt16 target, VOperand* out) {
    SInt32 dst;
    SInt32 acc;
    VOperand vx;
    VOperand vy;
    VOperand vz;

    memclrw(&vx, sizeof(VOperand));
    memclrw(&vy, sizeof(VOperand));
    memclrw(&vz, sizeof(VOperand));
    if (y->cost > x->cost + 2) {
        data_005D0140[y->type](y, NULL, &vy);
        if (vy.kind) {
            func_004A64C0(&vy, y->rtype, 0);
        }
        data_005D0140[x->type](x, NULL, &vx);
        if (vx.kind) {
            func_004A64C0(&vx, x->rtype, 0);
        }
    } else {
        data_005D0140[x->type](x, NULL, &vx);
        if (vx.kind) {
            func_004A64C0(&vx, x->rtype, 0);
        }
        data_005D0140[y->type](y, NULL, &vy);
        if (vy.kind) {
            func_004A64C0(&vy, y->rtype, 0);
        }
    }
    data_005D0140[z->type](z, NULL, &vz);
    if (vz.kind) {
        func_004A64C0(&vz, z->rtype, 0);
    }
    if (target) {
        dst = target;
    } else {
        dst = data_00625A98[1]++;
    }
    acc = data_00625A98[1]++;
    emitpcode(0xAD, 0, acc);
    emitpcode(0x1DD, acc, vz.reg);
    emitpcode(op, dst, vx.reg, vy.reg);
    memclrw(out, sizeof(VOperand));
    out->kind = 1;
    out->reg = dst;
}

extern Object* rt_f_mul;
extern Object* rt_muldf3;
extern Object* rt_muldi3;
extern Object* rt_muldi3_2;
extern Object* rt_l_muls;
extern Object* rt_l_mulu;
extern SInt16 data_00625CE0; // multiply
extern SInt16 data_00625E4E; // multiply (long long)
void func_00493FA0(
    SInt16 op, ENode* left, ENode* right, VOperand* out
); // a floating-point operation
extern VOperand MultiplyByConstant(
    VOperand* src, SInt32 value, Type* type
); // multiply by a constant
#if MWCCPS2
extern SInt16 ps2_pipeop3_63732A; // multiply, unsigned (data_00625E4A)
// b210's func_00491840 (0x49D250) takes the value as a native 64-bit integer,
// not a 128-bit CInt64
extern int ps2_bitnum_49D250(unsigned long long v);

// the size of a multiply operand: a widening cast (same signedness, not from
// a float) counts as the size it converts from; an assignment as its value
static inline SInt32 ll_mul_size(ENode* e) {
    if (e->type == EASS) {
        e = e->data.diadic.right;
    }
    if (e->type == ETYPCON &&
        e->data.monadic->rtype->size < e->rtype->size &&
        e->data.monadic->rtype->type != TYPEFLOAT &&
        is_unsigned(e->rtype) == is_unsigned(e->data.monadic->rtype)) {
        return e->data.monadic->rtype->size;
    }
    return e->rtype->size;
}

// a constant operand's low 64 bits (the low 32 only when it is narrower)
static inline unsigned long long ll_mul_const(ENode* e, SInt32 size) {
    return ((unsigned long long)(size < 8 ? 0 : e->data.intval.mid[1]) << 32) + e->data.intval.lo;
}

// the shift a power-of-two constant multiplies by (1..62), else 0
static inline int ll_mul_shift(unsigned long long v) {
    int n = ps2_bitnum_49D250(v);
    if (n > 0 && n < 0x3F) {
        return n;
    }
    return 0;
}

#define LL_MUL_GEN(e, v) \
    do { \
        data_005D0140[(e)->type]((e), NULL, &(v)); \
        if ((v).kind) { \
            func_004A64C0(&(v), (e)->rtype, 0); \
        } \
    } while (0)

// long long multiply in the 64-bit registers (EMUL with mips_option25): a
// power-of-two constant becomes a shift; otherwise 32x32 multiplies of the
// halves, the cross products added in shifted left by 32
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -34 // b145 asserts
void func_00490BB0(ENode* expr, SInt16 target, VOperand* out) {
    ENode* left;
    ENode* right;
    Type* type;
    SInt32 lsize;
    SInt32 rsize;
    SInt32 ts;
    unsigned long long lval;
    unsigned long long rval;
    unsigned long long tv;
    Boolean lsigned;
    Boolean rsigned;
    Boolean tb;
    int n;
    UInt16 dst;
    UInt16 t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11;
    VOperand a;
    VOperand b;
    VOperand tmp;

    left = expr->data.diadic.left;
    right = expr->data.diadic.right;
    type = expr->rtype;
    if (right->type == EINTCONST && left->type == EINTCONST) {
        CError_Internal(__FILE__, CError_B145_LINE(0x12D9));
    }
    memclrw(&a, sizeof(VOperand));
    memclrw(&b, sizeof(VOperand));
    lsize = ll_mul_size(left);
    rsize = ll_mul_size(right);
    if (left->type == EINTCONST) {
        lval = ll_mul_const(left, lsize);
    }
    if (right->type == EINTCONST) {
        rval = ll_mul_const(right, rsize);
    }
    if (!right->hascall) {
        if (left->type != EINTCONST || ll_mul_shift(lval) <= 0) {
            LL_MUL_GEN(left, a);
        }
        if (right->type != EINTCONST || ll_mul_shift(rval) <= 0) {
            LL_MUL_GEN(right, b);
        }
    } else {
        LL_MUL_GEN(right, b);
        if (left->type != EINTCONST || ll_mul_shift(lval) <= 0) {
            LL_MUL_GEN(left, a);
        }
    }
    lsigned = !is_unsigned(left->rtype);
    rsigned = !is_unsigned(right->rtype);
    if (lsize < rsize) {
        expr->data.diadic.left = right;
        expr->data.diadic.right = left;
        left = expr->data.diadic.left;
        right = expr->data.diadic.right;
        tmp = a;
        a = b;
        b = tmp;
        tv = lval;
        lval = rval;
        rval = tv;
        ts = lsize;
        lsize = rsize;
        rsize = ts;
        tb = lsigned;
        lsigned = rsigned;
        rsigned = tb;
    }
    dst = data_00625A98[0]++;
    if (left->type == EINTCONST && (n = ll_mul_shift(lval)) > 0) {
        *out = func_0047EBB0(&b, n, type);
        return;
    }
    if (right->type == EINTCONST && (n = ll_mul_shift(rval)) > 0) {
        *out = func_0047EBB0(&a, n, type);
        return;
    }
    switch (lsize + rsize) {
        case 2:
        case 3:
        case 4:
            if (!rsigned && !lsigned) {
                emitpcode(ps2_pipeop3_63732A, dst, a.reg, b.reg);
                emitpcode(0x48C, dst, dst, 0x20);
            } else {
                emitpcode(data_00625CE0, dst, a.reg, b.reg);
                emitpcode(0x48B, dst, dst, 0x20);
            }
            break;
        case 5:
        case 6:
        case 8:
            t1 = data_00625A98[0]++;
            if (!rsigned && !lsigned) {
                emitpcode(ps2_pipeop3_63732A, t1, a.reg, b.reg);
            } else {
                emitpcode(data_00625CE0, t1, a.reg, b.reg);
            }
            t2 = data_00625A98[0]++;
            emitpcode(0xA8, t2);
            emitpcode(0x228, dst, t2, t1);
            break;
        case 9:
        case 10:
        case 12:
            if (lsize != 8) {
                CError_Internal(__FILE__, CError_B145_LINE(0x136B));
            }
            if (!rsigned) {
                t1 = data_00625A98[0]++;
                emitpcode(0x48B, t1, a.reg, 0x20);
                t2 = data_00625A98[0]++;
                emitpcode(ps2_pipeop3_63732A, t2, t1, b.reg);
                t3 = data_00625A98[0]++;
                emitpcode(0xA8, t3);
                t4 = data_00625A98[0]++;
                emitpcode(0x228, t4, t3, t2);
                t5 = data_00625A98[0]++;
                emitpcode(0x88, t5, a.reg, 0);
                t6 = data_00625A98[0]++;
                emitpcode(data_00625CE0, t6, b.reg, t5);
                t7 = data_00625A98[0]++;
                emitpcode(0x85, t7, t6, 0);
                emitpcode(0x79, dst, t7, t4);
                break;
            }
            // a signed narrow operand: as 8 x 8
        case 16:
            t1 = data_00625A98[0]++;
            emitpcode(0x48B, t1, a.reg, 0x20);
            t2 = data_00625A98[0]++;
            emitpcode(0x48B, t2, b.reg, 0x20);
            t3 = data_00625A98[0]++;
            emitpcode(ps2_pipeop3_63732A, t3, t1, t2);
            t4 = data_00625A98[0]++;
            emitpcode(0xA8, t4);
            t5 = data_00625A98[0]++;
            emitpcode(0x228, t5, t4, t3);
            t6 = data_00625A98[0]++;
            emitpcode(0x88, t6, b.reg, 0);
            t7 = data_00625A98[0]++;
            emitpcode(data_00625CE0, t7, t6, t1);
            t8 = data_00625A98[0]++;
            emitpcode(0x88, t8, a.reg, 0);
            t9 = data_00625A98[0]++;
            emitpcode(data_00625CE0, t9, t2, t8);
            t10 = data_00625A98[0]++;
            emitpcode(0x79, t10, t7, t9);
            t11 = data_00625A98[0]++;
            emitpcode(0x85, t11, t10, 0);
            emitpcode(0x79, dst, t11, t5);
            break;
    }
    *out = func_004C3B10(data_006246B4->last->args[0]);
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#undef LL_MUL_GEN
#else
extern void func_00490BB0(ENode* expr, SInt16 target, VOperand* out);
#endif

// EMUL
void func_0049A740(ENode* expr, SInt16 target, VOperand* out) {
    ENode* left;
    ENode* right;
    Type* type;
    SInt16 op;
    VOperand a;
    VOperand b;

    left = expr->data.diadic.left;
    right = expr->data.diadic.right;
    type = expr->rtype;
    if (type->type == TYPEFLOAT) {
        if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
            func_0049D8D0(left, right, &a, &b, 1);
            func_004A4890(&a, type, 1);
            func_004A4890(&b, type, 2);
            func_004A4A20(IS_SINGLE(type) ? rt_f_mul : rt_muldf3);
            *out = func_004A4710(type);
        } else if (IS_DOUBLE(type) && !(copts.fp_library & 2)) {
            func_0049D8D0(left, right, &a, &b, 1);
            func_004A4890(&a, type, 1);
            func_004A4890(&b, type, 2);
            func_004A4A20(rt_muldf3);
            *out = func_004A4710(type);
        } else {
            func_00493FA0(IS_SINGLE(type) ? 0xB5 : 0xB4, left, right, out);
        }
        return;
    }
#if !MWCCPS2 // b210 multiplies a long long in the 64-bit registers
    if (IS_TYPE_LL(type)) {
        func_0049D8D0(left, right, &a, &b, 1);
        func_004A47E0(&a, 1, 1);
        func_004A47E0(&b, 1, 2);
        func_004A4A20(is_unsigned(type) ? rt_muldi3_2 : rt_muldi3);
        *out = func_004A46E0(1);
        return;
    }
#endif
    if (right->type == EINTCONST && CInt64_IsInRange(right->data.intval, 4)) {
        data_005D0140[left->type](left, NULL, &a);
        if ((a.kind >= 4 || a.kind == 2) && a.kind) {
            func_004A64C0(&a, left->rtype, 0);
        }
        if (a.kind == 3) {
            a.imm = CInt64_Mul(a.imm, right->data.intval);
            *out = a;
            return;
        }
        *out = MultiplyByConstant(&a, right->data.intval.lo, type);
        return;
    }
    if (left->type == EINTCONST && CInt64_IsInRange(left->data.intval, 4)) {
        data_005D0140[right->type](right, NULL, &b);
        if ((b.kind >= 4 || b.kind == 2) && b.kind) {
            func_004A64C0(&b, right->rtype, 0);
        }
        *out = MultiplyByConstant(&b, left->data.intval.lo, type);
        return;
    }
    if (IS_TYPE_LL(type) && copts.mips_option25) {
        func_00490BB0(expr, target, out);
        return;
    }
    if (copts.mips_option26) {
        func_0049D8D0(left, right, &a, &b, 1);
        func_004A47E0(&a, 0, 1);
        func_004A47E0(&b, 0, 2);
        func_004A4A20(is_unsigned(type) ? rt_l_mulu : rt_l_muls);
        *out = func_004A46E0(0);
        return;
    }
    op = IS_TYPE_LL(type) ? data_00625E4E : data_00625CE0;
    func_0049D8D0(left, right, &a, &b, 0);
    if (a.kind) {
        func_004A64C0(&a, left->rtype, 0);
    }
    if (b.kind) {
        func_004A64C0(&b, right->rtype, 0);
    }
    emitpcode(op, data_00625A98[0]++, a.reg, b.reg);
    *out = func_004C3B10(data_006246B4->last->args[0]);
}

extern Object* rt_f_div;
extern Object* rt_divdf3;
extern Object* rt_divdi3;
extern Object* rt_udivdi3;
extern Object* rt_l_divs;
extern Object* rt_l_divu;
extern SInt16 data_00625E42; // signed divide
extern SInt16 data_00625E54; // signed divide (long long)
VOperand func_0049A5C0(VOperand* src, UInt32 divisor); // unsigned divide by a constant
VOperand func_0049A430(VOperand* src, SInt32 divisor); // signed divide by a constant

// log2 of a power-of-two constant, else -1 (or -2 when it needs more than 32 bits).
inline SInt32 const_log2(CInt64 value) {
    return CInt64_IsInRange(value, 4) ? getbit(value.lo) : -2;
}

// EDIV
void func_00499820(ENode* expr, SInt16 target, VOperand* out) {
    ENode* left;
    ENode* right;
    Type* type;
    SInt32 n;
    SInt16 op;
    SInt16 divop;
    MPCodeLabel* ok;
    MPCodeLabel* trap;
    VOperand a;
    VOperand b;

    left = expr->data.diadic.left;
    right = expr->data.diadic.right;
    type = expr->rtype;
    if (type->type == TYPEFLOAT) {
        if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
            func_0049D8D0(left, right, &a, &b, 1);
            func_004A4890(&a, type, 1);
            func_004A4890(&b, type, 2);
            func_004A4A20(IS_SINGLE(type) ? rt_f_div : rt_divdf3);
            *out = func_004A4710(type);
        } else if (IS_DOUBLE(type) && !(copts.fp_library & 2)) {
            func_0049D8D0(left, right, &a, &b, 1);
            func_004A4890(&a, type, 1);
            func_004A4890(&b, type, 2);
            func_004A4A20(rt_divdf3);
            *out = func_004A4710(type);
        } else {
            func_00493FA0(IS_SINGLE(type) ? 0xF3 : 0xF2, left, right, out);
        }
        return;
    }

    func_0049D8D0(left, right, &a, &b, 1);
    if (is_unsigned(type)) {
        if (right->type == EINTCONST) {
            if (CInt64_Equal(right->data.intval, cint64_zero)) {
                CError_Warning(40);
                *out = a;
                return;
            }
            if (CInt64_Equal(right->data.intval, cint64_one)) {
                *out = a;
                return;
            }
            if ((n = const_log2(right->data.intval)) > 0) {
                func_00494360(&a, type, n, out);
                return;
            }
            if (!copts.optimizesize && !IS_TYPE_LL(type)) {
                *out = func_0049A5C0(&a, right->data.intval.lo);
                return;
            }
            if (b.kind == 3 && b.kind) {
                func_004A64C0(&b, right->rtype, 0);
            }
        }
        op = IS_TYPE_LL(type) ? MACRO_OP(0x33) : MACRO_OP(0x38);
        divop = IS_TYPE_LL(type) ? 0x7B : 0x7D;
    } else {
        if (right->type == EINTCONST) {
            if (CInt64_Equal(right->data.intval, cint64_zero)) {
                CError_Warning(40);
                *out = a;
                return;
            }
            if (CInt64_Equal(right->data.intval, cint64_one)) {
                *out = a;
                return;
            }
            if (CInt64_Equal(right->data.intval, cint64_negone)) {
#if !MWCCPS2 // b210 negates a long long in one register
                if (IS_TYPE_LL(type)) {
                    CError_FATAL(1018);
                }
#endif
                *out = func_0047F900(IS_TYPE_LL(type) ? 0x83 : 0xB9, &a, 0);
                return;
            }
            n = const_log2(right->data.intval);
            if (n > 0 && n < 16 && func_00494170(&a, n, 0, out, type)) {
                return;
            }
            n = const_log2(CInt64_Neg(right->data.intval));
            if (n > 0 && n < 16 && func_00494170(&a, n, 1, out, type)) {
                return;
            }
            if (!copts.optimizesize && !IS_TYPE_LL(type)) {
                *out = func_0049A430(&a, right->data.intval.lo);
                return;
            }
            if (b.kind == 3 && b.kind) {
                func_004A64C0(&b, right->rtype, 0);
            }
        }
        op = IS_TYPE_LL(type) ? data_00625E54 : data_00625E42;
        divop = IS_TYPE_LL(type) ? 0x7A : 0x7C;
    }

#if MWCCPS2
    // b210: a long long divides in the 64-bit registers unless copts+0x25 asks for the library
    if (IS_TYPE_LL(type) && copts.mips_option25) {
#else
    if (IS_TYPE_LL(type) || (IS_TYPE_LL(type) && copts.mips_option25)) {
#endif
        func_004A47E0(&a, 1, 1);
        func_004A47E0(&b, 1, 2);
        func_004A4A20(is_unsigned(type) ? rt_udivdi3 : rt_divdi3);
        *out = func_004A46E0(1);
        return;
    }
    if (copts.mips_option26) {
        func_004A47E0(&a, 0, 1);
        func_004A47E0(&b, 0, 2);
        func_004A4A20(is_unsigned(type) ? rt_l_divu : rt_l_divs);
        *out = func_004A46E0(0);
        return;
    }
    if (copts.divbyzerocheck) {
        // div; bnez divisor, ok; break 7; ok: mflo
        ok = makepclabel();
        trap = makepclabel();
        emitpcode(divop, a.reg, b.reg);
        emitpcode(0x2A, b.reg, ok);
        pcbranch(data_006246B4, ok, 0);
        pcbranch(data_006246B4, trap, 0);
        makepcblock();
        pclabel(data_006246B4, trap);
        emitpcode(0x3E, 7);
        pcbranch(data_006246B4, ok, 0);
        makepcblock();
        pclabel(data_006246B4, ok);
        emitpcode(0xA9, data_00625A98[0]++);
    } else {
        emitpcode(op, data_00625A98[0]++, a.reg, b.reg);
    }
    *out = func_004C3B10(data_006246B4->last->args[0]);
}

extern Object* rt_moddi3;
extern Object* rt_umoddi3;
extern VOperand func_00479E40(
    UInt16 op, VOperand* src, CInt64 value
); // an operation with a constant

// EMODULO
void func_00499200(ENode* expr, SInt16 target, VOperand* out) {
    ENode* left;
    ENode* right;
    Type* type;
    SInt32 n;
    SInt16 divop;
    CLabel* label;
    MPCodeLabel* ok;
    MPCodeLabel* trap;
    CInt64 m;
    VOperand a;
    VOperand b;
    VOperand tmp;
    VOperand t;

    left = expr->data.diadic.left;
    type = expr->rtype;
    right = expr->data.diadic.right;
#if MWCCPS2
    // b210 (0x4A42F0): the library only with copts+0x25, as EDIV
    if (IS_TYPE_LL(type) && copts.mips_option25) {
#else
    if (IS_TYPE_LL(type) || (IS_TYPE_LL(type) && copts.mips_option25)) {
#endif
        func_0049D8D0(left, right, &a, &b, 1);
        func_004A47E0(&a, 1, 1);
        func_004A47E0(&b, 1, 2);
        func_004A4A20(is_unsigned(type) ? rt_umoddi3 : rt_moddi3);
        *out = func_004A46E0(1);
        return;
    }
    func_0049D8D0(left, right, &a, &b, 1);
    if (IS_TYPE_LL(type)) {
        divop = is_unsigned(type) ? 0x7B : 0x7A;
    } else {
        divop = is_unsigned(type) ? 0x7D : 0x7C;
    }
    if (right->type == EINTCONST) {
        if ((n = const_log2(right->data.intval)) > 0) {
            // a power of two: mask; a negative signed value needs a correction
            m = CInt64_Sub(right->data.intval, cint64_one);
            if (m.hi != -1) {
                tmp = func_00479E40(MACRO_OP(0x02), &a, m);
            } else {
                tmp = a;
            }
            if (is_unsigned(left->rtype)) {
                *out = tmp;
                return;
            }
            label = newlabel();
            label->pclabel = (struct PCodeLabel*)makepclabel();
            func_00479540(0x1C, &a, label);
            func_00479540(0x1A, &tmp, label);
            t = func_00479E40(
                IS_TYPE_LL(type) ? 0x78 : 3, &tmp, CInt64_Sub(CInt64_Neg(m), cint64_one)
            );
            emitpcode(data_00625E48, tmp.reg, t.reg);
            func_0046FA90((MPCodeLabel*)label->pclabel);
            *out = func_0047F9A0(&tmp);
            return;
        }
        emitpcode(divop, a.reg, b.reg);
        emitpcode(0xA8, data_00625A98[0]++);
        *out = func_004C3B10(data_006246B4->last->args[0]);
        return;
    }
    emitpcode(divop, a.reg, b.reg);
    if (copts.divbyzerocheck) {
        ok = makepclabel();
        trap = makepclabel();
        emitpcode(0x2A, b.reg, ok);
        pcbranch(data_006246B4, ok, 0);
        pcbranch(data_006246B4, trap, 0);
        makepcblock();
        pclabel(data_006246B4, trap);
        emitpcode(0x3E, 7);
        pcbranch(data_006246B4, ok, 0);
        makepcblock();
        pclabel(data_006246B4, ok);
    }
    emitpcode(0xA8, data_00625A98[0]++);
    *out = func_004C3B10(data_006246B4->last->args[0]);
}

extern VOperand func_0047F660(SInt32 value); // a constant in a new register

// ++/--: the old value in *old, the new one in *updated (already stored).
void func_0049B930(ENode* expr, VOperand* old, VOperand* updated) {
    ENode* addr;
    Type* type;
    Type* bitfield;
    SInt32 delta;
    SInt32 f;
    SInt16 op;
    Boolean inc;
    Object* rt;
    CInt64 c;
    VOperand v;
    VOperand loc;
    VOperand one;
    VOperand t;

    addr = expr->data.monadic->data.monadic;
    type = expr->rtype;
    bitfield = NULL;
    if (type->type == TYPEFLOAT) {
        data_005D0140[addr->type](addr, NULL, &v);
        func_004A72B0(&v, addr);
        loc = v;
        *old = v;
        if (old->kind) {
            func_004A64C0(old, type, 1);
        }
        inc = expr->type == EPOSTINC || expr->type == EPREINC;
        if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
            if (IS_SINGLE(type)) {
                one = func_0047F660(0x3F800000);
            } else {
#if MWCCPS2 // b210: 1.0 as a 64-bit constant in one register
                c.hi = 0;
                c.mid[0] = 0;
                c.mid[1] = 0x3FF00000;
                c.lo = 0;
                one = func_0047F9F0(c, TYPE(&stsignedlonglong));
#else
                one = func_004C3BA0(0, 0);
                t = func_0047F660(0x3FF00000);
                one.regHi = t.reg;
#endif
            }
            func_004A47E0(old, IS_DOUBLE(type), 1);
            func_004A47E0(&one, IS_DOUBLE(type), 2);
            if (inc) {
                rt = IS_SINGLE(type) ? rt_f_add : rt_adddf3;
            } else {
                rt = IS_SINGLE(type) ? rt_f_sub : rt_subdf3;
            }
            func_004A4A20(rt);
            *updated = func_004A4710(type);
        } else {
            one = func_0047F660(0x3F800000);
            f = data_00625A98[1]++;
            emitpcode(0xAD, one.reg, f);
            setpcodeflags(0x4000);
            if (IS_SINGLE(type)) {
                op = inc ? 0xED : 0x103;
            } else {
                emitpcode(0x6D, delta = data_00625A98[1]++, f);
                f = delta;
                op = inc ? 0xEC : 0x102;
            }
            emitpcode(op, data_00625A98[1]++, old->reg, f);
            *updated = func_004C3B10(data_006246B4->last->args[0]);
        }
        func_004A5A90(*updated, &loc, type);
        return;
    }

    if (type->type == TYPEPOINTER) {
        if (expr->type == EPOSTINC || expr->type == EPREINC) {
            delta = TPTR_TARGET(type)->size;
        } else {
            delta = -TPTR_TARGET(type)->size;
        }
    } else if (expr->type == EPOSTINC || expr->type == EPREINC) {
        delta = 1;
    } else {
        delta = -1;
    }
    if (addr->type == EBITFIELD) {
        bitfield = addr->rtype;
        addr = addr->data.monadic;
    }
    data_005D0140[addr->type](addr, NULL, &v);
    func_004A72B0(&v, addr);
    loc = v;
    if (v.kind) {
        func_004A64C0(&v, type, 1);
    }
    *old = v;
    if (bitfield) {
        func_004A5730(*old, (PtrInt)bitfield, old);
    }
    CInt64_SetLong(&c, delta);
#if MWCCPS2 // b210 calls the constant add directly (func_0049C060 inlined)
    *updated = func_0047ECC0(old, &c, type);
#else
    *updated = func_0049C060(type, old, c);
#endif
    if (bitfield) {
        func_004A5380(*updated, &v, (PtrInt)bitfield);
        func_004A5A90(v, &loc, type);
    } else {
        func_004A5A90(*updated, &loc, type);
    }
}

// A floating-point operation; division may become reciprocal and multiply.
void func_00493FA0(SInt16 op, ENode* left, ENode* right, VOperand* out) {
    Boolean one;
    SInt32 r;
    VOperand a;
    VOperand b;

    one = 0;
    if (left->type == EFLOATCONST) {
        one = CMach_FloatIsOne(left->data.floatval);
    }
    func_0049D8D0(left, right, &a, &b, 0);
    if (a.kind) {
        func_004A64C0(&a, left->rtype, 0);
    }
    if (b.kind) {
        func_004A64C0(&b, right->rtype, 0);
    }
    if (copts.mips_isa >= 4 &&
        copts.mips_option11 &&
        (op == 0xF2 || op == 0xF3) &&
        !copts.optimizesize) {
        emitpcode(op == 0xF3 ? 0x123 : 0x122, r = data_00625A98[1]++, b.reg);
        if (!one) {
            emitpcode(op == 0xF3 ? 0xB5 : 0xB4, data_00625A98[1]++, a.reg, r);
        }
    } else {
        emitpcode(op, data_00625A98[1]++, a.reg, b.reg);
    }
    *out = func_004C3B10(data_006246B4->last->args[0]);
}

extern SInt32 func_0047F680(SInt32 value); // a constant in a new register
extern VOperand func_00479F10(VOperand* src);

// EMONMIN
void func_0049B1D0(ENode* expr, void* target, VOperand* out) {
    ENode* sub;
    Type* type;
    SInt16 op;
    SInt32 n;
    VOperand v;
    VOperand hi;
    VOperand zero;
    VOperand borrow;
    VOperand lo;
#if MWCCPS2
    CInt64 c;
    VOperand k;
#endif

    sub = expr->data.monadic;
    type = expr->rtype;
    if (type->type == TYPEFLOAT) {
        if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
            // flip the sign bit
            data_005D0140[sub->type](sub, NULL, out);
            if ((out->kind >= 4 || out->kind == 2) && out->kind) {
                func_004A64C0(out, sub->rtype, 0);
            }
            if (copts.fastmath && out->kind == 1) {
                *out = func_00479F10(out);
            }
            if (IS_SINGLE(type)) {
#if MWCCPSP_NATIVE // 3.0.1 evaluates arguments right to left
                {
                    SInt32 k = func_0047F680(0x80000000);
                    emitpcode(0xE4, data_00625A98[0]++, out->reg, k);
                }
#else
                emitpcode(0xE4, data_00625A98[0]++, out->reg, func_0047F680(0x80000000));
#endif
                *out = func_004C3B10(data_006246B4->last->args[0]);
            } else {
#if MWCCPS2 // b210: the double is one 64-bit register; xor its bit 63
                c.hi = 0;
                c.mid[0] = 0;
                c.mid[1] = 0x80000000;
                c.lo = 0;
                k = func_0047F9F0(c, TYPE(&stunsignedlonglong));
                emitpcode(0xE4, data_00625A98[0]++, out->reg, k.reg);
                *out = func_004C3B10(data_006246B4->last->args[0]);
#elif MWCCPSP_NATIVE // 3.0.1 evaluates arguments right to left
                {
                    SInt32 k = func_0047F680(0x80000000);
                    emitpcode(0xE4, n = data_00625A98[0]++, out->regHi, k);
                }
#else
                emitpcode(0xE4, n = data_00625A98[0]++, out->regHi, func_0047F680(0x80000000));
#endif
#if !MWCCPS2
                out->regHi = n;
#endif
            }
            return;
        }
        op = IS_SINGLE(type) ? 0xFB : 0xFA;
        data_005D0140[sub->type](sub, NULL, &v);
        if (v.kind) {
            func_004A64C0(&v, sub->rtype, 0);
        }
        emitpcode(op, data_00625A98[1]++, v.reg);
        *out = func_004C3B10(data_006246B4->last->args[0]);
        return;
    }
#if !MWCCPS2 // b210 negates a long long in one register (dneg below)
    if (IS_TYPE_LL(type)) {
        data_005D0140[sub->type](sub, NULL, &v);
        if ((v.kind >= 4 || v.kind == 2) && v.kind) {
            func_004A64C0(&v, sub->rtype, 0);
        }
        hi = func_004A7390(&v);
        zero = func_004C3BA0(0, 0);
        borrow = func_0047F740(0xD7, &zero, &v, 0);
        lo = func_0047F740(0xDD, &zero, &v, 0);
        hi = func_0047F740(0xDD, &zero, &hi, 0);
        hi = func_0047F740(0xDD, &hi, &borrow, 0);
        *out = lo;
        out->kind = 0;
        out->regHi = hi.reg;
        return;
    }
#endif
    op = IS_TYPE_LL(type) ? 0x83 : 0xB9;
    data_005D0140[sub->type](sub, NULL, &v);
    if (v.kind) {
        func_004A64C0(&v, sub->rtype, 0);
    }
    emitpcode(op, data_00625A98[0]++, v.reg);
    *out = func_004C3B10(data_006246B4->last->args[0]);
}

// --- conditionalstatement ---------------------------------------------------

extern UInt8 data_00625E83; // something branches to the return label
extern CLabel* data_006242A4; // returnlabel
extern void func_00479430(
    UInt16 op, VOperand* a, VOperand* b, CLabel* label
); // compare two registers and branch
extern void func_00479310(
    UInt16 op, VOperand* a, SInt32 imm, CLabel* label
); // compare with an immediate and branch
extern void func_00479640(UInt16 op, CLabel* label);
extern void func_00479A50(UInt16 op, CLabel* label); // branch on the FP condition
extern void func_0047A170(VOperand* a, CInt64* imm, Type* type, CLabel* label);
extern void func_0047A220(VOperand* a, CInt64* imm, Type* type, CLabel* label);
extern void func_0047A850(VOperand* a, CInt64* imm, Type* type, CLabel* label);
extern void func_0047AE80(VOperand* a, VOperand* b, CLabel* label);
extern void func_0047B180(VOperand* a, VOperand* b, CLabel* label);
extern void func_0047B480(VOperand* a, SInt32 imm, CLabel* label);
extern void func_0047B6A0(VOperand* a, SInt32 imm, CLabel* label);
extern void func_0047B8C0(VOperand* a, SInt32 imm, CLabel* label);
extern void func_0047BAF0(VOperand* a, SInt32 imm, CLabel* label);
extern void func_0047BD20(VOperand* a, SInt32 imm, CLabel* label);
extern void func_0047BF50(VOperand* a, SInt32 imm, CLabel* label);
extern void func_0047C290(VOperand* a, SInt32 imm, CLabel* label);
Object* func_0049D400(int op, Type* type); // the soft-float comparison routine
void func_0049D530(
    int op, Type* type, SInt16* brop, SInt16* cmpop, Boolean truth
); // FP compare and branch opcodes

// Branch to label when cond is (truth ? true : false).
void func_0049C250(ENode* cond, CLabel* label, Boolean truth) {
    ENode* left;
    ENode* right;
    ENode* tmpnode;
    Type* type;
    Boolean isfloat;
    Boolean uns;
    Boolean iszero;
    Boolean isone;
    UInt8 op;
    int binary;
    SInt32 hi;
    SInt32 lo;
    SInt16 brop;
    SInt16 cmpop;
    CLabel* skip;
    VOperand a;
    VOperand b;
    VOperand tmp;
    VOperand r;
    VOperand ahi;
    VOperand bhi;

    left = cond->data.diadic.left;
    right = cond->data.diadic.right;
    type = cond->rtype;
    isfloat = left->rtype->type == TYPEFLOAT;
    if (data_006242A4 == label) {
        data_00625E83 = 1;
    }
    uns = is_unsigned(left->rtype) || is_unsigned(right->rtype);
    op = cond->type;
    b.kind = 3;
    b.imm = cint64_zero;
    binary = 1;
    if (!truth && !isfloat) {
        // branch on the opposite condition instead
        truth = 1;
        switch (op) {
            case EPOSTINC:
            case EPOSTDEC:
            case EPREINC:
            case EPREDEC:
                op = EEQU;
                binary = 0;
                break;
            case ELESS:
                op = EGREATEREQU;
                break;
            case EGREATER:
                op = ELESSEQU;
                break;
            case ELESSEQU:
                op = EGREATER;
                break;
            case EGREATEREQU:
                op = ELESS;
                break;
            case EEQU:
                op = ENOTEQU;
                break;
            case ENOTEQU:
                op = EEQU;
                break;
            default:
                CError_FATAL(516);
        }
    } else {
        switch (op) {
            case EPOSTINC:
            case EPOSTDEC:
            case EPREINC:
            case EPREDEC:
                op = ENOTEQU;
                binary = 0;
                break;
        }
    }
    if (!label->pclabel) {
        label->pclabel = (struct PCodeLabel*)makepclabel();
    }
    CError_ASSERT(537, type->type != TYPEFLOAT);

    if (right->rtype->type == TYPEFLOAT) {
        if (binary) {
            func_0049D8D0(left, right, &a, &b, 1);
        } else {
            data_005D0140[cond->type](cond, NULL, &a);
        }
        if (right->rtype->type == TYPEFLOAT && IS_SOFTFLOAT(right->rtype)) {
            func_004A4890(&a, right->rtype, 1);
            func_004A4890(&b, right->rtype, 2);
            func_004A4A20(func_0049D400(op, right->rtype));
            r = func_004A46E0(0);
            func_00479540(truth ? 0x2A : 0x1A, &r, label);
        } else {
            func_0049D530(op, right->rtype, &brop, &cmpop, truth);
            emitpcode(cmpop, a.reg, b.reg);
            func_00479A50(brop, label);
        }
        return;
    }

    data_005D0140[left->type](left, NULL, &a);
    if (a.kind != 3 && a.kind) {
        func_004A64C0(&a, left->rtype, 0);
    }
    data_005D0140[right->type](right, NULL, &b);
    if (b.kind != 3 && b.kind) {
        func_004A64C0(&b, right->rtype, 0);
    }
    if (a.kind == 3 && b.kind == 3 && a.kind) {
        func_004A64C0(&a, left->rtype, 0);
    }
#if MWCCPS2 && MWCCPS2_BUILD < 210 // b145 moves only a constant to the right
    if (a.kind == 3) {
#else
    if (a.kind == 3 || (!IS_TYPE_LL(left->rtype) && a.kind == 0 && a.reg == 0 && b.kind != 3)) {
#endif
        // keep a constant or $zero on the right
        tmpnode = left;
        left = right;
        right = tmpnode;
        tmp = a;
        a = b;
        b = tmp;
        switch (op) {
            case ELESS:
                op = EGREATER;
                break;
            case EGREATER:
                op = ELESS;
                break;
            case ELESSEQU:
                op = EGREATEREQU;
                break;
            case EGREATEREQU:
                op = ELESSEQU;
                break;
            case EEQU:
            case ENOTEQU:
                break;
            default:
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -8 // b145 asserts
                CError_FATAL(599);
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
        }
    }

    if (b.kind == 3 && !IS_TYPE_LL(left->rtype)) {
        // comparisons with the extremes
        lo = b.imm.lo;
        switch (op) {
            case ELESS:
                if (uns) {
                    if (lo == -1) {
                        op = ENOTEQU;
                    }
                } else if (lo == (SInt32)0x80000000) {
                    return;
                }
                break;
            case EGREATER:
                if (uns) {
                    if (lo == -1) {
                        return;
                    }
                } else if (lo == 0x7FFFFFFF) {
                    return;
                }
                break;
            case ELESSEQU:
                if (uns) {
                    if (lo == -1) {
                        func_00480710((MPCodeLabel*)label->pclabel);
                        return;
                    }
                } else if (lo == 0x7FFFFFFF) {
                    func_00480710((MPCodeLabel*)label->pclabel);
                    return;
                } else if (lo == (SInt32)0x80000000) {
                    op = EEQU;
                }
                break;
            case EGREATEREQU:
                if (uns) {
                    if (lo == -1) {
                        op = EEQU;
                    }
                } else if (lo == (SInt32)0x80000000) {
                    func_00480710((MPCodeLabel*)label->pclabel);
                } else if (lo == 0x7FFFFFFF) {
                    op = EEQU;
                }
                break;
        }
#if MWCCPS2 // b210 sign-extends the (int) constant through all 128 bits
        CInt64_SetLong(&b.imm, lo);
#endif
    }

#if !MWCCPS2 // b210: a long long is one 64-bit register, compared like an int
    if (IS_TYPE_LL(left->rtype)) {
        // compare the high words, then the low words
        skip = newlabel();
        skip->pclabel = (struct PCodeLabel*)makepclabel();
        ahi = func_004A7390(&a);
        switch (op) {
            case ELESS:
                if (b.kind == 3) {
                    hi = b.imm.hi;
                    lo = b.imm.lo;
                    if (hi == 0 && lo == 0 && !uns) {
                        func_00479540(0x24, &ahi, label);
                    } else {
                        if (uns) {
                            func_0047B8C0(&ahi, hi, label);
                            func_0047BAF0(&ahi, hi, skip);
                        } else {
                            func_0047BF50(&ahi, hi, label);
                            func_0047BD20(&ahi, hi, skip);
                        }
                        func_0047B8C0(&a, lo, label);
                    }
                } else {
                    bhi = func_004A7390(&b);
                    func_00479430(uns ? MACRO_OP(0x21) : MACRO_OP(0x1D), &ahi, &bhi, label);
                    func_00479430(uns ? MACRO_OP(0x11) : MACRO_OP(0x0D), &ahi, &bhi, skip);
                    func_00479430(MACRO_OP(0x21), &a, &b, label);
                }
                break;
            case EGREATER:
                if (b.kind == 3) {
                    hi = b.imm.hi;
                    lo = b.imm.lo;
                    if (uns) {
                        func_0047BAF0(&ahi, hi, label);
                        func_0047B8C0(&ahi, hi, skip);
                    } else {
                        func_0047BD20(&ahi, hi, label);
                        func_0047BF50(&ahi, hi, skip);
                    }
                    func_0047BAF0(&a, lo, label);
                } else {
                    bhi = func_004A7390(&b);
                    func_00479430(uns ? MACRO_OP(0x11) : MACRO_OP(0x0D), &ahi, &bhi, label);
                    func_00479430(uns ? MACRO_OP(0x21) : MACRO_OP(0x1D), &ahi, &bhi, skip);
                    func_00479430(MACRO_OP(0x11), &a, &b, label);
                }
                break;
            case ELESSEQU:
                if (b.kind == 3) {
                    hi = b.imm.hi;
                    lo = b.imm.lo;
                    if (uns) {
                        func_0047B8C0(&ahi, hi, label);
                        func_0047BAF0(&ahi, hi, skip);
                    } else {
                        func_0047BF50(&ahi, hi, label);
                        func_0047BD20(&ahi, hi, skip);
                    }
                    func_0047C290(&a, lo, label);
                } else {
                    bhi = func_004A7390(&b);
                    func_00479430(uns ? MACRO_OP(0x21) : MACRO_OP(0x1D), &ahi, &bhi, label);
                    func_00479430(uns ? MACRO_OP(0x11) : MACRO_OP(0x0D), &ahi, &bhi, skip);
                    func_00479430(MACRO_OP(0x19), &a, &b, label);
                }
                break;
            case EGREATEREQU:
                if (b.kind == 3) {
                    hi = b.imm.hi;
                    lo = b.imm.lo;
                    if (uns) {
                        func_0047BAF0(&ahi, hi, label);
                        func_0047B8C0(&ahi, hi, skip);
                    } else {
                        func_0047BD20(&ahi, hi, label);
                        func_0047BF50(&ahi, hi, skip);
                    }
                    func_00479310(MACRO_OP(0x0B), &a, lo, label);
                } else {
                    bhi = func_004A7390(&b);
                    func_00479430(uns ? MACRO_OP(0x11) : MACRO_OP(0x0D), &ahi, &bhi, label);
                    func_00479430(uns ? MACRO_OP(0x21) : MACRO_OP(0x1D), &ahi, &bhi, skip);
                    func_00479430(MACRO_OP(0x09), &a, &b, label);
                }
                break;
            case EEQU:
                if (b.kind == 3) {
                    lo = b.imm.lo;
                    func_0047B480(&ahi, b.imm.hi, skip);
                    func_0047B6A0(&a, lo, label);
                } else {
                    bhi = func_004A7390(&b);
                    func_00479430(0x28, &ahi, &bhi, skip);
                    func_00479430(0x18, &a, &b, label);
                }
                break;
            case ENOTEQU:
                if (b.kind == 3) {
                    lo = b.imm.lo;
                    func_0047B480(&ahi, b.imm.hi, label);
                    func_0047B480(&a, lo, label);
                } else {
                    bhi = func_004A7390(&b);
                    func_00479430(0x28, &ahi, &bhi, label);
                    func_00479430(0x28, &a, &b, label);
                }
                break;
            default:
                CError_FATAL(853);
        }
        func_0046FA90((MPCodeLabel*)skip->pclabel);
        return;
    }
#endif

    if (b.kind == 3) {
#if MWCCPS2 // b210 tests the 64-bit value: words 2 and 3 of the constant
        hi = b.imm.mid[1];
#else
        hi = b.imm.hi;
#endif
        lo = b.imm.lo;
        iszero = hi == 0 && lo == 0;
        isone = hi == 0 && lo == 1;
    } else {
        iszero = b.reg == 0 ? 1 : 0;
        isone = 0;
    }
    switch (op) {
        case ELESS:
            if (iszero) {
                if (!uns) {
                    func_00479540(0x24, &a, label);
                }
            } else if (isone) {
                func_00479540(uns ? 0x1A : 0x22, &a, label);
            } else if (b.kind == 3) {
                func_0047A170(&a, &b.imm, left->rtype, label);
            } else if (uns) {
                func_0047AE80(&a, &b, label);
            } else {
                func_0047B180(&a, &b, label);
            }
            break;
        case EGREATER:
            if (iszero) {
                func_00479540(uns ? 0x2A : 0x20, &a, label);
            } else {
                if (b.kind == 3) {
                    if (lo == -1) {
                        if (hi == 0x7FFFFFFF && !uns) {
                            return;
                        }
                        if (hi == -1 && uns) {
                            return;
                        }
                    }
                    brop = uns ? MACRO_OP(0x13) : MACRO_OP(0x0F);
                } else {
                    brop = uns ? MACRO_OP(0x11) : MACRO_OP(0x0D);
                }
                func_00479430(brop, &a, &b, label);
            }
            break;
        case ELESSEQU:
            if (iszero) {
                func_00479540(uns ? 0x1A : 0x22, &a, label);
            } else {
                if (b.kind == 3) {
                    if (lo == -1) {
                        if (hi == -1 && uns) {
                            func_00480710((MPCodeLabel*)label->pclabel);
                            return;
                        }
                        if (hi == 0x7FFFFFFF && !uns) {
                            func_00480710((MPCodeLabel*)label->pclabel);
                            return;
                        }
                    }
                    if (hi == (SInt32)0x80000000 && lo == 0 && !uns) {
                        func_0047A850(&a, &b.imm, right->rtype, label);
                        return;
                    }
                    brop = uns ? MACRO_OP(0x1B) : MACRO_OP(0x17);
                } else {
                    brop = uns ? MACRO_OP(0x19) : MACRO_OP(0x15);
                }
                func_00479430(brop, &a, &b, label);
            }
            break;
        case EGREATEREQU:
            if (iszero) {
                if (!uns) {
                    func_00479540(0x1C, &a, label);
                } else {
                    func_00479640(0x39, label);
                }
            } else if (isone) {
                func_00479540(uns ? 0x2A : 0x20, &a, label);
            } else {
                if (b.kind == 3) {
                    brop = uns ? MACRO_OP(0x0B) : MACRO_OP(0x07);
                } else {
                    brop = uns ? MACRO_OP(0x09) : MACRO_OP(0x05);
                }
                func_00479430(brop, &a, &b, label);
            }
            break;
        case EEQU:
#if MWCCPS2 && MWCCPS2_BUILD < 210 // b145 has no compare-with-zero branch here
            if (b.kind == 3) {
#else
            if (iszero) {
                func_00479540(0x1A, &a, label);
            } else if (b.kind == 3) {
#endif
                func_0047A850(&a, &b.imm, right->rtype, label);
            } else {
                func_00479430(0x18, &a, &b, label);
            }
            break;
        case ENOTEQU:
#if MWCCPS2 && MWCCPS2_BUILD < 210
            if (b.kind == 3) {
#else
            if (iszero) {
                func_00479540(0x2A, &a, label);
            } else if (b.kind == 3) {
#endif
                func_0047A220(&a, &b.imm, right->rtype, label);
            } else {
                func_00479430(0x28, &a, &b, label);
            }
            break;
        default:
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -34 // b145 asserts
            CError_FATAL(989);
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
    }
}

// --- division by constants ---------------------------------------------------

typedef struct SMagic {
    SInt32 magic;
    SInt32 shift;
} SMagic;

typedef struct UMagic {
    UInt32 magic;
    SInt32 shift;
    SInt32 add;
} UMagic;

extern SMagic func_0053EE10(SInt32 d); // magic number for signed division
extern UMagic func_0053ECC0(UInt32 d); // magic number for unsigned division
extern VOperand func_00479D10(UInt16 op, VOperand* src, SInt32 shift); // shift by a constant

// src / d (signed) with a multiply by the magic number.
VOperand func_0049A430(VOperand* src, SInt32 d) {
    SMagic m;
    VOperand q;
    VOperand sign;

    m = func_0053EE10(d);
    emitpcode(0xB6, func_0047F680(m.magic), src->reg);
    if (d > 0) {
        sign = func_00479D10(0xDA, src, 31);
    }
    emitpcode(0xA8, data_00625A98[0]++);
    q = func_004C3B10(data_006246B4->last->args[0]);
    if (d > 0 && m.magic < 0) {
        q = func_0047F740(4, &q, src, 0);
    } else if (d < 0 && m.magic > 0) {
        q = func_0047F740(0xDD, &q, src, 0);
    }
    if (m.shift) {
        q = func_00479D10(0xD8, &q, m.shift);
    }
    if (d < 0) {
        sign = func_00479D10(0xDA, &q, 31);
    }
    q = func_0047F740(4, &q, &sign, 0);
    return q;
}

// src / d (unsigned) with a multiply by the magic number.
VOperand func_0049A5C0(VOperand* src, UInt32 d) {
    UMagic m;
    VOperand q;
    VOperand t;

    m = func_0053ECC0(d);
    emitpcode(0xB7, func_0047F680(m.magic), src->reg);
    emitpcode(0xA8, data_00625A98[0]++);
    q = func_004C3B10(data_006246B4->last->args[0]);
    if (!m.add) {
        if (m.shift) {
            q = func_00479D10(0xDA, &q, m.shift);
        }
        return q;
    }
    t = func_0047F740(0xDD, src, &q, 0);
    t = func_00479D10(0xDA, &t, 1);
    q = func_0047F740(4, &t, &q, 0);
    if (m.shift - 1 != 0) {
        q = func_00479D10(0xDA, &q, m.shift - 1);
    } else {
        q = t;
    }
    return q;
}

extern Object* rt_f_flt;
extern Object* rt_f_fgt;
extern Object* rt_f_fle;
extern Object* rt_f_fge;
extern Object* rt_f_feq;
extern Object* rt_f_fne;
extern Object* rt_dpflt;
extern Object* rt_dpfgt;
extern Object* rt_dpfle;
extern Object* rt_dpfge;
extern Object* rt_dpfeq;
extern Object* rt_dpfne;

// The soft-float routine for a comparison.
Object* func_0049D400(int op, Type* type) {
    Boolean single;

    single = IS_SINGLE(type);
    CError_ASSERT(456, !(type->type == TYPEFLOAT && IS_HARDFLOAT(type)));
    switch (op) {
        case EGREATER:
            return single ? rt_f_fgt : rt_dpfgt;
        case ELESS:
            return single ? rt_f_flt : rt_dpflt;
        case EGREATEREQU:
            return single ? rt_f_fge : rt_dpfge;
        case ELESSEQU:
            return single ? rt_f_fle : rt_dpfle;
        case EEQU:
            return single ? rt_f_feq : rt_dpfeq;
        case ENOTEQU:
            return single ? rt_f_fne : rt_dpfne;
        default:
            CError_FATAL(465);
            return NULL;
    }
}

// The FPU compare (c.cond.fmt) and branch (bc1t/bc1f) for a comparison.
void func_0049D530(int op, Type* type, SInt16* brop, SInt16* cmpop, Boolean truth) {
    Boolean single;

    single = IS_SINGLE(type);
    CError_ASSERT(403, !(type->type == TYPEFLOAT && IS_SOFTFLOAT(type)));
    switch (op) {
        case EGREATER:
#if MWCCPS2 // b210 (0x4A8EC0): a > b is !(a <= b) with c.le.s (the R5900 FPU is single precision)
            *cmpop = 0x49;
#else
            *cmpop = single ? 0x53 : 0x52;
#endif
            *brop = truth ? 0xC : 0xE;
            return;
        case ELESS:
            *cmpop = single ? 0x4B : 0x4A;
            *brop = truth ? 0xE : 0xC;
            return;
        case EGREATEREQU:
#if MWCCPS2 // b210: a >= b is !(a < b) with c.lt.s
            *cmpop = 0x4B;
#else
            *cmpop = single ? 0x4D : 0x4C;
#endif
            *brop = truth ? 0xC : 0xE;
            return;
        case ELESSEQU:
            *cmpop = single ? 0x49 : 0x48;
            *brop = truth ? 0xE : 0xC;
            return;
        case EEQU:
            *cmpop = single ? 0x45 : 0x44;
            *brop = truth ? 0xE : 0xC;
            return;
        case ENOTEQU:
            *cmpop = single ? 0x45 : 0x44;
            *brop = truth ? 0xC : 0xE;
            return;
        default:
            CError_FATAL(448);
    }
}

// --- ETYPCON ---------------------------------------------------------------

extern Object* rt_f_ulltof;
extern Object* rt_f_utof;
extern Object* rt_floatdisf;
extern Object* rt_f_itof;
extern Object* rt_d_ulltod;
extern Object* rt_d_utod;
extern Object* rt_floatdidf;
extern Object* rt_floatsidf;
extern Object* rt_extendsfdf2;
extern Object* rt_truncdfsf2;
extern Object* rt_fixunssfdi;
extern Object* rt_fixsfdi;
extern Object* rt_fixunsdfdi;
extern Object* rt_fixdfdi;
extern Object* rt_f_ftou;
extern Object* rt_f_ftoi;
extern Object* rt_fixunsdfsi;
extern Object* rt_fixdfsi;
extern VOperand func_0047A060(VOperand* src); // into a single-precision FPR
extern VOperand func_00479F70(VOperand* src); // into a double-precision FPR
extern void func_00479B00(UInt16 op, MPCodeLabel* label); // FP condition branch

// Is the operand of this conversion a bitfield (which arrives already extended)?
inline Boolean from_bitfield(ENode* sub) {
    if (sub->type == EINDIRECT && sub->data.monadic->type == EBITFIELD) {
        return 1;
    }
    if (sub->type >= EASS &&
        sub->type <= EORASS &&
        sub->data.diadic.left->data.monadic->type == EBITFIELD) {
        return 1;
    }
    if (sub->type <= EPREDEC && sub->data.monadic->data.monadic->type == EBITFIELD) {
        return 1;
    }
    return 0;
}

// Unsigned 32-bit from a float: values of 2^31 and above are reduced first.
static void float_to_uint(
    VOperand* out, VOperand* k, UInt16 trunc, UInt16 cmp, UInt16 sub, SInt32 topbit
) {
    MPCodeLabel* large;
    MPCodeLabel* end;
    SInt32 src;
    SInt32 f;
    SInt32 r;

    large = makepclabel();
    src = out->reg;
    if (copts.mips_flags02 & 0x4000) {
        emitpcode(trunc, f = data_00625A98[1]++, src);
        emitpcode(0xA5, data_00625A98[0]++, f);
        setpcodeflags(0x4000);
        *out = func_004C3B10(data_006246B4->last->args[0]);
        emitpcode(cmp, k->reg, src);
        func_00479B00(0xC, large);
        emitpcode(sub, f = data_00625A98[1]++, src, k->reg);
        emitpcode(trunc, r = data_00625A98[1]++, f);
        emitpcode(0xA5, f = data_00625A98[0]++, r);
        setpcodeflags(0x4000);
        emitpcode(0xBE, out->reg, f, func_0047F680(topbit));
        func_0046FA90(large);
    } else {
        end = makepclabel();
        emitpcode(cmp, k->reg, src);
        func_00479B00(0xE, large);
        emitpcode(trunc, f = data_00625A98[1]++, src);
        emitpcode(0xA5, data_00625A98[0]++, f);
        setpcodeflags(0x4000);
        *out = func_004C3B10(data_006246B4->last->args[0]);
        func_00480710(end);
        func_0046FA90(large);
        emitpcode(sub, f = data_00625A98[1]++, src, k->reg);
        emitpcode(trunc, r = data_00625A98[1]++, f);
        emitpcode(0xA5, f = data_00625A98[0]++, r);
        setpcodeflags(0x4000);
        emitpcode(0xBE, out->reg, f, func_0047F680(topbit));
        func_0046FA90(end);
    }
}

#if MWCCPS2
// mwccps2 b210 (0x49FCD0): 64-bit general registers, so widening to long
// long needs no high register; float <-> long long with the FPU (cvt.l/
// trunc.l, #pragma trunc with fp_library bit 2), and #pragma fast_fptosi /
// fastmath truncate a float without the library.
extern VOperand ps2_tofpr64_487210(VOperand* src); // into a double FPR (dmtc1)
extern VOperand ps2_const128_48E3D0(CInt64 value, Type* type); // load a constant (128-bit CInt64)
extern void ps2_ulltod_4A5C00(Type* to, VOperand* out); // unsigned long long to double
extern void ps2_lltof_4A5810(VOperand* out); // long long to float
extern void ps2_lltod_4A5A10(VOperand* out); // long long to double

// float_to_uint with the constant made after the label (b210 inlines it)
inline void ps2_float_to_uint(
    VOperand* out, Boolean dbl, UInt16 trunc, UInt16 cmp, UInt16 sub, SInt32 topbit
) {
    MPCodeLabel* large;
    MPCodeLabel* end;
    VOperand k;
    CInt64 c;
    SInt32 src;
    SInt32 f;
    SInt32 r;

    large = makepclabel();
    if (dbl) {
        c.hi = 0;
        c.mid[0] = 0;
        c.mid[1] = 0x41E00000;
        c.lo = 0;
        k = ps2_const128_48E3D0(c, TYPE(&stunsignedlonglong));
        k = ps2_tofpr64_487210(&k);
    } else {
        k = func_0047F660(0x4F000000);
        k = func_0047A060(&k);
    }
    if (copts.mips_flags02 & 0x4000) {
        src = out->reg;
        emitpcode(trunc, f = data_00625A98[1]++, src);
        emitpcode(0xA5, data_00625A98[0]++, f);
        setpcodeflags(0x4000);
        *out = func_004C3B10(data_006246B4->last->args[0]);
        emitpcode(cmp, k.reg, src);
        func_00479B00(0xC, large);
        emitpcode(sub, f = data_00625A98[1]++, src, k.reg);
        emitpcode(trunc, r = data_00625A98[1]++, f);
        emitpcode(0xA5, f = data_00625A98[0]++, r);
        setpcodeflags(0x4000);
        emitpcode(0xBE, out->reg, f, func_0047F680(topbit));
        func_0046FA90(large);
    } else {
        end = makepclabel();
        src = out->reg;
        emitpcode(cmp, k.reg, src);
        func_00479B00(0xE, large);
        emitpcode(trunc, f = data_00625A98[1]++, src);
        emitpcode(0xA5, data_00625A98[0]++, f);
        setpcodeflags(0x4000);
        *out = func_004C3B10(data_006246B4->last->args[0]);
        func_00480710(end);
        func_0046FA90(large);
        emitpcode(sub, f = data_00625A98[1]++, src, k.reg);
        emitpcode(trunc, r = data_00625A98[1]++, f);
        emitpcode(0xA5, f = data_00625A98[0]++, r);
        setpcodeflags(0x4000);
        emitpcode(0xBE, out->reg, f, func_0047F680(topbit));
        func_0046FA90(end);
    }
}

// ETYPCON: a conversion.
void func_00495500(ENode* expr, void* target, VOperand* out) {
    ENode* sub;
    Type* from;
    Type* to;
    SInt32 f;
    SInt32 r;
    CLabel* neg;
    CLabel* done;
    MPCodeLabel* big;
    MPCodeLabel* fin;
    Object* rt;
    VOperand tmp;
    VOperand k;

    sub = expr->data.monadic;
    from = sub->rtype;
    to = expr->rtype;
    data_005D0140[sub->type](sub, NULL, out);
    if (to->type == TYPEPOINTER) {
        if (to->size < from->size && out->kind) {
            func_004A64C0(out, from, 0);
        }
    } else if (out->kind) {
        func_004A64C0(out, from, 0);
    }

    if (to->type == TYPEVOID) {
        out->kind = 3;
        out->imm = cint64_zero;
        return;
    }

    if (from->type == TYPEINT || from->type == TYPEENUM) {
        if (to->type == TYPEFLOAT) {
            goto int_to_float;
        }
        if (to->type == TYPEPOINTER) {
            return;
        }
        if (from->size < to->size) {
            // widening: the 64-bit registers hold long long whole
            if (from_bitfield(sub)) {
                return;
            }
            *out = func_00479C30(
                is_unsigned(from) ? MACRO_OP(0x82) : MACRO_OP(0x81), out, from->size * 8
            );
            return;
        }
        if (to->size < from->size) {
            if (to->size == 4) {
                // 32-bit values stay sign-extended in the 64-bit registers
                *out = func_00479C30(MACRO_OP(0x81), out, 32);
                return;
            }
            *out =
                func_00479C30(is_unsigned(to) ? MACRO_OP(0x82) : MACRO_OP(0x81), out, to->size * 8);
            return;
        }
        if (is_unsigned(from) == is_unsigned(to)) {
            return;
        }
        if ((to->type == TYPEINT || to->type == TYPEENUM) && to->size == 8) {
            return;
        }
        if (to->size == 4) {
            return;
        }
        *out = func_00479C30(is_unsigned(to) ? MACRO_OP(0x82) : MACRO_OP(0x81), out, to->size * 8);
        return;
    }
    if (from->type == TYPEPOINTER || (from->type == TYPEMEMBERPOINTER && from->size == 4)) {
        return;
    }

    if (to->type == TYPEFLOAT) {
        if ((to->type == TYPEFLOAT && IS_SOFTFLOAT(to)) ||
            (from->type == TYPEFLOAT && IS_SOFTFLOAT(from))) {
            if (to->size != from->size) {
                func_004A4890(out, from, 1);
                func_004A4A20(IS_DOUBLE(to) ? rt_extendsfdf2 : rt_truncdfsf2);
                *out = func_004A4710(to);
            }
            return;
        }
        if (to->size < from->size) {
            emitpcode(0x71, data_00625A98[1]++, out->reg);
            *out = func_004C3B10(data_006246B4->last->args[0]);
        } else if (to->size > from->size) {
            emitpcode(0x6D, data_00625A98[1]++, out->reg);
            *out = func_004C3B10(data_006246B4->last->args[0]);
        }
        return;
    }

    // float to integer
    if (copts.trunc && (copts.fp_library & 4)) {
        if ((copts.fp_library & 1) && IS_SINGLE(from)) {
            if (IS_TYPE_LL(to) && !is_unsigned(to)) {
                emitpcode(0x105, f = data_00625A98[1]++, out->reg);
                emitpcode(0x7F, data_00625A98[0]++, f);
                *out = func_004C3B10(data_006246B4->last->args[0]);
                return;
            }
            if (!IS_TYPE_LL(to)) {
                if (!is_unsigned(to)) {
                    emitpcode(0x107, f = data_00625A98[1]++, out->reg);
                    emitpcode(0xA5, data_00625A98[0]++, f);
                    setpcodeflags(0x4000);
                    *out = func_004C3B10(data_006246B4->last->args[0]);
                    return;
                }
                if (is_unsigned(to)) {
                    ps2_float_to_uint(out, 0, 0x107, 0x49, 0x103, -0x8000);
                    return;
                }
            }
        }
        if ((copts.fp_library & 2) && IS_DOUBLE(from)) {
            if (IS_TYPE_LL(to) && !is_unsigned(to)) {
                emitpcode(0x104, f = data_00625A98[1]++, out->reg);
                emitpcode(0x7F, data_00625A98[0]++, f);
                *out = func_004C3B10(data_006246B4->last->args[0]);
                return;
            }
            if (!IS_TYPE_LL(to)) {
                if (!is_unsigned(to)) {
                    emitpcode(0x106, f = data_00625A98[1]++, out->reg);
                    emitpcode(0xA5, data_00625A98[0]++, f);
                    setpcodeflags(0x4000);
                    *out = func_004C3B10(data_006246B4->last->args[0]);
                    return;
                }
                if (is_unsigned(to)) {
                    ps2_float_to_uint(out, 1, 0x106, 0x48, 0x102, 0x80000000);
                    return;
                }
            }
        }
    }
    if ((copts.fast_fptosi || copts.fastmath) && IS_SINGLE(from) && !IS_TYPE_LL(to)) {
        if (!is_unsigned(to)) {
            emitpcode(0x75, f = data_00625A98[1]++, out->reg);
            emitpcode(0xA5, r = data_00625A98[0]++, f);
            setpcodeflags(0x4000);
            if (to == TYPE(&stchar) || to == TYPE(&stsignedchar)) {
                emitpcode(MACRO_OP(0x81), data_00625A98[0]++, r, 8);
            } else if (to == TYPE(&stsignedshort)) {
                emitpcode(MACRO_OP(0x81), data_00625A98[0]++, r, 16);
            }
            *out = func_004C3B10(data_006246B4->last->args[0]);
            return;
        }
        big = makepclabel();
        fin = makepclabel();
        k = func_0047F660(0x4F000000);
        k = func_0047A060(&k);
        emitpcode(0xF9, f = data_00625A98[1]++, out->reg);
        emitpcode(0x49, k.reg, f);
        func_00479B00(0xE, fin);
        emitpcode(0x75, f, f);
        emitpcode(0xA5, r = data_00625A98[0]++, f);
        setpcodeflags(0x4000);
        if (to == TYPE(&stunsignedchar)) {
            emitpcode(MACRO_OP(0x82), r, r, 8);
        } else if (to == TYPE(&stunsignedshort)) {
            emitpcode(MACRO_OP(0x82), r, r, 16);
        }
        *out = func_004C3B10(data_006246B4->last->args[0]);
        func_00480710(big);
        func_0046FA90(fin);
        emitpcode(0x103, f, f, k.reg);
        emitpcode(0x75, f, f);
        emitpcode(0xA5, r, f);
        setpcodeflags(0x4000);
        emitpcode(0xBE, r, r, func_0047F680(0x80000000));
        if (to == TYPE(&stunsignedchar)) {
            emitpcode(MACRO_OP(0x82), r, r, 8);
        } else if (to == TYPE(&stunsignedshort)) {
            emitpcode(MACRO_OP(0x82), r, r, 16);
        }
        *out = func_004C3B10(data_006246B4->last->args[0]);
        func_0046FA90(big);
        return;
    }
    func_004A4890(out, from, 1);
    if (IS_TYPE_LL(to)) {
        if (IS_SINGLE(from)) {
            rt = is_unsigned(to) ? rt_fixunssfdi : rt_fixsfdi;
        } else {
            rt = is_unsigned(to) ? rt_fixunsdfdi : rt_fixdfdi;
        }
    } else if (IS_SINGLE(from)) {
        rt = is_unsigned(to) ? rt_f_ftou : rt_f_ftoi;
    } else {
        rt = is_unsigned(to) ? rt_fixunsdfsi : rt_fixdfsi;
    }
    func_004A4A20(rt);
    *out = func_004A46E0(IS_TYPE_LL(to));
    return;

int_to_float:
    if (IS_SINGLE(to)) {
        if (copts.fp_library & 1) {
            if (!is_unsigned(from) && !IS_TYPE_LL(from)) {
                *out = func_0047A060(out);
                *out = func_0047F900(0x73, out, 1);
                return;
            }
            if (is_unsigned(from) && !IS_TYPE_LL(from)) {
                // unsigned: halve (keeping the low bit) and double when the top bit is set
                neg = newlabel();
                done = newlabel();
                f = data_00625A98[1]++;
                neg->pclabel = (struct PCodeLabel*)makepclabel();
                done->pclabel = (struct PCodeLabel*)makepclabel();
                func_00479540(0x24, out, neg);
                tmp = func_0047A060(out);
                emitpcode(0x73, f, tmp.reg);
                func_00480710((MPCodeLabel*)done->pclabel);
                func_0046FA90((MPCodeLabel*)neg->pclabel);
                emitpcode(0xDA, r = data_00625A98[0]++, out->reg, 1);
                tmp = func_0047E650(out, 1);
                emitpcode(0xBE, r, r, tmp.reg);
                tmp.reg = r;
                tmp = func_0047A060(&tmp);
                emitpcode(0x73, f, tmp.reg);
                emitpcode(0xED, f, f, f);
                *out = func_004C3B10(data_006246B4->last->args[0]);
                func_0046FA90((MPCodeLabel*)done->pclabel);
                return;
            }
        }
        func_004A47E0(out, IS_TYPE_LL(from), 1);
        if (is_unsigned(from)) {
            rt = IS_TYPE_LL(from) ? rt_f_ulltof : rt_f_utof;
        } else {
            rt = IS_TYPE_LL(from) ? rt_floatdisf : rt_f_itof;
        }
        func_004A4A20(rt);
        *out = func_004A4710(to);
    } else {
        if (copts.fp_library & 2) {
            if (!is_unsigned(from) && !IS_TYPE_LL(from)) {
                *out = func_0047A060(out);
                *out = func_0047F900(0x6E, out, 1);
                return;
            }
            if (is_unsigned(from) && !IS_TYPE_LL(from)) {
                // unsigned 32-bit: zero-extend, then convert the 64-bit value
                *out = func_00479C30(MACRO_OP(0x82), out, 32);
                *out = ps2_tofpr64_487210(out);
                *out = func_0047F900(0x6C, out, 1);
                return;
            }
            if (is_unsigned(from)) {
                ps2_ulltod_4A5C00(to, out);
            } else if (IS_SINGLE(to)) {
                ps2_lltof_4A5810(out);
            } else {
                ps2_lltod_4A5A10(out);
            }
            return;
        }
        func_004A47E0(out, IS_TYPE_LL(from), 1);
        if (is_unsigned(from)) {
            rt = IS_TYPE_LL(from) ? rt_d_ulltod : rt_d_utod;
        } else {
            rt = IS_TYPE_LL(from) ? rt_floatdidf : rt_floatsidf;
        }
        func_004A4A20(rt);
        *out = func_004A4710(to);
    }
}
#else
// ETYPCON: a conversion.
void func_00495500(ENode* expr, void* target, VOperand* out) {
    ENode* sub;
    Type* from;
    Type* to;
    SInt32 f;
    SInt32 r;
    CLabel* neg;
    CLabel* done;
    Object* rt;
    CInt64 c;
    VOperand tmp;
    VOperand k;

    sub = expr->data.monadic;
    from = sub->rtype;
    to = expr->rtype;
    data_005D0140[sub->type](sub, NULL, out);
    if (to->type == TYPEPOINTER) {
        if (to->size < from->size && out->kind) {
            func_004A64C0(out, from, 0);
        }
    } else if (out->kind) {
        func_004A64C0(out, from, 0);
    }

    if (to->type == TYPEVOID) {
        out->kind = 3;
        out->imm = cint64_zero;
        return;
    }

    if (from->type == TYPEINT || from->type == TYPEENUM) {
        if (to->type == TYPEFLOAT) {
            goto int_to_float;
        }
        if (to->type == TYPEPOINTER) {
            return;
        }
        if (from->size < to->size) {
            // widening
            if ((to->type == TYPEINT || to->type == TYPEENUM) && to->size == 8) {
                if (is_unsigned(to) && is_unsigned(from)) {
                    out->regHi = 0;
                } else {
                    tmp = func_00479D10(0xD8, out, 31);
                    out->regHi = tmp.reg;
                }
                return;
            }
            if (from_bitfield(sub)) {
                return;
            }
            *out = func_00479C30(
                is_unsigned(from) ? MACRO_OP(0x82) : MACRO_OP(0x81), out, from->size * 8
            );
            return;
        }
        if (to->size < from->size) {
            *out =
                func_00479C30(is_unsigned(to) ? MACRO_OP(0x82) : MACRO_OP(0x81), out, to->size * 8);
            return;
        }
        if (is_unsigned(to) == is_unsigned(from)) {
            return;
        }
        if ((to->type == TYPEINT || to->type == TYPEENUM) && to->size == 8) {
            return;
        }
        if (to->size == 4) {
            return;
        }
        *out = func_00479C30(is_unsigned(to) ? MACRO_OP(0x82) : MACRO_OP(0x81), out, to->size * 8);
        return;
    }
    if (from->type == TYPEPOINTER || (from->type == TYPEMEMBERPOINTER && from->size == 4)) {
        return;
    }

    if (to->type == TYPEFLOAT) {
        if ((to->type == TYPEFLOAT && IS_SOFTFLOAT(to)) ||
            (from->type == TYPEFLOAT && IS_SOFTFLOAT(from))) {
            if (to->size != from->size) {
                func_004A4890(out, from, 1);
                func_004A4A20(IS_DOUBLE(to) ? rt_extendsfdf2 : rt_truncdfsf2);
                *out = func_004A4710(to);
            }
            return;
        }
        if (to->size < from->size) {
            emitpcode(0x71, data_00625A98[1]++, out->reg);
            *out = func_004C3B10(data_006246B4->last->args[0]);
        } else if (to->size > from->size) {
            emitpcode(0x6D, data_00625A98[1]++, out->reg);
            *out = func_004C3B10(data_006246B4->last->args[0]);
        }
        return;
    }

    // float to integer
    if (copts.trunc) {
        if ((copts.fp_library & 1) && IS_SINGLE(from)) {
            if (!IS_TYPE_LL(to)) {
                if (!is_unsigned(to)) {
                    emitpcode(0x107, f = data_00625A98[1]++, out->reg);
                    emitpcode(0xA5, data_00625A98[0]++, f);
                    setpcodeflags(0x4000);
                    *out = func_004C3B10(data_006246B4->last->args[0]);
                    return;
                }
                if (is_unsigned(to)) {
                    k = func_0047F660(0x4F000000);
                    k = func_0047A060(&k);
                    float_to_uint(out, &k, 0x107, 0x49, 0x103, -0x8000);
                    return;
                }
            }
        }
        if ((copts.fp_library & 2) && IS_DOUBLE(from)) {
            if (!IS_TYPE_LL(to)) {
                if (!is_unsigned(to)) {
                    emitpcode(0x106, f = data_00625A98[1]++, out->reg);
                    emitpcode(0xA5, data_00625A98[0]++, f);
                    setpcodeflags(0x4000);
                    *out = func_004C3B10(data_006246B4->last->args[0]);
                    return;
                }
                if (is_unsigned(to)) {
                    c.hi = 0x41E00000;
                    c.lo = 0;
                    k = func_0047F9F0(c, TYPE(&stunsignedlonglong));
                    k = func_00479F70(&k);
                    float_to_uint(out, &k, 0x106, 0x48, 0x102, 0x80000000);
                    return;
                }
            }
        }
    }
    func_004A4890(out, from, 1);
    if (IS_TYPE_LL(to)) {
        if (IS_SINGLE(from)) {
            rt = is_unsigned(to) ? rt_fixunssfdi : rt_fixsfdi;
        } else {
            rt = is_unsigned(to) ? rt_fixunsdfdi : rt_fixdfdi;
        }
    } else if (IS_SINGLE(from)) {
        rt = is_unsigned(to) ? rt_f_ftou : rt_f_ftoi;
    } else {
        rt = is_unsigned(to) ? rt_fixunsdfsi : rt_fixdfsi;
    }
    func_004A4A20(rt);
    *out = func_004A46E0(IS_TYPE_LL(to));
    return;

int_to_float:
    if (IS_SINGLE(to)) {
        if (copts.fp_library & 1) {
            if (!is_unsigned(from) && !IS_TYPE_LL(from)) {
                *out = func_0047A060(out);
                *out = func_0047F900(0x73, out, 1);
                return;
            }
            if (is_unsigned(from) && !IS_TYPE_LL(from)) {
                // unsigned: halve (keeping the low bit) and double when the top bit is set
                neg = newlabel();
                done = newlabel();
                f = data_00625A98[1]++;
                neg->pclabel = (struct PCodeLabel*)makepclabel();
                done->pclabel = (struct PCodeLabel*)makepclabel();
                func_00479540(0x24, out, neg);
                tmp = func_0047A060(out);
                emitpcode(0x73, f, tmp.reg);
                func_00480710((MPCodeLabel*)done->pclabel);
                func_0046FA90((MPCodeLabel*)neg->pclabel);
                emitpcode(0xDA, r = data_00625A98[0]++, out->reg, 1);
                tmp = func_0047E650(out, 1);
                emitpcode(0xBE, r, r, tmp.reg);
                tmp.reg = r;
                tmp = func_0047A060(&tmp);
                emitpcode(0x73, f, tmp.reg);
                emitpcode(0xED, f, f, f);
                *out = func_004C3B10(data_006246B4->last->args[0]);
                func_0046FA90((MPCodeLabel*)done->pclabel);
                return;
            }
        }
        func_004A47E0(out, IS_TYPE_LL(from), 1);
        if (is_unsigned(from)) {
            rt = IS_TYPE_LL(from) ? rt_f_ulltof : rt_f_utof;
        } else {
            rt = IS_TYPE_LL(from) ? rt_floatdisf : rt_f_itof;
        }
        func_004A4A20(rt);
        *out = func_004A4710(to);
    } else {
        if ((copts.fp_library & 2) && !is_unsigned(from) && !IS_TYPE_LL(from)) {
            *out = func_0047A060(out);
            *out = func_0047F900(0x6E, out, 1);
            return;
        }
        func_004A47E0(out, IS_TYPE_LL(from), 1);
        if (is_unsigned(from)) {
            rt = IS_TYPE_LL(from) ? rt_d_ulltod : rt_d_utod;
        } else {
            rt = IS_TYPE_LL(from) ? rt_floatdidf : rt_floatsidf;
        }
        func_004A4A20(rt);
        *out = func_004A4710(to);
    }
}
#endif

extern void func_00492050(ENode* expr, VOperand* out);

void func_00493F90(ENode* expr, void* target, VOperand* out) {
    func_00492050(expr, out);
}

// expression types the generator table does not expect
#undef CError_PS2_SHIFT
#define CError_PS2_SHIFT -15 // mwccps2: 0xF35 and 0xE1A
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -34 // b145 asserts
void func_00494940(ENode* expr, void* target, VOperand* out) {
    CError_FATAL(3908);
}

void func_004951D0(ENode* expr, void* target, VOperand* out) {
    CError_FATAL(3625);
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#undef CError_PS2_SHIFT
#define CError_PS2_SHIFT 0

// EBITFIELD: the word holding the field, with the field's type attached
void func_004954B0(ENode* expr, void* target, VOperand* out) {
    data_005D0140[expr->data.monadic->type](expr->data.monadic, NULL, out);
    if (out->kind == 4 && out->kind) {
        func_004A64C0(out, TYPE(&void_ptr), 0);
    }
    out->unk18 = (PtrInt)expr->rtype;
}

// ECOMMA
void func_00496C70(ENode* expr, short target, VOperand* out) {
    ENode* right = expr->data.diadic.right;

    data_005D0140[expr->data.diadic.left->type](expr->data.diadic.left, NULL, out);
    data_005D0140[right->type](right, (void*)target, out);
}

// EFORCELOAD: scalars end up in a new register
void func_0049AEF0(ENode* expr, void* target, VOperand* out) {
    ENode* sub = expr->data.monadic;
    Type* type;

    data_005D0140[sub->type](sub, NULL, out);
    type = sub->rtype;
    if (type->type == TYPEINT ||
        type->type == TYPEENUM ||
        type->type == TYPEPOINTER ||
        (type->type == TYPEMEMBERPOINTER && type->size == 4) ||
        type->type == TYPEFLOAT) {
        if (out->kind) {
            func_004A64C0(out, type, 1);
        }
    }
}

// ++x / --x: the updated value
void func_0049B870(ENode* expr, void* target, VOperand* out) {
    VOperand b;
    VOperand a;

    func_0049B930(expr, &a, &b);
    *out = b;
}

typedef struct ObjOperandCache {
    struct ObjOperandCache* next;
    Object* object;
    VOperand opnd;
} ObjOperandCache;

extern ObjOperandCache* data_0062468C;

// an object whose operand was set up before (it must be in the list)
void func_00493B50(ENode* expr, void* target, VOperand* out) {
    ObjOperandCache* c;

    for (c = data_0062468C; c; c = c->next) {
        if (c->object == expr->data.objref) {
            break;
        }
    }
    *out = c->opnd;
}

// ELOGNOT: x < 1, unsigned
void func_00493EF0(ENode* expr, void* target, VOperand* out) {
    ENode* sub = expr->data.monadic;
    VOperand a;

    data_005D0140[sub->type](sub, NULL, &a);
    if (a.kind >= 4 && a.kind) {
        func_004A64C0(&a, sub->rtype, 0);
    }
    *out = func_00479D70(0xE5, &a, 1);
}

extern void func_0047B480(VOperand* a, SInt32 imm, CLabel* label);
extern void func_0047B6A0(VOperand* a, SInt32 imm, CLabel* label);

// && and || as values: the left value in a new register, replaced by the
// right one unless the left decides
#undef CError_PS2_SHIFT
#define CError_PS2_SHIFT -15 // mwccps2: 0x106C
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -34 // b145 asserts
void func_00493D90(ENode* expr, void* target, VOperand* out) {
    VOperand a;
    VOperand b;
    ENode* left = expr->data.diadic.left;
    ENode* right = expr->data.diadic.right;
    CLabel* label = newlabel();

    data_005D0140[left->type](left, NULL, &a);
    if (a.kind) {
        func_004A64C0(&a, left->rtype, 1);
    }
    if (expr->type == ELAND) {
        func_0047B6A0(&a, 0, label);
    } else if (expr->type == ELOR) {
        func_0047B480(&a, 0, label);
    } else {
        CError_FATAL(4219);
    }
    data_005D0140[right->type](right, NULL, &b);
    if (b.kind) {
        func_004A64C0(&b, right->rtype, 0);
    }
    emitpcode(data_00625E48, a.reg, b.reg);
    func_0046FA90((MPCodeLabel*)label->pclabel);
    *out = func_004C3BA0(a.reg, 0);
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#undef CError_PS2_SHIFT
#define CError_PS2_SHIFT 0

extern VOperand func_0047F660(SInt32 value); // a constant in a new register
extern VOperand func_0047A060(VOperand* src); // into a single-precision FPR
extern VOperand func_00479F70(VOperand* src); // into a double-precision FPR

// EFLOATCONST: the bits as an integer constant, moved to an FPR when floats
// are in hardware
void func_004951F0(ENode* expr, void* target, VOperand* out) {
    float f;
    UInt32 bits;
    union {
        double d;
        UInt32 w[2];
    } dbl;
    CInt64 c;

    if (IS_SINGLE(expr->rtype)) {
        f = expr->data.floatval.data.value;
        bits = *(UInt32*)&f;
        *out = func_0047F660(bits);
        if (expr->rtype->type == TYPEFLOAT && IS_HARDFLOAT(expr->rtype)) {
            *out = func_0047A060(out);
        }
    } else {
        dbl.d = expr->data.floatval.data.value;
#if MWCCPS2
        // b210 (0x49F940): the double's bits in the low 64 of the 128-bit constant
        c.hi = 0;
        c.mid[0] = 0;
        c.mid[1] = dbl.w[1];
        c.lo = dbl.w[0];
#else
        c.lo = dbl.w[0];
        c.hi = dbl.w[1];
#endif
        *out = func_0047F9F0(c, TYPE(&stunsignedlonglong));
        if (expr->rtype->type == TYPEFLOAT && IS_HARDFLOAT(expr->rtype)) {
            *out = func_00479F70(out);
        }
    }
}

extern void func_004A5A60(VOperand* op, Type* type, int unused); // make op a value unless it is one

// ENULLCHECK: the checked value in a new register (and cached for the
// precomputed references to it); the conditional expression's value only
// when the check passes
void func_00493BB0(ENode* expr, void* target, VOperand* out) {
    VOperand a;
    VOperand b;
    ObjOperandCache* cache;
    CLabel* label;
    ENode* check = expr->data.nullcheck.nullcheckexpr;
    ENode* cond = expr->data.nullcheck.condexpr;
    int reg = data_00625A98[0]++;

    data_005D0140[check->type](check, NULL, &a);
    if (check->rtype->size < 4) {
        func_004A5A60(&a, check->rtype, 0);
    }
    if (a.kind) {
        func_004A64C0(&a, check->rtype, 0);
    }
    emitpcode(data_00625E48, reg, a.reg);
    cache = lalloc(sizeof(ObjOperandCache));
    cache->object = (Object*)expr->data.nullcheck.precompid;
    cache->opnd = a;
    cache->next = data_0062468C;
    data_0062468C = cache;
    label = newlabel();
    func_0047B6A0(&a, 0, label);
    data_005D0140[cond->type](cond, NULL, &b);
    data_0062468C = data_0062468C->next;
    if (expr->rtype->type != TYPEVOID && !expr->ignored) {
        out->kind = 0;
        if (b.kind) {
            func_004A64C0(&b, cond->rtype, 0);
        }
        emitpcode(data_00625E48, reg, b.reg);
    } else {
        out->kind = 3;
        out->reg = 0;
    }
    func_0046FA90((MPCodeLabel*)label->pclabel);
    *out = func_004C3BA0(reg, 0);
}

// A floating-point comparison as 0 or 1: the runtime routine with the float
// library, else c.cond and a branch around li 1 / move 0.
void func_00491E40(short cond, ENode* left, ENode* right, VOperand* out) {
    VOperand a;
    VOperand b;
    SInt16 brop;
    SInt16 cmpop;
    Type* type;
    MPCodeLabel* label;
    int reg = data_00625A98[0]++;

    func_0049D8D0(left, right, &a, &b, 1);
    type = left->rtype;
    if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
        func_004A4890(&a, type, 1);
        func_004A4890(&b, right->rtype, 2);
        func_004A4A20(func_0049D400(cond, right->rtype));
        *out = func_004A46E0(0);
    } else {
        label = makepclabel();
        func_0049D530(cond, right->rtype, &brop, &cmpop, 1);
        emitpcode(cmpop, a.reg, b.reg);
        emitpcode(data_00625E48, reg, func_0047F680(1));
        func_00479B00(brop, label);
        emitpcode(data_00625E48, reg, 0);
        func_0046FA90(label);
        *out = func_004C3BA0(reg, 0);
    }
}

#define IS_LONGLONG(t) (((t)->type == TYPEINT || (t)->type == TYPEENUM) && (t)->size == 8)

// EBINNOT: nor with $zero; ~(a | b) is a single nor
void func_0049AF50(ENode* expr, void* target, VOperand* out) {
    VOperand zero;
    VOperand a;
    VOperand b;
    Type* type = expr->rtype;
    ENode* sub = expr->data.monadic;

    zero = func_004C3BA0(0, 0);
    if (sub->type == EOR) {
        func_0049D8D0(sub->data.diadic.left, sub->data.diadic.right, &a, &b, 1);
        *out = func_0047F740(0xBC, &a, &b, 0);
        if (IS_LONGLONG(type)) {
            a = func_004A7390(&a);
            b = func_004A7390(&b);
            a = func_0047F740(0xBC, &a, &b, 0);
            out->regHi = a.reg;
            out->kind = 0;
        }
        return;
    }
    data_005D0140[sub->type](sub, NULL, &a);
    if ((a.kind >= 4 || a.kind == 2) && a.kind) {
        func_004A64C0(&a, sub->rtype, 0);
    }
#if !MWCCPS2 // b210 keeps the pair only in the EOR case above
    if (IS_LONGLONG(type)) {
        b = func_004A7390(&a);
        a = func_0047F740(0xBC, &a, &zero, 0);
        b = func_0047F740(0xBC, &b, &zero, 0);
        *out = a;
        out->regHi = b.reg;
        out->kind = 0;
    } else
#endif
    {
        *out = func_0047F740(0xBC, &a, &zero, 0);
    }
}

// Which single bit a 64-bit value has set (the high word's bits first, as
// 0..31, then the low word's as 32..63); -1 for zero, -2 otherwise.
#define BITIS(h, l, n) \
    if (v.hi == (h) && v.lo == (l)) { \
        return n; \
    }

int func_00491840(CInt64 v) {
    BITIS(0, 0, -1)
    BITIS(0x1U, 0, 0)
    BITIS(0x2U, 0, 1)
    BITIS(0x4U, 0, 2)
    BITIS(0x8U, 0, 3)
    BITIS(0x10U, 0, 4)
    BITIS(0x20U, 0, 5)
    BITIS(0x40U, 0, 6)
    BITIS(0x80U, 0, 7)
    BITIS(0x100U, 0, 8)
    BITIS(0x200U, 0, 9)
    BITIS(0x400U, 0, 10)
    BITIS(0x800U, 0, 11)
    BITIS(0x1000U, 0, 12)
    BITIS(0x2000U, 0, 13)
    BITIS(0x4000U, 0, 14)
    BITIS(0x8000U, 0, 15)
    BITIS(0x10000U, 0, 16)
    BITIS(0x20000U, 0, 17)
    BITIS(0x40000U, 0, 18)
    BITIS(0x80000U, 0, 19)
    BITIS(0x100000U, 0, 20)
    BITIS(0x200000U, 0, 21)
    BITIS(0x400000U, 0, 22)
    BITIS(0x800000U, 0, 23)
    BITIS(0x1000000U, 0, 24)
    BITIS(0x2000000U, 0, 25)
    BITIS(0x4000000U, 0, 26)
    BITIS(0x8000000U, 0, 27)
    BITIS(0x10000000U, 0, 28)
    BITIS(0x20000000U, 0, 29)
    BITIS(0x40000000U, 0, 30)
    BITIS(0x80000000U, 0, 31)
    BITIS(0, 0x1U, 32)
    BITIS(0, 0x2U, 33)
    BITIS(0, 0x4U, 34)
    BITIS(0, 0x8U, 35)
    BITIS(0, 0x10U, 36)
    BITIS(0, 0x20U, 37)
    BITIS(0, 0x40U, 38)
    BITIS(0, 0x80U, 39)
    BITIS(0, 0x100U, 40)
    BITIS(0, 0x200U, 41)
    BITIS(0, 0x400U, 42)
    BITIS(0, 0x800U, 43)
    BITIS(0, 0x1000U, 44)
    BITIS(0, 0x2000U, 45)
    BITIS(0, 0x4000U, 46)
    BITIS(0, 0x8000U, 47)
    BITIS(0, 0x10000U, 48)
    BITIS(0, 0x20000U, 49)
    BITIS(0, 0x40000U, 50)
    BITIS(0, 0x80000U, 51)
    BITIS(0, 0x100000U, 52)
    BITIS(0, 0x200000U, 53)
    BITIS(0, 0x400000U, 54)
    BITIS(0, 0x800000U, 55)
    BITIS(0, 0x1000000U, 56)
    BITIS(0, 0x2000000U, 57)
    BITIS(0, 0x4000000U, 58)
    BITIS(0, 0x8000000U, 59)
    BITIS(0, 0x10000000U, 60)
    BITIS(0, 0x20000000U, 61)
    BITIS(0, 0x40000000U, 62)
    BITIS(0, 0x80000000U, 63)
    return -2;
}

// op applied to expr, into a new floating-point register
VOperand func_0049D710(short op, ENode* expr) {
    VOperand v;

    memset(&v, 0, sizeof(VOperand));
    data_005D0140[expr->type](expr, NULL, &v);
    if (v.kind) {
        func_004A64C0(&v, expr->rtype, 0);
    }
    emitpcode(op, data_00625A98[1]++, v.reg);
    return func_004C3B10(data_006246B4->last->args[0]);
}

// 0x492050: an integer comparison as a value (0 or 1). A constant left
// side is swapped to the right (and the relation mirrored); a long long
// compares the high words, and the low words (unsigned) when those are equal.
extern VOperand func_0047D480(VOperand* src, SInt32 value); // src < value (signed)
extern VOperand func_0047CF50(VOperand* src, SInt32 value); // src < value (unsigned)
extern VOperand func_0047E210(VOperand* src, SInt32 value); // src == value
extern VOperand func_0047C9D0(VOperand* src, SInt32 value); // src > value (signed)
extern VOperand func_0047C740(VOperand* src, SInt32 value); // src > value (unsigned)
extern VOperand func_0047D870(VOperand* src, SInt32 value); // src <= value (unsigned)
extern VOperand func_0047CC70(VOperand* src, SInt32 value); // src >= value (unsigned)
extern VOperand func_0047D240(VOperand* src, CInt64* value, Type* type); // <, signed
extern VOperand func_0047CD10(VOperand* src, CInt64* value, Type* type); // <, unsigned
extern VOperand func_0047C990(VOperand* src, CInt64* value, Type* type); // >, signed
extern VOperand func_0047C700(VOperand* src, CInt64* value, Type* type); // >, unsigned
extern VOperand func_0047D8B0(VOperand* src, CInt64* value, Type* type); // <=, signed
extern VOperand func_0047D780(VOperand* src, CInt64* value, Type* type); // <=, unsigned
extern VOperand func_0047CCD0(VOperand* src, CInt64* value, Type* type); // >=, signed
extern VOperand func_0047CC30(VOperand* src, CInt64* value, Type* type); // >=, unsigned
extern VOperand func_0047DFC0(VOperand* src, CInt64* value, Type* type); // ==
extern VOperand func_0047D9F0(VOperand* src, CInt64* value, Type* type); // !=
extern void func_004C5940(int code, ...); // MIPSError_Error
extern void func_00491E40(short cond, ENode* left, ENode* right, VOperand* out);

void func_00492050(ENode* expr, VOperand* out) {
    ENode* left;
    ENode* right;
    Type* type;
#if MWCCPS2
    int op;
#else
    UInt8 op;
#endif
    Boolean uns;
    VOperand zero;
    VOperand a;
    VOperand b;
    VOperand tmp;
    VOperand ahi;
    VOperand bhi;
    VOperand t1;
    VOperand t2;
    VOperand t3;

    left = expr->data.diadic.left;
    right = expr->data.diadic.right;
    type = left->rtype;
    op = expr->type;
    zero = func_004C3BA0(0, 0);

    if (left->rtype->type == TYPEFLOAT) {
        func_00491E40(op, left, right, out);
        return;
    }

    func_0049D8D0(left, right, &a, &b, 0);
    if (a.kind == 3) {
        tmp = a;
        a = b;
        b = tmp;
        switch (op) {
            case ELESS:
                op = EGREATER;
                break;
            case EGREATER:
                op = ELESS;
                break;
            case ELESSEQU:
                op = EGREATEREQU;
                break;
            case EGREATEREQU:
                op = ELESSEQU;
                break;
        }
    }
    if (a.kind != 0) {
        func_004A64C0(&a, left->rtype, 0);
    }
    if (b.kind != 3 && b.kind != 0) {
        func_004A64C0(&b, right->rtype, 0);
    }
    uns = is_unsigned(right->rtype) | is_unsigned(left->rtype);

#if !MWCCPS2 // b210: a long long is one 64-bit register, compared like an int
    if ((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8) {
        ahi = func_004A7390(&a);
        switch (op) {
            case ELESS:
                if (b.kind == 3) {
                    *out = uns ? func_0047CF50(&ahi, b.imm.hi) : func_0047D480(&ahi, b.imm.hi);
                    t1 = func_0047E210(&ahi, b.imm.hi);
                    t2 = func_0047CF50(&a, b.imm.lo);
                } else {
                    bhi = func_004A7390(&b);
                    *out = func_0047F740(uns ? 0xD7 : 0xD4, &ahi, &bhi, 0);
                    t1 = func_0047F740(MACRO_OP(0x83), &ahi, &bhi, 0);
                    t2 = func_0047F740(0xD7, &a, &b, 0);
                }
                t2 = func_0047F740(6, &t2, &t1, 0);
                *out = func_0047F740(0xBE, &t2, out, 0);
                break;
            case EGREATER:
                if (b.kind == 3) {
                    *out = uns ? func_0047C740(&ahi, b.imm.hi) : func_0047C9D0(&ahi, b.imm.hi);
                    t1 = func_0047E210(&ahi, b.imm.hi);
                    t2 = func_0047C740(&a, b.imm.lo);
                } else {
                    bhi = func_004A7390(&b);
                    *out = func_0047F740(uns ? MACRO_OP(0x8B) : MACRO_OP(0x89), &ahi, &bhi, 0);
                    t1 = func_0047F740(MACRO_OP(0x83), &ahi, &bhi, 0);
                    t2 = func_0047F740(MACRO_OP(0x8B), &a, &b, 0);
                }
                t2 = func_0047F740(6, &t2, &t1, 0);
                *out = func_0047F740(0xBE, &t2, out, 0);
                break;
            case ELESSEQU:
                if (b.kind == 3) {
                    *out = uns ? func_0047CF50(&ahi, b.imm.hi) : func_0047D480(&ahi, b.imm.hi);
                    t1 = func_0047E210(&ahi, b.imm.hi);
                    t2 = func_0047D870(&a, b.imm.lo);
                } else {
                    bhi = func_004A7390(&b);
                    *out = func_0047F740(uns ? 0xD7 : 0xD4, &ahi, &bhi, 0);
                    t1 = func_0047F740(MACRO_OP(0x83), &ahi, &bhi, 0);
                    t2 = func_0047F740(MACRO_OP(0x8F), &a, &b, 0);
                }
                t2 = func_0047F740(6, &t2, &t1, 0);
                *out = func_0047F740(0xBE, &t2, out, 0);
                break;
            case EGREATEREQU:
                if (b.kind == 3) {
                    *out = uns ? func_0047C740(&ahi, b.imm.hi) : func_0047C9D0(&ahi, b.imm.hi);
                    t1 = func_0047E210(&ahi, b.imm.hi);
                    t2 = func_0047CC70(&a, b.imm.lo);
                } else {
                    bhi = func_004A7390(&b);
                    *out = func_0047F740(uns ? MACRO_OP(0x8B) : MACRO_OP(0x89), &ahi, &bhi, 0);
                    t1 = func_0047F740(MACRO_OP(0x83), &ahi, &bhi, 0);
                    t2 = func_0047F740(MACRO_OP(0x87), &a, &b, 0);
                }
                t2 = func_0047F740(6, &t2, &t1, 0);
                *out = func_0047F740(0xBE, &t2, out, 0);
                break;
            case EEQU:
            case ENOTEQU:
                if (b.kind == 3) {
                    t1 = b.imm.hi ? func_00479D70(MACRO_OP(0x96), &ahi, b.imm.hi) : ahi;
                    t2 = b.imm.lo ? func_00479D70(MACRO_OP(0x96), &a, b.imm.lo) : a;
                } else {
                    bhi = func_004A7390(&b);
                    t1 = func_0047F740(0xDD, &ahi, &bhi, 0);
                    t2 = func_0047F740(0xDD, &a, &b, 0);
                }
                t3 = func_0047F740(0xBE, &t1, &t2, 0);
                if (op == EEQU) {
                    *out = func_00479D70(0xD6, &t3, 1);
                } else {
                    *out = func_00479D70(MACRO_OP(0x94), &t3, 0);
                }
                break;
            default:
                func_004C5940(0x10);
        }
    } else
#endif
#if MWCCPS2 // b210's case order; each constant compare is its own call and copy
        switch (op) {
            case EEQU:
                if (b.kind == 3) {
                    *out = func_0047DFC0(&a, &b.imm, type);
                } else {
                    *out = func_0047F740(MACRO_OP(0x83), &a, &b, 0);
                }
                break;
            case ENOTEQU:
                if (b.kind == 3) {
                    *out = func_0047D9F0(&a, &b.imm, type);
                } else {
                    *out = func_0047F740(MACRO_OP(0x93), &a, &b, 0);
                }
                break;
            case ELESS:
                if (b.kind == 3) {
                    if (uns) {
                        *out = func_0047CD10(&a, &b.imm, type);
                    } else {
                        *out = func_0047D240(&a, &b.imm, type);
                    }
                } else {
                    *out = func_0047F740(uns ? 0xD7 : 0xD4, &a, &b, 0);
                }
                break;
            case EGREATER:
                if (b.kind == 3) {
                    if (uns) {
                        *out = func_0047C700(&a, &b.imm, type);
                    } else {
                        *out = func_0047C990(&a, &b.imm, type);
                    }
                } else {
                    *out = func_0047F740(uns ? MACRO_OP(0x8B) : MACRO_OP(0x89), &a, &b, 0);
                }
                break;
            case ELESSEQU:
                if (b.kind == 3) {
                    if (uns) {
                        *out = func_0047D780(&a, &b.imm, type);
                    } else {
                        *out = func_0047D8B0(&a, &b.imm, type);
                    }
                } else {
                    *out = func_0047F740(uns ? MACRO_OP(0x8F) : MACRO_OP(0x8D), &a, &b, 0);
                }
                break;
            case EGREATEREQU:
                if (b.kind == 3) {
                    if (uns) {
                        *out = func_0047CC30(&a, &b.imm, type);
                    } else {
                        *out = func_0047CCD0(&a, &b.imm, type);
                    }
                } else {
                    *out = func_0047F740(uns ? MACRO_OP(0x87) : MACRO_OP(0x85), &a, &b, 0);
                }
                break;
            default:
                func_004C5940(0x10);
        }
#else
    {
        switch (op) {
            case ELESS:
                if (b.kind == 3) {
                    *out = uns ? func_0047CD10(&a, &b.imm, type) : func_0047D240(&a, &b.imm, type);
                } else {
                    *out = func_0047F740(uns ? 0xD7 : 0xD4, &a, &b, 0);
                }
                break;
            case EGREATER:
                if (b.kind == 3) {
                    *out = uns ? func_0047C700(&a, &b.imm, type) : func_0047C990(&a, &b.imm, type);
                } else {
                    *out = func_0047F740(uns ? MACRO_OP(0x8B) : MACRO_OP(0x89), &a, &b, 0);
                }
                break;
            case ELESSEQU:
                if (b.kind == 3) {
                    *out = uns ? func_0047D780(&a, &b.imm, type) : func_0047D8B0(&a, &b.imm, type);
                } else {
                    *out = func_0047F740(uns ? MACRO_OP(0x8F) : MACRO_OP(0x8D), &a, &b, 0);
                }
                break;
            case EGREATEREQU:
                if (b.kind == 3) {
                    *out = uns ? func_0047CC30(&a, &b.imm, type) : func_0047CCD0(&a, &b.imm, type);
                } else {
                    *out = func_0047F740(uns ? MACRO_OP(0x87) : MACRO_OP(0x85), &a, &b, 0);
                }
                break;
            case EEQU:
                if (b.kind == 3) {
                    *out = func_0047DFC0(&a, &b.imm, type);
                } else {
                    *out = func_0047F740(MACRO_OP(0x83), &a, &b, 0);
                }
                break;
            case ENOTEQU:
                if (b.kind == 3) {
                    *out = func_0047D9F0(&a, &b.imm, type);
                } else {
                    *out = func_0047F740(MACRO_OP(0x93), &a, &b, 0);
                }
                break;
            default:
                func_004C5940(0x10);
        }
    }
#endif
}
