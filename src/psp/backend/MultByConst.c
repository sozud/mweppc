// Multiplication by a constant (0x53EEF0..0x540710): the cheapest sequence
// of shifts, adds and subtracts that builds the constant, if it beats the
// multiply instruction (or the 64-bit runtime call). Values are found by a
// memoized depth-first search over seven ways of building a value from a
// smaller one; the result is emitted bottom up. Unit name not known (the
// code sits between DIVbyCnst.c and the template code).
#include "compiler/common.h"
#include "compiler/CError.h"
#include "compiler/CParser.h"
#include "compiler/CompilerTools.h"
#include "compiler/types.h"
#include "psp/BE.h"
#include <stdlib.h>
#include <string.h>

typedef struct MultEntry {
    struct MultEntry* best; // +0x00: the value this one is built from (itself for 1)
    SInt32 value; // +0x04
    UInt32 cost; // +0x08: cost of the best sequence, or the search bound so far
    UInt8 method; // +0x0C: how (MULT_*)
    struct MultEntry* next; // +0x10: hash chain
    VOperand op; // +0x14: the emitted value
} MultEntry; // 0x38

enum {
    MULT_ONE, // the source itself
    MULT_NEG, // -x
    MULT_SHL_ADD1, // (x << k) + 1
    MULT_SHL_SUB1, // (x << k) - 1
    MULT_1_SUB_SHL, // 1 - (x << k)
    MULT_ADD_SHL, // x + (x << k)
    MULT_SHL_SUB, // (x << k) - x
    MULT_SUB_SHL // x - (x << k)
};

#define MULT_HASH 0x1FF

// cost of each method's instruction (index method - 1)
const UInt32 MultCosts[7] = {1, 2, 2, 2, 2, 2, 2};
static Type* MultType;
static MultEntry* MultHash[MULT_HASH];

extern VOperand func_004C3BA0(SInt32 reg, UInt8 kind);
extern VOperand func_0047EBB0(VOperand* src, SInt32 shift, Type* type);
extern VOperand func_0047F660(SInt32 value);
extern VOperand func_0047F740(UInt16 op, VOperand* a, VOperand* b, char rclass);
extern VOperand func_0047F900(UInt16 op, VOperand* a, char rclass);
extern void func_004A47E0(VOperand* op, int unk, int argno);
extern void func_004A4A20(Object* rtfunc);
extern VOperand func_004A46E0(int pair);
extern SInt16 data_00625CE0;
extern SInt16 data_00625E4E;
extern Object* rt_muldi3;
extern Object* rt_muldi3_2;

#define IS_64BIT_INT(t) (((t)->type == TYPEINT || (t)->type == TYPEENUM) && (t)->size == 8)

static inline MultEntry* FindMultEntry(SInt32 value) {
    int h;
    MultEntry* e;

    h = abs(value) % MULT_HASH;
    for (e = MultHash[h]; e && e->value != value; e = e->next) {}
    if (!e) {
        e = lalloc(sizeof(MultEntry));
        memset(e, 0, sizeof(MultEntry));
        e->value = value;
        e->best = NULL;
        e->next = MultHash[h];
        MultHash[h] = e;
    }
    return e;
}

// what the multiply instruction (or the runtime call) costs
#if MWCCPS2
__declspec(noinline) // b210 MultiplyByConstant calls it
#endif
static UInt32 MultCost(SInt32 value, Type* type) {
#if MWCCPS2 // b210: a 64-bit multiply is only expensive with mips_option25
    if (IS_64BIT_INT(type) && copts.mips_option25) {
#else
    if (IS_64BIT_INT(type) || (IS_64BIT_INT(type) && copts.mips_option25)) {
#endif
        return 20;
    }
    if (value >= 0) {
        if (value >= 0x10000) {
            return 10;
        }
        return 6;
    }
    if (value <= -0x10000) {
        return 10;
    }
    return 6;
}

#if MWCCPS2
__declspec(noinline) // b210 MultiplyByConstant calls it
#endif
static void InitMultTable(VOperand* src) {
    MultEntry* one;
    MultEntry* neg;
#if MWCCPS2
    UInt32 i; // b210 compares unsigned (jb)
#else
    int i;
#endif

    for (i = 0; i < MULT_HASH; i++) {
        MultHash[i] = NULL;
    }
    one = FindMultEntry(1);
    one->best = one;
    one->method = MULT_ONE;
    one->cost = 0;
    one->op = *src;
    neg = FindMultEntry(-1);
    neg->best = one;
    neg->method = MULT_NEG;
    neg->cost = 1;
}

#if MWCCPS2
__declspec(noinline) // b210 MultiplyByConstant calls it
#endif
static MultEntry* SearchMult(SInt32 value, UInt32 budget) {
    MultEntry* e;
    MultEntry* sub;
    SInt32 k;
    SInt32 m;
    UInt32 c;
    UInt32 limit;

    e = FindMultEntry(value);
    k = 4;
    if (!e->best && e->cost < budget) {
        e->cost = budget;
        if (value < 1) {
            if (4 < (-value >> 1)) {
                do {
                    c = MultCosts[MULT_SUB_SHL - 1];
                    if (value % (1 - k) == 0) {
                        limit = e->cost - c;
                        sub = SearchMult(value / (1 - k), limit);
                        if (sub->best && sub->cost < limit) {
                            e->best = sub;
                            e->method = MULT_SUB_SHL;
                            e->cost = sub->cost + c;
                        }
                    }
                    c = MultCosts[MULT_ADD_SHL - 1];
                    if (value % (k + 1) == 0) {
                        limit = e->cost - c;
                        sub = SearchMult(value / (k + 1), limit);
                        if (sub->best && sub->cost < limit) {
                            e->best = sub;
                            e->method = MULT_ADD_SHL;
                            e->cost = sub->cost + c;
                        }
                    }
                    k <<= 1;
                } while (k < (-value >> 1));
            }
            c = MultCosts[MULT_1_SUB_SHL - 1];
            m = 1 - value;
            do {
                m /= 2;
            } while (!(m & 1));
            limit = e->cost - c;
            sub = SearchMult(m, limit);
            if (sub->best && sub->cost < limit) {
                e->best = sub;
                e->method = MULT_1_SUB_SHL;
                e->cost = sub->cost + c;
            }
            c = MultCosts[MULT_SHL_SUB1 - 1];
            m = value + 1;
            do {
                m /= 2;
            } while (!(m & 1));
            limit = e->cost - c;
            sub = SearchMult(m, limit);
            if (sub->best && sub->cost < limit) {
                e->best = sub;
                e->method = MULT_SHL_SUB1;
                e->cost = sub->cost + c;
            }
        } else {
            if (4 < (value >> 1)) {
                do {
                    c = MultCosts[MULT_SHL_SUB - 1];
                    if (value % (k - 1) == 0) {
                        limit = e->cost - c;
                        sub = SearchMult(value / (k - 1), limit);
                        if (sub->best && sub->cost < limit) {
                            e->best = sub;
                            e->method = MULT_SHL_SUB;
                            e->cost = sub->cost + c;
                        }
                    }
                    c = MultCosts[MULT_ADD_SHL - 1];
                    if (value % (k + 1) == 0) {
                        limit = e->cost - c;
                        sub = SearchMult(value / (k + 1), limit);
                        if (sub->best && sub->cost < limit) {
                            e->best = sub;
                            e->method = MULT_ADD_SHL;
                            e->cost = sub->cost + c;
                        }
                    }
                    k <<= 1;
                } while (k < (value >> 1));
            }
            c = MultCosts[MULT_SHL_ADD1 - 1];
            m = value - 1;
            do {
                m /= 2;
            } while (!(m & 1));
            limit = e->cost - c;
            sub = SearchMult(m, limit);
            if (sub->best && sub->cost < limit) {
                e->best = sub;
                e->method = MULT_SHL_ADD1;
                e->cost = sub->cost + c;
            }
            c = MultCosts[MULT_SHL_SUB1 - 1];
            m = value + 1;
            do {
                m /= 2;
            } while (!(m & 1));
            limit = e->cost - c;
            sub = SearchMult(m, limit);
            if (sub->best && sub->cost < limit) {
                e->best = sub;
                e->method = MULT_SHL_SUB1;
                e->cost = sub->cost + c;
            }
        }
    }
    return e;
}

#define ADD_OP (IS_64BIT_INT(MultType) ? 0x79 : 4)
#define SUB_OP (IS_64BIT_INT(MultType) ? 0x8E : 0xDD)

// emits e's sequence (its sources first); returns e's value
static SInt32 EmitMult(MultEntry* e) {
    SInt32 value;
    SInt32 v;
    SInt32 target;
    SInt32 k;
    MultEntry* one;
    MultEntry* a;
    MultEntry* b;
    VOperand tmp;

    value = e->value;
    one = FindMultEntry(1);
    switch (e->method) {
        case MULT_NEG:
            v = EmitMult(e->best);
            a = FindMultEntry(v);
            e->op = func_0047F900(IS_64BIT_INT(MultType) ? 0x83 : 0xB9, &a->op, 0);
            break;
        case MULT_SHL_ADD1:
            v = EmitMult(e->best);
            a = FindMultEntry(v);
            k = 0;
            do {
                v *= 2;
                k++;
            } while (value - 1 != v);
            tmp = func_0047EBB0(&a->op, k, MultType);
            e->op = func_0047F740(ADD_OP, &tmp, &one->op, 0);
            break;
        case MULT_SHL_SUB1:
            v = EmitMult(e->best);
            a = FindMultEntry(v);
            k = 0;
            do {
                v *= 2;
                k++;
            } while (value + 1 != v);
            tmp = func_0047EBB0(&a->op, k, MultType);
            e->op = func_0047F740(SUB_OP, &tmp, &one->op, 0);
            break;
        case MULT_1_SUB_SHL:
            v = EmitMult(e->best);
            a = FindMultEntry(v);
            k = 0;
            do {
                v *= 2;
                k++;
            } while (1 - value != v);
            tmp = func_0047EBB0(&a->op, k, MultType);
            e->op = func_0047F740(SUB_OP, &one->op, &tmp, 0);
            break;
        case MULT_ADD_SHL:
            v = EmitMult(e->best);
            a = FindMultEntry(v);
            b = FindMultEntry(v);
            target = value - v;
            k = 0;
            do {
                v *= 2;
                k++;
            } while (target != v);
            tmp = func_0047EBB0(&b->op, k, MultType);
            e->op = func_0047F740(ADD_OP, &a->op, &tmp, 0);
            break;
        case MULT_SHL_SUB:
            v = EmitMult(e->best);
            a = FindMultEntry(v);
            b = FindMultEntry(v);
            target = value + v;
            k = 0;
            do {
                v *= 2;
                k++;
            } while (target != v);
            tmp = func_0047EBB0(&b->op, k, MultType);
            e->op = func_0047F740(SUB_OP, &tmp, &a->op, 0);
            break;
        case MULT_SUB_SHL:
            v = EmitMult(e->best);
            a = FindMultEntry(v);
            b = FindMultEntry(v);
            target = v - value;
            k = 0;
            do {
                v *= 2;
                k++;
            } while (target != v);
            tmp = func_0047EBB0(&b->op, k, MultType);
            e->op = func_0047F740(SUB_OP, &a->op, &tmp, 0);
            break;
    }
    return value;
}

VOperand MultiplyByConstant(VOperand* src, SInt32 value, Type* type) {
    MultEntry* e;
    MultEntry* s;
    UInt32 budget;
    SInt32 m;
    SInt32 v;
    SInt32 k;
    VOperand cst;

    MultType = type;
    if (value == 0) {
        return func_004C3BA0(0, 0);
    }
    budget = MultCost(value, type);
    InitMultTable(src);
    if (!(value & 1)) {
        m = value;
        do {
            m /= 2;
        } while (!(m & 1));
        e = SearchMult(m, budget - 1);
        if (e->best && e->cost + 1 < budget) {
            v = EmitMult(e);
            s = FindMultEntry(v);
            k = 0;
            do {
                v += v;
                k++;
            } while (value != v);
            return func_0047EBB0(&s->op, k, MultType);
        }
    } else {
        e = SearchMult(value, budget);
        if (e->best && e->cost < budget) {
            EmitMult(e);
            return e->op;
        }
    }

    cst = func_0047F660(value);
#if MWCCPS2
    // b210 (0x54F230): a long long multiplies in the 64-bit registers unless copts+0x25
    if (IS_64BIT_INT(MultType) && copts.mips_option25) {
#else
    if (IS_64BIT_INT(MultType) || (IS_64BIT_INT(MultType) && copts.mips_option25)) {
#endif
        func_004A47E0(src, 1, 1);
        func_004A47E0(&cst, 1, 2);
        func_004A4A20(is_unsigned(MultType) ? rt_muldi3_2 : rt_muldi3);
        return func_004A46E0(1);
    }
    return func_0047F740(IS_64BIT_INT(MultType) ? data_00625E4E : data_00625CE0, src, &cst, 0);
}
