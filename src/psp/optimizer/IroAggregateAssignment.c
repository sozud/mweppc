// mwccpsp 3.0.1: IroAggregateAssignment.c (by its asserts), not in the 2.4.7
// source. Decomposing a struct copy into copies of its members
// (#pragma opt_decomposeaggregates), and recomposing them.
//
//     *&a = *&b      becomes   *(&a + 0) = *(&b + 0); *(&a + 4) = *(&b + 4); ...
//
// Base classes come first, then the members; a member that is itself a
// struct is decomposed in turn.
#include "compiler/common.h"
#include "compiler/CClass.h"
#include "compiler/CError.h"
#include "compiler/CInt64.h"
#include "compiler/CMachine.h"
#include "compiler/CParser.h"
#include "compiler/CExpr.h"
#include "compiler/CDecl.h"
#include "compiler/CompilerTools.h"
#include "compiler/enode.h"
#include "compiler/IroDump.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroMalloc.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"
#include "compiler/BitVector.h"
#include "compiler/IroLoop301.h"
#include "compiler/IROUseDef301.h"
#include "compiler/objects.h"
#include "compiler/types.h"
#if MWCCPS2
#include <string.h> // b210 expands memset inline
#endif

extern void IRO_UpdateFlagsOnInts(IROLinear* linear);

// the members of a struct, or the bases and then the members of a class
typedef struct MemberIter {
    Type* type;
    void* member; // StructMember or ObjMemberVar
    ClassList* base;
} MemberIter;

// --- IroType.c ------------------------------------------------------------

// 0x55A650: the type of an offset added to an address
#if MWCCPS2
__declspec(noinline) // b210 calls it (func_0057DC90)
#endif
Type* func_0055A650(Type* type) {
    return TYPE(&stunsignedint);
}

// --- 0x57F950.. -----------------------------------------------------------

// 0x57FA50
void func_0057FA50(Type* type, MemberIter* iter) {
    iter->type = type;
    if (type->type == TYPESTRUCT) {
        iter->member = TYPE_STRUCT(type)->members;
    } else {
        iter->base = TYPE_CLASS(type)->bases;
        iter->member = TYPE_CLASS(type)->ivars;
    }
}

static inline Type* IterType(MemberIter* iter) {
    if (iter->type->type == TYPESTRUCT) {
        return iter->member ? ((StructMember*)iter->member)->type : NULL;
    }
    if (iter->base) {
        return TYPE(iter->base->base);
    }
    return iter->member ? ((ObjMemberVar*)iter->member)->type : NULL;
}

static inline SInt32 IterOffset(MemberIter* iter) {
    if (iter->type->type == TYPESTRUCT) {
        return iter->member ? ((StructMember*)iter->member)->offset : 0;
    }
    if (iter->base) {
        return iter->base->offset;
    }
    return iter->member ? ((ObjMemberVar*)iter->member)->offset : 0;
}

static inline void IterNext(MemberIter* iter) {
    if (iter->type->type == TYPESTRUCT) {
        if (iter->member) {
            iter->member = ((StructMember*)iter->member)->next;
        }
    } else if (iter->base) {
        iter->base = iter->base->next;
    } else if (iter->member) {
        iter->member = ((ObjMemberVar*)iter->member)->next;
    }
}

// 0x57F9E0: the current member's type, NULL at the end
Type* func_0057F9E0(MemberIter* iter) {
    if (iter->type->type == TYPESTRUCT) {
        if (iter->member) {
            return ((StructMember*)iter->member)->type;
        }
        return NULL;
    }
    if (iter->base) {
        return TYPE(iter->base->base);
    }
    if (iter->member) {
        return ((ObjMemberVar*)iter->member)->type;
    }
    return NULL;
}

// 0x57FA10
void func_0057FA10(MemberIter* iter) {
    IterNext(iter);
}

// --- decomposing ----------------------------------------------------------

// 0x57BAF0: *a = *b of a struct with members (or a class copied bit for bit),
// where a and b are an object's address or an object's address plus
// something
Boolean func_0057BAF0(IROLinear* nd, IROLinear** dstp, IROLinear** srcp, Type** typep) {
    Type* type;
    IROLinear* dst;
    IROLinear* src;
    IROLinear* objref;
    int numObjRefs;
    int numOther;
    Boolean ok;
    MemberIter iter;

    if (IRO_IsAssignment(nd) &&
        !(nd->flags & IROLF_Reffed) &&
        !IRO_IsModifyOp[nd->nodetype] &&
        (type = nd->rtype)) {
        if (type->type == TYPESTRUCT ||
            (type->type == TYPECLASS && CClass_IsTrivialCopyAssignClass(TYPE_CLASS(type)))) {
            func_0057FA50(type, &iter);
            ok = IterType(&iter) != NULL;
        } else {
            ok = 0;
        }
        if (ok &&
            (dst = nd->u.diadic.left) &&
            dst->nodetype == EINDIRECT &&
            (dst = dst->u.monadic)) {
            src = (nd->type == IROLinearOp1Arg) ? NULL : nd->u.diadic.right;
            if (src && src->nodetype == EINDIRECT && (src = src->u.monadic)) {
                if (dst->nodetype == EOBJREF) {
                    numOther = 0;
                } else if (dst->nodetype == EADD) {
                    IRO_FindAddressObjRefs(dst, &objref, &numObjRefs, &numOther);
                } else {
                    numOther = 1;
                }
                if (!numOther) {
                    if (src->nodetype == EOBJREF) {
                        numOther = 0;
                    } else if (src->nodetype == EADD) {
                        IRO_FindAddressObjRefs(src, &objref, &numObjRefs, &numOther);
                    } else {
                        numOther = 1;
                    }
                    if (!numOther) {
                        *dstp = dst;
                        *srcp = src;
                        *typep = type;
                        return 1;
                    }
                }
            }
        }
    }
    *dstp = NULL;
    *srcp = NULL;
    *typep = NULL;
    return 0;
}

static IROLinear* NewOp(IROLinearType kind, ENodeType op, Type* type) {
    IROLinear* nd = IRO_NewLinear(kind);
    nd->nodetype = op;
    nd->rtype = IRO_UnqualifiedType(type);
    return nd;
}

// *(addr + offset), as a bitfield when the member is one
#if MWCCPS2
// b210 inlines MemberRef and NewOp into func_0057BD10; CW's inline depth stops at NewOp
#define NewOp(kind, op, type) \
    (nd = IRO_NewLinear(kind), nd->nodetype = (op), nd->rtype = IRO_UnqualifiedType(type), nd)
#pragma always_inline on
static inline
#else
static
#endif
    IROLinear* MemberRef(
        IROLinear* addr,
        SInt32 offset,
        Type* mtype,
        Type* atype,
        Boolean bitfield,
        UInt32 flags,
        IROList* list
    ) {
    IROLinear* constant;
    IROLinear* nd;
    CInt64 value;

    if (offset) {
        CInt64_SetLong(&value, offset);
        constant = IRO_NewIntConst(value, func_0055A650(addr->rtype));
        constant->flags |= IROLF_Reffed;
        IRO_AddToList(constant, list);
    }
    IRO_DuplicateExpr(addr, list);
    if (offset) {
        nd = NewOp(IROLinearOp2Arg, EADD, addr->rtype);
        nd->u.diadic.left = list->tail;
        nd->u.diadic.right = constant;
        nd->index = ++IRO_NumLinear;
        nd->flags |= IROLF_Reffed;
        IRO_AddToList(nd, list);
    }
    if (bitfield) {
        nd = NewOp(IROLinearOp1Arg, EBITFIELD, mtype);
        nd->u.monadic = list->tail;
        nd->index = ++IRO_NumLinear;
        nd->flags |= IROLF_Reffed;
        IRO_AddToList(nd, list);
    }
    nd = NewOp(IROLinearOp1Arg, EINDIRECT, atype);
    nd->u.monadic = list->tail;
    nd->index = ++IRO_NumLinear;
    nd->flags |= flags;
    IRO_AddToList(nd, list);
    return nd;
}

// 0x57BD10: the copy of the current member, appended to list
void func_0057BD10(
    MemberIter* iter, IROLinear* dst, IROLinear* src, Statement* stmt, IROList* list
) {
    SInt32 offset = IterOffset(iter);
    Type* mtype = IterType(iter);
    Type* atype;
    Boolean bitfield;
    IROLinear* right;
    IROLinear* left;
    IROLinear* nd;

    if (mtype->type == TYPEBITFIELD) {
        bitfield = 1;
        atype = TYPE_BITFIELD(mtype)->bitfieldtype;
    } else {
        bitfield = 0;
        atype = mtype;
    }
    right = MemberRef(src, offset, mtype, atype, bitfield, IROLF_Reffed, list);
    left = MemberRef(dst, offset, mtype, atype, bitfield, IROLF_Reffed | IROLF_Assigned, list);
    nd = IRO_NewLinear(IROLinearOp2Arg);
    nd->nodetype = EASS;
    nd->u.diadic.left = left;
    nd->u.diadic.right = right;
    nd->rtype = IRO_UnqualifiedType(atype);
    nd->index = ++IRO_NumLinear;
    nd->stmt = stmt;
    IRO_AddToList(nd, list);
}
#if MWCCPS2
#pragma always_inline reset
#undef NewOp
#endif

// 0x57B8F0: replace the struct copy nd by copies of its members
void func_0057B8F0(IROLinear* nd, IROLinear* dst, IROLinear* src, Type* type) {
    MemberIter iter;
    MemberIter inner;
    IROList list;
    IROList list2;
    IROLinear* asg;
    IROLinear* dst2;
    IROLinear* src2;
    Type* type2;
    IROLinear* dst3;
    IROLinear* src3;
    Type* type3;

    func_0057FA50(type, &iter);
    while (IterType(&iter)) {
        IRO_InitList(&list);
        func_0057BD10(&iter, dst, src, nd->stmt, &list);
        IRO_Paste(list.head, list.tail, nd);
        if (func_0057BAF0(list.tail, &dst2, &src2, &type2)) {
            asg = list.tail;
            func_0057FA50(type2, &inner);
            while (func_0057F9E0(&inner)) {
                IRO_InitList(&list2);
                func_0057BD10(&inner, dst2, src2, asg->stmt, &list2);
                IRO_Paste(list2.head, list2.tail, asg);
                if (func_0057BAF0(list2.tail, &dst3, &src3, &type3)) {
                    func_0057B8F0(list2.tail, dst3, src3, type3);
                }
                func_0057FA10(&inner);
            }
            IRO_NopOut(asg);
        }
        IterNext(&iter);
    }
    IRO_NopOut(nd);
}

// 0x57B880
void IRO_DecomposeAssignments(void) {
    IROLinear* nd;
    IROLinear* dst;
    IROLinear* src;
    Type* type;

    for (nd = IRO_FirstLinear; nd; nd = nd->next) {
        if (func_0057BAF0(nd, &dst, &src, &type)) {
            IRO_Dump("decomposing assignment at %d\n", nd->index);
            func_0057B8F0(nd, dst, src, type);
        }
    }
    IRO_UpdateFlagsOnInts(IRO_FirstLinear);
}

// --- recomposing -----------------------------------------------------------
//
// Assignments to the members of a small struct variable (at most 99 bytes)
// are found by a use-def pass that keeps a record per bit of the variable
// (0x578130); member copies that together copy a whole variable go back to
// one struct copy.

// var->unk24 while recomposing (0x10 bytes)
typedef struct IROAggVar {
    BitVector* padding; // +0: the bits that are not in a member
    struct IROAggPart* parts; // +4: the variables its bits were copied from
    BitVector* killed; // +8: the bits that are not copies
    Boolean killedClear; // +0xC
    Boolean paddingSet; // +0xD
    Boolean done; // +0xE
} IROAggVar;

// a source variable whose bits were copied into the variable (0xC bytes)
typedef struct IROAggPart {
    VarRecord* src; // +0
    BitVector* bits; // +4: the bits copied from it
    struct IROAggPart* next; // +8
} IROAggPart;

// a run of bits copied from one variable to another, a candidate for one
// struct copy (0x58 bytes)
typedef struct IROAggCand {
    SInt32 index; // +0: the use's index
    IRONode* node; // +4
    IROLinear* linear; // +8: the use
    BitVector* reaching; // +0xC: the definitions reaching it
    IROLinear* start; // +0x10: the start of its statement
    VarRecord* var; // +0x14
    VarRecord* src; // +0x18
    SInt32 offset; // +0x1C: the first bit
    SInt32 count; // +0x20: how many
    struct IROAggCand* next; // +0x24
    Boolean costDone; // +0x28: cost and numDefs are set (0x57F1F0)
    SInt32 x2C;
    double cost; // +0x30: how many loops its definitions are in, on average
    SInt32 numDefs; // +0x38: the assignments that define its bits
    Boolean trimDone; // +0x3C: the trim fields are set (0x57EF00)
    SInt32 trimNumDefs; // +0x40
    SInt32 trimOffset; // +0x44: the run cut to whole bytes
    SInt32 trimCount; // +0x48
    SInt32 x4C;
    double trimCost; // +0x50
} IROAggCand;

IROAggPart* data_00608C60; // the part last looked up
BitVector** data_00608C68; // per node: the loops it belongs to
BitVector* data_00608C64; // a set of loops
SInt32 data_00608C6C; // how many candidates
IROAggCand* data_00608C70; // the last candidate
IROAggCand* data_00608C74; // the candidates

extern IRONaturalLoop* IRO_FindLoopHeaders301(void); // IroLoop.c
extern void func_00589A80(IRONaturalLoop* loops); // IroLoop.c: free the loops
extern Boolean func_00578130(void); // IROUseDef301.c: the per-bit definitions and uses
VarRecord* func_0057F400(IROLinear* nd, SInt32 offset, SInt32 count, Boolean definite);
void func_0057C360(BitVector* a, BitVector* b, IROLinear* linear, IRONode* node, SInt32 index);
void func_0057D220(void);

// the node's predecessors (and theirs) up to the loop header join loop i
#if MWCCPS2
// b210 has no separate helper: 0x57D020's own body recurses (0x592CE0)
void func_0057D020(IRONode* node, BitVector* nodes, UInt16 i);
#define AddLoopPreds301 func_0057D020
void func_0057D020(IRONode* node, BitVector* nodes, UInt16 i) {
#else
static void AddLoopPreds301(IRONode* node, BitVector* nodes, UInt16 i) {
#endif
    IRONode* pred;
    UInt16 k;

    for (k = 0; k < node->numpred; k++) {
        pred = IRO_NodeTable[node->pred[k]];
        if (!Bv_IsBitSet(pred->index, nodes)) {
            Bv_SetBit(pred->index, nodes);
            Bv_SetBit(i, data_00608C68[pred->index]);
            AddLoopPreds301(pred, nodes, i);
        }
    }
}

#if !MWCCPS2
// 0x57D020
void func_0057D020(IRONode* node, BitVector* nodes, UInt16 i) {
    AddLoopPreds301(node, nodes, i);
}
#endif

// 0x57CE20: the nodes of loop i (2.4.7's ComputeLoopBody), each marked as in
// loop i
void func_0057CE20(IRONaturalLoop* loop, UInt16 i) {
    BitVector* nodes;
    IRONode* header;
    IRONode* pred;
    UInt16 k;

    if (loop->nodes) {
        IRO_free(loop->nodes);
        loop->nodes = NULL;
    }
    header = loop->header;
    Bv_AllocVector(&nodes, IRO_NumNodes + 1);
    Bv_Clear(nodes);
    Bv_SetBit(header->index, nodes);
    Bv_SetBit(i, data_00608C68[header->index]);
    for (k = 0; k < header->numpred; k++) {
        pred = IRO_NodeTable[header->pred[k]];
        if (Bv_IsBitSet(header->index, pred->dom)) {
            Bv_SetBit(pred->index, nodes);
            Bv_SetBit(i, data_00608C68[pred->index]);
            if (pred != header) {
                func_0057D020(pred, nodes, i);
            }
        }
    }
    loop->nodes = nodes;
}

// 0x57CD60: which loops each node is in
void func_0057CD60(void) {
    IRONaturalLoop* loops;
    IRONaturalLoop* loop;
    IRONode* node;
    UInt16 count;
    UInt16 i;

    loops = IRO_FindLoopHeaders301();
    count = 0;
    for (loop = loops; loop; loop = loop->next) {
        count++;
    }
    data_00608C68 = IRO_malloc(IRO_NumNodes * sizeof(BitVector*));
    memset(data_00608C68, 0, IRO_NumNodes * sizeof(BitVector*));
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        Bv_AllocVector(&data_00608C68[node->index], count);
    }
    Bv_AllocVector(&data_00608C64, count);
    i = 0;
    for (loop = loops; loop; loop = loop->next) {
        func_0057CE20(loop, i);
        i++;
    }
    func_00589A80(loops);
    IRO_CheckForUserBreak();
}

// 0x57CA20: the definitions made by one assignment share one vector of the
// bits it defines
void func_0057CA20(void) {
    IRODef301* def;
    IRODef301* def2;
    IROLinear* nd;
    VarRecord* bits;
    SInt32 lo;
    SInt32 hi;
    Boolean definite;

    for (def = data_00624C38; def; def = def->globalnext) {
        nd = def->linear;
        if (nd && !(nd->flags & IROLF_Unk200000)) {
            lo = def->bit;
            hi = def->bit;
            definite = def->definite;
            for (def2 = def->globalnext; def2; def2 = def2->globalnext) {
                if (def2->linear == nd) {
                    if (def2->bit < lo) {
                        lo = def2->bit;
                    }
                    if (def2->bit > hi) {
                        hi = def2->bit;
                    }
                    definite = definite && def2->definite;
                }
            }
            bits = func_0057F400(nd, lo, hi + 1 - lo, definite);
            for (def2 = def; def2; def2 = def2->globalnext) {
                if (def2->linear == nd) {
                    def2->x24 = (BitVector*)bits;
                    def2->linear->flags |= IROLF_Unk200000;
                }
            }
        }
    }
    for (def = data_00624C38; def; def = def->globalnext) {
        if (def->linear) {
            def->linear->flags &= ~IROLF_Unk200000;
        }
    }
}

// a struct (or a class copied bit for bit) with members
static inline Boolean HasMembers301(Type* type) {
    MemberIter iter;

    if (type->type == TYPESTRUCT ||
        (type->type == TYPECLASS && CClass_IsTrivialCopyAssignClass(TYPE_CLASS(type)))) {
        func_0057FA50(type, &iter);
        return IterType(&iter) != NULL;
    }
    return FALSE;
}

extern VarRecord* func_00558750(IROLinear* addr); // IroUtil.c
extern IROLinear* func_00552E90(IROLinear* nd); // IroSimplifyIfThenElse.c: the expression's root
extern VarRecord* func_00558760(IROLinear* nd); // IroUtil.c
extern Boolean func_005582F0(
    IROLinear* nd, VarRecord* var, SInt32* offset, SInt32* count
); // IroUtil.c

// 0x57F400: nd copies bits offset..offset+count-1 of a variable of the same
// struct type (and all of them definitely): that variable
VarRecord* func_0057F400(IROLinear* nd, SInt32 offset, SInt32 count, Boolean definite) {
    IROLinear* src;
    VarRecord* srcVar;
    VarRecord* dstVar;
    SInt32 srcOffset;
    SInt32 srcCount;

    if (nd && nd->type == IROLinearOp2Arg && nd->nodetype == EASS && !HasMembers301(nd->rtype)) {
        src = (nd->type == IROLinearOp1Arg) ? NULL : nd->u.diadic.right;
        if (src &&
            src->type == IROLinearOp1Arg &&
            src->nodetype == EINDIRECT &&
            (srcVar = func_00558750(src->u.monadic)) &&
            (dstVar = func_00558760(nd)) &&
            srcVar != dstVar &&
            HasMembers301(srcVar->object->type) &&
            HasMembers301(dstVar->object->type) &&
            IRO_TypesEqual(srcVar->object->type, dstVar->object->type) &&
            func_005582F0(src->u.monadic, srcVar, &srcOffset, &srcCount) &&
            srcOffset == offset &&
            srcCount == count &&
            definite) {
            return srcVar;
        }
    }
    return NULL;
}

// 0x57F6B0: clear the bits of the members of type (at bit offset); the
// padding stays set. (3.0.1 adds offset twice for a type without members.)
void func_0057F6B0(Type* type, BitVector* bv, SInt32 offset) {
    MemberIter iter;
    Type* mtype;
    SInt32 start;
    SInt32 end;
    SInt32 i;

    if (HasMembers301(type)) {
        func_0057FA50(type, &iter);
        while ((mtype = IterType(&iter))) {
            start = IterOffset(&iter) * 8;
            if (HasMembers301(mtype)) {
                func_0057F6B0(mtype, bv, start + offset);
            } else {
                if (mtype->type == TYPEBITFIELD) {
                    start += TYPE_BITFIELD(mtype)->offset;
                    end = start + TYPE_BITFIELD(mtype)->bitlength;
                } else {
                    end = start + mtype->size * 8;
                }
                for (i = start; i < end; i++) {
                    Bv_ClearBit(i + offset, bv);
                }
            }
            IterNext(&iter);
        }
    } else {
        end = offset + type->size * 8;
        for (i = offset; i < end; i++) {
            Bv_ClearBit(i + offset, bv);
        }
    }
}

// 0x57F300: bit of var was copied from src
void func_0057F300(VarRecord* var, IROAggVar* avar, VarRecord* src, SInt32 bit) {
    BitVector* bits = NULL;
    IROAggPart* part;

    if (data_00608C60 && data_00608C60->src == src) {
        bits = data_00608C60->bits;
    } else {
        for (part = avar->parts; part; part = part->next) {
            if (part->src == src) {
                data_00608C60 = part;
                bits = part->bits;
                break;
            }
        }
    }
    if (!bits) {
        part = IRO_malloc(sizeof(IROAggPart));
        part->src = NULL;
        part->bits = NULL;
        part->next = NULL;
        part->src = src;
        Bv_AllocVector(&bits, var->object->type->size * 8);
        part->bits = bits;
        part->next = avar->parts;
        avar->parts = part;
        data_00608C60 = part;
    }
    Bv_SetBit(bit, bits);
}

// 0x57F950: a new candidate
IROAggCand* func_0057F950(
    SInt32 index,
    IRONode* node,
    IROLinear* linear,
    BitVector* reaching,
    IROLinear* start,
    VarRecord* var,
    VarRecord* src,
    SInt32 offset,
    SInt32 count
) {
    IROAggCand* cand = IRO_malloc(sizeof(IROAggCand));

    memset(cand, 0, sizeof(IROAggCand));
    cand->index = index;
    cand->node = node;
    cand->linear = linear;
    cand->reaching = reaching;
    cand->start = start;
    cand->var = var;
    cand->src = src;
    cand->offset = offset;
    cand->count = count;
    cand->next = NULL;
    if (data_00608C74) {
        data_00608C70->next = cand;
    } else {
        data_00608C74 = cand;
    }
    data_00608C70 = cand;
    data_00608C6C++;
    return cand;
}

// 0x59B950: MSL's qsort (a heap sort; equal elements keep MSL's order)
void func_0059B950(
    void* base, size_t count, size_t size, int (*compare)(const void*, const void*)
) {
    size_t l;
    size_t r;
    size_t j;
    char* lp;
    char* rp;
    char* ip;
    char* jp;
    char* kp;
    char* p;
    char* q;
    size_t n;
    char tmp;

#define TABLE_PTR(i) (((char*)base) + size * ((i) - 1))
#define SWAP(a, b) \
    for (p = (a), q = (b), n = size; n; n--) { \
        tmp = *p; \
        *p++ = *q; \
        *q++ = tmp; \
    }

    if (count < 2) {
        return;
    }
    r = count;
    l = (r / 2) + 1;
    lp = TABLE_PTR(l);
    rp = TABLE_PTR(r);
    for (;;) {
        if (l > 1) {
            l--;
            lp -= size;
        } else {
            SWAP(lp, rp);
            if (--r == 1) {
                return;
            }
            rp -= size;
        }
        j = l;
        jp = TABLE_PTR(j);
        while (j * 2 <= r) {
            j *= 2;
            ip = jp;
            jp = TABLE_PTR(j);
            if (j < r) {
                kp = jp + size;
                if (compare(jp, kp) < 0) {
                    j++;
                    jp = kp;
                }
            }
            if (compare(ip, jp) < 0) {
                SWAP(ip, jp);
            } else {
                break;
            }
        }
    }
#undef TABLE_PTR
#undef SWAP
}

extern UInt32 func_0059B130(BitVector* bv); // IroLoop.c: how many bits are set

// marks (IROLF_Unk200000) the assignments that define bits lo..hi-1 of the
// candidate's variable from its source and reach its use
static inline void MarkDefs301(IROAggCand* cand, SInt32 lo, SInt32 hi) {
    IRODef301* d;

    for (d = (IRODef301*)cand->var->defs; d; d = d->varnext) {
        if (d->linear &&
            (VarRecord*)d->x24 == cand->src &&
            Bv_IsBitSet(d->index, cand->reaching) &&
            d->bit >= lo &&
            d->bit < hi) {
            d->linear->flags |= IROLF_Unk200000;
        }
    }
}

// the marked assignments, unmarked: how many, and the loops they are in
static inline SInt32 CountMarked301(IROAggCand* cand, UInt16* loops) {
    IRODef301* d;
    SInt32 count = 0;
    UInt16 sum = 0;

    for (d = (IRODef301*)cand->var->defs; d; d = d->varnext) {
        if (d->linear && (d->linear->flags & IROLF_Unk200000)) {
            sum += func_0059B130(data_00608C68[d->node->index]);
            count++;
            d->linear->flags &= ~IROLF_Unk200000;
        }
    }
    *loops = sum;
    return count;
}

// 0x57F1F0
void func_0057F1F0(IROAggCand* cand) {
    UInt16 loops;
    SInt32 count;

    MarkDefs301(cand, cand->offset, cand->offset + cand->count);
    count = CountMarked301(cand, &loops);
    cand->numDefs = count;
    cand->cost = (double)loops / (double)count;
    cand->costDone = TRUE;
}

// 0x57EF00: the run cut to whole bytes at both ends (dropping the
// assignments that reach past them)
void func_0057EF00(IROAggCand* cand) {
    IRODef301* d;
    UInt16 loops;
    SInt32 count;
    SInt32 lo = cand->offset;
    SInt32 hi = cand->offset + cand->count;

    while (lo < hi && lo % 8 != 0) {
        for (d = (IRODef301*)cand->var->defs; d; d = d->varnext) {
            if (d->linear &&
                (VarRecord*)d->x24 == cand->src &&
                Bv_IsBitSet(d->index, cand->reaching) &&
                d->bit == lo) {
                d->linear->flags |= IROLF_Unk200000;
            }
        }
        for (d = (IRODef301*)cand->var->defs; d; d = d->varnext) {
            if (d->linear && (d->linear->flags & IROLF_Unk200000) && lo <= d->bit) {
                lo = d->bit + 1;
            }
        }
        for (d = (IRODef301*)cand->var->defs; d; d = d->varnext) {
            if (d->linear) {
                d->linear->flags &= ~IROLF_Unk200000;
            }
        }
    }
    while (hi > lo && hi % 8 != 0) {
        for (d = (IRODef301*)cand->var->defs; d; d = d->varnext) {
            if (d->linear &&
                (VarRecord*)d->x24 == cand->src &&
                Bv_IsBitSet(d->index, cand->reaching) &&
                d->bit == hi - 1) {
                d->linear->flags |= IROLF_Unk200000;
            }
        }
        for (d = (IRODef301*)cand->var->defs; d; d = d->varnext) {
            if (d->linear && (d->linear->flags & IROLF_Unk200000) && hi > d->bit) {
                hi = d->bit;
            }
        }
        for (d = (IRODef301*)cand->var->defs; d; d = d->varnext) {
            if (d->linear) {
                d->linear->flags &= ~IROLF_Unk200000;
            }
        }
    }
    cand->trimOffset = lo;
    cand->trimCount = hi - lo;
    MarkDefs301(cand, lo, hi);
    count = CountMarked301(cand, &loops);
    cand->trimNumDefs = count;
    cand->trimCost = (double)loops / (double)count;
    cand->trimDone = TRUE;
}

// the widest copy one bitfield access may make
#define MAX_COPY_BITS (copts.has8bytebitfields ? 64 : 32)

// a run of only part of the variable too wide for one access is cut to bytes
#if MWCCPS2 // a macro: CW stops inlining it inside the other inlines (func_0057D2A0)
#define CandTrimmed(cand) \
    ((cand)->count != (cand)->var->object->type->size * 8 && (cand)->count > MAX_COPY_BITS)
#else
static inline Boolean CandTrimmed(IROAggCand* cand) {
    return cand->count != cand->var->object->type->size * 8 && cand->count > MAX_COPY_BITS;
}
#endif

static inline double CandCost(IROAggCand* cand) {
    if (!cand->costDone) {
        func_0057F1F0(cand);
    }
    if (CandTrimmed(cand)) {
        if (!cand->trimDone) {
            func_0057EF00(cand);
        }
        return cand->trimCost;
    }
    return cand->cost;
}

static inline SInt32 CandNumDefs(IROAggCand* cand) {
    if (!cand->costDone) {
        func_0057F1F0(cand);
    }
    if (CandTrimmed(cand)) {
        if (!cand->trimDone) {
            func_0057EF00(cand);
        }
        return cand->trimNumDefs;
    }
    return cand->numDefs;
}

static inline SInt32 CandOffset(IROAggCand* cand) {
    if (CandTrimmed(cand)) {
        if (!cand->trimDone) {
            func_0057EF00(cand);
        }
        return cand->trimOffset;
    }
    return cand->offset;
}

static inline SInt32 CandCount(IROAggCand* cand) {
    if (CandTrimmed(cand)) {
        if (!cand->trimDone) {
            func_0057EF00(cand);
        }
        return cand->trimCount;
    }
    return cand->count;
}

// how far the run is from whole bytes
static inline SInt32 CandMisalign(IROAggCand* cand) {
    SInt32 offset = CandOffset(cand);
    SInt32 count = CandCount(cand);

    return ((8 - offset % 8) % 8) + ((count + offset) % 8);
}

// loops the use is in, less those its definitions are in
static inline double CandGain(IROAggCand* cand) {
    double cost = CandCost(cand);
#if MWCCPS2 // b210 converts the loop count as signed
    return (double)(SInt32)func_0059B130(data_00608C68[cand->node->index]) - cost;
#else
    return (double)func_0059B130(data_00608C68[cand->node->index]) - cost;
#endif
}

// 0x57E050: the qsort order of the candidates
#if MWCCPS2 // b210 inlines CandCost inside every CandGain; CW's inline depth stops short
#define CandGain(cand) \
    (gaincost = CandCost(cand), \
     (double)(SInt32)func_0059B130(data_00608C68[(cand)->node->index]) - gaincost)
#endif
int func_0057E050(const void* pa, const void* pb) {
    IROAggCand* a = *(IROAggCand**)pa;
    IROAggCand* b = *(IROAggCand**)pb;
    double gainB;
    double gainA;
    double diff;
#if MWCCPS2
    double gaincost;
#endif

    gainB = CandGain(b);
    diff = gainB - CandGain(a);
    if ((diff < 0.0 ? -diff : diff) > 0.1) { // b210: fabs (no x87 intrinsic in CW 9.2)
        gainA = CandGain(a);
        return (int)((gainA - CandGain(b)) * 10.0);
    }
    if (CandNumDefs(b) != CandNumDefs(a)) {
        return CandNumDefs(b) - CandNumDefs(a);
    }
    if (CandMisalign(b) == 0 && CandMisalign(a) != 0) {
        return 1;
    }
    if (CandMisalign(b) != 0 && CandMisalign(a) == 0) {
        return -1;
    }
    if (CandCount(b) != CandCount(a)) {
        return CandCount(b) - CandCount(a);
    }
    if (CandMisalign(b) != CandMisalign(a)) {
        return CandMisalign(a) != CandMisalign(b);
    }
    return 0;
}
#if MWCCPS2
#undef CandGain
#endif

// a new linear of the copy, appended to list
static inline IROLinear* AddLinear301(
    IROLinearType kind, ENodeType op, Type* type, SInt32 flags, IROList* list
) {
    IROLinear* nd = IRO_NewLinear(kind);

    nd->nodetype = op;
    nd->rtype = IRO_UnqualifiedType(type);
    if (kind == IROLinearOp1Arg) {
        nd->u.monadic = list->tail;
    } else if (kind == IROLinearOp2Arg) {
        nd->u.diadic.left = list->tail;
    }
    nd->index = ++IRO_NumLinear;
    nd->flags |= flags;
    IRO_AddToList(nd, list);
    return nd;
}

// obj (+ byteOffset), its address appended to list
static inline void AddAddress301(Object* obj, SInt32 byteOffset, SInt32 flags, IROList* list) {
    IROLinear* offset;
    IROLinear* objref;
    IROLinear* add;
    CInt64 value;

    if (byteOffset) {
        CInt64_SetLong(&value, byteOffset);
        offset = IRO_NewIntConst(value, func_0055A650(obj->type));
        offset->flags |= IROLF_Reffed;
        IRO_AddToList(offset, list);
    }
    objref = IRO_NewLinear(IROLinearOperand);
    objref->nodetype = EOBJREF;
    objref->u.node = create_objectrefnode(obj);
    objref->rtype = IRO_UnqualifiedType(objref->u.node->rtype);
    objref->index = ++IRO_NumLinear;
    objref->flags |= flags;
    IRO_AddToList(objref, list);
    if (byteOffset) {
        add = IRO_NewLinear(IROLinearOp2Arg);
        add->nodetype = EADD;
        add->rtype = IRO_UnqualifiedType(list->tail->rtype);
        add->u.diadic.left = list->tail;
        add->u.diadic.right = offset;
        add->index = ++IRO_NumLinear;
        add->flags |= IROLF_Reffed;
        IRO_AddToList(add, list);
    }
}

// 0x57D850: dst's bits offset..offset+count-1 = src's, as one bitfield copy
void func_0057D850(
    Object* dst, Object* src, SInt32 offset, SInt32 count, Statement* stmt, IROList* list
) {
    TypeBitfield* bf;
    Type* type = NULL;
    IROLinear* value;
    IROLinear* target;
    IROLinear* ass;
    SInt32 bitOffset = offset % 8;
    SInt32 bits = count + bitOffset;
    SInt32 byteOffset;

    if (bits > stunsignedlong.size * 8) {
        type = TYPE(&stunsignedlonglong);
    } else if (bits > stunsignedint.size * 8) {
        type = TYPE(&stunsignedlong);
    } else if (bits > stunsignedshort.size * 8) {
        type = TYPE(&stunsignedint);
    } else if (bits > stunsignedchar.size * 8) {
        type = TYPE(&stunsignedshort);
    } else if (bits > 0) {
        type = TYPE(&stunsignedchar);
    }
    bf = galloc(sizeof(TypeBitfield));
    memclrw(bf, sizeof(TypeBitfield));
    bf->type = TYPEBITFIELD;
    bf->size = type->size;
    bf->bitfieldtype = type;
    bf->offset = bitOffset;
    bf->bitlength = count;
    byteOffset = offset / 8;

    AddAddress301(src, byteOffset, IROLF_Reffed | IROLF_Ind, list);
    AddLinear301(IROLinearOp1Arg, EBITFIELD, TYPE(bf), IROLF_Reffed | IROLF_Ind, list);
    value = AddLinear301(IROLinearOp1Arg, EINDIRECT, type, IROLF_Reffed, list);
    AddAddress301(dst, byteOffset, IROLF_Reffed | IROLF_Assigned | IROLF_Ind, list);
    AddLinear301(IROLinearOp1Arg, EBITFIELD, TYPE(bf), IROLF_Reffed | IROLF_Ind, list);
    target = AddLinear301(IROLinearOp1Arg, EINDIRECT, type, IROLF_Reffed | IROLF_Assigned, list);
    ass = IRO_NewLinear(IROLinearOp2Arg);
    ass->nodetype = EASS;
    ass->u.diadic.left = target;
    ass->u.diadic.right = value;
    ass->rtype = IRO_UnqualifiedType(type);
    ass->index = ++IRO_NumLinear;
    ass->stmt = stmt;
    IRO_AddToList(ass, list);
}

// 0x57DC90: dst's bytes offset/8.. = src's, as one copy of an integer (or a
// struct of that size)
void func_0057DC90(
    Object* dst, Object* src, SInt32 offset, SInt32 count, Statement* stmt, IROList* list
) {
    Type* type = dst->type;
    IROLinear* value;
    IROLinear* target;
    IROLinear* ass;
    SInt32 byteOffset;

    if (count != type->size * 8) {
        if (count == stunsignedchar.size * 8) {
            type = TYPE(&stunsignedchar);
        } else if (count == stunsignedshort.size * 8) {
            type = TYPE(&stunsignedshort);
        } else if (count == stunsignedint.size * 8) {
            type = TYPE(&stunsignedint);
        } else if (count == stunsignedlong.size * 8) {
            type = TYPE(&stunsignedlong);
        } else if (count == stunsignedlonglong.size * 8) {
            type = TYPE(&stunsignedlonglong);
        } else {
            type = TYPE(CDecl_NewStructType(count / 8, 1));
        }
    }
    byteOffset = offset / 8;

    AddAddress301(src, byteOffset, IROLF_Reffed | IROLF_Ind, list);
    value = AddLinear301(IROLinearOp1Arg, EINDIRECT, type, IROLF_Reffed, list);
    AddAddress301(dst, byteOffset, IROLF_Reffed | IROLF_Assigned | IROLF_Ind, list);
    target = AddLinear301(IROLinearOp1Arg, EINDIRECT, type, IROLF_Reffed | IROLF_Assigned, list);
    ass = IRO_NewLinear(IROLinearOp2Arg);
    ass->nodetype = EASS;
    ass->u.diadic.left = target;
    ass->u.diadic.right = value;
    ass->rtype = IRO_UnqualifiedType(type);
    ass->index = ++IRO_NumLinear;
    ass->stmt = stmt;
    IRO_AddToList(ass, list);
}

// 0x57D2A0: replace the candidate's member copies by one copy before its
// statement, when its use is in no more loops than the copies
void func_0057D2A0(IROAggCand* cand) {
    IRODef301* d;
    IROLinear* nd;
    IROLinear* right;
    VarRecord* var;
    VarRecord* src;
    BitVector* reaching;
    Statement* stmt;
    IROList list;
    SInt32 offset;
    SInt32 count;
    SInt32 hi;
    Boolean seen;
#if MWCCPS2
    double gainCost;
#endif

#if MWCCPS2
    // b210 takes the cost first and converts the loop count as signed
    gainCost = CandCost(cand);
    if ((double)(SInt32)func_0059B130(data_00608C68[cand->node->index]) - gainCost > 0.0) {
        return;
    }
#else
    if (!cand->costDone) {
        func_0057F1F0(cand);
    }
    if ((double)func_0059B130(data_00608C68[cand->node->index]) - CandCost(cand) > 0.0) {
        return;
    }
#endif
    reaching = cand->reaching;
    var = cand->var;
    src = cand->src;
    offset = CandOffset(cand);
    count = CandCount(cand);
    hi = offset + count;
    if (!count) {
        return;
    }
    for (d = (IRODef301*)var->defs; d; d = d->varnext) {
        if (d->linear &&
            (VarRecord*)d->x24 == src &&
            Bv_IsBitSet(d->index, reaching) &&
            d->bit >= offset &&
            d->bit < hi &&
            d->linear->type == IROLinearNop) {
            return;
        }
    }
    stmt = NULL;
    seen = FALSE;
    for (d = (IRODef301*)var->defs; d; d = d->varnext) {
        if ((VarRecord*)d->x24 == src &&
            Bv_IsBitSet(d->index, reaching) &&
            d->bit >= offset &&
            d->bit < hi) {
            nd = d->linear;
            if (nd->stmt) {
                if (!seen) {
                    stmt = nd->stmt;
                    seen = TRUE;
                } else if (stmt != nd->stmt) {
                    stmt = NULL;
                }
            }
            if (nd->type != IROLinearNop) {
                IRO_Dump("nopping assignment at %d\n", nd->index);
                if (nd->flags & IROLF_Reffed) {
                    right = (nd->type == IROLinearOp1Arg) ? NULL : nd->u.diadic.right;
                    IRO_NopOut(nd->u.diadic.left);
                    IRO_LocateFather_Cut_And_Paste_Without_Nopping(nd, right);
                    nd->type = IROLinearNop;
                    nd->expr = NULL;
                } else {
                    IRO_NopOut(nd);
                }
            }
        }
    }
    IRO_Dump(
        "adding assignment before %d for use at %d\n", cand->start->index, cand->linear->index
    );
    IRO_InitList(&list);
    if (count == var->object->type->size * 8) {
        func_0057DC90(var->object, src->object, 0, count, stmt, &list);
    } else if (CandMisalign(cand) == 0) {
        func_0057DC90(var->object, src->object, offset, count, stmt, &list);
    } else if (count <= MAX_COPY_BITS) {
        func_0057D850(var->object, src->object, offset, count, stmt, &list);
    }
    IRO_Paste(list.head, list.tail, cand->start);
}

// 0x57D220: the candidates, best first
void func_0057D220(void) {
    IROAggCand** cands;
    IROAggCand* cand;
    SInt32 i;

    if (data_00608C6C > 0) {
        cands = IRO_malloc(data_00608C6C * sizeof(IROAggCand*));
        i = 0;
        for (cand = data_00608C74; cand; cand = cand->next) {
            cands[i++] = cand;
        }
        func_0059B950(cands, data_00608C6C, sizeof(IROAggCand*), func_0057E050);
        for (i = 0; i < data_00608C6C; i++) {
            func_0057D2A0(cands[i]);
        }
        IRO_free(cands);
    }
}

// 0x57CAE0: every other use def reaches is dominated by node, and shares no
// loop with it
Boolean func_0057CAE0(IRONode* node, SInt32 index, IRODef301* def, VarRecord* var, VarRecord* src) {
    IROUse301* use;

    for (use = data_006242B8; use; use = use->globalnext) {
        if (use->index != index && Bv_IsBitSet(def->index, use->reachingDefs)) {
            if (node == use->node || !Bv_IsBitSet(node->index, use->node->dom)) {
                return FALSE;
            }
            Bv_Copy(data_00608C68[node->index], data_00608C64);
            Bv_And(data_00608C68[use->node->index], data_00608C64);
            if (!Bv_IsEmpty(data_00608C64)) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

// 0x57CBF0: the one definition of the same bit of src that reaches the use
// (a) also reaches def (its x28), and no loop holds both without node
Boolean func_0057CBF0(VarRecord* src, IRODef301* def, BitVector* a, IRONode* node) {
    IRODef301* found = NULL;
    IRODef301* d;

    for (d = (IRODef301*)src->defs; d; d = d->varnext) {
        if (d->bit == def->bit) {
            if (Bv_IsBitSet(d->index, a)) {
                if (!Bv_IsBitSet(d->index, def->x28) || found) {
                    found = NULL;
                    break;
                }
                found = d;
            } else if (Bv_IsBitSet(d->index, def->x28)) {
                found = NULL;
                break;
            }
        }
    }
    if (!found) {
        return FALSE;
    }
    if (!found->node) {
        return TRUE;
    }
    Bv_Copy(data_00608C68[def->node->index], data_00608C64);
    Bv_And(data_00608C68[found->node->index], data_00608C64);
    if (Bv_IsEmpty(data_00608C64)) {
        return TRUE;
    }
    Bv_Minus(data_00608C68[node->index], data_00608C64);
    return Bv_IsEmpty(data_00608C64);
}

// 0x57C360: at a use (reached by the definitions in a, b), the runs of bits
// of each variable copied whole from another variable become candidates
void func_0057C360(BitVector* a, BitVector* b, IROLinear* linear, IRONode* node, SInt32 index) {
    IRODef301* def;
    IRODef301* d2;
    VarRecord* var;
    VarRecord* src;
    IROAggVar* avar;
    IROAggPart* part;
    IROAggPart* nextPart;
    IROLinear* top;
    IROLinear* father;
    IROLinear* start;
    BitVector* padding;
    BitVector* killed;
    BitVector* bits;
    SInt32 size;
    SInt32 i;
    SInt32 run;
    Boolean all;
    Boolean flag;

    data_00608C60 = NULL;
    for (def = data_00624C38; def; def = def->globalnext) {
        var = def->var;
        src = (VarRecord*)def->x24;
        avar = var->unk24;
        if (src && avar && !src->usedAtCall && Bv_IsBitSet(def->index, b)) {
            if (!avar->paddingSet) {
                Bv_Set(avar->padding);
                func_0057F6B0(var->object->type, avar->padding, 0);
                avar->paddingSet = TRUE;
            }
            func_0057F300(var, avar, src, def->bit);
            if (!avar->killedClear) {
                Bv_Clear(avar->killed);
                avar->killedClear = TRUE;
            }
        }
    }
    for (def = data_00624C38; def; def = def->globalnext) {
        src = (VarRecord*)def->x24;
        avar = def->var->unk24;
        if (Bv_IsBitSet(def->index, a) && avar) {
            avar->done = FALSE;
            if (!src || !def->linear) {
                Bv_SetBit(def->bit, avar->killed);
            }
        }
    }
    for (def = data_00624C38; def; def = def->globalnext) {
        src = (VarRecord*)def->x24;
        var = def->var;
        avar = var->unk24;
        if (Bv_IsBitSet(def->index, a) && avar && !Bv_IsBitSet(def->bit, avar->killed)) {
            if (def->x34 == 1 &&
                def->x38 &&
                def->x38->index == index &&
                Bv_IsBitSet(def->index, b) &&
                func_0057CBF0(src, def, a, node) &&
                func_0057CAE0(node, index, def, var, src)) {
                continue;
            }
            for (d2 = def->linear->x24.aggDefs; d2; d2 = d2->linearnext) {
                Bv_SetBit(d2->bit, avar->killed);
            }
        }
    }

    top = linear;
    while ((father = IRO_LocateFather(top))) {
        top = father;
    }
    if (func_00552E90(top)) {
        top = func_00552E90(top);
    }
    start = IRO_FindStart(top);

    for (def = data_00624C38; def; def = def->globalnext) {
        var = def->var;
        avar = var->unk24;
        if (def->x24 && avar && Bv_IsBitSet(def->index, b)) {
            avar->killedClear = FALSE;
            if (avar->paddingSet && !avar->done) {
                padding = avar->padding;
                killed = avar->killed;
                for (part = avar->parts; part; part = part->next) {
                    bits = part->bits;
                    src = part->src;
                    Bv_Minus(killed, bits);
                    Bv_Or(padding, bits);
                    size = var->object->type->size * 8;
                    all = TRUE;
                    for (i = 0; i < size; i++) {
                        if (!Bv_IsBitSet(i, bits)) {
                            all = FALSE;
                        }
                    }
                    if (!all) {
                        // padding at either end of a run is not copied
                        flag = FALSE;
                        for (i = 0; i < size; i++) {
                            if (Bv_IsBitSet(i, bits)) {
                                if (!Bv_IsBitSet(i, padding)) {
                                    flag = TRUE;
                                } else if (!flag) {
                                    Bv_ClearBit(i, bits);
                                }
                            } else {
                                flag = FALSE;
                            }
                        }
                        flag = FALSE;
                        for (i = size - 1; i >= 0; i--) {
                            if (Bv_IsBitSet(i, bits)) {
                                if (!Bv_IsBitSet(i, padding)) {
                                    flag = TRUE;
                                } else if (!flag) {
                                    Bv_ClearBit(i, bits);
                                }
                            } else {
                                flag = FALSE;
                            }
                        }
                    }
                    run = 0;
                    for (i = 0; i < size; i++) {
                        if (Bv_IsBitSet(i, bits)) {
                            run++;
                        } else {
                            if (run) {
                                func_0057F950(
                                    index, node, linear, b, start, var, src, i - run, run
                                );
                            }
                            run = 0;
                        }
                    }
                    if (run) {
                        func_0057F950(index, node, linear, b, start, var, src, size - run, run);
                    }
                }
                for (part = avar->parts; part; part = nextPart) {
                    nextPart = part->next;
                    if (part->bits) {
                        IRO_free(part->bits);
                        part->bits = NULL;
                    }
                    IRO_free(part);
                }
                avar->parts = NULL;
                avar->done = TRUE;
            }
        }
    }
}

// 0x57C0C0
void IRO_RecomposeAssignments(void) {
    IRODef301* def;
    IRODef301* d;
    IROUse301* use;
    VarRecord* var;
    IROAggVar* rec;
    IROAggCand* cand;
    IROAggCand* next;
    Type* type;
    UInt16 i;
    Boolean ok;

    func_0057CD60();
    if (func_00578130()) {
        func_0057CA20();
        for (var = IRO_FirstVar; var; var = var->next) {
            var->unk24 = NULL;
        }
        for (def = data_00624C38; def; def = def->globalnext) {
            var = def->var;
            if (!var->usedAtCall && !var->unk24) {
                ok = TRUE;
                if (!var->object || !var->object->type) {
                    ok = FALSE;
                } else {
                    for (d = (IRODef301*)var->defs; d; d = d->varnext) {
                        if (d->bit < 0 || (UInt32)d->bit >= (UInt32)(var->object->type->size * 8)) {
                            ok = FALSE;
                            break;
                        }
                    }
                }
                if (ok) {
                    rec = IRO_malloc(sizeof(IROAggVar));
                    memset(rec, 0, sizeof(IROAggVar));
                    var->unk24 = rec;
                    if (!var->object || !var->object->type) {
                        CError_FATAL(1691);
                    }
                    type = var->object->type;
                    Bv_AllocVector(&rec->padding, type->size * 8);
                    rec->parts = NULL;
                    Bv_AllocVector(&rec->killed, var->object->type->size * 8);
                    rec->killedClear = 0;
                    rec->paddingSet = 0;
                    rec->done = 0;
                }
            }
        }
        data_00608C6C = 0;
        for (use = data_006242B8; use; use = use->globalnext) {
            func_0057C360(use->x24, use->x28, use->linear, use->node, use->index);
        }
        func_0057D220();
        for (cand = data_00608C74; cand; cand = next) {
            next = cand->next;
            IRO_free(cand);
        }
        data_00608C74 = NULL;
        data_00608C70 = NULL;
        for (var = IRO_FirstVar; var; var = var->next) {
            if ((rec = var->unk24)) {
                if (rec->padding) {
                    IRO_free(rec->padding);
                    rec->padding = NULL;
                }
                if (rec->killed) {
                    IRO_free(rec->killed);
                    rec->killed = NULL;
                }
                IRO_free(rec);
                var->unk24 = NULL;
            }
        }
        func_0057B300();
    }
    for (i = 0; i < IRO_NumNodes; i++) {
        if (data_00608C68[i]) {
            IRO_free(data_00608C68[i]);
            data_00608C68[i] = NULL;
        }
    }
    IRO_free(data_00608C68);
    data_00608C68 = NULL;
    if (data_00608C64) {
        IRO_free(data_00608C64);
        data_00608C64 = NULL;
    }
}
