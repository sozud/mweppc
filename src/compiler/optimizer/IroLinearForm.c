#include "compiler/IroLinearForm.h"
#if MWCCPSP
#include "compiler/IrOptimizer.h"
#include "compiler/IroMalloc.h"
#include "compiler/Exceptions.h"
#include "compiler/objects.h"
#endif
#include "compiler/IroDump.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"
#include "compiler/CError.h"
#if MWCCPSP
#include "compiler/CDecl.h"
#endif
#include "compiler/CExpr.h"
#include "compiler/CFunc.h"
#include "compiler/CInt64.h"
#include "compiler/CompilerTools.h"
#include "compiler/CParser.h"
#include "compiler/enode.h"
#include "compiler/InlineAsmPPC.h"
#include "compiler/types.h"
#include <string.h>

typedef struct NullCheckListNode {
    SInt32 precompid;
    Object* tempobj;
    struct NullCheckListNode* next;
} NullCheckListNode;

IROLinear* IRO_FirstLinear;
IROLinear* IRO_LastLinear;
UInt32 IRO_NumLinear;
Statement* CurStat;
static NullCheckListNode* NullCheckList;
static Statement* CurrStmt;
static Statement* PrevStmt;
#if MWCCPSP
extern Boolean data_00625ECA;
static Statement* FirstStmt;
#endif

static void LinkLinear(IROLinear* linear) {
    linear->index = IRO_NumLinear++;
    if (IRO_FirstLinear) {
        IRO_LastLinear->next = linear;
    } else {
        IRO_FirstLinear = linear;
    }
    IRO_LastLinear = linear;
}

#if MWCCPSP
// 3.0.1 allocates through IRO_Alloc (0x4F8B10, oalloc); inlined into its
// callers.
IROLinear* IRO_NewLinear(IROLinearType type) {
    IROLinear* linear = IRO_Alloc(sizeof(IROLinear));
    memset(linear, 0, sizeof(IROLinear));
    linear->stmt = CurStat;
    linear->nodetype = EPOSTINC;
    linear->next = NULL;
    linear->type = type;
    linear->rtype = NULL;
    linear->unk18 = NULL;
    linear->flags = 0;
    linear->nodeflags = 0;
    linear->expr = NULL;
    return linear;
}
#else
IROLinear* IRO_NewLinear(IROLinearType type) {
    IROLinear* linear = oalloc(sizeof(IROLinear));
    memset(linear, 0, sizeof(IROLinear));
    linear->stmt = CurStat;
    linear->nodetype = EPOSTINC;
    linear->next = NULL;
    linear->type = type;
    linear->rtype = NULL;
    linear->flags = 0;
    linear->nodeflags = 0;
    linear->expr = NULL;
    linear->range = NULL;
    return linear;
}
#endif

#if MWCCPSP
static void MarkAssigned(IROLinear* linear, Boolean isModify) {
    IROLinear* inner;
    IROLinear* objref;
    int numObjRefs;
    int numOther;

    if (IS_LINEAR_MONADIC(linear, EINDIRECT) && IS_LINEAR_MONADIC(linear->u.monadic, EBITFIELD)) {
        linear->flags |= IROLF_Assigned;
        inner = linear->u.monadic->u.monadic;
        if (inner->type == IROLinearOperand) {
            inner->flags |= IROLF_Assigned;
            if (isModify) {
                linear->flags |= IROLF_Used;
                inner->flags |= IROLF_Used;
            }
        }

        if (IS_LINEAR_DIADIC(inner, EADD)) {
            if (IS_LINEAR_ENODE(inner->u.diadic.left, EOBJREF)) {
                if (IRO_IsIntConstant(inner->u.diadic.right)) {
                    inner->u.diadic.left->flags |= IROLF_Assigned;
                }
                if (isModify) {
                    linear->flags |= IROLF_Used;
                    inner->u.diadic.left->flags |= IROLF_Used;
                }
            }

#if MWCCPS2 && MWCCPS2_BUILD < 210
            // b145 decomposes into an allocated address record
            {
                IROAddrRecord301* rec = IRO_Alloc(sizeof(IROAddrRecord301));
                func_005513A0(rec, inner);
                func_005510D0(inner, rec, IRO_Alloc);
                if (rec->numObjRefs == 1) {
                    ((IROLinear*)rec->objRefs->element)->flags |= IROLF_Assigned;
                    if (isModify) {
                        linear->flags |= IROLF_Used;
                        ((IROLinear*)rec->objRefs->element)->flags |= IROLF_Used;
                    }
                }
            }
#else
            IRO_FindAddressObjRefs(inner, &objref, &numObjRefs, &numOther);
            if (numObjRefs == 1) {
                objref->flags |= IROLF_Assigned;
                if (isModify) {
                    linear->flags |= IROLF_Used;
                    objref->flags |= IROLF_Used;
                }
            }
#endif
        }
    }

    if (IS_LINEAR_MONADIC(linear, EINDIRECT)) {
        linear->flags |= IROLF_Assigned;
        if (linear->u.monadic->type == IROLinearOperand) {
            linear->u.monadic->flags |= IROLF_Assigned;
            if (isModify) {
                linear->flags |= IROLF_Used;
                linear->u.monadic->flags |= IROLF_Used;
            }
        }
    }

    if (IS_LINEAR_MONADIC(linear, EINDIRECT)) {
        linear->flags |= IROLF_Assigned;
        inner = linear->u.monadic;

        if (IS_LINEAR_DIADIC(inner, EADD)) {
            if (IS_LINEAR_ENODE(inner->u.diadic.left, EOBJREF)) {
                if (IRO_IsIntConstant(inner->u.diadic.right)) {
                    inner->u.diadic.left->flags |= IROLF_Assigned;
                }
                if (isModify) {
                    linear->flags |= IROLF_Used;
                    inner->u.diadic.left->flags |= IROLF_Used;
                }
            }

#if MWCCPS2 && MWCCPS2_BUILD < 210
            // b145 decomposes into an allocated address record
            {
                IROAddrRecord301* rec = IRO_Alloc(sizeof(IROAddrRecord301));
                func_005513A0(rec, inner);
                func_005510D0(inner, rec, IRO_Alloc);
                if (rec->numObjRefs == 1) {
                    ((IROLinear*)rec->objRefs->element)->flags |= IROLF_Assigned;
                    if (isModify) {
                        linear->flags |= IROLF_Used;
                        ((IROLinear*)rec->objRefs->element)->flags |= IROLF_Used;
                    }
                }
            }
#else
            IRO_FindAddressObjRefs(inner, &objref, &numObjRefs, &numOther);
            if (numObjRefs == 1) {
                objref->flags |= IROLF_Assigned;
                if (isModify) {
                    linear->flags |= IROLF_Used;
                    objref->flags |= IROLF_Used;
                }
            }
#endif
        }
    }
}
#else
static void MarkAssigned(IROLinear* linear, Boolean isModify) {
    IROLinear* inner;
    IROAddrRecord* rec;

    if (IS_LINEAR_MONADIC(linear, EINDIRECT) && IS_LINEAR_MONADIC(linear->u.monadic, EBITFIELD)) {
        linear->flags |= IROLF_Assigned;
        inner = linear->u.monadic->u.monadic;
        if (inner->type == IROLinearOperand) {
            inner->flags |= IROLF_Assigned;
            if (isModify) {
                linear->flags |= IROLF_Used;
                inner->flags |= IROLF_Used;
            }
        }

        if (IS_LINEAR_DIADIC(inner, EADD)) {
            if (IS_LINEAR_ENODE(inner->u.diadic.left, EOBJREF)) {
                if (IS_LINEAR_ENODE(inner->u.diadic.right, EINTCONST)) {
                    inner->u.diadic.left->flags |= IROLF_Assigned;
                }
                if (isModify) {
                    linear->flags |= IROLF_Used;
                    inner->u.diadic.left->flags |= IROLF_Used;
                }
            }

            rec = IRO_InitAddrRecordPointer(inner);
            IRO_DecomposeAddressExpression(inner, rec);
            if (rec->numObjRefs == 1 && IS_LINEAR_ENODE(inner->u.diadic.left, EOBJREF)) {
                ((IROLinear*)rec->objRefs->element)->flags |= IROLF_Assigned;
                if (isModify) {
                    linear->flags |= IROLF_Used;
                    ((IROLinear*)rec->objRefs->element)->flags |= IROLF_Used;
                }
            }
        }
    }

    if (IS_LINEAR_MONADIC(linear, EINDIRECT)) {
        linear->flags |= IROLF_Assigned;
        if (linear->u.monadic->type == IROLinearOperand) {
            linear->u.monadic->flags |= IROLF_Assigned;
            if (isModify) {
                linear->flags |= IROLF_Used;
                linear->u.monadic->flags |= IROLF_Used;
            }
        }
    }

    if (IS_LINEAR_MONADIC(linear, EINDIRECT)) {
        linear->flags |= IROLF_Assigned;
        inner = linear->u.monadic;

        if (IS_LINEAR_DIADIC(inner, EADD)) {
            if (IS_LINEAR_ENODE(inner->u.diadic.left, EOBJREF)) {
                if (IS_LINEAR_ENODE(inner->u.diadic.right, EINTCONST)) {
                    inner->u.diadic.left->flags |= IROLF_Assigned;
                }
                if (isModify) {
                    linear->flags |= IROLF_Used;
                    inner->u.diadic.left->flags |= IROLF_Used;
                }
            }

            rec = IRO_InitAddrRecordPointer(inner);
            IRO_DecomposeAddressExpression(inner, rec);
            if (rec->numObjRefs == 1 && IS_LINEAR_ENODE(inner->u.diadic.left, EOBJREF)) {
                ((IROLinear*)rec->objRefs->element)->flags |= IROLF_Assigned;
                if (isModify) {
                    linear->flags |= IROLF_Used;
                    ((IROLinear*)rec->objRefs->element)->flags |= IROLF_Used;
                }
            }
        }
    }
}
#endif

#if MWCCPSP
// 3.0.1 asks the back end (IRO_IsAddressMultiplyHook) instead of
// IRO_IsAddressMultiply.
static void MarkSubscript(IROLinear* linear) {
    linear->flags |= IROLF_Subs;
    if (IS_LINEAR_DIADIC(linear, EADD)) {
        if ((IRO_IsAddressMultiplyHook && IRO_IsAddressMultiplyHook(linear->u.diadic.left)) ||
            IS_LINEAR_MONADIC(linear->u.diadic.left, EINDIRECT)) {
            linear->u.diadic.left->flags |= IROLF_Subs;
        }
        if ((IRO_IsAddressMultiplyHook && IRO_IsAddressMultiplyHook(linear->u.diadic.right)) ||
            IS_LINEAR_MONADIC(linear->u.diadic.right, EINDIRECT)) {
            linear->u.diadic.right->flags |= IROLF_Subs;
        }

        if (IS_LINEAR_DIADIC(linear->u.diadic.left, EADD)) {
            MarkSubscript(linear->u.diadic.left);
        }
        if (IS_LINEAR_DIADIC(linear->u.diadic.right, EADD)) {
            MarkSubscript(linear->u.diadic.right);
        }
    }
}
#else
static void MarkSubscript(IROLinear* linear) {
    linear->flags |= IROLF_Subs;
    if (IS_LINEAR_DIADIC(linear, EADD)) {
        if (IRO_IsAddressMultiply(linear->u.diadic.left) ||
            IS_LINEAR_MONADIC(linear->u.diadic.left, EINDIRECT)) {
            linear->u.diadic.left->flags |= IROLF_Subs;
        }
        if (IRO_IsAddressMultiply(linear->u.diadic.right) ||
            IS_LINEAR_MONADIC(linear->u.diadic.right, EINDIRECT)) {
            linear->u.diadic.right->flags |= IROLF_Subs;
        }

        if (IS_LINEAR_DIADIC(linear->u.diadic.left, EADD)) {
            MarkSubscript(linear->u.diadic.left);
        }
        if (IS_LINEAR_DIADIC(linear->u.diadic.right, EADD)) {
            MarkSubscript(linear->u.diadic.right);
        }
    }
}
#endif

#if MWCCPSP
static void MarkArgs(IROLinear* linear) {
    int i;
    linear->flags |= IROLF_InCallArg;

    switch (linear->type) {
        case IROLinearGotoExpr:
            MarkArgs(linear->u.monadic);
            break;
        case IROLinearNop:
        case IROLinearEnd:
            break;
        case IROLinearOp1Arg:
        case IROLinearBeginCatch:
        case IROLinearEndCatch:
        case IROLinearEndCatchDtor:
            MarkArgs(linear->u.monadic);
            break;
        case IROLinearOp2Arg:
            MarkArgs(linear->u.diadic.left);
            MarkArgs(linear->u.diadic.right);
            break;
        case IROLinearIf:
        case IROLinearIfNot:
            MarkArgs(linear->u.label.cond);
            break;
        case IROLinearReturn:
            if (linear->u.monadic) {
                MarkArgs(linear->u.monadic);
            }
            break;
        case IROLinearSwitch:
            MarkArgs(linear->u.swtch.cond);
            break;
        case IROLinearOp3Arg:
            MarkArgs(linear->u.args3.a);
            MarkArgs(linear->u.args3.b);
            MarkArgs(linear->u.args3.c);
            break;
        case IROLinearFunccall:
            MarkArgs(linear->u.funccall.callee);
            for (i = 0; i < linear->u.funccall.argCount; i++) {
                MarkArgs(linear->u.funccall.args[i]);
            }
            break;
        default:
            break;
    }
}
#else
static void MarkArgs(IROLinear* linear) {
    int i;
    linear->flags |= IROLF_InCallArg;

    switch (linear->type) {
        case IROLinearNop:
        case IROLinearEnd:
            break;
        case IROLinearOp1Arg:
        case IROLinearBeginCatch:
        case IROLinearEndCatch:
        case IROLinearEndCatchDtor:
            MarkArgs(linear->u.monadic);
            break;
        case IROLinearOp2Arg:
            MarkArgs(linear->u.diadic.left);
            MarkArgs(linear->u.diadic.right);
            break;
        case IROLinearIf:
        case IROLinearIfNot:
            MarkArgs(linear->u.label.cond);
            break;
        case IROLinearReturn:
            if (linear->u.monadic) {
                MarkArgs(linear->u.monadic);
            }
            break;
        case IROLinearSwitch:
            MarkArgs(linear->u.swtch.cond);
            break;
        case IROLinearOp3Arg:
            MarkArgs(linear->u.args3.a);
            MarkArgs(linear->u.args3.b);
            MarkArgs(linear->u.args3.c);
            break;
        case IROLinearFunccall:
            MarkArgs(linear->u.funccall.callee);
            for (i = 0; i < linear->u.funccall.argCount; i++) {
                MarkArgs(linear->u.funccall.args[i]);
            }
            break;
        default:
            break;
    }
}
#endif

#if MWCCPSP
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 12 // b145 asserts (inlined into MarkAllSubExprs)
static inline void MarkSubExpr(IROLinear* linear) {
    int i;

    switch (linear->type) {
        case IROLinearGotoExpr:
            linear->u.monadic->flags |= IROLF_Reffed;
            break;
        case IROLinearNop:
        case IROLinearOperand:
        case IROLinearScope:
        case IROLinearGoto:
        case IROLinearLabel:
        case IROLinearEntry:
        case IROLinearExit:
        case IROLinearAsm:
        case IROLinearEnd:
            break;
        case IROLinearOp1Arg:
        case IROLinearBeginCatch:
        case IROLinearEndCatch:
        case IROLinearEndCatchDtor:
            linear->u.monadic->flags |= IROLF_Reffed;
            break;
        case IROLinearOp2Arg:
            linear->u.diadic.left->flags |= IROLF_Reffed;
            linear->u.diadic.right->flags |= IROLF_Reffed;
            break;
        case IROLinearIf:
        case IROLinearIfNot:
            linear->u.label.cond->flags |= IROLF_Reffed;
            break;
        case IROLinearReturn:
            if (linear->u.monadic) {
                linear->u.monadic->flags |= IROLF_Reffed;
            }
            break;
        case IROLinearSwitch:
            linear->u.swtch.cond->flags |= IROLF_Reffed;
            break;
        case IROLinearOp3Arg:
            linear->u.args3.a->flags |= IROLF_Reffed;
            linear->u.args3.b->flags |= IROLF_Reffed;
            linear->u.args3.c->flags |= IROLF_Reffed;
            break;
        case IROLinearFunccall:
            linear->u.funccall.callee->flags |= IROLF_Reffed;
            for (i = 0; i < linear->u.funccall.argCount; i++) {
                linear->u.funccall.args[i]->flags |= IROLF_Reffed;
            }
            break;
        default:
            CError_FATAL(396);
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#else
static inline void MarkSubExpr(IROLinear* linear) {
    int i;

    switch (linear->type) {
        case IROLinearNop:
        case IROLinearOperand:
        case IROLinearGoto:
        case IROLinearLabel:
        case IROLinearEntry:
        case IROLinearExit:
        case IROLinearAsm:
        case IROLinearEnd:
            break;
        case IROLinearOp1Arg:
        case IROLinearBeginCatch:
        case IROLinearEndCatch:
        case IROLinearEndCatchDtor:
            linear->u.monadic->flags |= IROLF_Reffed;
            break;
        case IROLinearOp2Arg:
            linear->u.diadic.left->flags |= IROLF_Reffed;
            linear->u.diadic.right->flags |= IROLF_Reffed;
            break;
        case IROLinearIf:
        case IROLinearIfNot:
            linear->u.label.cond->flags |= IROLF_Reffed;
            break;
        case IROLinearReturn:
            if (linear->u.monadic) {
                linear->u.monadic->flags |= IROLF_Reffed;
            }
            break;
        case IROLinearSwitch:
            linear->u.swtch.cond->flags |= IROLF_Reffed;
            break;
        case IROLinearOp3Arg:
            linear->u.args3.a->flags |= IROLF_Reffed;
            linear->u.args3.b->flags |= IROLF_Reffed;
            linear->u.args3.c->flags |= IROLF_Reffed;
            break;
        case IROLinearFunccall:
            linear->u.funccall.callee->flags |= IROLF_Reffed;
            for (i = 0; i < linear->u.funccall.argCount; i++) {
                linear->u.funccall.args[i]->flags |= IROLF_Reffed;
            }
            break;
        default:
            CError_FATAL(368);
    }
}
#endif

#if MWCCPSP
static void MarkSubs1(IROLinear* linear) {
    IROLinear* objref;
    int numObjRefs;
    int numOther;

    if (IS_LINEAR_MONADIC(linear, EBITFIELD)) {
        linear = linear->u.monadic;
        if (IS_LINEAR_ENODE(linear, EOBJREF)) {
            linear->flags |= IROLF_Ind;
        }
    }

    if (IS_LINEAR_DIADIC(linear, EADD)) {
        if (IS_LINEAR_ENODE(linear->u.diadic.left, EOBJREF) &&
            IRO_IsIntConstant(linear->u.diadic.right)) {
            linear->u.diadic.left->flags |= IROLF_Ind;
        }

#if MWCCPS2 && MWCCPS2_BUILD < 210
        // b145 decomposes into an allocated address record
        {
            IROAddrRecord301* rec = IRO_Alloc(sizeof(IROAddrRecord301));
            func_005513A0(rec, linear);
            func_005510D0(linear, rec, IRO_Alloc);
            if (rec->numObjRefs == 1) {
                ((IROLinear*)rec->objRefs->element)->flags |= IROLF_Ind;
            }
        }
#else
        IRO_FindAddressObjRefs(linear, &objref, &numObjRefs, &numOther);
        if (numObjRefs == 1) {
            objref->flags |= IROLF_Ind;
        }
#endif
    }
}
#else
static void MarkSubs1(IROLinear* linear) {
    IROAddrRecord* rec;

    if (IS_LINEAR_MONADIC(linear, EBITFIELD)) {
        linear = linear->u.monadic;
        if (IS_LINEAR_ENODE(linear, EOBJREF)) {
            linear->flags |= IROLF_Ind;
        }
    }

    if (IS_LINEAR_DIADIC(linear, EADD)) {
        if (IS_LINEAR_ENODE(linear->u.diadic.left, EOBJREF) &&
            IS_LINEAR_ENODE(linear->u.diadic.right, EINTCONST)) {
            linear->u.diadic.left->flags |= IROLF_Ind;
        }

        rec = IRO_InitAddrRecordPointer(linear);
        IRO_DecomposeAddressExpression(linear, rec);
        if (rec->numObjRefs == 1 && IS_LINEAR_ENODE(linear->u.diadic.left, EOBJREF)) {
            ((IROLinear*)rec->objRefs->element)->flags |= IROLF_Ind;
        }
    }
}
#endif

#if !MWCCPSP // 3.0.1: see the pre-linearizer block below
static void MakeLeftChildAsAssignment(ENode* enode) {
    Statement* stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_EXPRESSION;
    stmt->expr = enode->data.diadic.left;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;
}

static void AssignCommaRToTemp(ENode* enode, Boolean discardResult) {
    Statement* stmt;

    if (!IS_TYPE_VOID(enode->rtype) && !discardResult) {
        Object* obj = create_temp_object(enode->rtype);
        ENode* indnode = IRO_NewENode(EINDIRECT);
        indnode->data.monadic = create_objectrefnode(obj);

        if (!IS_TYPE_VOID(enode->rtype)) {
            indnode->rtype = enode->rtype;
        } else {
            CError_FATAL(548);
        }

        stmt = lalloc(sizeof(Statement));
        memset(stmt, 0, sizeof(Statement));
        stmt->type = ST_EXPRESSION;
        stmt->expr = IRO_NewENode(EASS);
        stmt->expr->data.diadic.left = indnode;
        stmt->expr->data.diadic.right = enode->data.diadic.right;
        stmt->expr->rtype = enode->rtype;
        stmt->dobjstack = CurrStmt->dobjstack;
        stmt->sourceoffset = CurrStmt->sourceoffset;
        stmt->sourcefilepath = CurrStmt->sourcefilepath;
        stmt->value = CurrStmt->value;
        stmt->flags = CurrStmt->flags;

        if (PrevStmt) {
            stmt->next = PrevStmt->next;
            PrevStmt->next = stmt;
        } else {
            stmt->next = NULL;
        }
        PrevStmt = stmt;

        enode->type = EINDIRECT;
        enode->data.monadic = create_objectrefnode(obj);
        CError_ASSERT(580, !IS_TYPE_VOID(enode->rtype));
    } else {
        stmt = lalloc(sizeof(Statement));
        memset(stmt, 0, sizeof(Statement));
        stmt->type = ST_EXPRESSION;
        stmt->expr = enode->data.diadic.right;
        stmt->dobjstack = CurrStmt->dobjstack;
        stmt->sourceoffset = CurrStmt->sourceoffset;
        stmt->sourcefilepath = CurrStmt->sourcefilepath;
        stmt->value = CurrStmt->value;
        stmt->flags = CurrStmt->flags;

        if (PrevStmt) {
            stmt->next = PrevStmt->next;
            PrevStmt->next = stmt;
        } else {
            stmt->next = NULL;
        }
        PrevStmt = stmt;

        enode->type = EINTCONST;
        enode->data.intval = cint64_zero;
    }
}

static void AssignDangerousArgumentToTemp(ENode* enode) {
    Statement* stmt;
    Object* obj;
    ENode* indnode;
    ENode* rightnode;

    obj = create_temp_object(enode->rtype);
    indnode = IRO_NewENode(EINDIRECT);
    indnode->data.monadic = create_objectrefnode(obj);

    if (!IS_TYPE_VOID(enode->rtype)) {
        indnode->rtype = enode->rtype;
    } else {
        CError_FATAL(627);
    }

    rightnode = IRO_NewENode(enode->type);
    memcpy(rightnode, enode, sizeof(ENode));

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_EXPRESSION;
    stmt->expr = IRO_NewENode(EASS);
    stmt->expr->data.diadic.left = indnode;
    stmt->expr->data.diadic.right = rightnode;
    stmt->expr->rtype = enode->rtype;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;

    enode->type = EINDIRECT;
    enode->data.monadic = create_objectrefnode(obj);
}

static void CreateTempAssignmentToZero(ENode* enode, Object** objptr) {
    Statement* stmt;
    ENode* indnode;
    ENode* rightnode;

    *objptr = create_temp_object(enode->rtype);
    indnode = IRO_NewENode(EINDIRECT);
    indnode->data.monadic = create_objectrefnode(*objptr);
    if (!IS_TYPE_VOID(enode->rtype)) {
        indnode->rtype = enode->rtype;
    } else {
        CError_FATAL(678);
    }

    rightnode = IRO_NewENode(EINTCONST);
    rightnode->data.intval = cint64_zero;
    rightnode->rtype = enode->rtype;

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_EXPRESSION;
    stmt->expr = IRO_NewENode(EASS);
    stmt->expr->data.diadic.left = indnode;
    stmt->expr->data.diadic.right = rightnode;
    stmt->expr->rtype = enode->rtype;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;
}

static void CreateTempAssignmentToOne(ENode* enode, Object** objptr) {
    Statement* stmt;
    ENode* indnode;
    ENode* rightnode;

    *objptr = create_temp_object(enode->rtype);
    indnode = IRO_NewENode(EINDIRECT);
    indnode->data.monadic = create_objectrefnode(*objptr);
    if (!IS_TYPE_VOID(enode->rtype)) {
        indnode->rtype = enode->rtype;
    } else {
        CError_FATAL(720);
    }

    rightnode = IRO_NewENode(EINTCONST);
    rightnode->data.intval = cint64_one;
    rightnode->rtype = enode->rtype;

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_EXPRESSION;
    stmt->expr = IRO_NewENode(EASS);
    stmt->expr->data.diadic.left = indnode;
    stmt->expr->data.diadic.right = rightnode;
    stmt->expr->rtype = enode->rtype;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;
}

static void GenerateCondExpression(ENode* enode, CLabel** label) {
    Statement* stmt;

    *label = IRO_NewLabel();

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_IFNGOTO;
    stmt->expr = enode->data.cond.cond;
    stmt->label = *label;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;
}

static void GenerateGoToStmt(ENode* enode, CLabel** label) {
    Statement* stmt;

    *label = IRO_NewLabel();

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_GOTO;
    stmt->label = *label;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;
}

static void PutTempOnNullCheckList(ENode* expr, Object* obj) {
    NullCheckListNode* n = lalloc(sizeof(NullCheckListNode));
    n->precompid = expr->data.nullcheck.precompid;
    n->tempobj = obj;
    n->next = NULL;
    if (NullCheckList) {
        n->next = NullCheckList;
        NullCheckList = n;
    } else {
        NullCheckList = n;
    }
}

static Object* GetTempFromtheList(ENode* expr) {
    NullCheckListNode* n;

    for (n = NullCheckList; n; n = n->next) {
        if (n->precompid == expr->data.precompid) {
            break;
        }
    }

    CError_ASSERT(839, n);
    return n->tempobj;
}

static void GenerateNullcheckExprTempAssignment(ENode* enode, Object** objptr) {
    Statement* stmt;
    ENode* indnode;

    if (!IS_TYPE_VOID(enode->rtype)) {
        *objptr = create_temp_object(enode->rtype);
    } else {
        *objptr = create_temp_object(enode->data.nullcheck.nullcheckexpr->rtype);
    }

    PutTempOnNullCheckList(enode, *objptr);

    indnode = IRO_NewENode(EINDIRECT);
    indnode->data.monadic = create_objectrefnode(*objptr);
    if (!IS_TYPE_VOID(enode->rtype)) {
        indnode->rtype = enode->rtype;
    } else {
        indnode->rtype = enode->data.nullcheck.nullcheckexpr->rtype;
    }

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_EXPRESSION;
    stmt->expr = IRO_NewENode(EASS);
    stmt->expr->data.diadic.left = indnode;
    stmt->expr->data.diadic.right = enode->data.nullcheck.nullcheckexpr;
    if (!IS_TYPE_VOID(enode->rtype)) {
        stmt->expr->rtype = enode->rtype;
    } else {
        stmt->expr->rtype = enode->data.nullcheck.nullcheckexpr->rtype;
    }

    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;
}

static void GenerateExpr1TempAssignment(ENode* enode, Object** objptr) {
    Statement* stmt;
    ENode* indnode;

    if (!IS_TYPE_VOID(enode->rtype) && !IS_TYPE_VOID(enode->data.cond.expr1->rtype)) {
        *objptr = create_temp_object(enode->rtype);
        indnode = IRO_NewENode(EINDIRECT);
        indnode->data.monadic = create_objectrefnode(*objptr);
        indnode->rtype = enode->rtype;
        CError_ASSERT(905, !IS_TYPE_VOID(enode->rtype));
    }

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_EXPRESSION;
    if (!IS_TYPE_VOID(enode->rtype) && !IS_TYPE_VOID(enode->data.cond.expr1->rtype)) {
        stmt->expr = IRO_NewENode(EASS);
        stmt->expr->data.diadic.left = indnode;
        stmt->expr->data.diadic.right = enode->data.cond.expr1;
        stmt->expr->rtype = enode->rtype;
    } else {
        stmt->expr = enode->data.cond.expr1;
    }

    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;
}

static void GenerateExpr2TempAssignment(ENode* enode, Object** objptr) {
    Statement* stmt;
    ENode* indnode;

    if (!IS_TYPE_VOID(enode->rtype) && !IS_TYPE_VOID(enode->data.cond.expr2->rtype)) {
        if (!*objptr) {
            *objptr = create_temp_object(enode->rtype);
        }
        indnode = IRO_NewENode(EINDIRECT);
        indnode->data.monadic = create_objectrefnode(*objptr);
        indnode->rtype = enode->rtype;
        CError_ASSERT(956, !IS_TYPE_VOID(enode->rtype));
    }

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_EXPRESSION;
    if (!IS_TYPE_VOID(enode->rtype) && !IS_TYPE_VOID(enode->data.cond.expr2->rtype)) {
        stmt->expr = IRO_NewENode(EASS);
        stmt->expr->data.diadic.left = indnode;
        stmt->expr->data.diadic.right = enode->data.cond.expr2;
        stmt->expr->rtype = enode->rtype;
    } else {
        stmt->expr = enode->data.cond.expr2;
    }

    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;
}

static void GenerateForceLoadTempAssignment(ENode* enode, Object** objptr) {
    Statement* stmt;
    ENode* indnode;

    if (!IS_TYPE_VOID(enode->rtype)) {
        *objptr = create_temp_object(enode->rtype);
        indnode = IRO_NewENode(EINDIRECT);
        indnode->data.monadic = create_objectrefnode(*objptr);
        indnode->rtype = enode->rtype;
        CError_ASSERT(1006, !IS_TYPE_VOID(enode->rtype));
    }

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_EXPRESSION;
    if (!IS_TYPE_VOID(enode->rtype)) {
        stmt->expr = IRO_NewENode(EASS);
        stmt->expr->data.diadic.left = indnode;
        stmt->expr->data.diadic.right = enode->data.monadic;
        stmt->expr->rtype = enode->rtype;
    } else {
        stmt->expr = enode->data.monadic;
    }

    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;
}

static void GenerateLabel(CLabel** labelptr) {
    Statement* stmt;

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_LABEL;
    stmt->label = *labelptr;
    stmt->label->stmt = stmt;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;
}

static void GenerateIfNotTemp(ENode* enode, Object** objptr, CLabel** labelptr) {
    Statement* stmt;
    ENode* indnode;

    indnode = IRO_NewENode(EINDIRECT);
    indnode->data.monadic = create_objectrefnode(*objptr);
    if (!IS_TYPE_VOID(enode->rtype)) {
        indnode->rtype = enode->rtype;
    } else {
        indnode->rtype = enode->data.monadic->rtype;
    }

    *labelptr = IRO_NewLabel();

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_IFNGOTO;
    stmt->expr = indnode;
    stmt->label = *labelptr;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;
}

static void GenerateNullcheckCondExpr(ENode* enode, Object** objptr) {
    Statement* stmt;
    ENode* indnode;

    indnode = IRO_NewENode(EINDIRECT);
    indnode->data.monadic = create_objectrefnode(*objptr);
    if (!IS_TYPE_VOID(enode->rtype)) {
        indnode->rtype = enode->rtype;
    } else {
        indnode->rtype = enode->data.nullcheck.nullcheckexpr->rtype;
    }

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_EXPRESSION;
    stmt->expr = IRO_NewENode(EASS);
    stmt->expr->data.diadic.left = indnode;
    stmt->expr->data.diadic.right = enode->data.nullcheck.condexpr;
    if (!IS_TYPE_VOID(enode->rtype)) {
        stmt->expr->rtype = enode->rtype;
    } else {
        stmt->expr->rtype = enode->data.nullcheck.nullcheckexpr->rtype;
    }
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;
}

static void TransformLogicalAndLHS(ENode* enode, Object** objptr, CLabel** labelptr) {
    Statement* stmt;

    *labelptr = IRO_NewLabel();

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_IFNGOTO;
    stmt->expr = enode->data.diadic.left;
    stmt->label = *labelptr;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;
}

static void TransformLogicalAndRHS(ENode* enode, Object** objptr, CLabel** labelptr) {
    Statement* stmt;
    ENode* indnode;
    ENode* rightnode;

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_IFNGOTO;
    stmt->expr = enode->data.diadic.right;
    stmt->label = *labelptr;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;

    indnode = IRO_NewENode(EINDIRECT);
    indnode->data.monadic = create_objectrefnode(*objptr);
    if (!IS_TYPE_VOID(enode->rtype)) {
        indnode->rtype = enode->rtype;
    } else {
        CError_FATAL(1228);
    }

    rightnode = IRO_NewENode(EINTCONST);
    rightnode->data.intval = cint64_one;
    rightnode->rtype = enode->rtype;

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_EXPRESSION;
    stmt->expr = IRO_NewENode(EASS);
    stmt->expr->data.diadic.left = indnode;
    stmt->expr->data.diadic.right = rightnode;
    stmt->expr->rtype = enode->rtype;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_LABEL;
    stmt->label = *labelptr;
    stmt->label->stmt = stmt;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;

    enode->type = EINDIRECT;
    enode->data.monadic = create_objectrefnode(*objptr);
    CError_ASSERT(1279, !IS_TYPE_VOID(enode->rtype));
}

static void TransformLogicalOrLHS(ENode* enode, Object** objptr, CLabel** labelptr) {
    Statement* stmt;

    *labelptr = IRO_NewLabel();

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_IFGOTO;
    stmt->expr = enode->data.diadic.left;
    stmt->label = *labelptr;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;
}

static void TransformLogicalOrRHS(ENode* enode, Object** objptr, CLabel** labelptr) {
    Statement* stmt;
    ENode* indnode;
    ENode* rightnode;

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_IFGOTO;
    stmt->expr = enode->data.diadic.right;
    stmt->label = *labelptr;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;

    indnode = IRO_NewENode(EINDIRECT);
    indnode->data.monadic = create_objectrefnode(*objptr);
    if (!IS_TYPE_VOID(enode->rtype)) {
        indnode->rtype = enode->rtype;
    } else {
        CError_FATAL(1357);
    }

    rightnode = IRO_NewENode(EINTCONST);
    rightnode->data.intval = cint64_zero;
    rightnode->rtype = enode->rtype;

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_EXPRESSION;
    stmt->expr = IRO_NewENode(EASS);
    stmt->expr->data.diadic.left = indnode;
    stmt->expr->data.diadic.right = rightnode;
    stmt->expr->rtype = enode->rtype;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;

    stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = ST_LABEL;
    stmt->label = *labelptr;
    stmt->label->stmt = stmt;
    stmt->dobjstack = CurrStmt->dobjstack;
    stmt->sourceoffset = CurrStmt->sourceoffset;
    stmt->sourcefilepath = CurrStmt->sourcefilepath;
    stmt->value = CurrStmt->value;
    stmt->flags = CurrStmt->flags;

    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
    }
    PrevStmt = stmt;

    enode->type = EINDIRECT;
    enode->data.monadic = create_objectrefnode(*objptr);
    CError_ASSERT(1408, !IS_TYPE_VOID(enode->rtype));
}

#endif
#if MWCCPSP
// 3.0.1's pre-linearizer. Statements are made by IRO_NewStatement and get
// the current statement's source position from IRO_CopySourceInfo; a
// statement inserted in front of the first one becomes FirstStmt (with no
// successor, as in the original). Temporaries and the nodes that refer to
// them take the unqualified type. The fourth argument asks IRO_Hook3
// whether the expression (an argument) must be evaluated into a temporary.
static void LinkStatement(Statement* stmt) {
    IRO_CopySourceInfo(stmt, CurrStmt);
    if (PrevStmt) {
        stmt->next = PrevStmt->next;
        PrevStmt->next = stmt;
    } else {
        stmt->next = NULL;
        FirstStmt = stmt;
    }
    PrevStmt = stmt;
}

static ENode* NewObjRef(Object* obj, Type* rtype) {
    ENode* ind = IRO_NewENode(EINDIRECT);
    ind->data.monadic = create_objectrefnode(obj);
    ind->rtype = rtype;
    return ind;
}

#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
static void AssignCommaRToTemp(ENode* enode, Boolean discardResult) {
    Statement* stmt;
    Object* obj;
    ENode* ind;

    if (IS_TYPE_VOID(enode->rtype) || discardResult) {
        stmt = IRO_NewStatement(ST_EXPRESSION, enode->data.diadic.right);
        LinkStatement(stmt);
        enode->type = EINTCONST;
        enode->data.intval = cint64_zero;
    } else {
        obj = create_temp_object(IRO_UnqualifiedType(enode->rtype));
        ind = IRO_NewENode(EINDIRECT);
        ind->data.monadic = create_objectrefnode(obj);
        if (!IS_TYPE_VOID(enode->rtype)) {
            ind->rtype = IRO_UnqualifiedType(enode->rtype);
        } else {
            CError_FATAL(579);
        }
        stmt = IRO_NewStatement(ST_EXPRESSION, IRO_NewENode(EASS));
        stmt->expr->data.diadic.left = ind;
        stmt->expr->data.diadic.right = enode->data.diadic.right;
        stmt->expr->rtype = IRO_UnqualifiedType(enode->rtype);
        LinkStatement(stmt);
        enode->type = EINDIRECT;
        enode->data.monadic = create_objectrefnode(obj);
        CError_ASSERT(598, !IS_TYPE_VOID(enode->rtype));
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
static void AssignDangerousArgumentToTemp(ENode* enode) {
    Statement* stmt;
    Object* obj;
    ENode* ind;
    ENode* copy;

    obj = create_temp_object(IRO_UnqualifiedType(enode->rtype));
    ind = IRO_NewENode(EINDIRECT);
    ind->data.monadic = create_objectrefnode(obj);
    if (!IS_TYPE_VOID(enode->rtype)) {
        ind->rtype = obj->type;
    } else {
        CError_FATAL(631);
    }
    copy = IRO_NewENode(enode->type);
    *copy = *enode;
    stmt = IRO_NewStatement(ST_EXPRESSION, IRO_NewENode(EASS));
    stmt->expr->data.diadic.left = ind;
    stmt->expr->data.diadic.right = copy;
    stmt->expr->rtype = IRO_UnqualifiedType(enode->rtype);
    LinkStatement(stmt);
    enode->type = EINDIRECT;
    enode->data.monadic = create_objectrefnode(obj);
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
static void TransformLogicalOrRHS(ENode* enode, Object** objptr, CLabel** labelptr) {
    Statement* stmt;
    ENode* ind;
    ENode* zero;

    stmt = IRO_NewStatement(ST_IFGOTO, enode->data.diadic.right);
    stmt->label = *labelptr;
    LinkStatement(stmt);

    ind = IRO_NewENode(EINDIRECT);
    ind->data.monadic = create_objectrefnode(*objptr);
    if (!IS_TYPE_VOID(enode->rtype)) {
        ind->rtype = IRO_UnqualifiedType(enode->rtype);
    } else {
        CError_FATAL(1141);
    }
    zero = IRO_NewIntConstENode(cint64_zero, enode->rtype);
    stmt = IRO_NewStatement(ST_EXPRESSION, IRO_NewENode(EASS));
    stmt->expr->data.diadic.left = ind;
    stmt->expr->data.diadic.right = zero;
    stmt->expr->rtype = zero->rtype;
    LinkStatement(stmt);

    stmt = IRO_NewStatement(ST_LABEL, NULL);
    stmt->label = *labelptr;
    stmt->label->stmt = stmt;
    LinkStatement(stmt);

    enode->type = EINDIRECT;
    enode->data.monadic = create_objectrefnode(*objptr);
    CError_ASSERT(1165, !IS_TYPE_VOID(enode->rtype));
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
static void TransformLogicalAndRHS(ENode* enode, Object** objptr, CLabel** labelptr) {
    Statement* stmt;
    ENode* ind;
    ENode* one;

    stmt = IRO_NewStatement(ST_IFNGOTO, enode->data.diadic.right);
    stmt->label = *labelptr;
    LinkStatement(stmt);

    ind = IRO_NewENode(EINDIRECT);
    ind->data.monadic = create_objectrefnode(*objptr);
    if (!IS_TYPE_VOID(enode->rtype)) {
        ind->rtype = IRO_UnqualifiedType(enode->rtype);
    } else {
        CError_FATAL(1065);
    }
    one = IRO_NewIntConstENode(cint64_one, enode->rtype);
    stmt = IRO_NewStatement(ST_EXPRESSION, IRO_NewENode(EASS));
    stmt->expr->data.diadic.left = ind;
    stmt->expr->data.diadic.right = one;
    stmt->expr->rtype = one->rtype;
    LinkStatement(stmt);

    stmt = IRO_NewStatement(ST_LABEL, NULL);
    stmt->label = *labelptr;
    stmt->label->stmt = stmt;
    LinkStatement(stmt);

    enode->type = EINDIRECT;
    enode->data.monadic = create_objectrefnode(*objptr);
    CError_ASSERT(1089, !IS_TYPE_VOID(enode->rtype));
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
static void GenerateForceLoadTempAssignment(ENode* enode, Object** objptr) {
    Statement* stmt;
    ENode* ind;

    if (!IS_TYPE_VOID(enode->rtype)) {
        *objptr = create_temp_object(IRO_UnqualifiedType(enode->rtype));
        ind = IRO_NewENode(EINDIRECT);
        ind->data.monadic = create_objectrefnode(*objptr);
        ind->rtype = (*objptr)->type;
        if (IS_TYPE_VOID(enode->rtype)) {
            CError_FATAL(917);
        }
    }
    stmt = IRO_NewStatement(ST_EXPRESSION, NULL);
    if (IS_TYPE_VOID(enode->rtype)) {
        stmt->expr = enode->data.monadic;
    } else {
        stmt->expr = IRO_NewENode(EASS);
        stmt->expr->data.diadic.left = ind;
        stmt->expr->data.diadic.right = enode->data.monadic;
        stmt->expr->rtype = IRO_UnqualifiedType(enode->rtype);
    }
    LinkStatement(stmt);
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
static void GenerateExpr2TempAssignment(ENode* enode, Object** objptr) {
    Statement* stmt;
    ENode* ind;

    if (!IS_TYPE_VOID(enode->rtype) && !IS_TYPE_VOID(enode->data.cond.expr2->rtype)) {
        if (!*objptr) {
            *objptr = create_temp_object(IRO_UnqualifiedType(enode->rtype));
        }
        ind = IRO_NewENode(EINDIRECT);
        ind->data.monadic = create_objectrefnode(*objptr);
        ind->rtype = IRO_UnqualifiedType(enode->rtype);
        if (IS_TYPE_VOID(enode->rtype)) {
            CError_FATAL(879);
        }
    }
    stmt = IRO_NewStatement(ST_EXPRESSION, NULL);
    if (IS_TYPE_VOID(enode->rtype) || IS_TYPE_VOID(enode->data.cond.expr2->rtype)) {
        stmt->expr = enode->data.cond.expr2;
    } else {
        stmt->expr = IRO_NewENode(EASS);
        stmt->expr->data.diadic.left = ind;
        stmt->expr->data.diadic.right = enode->data.cond.expr2;
        stmt->expr->rtype = IRO_UnqualifiedType(enode->rtype);
    }
    LinkStatement(stmt);
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
static void GenerateExpr1TempAssignment(ENode* enode, Object** objptr) {
    Statement* stmt;
    ENode* ind;

    if (!IS_TYPE_VOID(enode->rtype) && !IS_TYPE_VOID(enode->data.cond.expr1->rtype)) {
        *objptr = create_temp_object(IRO_UnqualifiedType(enode->rtype));
        ind = IRO_NewENode(EINDIRECT);
        ind->data.monadic = create_objectrefnode(*objptr);
        ind->rtype = (*objptr)->type;
        if (IS_TYPE_VOID(enode->rtype)) {
            CError_FATAL(840);
        }
    }
    stmt = IRO_NewStatement(ST_EXPRESSION, NULL);
    if (IS_TYPE_VOID(enode->rtype) || IS_TYPE_VOID(enode->data.cond.expr1->rtype)) {
        stmt->expr = enode->data.cond.expr1;
    } else {
        stmt->expr = IRO_NewENode(EASS);
        stmt->expr->data.diadic.left = ind;
        stmt->expr->data.diadic.right = enode->data.cond.expr1;
        stmt->expr->rtype = IRO_UnqualifiedType(enode->rtype);
    }
    LinkStatement(stmt);
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// the type a null check's temporary has: the check's own, or for a void
// check the checked expression's
#define NULLCHECK_TYPE(e) \
    (IS_TYPE_VOID((e)->rtype) ? IRO_UnqualifiedType((e)->data.nullcheck.nullcheckexpr->rtype) \
                              : IRO_UnqualifiedType((e)->rtype))

static void GenerateNullcheckExprTempAssignment(ENode* enode, Object** objptr) {
    Statement* stmt;
    ENode* ind;
    ENode* conv;
    NullCheckListNode* n;

    if (IS_TYPE_VOID(enode->rtype)) {
        *objptr =
            create_temp_object(IRO_UnqualifiedType(enode->data.nullcheck.nullcheckexpr->rtype));
    } else {
        *objptr = create_temp_object(IRO_UnqualifiedType(enode->rtype));
    }

    n = lalloc(sizeof(NullCheckListNode));
    n->precompid = enode->data.nullcheck.precompid;
    n->tempobj = *objptr;
    n->next = NULL;
    if (NullCheckList) {
        n->next = NullCheckList;
    }
    NullCheckList = n;

    ind = IRO_NewENode(EINDIRECT);
    ind->data.monadic = create_objectrefnode(*objptr);
    ind->rtype = NULLCHECK_TYPE(enode);

    stmt = IRO_NewStatement(ST_EXPRESSION, IRO_NewENode(EASS));
    stmt->expr->data.diadic.left = ind;
    stmt->expr->data.diadic.right = enode->data.nullcheck.nullcheckexpr;
    if (!IRO_TypesEqual(
            stmt->expr->data.diadic.left->rtype, stmt->expr->data.diadic.right->rtype
        )) {
        conv = IRO_NewENode(ETYPCON);
        conv->data.monadic = stmt->expr->data.diadic.right;
        conv->rtype = IRO_UnqualifiedType(stmt->expr->data.diadic.left->rtype);
        stmt->expr->data.diadic.right = conv;
    }
    stmt->expr->rtype = NULLCHECK_TYPE(enode);
    LinkStatement(stmt);
}

static void GenerateNullcheckCondExpr(ENode* enode, Object** objptr) {
    Statement* stmt;
    ENode* ind;
    ENode* conv;

    if (IS_TYPE_VOID(enode->data.nullcheck.condexpr->rtype)) {
        stmt = IRO_NewStatement(ST_EXPRESSION, enode->data.nullcheck.condexpr);
    } else {
        ind = IRO_NewENode(EINDIRECT);
        ind->data.monadic = create_objectrefnode(*objptr);
        ind->rtype = NULLCHECK_TYPE(enode);
        stmt = IRO_NewStatement(ST_EXPRESSION, IRO_NewENode(EASS));
        stmt->expr->data.diadic.left = ind;
        stmt->expr->data.diadic.right = enode->data.nullcheck.condexpr;
        if (!IRO_TypesEqual(
                stmt->expr->data.diadic.left->rtype, stmt->expr->data.diadic.right->rtype
            )) {
            conv = IRO_NewENode(ETYPCON);
            conv->data.monadic = stmt->expr->data.diadic.right;
            conv->rtype = IRO_UnqualifiedType(stmt->expr->data.diadic.left->rtype);
            stmt->expr->data.diadic.right = conv;
        }
        stmt->expr->rtype = NULLCHECK_TYPE(enode);
    }
    LinkStatement(stmt);
}

#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
static void LinearizeExpr1(
    ENode* expr, ENode* parent, Boolean discardResult, Boolean checkArgument
) {
    Statement* stmt;
    Boolean isAssign;

    isAssign = FALSE;
    switch (expr->type) {
        case EPOSTINC:
        case EPOSTDEC:
        case EPREINC:
        case EPREDEC:
        case EINDIRECT:
        case EMONMIN:
        case EBINNOT:
        case ELOGNOT:
        case EFORCELOAD:
        case ETYPCON:
        case EBITFIELD:
            LinearizeExpr1(expr->data.monadic, expr, FALSE, FALSE);
            if (expr->type == EFORCELOAD) {
                Object* obj;
                GenerateForceLoadTempAssignment(expr, &obj);
                expr->type = EINDIRECT;
                expr->data.monadic = create_objectrefnode(obj);
                CError_ASSERT(1195, !IS_TYPE_VOID(expr->rtype));
            }
            break;
        case ELAND:
        case ELOR:
            if (expr->type == ELAND) {
                CLabel* label;
                Object* obj;
                ENode* ind;
                ENode* zero;

                obj = create_temp_object(IRO_UnqualifiedType(expr->rtype));
                ind = IRO_NewENode(EINDIRECT);
                ind->data.monadic = create_objectrefnode(obj);
                if (!IS_TYPE_VOID(expr->rtype)) {
                    ind->rtype = obj->type;
                } else {
                    CError_FATAL(669);
                }
                zero = IRO_NewIntConstENode(cint64_zero, obj->type);
                stmt = IRO_NewStatement(ST_EXPRESSION, IRO_NewENode(EASS));
                stmt->expr->data.diadic.left = ind;
                stmt->expr->data.diadic.right = zero;
                stmt->expr->rtype = IRO_UnqualifiedType(expr->rtype);
                LinkStatement(stmt);

                LinearizeExpr1(expr->data.diadic.left, expr, FALSE, FALSE);
                label = IRO_NewLabel();
                stmt = IRO_NewStatement(ST_IFNGOTO, expr->data.diadic.left);
                stmt->label = label;
                LinkStatement(stmt);

                LinearizeExpr1(expr->data.diadic.right, expr, FALSE, FALSE);
                TransformLogicalAndRHS(expr, &obj, &label);
            } else if (expr->type == ELOR) {
                CLabel* label;
                Object* obj;
                ENode* ind;
                ENode* one;

                obj = create_temp_object(IRO_UnqualifiedType(expr->rtype));
                ind = IRO_NewENode(EINDIRECT);
                ind->data.monadic = create_objectrefnode(obj);
                if (!IS_TYPE_VOID(expr->rtype)) {
                    ind->rtype = obj->type;
                } else {
                    CError_FATAL(696);
                }
                one = IRO_NewIntConstENode(cint64_one, expr->rtype);
                stmt = IRO_NewStatement(ST_EXPRESSION, IRO_NewENode(EASS));
                stmt->expr->data.diadic.left = ind;
                stmt->expr->data.diadic.right = one;
                stmt->expr->rtype = obj->type;
                LinkStatement(stmt);

                LinearizeExpr1(expr->data.diadic.left, expr, FALSE, FALSE);
                label = IRO_NewLabel();
                stmt = IRO_NewStatement(ST_IFGOTO, expr->data.diadic.left);
                stmt->label = label;
                LinkStatement(stmt);

                LinearizeExpr1(expr->data.diadic.right, expr, FALSE, FALSE);
                TransformLogicalOrRHS(expr, &obj, &label);
            }
            break;
        case EASS:
        case EMULASS:
        case EDIVASS:
        case EMODASS:
        case EADDASS:
        case ESUBASS:
        case ESHLASS:
        case ESHRASS:
        case EANDASS:
        case EXORASS:
        case EORASS:
        case EBCLR:
        case EBSET:
            isAssign = TRUE;
        case EMUL:
        case EMULV:
        case EDIV:
        case EMODULO:
        case EADDV:
        case ESUBV:
        case EADD:
        case ESUB:
        case ESHL:
        case ESHR:
        case ELESS:
        case EGREATER:
        case ELESSEQU:
        case EGREATEREQU:
        case EEQU:
        case ENOTEQU:
        case EAND:
        case EXOR:
        case EOR:
        case ECOMMA:
        case EMIN:
        case EMAX:
        case EPMODULO:
        case EROTL:
        case EROTR:
        case EBTST:
            if (expr->type == ECOMMA) {
                LinearizeExpr1(expr->data.diadic.left, expr, TRUE, FALSE);
                stmt = IRO_NewStatement(ST_EXPRESSION, expr->data.diadic.left);
                LinkStatement(stmt);
                LinearizeExpr1(expr->data.diadic.right, expr, FALSE, FALSE);
                AssignCommaRToTemp(expr, discardResult);
            } else if (expr->data.diadic.left->cost <= expr->data.diadic.right->cost ||
                       (isAssign && !copts.opt_useexpressioncostswhenlinearizingassignments)) {
                LinearizeExpr1(expr->data.diadic.right, expr, FALSE, FALSE);
                LinearizeExpr1(expr->data.diadic.left, expr, FALSE, FALSE);
            } else {
                LinearizeExpr1(expr->data.diadic.left, expr, FALSE, FALSE);
                LinearizeExpr1(expr->data.diadic.right, expr, FALSE, FALSE);
            }
            break;
        case EINTCONST:
        case EFLOATCONST:
        case ESTRINGCONST:
        case EOBJREF:
        case ELABEL:
        case ESCOPEBEGIN:
        case ESCOPEEND:
        case ESETCONST:
            break;
        case ECOND: {
            CLabel* label1;
            CLabel* label2;
            Object* obj = NULL;

            LinearizeExpr1(expr->data.cond.cond, expr, FALSE, FALSE);
            label1 = IRO_NewLabel();
            stmt = IRO_NewStatement(ST_IFNGOTO, expr->data.cond.cond);
            stmt->label = label1;
            LinkStatement(stmt);

            LinearizeExpr1(expr->data.cond.expr1, expr, FALSE, FALSE);
            GenerateExpr1TempAssignment(expr, &obj);
            label2 = IRO_NewLabel();
            stmt = IRO_NewStatement(ST_GOTO, NULL);
            stmt->label = label2;
            LinkStatement(stmt);

            stmt = IRO_NewStatement(ST_LABEL, NULL);
            stmt->label = label1;
            stmt->label->stmt = stmt;
            LinkStatement(stmt);

            LinearizeExpr1(expr->data.cond.expr2, expr, FALSE, FALSE);
            GenerateExpr2TempAssignment(expr, &obj);
            stmt = IRO_NewStatement(ST_LABEL, NULL);
            stmt->label = label2;
            stmt->label->stmt = stmt;
            LinkStatement(stmt);

            if (IS_TYPE_VOID(expr->rtype)) {
                expr->type = EINTCONST;
                expr->data.intval = cint64_zero;
            } else {
                expr->type = EINDIRECT;
                expr->data.monadic = create_objectrefnode(obj);
                CError_ASSERT(1328, !IS_TYPE_VOID(expr->rtype));
            }
            break;
        }
        case EFUNCCALL:
        case EFUNCCALLP: {
            SInt32 count;
            SInt32 i;
            ENodeList* list;
            ENode** arr;
            SInt16* arr2;

            data_00625ECA = FALSE;
            count = 0;
            for (list = expr->data.funccall.args; list; list = list->next) {
                count++;
            }

            if (count) {
                arr = IRO_Alloc(sizeof(ENode*) * count);
                count = 0;
                for (list = expr->data.funccall.args; list; list = list->next) {
                    arr[count] = list->node;
                    count++;
                }

                arr2 = IRO_Alloc(sizeof(SInt16) * count);
                if (copts.opt_defineargorder && IRO_Hook2) {
                    ((void (*)(ENode*, SInt16*))IRO_Hook2)(expr, arr2);
                } else {
                    for (i = 0; i < count; i++) {
                        arr2[i] = count - i - 1;
                    }
                }

                if (IRO_Hook3) {
                    for (i = 0; i < count; i++) {
                        if (((Boolean (*)(ENode*, ENode*))IRO_Hook3)(arr[arr2[i]], expr)) {
                            LinearizeExpr1(arr[arr2[i]], expr, FALSE, TRUE);
                        }
                    }
                    if (copts.opt_defineargorder && IRO_Hook2) {
                        ((void (*)(ENode*, SInt16*))IRO_Hook2)(expr, arr2);
                    }
                }

                for (i = 0; i < count; i++) {
                    LinearizeExpr1(arr[arr2[i]], expr, FALSE, FALSE);
                }
            }

            LinearizeExpr1(expr->data.funccall.funcref, expr, FALSE, FALSE);
            break;
        }
        case ENULLCHECK: {
            CLabel* label;
            Object* obj;
            ENode* ind;

            LinearizeExpr1(expr->data.nullcheck.nullcheckexpr, expr, FALSE, FALSE);
            GenerateNullcheckExprTempAssignment(expr, &obj);
            ind = IRO_NewENode(EINDIRECT);
            ind->data.monadic = create_objectrefnode(obj);
            ind->rtype = NULLCHECK_TYPE(expr);
            label = IRO_NewLabel();
            stmt = IRO_NewStatement(ST_IFNGOTO, ind);
            stmt->label = label;
            LinkStatement(stmt);

            LinearizeExpr1(expr->data.nullcheck.condexpr, expr, FALSE, FALSE);
            GenerateNullcheckCondExpr(expr, &obj);
            stmt = IRO_NewStatement(ST_LABEL, NULL);
            stmt->label = label;
            stmt->label->stmt = stmt;
            LinkStatement(stmt);

            if (IS_TYPE_VOID(expr->rtype)) {
                expr->type = EINTCONST;
                expr->data.intval = cint64_zero;
            } else {
                expr->type = EINDIRECT;
                expr->data.monadic = create_objectrefnode(obj);
                CError_ASSERT(1373, !IS_TYPE_VOID(expr->rtype));
            }
            break;
        }
        case EPRECOMP: {
            NullCheckListNode* n;

            for (n = NullCheckList; n; n = n->next) {
                if (n->precompid == expr->data.precompid) {
                    break;
                }
            }
            CError_ASSERT(779, n);
            expr->type = EINDIRECT;
            expr->data.monadic = create_objectrefnode(n->tempobj);
            CError_ASSERT(1347, !IS_TYPE_VOID(expr->rtype));
            break;
        }
        default:
            CError_FATAL(1471);
    }

    if (checkArgument && IRO_Hook3 && ((Boolean (*)(ENode*, ENode*))IRO_Hook3)(expr, parent)) {
        AssignDangerousArgumentToTemp(expr);
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#else
static void LinearizeExpr1(ENode* expr, ENode* parent, Boolean discardResult, Boolean unused) {
    switch (expr->type) {
        case EPOSTINC:
        case EPOSTDEC:
        case EPREINC:
        case EPREDEC:
        case EINDIRECT:
        case EMONMIN:
        case EBINNOT:
        case ELOGNOT:
        case EFORCELOAD:
        case ETYPCON:
        case EBITFIELD:
            LinearizeExpr1(expr->data.monadic, expr, FALSE, FALSE);
            if (expr->type == EFORCELOAD) {
                Object* obj;
                GenerateForceLoadTempAssignment(expr, &obj);
                expr->type = EINDIRECT;
                expr->data.monadic = create_objectrefnode(obj);
                CError_ASSERT(1431, !IS_TYPE_VOID(expr->rtype));
            }
            break;
        case EMUL:
        case EMULV:
        case EDIV:
        case EMODULO:
        case EADDV:
        case ESUBV:
        case EADD:
        case ESUB:
        case ESHL:
        case ESHR:
        case ELESS:
        case EGREATER:
        case ELESSEQU:
        case EGREATEREQU:
        case EEQU:
        case ENOTEQU:
        case EAND:
        case EXOR:
        case EOR:
        case ECOMMA:
        case EPMODULO:
        case EROTL:
        case EROTR:
        case EBTST:
            if (expr->type == ECOMMA) {
                LinearizeExpr1(expr->data.diadic.left, expr, TRUE, FALSE);
                MakeLeftChildAsAssignment(expr);
                LinearizeExpr1(expr->data.diadic.right, expr, FALSE, FALSE);
                AssignCommaRToTemp(expr, discardResult);
            } else if (expr->data.diadic.right->cost >= expr->data.diadic.left->cost) {
                LinearizeExpr1(expr->data.diadic.right, expr, FALSE, FALSE);
                LinearizeExpr1(expr->data.diadic.left, expr, FALSE, FALSE);
            } else {
                LinearizeExpr1(expr->data.diadic.left, expr, FALSE, FALSE);
                LinearizeExpr1(expr->data.diadic.right, expr, FALSE, FALSE);
            }
            break;
        case EASS:
        case EMULASS:
        case EDIVASS:
        case EMODASS:
        case EADDASS:
        case ESUBASS:
        case ESHLASS:
        case ESHRASS:
        case EANDASS:
        case EXORASS:
        case EORASS:
        case EBCLR:
        case EBSET:
            LinearizeExpr1(expr->data.diadic.right, expr, FALSE, FALSE);
            LinearizeExpr1(expr->data.diadic.left, expr, FALSE, FALSE);
            break;
        case ELAND:
        case ELOR:
            if (expr->type == ELAND) {
                CLabel* label;
                Object* obj;
                CreateTempAssignmentToZero(expr, &obj);
                LinearizeExpr1(expr->data.diadic.left, expr, FALSE, FALSE);
                TransformLogicalAndLHS(expr, &obj, &label);
                LinearizeExpr1(expr->data.diadic.right, expr, FALSE, FALSE);
                TransformLogicalAndRHS(expr, &obj, &label);
            } else if (expr->type == ELOR) {
                CLabel* label;
                Object* obj;
                CreateTempAssignmentToOne(expr, &obj);
                LinearizeExpr1(expr->data.diadic.left, expr, FALSE, FALSE);
                TransformLogicalOrLHS(expr, &obj, &label);
                LinearizeExpr1(expr->data.diadic.right, expr, FALSE, FALSE);
                TransformLogicalOrRHS(expr, &obj, &label);
            }
            break;
        case ECOND: {
            CLabel *label1, *label2;
            Object* obj = NULL;
            LinearizeExpr1(expr->data.cond.cond, expr, FALSE, FALSE);
            GenerateCondExpression(expr, &label1);
            LinearizeExpr1(expr->data.cond.expr1, expr, FALSE, FALSE);
            GenerateExpr1TempAssignment(expr, &obj);
            GenerateGoToStmt(expr, &label2);
            GenerateLabel(&label1);
            LinearizeExpr1(expr->data.cond.expr2, expr, FALSE, FALSE);
            GenerateExpr2TempAssignment(expr, &obj);
            GenerateLabel(&label2);
            if (!IS_TYPE_VOID(expr->rtype)) {
                expr->type = EINDIRECT;
                expr->data.monadic = create_objectrefnode(obj);
                CError_ASSERT(1599, !IS_TYPE_VOID(expr->rtype));
            } else {
                expr->type = EINTCONST;
                expr->data.intval = cint64_zero;
            }
            break;
        }
        case EPRECOMP: {
            Object* temp = GetTempFromtheList(expr);
            expr->type = EINDIRECT;
            expr->data.monadic = create_objectrefnode(temp);
            CError_ASSERT(1617, !IS_TYPE_VOID(expr->rtype));
            break;
        }
        case ENULLCHECK: {
            CLabel* label;
            Object* obj;
            LinearizeExpr1(expr->data.nullcheck.nullcheckexpr, expr, FALSE, FALSE);
            GenerateNullcheckExprTempAssignment(expr, &obj);
            GenerateIfNotTemp(expr, &obj, &label);
            LinearizeExpr1(expr->data.nullcheck.condexpr, expr, FALSE, FALSE);
            GenerateNullcheckCondExpr(expr, &obj);
            GenerateLabel(&label);
            if (!IS_TYPE_VOID(expr->rtype)) {
                expr->type = EINDIRECT;
                expr->data.monadic = create_objectrefnode(obj);
                CError_ASSERT(1642, !IS_TYPE_VOID(expr->rtype));
            } else {
                expr->type = EINTCONST;
                expr->data.intval = cint64_zero;
            }
            break;
        }
        case EFUNCCALL:
        case EFUNCCALLP: {
            SInt32 count;
            SInt32 i;
            ENodeList* list;
            ENode** arr;
            SInt16* arr2;

            IRO_IsLeafFunction = FALSE;

            list = expr->data.funccall.args;
            count = 0;
            while (list) {
                count++;
                list = list->next;
            }

            if (count) {
                arr = oalloc(sizeof(ENode*) * count);
                list = expr->data.funccall.args;
                count = 0;
                while (list) {
                    arr[count] = list->node;
                    list = list->next;
                    count++;
                }

                arr2 = oalloc(sizeof(SInt16) * count);
                for (i = 0; i < count; i++) {
                    arr2[i] = count - i - 1;
                }

                for (i = 0; i < count; i++) {
                    LinearizeExpr1(arr[arr2[i]], expr, FALSE, FALSE);
                }
            }

            LinearizeExpr1(expr->data.funccall.funcref, expr, FALSE, FALSE);
            break;
        }

        case EINTCONST:
        case EFLOATCONST:
        case ESTRINGCONST:
        case EOBJREF:
        case ESETCONST:
        case EVECTOR128CONST:
            break;

        default:
            CError_FATAL(1726);
    }
}
#endif

#if MWCCPSP
// 3.0.1: operands are ordered by cost (assignments too, unless
// copts.opt_useexpressioncostswhenlinearizingassignments is off, when the
// right side goes first), the back end may choose the argument order
// (IRO_Hook2), calls to sin/cos and divisions are noted for later passes,
// scope markers survive only when nothing optimizes, and every node is shown
// to IRO_Hook15 while data_00625ECA is set.
extern void RearrangeMultipleSubscripts(ENode* enode);

// 0x5647A0: whether no optimization is enabled
Boolean IRO_AllOptimizationsOff(void) {
    return !copts.multiplepasses &&
        !copts.opt_rotateloops &&
        !copts.opt_optimizenonregaccess &&
        !copts.opt_removeemptyloops &&
        !copts.loopinvariants &&
        !copts.strengthreduction &&
        !copts.lifetimes &&
        !copts.unrolling &&
        !copts.opt_reverseloops &&
        !copts.vectorizeloops &&
        !copts.deadstore &&
        !copts.commonsubs &&
        !copts.opt_collapselogicalselectionintobittest &&
        !copts.propagation &&
        !copts.deadcode &&
        !copts.opt_scalarize &&
        !copts.opt_treetransformation &&
        !copts.opt_arithtransformation &&
        !copts.opt_rebuildconditionals &&
        !copts.opt_rebuildlogicals &&
        !copts.opt_foldconstants &&
        !copts.opt_optimizecontrolflow &&
        !copts.opt_markcounterloops &&
        !copts.opt_marknonregtemps;
}
extern Boolean IRO_IsSinCall(IROLinear* linear, IROLinear** arg);
extern Boolean IRO_IsCosCall(IROLinear* linear, IROLinear** arg);
extern Boolean IRO_HasModulo;
extern Boolean data_00625ECA;

#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
static IROLinear* LinearizeExpr(ENode* enode) {
    IROLinear* linear;
    Boolean isAssign;

    linear = NULL;
    isAssign = FALSE;
    switch (enode->type) {
        case EPOSTINC:
        case EPOSTDEC:
        case EPREINC:
        case EPREDEC:
        case EINDIRECT:
        case EMONMIN:
        case EBINNOT:
        case ELOGNOT:
        case EFORCELOAD:
        case ETYPCON:
        case EBITFIELD:
            if (copts.opt_rearrangemultiplesubscripts &&
                enode->type == EINDIRECT &&
                enode->data.monadic->type == EADD) {
                RearrangeMultipleSubscripts(enode->data.monadic);
            }
            linear = IRO_NewLinear(IROLinearOp1Arg);
            linear->u.monadic = LinearizeExpr(enode->data.monadic);
            linear->nodetype = enode->type;
            linear->rtype = IRO_UnqualifiedType(enode->rtype);
            linear->nodeflags = enode->flags;
            if (IRO_IsAssignOp[linear->nodetype]) {
                MarkAssigned(linear->u.monadic, TRUE);
            }
            if (linear->nodetype == EINDIRECT) {
                MarkSubs1(linear->u.monadic);
                linear->u.monadic->flags |= IROLF_Immind | IROLF_Ind;
                if (IS_LINEAR_DIADIC(linear->u.monadic, EADD)) {
                    MarkSubscript(linear->u.monadic);
                }
            }
            break;
        case ELESSEQU:
            if (copts.opt_intelcompareoplinearizationhack &&
                IS_TYPE_FLOAT(enode->data.diadic.left->rtype)) {
                enode->data.diadic.left->cost = 2;
                enode->data.diadic.right->cost = 1;
            }
            goto diadic;
        case EGREATEREQU:
            if (copts.opt_intelcompareoplinearizationhack &&
                IS_TYPE_FLOAT(enode->data.diadic.left->rtype)) {
                enode->data.diadic.left->cost = 1;
                enode->data.diadic.right->cost = 2;
            }
            goto diadic;
        case EASS:
        case EMULASS:
        case EDIVASS:
        case EMODASS:
        case EADDASS:
        case ESUBASS:
        case ESHLASS:
        case ESHRASS:
        case EANDASS:
        case EXORASS:
        case EORASS:
        case EBCLR:
        case EBSET:
            isAssign = TRUE;
        case EMUL:
        case EMULV:
        case EDIV:
        case EMODULO:
        case EADDV:
        case ESUBV:
        case EADD:
        case ESUB:
        case ESHL:
        case ESHR:
        case ELESS:
        case EGREATER:
        case EEQU:
        case ENOTEQU:
        case EAND:
        case EXOR:
        case EOR:
        case ECOMMA:
        case EMIN:
        case EMAX:
        case EPMODULO:
        case EROTL:
        case EROTR:
        case EBTST:
diadic:
            linear = IRO_NewLinear(IROLinearOp2Arg);
            linear->nodeflags = enode->flags;
            if ((!ENODE_IS(enode, ECOMMA) &&
                 enode->data.diadic.left->cost <= enode->data.diadic.right->cost) ||
                (isAssign && !copts.opt_useexpressioncostswhenlinearizingassignments)) {
                linear->u.diadic.right = LinearizeExpr(enode->data.diadic.right);
                linear->u.diadic.left = LinearizeExpr(enode->data.diadic.left);
                linear->flags |= IROLF_RightFirst;
            } else {
                linear->u.diadic.left = LinearizeExpr(enode->data.diadic.left);
                linear->u.diadic.right = LinearizeExpr(enode->data.diadic.right);
            }
            linear->nodetype = enode->type;
            linear->rtype = IRO_UnqualifiedType(enode->rtype);
            if (IRO_IsAssignOp[linear->nodetype]) {
                MarkAssigned(linear->u.diadic.left, IRO_IsModifyOp[linear->nodetype]);
            }
            if (linear->nodetype == EDIV) {
                IRO_HasDivide = TRUE;
            } else if (linear->nodetype == EMODULO) {
                IRO_HasModulo = TRUE;
            }
            break;
        case EINTCONST:
        case EFLOATCONST:
        case ESTRINGCONST:
        case EOBJREF:
        case ELABEL:
            linear = IRO_NewLinear(IROLinearOperand);
            linear->nodetype = enode->type;
            linear->nodeflags = enode->flags;
            linear->u.node = enode;
            linear->rtype = IRO_UnqualifiedType(enode->rtype);
            break;
        case EFUNCCALL:
        case EFUNCCALLP: {
            SInt32 count;
            SInt32 i;
            ENodeList* list;
            ENode** arr;
            SInt16* arr2;

            linear = IRO_NewLinear(IROLinearFunccall);
            linear->nodeflags = enode->flags;
            linear->u.funccall.ispascal = enode->type == EFUNCCALLP;

            count = 0;
            for (list = enode->data.funccall.args; list; list = list->next) {
                count++;
            }

            arr = NULL;
            if (count) {
                arr = IRO_Alloc(sizeof(ENode*) * count);
                count = 0;
                for (list = enode->data.funccall.args; list; list = list->next) {
                    arr[count] = list->node;
                    count++;
                }

                arr2 = IRO_Alloc(sizeof(SInt16) * count);
                if (copts.opt_defineargorder && IRO_Hook2) {
                    ((void (*)(ENode*, SInt16*))IRO_Hook2)(enode, arr2);
                } else {
                    for (i = 0; i < count; i++) {
                        arr2[i] = count - i - 1;
                    }
                }

                for (i = 0; i < count; i++) {
                    arr[arr2[i]] = (ENode*)LinearizeExpr(arr[arr2[i]]);
                    MarkArgs((IROLinear*)arr[arr2[i]]);
                }
            }

            linear->u.funccall.argCount = count;
            linear->u.funccall.args = (IROLinear**)arr;
            linear->u.funccall.callee = LinearizeExpr(enode->data.funccall.funcref);
            linear->u.funccall.functype = enode->data.funccall.functype;
            linear->rtype = IRO_UnqualifiedType(enode->rtype);
            if (IRO_Hook19 && IRO_IsSinCall(linear, NULL)) {
                IRO_HasSinCall = TRUE;
            } else if (IRO_Hook19 && IRO_IsCosCall(linear, NULL)) {
                IRO_HasCosCall = TRUE;
            }
            break;
        }
        case ESCOPEBEGIN:
        case ESCOPEEND:
            if (IRO_AllOptimizationsOff()) {
                linear = IRO_NewLinear(IROLinearScope);
                linear->nodetype = enode->type;
                linear->nodeflags = enode->flags;
                linear->u.node = enode;
                linear->rtype = IRO_UnqualifiedType(enode->rtype);
            }
            break;
        default:
            CError_FATAL(1792);
    }

    if (linear) {
        LinkLinear(linear);
    }
    if (linear &&
        data_00625ECA &&
        IRO_Hook15 &&
        ((Boolean (*)(IROLinear*, int))IRO_Hook15)(linear, 1)) {
        data_00625ECA = FALSE;
    }
    return linear;
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#else
static IROLinear* LinearizeExpr(ENode* enode) {
    IROLinear* linear = NULL;

    switch (enode->type) {
        case EPOSTINC:
        case EPOSTDEC:
        case EPREINC:
        case EPREDEC:
        case EINDIRECT:
        case EMONMIN:
        case EBINNOT:
        case ELOGNOT:
        case EFORCELOAD:
        case ETYPCON:
        case EBITFIELD:
            linear = IRO_NewLinear(IROLinearOp1Arg);
            linear->u.monadic = LinearizeExpr(enode->data.monadic);
            linear->nodetype = enode->type;
            linear->rtype = enode->rtype;
            linear->nodeflags = enode->flags;
            if (IRO_IsAssignOp[linear->nodetype]) {
                MarkAssigned(linear->u.monadic, TRUE);
            }
            if (linear->nodetype == EINDIRECT) {
                MarkSubs1(linear->u.monadic);
                linear->u.monadic->flags |= IROLF_Immind | IROLF_Ind;
                if (IS_LINEAR_DIADIC(linear->u.monadic, EADD)) {
                    MarkSubscript(linear->u.monadic);
                }
            }
            break;
        case EMUL:
        case EMULV:
        case EDIV:
        case EMODULO:
        case EADDV:
        case ESUBV:
        case EADD:
        case ESUB:
        case ESHL:
        case ESHR:
        case ELESS:
        case EGREATER:
        case ELESSEQU:
        case EGREATEREQU:
        case EEQU:
        case ENOTEQU:
        case EAND:
        case EXOR:
        case EOR:
        case ECOMMA:
        case EPMODULO:
        case EROTL:
        case EROTR:
        case EBTST:
            linear = IRO_NewLinear(IROLinearOp2Arg);
            linear->nodeflags = enode->flags;
            if (!ENODE_IS(enode, ECOMMA) &&
                enode->data.diadic.right->cost >= enode->data.diadic.left->cost) {
                linear->u.diadic.right = LinearizeExpr(enode->data.diadic.right);
                linear->u.diadic.left = LinearizeExpr(enode->data.diadic.left);
                linear->flags |= IROLF_RightFirst;
            } else {
                linear->u.diadic.left = LinearizeExpr(enode->data.diadic.left);
                linear->u.diadic.right = LinearizeExpr(enode->data.diadic.right);
            }
            linear->nodetype = enode->type;
            linear->rtype = enode->rtype;
            if (IRO_IsAssignOp[linear->nodetype]) {
                MarkAssigned(linear->u.diadic.left, IRO_IsModifyOp[linear->nodetype]);
            }
            break;
        case EASS:
        case EMULASS:
        case EDIVASS:
        case EMODASS:
        case EADDASS:
        case ESUBASS:
        case ESHLASS:
        case ESHRASS:
        case EANDASS:
        case EXORASS:
        case EORASS:
        case EBCLR:
        case EBSET:
            linear = IRO_NewLinear(IROLinearOp2Arg);
            linear->nodeflags = enode->flags;
            linear->u.diadic.right = LinearizeExpr(enode->data.diadic.right);
            linear->u.diadic.left = LinearizeExpr(enode->data.diadic.left);
            linear->flags |= IROLF_RightFirst;
            linear->nodetype = enode->type;
            linear->rtype = enode->rtype;
            MarkAssigned(linear->u.diadic.left, IRO_IsModifyOp[linear->nodetype]);
            break;
        case EINTCONST:
        case EFLOATCONST:
        case ESTRINGCONST:
        case EOBJREF:
        case EVECTOR128CONST:
            linear = IRO_NewLinear(IROLinearOperand);
            linear->nodeflags = enode->flags;
            linear->u.node = enode;
            linear->rtype = enode->rtype;
            break;
        case EFUNCCALL:
        case EFUNCCALLP: {
            SInt32 count;
            SInt32 i;
            ENodeList* list;
            ENode** arr;
            SInt16* arr2;

            linear = IRO_NewLinear(IROLinearFunccall);
            linear->nodeflags = enode->flags;
            linear->u.funccall.ispascal = enode->type == EFUNCCALLP;

            list = enode->data.funccall.args;
            count = 0;
            while (list) {
                count++;
                list = list->next;
            }

            arr = NULL;
            if (count) {
                arr = oalloc(sizeof(ENode*) * count);
                list = enode->data.funccall.args;
                count = 0;
                while (list) {
                    arr[count] = list->node;
                    list = list->next;
                    count++;
                }

                arr2 = oalloc(sizeof(SInt16) * count);
                for (i = 0; i < count; i++) {
                    arr2[i] = count - i - 1;
                }

                for (i = 0; i < count; i++) {
                    arr[arr2[i]] = (ENode*)LinearizeExpr(arr[arr2[i]]);
                    MarkArgs((IROLinear*)arr[arr2[i]]);
                }
            }

            linear->u.funccall.argCount = count;
            linear->u.funccall.args = (IROLinear**)arr;
            linear->u.funccall.callee = LinearizeExpr(enode->data.funccall.funcref);
            linear->u.funccall.functype = enode->data.funccall.functype;
            linear->rtype = enode->rtype;
            break;
        }
        default:
            CError_FATAL(1946);
    }

    if (linear) {
        LinkLinear(linear);
    }
    return linear;
}
#endif

#if MWCCPSP
// 3.0.1 returns the (possibly new) first statement: code the pre-pass
// inserts in front of the first statement becomes the head. It also
// handles ST_GOTOEXPR and turns on IRO_Hook15's node check (data_00625ECA).
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
Statement* IRO_PreLinearize301(Statement* first) {
    Statement* stmt;

    IRO_FirstLinear = IRO_LastLinear = NULL;
    IRO_NumLinear = 0;
    CurrStmt = PrevStmt = FirstStmt = NULL;
    data_00625ECA = TRUE;

    for (stmt = first; stmt; stmt = stmt->next) {
        CurStat = stmt;
        CurrStmt = stmt;
        NullCheckList = NULL;

        switch (stmt->type) {
            case ST_GOTOEXPR:
                LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                break;
            case ST_NOP:
            case ST_LABEL:
            case ST_GOTO:
                break;
            case ST_OVF:
                CError_FATAL(1869);
            case ST_EXPRESSION:
                LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                break;
            case ST_SWITCH:
                LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                break;
            case ST_IFGOTO:
                LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                break;
            case ST_IFNGOTO:
                LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                break;
            case ST_RETURN:
                if (stmt->expr) {
                    LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                }
                break;
            case ST_BEGINCATCH:
                LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                break;
            case ST_ENDCATCH:
                LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                break;
            case ST_ENDCATCHDTOR:
                LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                break;
            case ST_EXIT:
            case ST_ENTRY:
            case ST_ASM:
                break;
            default:
                CError_FATAL(1918);
        }

        NullCheckList = NULL;
        if (!FirstStmt) {
            FirstStmt = stmt;
        }
        PrevStmt = stmt;
    }

    IRO_CheckForUserBreak();
    return FirstStmt;
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#else
void IRO_PreLinearize(Statement* stmt) {
    IRO_FirstLinear = IRO_LastLinear = NULL;
    IRO_NumLinear = 0;
    CurrStmt = PrevStmt = NULL;

    while (stmt) {
        CurStat = stmt;
        CurrStmt = stmt;
        NullCheckList = NULL;

        switch (stmt->type) {
            case ST_NOP:
            case ST_LABEL:
            case ST_GOTO:
                break;
            case ST_OVF:
                CError_FATAL(1992);
            case ST_EXPRESSION:
                LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                break;
            case ST_SWITCH:
                LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                break;
            case ST_IFGOTO:
                LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                break;
            case ST_IFNGOTO:
                LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                break;
            case ST_RETURN:
                if (stmt->expr) {
                    LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                }
                break;
            case ST_BEGINCATCH:
                LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                break;
            case ST_ENDCATCH:
                LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                break;
            case ST_ENDCATCHDTOR:
                LinearizeExpr1(stmt->expr, NULL, FALSE, FALSE);
                break;

            case ST_EXIT:
            case ST_ENTRY:
            case ST_ASM:
                break;

            default:
                CError_FATAL(2041);
        }

        PrevStmt = stmt;
        stmt = stmt->next;
    }

    IRO_CheckForUserBreak();
}
#endif

static void MarkAllSubExprs(IROLinear* linear) {
    IROLinear* scan;

    for (scan = linear; scan; scan = scan->next) {
        scan->flags &= ~IROLF_Reffed;
    }

    for (scan = linear; scan; scan = scan->next) {
        MarkSubExpr(scan);
    }
}

#if MWCCPSP
// 3.0.1: with copts.microsoft_EH, a catch block's object is copied from a
// temporary right at the catch label (CollectCatchObjects and
// CopyCatchObject); a goto through an expression is linearized; asm
// statements ask the back end for their effects.
typedef struct CatchObjectCopy {
    Object* object;
    Type* type;
    CLabel* label;
    Object* temp;
    struct CatchObjectCopy* next;
} CatchObjectCopy;

static CatchObjectCopy* CatchObjects;

static void CollectCatchObjects(Statement* first) {
    Statement* stmt;
    ExceptionAction* act;
    CatchObjectCopy* copy;

    CatchObjects = NULL;
    for (stmt = first; stmt; stmt = stmt->next) {
        if (stmt->type == ST_BEGINCATCH) {
            for (act = stmt->dobjstack; act; act = act->prev) {
                if (act->type == EAT_CATCHBLOCK &&
                    act->data.catch_block.catch_type &&
                    act->data.catch_block.catch_object) {
                    copy = IRO_malloc(sizeof(CatchObjectCopy));
                    copy->object = act->data.catch_block.catch_object;
                    copy->type = act->data.catch_block.catch_type;
                    copy->label = act->data.catch_block.catch_label;
                    copy->temp = NULL;
                    copy->next = CatchObjects;
                    CatchObjects = copy;
                }
            }
        }
    }
}

static CatchObjectCopy* FindCatchObject(CLabel* label) {
    CatchObjectCopy* copy;

    for (copy = CatchObjects; copy; copy = copy->next) {
        if (copy->label == label) {
            return copy;
        }
    }
    return NULL;
}

// 0x563960: the assignment CopyCatchObject made (IRO_Delinearize drops it)
Boolean IRO_IsCatchCopyAssignment(IROLinear* linear) {
    IROLinear* dst;
    IROLinear* src;
    Object* dstObj;
    CatchObjectCopy* copy;
    Boolean found;
    Boolean result;

    result = FALSE;
    if (IRO_IsAssignment(linear) && linear->nodetype == EASS) {
        dst = linear->u.diadic.left;
        if (dst) {
            src = (linear->type == IROLinearOp1Arg) ? NULL : linear->u.diadic.right;
            if (src && (dstObj = IRO_IsVariable(dst)) && IRO_IsVariable(src)) {
                copy = NULL;
                found = FALSE;
                if (src->u.monadic->type == IROLinearOperand &&
                    src->u.monadic->u.node &&
                    src->u.monadic->u.node->type == EOBJREF) {
                    for (copy = CatchObjects; copy; copy = copy->next) {
                        if (copy->temp == src->u.monadic->u.node->data.objref) {
                            break;
                        }
                    }
                    if (copy) {
                        found = TRUE;
                    }
                }
                if (found && dstObj == copy->object && IRO_TypesEqual(dst->rtype, copy->type)) {
                    result = TRUE;
                }
            }
        }
    }
    return result;
}

static IROLinear* CopyCatchObject(CatchObjectCopy* copy) {
    IROLinear* ass;
    IROLinear* src;
    IROLinear* dst;
    IROLinear* ref;
    Object* temp;

    temp = create_temp_object(IRO_UnqualifiedType(copy->type));
    copy->temp = temp;
    ass = IRO_NewLinear(IROLinearOp2Arg);

    src = IRO_NewLinear(IROLinearOp1Arg);
    ref = IRO_NewLinear(IROLinearOperand);
    ref->nodetype = EOBJREF;
    ref->u.node = create_objectrefnode(temp);
    ref->rtype = IRO_UnqualifiedType(ref->u.node->rtype);
    TPTR_QUAL(ref->rtype) |= Q_VOLATILE;
    LinkLinear(ref);
    src->u.monadic = ref;
    src->nodetype = EINDIRECT;
    src->rtype = IRO_UnqualifiedType(copy->type);
    MarkSubs1(src->u.monadic);
    src->u.monadic->flags |= IROLF_Immind | IROLF_Ind;
    LinkLinear(src);

    dst = IRO_NewLinear(IROLinearOp1Arg);
    ref = IRO_NewLinear(IROLinearOperand);
    ref->nodetype = EOBJREF;
    ref->u.node = create_objectrefnode(copy->object);
    ref->rtype = IRO_UnqualifiedType(ref->u.node->rtype);
    LinkLinear(ref);
    dst->u.monadic = ref;
    dst->nodetype = EINDIRECT;
    dst->rtype = IRO_UnqualifiedType(copy->type);
    MarkSubs1(dst->u.monadic);
    dst->u.monadic->flags |= IROLF_Immind | IROLF_Ind;
    LinkLinear(dst);

    ass->u.diadic.right = src;
    ass->u.diadic.left = dst;
    ass->flags |= IROLF_RightFirst;
    ass->nodetype = EASS;
    ass->rtype = IRO_UnqualifiedType(copy->type);
    MarkAssigned(ass->u.diadic.left, FALSE);
    return ass;
}

#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
void IRO_Linearize(Statement* stmt) {
    IROLinear* linear;
    CatchObjectCopy* copy;

    IRO_FirstLinear = IRO_LastLinear = NULL;
    IRO_NumLinear = 0;
    DisableDueToAsm = FALSE;
    IRO_HasSinCall = IRO_HasCosCall = IRO_HasDivide = FALSE;

    if (copts.microsoft_EH) {
        CollectCatchObjects(stmt);
    }

    while (stmt) {
        CurStat = stmt;
        linear = NULL;

        switch (stmt->type) {
            case ST_NOP:
                linear = IRO_NewLinear(IROLinearNop);
                break;
            case ST_LABEL:
                linear = IRO_NewLinear(IROLinearLabel);
                linear->u.label.label = stmt->label;
                linear->flags |= IROLF_StartsNode;
                if (copts.microsoft_EH) {
                    copy = FindCatchObject(stmt->label);
                    if (copy) {
                        LinkLinear(linear);
                        linear = CopyCatchObject(copy);
                    }
                }
                break;
            case ST_GOTO:
                linear = IRO_NewLinear(IROLinearGoto);
                linear->u.label.label = stmt->label;
                break;
            case ST_EXPRESSION:
                LinearizeExpr(stmt->expr);
                break;
            case ST_SWITCH:
                linear = IRO_NewLinear(IROLinearSwitch);
                linear->u.swtch.cond = LinearizeExpr(stmt->expr);
                linear->u.swtch.info = (SwitchInfo*)stmt->label;
                break;
            case ST_IFGOTO:
                linear = IRO_NewLinear(IROLinearIf);
                linear->u.label.cond = LinearizeExpr(stmt->expr);
                linear->u.label.label = stmt->label;
                break;
            case ST_IFNGOTO:
                linear = IRO_NewLinear(IROLinearIfNot);
                linear->u.label.cond = LinearizeExpr(stmt->expr);
                linear->u.label.label = stmt->label;
                break;
            case ST_RETURN:
                linear = IRO_NewLinear(IROLinearReturn);
                if (stmt->expr) {
                    linear->u.monadic = LinearizeExpr(stmt->expr);
                } else {
                    linear->u.monadic = NULL;
                }
                break;
            case ST_OVF:
                CError_FATAL(2277);
                break;
            case ST_EXIT:
                linear = IRO_NewLinear(IROLinearExit);
                linear->u.label.label = stmt->label;
                break;
            case ST_ENTRY:
                linear = IRO_NewLinear(IROLinearEntry);
                linear->u.label.label = stmt->label;
                linear->flags |= IROLF_StartsNode;
                break;
            case ST_BEGINCATCH:
                linear = IRO_NewLinear(IROLinearBeginCatch);
                linear->u.ctch.linear = LinearizeExpr(stmt->expr);
                break;
            case ST_ENDCATCH:
                linear = IRO_NewLinear(IROLinearEndCatch);
                linear->u.monadic = LinearizeExpr(stmt->expr);
                break;
            case ST_ENDCATCHDTOR:
                linear = IRO_NewLinear(IROLinearEndCatchDtor);
                linear->u.monadic = LinearizeExpr(stmt->expr);
                break;
            case ST_GOTOEXPR:
                linear = IRO_NewLinear(IROLinearGotoExpr);
                linear->u.monadic = LinearizeExpr(stmt->expr);
                break;
            case ST_ASM:
                linear = IRO_NewLinear(IROLinearAsm);
                linear->u.asm_stmt = stmt;
                if (copts.optimizewithasm && IRO_GetAsmEffectsHook) {
                    IROAsmEffects effects = IRO_EmptyAsmEffects;
                    IRO_GetAsmEffectsHook(stmt, &effects);
                    if (effects.optimizationBarrier || effects.unmodeledControlFlow) {
                        DisableDueToAsm = TRUE;
                    }
                } else {
                    DisableDueToAsm = TRUE;
                }
                break;
            default:
                CError_FATAL(2327);
        }

        if (linear) {
            LinkLinear(linear);
        }
        stmt = stmt->next;
    }

    linear = IRO_NewLinear(IROLinearEnd);
    linear->flags |= IROLF_StartsNode;
    LinkLinear(linear);

    MarkAllSubExprs(IRO_FirstLinear);
    IRO_CheckForUserBreak();
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#else
void IRO_Linearize(Statement* stmt) {
    IROLinear* linear;

    IRO_FirstLinear = IRO_LastLinear = NULL;
    IRO_NumLinear = 0;

    while (stmt) {
        CurStat = stmt;
        linear = NULL;

        switch (stmt->type) {
            case ST_NOP:
                linear = IRO_NewLinear(IROLinearNop);
                break;
            case ST_LABEL:
                linear = IRO_NewLinear(IROLinearLabel);
                linear->u.label.label = stmt->label;
                linear->flags |= IROLF_StartsNode;
                break;
            case ST_GOTO:
                linear = IRO_NewLinear(IROLinearGoto);
                linear->u.label.label = stmt->label;
                break;
            case ST_EXPRESSION:
                LinearizeExpr(stmt->expr);
                break;
            case ST_SWITCH:
                linear = IRO_NewLinear(IROLinearSwitch);
                linear->u.swtch.cond = LinearizeExpr(stmt->expr);
                linear->u.swtch.info = (SwitchInfo*)stmt->label;
                break;
            case ST_IFGOTO:
                linear = IRO_NewLinear(IROLinearIf);
                linear->u.label.cond = LinearizeExpr(stmt->expr);
                linear->u.label.label = stmt->label;
                break;
            case ST_IFNGOTO:
                linear = IRO_NewLinear(IROLinearIfNot);
                linear->u.label.cond = LinearizeExpr(stmt->expr);
                linear->u.label.label = stmt->label;
                break;
            case ST_RETURN:
                IRO_FunctionHasReturn = TRUE;
                linear = IRO_NewLinear(IROLinearReturn);
                if (stmt->expr) {
                    linear->u.monadic = LinearizeExpr(stmt->expr);
                } else {
                    linear->u.monadic = NULL;
                }
                break;
            case ST_OVF:
                CError_FATAL(2146);
                break;
            case ST_EXIT:
                linear = IRO_NewLinear(IROLinearExit);
                linear->u.label.label = stmt->label;
                break;
            case ST_ENTRY:
                linear = IRO_NewLinear(IROLinearEntry);
                linear->u.label.label = stmt->label;
                linear->flags |= IROLF_StartsNode;
                break;
            case ST_BEGINCATCH:
                linear = IRO_NewLinear(IROLinearBeginCatch);
                linear->u.ctch.linear = LinearizeExpr(stmt->expr);
                linear->u.ctch.unused1 = 0;
                linear->u.ctch.unused2 = 0;
                break;
            case ST_ENDCATCH:
                linear = IRO_NewLinear(IROLinearEndCatch);
                linear->u.monadic = LinearizeExpr(stmt->expr);
                break;
            case ST_ENDCATCHDTOR:
                linear = IRO_NewLinear(IROLinearEndCatchDtor);
                linear->u.monadic = LinearizeExpr(stmt->expr);
                break;
            case ST_ASM:
                linear = IRO_NewLinear(IROLinearAsm);
                linear->u.asm_stmt = stmt;
                if (copts.optimizewithasm) {
                    IAEffects effects;
                    CodeGen_GetAsmEffects(stmt, &effects);
                    if (effects.optimizationBarrier || effects.unmodeledControlFlow) {
                        DisableDueToAsm = TRUE;
                    }
                } else {
                    DisableDueToAsm = TRUE;
                }
                break;
            default:
                CError_FATAL(2197);
        }

        if (linear) {
            LinkLinear(linear);
        }
        stmt = stmt->next;
    }

    linear = IRO_NewLinear(IROLinearEnd);
    linear->flags |= IROLF_StartsNode;
    LinkLinear(linear);

    MarkAllSubExprs(IRO_FirstLinear);
    IRO_CheckForUserBreak();
}
#endif

static Statement* NewStatement(IROLinear* linear, StatementType sttype) {
    Statement* stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = sttype;
    stmt->value = 1;
    if (linear->stmt) {
        stmt->dobjstack = linear->stmt->dobjstack;
        stmt->sourceoffset = linear->stmt->sourceoffset;
        stmt->sourcefilepath = linear->stmt->sourcefilepath;
        stmt->value = linear->stmt->value;
        stmt->flags = linear->stmt->flags;
    } else {
        stmt->sourceoffset = -1;
        stmt->sourcefilepath = NULL;
    }
    return stmt;
}

#if !MWCCPSP // 3.0.1: in IroUtil.c (0x54FC00), not inlined here
ENode* IRO_NewENode(ENodeType nodetype) {
    ENode* enode = lalloc(sizeof(ENode));
    memset(enode, 0, sizeof(ENode));
    enode->type = nodetype;
    return enode;
}
#endif

#if MWCCPSP
// 3.0.1: a reference to a catch object's temporary becomes the catch
// object again; an indirection whose object pointer points to another kind
// of value (float vs. integer, or a struct) is retyped first
// (IRO_MakeTypeConversion); operands evaluated right to left under the
// argument-order hook are marked (ENODE_FLAG 0x20).
extern IROLinear* IRO_MakeTypeConversion(IROLinear* linear, Type* type, IROList* list);

#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
static ENode* BuildExpr(IROLinear* linear) {
    ENode* enode;
    CatchObjectCopy* copy;

    switch (linear->type) {
        case IROLinearOperand:
            if (copts.microsoft_EH) {
                copy = NULL;
                if (linear->type == IROLinearOperand &&
                    linear->u.node &&
                    linear->u.node->type == EOBJREF) {
                    for (copy = CatchObjects; copy; copy = copy->next) {
                        if (copy->temp == linear->u.node->data.objref) {
                            break;
                        }
                    }
                }
                if (copy) {
                    enode = create_objectrefnode(copy->object);
                    break;
                }
            }
            enode = IRO_NewENode(linear->u.node->type);
            *enode = *linear->u.node;
            break;
        case IROLinearOp1Arg:
            if (linear->nodetype == EINDIRECT && linear->u.monadic->nodetype == EOBJREF) {
                Type* ptype = linear->u.monadic->u.node->rtype;
                if (ptype && ptype->type == TYPEPOINTER && TPTR_TARGET(ptype) && linear->rtype) {
                    TypeType target = TPTR_TARGET(ptype)->type;
                    TypeType mine = linear->rtype->type;
                    if (((mine == TYPEFLOAT) != (target == TYPEFLOAT) &&
                         mine != TYPESTRUCT &&
                         target != TYPESTRUCT &&
                         mine != TYPECLASS &&
                         target != TYPECLASS) ||
                        ((mine == TYPESTRUCT || mine == TYPECLASS) &&
                         target != TYPESTRUCT &&
                         target != TYPECLASS)) {
                        linear->u.monadic = IRO_MakeTypeConversion(
                            linear->u.monadic, CDecl_NewPointerType(linear->rtype), NULL
                        );
                    }
                }
            }
            enode = IRO_NewENode(linear->nodetype);
            enode->flags = linear->nodeflags;
            enode->data.monadic = BuildExpr(linear->u.monadic);
            enode->rtype = IRO_UnqualifiedType(linear->rtype);
            enode->cost = enode->data.monadic->cost;
            if (!enode->cost) {
                enode->cost = 1;
            }
            break;
        case IROLinearOp2Arg:
            enode = IRO_NewENode(linear->nodetype);
            enode->flags = linear->nodeflags;
            enode->data.diadic.left = BuildExpr(linear->u.diadic.left);
            enode->data.diadic.right = BuildExpr(linear->u.diadic.right);
            if (copts.opt_defineargorder &&
                IRO_Hook2 &&
                linear->u.diadic.right->index < linear->u.diadic.left->index) {
                enode->flags |= 0x20;
            }

            enode->cost = enode->data.diadic.left->cost;
            if (enode->data.diadic.right->cost > enode->cost) {
                enode->cost = enode->data.diadic.right->cost;
            } else if (enode->data.diadic.right->cost == enode->cost) {
                enode->cost += 1;
            }

            if (ENODE_IS4(enode, ESHL, ESHR, EDIV, EMODULO)) {
                enode->cost += 2;
            }
            if (enode->cost > 200) {
                enode->cost = 200;
            }

            enode->rtype = IRO_UnqualifiedType(linear->rtype);
            break;
        case IROLinearOp3Arg:
            enode = IRO_NewENode(linear->nodetype);
            enode->flags = linear->nodeflags;
            enode->data.cond.cond = BuildExpr(linear->u.args3.a);
            enode->data.cond.expr1 = BuildExpr(linear->u.args3.b);
            enode->data.cond.expr2 = BuildExpr(linear->u.args3.c);
            enode->rtype = IRO_UnqualifiedType(linear->rtype);

            enode->cost = enode->data.cond.cond->cost;
            if (enode->data.cond.expr1->cost > enode->cost) {
                enode->cost = enode->data.cond.expr1->cost;
            }
            if (enode->data.cond.expr2->cost > enode->cost) {
                enode->cost = enode->data.cond.expr2->cost;
            }
            enode->cost += 1;

            if (enode->cost > 200) {
                enode->cost = 200;
            }
            break;
        case IROLinearScope:
            enode = IRO_NewENode(linear->u.node->type);
            *enode = *linear->u.node;
            break;
        case IROLinearFunccall: {
            ENodeList* list;
            int i;
            enode = IRO_NewENode(linear->u.funccall.ispascal ? EFUNCCALLP : EFUNCCALL);
            enode->flags = linear->nodeflags;
            enode->data.funccall.funcref = BuildExpr(linear->u.funccall.callee);
            enode->data.funccall.functype = linear->u.funccall.functype;
            enode->data.funccall.args = NULL;
            enode->cost = 200;
            for (i = linear->u.funccall.argCount - 1; i >= 0; i--) {
                list = lalloc(sizeof(ENodeList));
                list->node = BuildExpr(linear->u.funccall.args[i]);
                list->next = enode->data.funccall.args;
                enode->data.funccall.args = list;
            }
            enode->rtype = IRO_UnqualifiedType(linear->rtype);
            break;
        }
        default:
            IRO_Dump("Oh, oh, bad expression type in BuildExpr at: %d\n", linear->index);
            CError_FATAL(2578);
    }

    enode->pointsTo = linear->pointsToFunction;
    return enode;
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#else
static ENode* BuildExpr(IROLinear* linear) {
    ENode* enode;

    switch (linear->type) {
        case IROLinearOperand:
            enode = IRO_NewENode(linear->u.node->type);
            enode->flags = linear->nodeflags;
            *enode = *linear->u.node;
            break;
        case IROLinearOp1Arg:
            enode = IRO_NewENode(linear->nodetype);
            enode->flags = linear->nodeflags;
            enode->data.monadic = BuildExpr(linear->u.monadic);
            enode->rtype = linear->rtype;
            enode->cost = enode->data.monadic->cost;
            if (!enode->cost) {
                enode->cost = 1;
            }
            break;
        case IROLinearOp2Arg:
            enode = IRO_NewENode(linear->nodetype);
            enode->flags = linear->nodeflags;
            enode->data.diadic.left = BuildExpr(linear->u.diadic.left);
            enode->data.diadic.right = BuildExpr(linear->u.diadic.right);

            enode->cost = enode->data.diadic.left->cost;
            if (enode->data.diadic.right->cost > enode->cost) {
                enode->cost = enode->data.diadic.right->cost;
            } else if (enode->data.diadic.right->cost == enode->cost) {
                enode->cost += 1;
            }

            if (ENODE_IS4(enode, ESHL, ESHR, EDIV, EMODULO)) {
                enode->cost += 2;
            }
            if (enode->cost > 200) {
                enode->cost = 200;
            }

            enode->rtype = linear->rtype;
            break;
        case IROLinearOp3Arg:
            enode = IRO_NewENode(linear->nodetype);
            enode->flags = linear->nodeflags;
            enode->data.cond.cond = BuildExpr(linear->u.args3.a);
            enode->data.cond.expr1 = BuildExpr(linear->u.args3.b);
            enode->data.cond.expr2 = BuildExpr(linear->u.args3.c);
            enode->rtype = linear->rtype;

            enode->cost = enode->data.cond.cond->cost;
            if (enode->data.cond.expr1->cost > enode->cost) {
                enode->cost = enode->data.cond.expr1->cost;
            }
            if (enode->data.cond.expr2->cost > enode->cost) {
                enode->cost = enode->data.cond.expr2->cost;
            }
            enode->cost += 1;

            if (enode->cost > 200) {
                enode->cost = 200;
            }

            break;

        case IROLinearFunccall: {
            ENodeList* list;
            int i;
            enode = IRO_NewENode(linear->u.funccall.ispascal ? EFUNCCALLP : EFUNCCALL);
            enode->flags = linear->nodeflags;
            enode->data.funccall.funcref = BuildExpr(linear->u.funccall.callee);
            enode->data.funccall.functype = linear->u.funccall.functype;
            enode->data.funccall.args = NULL;
            enode->cost = 200;
            for (i = linear->u.funccall.argCount - 1; i >= 0; i--) {
                list = lalloc(sizeof(ENodeList));
                list->node = BuildExpr(linear->u.funccall.args[i]);
                list->next = enode->data.funccall.args;
                enode->data.funccall.args = list;
            }
            enode->rtype = linear->rtype;
            break;
        }

        default:
            IRO_Dump("Oh, oh, bad expression type in BuildExpr at: %d\n", linear->index);
            CError_FATAL(2393);
    }

    enode->pointsTo = linear->pointsToFunction;
    return enode;
}
#endif

#if MWCCPSP
// 3.0.1: statements get the loop-depth weight (or the raw depth under
// copts.opt_returnloopdepthinstatementvalue); with
// copts.opt_markcounterloops a counter loop's test becomes an ST_BEGINLOOP
// statement that carries the test (StmtLoopInfo), its back branch is
// preceded by ST_ENDLOOP and its update is dropped; in a function without a
// return, a final end becomes a jump to a closing label; catch-object
// copies (IRO_IsCatchCopyAssignment) are dropped again.
extern Boolean IRO_IsCatchCopyAssignment(IROLinear* linear);

static UInt16 StmtValue(IRONode* node) {
    UInt16 depth;
    UInt16 value;
    UInt16 i;

    depth = node->loopdepth;
    value = depth;
    if (!copts.opt_returnloopdepthinstatementvalue) {
        i = 0;
        value = 1;
        if (depth) {
            do {
                if (value < 4096) {
                    value *= 8;
                } else {
                    value += 1;
                }
                i++;
            } while (i < depth);
        }
    }
    return value;
}

static Statement* NewDelinStatement(IROLinear* linear, int type) {
    Statement* stmt = IRO_NewStatement(type, NULL);
    stmt->value = 1;
    if (linear->stmt) {
        IRO_CopySourceInfo(stmt, linear->stmt);
    } else {
        stmt->sourceoffset = -1;
        stmt->sourcefilepath = NULL;
    }
    return stmt;
}

#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
Statement* IRO_Delinearize(IRONode* node, IROLinear* linear) {
    IROLinear* scanlin;
    IROLinear* endlin;
    Statement* firstStmt;
    Statement* lastStmt;
    Statement* stmt;
    Statement* test;
    IRONode* scan;
    IRONode mynode;
    Boolean hasReturn;

    lastStmt = NULL;
    firstStmt = NULL;

    IRO_RenumberInts();
    if (!node) {
        memset(&mynode, 0, sizeof(IRONode));
        mynode.first = linear;
        MarkAllSubExprs(linear);
        node = &mynode;
    } else {
        MarkAllSubExprs(IRO_FirstLinear);
    }

    endlin = NULL;
    hasReturn = FALSE;
    for (scan = node; scan; scan = scan->nextnode) {
        for (scanlin = scan->first; scanlin; scanlin = scanlin->next) {
            if (scanlin->type == IROLinearReturn) {
                hasReturn = TRUE;
            }
            if (scanlin == scan->last) {
                break;
            }
        }
    }

    for (scan = node; scan; scan = scan->nextnode) {
        for (scanlin = scan->first; scanlin; scanlin = scanlin->next) {
            test = NULL;
            stmt = NULL;
            if (!(scanlin->flags & IROLF_Reffed) &&
                (!copts.microsoft_EH || !IRO_IsCatchCopyAssignment(scanlin))) {
                switch (scanlin->type) {
                    case IROLinearNop:
                        if (scan == node) {
                            stmt = NewDelinStatement(scanlin, ST_NOP);
                        } else {
                            stmt = NULL;
                        }
                        break;
                    case IROLinearOperand:
                    case IROLinearOp1Arg:
                    case IROLinearOp2Arg:
                    case IROLinearOp3Arg:
                    case IROLinearScope:
                    case IROLinearFunccall:
                        if (!copts.opt_markcounterloops ||
                            ((scanlin->type != IROLinearOp1Arg || !scanlin->u.op1x.counterUpdate) &&
                             (scanlin->type != IROLinearOp2Arg ||
                              !scanlin->u.op2x.counterUpdate))) {
                            stmt = NewDelinStatement(scanlin, ST_EXPRESSION);
                            stmt->expr = BuildExpr(scanlin);
                        }
                        break;
                    case IROLinearEnd:
                        if (!hasReturn && scanlin->next) {
                            scanlin->type = IROLinearGoto;
                            scanlin->u.label.label = IRO_NewLabel();
                            endlin = scanlin;
                            goto gotos;
                        }
                        stmt = NULL;
                        break;
                    case IROLinearGoto:
                    case IROLinearGotoExpr:
gotos:
                        if (copts.opt_markcounterloops && (scanlin->flags & IROLF_EndLoop)) {
                            stmt = NewDelinStatement(scanlin, ST_ENDLOOP);
                            if (stmt) {
                                stmt->value = StmtValue(scan);
                                if (firstStmt) {
                                    lastStmt->next = stmt;
                                } else {
                                    firstStmt = stmt;
                                }
                                lastStmt = stmt;
                            }
                        }
                        if (scanlin->type == IROLinearGoto) {
                            stmt = NewDelinStatement(scanlin, ST_GOTO);
                            stmt->label = scanlin->u.label.label;
                        } else if (scanlin->type == IROLinearGotoExpr) {
                            stmt = NewDelinStatement(scanlin, ST_GOTOEXPR);
                            stmt->expr = BuildExpr(scanlin->u.monadic);
                        }
                        break;
                    case IROLinearIf:
                    case IROLinearIfNot:
                        if (copts.opt_markcounterloops && (scanlin->flags & IROLF_BeginLoop)) {
                            IROCounterLoopInfo* li;
                            StmtLoopInfo* info;

                            stmt = NewDelinStatement(scanlin, ST_BEGINLOOP);
                            info = lalloc(sizeof(StmtLoopInfo));
                            stmt->expr = (ENode*)info;
                            memset(info, 0, sizeof(StmtLoopInfo));
                            li = scanlin->u.label.loop;
                            info->x4 = BuildExpr(li->x8);
                            info->cond = BuildExpr(scanlin->u.label.cond);
                            test = NewDelinStatement(
                                scanlin, (scanlin->type == IROLinearIf) ? ST_IFGOTO : ST_IFNGOTO
                            );
                            test->label = scanlin->u.label.label;
                            test->expr = BuildExpr(scanlin->u.label.cond);
                            info->test = test;
                            if (li->xC) {
                                if (IRO_IsIntConstant(li->xC)) {
                                    info->x14 = li->xC->u.node->data.intval;
                                } else {
                                    info->x1C = BuildExpr(li->xC);
                                }
                            }
                            if (li->x1C) {
                                info->x2C = BuildExpr(li->x1C);
                            }
                            if (li->x4) {
                                info->x10 = BuildExpr(li->x4);
                            }
                            if (li->kind == 1) {
                                info->x20 = li->x10;
                                if (li->x18) {
                                    info->x28 = BuildExpr(li->x18);
                                }
                                info->flags |= 1;
                            }
                            if (stmt) {
                                stmt->value = StmtValue(scan);
                            }
                        }
                        if (!test) {
                            stmt = NewDelinStatement(
                                scanlin, (scanlin->type == IROLinearIf) ? ST_IFGOTO : ST_IFNGOTO
                            );
                            stmt->label = scanlin->u.label.label;
                            stmt->expr = BuildExpr(scanlin->u.label.cond);
                        }
                        if (copts.opt_markcounterloops && test) {
                            test->value = stmt->value;
                        }
                        break;
                    case IROLinearReturn:
                        stmt = NewDelinStatement(scanlin, ST_RETURN);
                        if (scanlin->u.monadic) {
                            stmt->expr = BuildExpr(scanlin->u.monadic);
                        }
                        break;
                    case IROLinearLabel:
                        stmt = NewDelinStatement(scanlin, ST_LABEL);
                        stmt->label = scanlin->u.label.label;
                        stmt->label->stmt = stmt;
                        break;
                    case IROLinearSwitch:
                        stmt = NewDelinStatement(scanlin, ST_SWITCH);
                        stmt->expr = BuildExpr(scanlin->u.swtch.cond);
                        stmt->label = (CLabel*)scanlin->u.swtch.info;
                        break;
                    case IROLinearEntry:
                        stmt = NewDelinStatement(scanlin, ST_ENTRY);
                        stmt->label = scanlin->u.label.label;
                        stmt->label->stmt = stmt;
                        break;
                    case IROLinearExit:
                        stmt = NewDelinStatement(scanlin, ST_EXIT);
                        stmt->label = scanlin->u.label.label;
                        break;
                    case IROLinearBeginCatch:
                        stmt = NewDelinStatement(scanlin, ST_BEGINCATCH);
                        stmt->expr = BuildExpr(scanlin->u.ctch.linear);
                        break;
                    case IROLinearEndCatch:
                        stmt = NewDelinStatement(scanlin, ST_ENDCATCH);
                        stmt->expr = BuildExpr(scanlin->u.monadic);
                        break;
                    case IROLinearEndCatchDtor:
                        stmt = NewDelinStatement(scanlin, ST_ENDCATCHDTOR);
                        stmt->expr = BuildExpr(scanlin->u.monadic);
                        break;
                    case IROLinearAsm:
                        stmt = scanlin->u.asmx.stmt;
                        if (!stmt) {
                            stmt = NewDelinStatement(scanlin, ST_EXPRESSION);
                            stmt->expr = IRO_NewENode(scanlin->u.asmx.expr->type);
                            *stmt->expr = *scanlin->u.asmx.expr;
                            stmt->expr->pointsTo = scanlin->pointsToFunction;
                        }
                        break;
                    default:
                        CError_FATAL(2943);
                        break;
                }

                if (stmt) {
                    stmt->value = StmtValue(scan);
                    stmt->next = NULL;
                    if (firstStmt) {
                        lastStmt->next = stmt;
                    } else {
                        firstStmt = stmt;
                    }
                    lastStmt = stmt;
                }
            }
            if (scanlin == scan->last) {
                break;
            }
        }
    }

    if (endlin) {
        stmt = NewDelinStatement(endlin, ST_LABEL);
        stmt->label = endlin->u.label.label;
        stmt->label->stmt = stmt;
        stmt->value = !copts.opt_returnloopdepthinstatementvalue;
        stmt->next = NULL;
        lastStmt->next = stmt;
    }
    return firstStmt;
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#else
Statement* IRO_Delinearize(IRONode* node, IROLinear* linear) {
    IROLinear* scanlin;
    Statement* firstStmt;
    Statement* lastStmt;
    IRONode* scan;
    Statement* stmt;
    IRONode mynode;

    firstStmt = lastStmt = NULL;

    IRO_RenumberInts();
    if (node) {
        scan = node;
        MarkAllSubExprs(IRO_FirstLinear);
    } else {
        memset(&mynode, 0, sizeof(IRONode));
        mynode.first = linear;
        scan = &mynode;
        MarkAllSubExprs(linear);
    }

    while (scan) {
        for (scanlin = scan->first; scanlin; scanlin = scanlin->next) {
            stmt = NULL;
            if (!(scanlin->flags & IROLF_Reffed)) {
                switch (scanlin->type) {
                    case IROLinearNop:
                        if (scan == node) {
                            stmt = NewStatement(scanlin, ST_NOP);
                        } else {
                            stmt = NULL;
                        }
                        break;
                    case IROLinearOperand:
                    case IROLinearOp1Arg:
                    case IROLinearOp2Arg:
                    case IROLinearOp3Arg:
                    case IROLinearFunccall:
                        stmt = NewStatement(scanlin, ST_EXPRESSION);
                        stmt->expr = BuildExpr(scanlin);
                        break;
                    case IROLinearGoto:
                        stmt = NewStatement(scanlin, ST_GOTO);
                        stmt->label = scanlin->u.label.label;
                        break;
                    case IROLinearExit:
                        stmt = NewStatement(scanlin, ST_EXIT);
                        stmt->label = scanlin->u.label.label;
                        break;
                    case IROLinearIf:
                    case IROLinearIfNot:
                        stmt = NewStatement(
                            scanlin, (scanlin->type == IROLinearIf) ? ST_IFGOTO : ST_IFNGOTO
                        );
                        stmt->label = scanlin->u.label.label;
                        stmt->expr = BuildExpr(scanlin->u.label.cond);
                        break;
                    case IROLinearReturn:
                        stmt = NewStatement(scanlin, ST_RETURN);
                        if (scanlin->u.monadic) {
                            stmt->expr = BuildExpr(scanlin->u.monadic);
                        }
                        break;
                    case IROLinearLabel:
                        stmt = NewStatement(scanlin, ST_LABEL);
                        stmt->label = scanlin->u.label.label;
                        stmt->label->stmt = stmt;
                        break;
                    case IROLinearEntry:
                        stmt = NewStatement(scanlin, ST_ENTRY);
                        stmt->label = scanlin->u.label.label;
                        stmt->label->stmt = stmt;
                        break;
                    case IROLinearSwitch:
                        stmt = NewStatement(scanlin, ST_SWITCH);
                        stmt->expr = BuildExpr(scanlin->u.swtch.cond);
                        stmt->label = (CLabel*)scanlin->u.swtch.info;
                        break;
                    case IROLinearBeginCatch:
                        stmt = NewStatement(scanlin, ST_BEGINCATCH);
                        stmt->expr = BuildExpr(scanlin->u.ctch.linear);
                        break;
                    case IROLinearEndCatch:
                        stmt = NewStatement(scanlin, ST_ENDCATCH);
                        stmt->expr = BuildExpr(scanlin->u.monadic);
                        break;
                    case IROLinearEndCatchDtor:
                        stmt = NewStatement(scanlin, ST_ENDCATCHDTOR);
                        stmt->expr = BuildExpr(scanlin->u.monadic);
                        break;
                    case IROLinearAsm:
                        stmt = scanlin->u.asm_stmt;
                        break;
                    case IROLinearEnd:
                        stmt = NULL;
                        break;
                    default:
                        CError_FATAL(2692);
                }

                if (stmt) {
                    if (LoopOptimizerRun) {
                        SInt32 i;
                        SInt32 value = 1;
                        for (i = 0; i < scan->loopdepth; i++) {
                            value = (value < 4096) ? (value * 8) : (value + 1);
                        }
                        stmt->value = value;
                    }
                    if (firstStmt) {
                        lastStmt->next = stmt;
                    } else {
                        firstStmt = stmt;
                    }
                    lastStmt = stmt;
                }
            }
            if (scanlin == scan->last) {
                break;
            }
        }
        scan = scan->nextnode;
    }

    return firstStmt;
}
#endif

void IRO_RenumberInts(void) {
    IROLinear* linear = IRO_FirstLinear;
    IRO_NumLinear = 0;
    while (linear) {
        linear->index = IRO_NumLinear++;
        linear = linear->next;
    }
}

#if MWCCPSP
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
static void TravExprToUpdateFlags(IROLinear* linear) {
    int i;

    linear->flags &=
        ~(IROLF_Assigned | IROLF_ReplaceCSE | IROLF_Used | IROLF_Ind | IROLF_Subs |
          IROLF_LoopInvariant | IROLF_Ris | IROLF_Immind | IROLF_CouldError | IROLF_Unk200000);
    switch (linear->type) {
        case IROLinearOperand:
        case IROLinearScope:
            break;
        case IROLinearOp1Arg:
            TravExprToUpdateFlags(linear->u.monadic);
            if (IRO_IsAssignOp[linear->nodetype]) {
                MarkAssigned(linear->u.monadic, TRUE);
            }
            if (linear->nodetype == EINDIRECT) {
                MarkSubs1(linear->u.monadic);
                linear->u.monadic->flags |= IROLF_Immind | IROLF_Ind;
                if (IS_LINEAR_DIADIC(linear->u.monadic, EADD)) {
                    MarkSubscript(linear->u.monadic);
                }
            }
            break;
        case IROLinearOp2Arg:
            TravExprToUpdateFlags(linear->u.diadic.left);
            TravExprToUpdateFlags(linear->u.diadic.right);
            if (IRO_IsAssignOp[linear->nodetype]) {
                MarkAssigned(linear->u.diadic.left, IRO_IsModifyOp[linear->nodetype]);
            }
            break;
        case IROLinearOp3Arg:
            TravExprToUpdateFlags(linear->u.args3.a);
            TravExprToUpdateFlags(linear->u.args3.b);
            TravExprToUpdateFlags(linear->u.args3.c);
            break;
        case IROLinearFunccall:
            TravExprToUpdateFlags(linear->u.funccall.callee);
            for (i = linear->u.funccall.argCount - 1; i >= 0; i--) {
                TravExprToUpdateFlags(linear->u.funccall.args[i]);
            }
            break;
        default:
            IRO_Dump(
                "Oh, oh, bad expression type in TravExprToUpdateFlags at: %d\n", linear->index
            );
            CError_FATAL(3114);
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#else
static void TravExprToUpdateFlags(IROLinear* linear) {
    int i;

    linear->flags &=
        ~(IROLF_Assigned | IROLF_ReplaceCSE | IROLF_Used | IROLF_Ind | IROLF_Subs |
          IROLF_LoopInvariant | IROLF_Ris | IROLF_Immind | IROLF_CouldError);
    switch (linear->type) {
        case IROLinearNop:
        case IROLinearOperand:
            break;
        case IROLinearOp1Arg:
            TravExprToUpdateFlags(linear->u.monadic);
            if (IRO_IsAssignOp[linear->nodetype]) {
                MarkAssigned(linear->u.monadic, TRUE);
            }
            if (linear->nodetype == EINDIRECT) {
                MarkSubs1(linear->u.monadic);
                linear->u.monadic->flags |= IROLF_Immind | IROLF_Ind;
                if (IS_LINEAR_DIADIC(linear->u.monadic, EADD)) {
                    MarkSubscript(linear->u.monadic);
                }
            }
            break;
        case IROLinearOp2Arg:
            TravExprToUpdateFlags(linear->u.diadic.left);
            TravExprToUpdateFlags(linear->u.diadic.right);
            if (IRO_IsAssignOp[linear->nodetype]) {
                MarkAssigned(linear->u.diadic.left, IRO_IsModifyOp[linear->nodetype]);
            }
            break;
        case IROLinearOp3Arg:
            TravExprToUpdateFlags(linear->u.args3.a);
            TravExprToUpdateFlags(linear->u.args3.b);
            TravExprToUpdateFlags(linear->u.args3.c);
            break;
        case IROLinearFunccall:
            TravExprToUpdateFlags(linear->u.funccall.callee);
            for (i = linear->u.funccall.argCount - 1; i >= 0; i--) {
                TravExprToUpdateFlags(linear->u.funccall.args[i]);
            }
            break;
        default:
            IRO_Dump(
                "Oh, oh, bad expression type in TravExprToUpdateFlags at: %d\n", linear->index
            );
            CError_FATAL(2860);
    }
}
#endif

#if MWCCPSP
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
void IRO_UpdateFlagsOnInts(IROLinear* first) {
    IROLinear* linear;

    MarkAllSubExprs(first);

    for (linear = first; linear; linear = linear->next) {
        if (!(linear->flags & IROLF_Reffed)) {
            switch (linear->type) {
                case IROLinearOperand:
                case IROLinearOp1Arg:
                case IROLinearOp2Arg:
                case IROLinearOp3Arg:
                case IROLinearScope:
                case IROLinearFunccall:
                    TravExprToUpdateFlags(linear);
                    break;
                case IROLinearGotoExpr:
                    TravExprToUpdateFlags(linear->u.monadic);
                    break;
                case IROLinearIf:
                case IROLinearIfNot:
                    TravExprToUpdateFlags(linear->u.label.cond);
                    break;
                case IROLinearReturn:
                    if (linear->u.monadic) {
                        TravExprToUpdateFlags(linear->u.monadic);
                    }
                    break;
                case IROLinearSwitch:
                    TravExprToUpdateFlags(linear->u.swtch.cond);
                    break;
                case IROLinearBeginCatch:
                    TravExprToUpdateFlags(linear->u.ctch.linear);
                    break;
                case IROLinearEndCatch:
                    TravExprToUpdateFlags(linear->u.monadic);
                    break;
                case IROLinearEndCatchDtor:
                    TravExprToUpdateFlags(linear->u.monadic);
                    break;
                case IROLinearNop:
                case IROLinearGoto:
                case IROLinearLabel:
                case IROLinearEntry:
                case IROLinearExit:
                case IROLinearAsm:
                case IROLinearEnd:
                    break;
                default:
                    CError_FATAL(3188);
            }
        }
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#else
void IRO_UpdateFlagsOnInts(void) {
    IROLinear* linear;
    IRONode* node;

    MarkAllSubExprs(IRO_FirstLinear);

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        for (linear = node->first; linear; linear = linear->next) {
            if (!(linear->flags & IROLF_Reffed)) {
                switch (linear->type) {
                    case IROLinearOperand:
                    case IROLinearOp1Arg:
                    case IROLinearOp2Arg:
                    case IROLinearOp3Arg:
                    case IROLinearFunccall:
                        TravExprToUpdateFlags(linear);
                        break;
                    case IROLinearIf:
                    case IROLinearIfNot:
                        TravExprToUpdateFlags(linear->u.label.cond);
                        break;
                    case IROLinearReturn:
                        if (linear->u.monadic) {
                            TravExprToUpdateFlags(linear->u.monadic);
                        }
                        break;
                    case IROLinearSwitch:
                        TravExprToUpdateFlags(linear->u.swtch.cond);
                        break;
                    case IROLinearBeginCatch:
                        TravExprToUpdateFlags(linear->u.ctch.linear);
                        break;
                    case IROLinearEndCatch:
                        TravExprToUpdateFlags(linear->u.monadic);
                        break;
                    case IROLinearEndCatchDtor:
                        TravExprToUpdateFlags(linear->u.monadic);
                        break;
                    case IROLinearNop:
                    case IROLinearGoto:
                    case IROLinearLabel:
                    case IROLinearEntry:
                    case IROLinearExit:
                    case IROLinearAsm:
                    case IROLinearEnd:
                        break;
                    default:
                        CError_FATAL(2938);
                }
            }
            if (linear == node->last) {
                break;
            }
        }
    }
}
#endif

void IRO_SaveLinearIR(IROLinearIRSave* save) {
    save->firstLinear = IRO_FirstLinear;
    save->lastLinear = IRO_LastLinear;
    save->numLinear = IRO_NumLinear;
    save->curStat = CurStat;
    save->disableDueToAsm = DisableDueToAsm;
    save->isLeafFunction = IRO_IsLeafFunction;
    save->functionHasReturn = IRO_FunctionHasReturn;
    save->nullCheckList = NullCheckList;
    save->currStmt = CurrStmt;
    save->prevStmt = PrevStmt;
}

void IRO_RestoreLinearIR(IROLinearIRSave* save) {
    IRO_FirstLinear = save->firstLinear;
    IRO_LastLinear = save->lastLinear;
    IRO_NumLinear = save->numLinear;
    CurStat = save->curStat;
    DisableDueToAsm = save->disableDueToAsm;
    IRO_IsLeafFunction = save->isLeafFunction;
    IRO_FunctionHasReturn = save->functionHasReturn;
    NullCheckList = save->nullCheckList;
    CurrStmt = save->currStmt;
    PrevStmt = save->prevStmt;
}
#if MWCCPSP

#endif
#if MWCCPSP
extern Boolean data_00625ECA;

// 0x561320
void func_00561320(IROLinearState* state) {
    IRO_FirstLinear = state->firstLinear;
    IRO_LastLinear = state->lastLinear;
    IRO_NumLinear = state->numLinear;
    CurStat = state->curStat;
    DisableDueToAsm = state->disableDueToAsm;
    data_00625ECA = state->x11;
    NullCheckList = state->nullCheckList;
    CurrStmt = state->currStmt;
    PrevStmt = state->prevStmt;
    FirstStmt = state->firstStmt;
    CatchObjects = state->catchObjects;
}

// 0x561390
void func_00561390(IROLinearState* state) {
    state->firstLinear = IRO_FirstLinear;
    state->lastLinear = IRO_LastLinear;
    state->numLinear = IRO_NumLinear;
    state->curStat = CurStat;
    state->disableDueToAsm = DisableDueToAsm;
    state->x11 = data_00625ECA;
    state->nullCheckList = NullCheckList;
    state->currStmt = CurrStmt;
    state->prevStmt = PrevStmt;
    state->firstStmt = FirstStmt;
    state->catchObjects = CatchObjects;
}
#endif
