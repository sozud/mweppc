// mwccpsp 3.0.1: if-then-else whose branches do the same thing
// (IRO_SimplifyIfThenElse, 0x58B8C0..0x58BC10), with 3.0.1's semantic
// expression comparison from IroUtil.c (0x555190..0x555870, with a depth
// and matching of associative chains in any order) and the statement root
// helper (0x552E90). Not in the 2.4.7 source.
//
//     if (c) x = a + b; else x = b + a;     becomes   x = a + b;
#include "compiler/common.h"
#include "compiler/CParser.h"
#include "compiler/CError.h"
#include "compiler/CInt64.h"
#include "compiler/CMachine.h"
#include "compiler/enode.h"
#include "compiler/IroDump.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroJump.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroUtil.h"
#include "compiler/IrOptimizer.h"
#include "compiler/tokens.h"
#include <string.h>

extern Boolean IsAssociativeENodeType[IRO_MAXEXPR]; // IroEval.c
extern Boolean IsSubableOp[IRO_MAXEXPR]; // IroSubable.c
extern Boolean DisableDueToAsm;
extern Boolean IRO_RemoveUnreachable(void);
extern void IRO_ComputeSuccPred(void);
extern void IRO_ComputeDom(void);

// --- IroUtil.c ------------------------------------------------------------

// 0x552E90: the expression a statement linear holds
IROLinear* func_00552E90(IROLinear* nd) {
    switch (nd->type) {
        case IROLinearOperand:
        case IROLinearOp1Arg:
        case IROLinearOp2Arg:
        case IROLinearOp3Arg:
        case IROLinearScope:
        case IROLinearFunccall:
            return nd;
        case IROLinearGotoExpr:
            return nd->u.monadic;
        case IROLinearIf:
        case IROLinearIfNot:
            return nd->u.label.cond;
        case IROLinearReturn:
            return nd->u.monadic ? nd->u.monadic : NULL;
        case IROLinearSwitch:
            return nd->u.swtch.cond;
        case IROLinearBeginCatch:
        case IROLinearEndCatch:
        case IROLinearEndCatchDtor:
            return nd->u.ctch.linear;
        case IROLinearNop:
        case IROLinearGoto:
        case IROLinearLabel:
        case IROLinearAsm:
        case IROLinearEnd:
            return NULL;
        default:
            CError_FATAL(2637);
            return NULL;
    }
}

Boolean func_005551B0(IROLinear* a, IROLinear* b, int depth);

// 0x555680: the operands of a chain of op, at most 11
int func_00555680(ENodeType op, IROLinear* nd, int count, IROLinear** list) {
    if (count > 10) {
        return count;
    }
    if (nd->nodetype != op) {
        list[count] = nd;
        return count + 1;
    }
    count = func_00555680(op, nd->u.diadic.left, count, list);
    return func_00555680(op, nd->u.diadic.right, count, list);
}

// 0x555550: two chains of the same associative operator with the same
// operands in some order
Boolean func_00555550(IROLinear* a, IROLinear* b, int depth) {
    IROLinear* la[11];
    IROLinear* lb[11];
    int n;
    int m;
    int i;
    int j;

    if (depth > 6) {
        return FALSE;
    }
    n = func_00555680(a->nodetype, a->u.diadic.left, 0, la);
    if (n > 10) {
        return FALSE;
    }
    n = func_00555680(a->nodetype, a->u.diadic.right, n, la);
    if (n > 10) {
        return FALSE;
    }
    m = func_00555680(a->nodetype, b->u.diadic.left, 0, lb);
    if (m >= n) {
        return FALSE;
    }
    m = func_00555680(a->nodetype, b->u.diadic.right, m, lb);
    if (n != m) {
        return FALSE;
    }
    for (i = 0; i < n; i++) {
        for (j = 0; j < m; j++) {
            if (lb[j] && func_005551B0(la[i], lb[j], depth)) {
                lb[j] = NULL;
                break;
            }
        }
        if (j == m) {
            return FALSE;
        }
    }
    return TRUE;
}

// 0x5551B0
Boolean func_005551B0(IROLinear* a, IROLinear* b, int depth) {
    UInt16 mask;
    ENode* na;
    ENode* nb;
    int i;
    int argc;
    Boolean same;

    if (a->type == IROLinearOp1Arg &&
        b->type == IROLinearOp1Arg &&
        a->nodetype == ETYPCON &&
        b->nodetype == ETYPCON) {
        mask = 0xFD7F;
    } else {
        mask = 0xFDFF;
    }
    if (a->type != b->type ||
        (a->nodeflags & mask) != (b->nodeflags & mask) ||
        !IRO_TypesEqual(a->rtype, b->rtype)) {
        return FALSE;
    }
    switch (a->type) {
        case IROLinearOperand:
            na = a->u.node;
            nb = b->u.node;
            if (na->type != nb->type) {
                return FALSE;
            }
            switch (na->type) {
                case EINTCONST:
                    return CInt64_Equal(na->data.intval, nb->data.intval);
                case EFLOATCONST:
                    if (CMach_FloatIsZero(na->data.floatval)) {
                        return FALSE;
                    }
                    return CMach_CalcFloatDiadicBool(
                               na->rtype, na->data.floatval, TK_LOGICAL_EQ, nb->data.floatval
                           ) != 0;
                case ESTRINGCONST:
                    if (na->data.string.size != nb->data.string.size) {
                        return FALSE;
                    }
                    return memcmp(
                               na->data.string.data, nb->data.string.data, na->data.string.size
                           ) == 0;
                case EOBJREF:
                    return na->data.objref == nb->data.objref;
                default:
                    return FALSE;
            }
        case IROLinearOp1Arg:
            if (a->nodetype != b->nodetype) {
                return FALSE;
            }
            return func_005551B0(a->u.monadic, b->u.monadic, depth);
        case IROLinearOp2Arg:
            if (a->nodetype != b->nodetype) {
                return FALSE;
            }
            if (IsAssociativeENodeType[a->nodetype] &&
                IsSubableOp[a->nodetype] &&
                !IRO_HasSideEffect(a) &&
                !IRO_HasSideEffect(b)) {
                return func_00555550(a, b, depth + 1);
            }
            same = func_005551B0(a->u.diadic.left, b->u.diadic.left, depth) &&
                func_005551B0(a->u.diadic.right, b->u.diadic.right, depth);
            if (!same && IsSubableOp[a->nodetype] && !IRO_HasSideEffect(a)) {
                same = func_005551B0(a->u.diadic.left, b->u.diadic.right, depth) &&
                    func_005551B0(a->u.diadic.right, b->u.diadic.left, depth);
            }
            return same;
        case IROLinearOp3Arg:
            if (a->nodetype != b->nodetype) {
                return FALSE;
            }
            return func_005551B0(a->u.args3.a, b->u.args3.a, depth) &&
                func_005551B0(a->u.args3.b, b->u.args3.b, depth) &&
                func_005551B0(a->u.args3.c, b->u.args3.c, depth);
        case IROLinearFunccall:
            if (!func_005551B0(a->u.funccall.callee, b->u.funccall.callee, depth)) {
                return FALSE;
            }
            argc = a->u.funccall.argCount;
            if (argc != b->u.funccall.argCount) {
                return FALSE;
            }
            for (i = 0; i < argc; i++) {
                if (!func_005551B0(a->u.funccall.args[i], b->u.funccall.args[i], depth)) {
                    return FALSE;
                }
            }
            return TRUE;
        default:
            return FALSE;
    }
}

// 0x555190
Boolean func_00555190(IROLinear* a, IROLinear* b) {
    return func_005551B0(a, b, 0);
}

// --- IRO_SimplifyIfThenElse -----------------------------------------------

// the next linear of the node that does something
static inline IROLinear* NextWork(IROLinear* nd, IRONode* node) {
    while (nd &&
           nd != node->last->next &&
           (nd->type == IROLinearNop ||
            nd->type == IROLinearLabel ||
            nd->type == IROLinearGoto ||
            (nd->flags & IROLF_Reffed))) {
        nd = nd->next;
    }
    if (!nd || nd == node->last->next) {
        return NULL;
    }
    return nd;
}

// 0x58B9D0: two nodes that do the same
Boolean func_0058B9D0(IRONode* a, IRONode* b) {
    IROLinear* x = NextWork(a->first, a);
    IROLinear* y = NextWork(b->first, b);
    IROLinear* rx;
    IROLinear* ry;
    Boolean xe;
    Boolean ye;

    while (x && y) {
        xe = x->type >= IROLinearOperand && x->type <= IROLinearFunccall;
        ye = y->type >= IROLinearOperand && y->type <= IROLinearFunccall;
        if (xe != ye) {
            return FALSE;
        }
        if (xe) {
            rx = x;
            ry = y;
        } else {
            if (x->type != y->type) {
                return FALSE;
            }
            switch (x->type) {
                case IROLinearEntry:
                case IROLinearExit:
                case IROLinearAsm:
                    return FALSE;
                case IROLinearGotoExpr:
                case IROLinearReturn:
                case IROLinearSwitch:
                case IROLinearBeginCatch:
                case IROLinearEndCatch:
                case IROLinearEndCatchDtor:
                case IROLinearEnd:
                    break;
                default:
                    CError_FATAL(1218);
                    break;
            }
            rx = func_00552E90(x);
            ry = func_00552E90(y);
        }
        if (!rx || !ry || !func_00555190(rx, ry)) {
            return FALSE;
        }
        x = NextWork(x->next, a);
        y = NextWork(y->next, b);
    }
    return !x && !y;
}

// 0x58B8C0
void IRO_SimplifyIfThenElse(void) {
    IRONode* node;
    IRONode* a;
    IRONode* b;
    IROLinear* last;
    Boolean changed = FALSE;

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->numsucc == 2 &&
            (node->last->type == IROLinearIf || node->last->type == IROLinearIfNot) &&
            (a = IRO_NodeTable[node->succ[0]]) &&
            (b = IRO_NodeTable[node->succ[1]]) &&
            a->numsucc == b->numsucc &&
            (a->numsucc == 0 || (a->numsucc == 1 && a->succ[0] == b->succ[0])) &&
            func_0058B9D0(a, b)) {
            last = node->last;
            if (last->type == IROLinearIf || last->type == IROLinearIfNot) {
                last->u.label.cond->flags &= ~IROLF_Reffed;
                last->type = IROLinearNop;
                IRO_Dump("Nopping If at %d because its successors are equivalent.\n", last->index);
                node->numsucc = 1;
                node->succ[0] = node->nextnode->index;
            }
            changed = TRUE;
        }
    }
    if (changed) {
        if (!DisableDueToAsm && copts.deadcode) {
            IRO_RemoveUnreachable();
        } else {
            IRO_ComputeSuccPred();
            IRO_ComputeDom();
        }
    }
}
