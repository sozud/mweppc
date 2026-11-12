#include "compiler/IroCSE.h"
#include "compiler/IroDump.h"
#if MWCCPSP
#include "compiler/IrOptimizer.h"
#include "compiler/CFunc.h"
#include "compiler/Exceptions.h"
#endif
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroMalloc.h"
#include "compiler/IroPointerAnalysis.h"
#include "compiler/IroSubable.h"
#include "compiler/IROUseDef.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"
#include "compiler/BitVector.h"
#include "compiler/CError.h"
#include "compiler/CExpr.h"
#include "compiler/CInt64.h"
#include "compiler/CompilerTools.h"
#include "compiler/CParser.h"
#include "compiler/enode.h"
#include "compiler/objects.h"
#include <string.h>

BitVector* IRO_Depends;
Boolean IRO_NotSubable;
Boolean IRO_IsVolatile;
Boolean IRO_CouldError;
IROExpr* IRO_FirstExpr;
IROExpr* IRO_LastExpr;
SInt32 IRO_NumExprs;
static Boolean HasVectorOperand;

#if !MWCCPSP
static void IRO_DependsOnForDataFlow(IROLinear* linear, Boolean isIndirect);
#endif

#if !MWCCPSP
static void GetDependsOfIndirect(IROLinear* nd) {
    IROListNode* resultList;
    IROListNode* next;
    IROListNode* scan;
    IROLinear* scannd;
    Object* obj;
    VarRecord* var;
    int index;
    Boolean result;
    Boolean foundObjRef;

    result = FALSE;

    if (nd && copts.opt_pointer_analysis && nd->pointsToFunction && FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupLinearNodePointerExpr(FunctionName, nd, &resultList);
        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail) {
                    result = TRUE;
                    break;
                }

                foundObjRef = FALSE;
                for (scannd = scan->list.head; scannd != scan->list.tail->next;
                     scannd = scannd->next) {
                    if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                        foundObjRef = TRUE;
                        break;
                    }
                }

                if (!foundObjRef) {
                    result = TRUE;
                    break;
                }
            }

            if (!result) {
                for (scan = resultList; scan; scan = scan->nextList) {
                    for (scannd = scan->list.head; scannd != scan->list.tail->next;
                         scannd = scannd->next) {
                        if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                            obj = scannd->u.node->data.objref;
                            CError_ASSERT(119, obj != NULL);
                            var = IRO_FindVar(obj, TRUE, TRUE);
                            CError_ASSERT(121, var != NULL);
                            index = var->index;
                            CError_ASSERT(123, index != 0);

                            if (is_volatile_object(obj)) {
                                IRO_IsVolatile = TRUE;
                                IRO_NotSubable = TRUE;
                            }
                            Bv_SetBit(index, IRO_Depends);
                        }
                    }
                }
            }

            while (resultList) {
                next = resultList->nextList;
                IRO_free(resultList);
                resultList = next;
            }
        } else {
            result = TRUE;
        }
    } else {
        result = TRUE;
    }

    if (result) {
        nd = nd->u.monadic;
        if (nd->type == IROLinearOp1Arg && nd->nodetype == EBITFIELD) {
            nd = nd->u.monadic;
        }

        if (nd->type == IROLinearOp2Arg && nd->nodetype == EADD) {
            IRO_BaseTerms = 0;
            IRO_VarTerms = 0;
            IRO_DecomposeAddressExpression_Cheap(nd);
            if (IRO_BaseTerms != 1) {
                IRO_CouldError = TRUE;
                Bv_SetBit(0, IRO_Depends);
                Bv_Or(IRO_FuncKills, IRO_Depends);
            }
            if (IRO_VarTerms) {
                IRO_CouldError = TRUE;
            }
        } else {
            IRO_CouldError = TRUE;
            Bv_SetBit(0, IRO_Depends);
            Bv_Or(IRO_FuncKills, IRO_Depends);
        }
    }
}

static void GetDependsOfFunctionCallForDataFlow(IROLinear* nd) {
    IROLinear* innernd;
    IROListNode* resultList;
    IROListNode* scan;
    IROLinear* scannd;
    Object* obj;
    VarRecord* var;
    int index;
    int i;
    IROListNode* next;
    Boolean result;
    Boolean foundObjRef;
    ObjectList* olist;
    ObjectList* depsList;

    result = FALSE;
    innernd = nd->u.funccall.callee;

    if (innernd && copts.opt_pointer_analysis && innernd->pointsToFunction && FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupLinearNodePointerExpr(FunctionName, innernd, &resultList);
        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail) {
                    result = TRUE;
                    break;
                }

                foundObjRef = FALSE;
                for (scannd = scan->list.head; scannd != scan->list.tail->next;
                     scannd = scannd->next) {
                    if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                        foundObjRef = TRUE;
                        obj = scannd->u.node->data.objref;
                        CError_ASSERT(234, obj != NULL);

                        depsList = NULL;
                        PointerAnalysis_GetFunctionDependencies(obj, nd, &depsList);

                        for (olist = depsList; olist; olist = olist->next) {
                            if (!olist->object) {
                                result = TRUE;
                                break;
                            }
                        }

                        while (depsList) {
                            olist = depsList->next;
                            IRO_free(depsList);
                            depsList = olist;
                        }

                        if (result) {
                            break;
                        }
                    }
                }

                if (!foundObjRef) {
                    result = TRUE;
                }
                if (result) {
                    break;
                }
            }

            if (!result) {
                for (scan = resultList; scan; scan = scan->nextList) {
                    for (scannd = scan->list.head; scannd != scan->list.tail->next;
                         scannd = scannd->next) {
                        if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                            obj = scannd->u.node->data.objref;

                            depsList = NULL;
                            PointerAnalysis_GetFunctionDependencies(obj, nd, &depsList);

                            for (olist = depsList; olist; olist = olist->next) {
                                var = IRO_FindVar(olist->object, TRUE, TRUE);
                                CError_ASSERT(285, var != NULL);
                                index = var->index;
                                CError_ASSERT(287, index != 0);

                                if (is_volatile_object(olist->object)) {
                                    IRO_IsVolatile = TRUE;
                                    IRO_NotSubable = TRUE;
                                }
                                Bv_SetBit(index, IRO_Depends);
                            }

                            while (depsList) {
                                olist = depsList->next;
                                IRO_free(depsList);
                                depsList = olist;
                            }
                        }
                    }
                }
            }

            while (resultList) {
                next = resultList->nextList;
                IRO_free(resultList);
                resultList = next;
            }
        } else {
            result = TRUE;
        }
    } else {
        result = TRUE;
    }

    if (result) {
        IRO_DependsOnForDataFlow(nd->u.funccall.callee, FALSE);
        Bv_Or(IRO_FuncKills, IRO_Depends);

        for (i = nd->u.funccall.argCount - 1; i >= 0; i--) {
            IRO_DependsOnForDataFlow(nd->u.funccall.args[i], FALSE);
        }
    }
}

static void IRO_DependsOn(IROLinear* linear, Boolean isIndirect) {
    VarRecord* var;
    IROLinear* inner;

    if (linear->rtype && CParser_IsVolatile(linear->rtype, linear->nodeflags & ENODE_FLAG_QUALS)) {
        IRO_IsVolatile = TRUE;
        IRO_NotSubable = TRUE;
    }

    if (IRO_NotSubable) {
        return;
    }

    switch (linear->type) {
        case IROLinearOperand:
            if (isIndirect && linear->u.node->type == EOBJREF) {
                if ((var = IRO_FindVar(linear->u.node->data.objref, FALSE, TRUE))) {
                    if (is_volatile_object(var->object)) {
                        IRO_IsVolatile = TRUE;
                        IRO_NotSubable = TRUE;
                    }
                    Bv_SetBit(var->index, IRO_Depends);
                } else {
                    IRO_NotSubable = TRUE;
                }
            }
            break;
        case IROLinearOp1Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                IRO_NotSubable = TRUE;
                return;
            }
            inner = linear->u.monadic;
            if (linear->nodetype == EINDIRECT) {
                if (inner->type == IROLinearOp1Arg && inner->nodetype == EBITFIELD) {
                    inner = inner->u.monadic;
                }
                if (inner->type != IROLinearOperand || inner->u.node->type != EOBJREF) {
                    GetDependsOfIndirect(linear);
                }
            }

            IRO_DependsOn(inner, linear->nodetype == EINDIRECT);
            break;
        case IROLinearOp2Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                IRO_NotSubable = TRUE;
                return;
            }
            if (linear->nodetype == EDIV || linear->nodetype == EMODULO) {
                if (IRO_IsIntConstant(linear->u.diadic.right)) {
                    if (CInt64_Equal(linear->u.diadic.right->u.node->data.intval, cint64_zero)) {
                        IRO_CouldError = TRUE;
                    }
                } else {
                    IRO_CouldError = TRUE;
                }
            }

            IRO_DependsOn(linear->u.diadic.left, isIndirect);
            IRO_DependsOn(linear->u.diadic.right, isIndirect);
            break;
        case IROLinearOp3Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                IRO_NotSubable = TRUE;
                return;
            }
            IRO_DependsOn(linear->u.args3.a, isIndirect);
            IRO_DependsOn(linear->u.args3.b, isIndirect);
            IRO_DependsOn(linear->u.args3.c, isIndirect);
            break;
        case IROLinearFunccall:
            IRO_NotSubable = TRUE;
            break;
        default:
            CError_FATAL(479);
    }
}

static void IRO_DependsOnForDataFlow(IROLinear* linear, Boolean isIndirect) {
    VarRecord* var;
    IROLinear* inner;

    if (linear->rtype && CParser_IsVolatile(linear->rtype, linear->nodeflags & ENODE_FLAG_QUALS)) {
        IRO_IsVolatile = TRUE;
        IRO_NotSubable = TRUE;
    }

    switch (linear->type) {
        case IROLinearOperand:
            if (isIndirect && linear->u.node->type == EOBJREF) {
                if ((var = IRO_FindVar(linear->u.node->data.objref, FALSE, TRUE))) {
                    if (is_volatile_object(var->object)) {
                        IRO_IsVolatile = TRUE;
                        IRO_NotSubable = TRUE;
                    }
                    Bv_SetBit(var->index, IRO_Depends);
                } else {
                    IRO_NotSubable = TRUE;
                }
            }
            break;
        case IROLinearOp1Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                IRO_NotSubable = TRUE;
            }

            inner = linear->u.monadic;
            if (linear->nodetype == EINDIRECT) {
                if (inner->type == IROLinearOp1Arg && inner->nodetype == EBITFIELD) {
                    inner = inner->u.monadic;
                }
                if (inner->type != IROLinearOperand || inner->u.node->type != EOBJREF) {
                    GetDependsOfIndirect(linear);
                }
            }

            IRO_DependsOnForDataFlow(inner, linear->nodetype == EINDIRECT);
            break;
        case IROLinearOp2Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                IRO_NotSubable = TRUE;
            }

            if (linear->nodetype == EDIV || linear->nodetype == EMODULO) {
                if (IRO_IsIntConstant(linear->u.diadic.right)) {
                    if (CInt64_Equal(linear->u.diadic.right->u.node->data.intval, cint64_zero)) {
                        IRO_CouldError = TRUE;
                    }
                } else {
                    IRO_CouldError = TRUE;
                }
            }

            IRO_DependsOnForDataFlow(linear->u.diadic.left, isIndirect);
            IRO_DependsOnForDataFlow(linear->u.diadic.right, isIndirect);
            break;
        case IROLinearOp3Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                IRO_NotSubable = TRUE;
            }

            IRO_DependsOnForDataFlow(linear->u.args3.a, isIndirect);
            IRO_DependsOnForDataFlow(linear->u.args3.b, isIndirect);
            IRO_DependsOnForDataFlow(linear->u.args3.c, isIndirect);
            break;
        case IROLinearFunccall:
            IRO_NotSubable = TRUE;
            GetDependsOfFunctionCallForDataFlow(linear);
            break;
        default:
            CError_FATAL(650);
    }
}

void IRO_FindDepends_NoAlloc(IROLinear* linear) {
    Bv_Clear(IRO_Depends);
    IRO_CouldError = FALSE;
    IRO_NotSubable = FALSE;
    IRO_IsVolatile = FALSE;
    IRO_DependsOnForDataFlow(linear, FALSE);
}

void IRO_FindDepends(IROLinear* linear) {
    Bv_AllocVector(&IRO_Depends, IRO_NumVars + 1);
    IRO_CouldError = FALSE;
    IRO_NotSubable = FALSE;
    IRO_DependsOn(linear, FALSE);
}
#else
// 3.0.1 (IroDependsKills.c, 0x569AC0, 0x56AB70..0x56B930): one walker with
// three switches: allocate the vector, stop at the first thing that makes the
// expression unsubable, and descend into operands. A memory access depends
// on what a store of its size may reach (IRO_SizeKills, IRO_KillAll) instead
// of 2.4.7's IRO_FuncKills; an address is checked with
// IRO_FindAddressObjRefs; a call adds the objects of its exception actions.
extern BitVector* data_00624DA8;
extern SInt32 IRO_MaxKillSize;
extern BitVector* IRO_SizeKills[0x401];
extern BitVector* IRO_KillAll;
extern Boolean IRO_FunctionCallMightThrowException(IROLinear* linear);
extern void func_0054FC80(ExceptionAction* action, void (*func)(Object*, void*), void* arg);

#if MWCCPS2
#line 443 "IroDependsKills.c" // b210 names this part in its asserts
#endif
#if MWCCPS2
#define CSE_NOINLINE \
    __declspec(noinline) // b210 calls these (IRO_AddExpression, IRO_CommonSubs, func_0058DFE0)
#else
#define CSE_NOINLINE
#endif
#if MWCCPS2
#define CSE_BIT(x) ((UInt32)(x)) // b210 shifts a bit number unsigned
#else
#define CSE_BIT(x) (x)
#endif

// 0x569AC0: what an access may read: the variables a load of its size may
// reach, else every addressed or global variable
void func_00569AC0(BitVector* bv, IROLinear* nd) {
    SInt32 size;
    BitVector** table;

    if (nd->type == IROLinearOp1Arg && nd->nodetype == EINDIRECT && (nd->flags & IROLF_Assigned)) {
        size = nd->rtype->size;
        if (size > 0x400) {
            size = 0x400;
        }
        table = IRO_SizeKills;
    } else {
        size = 0;
        table = &IRO_KillAll;
    }
    if (size > IRO_MaxKillSize) {
        Bv_Clear(bv);
        Bv_SetBit(0, bv);
    } else {
        Bv_Copy(table[size], bv);
    }
}

// 0x56B4D0: an object of an exception action
#if MWCCPSP && !MWCCPS2
#line 469 "IroDependsKills.c"
#endif
static void DependsOnExceptionObject(Object* obj, void* arg) {
    VarRecord* var;
    int index;

    var = IRO_FindVar(obj, TRUE, TRUE);
    CError_ASSERT(215, var != NULL);
    index = var->index;
    CError_ASSERT(217, index != 0);
    if (is_volatile_object(obj)) {
        IRO_IsVolatile = TRUE;
        IRO_NotSubable = TRUE;
    }
    Bv_SetBit(index, IRO_Depends);
}
#if MWCCPSP && !MWCCPS2
#line 486 "IroCSE.c"
#endif

static inline Boolean ListHasObject(IROListNode* scan) {
    IROLinear* nd;

    for (nd = scan->list.head; nd != scan->list.tail->next; nd = nd->next) {
        if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
            return TRUE;
        }
    }
    return FALSE;
}

// 0x56B570: *p where p is not an object's address
static void GetDependsOfIndirect(IROLinear* nd) {
    IROListNode* resultList;
    IROListNode* scan;
    IROListNode* next;
    IROLinear* scannd;
    IROLinear* inner;
    IROLinear* objref;
    Object* obj;
    VarRecord* var;
    int index;
    int numObjRefs;
    int numOther;
    Boolean result;

    result = FALSE;
    if (nd && copts.opt_pointer_analysis && nd->pointsToFunction && FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupLinearNodePointerExpr(FunctionName, nd, &resultList);
        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail || !ListHasObject(scan)) {
                    result = TRUE;
                    break;
                }
            }
            if (!result) {
                for (scan = resultList; scan; scan = scan->nextList) {
                    for (scannd = scan->list.head; scannd != scan->list.tail->next;
                         scannd = scannd->next) {
                        if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                            obj = scannd->u.node->data.objref;
                            CError_ASSERT(126, obj != NULL);
                            var = IRO_FindVar(obj, TRUE, TRUE);
                            CError_ASSERT(128, var != NULL);
                            index = var->index;
                            CError_ASSERT(130, index != 0);
                            if (is_volatile_object(obj)) {
                                IRO_IsVolatile = TRUE;
                                IRO_NotSubable = TRUE;
                            }
                            Bv_SetBit(index, IRO_Depends);
                        }
                    }
                }
            }
            while (resultList) {
                next = resultList->nextList;
                IRO_free2(resultList);
                resultList = next;
            }
        } else {
            result = TRUE;
        }
    } else {
        result = TRUE;
    }

    if (result) {
        inner = nd->u.monadic;
        if (inner->type == IROLinearOp1Arg && inner->nodetype == EBITFIELD) {
            inner = inner->u.monadic;
        }
        if (inner->type == IROLinearOp2Arg && inner->nodetype == EADD) {
            IRO_FindAddressObjRefs(inner, &objref, &numObjRefs, &numOther);
            if (numObjRefs == 1) {
                obj = objref->u.node->data.objref;
                var = IRO_FindVar(obj, TRUE, TRUE);
                if (is_volatile_object(obj)) {
                    IRO_IsVolatile = TRUE;
                    IRO_NotSubable = TRUE;
                }
                Bv_SetBit(var->index, IRO_Depends);
            } else {
                IRO_CouldError = TRUE;
                Bv_SetBit(0, IRO_Depends);
                func_00569AC0(data_00624DA8, nd);
                Bv_Or(data_00624DA8, IRO_Depends);
            }
            if (numOther) {
                IRO_CouldError = TRUE;
            }
        } else {
            IRO_CouldError = TRUE;
            Bv_SetBit(0, IRO_Depends);
            func_00569AC0(data_00624DA8, nd);
            Bv_Or(data_00624DA8, IRO_Depends);
        }
    }
}

// 0x56B190
#if MWCCPSP && !MWCCPS2
#line 592 "IroDependsKills.c"
#endif
static void GetDependsOfFunctionCall(IROLinear* nd) {
    IROLinear* innernd;
    IROListNode* resultList;
    IROListNode* scan;
    IROListNode* next;
    IROLinear* scannd;
    Object* obj;
    VarRecord* var;
    int index;
    Boolean result;
    Boolean foundObjRef;
    ObjectList* olist;
    ObjectList* depsList;

    result = FALSE;
    innernd = nd->u.funccall.callee;
    if (innernd && copts.opt_pointer_analysis && innernd->pointsToFunction && FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupLinearNodePointerExpr(FunctionName, innernd, &resultList);
        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail) {
                    result = TRUE;
                    break;
                }
                foundObjRef = FALSE;
                for (scannd = scan->list.head; scannd != scan->list.tail->next;
                     scannd = scannd->next) {
                    if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                        foundObjRef = TRUE;
                        obj = scannd->u.node->data.objref;
                        CError_ASSERT(273, obj != NULL);
                        depsList = NULL;
                        PointerAnalysis_GetFunctionDependencies(obj, nd, &depsList);
                        for (olist = depsList; olist; olist = olist->next) {
                            if (!olist->object) {
                                result = TRUE;
                                break;
                            }
                        }
                        while (depsList) {
                            olist = depsList->next;
                            IRO_free2(depsList);
                            depsList = olist;
                        }
                        if (result) {
                            break;
                        }
                    }
                }
                if (!foundObjRef) {
                    result = TRUE;
                }
                if (result) {
                    break;
                }
            }
            if (!result) {
                for (scan = resultList; scan; scan = scan->nextList) {
                    for (scannd = scan->list.head; scannd != scan->list.tail->next;
                         scannd = scannd->next) {
                        if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                            obj = scannd->u.node->data.objref;
                            depsList = NULL;
                            PointerAnalysis_GetFunctionDependencies(obj, nd, &depsList);
                            for (olist = depsList; olist; olist = olist->next) {
                                var = IRO_FindVar(olist->object, TRUE, TRUE);
                                CError_ASSERT(324, var != NULL);
                                index = var->index;
                                CError_ASSERT(326, index != 0);
                                if (is_volatile_object(olist->object)) {
                                    IRO_IsVolatile = TRUE;
                                    IRO_NotSubable = TRUE;
                                }
                                Bv_SetBit(index, IRO_Depends);
                            }
                            while (depsList) {
                                olist = depsList->next;
                                IRO_free2(depsList);
                                depsList = olist;
                            }
                        }
                    }
                }
            }
            while (resultList) {
                next = resultList->nextList;
                IRO_free2(resultList);
                resultList = next;
            }
        } else {
            result = TRUE;
        }
    } else {
        result = TRUE;
    }
    if (result) {
        func_00569AC0(data_00624DA8, nd);
        Bv_Or(data_00624DA8, IRO_Depends);
    }
}
#if MWCCPSP && !MWCCPS2
#line 694 "IroCSE.c"
#endif

// 0x56ABD0
static void DependsOn(IROLinear* linear, Boolean stop, Boolean recurse) {
    VarRecord* var;
    IROLinear* inner;
    int i;

    if (linear->rtype && CParser_IsVolatile(linear->rtype, linear->nodeflags & ENODE_FLAG_QUALS)) {
        IRO_IsVolatile = TRUE;
        IRO_NotSubable = TRUE;
    }
    if (IRO_NotSubable && stop) {
        return;
    }

    switch (linear->type) {
        case IROLinearOperand:
        case IROLinearScope:
            break;
        case IROLinearOp1Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                IRO_NotSubable = TRUE;
                if (stop) {
                    return;
                }
            }
            inner = linear->u.monadic;
            if (linear->nodetype == EINDIRECT) {
                if (inner->type == IROLinearOp1Arg && inner->nodetype == EBITFIELD) {
                    inner = inner->u.monadic;
                }
                if (inner->type == IROLinearOperand && inner->u.node->type == EOBJREF) {
                    if ((var = IRO_FindVar(inner->u.node->data.objref, FALSE, TRUE))) {
                        if (is_volatile_object(var->object)) {
                            IRO_IsVolatile = TRUE;
                            IRO_NotSubable = TRUE;
                        }
                        Bv_SetBit(var->index, IRO_Depends);
                    } else {
                        IRO_NotSubable = TRUE;
                    }
                } else {
                    GetDependsOfIndirect(linear);
                }
            }
            if (recurse) {
                DependsOn(inner, stop, recurse);
            }
            break;
        case IROLinearOp2Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                IRO_NotSubable = TRUE;
                if (stop) {
                    return;
                }
            }
            if (linear->nodetype == EDIV || linear->nodetype == EMODULO) {
                if (!IRO_IsIntConstant(linear->u.diadic.right)) {
                    IRO_CouldError = TRUE;
                } else if (CInt64_Equal(linear->u.diadic.right->u.node->data.intval, cint64_zero)) {
                    IRO_CouldError = TRUE;
                }
            }
            if (recurse) {
                DependsOn(linear->u.diadic.left, stop, recurse);
                DependsOn(linear->u.diadic.right, stop, recurse);
            }
            break;
        case IROLinearOp3Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                IRO_NotSubable = TRUE;
                if (stop) {
                    return;
                }
            }
            if (recurse) {
                DependsOn(linear->u.args3.a, stop, recurse);
                DependsOn(linear->u.args3.b, stop, recurse);
                DependsOn(linear->u.args3.c, stop, recurse);
            }
            break;
        case IROLinearFunccall:
            IRO_NotSubable = TRUE;
            if (stop) {
                return;
            }
            GetDependsOfFunctionCall(linear);
            if (linear->stmt && IRO_FunctionCallMightThrowException(linear)) {
                func_0054FC80(linear->stmt->dobjstack, DependsOnExceptionObject, NULL);
            }
            if (recurse) {
                DependsOn(linear->u.funccall.callee, FALSE, recurse);
                for (i = linear->u.funccall.argCount - 1; i >= 0; i--) {
                    DependsOn(linear->u.funccall.args[i], FALSE, recurse);
                }
            }
            break;
        case IROLinearAsm:
            if (IRO_GetAsmEffectsHook && linear->u.asm_stmt) {
                IROAsmEffects effects = IRO_EmptyAsmEffects;
                IRO_GetAsmEffectsHook(linear->u.asm_stmt, &effects);
                for (i = 0; i < effects.numoperands; i++) {
                    var = IRO_FindVar(effects.operands[i].object, FALSE, TRUE);
                    switch (effects.operands[i].type) {
                        case 1: // written
                        case 2:
                            IRO_NotSubable = TRUE;
                            if (stop) {
                                return;
                            }
                            Bv_SetBit(var->index, IRO_Depends);
                            break;
                        case 0: // read
                        case 4:
                            Bv_SetBit(var->index, IRO_Depends);
                            break;
                    }
                }
                if (effects.flag2 || effects.flag1) {
                    Bv_Set(IRO_Depends);
                    if (effects.flag1) {
                        IRO_NotSubable = TRUE;
                        if (stop) {
                            return;
                        }
                    }
                }
                if (effects.mayThrow) {
                    IRO_NotSubable = TRUE;
                    if (stop) {
                        return;
                    }
                    func_00569AC0(data_00624DA8, linear);
                    Bv_Or(data_00624DA8, IRO_Depends);
                    if (linear->stmt && IRO_FunctionCallMightThrowException(linear)) {
                        func_0054FC80(linear->stmt->dobjstack, DependsOnExceptionObject, NULL);
                    }
                }
            }
            break;
        default:
            CError_FATAL(629);
    }
}

// 0x56AB70: the variables linear depends on, in IRO_Depends
CSE_NOINLINE void func_0056AB70(IROLinear* linear, Boolean alloc, Boolean stop, Boolean recurse) {
    if (alloc) {
        Bv_AllocVector(&IRO_Depends, IRO_NumVars + 1);
    } else {
        Bv_Clear(IRO_Depends);
    }
    IRO_CouldError = FALSE;
    IRO_NotSubable = FALSE;
    IRO_IsVolatile = FALSE;
    DependsOn(linear, stop, recurse);
}

// 2.4.7's entry points, for the 2.4.7 passes still in use
void IRO_FindDepends_NoAlloc(IROLinear* linear) {
    func_0056AB70(linear, FALSE, FALSE, TRUE);
}

void IRO_FindDepends(IROLinear* linear) {
    func_0056AB70(linear, TRUE, TRUE, TRUE);
}
#endif

static void VecAct(IROLinear* linear, Boolean isFirst) {
    if (!isFirst && (linear->flags & IROLF_VecOpBase)) {
        HasVectorOperand = TRUE;
    }
}

static Boolean IRO_DoesNotHaveVectorOperand(IROLinear* linear) {
    HasVectorOperand = FALSE;
    IRO_WalkTree(linear, VecAct);
    return !HasVectorOperand;
}

#if MWCCPSP
// 0x4C47F0: the back end's vector-operand check (none on the PSP)
CSE_NOINLINE Boolean func_004C47F0(IROLinear* linear) {
    return TRUE;
}

#if MWCCPS2
#line 873 "IroExprs.c" // b210 names this part in its asserts
#endif
// 0x575FE0: subable is the back end's call (IRO_Hook0)
static void IRO_AddExpression(IROLinear* linear, IRONode* node, Boolean unused) {
    IROExpr* expr;

    if ((linear->flags & IROLF_Reffed) &&
        IRO_Hook0 &&
        ((Boolean (*)(IROLinear*))IRO_Hook0)(linear) &&
        func_004C47F0(linear)) {
        expr = IRO_Alloc(sizeof(IROExpr));
        expr->flags = FALSE;
        expr->index = ++IRO_NumExprs;
        expr->linear = linear;
        expr->temp = NULL;
        expr->node = node;
        func_0056AB70(linear, TRUE, TRUE, TRUE);
        expr->depends = IRO_Depends;
        expr->notSubable = IRO_NotSubable;
        expr->couldError = IRO_CouldError;
        expr->next = NULL;
        expr->use = NULL;
        if (IRO_FirstExpr) {
            IRO_LastExpr->next = expr;
        } else {
            IRO_FirstExpr = expr;
        }
        IRO_LastExpr = expr;
        linear->expr = expr;
    }
}

extern Boolean data_00625ECA; // IroLinearForm.c
extern Boolean func_00555190(
    IROLinear* a, IROLinear* b
); // IroSimplifyIfThenElse.c: same semantically

// 3.0.1 CSE: the expressions in classes of equal ones
IROElmList** data_00608CA4; // the classes (IROExpr* elements)
int data_00608CA8; // their count

BitVector* data_0062509C; // the expressions a linear kills
BitVector* data_006258C4; // the classes a linear kills
extern VarRecord* IRO_FindAssigned301(IROLinear* linear, Boolean* isBitfield);
extern void func_00569B40(BitVector* bv, IROLinear* nd); // IroVars.c: what a store may change
extern void func_00569B40(BitVector* bv, IROLinear* nd); // IroVars.c: what a store may change

// the classes reading a variable in bv
static inline void KillClassesOf(BitVector* bv) {
    IROExpr* expr;

    for (expr = IRO_FirstExpr; expr; expr = expr->next) {
        if (Bv_BitsInCommon(bv, expr->depends)) {
            Bv_SetBit(expr->x18, data_006258C4);
        }
    }
}

// the classes reading the variable with this index
#if MWCCPS2
static inline void KillClassesOfVar(UInt32 index) {
#else
static inline void KillClassesOfVar(int index) {
#endif
    IROExpr* expr;

    for (expr = IRO_FirstExpr; expr; expr = expr->next) {
        if (Bv_IsBitSet(index, expr->depends)) {
            Bv_SetBit(expr->x18, data_006258C4);
        }
    }
}

// 0x575BB0: the classes an indirect store kills (through pointer analysis
// when it knows every target)
void func_00575BB0(IROLinear* linear) {
    IROLinear* target = linear->u.monadic;
    IROLinear* inner;
    IROLinear* scannd;
    IROListNode* resultList;
    IROListNode* scan;
    IROListNode* next;
    IROExpr* expr;
    VarRecord* var;
    Object* obj;
    Boolean unknown = FALSE;
    Boolean found;

    if (target &&
        target->type == IROLinearOp1Arg &&
        target->nodetype == EINDIRECT &&
        (inner = target->u.monadic) &&
        copts.opt_pointer_analysis &&
        inner->pointsToFunction &&
        FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupLinearNodePointerExpr(FunctionName, inner, &resultList);
        if (!resultList) {
            unknown = TRUE;
        } else {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail) {
                    unknown = TRUE;
                    break;
                }
                found = FALSE;
                for (scannd = scan->list.head; scannd != scan->list.tail->next;
                     scannd = scannd->next) {
                    if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                        found = TRUE;
                        break;
                    }
                }
                if (!found) {
                    unknown = TRUE;
                    break;
                }
            }
            if (!unknown) {
                for (scan = resultList; scan; scan = scan->nextList) {
                    for (scannd = scan->list.head; scannd != scan->list.tail->next;
                         scannd = scannd->next) {
                        if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                            obj = scannd->u.node->data.objref;
                            if (!obj) {
                                CError_FATAL(425);
                            }
                            var = IRO_FindVar(obj, TRUE, TRUE);
                            if (!var) {
                                CError_FATAL(427);
                            }
#if MWCCPS2
                            KillClassesOfVar(var->index);
#else
                            for (expr = IRO_FirstExpr; expr; expr = expr->next) {
                                if (Bv_IsBitSet(var->index, expr->depends)) {
                                    Bv_SetBit(expr->x18, data_006258C4);
                                }
                            }
#endif
                        }
                    }
                }
            }
            while (resultList) {
                next = resultList->nextList;
                IRO_free2(resultList);
                resultList = next;
            }
        }
    } else {
        unknown = TRUE;
    }
    if (unknown) {
        func_00569B40(data_00624DA8, target);
        KillClassesOf(data_00624DA8);
    }
}

// 0x575AF0: exception action callback, the classes reading an object an
// exception may destroy
void func_00575AF0(Object* obj, void* arg) {
    VarRecord* var = IRO_FindVar(obj, TRUE, TRUE);
    int index;

    if (!var) {
        CError_FATAL(672);
    }
    index = var->index;
    if (!index) {
        CError_FATAL(674);
    }
    KillClassesOfVar(index);
}

// 0x5756B0: the classes a call kills (through pointer analysis when it knows
// every callee's kills)
void func_005756B0(IROLinear* linear) {
    IROListNode* resultList;
    IROListNode* scan;
    IROListNode* next;
    IROLinear* nd;
    IROLinear* inner;
    Boolean result;
    Boolean foundLinear;
    Object* obj;
    ObjectList* killList;
    ObjectList* olist;
    ObjectList* onext;
    VarRecord* var;
    int index;

    inner = linear->u.funccall.callee;
    result = FALSE;
    if (inner && copts.opt_pointer_analysis && inner->pointsToFunction && FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupLinearNodePointerExpr(FunctionName, inner, &resultList);
        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail) {
                    result = TRUE;
                    break;
                }
                foundLinear = FALSE;
                for (nd = scan->list.head; nd != scan->list.tail->next; nd = nd->next) {
                    if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
                        foundLinear = TRUE;
                        obj = nd->u.node->data.objref;
                        if (!obj) {
                            CError_FATAL(736);
                        }
                        killList = NULL;
                        PointerAnalysis_GetFunctionKills(obj, linear, &killList);
                        for (olist = killList; olist; olist = olist->next) {
                            if (!olist->object) {
                                result = TRUE;
                                break;
                            }
                        }
                        while (killList) {
                            onext = killList->next;
                            IRO_free2(killList);
                            killList = onext;
                        }
                        if (result) {
                            break;
                        }
                    }
                }
                if (!foundLinear) {
                    result = TRUE;
                }
                if (result) {
                    break;
                }
            }
            if (!result) {
                for (scan = resultList; scan; scan = scan->nextList) {
                    for (nd = scan->list.head; nd != scan->list.tail->next; nd = nd->next) {
                        if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
                            obj = nd->u.node->data.objref;
                            killList = NULL;
                            PointerAnalysis_GetFunctionKills(obj, linear, &killList);
                            for (olist = killList; olist; olist = olist->next) {
                                var = IRO_FindVar(olist->object, TRUE, TRUE);
                                if (!var) {
                                    CError_FATAL(787);
                                }
                                index = var->index;
                                if (!index) {
                                    CError_FATAL(789);
                                }
                                KillClassesOfVar(index);
                            }
                            while (killList) {
                                onext = killList->next;
                                IRO_free2(killList);
                                killList = onext;
                            }
                        }
                    }
                }
            }
            while (resultList) {
                next = resultList->nextList;
                IRO_free2(resultList);
                resultList = next;
            }
        } else {
            result = TRUE;
        }
    } else {
        result = TRUE;
    }
    if (result) {
        func_00569B40(data_00624DA8, linear);
        KillClassesOf(data_00624DA8);
    }
    if (linear->stmt && IRO_FunctionCallMightThrowException(linear)) {
        func_0054FC80(linear->stmt->dobjstack, func_00575AF0, NULL);
    }
}

// 0x575520: the expression classes linear kills, in data_006258C4 (3.0.1's
// IRO_GetExprKills)
void func_00575520(IROLinear* linear) {
    VarRecord* var;
    IROExpr* expr;
    UInt32 index;

    Bv_Clear(data_006258C4);
    switch (linear->type) {
        case IROLinearOp1Arg:
        case IROLinearOp2Arg:
            if (!IRO_IsAssignOp[linear->nodetype]) {
                break;
            }
            var = IRO_FindAssigned301(linear, NULL);
            index = var ? var->index : 0;
            if (!index) {
                func_00575BB0(linear);
                break;
            }
            for (expr = IRO_FirstExpr; expr; expr = expr->next) {
                if (Bv_IsBitSet(index, expr->depends)) {
                    Bv_SetBit(expr->x18, data_006258C4);
                }
            }
            break;
        case IROLinearFunccall:
            func_005756B0(linear);
            break;
        case IROLinearAsm:
            if (!IRO_GetAsmEffectsHook) {
                break;
            }
            Bv_AllocVector(&IRO_VarKills, IRO_NumVars + 1);
            IRO_GetKills(linear);
            for (expr = IRO_FirstExpr; expr; expr = expr->next) {
                if (Bv_BitsInCommon(IRO_VarKills, expr->depends)) {
                    Bv_SetBit(expr->x18, data_006258C4);
                }
            }
            break;
        default:
            break;
    }
}

// 0x58E790: available expression classes (3.0.1's IRO_ComputeAvail)
CSE_NOINLINE void func_0058E790(void) {
    IRONode* node;
    IROLinear* nd;
    BitVector* bv;
    SInt32 counter = 0;
    UInt16 i;
    Boolean changed;

    Bv_AllocVector(&IRO_VarKills, IRO_NumVars + 1);
    Bv_AllocVector(&data_0062509C, IRO_NumExprs + 1);
    Bv_AllocVector(&data_006258C4, data_00608CA8);
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        Bv_AllocVector(&node->in, data_00608CA8);
        if (node->numpred && node != IRO_FirstNode) {
            Bv_Set(node->in);
        }
        Bv_AllocVector(&node->kill, data_00608CA8);
        Bv_AllocVector(&node->gen, data_00608CA8);
        Bv_AllocVector(&node->out, data_00608CA8);
        for (nd = node->first; nd; nd = nd->next) {
            if (nd->expr) {
                Bv_SetBit(nd->expr->x18, node->gen);
            }
            func_00575520(nd);
            Bv_Or(data_006258C4, node->kill);
            Bv_Minus(data_006258C4, node->gen);
            if (nd == node->last) {
                break;
            }
            if (counter > 250) {
                IRO_CheckForUserBreak();
                counter = 0;
            } else {
                counter++;
            }
        }
        Bv_Copy(node->in, node->out);
        Bv_Minus(node->kill, node->out);
        Bv_Or(node->gen, node->out);
    }
    IRO_CheckForUserBreak();
    Bv_AllocVector(&bv, data_00608CA8);
    do {
        changed = FALSE;
        for (node = IRO_FirstNode; node; node = node->nextnode) {
            if (!node->numpred || node == IRO_FirstNode) {
                Bv_Clear(bv);
            } else {
                Bv_Set(bv);
                for (i = 0; i < node->numpred; i++) {
                    Bv_And(IRO_NodeTable[node->pred[i]]->out, bv);
                }
            }
            if (!Bv_Compare(bv, node->in)) {
                changed = TRUE;
                Bv_Copy(bv, node->in);
            }
            Bv_Copy(node->in, node->out);
            Bv_Minus(node->kill, node->out);
            Bv_Or(node->gen, node->out);
        }
        IRO_CheckForUserBreak();
    } while (changed);
}

// 3.0.1 keeps a CSE state byte where range propagation kept its range
// (IROLinear +0x28): 0 none, 1 recomputed, 2 first computation
#define CSE_STATE(nd) (*(UInt8*)&(nd)->range)

// an earlier computation of the same class (IROExpr.use)
typedef struct IROExprUse {
    IROExpr* expr;
    struct IROExprUse* next;
} IROExprUse;

// 0x58E4D0: within each node, link recomputations of an available class to
// their earlier computation and mark first computations
CSE_NOINLINE void func_0058E4D0(void) {
    IRONode* node;
    IROLinear* nd;
    IROLinear* scan;
    IROLinear* found;
    IROExpr* expr;
    IROExprUse* use;
    BitVector* here;
    BitVector* avail;
    BitVector* first;
    int cls;

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        for (nd = node->first; nd; nd = nd->next) {
            if (nd->expr) {
                nd->expr->use = NULL;
            }
        }
    }
    Bv_AllocVector(&here, data_00608CA8);
    Bv_AllocVector(&avail, data_00608CA8);
    Bv_AllocVector(&first, data_00608CA8);
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        Bv_Clear(here);
        Bv_Copy(node->in, avail);
        Bv_Clear(first);
        for (nd = node->first; nd; nd = nd->next) {
            CSE_STATE(nd) = 0;
            if ((expr = nd->expr)) {
                expr->temp = NULL;
                expr->x20 = FALSE;
                cls = expr->x18;
                if (cls >= data_00608CA8) {
                    expr->use = NULL;
                } else {
                    if (Bv_IsBitSet(CSE_BIT(cls), here)) {
                        found = NULL;
                        for (scan = node->first; scan != nd; scan = scan->next) {
                            if (scan->expr && !scan->expr->use && scan->expr->x18 == cls) {
                                found = scan;
                            }
                        }
                        use = IRO_Alloc(sizeof(IROExprUse));
                        use->expr = found->expr;
                        use->next = NULL;
                        expr->use = (IROExpr*)use;
                        CSE_STATE(nd) = 0;
                    } else {
                        expr->use = NULL;
                        if (Bv_IsBitSet(CSE_BIT(cls), first)) {
                            CSE_STATE(nd) = 1;
                        } else {
                            CSE_STATE(nd) = 2;
                            Bv_SetBit(cls, first);
                            if (Bv_IsBitSet(CSE_BIT(cls), avail)) {
                                expr->x20 = TRUE;
                            }
                        }
                    }
                    Bv_SetBit(cls, here);
                    Bv_SetBit(cls, avail);
                }
            }
            func_00575520(nd);
            Bv_Minus(data_006258C4, here);
            Bv_Minus(data_006258C4, avail);
            if (nd == node->last) {
                break;
            }
        }
    }
}

static inline IROExprUse* AppendUses(IROExprUse* list, IROExprUse* more) {
    IROExprUse* tail;

    if (list) {
        for (tail = list; tail->next; tail = tail->next) {}
        tail->next = more;
        return list;
    }
    return more;
}

static inline IROExprUse* NewUse(IROExpr* expr) {
    IROExprUse* use = IRO_Alloc(sizeof(IROExprUse));

    use->expr = expr;
    use->next = NULL;
    return use;
}

// 0x58E350: the computations of expr's class that reach the end of node
// (searching back through the predecessors)
CSE_NOINLINE IROExprUse* func_0058E350(IRONode* node, IROExpr* expr) {
    IROLinear* nd;
    IROLinear* found;
    IROExpr* e;
    IROExprUse* list;
    IROExprUse* more;
    UInt32 cls;
#if MWCCPS2
    int i;
#else
    UInt16 i;
#endif

    if (node->marked) {
        return NULL;
    }
    node->marked = TRUE;
    cls = expr->x18;
    found = NULL;
    for (nd = node->first; nd; nd = nd->next) {
        if ((e = nd->expr)) {
            if (CSE_STATE(nd) && cls == e->x18) {
                found = nd;
            }
            if (e == expr) {
                found = NULL;
            }
        }
        if (nd == node->last) {
            break;
        }
    }
    if (found) {
        if (CSE_STATE(found) == 1) {
            return NewUse(found->expr);
        }
        if (!found->expr->x20) {
            return NewUse(found->expr);
        }
    }
    list = NULL;
    for (i = 0; i < node->numpred; i++) {
        if ((more = func_0058E350(IRO_NodeTable[node->pred[i]], expr))) {
            list = AppendUses(list, more);
        }
    }
    return list;
}

// 0x58E240: a first computation available on entry gets the computations
// reaching it from the predecessors (IROExpr.use)
CSE_NOINLINE void func_0058E240(void) {
    IRONode* node;
    IRONode* scan;
    IROLinear* nd;
    IROExpr* expr;
    IROExprUse* list;
    IROExprUse* more;
    int i;

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        for (nd = node->first; nd; nd = nd->next) {
            if ((expr = nd->expr) &&
                CSE_STATE(nd) == 2 &&
                Bv_IsBitSet(expr->x18, node->in) &&
                expr->x20) {
                list = NULL;
                for (i = 0; i < node->numpred; i++) {
                    for (scan = IRO_FirstNode; scan; scan = scan->nextnode) {
                        scan->marked = FALSE;
                    }
                    if ((more = func_0058E350(IRO_NodeTable[node->pred[i]], nd->expr))) {
                        list = AppendUses(list, more);
                    }
                }
                nd->expr->use = (IROExpr*)list;
            }
            if (nd == node->last) {
                break;
            }
        }
    }
}

extern void func_00552150(IROLinear* nd, Object* obj, IROLinear* after); // IroUtil.c
extern IROLinear* func_00551ED0(Object* temp, IROLinear* value, IROList* list); // IroUtil.c
extern void func_00551FC0(IROLinear* nd, IROLinear* replacement); // IroUtil.c

// 0x58E150: the computations a recomputation may come from share one
// temporary
CSE_NOINLINE void func_0058E150(void) {
    IRONode* node;
    IROLinear* nd;
    IROExprUse* uses;
    IROExprUse* use;
    Object* temp;
    Object* t2;
    Boolean changed;

    do {
        changed = FALSE;
        for (node = IRO_FirstNode; node; node = node->nextnode) {
            for (nd = node->first; nd; nd = nd->next) {
                if (nd->expr && (uses = (IROExprUse*)nd->expr->use)) {
                    temp = uses->expr->temp;
                    for (use = uses->next; use; use = use->next) {
                        t2 = use->expr->temp;
                        if (temp == t2) {
                            continue;
                        }
                        if (!temp) {
                            temp = t2;
#if MWCCPS2
                        } else if (t2 && (SInt32)temp >= (SInt32)t2) { // b210 compares them signed
#else
                        } else if (t2 && (char*)temp >= (char*)t2) {
#endif
                            temp = t2;
                        }
                        changed = TRUE;
                    }
                    if (!temp) {
                        temp = create_temp_object(uses->expr->linear->rtype);
                        IRO_FindVar(temp, TRUE, TRUE);
                    }
                    for (use = uses; use; use = use->next) {
                        use->expr->temp = temp;
                    }
                }
                if (nd == node->last) {
                    break;
                }
            }
        }
    } while (changed);
}

// 0x58DFE0: computations with a temporary assign it; recomputations read it
CSE_NOINLINE void func_0058DFE0(void) {
    IRONode* node;
    IROLinear* nd;
    IROLinear* insertAt;
    IROLinear* ass;
    IROExpr* expr;
    IROExpr* from;
    IROList list;
    IROList copy;
    IROList left;

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        insertAt = node->first;
#if MWCCPS2
        for (nd = node->first;; nd = nd->next) {
#else
        for (nd = node->first; nd; nd = nd->next) {
#endif
            if ((expr = nd->expr)) {
                if (!expr->temp) {
                    if (expr->use) {
                        from = ((IROExprUse*)expr->use)->expr;
                        func_00552150(nd, from->temp, nd);
                        IRO_RemoveExpr(nd->expr);
                        IRO_NopOut(nd);
                        IRO_Dump("Replacing common sub at %d\n", nd->index);
                    }
                } else {
                    IRO_InitList(&list);
                    ass = func_00551ED0(expr->temp, expr->linear, &list);
                    func_00551FC0(expr->linear, ass);
                    IRO_PasteAfter(list.head, list.tail, expr->linear);
                    if (ass->flags & IROLF_Reffed) {
                        IRO_InitList(&copy);
                        IRO_DuplicateExpr(ass, &copy);
                        copy.tail->flags &= ~IROLF_Reffed;
                        IRO_Paste(copy.head, copy.tail, insertAt);
                        IRO_InitList(&left);
                        IRO_DuplicateExpr(ass->u.diadic.left, &left);
                        IRO_PasteAfter(left.head, left.tail, ass);
                        IRO_LocateFather_Cut_And_Paste(ass, left.tail);
                    }
                    IRO_Dump("Replacing common sub at %d\n", nd->index);
                }
            }
            if (!(nd->flags & IROLF_Reffed) && (nd->type != IROLinearNop || nd == insertAt)) {
                insertAt = nd->next;
            }
            if (nd == node->last) {
                break;
            }
        }
    }
}

// 0x58EA00: put each expression in its class (IROExpr.x18 = class)
CSE_NOINLINE void func_0058EA00(void) {
    IRONode* node;
    IROLinear* nd;
    IROExpr* expr;
    IROElmList* elm;
    Boolean found;
    int i;

    data_00608CA8 = 0;
    data_00608CA4 = IRO_Alloc(IRO_NumExprs * sizeof(IROElmList*));
    memset(data_00608CA4, 0, IRO_NumExprs * sizeof(IROElmList*));
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        for (nd = node->first; nd; nd = nd->next) {
            if (nd->expr) {
                found = FALSE;
                for (i = 0; i < data_00608CA8; i++) {
                    if (data_00608CA4[i] &&
                        func_00555190(
                            nd->expr->linear, ((IROExpr*)data_00608CA4[i]->element)->linear
                        )) {
                        expr = nd->expr;
                        elm = IRO_Alloc(sizeof(IROElmList));
                        elm->element = NULL;
                        elm->next = NULL;
                        elm->element = (IROLinear*)expr;
                        elm->next = data_00608CA4[i];
                        data_00608CA4[i] = elm;
                        expr->x18 = i;
                        found = TRUE;
                        break;
                    }
                }
                if (!found) {
                    expr = nd->expr;
                    i = data_00608CA8;
                    elm = IRO_Alloc(sizeof(IROElmList));
                    elm->element = NULL;
                    elm->next = NULL;
                    elm->element = (IROLinear*)expr;
                    elm->next = data_00608CA4[i];
                    data_00608CA4[i] = elm;
                    expr->x18 = i;
                    data_00608CA8++;
                }
            }
            if (nd == node->last) {
                break;
            }
        }
    }
}

// 0x58EB60: the back end may want a variable loaded once at the start: the
// first load of each such variable is copied to the function's start
// (IRO_Hook14)
void func_0058EB60(void) {
    BitVector* seen;
    IROLinear* nd;
    IROLinear* copy;
    VarRecord* var;

    if (!IRO_Hook0 || !IRO_Hook14) {
        return;
    }
    Bv_AllocVector(&seen, IRO_NumVars + 1);
    for (nd = IRO_FirstLinear; nd; nd = nd->next) {
        nd->expr = NULL;
        if ((nd->flags & IROLF_Reffed) &&
            nd->type >= IROLinearOperand &&
            nd->type <= IROLinearFunccall &&
            ((Boolean (*)(IROLinear*))IRO_Hook0)(nd) &&
            func_004C47F0(nd) &&
            nd->type == IROLinearOperand &&
            nd->u.node &&
            nd->u.node->type == EOBJREF &&
            (var = IRO_FindVar(nd->u.node->data.objref, FALSE, TRUE)) &&
            !Bv_IsBitSet(CSE_BIT(var->index), seen) &&
            ((Boolean (*)(IROLinear*, VarRecord*, Boolean))IRO_Hook14)(nd, var, data_00625ECA)) {
            copy = IRO_NewLinear(IROLinearNop);
            *copy = *nd;
            copy->index = ++IRO_NumLinear;
            if (IRO_FirstLinear && IRO_FirstLinear->type == IROLinearNop) {
                IRO_PasteAfter(copy, copy, IRO_FirstLinear);
            } else {
                IRO_Paste(copy, copy, IRO_FirstLinear);
            }
            Bv_SetBit(var->index, seen);
        }
    }
    if (seen) {
        IRO_free(seen);
        seen = NULL;
    }
}

// 0x575F10: only expression linears get records
void IRO_FindExpressions(BitVector* bv, Boolean unused) {
    IROLinear* nd;
    IRONode* fnode;

    IRO_LastExpr = NULL;
    IRO_FirstExpr = IRO_LastExpr;
    IRO_NumExprs = 0;
    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (!bv || Bv_IsBitSet(CSE_BIT(fnode->index), bv)) {
            for (nd = fnode->first; nd; nd = nd->next) {
                nd->expr = NULL;
                if (nd->type >= IROLinearOperand && nd->type <= IROLinearFunccall) {
                    IRO_AddExpression(nd, fnode, unused);
                }
                if (nd == fnode->last) {
                    break;
                }
            }
        } else {
            for (nd = fnode->first; nd; nd = nd->next) {
                nd->expr = NULL;
                if (nd == fnode->last) {
                    break;
                }
            }
        }
    }
    IRO_CheckForUserBreak();
}

// 0x575EB0
#if MWCCPSP && !MWCCPS2 // b210: IroExprs.c from 873 on
#line 1663 "IroExprs.c"
#endif
CSE_NOINLINE void IRO_RemoveExpr(IROExpr* expr) {
    IROExpr* prev;
    IROExpr* scan;

    scan = IRO_FirstExpr;
    prev = NULL;
    while (scan != expr) {
        prev = scan;
        scan = scan->next;
        if (!scan) {
            CError_FATAL(212);
        }
    }
    expr->linear->expr = NULL;
    if (prev) {
        prev->next = expr->next;
    } else {
        IRO_FirstExpr = expr->next;
    }
}
#if MWCCPS2
#line 1685 "IroCSE.c"
#elif MWCCPSP
#line 1686 "IroCSE.c"
#endif
#else
static void IRO_AddExpression(IROLinear* linear, IRONode* node, Boolean unused) {
    IROExpr* expr;

    if ((linear->flags & IROLF_Reffed) &&
        IRO_IsSubableExpression(linear) &&
        IRO_DoesNotHaveVectorOperand(linear)) {
        expr = oalloc(sizeof(IROExpr));
        expr->flags = FALSE;
        expr->index = ++IRO_NumExprs;
        expr->linear = linear;
        expr->temp = NULL;
        expr->node = node;
        IRO_FindDepends(linear);
        expr->depends = IRO_Depends;
        expr->notSubable = IRO_NotSubable;
        expr->couldError = IRO_CouldError;
        expr->next = NULL;
        expr->use = NULL;
        if (IRO_FirstExpr) {
            IRO_LastExpr->next = expr;
        } else {
            IRO_FirstExpr = expr;
        }
        IRO_LastExpr = expr;
        linear->expr = expr;
    }
}

void IRO_FindExpressions(BitVector* bv, Boolean unused) {
    IROLinear* nd;
    IRONode* fnode;

    IRO_FirstExpr = IRO_LastExpr = NULL;
    IRO_NumExprs = 0;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (!bv || Bv_IsBitSet(fnode->index, bv)) {
            for (nd = fnode->first; nd; nd = nd->next) {
                nd->expr = NULL;
                IRO_AddExpression(nd, fnode, unused);
                if (nd == fnode->last) {
                    break;
                }
            }
        } else {
            for (nd = fnode->first; nd; nd = nd->next) {
                nd->expr = NULL;
                if (nd == fnode->last) {
                    break;
                }
            }
        }
    }

    IRO_CheckForUserBreak();
}

void IRO_RemoveExpr(IROExpr* expr) {
    IROExpr* prev;
    IROExpr* scan;

    scan = IRO_FirstExpr;
    prev = NULL;
    while (scan != expr) {
        prev = scan;
        scan = scan->next;
        CError_ASSERT(809, scan);
    }

    expr->linear->expr = NULL;
    if (prev) {
        prev->next = expr->next;
    } else {
        IRO_FirstExpr = expr->next;
    }
}
#endif

static void GetExprKillsByIndirectAssignment(IROLinear* linear) {
    int index;
    IROListNode* scan;
    IROLinear* inner;
    VarRecord* var;
    Boolean foundObjRef;
    IROListNode* resultList;
    IROLinear* scannd;
    Boolean result;
    IROListNode* next;
    Object* obj;
    IROExpr* expr;
    IROListNode* list;

    result = FALSE;
    if (linear->type == IROLinearOp2Arg) {
        linear = linear->u.diadic.left;
    } else {
        linear = linear->u.monadic;
    }

    if (linear &&
        linear->type == IROLinearOp1Arg &&
        linear->nodetype == EINDIRECT &&
        (inner = linear->u.monadic) &&
        copts.opt_pointer_analysis &&
        inner->pointsToFunction &&
        FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupLinearNodePointerExpr(FunctionName, inner, &resultList);
        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail) {
                    result = TRUE;
                    break;
                }

                foundObjRef = FALSE;
                for (scannd = scan->list.head; scannd != scan->list.tail->next;
                     scannd = scannd->next) {
                    if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                        foundObjRef = TRUE;
                        break;
                    }
                }

                if (!foundObjRef) {
                    result = TRUE;
                    break;
                }
            }

            if (!result) {
                for (list = resultList; list; list = list->nextList) {
                    for (scannd = list->list.head; scannd != list->list.tail->next;
                         scannd = scannd->next) {
                        if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                            obj = scannd->u.node->data.objref;
                            CError_ASSERT(893, obj != NULL);
                            var = IRO_FindVar(obj, TRUE, TRUE);
                            CError_ASSERT(895, var != NULL);
                            index = var->index;

                            for (expr = IRO_FirstExpr; expr; expr = expr->next) {
                                if (Bv_IsBitSet(index, expr->depends)) {
                                    Bv_SetBit(expr->index, IRO_ExprKills);
                                }
                            }
                        }
                    }
                }
            }

            while (resultList) {
                next = resultList->nextList;
                IRO_free(resultList);
                resultList = next;
            }
        } else {
            result = TRUE;
        }
    } else {
        result = TRUE;
    }

    if (result) {
        for (expr = IRO_FirstExpr; expr; expr = expr->next) {
            if (Bv_BitsInCommon(expr->depends, IRO_FuncKills)) {
                Bv_SetBit(expr->index, IRO_ExprKills);
            }
        }
    }
}

static void GetExprKillsByFunctionCall(IROLinear* funccall) {
    IROListNode* list;
    VarRecord* var;
    IROListNode* resultList;
    IROLinear* innernd;
    ObjectList* depsList;
    IROListNode* scan;
    ObjectList* olist;
    IROExpr* expr;
    Boolean foundObjRef;
    Boolean result;
    IROLinear* scannd;
    int index;
    Object* obj;
    IROListNode* next;

    result = FALSE;
    innernd = funccall->u.funccall.callee;

    if (innernd && copts.opt_pointer_analysis && innernd->pointsToFunction && FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupLinearNodePointerExpr(FunctionName, innernd, &resultList);
        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail) {
                    result = TRUE;
                    break;
                }

                foundObjRef = FALSE;
                for (scannd = scan->list.head; scannd != scan->list.tail->next;
                     scannd = scannd->next) {
                    if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                        foundObjRef = TRUE;
                        obj = scannd->u.node->data.objref;
                        CError_ASSERT(991, obj != NULL);

                        depsList = NULL;
                        PointerAnalysis_GetFunctionKills(obj, funccall, &depsList);

                        for (olist = depsList; olist; olist = olist->next) {
                            if (!olist->object) {
                                result = TRUE;
                                break;
                            }
                        }

                        while (depsList) {
                            olist = depsList->next;
                            IRO_free(depsList);
                            depsList = olist;
                        }

                        if (result) {
                            break;
                        }
                    }
                }

                if (!foundObjRef) {
                    result = TRUE;
                }
                if (result) {
                    break;
                }
            }

            if (!result) {
                for (list = resultList; list; list = list->nextList) {
                    for (scannd = list->list.head; scannd != list->list.tail->next;
                         scannd = scannd->next) {
                        if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                            obj = scannd->u.node->data.objref;

                            depsList = NULL;
                            PointerAnalysis_GetFunctionKills(obj, funccall, &depsList);

                            for (olist = depsList; olist; olist = olist->next) {
                                var = IRO_FindVar(olist->object, TRUE, TRUE);
                                CError_ASSERT(1042, var != NULL);
                                index = var->index;
                                CError_ASSERT(1044, index != 0);

                                for (expr = IRO_FirstExpr; expr; expr = expr->next) {
                                    if (Bv_IsBitSet(index, expr->depends)) {
                                        Bv_SetBit(expr->index, IRO_ExprKills);
                                    }
                                }
                            }

                            while (depsList) {
                                olist = depsList->next;
                                IRO_free(depsList);
                                depsList = olist;
                            }
                        }
                    }
                }
            }

            while (resultList) {
                next = resultList->nextList;
                IRO_free(resultList);
                resultList = next;
            }
        } else {
            result = TRUE;
        }
    } else {
        result = TRUE;
    }

    if (result) {
        for (expr = IRO_FirstExpr; expr; expr = expr->next) {
            if (Bv_BitsInCommon(expr->depends, IRO_FuncKills)) {
                Bv_SetBit(expr->index, IRO_ExprKills);
            }
        }
    }
}

static void IRO_GetExprKills(IROLinear* linear) {
    VarRecord* var;
    int index;
    IROExpr* expr;

    Bv_Clear(IRO_ExprKills);
    switch (linear->type) {
        case IROLinearOp1Arg:
        case IROLinearOp2Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                var = IRO_FindAssigned(linear);
                index = 0;
                if (var) {
                    index = var->index;
                }
                if (!index) {
                    GetExprKillsByIndirectAssignment(linear);
                } else {
                    for (expr = IRO_FirstExpr; expr; expr = expr->next) {
                        if (Bv_IsBitSet(index, expr->depends)) {
                            Bv_SetBit(expr->index, IRO_ExprKills);
                        }
                    }
                }
            }
            break;
        case IROLinearAsm:
            IRO_GetKills(linear);
            for (expr = IRO_FirstExpr; expr; expr = expr->next) {
                if (Bv_BitsInCommon(expr->depends, IRO_VarKills)) {
                    Bv_SetBit(expr->index, IRO_ExprKills);
                }
            }
            break;
        case IROLinearFunccall:
            GetExprKillsByFunctionCall(linear);
            break;
        default:
            break;
    }
}

void IRO_ComputeAvail(void) {
    IRONode* node;
    IROLinear* linear;
    SInt32 counter;
    BitVector* bv;
    Boolean changed;

    counter = 0;
    node = IRO_FirstNode;
    Bv_AllocVector(&IRO_VarKills, IRO_NumVars + 1);
    Bv_AllocVector(&IRO_ExprKills, IRO_NumExprs + 1);

    while (node) {
        Bv_AllocVector(&node->in, IRO_NumExprs);
        if (node->numpred) {
            Bv_Set(node->in);
        }
        Bv_AllocVector(&node->kill, IRO_NumExprs);
        Bv_AllocVector(&node->gen, IRO_NumExprs);
        Bv_AllocVector(&node->out, IRO_NumExprs);

        for (linear = node->first; linear; linear = linear->next) {
            if (linear->expr) {
                Bv_SetBit(linear->expr->index, node->gen);
            }
            IRO_GetExprKills(linear);
            Bv_Or(IRO_ExprKills, node->kill);
            Bv_Minus(IRO_ExprKills, node->gen);
            if (linear == node->last) {
                break;
            }

            if (counter > 250) {
                IRO_CheckForUserBreak();
                counter = 0;
            } else {
                counter++;
            }
        }

        Bv_Copy(node->in, node->out);
        Bv_Minus(node->kill, node->out);
        Bv_Or(node->gen, node->out);
        node = node->nextnode;
    }

    IRO_CheckForUserBreak();

    Bv_AllocVector(&bv, IRO_NumExprs);
    do {
        IRONode* scan;
        changed = FALSE;
        for (scan = IRO_FirstNode; scan; scan = scan->nextnode) {
            if (!scan->numpred) {
                Bv_Clear(bv);
            } else {
                UInt16 i;
                Bv_Set(bv);
                for (i = 0; i < scan->numpred; i++) {
                    Bv_And(IRO_NodeTable[scan->pred[i]]->out, bv);
                }
            }

            if (!Bv_Compare(bv, scan->in)) {
                changed = TRUE;
                Bv_Copy(bv, scan->in);
            }

            Bv_Copy(scan->in, scan->out);
            Bv_Minus(scan->kill, scan->out);
            Bv_Or(scan->gen, scan->out);
        }
        IRO_CheckForUserBreak();
    } while (changed);
}

static void IRO_MakeReplacementEmbedded(IROExpr* expr) {
    IROLinear* opnd;
    IROLinear* ind;
    IROLinear* ass;

    IRO_GetTemp(expr);

    opnd = IRO_NewLinear(IROLinearOperand);
    opnd->u.node = create_objectrefnode(expr->temp);
    opnd->rtype = opnd->u.node->data.objref->type;
    opnd->index = ++IRO_NumLinear;
    opnd->flags |= IROLF_Reffed | IROLF_Assigned | IROLF_Ind;

    ind = IRO_NewLinear(IROLinearOp1Arg);
    ind->nodetype = EINDIRECT;
    ind->rtype = expr->linear->rtype;
    ind->u.monadic = opnd;
    ind->index = ++IRO_NumLinear;
    ind->flags |= IROLF_Reffed | IROLF_Assigned;

    ass = IRO_NewLinear(IROLinearOp2Arg);
    ass->nodetype = EASS;
    ass->u.diadic.left = ind;
    ass->u.diadic.right = expr->linear;
    ass->rtype = expr->linear->rtype;
    ass->index = ++IRO_NumLinear;

    opnd->next = ind;
    ind->next = ass;
    IRO_ReplaceReferenceWithNode(expr->linear, ass);
    IRO_PasteAfter(opnd, ass, expr->linear);
}

static void IRO_ActUnmarkSubExpressions(IROLinear* linear, Boolean isFirst) {
    if (isFirst) {
        linear->flags &= ~IROLF_ReplaceCSE;
    }
}

static void CheckCommonSub(IROLinear* linear) {
    IROExpr* expr;

    for (expr = IRO_FirstExpr; expr; expr = expr->next) {
        if (expr->linear != linear && !expr->use) {
            if (Bv_IsBitSet(expr->index, IRO_Avail) &&
                !expr->notSubable &&
                IRO_ExprsSame(linear, expr->linear)) {
                IRO_WalkTree(linear, IRO_ActUnmarkSubExpressions);
                linear->flags |= IROLF_ReplaceCSE;
                linear->expr->use = expr;
                break;
            }
        }
    }
}

static void MoveCommonSub(IROExpr* expr) {
    IROExpr* scanexpr;
    SInt32 sz1;
    SInt32 sz2;
    SInt32 i1;
    SInt32 i2;
    IROLinear* scan;
    IROLinear* array1[64];
    IROLinear* array2[64];
    SInt32 best;

    sz1 = 0;
    scan = expr->linear;
    do {
        scan = IRO_LocateFather(scan);
        if (scan) {
            if (sz1 == 64) {
                return;
            }
            array1[sz1++] = scan;
        }
    } while (scan);

    best = -1;
    for (scanexpr = IRO_FirstExpr; scanexpr; scanexpr = scanexpr->next) {
        if (scanexpr->use == expr) {
            scan = scanexpr->linear;
            sz2 = 0;
            do {
                scan = IRO_LocateFather(scan);
                if (scan) {
                    if (sz2 == 64) {
                        return;
                    }
                    array2[sz2++] = scan;
                }
            } while (scan);

            i1 = sz1;
            i2 = sz2;
            while (i1 && i2 && array1[i1 - 1] == array2[i2 - 1]) {
                i1--;
                i2--;
            }

            if (i1 != sz1 && i1 > best) {
                best = i1;
            }
        }
    }

    if (best < 0) {
        IRO_MakeReplacementEmbedded(expr);
    } else {
        IROLinear* start;
        IROLinear* comma;
        IRO_Dump(
            "Moving common sub from node %d to %d\n", expr->linear->index, array1[best]->index
        );
        start = IRO_FindStart(array1[best]);
        IRO_GetTemp(expr);
        IRO_ReplaceReference(expr->linear, expr->temp, expr->linear);
        IRO_MoveExpression(expr, start);

        comma = IRO_NewLinear(IROLinearOp2Arg);
        comma->nodetype = ECOMMA;
        comma->rtype = array1[best]->rtype;
        comma->u.diadic.left = IRO_AssignToTemp(expr);
        comma->u.diadic.right = array1[best];
        comma->stmt = array1[best]->stmt;
        IRO_ReplaceReferenceWithNode(array1[best], comma);
        IRO_PasteAfter(comma, comma, array1[best]);
    }
}

static void ReplaceCommonSub(IROLinear* linear) {
    IROExpr* expr = linear->expr->use;
    if (!expr->temp) {
        MoveCommonSub(expr);
        if (!expr->temp) {
            return;
        }
    }

    IRO_Dump("Replacing common sub at %d with %d\n", linear->index, expr->linear->index);
    IRO_ReplaceReference(linear, expr->temp, linear);
    IRO_RemoveExpr(linear->expr);
    IRO_NopOut(linear);
}

#if MWCCPSP
// 0x58DF80: 3.0.1's common subexpression elimination by expression class
void IRO_CommonSubs(void) {
    IROExpr* expr;
    IROExpr* next;

    IRO_FindExpressions(NULL, FALSE);
    for (expr = IRO_FirstExpr; expr; expr = next) {
        next = expr->next;
        if (expr->notSubable) {
            IRO_RemoveExpr(expr);
        }
    }
    func_0058EA00();
    func_0058E790();
    func_0058E4D0();
    func_0058E240();
    func_0058E150();
    func_0058DFE0();
    IRO_UpdateFlagsOnInts(IRO_FirstLinear);
}
#else
void IRO_CommonSubs(void) {
    IRONode* node;
    IROLinear* linear;
    SInt32 counter;

    counter = 0;
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        IRO_Avail = node->in;
        linear = node->first;
        while (1) {
            if (!linear) {
                break;
            }
            if (linear->expr && !linear->expr->notSubable) {
                CheckCommonSub(linear);
            }
            if (linear->expr) {
                Bv_SetBit(linear->expr->index, IRO_Avail);
            }
            IRO_GetExprKills(linear);
            Bv_Minus(IRO_ExprKills, IRO_Avail);
            if (linear == node->last) {
                break;
            }
            if (counter > 250) {
                IRO_CheckForUserBreak();
                counter = 0;
            } else {
                counter++;
            }
            linear = linear->next;
        }
    }

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        for (linear = node->first; linear; linear = linear->next) {
            if (linear->expr && (linear->flags & IROLF_ReplaceCSE) && !IRO_HasSideEffect(linear)) {
                ReplaceCommonSub(linear);
            }
            if (linear == node->last) {
                break;
            }
            if (counter > 250) {
                IRO_CheckForUserBreak();
                counter = 0;
            } else {
                counter++;
            }
        }
    }

    IRO_CheckForUserBreak();
}
#endif

static Boolean CountThisSubableOperandUse(IROUse* use) {
    return use->reachingDefCount != 0;
}

static int GetSubableOperandUseCount(VarRecord* var) {
    int count = 0;
    IROUse* use;

    if (var->uses) {
        for (use = var->uses; use; use = use->varnext) {
            if (CountThisSubableOperandUse(use)) {
                count++;
            }
        }
    }

    return count;
}

static void IRO_MakeTopLevelExprForSubableOperand(IROLinear* linear) {
    IROLinear* copy = IRO_NewLinear(IROLinearOperand);
    memcpy(copy, linear, sizeof(IROLinear));
    copy->index = ++IRO_NumLinear;

    if (IRO_FirstLinear && IRO_FirstLinear->type == IROLinearNop) {
        IRO_PasteAfter(copy, copy, IRO_FirstLinear);
    } else {
        IRO_Paste(copy, copy, IRO_FirstLinear);
    }
}

void IRO_GenerateTopLevelExprsForSubableOperands(void) {
    IROLinear* nd;
    IRONode* fnode;
    VarRecord* var;
    BitVector* bv;

    Bv_AllocVector(&bv, IRO_NumVars + 1);

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        for (nd = fnode->first; nd; nd = nd->next) {
            nd->expr = NULL;
            if ((nd->flags & IROLF_Reffed) &&
                IRO_IsSubableExpression(nd) &&
                IRO_DoesNotHaveVectorOperand(nd)) {
                if (nd->type == IROLinearOperand && nd->u.node && nd->u.node->type == EOBJREF) {
                    if ((var = IRO_FindVar(nd->u.node->data.objref, FALSE, TRUE))) {
                        if (!Bv_IsBitSet(var->index, bv)) {
                            IRO_MakeTopLevelExprForSubableOperand(nd);
                            Bv_SetBit(var->index, bv);
                        }
                    }
                }
            }

            if (nd == fnode->last) {
                break;
            }
        }
    }
}
