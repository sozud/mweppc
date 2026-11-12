// mwccpsp 3.0.1: removing loops that do nothing (IRO_RemoveAllEmptyLoops
// and its helpers, 0x58D780..0x58DF80), on 3.0.1's loop records. 2.4.7's
// IroEmptyLoop.c does this on its own loop analysis.
//
//     for (i = 0; i < n; i++) {}      becomes   i = n (when i is live after)
//     while (k) { ... } with a count of 0 or 1 loses its branch back
#include "compiler/common.h"
#include "compiler/BitVector.h"
#include "compiler/CError.h"
#include "compiler/CParser.h"
#include "compiler/enode.h"
#include "compiler/objects.h"
#include "compiler/IroDump.h"
#include "compiler/IroEval.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroLoop301.h"
#include "compiler/IroMalloc.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"

extern VarRecord* IRO_FindAssigned301(IROLinear* linear, Boolean* isBitfield);
extern void func_00594C60(void); // IroLifetime.c: the variables live at each linear
extern void IRO_DumpIntTree(IROLinear* linear);

extern Boolean data_00625FD5; // IroLoop.c: the lifetimes (VarRecord +8) are computed

// the induction's value must be stored after the loop: it may be live there
static inline Boolean NeedsFinalStore(IRONaturalLoop* loop, IROLoopInd301* ind) {
    VarRecord* var;

    if (data_00625FD5 &&
        !(loop->flags & LP301_HAS_MULTIPLE_EXITS) &&
        (var = ind->var) &&
        !var->noregister &&
        var->object->datatype == DLOCAL &&
        !is_volatile_object(var->object) &&
        !Bv_IsBitSet(loop->x10->nextnode->first->index, (BitVector*)var->unused)) {
        return FALSE;
    }
    return TRUE;
}

extern IROLinear* func_00588A10(
    IRONaturalLoop* loop, IROLoopInd301* ind
); // IroLoop.c: the start value
extern IROLinear* func_00585DC0(
    IROLinear* a, ENodeType op, IROLinear* b, IROList* list
); // IroLoop.c
extern void func_00587120(IROList* list); // IroLoop.c: fold a detached list
extern Type* func_0055A650(Type* type); // IroAggregateAssignment.c
extern Boolean func_0055A3F0(IROLinear* conv); // IroTypeConv.c

// 0x584020: the induction's value after the loop, from the loop's trip
// count: a constant (ind->finalValue) or an expression (ind->finalList,
// init + count * step), with the conditions it needs in ind->finalConds when
// withConds. Without a count, the primary induction of a `i != n` or
// `++/--` test with an unsigned step of one ends at the bound. TRUE when
// known (ind->x18)
Boolean func_00584020(IRONaturalLoop* loop, IROLoopInd301* ind, Boolean withConds) {
    IROList* conds;
    IROList* list;
    IROLinear* init;
    IROLinear* countNd;
    IROLinear* stepNd;
    IROLinear* neg;
    IROLinear* nd;
    IROLinear* cond;
    IROLinear* limit;
    IROLinear* bound;
    IROLoopInd301* other;
    VarRecord* var;
    Type* type;
    IROList tmp;
    CInt64 d;
    CInt64 initVal;
    Boolean uns;
    Boolean ok;
    ENodeType op;

    ind->x18 = FALSE;
    ind->finalValue = cint64_zero;
    IRO_InitList(&ind->finalConds);
    IRO_InitList(&ind->finalList);
    conds = withConds ? &ind->finalConds : NULL;
    list = &ind->finalList;

    if (!ind->fnode->mustreach1) {
        goto done;
    }

    init = func_00588A10(loop, ind);
    if (init && loop->hasCount) {
        type = IRO_UnqualifiedType(init->rtype);
        uns = is_unsigned(type);
        d = ind->step;
        if (ind->flags & 4) {
            d = CInt64_Neg(d);
        }
        if (!CInt64_Equal(loop->count, cint64_negone)) {
            if (uns ? CInt64_LessU(loop->count, cint64_zero)
                    : CInt64_Less(loop->count, cint64_zero)) {
                {
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -4 // b145 asserts
                    CError_Internal("IroLoop.c", CError_B145_LINE(0x1256));
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
                }
            }
            if (IRO_IsIntConstant(init) && !CInt64_IsZero(&d)) {
                initVal = init->u.node->data.intval;
                ind->finalValue = uns ? CInt64_MulU(loop->count, d) : CInt64_Mul(loop->count, d);
                ind->finalValue = CInt64_Add(ind->finalValue, initVal);
                // while (n--) / while (n++): the test's update runs once more
                if ((loop->flags & LP301_WHILE_LOOP) &&
                    (var = IRO_FindVar(
                         loop->cond->u.monadic->u.monadic->u.node->data.objref, FALSE, TRUE
                     ))) {
                    for (other = loop->inductions; other; other = other->next) {
                        if (other->var == var) {
                            break;
                        }
                    }
                    if (other && other == ind) {
                        op = loop->cond->nodetype;
                        if (op == EPOSTINC) {
                            ind->finalValue = CInt64_Add(ind->finalValue, cint64_one);
                        } else if (op == EPOSTDEC) {
                            ind->finalValue = CInt64_Sub(ind->finalValue, cint64_one);
                        }
                    }
                }
                IRO_TruncateValueToType(&ind->finalValue, type);
                ind->x18 = TRUE;
            }
        }

        if (!ind->x18) {
            if (type->type == TYPEPOINTER) {
                type = func_0055A650(type);
            }
            if (CInt64_Equal(loop->count, cint64_negone)) {
                countNd = loop->countExpr.tail;
            } else {
                countNd = IRO_NewIntConst(loop->count, type);
            }
            IRO_InitList(&tmp);
            if (ind->stepExpr) {
                IRO_DuplicateExpr(ind->stepExpr, &tmp);
            } else {
                IRO_AddToList(IRO_NewIntConst(ind->step, ind->nd->rtype), &tmp);
            }
            stepNd = tmp.tail;
            if (ind->flags & 4) {
                neg = IRO_NewLinear(IROLinearOp1Arg);
                *neg = *stepNd;
                neg->type = IROLinearOp1Arg;
                neg->nodetype = EMONMIN;
                neg->index = IRO_NumLinear++;
                neg->u.monadic = stepNd;
                IRO_AddToList(neg, &tmp);
            }
            nd = func_00585DC0(countNd, EMUL, tmp.tail, conds);
            nd = func_00585DC0(init, EADD, nd, conds);
            IRO_DuplicateExpr(nd, list);
            ind->x18 = TRUE;
        }
        if (ind->x18 && conds && loop->countConds.tail) {
            IRO_DuplicateExpr(loop->countConds.tail, conds);
        }
    } else if (ind == loop->induction) {
        cond = loop->cond;
        ok = FALSE;
        if (cond &&
            (cond->type == IROLinearOp1Arg || cond->type == IROLinearOp2Arg) &&
            (cond->nodetype <= EINDIRECT ||
             (cond->nodetype >= ELESS && cond->nodetype <= ENOTEQU))) {
            ok = TRUE;
        }
        if (ok &&
            (cond->nodetype <= EINDIRECT || cond->nodetype == ENOTEQU) &&
            (limit = func_00588900(loop)) &&
            !IRO_HasSideEffect(limit) &&
            (IRO_IsIntConstant(limit) || (limit->flags & IROLF_LoopInvariant))) {
            cond = loop->cond;
            bound = NULL;
            if (cond->type == IROLinearOp2Arg) {
                bound = (loop->flags & LP301_INDUCTION_AT_LEFT) ? cond->u.diadic.left
                                                                : cond->u.diadic.right;
            } else if (cond->type == IROLinearOp1Arg) {
                bound = cond->u.monadic;
            } else {
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 2 // b145 asserts
                CError_Internal("IroLoop.c", CError_B145_LINE(0x870));
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
            }
            if (bound &&
                (IRO_IsVariable(bound) ||
                 (bound->type == IROLinearOp1Arg &&
                  bound->nodetype == ETYPCON &&
                  func_0055A3F0(bound) &&
                  IRO_IsVariable(bound->u.monadic))) &&
                !IRO_HasSideEffect(bound)) {
                uns = is_unsigned(limit->rtype);
                d = ind->step;
                if (ind->flags & 4) {
                    d = CInt64_Neg(d);
                }
                if (!CInt64_IsZero(&d) && uns && CInt64_IsOne(&ind->step)) {
                    if (IRO_IsIntConstant(limit)) {
                        ind->finalValue = limit->u.node->data.intval;
                    } else {
                        IRO_DuplicateExpr(limit, list);
                    }
                    ind->x18 = TRUE;
                }
            }
        }
    }

done:
    if (ind->x18) {
        func_00587120(list);
        func_00587120(conds);
    }
    return ind->x18;
}

// 0x58D870: everything the loop does is its test and its induction updates
Boolean func_0058D870(IRONaturalLoop* loop) {
    IRONode* node;
    IROLinear* nd;

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (Bv_IsBitSet(node->index, loop->nodes)) {
            for (nd = node->first; nd; nd = nd->next) {
                if (!func_00588740(loop, nd) &&
                    !func_00588690(loop, nd) &&
                    nd->type != IROLinearNop &&
                    nd->type != IROLinearLabel &&
                    !(nd->flags & IROLF_Reffed)) {
                    IRO_Dump("Found top level node in the loop\n");
                    return FALSE;
                }
                if (nd == node->last) {
                    break;
                }
            }
        }
    }
    return TRUE;
}

// 0x58D920: the loop computes nothing but its inductions' final values,
// which can be found and stored
Boolean func_0058D920(IRONaturalLoop* loop) {
    UInt32 flags = loop->flags;
    IROLinear* limit;
    IROLoopInd301* ind;
    IRONode* exitNode;
    IROLinear* at;

    if (flags &
        (LP301_HAS_CALLS | LP301_HAS_NONASSIGN | LP301_INDUCTION_NOT_FOUND |
         LP301_IND_USED_IN_LOOP | LP301_HAS_ASM | LP301_HAS_NONCANONICAL_STEP |
         LP301_FORM_UNRECOGNIZABLE)) {
        IRO_Dump("CanRemoveRedundantLoop:No due to prohibited loop flags that are set ");
        func_00583D30(loop->flags & 0x24401A);
        IRO_Dump("\n");
        return FALSE;
    }
    if (!(flags & LP301_HEADER_FOLLOWS_UPDATE)) {
        IRO_Dump("CanRemoveRedundantLoop:No due to required loop flags that are not set ");
        func_00583D30((loop->flags & LP301_HEADER_FOLLOWS_UPDATE) ^ LP301_HEADER_FOLLOWS_UPDATE);
        IRO_Dump("\n");
        return FALSE;
    }
    if (!((IROLoopInd301*)loop->induction)->fnode->mustreach1) {
        IRO_Dump(
            "CanRemoveRedundantLoop:No because update of primary induction is not in a mustreach1 block\n"
        );
        return FALSE;
    }
    if (!func_0058D870(loop)) {
        IRO_Dump("CanRemoveRedundantLoop:No because loop is not redundant\n");
        return FALSE;
    }
    limit = func_00588900(loop);
    if (!IRO_IsIntConstant(limit) && !(limit->flags & IROLF_LoopInvariant)) {
        IRO_Dump("CanRemoveRedundantLoop:No because Loop Upper Bound is Variant in the loop\n");
        return FALSE;
    }
    for (ind = loop->inductions; ind; ind = ind->next) {
        if (ind != loop->induction) {
            func_00588AE0(loop, ind);
        }
        if (func_00584020(loop, ind, FALSE)) {
            if (!(ind->fnode && ind->fnode->mustreach)) {
                func_00589F20(loop, &exitNode, &at);
                if (!exitNode || !at) {
                    IRO_Dump(
                        "CanRemoveRedundantLoop:No because there's no place to put the induction final store "
                        "assignment\n"
                    );
                    return FALSE;
                }
            }
        } else {
            if (loop->flags & LP301_IF_EXPR_NONCANONICAL) {
                IRO_Dump(
                    "CanRemoveRedundantLoop:No because LP_IF_EXPR_NONCANONICAL combined with an unknown final "
                    "value of induction means that the loop could be infinite\n"
                );
                return FALSE;
            }
            if (!data_00625FD5) {
                func_00594C60();
                data_00625FD5 = TRUE;
            }
            if (NeedsFinalStore(loop, ind)) {
                IRO_Dump(
                    "CanRemoveRedundantLoop:No because can't compute final value of induction, or it will "
                    "overflow\n"
                );
                return FALSE;
            }
        }
    }
    return TRUE;
}

// the loop test node's branch back goes
void RemoveLoopBranch301(IRONode* testNode) {
    testNode->last->u.label.cond->flags &= ~IROLF_Reffed;
    testNode->last->type = IROLinearNop;
    IRO_CheckSideEffect(testNode->last->u.label.cond);
}

// 0x58DB90: remove a loop that does nothing, storing its inductions' final
// values where they may be read
Boolean func_0058DB90(IRONaturalLoop* loop) {
    IRONode* testNode;
    IRONode* exitNode;
    IROLoopInd301* ind;
    IROLinear* value;
    IROLinear* ass;
    IROLinear* at;
    IROList list;
    Type* type;
    Boolean removable;

    func_00587540(loop);
    func_005887B0(loop);
    func_00588700(loop);
    testNode = loop->x10;
    removable = func_0058D920(loop);
    if (removable) {
        if (loop->flags & LP301_WHILE_LOOP) {
            IRO_Dump("RemovePossiblyEmptyLoop: removing self recursive do-while(--n ) loop\n");
        } else {
            IRO_Dump("RemovePossiblyEmptyLoop: removing standard loop\n");
        }
        RemoveLoopBranch301(testNode);
        for (ind = loop->inductions; ind; ind = ind->next) {
            if (NeedsFinalStore(loop, ind)) {
                if (ind->finalList.tail) {
                    IRO_Dump(
                        "RemovePossiblyEmptyLoop: induction %s has finalStoreVal\n",
                        IRO_FindAssigned301(ind->nd, NULL)->object->name->name
                    );
                    IRO_DumpIntTree(ind->finalList.tail);
                } else {
                    IRO_Dump(
                        "RemovePossiblyEmptyLoop: induction %s has finalStoreVal = %ld\n",
                        IRO_FindAssigned301(ind->nd, NULL)->object->name->name,
                        ind->finalValue.lo
                    );
                }
                type = IRO_UnqualifiedType(ind->nd->rtype);
                IRO_InitList(&list);
                if (ind->finalList.tail) {
                    value = IRO_DuplicateExpr(ind->finalList.tail, &list);
                } else {
                    value = IRO_NewIntConst(ind->finalValue, type);
                    IRO_AddToList(value, &list);
                }
                IRO_DuplicateExpr(ind->nd->u.diadic.left, &list);
                ass = IRO_NewLinear(IROLinearOp2Arg);
                *ass = *ind->nd;
                ass->type = IROLinearOp2Arg;
                ass->nodetype = EASS;
                ass->u.diadic.left = list.tail;
                ass->u.diadic.right = value;
                ass->index = ++IRO_NumLinear;
                ass->next = NULL;
                IRO_AddToList(ass, &list);
                if (ind->fnode && ind->fnode->mustreach) {
                    IRO_PasteAfter(list.head, list.tail, ind->nd);
                } else {
                    func_00589F20(loop, &exitNode, &at);
                    IRO_Paste(list.head, list.tail, at);
                }
                IRO_NopOut(ind->nd);
            } else {
                IRO_Dump(
                    "RemovePossiblyEmptyLoop: induction %s was not live at exit\n",
                    IRO_FindAssigned301(ind->nd, NULL)->object->name->name
                );
            }
        }
    }
    func_00588770(loop);
    func_005886C0(loop);
    return removable;
}

// 0x58D780: the loop runs at most once and its branch back can go
Boolean func_0058D780(IRONaturalLoop* loop) {
    Boolean zero;
    Boolean one;
    Boolean headerTest;
    Boolean tailTest;

    if (!loop->hasCount) {
        IRO_Dump("CanRemoveZeroOrOneIterationLoop:No because iteration count is unknown\n");
        return FALSE;
    }
    zero = CInt64_IsZero(&loop->count);
    one = CInt64_IsOne(&loop->count);
    if (!zero && !one) {
        IRO_Dump("CanRemoveZeroOrOneIterationLoop:No because iteration count is not zero or one\n");
        return FALSE;
    }
    func_00587270(loop);
    if (loop->flags & LP301_FORM_UNRECOGNIZABLE) {
        tailTest = FALSE;
        headerTest = FALSE;
    } else {
        headerTest = loop->x10 == loop->header && loop->x10 != loop->x20;
        tailTest = !headerTest;
    }
    if (!tailTest && !headerTest) {
        IRO_Dump("CanRemoveZeroOrOneIterationLoop:No because loop form is not recognizable\n");
        return FALSE;
    }
    if (tailTest && !one) {
        IRO_Dump(
            "CanRemoveZeroOrOneIterationLoop:No because do while loop's iteration count is not one\n"
        );
        return FALSE;
    }
    if (headerTest && !zero) {
        IRO_Dump(
            "CanRemoveZeroOrOneIterationLoop:No because standard loop's iteration count is not zero\n"
        );
        return FALSE;
    }
    return TRUE;
}
