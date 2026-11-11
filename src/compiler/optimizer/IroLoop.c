#if MWCCPSP
#include "compiler/IroMalloc.h"
#endif
#include "compiler/IroLoop.h"
#include "compiler/IroCSE.h"
#include "compiler/IroDump.h"
#if MWCCPSP
#include "compiler/IroEval.h"
#endif
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#if MWCCPSP
#include "compiler/IrOptimizer.h"
#endif
#include "compiler/IroPropagate.h"
#include "compiler/IroSubable.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"
#include "compiler/BitVector.h"
#if MWCCPSP
#include "compiler/CABI.h"
#endif
#include "compiler/CError.h"
#include "compiler/CFunc.h"
#include "compiler/CInt64.h"
#include "compiler/CMachine.h"
#include "compiler/CompilerTools.h"
#include "compiler/CParser.h"
#include "compiler/enode.h"
#include "compiler/objects.h"
#include "compiler/Switch.h"
#include "compiler/types.h"
#include "version.h"
#include <string.h>

typedef enum IROLoopMemRefFlags {
    LoopMemRef_Load = 1,
    LoopMemRef_Store = 2,
    LoopMemRef_Invalid = 8,
    LoopMemRef_Speculative = 0x10
} IROLoopMemRefFlags;

#pragma pack(push, 2)
typedef struct IROLoopMemRef {
    IROLoopMemRefFlags flags;
    IROLinear* nd;
    IROElmList* list;
    IROAddrRecord* rec;
    struct IROLoopMemRef* next;
} IROLoopMemRef;
#pragma pack(pop)

IRONode* LoopNode;
Boolean ConditionalHeaderAtBottom;
IROLoopInd* FirstInd;
BitVector* InLoop;
BitVector* InLoop_Exits;
BitVector* InLoop_Tails;
UInt32 LoopExitNumber;
UInt32 LoopTailNum;
IRONode* LoopExitSuccessor;
IRONode* LoopTail;
IROLoopMemRef* IRO_LoopMemRefFirst;
IROLoopMemRef* IRO_LoopMemRefCurrent;
static IROExpr* RisList;
static BitVector* AllKills;
static Boolean KnownBounds;
static SInt32 Times;
static IROLinear* PredInt;
static IROElmList* FirstAddendLinear;
static IROElmList* LastAddendLinear;
static int NoSubableSubs;
#if VERSION >= VERSION_GC_2_5
static Boolean SearchTargetFound;
static IROLinear* SearchTarget;
#endif

static void MyHandleLoop_Vector(IRONode* fnode);
static void MyHandleLoop_Motion(IRONode* fnode);
static void CheckAllLoopAddresses(IRONode* fnode, BitVector* bv, Boolean* resultFlag);
static void MakeLoopEntry(
    IROLinear* nd, IROAddrRecord* rec, Boolean mustreach1, Boolean cannotSpeculate
);
static void MoveInvarianceInAddressExpr(void);
static IROLinear* RearrangeInvarianceInAddressExpr(IROLinear* nd, IROList* list);
static UInt32 IsAssignmentReductionCandidate(IROLinear* nd);

void FindMustReach(void) {
    IRONode* fnode;
    IRONode* fnode2;
    IRONode* pred;
    int i;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        fnode->mustreach = FALSE;
        if (Bv_IsBitSet(fnode->index, InLoop)) {
            fnode->mustreach = TRUE;

            for (i = 0; i < LoopNode->numpred; i++) {
                pred = IRO_NodeTable[LoopNode->pred[i]];
                if (Bv_IsBitSet(pred->index, InLoop) && !Bv_IsBitSet(fnode->index, pred->dom)) {
                    fnode->mustreach = FALSE;
                    break;
                }
            }

            for (fnode2 = IRO_FirstNode; fnode2; fnode2 = fnode2->nextnode) {
                if (Bv_IsBitSet(fnode2->index, InLoop)) {
                    for (i = 0; i < fnode2->numsucc; i++) {
                        if (!Bv_IsBitSet(fnode2->succ[i], InLoop) &&
                            !Bv_IsBitSet(fnode->index, fnode2->dom)) {
                            fnode->mustreach = FALSE;
                            break;
                        }
                    }

                    if (!fnode->mustreach) {
                        break;
                    }
                }
            }
        }
    }
}

void FindMustReach1(IRONode* checkfnode) {
    IRONode* fnode;
    IRONode* fnode2;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (Bv_IsBitSet(fnode->index, InLoop)) {
            fnode->mustreach1 = TRUE;
            for (fnode2 = IRO_FirstNode; fnode2; fnode2 = fnode2->nextnode) {
                if (Bv_IsBitSet(fnode2->index, InLoop)) {
                    if (Bv_IsBitSet(fnode2->index, InLoop_Tails) &&
                        !Bv_IsBitSet(fnode->index, fnode2->dom)) {
                        fnode->mustreach1 = FALSE;
                    }

                    if (Bv_IsBitSet(fnode2->index, InLoop_Exits) &&
                        fnode2 != checkfnode &&
                        !Bv_IsBitSet(fnode->index, fnode2->dom)) {
                        fnode->mustreach1 = FALSE;
                    }

                    if (!fnode->mustreach1) {
                        break;
                    }
                }
            }
        }
    }
}

static void IRO_FindLoopTailsAndExits(IRONode* fnode) {
    IRONode* scan;
    IRONode* succ;
    int i;

    for (scan = IRO_FirstNode; scan; scan = scan->nextnode) {
        if (Bv_IsBitSet(scan->index, InLoop)) {
            for (i = 0; i < scan->numsucc; i++) {
                succ = IRO_NodeTable[scan->succ[i]];
                if (succ == fnode) {
                    Bv_SetBit(scan->index, InLoop_Tails);
                    LoopTail = scan;
                    LoopTailNum++;
                }
                if (!Bv_IsBitSet(succ->index, InLoop)) {
                    LoopExitNumber++;
                    LoopExitSuccessor = succ;
                    Bv_SetBit(scan->index, InLoop_Exits);
                }
            }
        }
    }

    IRO_Dump(
        "IRO_FindLoopTailsAndExits:For  header %d, Loop exits and loop tails are  \n", fnode->index
    );
    IRO_DumpBits("Loop Exits: ", InLoop_Exits);
    IRO_DumpBits("Loop Tails: ", InLoop_Tails);
    IRO_Dump("LoopExitNum=%d: \n", LoopExitNumber);
    if (LoopExitSuccessor) {
        IRO_Dump("LoopExitSuccessor node =%d: \n", LoopExitSuccessor->index);
    }
}

static int IsSafeTypcon(IROLinear* nd) {
    Type* srcType;
    Type* destType;
    SInt32 srcSize;
    SInt32 destSize;
    Boolean srcUnsigned;
    Boolean destUnsigned;

    srcType = nd->u.monadic->rtype;
    destType = nd->rtype;
    if (!IS_TYPE_INT(srcType) || !IS_TYPE_INT(destType)) {
        return FALSE;
    }

    srcSize = srcType->size;
    destSize = destType->size;
    srcUnsigned = is_unsigned(srcType);
    destUnsigned = is_unsigned(destType);

    if (srcUnsigned == destUnsigned && destSize >= srcSize) {
        return TRUE;
    }
    if (srcUnsigned == TRUE && destUnsigned == FALSE && destSize > srcSize) {
        return TRUE;
    }
    return FALSE;
}

#if VERSION >= VERSION_GC_2_5
static void IRO_ActFindSearchTarget(IROLinear* linear, Boolean isFirst) {
    if (isFirst && linear == SearchTarget) {
        SearchTargetFound = TRUE;
    }
}

static Boolean TreeContains(IROLinear* tree, IROLinear* target) {
    SearchTarget = target;
    SearchTargetFound = FALSE;
    IRO_WalkTree(tree, IRO_ActFindSearchTarget);
    return SearchTargetFound;
}
#endif

#if VERSION < VERSION_GC_2_5
#undef CError_LINE_SHIFT
#define CError_LINE_SHIFT -26
#endif
static int Reducable(
    IROLinear* nd, IROLinear** resultNd1, IROLinear** resultNd2, VarRecord** resultVar
) {
    IROLinear* indirect;
    IROLinear* left;
    IROLinear* right;
    Boolean leftInvariant;
    Boolean rightInvariant;
    Boolean leftTypcon;
    Boolean rightTypcon;
    Object* obj;
    ENode* enode;
    IROLoopInd* ind;
    CInt64 divisor;
    SInt32 val;
    SInt32 div;

    leftInvariant = FALSE;
    rightInvariant = FALSE;
    leftTypcon = FALSE;
    rightTypcon = FALSE;

    if (nd->type == IROLinearOp2Arg &&
        nd->nodetype == EADD &&
        (IS_TYPE_INT(nd->rtype) || IS_TYPE_POINTER_ONLY(nd->rtype))) {
        left = nd->u.diadic.left;
        right = nd->u.diadic.right;
        if (left->type == IROLinearOp1Arg && left->nodetype == ETYPCON) {
            leftTypcon = TRUE;
            leftInvariant = (left->flags & IROLF_LoopInvariant) != 0;
            left = left->u.monadic;
        }
        if (right->type == IROLinearOp1Arg && right->nodetype == ETYPCON) {
            rightTypcon = TRUE;
            rightInvariant = (right->flags & IROLF_LoopInvariant) != 0;
            right = right->u.monadic;
        }

        if (((left->flags & IROLF_LoopInvariant) || leftInvariant) && IRO_IsVariable(right)) {
            if (leftInvariant ||
                ((!(obj = IRO_IsVariable(left)) || !IRO_IsRegable(obj)) && !IRO_IsConstant(left))) {
                if (left->type == IROLinearOp2Arg &&
                    left->nodetype == EADD &&
                    (obj = IRO_IsVariable(left->u.diadic.left)) &&
                    IRO_IsRegable(obj) &&
                    IRO_IsConstant(left->u.diadic.right)) {
                    return FALSE;
                }

                if (rightTypcon) {
                    if (!IsSafeTypcon(nd->u.diadic.right)) {
                        return FALSE;
                    }
                    *resultNd2 = nd->u.diadic.right;
                } else {
                    *resultNd2 = right;
                }

                indirect = right;
                *resultNd1 = IRO_NewLinear(IROLinearOperand);

                enode = IRO_NewENode(EINTCONST);
                enode->data.intval = cint64_one;
                enode->rtype = nd->u.diadic.right->rtype;
                (*resultNd1)->rtype = nd->u.diadic.right->rtype;
                (*resultNd1)->u.node = enode;
            } else {
                return FALSE;
            }
        } else if (((left->flags & IROLF_LoopInvariant) || leftInvariant) &&
                   right->type == IROLinearOp2Arg &&
                   !rightTypcon &&
                   (right->nodetype == EMUL || right->nodetype == ESHL)) {
            if (IRO_IsConstant(right->u.diadic.right)) {
                if (right->nodetype == ESHL) {
                    right->nodetype = EMUL;
                    right->u.diadic.right->u.node->data.intval =
                        CInt64_Shl(cint64_one, right->u.diadic.right->u.node->data.intval);
                }
                if (right->u.diadic.left->type == IROLinearOp1Arg) {
                    if (IRO_IsVariable(right->u.diadic.left)) {
                        *resultNd2 = right->u.diadic.left;
                        indirect = right->u.diadic.left;
                        *resultNd1 = right->u.diadic.right;
                    } else if (right->u.diadic.left->nodetype == ETYPCON &&
                               IRO_IsVariable(right->u.diadic.left->u.monadic)) {
                        if (!IsSafeTypcon(right->u.diadic.left)) {
                            return FALSE;
                        }
                        *resultNd2 = right->u.diadic.left;
                        indirect = right->u.diadic.left->u.monadic;
                        *resultNd1 = right->u.diadic.right;
                    } else {
                        return FALSE;
                    }
                } else {
                    return FALSE;
                }
            } else {
                return FALSE;
            }
        } else if (((right->flags & IROLF_LoopInvariant) || rightInvariant) &&
                   left->type == IROLinearOp2Arg &&
                   !leftTypcon &&
                   (left->nodetype == EMUL || left->nodetype == ESHL)) {
            if (IRO_IsConstant(left->u.diadic.right)) {
                if (left->nodetype == ESHL) {
                    left->nodetype = EMUL;
                    left->u.diadic.right->u.node->data.intval =
                        CInt64_Shl(cint64_one, left->u.diadic.right->u.node->data.intval);
                }
                if (left->u.diadic.left->type == IROLinearOp1Arg) {
                    if (IRO_IsVariable(left->u.diadic.left)) {
                        *resultNd2 = left->u.diadic.left;
                        indirect = left->u.diadic.left;
                        *resultNd1 = left->u.diadic.right;
                    } else if (left->u.diadic.left->nodetype == ETYPCON &&
                               IRO_IsVariable(left->u.diadic.left->u.monadic)) {
                        if (!IsSafeTypcon(left->u.diadic.left)) {
                            return FALSE;
                        }
                        *resultNd2 = left->u.diadic.left;
                        indirect = left->u.diadic.left->u.monadic;
                        *resultNd1 = left->u.diadic.right;
                    } else {
                        return FALSE;
                    }
                } else {
                    return FALSE;
                }
            } else {
                return FALSE;
            }
        } else if (((right->flags & IROLF_LoopInvariant) || rightInvariant) &&
                   IRO_IsVariable(left)) {
            if (rightInvariant ||
                ((!(obj = IRO_IsVariable(right)) || !IRO_IsRegable(obj)) &&
                 !IRO_IsConstant(right))) {
                if (right->type == IROLinearOp2Arg &&
                    right->nodetype == EADD &&
                    (obj = IRO_IsVariable(right->u.diadic.left)) &&
                    IRO_IsRegable(obj) &&
                    IRO_IsConstant(right->u.diadic.right)) {
                    return FALSE;
                }

                if (leftTypcon) {
                    if (!IsSafeTypcon(nd->u.diadic.left)) {
                        return FALSE;
                    }
                    *resultNd2 = nd->u.diadic.left;
                } else {
                    *resultNd2 = left;
                }

                indirect = left;
                *resultNd1 = IRO_NewLinear(IROLinearOperand);

                enode = IRO_NewENode(EINTCONST);
                enode->data.intval = cint64_one;
                enode->rtype = nd->u.diadic.left->rtype;
                (*resultNd1)->rtype = nd->u.diadic.left->rtype;
                (*resultNd1)->u.node = enode;
            } else {
                return FALSE;
            }
        } else {
            return FALSE;
        }
    } else if (nd->type == IROLinearOp2Arg &&
               (nd->nodetype == EMUL || nd->nodetype == ESHL) &&
               nd->rtype->size <= 4 &&
               (IS_TYPE_INT(nd->rtype) || IS_TYPE_POINTER_ONLY(nd->rtype))) {
        left = nd->u.diadic.left;
        right = nd->u.diadic.right;

        if (IRO_IsConstant(nd->u.diadic.right) && IRO_IsVariable(left)) {
            *resultNd2 = left;
            indirect = left;
            *resultNd1 = right;
        } else if (IRO_IsConstant(nd->u.diadic.right) &&
                   left->type == IROLinearOp1Arg &&
                   left->nodetype == ETYPCON &&
                   IRO_IsVariable(left->u.monadic)) {
            if (!IsSafeTypcon(left)) {
                return FALSE;
            }
            *resultNd2 = left;
            indirect = left->u.monadic;
            *resultNd1 = right;
        } else {
            if (nd->type == IROLinearOp2Arg && nd->nodetype == ESHL) {
                return FALSE;
            }

            if (nd->u.diadic.right->flags & IROLF_LoopInvariant) {
                if (IRO_IsVariable(left)) {
                    *resultNd2 = left;
                    indirect = left;
                    *resultNd1 = right;
                } else if (left->type == IROLinearOp1Arg &&
                           left->nodetype == ETYPCON &&
                           IRO_IsVariable(left->u.monadic)) {
                    if (!IsSafeTypcon(left)) {
                        return FALSE;
                    }
                    *resultNd2 = left;
                    indirect = left->u.monadic;
                    *resultNd1 = right;
                } else {
                    return FALSE;
                }
            } else if (nd->u.diadic.left->flags & IROLF_LoopInvariant) {
                if (IRO_IsVariable(right)) {
                    *resultNd2 = right;
                    indirect = right;
                    *resultNd1 = left;
                } else if (right->type == IROLinearOp1Arg &&
                           right->nodetype == ETYPCON &&
                           IRO_IsVariable(right->u.monadic) &&
                           nd->type == IROLinearOp2Arg &&
                           nd->nodetype == EMUL) {
                    if (!IsSafeTypcon(right)) {
                        return FALSE;
                    }
                    *resultNd2 = right;
                    indirect = right->u.monadic;
                    *resultNd1 = left;
                } else {
                    return FALSE;
                }
            } else {
                return FALSE;
            }
        }
    } else if (nd->type == IROLinearOp2Arg &&
               (nd->nodetype == EDIV || nd->nodetype == ESHR) &&
               nd->rtype->size <= 4 &&
               IS_TYPE_INT(nd->rtype)) {
        if (IRO_IsVariable(nd->u.diadic.left) && IRO_IsConstant(nd->u.diadic.right)) {
            divisor = nd->u.diadic.right->u.node->data.intval;
            if (nd->type == IROLinearOp2Arg && nd->nodetype == ESHR) {
                if (CInt64_GetULong(&divisor) > 32 || divisor.hi) {
                    return FALSE;
                }
                divisor = CInt64_Shl(cint64_one, divisor);
            }
            *resultNd2 = nd->u.diadic.left;
            indirect = nd->u.diadic.left;
        } else {
            return FALSE;
        }
    } else {
        return FALSE;
    }

    if (nd->type == IROLinearOp2Arg &&
        nd->nodetype == ESHL &&
        (!IRO_IsConstant(*resultNd1) ||
         (SInt32)CInt64_GetULong(&(*resultNd1)->u.node->data.intval) < 0 ||
         (SInt32)CInt64_GetULong(&(*resultNd1)->u.node->data.intval) > 32 ||
         (div = (*resultNd1)->u.node->data.intval.hi))) {
        return FALSE;
    }

    CError_ASSERT(834, indirect->u.monadic->u.node != NULL);
    *resultVar = IRO_FindVar(indirect->u.monadic->u.node->data.objref, FALSE, TRUE);
    if (!*resultVar || (*resultVar)->inductionState != 2) {
        return FALSE;
    }

    if (copts.ANSIstrict || copts.strengthreductionstrict) {
        Type* type = (*resultVar)->object->type;
        if (is_unsigned(type) && type->size < stunsignedlong.size) {
            return FALSE;
        }
    }

#if VERSION >= VERSION_GC_2_5
    ind = FirstInd;
    while (ind && ind->var != *resultVar) {
        ind = ind->next;
    }

    CError_ASSERT(873, ind != NULL);
#endif

    if (nd->type == IROLinearOp2Arg && (nd->nodetype == ESHR || nd->nodetype == EDIV)) {
#if VERSION < VERSION_GC_2_5
        ind = FirstInd;
        while (ind && ind->var != *resultVar) {
            ind = ind->next;
        }

#undef CError_LINE_SHIFT
#define CError_LINE_SHIFT -22
        CError_ASSERT(873, ind != NULL);
#endif

        if (ind->addNode == NULL) {
            SInt32 addConst = ind->addConst;
            if (addConst < (val = CInt64_GetULong(&divisor))) {
                return FALSE;
            }
            if ((div = addConst / val) <= 0) {
                return FALSE;
            }
#if VERSION >= VERSION_GC_2_5
            if (addConst % val) {
                return FALSE;
            }
#endif

            *resultNd1 = IRO_NewLinear(IROLinearOperand);
            enode = IRO_NewENode(EINTCONST);
            CInt64_SetULong(&enode->data.intval, div);
            enode->rtype = nd->u.diadic.left->rtype;
            (*resultNd1)->rtype = nd->u.diadic.left->rtype;
            (*resultNd1)->u.node = enode;
        } else {
            return FALSE;
        }
    }

#if VERSION >= VERSION_GC_2_5
    if (nd->type == IROLinearOp2Arg &&
        nd->nodetype == EADD &&
        (IS_TYPE_INT(nd->rtype) || IS_TYPE_POINTER_ONLY(nd->rtype))) {
        if (TreeContains(ind->nd, nd) && (ind->nd->flags & IROLF_Reffed)) {
            return FALSE;
        }
    }
#endif

    return TRUE;
}

static void IRO_RemoveExpr_Action(IROLinear* linear, Boolean isFirst) {
    if (isFirst && linear->expr) {
        IRO_RemoveExpr(linear->expr);
    }
}

static void IRO_ActUnmarkRISCandidate(IROLinear* linear, Boolean isFirst) {
    if (isFirst && linear->expr) {
        linear->flags &= ~IROLF_Ris;
    }
}

static IROExpr* CreateRIS(
    IROExpr* expr, IROLinear* inductionNd, IROLinear* factorNd, IROLoopInd* induction, int squared
) {
    Object* tempObj;
    IROLinear* thirdnode;
    Object* stepTempObj;
    Boolean stepTemp;
    IROLinear* firstnode;
    IROLinear* secondnode;
    IROLinear* fifthnode;
    IROLinear* fourthnode;
    Type* type;
    IROLinear* cast;
    ENode* enode;
    IROList list1;
    IROList list2;

    stepTemp = FALSE;
    type = expr->linear->rtype;
    tempObj = create_temp_object(type);
    IRO_FindVar(tempObj, TRUE, TRUE);
    IRO_InitList(&list1);

    if (IS_LINEAR_DIADIC(expr->linear, EADD)) {
        firstnode = IRO_DuplicateExpr(expr->linear, &list1);
    } else if (IS_LINEAR_DIADIC_2(expr->linear, EDIV, ESHR)) {
        firstnode = IRO_DuplicateExpr(expr->linear, &list1);
    } else {
        firstnode = IRO_NewLinear(IROLinearOp2Arg);
        firstnode->index = ++IRO_NumLinear;
        firstnode->rtype = type;
        firstnode->nodetype = EMUL;
        firstnode->u.diadic.left = IRO_DuplicateExpr(inductionNd, &list1);
        if (squared) {
            firstnode->u.diadic.right = IRO_DuplicateExpr(inductionNd, &list1);
        } else {
            firstnode->u.diadic.right = IRO_DuplicateExpr(factorNd, &list1);
        }
        IRO_AddToList(firstnode, &list1);
    }

    secondnode = IRO_NewLinear(IROLinearOp2Arg);
    secondnode->index = ++IRO_NumLinear;
    secondnode->rtype = type;
    secondnode->nodetype = EASS;
    secondnode->u.diadic.left = IRO_TempReference(tempObj, &list1);
    secondnode->u.diadic.left->flags |= IROLF_Ind | IROLF_Assigned;
    secondnode->u.diadic.left->u.monadic->flags |= IROLF_Ind | IROLF_Assigned;
    secondnode->u.diadic.right = firstnode;
    IRO_AddToList(secondnode, &list1);
    IRO_Paste(list1.head, list1.tail, PredInt);
    if (!IS_LINEAR_DIADIC_2(expr->linear, EDIV, ESHR) &&
        induction->addConst != 1 &&
        (induction->addNode || !IRO_IsConstant(factorNd))) {
        stepTemp = TRUE;
        IRO_InitList(&list1);
        thirdnode = IRO_NewLinear(IROLinearOp2Arg);
        thirdnode->index = ++IRO_NumLinear;
        thirdnode->rtype = factorNd->rtype;
        thirdnode->nodetype = EMUL;

        if (!induction->addNode) {
            thirdnode->u.diadic.left = IRO_DuplicateExpr(factorNd, &list1);
            thirdnode->u.diadic.right = IRO_NewLinear(IROLinearOperand);
            thirdnode->u.diadic.right->index = ++IRO_NumLinear;
            enode = IRO_NewENode(EINTCONST);
            enode->rtype = factorNd->rtype;
            thirdnode->u.diadic.right->rtype = factorNd->rtype;
            CInt64_SetLong(&enode->data.intval, induction->addConst);
            thirdnode->u.diadic.right->u.node = enode;
            IRO_AddToList(thirdnode->u.diadic.right, &list1);
        } else {
            thirdnode->u.diadic.left = IRO_DuplicateExpr(factorNd, &list1);
            thirdnode->u.diadic.right = IRO_DuplicateExpr(induction->addNode, &list1);
            if (factorNd->rtype != induction->addNode->rtype) {
                cast = IRO_NewLinear(IROLinearOp1Arg);
                cast->nodetype = ETYPCON;
                cast->index = ++IRO_NumLinear;
                cast->rtype = factorNd->rtype;
                cast->u.monadic = thirdnode->u.diadic.right;
                IRO_AddToList(cast, &list1);
                thirdnode->u.diadic.right = cast;
            }
        }
        IRO_AddToList(thirdnode, &list1);

        stepTempObj = create_temp_object(factorNd->rtype);

        fourthnode = IRO_NewLinear(IROLinearOp2Arg);
        fourthnode->index = ++IRO_NumLinear;
        fourthnode->rtype = factorNd->rtype;
        fourthnode->nodetype = EASS;
        fourthnode->u.diadic.left = IRO_TempReference(stepTempObj, &list1);
        fourthnode->u.diadic.left->flags |= IROLF_Ind | IROLF_Assigned;
        fourthnode->u.diadic.left->u.monadic->flags |= IROLF_Ind | IROLF_Assigned;
        fourthnode->u.diadic.right = thirdnode;
        IRO_AddToList(fourthnode, &list1);
        IRO_Paste(list1.head, list1.tail, PredInt);
    }

    IRO_InitList(&list2);
    fifthnode = IRO_NewLinear(IROLinearOp2Arg);
    fifthnode->index = ++IRO_NumLinear;
    fifthnode->rtype = type;
    if (induction->nd->type == IROLinearOp2Arg) {
        if (induction->nd->nodetype == EASS &&
            IS_LINEAR_DIADIC(induction->nd->u.diadic.right, EADD)) {
            fifthnode->nodetype = EADDASS;
        } else if (induction->nd->nodetype == EASS &&
                   IS_LINEAR_DIADIC(induction->nd->u.diadic.right, ESUB)) {
            fifthnode->nodetype = ESUBASS;
        } else {
            fifthnode->nodetype = induction->nd->nodetype;
        }
    } else {
        if (induction->nd->nodetype == EPREINC || induction->nd->nodetype == EPOSTINC) {
            fifthnode->nodetype = EADDASS;
        } else {
            fifthnode->nodetype = ESUBASS;
        }
    }

    fifthnode->u.diadic.left = IRO_TempReference(tempObj, &list2);
    fifthnode->u.diadic.left->flags |= IROLF_Ind | IROLF_Used | IROLF_Assigned;
    fifthnode->u.diadic.left->u.monadic->flags |= IROLF_Ind | IROLF_Used | IROLF_Assigned;
    if (!squared) {
        if (!stepTemp) {
            if (induction->addConst == 1 || IS_LINEAR_DIADIC_2(expr->linear, EDIV, ESHR)) {
                fifthnode->u.diadic.right = IRO_DuplicateExpr(factorNd, &list2);
            } else {
                fifthnode->u.diadic.right = IRO_NewLinear(IROLinearOperand);
                fifthnode->u.diadic.right->index = ++IRO_NumLinear;
                enode = IRO_NewENode(EINTCONST);
                enode->rtype = factorNd->rtype;
                fifthnode->u.diadic.right->rtype = factorNd->rtype;
                CInt64_SetLong(
                    &enode->data.intval,
                    induction->addConst * CInt64_GetULong(&factorNd->u.node->data.intval)
                );
                fifthnode->u.diadic.right->u.node = enode;
                IRO_AddToList(fifthnode->u.diadic.right, &list2);
            }
        } else {
            fifthnode->u.diadic.right = IRO_TempReference(stepTempObj, &list2);
            fifthnode->u.diadic.right->flags |= IROLF_Used;
        }
    }

    fifthnode->index = ++IRO_NumLinear;
    IRO_AddToList(fifthnode, &list2);
#if VERSION < VERSION_GC_2_5
    IRO_Paste(list2.head, list2.tail, IRO_FindStart(induction->nd));
#else
    if (IS_LINEAR_DIADIC(expr->linear, EADD) &&
        (IS_TYPE_INT(expr->linear->rtype) || IS_TYPE_POINTER_ONLY(expr->linear->rtype)) &&
        TreeContains(induction->nd, expr->linear) &&
        !(induction->nd->flags & IROLF_Reffed)) {
        IRO_PasteAfter(list2.head, list2.tail, induction->nd);
    } else {
        IRO_Paste(list2.head, list2.tail, IRO_FindStart(induction->nd));
    }
#endif
    IRO_WalkTree(expr->linear, IRO_RemoveExpr_Action);
    expr->temp = tempObj;
    expr->next = RisList;
    RisList = expr;
    return expr;
}

#if VERSION < VERSION_GC_2_5
#undef CError_LINE_SHIFT
#define CError_LINE_SHIFT -53
#endif
static IRONode* CreatePreHeader(IRONode* fnode1, IRONode* fnode2) {
    IROLinear* labelnode;
    IRONode* newfnode;
    IRONode* iter;
    CLabel* oldlabel;
    CLabel* newlabel;
    SwitchInfo* swinfo;
    SwitchCase* swcase;

    newfnode = oalloc(sizeof(IRONode));
    memset(newfnode, 0, sizeof(IRONode));
    newfnode->index = IRO_NumNodes;
    IRO_NumNodes++;

    labelnode = IRO_NewLinear(IROLinearLabel);
    labelnode->index = IRO_NumLinear++;
    labelnode->next = NULL;
    labelnode->u.label.label = IRO_NewLabel();
    labelnode->flags |= IROLF_StartsNode;
    labelnode->u.label.label->stmt = (Statement*)newfnode;
    newfnode->first = labelnode;
    newfnode->last = labelnode;

    if (fnode2) {
        fnode2->last->next = labelnode;
        labelnode->next = IRO_NewLinear(IROLinearNop);
        labelnode->next->next = fnode1->first;
        fnode2->nextnode = newfnode;
        newfnode->nextnode = fnode1;
    } else {
        CError_ASSERT(1313, fnode1->first->type == IROLinearLabel);
        labelnode->next = IRO_NewLinear(IROLinearGoto);
        labelnode->next->u.label.label = fnode1->first->u.label.label;
        IRO_LastNode->last->next = labelnode;
        IRO_LastNode->nextnode = newfnode;
        IRO_LastNode = newfnode;
        IRO_LastLinear = labelnode->next;
    }

    newfnode->last = labelnode->next;

#if MWCCPSP
    // 3.0.1 frees the table back to the pools (IRO_RebuildNodeTable)
    IRO_NodeTable = IRO_malloc(sizeof(IRONode*) * IRO_NumNodes);
#else
    IRO_NodeTable = oalloc(sizeof(IRONode*) * IRO_NumNodes);
#endif
    memset(IRO_NodeTable, 0, sizeof(IRONode*) * IRO_NumNodes);
    for (iter = IRO_FirstNode; iter; iter = iter->nextnode) {
        IRO_NodeTable[iter->index] = iter;
    }

    if (fnode1->first->type == IROLinearLabel) {
        oldlabel = fnode1->first->u.label.label;
        newlabel = newfnode->first->u.label.label;
        for (iter = IRO_FirstNode; iter; iter = iter->nextnode) {
            if (!Bv_IsBitSet(iter->index, InLoop) && iter != newfnode) {
                switch (iter->last->type) {
                    case IROLinearGoto:
                        if (iter->last->u.label.label == oldlabel) {
                            iter->last->u.label.label = newlabel;
                        }
                        break;
                    case IROLinearIf:
                    case IROLinearIfNot:
                        if (iter->last->u.label.label == oldlabel) {
                            iter->last->u.label.label = newlabel;
                        }
                        break;
                    case IROLinearSwitch:
                        swinfo = iter->last->u.swtch.info;
                        for (swcase = swinfo->cases; swcase; swcase = swcase->next) {
                            if (swcase->label == oldlabel) {
                                swcase->label = newlabel;
                            }
                        }
                        if (swinfo->defaultlabel == oldlabel) {
                            swinfo->defaultlabel = newlabel;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }

    IRO_ComputeSuccPred();
    IRO_ComputeDom();
    return newfnode;
}

static IRONode* CreateNewLoopExitSuccessor(IRONode* fnode1) {
    IROLinear* labelnode;
    IRONode* fnode2;
    CLabel* oldlabel;
    SwitchInfo* swinfo;
    IRONode* iter;
    UInt16 i;
    IRONode* newfnode;
    Boolean placeAfterExit;
    CLabel* newlabel;
    SwitchCase* swcase;

    CError_ASSERT(1414, fnode1 != NULL && LoopExitNumber == 1);

    fnode2 = NULL;
    placeAfterExit = FALSE;

    for (i = 0; i < fnode1->numpred; i++) {
        iter = IRO_NodeTable[fnode1->pred[i]];
        if (Bv_IsBitSet(iter->index, InLoop_Exits)) {
            CError_ASSERT(1425, fnode2 == NULL);
            fnode2 = iter;
            if (!placeAfterExit) {
                if (!iter->last ||
                    !fnode1->first ||
                    fnode1->first->type != IROLinearLabel ||
                    ((iter->last->type == IROLinearIf || iter->last->type == IROLinearIfNot) &&
                     iter->last->u.label.label != fnode1->first->u.label.label)) {
                    placeAfterExit = TRUE;
                }
            }
        }
    }

    CError_ASSERT(1441, fnode2 != NULL);

    newfnode = oalloc(sizeof(IRONode));
    memset(newfnode, 0, sizeof(IRONode));
    newfnode->index = IRO_NumNodes;
    IRO_NumNodes++;

    labelnode = IRO_NewLinear(IROLinearLabel);
    labelnode->index = IRO_NumLinear++;
    labelnode->next = NULL;
    labelnode->u.label.label = IRO_NewLabel();
    labelnode->flags |= IROLF_StartsNode;
    labelnode->u.label.label->stmt = (Statement*)newfnode;
    newfnode->first = labelnode;
    newfnode->last = labelnode;

    if (placeAfterExit) {
        fnode2->last->next = labelnode;
        labelnode->next = IRO_NewLinear(IROLinearNop);
        labelnode->next->next = fnode1->first;
        fnode2->nextnode = newfnode;
        newfnode->nextnode = fnode1;
    } else {
        CError_ASSERT(1481, fnode1->first->type == IROLinearLabel);
        labelnode->next = IRO_NewLinear(IROLinearGoto);
        labelnode->next->u.label.label = fnode1->first->u.label.label;
        IRO_LastNode->last->next = labelnode;
        IRO_LastNode->nextnode = newfnode;
        IRO_LastNode = newfnode;
        IRO_LastLinear = labelnode->next;
    }

    newfnode->last = labelnode->next;

#if MWCCPSP
    // 3.0.1 frees the table back to the pools (IRO_RebuildNodeTable)
    IRO_NodeTable = IRO_malloc(sizeof(IRONode*) * IRO_NumNodes);
#else
    IRO_NodeTable = oalloc(sizeof(IRONode*) * IRO_NumNodes);
#endif
    memset(IRO_NodeTable, 0, sizeof(IRONode*) * IRO_NumNodes);
    for (iter = IRO_FirstNode; iter; iter = iter->nextnode) {
        IRO_NodeTable[iter->index] = iter;
    }

    if (fnode1->first->type == IROLinearLabel) {
        oldlabel = fnode1->first->u.label.label;
        newlabel = newfnode->first->u.label.label;
        switch (fnode2->last->type) {
            case IROLinearGoto:
                if (fnode2->last->u.label.label == oldlabel) {
                    fnode2->last->u.label.label = newlabel;
                }
                break;
            case IROLinearIf:
            case IROLinearIfNot:
                if (fnode2->last->u.label.label == oldlabel) {
                    fnode2->last->u.label.label = newlabel;
                }
                break;
            case IROLinearSwitch:
                swinfo = fnode2->last->u.swtch.info;
                for (swcase = swinfo->cases; swcase; swcase = swcase->next) {
                    if (swcase->label == oldlabel) {
                        swcase->label = newlabel;
                    }
                }
                if (swinfo->defaultlabel == oldlabel) {
                    swinfo->defaultlabel = newlabel;
                }
                break;
            default:
                break;
        }
    }

    IRO_ComputeSuccPred();
    IRO_ComputeDom();
    return newfnode;
}

void AddPreds(IRONode* fnode) {
    IRONode* pred;
    int i;

    for (i = 0; i < fnode->numpred; i++) {
        pred = IRO_NodeTable[fnode->pred[i]];
        if (!Bv_IsBitSet(pred->index, InLoop)) {
            Bv_SetBit(pred->index, InLoop);
            AddPreds(pred);
        }
    }
}

static void RemoveNoopsFromExprList(void) {
    IROExpr* expr;
    IROExpr* prev;

    expr = IRO_FirstExpr;
    prev = NULL;
    while (expr) {
        if (expr->linear->type == IROLinearNop) {
            if (prev) {
                prev->next = expr->next;
            } else {
                IRO_FirstExpr = expr->next;
            }
            expr = expr->next;
        } else {
            prev = expr;
            expr = expr->next;
        }
    }
}

void IncLoopDepth(void) {
    IRONode* fnode;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (Bv_IsBitSet(fnode->index, InLoop)) {
            fnode->loopdepth++;
        }
    }
}

#if MWCCPSP
#include "compiler/IroLoop301.h"

extern void IRO_MarkCounterLoops(IRONaturalLoop* loops);

#define FREE_BV(bv) \
    if (bv) { \
        IRO_free(bv); \
        (bv) = NULL; \
    }

static IRONaturalLoop* FindLoopHeaders(void) {
    IRONaturalLoop* list;
    IRONaturalLoop* loop;
    IRONode* fnode;
    UInt16 i;
    Boolean isHeader;

    list = NULL;
    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        isHeader = FALSE;
        for (i = 0; i < fnode->numpred; i++) {
            if (Bv_IsBitSet(fnode->index, IRO_NodeTable[fnode->pred[i]]->dom)) {
                isHeader = TRUE;
            }
        }
        if (isHeader) {
            loop = IRO_malloc(sizeof(IRONaturalLoop));
            memset(loop, 0, sizeof(IRONaturalLoop));
            loop->next = list;
            loop->header = fnode;
            loop->x10 = fnode;
            list = loop;
        }
    }
    return list;
}

void IRO_AddLoopPreds(IRONode* fnode, BitVector* bv) {
    UInt16 i;
    IRONode* pred;

    for (i = 0; i < fnode->numpred; i++) {
        pred = IRO_NodeTable[fnode->pred[i]];
        if (!Bv_IsBitSet(pred->index, bv)) {
            Bv_SetBit(pred->index, bv);
            IRO_AddLoopPreds(pred, bv);
        }
    }
}

static void ComputeLoopBody(IRONaturalLoop* loop) {
    IRONode* header;
    IRONode* pred;
    BitVector* bv;
    UInt16 i;

    if (loop->nodes) {
        FREE_BV(loop->nodes);
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

extern void func_0056AB70(IROLinear* linear, Boolean alloc, Boolean stop, Boolean recurse);
extern Boolean func_0055A3F0(IROLinear* conv); // IroTypeConv.c
extern VarRecord* IRO_FindAssigned301(IROLinear* linear, Boolean* isBitfield);

// 0x5894E0: what the loop's linears may change, in loop->xC
void func_005894E0(IRONaturalLoop* loop) {
    IRONode* fnode;
    IROLinear* nd;

    if (loop->xC) {
        FREE_BV(loop->xC);
        loop->xC = NULL;
    }
    Bv_AllocVector(&loop->xC, IRO_NumVars + 1);
    Bv_AllocVector(&IRO_VarKills, IRO_NumVars + 1);
    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (!Bv_IsBitSet(fnode->index, loop->nodes)) {
            continue;
        }
        for (nd = fnode->first; nd; nd = nd->next) {
            Bv_Clear(IRO_VarKills);
            IRO_GetKills(nd);
            Bv_Or(IRO_VarKills, loop->xC);
            if (nd == fnode->last) {
                break;
            }
        }
    }
    IRO_DumpBits("Killed in loop: ", loop->xC);
}

// 0x591FB0: the loop's referenced linears that depend on nothing the loop
// changes are invariant; 3.0.1 clears the mark only on each node's first
// linear first
void func_00591FB0(IRONaturalLoop* loop) {
    IRONode* fnode;
    IROLinear* nd;

    func_005894E0(loop);
    IRO_Depends = IRO_VarKills;
    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (!Bv_IsBitSet(fnode->index, loop->nodes)) {
            continue;
        }
        if (!(nd = fnode->first)) {
            continue;
        }
        nd->flags &= ~IROLF_LoopInvariant;
        for (;;) {
            if ((nd->flags & IROLF_Reffed) && nd->type != IROLinearNop) {
                func_0056AB70(nd, FALSE, FALSE, TRUE);
                if (!IRO_IsVolatile && !Bv_BitsInCommon(loop->xC, IRO_Depends)) {
                    nd->flags |= IROLF_LoopInvariant;
                }
                if (IRO_CouldError) {
                    nd->flags |= IROLF_CouldError;
                } else {
                    nd->flags &= ~IROLF_CouldError;
                }
            }
            if (nd == fnode->last) {
                break;
            }
            nd = nd->next;
        }
    }
}

// 0x58B050: IRO_FindLoopTailsAndExits
void func_0058B050(IRONaturalLoop* loop) {
    IRONode* fnode;
    IRONode* succ;
    UInt16 i;

    Bv_AllocVector(&loop->x54, IRO_NumNodes + 1);
    Bv_AllocVector(&loop->x58, IRO_NumNodes + 1);
    loop->numExits = 0;
    loop->exitSuccessor = NULL;
    loop->numTails = 0;
    loop->tail = NULL;
    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (!Bv_IsBitSet(fnode->index, loop->nodes)) {
            continue;
        }
        for (i = 0; i < fnode->numsucc; i++) {
            succ = IRO_NodeTable[fnode->succ[i]];
            if (succ == loop->header) {
                Bv_SetBit(fnode->index, loop->x58);
                loop->tail = fnode;
                loop->numTails++;
            }
            if (!Bv_IsBitSet(succ->index, loop->nodes)) {
                loop->numExits++;
                loop->exitSuccessor = succ;
                Bv_SetBit(fnode->index, loop->x54);
            }
        }
    }
    IRO_Dump(
        "IRO_FindLoopTailsAndExits:For header %d, Loop exits and loop tails are  \n",
        loop->header->index
    );
    IRO_DumpBits("Loop Exits: ", loop->x54);
    IRO_DumpBits("Loop Tails: ", loop->x58);
    IRO_Dump("LoopExitNum = %d: \n", loop->numExits);
    if (loop->exitSuccessor) {
        IRO_Dump("lp->loopExitSuccessor node = %d: \n", loop->exitSuccessor->index);
    }
}

// 0x58B250: the loop's nodes that dominate every tail and every exit but
// the header (mustreach1)
void func_0058B250(IRONaturalLoop* loop) {
    IRONode* fnode;
    IRONode* scan;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        fnode->mustreach1 = FALSE;
        if (!Bv_IsBitSet(fnode->index, loop->nodes)) {
            continue;
        }
        fnode->mustreach1 = TRUE;
        for (scan = IRO_FirstNode; scan; scan = scan->nextnode) {
            if (!Bv_IsBitSet(scan->index, loop->nodes)) {
                continue;
            }
            if (Bv_IsBitSet(scan->index, loop->x58) && !Bv_IsBitSet(fnode->index, scan->dom)) {
                fnode->mustreach1 = FALSE;
            }
            if (Bv_IsBitSet(scan->index, loop->x54) &&
                scan != loop->header &&
                !Bv_IsBitSet(fnode->index, scan->dom)) {
                fnode->mustreach1 = FALSE;
            }
            if (!fnode->mustreach1) {
                break;
            }
        }
    }
}

// 0x58B3D0: the loop's nodes that dominate the loop's back edges and every
// exit edge (mustreach, unk3F)
void func_0058B3D0(IRONaturalLoop* loop) {
    IRONode* fnode;
    IRONode* pred;
    IRONode* scan;
    UInt16* list;
    UInt16 count;
    UInt16 i;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        fnode->mustreach = FALSE;
        fnode->unk3F = FALSE;
        if (!Bv_IsBitSet(fnode->index, loop->nodes)) {
            continue;
        }
        fnode->mustreach = TRUE;
        fnode->unk3F = TRUE;
        count = loop->header->numpred;
        list = loop->header->pred;
        for (i = 0; i < count; i++) {
            pred = IRO_NodeTable[list[i]];
            if (Bv_IsBitSet(pred->index, loop->nodes) && !Bv_IsBitSet(fnode->index, pred->dom)) {
                fnode->mustreach = FALSE;
                fnode->unk3F = FALSE;
                break;
            }
        }
        for (scan = IRO_FirstNode; scan; scan = scan->nextnode) {
            if (!Bv_IsBitSet(scan->index, loop->nodes)) {
                continue;
            }
            count = scan->numsucc;
            list = scan->succ;
            for (i = 0; i < count; i++) {
                if (!Bv_IsBitSet(list[i], loop->nodes) && !Bv_IsBitSet(fnode->index, scan->dom)) {
                    fnode->mustreach = FALSE;
                    fnode->unk3F = FALSE;
                    break;
                }
            }
            if (!fnode->mustreach) {
                break;
            }
        }
    }
}

static inline Boolean IsIntOrPointer(Type* type) {
    return (type->type == TYPEINT && TYPE_INTEGRAL(type)->integral < IT_17) ||
        type->type == TYPEPOINTER;
}

static inline Boolean IsStep(IROLinear* nd) {
    return nd && (IRO_IsIntConstant(nd) || (nd->flags & IROLF_LoopInvariant));
}

// 0x588CC0: the loop's induction variables: updated only by +=, -= or ++/--
// by an invariant non-zero step, or v = v + step / v = v - step, in the loop
void func_00588CC0(IRONaturalLoop* loop) {
    VarRecord* var;
    IRONode* fnode;
    IROLinear* nd;
    IROLinear* right;
    IROLinear* swap;
    IROLoopInd301* ind;
    Object* obj;
    Object* left;
    Object* other;
    CInt64 value;
    Boolean isUpdate;

    for (var = IRO_FirstVar; var; var = var->next) {
        var->inductionState = 1;
    }
    Bv_AllocVector(&IRO_VarKills, IRO_NumVars + 1);
    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (!Bv_IsBitSet(fnode->index, loop->nodes) || !(nd = fnode->first)) {
            continue;
        }
        for (;;) {
            isUpdate = FALSE;
            if (nd->type == IROLinearOp2Arg &&
                (nd->nodetype == EADDASS || nd->nodetype == ESUBASS) &&
                (obj = IRO_IsVariable(nd->u.diadic.left)) &&
                IsIntOrPointer(nd->rtype) &&
                obj->type->size == nd->rtype->size &&
                IsStep(nd->u.diadic.right) &&
                !IRO_IsConstantZero(nd->u.diadic.right)) {
                isUpdate = TRUE;
            } else if (nd->type == IROLinearOp1Arg &&
                       nd->nodetype <= EPREDEC &&
                       (obj = IRO_IsVariable(nd->u.monadic)) &&
                       IsIntOrPointer(nd->rtype) &&
                       obj->type->size == nd->rtype->size) {
                isUpdate = TRUE;
            } else if (nd->type == IROLinearOp2Arg &&
                       nd->nodetype == EASS &&
                       (obj = IRO_IsVariable(nd->u.diadic.left)) &&
                       IsIntOrPointer(nd->rtype) &&
                       obj->type->size == nd->rtype->size &&
                       (right = nd->u.diadic.right)->type == IROLinearOp2Arg &&
                       (right->nodetype == EADD || right->nodetype == ESUB)) {
                left = IRO_IsVariable(right->u.diadic.left);
                other = IRO_IsVariable(right->u.diadic.right);
                if (right->nodetype == EADD) {
                    if (left == obj &&
                        other != obj &&
                        IsStep(right->u.diadic.right) &&
                        !IRO_IsConstantZero(right->u.diadic.right)) {
                        isUpdate = TRUE;
                    }
                    if (other == obj &&
                        left != obj &&
                        IsStep(right->u.diadic.left) &&
                        !IRO_IsConstantZero(right->u.diadic.left)) {
                        isUpdate = TRUE;
                        swap = right->u.diadic.left;
                        right->u.diadic.left = right->u.diadic.right;
                        nd->u.diadic.right->u.diadic.right = swap;
                    }
                } else if (left == obj &&
                           other != obj &&
                           IsStep(right->u.diadic.right) &&
                           !IRO_IsConstantZero(right->u.diadic.right)) {
                    isUpdate = TRUE;
                }
            }
            if (isUpdate) {
                if ((var = IRO_FindAssigned301(nd, NULL))) {
                    if (var->inductionState == 2) {
                        var->inductionState = 0;
                    } else if (var->inductionState == 1) {
                        var->inductionState = 2;
                    }
                }
            } else {
                IRO_GetKills(nd);
            }
            if (nd == fnode->last) {
                break;
            }
            nd = nd->next;
        }
    }
    for (var = IRO_FirstVar; var; var = var->next) {
        if (Bv_IsBitSet(var->index, IRO_VarKills)) {
            var->inductionState = 0;
        }
    }

    loop->inductions = NULL;
    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (!Bv_IsBitSet(fnode->index, loop->nodes) || !(nd = fnode->first)) {
            continue;
        }
        for (;;) {
            if (((nd->type == IROLinearOp2Arg &&
                  (nd->nodetype == EADDASS || nd->nodetype == ESUBASS || nd->nodetype == EASS)) ||
                 (nd->type == IROLinearOp1Arg && nd->nodetype <= EPREDEC)) &&
                (var = IRO_FindAssigned301(nd, NULL)) &&
                var->inductionState == 2) {
                ind = IRO_Alloc(sizeof(IROLoopInd301));
                ind->flags = 0;
                ind->var = var;
                ind->fnode = fnode;
                ind->nd = nd;
                ind->init = NULL;
                ind->initNode = NULL;
                ind->x18 = 0;
                ind->finalValue = cint64_zero;
                ind->step = cint64_zero;
                ind->stepExpr = NULL;
                ind->next = (IROLoopInd301*)loop->inductions;
                if (nd->type == IROLinearOp2Arg) {
                    is_unsigned(IRO_UnqualifiedType(nd->rtype));
                    if (nd->nodetype == EADDASS || nd->nodetype == ESUBASS) {
                        right = nd->u.diadic.right;
                        if (IRO_IsIntConstant(right)) {
                            value = right->u.node->data.intval;
                            if (nd->nodetype == EADDASS && CInt64_Less(value, cint64_zero)) {
                                nd->nodetype = ESUBASS;
                                right->u.node->data.intval = CInt64_Neg(value);
                            }
                            IRO_TruncateValueToType(
                                &nd->u.diadic.right->u.node->data.intval, nd->u.diadic.right->rtype
                            );
                            ind->step = nd->u.diadic.right->u.node->data.intval;
                            ind->stepExpr = NULL;
                        } else {
                            ind->stepExpr = nd->u.diadic.right;
                        }
                        if (nd->nodetype == ESUBASS) {
                            ind->flags |= 4;
                        }
                    } else if (nd->nodetype == EASS) {
                        right = nd->u.diadic.right;
                        if (IRO_IsIntConstant(right->u.diadic.right)) {
                            value = right->u.diadic.right->u.node->data.intval;
                            if (right->nodetype == EADD && CInt64_Less(value, cint64_zero)) {
                                right->nodetype = ESUB;
                                right->u.diadic.right->u.node->data.intval = CInt64_Neg(value);
                            }
                            IRO_TruncateValueToType(
                                &right->u.diadic.right->u.node->data.intval,
                                right->u.diadic.right->rtype
                            );
                            ind->step = right->u.diadic.right->u.node->data.intval;
                            ind->stepExpr = NULL;
                        } else {
                            ind->stepExpr = right->u.diadic.right;
                        }
                        if (right->nodetype == ESUB) {
                            ind->flags |= 4;
                        }
                    }
                } else {
                    if (nd->rtype->type == TYPEPOINTER) {
                        CInt64_SetLong(&value, TYPE_POINTER(nd->rtype)->target->size);
                        ind->step = value;
                    } else {
                        ind->step = cint64_one;
                    }
                    ind->stepExpr = NULL;
                    if (nd->nodetype == EPOSTDEC || nd->nodetype == EPREDEC) {
                        ind->flags |= 4;
                    }
                }
                loop->inductions = ind;
                if (nd->type == IROLinearOp2Arg &&
                    (nd->nodetype == EADDASS || nd->nodetype == ESUBASS)) {
                    if (nd->u.diadic.right->flags & IROLF_LoopInvariant) {
                        IRO_Dump(
                            "Found induction variable the new way: %s\n", var->object->name->name
                        );
                    } else {
                        IRO_Dump(
                            "Found induction variable the old way: %s\n", var->object->name->name
                        );
                    }
                } else if (nd->type == IROLinearOp2Arg &&
                           (nd->u.diadic.right->nodetype == EADD ||
                            nd->u.diadic.right->nodetype == ESUB)) {
                    IRO_Dump("Found induction variable the new way: %s\n", var->object->name->name);
                } else {
                    IRO_Dump("Found induction variable the old way: %s\n", var->object->name->name);
                }
            }
            if (nd == fnode->last) {
                break;
            }
            nd = nd->next;
        }
    }
}

// 0x59B190: the set bits of bv in [from, to)
UInt32 func_0059B190(BitVector* bv, UInt32 from, UInt32 to) {
    UInt32 count = 0;

    if (from > to || to > bv->size * 32) {
        CError_FATAL(347);
    }
    for (; from < to; from++) {
        if (Bv_IsBitSet(from, bv)) {
            count++;
        }
    }
    return count;
}

// 0x59B130: the set bits of bv
UInt32 func_0059B130(BitVector* bv) {
    return func_0059B190(bv, 0, bv->size * 32);
}

// 0x588AE0: the assignment that gives an induction variable its value before
// the loop, looking back from the preheader through single predecessors; it
// must be a plain assignment that changes nothing else
void func_00588AE0(IRONaturalLoop* loop, IROLoopInd301* ind) {
    IRONode* fnode;
    IROLinear* nd;
    UInt32 index;
    UInt16 pred;

    ind->init = NULL;
    ind->initNode = NULL;
    if (!ind) {
        return;
    }
    index = ind->var->index;
    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        fnode->marked = FALSE;
    }
    for (fnode = loop->preheader; fnode;) {
        fnode->marked = TRUE;
        for (nd = fnode->first; nd; nd = nd->next) {
            Bv_Clear(IRO_VarKills);
            IRO_GetKills(nd);
            if (Bv_IsBitSet(index, IRO_VarKills)) {
                ind->init = nd;
                ind->initNode = fnode;
            }
            if (nd == fnode->last) {
                break;
            }
        }
        if (ind->init) {
            break;
        }
        if (fnode == IRO_FirstNode || fnode->numpred != 1) {
            break;
        }
        pred = fnode->pred[0];
        if (Bv_IsBitSet(pred, loop->nodes) || !(fnode = IRO_NodeTable[pred]) || fnode->marked) {
            break;
        }
    }
    if ((nd = ind->init)) {
        if (nd->type == IROLinearOp2Arg && nd->nodetype == EASS) {
            Bv_Clear(IRO_VarKills);
            IRO_GetKills(nd);
            if (func_0059B130(IRO_VarKills) == 1) {
                return;
            }
        }
        ind->init = NULL;
        ind->initNode = NULL;
    }
}

#define LOOP_UP1 (0x80 | 0x200)
#define LOOP_DOWN1 (0x1000 | 0x2000)
#define LOOP_UP 0x200
#define LOOP_DOWN 0x2000

// a step's direction, when it is not +1 or -1
static inline void StepDirection(
    IRONaturalLoop* loop, CInt64 step, Type* type, UInt32 positive, UInt32 negative
) {
    if (CInt64_IsZero(&step)) {
        if (!is_unsigned(type) && CInt64_IsNegative(&step)) {
            loop->flags |= negative;
        }
    } else if (is_unsigned(type) || !CInt64_IsNegative(&step)) {
        loop->flags |= positive;
    } else if (!is_unsigned(type) && CInt64_IsNegative(&step)) {
        loop->flags |= negative;
    }
}

// 0x588260: which way the loop's induction variable goes
void func_00588260(IRONaturalLoop* loop) {
    IROLoopInd301* ind = (IROLoopInd301*)loop->induction;
    IROLinear* nd = ind->nd;
    IROLinear* right;
    IROLinear* constant;
    CInt64 value;

    if (nd->type == IROLinearOp2Arg) {
        switch (nd->nodetype) {
            case EADDASS:
                if (CInt64_IsOne(&ind->step)) {
                    loop->flags |= LOOP_UP1;
                } else if (CInt64_Equal(ind->step, cint64_negone)) {
                    loop->flags |= LOOP_DOWN1;
                } else {
                    StepDirection(loop, ind->step, nd->rtype, LOOP_UP, LOOP_DOWN);
                }
                break;
            case ESUBASS:
                if (CInt64_IsOne(&ind->step)) {
                    loop->flags |= LOOP_DOWN1;
                } else if (CInt64_Equal(ind->step, cint64_negone)) {
                    loop->flags |= LOOP_UP1;
                } else {
                    StepDirection(loop, ind->step, nd->rtype, LOOP_DOWN, LOOP_UP);
                }
                break;
            case EASS:
                right = nd->u.diadic.right;
                if (right->type != IROLinearOp2Arg ||
                    !IRO_IsIntConstant(constant = right->u.diadic.right)) {
                    break;
                }
                value = constant->u.node->data.intval;
                if (right->nodetype == EADD) {
                    if (CInt64_IsOne(&value)) {
                        loop->flags |= LOOP_UP1;
                    } else if (CInt64_Equal(value, cint64_negone)) {
                        loop->flags |= LOOP_DOWN1;
                    } else {
                        StepDirection(loop, value, constant->rtype, LOOP_UP, LOOP_DOWN);
                    }
                } else if (right->nodetype == ESUB) {
                    if (CInt64_IsOne(&value)) {
                        loop->flags |= LOOP_DOWN1;
                    } else if (CInt64_Equal(value, cint64_negone)) {
                        loop->flags |= LOOP_UP1;
                    } else {
                        StepDirection(loop, value, constant->rtype, LOOP_DOWN, LOOP_UP);
                    }
                }
                break;
        }
    } else if (nd->type == IROLinearOp1Arg) {
        switch (nd->nodetype) {
            case EPREINC:
            case EPOSTINC:
                if (CInt64_IsOne(&ind->step)) {
                    loop->flags |= LOOP_UP1;
                } else if (!CInt64_IsZero(&ind->step) &&
                           (is_unsigned(nd->rtype) || !CInt64_IsNegative(&ind->step))) {
                    loop->flags |= LOOP_UP;
                }
                break;
            case EPREDEC:
            case EPOSTDEC:
                if (CInt64_IsOne(&ind->step)) {
                    loop->flags |= LOOP_DOWN1;
                } else if (!CInt64_IsZero(&ind->step) &&
                           (is_unsigned(nd->rtype) || !CInt64_IsNegative(&ind->step))) {
                    loop->flags |= LOOP_DOWN;
                }
                break;
        }
    } else {
        CError_FATAL(2518);
    }
}

// --- IroType.c ---

// 0x55A660
Type* func_0055A660(void) {
    return CABI_GetPtrDiffTType();
}

extern Boolean func_004CF1B0(Object* func, Object* obj, struct PointsToFunction* ptf);
extern void func_004CE930(
    Object* func, Object* obj, struct PointsToFunction* ptf, IROListNode** list
);

// 0x588A10: the value an induction variable starts the loop with: the right
// side of its initializing assignment, or what pointer analysis knows at the
// end of the preheader
IROLinear* func_00588A10(IRONaturalLoop* loop, IROLoopInd301* ind) {
    IROLinear* result = NULL;
    IROLinear* last;
    IROListNode* list;
    IROListNode* next;

    if (ind->init) {
        return ind->init->u.diadic.right;
    }
    if (loop->preheader &&
        (last = loop->preheader->last) &&
        copts.opt_pointer_analysis &&
        last->pointsToFunction &&
        FunctionName &&
        func_004CF1B0(FunctionName, ind->var->object, last->pointsToFunction)) {
        list = NULL;
        func_004CE930(FunctionName, ind->var->object, last->pointsToFunction, &list);
        if (list) {
            if (list->list.head && list->list.tail && !list->nextList) {
                result = list->list.tail;
            }
            while (list) {
                next = list->nextList;
                IRO_free2(list);
                list = next;
            }
        }
    }
    return result;
}

// 0x588900: what the loop's test compares the induction variable with: the
// other side of a comparison (flag 1: the right one), or for a ++/-- test
// the step it compares against
IROLinear* func_00588900(IRONaturalLoop* loop) {
    IROLinear* cond = loop->cond;
    IROLinear* result = NULL;
    Type* type;
    CInt64 value;

    if (cond->type == IROLinearOp2Arg) {
        return (loop->flags & 1) ? cond->u.diadic.right : cond->u.diadic.left;
    }
    if (cond->type != IROLinearOp1Arg) {
        CError_FATAL(2131);
        return NULL;
    }
    type = IRO_UnqualifiedType(cond->rtype);
    if (type->type == TYPEPOINTER || type->type == TYPEARRAY) {
        CInt64_SetLong(&value, TYPE_POINTER(type)->target->size);
    } else if (type->type == TYPEMEMBERPOINTER) {
        CInt64_SetLong(&value, TYPE_MEMBER_POINTER(type)->ty1->size);
    } else {
        value = cint64_one;
    }
    switch (cond->nodetype) {
        case EPOSTINC:
            result = IRO_NewIntConst(value, cond->rtype);
            break;
        case EPOSTDEC:
            result = IRO_NewIntConst(CInt64_Neg(value), cond->rtype);
            break;
        case EPREINC:
        case EPREDEC:
            result = IRO_NewIntConst(cint64_zero, cond->rtype);
            break;
    }
    return result;
}

extern Boolean data_00625ECA;
extern void IRO_DoTransformations301(IROLinear* first);
extern void IRO_RenumberInts(void);

// 0x587120: the arithmetic transformations on a detached list of linears,
// inside a flowgraph of one node; the list's end moves to its last linear
// that is left
void func_00587120(IROList* list) {
    IROLinearState state;
    IRONode node;
    IRONode* nodeptr;
    IRONode* firstNode;
    IRONode* lastNode;
    IRONode** nodeTable;
    UInt16 numNodes;
    IROLinear* nd;
    IROLinear* last = NULL;

    if (!list || !list->head || !copts.opt_arithtransformation) {
        return;
    }
    for (nd = list->head; nd; nd = nd->next) {
        nd->flags |= IROLF_Reffed;
    }
    func_00561390(&state);
    numNodes = IRO_NumNodes;
    firstNode = IRO_FirstNode;
    lastNode = IRO_LastNode;
    nodeTable = IRO_NodeTable;
    IRO_FirstLinear = list->head;
    IRO_LastLinear = list->tail;
    CurStat = NULL;
    DisableDueToAsm = FALSE;
    data_00625ECA = FALSE;
    IRO_RenumberInts();
    memset(&node, 0, sizeof(node));
    node.first = IRO_FirstLinear;
    node.last = IRO_LastLinear;
    IRO_FirstNode = &node;
    IRO_LastNode = &node;
    IRO_NumNodes = 1;
    nodeptr = &node;
    IRO_NodeTable = &nodeptr;
    node.index = 0;
    node.numsucc = 0;
    node.numpred = 0;
    IRO_DoTransformations301(list->head);
    IRO_NumNodes = numNodes;
    IRO_FirstNode = firstNode;
    IRO_LastNode = lastNode;
    IRO_NodeTable = nodeTable;
    func_00561320(&state);
    for (nd = list->head; nd; nd = nd->next) {
        if (nd->type != IROLinearNop) {
            last = nd;
        }
    }
    list->tail = last;
}

typedef Boolean (*CInt64CompareFunc)(CInt64 a, CInt64 b);

// 0x5868D0: a op b: TRUE or FALSE for two constants, else (with a list) the
// comparison is appended to the list, joined to what is there with &&
Boolean func_005868D0(IROLinear* a, ENodeType op, IROLinear* b, IROList* list) {
    CInt64CompareFunc cmp = NULL;
    CInt64 va;
    CInt64 vb;
    Boolean uns;
    IROList tmp;
    IROLinear* nd;
    IROLinear* land;

    if (IRO_IsIntConstant(a) && IRO_IsIntConstant(b)) {
        va = a->u.node->data.intval;
        vb = b->u.node->data.intval;
        uns = is_unsigned(a->rtype);
        switch (op) {
            case ELESS:
                cmp = uns ? CInt64_LessU : CInt64_Less;
                break;
            case EGREATER:
                cmp = uns ? CInt64_GreaterU : CInt64_Greater;
                break;
            case ELESSEQU:
                cmp = uns ? CInt64_LessEqualU : CInt64_LessEqual;
                break;
            case EGREATEREQU:
                cmp = uns ? CInt64_GreaterEqualU : CInt64_GreaterEqual;
                break;
            case EEQU:
                cmp = CInt64_Equal;
                break;
            case ENOTEQU:
                cmp = CInt64_NotEqual;
                break;
        }
    }
    if (cmp) {
        return cmp(va, vb);
    }
    if (!list) {
        return FALSE;
    }
    IRO_InitList(&tmp);
    nd = IRO_NewLinear(IROLinearOp2Arg);
    nd->index = ++IRO_NumLinear;
    nd->nodetype = op;
    IRO_DuplicateExpr(a, &tmp);
    nd->u.diadic.left = tmp.tail;
    IRO_DuplicateExpr(b, &tmp);
    nd->u.diadic.right = tmp.tail;
    nd->rtype = CParser_GetBoolType();
    if (list && nd) {
        if (!list->head) {
            IRO_DuplicateExpr(nd, list);
        } else {
            land = IRO_NewLinear(IROLinearOp2Arg);
            land->index = ++IRO_NumLinear;
            land->nodetype = ELAND;
            land->u.diadic.left = list->tail;
            land->rtype = IRO_UnqualifiedType(list->tail->rtype);
            IRO_DuplicateExpr(nd, list);
            land->u.diadic.right = list->tail;
            IRO_AddToList(land, list);
        }
    }
    return TRUE;
}

extern Boolean data_00608C98; // set when a value the loop analysis builds may overflow
extern Type* func_0055A650(Type* type);
extern Type* func_0055CA90(Type* type);
extern CInt64 func_0055A710(Type* type);
extern CInt64 func_0055A670(Type* type);
extern void func_0058C530(IROLinear* nd);
IROLinear* func_00585DC0(IROLinear* a, ENodeType op, IROLinear* b, IROList* list);
extern ENodeType ComplementaryOpLogical[IRO_MAXEXPR]; // IroTransform.c

#define SIGN64(v) CInt64_And((v), cint64_min)

// 0x5867A0: a + b in type, noting an overflow
CInt64 func_005867A0(CInt64 a, CInt64 b, Type* type, Boolean* overflow) {
    CInt64 r = CInt64_Add(a, b);

    IRO_TruncateValueToType(&r, type);
    if (is_unsigned(type)) {
        if (CInt64_LessU(r, b) || CInt64_LessU(r, a)) {
            *overflow = TRUE;
        }
    } else if (CInt64_Equal(SIGN64(a), SIGN64(b)) && CInt64_NotEqual(SIGN64(r), SIGN64(a))) {
        *overflow = TRUE;
    }
    return r;
}

// 0x586640: a - b in type, noting an overflow
CInt64 func_00586640(CInt64 a, CInt64 b, Type* type, Boolean* overflow) {
    CInt64 r = CInt64_Sub(a, b);

    IRO_TruncateValueToType(&r, type);
    if (is_unsigned(type)) {
        if (CInt64_LessU(a, b)) {
            *overflow = TRUE;
        }
    } else if (CInt64_Equal(b, func_0055A710(type))) {
        *overflow = TRUE;
    } else {
        b = CInt64_Neg(b);
        IRO_TruncateValueToType(&b, type);
        if (CInt64_Equal(SIGN64(a), SIGN64(b)) && CInt64_NotEqual(SIGN64(r), SIGN64(a))) {
            *overflow = TRUE;
        }
    }
    return r;
}

typedef CInt64 (*CInt64OpFunc)(CInt64 a, CInt64 b);
typedef CInt64 (*CInt64CheckedFunc)(CInt64 a, CInt64 b, Type* type, Boolean* overflow);

// a cast of the list's last linear to type, when it differs
static inline void CastTail(IROList* list, Type* type, int line) {
    IROLinear* cast;

    if (type && list->tail->rtype != type && !IRO_TypesEqual(list->tail->rtype, type)) {
        cast = IRO_NewLinear(IROLinearOp1Arg);
        cast->nodetype = ETYPCON;
        cast->u.monadic = list->tail;
        cast->rtype = type;
        if (type->type == TYPEINT && TYPE_INTEGRAL(type)->integral == IT_BOOL) {
            CError_FATAL(line);
        }
        IRO_AddToList(cast, list);
    }
}

// the condition that a op b (+ or -, signed) does not overflow, given
// r = a op b and nb the second operand as added: sign(a) != sign(nb) ||
// sign(a) == sign(r)
static inline Boolean NoSignedOverflow(
    IROLinear* a, IROLinear* nb, IROLinear* r, IROLinear* min, IROList* list
) {
    IROLinear* t1 = func_00585DC0(r, EAND, min, list);
    IROLinear* t2 = func_00585DC0(a, EAND, min, list);
    IROLinear* same = func_00585DC0(t2, EEQU, t1, list);
    IROLinear* v1 = func_00585DC0(nb, EAND, min, list);
    IROLinear* v2 = func_00585DC0(a, EAND, min, list);
    IROLinear* diff = func_00585DC0(v2, ENOTEQU, v1, list);
    return func_005868D0(diff, ELOR, same, list);
}

// 0x585DC0: a op b for the loop analysis: a constant when both are, else a
// new linear (with casts to the result type) whose overflow conditions for
// + and - are added to list; data_00608C98 is set when they cannot hold
IROLinear* func_00585DC0(IROLinear* a, ENodeType op, IROLinear* b, IROList* list) {
    Boolean bothPtr = FALSE;
    Boolean uns;
    Boolean anyPtr = FALSE;
    Boolean cmpUnsigned;
    Type* ptrType;
    Type* type;
    Type* castType;
    CInt64OpFunc arith = NULL;
    CInt64CheckedFunc checked = NULL;
    CInt64CompareFunc cmp = NULL;
    CInt64 v1;
    CInt64 v2;
    IROList tmp;
    IROLinear* r;
    IROLinear* min;
    IROLinear* nb;
    IROLinear* nd;

    if (a->rtype->type == TYPEPOINTER) {
        anyPtr = TRUE;
        ptrType = IRO_UnqualifiedType(a->rtype);
    }
    if (b->rtype->type == TYPEPOINTER) {
        if (anyPtr) {
            bothPtr = TRUE;
        } else {
            anyPtr = TRUE;
        }
        ptrType = IRO_UnqualifiedType(b->rtype);
    }
    if (ComplementaryOpLogical[op] != IRO_MAXEXPR || op == ELAND || op == ELOR) {
        type = CParser_GetBoolType();
        cmpUnsigned = is_unsigned(a->rtype);
    } else if (bothPtr && op == ESUB) {
        type = func_0055A660();
    } else if (anyPtr && (op == EADD || op == ESUB)) {
        type = ptrType;
    } else {
        type = IRO_UnqualifiedType(a->rtype->size >= b->rtype->size ? a->rtype : b->rtype);
        if ((is_unsigned(a->rtype) || is_unsigned(b->rtype)) && !is_unsigned(type)) {
            type = func_0055CA90(type);
        }
    }
    uns = is_unsigned(type);

    if (IRO_IsIntConstant(a) && IRO_IsIntConstant(b)) {
        v1 = a->u.node->data.intval;
        v2 = b->u.node->data.intval;
        switch (op) {
            case EMUL:
                arith = uns ? CInt64_MulU : CInt64_Mul;
                break;
            case EDIV:
                arith = uns ? CInt64_DivU : CInt64_Div;
                break;
            case EMODULO:
                arith = uns ? CInt64_ModU : CInt64_Mod;
                break;
            case EADDV:
                checked = func_005867A0;
                break;
            case ESUBV:
                checked = func_00586640;
                break;
            case EADD:
                arith = CInt64_Add;
                break;
            case ESUB:
                arith = CInt64_Sub;
                break;
            case ELESS:
                cmp = cmpUnsigned ? CInt64_LessU : CInt64_Less;
                break;
            case EGREATER:
                cmp = cmpUnsigned ? CInt64_GreaterU : CInt64_Greater;
                break;
            case ELESSEQU:
                cmp = cmpUnsigned ? CInt64_LessEqualU : CInt64_LessEqual;
                break;
            case EGREATEREQU:
                cmp = cmpUnsigned ? CInt64_GreaterEqualU : CInt64_GreaterEqual;
                break;
            case EEQU:
                cmp = CInt64_Equal;
                break;
            case ENOTEQU:
                cmp = CInt64_NotEqual;
                break;
            case EAND:
                arith = CInt64_And;
                break;
        }
    }
    if (cmp) {
        return IRO_NewIntConst(cmp(v1, v2) ? cint64_one : cint64_zero, type);
    }
    if (checked) {
        IRO_TruncateValueToType(&v1, type);
        IRO_TruncateValueToType(&v2, type);
        return IRO_NewIntConst(checked(v1, v2, type, &data_00608C98), type);
    }
    if (arith) {
        IRO_TruncateValueToType(&v1, type);
        IRO_TruncateValueToType(&v2, type);
        return IRO_NewIntConst(arith(v1, v2), type);
    }

    IRO_InitList(&tmp);
    if (op == EADDV || op == ESUBV) {
        op = (op == EADDV) ? EADD : ESUB;
        r = func_00585DC0(a, op, b, list);
        if (op == EADD) {
            if (!IRO_IsConstantZero(a) && !IRO_IsConstantZero(b)) {
                if (uns) {
                    if (!func_005868D0(r, EGREATEREQU, a, list) ||
                        !func_005868D0(r, EGREATEREQU, b, list)) {
                        data_00608C98 = TRUE;
                    }
                } else {
                    min = IRO_NewIntConst(func_0055A710(type), type);
                    if (!NoSignedOverflow(a, b, r, min, list)) {
                        data_00608C98 = TRUE;
                    }
                }
            }
        } else if (!IRO_IsConstantZero(b)) {
            if (uns) {
                if (!func_005868D0(a, EGREATEREQU, b, list)) {
                    data_00608C98 = TRUE;
                }
            } else {
                min = IRO_NewIntConst(func_0055A710(type), type);
                if (!func_005868D0(b, ENOTEQU, min, list)) {
                    data_00608C98 = TRUE;
                } else {
                    if (IRO_IsIntConstant(b)) {
                        nb = IRO_NewIntConst(CInt64_Neg(b->u.node->data.intval), type);
                    } else {
                        nb = IRO_NewLinear(IROLinearOp1Arg);
                        nb->index = ++IRO_NumLinear;
                        nb->nodetype = EMONMIN;
                        nb->rtype = type;
                        nb->u.monadic = b;
                    }
                    if (!NoSignedOverflow(a, nb, r, min, list)) {
                        data_00608C98 = TRUE;
                    }
                }
            }
        }
    }

    nd = IRO_NewLinear(IROLinearOp2Arg);
    nd->index = ++IRO_NumLinear;
    nd->nodetype = op;
    nd->rtype = type;
    IRO_DuplicateExpr(a, &tmp);
    castType = NULL;
    if (ComplementaryOpLogical[op] == IRO_MAXEXPR && op != ELAND && op != ELOR) {
        if (!anyPtr) {
            castType = type;
        } else if (!bothPtr && a->rtype->type != TYPEPOINTER) {
            castType = func_0055A650(ptrType);
        }
    }
    CastTail(&tmp, castType, 4025);
    nd->u.diadic.left = tmp.tail;
    IRO_DuplicateExpr(b, &tmp);
    castType = NULL;
    if (ComplementaryOpLogical[op] == IRO_MAXEXPR && op != ELAND && op != ELOR) {
        if (!anyPtr) {
            castType = type;
        } else if (!bothPtr && b->rtype->type != TYPEPOINTER) {
            castType = func_0055A650(ptrType);
        }
    }
    CastTail(&tmp, castType, 4043);
    nd->u.diadic.right = tmp.tail;
    func_0058C530(nd);
    return nd;
}

// 0x585D30: the condition that the loop, whose test is cond and whose
// counter moves by step, runs at all when the counter starts at a and the
// bound is b
Boolean func_00585D30(IROLinear* cond, CInt64 step, IROLinear* a, IROLinear* b, IROList* list) {
    switch (cond->nodetype) {
        case EPOSTINC:
        case EPREINC:
            return func_005868D0(a, EGREATEREQU, b, list);
        case EPOSTDEC:
        case EPREDEC:
            return func_005868D0(a, ELESSEQU, b, list);
        case ELESS:
        case ELESSEQU:
            return !CInt64_IsNegative(&step);
        case EGREATER:
        case EGREATEREQU:
            return CInt64_IsNegative(&step);
        case EEQU:
        case ENOTEQU:
            if (CInt64_IsNegative(&step)) {
                return func_005868D0(a, ELESSEQU, b, list);
            } else {
                return func_005868D0(a, EGREATEREQU, b, list);
            }
        default:
            return FALSE;
    }
}

// the linears from scan to the end of node: TRUE when one kills a variable
// in depends
static inline Boolean KillsDepends(IROLinear* scan, IRONode* node, BitVector* depends) {
    IROLinear* end;
    Boolean killed = FALSE;

    if (scan && node->last && (end = node->last->next)) {
        while (scan != end && !killed) {
            IRO_GetKills(scan);
            Bv_And(depends, IRO_VarKills);
            killed = !Bv_IsEmpty(IRO_VarKills);
            scan = scan->next;
        }
    }
    return killed;
}

// 0x586F30: TRUE when, between the counter's initial assignment and the
// loop (following single successors outside it), something changes a
// variable nd reads
Boolean func_00586F30(IRONaturalLoop* loop, IROLinear* nd) {
    BitVector* savedKills = IRO_VarKills;
    BitVector* savedDepends;
    IROLoopInd301* ind;
    IRONode* node;
    Boolean killed;

    Bv_AllocVector(&IRO_VarKills, IRO_NumVars + 1);
    savedDepends = IRO_Depends;
    Bv_AllocVector(&IRO_Depends, IRO_NumVars + 1);
    func_0056AB70(nd, FALSE, FALSE, TRUE);
    ind = loop->induction;
    node = ind->initNode;
    killed = KillsDepends(ind->init->next, node, IRO_Depends);
    while (node && !killed) {
        if (node->numsucc == 1 &&
            !Bv_IsBitSet(node->succ[0], loop->nodes) &&
            (node = IRO_NodeTable[node->succ[0]])) {
            killed = KillsDepends(node->first, node, IRO_Depends);
        } else {
            node = NULL;
        }
    }
    FREE_BV(IRO_Depends);
    IRO_Depends = savedDepends;
    FREE_BV(IRO_VarKills);
    IRO_VarKills = savedKills;
    return killed;
}

static inline void AddOperand301(IROLinear* nd, IROAddrRecord301* rec) {
    if (nd->type == IROLinearOp2Arg && nd->nodetype == EADD) {
        func_005510D0(nd, rec, IRO_Alloc);
    } else if (IRO_IsIntConstant(nd)) {
        rec->numInts++;
        func_005512E0(nd, &rec->ints, IRO_Alloc);
        rec->numObjRefs = 0;
        rec->numMisc = 0;
    } else if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
        rec->numObjRefs++;
        func_005512E0(nd, &rec->objRefs, IRO_Alloc);
        rec->numMisc = 0;
        rec->numInts = 0;
    } else {
        rec->numMisc++;
        func_005512E0(nd, &rec->misc, IRO_Alloc);
        rec->numObjRefs = 0;
        rec->numInts = 0;
    }
}

static inline IROLinear* StripConversions301(IROLinear* nd) {
    while (nd->type == IROLinearOp1Arg && nd->nodetype == ETYPCON && func_0055A3F0(nd)) {
        nd = nd->u.monadic;
    }
    return nd;
}

// the terms of two lists are the same, pairwise
static inline Boolean SameTerms301(IROElmList* la, IROElmList* lb) {
    IROLinear* x;
    IROLinear* y;

    while (la && lb) {
        x = StripConversions301(la->element);
        y = StripConversions301(lb->element);
        if (!IRO_ExprsSame(x, y)) {
            return FALSE;
        }
        la = la->next;
        lb = lb->next;
    }
    return TRUE;
}

static inline CInt64 SumInts301(IROElmList* list) {
    CInt64 sum = cint64_zero;
    IROLinear* nd;

    for (; list; list = list->next) {
        nd = list->element;
        sum = CMach_CalcIntDiadic(nd->rtype, sum, '+', nd->u.node->data.intval);
    }
    return sum;
}

// 0x586AB0: a and b are sums that differ by a constant: *diff = b - a, unless
// reject(b, a)
Boolean func_00586AB0(IROLinear* a, IROLinear* b, CInt64CompareFunc reject, CInt64* diff) {
    IROAddrRecord301* ra = IRO_Alloc(sizeof(IROAddrRecord301));
    IROAddrRecord301* rb = IRO_Alloc(sizeof(IROAddrRecord301));
    CInt64 va;
    CInt64 vb;

    func_005513A0(ra, a);
    func_005513A0(rb, b);
    AddOperand301(a, ra);
    AddOperand301(b, rb);
    if (ra->numObjRefs == rb->numObjRefs && rb->numObjRefs > 0) {
        if (!SameTerms301(ra->objRefs, rb->objRefs)) {
            return FALSE;
        }
    } else if (ra->numObjRefs != rb->numObjRefs) {
        return FALSE;
    }
    if (ra->numMisc == rb->numMisc && rb->numMisc > 0) {
        if (!SameTerms301(ra->misc, rb->misc)) {
            return FALSE;
        }
    } else if (ra->numMisc != rb->numMisc) {
        return FALSE;
    }
    va = SumInts301(ra->ints);
    vb = SumInts301(rb->ints);
    if (reject && reject(vb, va)) {
        return FALSE;
    }
    *diff = CInt64_Sub(vb, va);
    return TRUE;
}

// nd stays in its type when the counter moves by step once more
static inline Boolean StepFits(IROLinear* nd, CInt64 step, IROList* list) {
    Boolean down = CInt64_IsNegative(&step);
    CInt64 v;

    v = down ? func_0055A710(nd->rtype) : func_0055A670(nd->rtype);
    v = CInt64_Sub(v, step);
    IRO_TruncateValueToType(&v, nd->rtype);
    return func_005868D0(nd, down ? EGREATEREQU : ELESSEQU, IRO_NewIntConst(v, nd->rtype), list);
}

// a constant count from two sums that differ by a constant d (b - a)
static inline IROLinear* ConstCount(
    IRONaturalLoop* loop,
    IROLinear* a,
    IROLinear* b,
    CInt64* d,
    Boolean negate,
    Boolean incl,
    Type* type
) {
    if (negate) {
        *d = CInt64_Neg(*d);
    }
    if (incl) {
        *d = CInt64_Add(*d, cint64_one);
    }
    IRO_TruncateValueToType(d, type);
    return IRO_NewIntConst(*d, type);
}

// 0x584E20: the loop's count numerator, step' + (b - a) (or a - b), where
// step' is |step| (less one when the test is strict), with the conditions
// it needs added to list; NULL when it cannot be had. a is the counter's
// start, b and x the bounds the test uses
IROLinear* func_00584E20(
    IRONaturalLoop* loop,
    IROLinear* a,
    IROLinear* b,
    IROLinear* x,
    CInt64 step,
    Boolean flag,
    IROList* list,
    Type* type
) {
    Type* btype;
    Boolean uns;
    Boolean neg;
    Boolean incl;
    Boolean notNE;
    Boolean bothConst;
    Boolean runs;
    CInt64 absstep;
    CInt64 d;
    ENodeType op;
    IROLinear* result;
    IROLinear* zero;
    IROLinear* one;
    IROLinear* stepNd;
    IROLinear* maxNd;
    IROLinear* minNd;
    IROLinear* t;

    type = IRO_UnqualifiedType(type);
    btype = IRO_UnqualifiedType(b->rtype);
    uns = is_unsigned(btype);
    neg = CInt64_IsNegative(&step);
    absstep = neg ? CInt64_Neg(step) : step;
    result = NULL;
    op = loop->cond->nodetype;
    switch (op) {
        case ELESSEQU:
        case EGREATEREQU:
        case EEQU:
            incl = TRUE;
            break;
        default:
            incl = FALSE;
            break;
    }
    notNE = !(op <= EINDIRECT || op == ENOTEQU);
    bothConst = IRO_IsIntConstant(b) && IRO_IsIntConstant(a);
    zero = IRO_NewIntConst(cint64_zero, type);
    one = IRO_NewIntConst(cint64_one, type);
    stepNd = IRO_NewIntConst(absstep, type);

    if (uns) {
        runs = func_00585D30(loop->cond, step, a, b, list);
        maxNd = IRO_NewIntConst(func_0055A670(btype), btype);
        minNd = IRO_NewIntConst(func_0055A710(btype), btype);
        if (neg) {
            if (runs && func_005868D0(b, EGREATEREQU, a, list)) {
                if (!incl || func_005868D0(a, EGREATER, minNd, list)) {
                    if (notNE) {
                        if (!incl) {
                            stepNd = func_00585DC0(stepNd, ESUB, one, list);
                        }
                        result =
                            func_00585DC0(stepNd, EADDV, func_00585DC0(b, ESUB, a, list), list);
                    } else {
                        result = func_00585DC0(b, ESUB, a, list);
                    }
                }
            } else if (bothConst) {
                if (!notNE) {
                    t = func_00585DC0(func_00585DC0(b, ESUB, minNd, list), EADDV, one, list);
                    result = func_00585DC0(func_00585DC0(maxNd, ESUB, a, list), EADDV, t, list);
                } else if (runs || !func_005868D0(b, incl ? ELESSEQU : ELESS, a, list)) {
                    result = zero;
                } else if (func_005868D0(
                               func_00585DC0(maxNd, EADD, stepNd, list), EGREATER, a, list
                           )) {
                    result =
                        func_00585DC0(func_00585DC0(b, EADD, minNd, list), EADDV, stepNd, list);
                }
            } else if (!notNE && func_00586AB0(b, a, NULL, &d) && !func_00586F30(loop, b)) {
                result = ConstCount(loop, a, b, &d, TRUE, FALSE, type);
            }
        } else {
            if (runs && func_005868D0(b, ELESSEQU, a, list)) {
                if (!incl || func_005868D0(a, ELESS, maxNd, list)) {
                    if (notNE) {
                        if (!incl) {
                            stepNd = func_00585DC0(stepNd, ESUB, one, list);
                        }
                        result =
                            func_00585DC0(stepNd, EADDV, func_00585DC0(a, ESUB, b, list), list);
                    } else {
                        result = func_00585DC0(a, ESUB, b, list);
                    }
                }
            } else if (bothConst) {
                if (!notNE) {
                    t = func_00585DC0(func_00585DC0(a, ESUB, minNd, list), EADDV, one, list);
                    result = func_00585DC0(func_00585DC0(maxNd, ESUB, b, list), EADDV, t, list);
                } else if (runs || !func_005868D0(b, incl ? EGREATEREQU : EGREATER, a, list)) {
                    result = zero;
                } else if (func_005868D0(stepNd, ELESSEQU, a, list)) {
                    result =
                        func_00585DC0(func_00585DC0(maxNd, ESUB, b, list), EADDV, stepNd, list);
                }
            } else if (!notNE && func_00586AB0(b, a, NULL, &d) && !func_00586F30(loop, b)) {
                result = ConstCount(loop, a, b, &d, FALSE, FALSE, type);
            }
        }
    } else {
        runs = FALSE;
        if (func_00585D30(loop->cond, step, a, x, list) &&
            (flag || func_00585D30(loop->cond, step, a, b, list))) {
            runs = TRUE;
        }
        if (neg) {
            if (runs && func_005868D0(b, EGREATEREQU, a, list)) {
                if (StepFits(a, step, list) &&
                    StepFits(x, step, list) &&
                    (flag || StepFits(b, step, list))) {
                    if (notNE) {
                        if (!incl) {
                            stepNd = func_00585DC0(stepNd, ESUB, one, list);
                        }
                        result =
                            func_00585DC0(stepNd, EADDV, func_00585DC0(b, ESUBV, a, list), list);
                    } else {
                        result = func_00585DC0(b, ESUBV, a, list);
                    }
                }
            } else if (bothConst) {
                if (runs || func_005868D0(b, incl ? EGREATER : EGREATEREQU, a, list)) {
                    result = zero;
                }
            } else if (runs) {
                if (func_00586AB0(b, a, CInt64_Greater, &d)) {
                    if (!func_00586F30(loop, b)) {
                        result = ConstCount(loop, a, b, &d, TRUE, incl, type);
                    }
                } else if (func_00586AB0(b, a, CInt64_LessEqual, &d) && !func_00586F30(loop, b)) {
                    result = zero;
                }
            }
        } else {
            if (runs && func_005868D0(b, ELESSEQU, a, list)) {
                if (StepFits(a, step, list) &&
                    StepFits(x, step, list) &&
                    (flag || StepFits(b, step, list))) {
                    if (notNE) {
                        if (!incl) {
                            stepNd = func_00585DC0(stepNd, ESUB, one, list);
                        }
                        result =
                            func_00585DC0(stepNd, EADDV, func_00585DC0(a, ESUBV, b, list), list);
                    } else {
                        result = func_00585DC0(a, ESUBV, b, list);
                    }
                }
            } else if (bothConst) {
                if (runs || func_005868D0(b, incl ? ELESS : ELESSEQU, a, list)) {
                    result = zero;
                }
            } else if (runs) {
                if (func_00586AB0(b, a, CInt64_Less, &d)) {
                    if (!func_00586F30(loop, b)) {
                        result = ConstCount(loop, a, b, &d, FALSE, incl, type);
                    }
                } else if (func_00586AB0(b, a, CInt64_GreaterEqual, &d) &&
                           !func_00586F30(loop, b)) {
                    result = zero;
                }
            }
        }
    }
    return result;
}

extern IROLinear* IRO_MakeTypeConversion(IROLinear* linear, Type* type, IROList* list);

// a + d (d a constant of dtype) as a new folded linear
static inline IROLinear* AddConst584670(IROLinear* a, CInt64 d, Type* dtype) {
    IROList list;
    IROLinear* nd;

    IRO_InitList(&list);
    nd = IRO_NewLinear(IROLinearOp2Arg);
    nd->index = ++IRO_NumLinear;
    nd->nodetype = EADD;
    IRO_DuplicateExpr(a, &list);
    nd->u.diadic.left = list.tail;
    IRO_AddToList(IRO_NewIntConst(d, dtype), &list);
    nd->u.diadic.right = list.tail;
    nd->rtype = IRO_UnqualifiedType(a->rtype);
    func_0058C530(nd);
    return nd;
}

// 0x584670: the loop's trip count, a constant (loop->count) or an
// expression (loop->countExpr), with the conditions it needs in
// loop->countConds when withConds; TRUE when known
Boolean func_00584670(IRONaturalLoop* loop, Boolean withConds) {
    IROList* conds;
    IRONode* node;
    IRONode* succ;
    IRONode* found;
    IROLoopInd301* ind;
    IROLinear* cond;
    IROLinear* init;
    IROLinear* start;
    IROLinear* bound;
    IROLinear* limit;
    IROLinear* count;
    IROLinear* stepNd;
    IROLinear* zeroNd;
    IROLinear* oneNd;
    IROList list;
    Type* countType;
    Type* limitType;
    CInt64 step;
    CInt64 d;
    UInt16 i;
    UInt16 n;
    int numFound;
    Boolean headerTest;
    Boolean tailTest;
    Boolean ok;
    ENodeType op;

    data_00608C98 = FALSE;
    loop->hasCount = FALSE;
    loop->count = cint64_negone;
    IRO_InitList(&loop->countExpr);
    IRO_InitList(&loop->countConds);
    conds = withConds ? &loop->countConds : NULL;

    found = NULL;
    numFound = 0;
    node = loop->x10;
    n = node->numsucc;
    for (i = 0; i < n; i++) {
        succ = IRO_NodeTable[node->succ[i]];
        if (Bv_IsBitSet(succ->index, loop->nodes)) {
            numFound++;
            found = succ;
        }
    }
    if (numFound != 1) {
        found = NULL;
    }
    loop->x20 = found;

    if (loop->flags & 0x200000) {
        tailTest = FALSE;
        headerTest = FALSE;
    } else {
        headerTest = loop->x10 == loop->header && loop->x10 != loop->x20;
        tailTest = !headerTest;
    }

    if ((tailTest || headerTest) &&
        (ind = loop->induction) &&
        ind->fnode->mustreach1 &&
        (cond = loop->cond)) {
        ok = FALSE;
        if (cond &&
            (cond->type == IROLinearOp1Arg || cond->type == IROLinearOp2Arg) &&
            (cond->nodetype <= EINDIRECT ||
             (cond->nodetype >= ELESS && cond->nodetype <= ENOTEQU))) {
            ok = TRUE;
        }
        if (ok && (init = func_00588A10(loop, ind)) && !IRO_HasSideEffect(init)) {
            start = init;
            cond = loop->cond;
            if (cond->type == IROLinearOp2Arg) {
                bound = (loop->flags & 1) ? cond->u.diadic.left : cond->u.diadic.right;
            } else if (cond->type == IROLinearOp1Arg) {
                bound = cond->u.monadic;
            } else {
                CError_FATAL(2160);
            }
            if (bound &&
                (IRO_IsVariable(bound) ||
                 (bound->type == IROLinearOp1Arg &&
                  bound->nodetype == ETYPCON &&
                  func_0055A3F0(bound) &&
                  IRO_IsVariable(bound->u.monadic))) &&
                !IRO_HasSideEffect(bound) &&
                (limit = func_00588900(loop)) &&
                (IRO_IsIntConstant(limit) || (limit->flags & IROLF_LoopInvariant)) &&
                !IRO_HasSideEffect(limit) &&
                limit->rtype->type == init->rtype->type) {
                step = ind->step;
                d = step;
                if (ind->flags & 4) {
                    d = CInt64_Neg(step);
                }
                if (!CInt64_IsZero(&d)) {
                    if (init->rtype->type == TYPEPOINTER && limit->rtype->type == TYPEPOINTER) {
                        limitType = func_0055A660();
                        countType = limitType;
                    } else {
                        countType = IRO_UnqualifiedType(init->rtype);
                        limitType = IRO_UnqualifiedType(limit->rtype);
                    }
                    if (tailTest) {
                        init = AddConst584670(init, d, countType);
                    } else if (headerTest &&
                               loop->cond->type == IROLinearOp1Arg &&
                               loop->cond->nodetype <= EPREDEC) {
                        limit = AddConst584670(limit, CInt64_Neg(d), limitType);
                    }
                    if (IRO_TypesEqual(limit->rtype, init->rtype) ||
                        (func_005868D0(
                             limit,
                             EGREATEREQU,
                             IRO_NewIntConst(func_0055A710(init->rtype), init->rtype),
                             conds
                         ) &&
                         func_005868D0(
                             limit,
                             ELESSEQU,
                             IRO_NewIntConst(func_0055A670(init->rtype), init->rtype),
                             conds
                         ))) {
                        if (!IRO_TypesEqual(limit->rtype, init->rtype)) {
                            IRO_InitList(&list);
                            IRO_DuplicateExpr(limit, &list);
                            limit = IRO_MakeTypeConversion(list.tail, init->rtype, &list);
                            func_0058C530(limit);
                        }
                        count = func_00584E20(
                            loop, limit, init, start, d, headerTest, conds, countType
                        );
                        if (count) {
                            stepNd = IRO_NewIntConst(step, countType);
                            op = loop->cond->nodetype;
                            ok = TRUE;
                            if (op <= EINDIRECT || op == ENOTEQU) {
                                zeroNd = IRO_NewIntConst(cint64_zero, countType);
                                ok = func_005868D0(
                                    func_00585DC0(count, EMODULO, stepNd, conds),
                                    EEQU,
                                    zeroNd,
                                    conds
                                );
                            }
                            if (ok) {
                                if (tailTest) {
                                    oneNd = IRO_NewIntConst(cint64_one, countType);
                                    count = func_00585DC0(
                                        func_00585DC0(count, EDIV, stepNd, conds),
                                        EADDV,
                                        oneNd,
                                        conds
                                    );
                                } else {
                                    count = func_00585DC0(count, EDIV, stepNd, conds);
                                }
                                if (!data_00608C98) {
                                    if (IRO_IsIntConstant(count)) {
                                        loop->count = count->u.node->data.intval;
                                    } else {
                                        IRO_DuplicateExpr(count, &loop->countExpr);
                                    }
                                    loop->hasCount = TRUE;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (loop->hasCount) {
        func_00587120(&loop->countExpr);
        func_00587120(conds);
    }
    if (loop->hasCount && CInt64_Greater(loop->count, cint64_zero) && !loop->countConds.tail) {
        for (node = IRO_FirstNode; node; node = node->nextnode) {
            if (Bv_IsBitSet(node->index, loop->nodes) && node->mustreach1) {
                node->unk3F = TRUE;
            }
        }
    }
    return loop->hasCount;
}

extern ENodeType ComplementaryOp[IRO_MAXEXPR]; // IroTransform.c
extern IROLinear* func_00552E90(IROLinear* nd); // IroSimplifyIfThenElse.c
extern Boolean IsSubableOp[IRO_MAXEXPR]; // IroSubable.c
Boolean func_0058A4F0(IRONaturalLoop* loop);
extern IROLinear* func_00554640(IROLinear* nd, Boolean toBool);
extern void func_005530D0(IROLinear* linear, IROWalkTreeArgFunc func, void* arg);

static inline Boolean InElmList(IROElmList* list, IROLinear* nd) {
    for (; list; list = list->next) {
        if (list->element == nd) {
            return TRUE;
        }
    }
    return FALSE;
}

static inline void PushElm(IROElmList** list, IROLinear* nd) {
    IROElmList* elm = IRO_malloc(sizeof(IROElmList));

    elm->element = nd;
    elm->next = *list;
    *list = elm;
}

static inline void FreeElms(IROElmList** list) {
    IROElmList* elm;
    IROElmList* next;

    if (*list) {
        for (elm = *list; elm; elm = next) {
            next = elm->next;
            IRO_free(elm);
        }
        *list = NULL;
    }
}

// 0x588850: walker callback, the linears of the loop test
void func_00588850(IROLinear* nd, Boolean isFirst, void* arg) {
    if (!isFirst) {
        PushElm(&((IRONaturalLoop*)arg)->x28, nd);
    }
}

// 0x588880: walker callback, the linears of the induction updates
void func_00588880(IROLinear* nd, Boolean isFirst, void* arg) {
    if (!isFirst) {
        PushElm(&((IRONaturalLoop*)arg)->x30, nd);
    }
}

static inline Boolean InLoop301(IRONaturalLoop* loop, IRONode* node) {
    return Bv_IsBitSet(node->index, loop->nodes);
}

// the If ending node, whose fall-through leaves the loop
static inline Boolean IsExitTest(IRONaturalLoop* loop, IRONode* node) {
    return (node->last->type == IROLinearIf || node->last->type == IROLinearIfNot) &&
        node->nextnode &&
        !InLoop301(loop, node->nextnode);
}

static inline IROLoopInd301* FindInd301(IRONaturalLoop* loop, VarRecord* var) {
    IROLoopInd301* ind;

    for (ind = loop->inductions; ind; ind = ind->next) {
        if (ind->var == var) {
            break;
        }
    }
    return ind;
}

static inline IROLinear* VarOperand301(IROLinear* nd) {
    if (IRO_IsVariable(nd)) {
        return nd;
    }
    if (nd->type == IROLinearOp1Arg && nd->nodetype == ETYPCON && IRO_IsVariable(nd->u.monadic)) {
        return nd->u.monadic;
    }
    return NULL;
}

static inline Boolean IsSmallInt(Type* type) {
    return type->type == TYPEINT && TYPE_INTEGRAL(type)->integral < IT_17;
}

// 0x587540: what the loop is: its test, counter, trip count and the
// properties the loop transformations check (loop->flags)
void func_00587540(IRONaturalLoop* loop) {
    Boolean hasCall = FALSE;
    Boolean hasCtrl = FALSE;
    UInt8 numGotos = 0;
    int numAssigns = 0;
    IRONode* testNode;
    IRONode* node;
    IROLinear* cond;
    IROLinear* nd;
    IROLinear* zero;
    IROLinear* left;
    IROLinear* right;
    IROLinear* indNd;
    IROLoopInd301* ind;
    VarRecord* var;
    Object* obj;
    CInt64 saved;
    UInt16 i;
    UInt16 succ;
    UInt32 flags;

    loop->flags = 0;
    loop->induction = NULL;
    loop->x50 = 0;
    func_00591FB0(loop);
    func_00588CC0(loop);
    func_0058B050(loop);
    func_0058A4F0(loop);
    func_0058B3D0(loop);
    func_0058B250(loop);

    testNode = loop->header;
    if (IsExitTest(loop, loop->header)) {
        testNode = loop->header;
    } else if (loop->numTails == 1 && IsExitTest(loop, loop->tail)) {
        testNode = loop->tail;
    } else {
        loop->flags |= 0x200000;
    }
    if (testNode->last->type == IROLinearIfNot) {
        testNode->last->type = IROLinearIf;
        testNode->last->u.label.cond = func_00554640(testNode->last->u.label.cond, TRUE);
    }
    loop->x10 = testNode;
    cond = (testNode->last->type == IROLinearIf) ? testNode->last->u.label.cond : NULL;

    // while (*p) becomes while (*p != 0)
    if (cond &&
        cond->type == IROLinearOp1Arg &&
        cond->nodetype == EINDIRECT &&
        IsIntOrPointer(cond->rtype)) {
        nd = IRO_NewLinear(IROLinearOp2Arg);
        nd->type = IROLinearOp2Arg;
        nd->flags = cond->flags;
        nd->index = ++IRO_NumLinear;
        nd->nodetype = ENOTEQU;
        nd->rtype = CParser_GetBoolType();
        nd->u.diadic.left = cond;
        zero = IRO_NewIntConst(cint64_zero, cond->rtype);
        nd->u.diadic.right = zero;
        zero->flags = cond->flags;
        zero->next = nd;
        IRO_PasteAfter(zero, nd, cond);
        testNode->last->u.label.cond = nd;
        cond = nd;
    }
    loop->cond = cond;

    if (cond && cond->type == IROLinearOp2Arg && IsSmallInt(cond->rtype)) {
        left = cond->u.diadic.left;
        right = cond->u.diadic.right;
        if ((left = VarOperand301(left)) &&
            (var = IRO_FindVar(left->u.monadic->u.node->data.objref, FALSE, TRUE)) &&
            (ind = FindInd301(loop, var))) {
            loop->flags |= 1;
            loop->induction = ind;
        }
        if ((right = VarOperand301(right)) &&
            (var = IRO_FindVar(right->u.monadic->u.node->data.objref, FALSE, TRUE)) &&
            (ind = FindInd301(loop, var))) {
            loop->flags &= ~1;
            loop->induction = ind;
        }
        // the counter on the left
        if (loop->induction &&
            !(loop->flags & 1) &&
            ComplementaryOpLogical[cond->nodetype] != IRO_MAXEXPR) {
            left = cond->u.diadic.left;
            cond->u.diadic.left = cond->u.diadic.right;
            cond->u.diadic.right = left;
            if (!IsSubableOp[cond->nodetype]) {
                cond->nodetype = ComplementaryOp[cond->nodetype];
                loop->flags |= 1;
            }
        }
        if (!loop->induction) {
            loop->flags |= 0x10;
        }
        if ((ind = loop->induction)) {
            indNd = ind->nd;
            func_00588AE0(loop, ind);
            func_00588260(loop);
            func_00584670(loop, FALSE);
            // i != n counted as i < n (or i > n) when that gives the same count
            if (loop->hasCount && (loop->flags & (0x200 | 0x2000)) && cond->nodetype == ENOTEQU) {
                cond->nodetype = (loop->flags & 0x200) ? ELESS : EGREATER;
                saved = loop->count;
                func_00584670(loop, FALSE);
                if (!loop->hasCount || !CInt64_Equal(loop->count, saved)) {
                    cond->nodetype = ENOTEQU;
                    loop->count = saved;
                    loop->hasCount = TRUE;
                }
            }
            if (!(cond->type == IROLinearOp2Arg &&
                  (cond->nodetype == ELESS ||
                   cond->nodetype == ELESSEQU ||
                   cond->nodetype == EGREATER ||
                   cond->nodetype == EGREATEREQU) &&
                  cond->u.diadic.left &&
                  cond->u.diadic.left->rtype &&
                  IsIntOrPointer(cond->u.diadic.left->rtype) &&
                  cond->u.diadic.right &&
                  cond->u.diadic.right->rtype &&
                  IsIntOrPointer(cond->u.diadic.right->rtype))) {
                loop->flags |= 0x20;
            }
            if (CInt64_IsZero(&((IROLoopInd301*)loop->induction)->step) ||
                (!is_unsigned(indNd->rtype) &&
                 CInt64_IsNegative(&((IROLoopInd301*)loop->induction)->step))) {
                loop->flags |= 0x40000;
            }
        }
    } else if (cond && cond->type == IROLinearOp1Arg && IsSmallInt(cond->rtype)) {
        if (cond->nodetype <= EPREDEC &&
            IRO_IsVariable(cond->u.monadic) &&
            (var = IRO_FindVar(cond->u.monadic->u.monadic->u.node->data.objref, FALSE, TRUE)) &&
            (ind = FindInd301(loop, var))) {
            loop->flags |= 0x8000;
            loop->induction = ind;
        }
        if (!loop->induction) {
            loop->flags |= 0x10;
        }
        if ((ind = loop->induction)) {
            indNd = ind->nd;
            func_00588AE0(loop, ind);
            func_00588260(loop);
            func_00584670(loop, FALSE);
        }
    } else {
        loop->flags |= 0x20;
    }

    if (loop->inductions && ((IROLoopInd301*)loop->inductions)->next) {
        loop->flags |= 0x40;
    }
    for (ind = loop->inductions; ind; ind = ind->next) {
        func_005530D0(ind->nd, func_00588880, loop);
    }
    if (loop->cond) {
        func_005530D0(loop->cond, func_00588850, loop);
    }
    PushElm(&loop->x28, loop->x10->last);

    // the test follows the update with nothing else between
    if (loop->induction) {
        loop->flags |= 0x100;
        nd = (loop->flags & 0x8000) ? testNode->first : indNd->next;
        for (; nd && nd != loop->cond; nd = nd->next) {
            if (!InElmList(loop->x30, nd) &&
                !InElmList(loop->x28, nd) &&
                nd->type != IROLinearLabel &&
                nd->type != IROLinearNop) {
                loop->flags &= ~0x100;
            }
        }
    }

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (InLoop301(loop, node) && (nd = node->first)) {
            for (;;) {
                if (nd->type == IROLinearFunccall &&
                    (!(nd->u.funccall.functype->flags & FUNC_INTRINSIC) ||
                     copts.opt_intrinsicfunctioncallscanhavesideeffects)) {
                    hasCall = TRUE;
                } else if (IRO_Hook15 && ((Boolean (*)(IROLinear*, int))IRO_Hook15)(nd, 0)) {
                    hasCall = TRUE;
                }
                if (nd->type == IROLinearGoto || nd->type == IROLinearGotoExpr) {
                    numGotos++;
                }
                if (numGotos >= 1) {
                    hasCtrl = TRUE;
                }
                if (!InElmList(loop->x28, nd)) {
                    if (nd->type == IROLinearIf ||
                        nd->type == IROLinearIfNot ||
                        nd->type == IROLinearSwitch ||
                        nd->type == IROLinearReturn) {
                        hasCtrl = TRUE;
                    }
                    if (!(nd->flags & IROLF_Reffed) &&
                        nd->type != IROLinearNop &&
                        nd->type != IROLinearLabel) {
                        if (!IRO_IsAssignment(nd)) {
                            loop->flags |= 8;
                        } else if (!InElmList(loop->x30, nd)) {
                            numAssigns++;
                            if (IRO_Hook10 &&
                                ((Boolean (*)(IROLinear*))IRO_Hook10)(nd) &&
                                numAssigns == 1) {
                                loop->flags |= 0x20000;
                            }
                            if (numAssigns > 1) {
                                loop->flags &= ~0x20000;
                            }
                        }
                    }
                }
                if (nd->type == IROLinearAsm) {
                    loop->flags |= 0x4000;
                }
                if (!InElmList(loop->x30, nd) &&
                    !InElmList(loop->x28, nd) &&
                    nd->type == IROLinearOperand &&
                    nd->u.node->type == EOBJREF) {
                    flags = nd->flags;
                    obj = nd->u.node->data.objref;
                    if ((flags & IROLF_Ind) &&
                        obj &&
                        loop->induction &&
                        obj == ((IROLoopInd301*)loop->induction)->var->object &&
                        (!(flags & IROLF_Assigned) || (flags & IROLF_Used))) {
                        IRO_Dump("Induction Used in loop\n");
                        loop->flags |= 0x400;
                    }
                }
                if (!InElmList(loop->x30, nd)) {
                    // x >> c and x & c of a counter
                    if (nd->type == IROLinearOp2Arg &&
                        nd->nodetype == ESHR &&
                        (obj = IRO_IsVariable(nd->u.diadic.left)) &&
                        IRO_IsIntConstant(nd->u.diadic.right)) {
                        for (ind = loop->inductions; ind; ind = ind->next) {
                            if (ind->var->object == obj) {
                                IRO_Dump("Induction has DIV: %s\n", obj->name->name);
                                ind->flags |= 2;
                            }
                        }
                    }
                    if (nd->type == IROLinearOp2Arg &&
                        nd->nodetype == EAND &&
                        (obj = IRO_IsVariable(nd->u.diadic.left)) &&
                        IRO_IsIntConstant(nd->u.diadic.right)) {
                        for (ind = loop->inductions; ind && obj; ind = ind->next) {
                            if (ind->var->object == obj) {
                                IRO_Dump("Induction has MOD: %s\n", obj->name->name);
                                ind->flags |= 1;
                            }
                        }
                    }
                }
                if (nd->type != IROLinearNop && nd->type != IROLinearLabel) {
                    loop->x50++;
                }
                if (nd == node->last) {
                    break;
                }
                nd = nd->next;
            }
        }
        if (hasCall) {
            loop->flags |= 2;
        }
        if (hasCtrl) {
            loop->flags |= 4;
        }
        if (InLoop301(loop, node) && node != testNode) {
            for (i = 0; i < node->numsucc; i++) {
                succ = node->succ[i];
                if (!Bv_IsBitSet(succ, loop->nodes)) {
                    IRO_Dump("Node %d has an out of loop successor %d\n", node->index, succ);
                    IRO_DumpBits("Loop includes: ", loop->nodes);
                    IRO_Dump("loop has multiple exits\n");
                    loop->flags |= 0x800;
                }
            }
        }
    }
    FreeElms(&loop->x30);
    FreeElms(&loop->x28);
}

// a jump out of node may not be retargeted (a computed goto, or asm with
// labels)
static inline Boolean JumpIsFixed(IROLinear* last) {
    IROAsmEffects effects;

    switch (last->type) {
        case IROLinearGotoExpr:
            return TRUE;
        case IROLinearAsm:
            if (IRO_GetAsmEffectsHook && last->u.asm_stmt) {
                effects = IRO_EmptyAsmEffects;
                IRO_GetAsmEffectsHook(last->u.asm_stmt, &effects);
                return effects.numlabels != 0;
            }
            return TRUE;
        default:
            return FALSE;
    }
}

// a jump of node's last linear to oldlabel goes to newlabel
static inline void RetargetJump(IRONode* node, CLabel* oldlabel, CLabel* newlabel) {
    IROLinear* last = node->last;
    SwitchInfo* info;
    SwitchCase* swcase;

    switch (last->type) {
        case IROLinearGoto:
            if (last->u.label.label == oldlabel) {
                last->u.label.label = newlabel;
            }
            break;
        case IROLinearIf:
        case IROLinearIfNot:
            if (last->u.label.label == oldlabel) {
                last->u.label.label = newlabel;
            }
            break;
        case IROLinearSwitch:
            info = last->u.swtch.info;
            for (swcase = info->cases; swcase; swcase = swcase->next) {
                if (swcase->label == oldlabel) {
                    swcase->label = newlabel;
                }
            }
            if (info->defaultlabel == oldlabel) {
                info->defaultlabel = newlabel;
            }
            break;
        default:
            break;
    }
}

// a new node holding a new label
static inline IRONode* NewLabelNode(IROLinear** plabel) {
    IRONode* node = IRO_NewFlowGraphNode();
    IROLinear* label = func_00554490();

    label->u.label.label->stmt = (Statement*)node;
    node->first = label;
    node->last = label;
    *plabel = label;
    return node;
}

// 0x58AB50: a new preheader for the loop, after pred (the header's fall-in
// predecessor) or before the loop; NULL when a jump into the loop cannot be
// retargeted (2.4.7's CreatePreHeader)
IRONode* func_0058AB50(IRONaturalLoop* loop, IRONode* header, IRONode* pred) {
    Boolean ok = TRUE;
    IRONode* newnode;
    IRONode* node;
    IRONode* prev;
    IROLinear* label;
    CLabel* oldlabel;
    CLabel* newlabel;
    UInt16 i;

    if (header->first->type == IROLinearLabel) {
        for (i = 0; i < header->numpred; i++) {
            node = IRO_NodeTable[header->pred[i]];
            if (!InLoop301(loop, node) && JumpIsFixed(node->last)) {
                ok = FALSE;
            }
        }
    }
    if (!ok) {
        return NULL;
    }

    newnode = NewLabelNode(&label);
    if (pred) {
        label->next = IRO_NewLinear(IROLinearNop);
        label->next->next = pred->last->next;
        pred->last->next = label;
        newnode->nextnode = pred->nextnode;
        pred->nextnode = newnode;
        if (pred == IRO_LastNode) {
            IRO_LastNode = newnode;
            IRO_LastLinear = label->next;
        }
    } else {
        prev = NULL;
        for (node = IRO_FirstNode; node && !InLoop301(loop, node); node = node->nextnode) {
            prev = node;
        }
        if (!node || prev == IRO_LastNode) {
            CError_FATAL(393);
        }
        if (node == header) {
            label->next = IRO_NewLinear(IROLinearNop);
        } else {
            label->next = IRO_NewLinear(IROLinearGoto);
            label->next->u.label.label = header->first->u.label.label;
        }
        if (prev) {
            label->next->next = prev->last->next;
            prev->last->next = label;
            newnode->nextnode = prev->nextnode;
            prev->nextnode = newnode;
        } else {
            label->next->next = IRO_FirstLinear;
            IRO_FirstLinear = label;
            newnode->nextnode = IRO_FirstNode;
            IRO_FirstNode = newnode;
        }
    }
    newnode->last = label->next;
    IRO_RebuildNodeTable();

    if (header->first->type == IROLinearLabel) {
        oldlabel = header->first->u.label.label;
        newlabel = newnode->first->u.label.label;
        for (i = 0; i < header->numpred; i++) {
            node = IRO_NodeTable[header->pred[i]];
            if (!InLoop301(loop, node) && node != newnode) {
                RetargetJump(node, oldlabel, newlabel);
            }
        }
    }
    IRO_ComputeSuccPred();
    IRO_ComputeDom();
    if (newnode->nextnode == header) {
        loop->x18 = newnode;
    }
    return newnode;
}

// 0x58A5D0: a new node on the loop's exit edge (2.4.7's
// CreateNewLoopExitSuccessor); NULL when the exit jump cannot be retargeted
IRONode* func_0058A5D0(IRONaturalLoop* loop) {
    IRONode* succ = loop->exitSuccessor;
    IRONode* exitNode;
    IRONode* node;
    IRONode* prev;
    IRONode* newnode;
    IROLinear* label;
    UInt16 i;
    Boolean placeAfterExit;

    if (!succ || loop->numExits != 1) {
        CError_FATAL(520);
    }
    exitNode = NULL;
    placeAfterExit = FALSE;
    for (i = 0; i < succ->numpred; i++) {
        node = IRO_NodeTable[succ->pred[i]];
        if (Bv_IsBitSet(node->index, loop->x54)) {
            if (exitNode) {
                CError_FATAL(531);
            }
            exitNode = node;
            if (!placeAfterExit) {
                if (!node->last ||
                    !succ->first ||
                    succ->first->type != IROLinearLabel ||
                    ((node->last->type == IROLinearIf || node->last->type == IROLinearIfNot) &&
                     node->last->u.label.label != succ->first->u.label.label)) {
                    placeAfterExit = TRUE;
                }
            }
        }
    }
    if (!exitNode) {
        CError_FATAL(547);
    }
    if (!placeAfterExit && JumpIsFixed(exitNode->last)) {
        return NULL;
    }

    newnode = NewLabelNode(&label);
    if (placeAfterExit) {
        exitNode->last->next = label;
        label->next = IRO_NewLinear(IROLinearNop);
        label->next->next = succ->first;
        exitNode->nextnode = newnode;
        newnode->nextnode = succ;
    } else {
        if (succ->first->type != IROLinearLabel) {
            CError_FATAL(615);
        }
        prev = NULL;
        for (node = IRO_FirstNode; node; node = node->nextnode) {
            if (InLoop301(loop, node)) {
                prev = node;
            }
        }
        if (!prev) {
            CError_FATAL(626);
        }
        if (prev->nextnode == succ) {
            label->next = IRO_NewLinear(IROLinearNop);
        } else {
            label->next = IRO_NewLinear(IROLinearGoto);
            label->next->u.label.label = succ->first->u.label.label;
        }
        if (prev == IRO_LastNode) {
            IRO_LastNode->last->next = label;
            IRO_LastNode->nextnode = newnode;
            IRO_LastNode = newnode;
            IRO_LastLinear = label->next;
        } else {
            if (!prev->nextnode) {
                CError_FATAL(641);
            }
            prev->last->next = label;
            label->next->next = prev->nextnode->first;
            newnode->nextnode = prev->nextnode;
            prev->nextnode = newnode;
        }
    }
    newnode->last = label->next;
    IRO_RebuildNodeTable();
    if (succ->first->type == IROLinearLabel) {
        RetargetJump(exitNode, succ->first->u.label.label, newnode->first->u.label.label);
    }
    IRO_ComputeSuccPred();
    IRO_ComputeDom();
    return newnode;
}

// 0x58A430: the loop's preheader (made when *made is not set) and its last
// linear in *at
void func_0058A430(
    IRONaturalLoop* loop, IRONode* header, IRONode* pred, IROLinear** at, Boolean* made
) {
    IRONode* node;

    if (!*made) {
        if (!loop->preheader || loop->preheader->last->type != IROLinearGoto) {
            loop->preheader = func_0058AB50(loop, header, pred);
        }
        *made = loop->preheader != NULL;
    } else if (!loop->preheader) {
        CError_FATAL(772);
    }
    if (loop->preheader) {
        *at = loop->preheader->last;
        if ((*at)->type == IROLinearGoto &&
            (*at)->u.label.label != loop->x10->first->u.label.label) {
            for (node = IRO_FirstNode; node; node = node->nextnode) {
                node->mustreach = FALSE;
                node->unk3F = FALSE;
            }
        }
        IRO_DumpAfterPhase("Creating PreHeader", FALSE);
    } else {
        *at = NULL;
    }
}

// 0x589F20: the loop's one exit successor (made when it has other
// predecessors) and its first linear after the labels
void func_00589F20(IRONaturalLoop* loop, IRONode** exitNode, IROLinear** at) {
    IRONode* succ;
    UInt16 i;
    UInt16 n;

    if (loop->numExits == 1 && (succ = loop->exitSuccessor)) {
        *exitNode = succ;
        n = succ->numpred;
        for (i = 0; i < n; i++) {
            if (!Bv_IsBitSet(IRO_NodeTable[succ->pred[i]]->index, loop->nodes)) {
                *exitNode = func_0058A5D0(loop);
                break;
            }
        }
        if (*exitNode) {
            loop->exitSuccessor = *exitNode;
            *at = (*exitNode)->first;
            if ((*exitNode)->last) {
                while (*at &&
                       (*at)->type == IROLinearLabel &&
                       (*at)->next != (*exitNode)->last->next) {
                    *at = (*at)->next;
                }
            }
        } else {
            *at = NULL;
        }
    } else {
        *exitNode = NULL;
        *at = NULL;
    }
}

// 0x58A240: the loop's test can be copied before it: nothing from the header
// to the test's successor in the loop has a side effect
Boolean func_0058A240(IRONaturalLoop* loop) {
    IRONode* node;
    IRONode* succ;
    IRONode* found;
    IROLinear* nd;
    IROLinear* root;
    UInt16 i;
    int n;

    if (loop->x10 != loop->header && (loop->flags & 4)) {
        return FALSE;
    }
    if (!loop->x20) {
        found = NULL;
        n = 0;
        node = loop->x10;
        for (i = 0; i < node->numsucc; i++) {
            succ = IRO_NodeTable[node->succ[i]];
            if (Bv_IsBitSet(succ->index, loop->nodes)) {
                n++;
                found = succ;
            }
        }
        if (n != 1) {
            found = NULL;
        }
        loop->x20 = found;
    }
    if (!loop->x20) {
        return FALSE;
    }
    node = loop->header;
    if (loop->x20 == node) {
        return TRUE;
    }
    for (; node && node != loop->x10->nextnode; node = node->nextnode) {
        if (!Bv_IsBitSet(node->index, loop->nodes)) {
            CError_FATAL(840);
        }
        for (nd = node->first; nd && nd != node->last->next; nd = nd->next) {
            if (!(nd->flags & IROLF_Reffed) &&
                (root = func_00552E90(nd)) &&
                IRO_HasSideEffect(root)) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

// 0x58A030: copy the loop test before the loop (into a fresh preheader),
// inverted to jump to the exit node
Boolean func_0058A030(
    IRONaturalLoop* loop, IROLinear** pre, IRONode* exitNode, IRONode** newPre, void* unused
) {
    IRONode* oldpre = loop->preheader;
    IRONode* made;
    IROLinear* label;
    IROLinear* condCopy;
    IROLinearType lastType;
    IROList list;
    Boolean flag;

    if (!oldpre || !func_0058A240(loop)) {
        *pre = NULL;
        return FALSE;
    }
    label = exitNode->first;
    if (label && label->type != IROLinearLabel) {
        label = func_00554490();
        IRO_Paste(label, label, exitNode->first);
    }
    lastType = oldpre->last->type;
    flag = FALSE;
    loop->preheader = NULL;
    func_0058A430(loop, loop->header, loop->x18, pre, &flag);
    made = loop->preheader;
    if (!made) {
        loop->preheader = oldpre;
        *pre = NULL;
        return FALSE;
    }
    switch (lastType) {
        case IROLinearGoto:
        case IROLinearFunccall:
        case IROLinearGotoExpr:
        case IROLinearIf:
        case IROLinearIfNot:
        case IROLinearReturn:
        case IROLinearSwitch:
        case IROLinearEntry:
        case IROLinearExit:
        case IROLinearAsm:
        case IROLinearEnd:
            break;
        default:
            if (oldpre->nextnode == loop->header) {
                goto haveNode;
            }
            break;
    }
    flag = FALSE;
    loop->preheader = NULL;
    func_0058A430(loop, loop->header, loop->x18, pre, &flag);
    if (loop->preheader) {
        if (made->last->type == IROLinearGoto) {
            IRO_NopOut(made->last);
        }
        oldpre = made;
    } else {
        oldpre = NULL;
        loop->preheader = made;
    }
haveNode:
    if (!oldpre) {
        *pre = NULL;
        return FALSE;
    }
    IRO_InitList(&list);
    func_00552B00(loop->header->first, loop->cond, &list);
    IRO_DuplicateExpr(loop->cond, &list);
    condCopy = list.tail;
    IRO_DuplicateExpr(loop->x10->last, &list);
    IRO_PasteAfter(list.head, list.tail, oldpre->last);
    if (oldpre->last->type == IROLinearIf) {
        oldpre->last->type = IROLinearIfNot;
    } else {
        oldpre->last->type = IROLinearIf;
    }
    oldpre->last->u.label.label = label->u.label.label;
    oldpre->last->u.label.cond = condCopy;
    IRO_ComputeSuccPred();
    IRO_ComputeDom();
    IRO_DumpAfterPhase("IRO_CopyLoopTestToLoopPreheaderAndBranchToSuccNode", FALSE);
    if (newPre) {
        *newPre = oldpre;
    }
    return TRUE;
}

// 0x588C90: the loop's induction record of var
IROLoopInd301* func_00588C90(IRONaturalLoop* loop, VarRecord* var) {
    return FindInd301(loop, var);
}

// 0x583FD0: the counter's step with its sign
CInt64 func_00583FD0(IROLoopInd301* ind, Type* type) {
    CInt64 step = ind->step;

    if (ind->flags & 4) {
        step = CInt64_Neg(step);
    }
    return step;
}

// 0x583D30: dump loop flags by name
void func_00583D30(UInt32 flags) {
    static const char* names[22] = {
        "LP_INDUCTION_AT_LEFT ",
        "LP_HAS_CALLS ",
        "LP_HAS_CNTRLFLOW ",
        "LP_HAS_NONASSIGN ",
        "LP_INDUCTION_NOT_FOUND ",
        "LP_IF_EXPR_NONCANONICAL ",
        "LP_HAS_MULTIPLE_INDUCTIONS ",
        "LP_STEP_IS_ONE ",
        "LP_HEADER_FOLLOWS_UPDATE ",
        "LP_STEP_IS_POS ",
        "LP_IND_USED_IN_LOOP ",
        "LP_HAS_MULTIPLE_EXITS ",
        "LP_STEP_IS_NEG_ONE ",
        "LP_STEP_IS_NEG ",
        "LP_HAS_ASM ",
        "LP_WHILE_LOOP ",
        "LP_RECURSIVE_LOOP ",
        "LP_IS_REDUCTION_CAND ",
        "LP_HAS_NONCANONICAL_STEP ",
        "LP_INNER_LOOP ",
        "LP_TO_BE_VISITED ",
        "LP_FORM_UNRECOGNIZABLE "
    };
    int i;

    if (!flags) {
        IRO_Dump("LP_FLAGS_NONE");
        return;
    }
    for (i = 0; i < 22; i++) {
        if (flags & (1 << i)) {
            IRO_Dump((char*)names[i]);
        }
    }
    if (flags & 0xFFC00000) {
        IRO_Dump("%ul", flags & 0xFFC00000);
    }
}

// 0x587270: the test node's one successor in the loop (loop->x20)
void func_00587270(IRONaturalLoop* loop) {
    IRONode* node = loop->x10;
    IRONode* succ;
    IRONode* found = NULL;
    UInt16 i;
    int n = 0;

    for (i = 0; i < node->numsucc; i++) {
        succ = IRO_NodeTable[node->succ[i]];
        if (Bv_IsBitSet(succ->index, loop->nodes)) {
            n++;
            found = succ;
        }
    }
    if (n != 1) {
        found = NULL;
    }
    loop->x20 = found;
}

// 0x588690: nd is part of the loop test
Boolean func_00588690(IRONaturalLoop* loop, IROLinear* nd) {
    return InElmList(loop->x28, nd);
}

// 0x588740: nd is part of an induction update
Boolean func_00588740(IRONaturalLoop* loop, IROLinear* nd) {
    return InElmList(loop->x30, nd);
}

// 0x5886C0
void func_005886C0(IRONaturalLoop* loop) {
    if (loop) {
        FreeElms(&loop->x28);
    }
}

// 0x588770
void func_00588770(IRONaturalLoop* loop) {
    if (loop) {
        FreeElms(&loop->x30);
    }
}

// 0x588700: collect the loop test's linears
void func_00588700(IRONaturalLoop* loop) {
    if (loop && loop->cond) {
        func_005530D0(loop->cond, func_00588850, loop);
    }
    PushElm(&loop->x28, loop->x10->last);
}

// 0x5887B0: collect the induction updates' linears
void func_005887B0(IRONaturalLoop* loop) {
    IROLoopInd301* ind;

    if (loop) {
        for (ind = loop->inductions; ind; ind = ind->next) {
            func_005530D0(ind->nd, func_00588880, loop);
        }
    }
}

// 0x58A4F0: the loop's predecessors outside it; TRUE if there is one
Boolean func_0058A4F0(IRONaturalLoop* loop) {
    IRONode* pred;
    Boolean found;
    UInt16 i;

    loop->x18 = NULL;
    found = FALSE;
    loop->preheader = NULL;
    for (i = 0; i < loop->header->numpred; i++) {
        pred = IRO_NodeTable[loop->header->pred[i]];
        if (Bv_IsBitSet(pred->index, loop->nodes)) {
            continue;
        }
        if (!found) {
            loop->preheader = pred;
        } else {
            loop->preheader = NULL;
        }
        found = TRUE;
        if (pred->nextnode == loop->header) {
            if (loop->x18 && pred != loop->x18) {
                CError_FATAL(748);
            }
            loop->x18 = pred;
        }
    }
    return found;
}

void IRO_SetLoopDepth(void) {
    IRONode* fnode;
    IRONaturalLoop* loops;
    IRONaturalLoop* loop;
    IRONaturalLoop* next;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        fnode->loopdepth = 0;
    }

    loops = FindLoopHeaders();
    for (loop = loops; loop; loop = loop->next) {
        ComputeLoopBody(loop);
        for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
            if (Bv_IsBitSet(fnode->index, loop->nodes)) {
                fnode->loopdepth++;
            }
        }
    }

    if (copts.opt_markcounterloops) {
        IRO_MarkCounterLoops(loops);
    }

    while (loops) {
        next = loops->next;
        if (loops->nodes) {
            FREE_BV(loops->nodes);
        }
        if (loops->xC) {
            FREE_BV(loops->xC);
        }
        if (loops->x54) {
            FREE_BV(loops->x54);
        }
        if (loops->x58) {
            FREE_BV(loops->x58);
        }
        IRO_free(loops);
        loops = next;
    }

    IRO_CheckForUserBreak();
}

// 0x589A80: free the loop records
// FindLoopHeaders for IroAggregateAssignment.c (0x57CD60)
IRONaturalLoop* IRO_FindLoopHeaders301(void) {
    return FindLoopHeaders();
}

void func_00589A80(IRONaturalLoop* loops) {
    IRONaturalLoop* next;

    while (loops) {
        next = loops->next;
        if (loops->nodes) {
            FREE_BV(loops->nodes);
        }
        if (loops->xC) {
            FREE_BV(loops->xC);
        }
        if (loops->x54) {
            FREE_BV(loops->x54);
        }
        if (loops->x58) {
            FREE_BV(loops->x58);
        }
        IRO_free(loops);
        loops = next;
    }
}

// 0x589B10: empty in 3.0.1 (the loops are left in the order found)
void func_00589B10(IRONaturalLoop** loops) {}

extern Boolean func_005922C0(IRONaturalLoop* loop); // IroLoopInvariants.c

// 0x593F60: hoist the invariant assignments of every loop
Boolean IRO_PullOutInvariantAssignments(void) {
    IRONaturalLoop* loops;
    IRONaturalLoop* loop;
    Boolean changed;

    loops = FindLoopHeaders();
    func_00589B10(&loops);
    changed = FALSE;
    for (loop = loops; loop; loop = loop->next) {
        ComputeLoopBody(loop);
        IRO_Dump(
            "IRO_PullOutInvariantAssignments:transforming loop with header %d\n",
            loop->header->index
        );
        IRO_DumpBits("Loop includes: ", loop->nodes);
        changed |= func_005922C0(loop);
        IRO_UpdateFlagsOnInts(IRO_FirstLinear);
    }
    func_00589A80(loops);
    IRO_CheckForUserBreak();
    return changed;
}
Boolean data_00625FD5; // the lifetimes (VarRecord +8) are computed (IroEmptyLoop301.c)
// --- 3.0.1 address expression regrouping ---------------------------------------

extern Type* func_0055C980(Type* type);
extern Type* func_0055A620(Type* type);

// an integer added to a pointer, as a ptrdiff-sized integer
static inline Type* PtrAddendType(Type* type, Type* pd, Type* upd, Type* spd) {
    if (is_unsigned(type) == is_unsigned(pd) && pd->size >= type->size) {
        return pd;
    }
    if (!is_unsigned(pd) && is_unsigned(type) && pd->size > type->size) {
        return pd;
    }
    return is_unsigned(type) ? upd : spd;
}

// 0x5916C0: the types two addends are added in
void func_005916C0(Type** pa, Type** pb) {
    Type* a = *pa;
    Type* b = *pb;
    Type* pd = func_0055A660();
    Type* upd = func_0055CA90(pd);
    Type* spd = func_0055C980(pd);

    if (b->type == TYPEPOINTER) {
        a = PtrAddendType(a, pd, upd, spd);
    } else if (a->type == TYPEPOINTER) {
        b = PtrAddendType(b, pd, upd, spd);
    } else {
        if (is_unsigned(a)) {
            b = func_0055CA90(b);
        } else if (is_unsigned(b)) {
            a = func_0055CA90(a);
        }
        a = func_0055A620(a);
        b = func_0055A620(b);
        if (TYPE_INTEGRAL(a)->integral > TYPE_INTEGRAL(b)->integral) {
            b = a;
        } else if (TYPE_INTEGRAL(b)->integral > TYPE_INTEGRAL(a)->integral) {
            a = b;
        }
    }
    *pa = a;
    *pb = b;
}

// 0x591C70: the addends of a sum (through value-preserving conversions)
void func_00591C70(IROLinear* nd) {
    IROElmList* elm;

    while (nd->type == IROLinearOp1Arg && nd->nodetype == ETYPCON && func_0055A3F0(nd)) {
        nd = nd->u.monadic;
    }
    if (nd->type == IROLinearOp2Arg && nd->nodetype == EADD) {
        func_00591C70(nd->u.diadic.left);
        func_00591C70(nd->u.diadic.right);
    } else {
        elm = IRO_malloc(sizeof(IROElmList));
        elm->element = nd;
        elm->next = NULL;
        if (FirstAddendLinear) {
            LastAddendLinear->next = elm;
        } else {
            FirstAddendLinear = elm;
        }
        LastAddendLinear = elm;
    }
}

// result + a copy of addend, in the types the two have in common
static inline IROLinear* AddAddend301(
    IROLinear* result, IROLinear* addend, Boolean invariant, IROList* list
) {
    Type* type = addend->rtype;
    Type* rtype;
    IROLinear* copy;
    IROLinear* sum;

    if (!result) {
        return IRO_DuplicateExpr(addend, list);
    }
    rtype = result->rtype;
    func_005916C0(&type, &rtype);
    if (!IRO_TypesEqual(result->rtype, rtype)) {
        result = IRO_MakeTypeConversion(result, rtype, list);
    }
    copy = IRO_DuplicateExpr(addend, list);
    if (!IRO_TypesEqual(copy->rtype, type)) {
        copy = IRO_MakeTypeConversion(copy, type, list);
    }
    sum = IRO_NewLinear(IROLinearOp2Arg);
    sum->index = ++IRO_NumLinear;
    sum->nodetype = EADD;
    sum->u.diadic.left = result;
    sum->u.diadic.right = copy;
    IRO_AddToList(sum, list);
    if (invariant) {
        sum->flags |= IROLF_LoopInvariant;
    }
    sum->flags |= IROLF_Reffed;
    sum->rtype = IRO_UnqualifiedType(type->type == TYPEPOINTER ? type : rtype);
    return sum;
}

// 0x591860: the sum nd rebuilt on list as invariant addends + the other
// variable addends + the constants (2.4.7's RearrangeInvarianceInAddressExpr,
// with the addends' types balanced)
IROLinear* func_00591860(IROLinear* nd, IROList* list) {
    IROLinear* result = NULL;
    IROLinear* addend;
    IROElmList* elm;
    IROElmList* next;
    IROElmList* prev;

    IRO_InitList(list);
    LastAddendLinear = NULL;
    FirstAddendLinear = LastAddendLinear;
    func_00591C70(nd->u.diadic.left);
    func_00591C70(nd->u.diadic.right);

    prev = NULL;
    for (elm = FirstAddendLinear; elm; elm = next) {
        addend = elm->element;
        next = elm->next;
        if (!IRO_IsIntConstant(addend) && (addend->flags & IROLF_LoopInvariant)) {
            result = AddAddend301(result, addend, TRUE, list);
            if (prev) {
                prev->next = next;
            } else {
                FirstAddendLinear = next;
            }
        } else {
            prev = elm;
        }
    }
    prev = NULL;
    for (elm = FirstAddendLinear; elm; elm = next) {
        addend = elm->element;
        next = elm->next;
        if (!IRO_IsIntConstant(addend)) {
            result = AddAddend301(result, addend, FALSE, list);
            if (prev) {
                prev->next = next;
            } else {
                FirstAddendLinear = next;
            }
        } else {
            prev = elm;
        }
    }
    for (elm = FirstAddendLinear; elm; elm = elm->next) {
        result = AddAddend301(result, elm->element, FALSE, list);
    }
    for (elm = FirstAddendLinear; elm; elm = next) {
        next = elm->next;
        IRO_free(elm);
    }
    return result;
}

// the loop's *(a + b) linears
#define FOR_LOOP_ADDRESS_SUMS(loop, node, nd) \
    for (node = IRO_FirstNode; node; node = node->nextnode) \
        if (Bv_IsBitSet(node->index, (loop)->nodes)) \
            for (nd = node->first; nd; nd = (nd == node->last) ? NULL : nd->next) \
                if (nd->type == IROLinearOp1Arg && \
                    nd->nodetype == EINDIRECT && \
                    nd->u.monadic->type == IROLinearOp2Arg && \
                    nd->u.monadic->nodetype == EADD)

// 0x591CF0: the invariant addends of the loop's address sums first (2.4.7's
// MoveInvarianceInAddressExpr)
void func_00591CF0(IRONaturalLoop* loop) {
    IRONode* node;
    IROLinear* nd;
    IROLinear* start;
    IROList list;

    IRO_LastExpr = NULL;
    IRO_FirstExpr = IRO_LastExpr;
    IRO_NumExprs = 0;
    FOR_LOOP_ADDRESS_SUMS(loop, node, nd) {
        func_00591860(nd->u.monadic, &list);
        start = IRO_FindStart(nd->u.monadic);
        IRO_LocateFather_Cut_And_Paste(nd->u.monadic, list.tail);
        IRO_Paste(list.head, list.tail, start);
    }
    IRO_UpdateFlagsOnInts(IRO_FirstLinear);
    func_00591FB0(loop);
}

// 0x590E90: (... + addend + ...) * inv, with addend invariant, becomes
// addend * inv + (...) * inv
void func_00590E90(IROLinear* nd, IROLinear* inv, IROLinear* sum, IROLinear* addend) {
    IROLinear* father;
    IROLinear* prod;
    IROLinear* add;
    IROList list;

    if (!(addend->flags & IROLF_LoopInvariant)) {
        return;
    }
    father = IRO_LocateFather(addend);
    if (!father || father->type != IROLinearOp2Arg || father->nodetype != EADD) {
        return;
    }
    IRO_InitList(&list);
    prod = IRO_NewLinear(IROLinearOp2Arg);
    *prod = *nd;
    prod->index = ++IRO_NumLinear;
    IRO_DuplicateExpr(addend, &list);
    if (!IRO_TypesEqual(addend->rtype, nd->rtype)) {
        IRO_MakeTypeConversion(list.tail, nd->rtype, &list);
    }
    prod->u.diadic.left = list.tail;
    IRO_NopOut(addend);
    if (father->u.diadic.left != addend) {
        IRO_LocateFather_Cut_And_Paste_Without_Nopping(father, father->u.diadic.left);
    } else {
        IRO_LocateFather_Cut_And_Paste_Without_Nopping(father, father->u.diadic.right);
    }
    father->type = IROLinearNop;
    IRO_DuplicateExpr(inv, &list);
    prod->u.diadic.right = list.tail;
    prod->next = NULL;
    IRO_AddToList(prod, &list);
    add = IRO_NewLinear(IROLinearOp2Arg);
    *add = *sum;
    add->type = IROLinearOp2Arg;
    add->index = ++IRO_NumLinear;
    add->u.diadic.left = prod;
    add->u.diadic.right = nd;
    if (!IRO_TypesEqual(add->rtype, nd->rtype)) {
        add->rtype = nd->rtype;
    }
    add->next = NULL;
    IRO_AddToList(add, &list);
    IRO_LocateFather_Cut_And_Paste_Without_Nopping(nd, add);
    IRO_PasteAfter(list.head, list.tail, nd);
}

// 0x591060: each addend of the sum
void func_00591060(IROLinear* nd, IROLinear* inv, IROLinear* sum, IROLinear* addend) {
    if (addend->type == IROLinearOp2Arg && addend->nodetype == EADD) {
        func_00591060(nd, inv, sum, addend->u.diadic.left);
        func_00591060(nd, inv, sum, addend->u.diadic.right);
    } else {
        func_00590E90(nd, inv, sum, addend);
    }
}

extern void func_00552150(IROLinear* nd, Object* obj, IROLinear* after);
extern void func_00552BC0(IROExpr* expr, IROLinear* linear);
extern IROLinear* func_00551ED0(Object* temp, IROLinear* value, IROList* list);

// 0x591E00: the loop's invariant expressions computed into temporaries
// before the loop (pre), equal ones sharing a temporary
void func_00591E00(IRONaturalLoop* loop, IROLinear* pre) {
    IROExpr* moved = NULL;
    IROExpr* expr;
    IROExpr* next;
    IROExpr* scan;
    IROList list;
    Boolean found;

    for (expr = IRO_FirstExpr; expr; expr = next) {
        next = expr->next;
        if (Bv_IsBitSet(expr->node->index, loop->nodes) &&
            !expr->notSubable &&
            (!expr->couldError || expr->node->unk3F) &&
            (expr->linear->flags & IROLF_LoopInvariant)) {
            IRO_Dump("Found loop invariant: %d\n", expr->linear->index);
            found = FALSE;
            for (scan = moved; scan; scan = scan->next) {
                if (IRO_ExprsSame(scan->linear, expr->linear)) {
                    func_00552150(expr->linear, scan->temp, expr->linear);
                    IRO_NopOut(expr->linear);
                    IRO_Dump("Using already removed expr: %d\n", scan->linear->index);
                    IRO_RemoveExpr(expr);
                    found = TRUE;
                    break;
                }
            }
            if (!found && !expr->temp) {
                expr->temp = create_temp_object(IRO_UnqualifiedType(expr->linear->rtype));
                IRO_FindVar(expr->temp, TRUE, TRUE);
                func_00552150(expr->linear, expr->temp, expr->linear);
                func_00552BC0(expr, pre);
                IRO_InitList(&list);
                func_00551ED0(expr->temp, expr->linear, &list);
                IRO_PasteAfter(list.head, list.tail, expr->linear);
                IRO_RemoveExpr(expr);
                expr->next = moved;
                moved = expr;
            }
        }
    }
}

extern IROLinear* func_00554440(CLabel* label, IROLinear* start);

// 0x59C350: rotate a loop tested at its header: the test is copied,
// inverted, into the preheader (jumping past the loop) and the loop is
// entered at the test's successor
Boolean func_0059C350(IRONaturalLoop* loop) {
    Boolean rotated = FALSE;
    IRONode* pre;
    IRONode* node;
    IROLinear* target;
    IROLinear* label;
    CLabel* newlabel;
    IROList list;

    func_00587540(loop);
    if (loop->x10 == loop->header &&
        !(loop->flags & LP301_FORM_UNRECOGNIZABLE) &&
        (!copts.optimizesize || (loop->hasCount && CInt64_Greater(loop->count, cint64_zero)))) {
        func_00587270(loop);
        pre = loop->preheader;
        if (pre &&
            pre->last->type == IROLinearGoto &&
            pre->nextnode == loop->x20 &&
            loop->x20 != loop->x10) {
            target = func_00554440(pre->last->u.label.label, pre->last);
            IRO_InitList(&list);
            func_00552B00(target->next, loop->cond, &list);
            IRO_DuplicateExpr(loop->cond, &list);
            IRO_Paste(list.head, list.tail, pre->last);
            if (loop->x10->last->type == IROLinearIf) {
                pre->last->type = IROLinearIfNot;
            } else {
                pre->last->type = IROLinearIf;
            }
            newlabel = IRO_NewLabel();
            pre->last->u.label.label = newlabel;
            pre->last->u.label.cond = list.tail;
            node = IRO_NewFlowGraphNode();
            label = IRO_NewLinear(IROLinearLabel);
            label->index = IRO_NumLinear++;
            label->next = NULL;
            label->u.label.label = newlabel;
            label->flags |= IROLF_StartsNode;
            newlabel->stmt = (Statement*)node;
            node->first = label;
            node->last = label;
            label->next = loop->x10->last->next;
            loop->x10->last->next = label;
            node->nextnode = loop->x10->nextnode;
            loop->x10->nextnode = node;
            loop->preheader = func_0058AB50(loop, pre->nextnode, pre);
            if (loop->preheader) {
                rotated = TRUE;
            }
            loop->x10 = loop->x20;
        }
    }
    if (rotated) {
        IRO_RebuildNodeTable();
    }
    return rotated;
}

// 0x591250: (sum) * inv and (sum) << inv: distribute over the sum's addends
void func_00591250(IROLinear* nd) {
    IROLinear* left;
    IROLinear* right;
    IROLinear* inv;
    IROLinear* var;

    if (nd->type != IROLinearOp2Arg) {
        return;
    }
    inv = NULL;
    var = NULL;
    left = nd->u.diadic.left;
    right = nd->u.diadic.right;
    if (nd->nodetype == EMUL) {
        if (right->flags & IROLF_LoopInvariant) {
            if (!(left->flags & IROLF_LoopInvariant)) {
                inv = right;
                var = left;
            }
        } else if (left->flags & IROLF_LoopInvariant) {
            inv = left;
            var = right;
        }
    } else if (nd->nodetype == ESHL &&
               (right->flags & IROLF_LoopInvariant) &&
               !(left->flags & IROLF_LoopInvariant)) {
        inv = right;
        var = left;
    }
    while (var && var->type == IROLinearOp1Arg && var->nodetype == ETYPCON && func_0055A3F0(var)) {
        var = var->u.monadic;
    }
    if (inv && var && var->type == IROLinearOp2Arg && var->nodetype == EADD) {
        func_00591060(nd, inv, var, var->u.diadic.left);
        func_00591060(nd, inv, var, var->u.diadic.right);
    }
}

// 0x591310
void func_00591310(IROLinear* nd) {
    if (nd->type == IROLinearOp2Arg && nd->nodetype == EADD) {
        func_00591310(nd->u.diadic.left);
        func_00591310(nd->u.diadic.right);
    } else {
        func_00591250(nd);
    }
}

// 0x5915E0: distribute invariant factors over the loop's address sums
void func_005915E0(IRONaturalLoop* loop) {
    IRONode* node;
    IROLinear* nd;

    IRO_LastExpr = NULL;
    IRO_FirstExpr = IRO_LastExpr;
    IRO_NumExprs = 0;
    FOR_LOOP_ADDRESS_SUMS(loop, node, nd) {
        if (!IRO_HasSideEffect(nd)) {
            func_00591310(nd->u.monadic->u.diadic.left);
            func_00591310(nd->u.monadic->u.diadic.right);
        }
    }
    IRO_ConstantFolding();
    IRO_UpdateFlagsOnInts(IRO_FirstLinear);
    func_00591FB0(loop);
}

static void IRO_ActUnmarkLoopInvariance(IROLinear* linear, Boolean isFirst);

// 0x5920D0: the loop's invariant pointer sums keep only their variable
// part marked invariant (the constant offset stays in the address)
void func_005920D0(IRONaturalLoop* loop) {
    IROExpr* expr;
    IROLinear* nd;
    IROLinear* next;

    for (expr = IRO_FirstExpr; expr; expr = expr->next) {
        if (Bv_IsBitSet(expr->node->index, loop->nodes) &&
            !expr->notSubable &&
            (!expr->couldError || expr->node->unk3F) &&
            (expr->linear->flags & IROLF_LoopInvariant)) {
            IRO_WalkTree(expr->linear, IRO_ActUnmarkLoopInvariance);
            expr->linear->flags |= IROLF_LoopInvariant;
        }
    }
    for (expr = IRO_FirstExpr; expr; expr = expr->next) {
        if (Bv_IsBitSet(expr->node->index, loop->nodes) &&
            !expr->notSubable &&
            (!expr->couldError || expr->node->unk3F)) {
            for (nd = expr->linear;
                 nd && (nd->flags & IROLF_LoopInvariant) && nd->rtype->type == TYPEPOINTER;
                 nd = next) {
                if (nd->type == IROLinearOperand && nd->u.node && nd->u.node->type == EINTCONST) {
                    next = NULL;
                } else if (nd->type == IROLinearOp2Arg && IRO_IsIntConstant(nd->u.diadic.right)) {
                    next = nd->u.diadic.left;
                } else if (nd->type == IROLinearOp2Arg && IRO_IsIntConstant(nd->u.diadic.left)) {
                    next = nd->u.diadic.right;
                } else {
                    next = nd;
                }
                if (next == nd) {
                    break;
                }
                nd->flags &= ~IROLF_LoopInvariant;
                if (next) {
                    next->flags |= IROLF_LoopInvariant;
                }
            }
        }
    }
}

extern Boolean func_0058DB90(IRONaturalLoop* loop); // IroEmptyLoop301.c
extern Boolean func_0058D780(IRONaturalLoop* loop);
extern void RemoveLoopBranch301(IRONode* testNode);

// 0x58DE60
Boolean IRO_RemoveAllEmptyLoops(void) {
    IRONaturalLoop* loops;
    IRONaturalLoop* loop;
    IRONode* testNode;
    Boolean changed;
    Boolean removed;

    data_00625FD5 = FALSE;
    loops = FindLoopHeaders();
    func_00589B10(&loops);
    changed = FALSE;
    for (loop = loops; loop; loop = loop->next) {
        ComputeLoopBody(loop);
        IRO_Dump("IRO_RemoveAllEmptyLoops:Found loop with header %d\n", loop->header->index);
        IRO_DumpBits("Loop includes: ", loop->nodes);
        if (func_0058DB90(loop)) {
            changed = TRUE;
            IRO_UpdateFlagsOnInts(IRO_FirstLinear);
            continue;
        }
        testNode = loop->x10;
        removed = func_0058D780(loop);
        if (removed) {
            IRO_Dump("RemoveZeroOrOneIterationLoop: removing loop branch\n");
            RemoveLoopBranch301(testNode);
        }
        if (removed) {
            changed = TRUE;
            IRO_UpdateFlagsOnInts(IRO_FirstLinear);
        }
    }
    func_00589A80(loops);
    if (changed) {
        if (copts.deadcode) {
            IRO_RemoveUnreachable();
        } else {
            IRO_ComputeSuccPred();
            IRO_ComputeDom();
        }
    } else {
        IRO_UpdateFlagsOnInts(IRO_FirstLinear);
    }
    IRO_CheckForUserBreak();
    return changed;
}
extern void func_0059C270(IRONaturalLoop* loop); // IRO_TransformCounterLoop (prepare)
extern void func_0059BDD0(IRONaturalLoop* loop, Boolean* changed); // IRO_TransformCounterLoop
extern void func_0059BAF0(IRONaturalLoop* loop); // counter loop marking
extern void func_0059CCD0(IRONaturalLoop* loop, Boolean* changed); // IRO_OptimizeNonRegAccesses
extern void func_0059CF80(IRONaturalLoop* loop); // IRO_MarkStrengthReductionCandidates
extern void func_0059CE30(IRONaturalLoop* loop, IROLinear* pre); // IRO_DoStrengthReduction

// 0x5895D0: the per-loop transformations of 3.0.1's loop optimizer
void IRO_DoCommonLoopTransformations(void) {
    IRONaturalLoop* loops;
    IRONaturalLoop* loop;
    IRONode* header;
    IROLinear* pre;
    IROExpr* expr;
    IROExpr* prev;
    Boolean changed;
    Boolean transformed;

    loops = FindLoopHeaders();
    for (loop = loops; loop; loop = loop->next) {
        changed = FALSE;
        header = loop->header;
        ComputeLoopBody(loop);
        IRO_Dump(
            "IRO_DoCommonLoopTransformations:transforming loop with header %d\n", header->index
        );
        IRO_DumpBits("Loop includes: ", loop->nodes);
        func_00587540(loop);
        if (func_0058A4F0(loop) && copts.opt_markcounterloops) {
            func_0059C270(loop);
            transformed = FALSE;
            func_0059BDD0(loop, &transformed);
            if (transformed) {
                header = loop->header;
                IRO_RebuildNodeTable();
                func_0058B3D0(loop);
                func_0058B250(loop);
                IRO_DumpAfterPhase("IRO_TransformCounterLoop", FALSE);
                changed = TRUE;
            }
        }
        if (copts.opt_optimizenonregaccess || copts.opt_marknonregtemps) {
            func_0059CCD0(loop, &changed);
            IRO_DumpAfterPhase("IRO_OptimizeNonRegAccesses", FALSE);
        }
        IRO_FirstExpr = NULL;
        if (!func_0058A4F0(loop)) {
            IRO_Dump("No predecessor outside the loop\n");
        } else {
            if (copts.opt_rotateloops && func_0059C350(loop)) {
                IRO_DumpAfterPhase("IRO_RotateLoop", FALSE);
                changed = TRUE;
            }
            func_0058A430(loop, header, loop->x18, &pre, &changed);
            if (pre && (copts.loopinvariants || (copts.strengthreduction && !copts.optimizesize))) {
                func_00591FB0(loop);
                func_005915E0(loop);
                IRO_DumpAfterPhase("IRO_DistributePartiallyInvariantMultiplies", FALSE);
                func_00591CF0(loop);
                IRO_DumpAfterPhase("IRO_MoveInvariantsInAddressExpr", FALSE);
                IRO_ConstantFolding();
                IRO_FindExpressions(loop->nodes, FALSE);
                IRO_DumpExprs();
                if (copts.loopinvariants) {
                    func_005920D0(loop);
                }
                if (copts.strengthreduction && !copts.optimizesize) {
                    IRO_DumpBeforePhase("IRO_MarkStrengthReductionCandidates", FALSE);
                    func_0059CF80(loop);
                    IRO_DumpAfterPhase("IRO_MarkStrengthReductionCandidates", FALSE);
                    func_0059CE30(loop, pre);
                    IRO_DumpAfterPhase("IRO_DoStrengthReduction", FALSE);
                }
                if (copts.loopinvariants) {
                    func_00591E00(loop, pre);
                    IRO_DumpAfterPhase("IRO_MoveInvariantExpressions", FALSE);
                }
            }
        }
        if (copts.opt_markcounterloops) {
            func_0059BAF0(loop);
        }
        prev = NULL;
        for (expr = IRO_FirstExpr; expr; expr = expr->next) {
            if (expr->linear->type == IROLinearNop) {
                if (prev) {
                    prev->next = expr->next;
                } else {
                    IRO_FirstExpr = expr->next;
                }
            } else {
                prev = expr;
            }
        }
        IRO_UpdateFlagsOnInts(IRO_FirstLinear);
        IRO_UpdateVars();
    }
    func_00589A80(loops);
}

#else
void IRO_SetLoopDepth(void) {
    IRONode* fnode;
    IRONode* pred;
    UInt16 i;
    UInt16 isHeader;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        fnode->loopdepth = 0;
    }

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        isHeader = FALSE;

        for (i = 0; i < fnode->numpred; i++) {
            pred = IRO_NodeTable[fnode->pred[i]];
            if (Bv_IsBitSet(fnode->index, pred->dom)) {
                if (!isHeader) {
                    Bv_AllocVector(&InLoop, IRO_NumNodes + 1);
                    Bv_Clear(InLoop);
                    Bv_SetBit(fnode->index, InLoop);
                }
                isHeader = TRUE;
                Bv_SetBit(pred->index, InLoop);
                if (pred != fnode) {
                    AddPreds(pred);
                }
            }
        }

        if (isHeader) {
            IncLoopDepth();
        }
    }

    IRO_CheckForUserBreak();
}
#endif

static void InsertBranchAroundLoopPreheaders(void) {
    IRONode* fnode;
    IROLinear* labelnode;
    IRONode* iter;
    IROLinear* endnode;
    CLabel* label;

    if (IRO_EndNode && IRO_EndNode->last && IRO_EndNode->last->type == IROLinearEnd) {
        label = IRO_NewLabel();
        fnode = IRO_NewFlowGraphNode();
        IRO_LastNode->nextnode = fnode;
        label->stmt = (Statement*)fnode;

#if MWCCPSP
        // 3.0.1 frees the table back to the pools (IRO_RebuildNodeTable)
        IRO_NodeTable = IRO_malloc(sizeof(IRONode*) * IRO_NumNodes);
#else
        IRO_NodeTable = oalloc(sizeof(IRONode*) * IRO_NumNodes);
#endif
        memset(IRO_NodeTable, 0, sizeof(IRONode*) * IRO_NumNodes);
        for (iter = IRO_FirstNode; iter; iter = iter->nextnode) {
            IRO_NodeTable[iter->index] = iter;
        }

        labelnode = IRO_NewLinear(IROLinearLabel);
        labelnode->index = ++IRO_NumLinear;
        labelnode->u.label.label = label;
        labelnode->flags |= IROLF_StartsNode;
        fnode->first = labelnode;
        IRO_LastLinear->next = labelnode;

        endnode = IRO_NewLinear(IROLinearEnd);
        memcpy(endnode, IRO_EndNode->last, sizeof(IROLinear));
        endnode->index = ++IRO_NumLinear;
        endnode->next = NULL;
        fnode->last = endnode;
        labelnode->next = endnode;
        IRO_LastLinear = endnode;

        IRO_EndNode->last->type = IROLinearGoto;
        IRO_EndNode->last->u.label.label = label;
        IRO_EndNode = IRO_LastNode = fnode;

        IRO_ComputeSuccPred();
        IRO_ComputeDom();
    }
}

void IRO_FindLoops(void) {
    IRONode* fnode;
    IRONode* pred;
    UInt16 i;
    UInt16 isHeader;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        isHeader = FALSE;

        for (i = 0; i < fnode->numpred; i++) {
            pred = IRO_NodeTable[fnode->pred[i]];
            if (Bv_IsBitSet(fnode->index, pred->dom)) {
                if (!isHeader) {
                    Bv_AllocVector(&InLoop, IRO_NumNodes + 1);
                    Bv_Clear(InLoop);
                    Bv_SetBit(fnode->index, InLoop);
                }
                isHeader = TRUE;
                Bv_SetBit(pred->index, InLoop);
                if (pred != fnode) {
                    AddPreds(pred);
                }
            }
        }

        if (isHeader) {
            IncLoopDepth();
            IRO_Dump("IRO_FindLoops:Found loop with header %d\n", fnode->index);
            IRO_DumpBits("Loop includes: ", InLoop);
            MyHandleLoop_Motion(fnode);
#if MWCCPSP
            IRO_UpdateFlagsOnInts(IRO_FirstLinear);
#else
            IRO_UpdateFlagsOnInts();
#endif
            MyHandleLoop_Vector(fnode);
            RemoveNoopsFromExprList();
#if MWCCPSP
            IRO_UpdateFlagsOnInts(IRO_FirstLinear);
#else
            IRO_UpdateFlagsOnInts();
#endif
            IRO_UpdateVars();
        }
    }

    if (!IRO_FunctionHasReturn && IRO_EndNode != IRO_LastNode) {
        InsertBranchAroundLoopPreheaders();
    }
}

static void CheckSubableSub(IROLinear* linear, Boolean isFirst) {
    if (isFirst && IRO_IsSubableExpression(linear)) {
        IRO_Dump("Subable Expression is %d\n", linear->index);
        NoSubableSubs = FALSE;
    }
}

static int NoSubableSubExprs(IROLinear* nd) {
    NoSubableSubs = TRUE;
    IRO_WalkTree(nd, CheckSubableSub);
    return NoSubableSubs;
}

void ComputeLoopKills(void) {
    IRONode* fnode;
    IROLinear* nd;

    Bv_AllocVector(&AllKills, IRO_NumVars + 1);
    Bv_AllocVector(&IRO_VarKills, IRO_NumVars + 1);

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (Bv_IsBitSet(fnode->index, InLoop) && (nd = fnode->first)) {
            Bv_AllocVector(&fnode->in, IRO_NumVars + 1);
            Bv_AllocVector(&fnode->gen, IRO_NumVars + 1);
            while (1) {
                Bv_Clear(IRO_VarKills);
                IRO_GetKills(nd);
                Bv_Or(IRO_VarKills, AllKills);
                if (nd == fnode->last) {
                    break;
                }
                nd = nd->next;
            }
        }
    }
}

void ComputeLoopInvariance(void) {
    IRONode* fnode;
    IROLinear* nd;

    IRO_Depends = IRO_VarKills;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (Bv_IsBitSet(fnode->index, InLoop) && (nd = fnode->first)) {
            nd->flags &= ~IROLF_LoopInvariant;
            while (1) {
                if ((nd->flags & IROLF_Reffed) && nd->type != IROLinearNop) {
                    IRO_FindDepends_NoAlloc(nd);
                    if (!IRO_IsVolatile && !Bv_BitsInCommon(IRO_Depends, AllKills)) {
                        nd->flags |= IROLF_LoopInvariant;
                    }

                    if (IRO_CouldError) {
                        nd->flags |= IROLF_CouldError;
                    } else {
                        nd->flags &= ~IROLF_CouldError;
                    }
                }

                if (nd == fnode->last) {
                    break;
                }
                nd = nd->next;
            }
        }
    }
}

void ComputeLoopInduction(void) {
    IRONode* fnode;
    IROLinear* nd;
    Boolean isUpdate;
    Object* obj;
    Object* leftObj;
    Object* rightObj;
    VarRecord* var;
    IROLinear* swapNd;
    IROLoopInd* ind;
    Boolean isUnsigned;

    Bv_AllocVector(&AllKills, IRO_NumVars + 1);
    Bv_AllocVector(&IRO_VarKills, IRO_NumVars + 1);

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (Bv_IsBitSet(fnode->index, InLoop) && (nd = fnode->first)) {
            while (1) {
                Bv_Clear(IRO_VarKills);
                IRO_GetKills(nd);
                Bv_Or(IRO_VarKills, AllKills);
                isUpdate = FALSE;
                if ((nd->type == IROLinearOp2Arg &&
                     nd->rtype->size <= 4 &&
                     (nd->nodetype == EADDASS || nd->nodetype == ESUBASS) &&
                     (obj = IRO_IsVariable(nd->u.diadic.left)) &&
                     obj->type->size == nd->rtype->size &&
                     IS_TYPE_INT(obj->type) &&
                     ((IRO_IsIntConstant(nd->u.diadic.right) &&
                       CInt64_GetULong(&nd->u.diadic.right->u.node->data.intval)) ||
                      (nd->u.diadic.right->flags & IROLF_LoopInvariant))) ||
                    (nd->type == IROLinearOp1Arg &&
                     nd->rtype->size <= 4 &&
                     (nd->nodetype == EPOSTINC ||
                      nd->nodetype == EPOSTDEC ||
                      nd->nodetype == EPREINC ||
                      nd->nodetype == EPREDEC) &&
                     (obj = IRO_IsVariable(nd->u.monadic)) &&
                     obj->type->size == nd->rtype->size &&
                     IS_TYPE_INT(obj->type))) {
                    isUpdate = TRUE;
                } else if (nd->type == IROLinearOp2Arg &&
                           nd->rtype->size <= 4 &&
                           nd->nodetype == EASS &&
                           (obj = IRO_IsVariable(nd->u.diadic.left)) &&
                           obj->type->size == nd->rtype->size &&
                           IS_TYPE_INT(obj->type) &&
                           nd->u.diadic.right->type == IROLinearOp2Arg &&
                           (nd->u.diadic.right->nodetype == EADD ||
                            nd->u.diadic.right->nodetype == ESUB)) {
                    if (nd->u.diadic.right->nodetype == EADD) {
                        leftObj = IRO_IsVariable(nd->u.diadic.right->u.diadic.left);
                        rightObj = IRO_IsVariable(nd->u.diadic.right->u.diadic.right);
                        if (leftObj == obj &&
                            rightObj != obj &&
                            (nd->u.diadic.right->u.diadic.right->flags & IROLF_LoopInvariant)) {
                            isUpdate = TRUE;
                        }

                        if (rightObj == obj &&
                            leftObj != obj &&
                            (nd->u.diadic.right->u.diadic.left->flags & IROLF_LoopInvariant)) {
                            isUpdate = TRUE;
                            swapNd = nd->u.diadic.right->u.diadic.left;
                            nd->u.diadic.right->u.diadic.left = nd->u.diadic.right->u.diadic.right;
                            nd->u.diadic.right->u.diadic.right = swapNd;
                        }
                    } else {
                        leftObj = IRO_IsVariable(nd->u.diadic.right->u.diadic.left);
                        rightObj = IRO_IsVariable(nd->u.diadic.right->u.diadic.right);
                        if (leftObj == obj &&
                            rightObj != obj &&
                            (nd->u.diadic.right->u.diadic.right->flags & IROLF_LoopInvariant)) {
                            isUpdate = TRUE;
                        }
                    }
                }

                if (isUpdate) {
                    if ((var = IRO_FindAssigned(nd))) {
                        if (var->inductionState == 2) {
                            var->inductionState = FALSE;
                        } else if (var->inductionState == 1) {
                            var->inductionState = 2;
                        }
                    }
                } else {
                    for (var = IRO_FirstVar; var; var = var->next) {
                        if (Bv_IsBitSet(var->index, IRO_VarKills)) {
                            var->inductionState = FALSE;
                        }
                    }
                }

                if (nd == fnode->last) {
                    break;
                }
                nd = nd->next;
            }
        }
    }

    IRO_DumpBits("Killed in loop: ", AllKills);

    for (fnode = IRO_FirstNode, FirstInd = NULL; fnode; fnode = fnode->nextnode) {
        if (Bv_IsBitSet(fnode->index, InLoop) && (nd = fnode->first)) {
            while (1) {
                if ((
                        nd->type == IROLinearOp2Arg &&
                        (nd->nodetype == EADDASS || nd->nodetype == ESUBASS || nd->nodetype == EASS)
                    ) ||
                    (nd->type == IROLinearOp1Arg &&
                     (nd->nodetype == EPOSTINC ||
                      nd->nodetype == EPOSTDEC ||
                      nd->nodetype == EPREINC ||
                      nd->nodetype == EPREDEC))) {
                    if ((var = IRO_FindAssigned(nd)) &&
                        var->inductionState == 2 &&
                        var->object->type->size <= 4) {
                        ind = oalloc(sizeof(IROLoopInd));
                        ind->fnode = fnode;
                        ind->var = var;
                        ind->nd = nd;
                        ind->next = FirstInd;
                        ind->addNode = NULL;
                        ind->addConst = 0;
                        ind->flags = 0;

                        if (nd->type == IROLinearOp2Arg) {
                            isUnsigned = is_unsigned(nd->rtype);
                            if (IRO_IsIntConstant(nd->u.diadic.right)) {
                                CInt64 val = nd->u.diadic.right->u.node->data.intval;
                                if (IS_LINEAR_DIADIC(nd, EADDASS) &&
                                    CInt64_Less(val, cint64_zero)) {
                                    nd->nodetype = ESUBASS;
                                    nd->u.diadic.right->u.node->data.intval = CInt64_Neg(val);
                                }
                                if (isUnsigned) {
                                    CInt64_ConvertUInt32(&nd->u.diadic.right->u.node->data.intval);
                                    ind->addConst =
                                        CInt64_GetULong(&nd->u.diadic.right->u.node->data.intval);
                                } else {
                                    CInt64_ConvertInt32(&nd->u.diadic.right->u.node->data.intval);
                                    ind->addConst =
                                        CInt64_GetULong(&nd->u.diadic.right->u.node->data.intval);
                                }
                                ind->addNode = NULL;
                            } else if (nd->nodetype == EADDASS || nd->nodetype == ESUBASS) {
                                ind->addNode = nd->u.diadic.right;
                            } else if (nd->nodetype == EASS) {
                                ind->addNode = nd->u.diadic.right->u.diadic.right;
                            }
                        } else {
                            if (IS_TYPE_POINTER_ONLY(nd->rtype)) {
                                ind->addConst = TPTR_TARGET(nd->rtype)->size;
                            } else {
                                ind->addConst = 1;
                            }
                            ind->addNode = NULL;
                        }

                        FirstInd = ind;

                        if (IS_LINEAR_DIADIC_2(nd, EADDASS, ESUBASS)) {
                            if (nd->u.diadic.right->flags & IROLF_LoopInvariant) {
                                IRO_Dump(
                                    "Found induction variable the new way: %s\n",
                                    var->object->name->name
                                );
                            } else {
                                IRO_Dump(
                                    "Found induction variable the old way: %s\n",
                                    var->object->name->name
                                );
                            }
                        } else if (nd->type == IROLinearOp2Arg &&
                                   (nd->u.diadic.right->nodetype == EADD ||
                                    nd->u.diadic.right->nodetype == ESUB)) {
                            IRO_Dump(
                                "Found induction variable the new way: %s\n",
                                var->object->name->name
                            );
                        } else {
                            IRO_Dump(
                                "Found induction variable the old way: %s\n",
                                var->object->name->name
                            );
                        }
                    }
                }

                if (nd == fnode->last) {
                    break;
                }
                nd = nd->next;
            }
        }
    }
}

static void IRO_ActUnmarkLoopInvariance(IROLinear* linear, Boolean isFirst) {
    if (isFirst) {
        linear->flags &= ~IROLF_LoopInvariant;
    }
}

static void UnmarkSubexpressionsOfInvariantExpressions(void) {
    IROExpr* expr;

    for (expr = IRO_FirstExpr; expr; expr = expr->next) {
        if (Bv_IsBitSet(expr->node->index, InLoop) &&
            !expr->notSubable &&
            (!expr->couldError || expr->node->mustreach) &&
            (expr->linear->flags & IROLF_LoopInvariant)) {
            IRO_WalkTree(expr->linear, IRO_ActUnmarkLoopInvariance);
            expr->linear->flags |= IROLF_LoopInvariant;
        }
    }
}

static void MyHandleLoop_Vector(IRONode* fnode) {
    IRONode* pred;
    UInt16 i;
    IROExpr* expr;
    IROExpr* removedExprs;
    IROExpr* exprnext;
    IROLoopInd* induction;
    IROExpr* exprinner;
    Boolean reused;
    IRONode* fallpred;
    IRONode* preheader;
    int hasOutsidePred;
    IRONode* iter;
    IROExpr* searchris;
    IROLinear* reduceNd1;
    VarRecord* var;
    IROLinear* reduceNd2;

    IRO_FirstExpr = NULL;
    LoopNode = fnode;
    FindMustReach();

    for (var = IRO_FirstVar; var; var = var->next) {
        var->inductionState = TRUE;
    }

    ComputeLoopKills();
    ComputeLoopInvariance();
    ComputeLoopInduction();
    LoopNode = fnode;
    ConditionalHeaderAtBottom = FALSE;

    fallpred = NULL;
    hasOutsidePred = FALSE;
    preheader = NULL;
    for (i = 0; i < LoopNode->numpred; i++) {
        pred = IRO_NodeTable[LoopNode->pred[i]];
        if (!Bv_IsBitSet(pred->index, InLoop)) {
            if (!hasOutsidePred) {
                preheader = pred;
            } else {
                preheader = NULL;
            }
            hasOutsidePred = TRUE;
            if (pred->nextnode == fnode) {
                CError_ASSERT(2377, fallpred == NULL || pred == fallpred);
                fallpred = pred;
            }
        }
    }

    if (!hasOutsidePred) {
        IRO_Dump("No predecessor outside the loop\n");
        return;
    }

    if (!preheader || preheader->last->type != IROLinearGoto) {
        preheader = CreatePreHeader(fnode, fallpred);
    }
    PredInt = preheader->last;

    if (PredInt->type == IROLinearGoto &&
        PredInt->u.label.label != LoopNode->first->u.label.label) {
        if (!KnownBounds || !Times) {
            for (iter = IRO_FirstNode; iter; iter = iter->nextnode) {
                iter->mustreach = FALSE;
            }
        } else {
            PredInt->u.label.label = LoopNode->first->u.label.label;
            IRO_ComputeSuccPred();
        }
    }

    if (copts.loopinvariants || copts.strengthreduction) {
        MoveInvarianceInAddressExpr();
        IRO_DumpAfterPhase("MoveInvarianceInAddressExpr", FALSE);
        IRO_FindExpressions(InLoop, TRUE);
        IRO_DumpExprs();
    }

    if (copts.loopinvariants) {
        UnmarkSubexpressionsOfInvariantExpressions();
    }

    if (!copts.optimizesize && copts.strengthreduction) {
        for (expr = IRO_FirstExpr; expr; expr = expr->next) {
            if (!(expr->flags & 4) &&
                Bv_IsBitSet(expr->node->index, InLoop) &&
                !expr->notSubable &&
                (!expr->couldError || expr->node->mustreach) &&
                Reducable(expr->linear, &reduceNd1, &reduceNd2, &var)) {
                IRO_WalkTree(expr->linear, IRO_ActUnmarkRISCandidate);
                expr->linear->flags |= IROLF_Ris;
                expr->factor = reduceNd1;
                expr->var = var;
                expr->indOperand = reduceNd2;
            }
        }
    }

    if (!copts.optimizesize && copts.strengthreduction) {
        RisList = NULL;
        for (expr = IRO_FirstExpr; expr; expr = exprnext) {
            exprnext = expr->next;
            if (!(expr->flags & 4) && (expr->linear->flags & IROLF_Ris)) {
                reduceNd1 = expr->factor;
                var = expr->var;
                reduceNd2 = expr->indOperand;

                induction = FirstInd;
                while (induction && induction->var != var) {
                    induction = induction->next;
                }
                CError_ASSERT(3035, induction != NULL);

                IRO_FindDepends(reduceNd1);
                if (!Bv_BitsInCommon(IRO_Depends, AllKills)) {
                    IRO_Dump("Found reduction in strength: %d\n", expr->linear->index);
                    if (IS_LINEAR_DIADIC(expr->linear, ESHL)) {
                        expr->linear->nodetype = EMUL;
                        CInt64_SetULong(
                            &reduceNd1->u.node->data.intval,
                            1 << CInt64_GetULong(&reduceNd1->u.node->data.intval)
                        );
                        reduceNd1->u.node->rtype = expr->linear->rtype;
                    }

                    searchris = RisList;
                    while (1) {
                        if (!searchris) {
                            break;
                        }
                        if (IRO_ExprsSame(expr->linear, searchris->linear)) {
                            break;
                        }
                        searchris = searchris->next;
                    }

                    if (searchris) {
                        IRO_Dump("Using existing RIS: %d\n", searchris->linear->index);
                        IRO_WalkTree(expr->linear, IRO_RemoveExpr_Action);
                    } else {
                        searchris = CreateRIS(expr, reduceNd2, reduceNd1, induction, FALSE);
                    }
                    IRO_ReplaceReference(expr->linear, searchris->temp, expr->linear);
                    IRO_ClipExpr(expr);
                }
            }
        }
    }

    RisList = NULL;

    expr = IRO_FirstExpr;
    removedExprs = NULL;
    if (copts.loopinvariants) {
        while (expr) {
            exprnext = expr->next;
            reused = FALSE;

            if (Bv_IsBitSet(expr->node->index, InLoop) &&
                !expr->notSubable &&
                (!expr->couldError || expr->node->mustreach) &&
                (expr->linear->flags & IROLF_LoopInvariant) &&
                !(expr->flags & 4)) {
                IRO_Dump("Found loop invariant: %d\n", expr->linear->index);

                for (exprinner = removedExprs; exprinner; exprinner = exprinner->next) {
                    if (IRO_ExprsSame(exprinner->linear, expr->linear)) {
                        IRO_ReplaceReference(expr->linear, exprinner->temp, expr->linear);
                        IRO_NopOut(expr->linear);
                        IRO_Dump("Using already removed expr: %d\n", exprinner->linear->index);
                        IRO_RemoveExpr(expr);
                        reused = TRUE;
                        break;
                    }
                }

                if (!reused && !expr->temp) {
                    IRO_GetTemp(expr);
                    IRO_ReplaceReference(expr->linear, expr->temp, expr->linear);
                    IRO_MoveExpression(expr, PredInt);
                    IRO_AssignToTemp(expr);
                    IRO_RemoveExpr(expr);
                    expr->next = removedExprs;
                    removedExprs = expr;
                }
            }

            expr = exprnext;
        }
    }
}

static void MyHandleLoop_Motion(IRONode* fnode) {
    IROLoopMemRef* memref;
    IRONode* pred;
    UInt16 i;
    IRONode* fallpred;
    IRONode* preheader;
    int hasOutsidePred;
    IRONode* iter;
    IROLinear* ass;
    Object* tempobj;
    VarRecord* var;
    Boolean addressesOk;
    IROLoop* loop;
    IROList list;
    IROElmList* refiter;

    IRO_FirstExpr = NULL;
    LoopNode = fnode;
    FindMustReach();

    for (var = IRO_FirstVar; var; var = var->next) {
        var->inductionState = TRUE;
    }

    ComputeLoopKills();
    ComputeLoopInvariance();
    ComputeLoopInduction();
    LoopNode = fnode;
    ConditionalHeaderAtBottom = FALSE;

    fallpred = NULL;
    hasOutsidePred = FALSE;
    preheader = NULL;
    for (i = 0; i < LoopNode->numpred; i++) {
        pred = IRO_NodeTable[LoopNode->pred[i]];
        if (!Bv_IsBitSet(pred->index, InLoop)) {
            if (!hasOutsidePred) {
                preheader = pred;
            } else {
                preheader = NULL;
            }
            hasOutsidePred = TRUE;
            if (pred->nextnode == fnode) {
                CError_ASSERT(3388, fallpred == NULL || pred == fallpred);
                fallpred = pred;
            }
        }
    }

    if (!hasOutsidePred) {
        IRO_Dump("No predecessor outside the loop\n");
        return;
    }

    if (!preheader || preheader->last->type != IROLinearGoto) {
        preheader = CreatePreHeader(fnode, fallpred);
    }
    PredInt = preheader->last;

    if (PredInt->type == IROLinearGoto &&
        PredInt->u.label.label != LoopNode->first->u.label.label) {
        if (!KnownBounds || !Times) {
            for (iter = IRO_FirstNode; iter; iter = iter->nextnode) {
                iter->mustreach = FALSE;
            }
        } else {
            PredInt->u.label.label = LoopNode->first->u.label.label;
            IRO_ComputeSuccPred();
        }
    }

    if (copts.loopinvariants || copts.strengthreduction) {
        MoveInvarianceInAddressExpr();
        IRO_DumpAfterPhase("MoveInvarianceInAddressExpr", FALSE);
        IRO_FindExpressions(InLoop, FALSE);
        IRO_DumpExprs();
    }

    if (copts.loopinvariants) {
        UnmarkSubexpressionsOfInvariantExpressions();
    }

    addressesOk = TRUE;
    Bv_AllocVector(&InLoop_Exits, IRO_NumNodes + 1);
    Bv_AllocVector(&InLoop_Tails, IRO_NumNodes + 1);
    LoopExitNumber = 0;
    LoopExitSuccessor = NULL;
    LoopTailNum = 0;
    LoopTail = NULL;
    IRO_FindLoopTailsAndExits(fnode);
    FindMustReach1(fnode);

    loop = NULL;
    if (LoopTailNum == 1) {
        if (fnode->last->type == IROLinearIf || fnode->last->type == IROLinearIfNot) {
            loop = ExtractLoopInfo(fnode);
        } else if (LoopTail->last->type == IROLinearIf || LoopTail->last->type == IROLinearIfNot) {
            loop = ExtractLoopInfo(LoopTail);
        }
    }

    if (loop && !(loop->flags & LP_IFEXPR_NON_CANONICAL) && LoopExitNumber == 1) {
        IRO_LoopMemRefFirst = NULL;
        IRO_LoopMemRefCurrent = NULL;
        CheckAllLoopAddresses(IRO_FirstNode, InLoop, &addressesOk);
        CheckAllLoopAddresses(IRO_FirstNode, InLoop, &addressesOk);
        if (addressesOk) {
            for (memref = IRO_LoopMemRefFirst; memref; memref = memref->next) {
                if (memref->flags & LoopMemRef_Speculative) {
                    memref->flags |= LoopMemRef_Invalid;
                }
            }

            for (memref = IRO_LoopMemRefFirst; memref; memref = memref->next) {
                IRO_Dump("Loop Motion Candidate:: Int= %d", memref->nd->index);
                if (memref->flags & LoopMemRef_Invalid) {
                    IRO_Dump("<invalid>");
                }
                if (memref->flags & LoopMemRef_Load) {
                    IRO_Dump("<load>");
                }
                if (memref->flags & LoopMemRef_Store) {
                    IRO_Dump("<store>");
                }
                IRO_Dump("\n");

                if (!(memref->flags & LoopMemRef_Invalid)) {
                    tempobj = create_temp_object(memref->nd->rtype);
                    IRO_FindVar(tempobj, TRUE, TRUE);
                    if (IRO_FindVar(
                            ((IROLinear*)memref->rec->objRefs->element)->u.node->data.objref,
                            FALSE,
                            TRUE
                        )) {
                        IRO_InitList(&list);
                        ass = IRO_NewLinear(IROLinearOp2Arg);
                        ass->index = ++IRO_NumLinear;
                        ass->rtype = memref->nd->rtype;
                        ass->nodetype = EASS;
                        ass->u.diadic.left = IRO_TempReference(tempobj, &list);
                        ass->u.diadic.left->flags |= IROLF_Ind | IROLF_Assigned;
                        ass->u.diadic.left->u.monadic->flags |= IROLF_Ind | IROLF_Assigned;
                        ass->u.diadic.right = IRO_DuplicateExpr(memref->nd, &list);
                        IRO_AddToList(ass, &list);
                        IRO_Paste(list.head, list.tail, PredInt);
                    } else {
                        CError_FATAL(3631);
                    }

                    if (LoopExitSuccessor->numpred != 1) {
                        LoopExitSuccessor = CreateNewLoopExitSuccessor(LoopExitSuccessor);
                    }

                    IRO_InitList(&list);
                    ass = IRO_NewLinear(IROLinearOp2Arg);
                    ass->index = ++IRO_NumLinear;
                    ass->rtype = memref->nd->rtype;
                    ass->nodetype = EASS;
                    ass->u.diadic.left = IRO_DuplicateExpr(memref->nd, &list);
                    ass->u.diadic.left->flags |= IROLF_Ind | IROLF_Assigned;
                    ass->u.diadic.left->u.monadic->flags |= IROLF_Ind | IROLF_Assigned;
                    ass->u.diadic.right = IRO_TempReference(tempobj, &list);
                    IRO_AddToList(ass, &list);

                    if (LoopExitSuccessor->first->type == IROLinearLabel) {
                        IRO_PasteAfter(list.head, list.tail, LoopExitSuccessor->first);
                    } else {
                        IRO_Paste(list.head, list.tail, LoopExitSuccessor->first);
                    }

                    for (refiter = memref->list; refiter; refiter = refiter->next) {
                        IRO_Dump(
                            "Loop Motion Candidate Reference at Int= %d\n",
                            ((IROLinear*)refiter->element)->index
                        );
                        IRO_InitList(&list);
                        IRO_TempReference(tempobj, &list);
                        IRO_Paste(list.head, list.tail, refiter->element);
                        IRO_LocateFather_Cut_And_Paste(refiter->element, list.tail);
                    }
                }
            }
        }
    }

    IRO_DumpAfterPhase("After Motion", FALSE);
}

static Boolean CheckLoopAddress(IROLinear* nd, Boolean mustreach1) {
    IROLoopMemRef* memref;
    IROAddrRecord* rec;
    IROAddrRecord* otherrec;
    IROLinear* inner;
    IROElmList* list;
    Boolean cannotSpeculate;

    inner = nd->u.monadic;
    rec = IRO_InitAddrRecordPointer(inner);
    if (IS_LINEAR_ENODE(inner, EOBJREF)) {
        rec->numObjRefs++;
        IRO_AddElmToList(inner, &rec->objRefs);
    } else if (IS_LINEAR_DIADIC(inner, EADD)) {
        IRO_DecomposeAddressExpression(inner, rec);
    } else {
        return FALSE;
    }

    if (rec->numObjRefs != 1) {
        return FALSE;
    }

    cannotSpeculate = (nd->flags & IROLF_CouldError) || ((nd->flags & IROLF_Reffed) == 0);
    if (!IRO_LoopMemRefFirst) {
        MakeLoopEntry(nd, rec, mustreach1, cannotSpeculate);
    } else {
        for (memref = IRO_LoopMemRefFirst; memref; memref = memref->next) {
            otherrec = memref->rec;
            if (((IROLinear*)rec->objRefs->element)->u.node->data.objref ==
                ((IROLinear*)otherrec->objRefs->element)->u.node->data.objref) {
                if (IRO_ExprsSame(inner, otherrec->linear)) {
                    list = oalloc(sizeof(IROElmList));
                    list->element = nd;
                    list->next = NULL;
                    if (!memref->list) {
                        memref->list = list;
                    } else {
                        list->next = memref->list;
                        memref->list = list;
                    }
                } else {
                    memref->flags |= LoopMemRef_Invalid;
                }

                if (!mustreach1 && cannotSpeculate) {
                    memref->flags |= LoopMemRef_Invalid;
                }
                if (mustreach1 || cannotSpeculate) {
                    memref->flags &= ~LoopMemRef_Speculative;
                }
                break;
            }
        }

        if (!memref) {
            MakeLoopEntry(nd, rec, mustreach1, cannotSpeculate);
        }
    }

    return TRUE;
}

static void CheckAllLoopAddresses(IRONode* fnode, BitVector* bv, Boolean* resultFlag) {
    IROLinear* nd;
    Boolean ok;

    ok = *resultFlag;
    while (fnode) {
        if (Bv_IsBitSet(fnode->index, bv) && (nd = fnode->first)) {
            while (ok) {
                if (nd->type == IROLinearOp1Arg && nd->nodetype == EINDIRECT) {
                    ok = CheckLoopAddress(nd, fnode->mustreach1);
                } else if ((nd->type == IROLinearFunccall &&
                            (nd->type == IROLinearFunccall &&
                             !(nd->u.funccall.functype->flags & FUNC_INTRINSIC))) ||
                           nd->type == IROLinearAsm) {
                    ok = FALSE;
                }

                if (nd == fnode->last) {
                    break;
                }
                nd = nd->next;
            }

            if (!ok) {
                break;
            }
        }
        fnode = fnode->nextnode;
    }
    *resultFlag = ok;
}

static void MakeLoopEntry(
    IROLinear* nd, IROAddrRecord* rec, Boolean mustreach1, Boolean cannotSpeculate
) {
    IROElmList* list;

    if (!IRO_IsRegable(((IROLinear*)rec->objRefs->element)->u.node->data.objref) &&
        (nd->u.monadic->flags & IROLF_LoopInvariant) &&
        !IRO_HasSideEffect(nd)) {
        if (!IRO_LoopMemRefFirst) {
            IRO_LoopMemRefCurrent = IRO_LoopMemRefFirst = oalloc(sizeof(IROLoopMemRef));
        } else {
            IRO_LoopMemRefCurrent->next = oalloc(sizeof(IROLoopMemRef));
            IRO_LoopMemRefCurrent = IRO_LoopMemRefCurrent->next;
        }
        IRO_LoopMemRefCurrent->flags = LoopMemRef_Speculative;
        IRO_LoopMemRefCurrent->nd = nd;
        IRO_LoopMemRefCurrent->rec = rec;
        IRO_LoopMemRefCurrent->next = NULL;
        IRO_LoopMemRefCurrent->list = NULL;

        list = oalloc(sizeof(IROElmList));
        list->element = nd;
        list->next = NULL;
        if (!IRO_LoopMemRefCurrent->list) {
            IRO_LoopMemRefCurrent->list = list;
        } else {
            list->next = IRO_LoopMemRefCurrent->list;
            IRO_LoopMemRefCurrent->list = list;
        }

        if (((IROLinear*)rec->objRefs->element)->flags & IROLF_Assigned) {
            IRO_LoopMemRefCurrent->flags |= LoopMemRef_Store;
            if (((IROLinear*)rec->objRefs->element)->flags & IROLF_Used) {
                IRO_LoopMemRefCurrent->flags |= LoopMemRef_Load;
            }
        } else {
            IRO_LoopMemRefCurrent->flags |= LoopMemRef_Load;
        }

        if (!mustreach1 && cannotSpeculate) {
            IRO_LoopMemRefCurrent->flags |= LoopMemRef_Invalid;
        }

        if (mustreach1 || cannotSpeculate) {
            IRO_LoopMemRefCurrent->flags &= ~LoopMemRef_Speculative;
        }
    }
}

void FindAssignmenttoInductionVar(IROLoop* loop, IRONode* fnode) {
    IROLinear* nd;
    UInt32 index;

    if (!loop->induction) {
        loop->init = NULL;
        return;
    }

    index = loop->induction->var->index;
    for (nd = fnode->first; nd; nd = nd->next) {
        Bv_Clear(IRO_VarKills);
        IRO_GetKills(nd);
        if (Bv_IsBitSet(index, IRO_VarKills)) {
            loop->init = nd;
        }

        if (nd == fnode->last) {
            break;
        }
    }

    if (loop->init && loop->init->type != IROLinearOp2Arg) {
        loop->init = NULL;
    }
}

static void ComputeIntWeight(IROLoop* loop, IROLinear* Int) {
    loop->weight++;
}

static IROLinear* IsTypconVar(IROLinear* nd) {
    if (IS_LINEAR_MONADIC(nd, ETYPCON) && IRO_IsVariable(nd->u.monadic)) {
        return nd->u.monadic;
    } else {
        return NULL;
    }
}

IROLoop* ExtractLoopInfo(IRONode* fnode) {
    Boolean hasCalls;
    Boolean hasCntrlFlow;
    Boolean gotoCount;
    UInt32 assignCount;
    IROLoopInd* testInd;
    Object* obj;
    IROLinear* nd;
    IRONode* scanfnode;
    IROLinear* testLeft;
    IROLinear* step;
    IROLinear* scannd;
    IROLinear* scannd2;
    IROLinear* condStart;
    VarRecord* var;
    IROLoopInd* scanind;
    IROLoopInd* ind;
    IROLinear* testRight;
    IROLinear* left;
    IROLinear* right;
    IROLinear* right2;
    UInt32 inductionCount;
    IROLoop* loop;
    UInt32 i;
    SInt32 hi;

    hasCalls = FALSE;
    hasCntrlFlow = FALSE;
    gotoCount = FALSE;
    assignCount = 0;
    LoopNode = fnode;

    loop = oalloc(sizeof(IROLoop));
    loop->fnode = fnode;
    nd = LoopNode->last->u.label.cond;
    loop->cond = nd;
    loop->flags = 0;
    loop->unused = 0;
    loop->init = NULL;
    loop->induction = NULL;
    loop->lo = -1;
    loop->hi = -1;
    loop->weight = 0;
    IRO_RenumberInts();

    if (nd->type == IROLinearOp2Arg && IS_TYPE_INT(nd->rtype)) {
        testInd = NULL;
        testLeft = nd->u.diadic.left;
        testRight = nd->u.diadic.right;
        if (IRO_IsVariable(testLeft) || (testLeft = IsTypconVar(testLeft))) {
            if ((var = IRO_FindVar(testLeft->u.monadic->u.node->data.objref, FALSE, TRUE))) {
                scanind = FirstInd;
                while (scanind && scanind->var != var) {
                    scanind = scanind->next;
                }
                if (scanind) {
                    testInd = scanind;
                    loop->flags |= LP_INDUCTION_AT_LEFT;
                    loop->induction = scanind;
                }
            }
        }

        if (IRO_IsVariable(testRight) || (testRight = IsTypconVar(testRight))) {
            if ((var = IRO_FindVar(testRight->u.monadic->u.node->data.objref, FALSE, TRUE))) {
                scanind = FirstInd;
                while (scanind && scanind->var != var) {
                    scanind = scanind->next;
                }
                if (scanind) {
                    testInd = scanind;
                    loop->flags &= ~LP_INDUCTION_AT_LEFT;
                    loop->induction = scanind;
                }
            }
        }

        if (testInd && testInd->addConst > 0) {
            if (loop->flags & LP_INDUCTION_AT_LEFT) {
                if (loop->cond->type != IROLinearOp2Arg ||
                    !(loop->cond->nodetype == ELESS ||
                      loop->cond->nodetype == EGREATER ||
                      loop->cond->nodetype == ELESSEQU ||
                      loop->cond->nodetype == EGREATEREQU) ||
                    !loop->cond->u.diadic.left ||
                    !loop->cond->u.diadic.left->rtype ||
                    !IS_TYPE_INT(loop->cond->u.diadic.left->rtype) ||
                    !loop->cond->u.diadic.right ||
                    !loop->cond->u.diadic.right->rtype ||
                    !IS_TYPE_INT(loop->cond->u.diadic.right->rtype)) {
                    loop->flags |= LP_IFEXPR_NON_CANONICAL;
                    return loop;
                }
            } else {
                if (loop->cond->type == IROLinearOp2Arg &&
                    (loop->cond->nodetype == EGREATER || loop->cond->nodetype == EGREATEREQU) &&
                    (testRight = loop->cond->u.diadic.left) &&
                    testRight->rtype &&
                    IS_TYPE_INT(testRight->rtype) &&
                    (right = loop->cond->u.diadic.right) &&
                    right->rtype &&
                    IS_TYPE_INT(right->rtype)) {
                    loop->cond->u.diadic.left = right;
                    loop->cond->u.diadic.right = testRight;
                    if (loop->cond->nodetype == EGREATER) {
                        loop->cond->nodetype = ELESS;
                    } else if (loop->cond->nodetype == EGREATEREQU) {
                        loop->cond->nodetype = ELESSEQU;
                    }
                    loop->flags |= LP_INDUCTION_AT_LEFT;
                } else if (loop->cond->type == IROLinearOp2Arg &&
                           (loop->cond->nodetype == ELESS || loop->cond->nodetype == ELESSEQU) &&
                           (left = loop->cond->u.diadic.left) &&
                           left->rtype &&
                           IS_TYPE_INT(left->rtype) &&
                           (right2 = loop->cond->u.diadic.right) &&
                           right2->rtype &&
                           IS_TYPE_INT(right2->rtype)) {
                    loop->cond->u.diadic.left = right2;
                    loop->cond->u.diadic.right = left;
                    if (loop->cond->nodetype == ELESS) {
                        loop->cond->nodetype = EGREATER;
                    } else if (loop->cond->nodetype == ELESSEQU) {
                        loop->cond->nodetype = EGREATEREQU;
                    }
                    loop->flags |= LP_INDUCTION_AT_LEFT;
                } else {
                    loop->flags |= LP_IFEXPR_NON_CANONICAL;
                    return loop;
                }
            }
        } else {
            loop->flags |= LP_INDUCTION_NOT_FOUND;
            return loop;
        }
    } else if (nd->type == IROLinearOp1Arg && IS_TYPE_INT(nd->rtype)) {
        if (nd->nodetype == EPREINC ||
            nd->nodetype == EPOSTINC ||
            nd->nodetype == EPREDEC ||
            nd->nodetype == EPOSTDEC) {
            if (IRO_IsVariable(nd->u.monadic)) {
                if ((var =
                         IRO_FindVar(nd->u.monadic->u.monadic->u.node->data.objref, FALSE, TRUE))) {
                    scanind = FirstInd;
                    while (scanind && scanind->var != var) {
                        scanind = scanind->next;
                    }
                    if (scanind) {
                        testInd = scanind;
                        loop->flags |= LP_WHILE_LOOP;
                        loop->induction = scanind;
                    } else {
                        loop->flags |= LP_INDUCTION_NOT_FOUND;
                        return loop;
                    }
                } else {
                    loop->flags |= LP_INDUCTION_NOT_FOUND;
                    return loop;
                }
            } else {
                loop->flags |= LP_INDUCTION_NOT_FOUND;
                return loop;
            }
        } else {
            loop->flags |= LP_INDUCTION_NOT_FOUND;
            return loop;
        }
    } else {
        loop->flags |= LP_IFEXPR_NON_CANONICAL;
        return loop;
    }

    inductionCount = 0;
    scanind = FirstInd;
    while (scanind) {
        inductionCount++;
        scanind = scanind->next;
    }

    if (inductionCount > 1) {
        loop->flags |= LP_HAS_MULTIPLE_INDUCTIONS;
    }

    if ((ind = loop->induction)) {
        nd = ind->nd;
        if (nd->type == IROLinearOp2Arg) {
            if (IS_LINEAR_DIADIC_2(loop->cond, ELESS, ELESSEQU)) {
                if (nd->nodetype == EADDASS) {
                    if (ind->addConst == 1) {
                        loop->flags |= LP_LOOP_STEP_ISADD;
                    }
                    if (ind->addConst > 0) {
                        loop->flags |= LP_LOOP_STEP_ISPOS;
                    }
                } else if (nd->nodetype == EASS &&
                           IS_LINEAR_DIADIC(nd->u.diadic.right, EADD) &&
                           IRO_IsIntConstant(step = nd->u.diadic.right->u.diadic.right) &&
                           (hi = step->u.node->data.intval.hi) == 0) {
                    if (CInt64_GetULong(&step->u.node->data.intval) == 1) {
                        loop->flags |= LP_LOOP_STEP_ISADD;
                    }
                    if (CInt64_GetULong(&step->u.node->data.intval) > 0) {
                        loop->flags |= LP_LOOP_STEP_ISPOS;
                    }
                }
            }
        } else if (nd->type == IROLinearOp1Arg) {
            if (nd->nodetype == EPREINC || nd->nodetype == EPOSTINC) {
                if (ind->addConst == 1) {
                    loop->flags |= LP_LOOP_STEP_ISADD;
                }
                if (ind->addConst > 0) {
                    loop->flags |= LP_LOOP_STEP_ISPOS;
                }
            }
            if (nd->nodetype == EPREDEC || nd->nodetype == EPOSTDEC) {
                if (ind->addConst == 1) {
                    loop->flags |= LP_STEP_IS_NEG_ONE;
                }
                if (ind->addConst > 0) {
                    loop->flags |= LP_LOOP_STEP_ISNEG;
                }
            }
        }
        loop->hi = nd->index;
        loop->lo = IRO_FindStart(nd)->index;
    }

    if (testInd) {
        condStart = IRO_FindStart(fnode->last->u.label.cond);
        loop->flags |= LP_HEADER_FOLLOWS_UPDATE;
        if (loop->flags & LP_WHILE_LOOP) {
            for (scannd = loop->fnode->first; scannd && scannd != condStart;
                 scannd = scannd->next) {
                if (scannd->type != IROLinearLabel && scannd->type != IROLinearNop) {
                    loop->flags &= ~LP_HEADER_FOLLOWS_UPDATE;
                }
            }
        } else {
            for (scannd = testInd->nd->next; scannd && scannd != condStart; scannd = scannd->next) {
                if (scannd->type != IROLinearLabel && scannd->type != IROLinearNop) {
                    loop->flags &= ~LP_HEADER_FOLLOWS_UPDATE;
                }
            }
            for (scannd2 = loop->fnode->first; scannd2 && scannd2 != condStart;
                 scannd2 = scannd2->next) {
                if ((scannd2->index < loop->lo || scannd2->index > loop->hi) &&
                    scannd2->type != IROLinearLabel &&
                    scannd2->type != IROLinearNop) {
                    loop->flags &= ~LP_HEADER_FOLLOWS_UPDATE;
                }
            }
        }
    }

    for (scanfnode = IRO_FirstNode; scanfnode; scanfnode = scanfnode->nextnode) {
        if (Bv_IsBitSet(scanfnode->index, InLoop) &&
            scanfnode != fnode &&
            (scannd2 = scanfnode->first)) {
            while (1) {
                if (scannd2->type == IROLinearFunccall) {
                    hasCalls = TRUE;
                }
                if (scannd2->type == IROLinearGoto) {
                    gotoCount++;
                }
                if (gotoCount >= 1) {
                    hasCntrlFlow = TRUE;
                }
                if (scannd2->type == IROLinearIf ||
                    scannd2->type == IROLinearIfNot ||
                    scannd2->type == IROLinearSwitch ||
                    scannd2->type == IROLinearReturn) {
                    hasCntrlFlow = TRUE;
                }

                if (scannd2->type == IROLinearAsm) {
                    loop->flags |= LP_LOOP_HAS_ASM;
                }

                if (!(scannd2->flags & IROLF_Reffed) &&
                    scannd2->type != IROLinearNop &&
                    scannd2->type != IROLinearLabel) {
                    if (!IRO_IsAssignOp[scannd2->nodetype]) {
                        loop->flags |= LP_HAS_NONASSIGN;
                    } else if (scannd2->index < loop->lo || scannd2->index > loop->hi) {
                        assignCount++;
                        if (IsAssignmentReductionCandidate(scannd2) && assignCount == 1) {
                            loop->flags |= LP_IS_REDUCTION_CAND;
                        }
                        if (assignCount > 1) {
                            loop->flags &= ~LP_IS_REDUCTION_CAND;
                        }
                    }
                }

                if (scannd2->index < loop->lo || scannd2->index > loop->hi) {
                    if (IS_LINEAR_ENODE(scannd2, EOBJREF)) {
                        obj = scannd2->u.node->data.objref;
                        if ((scannd2->flags & IROLF_Ind) &&
                            obj &&
                            obj == loop->induction->var->object) {
                            if (!(scannd2->flags & IROLF_Assigned) ||
                                (scannd2->flags & IROLF_Used)) {
                                IRO_Dump("Induction Used in loop\n");
                                loop->flags |= LP_IND_USED_IN_LOOP;
                            }
                        }
                    }

                    if (IS_LINEAR_DIADIC(scannd2, ESHR) &&
                        (obj = IRO_IsVariable(scannd2->u.diadic.left)) &&
                        IRO_IsIntConstant(scannd2->u.diadic.right)) {
                        for (scanind = FirstInd; scanind; scanind = scanind->next) {
                            if (scanind->var->object == obj) {
                                IRO_Dump("Induction has DIV: %s\n", obj->name->name);
                                scanind->flags |= LoopInd_HasDiv;
                            }
                        }
                    }

                    if (IS_LINEAR_DIADIC(scannd2, EAND) &&
                        (obj = IRO_IsVariable(scannd2->u.diadic.left)) &&
                        IRO_IsIntConstant(scannd2->u.diadic.right)) {
                        for (scanind = FirstInd; scanind && obj; scanind = scanind->next) {
                            if (scanind->var->object == obj) {
                                IRO_Dump("Induction has MOD: %s\n", obj->name->name);
                                scanind->flags |= LoopInd_HasMod;
                            }
                        }
                    }
                }

                ComputeIntWeight(loop, scannd2);
                if (scannd2 == scanfnode->last) {
                    break;
                }
                scannd2 = scannd2->next;
            }
        }

        if (hasCalls) {
            loop->flags |= LP_LOOP_HAS_CALLS;
        }
        if (hasCntrlFlow) {
            loop->flags |= LP_LOOP_HAS_CNTRLFLOW;
        }

        for (i = 0; i < scanfnode->numsucc && scanfnode != fnode; i++) {
            if (Bv_IsBitSet(scanfnode->index, InLoop) && !Bv_IsBitSet(scanfnode->succ[i], InLoop)) {
                IRO_Dump(
                    "Node %d has an out of loop successor %d\n",
                    scanfnode->index,
                    scanfnode->succ[i]
                );
                IRO_DumpBits("Loop includes: ", InLoop);
                IRO_Dump("loop has multiple exits\n");
                loop->flags |= LP_HAS_MULTIPLE_EXITS;
            }
        }
    }

    return loop;
}

CLabel* BuildLabel(IROList* list) {
    IROLinear* nd;

    nd = IRO_NewLinear(IROLinearLabel);
    nd->index = IRO_NumLinear++;
    nd->u.label.label = IRO_NewLabel();
    nd->flags |= IROLF_StartsNode;
    IRO_AddToList(nd, list);
    return nd->u.label.label;
}

static void MoveInvarianceInAddressExpr(void) {
    IROLinear* nd;
    IRONode* fnode;
    IROLinear* start;
    IROList list;

    IRO_FirstExpr = IRO_LastExpr = NULL;
    IRO_NumExprs = 0;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (Bv_IsBitSet(fnode->index, InLoop)) {
            for (nd = fnode->first; nd; nd = nd->next) {
                if (nd->type == IROLinearOp1Arg &&
                    (nd->nodetype == EINDIRECT || nd->nodetype == EINDIRECT) &&
                    IS_LINEAR_DIADIC(nd->u.monadic, EADD)) {
                    RearrangeInvarianceInAddressExpr(nd->u.monadic, &list);
                    start = IRO_FindStart(nd->u.monadic);
                    IRO_LocateFather_Cut_And_Paste(nd->u.monadic, list.tail);
                    IRO_Paste(list.head, list.tail, start);
                }

                if (nd == fnode->last) {
                    break;
                }
            }
        }
    }

#if MWCCPSP
    IRO_UpdateFlagsOnInts(IRO_FirstLinear);
#else
    IRO_UpdateFlagsOnInts();
#endif
    ComputeLoopInvariance();
}

static void AddAddendLinear(IROLinear* nd) {
    IROElmList* list;

    if (IS_LINEAR_DIADIC(nd, EADD)) {
        AddAddendLinear(nd->u.diadic.left);
        AddAddendLinear(nd->u.diadic.right);
    } else {
        list = oalloc(sizeof(IROElmList));
        list->element = nd;
        list->next = NULL;
        if (FirstAddendLinear) {
            LastAddendLinear->next = list;
        } else {
            FirstAddendLinear = list;
        }
        LastAddendLinear = list;
    }
}

static IROLinear* RearrangeInvarianceInAddressExpr(IROLinear* nd, IROList* list) {
    IROLinear* scannd;
    IROLinear* result;
    IROElmList* scanlist;
    IROElmList* elist;

    IRO_InitList(list);
    FirstAddendLinear = LastAddendLinear = NULL;
    AddAddendLinear(nd->u.diadic.left);
    AddAddendLinear(nd->u.diadic.right);

    result = NULL;
    for (scanlist = FirstAddendLinear, elist = NULL; scanlist; scanlist = scanlist->next) {
        scannd = scanlist->element;
        if (!IRO_IsIntConstant(scannd) && (scannd->flags & IROLF_LoopInvariant)) {
            if (result) {
                IROLinear* sum = IRO_NewLinear(IROLinearOp2Arg);
                sum->index = ++IRO_NumLinear;
                sum->nodetype = EADD;
                sum->u.diadic.left = result;
                sum->u.diadic.right = IRO_DuplicateExpr(scannd, list);
                IRO_AddToList(sum, list);
                sum->flags |= IROLF_LoopInvariant;
                sum->flags |= IROLF_Reffed;
                sum->rtype = result->rtype;
                result = sum;
            } else {
                result = IRO_DuplicateExpr(scannd, list);
            }

            if (elist) {
                elist->next = scanlist->next;
            } else {
                FirstAddendLinear = scanlist->next;
            }
        } else {
            elist = scanlist;
        }
    }

    for (scanlist = FirstAddendLinear, elist = NULL; scanlist; scanlist = scanlist->next) {
        scannd = scanlist->element;
        if (!IRO_IsIntConstant(scannd)) {
            if (result) {
                IROLinear* sum = IRO_NewLinear(IROLinearOp2Arg);
                sum->index = ++IRO_NumLinear;
                sum->nodetype = EADD;
                sum->u.diadic.left = result;
                sum->u.diadic.right = IRO_DuplicateExpr(scannd, list);
                IRO_AddToList(sum, list);
                sum->flags |= IROLF_Reffed;
                sum->rtype = result->rtype;
                result = sum;
            } else {
                result = IRO_DuplicateExpr(scannd, list);
            }

            if (elist) {
                elist->next = scanlist->next;
            } else {
                FirstAddendLinear = scanlist->next;
            }
        } else {
            elist = scanlist;
        }
    }

    for (scanlist = FirstAddendLinear; scanlist; scanlist = scanlist->next) {
        scannd = scanlist->element;
        if (result) {
            IROLinear* sum = IRO_NewLinear(IROLinearOp2Arg);
            sum->index = ++IRO_NumLinear;
            sum->nodetype = EADD;
            sum->u.diadic.left = result;
            sum->u.diadic.right = scannd;
            sum->u.diadic.right = IRO_DuplicateExpr(scannd, list);
            IRO_AddToList(sum, list);
            sum->flags |= IROLF_Reffed;
            sum->rtype = result->rtype;
            result = sum;
        } else {
            result = IRO_DuplicateExpr(scannd, list);
        }
    }

    return result;
}

static IROLinear* FindAddendForReductionPattern(
    IROLinear* target, IROLinear* sum, Boolean* inLeft
) {
    IROLinear* left;
    IROLinear* right;
    IROLinear* inner;
    Boolean subInLeft;

    left = sum->u.diadic.left;
    right = sum->u.diadic.right;
    if (sum->nodetype == EADD || sum->nodetype == ESUB) {
        inner = left;
        while (IS_LINEAR_MONADIC(inner, ETYPCON)) {
            inner = inner->u.monadic;
        }

        if (IS_LINEAR_MONADIC(inner, EINDIRECT) &&
            (inner->u.monadic->flags & IROLF_LoopInvariant) &&
            IRO_ExprsSame(target, inner)) {
            *inLeft = TRUE;
            return left;
        }

        if (IS_LINEAR_DIADIC_2(inner, EADD, ESUB)) {
            if ((inner = FindAddendForReductionPattern(target, inner, &subInLeft))) {
                *inLeft = TRUE;
                return inner;
            }
        }
    }

    *inLeft = FALSE;
    if (sum->nodetype == EADD) {
        inner = right;
        while (IS_LINEAR_MONADIC(inner, ETYPCON)) {
            inner = inner->u.monadic;
        }

        if (IS_LINEAR_MONADIC(inner, EINDIRECT) &&
            (inner->u.monadic->flags & IROLF_LoopInvariant) &&
            IRO_ExprsSame(target, inner)) {
            return right;
        }

        if (IS_LINEAR_DIADIC_2(inner, EADD, ESUB)) {
            if ((inner = FindAddendForReductionPattern(target, inner, &subInLeft))) {
                return inner;
            }
        }
    }

    return NULL;
}

#if MWCCPSP
// 0x4C46C0 (the back end's copy in 3.0.1, FEInterface.c's reduction hook)
IROLinear* func_004C46C0(IROLinear* target, IROLinear* sum, Boolean* inLeft) {
    return FindAddendForReductionPattern(target, sum, inLeft);
}
#endif
#if MWCCPSP

#endif
static void ReorderOperandsForReductionPattern(IROLinear* target, IROLinear* sum) {
    IROLinear* left;
    IROLinear* right;
    IROLinear* addend;
    IROLinear* swap;
    Boolean addendInLeft;

    left = sum->u.diadic.left;
    right = sum->u.diadic.right;

    if (sum->nodetype == EADD &&
        (addend = FindAddendForReductionPattern(target, sum, &addendInLeft)) &&
        addend != left) {
        if (addendInLeft && IS_LINEAR_DIADIC_2(left, EADD, ESUB) && addend->rtype == right->rtype) {
            swap = left;
            sum->u.diadic.left = right;
            left = sum->u.diadic.left;
            sum->u.diadic.right = swap;
            right = sum->u.diadic.right;
            addendInLeft = FALSE;
        }

        if (!addendInLeft &&
            IS_LINEAR_DIADIC_2(right, EADD, ESUB) &&
            addend->rtype == left->rtype) {
#if VERSION < VERSION_GC_2_5
            if (IRO_LocateFather_Cut_And_Paste_Without_Nopping(addend, left)) {
                sum->u.diadic.left = addend;
            }
#else
            IROList list;
            IRO_InitList(&list);
            IRO_DuplicateExpr(left, &list);
            IRO_NopOut(left);
            IRO_PasteAfter(list.head, list.tail, addend);
            if (IRO_LocateFather_Cut_And_Paste_Without_Nopping(addend, list.tail)) {
                IRO_InitList(&list);
                IRO_DuplicateExpr(addend, &list);
                IRO_NopOut(addend);
                IRO_Paste(list.head, list.tail, sum);
                sum->u.diadic.left = list.tail;
            }
#endif
        }
    }
}

static UInt32 IsAssignmentReductionCandidate(IROLinear* nd) {
    IROLinear* left;
    IROLinear* right;
    IROLinear* operand;

    if (nd->type == IROLinearOp2Arg) {
        if (nd->nodetype == EASS) {
            left = nd->u.diadic.left;
            right = nd->u.diadic.right;
            if (IS_LINEAR_MONADIC(left, EINDIRECT) &&
                (left->u.monadic->flags & IROLF_LoopInvariant)) {
                if (IS_LINEAR_MONADIC(right, ETYPCON) &&
                    right->rtype->type == right->u.monadic->rtype->type &&
                    IS_TYPE_INT(right->rtype) &&
                    right->rtype->size < right->u.monadic->rtype->size) {
                    right = right->u.monadic;
                }

                if (IS_LINEAR_DIADIC_2(right, EADD, ESUB)) {
                    ReorderOperandsForReductionPattern(left, right);
                    operand = right->u.diadic.left;
                    if (IS_LINEAR_MONADIC(operand, ETYPCON)) {
                        operand = operand->u.monadic;
                    }
                    if (IS_LINEAR_MONADIC(operand, EINDIRECT) &&
                        (operand->u.monadic->flags & IROLF_LoopInvariant) &&
                        IRO_ExprsSame(left, operand) &&
                        right->u.diadic.right->type == IROLinearOp2Arg) {
                        if (right->u.diadic.right->nodetype == EADD) {
                            return TRUE;
                        }
                        if (right->u.diadic.right->nodetype == ESUB) {
                            return TRUE;
                        }
                        if (right->u.diadic.right->nodetype == EMUL) {
                            return TRUE;
                        }
                        if (right->u.diadic.right->nodetype == EDIV) {
                            return TRUE;
                        }
                    }
                }
            }
        } else if (nd->nodetype == EADDASS || nd->nodetype == ESUBASS) {
            left = nd->u.diadic.left;
            right = nd->u.diadic.right;

            if (IS_LINEAR_MONADIC(right, ETYPCON) &&
                right->rtype->type == right->u.monadic->rtype->type &&
                IS_TYPE_INT(right->rtype) &&
                right->rtype->size < right->u.monadic->rtype->size) {
                right = right->u.monadic;
            }

            if (IS_LINEAR_MONADIC(left, EINDIRECT) &&
                (left->u.monadic->flags & IROLF_LoopInvariant) &&
                right->type == IROLinearOp2Arg) {
                if (right->nodetype == EADD) {
                    return TRUE;
                }
                if (right->nodetype == ESUB) {
                    return TRUE;
                }
                if (right->nodetype == EMUL) {
                    return TRUE;
                }
                if (right->nodetype == EDIV) {
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

#pragma pack(2)
