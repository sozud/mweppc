#include "compiler/common.h"
#include "compiler/CParser.h"
#include "compiler/CMachine.h"
#include "compiler/CompilerTools.h"
#include "compiler/objects.h"
#include "compiler/tokens.h"
#include "compiler/types.h"
#include "psp/BE.h"

// mwccpsp 3.0.1 around 0x4A4320..0x4A46A0, between expandIR and the
// "Operands.c" helpers: constant objects and frame offsets for locals.

extern void func_004A64C0(VOperand* op, Type* type, int flags); // make the operand a value
extern VOperand func_00479D70(UInt16 op, VOperand* src, SInt32 imm); // src op imm
extern VOperand func_0047E650(VOperand* src, UInt32 mask); // src & mask
extern VOperand func_004C3BA0(SInt32 reg, UInt8 kind);
extern VOperand func_00479B60(UInt16 op, VOperand* src, SInt32 imm, SInt32 flags);
extern void func_00480740(int op, SInt16 dst, SInt16 src, SInt32 imm); // dst = src op imm

// one constant object per key object, for the current function
typedef struct ConstObjCache {
    struct ConstObjCache* next;
    Object* obj;
    Object* key;
} ConstObjCache;

typedef struct PooledFloat {
    struct PooledFloat* next;
    Object* object;
} PooledFloat;

#define IS_SINGLE(t) \
    (TYPE_INTEGRAL(t)->integral == IT_FLOAT || ((t)->type == TYPEFLOAT && (t)->size == 4))

extern PooledFloat* data_00625970; // float constants to emit at the end
extern SInt32 data_00624D74; // section for 4-byte float literals
extern SInt32 data_00624D60; // section for 8-byte float literals
extern ConstObjCache* data_00624DAC; // reset by expandIR32
extern SInt32 data_0062421C; // stack alignment
extern SInt32 data_00624FCC; // locals start
extern SInt32 data_0062597C; // frame size

// A static const void* object named like key, made once per key.
Object* func_004A4320(Object* key) {
    ConstObjCache* cache;
    Object* obj;

    for (cache = data_00624DAC; cache; cache = cache->next) {
        if (cache->key == key) {
            return cache->obj;
        }
    }

    obj = galloc(sizeof(Object));
    obj->type = TYPE(&void_ptr);
    obj->name = key->name;
    obj->sclass = TK_STATIC;
    obj->qual = Q_CONST;
    obj->datatype = DDATA;
    obj->flags |= 2;

    cache = galloc(sizeof(ConstObjCache));
    cache->obj = obj;
    cache->key = key;
    cache->next = data_00624DAC;
    data_00624DAC = cache;
    return obj;
}

static inline Boolean float_equal(Float* a, Float* b) {
    return a->data.words[0] == b->data.words[0] && a->data.words[1] == b->data.words[1];
}

// The pooled object holding a float constant of this type, made once.
Object* func_004A43A0(Type* type, Float* value) {
    PooledFloat* pf;
    Object* obj;

    for (pf = data_00625970; pf; pf = pf->next) {
        obj = pf->object;
        if (obj->type == type && float_equal(obj->u.data.u.floatconst, value)) {
            return obj;
        }
    }

    obj = galloc(sizeof(Object));
    memclrw(obj, sizeof(Object));
    obj->type = type;
    obj->name = CParser_GetUniqueName();
    obj->sclass = TK_STATIC;
    obj->qual = Q_CONST | 0x10000;
    obj->datatype = DDATA;
    obj->flags |= 2;
    pf = galloc(sizeof(PooledFloat));
    pf->object = obj;
    obj->u.data.u.floatconst = galloc(sizeof(Float));
    *obj->u.data.u.floatconst = *value;
    if (copts.mips_option17) {
        obj->section = IS_SINGLE(type) ? data_00624D74 : data_00624D60;
    }
    pf->next = data_00625970;
    data_00625970 = pf;
    return obj;
}

// Whether a float constant's low halfword is zero (it can be built with lui
// alone); always when not optimizing for size.
Boolean func_004A4490(Type* type, Float value) {
    UInt32 lo;
    UInt32 hi;
    float f;
    UInt32 bits;
    union {
        double d;
        UInt32 w[2];
    } dbl;

    if (!copts.optimizesize) {
        return TRUE;
    }
    if (IS_SINGLE(type)) {
        f = value.data.value;
        bits = *(UInt32*)&f;
        return !(bits & 0xFFFF) ? (Boolean)TRUE : (Boolean)FALSE;
    }
    dbl.d = value.data.value;
    lo = ((UInt32*)&dbl.d)[0];
    hi = ((UInt32*)&dbl.d)[1];
    if (lo == 0) {
        return !(hi & 0xFFFF) ? (Boolean)TRUE : (Boolean)FALSE;
    }
    if (hi == 0) {
        return !(lo & 0xFFFF) ? (Boolean)TRUE : (Boolean)FALSE;
    }
    return FALSE;
}

// A local's offset from the stack pointer; locals in the parameter area are
// past the aligned locals block.
SInt32 func_004A4530(Object* obj) {
    SInt32 pad = ((data_00624FCC + (data_0062421C - 1)) & ~(data_0062421C - 1)) - data_00624FCC;
    SInt32 base = data_0062597C;

    if (obj->u.var.info->flags & VarInfoFlag_ParamArea) {
        base = data_0062597C + pad;
    }
    return obj->u.var.offset + base;
}

// alloca: move the stack pointer down by size, rounded up to the stack
// alignment, and return the register holding the new block's address.
int func_004A4580(VOperand* size) {
    VOperand v;
    SInt32 n;

    if (size->kind == 3) {
        func_00480740(
            0x5AE,
            29,
            29,
            ((size->imm.lo ? size->imm.lo : 0) + (data_0062421C - 1)) & ~(data_0062421C - 1)
        );
    } else {
        if (size->kind != 0) {
            func_004A64C0(size, TYPE(&stsignedint), 0);
        }
        v = func_00479D70(3, size, data_0062421C - 1);
        v = func_0047E650(&v, ~(data_0062421C - 1));
        emitpcode(0xDD, 29, 29, v.reg);
    }
    v = func_004C3BA0(29, 0);
    v = func_00479B60(3, &v, 0, 0x11);
    return v.reg;
}
