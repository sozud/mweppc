// mwccpsp 3.0.1: the loop unroller (IRO_LoopUnroller 0x561280 and the unit
// before it, 0x55CCF0..0x561280; IroUnrollLoop.c by its asserts), plus the
// IroLoop.c helpers only it uses (0x587340, 0x5887E0, 0x588820, 0x5888B0).
//
// 3.0.1 rewrote 2.4.7's unroller on top of the natural-loop records
// (IRONaturalLoop, compiler/IroLoop301.h): the inner loops are the ones whose
// body holds no other loop's header; each is analysed by the IroLoop.c
// passes (test/induction/trip count), checked by IsLoopUnrollable and then
// unrolled either as a counted loop (UnrollForLoop, 0x55D090) or as the
// "while (n--)" bit-set idiom (0x55F1B0). A counted loop gets its test
// copied in front of it (IRO_CopyLoopTestToLoopPreheaderAndBranchToSuccNode),
// the trip count kept in a compiler temporary, a guard that skips the
// unrolled body when the count is below the factor, the unrolled body with
// the induction references offset per copy, a test against the unrolled final
// value, and the original loop left behind for the remaining iterations
// (or a straight-line post loop with opt_unrollpostloop).
#include "compiler/common.h"
#include "compiler/BitVector.h"
#include "compiler/CError.h"
#include "compiler/CFunc.h"
#include "compiler/CInt64.h"
#include "compiler/CMachine.h"
#include "compiler/CParser.h"
#include "compiler/enode.h"
#include "compiler/IroDump.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroLoop301.h"
#include "compiler/IroMalloc.h"
#include "compiler/IrOptimizer.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"
#include "compiler/objects.h"
#include "compiler/types.h"
#include <string.h>

#define UNROLL_FILE "IroUnrollLoop.c"

// IroLoop.c (3.0.1)
extern IRONaturalLoop* IRO_FindLoopHeaders301(void); // FindLoopHeaders, 0x589B20
extern void IRO_AddLoopPreds(IRONode* fnode, BitVector* bv);
extern IROLinear* func_00588A10(
    IRONaturalLoop* loop, IROLoopInd301* ind
); // the induction's start value
extern void func_00589A80(IRONaturalLoop* loops); // free the loop records
extern Boolean func_00584670(IRONaturalLoop* loop, Boolean withConds); // trip count
extern void func_00588880(IROLinear* nd, Boolean isFirst, void* arg);
// IroUtil.c / IroType.c / others (3.0.1)
extern void func_005530D0(IROLinear* linear, IROWalkTreeArgFunc func, void* arg);
extern Boolean func_00555C60(IROLinear* nd); // an int or float constant
extern Boolean func_00555B60(IROLinear* linear, int* powvalue); // 2^n constant
extern Type* func_0055A650(Type* type); // the integer type for a pointer type
extern Type* func_0055A660(void); // the pointer-difference type
extern IROLinear* func_00552790(Object* obj); // *&obj
extern void func_0058C530(IROLinear* nd); // IroEval.c
extern IROLinear* func_00554440(CLabel* label, IROLinear* start);
extern Statement* IRO_PreLinearize301(Statement* stmt);
extern CInt64 cint64_negone;

// what the while-loop pattern match records (0x24 bytes, cleared by
// IsWhileLoopUnrollable)
typedef struct UnrollWhilePattern {
    IROLinear* index; // +0: the subscript added to the base (i >> k, or i)
    IROLinear* base; // +4: the base variable
    Type* baseType; // +8: the type of base + index
    IROLinear* bit; // +0xC: c << (i & m)
    IROLinear* value; // +0x10: the right side of the op= assignment
    CInt64 mask; // +0x14: the bits a whole word of iterations sets
    CInt64 indexShift; // +0x1C: k of i >> k (0 without a shift)
} UnrollWhilePattern;

typedef void (*UnrollFactorHook)(IRONaturalLoop* loop, int stride, CInt64* factor);

void func_00587340(IRONaturalLoop* loops);
void func_005887E0(IRONaturalLoop* loop);
void func_00588820(IRONaturalLoop* loop);
IROLinear* func_005888B0(IRONaturalLoop* loop);
IROLinear* func_0055CCF0(IRONaturalLoop* loop, CInt64 count, IROLinear* before);
void func_0055CDE0(
    IRONaturalLoop* loop, IROList* list, IROLinear* count, CInt64 factor, CLabel* label
);
void func_0055CF00(IROLinear* expr, IROList* list);
void func_0055CFC0(IRONaturalLoop* loop);
void func_0055D090(IRONaturalLoop* loop, CInt64 factor);
void func_0055DB60(IRONaturalLoop* loop, IRONode* after, CInt64 count);
void func_0055DC70(IRONaturalLoop* loop, IROLinear* before, CInt64* val, Boolean notFirst);
void func_0055DDA0(IROLinear* first, IROLinear* last, CInt64 val, IRONaturalLoop* loop);
IROLinear* func_0055E500(
    IROLinear* count, CInt64 factor, CInt64 leftover, IROList* list, IRONaturalLoop* loop
);
IROLinear* func_0055E6D0(IROList* list, IRONaturalLoop* loop, CInt64 adjust);
void func_0055EC40(IRONaturalLoop* loop, int stride, CInt64* factor);
void func_0055F1B0(IRONaturalLoop* loop, CInt64 factor, UnrollWhilePattern* pat);
void func_0055FB20(IRONaturalLoop* loop, UnrollWhilePattern* pat, CInt64 factor);
IROLinear* func_0055FF80(IROLinear* count, CInt64 factor, IROList* list, IRONaturalLoop* loop);
IROLinear* func_00560100(IROLinear* count, CInt64 factor, IROList* list, IRONaturalLoop* loop);
IROLinear* func_005602F0(IROLoopInd301* ind, CInt64 factor, IROList* list);
IROLinear* func_005604B0(IROList* list, IRONaturalLoop* loop);
Type* func_0055C960(void);
Boolean func_005605D0(IRONaturalLoop* loop, CInt64* factor, UnrollWhilePattern* pat);
Boolean func_00560840(IRONaturalLoop* loop, CInt64* factor, UnrollWhilePattern* pat);
Boolean func_00560C50(
    IRONode* node,
    IRONaturalLoop* loop,
    IROLoopInd301* ind,
    CInt64* factor,
    int* numMatches,
    int* numAssigns,
    UnrollWhilePattern* pat
);
Boolean func_00561160(CInt64 count, CInt64 bits, CInt64* mask);

static Boolean IsAssignOfConstant(IROLinear* father, IROLinear* nd) {
    return father &&
        father->type == IROLinearOp2Arg &&
        father->nodetype == EASS &&
        father->u.diadic.left == nd &&
        IRO_IsIntConstant(father->u.diadic.right);
}

static Type* ConstType(Type* type) {
    return (type->type == TYPEPOINTER) ? func_0055A650(type) : type;
}

// nop every linear of node but its labels
static void NopNonLabels(IRONode* node) {
    IROLinear* nd;

    for (nd = node->first; nd; nd = nd->next) {
        if (nd->type != IROLinearLabel) {
            nd->type = IROLinearNop;
        }
        if (nd == node->last) {
            break;
        }
    }
}

// an assignment of value to a new temporary on list; returns the temporary's
// reference (the left side). The counted-loop builders first run the
// constant evaluator over the value; the while-loop builders do not, and
// 0x5604B0 leaves the value without the reffed flag.
#if MWCCPS2 // b210 inlines it into every builder
inline
#endif
    static IROLinear* AssignToNewTemp(
        IROLinear* value, Type* type, IROList* list, Boolean eval, Boolean reff
    ) {
    Object* obj;
    IROLinear* ass;

    if (eval) {
        func_0058C530(value);
    }
    obj = create_temp_object(type);
    IRO_FindVar(obj, TRUE, TRUE);
    ass = IRO_NewLinear(IROLinearOp2Arg);
    ass->index = ++IRO_NumLinear;
    ass->nodetype = EASS;
    ass->u.diadic.left = func_00552790(obj);
    IRO_AddToList(ass->u.diadic.left->u.monadic, list);
    ass->u.diadic.left->flags |= IROLF_Reffed | IROLF_Assigned | IROLF_Ind;
    ass->u.diadic.left->u.monadic->flags |= IROLF_Assigned | IROLF_Ind;
    ass->u.diadic.right = value;
    if (reff) {
#if MWCCPS2
        ass->u.diadic.right->flags |= IROLF_Reffed;
#else
        value->flags |= IROLF_Reffed;
#endif
    }
    ass->rtype = type;
    IRO_AddToList(ass, list);
    return ass->u.diadic.left;
}

// IroLoop.c's ComputeLoopBody (0x589940, static there): the nodes of the loop
static void ComputeLoopBody(IRONaturalLoop* loop) {
    IRONode* header;
    IRONode* pred;
    BitVector* bv;
    UInt16 i;

    if (loop->nodes) {
        IRO_free(loop->nodes);
        loop->nodes = NULL;
    }
    header = loop->header;
    Bv_AllocVector(&bv, IRO_NumNodes + 1);
    Bv_Clear(bv);
    Bv_SetBit(header->index, bv);
    for (i = 0; i < header->numpred; i++) {
        pred = IRO_NodeTable[header->pred[i]];
        if (Bv_IsBitSet(header->index, pred->dom)) {
            Bv_SetBit(pred->index, bv);
            if (pred != header) {
                IRO_AddLoopPreds(pred, bv);
            }
        }
    }
    loop->nodes = bv;
}

// 0x561280
void IRO_LoopUnroller(void) {
    IRONaturalLoop* loops;
    IRONaturalLoop* loop;

    loops = IRO_FindLoopHeaders301();
    for (loop = loops; loop; loop = loop->next) {
        ComputeLoopBody(loop);
    }
    func_00587340(loops);
    for (loop = loops; loop; loop = loop->next) {
        if (loop->flags & LP301_INNER_LOOP) {
            IRO_Dump("IRO_LoopUnroller:Found loop with header %d\n", loop->header->index);
            IRO_DumpBits("Loop includes: ", loop->nodes);
            func_0055CFC0(loop);
            IRO_UpdateFlagsOnInts(IRO_FirstLinear);
        }
    }
    func_00589A80(loops);
    IRO_BuildFlowgraph(IRO_FirstLinear);
    IRO_CheckForUserBreak();
}

// 0x587340 (IroLoop.c): mark the loops that contain no other loop
void func_00587340(IRONaturalLoop* loops) {
    IRONaturalLoop* loop;
    IRONaturalLoop* other;

    for (loop = loops; loop; loop = loop->next) {
        loop->flags |= LP301_INNER_LOOP;
        loop->x4 = 0;
    }
    for (other = loops; other; other = other->next) {
        for (loop = loops; loop; loop = loop->next) {
            if (loop == other) {
                continue;
            }
            IRO_Dump(" header = %d \n", loop->header->index);
            IRO_Dump(" l1 bit vector = \n");
            IRO_DumpBits("", loop->nodes);
            IRO_Dump(" lp bit vector = \n");
            IRO_DumpBits("", other->nodes);
            if (Bv_IsSubset(other->nodes, loop->nodes)) {
                loop->flags &= ~LP301_INNER_LOOP;
            }
        }
    }
    for (loop = loops; loop; loop = loop->next) {
        if (loop->flags & LP301_INNER_LOOP) {
            IRO_Dump(
                "Found Inner Loop at header = %d, loopNest = %d \n", loop->header->index, loop->x4
            );
        }
    }
}

// 0x5887E0 (IroLoop.c): free the induction update linears list
#if MWCCPS2
__declspec(noinline) // b210 calls it (func_0055CFC0)
#endif
void func_005887E0(IRONaturalLoop* loop) {
    IROElmList* elm;
    IROElmList* next;

    if (loop && loop->x30) {
        for (elm = loop->x30; elm; elm = next) {
            next = elm->next;
            IRO_free(elm);
        }
        loop->x30 = NULL;
    }
}

// 0x588820 (IroLoop.c): collect the counting induction's update linears
#if MWCCPS2
__declspec(noinline) // b210 calls it (func_0055CFC0)
#endif
void func_00588820(IRONaturalLoop* loop) {
    if (loop && loop->induction) {
        func_005530D0(((IROLoopInd301*)loop->induction)->nd, func_00588880, loop);
    }
}

// 0x5888B0 (IroLoop.c): the induction side of the loop test
#if MWCCPS2
__declspec(noinline) // b210 calls it (func_0055D090)
#endif
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 2 // b145 asserts
IROLinear* func_005888B0(IRONaturalLoop* loop) {
    IROLinear* cond = loop->cond;

    if (cond->type == IROLinearOp2Arg) {
        return (loop->flags & LP301_INDUCTION_AT_LEFT) ? cond->u.diadic.left : cond->u.diadic.right;
    }
    if (cond->type == IROLinearOp1Arg) {
        return cond->u.monadic;
    }
    CError_Internal("IroLoop.c", CError_B145_LINE(0x870));
    return NULL;
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

#if MWCCPS2 // b210's calls pass only the induction (func_00583FD0 ignores the type)
#define IND_STEP(ind) (((CInt64 (*)(IROLoopInd301*))func_00583FD0)(ind))
#else
#define IND_STEP(ind) func_00583FD0(ind, NULL)
#endif

// 0x55CCF0: UpdateInductionIncrement: i += count * step (or -=) before
// `before`; returns the assignment
IROLinear* func_0055CCF0(IRONaturalLoop* loop, CInt64 count, IROLinear* before) {
    IROLoopInd301* ind = loop->induction;
    IROLinear* upd;
    IROLinear* value;
    IROLinear* dup;
    IROLinear* ass;
    Type* type;
    IROList list;

    IRO_InitList(&list);
    upd = ind->nd;
    type = IRO_UnqualifiedType(upd->rtype);
    value = IRO_NewIntConst(CInt64_Mul(count, ind->step), ConstType(type));
    IRO_AddToList(value, &list);
    dup = IRO_DuplicateExpr(upd->u.diadic.left, &list);
    ass = IRO_NewLinear(IROLinearOp2Arg);
    ass->index = ++IRO_NumLinear;
    ass->nodetype = (ind->flags & 4) ? ESUBASS : EADDASS;
    ass->u.diadic.left = dup;
    ass->u.diadic.right = value;
    ass->rtype = type;
    IRO_AddToList(ass, &list);
    IRO_Paste(list.head, list.tail, before);
    return list.tail;
}

// 0x55CDE0: BuildUnrolledBodyEntryTest: if !(count > factor) goto label (and
// the trip count's conditions, when it has any)
void func_0055CDE0(
    IRONaturalLoop* loop, IROList* list, IROLinear* count, CInt64 factor, CLabel* label
) {
    Type* type;
    IROLinear* value;
    IROLinear* dup;
    IROLinear* cmp;
    IROLinear* branch;
    IROList conds;

    type = IRO_UnqualifiedType(count->rtype);
    value = IRO_NewIntConst(factor, type);
    IRO_AddToList(value, list);
    dup = IRO_DuplicateExpr(count, list);
    cmp = IRO_NewLinear(IROLinearOp2Arg);
    cmp->index = ++IRO_NumLinear;
    cmp->nodetype = EGREATER;
    cmp->u.diadic.left = dup;
    cmp->u.diadic.right = value;
    value->flags |= IROLF_Reffed;
    cmp->rtype = type;
    IRO_AddToList(cmp, list);

    branch = IRO_NewLinear(IROLinearIfNot);
    branch->index = ++IRO_NumLinear;
    branch->u.label.cond = cmp;
    branch->u.label.cond->flags |= IROLF_Reffed;
    branch->rtype = type;
    branch->u.label.label = label;
    IRO_AddToList(branch, list);

    if (loop->hasCount && loop->countConds.tail) {
        func_0055CF00(loop->countConds.tail, &conds);
        IRO_AddToList(conds.head, list);
        branch = IRO_NewLinear(IROLinearIfNot);
        branch->index = ++IRO_NumLinear;
        branch->u.label.cond = list->tail;
        branch->u.label.cond->flags |= IROLF_Reffed;
        branch->rtype = type;
        branch->u.label.label = label;
        IRO_AddToList(branch, list);
    }
}

// 0x55CF00: a copy of expr relinearized on its own (delinearized to a
// statement and linearized again with the linear state saved around it)
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -24 // b145 asserts
void func_0055CF00(IROLinear* expr, IROList* list) {
    IROLinearState state;
    Statement* stmt;
    IROLinear* nd;
#if MWCCPS2 // b210 leaves it unset (the list always holds an expression)
    IROLinear* last;
#else
    IROLinear* last = NULL;
#endif

    func_00561390(&state);
    IRO_InitList(list);
    IRO_AddToList(IRO_NewLinear(IROLinearNop), list);
    IRO_DuplicateExpr(expr, list);
    stmt = IRO_Delinearize(NULL, list->head);
    if (!stmt) {
        CError_Internal(UNROLL_FILE, CError_B145_LINE(0xE1D));
    }
    IRO_Linearize(IRO_PreLinearize301(stmt));
    IRO_InitList(list);
    list->head = IRO_FirstLinear->next;
    for (nd = list->head; nd; nd = nd->next) {
        if (nd->type >= IROLinearOperand && nd->type <= IROLinearFunccall) {
            last = nd;
        }
    }
    list->tail = last;
    list->tail->next = NULL;
    func_00561320(&state);
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// 0x55CFC0: unroll one inner loop
void func_0055CFC0(IRONaturalLoop* loop) {
    UnrollWhilePattern pat;
    CInt64 factor;
    CInt64 tmp;

    func_00587540(loop);
    if (!(loop->flags & LP301_WHILE_LOOP)) {
        func_00584670(loop, TRUE);
    }
    func_00588820(loop);
    func_00588700(loop);
    func_00587270(loop);
    CInt64_SetLong(&tmp, copts.unrollfactor);
    factor = tmp;
    if (func_005605D0(loop, &factor, &pat)) {
        if (loop->flags & LP301_WHILE_LOOP) {
            func_0055F1B0(loop, factor, &pat);
        } else {
            func_0055D090(loop, factor);
        }
        IRO_RebuildNodeTable();
        IRO_ComputeSuccPred();
        IRO_ComputeDom();
    }
    func_005887E0(loop);
    func_005886C0(loop);
}

// 0x55D090: UnrollForLoop
void func_0055D090(IRONaturalLoop* loop, CInt64 factor) {
    IRONode* pre;
    IRONode* exitNode;
    IRONode* newPre;
    IRONode* node;
    IRONode* split;
    IRONode* oldNext;
    IRONode* gotoNode;
    IRONode* bodyNode;
    IRONode* testNode;
    IRONode* afterNode;
    IROLinear* at;
    IROLinear* preLinear;
    IROLinear* nd;
    IROLinear* splitAt;
    IROLinear* father;
    IROLinear* count;
    IROLinear* finalValue;
    IROLinear* countHead;
    IROLinear* earlyTest;
    IROLinear* gotoNd = NULL;
    IROLinear* bodyLabel;
    IROLinear* indUpdate;
    IROLinear* testLabel;
    IROLinear* indDup;
    IROLinear* cmp;
    IROLinear* branch;
    IROLinear* afterLabel;
    IROLinear* oldLast;
    IROLinear* newPreLast;
    CLabel* bodyTarget;
    CLabel* afterLbl;
    CLabel* testLbl;
    CLabel* bodyLbl;
    CLabel* lbl;
    IROList list;
    CInt64 one;
    CInt64 stride;
    CInt64 leftover;
    CInt64 adjust;
    CInt64 i;
    CInt64 val;
    Boolean bottomTest;
    Boolean isConst;
    Boolean needOrigLoop;
    Boolean needUnrollBodyTest;
    Boolean resetFinal;

    bottomTest = (loop->x10 != loop->header) || (loop->x10 == loop->x20);
    isConst = loop->hasCount && !CInt64_Equal(loop->count, cint64_negone);
    if (isConst && bottomTest) {
        loop->count = CInt64_Sub(loop->count, cint64_one);
    }

    if (IRO_Hook20) {
        ((UnrollFactorHook)IRO_Hook20)(loop, 1, &factor);
    } else {
        func_0055EC40(loop, 1, &factor);
    }
    if (CInt64_IsZero(&factor)) {
        IRO_Dump("UnrollForLoop:Aborting due unrollFactor == 0.");
        return;
    }

    if (loop->hasCount) {
        CInt64_SetLong(&one, 1);
        stride = CInt64_MulU(factor, one);
        leftover = CInt64_ModU(loop->count, stride);
        needOrigLoop = !CInt64_IsZero(&leftover);
        needUnrollBodyTest = CInt64_LessU(stride, loop->count);
    } else {
        leftover = cint64_zero;
        needOrigLoop = TRUE;
        needUnrollBodyTest = TRUE;
    }
    resetFinal = !needOrigLoop || !needUnrollBodyTest;
    IRO_Dump(
        "\tLeftOver = %d, NeedOrigLoop = %d, NeedUnrollBodyTest = %d, ResetUnrolledFinalValue = %d\n",
        (int)leftover.lo,
        needOrigLoop,
        needUnrollBodyTest,
        resetFinal
    );
    IRO_Dump(
        isConst ? "Unrolling loop with constant iteration count.\n"
                : "Unrolling loop with nonconstant iteration count.\n"
    );

    func_00589F20(loop, &exitNode, &at);
    if (!exitNode) {
        IRO_Dump("UnrollForLoop:Aborting due to no unique successor.\n");
        return;
    }
    if (!func_0058A030(loop, &preLinear, exitNode, &newPre, NULL)) {
        IRO_Dump(
            "UnrollForLoop:Aborting due to failure of IRO_CopyLoopTestToLoopPreheaderAndBranchToSuccNode.\n"
        );
        return;
    }

    bodyTarget = loop->x20->first->u.label.label;
    pre = loop->preheader;
    afterLbl = IRO_NewLabel();
    switch (pre->last->type) {
        case IROLinearGoto:
        case IROLinearGotoExpr:
        case IROLinearIf:
        case IROLinearIfNot:
        case IROLinearSwitch:
            break;
        default:
            // the preheader falls through: split the test off the header
            // when they share a node, then jump to the test
            if (loop->x10 == loop->header) {
                node = loop->x10;
                splitAt = node->first;
                for (nd = node->first; nd != node->last; nd = nd->next) {
                    if (nd->type != IROLinearNop && !(nd->flags & IROLF_Reffed)) {
                        splitAt = nd;
                    }
                }
                split = IRO_NewFlowGraphNode();
                loop->x10 = split;
                split->first = splitAt->next;
                split->last = loop->header->last;
                loop->header->last = splitAt;
                split->nextnode = loop->header->nextnode;
                loop->header->nextnode = split;
            }
            if (loop->x10->first->type != IROLinearLabel) {
                lbl = IRO_NewLabel();
                nd = IRO_NewLinear(IROLinearLabel);
                nd->index = IRO_NumLinear++;
                nd->u.label.label = lbl;
                nd->flags |= IROLF_StartsNode;
                IRO_Paste(nd, nd, loop->x10->first);
            } else {
                lbl = loop->x10->first->u.label.label;
            }
            nd = IRO_NewLinear(IROLinearGoto);
            nd->index = ++IRO_NumLinear;
            nd->u.label.label = lbl;
            IRO_PasteAfter(nd, nd, pre->last);
            break;
    }

    newPreLast = newPre->last;

    // the trip count in a temporary
    IRO_InitList(&list);
    if (bottomTest) {
        adjust = IND_STEP(loop->induction);
    } else {
        adjust = cint64_zero;
    }
    count = func_0055E6D0(&list, loop, adjust);
    IRO_Paste(list.head, list.tail, pre->last);
    countHead = list.head;
    father = IRO_LocateFather(count);
    if (IsAssignOfConstant(father, count)) {
        count = father->u.diadic.right;
    }

    // the unrolled loop's final value
    IRO_InitList(&list);
    finalValue = func_0055E500(count, factor, leftover, &list, loop);
    IRO_Paste(list.head, list.tail, pre->last);
    father = IRO_LocateFather(finalValue);
    if (IsAssignOfConstant(father, finalValue)) {
        finalValue = father->u.diadic.right;
    }

    // skip the unrolled body when the count is too small
    IRO_InitList(&list);
    func_0055CDE0(loop, &list, count, factor, afterLbl);
    IRO_Paste(list.head, list.tail, pre->last);
    earlyTest = list.tail;

    testLbl = IRO_NewLabel();
    if (copts.opt_addgotounrolledlooptest) {
        IRO_InitList(&list);
        gotoNd = IRO_NewLinear(IROLinearGoto);
        gotoNd->index = ++IRO_NumLinear;
        gotoNd->u.label.label = testLbl;
        IRO_AddToList(gotoNd, &list);
        IRO_Paste(list.head, list.tail, pre->last);
    }

    // the unrolled body
    IRO_InitList(&list);
    IRO_AddToList(func_00554490(), &list);
    bodyLbl = list.tail->u.label.label;
    IRO_Paste(list.head, list.tail, pre->last);
    bodyLabel = list.head;
    i = cint64_zero;
    val = cint64_zero;
    while (CInt64_Less(i, factor)) {
        func_0055DC70(loop, pre->last, &val, !CInt64_IsZero(&i));
        i = CInt64_Add(i, cint64_one);
    }
    indUpdate = func_0055CCF0(loop, factor, pre->last);

    // the unrolled loop's test
    IRO_InitList(&list);
    testLabel = IRO_NewLinear(IROLinearLabel);
    testLabel->index = IRO_NumLinear++;
    testLabel->u.label.label = testLbl;
    testLabel->flags |= IROLF_StartsNode;
    IRO_AddToList(testLabel, &list);
    IRO_Paste(list.head, list.tail, pre->last);

    IRO_InitList(&list);
    IRO_DuplicateExpr(func_005888B0(loop), &list);
    indDup = list.tail;
    if (resetFinal) {
        IRO_DuplicateExpr(func_00588900(loop), &list);
    } else {
        IRO_DuplicateExpr(finalValue, &list);
    }
    cmp = IRO_NewLinear(loop->cond->type);
    *cmp = *loop->cond;
    cmp->index = ++IRO_NumLinear;
    cmp->next = NULL;
    cmp->expr = NULL;
    if (loop->flags & LP301_INDUCTION_AT_LEFT) {
        cmp->u.diadic.left = indDup;
        cmp->u.diadic.right = list.tail;
    } else {
        cmp->u.diadic.left = list.tail;
        cmp->u.diadic.right = indDup;
    }
    IRO_AddToList(cmp, &list);
    IRO_Paste(list.head, list.tail, pre->last);
    cmp = list.tail;

    IRO_InitList(&list);
    branch = IRO_NewLinear(loop->x10->last->type);
    branch->index = ++IRO_NumLinear;
    IRO_AddToList(branch, &list);
    branch->u.label.label = bodyLbl;
    branch->u.label.cond = cmp;
    branch->u.label.cond->flags |= IROLF_Reffed;
    branch->rtype = IRO_UnqualifiedType(loop->x10->last->rtype);
    IRO_Paste(list.head, list.tail, pre->last);
    branch = list.tail;

    IRO_InitList(&list);
    afterLabel = IRO_NewLinear(IROLinearLabel);
    afterLabel->index = IRO_NumLinear++;
    afterLabel->u.label.label = afterLbl;
    afterLabel->flags |= IROLF_StartsNode;
    IRO_AddToList(afterLabel, &list);
    IRO_Paste(list.head, list.tail, pre->last);

    // the new nodes
    oldLast = pre->last;
    oldNext = pre->nextnode;
    pre->first = countHead;
    pre->last = earlyTest;
    gotoNode = NULL;
    if (copts.opt_addgotounrolledlooptest) {
        gotoNode = IRO_NewFlowGraphNode();
        gotoNode->first = gotoNd;
        gotoNode->last = gotoNd;
        pre->nextnode = gotoNode;
    }
    bodyNode = IRO_NewFlowGraphNode();
    bodyNode->first = bodyLabel;
    bodyNode->last = indUpdate;
    bodyLabel->u.label.label->stmt = (Statement*)bodyNode;
    if (gotoNode) {
        gotoNode->nextnode = bodyNode;
    } else {
        pre->nextnode = bodyNode;
    }
    testNode = IRO_NewFlowGraphNode();
    testNode->first = testLabel;
    testNode->last = branch;
    testLabel->u.label.label->stmt = (Statement*)testNode;
    bodyNode->nextnode = testNode;
    afterNode = IRO_NewFlowGraphNode();
    afterNode->first = afterLabel;
    afterNode->last = oldLast;
    testNode->nextnode = afterNode;
    afterNode->nextnode = oldNext;
    if (IRO_LastNode == pre) {
        IRO_LastNode = afterNode;
    }

    if (copts.opt_unrollpostloop && isConst && !CInt64_IsZero(&leftover)) {
        func_0055DB60(loop, testNode, leftover);
        needOrigLoop = FALSE;
    }

    if (!needOrigLoop) {
        for (node = loop->x20; node && node != loop->x10->nextnode; node = node->nextnode) {
            NopNonLabels(node);
        }
        NopNonLabels(((IROLoopInd301*)loop->induction)->fnode);
        IRO_NopOut(pre->last->u.label.cond);
        pre->last->type = IROLinearNop;
        oldLast->u.label.label = bodyTarget;
    }

    if (needUnrollBodyTest) {
        return;
    }

    IRO_NopOut(newPreLast->u.label.cond);
    newPreLast->type = IROLinearNop;
    IRO_NopOut(testNode->last->u.label.cond);
    testNode->last->type = IROLinearNop;
    if (gotoNode) {
        gotoNode->last->type = IROLinearNop;
    }
    for (nd = pre->first; nd; nd = nd->next) {
        if (!(nd->flags & IROLF_Reffed)) {
            IRO_NopOut(nd);
        }
        if (nd == pre->last) {
            break;
        }
    }
}

// 0x55DB60: the leftover iterations as straight-line code in a new node after
// `after`
void func_0055DB60(IRONaturalLoop* loop, IRONode* after, CInt64 count) {
    IRONode* node;
    IROLinear* nop;
    CInt64 i;
    CInt64 val;

    node = IRO_NewFlowGraphNode();
    nop = IRO_NewLinear(IROLinearNop);
    nop->index = IRO_NumLinear++;
    nop->next = NULL;
    nop->flags |= IROLF_StartsNode;
    node->last = nop;
    node->first = node->last;
    node->nextnode = after->nextnode;
    after->nextnode = node;
    nop->next = after->last->next;
    after->last->next = nop;

    i = cint64_zero;
    val = cint64_zero;
    while (CInt64_Less(i, count)) {
        func_0055DC70(loop, node->last, &val, !CInt64_IsZero(&i));
        i = CInt64_Add(i, cint64_one);
    }
    func_0055CCF0(loop, count, node->last);
}

// 0x55DC70: IterateForLoopBody: one copy of the loop body (from the test's
// successor to the test) before `before`; every copy after the first has its
// induction references offset by one more step
void func_0055DC70(IRONaturalLoop* loop, IROLinear* before, CInt64* val, Boolean notFirst) {
    IROLinear* first = NULL;
    IROLinear* last = NULL;
    IROLinear* nd;
    IROLinear* end;
    IRONode* node;
    IROList list;

    for (node = loop->x20; node && node != loop->x10->nextnode; node = node->nextnode) {
        IRO_InitList(&list);
        end = node->last->next;
        for (nd = node->first; nd != end; nd = nd->next) {
            if (func_00588690(loop, nd)) {
                continue;
            }
            if (nd->stmt) {
                nd->stmt->flags |= StmtFlag_Unrolled;
            }
            if (func_00588740(loop, nd) ||
                nd->type == IROLinearLabel ||
                (nd->flags & IROLF_Reffed)) {
                continue;
            }
            IRO_DuplicateExpr(nd, &list);
            if (!first) {
                first = list.head;
            }
            last = list.tail;
        }
        if (list.head && list.tail) {
            IRO_Paste(list.head, list.tail, before);
        }
    }

    if (notFirst) {
        *val = CInt64_Add(*val, IND_STEP(loop->induction));
        func_0055DDA0(first, last, *val, loop);
    }
}

// 0x55DDA0: ChangeInductionReference: every reference to the induction
// variable in first..last becomes i + val; inside a subscript (b + (i op c))
// the offset is folded into the subscript's add instead. With
// opt_docrossconversionofinductionreference also through an add/multiply
// under a shifted subscript.
void func_0055DDA0(IROLinear* first, IROLinear* last, CInt64 val, IRONaturalLoop* loop) {
    IROLoopInd301* ind = loop->induction;
    IROLinear* nd;
    IROLinear* next;
    IROLinear* add;
    IROLinear* father;
    IROLinear* f2;
    IROLinear* shl;
    IROLinear* top;
    IROLinear* value;
    Object* obj;
    Type* type;
    Type* typconType = NULL;
    Type* ctype;
    Boolean isUnsigned;
    Boolean hasTypcon;
    Boolean widens;
    Boolean done;
    CInt64 scale;
    CInt64 scaled;
    CInt64 product;
    IROList list;

    type = IRO_UnqualifiedType(ind->nd->rtype);
    isUnsigned = is_unsigned(type);

    for (nd = first; nd; nd = next) {
        next = nd->next;
        obj = IRO_IsVariable(nd);
        if (obj && ((IROLoopInd301*)loop->induction)->var->object == obj) {
            add = IRO_NewLinear(IROLinearOp2Arg);
            add->index = ++IRO_NumLinear;
            add->nodetype = EADD;
            add->rtype = type;

            father = IRO_LocateFather(nd);
            hasTypcon = FALSE;
            widens = TRUE;
            if (father && father->type == IROLinearOp1Arg && father->nodetype == ETYPCON) {
                typconType = IRO_UnqualifiedType(father->rtype);
                father = IRO_LocateFather(father);
                hasTypcon = TRUE;
                if (typconType->type != nd->rtype->type || typconType->size < nd->rtype->size) {
                    widens = FALSE;
                }
            }

            done = FALSE;
            if (copts.opt_docrossconversionofinductionreference &&
                widens &&
                father &&
                father->type == IROLinearOp2Arg &&
                father->nodetype == EADD) {
                // (b + (((i + x) << c)))
                if ((shl = IRO_LocateFather(father)) &&
                    shl->type == IROLinearOp2Arg &&
                    shl->nodetype == ESHL &&
                    IRO_IsIntConstant(shl->u.diadic.right) &&
                    (top = IRO_LocateFather(shl)) &&
                    top->type == IROLinearOp2Arg &&
                    top->nodetype == EADD &&
                    top->u.diadic.right == shl) {
                    IRO_InitList(&list);
                    scale = CInt64_Shl(cint64_one, shl->u.diadic.right->u.node->data.intval);
                    if (isUnsigned) {
                        product = CInt64_MulU(scale, val);
                    } else {
                        product = CInt64_Mul(scale, val);
                    }
                    ctype = (top->rtype->type == TYPEPOINTER) ? func_0055A650(top->rtype)
                                                              : (hasTypcon ? typconType : type);
                    value = IRO_NewIntConst(product, ctype);
                    IRO_AddToList(value, &list);
                    top->u.diadic.right = value;
                    value->flags |= IROLF_Reffed;
                    IRO_Paste(list.head, list.tail, top);
                    IRO_InitList(&list);
                    add->u.diadic.left = top;
                    add->u.diadic.right = shl;
                    add->flags |= IROLF_Reffed;
                    add->rtype = IRO_UnqualifiedType(top->rtype);
                    IRO_LocateFather_Cut_And_Paste_Without_Nopping(top, add);
                    IRO_AddToList(add, &list);
                    IRO_PasteAfter(list.head, list.tail, top);
                    done = TRUE;
                }
            } else if (copts.opt_docrossconversionofinductionreference &&
                       widens &&
                       father &&
                       father->type == IROLinearOp2Arg &&
                       father->nodetype == EMUL &&
                       IRO_IsIntConstant(father->u.diadic.right)) {
                // (b + (((i * m) + x) << c))
                if ((f2 = IRO_LocateFather(father)) &&
                    f2->type == IROLinearOp2Arg &&
                    f2->nodetype == EADD &&
                    (shl = IRO_LocateFather(f2)) &&
                    shl->type == IROLinearOp2Arg &&
                    shl->nodetype == ESHL &&
                    IRO_IsIntConstant(shl->u.diadic.right) &&
                    (top = IRO_LocateFather(shl)) &&
                    top->type == IROLinearOp2Arg &&
                    top->nodetype == EADD &&
                    top->u.diadic.right == shl) {
                    IRO_InitList(&list);
                    scale = CInt64_Shl(cint64_one, shl->u.diadic.right->u.node->data.intval);
                    if (isUnsigned) {
                        scaled = CInt64_MulU(scale, val);
                        product = CInt64_MulU(father->u.diadic.right->u.node->data.intval, scaled);
                    } else {
                        scaled = CInt64_Mul(scale, val);
                        product = CInt64_Mul(father->u.diadic.right->u.node->data.intval, scaled);
                    }
                    ctype = (top->rtype->type == TYPEPOINTER) ? func_0055A650(top->rtype)
                                                              : (hasTypcon ? typconType : type);
                    value = IRO_NewIntConst(product, ctype);
                    IRO_AddToList(value, &list);
                    top->u.diadic.right = value;
                    value->flags |= IROLF_Reffed;
                    IRO_Paste(list.head, list.tail, top);
                    IRO_InitList(&list);
                    add->u.diadic.left = top;
                    add->u.diadic.right = shl;
                    add->flags |= IROLF_Reffed;
                    add->rtype = top->rtype;
                    IRO_LocateFather_Cut_And_Paste_Without_Nopping(top, add);
                    IRO_AddToList(add, &list);
                    IRO_PasteAfter(list.head, list.tail, top);
                    done = TRUE;
                }
            } else if (widens &&
                       father &&
                       father->type == IROLinearOp2Arg &&
                       (father->nodetype == ESHL || father->nodetype == EMUL)) {
                // (b + (i op c)): fold val * scale into the add
                if (IRO_IsIntConstant(father->u.diadic.right) &&
                    (f2 = IRO_LocateFather(father)) &&
                    f2->type == IROLinearOp2Arg &&
                    f2->nodetype == EADD &&
                    f2->u.diadic.right == father &&
                    (top = IRO_LocateFather(f2))) {
                    IRO_InitList(&list);
                    scale = father->u.diadic.right->u.node->data.intval;
                    if (father->nodetype == ESHL) {
                        scale = CInt64_Shl(cint64_one, scale);
                    }
                    if (isUnsigned) {
                        product = CInt64_MulU(scale, val);
                    } else {
                        product = CInt64_Mul(scale, val);
                    }
                    ctype = (f2->rtype->type == TYPEPOINTER) ? func_0055A650(f2->rtype)
                                                             : (hasTypcon ? typconType : type);
                    value = IRO_NewIntConst(product, ctype);
                    IRO_AddToList(value, &list);
                    IRO_AddToList(add, &list);
                    add->u.diadic.right = value;
                    IRO_Paste(list.head, list.tail, top);
                    IRO_LocateFather_Cut_And_Paste_Without_Nopping(f2, add);
                    add->u.diadic.left = f2;
                    add->rtype = IRO_UnqualifiedType(f2->rtype);
                    done = TRUE;
                }
            }

            if (!done) {
                value = IRO_NewIntConst(val, ConstType(type));
                add->u.diadic.right = value;
                add->u.diadic.right->flags |= IROLF_Reffed;
                value->next = add;
                add->u.diadic.left = nd;
                IRO_LocateFather_Cut_And_Paste_Without_Nopping(nd, add);
                add->flags |= IROLF_Reffed;
                nd->next = value;
                add->next = next;
            }
        }
        if (nd == last) {
            break;
        }
    }
}

// 0x55E500: BuildNewFinalvalue: a temporary holding bound - step * leftover
// (!= tests) or bound - step * factor
IROLinear* func_0055E500(
    IROLinear* count, CInt64 factor, CInt64 leftover, IROList* list, IRONaturalLoop* loop
) {
    Type* type;
    IROLinear* value;
    IROLinear* dup;
    IROLinear* sub;

    type = IRO_UnqualifiedType(func_00588900(loop)->rtype);
    if (loop->cond->nodetype == ENOTEQU) {
        value = IRO_NewIntConst(CInt64_Mul(IND_STEP(loop->induction), leftover), ConstType(type));
    } else {
        value = IRO_NewIntConst(CInt64_Mul(IND_STEP(loop->induction), factor), ConstType(type));
    }
    IRO_AddToList(value, list);
    dup = IRO_DuplicateExpr(func_00588900(loop), list);
    sub = IRO_NewLinear(IROLinearOp2Arg);
    sub->index = ++IRO_NumLinear;
    sub->nodetype = ESUB;
    sub->u.diadic.left = dup;
    sub->u.diadic.right = value;
    sub->rtype = type;
    IRO_AddToList(sub, list);
    return AssignToNewTemp(sub, type, list, TRUE, TRUE);
}

// 0x55E6D0: BuildOrigIterationCount: a temporary holding the trip count:
// the constant count, the count expression, or
// ((bound - (init + adjust)) + step [+-1 for < / >]) / step
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -1 // b145 asserts
IROLinear* func_0055E6D0(IROList* list, IRONaturalLoop* loop, CInt64 adjust) {
    IROLoopInd301* ind;
    IROLinear* init;
    IROLinear* bound;
    IROLinear* expr;
    IROLinear* value;
    IROLinear* op;
    IROLinear* left;
    IROLinear* cond;
    Type* btype;
    Type* type;
    Boolean ptrDiff;
    Boolean initCancels;
    int shift;

    init = func_00588A10(loop, loop->induction);
    bound = func_00588900(loop);
    btype = IRO_UnqualifiedType(bound->rtype);
    ptrDiff = bound->rtype->type == TYPEPOINTER;
    if (ptrDiff) {
        ptrDiff = init->rtype->type == TYPEPOINTER;
    }

    if (loop->hasCount) {
        if (!CInt64_Equal(loop->count, cint64_negone)) {
            expr = IRO_NewIntConst(loop->count, ptrDiff ? func_0055A660() : btype);
            IRO_AddToList(expr, list);
        } else {
            IRO_DuplicateExpr(loop->countExpr.tail, list);
            expr = list->tail;
        }
#if MWCCPS2 && MWCCPS2_BUILD < 210 // b145 does not evaluate the count first
        return AssignToNewTemp(expr, expr->rtype, list, FALSE, TRUE);
#else
        return AssignToNewTemp(expr, expr->rtype, list, TRUE, TRUE);
#endif
    }

    initCancels = IRO_IsIntConstant(init);
    if (initCancels) {
        initCancels = CInt64_Equal(init->u.node->data.intval, CInt64_Neg(adjust)) != 0;
    }
    if (!initCancels) {
        init = IRO_DuplicateExpr(init, list);
        if (!CInt64_IsZero(&adjust)) {
            value = IRO_NewIntConst(adjust, ConstType(init->rtype));
            IRO_AddToList(value, list);
            op = IRO_NewLinear(IROLinearOp2Arg);
            op->index = ++IRO_NumLinear;
            op->nodetype = EADD;
            op->u.diadic.left = init;
            op->u.diadic.right = value;
            op->rtype = init->rtype;
            IRO_AddToList(op, list);
            init = op;
        }
    }

    bound = IRO_DuplicateExpr(bound, list);
    ind = loop->induction;
    if (!ind) {
        CError_Internal(UNROLL_FILE, CError_B145_LINE(0x81D));
    }
    if (CInt64_IsZero(&ind->step)) {
        CError_Internal(UNROLL_FILE, CError_B145_LINE(0x822));
    }
    if (!initCancels) {
        left = IRO_NewLinear(IROLinearOp2Arg);
        left->index = ++IRO_NumLinear;
        left->nodetype = ESUB;
        left->u.diadic.left = bound;
        left->u.diadic.right = init;
        left->rtype = ptrDiff ? func_0055A660() : btype;
        IRO_AddToList(left, list);
    } else {
        left = bound;
        if (ptrDiff) {
            bound->rtype = func_0055A660();
        }
    }

    value = IRO_NewIntConst(IND_STEP(loop->induction), left->rtype);
    IRO_AddToList(value, list);
    expr = IRO_NewLinear(IROLinearOp2Arg);
    expr->index = ++IRO_NumLinear;
    expr->nodetype = EADD;
    expr->u.diadic.left = left;
    expr->u.diadic.right = value;
    expr->rtype = IRO_UnqualifiedType(left->rtype);
    IRO_AddToList(expr, list);

    cond = loop->cond;
    if (cond->type == IROLinearOp2Arg && cond->nodetype == ELESS) {
        left = expr;
        value = IRO_NewIntConst(cint64_negone, expr->rtype);
        IRO_AddToList(value, list);
        expr = IRO_NewLinear(IROLinearOp2Arg);
        expr->index = ++IRO_NumLinear;
        expr->nodetype = EADD;
        expr->u.diadic.left = left;
        expr->u.diadic.right = value;
        expr->rtype = IRO_UnqualifiedType(left->rtype);
        IRO_AddToList(expr, list);
    } else if (cond->type == IROLinearOp2Arg && cond->nodetype == EGREATER) {
        left = expr;
        value = IRO_NewIntConst(cint64_one, expr->rtype);
        IRO_AddToList(value, list);
        expr = IRO_NewLinear(IROLinearOp2Arg);
        expr->index = ++IRO_NumLinear;
        expr->nodetype = EADD;
        expr->u.diadic.left = left;
        expr->u.diadic.right = value;
        expr->rtype = IRO_UnqualifiedType(left->rtype);
        IRO_AddToList(expr, list);
    }

    value = IRO_NewIntConst(IND_STEP(loop->induction), expr->rtype);
    if (CInt64_Equal(value->u.node->data.intval, cint64_one)) {
        return AssignToNewTemp(expr, expr->rtype, list, TRUE, TRUE);
    }
    type = value->rtype;
    if (type->type == TYPEINT &&
        TYPE_INTEGRAL(type)->integral < 17 &&
        func_00555B60(value, &shift) &&
        shift > 0) {
        op = IRO_NewLinear(IROLinearOp2Arg);
        op->index = ++IRO_NumLinear;
        op->nodetype = ESHR;
        op->u.diadic.left = expr;
        op->u.diadic.right = value;
        CInt64_SetLong(&value->u.node->data.intval, shift);
        op->rtype = expr->rtype;
    } else {
        op = IRO_NewLinear(IROLinearOp2Arg);
        op->index = ++IRO_NumLinear;
        op->nodetype = EDIV;
        op->u.diadic.left = expr;
        op->u.diadic.right = value;
        op->rtype = expr->rtype;
    }
    IRO_AddToList(value, list);
    IRO_AddToList(op, list);
    return AssignToNewTemp(op, op->rtype, list, TRUE, TRUE);
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// 0x55EC40: the unroll factor (when no back end hook chooses it): at most
// count / stride, the whole loop when that is within copts.unknownF6, else the
// factor (searching downwards with opt_findoptimalunrollfactor) that leaves
// the fewest iterations over
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -1 // b145 asserts
void func_0055EC40(IRONaturalLoop* loop, int stride, CInt64* factor) {
    CInt64 count = loop->count;
    CInt64 strideV;
    CInt64 maxFactor;
    CInt64 rem;
    CInt64 limit;
    CInt64 f;
    CInt64 best;
    CInt64 bestCost;
    CInt64 cost;
    CInt64 tmp;
    Boolean isUnsigned;
    Boolean small;

    if (loop->hasCount && !CInt64_Equal(count, cint64_negone)) {
        isUnsigned = is_unsigned(IRO_UnqualifiedType(func_00588900(loop)->rtype));
        if (stride == 0) {
            CError_Internal(UNROLL_FILE, CError_B145_LINE(0x744));
        }
        CInt64_SetLong(&strideV, stride);
        if (isUnsigned) {
            small = CInt64_LessU(count, strideV);
        } else {
            small = CInt64_Less(count, strideV);
        }
        if (small) {
            *factor = cint64_zero;
        } else {
            switch (stride) {
                case 1:
                    maxFactor = count;
                    break;
                case 2:
                    maxFactor = CInt64_ShrU(count, cint64_one);
                    break;
                case 4:
                    CInt64_SetLong(&tmp, 2);
                    maxFactor = CInt64_ShrU(count, tmp);
                    break;
                case 8:
                    CInt64_SetLong(&tmp, 3);
                    maxFactor = CInt64_ShrU(count, tmp);
                    break;
                case 16:
                    CInt64_SetLong(&tmp, 4);
                    maxFactor = CInt64_ShrU(count, tmp);
                    break;
                default:
                    maxFactor = CInt64_Div(count, strideV);
                    break;
            }
            if (CInt64_LessU(maxFactor, *factor)) {
                *factor = maxFactor;
            }
            f = *factor;
            switch (stride) {
                case 1:
                    rem = cint64_zero;
                    break;
                case 2:
                    rem = CInt64_And(count, cint64_one);
                    break;
                case 4:
                    CInt64_SetLong(&tmp, 3);
                    rem = CInt64_And(count, tmp);
                    break;
                case 8:
                    CInt64_SetLong(&tmp, 7);
                    rem = CInt64_And(count, tmp);
                    break;
                case 16:
                    CInt64_SetLong(&tmp, 15);
                    rem = CInt64_And(count, tmp);
                    break;
                default:
                    rem = CInt64_Mod(count, strideV);
                    break;
            }
            CInt64_SetULong(&limit, (UInt32)(SInt32)(SInt8)copts.unknownF6);
            if (CInt64_LessEqualU(maxFactor, limit)) {
                *factor = maxFactor;
            } else if (CInt64_IsZero(&f)) {
                *factor = cint64_zero;
            } else {
                CInt64_SetLong(&best, 0x7FFFFFFF);
                CInt64_SetLong(&bestCost, 0x7FFFFFFF);
                for (;;) {
                    cost = CInt64_Add(CInt64_Mul(CInt64_Mod(maxFactor, f), strideV), rem);
                    if (CInt64_Less(cost, bestCost)) {
                        bestCost = cost;
                        best = f;
                    }
                    if (!copts.opt_findoptimalunrollfactor && stride == 1) {
                        break;
                    }
                    if (!copts.opt_findoptimalvectorunrollfactor && stride > 1) {
                        break;
                    }
                    f = CInt64_Add(f, cint64_negone);
                    if (!CInt64_GreaterEqualU(CInt64_Mul(f, f), maxFactor)) {
                        break;
                    }
                }
                *factor = best;
            }
        }
    }
    IRO_Dump(
        "---- IterCount = %d, VectorStride = %d, unrollFactor = %d\n",
        (int)count.lo,
        stride,
        (int)factor->lo
    );
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// 0x5605D0: IsLoopUnrollable
Boolean func_005605D0(IRONaturalLoop* loop, CInt64* factor, UnrollWhilePattern* pat) {
    UInt32 flags = loop->flags;
    IROLinear* bound;
    IROLinear* cond;
    IROLoopInd301* ind;

    if (flags & 0x244016) {
        IRO_Dump("IsLoopUnrollable:No due to prohibited loop flags that are set ");
        func_00583D30(loop->flags & 0x244016);
        IRO_Dump("\n");
        return FALSE;
    }
    if (!(flags & LP301_HEADER_FOLLOWS_UPDATE)) {
        IRO_Dump("IsLoopUnrollable:No due to required loop flags that are not set ");
        func_00583D30((loop->flags & LP301_HEADER_FOLLOWS_UPDATE) ^ LP301_HEADER_FOLLOWS_UPDATE);
        IRO_Dump("\n");
        return FALSE;
    }
    // (3.0.1 tests for an unsupported form here with a condition that can
    // never hold: the test node is the header but is not the header)
    if (!loop->preheader) {
        IRO_Dump("IsLoopUnrollable:No because a unique preheader could not be found\n");
        return FALSE;
    }
    if (loop->header->first->flags & IROLF_VecOpBase) {
        IRO_Dump("IsLoopUnrollable:No because the loop has already been vectorized\n");
        return FALSE;
    }
    if ((SInt32)loop->x50 > copts.unrollinstrfactor) {
        IRO_Dump("IsLoopUnrollable:No because loop size greater than threshold\n");
        return FALSE;
    }
    if (flags & LP301_WHILE_LOOP) {
        return func_00560840(loop, factor, pat) ? TRUE : FALSE;
    }

    bound = func_00588900(loop);
    if (!IRO_IsIntConstant(bound) && !(bound->flags & IROLF_LoopInvariant)) {
        IRO_Dump("IsLoopUnrollable:No because Loop Upper Bound is Variant in the loop\n");
        return FALSE;
    }
    if (IRO_HasSideEffect(bound)) {
        IRO_Dump("IsLoopUnrollable:No because Loop Upper Bound is has a side effect\n");
        return FALSE;
    }
    ind = loop->induction;
    if (!ind->init) {
        IRO_Dump(
            "IsLoopUnrollable:No because there is no initialization of loop index in PreHeader\n"
        );
        return FALSE;
    }
    if (!IRO_IsVariable(ind->init->u.diadic.left)) {
        IRO_Dump("IsLoopUnrollable:No because initial value of induction stored through pointer\n");
        return FALSE;
    }
    if (IRO_HasSideEffect(func_00588A10(loop, loop->induction))) {
        IRO_Dump("IsLoopUnrollable:No because Loop lower bound is has a side effect\n");
        return FALSE;
    }
    if (!loop->hasCount) {
        IRO_Dump("IsLoopUnrollable:No because can't figure out an iteration count\n");
        return FALSE;
    }
    if (!(loop->flags & LP301_IF_EXPR_NONCANONICAL)) {
        return TRUE;
    }
    cond = loop->cond;
    if (cond &&
        cond->type == IROLinearOp2Arg &&
        cond->nodetype == ENOTEQU &&
        loop->hasCount &&
        !CInt64_Equal(loop->count, cint64_negone)) {
        return TRUE;
    }
    IRO_Dump("IsLoopUnrollable:No because loop test operator is unsupported\n");
    return FALSE;
}

// 0x55C960 (IroType.c): the type of a comparison: bool in C++ with bool
// keywords, else int
Type* func_0055C960(void) {
    if (copts.cplusplus && copts.booltruefalse) {
        return (Type*)&stbool;
    }
    return (Type*)&stsignedint;
}

// the induction variable < bound, for the while-loop unroller
static IROLinear* WhileLessTest(IROLoopInd301* ind, IROLinear* bound, IROList* list) {
    IROLinear* left;
    IROLinear* cmp;

    IRO_DuplicateExpr(ind->nd->u.diadic.left, list);
    list->tail->flags &= ~IROLF_Assigned;
    left = list->tail;
    IRO_DuplicateExpr(bound, list);
    list->tail->flags &= ~IROLF_Assigned;
    cmp = IRO_NewLinear(IROLinearOp2Arg);
    cmp->nodetype = ELESS;
    cmp->rtype = func_0055C960();
    cmp->index = ++IRO_NumLinear;
    cmp->next = NULL;
    cmp->u.diadic.left = left;
    cmp->u.diadic.right = list->tail;
    IRO_AddToList(cmp, list);
    cmp->flags |= IROLF_Reffed;
    return cmp;
}

// a branch like the loop's test to label on cond
static IROLinear* WhileBranch(IRONaturalLoop* loop, CLabel* label, IROLinear* cond, IROList* list) {
    IROLinear* br;

    br = IRO_NewLinear(loop->x10->last->type);
    br->index = ++IRO_NumLinear;
    IRO_AddToList(br, list);
    br->u.label.label = label;
    br->u.label.cond = cond;
    br->u.label.cond->flags |= IROLF_Reffed;
    br->rtype = loop->x10->last->rtype;
    return br;
}

// a duplicate of the induction variable's reference on list, not marked
// assigned (nor its address)
static void DupIndRef(IROLoopInd301* ind, IROList* list) {
    IRO_DuplicateExpr(ind->nd->u.diadic.left, list);
    list->tail->flags &= ~IROLF_Assigned;
    list->tail->u.monadic->flags &= ~IROLF_Assigned;
}

// 0x55F1B0: UnrollWhileLoop: while (n--) { b[i >> k] op= c << (i & m); i++; }
// gets a word-at-a-time loop in front: up to the next word boundary the
// original body, then 8 words per iteration with the whole-word mask, then
// the original loop for the rest
void func_0055F1B0(IRONaturalLoop* loop, CInt64 factor, UnrollWhilePattern* pat) {
    IRONode* pre;
    IRONode* node;
    IRONode* oldNext;
    IRONode* n1;
    IRONode* n2;
    IRONode* n3;
    IRONode* n4;
    IRONode* n5;
    IRONode* n6;
    IRONode* n7;
    IRONode* n8;
    IROLoopInd301* ind;
    IROLinear* target;
    IROLinear* condCopy;
    IROLinear* exitTest;
    IROLinear* count;
    IROLinear* countHead;
    IROLinear* limit;
    IROLinear* limit2;
    IROLinear* limit3;
    IROLinear* guardTail;
    IROLinear* bodyLabel;
    IROLinear* bodyBranch;
    IROLinear* gotoNd;
    IROLinear* body2Label;
    IROLinear* indUpdate;
    IROLinear* testLabel;
    IROLinear* test2;
    IROLinear* oldPreLast;
    IROLinear* oldCond;
    IROLinear* zero;
    IROLinear* ass;
    IROLinear* cmp;
    IROLinear* nd;
    IROLinear* last;
    IROLinear* lab;
    CLabel* preLbl;
    CLabel* exitLbl;
    CLabel* bodyLbl;
    CLabel* body2Lbl;
    CLabel* testLbl;
    CInt64 eight;
    IROList list;

    IRO_Dump("while(n--) loop \n");
    pre = loop->preheader;
    ind = loop->induction;
    preLbl = pre->last->u.label.label;
    target = func_00554440(preLbl, pre->last);

    // the test, copied before the loop: exit when it fails
    IRO_InitList(&list);
    func_00552B00(target->next, loop->cond->u.monadic, &list);
    IRO_DuplicateExpr(loop->cond, &list);
    IRO_Paste(list.head, list.tail, pre->last);
    condCopy = list.tail;

    IRO_InitList(&list);
    last = loop->x10->last;
    exitTest = IRO_NewLinear(IROLinearNop);
    exitTest->index = ++IRO_NumLinear;
    if (last->type == IROLinearIf) {
        exitTest->type = IROLinearIfNot;
    } else {
        exitTest->type = IROLinearIf;
    }
    IRO_AddToList(exitTest, &list);
    exitLbl = IRO_NewLabel();
    exitTest->u.label.label = exitLbl;
    exitTest->u.label.cond = condCopy;
    exitTest->u.label.cond->flags |= IROLF_Reffed;
    exitTest->rtype = IRO_UnqualifiedType(loop->x10->last->rtype);
    IRO_Paste(list.head, list.tail, pre->last);

    IRO_InitList(&list);
    count = func_005604B0(&list, loop);
    IRO_Paste(list.head, list.tail, pre->last);
    countHead = list.head;

    IRO_InitList(&list);
    limit = func_005602F0(ind, factor, &list);
    IRO_Paste(list.head, list.tail, pre->last);

    IRO_InitList(&list);
    limit2 = func_00560100(count, factor, &list, loop);
    IRO_Paste(list.head, list.tail, pre->last);

    IRO_InitList(&list);
    limit3 = func_0055FF80(count, factor, &list, loop);
    IRO_Paste(list.head, list.tail, pre->last);

    IRO_InitList(&list);
    func_0055CDE0(loop, &list, count, factor, preLbl);
    IRO_Paste(list.head, list.tail, pre->last);
    guardTail = list.tail;

    // the original body up to the word boundary
    IRO_InitList(&list);
    IRO_AddToList(func_00554490(), &list);
    nd = list.tail;
    bodyLbl = nd->u.label.label;
    IRO_Paste(list.head, nd, pre->last);
    bodyLabel = list.head;
    for (node = loop->x20; node && node != loop->x10; node = node->nextnode) {
        IRO_InitList(&list);
        last = node->last;
        for (nd = node->first;; nd = nd->next) {
            if (nd->stmt) {
                nd->stmt->flags |= StmtFlag_Unrolled;
            }
            if (nd->type != IROLinearLabel && !(nd->flags & IROLF_Reffed)) {
                IRO_DuplicateExpr(nd, &list);
            }
            if (nd == last) {
                break;
            }
        }
        if (list.head && list.tail) {
            IRO_Paste(list.head, list.tail, pre->last);
        }
    }

    IRO_InitList(&list);
    cmp = WhileLessTest(loop->induction, limit, &list);
    WhileBranch(loop, bodyLbl, cmp, &list);
    IRO_Paste(list.head, list.tail, pre->last);
    bodyBranch = list.tail;

    IRO_InitList(&list);
    testLbl = IRO_NewLabel();
    gotoNd = IRO_NewLinear(IROLinearOp1Arg);
    gotoNd->index = ++IRO_NumLinear;
    gotoNd->type = IROLinearGoto;
    gotoNd->u.label.label = testLbl;
    IRO_AddToList(gotoNd, &list);
    IRO_Paste(list.head, list.tail, pre->last);

    // eight words per iteration
    IRO_InitList(&list);
    IRO_AddToList(func_00554490(), &list);
    nd = list.tail;
    body2Lbl = nd->u.label.label;
    IRO_Paste(list.head, nd, pre->last);
    body2Label = list.head;
    func_0055FB20(loop, pat, factor);
    CInt64_SetLong(&eight, 8);
    indUpdate = func_0055CCF0(loop, CInt64_Mul(factor, eight), pre->last);

    IRO_InitList(&list);
    testLabel = IRO_NewLinear(IROLinearLabel);
    testLabel->index = IRO_NumLinear++;
    testLabel->u.label.label = testLbl;
    testLabel->flags |= IROLF_StartsNode;
    IRO_AddToList(testLabel, &list);
    IRO_Paste(list.head, list.tail, pre->last);

    IRO_InitList(&list);
    cmp = WhileLessTest(loop->induction, limit2, &list);
    WhileBranch(loop, body2Lbl, cmp, &list);
    IRO_Paste(list.head, list.tail, pre->last);
    test2 = list.tail;
    oldPreLast = pre->last;

    // the original loop now runs while i < limit3, then n = 0
    IRO_InitList(&list);
    cmp = WhileLessTest(loop->induction, limit3, &list);
    oldCond = loop->cond;
    IRO_Paste(list.head, list.tail, loop->x10->last);
    loop->x10->last->u.label.cond = list.tail;
    loop->cond = list.tail;

    IRO_InitList(&list);
    zero = IRO_NewIntConst(cint64_zero, oldCond->u.monadic->rtype);
    IRO_AddToList(zero, &list);
    zero->flags |= IROLF_Reffed;
    IRO_DuplicateExpr(oldCond->u.monadic, &list);
    ass = IRO_NewLinear(IROLinearOp2Arg);
    ass->nodetype = EASS;
    ass->rtype = list.tail->rtype;
    ass->index = ++IRO_NumLinear;
    ass->next = NULL;
    ass->u.diadic.left = list.tail;
    ass->u.diadic.right = zero;
    IRO_AddToList(ass, &list);
    ass->flags |= IROLF_Assigned;
    IRO_NopOut(oldCond);

    // the new nodes
    oldNext = pre->nextnode;
    pre->last = exitTest;
    n1 = IRO_NewFlowGraphNode();
    pre->nextnode = n1;
    n1->first = countHead;
    n1->last = guardTail;
    n2 = IRO_NewFlowGraphNode();
    n2->first = bodyLabel;
    n2->last = bodyBranch;
    bodyLabel->u.label.label->stmt = (Statement*)n2;
    n1->nextnode = n2;
    n3 = IRO_NewFlowGraphNode();
    n3->first = gotoNd;
    n3->last = gotoNd;
    n2->nextnode = n3;
    n4 = IRO_NewFlowGraphNode();
    n4->first = body2Label;
    n4->last = indUpdate;
    body2Label->u.label.label->stmt = (Statement*)n4;
    n3->nextnode = n4;
    n5 = IRO_NewFlowGraphNode();
    n5->first = testLabel;
    n5->last = test2;
    testLabel->u.label.label->stmt = (Statement*)n5;
    n4->nextnode = n5;
    n6 = IRO_NewFlowGraphNode();
    n6->first = oldPreLast;
    n6->last = oldPreLast;
    n5->nextnode = n6;
    n6->nextnode = oldNext;

    n7 = IRO_NewFlowGraphNode();
    n7->first = list.head;
    n7->last = list.tail;
    list.tail->next = loop->x10->last->next;
    loop->x10->last->next = list.head;
    n7->nextnode = loop->x10->nextnode;
    loop->x10->nextnode = n7;

    n8 = IRO_NewFlowGraphNode();
    lab = IRO_NewLinear(IROLinearLabel);
    lab->index = IRO_NumLinear++;
    lab->next = NULL;
    lab->u.label.label = exitLbl;
    lab->flags |= IROLF_StartsNode;
    exitLbl->stmt = (Statement*)n8;
    n8->first = lab;
    n8->last = lab;
    lab->next = n7->last->next;
    n7->last->next = lab;
    n8->nextnode = n7->nextnode;
    n7->nextnode = n8;
}

#undef CError_B145_SHIFT
#define CError_B145_SHIFT -1 // b145 asserts
// 0x55FB20: eight copies of the body's op= statement, one per word:
// *(base + k * (1 << shift) + index) op= mask (an and of an all-zero mask,
// an or of a full word: plain assignments)
void func_0055FB20(IRONaturalLoop* loop, UnrollWhilePattern* pat, CInt64 factor) {
    IRONode* node;
    IROLinear* nd;
    IROLinear* last;
    IROLinear* idx;
    IROLinear* off;
    IROLinear* add1;
    IROLinear* add2;
    IROLinear* ref;
    IROLinear* ass;
    IROLinear* value;
    IROLinear* rhs;
    CInt64 unused;
    CInt64 stride;
    CInt64 kv;
    CInt64 low;
    SInt32 size;
    UInt32 lo;
    int k;
    IROList list;

    CInt64_SetLong(&unused, 0); // (unused in 3.0.1 too)
    (void)unused;
    (void)factor;
    for (k = 0; k < 8; k++) {
        for (node = loop->x20; node && node != loop->x10; node = node->nextnode) {
            IRO_InitList(&list);
            last = node->last;
            for (nd = node->first;; nd = nd->next) {
                if (nd->stmt) {
                    nd->stmt->flags |= StmtFlag_Unrolled;
                }
                if (!func_00588740(loop, nd) &&
                    nd->type != IROLinearLabel &&
                    nd->type != IROLinearNop &&
                    !(nd->flags & IROLF_Reffed)) {
                    if (nd->nodetype != EORASS &&
                        nd->nodetype != EANDASS &&
                        nd->nodetype != EXORASS) {
                        CError_Internal(UNROLL_FILE, CError_B145_LINE(0x4C4));
                    }
                    IRO_DuplicateExpr(pat->index, &list);
                    idx = list.tail;
                    stride = CInt64_Shl(cint64_one, pat->indexShift);
                    CInt64_SetULong(&kv, k);
                    off = IRO_NewIntConst(CInt64_Mul(kv, stride), pat->index->rtype);
                    IRO_AddToList(off, &list);
                    IRO_DuplicateExpr(pat->base, &list);
                    add1 = IRO_NewLinear(IROLinearOp2Arg);
                    add1->index = ++IRO_NumLinear;
                    add1->nodetype = EADD;
                    add1->rtype = IRO_UnqualifiedType(pat->baseType);
                    add1->u.diadic.left = list.tail;
                    add1->u.diadic.right = off;
                    IRO_AddToList(add1, &list);
                    add2 = IRO_NewLinear(IROLinearOp2Arg);
                    add2->index = ++IRO_NumLinear;
                    add2->nodetype = EADD;
                    add2->rtype = IRO_UnqualifiedType(pat->baseType);
                    add2->u.diadic.left = add1;
                    add2->u.diadic.right = idx;
                    IRO_AddToList(add2, &list);
                    ref = IRO_NewLinear(IROLinearOp1Arg);
                    ref->index = ++IRO_NumLinear;
                    ref->nodetype = EINDIRECT;
                    ref->rtype = IRO_UnqualifiedType(nd->rtype);
                    ref->u.monadic = add2;
                    IRO_AddToList(ref, &list);

                    ass = IRO_NewLinear(IROLinearOp2Arg);
                    *ass = *nd;
                    ass->index = ++IRO_NumLinear;
                    ass->u.diadic.left = list.tail;
                    ass->next = NULL;
                    value = IRO_NewIntConst(pat->mask, pat->bit->rtype);
                    if (nd->type == IROLinearOp2Arg &&
                        nd->nodetype == EANDASS &&
                        CInt64_Equal(pat->mask, cint64_zero)) {
                        ass->nodetype = EASS;
                    } else if (nd->type == IROLinearOp2Arg && nd->nodetype == EORASS) {
                        CInt64_SetULong(&low, pat->mask.lo);
                        if (CInt64_Equal(pat->mask, low)) {
                            size = nd->rtype->size;
                            lo = pat->mask.lo;
                            if ((size == 1 && lo == 0xFF) ||
                                (size == 2 && lo == 0xFFFF) ||
                                (size == 4 && lo == 0xFFFFFFFF)) {
                                ass->nodetype = EASS;
                            }
                        }
                    }
                    IRO_AddToList(value, &list);
                    if (pat->value->type == IROLinearOp1Arg && pat->value->nodetype == ETYPCON) {
                        rhs = IRO_NewLinear(IROLinearOp1Arg);
                        *rhs = *pat->value;
                        rhs->index = ++IRO_NumLinear;
                        rhs->u.monadic = value;
                        rhs->next = NULL;
                        IRO_AddToList(rhs, &list);
                    } else {
                        rhs = value;
                    }
                    ass->u.diadic.right = rhs;
                    IRO_AddToList(ass, &list);
                }
                if (nd == last) {
                    break;
                }
            }
            if (list.head && list.tail) {
                IRO_Paste(list.head, list.tail, loop->preheader->last);
            }
        }
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// 0x55FF80: a temporary holding count * step + i (where the original loop
// stops)
IROLinear* func_0055FF80(IROLinear* count, CInt64 factor, IROList* list, IRONaturalLoop* loop) {
    IROLoopInd301* ind = loop->induction;
    Type* type;
    IROLinear* step;
    IROLinear* mul;
    IROLinear* add;

    type = IRO_UnqualifiedType(count->rtype);
    step = IRO_NewIntConst(ind->step, type);
    IRO_AddToList(step, list);
    step->flags |= IROLF_Reffed;
    mul = IRO_NewLinear(IROLinearOp2Arg);
    mul->index = ++IRO_NumLinear;
    mul->nodetype = EMUL;
    mul->u.diadic.left = IRO_DuplicateExpr(count, list);
    mul->u.diadic.right = step;
    mul->rtype = type;
    IRO_AddToList(mul, list);
    mul->flags |= IROLF_Reffed;
    mul->u.diadic.left->flags &= ~IROLF_Assigned;
    mul->u.diadic.left->u.monadic->flags &= ~IROLF_Assigned;
    DupIndRef(loop->induction, list);
    add = IRO_NewLinear(IROLinearOp2Arg);
    add->index = ++IRO_NumLinear;
    add->nodetype = EADD;
    add->u.diadic.left = mul;
    add->u.diadic.right = list->tail;
    add->rtype = type;
    IRO_AddToList(add, list);
    add->flags |= IROLF_Reffed;
    return AssignToNewTemp(add, type, list, FALSE, TRUE);
}

// 0x560100: a temporary holding count * step - step * factor + i (where the
// eight-word loop stops)
IROLinear* func_00560100(IROLinear* count, CInt64 factor, IROList* list, IRONaturalLoop* loop) {
    IROLoopInd301* ind = loop->induction;
    Type* type;
    IROLinear* whole;
    IROLinear* step;
    IROLinear* mul;
    IROLinear* sub;
    IROLinear* add;

    type = IRO_UnqualifiedType(count->rtype);
    whole = IRO_NewIntConst(CInt64_Mul(ind->step, factor), type);
    IRO_AddToList(whole, list);
    whole->flags |= IROLF_Reffed;
    step = IRO_NewIntConst(((IROLoopInd301*)loop->induction)->step, type);
    IRO_AddToList(step, list);
    step->flags |= IROLF_Reffed;
    mul = IRO_NewLinear(IROLinearOp2Arg);
    mul->index = ++IRO_NumLinear;
    mul->nodetype = EMUL;
    mul->u.diadic.left = IRO_DuplicateExpr(count, list);
    mul->u.diadic.right = step;
    mul->rtype = type;
    IRO_AddToList(mul, list);
    mul->flags |= IROLF_Reffed;
    mul->u.diadic.left->flags &= ~IROLF_Assigned;
    mul->u.diadic.left->u.monadic->flags &= ~IROLF_Assigned;
    sub = IRO_NewLinear(IROLinearOp2Arg);
    sub->index = ++IRO_NumLinear;
    sub->nodetype = ESUB;
    sub->u.diadic.left = mul;
    sub->u.diadic.right = whole;
    sub->rtype = type;
    IRO_AddToList(sub, list);
    sub->flags |= IROLF_Reffed;
    DupIndRef(loop->induction, list);
    add = IRO_NewLinear(IROLinearOp2Arg);
    add->index = ++IRO_NumLinear;
    add->nodetype = EADD;
    add->u.diadic.left = sub;
    add->u.diadic.right = list->tail;
    add->rtype = type;
    IRO_AddToList(add, list);
    add->flags |= IROLF_Reffed;
    return AssignToNewTemp(add, type, list, FALSE, TRUE);
}

// 0x5602F0: a temporary holding (i / factor + 1) * factor (the next word
// boundary)
IROLinear* func_005602F0(IROLoopInd301* ind, CInt64 factor, IROList* list) {
    Type* type;
    IROLinear* fac;
    IROLinear* div;
    IROLinear* one;
    IROLinear* add;
    IROLinear* mul;

    type = IRO_UnqualifiedType(ind->nd->rtype);
    fac = IRO_NewIntConst(factor, type);
    IRO_AddToList(fac, list);
    DupIndRef(ind, list);
    div = IRO_NewLinear(IROLinearOp2Arg);
    div->index = ++IRO_NumLinear;
    div->nodetype = EDIV;
    div->u.diadic.left = list->tail;
    div->u.diadic.right = fac;
    div->rtype = type;
    IRO_AddToList(div, list);
    div->flags |= IROLF_Reffed;
    one = IRO_NewIntConst(cint64_one, type);
    IRO_AddToList(one, list);
    one->flags |= IROLF_Reffed;
    add = IRO_NewLinear(IROLinearOp2Arg);
    add->index = ++IRO_NumLinear;
    add->nodetype = EADD;
    add->u.diadic.left = div;
    add->u.diadic.right = one;
    add->rtype = type;
    IRO_AddToList(add, list);
    add->flags |= IROLF_Reffed;
    IRO_DuplicateExpr(fac, list);
    mul = IRO_NewLinear(IROLinearOp2Arg);
    mul->index = ++IRO_NumLinear;
    mul->nodetype = EMUL;
    mul->u.diadic.left = add;
    mul->u.diadic.right = list->tail;
    mul->rtype = type;
    IRO_AddToList(mul, list);
    mul->flags |= IROLF_Reffed;
    return AssignToNewTemp(mul, type, list, FALSE, TRUE);
}

// 0x5604B0: a temporary holding n + 1 (the count the while (n--) test
// leaves)
IROLinear* func_005604B0(IROList* list, IRONaturalLoop* loop) {
    Type* type;
    IROLinear* one;
    IROLinear* add;

    type = IRO_UnqualifiedType(loop->cond->u.monadic->rtype);
    one = IRO_NewIntConst(cint64_one, type);
    IRO_AddToList(one, list);
    one->flags |= IROLF_Reffed;
    add = IRO_NewLinear(IROLinearOp2Arg);
    add->index = ++IRO_NumLinear;
    add->nodetype = EADD;
    add->rtype = type;
    add->u.diadic.left = IRO_DuplicateExpr(loop->cond->u.monadic, list);
    add->u.diadic.left->flags |= IROLF_Reffed;
    add->u.diadic.left->flags &= ~IROLF_Assigned;
    add->u.diadic.left->u.monadic->flags &= ~IROLF_Assigned;
    add->u.diadic.right = one;
    IRO_AddToList(add, list);
    return AssignToNewTemp(add, type, list, FALSE, FALSE);
}

// 0x560840: IsWhileLoopUnrollable
Boolean func_00560840(IRONaturalLoop* loop, CInt64* factor, UnrollWhilePattern* pat) {
    IRONode* pre;
    IRONode* node;
    IROLinear* last;
    IROLinear* upd;
    IROLinear* right;
    IROLoopInd301* ind;
    Type* type;
    UInt32 flags;
#if MWCCPS2 // b210 leaves them to func_00560C50
    int numMatches;
    int numAssigns;
#else
    int numMatches = 0;
    int numAssigns = 0;
#endif

    if (loop->x10 != loop->header) {
        IRO_Dump("IsWhileLoopUnrollable:No because the loop form is currently unsupported\n");
        return FALSE;
    }
    pre = loop->preheader;
    last = pre->last;
    if (!last || last->type != IROLinearGoto) {
        IRO_Dump("IsWhileLoopUnrollable:No because the preheader does not end with a goto\n");
        return FALSE;
    }
    if (pre->nextnode != loop->x20) {
        IRO_Dump(
            "IsWhileLoopUnrollable:No because the preheader does not precede the InLoopHeaderTarget\n"
        );
        return FALSE;
    }
    if (loop->x20 == loop->x10) {
        IRO_Dump("IsWhileLoopUnrollable:No because the InLoopHeaderTarget is the loop header\n");
        return FALSE;
    }
    if (!is_unsigned(loop->cond->u.monadic->rtype)) {
        IRO_Dump("IsWhileLoopUnrollable:No because the while loop induction is signed\n");
        return FALSE;
    }
    type = loop->cond->u.monadic->rtype;
    if (!(type->type == TYPEINT && TYPE_INTEGRAL(type)->integral < 17)) {
        IRO_Dump("IsWhileLoopUnrollable:No because the while loop induction is not integral\n");
        return FALSE;
    }
    flags = loop->flags;
    if (!(flags & LP301_STEP_IS_NEG_ONE)) {
        IRO_Dump(
            "IsWhileLoopUnrollable:No because the while loop operator is not of decrement form\n"
        );
        return FALSE;
    }
    if (flags & LP301_IF_EXPR_NONCANONICAL) {
        IRO_Dump("IsWhileLoopUnrollable: No because LP_IF_EXPR_NONCANONICAL \n");
        return FALSE;
    }
    if (flags & LP301_IND_USED_IN_LOOP) {
        IRO_Dump("IsWhileLoopUnrollable: No because LP_IND_USED_IN_LOOP \n");
        return FALSE;
    }
    if (flags & LP301_HAS_MULTIPLE_EXITS) {
        IRO_Dump("IsWhileLoopUnrollable: No because LP_HAS_MULTIPLE_EXITS \n");
        return FALSE;
    }
    if (!(flags & LP301_HAS_MULTIPLE_INDUCTIONS)) {
        IRO_Dump("IsWhileLoopUnrollable: No because LP_HAS_MULTIPLE_INDUCTIONS is not set \n");
        return FALSE;
    }

    // the induction that is used with a division and a modulo
    for (ind = loop->inductions; ind; ind = ind->next) {
        if ((ind->flags & 1) && (ind->flags & 2)) {
            break;
        }
    }
    if (!ind) {
        IRO_Dump(
            "IsWhileLoopUnrollable: No because could not find induction with MOD and DIV operation\n"
        );
        return FALSE;
    }
    if (!is_unsigned(ind->nd->rtype)) {
        IRO_Dump("IsWhileLoopUnrollable: No because induction is signed\n");
        return FALSE;
    }
    upd = ind->nd;
    if (upd->type == IROLinearOp2Arg) {
        if (!(upd->nodetype == EADDASS && func_00555C60(upd->u.diadic.right))) {
            if (ind->nd->nodetype != EASS) {
                IRO_Dump(
                    "IsWhileLoopUnrollable: No because induction assignment doesn't match pattern\n"
                );
                return FALSE;
            }
            right = ind->nd->u.diadic.right;
            if (!(right->type == IROLinearOp2Arg &&
                  right->nodetype == EADD &&
                  func_00555C60(right->u.diadic.right))) {
                IRO_Dump(
                    "IsWhileLoopUnrollable: No because induction EASS doesn't match pattern\n"
                );
                return FALSE;
            }
        }
        if (!CInt64_IsOne(&ind->step)) {
            IRO_Dump("IsWhileLoopUnrollable: No because induction step is not one\n");
            return FALSE;
        }
    } else if (upd->type == IROLinearOp1Arg &&
               upd->nodetype != EPREINC &&
               upd->nodetype != EPOSTINC) {
        IRO_Dump("IsWhileLoopUnrollable: No because induction assignment operator is bad\n");
        return FALSE;
    }

    func_005887E0(loop);
    loop->induction = ind;
    func_00588820(loop);
    memset(pat, 0, sizeof(UnrollWhilePattern));
    for (node = IRO_FirstNode; node; node = node->nextnode) {
#if MWCCPS2 // b210 shifts the bit number unsigned
        if (Bv_IsBitSet((UInt32)node->index, loop->nodes) && node != loop->header) {
#else
        if (Bv_IsBitSet(node->index, loop->nodes) && node != loop->header) {
#endif
            if (!func_00560C50(node, loop, ind, factor, &numMatches, &numAssigns, pat)) {
                IRO_Dump(
                    "IsWhileLoopUnrollable: No because PatternMatchWhileLoop returned false\n"
                );
                return FALSE;
            }
        }
    }
    if (numMatches > 1 || numAssigns > 1) {
        IRO_Dump("IsWhileLoopUnrollable: No because NumberOfMatches or NumberOfAssigns is bad\n");
        return FALSE;
    }
    return TRUE;
}

// 0x560C50: PatternMatchWhileLoop over one node of the loop: its only
// statement is b[(i >> k) << s] op= c << (i & (2^k - 1)) (op: &= ~, |=, ^=,
// possibly through a conversion); records the parts, the factor (2^k) and
// the mask a whole word of iterations touches
Boolean func_00560C50(
    IRONode* node,
    IRONaturalLoop* loop,
    IROLoopInd301* ind,
    CInt64* factor,
    int* numMatches,
    int* numAssigns,
    UnrollWhilePattern* pat
) {
    IROLinear* nd;
    IROLinear* ref;
    IROLinear* add;
    IROLinear* idx;
    IROLinear* value;
    IROLinear* amount;
    IROLinear* shift;
    IROLinear* maskNd;
    Object* obj;
    CInt64 bits;
    CInt64 wordBits;
    CInt64 mask;
    CInt64 low;

    *numMatches = 0;
    *numAssigns = 0;
    for (nd = node->first; nd; nd = nd->next) {
        if (!func_00588740(loop, nd) &&
            !(nd->flags & IROLF_Reffed) &&
            nd->type != IROLinearNop &&
            nd->type != IROLinearLabel) {
            if (nd->type != IROLinearOp2Arg) {
                return FALSE;
            }
            if ((UInt8)(nd->nodetype - EANDASS) > 2) {
                return FALSE;
            }
            (*numAssigns)++;
            ref = nd->u.diadic.left;
            if (!(ref->type == IROLinearOp1Arg && ref->nodetype == EINDIRECT)) {
                return FALSE;
            }
            add = ref->u.monadic;
            if (!(add->type == IROLinearOp2Arg && add->nodetype == EADD)) {
                return FALSE;
            }
            pat->base = add->u.diadic.left;
            pat->baseType = add->rtype;
            if (!IRO_IsVariable(add->u.diadic.left)) {
                return FALSE;
            }
            pat->index = add->u.diadic.right;
            if (pat->index->type == IROLinearOp2Arg &&
                pat->index->nodetype == ESHL &&
                func_00555C60(pat->index->u.diadic.right)) {
                pat->indexShift = pat->index->u.diadic.right->u.node->data.intval;
                idx = pat->index->u.diadic.left;
            } else {
                pat->indexShift = cint64_zero;
                idx = pat->index;
            }

            pat->value = nd->u.diadic.right;
            value = pat->value;
            if (value->type == IROLinearOp1Arg && value->nodetype == ETYPCON) {
                if (nd->type == IROLinearOp2Arg && nd->nodetype == EANDASS) {
                    if (!(value->u.monadic->type == IROLinearOp1Arg &&
                          value->u.monadic->nodetype == EBINNOT)) {
                        return FALSE;
                    }
                    pat->bit = value->u.monadic->u.monadic;
                } else {
                    pat->bit = value->u.monadic;
                }
                if (!(pat->bit->type == IROLinearOp2Arg &&
                      pat->bit->nodetype == ESHL &&
                      func_00555C60(pat->bit->u.diadic.left))) {
                    return FALSE;
                }
            } else if (value->type == IROLinearOp2Arg &&
                       value->nodetype == ESHL &&
                       nd->type == IROLinearOp2Arg &&
                       (nd->nodetype == EXORASS || nd->nodetype == EORASS)) {
                pat->bit = value;
                if (!func_00555C60(pat->bit->u.diadic.left)) {
                    return FALSE;
                }
            } else if (value->type == IROLinearOp1Arg &&
                       value->nodetype == EBINNOT &&
                       nd->type == IROLinearOp2Arg &&
                       nd->nodetype == EANDASS) {
                pat->bit = value->u.monadic;
                if (!(pat->bit->type == IROLinearOp2Arg &&
                      pat->bit->nodetype == ESHL &&
                      func_00555C60(pat->bit->u.diadic.left))) {
                    return FALSE;
                }
            } else {
                return FALSE;
            }
            bits = pat->bit->u.diadic.left->u.node->data.intval;
            amount = pat->bit->u.diadic.right;

            if (!(amount->type == IROLinearOp2Arg &&
                  amount->nodetype == EAND &&
                  idx->type == IROLinearOp2Arg &&
                  idx->nodetype == ESHR)) {
                return FALSE;
            }
            obj = IRO_IsVariable(idx->u.diadic.left);
            if (obj != IRO_IsVariable(amount->u.diadic.left)) {
                return FALSE;
            }
            if (obj != ind->var->object) {
                return FALSE;
            }
            shift = idx->u.diadic.right;
            maskNd = amount->u.diadic.right;
            if (!func_00555C60(shift) || !func_00555C60(maskNd)) {
                return FALSE;
            }
            wordBits = CInt64_Shl(cint64_one, shift->u.node->data.intval);
            mask = CInt64_Sub(wordBits, cint64_one);
            if (!CInt64_Equal(mask, maskNd->u.node->data.intval)) {
                return FALSE;
            }
            CInt64_SetULong(&low, mask.lo);
            if (!CInt64_Equal(mask, low)) {
                return FALSE;
            }
            *factor = CInt64_Add(mask, cint64_one);
            if (func_00561160(CInt64_Add(mask, cint64_one), bits, &pat->mask)) {
                (*numMatches)++;
                if (nd->type == IROLinearOp2Arg && nd->nodetype == EANDASS) {
                    pat->mask = CInt64_Not(pat->mask);
                }
            }
        }
        if (nd == node->last) {
            break;
        }
    }
    return TRUE;
}

// 0x561160: do bits << 0 .. bits << (count - 1) not overlap; *mask = their
// union
Boolean func_00561160(CInt64 count, CInt64 bits, CInt64* mask) {
    CInt64 acc = cint64_zero;
    CInt64 i = cint64_zero;
    CInt64 shifted;

    while (CInt64_Less(i, count)) {
        shifted = CInt64_Shl(bits, i);
        if (CInt64_NotEqual(CInt64_And(shifted, acc), cint64_zero)) {
            return FALSE;
        }
        acc = CInt64_Or(shifted, acc);
        i = CInt64_Add(i, cint64_one);
    }
    *mask = acc;
    return TRUE;
}
