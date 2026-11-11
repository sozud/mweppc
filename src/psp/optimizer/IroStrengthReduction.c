// mwccpsp 3.0.1: strength reduction of induction-variable expressions in
// loops (IroStrengthReduction.c, 0x59CE30..0x59E760). 2.4.7 does this in
// IroLoop.c (RisList).
//
//     for (i = 0; i < n; i++) p[i * 12] = 0;
// keeps i * 12 in a temporary stepped by 12.
#include "compiler/common.h"
#include "compiler/BitVector.h"
#include "compiler/CError.h"
#include "compiler/CParser.h"
#include "compiler/CFunc.h"
#include "compiler/enode.h"
#include "compiler/objects.h"
#include "compiler/IroCSE.h"
#include "compiler/IroDump.h"
#include "compiler/IroEval.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroLoop301.h"
#include "compiler/IroMalloc.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"

// an expression rewritten for the candidate search, with its original
// (0xC bytes)
typedef struct IROReductionPair {
    IROLinear* orig; // +0
    IROLinear* nd; // +4
    struct IROReductionPair* next; // +8
} IROReductionPair;

extern void func_005920D0(IRONaturalLoop* loop); // IroLoop.c
extern void func_0056AB70(
    IROLinear* linear, Boolean alloc, Boolean stop, Boolean recurse
); // IroCSE.c
Boolean func_0059DB80(
    IRONaturalLoop* loop, IROLinear* nd, IROLinear** factor, IROLinear** indOperand, VarRecord** var
);
extern Boolean func_005575F0(Object* obj); // IRO_IsRegable
extern Boolean func_0055A3F0(IROLinear* conv); // IroTypeConv.c
extern IROLinear* func_00552790(Object* obj); // IroUtil.c
extern Boolean func_00555C60(IROLinear* nd); // IroUtil.c
extern void func_005531A0(
    IROLinear* linear, IROWalkTreeArgFunc func, Boolean pre, void* arg
); // IroUtil.c
IROExpr* func_0059D500(
    IROExpr* expr,
    IROLinear* indOperand,
    IROLinear* factor,
    VarRecord* var,
    int squared,
    IROLinear* pre,
    IROExpr** done
);
extern void func_00552150(IROLinear* nd, Object* obj, IROLinear* after); // IroUtil.c
extern void func_00552CA0(IROExpr* expr);

// IroLoop.c's IRO_RemoveExpr_Action (0x59E740)
static void RemoveExprAction(IROLinear* linear, Boolean isFirst) {
    if (isFirst && linear->expr) {
        IRO_RemoveExpr(linear->expr);
    }
}

// IroLoop.c's IRO_ActUnmarkRISCandidate (0x59DB60)
static void ActUnmarkRISCandidate(IROLinear* linear, Boolean isFirst) {
    if (isFirst && linear->expr) {
        linear->flags &= ~IROLF_Ris;
    }
}

static inline Boolean ExprMayMove(IRONaturalLoop* loop, IROExpr* expr) {
    return Bv_IsBitSet(expr->node->index, loop->nodes) &&
        !expr->notSubable &&
        (!expr->couldError || expr->node->unk3F);
}

// 0x59D320: nd = (a +- b) * inv becomes a * inv +- b * inv; the pair keeps a
// copy of nd to put back
void func_0059D320(IROReductionPair** pairs, IROLinear* nd, IROLinear* inv, IROLinear* sum) {
    IROList list;
    IROList orig;
    IROLinear* m1;
    IROLinear* m2;
    IROLinear* add;
    IROReductionPair* pair;

    IRO_InitList(&list);
    IRO_InitList(&orig);
    IRO_DuplicateExpr(nd, &orig);
    m1 = IRO_NewLinear(IROLinearOp2Arg);
    *m1 = *nd;
    IRO_DuplicateExpr(sum->u.diadic.left, &list);
    m1->u.diadic.left = list.tail;
    IRO_DuplicateExpr(inv, &list);
    m1->u.diadic.right = list.tail;
    m1->next = NULL;
    IRO_AddToList(m1, &list);
    m2 = IRO_NewLinear(IROLinearOp2Arg);
    *m2 = *nd;
    IRO_DuplicateExpr(sum->u.diadic.right, &list);
    m2->u.diadic.left = list.tail;
    IRO_DuplicateExpr(inv, &list);
    m2->u.diadic.right = list.tail;
    m2->next = NULL;
    IRO_AddToList(m2, &list);
    add = IRO_NewLinear(IROLinearOp2Arg);
    *add = *sum;
    add->u.diadic.left = m1;
    add->u.diadic.right = m2;
    add->next = NULL;
    IRO_AddToList(add, &list);
    pair = IRO_malloc(sizeof(IROReductionPair));
    pair->orig = NULL;
    pair->nd = NULL;
    pair->next = NULL;
    pair->orig = orig.tail;
    pair->nd = add;
    pair->next = *pairs;
    *pairs = pair;
    IRO_PasteAfter(list.head, list.tail, nd);
    IRO_LocateFather_Cut_And_Paste(nd, add);
}

// 0x59D130: (v +- c) * inv with v an induction variable is distributed (so
// the product of the induction can be reduced); TRUE when one was
void func_0059D130(IRONaturalLoop* loop, IROReductionPair** pairs) {
    IROExpr* expr;
    IROExpr* next;
    IROLinear* nd;
    IROLinear* inv;
    IROLinear* sum;
    IROLinear* a;
    IROLinear* b;
    IROLinear* c;
    IROLinear* v;
    Object* obj;
    VarRecord* var;
    Boolean changed = FALSE;

    for (expr = IRO_FirstExpr; expr; expr = next) {
        next = expr->next;
        if (!ExprMayMove(loop, expr)) {
            continue;
        }
        c = NULL;
        v = NULL;
        nd = expr->linear;
        inv = NULL;
        sum = NULL;
        if (nd->type == IROLinearOp2Arg && nd->nodetype == EMUL) {
            if (nd->u.diadic.right->flags & IROLF_LoopInvariant) {
                inv = nd->u.diadic.right;
                sum = nd->u.diadic.left;
            } else if (nd->u.diadic.left->flags & IROLF_LoopInvariant) {
                inv = nd->u.diadic.left;
                sum = nd->u.diadic.right;
            }
        }
        if (inv &&
            sum &&
            sum->type == IROLinearOp2Arg &&
            (sum->nodetype == EADD || sum->nodetype == ESUB) &&
            (copts.loopinvariants || IRO_IsIntConstant(inv))) {
            a = sum->u.diadic.left;
            b = sum->u.diadic.right;
            if (IRO_IsIntConstant(b)) {
                c = b;
                v = a;
            } else if (sum->nodetype == EADD && IRO_IsIntConstant(a)) {
                c = a;
                v = b;
            }
        }
        if (inv &&
            sum &&
            c &&
            v &&
            (obj = IRO_IsVariable(v)) &&
            (var = IRO_FindVar(obj, FALSE, TRUE)) &&
            var->inductionState == 2) {
            func_0059D320(pairs, expr->linear, inv, sum);
            changed = TRUE;
        }
    }
    if (changed) {
        IRO_ConstantFolding();
        IRO_FindExpressions(loop->nodes, FALSE);
        if (copts.loopinvariants) {
            func_005920D0(loop);
        }
    }
}

// 0x59D090: put back the distributed products that found no candidate
void func_0059D090(IROReductionPair** pairs) {
    IROReductionPair* pair;
    IROLinear* nd;
    IROList list;

    for (pair = *pairs; pair; pair = pair->next) {
        nd = pair->nd;
        if (nd->type == IROLinearOp2Arg &&
            (nd->nodetype == EADD || nd->nodetype == ESUB) &&
            nd->u.diadic.left->type == IROLinearOp2Arg &&
            nd->u.diadic.left->nodetype == EMUL &&
            nd->u.diadic.right->type == IROLinearOp2Arg &&
            nd->u.diadic.right->nodetype == EMUL &&
            !(nd->u.diadic.left->flags & IROLF_Ris) &&
            !(nd->u.diadic.right->flags & IROLF_Ris)) {
            IRO_InitList(&list);
            IRO_DuplicateExpr(pair->orig, &list);
            IRO_PasteAfter(list.head, list.tail, pair->nd);
            IRO_LocateFather_Cut_And_Paste(pair->nd, list.tail);
        }
    }
}

// 0x59CF80: mark the loop's strength reduction candidates
void func_0059CF80(IRONaturalLoop* loop) {
    IROReductionPair* pairs = NULL;
    IROReductionPair* pair;
    IROReductionPair* next;
    IROExpr* expr;
    IROLinear* factor;
    IROLinear* indOperand;
    VarRecord* var;

    func_00587540(loop);
    func_0059D130(loop, &pairs);
    for (expr = IRO_FirstExpr; expr; expr = expr->next) {
        if (ExprMayMove(loop, expr) &&
            func_0059DB80(loop, expr->linear, &factor, &indOperand, &var)) {
            IRO_WalkTree(expr->linear, ActUnmarkRISCandidate);
            expr->linear->flags |= IROLF_Ris;
            expr->factor = factor;
            expr->indOperand = indOperand;
            expr->var = var;
        }
    }
    func_0059D090(&pairs);
    for (pair = pairs; pair; pair = next) {
        next = pair->next;
        IRO_free(pair);
    }
    pairs = NULL;
}

// 0x59CE30: reduce the marked candidates, equal ones sharing a temporary
void func_0059CE30(IRONaturalLoop* loop, IROLinear* pre) {
    IROExpr* done = NULL;
    IROExpr* expr;
    IROExpr* next;
    IROExpr* scan;
    IROLinear* nd;
    IROLinear* factor;
    IROLinear* indOperand;
    VarRecord* var;

    func_00587540(loop);
    for (expr = IRO_FirstExpr; expr; expr = next) {
        nd = expr->linear;
        next = expr->next;
        if (!(nd->flags & IROLF_Ris)) {
            continue;
        }
        indOperand = expr->indOperand;
        factor = expr->factor;
        var = expr->var;
        IRO_Dump("Found reduction in strength: %d\n", nd->index);
        if (expr->linear->type == IROLinearOp2Arg && expr->linear->nodetype == ESHL) {
            expr->linear->nodetype = EMUL;
            factor->u.node->data.intval = CInt64_Shl(cint64_one, factor->u.node->data.intval);
            factor->u.node->rtype = IRO_UnqualifiedType(expr->linear->rtype);
            factor->rtype = IRO_UnqualifiedType(expr->linear->rtype);
        }
        for (scan = done; scan; scan = scan->next) {
            if (IRO_ExprsSame(expr->linear, scan->linear)) {
                break;
            }
        }
        if (scan) {
            IRO_Dump("Using existing RIS: %d\n", scan->linear->index);
            IRO_WalkTree(expr->linear, RemoveExprAction);
        } else {
            scan = func_0059D500(expr, indOperand, factor, var, 0, pre, &done);
        }
        func_00552150(expr->linear, scan->temp, expr->linear);
        func_00552CA0(expr);
    }
}

typedef struct IROTreeSearch {
    IROLinear* target;
    Boolean found;
} IROTreeSearch;

// 0x59E720: walker callback, the tree holds the target
void func_0059E720(IROLinear* linear, Boolean isFirst, void* arg) {
    if (isFirst && linear == ((IROTreeSearch*)arg)->target) {
        ((IROTreeSearch*)arg)->found = TRUE;
    }
}

static inline Boolean IsDivOrShr(IROLinear* nd) {
    return nd->type == IROLinearOp2Arg && (nd->nodetype == EDIV || nd->nodetype == ESHR);
}

// temp = value onto list
static inline IROLinear* AssignTemp(Object* temp, Type* type, IROLinear* value, IROList* list) {
    IROLinear* ass = IRO_NewLinear(IROLinearOp2Arg);

    ass->index = ++IRO_NumLinear;
    ass->rtype = type;
    ass->nodetype = EASS;
    ass->u.diadic.left = func_00552790(temp);
    IRO_AddToList(ass->u.diadic.left->u.monadic, list);
    ass->u.diadic.left->flags |= IROLF_Reffed | IROLF_Assigned | IROLF_Ind;
    ass->u.diadic.left->u.monadic->flags |= IROLF_Assigned | IROLF_Ind;
    ass->u.diadic.right = value;
    IRO_AddToList(ass, list);
    return ass;
}

// 0x59D500: reduce expr (indOperand * factor) to a temporary set before the
// loop (pre) and stepped with the counter (2.4.7's CreateRIS); expr goes on
// the done list
IROExpr* func_0059D500(
    IROExpr* expr,
    IROLinear* indOperand,
    IROLinear* factor,
    VarRecord* var,
    int squared,
    IROLinear* pre,
    IROExpr** done
) {
    IROLoopInd301* ind = (IROLoopInd301*)var;
    Boolean stepTemp = FALSE;
    Type* type;
    Object* temp;
    Object* stepTempObj;
    IROLinear* nd;
    IROLinear* first;
    IROLinear* mul;
    IROLinear* cast;
    IROLinear* upd;
    IROList list1;
    IROList list2;
    IROTreeSearch search;

    type = IRO_UnqualifiedType(expr->linear->rtype);
    temp = create_temp_object(type);
    IRO_FindVar(temp, TRUE, TRUE);
    IRO_InitList(&list1);
    nd = expr->linear;
    if ((nd->type == IROLinearOp2Arg && nd->nodetype == EADD) || IsDivOrShr(nd)) {
        first = IRO_DuplicateExpr(nd, &list1);
    } else {
        first = IRO_NewLinear(IROLinearOp2Arg);
        first->index = ++IRO_NumLinear;
        first->rtype = type;
        first->nodetype = EMUL;
        first->u.diadic.left = IRO_DuplicateExpr(indOperand, &list1);
        first->u.diadic.right = IRO_DuplicateExpr(squared ? indOperand : factor, &list1);
        IRO_AddToList(first, &list1);
    }
    AssignTemp(temp, type, first, &list1);
    IRO_Paste(list1.head, list1.tail, pre);

    if (!IsDivOrShr(expr->linear) &&
        !CInt64_IsOne(&ind->step) &&
        (ind->stepExpr || !func_00555C60(factor))) {
        stepTemp = TRUE;
        IRO_InitList(&list1);
        mul = IRO_NewLinear(IROLinearOp2Arg);
        mul->index = ++IRO_NumLinear;
        mul->rtype = IRO_UnqualifiedType(factor->rtype);
        mul->nodetype = EMUL;
        if (!ind->stepExpr) {
            mul->u.diadic.left = IRO_DuplicateExpr(factor, &list1);
            mul->u.diadic.right = IRO_NewIntConst(ind->step, factor->rtype);
            IRO_AddToList(mul->u.diadic.right, &list1);
        } else {
            mul->u.diadic.left = IRO_DuplicateExpr(factor, &list1);
            mul->u.diadic.right = IRO_DuplicateExpr(ind->stepExpr, &list1);
            if (factor->rtype != ind->stepExpr->rtype &&
                !IRO_TypesEqual(factor->rtype, ind->stepExpr->rtype)) {
                cast = IRO_NewLinear(IROLinearOp1Arg);
                cast->nodetype = ETYPCON;
                cast->index = ++IRO_NumLinear;
                cast->rtype = IRO_UnqualifiedType(factor->rtype);
                if (cast->rtype->type == TYPEINT &&
                    TYPE_INTEGRAL(cast->rtype)->integral == IT_BOOL) {
                    CError_FATAL(836);
                }
                cast->u.monadic = mul->u.diadic.right;
                IRO_AddToList(cast, &list1);
                mul->u.diadic.right = cast;
            }
        }
        IRO_AddToList(mul, &list1);
        stepTempObj = create_temp_object(IRO_UnqualifiedType(factor->rtype));
        IRO_FindVar(stepTempObj, TRUE, TRUE);
        AssignTemp(stepTempObj, IRO_UnqualifiedType(factor->rtype), mul, &list1);
        IRO_Paste(list1.head, list1.tail, pre);
    }

    IRO_InitList(&list2);
    upd = IRO_NewLinear(IROLinearOp2Arg);
    upd->index = ++IRO_NumLinear;
    upd->rtype = type;
    nd = ind->nd;
    if (nd->type == IROLinearOp2Arg) {
        if (nd->nodetype == EASS &&
            nd->u.diadic.right->type == IROLinearOp2Arg &&
            nd->u.diadic.right->nodetype == EADD) {
            upd->nodetype = EADDASS;
        } else if (nd->nodetype == EASS &&
                   nd->u.diadic.right->type == IROLinearOp2Arg &&
                   nd->u.diadic.right->nodetype == ESUB) {
            upd->nodetype = ESUBASS;
        } else {
            upd->nodetype = nd->nodetype;
        }
    } else if (nd->nodetype == EPREINC || nd->nodetype == EPOSTINC) {
        upd->nodetype = EADDASS;
    } else {
        upd->nodetype = ESUBASS;
    }
    upd->u.diadic.left = func_00552790(temp);
    IRO_AddToList(upd->u.diadic.left->u.monadic, &list2);
    upd->u.diadic.left->flags |= IROLF_Reffed | IROLF_Assigned | IROLF_Used | IROLF_Ind;
    upd->u.diadic.left->u.monadic->flags |= IROLF_Assigned | IROLF_Used | IROLF_Ind;
    if (!squared) {
        if (!stepTemp) {
            if (CInt64_IsOne(&ind->step) || IsDivOrShr(expr->linear)) {
                upd->u.diadic.right = IRO_DuplicateExpr(factor, &list2);
            } else {
                upd->u.diadic.right = IRO_NewIntConst(
                    CInt64_Mul(factor->u.node->data.intval, ind->step), factor->rtype
                );
                IRO_AddToList(upd->u.diadic.right, &list2);
            }
        } else {
            upd->u.diadic.right = func_00552790(stepTempObj);
            IRO_AddToList(upd->u.diadic.right->u.monadic, &list2);
            upd->u.diadic.right->flags |= IROLF_Reffed | IROLF_Used;
        }
    }
    upd->index = ++IRO_NumLinear;
    IRO_AddToList(upd, &list2);

    nd = expr->linear;
    search.found = FALSE;
    if (nd->type == IROLinearOp2Arg &&
        nd->nodetype == EADD &&
        ((nd->rtype->type == TYPEINT && TYPE_INTEGRAL(nd->rtype)->integral < IT_17) ||
         nd->rtype->type == TYPEPOINTER)) {
        search.target = nd;
        search.found = FALSE;
        func_005531A0(ind->nd, func_0059E720, TRUE, &search);
    }
    if (search.found && !(ind->nd->flags & IROLF_Reffed)) {
        IRO_PasteAfter(list2.head, list2.tail, ind->nd);
    } else {
        IRO_Paste(list2.head, list2.tail, IRO_FindStart(ind->nd));
    }
    IRO_WalkTree(expr->linear, RemoveExprAction);
    expr->temp = temp;
    expr->next = *done;
    *done = expr;
    return expr;
}

static inline IROLinear* StripSafeConversions(IROLinear* nd) {
    while (nd->type == IROLinearOp1Arg && nd->nodetype == ETYPCON && func_0055A3F0(nd)) {
        nd = nd->u.monadic;
    }
    return nd;
}

static inline Boolean IsIntOrPtrType(Type* type) {
    return (type->type == TYPEINT && TYPE_INTEGRAL(type)->integral < IT_17) ||
        type->type == TYPEPOINTER;
}

// an invariant addend that is a register variable or a constant (or
// such + constant) is not worth a temporary
static inline Boolean CheapAddend(IROLinear* nd, Boolean viaConversion) {
    Object* obj;

    if (!viaConversion) {
        if ((obj = IRO_IsVariable(nd)) && func_005575F0(obj)) {
            return TRUE;
        }
        if (func_00555C60(nd)) {
            return TRUE;
        }
    }
    return nd->type == IROLinearOp2Arg &&
        nd->nodetype == EADD &&
        (obj = IRO_IsVariable(nd->u.diadic.left)) &&
        func_005575F0(obj) &&
        func_00555C60(nd->u.diadic.right);
}

// x * c or x << c with c below the type's bit count, x a variable (through
// conversions): the variable linear, or NULL; *shl gets a shift
static inline IROLinear* ScaledVariable(IROLinear* nd, IROLinear** shl) {
    IROLinear* x;
    CInt64 lim;

    if (!func_00555C60(nd->u.diadic.right)) {
        return NULL;
    }
    if (nd->nodetype == ESHL) {
        CInt64_SetULong(&lim, nd->rtype->size * 8);
        if (!CInt64_LessU(nd->u.diadic.right->u.node->data.intval, lim)) {
            return NULL;
        }
        *shl = nd;
    }
    x = nd->u.diadic.left;
    if (x->type != IROLinearOp1Arg) {
        return NULL;
    }
    x = StripSafeConversions(x);
    return IRO_IsVariable(x) ? x : NULL;
}

// 0x59DB80: nd is an expression of an induction variable that strength
// reduction can keep in a temporary: *indOperand * *factor, *var the
// induction record (2.4.7's Reducable)
Boolean func_0059DB80(
    IRONaturalLoop* loop, IROLinear* nd, IROLinear** factor, IROLinear** indOperand, VarRecord** var
) {
    IROLoopInd301** pind = (IROLoopInd301**)var;
    Boolean leftTypcon = FALSE;
    Boolean leftInvariant = FALSE;
    Boolean rightTypcon = FALSE;
    Boolean rightInvariant = FALSE;
    IROLinear* shl = NULL;
    IROLinear* left;
    IROLinear* right;
    IROLinear* indirect;
    IROLinear* x;
    IROLoopInd301* ind;
    VarRecord* rec;
    Type* type;
    CInt64 divisor;
    CInt64 lim;
    CInt64 q;
    CInt64 r;
    IROTreeSearch search;

    if (nd->type == IROLinearOp2Arg && nd->nodetype == EADD && IsIntOrPtrType(nd->rtype)) {
        left = nd->u.diadic.left;
        right = nd->u.diadic.right;
        while (left->type == IROLinearOp1Arg && left->nodetype == ETYPCON && func_0055A3F0(left)) {
            leftTypcon = TRUE;
            leftInvariant = (left->flags & IROLF_LoopInvariant) != 0;
            left = left->u.monadic;
        }
        while (
            right->type == IROLinearOp1Arg && right->nodetype == ETYPCON && func_0055A3F0(right)) {
            rightTypcon = TRUE;
            rightInvariant = (right->flags & IROLF_LoopInvariant) != 0;
            right = right->u.monadic;
        }
        if (((left->flags & IROLF_LoopInvariant) || leftInvariant) && IRO_IsVariable(right)) {
            if (CheapAddend(left, leftInvariant)) {
                return FALSE;
            }
            *indOperand = rightTypcon ? nd->u.diadic.right : right;
            indirect = right;
            type = nd->u.diadic.right->rtype;
            *factor = IRO_NewIntConst(
                cint64_one, type->type == TYPEPOINTER ? nd->u.diadic.left->rtype : type
            );
        } else if (((left->flags & IROLF_LoopInvariant) || leftInvariant) &&
                   right->type == IROLinearOp2Arg &&
                   !rightTypcon &&
                   (right->nodetype == EMUL || right->nodetype == ESHL)) {
            if (!(x = ScaledVariable(right, &shl))) {
                return FALSE;
            }
            *indOperand = right->u.diadic.left;
            *factor = right->u.diadic.right;
            indirect = x;
        } else if (((right->flags & IROLF_LoopInvariant) || rightInvariant) &&
                   left->type == IROLinearOp2Arg &&
                   !leftTypcon &&
                   (left->nodetype == EMUL || left->nodetype == ESHL)) {
            if (!(x = ScaledVariable(left, &shl))) {
                return FALSE;
            }
            *indOperand = left->u.diadic.left;
            *factor = left->u.diadic.right;
            indirect = x;
        } else if ((right->flags & IROLF_LoopInvariant) || rightInvariant) {
            if (!IRO_IsVariable(left)) {
                return FALSE;
            }
            if (CheapAddend(right, rightInvariant)) {
                return FALSE;
            }
            if (leftTypcon) {
                if (!func_0055A3F0(nd->u.diadic.left)) {
                    return FALSE;
                }
                *indOperand = nd->u.diadic.left;
            } else {
                *indOperand = left;
            }
            indirect = left;
            type = nd->u.diadic.left->rtype;
            *factor = IRO_NewIntConst(
                cint64_one, type->type == TYPEPOINTER ? nd->u.diadic.right->rtype : type
            );
        } else {
            return FALSE;
        }
    } else if (nd->type == IROLinearOp2Arg &&
               (nd->nodetype == EMUL || nd->nodetype == ESHL) &&
               IsIntOrPtrType(nd->rtype)) {
        left = nd->u.diadic.left;
        right = nd->u.diadic.right;
        x = StripSafeConversions(left);
        if (func_00555C60(right) && IRO_IsVariable(x)) {
            if (nd->nodetype == ESHL) {
                shl = nd;
            }
            *indOperand = left;
            *factor = right;
            indirect = x;
        } else {
            if (nd->nodetype == ESHL) {
                return FALSE;
            }
            if (nd->u.diadic.right->flags & IROLF_LoopInvariant) {
                x = StripSafeConversions(left);
                if (!IRO_IsVariable(x)) {
                    return FALSE;
                }
                *indOperand = left;
                *factor = right;
                indirect = x;
            } else if (nd->u.diadic.left->flags & IROLF_LoopInvariant) {
                x = StripSafeConversions(right);
                if (!IRO_IsVariable(x)) {
                    return FALSE;
                }
                *indOperand = right;
                *factor = left;
                indirect = x;
            } else {
                return FALSE;
            }
        }
    } else if (nd->type == IROLinearOp2Arg && (nd->nodetype == EDIV || nd->nodetype == ESHR)) {
        if (!(nd->rtype->type == TYPEINT && TYPE_INTEGRAL(nd->rtype)->integral < IT_17)) {
            return FALSE;
        }
        if (!IRO_IsVariable(nd->u.diadic.left) || !func_00555C60(nd->u.diadic.right)) {
            return FALSE;
        }
        divisor = nd->u.diadic.right->u.node->data.intval;
        if (nd->nodetype == ESHR) {
            if (!is_unsigned(nd->u.diadic.right->rtype) && CInt64_IsNegative(&divisor)) {
                return FALSE;
            }
            CInt64_SetLong(&lim, nd->rtype->size * 8);
            if (CInt64_Greater(divisor, lim)) {
                return FALSE;
            }
            divisor = CInt64_Shl(cint64_one, divisor);
        }
        *indOperand = nd->u.diadic.left;
        indirect = nd->u.diadic.left;
    } else {
        return FALSE;
    }

    if (nd->type == IROLinearOp2Arg && nd->nodetype == ESHL) {
        if (!func_00555C60(*factor)) {
            return FALSE;
        }
        if (!is_unsigned((*factor)->rtype) && CInt64_IsNegative(&(*factor)->u.node->data.intval)) {
            return FALSE;
        }
        CInt64_SetLong(&lim, nd->rtype->size * 8);
        if (CInt64_GreaterU((*factor)->u.node->data.intval, lim)) {
            return FALSE;
        }
    }
    if (!indirect->u.monadic->u.node) {
        CError_FATAL(550);
    }
    rec = IRO_FindVar(indirect->u.monadic->u.node->data.objref, FALSE, TRUE);
    if (!rec || rec->inductionState != 2) {
        return FALSE;
    }
    if (copts.ANSIstrict || copts.strengthreductionstrict) {
        type = rec->object->type;
        if (is_unsigned(type) && type->size < 4) {
            return FALSE;
        }
    }
    *pind = func_00588C90(loop, rec);
    if (!*pind) {
        CError_FATAL(585);
    }
    if (nd->type == IROLinearOp2Arg && (nd->nodetype == ESHR || nd->nodetype == EDIV)) {
        ind = *pind;
        if (ind->stepExpr) {
            return FALSE;
        }
        if (CInt64_Less(ind->step, divisor)) {
            return FALSE;
        }
        q = CInt64_Div(ind->step, divisor);
        if (CInt64_IsZero(&q) || CInt64_IsNegative(&q)) {
            return FALSE;
        }
        r = CInt64_Mod(ind->step, divisor);
        if (!CInt64_IsZero(&r)) {
            return FALSE;
        }
        *factor = IRO_NewIntConst(q, nd->u.diadic.left->rtype);
    }
    if (nd->type == IROLinearOp2Arg && nd->nodetype == EADD && IsIntOrPtrType(nd->rtype)) {
        search.target = nd;
        search.found = FALSE;
        func_005531A0((*pind)->nd, func_0059E720, TRUE, &search);
        if (search.found && ((*pind)->nd->flags & IROLF_Reffed)) {
            return FALSE;
        }
    }
    func_0056AB70(*factor, TRUE, TRUE, TRUE);
    if (Bv_BitsInCommon(loop->xC, IRO_Depends)) {
        return FALSE;
    }
    if (shl) {
        shl->nodetype = EMUL;
        shl->u.diadic.right->u.node->data.intval =
            CInt64_Shl(cint64_one, shl->u.diadic.right->u.node->data.intval);
        shl->u.diadic.right->u.node->rtype = IRO_UnqualifiedType(shl->rtype);
        shl->u.diadic.right->rtype = IRO_UnqualifiedType(shl->rtype);
    }
    return TRUE;
}
