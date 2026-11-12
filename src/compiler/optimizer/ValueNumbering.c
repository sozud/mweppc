#if MWCCPSP
// mwccpsp 3.0.1: value numbering on the MIPS back end's pcode
// (removecommonsubexpressions, 0x4B11D0..0x4B3090). 2.4.7's code below is
// the template; 3.0.1 reads an instruction's result from args[1] when flag
// 0x80000000 is set, propagates copies only in blocks without flag 0x180,
// and keeps its state at fixed addresses that Alias.c's killmemory shares.
#include "compiler/common.h"
#include "compiler/Alias.h"
#include "compiler/CError.h"
#include "compiler/CompilerTools.h"
#include "compiler/CInt64.h"
#include "compiler/Registers.h"
#include "psp/BE.h"
#include <stddef.h>

typedef struct ValueLabel301 {
    struct ValueLabel301* next;
    MOperand op; // +4
} ValueLabel301; // 0x14

typedef struct AvailableValue301 {
    struct AvailableValue301* next;
    ValueLabel301* labelled; // +4
    MPCode* pcode; // +8
    int killedregister; // +0xC
    int aliasnumber; // +0x10
    int opnumbers[1]; // +0x14
} AvailableValue301;

typedef struct RegValue301 {
    int number;
    int copyreg;
    AvailableValue301* available;
} RegValue301; // 0xC

typedef struct StackedValue301 {
    struct StackedValue301* next;
    MOperand op; // +4
    RegValue301 value; // +0x14
    Alias* alias; // +0x20
    MPCode* valuepcode; // +0x24
} StackedValue301; // 0x28

#define OPCODE_MAX_301 BE_NUM_OPDEFS

int nextvaluenumber;
int data_006245C0; // removedcommonsubexpressions
int data_005F2630; // moreaggressiveoptimization
int data_005F2634; // valueceiling
StackedValue301* data_005F2638; // stackedvalues
RegValue301* data_005F263C[7]; // regvalue
AvailableValue301* data_005F2658[OPCODE_MAX_301]; // opvalue

#define removedcommonsubexpressions data_006245C0
#define moreaggressiveoptimization data_005F2630
#define valueceiling data_005F2634
#define stackedvalues data_005F2638
#define regvalue data_005F263C
#define opvalue data_005F2658

extern SInt32 data_00624744[7]; // the first virtual register of each class
extern SInt32 data_00625A98[7]; // the registers used in each class
extern void gather_alias_info(void);
extern void initialize_alias_values(void);
extern void update_all_alias_values(void);
extern MPCode* func_00490280(MOperand* src, MOperand* dst); // a register copy
extern MPCode* func_0048FDC0(int kind, MOperand* src, MOperand* dst); // a copy for a load
extern SInt32 func_0048FFA0(MPCode* store, MPCode* load); // can_reuse_stored_value

#define RESULT_OP(pc) (&(pc)->args[((pc)->flags & 0x80000000) ? 1 : 0])
#define REGVALUE(op) (&regvalue[(op)->rclass][(op)->reg])

static void labelvalue(AvailableValue301* av, MOperand* op) {
    ValueLabel301* label = oalloc(sizeof(ValueLabel301));
    label->op = *op;
    label->next = av->labelled;
    av->labelled = label;
}

static void unlabelvalue(AvailableValue301* av, MOperand* op) {
    ValueLabel301* labelled;
    ValueLabel301** ptr;

    ptr = &av->labelled;
    while ((labelled = *ptr)) {
        if (labelled->op.reg == op->reg) {
            *ptr = labelled->next;
        } else {
            ptr = &labelled->next;
        }
    }
}

static void stackregistervalue(MOperand* op, RegValue301* value) {
    StackedValue301* stacked = oalloc(sizeof(StackedValue301));
    stacked->next = stackedvalues;
    stackedvalues = stacked;
    stacked->op = *op;
    stacked->value = *value;
}

static void stackmemoryvalue(Alias* alias) {
    StackedValue301* stacked = oalloc(sizeof(StackedValue301));
    stacked->next = stackedvalues;
    stackedvalues = stacked;
    stacked->op.kind = 4;
    stacked->alias = alias;
    stacked->value.number = alias->valuenumber;
    stacked->valuepcode = (MPCode*)alias->valuepcode;
}

static void unstackvalue(StackedValue301* stacked) {
    MOperand* op = &stacked->op;
    RegValue301* value;

    if (op->kind == 4) {
        stacked->alias->valuenumber = stacked->value.number;
        stacked->alias->valuepcode = (PCode*)stacked->valuepcode;
    } else {
        value = REGVALUE(op);
        if (value->available) {
            unlabelvalue(value->available, op);
        }
        value->number = stacked->value.number;
        value->copyreg = stacked->value.copyreg;
        value->available = stacked->value.available;
        if (value->available) {
            labelvalue(value->available, op);
        }
    }
}

static int samevalue(MOperand* op1, MOperand* op2) {
    return REGVALUE(op1)->number == REGVALUE(op2)->number;
}

static int killregister(MOperand* op) {
    RegValue301* value = REGVALUE(op);

    if (value->number < valueceiling && nextvaluenumber >= valueceiling) {
        stackregistervalue(op, value);
    }
    if (value->available) {
        unlabelvalue(value->available, op);
    }
    value->available = NULL;
    value->copyreg = 0;
    return value->number = nextvaluenumber++;
}

// 0x4B2EE0
void killmemory(Alias* alias, MPCode* newValue) {
    if (alias->valuenumber < valueceiling && nextvaluenumber >= valueceiling) {
        stackmemoryvalue(alias);
    }
    if (newValue) {
        alias->valuenumber = REGVALUE(&newValue->args[0])->number;
        alias->valuepcode = (PCode*)newValue;
    } else {
        alias->valuenumber = nextvaluenumber++;
        alias->valuepcode = NULL;
    }
}

// 0x4B2DC0
#if MWCCPS2 && MWCCPS2_BUILD >= 210
__declspec(noinline) // b210 calls it (functioncall); b145 inlines it
#endif
static void killallCSEs(void) {
    AvailableValue301* av;
    ValueLabel301* labelled;
    int i;

    for (i = 0; i < OPCODE_MAX_301; i++) {
        for (av = opvalue[i]; av; av = av->next) {
            for (labelled = av->labelled; labelled; labelled = labelled->next) {
                killregister(&labelled->op);
            }
        }
    }
}

static inline void killregisters(MPCode* pcode) {
    MOperand* op;
    int i;

    for (i = 0, op = pcode->args; i < pcode->argcount; i++, op++) {
        if (op->kind == 0 && (op->flags & 2)) {
            killregister(op);
        }
    }
}

// 0x4B2C80
static void copyregister(MOperand* src, MOperand* dest) {
    RegValue301* srcvalue = REGVALUE(src);
    RegValue301* destvalue = REGVALUE(dest);

    if (destvalue->number < valueceiling && nextvaluenumber >= valueceiling) {
        stackregistervalue(dest, destvalue);
    }
    if (destvalue->available) {
        unlabelvalue(destvalue->available, dest);
    }
    destvalue->available = srcvalue->available;
    if (destvalue->available) {
        labelvalue(destvalue->available, dest);
    }
    destvalue->number = srcvalue->number;
    if (srcvalue->copyreg && srcvalue->number == regvalue[src->rclass][srcvalue->copyreg].number) {
        destvalue->copyreg = srcvalue->copyreg;
    } else {
        destvalue->copyreg = src->reg;
    }
}

// 0x4B2A10
static int matchvalues(AvailableValue301* av, MPCode* match) {
    MPCode* avpc = av->pcode;
    MOperand* avOp = &avpc->args[0];
    MOperand* matchOp = &match->args[0];
    int base = (match->flags & 0x80000000) ? 1 : 0;
    int i;

    if (match->alias != avpc->alias) {
        return FALSE;
    }
    if (match->unk38 != avpc->unk38) {
        return FALSE;
    }
    for (i = 0; i < match->argcount; i++, avOp++, matchOp++) {
        if (i != base) {
            switch (avOp->kind) {
                case 0:
                    if (av->opnumbers[i] != REGVALUE(matchOp)->number) {
                        return FALSE;
                    }
                    break;
#if MWCCPS2 && MWCCPS2_BUILD < 210
                case 1:
                    CError_Internal("LocalCSE.c", 579); // b145's file name and line
                    break;
#else
                case 1:
                    if (matchOp->reg != avOp->reg) {
                        return FALSE;
                    }
                    break;
#endif
                case 2:
                    if (matchOp->kind != 2 || matchOp->value != avOp->value) {
                        return FALSE;
                    }
                    break;
                case 3:
                    if (matchOp->kind != 3 ||
                        !CInt64_Equal(*(CInt64*)&matchOp->value, *(CInt64*)&avOp->value)) {
                        return FALSE;
                    }
                    break;
                case 4:
                    if (matchOp->kind != 4 ||
                        MOP_OBJ(*matchOp) != MOP_OBJ(*avOp) ||
                        matchOp->value != avOp->value) {
                        return FALSE;
                    }
                    break;
                case 5:
#if MWCCPS2
                    // b210 compares the word after the 128-bit constant (+0x14)
                    if (matchOp->kind != 5 ||
                        matchOp->value_mid[1] != avOp->value_mid[1] ||
#else
                    if (matchOp->kind != 5 ||
                        matchOp->unk0C != avOp->unk0C ||
#endif
                        !CInt64_Equal(*(CInt64*)&matchOp->value, *(CInt64*)&avOp->value)) {
                        return FALSE;
                    }
                    break;
                case 6:
#if MWCCPSP_NATIVE
                    if (matchOp->kind != 6 || matchOp->ptr != avOp->ptr) {
#else
                    if (matchOp->kind != 6 || matchOp->value != avOp->value) {
#endif
                        return FALSE;
                    }
                    break;
            }
        }
    }
    if ((match->flags & 0x20002) && match->alias->valuenumber != av->aliasnumber) {
        return FALSE;
    }
    return TRUE;
}

static inline void chooselocation(AvailableValue301* av, MOperand* op) {
    MOperand* baseop = RESULT_OP(av->pcode);
    ValueLabel301* labelled;

    for (labelled = av->labelled; labelled; labelled = labelled->next) {
        if (labelled->op.reg == baseop->reg) {
            *op = labelled->op;
            return;
        }
    }
    *op = av->labelled->op;
}

// 0x4B2860
static int findavailablevalue(MPCode* pcode, MOperand* op) {
    AvailableValue301* av;
    MOperand swap;

    for (av = opvalue[pcode->op]; av; av = av->next) {
        if (av->labelled &&
            av->pcode->flags == pcode->flags &&
            av->pcode->argcount == pcode->argcount) {
            if (!matchvalues(av, pcode)) {
                if (!(pcode->flags & 0x2000)) {
                    continue;
                }
                swap = pcode->args[1];
                pcode->args[1] = pcode->args[2];
                pcode->args[2] = swap;
                if (!matchvalues(av, pcode)) {
                    swap = pcode->args[1];
                    pcode->args[1] = pcode->args[2];
                    pcode->args[2] = swap;
                    continue;
                }
            }
            chooselocation(av, op);
#if MWCCPS2 && MWCCPS2_BUILD < 210
            return (int)av; // b145 returns the value itself
#else
            return TRUE;
#endif
        }
    }
    return FALSE;
}

// 0x4B2670
static void addavailablevalue(MPCode* pcode) {
    AvailableValue301* av;
    MOperand* op;
    int i;

    av = oalloc(offsetof(AvailableValue301, opnumbers) + sizeof(int) * pcode->argcount);
    av->labelled = NULL;
    av->pcode = pcode;
    for (i = 0, op = &pcode->args[0]; i < pcode->argcount; i++, op++) {
        if (op->kind == 0) {
            av->opnumbers[i] = REGVALUE(op)->number;
        }
    }
    if (pcode->flags & 0x20002) {
        av->aliasnumber = pcode->alias->valuenumber;
    }
    op = RESULT_OP(pcode);
    av->killedregister = killregister(op);
    labelvalue(av, op);
    REGVALUE(op)->available = av;
    av->next = opvalue[pcode->op];
    opvalue[pcode->op] = av;
}

// exactly one register is written
static inline int isCSEload(MPCode* pcode) {
    MOperand* op;
    int i;
    int count;

    if (pcode->flags & 0x180) {
        return FALSE;
    }
    count = 0;
    for (i = 0, op = &pcode->args[0]; i < pcode->argcount; i++, op++) {
        if (op->kind == 0 && (op->flags & 2)) {
            count++;
        }
    }
    return count == 1;
}

// no other operand is the result register
static inline int isCSEop(MPCode* pcode) {
    MOperand* baseOp = RESULT_OP(pcode);
    MOperand* op;
    int i;

#if !MWCCPS2 || MWCCPS2_BUILD >= 210
    if (pcode->flags & 0x180) {
        return FALSE;
    }
#endif
    for (i = 0, op = &pcode->args[0]; i < pcode->argcount; i++, op++) {
        if (op != baseOp &&
            op->kind == baseOp->kind &&
            op->rclass == baseOp->rclass &&
            op->reg == baseOp->reg) {
            return FALSE;
        }
    }
    return TRUE;
}

// the value is available in op: copy it to the result and drop pcode
static inline void reusevalue(MPCode* pcode, MOperand* op, MOperand* base) {
    if (REGVALUE(base)->number != REGVALUE(op)->number) {
        insertpcodebefore(pcode, func_00490280(op, base));
        copyregister(op, base);
    }
    deletepcode(pcode);
    removedcommonsubexpressions = TRUE;
}

#if MWCCPS2 && MWCCPS2_BUILD < 210
// b145 (LocalCSE.c) also numbers stores, in the list of each store opcode: a
// store of a value the location already holds goes away, and a load takes the
// value an earlier store left in a register. Its loads have no copy kinds
// (func_0048FFA0 only says yes or no)
extern UInt16 storeopcode(MPCode* pcode); // b145 0x49AF40: a load's store opcode

// drop store's entry from its opcode's list
static inline void unlinkstore(MPCode* store) {
    AvailableValue301** ptr = &opvalue[store->op];
    AvailableValue301* av;

    while ((av = *ptr)) {
        if (av->pcode == store) {
            *ptr = av->next;
            break;
        }
        ptr = &av->next;
    }
}

static inline Boolean storemayalias(MPCode* store, MPCode* pcode) {
    if (store->flags & 0x108) {
        return TRUE;
    }
    if (pcode->flags & 0x108) {
        return TRUE;
    }
    return may_alias((PCode*)store, (PCode*)pcode) ? TRUE : FALSE;
}

static inline void killstoresof(int opcode, MPCode* pcode) {
    AvailableValue301* av;

    for (av = opvalue[opcode]; av; av = av->next) {
        if (av->aliasnumber && storemayalias(av->pcode, pcode)) {
            unlinkstore(av->pcode);
            av->aliasnumber = 0;
        }
    }
}

// b145 0x4BCF20: forget the stores (and, unless pcode is a load, the loads)
// that pcode may touch
static void killstores(MPCode* pcode) {
    int opcode;

    for (opcode = 0xC3; opcode <= 0xD1; opcode++) {
        killstoresof(opcode, pcode);
    }
    for (opcode = MACRO_OP(0x6C); opcode <= MACRO_OP(0x80); opcode++) {
        killstoresof(opcode, pcode);
    }
    killstoresof(0x1D2, pcode);
    killstoresof(0x268, pcode);
    if (pcode->flags & 0x20002) {
        return;
    }
    for (opcode = 0x91; opcode <= 0xA3; opcode++) {
        killstoresof(opcode, pcode);
    }
    for (opcode = MACRO_OP(0x48); opcode <= MACRO_OP(0x5E); opcode++) {
        killstoresof(opcode, pcode);
    }
    killstoresof(0x1D1, pcode);
    killstoresof(0x264, pcode);
}

// b145 0x4BD3E0: the store pcode repeats, or whose value the load pcode reads.
// The flags test is b145's as written: `!=` binds before `&`
static AvailableValue301* findstore(MPCode* pcode) {
    AvailableValue301* av;
    MOperand swap;

    for (av = opvalue[storeopcode(pcode)]; av; av = av->next) {
        if (av->aliasnumber &&
            !(av->pcode->flags & ~0x60006 != pcode->flags & ~0x60006) &&
            av->pcode->argcount == pcode->argcount) {
            if (matchvalues(av, pcode)) {
                return av;
            }
            if (pcode->flags & 0x2000) {
                swap = pcode->args[1];
                pcode->args[1] = pcode->args[2];
                pcode->args[2] = swap;
                if (matchvalues(av, pcode)) {
                    return av;
                }
                swap = pcode->args[1];
                pcode->args[1] = pcode->args[2];
                pcode->args[2] = swap;
            }
        }
    }
    return NULL;
}

// b145 0x4BD590
static void addstore(MPCode* pcode) {
    AvailableValue301* av;
    MOperand* op;
    int i;

    if (pcode->flags & 0x80) {
        return;
    }
    av = oalloc(offsetof(AvailableValue301, opnumbers) + sizeof(int) * pcode->argcount);
    av->labelled = NULL;
    av->pcode = pcode;
    for (i = 0, op = &pcode->args[0]; i < pcode->argcount; i++, op++) {
        if (op->kind == 0) {
            av->opnumbers[i] = REGVALUE(op)->number;
        }
    }
    if (pcode->flags & 0x40004) {
        av->aliasnumber = pcode->alias->valuenumber;
    }
    av->killedregister = nextvaluenumber++;
    av->next = opvalue[pcode->op];
    opvalue[pcode->op] = av;
}

// b145 0x4BCDA0: for a store, delete an earlier identical store of this
// level; for a load, the store whose value it can take (its register in op).
// The store returned, or NULL
static MPCode* storecse(MPCode* pcode, Boolean isload, MOperand* op) {
    AvailableValue301* av;
    MPCode* store;

    if (!(pcode->flags & 0x80)) {
        if ((av = findstore(pcode))) {
            store = av->pcode;
            if (!isload || func_0048FFA0(store, pcode)) {
                if (!store || !store->block) {
                    CError_Internal("LocalCSE.c", 1089);
                }
                if (store->flags & 0x80) {
                    CError_Internal("LocalCSE.c", 1091);
                }
                if (isload) {
                    if (av->aliasnumber == REGVALUE(&store->args[0])->number) {
                        *op = store->args[0];
                        return store;
                    }
                } else if (av->killedregister >= valueceiling || nextvaluenumber < valueceiling) {
                    deletepcode(store);
                    unlinkstore(store);
                    return store;
                }
            }
        } else if (!(pcode->flags & 0x40004) && findavailablevalue(pcode, op)) {
            return NULL;
        }
    }
    killstores(pcode);
    return NULL;
}

// b145 0x4BC4D0
static void pointerstore(MPCode* pcode) {
    MOperand op;

    if (storecse(pcode, FALSE, &op)) {
        removedcommonsubexpressions = TRUE;
    }
    update_alias_value(pcode->alias, (PCode*)pcode);
    addstore(pcode);
    killregisters(pcode);
}

// b145 0x4BC940 (the same code)
static void simplestore(MPCode* pcode) {
    MOperand op;

    if (storecse(pcode, FALSE, &op)) {
        removedcommonsubexpressions = TRUE;
    }
    update_alias_value(pcode->alias, (PCode*)pcode);
    addstore(pcode);
    killregisters(pcode);
}

// b145 0x4BC630
static void pointerload(MPCode* pcode, Boolean propagate) {
    MOperand* base = RESULT_OP(pcode);
    MOperand op;
    MPCode* stored = storecse(pcode, TRUE, &op);

    if ((pcode->flags & 0x80) || !isCSEload(pcode)) {
        killregisters(pcode);
    } else if (propagate && findavailablevalue(pcode, &op)) {
        reusevalue(pcode, &op, base);
    } else if (stored) {
        func_0048FFA0(stored, pcode);
        reusevalue(pcode, &op, base);
    } else {
        addavailablevalue(pcode);
    }
}

// b145 0x4BCAA0
static void simpleload(MPCode* pcode, Boolean propagate) {
    MOperand* base = RESULT_OP(pcode);
    MOperand op;
    MPCode* stored = storecse(pcode, TRUE, &op);

    if ((pcode->flags & 0x80) || !isCSEload(pcode)) {
        killregisters(pcode);
    } else if (propagate && findavailablevalue(pcode, &op)) {
        reusevalue(pcode, &op, base);
    } else if (stored) {
        reusevalue(pcode, &op, base);
    } else {
        addavailablevalue(pcode);
    }
}
#else
// 0x4B20B0
static void pointerload(MPCode* pcode, Boolean propagate) {
    MOperand* base = RESULT_OP(pcode);
    MOperand op;

    if ((pcode->flags & 0x200080) || !isCSEload(pcode)) {
        killregisters(pcode);
    } else if (propagate && findavailablevalue(pcode, &op)) {
        reusevalue(pcode, &op, base);
    } else {
        addavailablevalue(pcode);
    }
}

// 0x4B2300
static void simpleload(MPCode* pcode, Boolean propagate) {
    MOperand* base = RESULT_OP(pcode);
    MOperand op;
    MPCode* rs;
    MPCode* pc;
    MPCode* copy;
    SInt32 kind;

    if ((pcode->flags & 0x200080) || !isCSEload(pcode)) {
        killregisters(pcode);
        return;
    }
    if (propagate && findavailablevalue(pcode, &op)) {
        reusevalue(pcode, &op, base);
        return;
    }
    pc = (MPCode*)pcode->alias->valuepcode;
    if (pc &&
        !(pc->flags & 0x200000) &&
        pcode->alias->valuenumber == REGVALUE(&pc->args[0])->number) {
        op = pc->args[0];
        rs = pc;
    } else {
        rs = NULL;
    }
    if (rs) {
        kind = func_0048FFA0(rs, pcode);
    }
    if (propagate && rs && kind) {
        if (kind != 1 || !samevalue(&op, base)) {
            copy = func_0048FDC0(kind, &op, base);
            insertpcodebefore(pcode, copy);
            if (copy->flags & 0x10) {
                copyregister(&op, base);
            } else {
                addavailablevalue(copy);
            }
        }
        deletepcode(pcode);
        removedcommonsubexpressions = TRUE;
    } else {
        addavailablevalue(pcode);
    }
}
#endif

// 0x4B1C80
static void operatefrommemory(MPCode* pcode, Boolean propagate) {
    MOperand* base = RESULT_OP(pcode);
    MOperand op;

    if ((pcode->flags & 0x4000) && isCSEop(pcode) && !(pcode->flags & 0x80)) {
#if MWCCPS2 && MWCCPS2_BUILD < 210
        if (findavailablevalue(pcode, &op)) { // b145 does not test propagate
#else
        if (propagate && findavailablevalue(pcode, &op)) {
#endif
            reusevalue(pcode, &op, base);
        } else {
            addavailablevalue(pcode);
        }
    } else {
        killregisters(pcode);
    }
}

// 0x4B1F60
static void functioncall(MPCode* pcode) {
    killregisters(pcode);
#if MWCCPS2 && MWCCPS2_BUILD < 210
    killstores(pcode);
#endif
    if (coloring) {
        update_all_alias_values();
        killallCSEs();
    } else {
        update_alias_value(pcode->alias, NULL);
    }
}

#if MWCCPS2
#define VN_VU0OP(op) ((op) > 0x21E && (op) <= 0x2E7)
#define VN_MOVE_DST(pc) (VN_VU0OP((pc)->op) ? &(pc)->args[1] : &(pc)->args[0])
#define VN_MOVE_SRC(pc) (VN_VU0OP((pc)->op) ? &(pc)->args[2] : &(pc)->args[1])
#endif

// 0x4B1470
static void removecsesfrombasicblock(MPCodeBlock* block) {
    MPCode* pcode;
    MPCode* next;
    MOperand* op;
    MOperand* base;
    MOperand buf;
    RegValue301* value;
#if MWCCPS2 && MWCCPS2_BUILD < 210
    AvailableValue301* av;
#endif
    Boolean propagate;
    int i;

    propagate = !(block->flags2E & 0x180) && block->pcodecount != 0;
    for (pcode = block->first; pcode; pcode = next) {
        next = pcode->next;
        if (propagate) {
            for (i = 0, op = &pcode->args[0]; i < pcode->argcount; i++, op++) {
                if (op->kind == 0 &&
                    (op->flags & 0xB) == 1 &&
                    op->reg >= data_00624744[op->rclass]) {
                    value = REGVALUE(op);
                    if (value->copyreg &&
                        value->copyreg >= data_00624744[op->rclass] &&
                        value->number == regvalue[op->rclass][value->copyreg].number) {
                        op->reg = value->copyreg;
                    }
                }
            }
        }
        if (pcode->flags & 0x10) {
#if MWCCPS2
            // b210: VU0 instructions (0x21F..0x2E7) have the dest field first
            if (propagate && samevalue(VN_MOVE_DST(pcode), VN_MOVE_SRC(pcode))) {
                deletepcode(pcode);
                removedcommonsubexpressions = TRUE;
            } else {
                copyregister(VN_MOVE_SRC(pcode), VN_MOVE_DST(pcode));
            }
#else
            if (propagate && samevalue(&pcode->args[0], &pcode->args[1])) {
                deletepcode(pcode);
                removedcommonsubexpressions = TRUE;
            } else {
                copyregister(&pcode->args[1], &pcode->args[0]);
            }
#endif
        } else if (pcode->flags & 8) {
            functioncall(pcode);
        } else if (pcode->flags & 2) {
            if (pcode->flags & 0x20) {
                pointerload(pcode, propagate);
            } else {
                simpleload(pcode, propagate);
            }
        } else if (pcode->flags & 4) {
#if MWCCPS2 && MWCCPS2_BUILD < 210
            if (pcode->flags & 0x20) {
                pointerstore(pcode);
            } else {
                simplestore(pcode);
            }
#else
            update_alias_value(pcode->alias, (PCode*)pcode);
            killregisters(pcode);
#endif
        } else if (pcode->flags & 0x20000) {
            operatefrommemory(pcode, propagate);
        } else if (pcode->flags & 0x40000) {
#if MWCCPS2 && MWCCPS2_BUILD < 210
            CError_Internal("LocalCSE.c", 1405);
            update_alias_value(pcode->alias, NULL);
#else
            update_alias_value(pcode->alias, (PCode*)pcode);
#endif
            killregisters(pcode);
        } else if ((pcode->flags & 0x4000) && isCSEop(pcode)) {
            base = RESULT_OP(pcode);
#if MWCCPS2 && MWCCPS2_BUILD < 210
            // b145: no propagate test; a %hi's partner is pointed at the kept value
            if ((av = (AvailableValue301*)findavailablevalue(pcode, &buf))) {
                if (REGVALUE(base)->number != REGVALUE(&buf)->number) {
                    insertpcodebefore(pcode, func_00490280(&buf, base));
                    copyregister(&buf, base);
                    if (pcode->unk38 == 5) {
                        pcode->pair->pair = av->pcode;
                    }
                }
                deletepcode(pcode);
                removedcommonsubexpressions = TRUE;
            } else {
                addavailablevalue(pcode);
            }
#else
            if (propagate && findavailablevalue(pcode, &buf)) {
                reusevalue(pcode, &buf, base);
            } else {
                addavailablevalue(pcode);
            }
#endif
        } else {
            killregisters(pcode);
        }
    }
    block->flags2E |= 4;
}

// 0x4B1360
static void regressvaluestate(void) {
    AvailableValue301* av;
    AvailableValue301** ptr;
    StackedValue301* stacked;
    int i;

    for (i = 0; i < OPCODE_MAX_301; i++) {
        ptr = &opvalue[i];
        while ((av = *ptr)) {
            if (av->killedregister >= valueceiling) {
                *ptr = av->next;
            } else {
                ptr = &av->next;
            }
        }
    }
    for (stacked = stackedvalues; stacked; stacked = stacked->next) {
        unstackvalue(stacked);
    }
}

// a successor whose only predecessor is block, not yet visited
static inline int ExtendsBlock301(MPBlockLink* succ) {
    return !(succ->block->flags2E & 4) &&
        succ->block->predecessors &&
        !succ->block->predecessors->nextpred;
}

// 0x4B1280
static void removecsesfromextendedbasicblock(MPCodeBlock* block) {
    MPBlockLink* succ;
    StackedValue301* savedStack;
    int savedCeiling;
    int counter;

    removecsesfrombasicblock(block);
    while (block->successors &&
           !block->successors->next &&
           block->successors->block->predecessors &&
           !block->successors->block->predecessors->nextpred) {
        removecsesfrombasicblock(block = block->successors->block);
    }
    counter = 0;
    for (succ = block->successors; succ; succ = succ->next) {
        if (ExtendsBlock301(succ)) {
            counter++;
        }
    }
    if (!counter) {
        return;
    }
    savedCeiling = valueceiling;
    savedStack = stackedvalues;
    stackedvalues = NULL;
    valueceiling = nextvaluenumber;
    for (succ = block->successors; succ; succ = succ->next) {
        if (ExtendsBlock301(succ)) {
            removecsesfromextendedbasicblock(succ->block);
            regressvaluestate();
        }
    }
    stackedvalues = savedStack;
    valueceiling = savedCeiling;
}

// 0x4B2F70
static void initializecsedatastructures(void) {
    RegValue301* rv;
    char rclass;
    int i;

    nextvaluenumber = 0;
    for (i = 0; i < OPCODE_MAX_301; i++) {
        opvalue[i] = NULL;
    }
    for (rclass = 0; rclass < 7; rclass++) {
        rv = regvalue[rclass];
        for (i = 0; i < data_00625A98[rclass]; i++, rv++) {
            rv->number = nextvaluenumber++;
            rv->copyreg = 0;
            rv->available = NULL;
        }
    }
    initialize_alias_values();
    stackedvalues = NULL;
    valueceiling = 0x7FFFFFFF;
}

// 0x4B11D0
#if MWCCPS2 && MWCCPS2_BUILD < 210
// b145: no argument (no more-aggressive mode); a char class counter
void func_004B11D0(void) {
    MPCodeBlock* block;
    char rclass;

#else
void func_004B11D0(int moreAggressive) {
    MPCodeBlock* block;
    int rclass;

    moreaggressiveoptimization = moreAggressive;
#endif
    removedcommonsubexpressions = FALSE;
    gather_alias_info();
    for (rclass = 0; rclass < 7; rclass++) {
        regvalue[rclass] = oalloc(sizeof(RegValue301) * data_00625A98[rclass]);
    }
    for (block = data_00624FD0; block; block = block->next) {
        block->flags2E &= ~4;
    }
    for (block = data_00624FD0; block; block = block->next) {
        if (!(block->flags2E & 4)) {
            initializecsedatastructures();
            removecsesfromextendedbasicblock(block);
        }
    }
    freeoheap();
}

#else
#include "compiler/ValueNumbering.h"
#include "compiler/Alias.h"
#include "compiler/CError.h"
#include "compiler/CompilerTools.h"
#include "compiler/PCode.h"
#include "compiler/PCodeInfo.h"
#include "compiler/RegisterInfo.h"
#include "compiler/Registers.h"

typedef struct ValueLabel {
    struct ValueLabel* next;
    PCodeArg op;
} ValueLabel;

typedef struct AvailableValue {
    struct AvailableValue* next;
    ValueLabel* labelled;
    PCode* pcode;
    int killedregister;
    int aliasnumber;
    int opnumbers[0];
} AvailableValue;

typedef struct RegValue {
    int number;
    int copyreg;
    AvailableValue* available;
} RegValue;

typedef struct State {
    void* stackedvalues;
    int valueceiling;
} State;

typedef struct StackedValue {
    struct StackedValue* next;
    PCodeArg op;
    RegValue value;
    Alias* alias;
    PCode* valuepcode;
} StackedValue;

int removedcommonsubexpressions;
int nextvaluenumber;
static AvailableValue* opvalue[OPCODE_MAX];
static RegValue* regvalue[RegClassMax];
static StackedValue* stackedvalues;
static int valueceiling;
static int moreaggressiveoptimization;

static void allocatecsedatastructures(void) {
    RegClass rclass;

    for (rclass = 0; rclass < RegClassMax; rclass++) {
        regvalue[rclass] = oalloc(sizeof(RegValue) * used_virtual_registers[rclass]);
    }
}

static void initializecsedatastructures(void) {
    RegValue* rv;
    RegClass rclass;
    int i;

    nextvaluenumber = 0;

    for (i = 0; i < OPCODE_MAX; i++) {
        opvalue[i] = NULL;
    }

    for (rclass = 0; rclass < RegClassMax; rclass++) {
        rv = regvalue[rclass];
        for (i = 0; i < used_virtual_registers[rclass]; i++, rv++) {
            rv->number = nextvaluenumber++;
            rv->copyreg = 0;
            rv->available = NULL;
        }
    }

    initialize_alias_values();
    stackedvalues = NULL;
    valueceiling = 0x7FFFFFFF;
}

static void labelvalue(AvailableValue* av, PCodeArg* op) {
    ValueLabel* label = oalloc(sizeof(ValueLabel));
    label->op = *op;
    label->next = av->labelled;
    av->labelled = label;
}

static void unlabelvalue(AvailableValue* av, PCodeArg* op) {
    ValueLabel* labelled;
    ValueLabel** ptr;

    ptr = &av->labelled;
    while ((labelled = *ptr)) {
        if (labelled->op.data.reg.reg == op->data.reg.reg) {
            *ptr = labelled->next;
        } else {
            ptr = &labelled->next;
        }
    }
}

static void stackregistervalue(PCodeArg* op, RegValue* value) {
    StackedValue* stacked = oalloc(sizeof(StackedValue));
    stacked->next = stackedvalues;
    stackedvalues = stacked;

    stacked->op = *op;
    stacked->value = *value;
}

static void stackmemoryvalue(Alias* alias) {
    StackedValue* stacked = oalloc(sizeof(StackedValue));
    stacked->next = stackedvalues;
    stackedvalues = stacked;

    stacked->op.kind = PCOp_MEMORY;
    stacked->alias = alias;
    stacked->value.number = alias->valuenumber;
    stacked->valuepcode = alias->valuepcode;
}

static void unstackvalue(StackedValue* stacked) {
    PCodeArg* op = &stacked->op;
    RegValue* value;

    if (op->kind == PCOp_MEMORY) {
        stacked->alias->valuenumber = stacked->value.number;
        stacked->alias->valuepcode = stacked->valuepcode;
    } else {
        value = &regvalue[op->arg][op->data.reg.reg];
        if (value->available) {
            unlabelvalue(value->available, op);
        }
        value->number = stacked->value.number;
        value->copyreg = stacked->value.copyreg;
        value->available = stacked->value.available;
        if (value->available) {
            labelvalue(value->available, op);
        }
    }
}

static int samevalue(PCodeArg* op1, PCodeArg* op2) {
    return regvalue[op1->arg][op1->data.reg.reg].number ==
        regvalue[op2->arg][op2->data.reg.reg].number;
}

static int killregister(PCodeArg* op) {
    RegValue* value;

    value = &regvalue[op->arg][op->data.reg.reg];
    if (value->number < valueceiling && nextvaluenumber >= valueceiling) {
        stackregistervalue(op, value);
    }

    if (value->available) {
        unlabelvalue(value->available, op);
    }
    value->available = NULL;
    value->copyreg = 0;
    return value->number = nextvaluenumber++;
}

void killmemory(Alias* alias, PCode* newValue) {
    if (alias->valuenumber < valueceiling && nextvaluenumber >= valueceiling) {
        stackmemoryvalue(alias);
    }

    if (newValue) {
        alias->valuenumber = regvalue[newValue->args[0].arg][newValue->args[0].data.reg.reg].number;
        alias->valuepcode = newValue;
    } else {
        alias->valuenumber = nextvaluenumber++;
        alias->valuepcode = NULL;
    }
}

static void killspecificCSEs(short op) {
    AvailableValue* av;
    ValueLabel* labelled;

    for (av = opvalue[op]; av; av = av->next) {
        for (labelled = av->labelled; labelled; labelled = labelled->next) {
            killregister(&labelled->op);
        }
    }
}

static void killallCSEs(void) {
    AvailableValue* av;
    ValueLabel* labelled;
    int i;

    for (i = 0; i < OPCODE_MAX; i++) {
        for (av = opvalue[i]; av; av = av->next) {
            for (labelled = av->labelled; labelled; labelled = labelled->next) {
                killregister(&labelled->op);
            }
        }
    }
}

static void killregisters(PCode* pcode) {
    PCodeArg* op;
    int i;

    for (i = 0, op = pcode->args; i < pcode->argCount; i++, op++) {
        if (op->kind == PCOp_REGISTER && (op->data.reg.effect & EffectWrite)) {
            killregister(op);
        }
    }
}

static void copyregister(PCodeArg* src, PCodeArg* dest) {
    RegValue* srcvalue;
    RegValue* destvalue;

    srcvalue = &regvalue[src->arg][src->data.reg.reg];
    destvalue = &regvalue[dest->arg][dest->data.reg.reg];

    if (destvalue->number < valueceiling && nextvaluenumber >= valueceiling) {
        stackregistervalue(dest, destvalue);
    }

    if (destvalue->available) {
        unlabelvalue(destvalue->available, dest);
    }
    destvalue->available = srcvalue->available;
    if (destvalue->available) {
        labelvalue(destvalue->available, dest);
    }

    destvalue->number = srcvalue->number;

    if (srcvalue->copyreg && srcvalue->number == regvalue[src->arg][srcvalue->copyreg].number) {
        destvalue->copyreg = srcvalue->copyreg;
    } else {
        destvalue->copyreg = src->data.reg.reg;
    }
}

static int matchvalues(AvailableValue* av, PCode* match) {
    PCodeArg* avOp;
    PCodeArg* matchOp;
    int i;

    for (avOp = &av->pcode->args[0], matchOp = &match->args[0], i = 0; i < match->argCount;
         i++, avOp++, matchOp++) {
        if (i != 0) {
            switch (avOp->kind) {
                case PCOp_REGISTER:
                    if (av->opnumbers[i] != regvalue[matchOp->arg][matchOp->data.reg.reg].number) {
                        return FALSE;
                    }
                    break;
                case PCOp_MEMORY:
                    if (matchOp->kind != PCOp_MEMORY) {
                        return FALSE;
                    }
                    if (matchOp->data.mem.obj != avOp->data.mem.obj) {
                        return FALSE;
                    }
                    if (matchOp->data.mem.offset != avOp->data.mem.offset) {
                        return FALSE;
                    }
                    if ((unsigned char)matchOp->arg != (unsigned char)avOp->arg) {
                        return FALSE;
                    }
                    break;
                case PCOp_IMMEDIATE:
                    if (matchOp->kind != PCOp_IMMEDIATE) {
                        return FALSE;
                    }
                    if (matchOp->data.imm.value != avOp->data.imm.value) {
                        return FALSE;
                    }
                    break;
                case PCOp_LABEL:
                    if (matchOp->kind != PCOp_LABEL) {
                        return FALSE;
                    }
                    if (matchOp->data.label.label != avOp->data.label.label) {
                        return FALSE;
                    }
                    break;
                case PCOp_SYSREG:
                    if (matchOp->data.reg.reg != 912) {
                        return FALSE;
                    }
                    if (matchOp->data.reg.reg != avOp->data.reg.reg) {
                        return FALSE;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    if ((match->flags & (fIsRead | fOpFromMemory)) &&
        match->alias->valuenumber != av->aliasnumber) {
        return FALSE;
    }

    return TRUE;
}

static void chooselocation(AvailableValue* av, PCodeArg* op) {
    PCodeArg* baseop;
    ValueLabel* labelled;

    baseop = &av->pcode->args[0];
    labelled = av->labelled;
    while (labelled) {
        if (labelled->op.data.reg.reg == baseop->data.reg.reg) {
            *op = labelled->op;
            return;
        }
        labelled = labelled->next;
    }

    *op = av->labelled[0].op;
}

static int findavailablevalue(PCode* pcode, PCodeArg* op) {
    AvailableValue* av;
    PCodeArg swap;
    PCodeArg unswap;

    for (av = opvalue[pcode->op]; av; av = av->next) {
        if (av->labelled &&
            av->pcode->flags == pcode->flags &&
            av->pcode->argCount == pcode->argCount) {
            if (!matchvalues(av, pcode)) {
                if (!(pcode->flags & fCommutative)) {
                    continue;
                }

                swap = pcode->args[1];
                pcode->args[1] = pcode->args[2];
                pcode->args[2] = swap;

                if (!matchvalues(av, pcode)) {
                    unswap = pcode->args[1];
                    pcode->args[1] = pcode->args[2];
                    pcode->args[2] = unswap;
                    continue;
                }
            }
            chooselocation(av, op);
            return TRUE;
        }
    }

    return FALSE;
}

static void addavailablevalue(PCode* pcode) {
    AvailableValue* av;
    PCodeArg* op;
    int i;

    av = oalloc(sizeof(AvailableValue) + sizeof(int) * pcode->argCount);
    av->labelled = NULL;
    av->pcode = pcode;
    for (i = 0, op = &pcode->args[0]; i < pcode->argCount; i++, op++) {
        if (op->kind == PCOp_REGISTER) {
            av->opnumbers[i] = regvalue[op->arg][op->data.reg.reg].number;
        }
    }

    if (pcode->flags & (fIsRead | fOpFromMemory)) {
        av->aliasnumber = pcode->alias->valuenumber;
    }

    op = &pcode->args[0];
    av->killedregister = killregister(op);
    labelvalue(av, op);
    regvalue[op->arg][op->data.reg.reg].available = av;
    av->next = opvalue[pcode->op], opvalue[pcode->op] = av;
}

static int isCSEop(PCode* pcode) {
    PCodeArg* baseOp;
    PCodeArg* op;
    int i;

    baseOp = &pcode->args[0];

    switch (pcode->op) {
        case PC_CMPI:
        case PC_CMP:
        case PC_CMPLI:
        case PC_CMPL:
        case PC_FCMPU:
        case PC_FCMPO:
            if (!moreaggressiveoptimization) {
                return FALSE;
            }
            break;
        case PC_LI:
        case PC_LIS:
            if (!moreaggressiveoptimization) {
                return FALSE;
            }
            if (pcode->args[0].data.reg.reg < first_fe_temporary_register[RegClass_GPR] ||
                pcode->args[0].data.reg.reg > last_temporary_register[RegClass_GPR]) {
                return FALSE;
            }
            break;
    }

    if (PCODE_FLAG_SET_F(pcode) &
        (fIsVolatile | fSideEffects | fOverflow | fSetsCarry | fRecordBit)) {
        return FALSE;
    }

    for (i = 0, op = &pcode->args[0]; i < pcode->argCount; i++, op++) {
        if (op != baseOp &&
            op->kind == baseOp->kind &&
            op->arg == baseOp->arg &&
            op->data.reg.reg == baseOp->data.reg.reg) {
            return FALSE;
        }
    }

    return TRUE;
}

static int isCSEload(PCode* pcode) {
    PCodeArg* op;
    int i;
    int count;

    count = 0;
    for (i = 0, op = &pcode->args[0]; i < pcode->argCount; i++, op++) {
        if (op->kind == PCOp_REGISTER && (op->data.reg.effect & EffectWrite)) {
            count++;
        }
    }

    return count == 1;
}

static void registercopy(PCode* pcode) {
    PCodeArg* dest;
    PCodeArg* src;

    dest = &pcode->args[0];
    src = &pcode->args[1];
    if (samevalue(src, dest)) {
        deletepcode(pcode);
    } else {
        copyregister(src, dest);
    }
}

static PCode* recentlystored(Alias* alias, PCodeArg* op) {
    PCode* pc;
    if ((pc = alias->valuepcode) &&
        alias->valuenumber == regvalue[pc->args[0].arg][pc->args[0].data.reg.reg].number) {
        *op = pc->args[0];
        return pc;
    } else {
        return NULL;
    }
}

static void simpleload(PCode* pcode) {
    PCodeArg* origOp;
    PCodeArg op;
    PCode* rs;
    int kind;
    PCode* copy;

    origOp = &pcode->args[0];
    if ((pcode->flags & fIsVolatile) || !isCSEload(pcode)) {
        killregisters(pcode);
    } else if (findavailablevalue(pcode, &op)) {
        if (!samevalue(origOp, &op)) {
            insertpcodebefore(pcode, makecopyinstruction(&op, origOp));
            copyregister(&op, origOp);
        }
        deletepcode(pcode);
        removedcommonsubexpressions = TRUE;
    } else {
        rs = recentlystored(pcode->alias, &op);
        if (rs) {
            kind = can_reuse_stored_value(rs, pcode);
        }
        if (rs && kind) {
            if (kind != 1 || !samevalue(origOp, &op)) {
                copy = makecopyforload(kind, &op, origOp);
                insertpcodebefore(pcode, copy);
                if (copy->flags & fIsMove) {
                    copyregister(&op, origOp);
                } else {
                    addavailablevalue(copy);
                }
            }
            deletepcode(pcode);
            removedcommonsubexpressions = TRUE;
        } else {
            addavailablevalue(pcode);
        }
    }
}

static void simplestore(PCode* pcode) {
    update_alias_value(pcode->alias, pcode);
#if VERSION == VERSION_GC_2_0P2
    update_small_data_values(pcode);
#endif
    killregisters(pcode);
}

static void pointerload(PCode* pcode) {
    PCodeArg* op;
    PCodeArg buf;

    op = &pcode->args[0];

    if ((pcode->flags & fIsVolatile) || !isCSEload(pcode)) {
        killregisters(pcode);
    } else if (findavailablevalue(pcode, &buf)) {
        if (!samevalue(op, &buf)) {
            insertpcodebefore(pcode, makecopyinstruction(&buf, op));
            copyregister(&buf, op);
        }
        deletepcode(pcode);
        removedcommonsubexpressions = TRUE;
    } else {
        addavailablevalue(pcode);
    }
}

static void pointerstore(PCode* pcode) {
    update_alias_value(pcode->alias, NULL);
#if VERSION == VERSION_GC_2_0P2
    update_small_data_values(pcode);
#endif
    killregisters(pcode);
}

static void arithmeticop(PCode* pcode) {
    PCodeArg* op;
    PCodeArg buf;

    op = &pcode->args[0];

    if (findavailablevalue(pcode, &buf)) {
        if (!samevalue(op, &buf)) {
            insertpcodebefore(pcode, makecopyinstruction(&buf, op));
            copyregister(&buf, op);
        }
        deletepcode(pcode);
        removedcommonsubexpressions = TRUE;
    } else {
        addavailablevalue(pcode);
    }
}

static void functioncall(PCode* pcode) {
    killregisters(pcode);
    if (coloring) {
        update_all_alias_values();
        killallCSEs();
    } else {
        update_alias_value(pcode->alias, NULL);
    }
}

static void operatefrommemory(PCode* pcode) {
    CError_FATAL(1015);
}

static void operatetomemory(PCode* pcode) {
    CError_FATAL(1046);
}

static void propagatecopiesto(PCode* pcode) {
    PCodeArg* op;
    int i;
    RegValue* value;

    for (i = 0, op = &pcode->args[0]; i < pcode->argCount; i++, op++) {
        if (op->kind == PCOp_REGISTER &&
            (op->data.reg.effect & (EffectRead | EffectWrite | EffectPinned)) == EffectRead &&
            op->data.reg.reg >= n_real_registers[op->arg]) {
            value = &regvalue[op->arg][op->data.reg.reg];
            if (value->copyreg &&
                value->copyreg >= n_real_registers[op->arg] &&
                value->number == regvalue[op->arg][value->copyreg].number) {
                op->data.reg.reg = value->copyreg;
            }
        }
    }
}

static void removecsesfrombasicblock(PCodeBlock* block) {
    PCode* pcode;
    PCode* next;

    for (pcode = block->firstPCode; pcode; pcode = next) {
        next = pcode->nextPCode;
        propagatecopiesto(pcode);
        if (pcode->flags & fIsMove) {
            registercopy(pcode);
        } else if ((pcode->flags & fIsCall) && (pcode->flags & (fLink | fSideEffects))) {
            functioncall(pcode);
        } else if (pcode->flags & fIsRead) {
            if (pcode->flags & fIsPtrOp) {
                pointerload(pcode);
            } else {
                simpleload(pcode);
            }
        } else if (pcode->flags & fIsWrite) {
            if (pcode->flags & fIsPtrOp) {
                pointerstore(pcode);
            } else {
                simplestore(pcode);
            }
        } else if (pcode->flags & fOpFromMemory) {
            operatefrommemory(pcode);
        } else if (pcode->flags & fOpToMemory) {
            operatetomemory(pcode);
        } else if ((pcode->flags & fIsCSE) && isCSEop(pcode)) {
            arithmeticop(pcode);
        } else {
            killregisters(pcode);
        }
    }

    block->flags |= fVisited;
}

static void getvaluestate(State* state) {
    state->stackedvalues = stackedvalues;
    state->valueceiling = valueceiling;
}

static void setvaluestate(State* state) {
    stackedvalues = state->stackedvalues;
    valueceiling = state->valueceiling;
}

static void forkvaluestate(int number) {
    stackedvalues = NULL;
    valueceiling = number;
}

static void regressvaluestate(void) {
    AvailableValue* av;
    AvailableValue** ptr;
    int i;
    StackedValue* stacked;

    for (i = 0; i < OPCODE_MAX; i++) {
        ptr = &opvalue[i];
        while ((av = *ptr)) {
            if (av->killedregister >= valueceiling) {
                *ptr = av->next;
            } else {
                ptr = &av->next;
            }
        }
    }

    for (stacked = stackedvalues; stacked; stacked = stacked->next) {
        unstackvalue(stacked);
    }
}

static void removecsesfromextendedbasicblock(PCodeBlock* block) {
    PCLink* succ;
    int counter;
    State state;

    removecsesfrombasicblock(block);
    while (block->successors &&
           !block->successors->nextLink &&
           block->successors->block->predecessors &&
           !block->successors->block->predecessors->nextLink) {
        removecsesfrombasicblock(block = block->successors->block);
    }

    counter = 0;
    for (succ = block->successors; succ; succ = succ->nextLink) {
        if (!(succ->block->flags & fVisited) &&
            succ->block->predecessors &&
            !succ->block->predecessors->nextLink) {
            counter++;
        }
    }

    if (!counter) {
        return;
    }

    getvaluestate(&state);
    forkvaluestate(nextvaluenumber);
    for (succ = block->successors; succ; succ = succ->nextLink) {
        if (!(succ->block->flags & fVisited) &&
            succ->block->predecessors &&
            !succ->block->predecessors->nextLink) {
            removecsesfromextendedbasicblock(succ->block);
            regressvaluestate();
        }
    }
    setvaluestate(&state);
}

void removecommonsubexpressions(Object* proc, int moreAggressive) {
    PCodeBlock* block;

    moreaggressiveoptimization = moreAggressive;
    removedcommonsubexpressions = FALSE;
    gather_alias_info();
    allocatecsedatastructures();

    for (block = pcbasicblocks; block; block = block->nextBlock) {
        block->flags &= ~fVisited;
    }

    for (block = pcbasicblocks; block; block = block->nextBlock) {
        if (!(block->flags & fVisited)) {
            initializecsedatastructures();
            removecsesfromextendedbasicblock(block);
        }
    }

    freeoheap();
}
#endif
