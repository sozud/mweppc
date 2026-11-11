#if MWCCPSP
#include "compiler/CScope.h"
#include <string.h>
#include <stdlib.h>
#endif
#include "compiler/IroEval.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroMalloc.h"
#include "compiler/IroPointerAnalysis.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"
#include "compiler/CInt64.h"
#include "compiler/CMachine.h"
#include "compiler/CompilerTools.h"
#include "compiler/CParser.h"
#include "compiler/enode.h"
#include "compiler/objects.h"
#include "compiler/Switch.h"
#include "compiler/tokens.h"
#include "compiler/types.h"

#if MWCCPSP // 3.0.1's IRO_ExprsSameSemantically (IroSimplifyIfThenElse.c) reads it
Boolean IsAssociativeENodeType[IRO_MAXEXPR];
#else
static Boolean IsAssociativeENodeType[MAXEXPR];
#endif

#if MWCCPSP // 3.0.1 entries, generated from the original's stores
void IRO_InitializeIsAssociativeENodeTypeArray(void) {
    int i;

    for (i = 0; i < IRO_MAXEXPR; i++) {
        IsAssociativeENodeType[i] = FALSE;
    }

    IsAssociativeENodeType[EPOSTINC] = FALSE;
    IsAssociativeENodeType[EPOSTDEC] = FALSE;
    IsAssociativeENodeType[EPREINC] = FALSE;
    IsAssociativeENodeType[EPREDEC] = FALSE;
    IsAssociativeENodeType[EINDIRECT] = FALSE;
    IsAssociativeENodeType[EMONMIN] = FALSE;
    IsAssociativeENodeType[EBINNOT] = FALSE;
    IsAssociativeENodeType[ELOGNOT] = FALSE;
    IsAssociativeENodeType[EFORCELOAD] = FALSE;
    IsAssociativeENodeType[EMUL] = TRUE;
    IsAssociativeENodeType[EMULV] = TRUE;
    IsAssociativeENodeType[EDIV] = FALSE;
    IsAssociativeENodeType[EMODULO] = FALSE;
    IsAssociativeENodeType[EADDV] = TRUE;
    IsAssociativeENodeType[ESUBV] = FALSE;
    IsAssociativeENodeType[EADD] = TRUE;
    IsAssociativeENodeType[ESUB] = FALSE;
    IsAssociativeENodeType[ESHL] = FALSE;
    IsAssociativeENodeType[ESHR] = FALSE;
    IsAssociativeENodeType[ELESS] = FALSE;
    IsAssociativeENodeType[EGREATER] = FALSE;
    IsAssociativeENodeType[ELESSEQU] = FALSE;
    IsAssociativeENodeType[EGREATEREQU] = FALSE;
    IsAssociativeENodeType[EEQU] = FALSE;
    IsAssociativeENodeType[ENOTEQU] = FALSE;
    IsAssociativeENodeType[EAND] = TRUE;
    IsAssociativeENodeType[EXOR] = TRUE;
    IsAssociativeENodeType[EOR] = TRUE;
    IsAssociativeENodeType[ELAND] = FALSE;
    IsAssociativeENodeType[ELOR] = FALSE;
    IsAssociativeENodeType[EASS] = FALSE;
    IsAssociativeENodeType[EMULASS] = FALSE;
    IsAssociativeENodeType[EDIVASS] = FALSE;
    IsAssociativeENodeType[EMODASS] = FALSE;
    IsAssociativeENodeType[EADDASS] = FALSE;
    IsAssociativeENodeType[ESUBASS] = FALSE;
    IsAssociativeENodeType[ESHLASS] = FALSE;
    IsAssociativeENodeType[ESHRASS] = FALSE;
    IsAssociativeENodeType[EANDASS] = FALSE;
    IsAssociativeENodeType[EXORASS] = FALSE;
    IsAssociativeENodeType[EORASS] = FALSE;
    IsAssociativeENodeType[ECOMMA] = FALSE;
    IsAssociativeENodeType[EMIN] = TRUE;
    IsAssociativeENodeType[EMAX] = TRUE;
    IsAssociativeENodeType[EPMODULO] = FALSE;
    IsAssociativeENodeType[EROTL] = FALSE;
    IsAssociativeENodeType[EROTR] = FALSE;
    IsAssociativeENodeType[EBCLR] = FALSE;
    IsAssociativeENodeType[EBTST] = FALSE;
    IsAssociativeENodeType[EBSET] = FALSE;
    IsAssociativeENodeType[ETYPCON] = FALSE;
    IsAssociativeENodeType[EBITFIELD] = FALSE;
    IsAssociativeENodeType[EINTCONST] = FALSE;
    IsAssociativeENodeType[EFLOATCONST] = FALSE;
    IsAssociativeENodeType[ESTRINGCONST] = FALSE;
    IsAssociativeENodeType[ECOND] = FALSE;
    IsAssociativeENodeType[EFUNCCALL] = FALSE;
    IsAssociativeENodeType[EFUNCCALLP] = FALSE;
    IsAssociativeENodeType[EOBJREF] = FALSE;
    IsAssociativeENodeType[ENULLCHECK] = FALSE;
    IsAssociativeENodeType[EPRECOMP] = FALSE;
    IsAssociativeENodeType[ELABEL] = FALSE;
    IsAssociativeENodeType[ESCOPEBEGIN] = FALSE;
    IsAssociativeENodeType[ESCOPEEND] = FALSE;
    IsAssociativeENodeType[EMFPOINTER] = FALSE;
    IsAssociativeENodeType[ETEMP] = FALSE;
    IsAssociativeENodeType[ELOCOBJ] = FALSE;
    IsAssociativeENodeType[EARGOBJ] = FALSE;
    IsAssociativeENodeType[ESETCONST] = FALSE;
    IsAssociativeENodeType[ENEWEXCEPTION] = FALSE;
    IsAssociativeENodeType[ENEWEXCEPTIONARRAY] = FALSE;
    IsAssociativeENodeType[EINITTRYCATCH] = FALSE;
    IsAssociativeENodeType[EOBJACCESS] = FALSE;
    IsAssociativeENodeType[ETEMPLDEP] = FALSE;
    IsAssociativeENodeType[EPOINTERSTAR] = FALSE;
    IsAssociativeENodeType[EDOTSTAR] = FALSE;
    IsAssociativeENodeType[ECTORINIT] = FALSE;
    IsAssociativeENodeType[ESTMT] = FALSE;
    IsAssociativeENodeType[EINSTRUCTION] = FALSE;
    IsAssociativeENodeType[EDEFINE] = FALSE;
    IsAssociativeENodeType[EREUSE] = FALSE;
    IsAssociativeENodeType[EASSBLK] = FALSE;
    IsAssociativeENodeType[ECONDASS] = FALSE;
}
#else
void IRO_InitializeIsAssociativeENodeTypeArray(void) {
    int i;

    for (i = 0; i < MAXEXPR; i++) {
        IsAssociativeENodeType[i] = FALSE;
    }

    IsAssociativeENodeType[EPOSTINC] = FALSE;
    IsAssociativeENodeType[EPOSTDEC] = FALSE;
    IsAssociativeENodeType[EPREINC] = FALSE;
    IsAssociativeENodeType[EPREDEC] = FALSE;
    IsAssociativeENodeType[EINDIRECT] = FALSE;
    IsAssociativeENodeType[EMONMIN] = FALSE;
    IsAssociativeENodeType[EBINNOT] = FALSE;
    IsAssociativeENodeType[ELOGNOT] = FALSE;
    IsAssociativeENodeType[EFORCELOAD] = FALSE;
    IsAssociativeENodeType[EMUL] = TRUE;
    IsAssociativeENodeType[EMULV] = TRUE;
    IsAssociativeENodeType[EDIV] = FALSE;
    IsAssociativeENodeType[EMODULO] = FALSE;
    IsAssociativeENodeType[EADDV] = TRUE;
    IsAssociativeENodeType[ESUBV] = FALSE;
    IsAssociativeENodeType[EADD] = TRUE;
    IsAssociativeENodeType[ESUB] = FALSE;
    IsAssociativeENodeType[ESHL] = FALSE;
    IsAssociativeENodeType[ESHR] = FALSE;
    IsAssociativeENodeType[ELESS] = FALSE;
    IsAssociativeENodeType[EGREATER] = FALSE;
    IsAssociativeENodeType[ELESSEQU] = FALSE;
    IsAssociativeENodeType[EGREATEREQU] = FALSE;
    IsAssociativeENodeType[EEQU] = FALSE;
    IsAssociativeENodeType[ENOTEQU] = FALSE;
    IsAssociativeENodeType[EAND] = TRUE;
    IsAssociativeENodeType[EXOR] = TRUE;
    IsAssociativeENodeType[EOR] = TRUE;
    IsAssociativeENodeType[ELAND] = FALSE;
    IsAssociativeENodeType[ELOR] = FALSE;
    IsAssociativeENodeType[EASS] = FALSE;
    IsAssociativeENodeType[EMULASS] = FALSE;
    IsAssociativeENodeType[EDIVASS] = FALSE;
    IsAssociativeENodeType[EMODASS] = FALSE;
    IsAssociativeENodeType[EADDASS] = FALSE;
    IsAssociativeENodeType[ESUBASS] = FALSE;
    IsAssociativeENodeType[ESHLASS] = FALSE;
    IsAssociativeENodeType[ESHRASS] = FALSE;
    IsAssociativeENodeType[EANDASS] = FALSE;
    IsAssociativeENodeType[EXORASS] = FALSE;
    IsAssociativeENodeType[EORASS] = FALSE;
    IsAssociativeENodeType[ECOMMA] = FALSE;
    IsAssociativeENodeType[EPMODULO] = FALSE;
    IsAssociativeENodeType[EROTL] = FALSE;
    IsAssociativeENodeType[EROTR] = FALSE;
    IsAssociativeENodeType[EBCLR] = FALSE;
    IsAssociativeENodeType[EBTST] = FALSE;
    IsAssociativeENodeType[EBSET] = FALSE;
    IsAssociativeENodeType[ETYPCON] = FALSE;
    IsAssociativeENodeType[EBITFIELD] = FALSE;
    IsAssociativeENodeType[EINTCONST] = FALSE;
    IsAssociativeENodeType[EFLOATCONST] = FALSE;
    IsAssociativeENodeType[ESTRINGCONST] = FALSE;
    IsAssociativeENodeType[ECOND] = FALSE;
    IsAssociativeENodeType[EFUNCCALL] = FALSE;
    IsAssociativeENodeType[EFUNCCALLP] = FALSE;
    IsAssociativeENodeType[EOBJREF] = FALSE;
    IsAssociativeENodeType[EMFPOINTER] = FALSE;
    IsAssociativeENodeType[ENULLCHECK] = FALSE;
    IsAssociativeENodeType[EPRECOMP] = FALSE;
    IsAssociativeENodeType[ETEMP] = FALSE;
    IsAssociativeENodeType[EARGOBJ] = FALSE;
    IsAssociativeENodeType[ELOCOBJ] = FALSE;
    IsAssociativeENodeType[ELABEL] = FALSE;
    IsAssociativeENodeType[ESETCONST] = FALSE;
    IsAssociativeENodeType[ENEWEXCEPTION] = FALSE;
    IsAssociativeENodeType[ENEWEXCEPTIONARRAY] = FALSE;
    IsAssociativeENodeType[EOBJLIST] = FALSE;
    IsAssociativeENodeType[EMEMBER] = FALSE;
    IsAssociativeENodeType[ETEMPLDEP] = FALSE;
    IsAssociativeENodeType[EINSTRUCTION] = FALSE;
    IsAssociativeENodeType[EDEFINE] = FALSE;
    IsAssociativeENodeType[EREUSE] = FALSE;
    IsAssociativeENodeType[EASSBLK] = FALSE;
    IsAssociativeENodeType[EVECTOR128CONST] = FALSE;
    IsAssociativeENodeType[ECONDASS] = FALSE;
}
#endif

#if MWCCPSP
// 3.0.1 (0x55A940) also handles 8-byte types
void IRO_TruncateValueToType(CInt64* val, Type* type) {
    if (is_unsigned(type)) {
        switch (type->size) {
            case 1:
                CInt64_ConvertUInt8(val);
                break;
            case 2:
                CInt64_ConvertUInt16(val);
                break;
            case 4:
                CInt64_ConvertUInt32(val);
                break;
            case 8:
                CInt64_ConvertUInt64(val);
                break;
        }
    } else {
        switch (type->size) {
            case 1:
                CInt64_ConvertInt8(val);
                break;
            case 2:
                CInt64_ConvertInt16(val);
                break;
            case 4:
                CInt64_ConvertInt32(val);
                break;
            case 8:
                CInt64_ConvertInt64(val);
                break;
        }
    }
}
#else
void IRO_TruncateValueToType(CInt64* val, Type* type) {
    if (is_unsigned(type)) {
        switch (type->size) {
            case 1:
                CInt64_ConvertUInt8(val);
                break;
            case 2:
                CInt64_ConvertUInt16(val);
                break;
            case 4:
                CInt64_ConvertUInt32(val);
                break;
        }
    } else {
        switch (type->size) {
            case 1:
                CInt64_ConvertInt8(val);
                break;
            case 2:
                CInt64_ConvertInt16(val);
                break;
            case 4:
                CInt64_ConvertInt32(val);
                break;
        }
    }
}
#endif

void IRO_TruncateBitfieldValueToType(CInt64* val, Type* type, TypeBitfield* bitfieldType) {
    UInt32 limit;
    UInt32 i;
    UInt32 j;
    CInt64 work;

    work = cint64_zero;
    limit = bitfieldType->bitlength;
    for (i = 0; i < limit; i++) {
        work = CInt64_Or(work, CInt64_Shl(cint64_one, IRO_MakeULong(i)));
    }
    *val = CInt64_And(*val, work);

    if (!is_unsigned(type)) {
        work = cint64_zero;
        for (j = 0; j <= (i - 1); j++) {
            if (j == (i - 1)) {
                work = CInt64_Or(work, CInt64_Shl(cint64_one, IRO_MakeULong(j)));
            }
        }
        if (CInt64_NotEqual(CInt64_And(work, *val), cint64_zero)) {
            for (j = i - 1; j < 64; j++) {
                *val = CInt64_Or(*val, CInt64_Shl(cint64_one, IRO_MakeULong(j)));
            }
        }
    }

    IRO_TruncateValueToType(val, type);
}

#if !MWCCPSP
void IRO_ConstantFolding(void) {
    IROLinear* nd;
    ENode* expr;
    int isCompare;
    int folded;
    CInt64 val;

    for (nd = IRO_FirstLinear; nd; nd = nd->next) {
        switch (nd->type) {
            case IROLinearOp1Arg:
                if (IRO_IsIntConstant(nd->u.monadic)) {
                    expr = NULL;
                    folded = FALSE;
                    val = nd->u.monadic->u.node->data.intval;
                    if (nd->nodetype == ETYPCON && IS_TYPE_FLOAT(nd->rtype)) {
                        expr = IRO_NewENode(EFLOATCONST);
                        if (!is_unsigned(nd->u.monadic->rtype)) {
                            expr->data.floatval.data.value = CInt64_ConvertToLongDouble(&val);
                        } else {
                            expr->data.floatval.data.value = CInt64_ConvertUToLongDouble(&val);
                        }
                        expr->rtype = nd->rtype;
                    } else {
                        switch (nd->nodetype) {
                            case ETYPCON:
                                folded = TRUE;
                                break;
                            case ELOGNOT:
                                val = CInt64_Not(val);
                                folded = TRUE;
                                break;
                            case EBINNOT:
                                val = CInt64_Inv(val);
                                folded = TRUE;
                                break;
                            case EMONMIN:
                                val = CInt64_Neg(val);
                                folded = TRUE;
                                break;
                            default:
                                break;
                        }

                        if (folded) {
                            IRO_TruncateValueToType(&val, nd->rtype);
                            expr = IRO_NewENode(EINTCONST);
                            expr->rtype = nd->rtype;
                            expr->data.intval = val;
                        }
                    }

                    if (expr) {
                        nd->u.monadic->type = IROLinearNop;
                        nd->type = IROLinearOperand;
                        nd->u.node = expr;
                    }
                }
                break;

            case IROLinearOp2Arg:
                if (IRO_IsIntConstant(nd->u.diadic.left) &&
                    !IRO_IsIntConstant(nd->u.diadic.right) &&
                    IsAssociativeENodeType[nd->nodetype]) {
                    IROLinear* swap = nd->u.diadic.right;
                    nd->u.diadic.right = nd->u.diadic.left;
                    nd->u.diadic.left = swap;
                }

                if (IRO_IsIntConstant(nd->u.diadic.right) && nd->nodetype == ESUB) {
                    nd->nodetype = EADD;
                    if (IRO_IsIntConstant(nd->u.diadic.right)) {
                        CInt64 v;
                        v = CInt64_Neg(nd->u.diadic.right->u.node->data.intval);
                        nd->u.diadic.right->u.node->data.intval = v;
                    } else {
                        Float f;
                        f = CMach_CalcFloatMonadic(
                            nd->u.diadic.right->rtype,
                            '-',
                            nd->u.diadic.right->u.node->data.floatval
                        );
                        nd->u.diadic.right->u.node->data.floatval = f;
                    }
                }

                if (IRO_IsIntConstant(nd->u.diadic.right) &&
                    IsAssociativeENodeType[nd->nodetype] &&
                    nd->u.diadic.left->type == IROLinearOp2Arg &&
                    nd->u.diadic.left->nodetype == nd->nodetype &&
                    nd->u.diadic.left->rtype == nd->rtype &&
                    IRO_IsIntConstant(nd->u.diadic.left->u.diadic.right) &&
                    nd->u.diadic.left->u.diadic.right->rtype == nd->u.diadic.right->rtype) {
                    IROLinear* inner = nd->u.diadic.left;
                    nd->u.diadic.left = inner->u.diadic.left;
                    inner->u.diadic.left = inner->u.diadic.right;
                    inner->u.diadic.right = nd->u.diadic.right;
                    inner->rtype = inner->u.diadic.left->rtype;
                    nd->u.diadic.right = inner;
                    nd = inner;
                }

                if (IRO_IsIntConstant(nd->u.diadic.left) && IRO_IsIntConstant(nd->u.diadic.right)) {
                    CInt64 val1 = nd->u.diadic.left->u.node->data.intval;
                    CInt64 val2 = nd->u.diadic.right->u.node->data.intval;
                    folded = FALSE;
                    switch (nd->nodetype) {
                        case EADD:
                            val = CInt64_Add(val1, val2);
                            folded = TRUE;
                            break;
                        case ESUB:
                            val = CInt64_Sub(val1, val2);
                            folded = TRUE;
                            break;
                        case EMUL:
                            if (is_unsigned(nd->rtype)) {
                                val = CInt64_MulU(val1, val2);
                            } else {
                                val = CInt64_Mul(val1, val2);
                            }
                            folded = TRUE;
                            break;
                        case EDIV:
                            if (!CInt64_IsZero(&val2)) {
                                if (is_unsigned(nd->rtype)) {
                                    val = CInt64_DivU(val1, val2);
                                } else {
                                    val = CInt64_Div(val1, val2);
                                }
                                folded = TRUE;
                            }
                            break;
                        case EMODULO:
                            if (!CInt64_IsZero(&val2)) {
                                if (is_unsigned(nd->rtype)) {
                                    val = CInt64_ModU(val1, val2);
                                } else {
                                    val = CInt64_Mod(val1, val2);
                                }
                                folded = TRUE;
                            }
                            break;
                        case ESHL:
                            val = CInt64_Shl(val1, val2);
                            folded = TRUE;
                            break;
                        case ESHR:
                            if (is_unsigned(nd->rtype)) {
                                val = CInt64_ShrU(val1, val2);
                            } else {
                                val = CInt64_Shr(val1, val2);
                            }
                            folded = TRUE;
                            break;
                        case EAND:
                            val = CInt64_And(val1, val2);
                            folded = TRUE;
                            break;
                        case EOR:
                            val = CInt64_Or(val1, val2);
                            folded = TRUE;
                            break;
                        case EXOR:
                            val = CInt64_Xor(val1, val2);
                            folded = TRUE;
                            break;
                        case ELESS:
                            if (is_unsigned(nd->u.diadic.left->rtype)) {
                                CInt64_SetULong(&val, CInt64_LessU(val1, val2));
                            } else {
                                CInt64_SetULong(&val, CInt64_Less(val1, val2));
                            }
                            folded = TRUE;
                            break;
                        case EGREATER:
                            if (is_unsigned(nd->u.diadic.left->rtype)) {
                                CInt64_SetULong(&val, CInt64_GreaterU(val1, val2));
                            } else {
                                CInt64_SetULong(&val, CInt64_Greater(val1, val2));
                            }
                            folded = TRUE;
                            break;
                        case ELESSEQU:
                            if (is_unsigned(nd->u.diadic.left->rtype)) {
                                CInt64_SetULong(&val, CInt64_LessEqualU(val1, val2));
                            } else {
                                CInt64_SetULong(&val, CInt64_LessEqual(val1, val2));
                            }
                            folded = TRUE;
                            break;
                        case EGREATEREQU:
                            if (is_unsigned(nd->u.diadic.left->rtype)) {
                                CInt64_SetULong(&val, CInt64_GreaterEqualU(val1, val2));
                            } else {
                                CInt64_SetULong(&val, CInt64_GreaterEqual(val1, val2));
                            }
                            folded = TRUE;
                            break;
                        case EEQU:
                            CInt64_SetULong(&val, CInt64_Equal(val1, val2));
                            folded = TRUE;
                            break;
                        case ENOTEQU:
                            CInt64_SetULong(&val, CInt64_NotEqual(val1, val2));
                            folded = TRUE;
                            break;
                        default:
                            break;
                    }

                    if (folded) {
                        IRO_TruncateValueToType(&val, nd->rtype);
                        expr = IRO_NewENode(EINTCONST);
                        expr->rtype = nd->rtype;
                        expr->data.intval = val;
                        nd->u.diadic.left->type = IROLinearNop;
                        nd->u.diadic.right->type = IROLinearNop;
                        nd->type = IROLinearOperand;
                        nd->u.node = expr;
                    }
                }

                if (IRO_IsFloatConstant(nd->u.diadic.left) &&
                    IRO_IsFloatConstant(nd->u.diadic.right)) {
                    Float fval1 = nd->u.diadic.left->u.node->data.floatval;
                    Float fval2 = nd->u.diadic.right->u.node->data.floatval;
                    Float fval;
                    folded = FALSE;
                    isCompare = FALSE;
                    switch (nd->nodetype) {
                        case EADD:
                            fval = CMach_CalcFloatDiadic(nd->rtype, fval1, '+', fval2);
                            folded = TRUE;
                            break;
                        case ESUB:
                            fval = CMach_CalcFloatDiadic(nd->rtype, fval1, '-', fval2);
                            folded = TRUE;
                            break;
                        case EMUL:
                            fval = CMach_CalcFloatDiadic(nd->rtype, fval1, '*', fval2);
                            folded = TRUE;
                            break;
                        case EDIV:
                            fval = CMach_CalcFloatDiadic(nd->rtype, fval1, '/', fval2);
                            folded = TRUE;
                            break;
                        case ELESS:
                            CInt64_SetULong(
                                &val, CMach_CalcFloatDiadicBool(nd->rtype, fval1, '<', fval2)
                            );
                            folded = TRUE;
                            isCompare = TRUE;
                            break;
                        case EGREATER:
                            CInt64_SetULong(
                                &val, CMach_CalcFloatDiadicBool(nd->rtype, fval1, '>', fval2)
                            );
                            folded = TRUE;
                            isCompare = TRUE;
                            break;
                        case ELESSEQU:
                            CInt64_SetULong(
                                &val,
                                CMach_CalcFloatDiadicBool(nd->rtype, fval1, TK_LESS_EQUAL, fval2)
                            );
                            folded = TRUE;
                            isCompare = TRUE;
                            break;
                        case EGREATEREQU:
                            CInt64_SetULong(
                                &val,
                                CMach_CalcFloatDiadicBool(nd->rtype, fval1, TK_GREATER_EQUAL, fval2)
                            );
                            folded = TRUE;
                            isCompare = TRUE;
                            break;
                        case EEQU:
                            CInt64_SetULong(
                                &val,
                                CMach_CalcFloatDiadicBool(nd->rtype, fval1, TK_LOGICAL_EQ, fval2)
                            );
                            folded = TRUE;
                            isCompare = TRUE;
                            break;
                        case ENOTEQU:
                            CInt64_SetULong(
                                &val,
                                CMach_CalcFloatDiadicBool(nd->rtype, fval1, TK_LOGICAL_NE, fval2)
                            );
                            folded = TRUE;
                            isCompare = TRUE;
                            break;
                        default:
                            break;
                    }

                    if (folded) {
                        if (isCompare) {
                            IRO_TruncateValueToType(&val, nd->rtype);
                            expr = IRO_NewENode(EINTCONST);
                            expr->rtype = nd->rtype;
                            expr->data.intval = val;
                            nd->u.diadic.left->type = IROLinearNop;
                            nd->u.diadic.right->type = IROLinearNop;
                            nd->type = IROLinearOperand;
                            nd->u.node = expr;
                        } else {
                            expr = IRO_NewENode(EFLOATCONST);
                            expr->rtype = nd->rtype;
                            expr->data.floatval = fval;
                            nd->u.diadic.left->type = IROLinearNop;
                            nd->u.diadic.right->type = IROLinearNop;
                            nd->type = IROLinearOperand;
                            nd->u.node = expr;
                        }
                    }
                }

                break;
            default:
                break;
        }
    }

    IRO_CheckForUserBreak();
}
#else
extern ENode* func_00551500(Float val, Type* type);
extern ENode* IRO_NewIntConstENode(CInt64 val, Type* type);
extern Boolean IsSubableOp[IRO_MAXEXPR];

// an integer constant of an integral, pointer or enum type
static inline Boolean IsIntConst301(IROLinear* nd) {
    Type* type;

    if (nd->type != IROLinearOperand) {
        return FALSE;
    }
    type = nd->rtype;
    if (!((type->type == TYPEINT && TYPE_INTEGRAL(type)->integral < IT_17) ||
          type->type == TYPEPOINTER ||
          type->type == TYPEENUM)) {
        return FALSE;
    }
    return nd->u.node && nd->u.node->type == EINTCONST;
}

static inline void ReplaceByConstant(IROLinear* nd, ENode* expr) {
    nd->nodetype = expr->type;
    nd->u.diadic.left->type = IROLinearNop;
    nd->u.diadic.right->type = IROLinearNop;
    nd->type = IROLinearOperand;
    nd->u.node = expr;
}

// 0x58C570: fold nd if its operands are constants (3.0.1 also folds
// floating operations, min and max)
void func_0058C570(IROLinear* nd) {
    IROLinear* operand;
    IROLinear* swap;
    IROLinear* inner;
    ENode* expr;
    CInt64 val;
    CInt64 v1;
    CInt64 v2;
    Float f;
    Float f1;
    Float f2;
    Boolean folded;
    Boolean isInt;

    switch (nd->type) {
        case IROLinearOp1Arg:
            operand = nd->u.monadic;
            if (!IsIntConst301(operand)) {
                return;
            }
            val = operand->u.node->data.intval;
            expr = NULL;
            if (nd->nodetype == ETYPCON && nd->rtype->type == TYPEFLOAT) {
                if (copts.opt_convertfpconsts) {
                    expr = func_00551500(
                        CMach_CalcFloatConvertFromInt(operand->rtype, val),
                        IRO_UnqualifiedType(nd->rtype)
                    );
                }
            } else {
                folded = FALSE;
                switch (nd->nodetype) {
                    case EMONMIN:
                        val = CInt64_Neg(val);
                        folded = TRUE;
                        break;
                    case EBINNOT:
                        val = CInt64_Inv(val);
                        folded = TRUE;
                        break;
                    case ELOGNOT:
                        val = CInt64_Not(val);
                        folded = TRUE;
                        break;
                    case ETYPCON:
                        folded = TRUE;
                        break;
                }
                if (folded) {
                    IRO_TruncateValueToType(&val, nd->rtype);
                    expr = IRO_NewIntConstENode(val, nd->rtype);
                }
            }
            if (expr) {
                nd->nodetype = expr->type;
                nd->u.monadic->type = IROLinearNop;
                nd->type = IROLinearOperand;
                nd->u.node = expr;
            }
            break;

        case IROLinearOp2Arg:
            if (IsIntConst301(nd->u.diadic.left) &&
                !IsIntConst301(nd->u.diadic.right) &&
                IsSubableOp[nd->nodetype]) {
                swap = nd->u.diadic.left;
                nd->u.diadic.left = nd->u.diadic.right;
                nd->u.diadic.right = swap;
            }
            if (IsIntConst301(nd->u.diadic.right) && nd->nodetype == ESUB) {
                nd->nodetype = EADD;
                if (IsIntConst301(nd->u.diadic.right)) {
                    val = CInt64_Neg(nd->u.diadic.right->u.node->data.intval);
                    IRO_TruncateValueToType(&val, nd->rtype);
                    nd->u.diadic.right->u.node->data.intval = val;
                } else {
                    nd->u.diadic.right->u.node->data.floatval = CMach_CalcFloatMonadic(
                        nd->u.diadic.right->rtype, '-', nd->u.diadic.right->u.node->data.floatval
                    );
                }
            }
            if (IsIntConst301(nd->u.diadic.right) &&
                IsAssociativeENodeType[nd->nodetype] &&
                nd->u.diadic.left->type == IROLinearOp2Arg &&
                nd->u.diadic.left->nodetype == nd->nodetype &&
                (nd->u.diadic.left->rtype == nd->rtype ||
                 IRO_TypesEqual(nd->u.diadic.left->rtype, nd->rtype)) &&
                IsIntConst301(nd->u.diadic.left->u.diadic.right) &&
                (nd->u.diadic.left->u.diadic.right->rtype == nd->u.diadic.right->rtype ||
                 IRO_TypesEqual(
                     nd->u.diadic.left->u.diadic.right->rtype, nd->u.diadic.right->rtype
                 ))) {
                inner = nd->u.diadic.left;
                nd->u.diadic.left = inner->u.diadic.left;
                inner->u.diadic.left = inner->u.diadic.right;
                inner->u.diadic.right = nd->u.diadic.right;
                inner->rtype = IRO_UnqualifiedType(inner->u.diadic.left->rtype);
                nd->u.diadic.right = inner;
                nd = inner;
            }
            if (IsIntConst301(nd->u.diadic.left) && IsIntConst301(nd->u.diadic.right)) {
                v1 = nd->u.diadic.left->u.node->data.intval;
                v2 = nd->u.diadic.right->u.node->data.intval;
                folded = FALSE;
                switch (nd->nodetype) {
                    case EADD:
                        val = CInt64_Add(v1, v2);
                        folded = TRUE;
                        break;
                    case ESUB:
                        val = CInt64_Sub(v1, v2);
                        folded = TRUE;
                        break;
                    case EMUL:
                        val = is_unsigned(nd->rtype) ? CInt64_MulU(v1, v2) : CInt64_Mul(v1, v2);
                        folded = TRUE;
                        break;
                    case EDIV:
                        if (!CInt64_IsZero(&v2)) {
                            val = is_unsigned(nd->rtype) ? CInt64_DivU(v1, v2) : CInt64_Div(v1, v2);
                            folded = TRUE;
                        }
                        break;
                    case EMODULO:
                        if (!CInt64_IsZero(&v2)) {
                            val = is_unsigned(nd->rtype) ? CInt64_ModU(v1, v2) : CInt64_Mod(v1, v2);
                            folded = TRUE;
                        }
                        break;
                    case ESHL:
                        val = CInt64_Shl(v1, v2);
                        folded = TRUE;
                        break;
                    case ESHR:
                        val = is_unsigned(nd->rtype) ? CInt64_ShrU(v1, v2) : CInt64_Shr(v1, v2);
                        folded = TRUE;
                        break;
                    case EAND:
                        val = CInt64_And(v1, v2);
                        folded = TRUE;
                        break;
                    case EOR:
                        val = CInt64_Or(v1, v2);
                        folded = TRUE;
                        break;
                    case EXOR:
                        val = CInt64_Xor(v1, v2);
                        folded = TRUE;
                        break;
                    case ELESS:
                        CInt64_SetULong(
                            &val,
                            is_unsigned(nd->u.diadic.left->rtype) ? CInt64_LessU(v1, v2)
                                                                  : CInt64_Less(v1, v2)
                        );
                        folded = TRUE;
                        break;
                    case EGREATER:
                        CInt64_SetULong(
                            &val,
                            is_unsigned(nd->u.diadic.left->rtype) ? CInt64_GreaterU(v1, v2)
                                                                  : CInt64_Greater(v1, v2)
                        );
                        folded = TRUE;
                        break;
                    case ELESSEQU:
                        CInt64_SetULong(
                            &val,
                            is_unsigned(nd->u.diadic.left->rtype) ? CInt64_LessEqualU(v1, v2)
                                                                  : CInt64_LessEqual(v1, v2)
                        );
                        folded = TRUE;
                        break;
                    case EGREATEREQU:
                        CInt64_SetULong(
                            &val,
                            is_unsigned(nd->u.diadic.left->rtype) ? CInt64_GreaterEqualU(v1, v2)
                                                                  : CInt64_GreaterEqual(v1, v2)
                        );
                        folded = TRUE;
                        break;
                    case EEQU:
                        CInt64_SetULong(&val, CInt64_Equal(v1, v2));
                        folded = TRUE;
                        break;
                    case ENOTEQU:
                        CInt64_SetULong(&val, CInt64_NotEqual(v1, v2));
                        folded = TRUE;
                        break;
                    case EMIN:
                        if (is_unsigned(nd->u.diadic.left->rtype)) {
                            val = CInt64_LessU(v1, v2) ? v1 : v2;
                        } else {
                            val = CInt64_Less(v1, v2) ? v1 : v2;
                        }
                        folded = TRUE;
                        break;
                    case EMAX:
                        if (is_unsigned(nd->u.diadic.left->rtype)) {
                            val = CInt64_GreaterU(v1, v2) ? v1 : v2;
                        } else {
                            val = CInt64_Equal(v1, v2) ? v1 : v2; // sic, 3.0.1
                        }
                        folded = TRUE;
                        break;
                }
                if (folded) {
                    IRO_TruncateValueToType(&val, nd->rtype);
                    ReplaceByConstant(nd, IRO_NewIntConstENode(val, nd->rtype));
                    return;
                }
            }
            if (IRO_IsFloatConstant(nd->u.diadic.left) && IRO_IsFloatConstant(nd->u.diadic.right)) {
                f1 = nd->u.diadic.left->u.node->data.floatval;
                f2 = nd->u.diadic.right->u.node->data.floatval;
                folded = FALSE;
                isInt = FALSE;
                switch (nd->nodetype) {
                    case EADD:
                        f = CMach_CalcFloatDiadic(nd->rtype, f1, '+', f2);
                        folded = TRUE;
                        break;
                    case ESUB:
                        f = CMach_CalcFloatDiadic(nd->rtype, f1, '-', f2);
                        folded = TRUE;
                        break;
                    case EMUL:
                        f = CMach_CalcFloatDiadic(nd->rtype, f1, '*', f2);
                        folded = TRUE;
                        break;
                    case EDIV:
                        f = CMach_CalcFloatDiadic(nd->rtype, f1, '/', f2);
                        folded = TRUE;
                        break;
                    case ELESS:
                        CInt64_SetULong(&val, CMach_CalcFloatDiadicBool(nd->rtype, f1, '<', f2));
                        folded = isInt = TRUE;
                        break;
                    case EGREATER:
                        CInt64_SetULong(&val, CMach_CalcFloatDiadicBool(nd->rtype, f1, '>', f2));
                        folded = isInt = TRUE;
                        break;
                    case ELESSEQU:
                        CInt64_SetULong(
                            &val, CMach_CalcFloatDiadicBool(nd->rtype, f1, TK_LESS_EQUAL, f2)
                        );
                        folded = isInt = TRUE;
                        break;
                    case EGREATEREQU:
                        CInt64_SetULong(
                            &val, CMach_CalcFloatDiadicBool(nd->rtype, f1, TK_GREATER_EQUAL, f2)
                        );
                        folded = isInt = TRUE;
                        break;
                    case EEQU:
                        CInt64_SetULong(
                            &val, CMach_CalcFloatDiadicBool(nd->rtype, f1, TK_LOGICAL_EQ, f2)
                        );
                        folded = isInt = TRUE;
                        break;
                    case ENOTEQU:
                        CInt64_SetULong(
                            &val, CMach_CalcFloatDiadicBool(nd->rtype, f1, TK_LOGICAL_NE, f2)
                        );
                        folded = isInt = TRUE;
                        break;
                    case EMIN:
                        f = CMach_CalcFloatDiadicBool(nd->rtype, f1, '<', f2) ? f1 : f2;
                        folded = TRUE;
                        break;
                    case EMAX:
                        f = CMach_CalcFloatDiadicBool(nd->rtype, f1, '>', f2) ? f1 : f2;
                        folded = TRUE;
                        break;
                }
                if (folded) {
                    if (isInt) {
                        IRO_TruncateValueToType(&val, nd->rtype);
                        expr = IRO_NewIntConstENode(val, nd->rtype);
                    } else {
                        expr = func_00551500(f, nd->rtype);
                    }
                    ReplaceByConstant(nd, expr);
                }
            }
            break;
    }
}

// 0x58C550: IRO_WalkTree callback
static void FoldOnEntry(IROLinear* linear, Boolean isEntry) {
    if (isEntry) {
        func_0058C570(linear);
    }
}

// 0x58C530: fold an expression tree
void func_0058C530(IROLinear* nd) {
    IRO_WalkTree(nd, FoldOnEntry);
}

// 0x58C500
void IRO_ConstantFolding(void) {
    IROLinear* nd;

    for (nd = IRO_FirstLinear; nd; nd = nd->next) {
        func_0058C570(nd);
    }
    IRO_CheckForUserBreak();
}
#endif

#if MWCCPSP
// 3.0.1 (0x58C350): integer, pointer and enum constants; switch cases are
// ranges; the flow graph is rebuilt afterwards
static Boolean evalconst301(IROLinear* nd) {
    Type* t;

    if (nd->type != IROLinearOperand || !nd->u.node || nd->u.node->type != EINTCONST) {
        return FALSE;
    }
    t = nd->rtype;
    return (t->type == TYPEINT && TYPE_INTEGRAL(t)->integral < IT_17) ||
        t->type == TYPEPOINTER ||
        t->type == TYPEENUM;
}

Boolean IRO_EvaluateConditionals(void) {
    IRONode* fnode;
    IROLinear* nd;
    IROLinear* cond;
    Boolean changed = FALSE;
    Boolean isZero;
    Boolean found;
    SwitchInfo* switchInfo;
    SwitchCase* swcase;
    CInt64 val;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        nd = fnode->last;
        switch (nd->type) {
            case IROLinearIf:
            case IROLinearIfNot:
                cond = nd->u.label.cond;
                if (evalconst301(cond)) {
                    isZero = CInt64_IsZero(&cond->u.node->data.intval);
                    IRO_NopOut(cond);
                    nd->type =
                        (!isZero == (nd->type == IROLinearIf)) ? IROLinearGoto : IROLinearNop;
                    changed = TRUE;
                }
                break;
            case IROLinearSwitch:
                cond = nd->u.swtch.cond;
                if (evalconst301(cond)) {
                    val = cond->u.node->data.intval;
                    switchInfo = nd->u.swtch.info;
                    IRO_NopOut(cond);
                    nd->type = IROLinearGoto;
                    found = FALSE;
                    for (swcase = switchInfo->cases; swcase; swcase = swcase->next) {
                        if (CInt64_GreaterEqual(val, swcase->min) &&
                            CInt64_LessEqual(val, swcase->max)) {
                            found = TRUE;
                            nd->u.label.label = swcase->label;
                            break;
                        }
                    }
                    if (!found) {
                        nd->u.label.label = switchInfo->defaultlabel;
                    }
                    changed = TRUE;
                }
                break;
            default:
                break;
        }
    }

    if (changed) {
        IRO_BuildFlowgraph(IRO_FirstLinear);
    }
    IRO_CheckForUserBreak();
    return changed;
}
#else
Boolean IRO_EvaluateConditionals(void) {
    IRONode* fnode;
    IROLinear* nd;
    Boolean changed = FALSE;
    SwitchInfo* switchInfo;
    SwitchCase* swcase;
    char found;
    CInt64 val;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        nd = fnode->last;
        switch (nd->type) {
            case IROLinearIf:
            case IROLinearIfNot:
                if (IRO_IsIntConstant(nd->u.label.cond)) {
                    Boolean isZero = CInt64_IsZero(&nd->u.label.cond->u.node->data.intval);
                    IRO_NopOut(nd->u.label.cond);
                    if (((!isZero) != 0) == (nd->type == IROLinearIf)) {
                        nd->type = IROLinearGoto;
                    } else {
                        nd->type = IROLinearNop;
                    }
                    changed = TRUE;
                }
                break;

            case IROLinearSwitch:
                if (IRO_IsIntConstant(nd->u.swtch.cond)) {
                    val = nd->u.swtch.cond->u.node->data.intval;
                    switchInfo = nd->u.swtch.info;
                    swcase = switchInfo->cases;

                    IRO_NopOut(nd->u.swtch.cond);
                    nd->type = IROLinearGoto;

                    found = FALSE;
                    while (swcase) {
                        if (CInt64_Equal(swcase->min, val)) {
                            found = TRUE;
                            nd->u.label.label = swcase->label;
                            break;
                        }
                        swcase = swcase->next;
                    }

                    if (!found) {
                        nd->u.label.label = switchInfo->defaultlabel;
                    }
                    changed = TRUE;
                }
                break;
            default:
                break;
        }
    }

    if (changed) {
        IRO_ComputeSuccPred();
        IRO_ComputeDom();
    }
    IRO_CheckForUserBreak();

    return changed;
}
#endif

static int EEquConst(IROLinear* nd) {
    return nd && (nd->nodetype == EEQU) && IRO_IsIntConstant(nd->u.diadic.right);
}

static Object* VEquConst(IROLinear* nd) {
    if (EEquConst(nd)) {
        return IRO_IsVariable(nd->u.diadic.left);
    } else {
        return NULL;
    }
}

static int IsConsecutive(CInt64 a, CInt64 b) {
    CInt64 diff;

    if (!CInt64_Equal(a, cint64_min) && !CInt64_Equal(b, cint64_min)) {
        diff = CInt64_Sub(b, a);
        return CInt64_Equal(diff, cint64_one) || CInt64_Equal(diff, cint64_negone);
    }

    return FALSE;
}

static IROLinear* findLabel(CLabel* label) {
    IROLinear* nd;

    for (nd = IRO_FirstLinear; nd; nd = nd->next) {
        if (nd->type == IROLinearLabel && nd->u.label.label == label) {
            break;
        }
    }

    return nd;
}

static IROLinear* leftLeaveOf(IROLinear* nd) {
    switch (nd->type) {
        case IROLinearOp1Arg:
            return leftLeaveOf(nd->u.monadic);
        case IROLinearOp2Arg:
            return leftLeaveOf(nd->u.diadic.left);
        case IROLinearOperand:
            return nd;
        default:
            return NULL;
    }
}

static int checkNode(IRONode* fnode) {
    IROLinear* nd;

    if (fnode->numpred <= 1) {
        nd = fnode->first;
        while (nd != fnode->last && (nd->type == IROLinearNop || nd->type == IROLinearLabel)) {
            nd = nd->next;
        }

        if (nd == leftLeaveOf(fnode->last->u.label.cond)) {
            return TRUE;
        }
    }

    return FALSE;
}

static int checkLabel(CLabel* label, IRONode* fnode) {
    switch (fnode->last->type) {
        case IROLinearIf:
            if (label == fnode->last->u.label.label) {
                return TRUE;
            }
            break;
        default:
            break;
    }

    return FALSE;
}

static Object* checkExpr(Object* expected, IROLinear* nd) {
    Object* var = VEquConst(nd);

    if ((!expected || expected == var) && !IRO_HasSideEffect(nd)) {
        return var;
    }

    return NULL;
}

static int checkStruct(IRONode* fnode1, IRONode* fnode2) {
    CLabel* label;
    Object* var;

    if (fnode1 == fnode2) {
        return (int)checkExpr(NULL, fnode1->last->u.label.cond);
    }

    label = fnode1->last->u.label.label;
    var = IRO_IsVariable(fnode1->last->u.label.cond->u.monadic);
    return checkNode(fnode2) &&
        checkLabel(label, fnode2) &&
        checkExpr(var, fnode2->last->u.label.cond);
}

typedef struct ReduceInfo {
    int mark;
    int state;
    Object* var;
    IRONode* fnode;
    struct ReduceInfo* next;
    CInt64 val;
} ReduceInfo;

static int MarkPattern1(ReduceInfo* info1, ReduceInfo* info2, CInt64* val) {
    ReduceInfo* scan;

    if (!info2) {
        return FALSE;
    }

    if (info2->mark) {
        return MarkPattern1(info1, info2->next, val);
    }

    for (scan = info1; scan; scan = scan->next) {
        if (scan->mark == 2) {
            if (CInt64_Equal(info2->val, scan->val)) {
                IRO_NopOut(scan->fnode->last);
                IRO_NopOut(scan->fnode->last->u.label.cond);
                scan->mark = -1;
                return MarkPattern1(info1, info2->next, val);
            }

            if (IsConsecutive(info2->val, scan->val)) {
                info2->mark = 2;
                if (CInt64_Greater(*val, scan->val)) {
                    *val = scan->val;
                }
                if (CInt64_Greater(*val, info2->val)) {
                    *val = info2->val;
                }
                MarkPattern1(scan->next, info2, val);
                MarkPattern1(info1, info1->next, val);
                return TRUE;
            }
        }
    }

    return MarkPattern1(info1, info2->next, val);
}

static int DoReducible1(ReduceInfo* info, CInt64 val) {
    ReduceInfo* last;
    ReduceInfo* scan;
    IROLinear* cond;
    IROLinear* typconLeft;
    IROLinear* typconRight;
    IROLinear* left;
    IROLinear* right;
    int count;

    count = 0;
    for (scan = info; scan; scan = scan->next) {
        if (scan->mark == 2) {
            last = scan;
            count++;
        }
    }

    if (!count) {
        return 0;
    }

    for (scan = info; scan != last; scan = scan->next) {
        if (scan->mark == 2) {
            scan->mark = -1;
            IRO_NopOut(scan->fnode->last);
            IRO_NopOut(scan->fnode->last->u.label.cond);
        }
    }

    last->mark = -1;

    cond = last->fnode->last;
    cond->u.label.cond->nodetype = ELESSEQU;
    CInt64_SetULong(&cond->u.label.cond->u.diadic.right->u.node->data.intval, count - 1);

    typconLeft = IRO_NewLinear(IROLinearOp1Arg);
    typconLeft->nodetype = ETYPCON;
    typconLeft->rtype = IRO_UnsignedType(cond->u.label.cond->u.diadic.left->rtype);
    typconLeft->index = ++IRO_NumLinear;

    typconRight = IRO_NewLinear(IROLinearOp1Arg);
    *typconRight = *typconLeft;
    typconRight->index = ++IRO_NumLinear;

    left = IRO_NewLinear(IROLinearOp2Arg);
    left->nodetype = EADD;
    left->rtype = cond->u.label.cond->u.diadic.left->rtype;
    left->index = ++IRO_NumLinear;

    right = IRO_NewLinear(IROLinearOperand);
    right->nodetype = EINTCONST;
    right->rtype = cond->u.label.cond->u.diadic.left->rtype;
    right->index = ++IRO_NumLinear;
    right->u.node = IRO_NewENode(EINTCONST);
    right->u.node->data.intval = CInt64_Neg(val);
    right->u.node->rtype = right->rtype;

    typconLeft->next = cond->u.label.cond->u.diadic.left->next;
    cond->u.label.cond->u.diadic.left->next = right;
    right->next = left;
    left->next = typconLeft;

    typconRight->next = cond->u.label.cond->u.diadic.right->next;
    cond->u.label.cond->u.diadic.right->next = typconRight;

    typconLeft->u.monadic = left;
    left->u.diadic.left = cond->u.label.cond->u.diadic.left;
    left->u.diadic.right = right;
    cond->u.label.cond->u.diadic.left = typconLeft;
    typconRight->u.monadic = cond->u.label.cond->u.diadic.right;
    cond->u.label.cond->u.diadic.right = typconRight;

    return count;
}

static int ReducePattern1(IRONode* startnode, IRONode* endnode) {
    ReduceInfo* infos;
    ReduceInfo* info;
    int changed = FALSE;
    int count;
    IRONode* fnode;
    int i;
    int j;
    CInt64 val;

    if (startnode == endnode) {
        return FALSE;
    }

    fnode = startnode;
    count = 0;
    for (; fnode != endnode; fnode = fnode->nextnode) {
        count++;
    }

    infos = oalloc(sizeof(ReduceInfo) * ++count);

    fnode = startnode;
    for (i = 0; i < count; i++) {
        infos[i].mark = 0;
        infos[i].state = 0;
        infos[i].fnode = fnode;
        infos[i].next = NULL;
        infos[i].var = VEquConst(fnode->last->u.label.cond);
        if (infos[i].var) {
            infos[i].val = fnode->last->u.label.cond->u.diadic.right->u.node->data.intval;
            infos[i].state = 1;
        }
        fnode = fnode->nextnode;
    }

    for (j = 0; j < count; j++) {
        if (infos[j].state == 1 && infos[j].var) {
            infos[j].state = -1;
            info = &infos[j];
            for (i = j + 1; i < count; i++) {
                if (infos[j].var == infos[i].var) {
                    info->next = &infos[i];
                    info = info->next;
                    infos[i].state = 0;
                }
            }
        }
    }

    for (j = 0; j < count; j++) {
        if (infos[j].state == -1) {
            for (info = &infos[j]; info; info = info->next) {
                if (info->mark == 0) {
                    info->mark = 2;
                    val = info->val;
                    if (MarkPattern1(&infos[j], info->next, &val)) {
                        changed = TRUE;
                        DoReducible1(&infos[j], val);
                    } else {
                        info->mark = -1;
                    }
                }
            }
        }
    }

    return changed;
}

static int ReduceConsecutiveIf(IRONode* startnode, IRONode* endnode) {
    IRONode* runEnd;
    IRONode* lastMatch;
    int changed = FALSE;

    while (startnode != endnode) {
        if (checkStruct(startnode, startnode)) {
            break;
        }
        startnode = startnode->nextnode;
    }

    runEnd = startnode;
    if (startnode != endnode) {
        lastMatch = startnode;
        runEnd = startnode->nextnode;
        while (runEnd != endnode) {
            if (checkStruct(startnode, runEnd)) {
                lastMatch = runEnd;
                runEnd = runEnd->nextnode;
            } else {
                runEnd = lastMatch;
                break;
            }
        }

        if (runEnd == endnode && !checkStruct(startnode, runEnd)) {
            runEnd = lastMatch;
        }

        if (startnode != runEnd && ReducePattern1(startnode, runEnd)) {
            changed = TRUE;
        }

        if (runEnd != endnode) {
            runEnd = runEnd->nextnode;
        }
    }

    if (runEnd != endnode && ReduceConsecutiveIf(runEnd, endnode)) {
        changed = TRUE;
    }

    return changed;
}

#if MWCCPSP
// 3.0.1 (0x58BC10..0x58C350): a run of nodes that each branch on the same
// variable being equal to one of a set of consecutive constants becomes one
// unsigned range test on the last node.
extern Type* func_0055CA90(Type* type); // the unsigned type of the same size

// 0x554070: cond is `var == c` (c an integer constant of a small int type);
// the variable, else NULL
Object* func_00554070(IROLinear* cond) {
    IROLinear* right;
    IROLinear* left;

    if (!cond || cond->nodetype != EEQU) {
        return NULL;
    }
    right = cond->u.diadic.right;
    if (!(right->type == IROLinearOperand &&
          right->rtype->type == TYPEINT &&
          TYPE_INTEGRAL(right->rtype)->integral < IT_17 &&
          right->u.node &&
          right->u.node->type == EINTCONST)) {
        return NULL;
    }
    left = cond->u.diadic.left;
    if (left->type == IROLinearOp1Arg &&
        left->nodetype == EINDIRECT &&
        left->u.monadic->type == IROLinearOperand &&
        left->u.monadic->u.node->type == EOBJREF) {
        return left->u.monadic->u.node->data.objref;
    }
    return NULL;
}

// the node holds nothing but nops and labels before its condition
static Boolean condonly(IRONode* fnode) {
    IROLinear* nd;

    if (fnode->numpred > 1) {
        return FALSE;
    }
    for (nd = fnode->first;
         nd != fnode->last && (nd->type == IROLinearNop || nd->type == IROLinearLabel);
         nd = nd->next) {}
    return nd == leftLeaveOf(fnode->last->u.label.cond);
}

// the node's leading nops and labels include label
static Boolean startswithlabel(IRONode* fnode, CLabel* label) {
    IROLinear* nd;

    for (nd = fnode->first; nd != fnode->last; nd = nd->next) {
        if (nd->type != IROLinearNop && nd->type != IROLinearLabel) {
            return FALSE;
        }
        if (nd->type == IROLinearLabel && nd->u.label.label == label) {
            return TRUE;
        }
    }
    return FALSE;
}

// 0x58BC80: fnode's test becomes (unsigned)(var - min) <= max - min
void func_0058BC80(IRONode* fnode, CInt64 max, CInt64 min) {
    IROLinear* cond;
    IROLinear* tc1;
    IROLinear* tc2;
    IROLinear* sum;
    IROLinear* k;
    CInt64 diff;
    Type* utype;

    cond = fnode->last->u.label.cond;
    cond->nodetype = ELESSEQU;
    diff = CInt64_Sub(max, min);
    CInt64_SetULong(&fnode->last->u.label.cond->u.diadic.right->u.node->data.intval, diff.lo);
    utype = func_0055CA90(fnode->last->u.label.cond->u.diadic.left->rtype);
    if (utype != fnode->last->u.label.cond->u.diadic.left->rtype) {
        tc1 = IRO_NewLinear(IROLinearOp1Arg);
        tc1->nodetype = ETYPCON;
        tc1->rtype = utype;
        if (utype->type == TYPEINT && TYPE_INTEGRAL(utype)->integral == IT_BOOL) {
            CError_FATAL(1029);
        }
        tc1->index = ++IRO_NumLinear;
        tc2 = IRO_NewLinear(IROLinearOp1Arg);
        *tc2 = *tc1;
        tc2->index = ++IRO_NumLinear;
    } else {
        tc1 = tc2 = NULL;
    }
    cond = fnode->last->u.label.cond;
    sum = IRO_NewLinear(IROLinearOp2Arg);
    sum->nodetype = EADD;
    sum->rtype = cond->u.diadic.left->rtype;
    sum->index = ++IRO_NumLinear;
    k = IRO_NewIntConst(CInt64_Neg(min), sum->rtype);
    if (tc1) {
        tc1->next = cond->u.diadic.left->next;
        sum->next = tc1;
    } else {
        sum->next = cond->u.diadic.left->next;
    }
    cond->u.diadic.left->next = k;
    k->next = sum;
    if (tc2) {
        tc2->next = cond->u.diadic.right->next;
        cond->u.diadic.right->next = tc2;
    }
    if (tc1) {
        tc1->u.monadic = sum;
        sum->u.diadic.left = cond->u.diadic.left;
        sum->u.diadic.right = k;
        cond->u.diadic.left = tc1;
    } else {
        sum->u.diadic.left = cond->u.diadic.left;
        sum->u.diadic.right = k;
        cond->u.diadic.left = sum;
    }
    if (tc2) {
        tc2->u.monadic = cond->u.diadic.right;
        cond->u.diadic.right = tc2;
    }
}

// 0x58BE70: the run start..end tests consecutive constants: reduce it
Boolean func_0058BE70(IRONode* start, IRONode* end) {
    IRONode* fnode;
    CInt64* vals;
    CInt64 v;
    CInt64 max;
    CInt64 min;
    int n;
    int i;

    n = 1;
    for (fnode = start; (fnode = fnode->nextnode) != end;) {
        n++;
    }
    n++;
    vals = IRO_Alloc(n * sizeof(CInt64));
    memset(vals, 0, n * sizeof(CInt64));
    max = cint64_min;
    min = cint64_max;
    for (i = 0, fnode = start; i < n; i++, fnode = fnode->nextnode) {
        v = fnode->last->u.label.cond->u.diadic.right->u.node->data.intval;
        vals[i] = v;
        if (CInt64_Greater(v, max)) {
            max = v;
        }
        if (CInt64_Less(v, min)) {
            min = v;
        }
    }
    if ((SInt32)(max.lo - min.lo + 1) != n) {
        return FALSE;
    }
    for (fnode = start; fnode != end; fnode = fnode->nextnode) {
        IRO_NopOut(fnode->last);
        IRO_NopOut(fnode->last->u.label.cond);
    }
    func_0058BC80(end, max, min);
    return TRUE;
}

// 0x58C000: fnode ends the run: `ifnot (var == c) goto` one of its successors'
// labels, followed by next
IRONode* func_0058C000(IRONode* fnode, Object* var, IRONode* next) {
    IROLinear* last;
    CLabel* label;

    last = fnode->last;
    if (fnode->numpred != 1 ||
        last->type != IROLinearIfNot ||
        func_00554070(last->u.label.cond) != var ||
        fnode->numsucc != 2 ||
        next != fnode->nextnode ||
        !condonly(fnode)) {
        return NULL;
    }
    label = last->u.label.label;
    if (!label) {
        return NULL;
    }
    if (startswithlabel(IRO_NodeTable[fnode->succ[0]], label) ||
        startswithlabel(IRO_NodeTable[fnode->succ[1]], label)) {
        return fnode;
    }
    return NULL;
}

// 0x58C140: fnode continues the run: `if (var == c) goto label` with the
// same target; the run's end
IRONode* func_0058C140(IRONode* fnode, Object* var, IRONode* target, CLabel* label) {
    IROLinear* last;
    IRONode* next;
    IRONode* r;

    last = fnode->last;
    if (fnode->numpred != 1 ||
        last->type != IROLinearIf ||
        func_00554070(last->u.label.cond) != var ||
        last->u.label.label != label ||
        fnode->numsucc != 2) {
        return NULL;
    }
    next = fnode->nextnode;
    if (!next) {
        return NULL;
    }
    if (!((next == IRO_NodeTable[fnode->succ[0]] && target == IRO_NodeTable[fnode->succ[1]]) ||
          (next == IRO_NodeTable[fnode->succ[1]] && target == IRO_NodeTable[fnode->succ[0]]))) {
        return NULL;
    }
    if (!condonly(fnode)) {
        return NULL;
    }
    if ((r = func_0058C140(next, var, target, label))) {
        return r;
    }
    return func_0058C000(next, var, target);
}

// 0x58C260: the end of the run fnode starts, or NULL
IRONode* func_0058C260(IRONode* fnode) {
    IROLinear* last;
    Object* var;
    CLabel* label;
    IRONode* next;
    IRONode* target;
    IRONode* r;

    last = fnode->last;
    if (last->type != IROLinearIf) {
        return NULL;
    }
    var = func_00554070(last->u.label.cond);
    if (!var) {
        return NULL;
    }
    label = last->u.label.label;
    if (!label || fnode->numsucc != 2) {
        return NULL;
    }
    next = fnode->nextnode;
    if (!next) {
        return NULL;
    }
    if (next == IRO_NodeTable[fnode->succ[0]] && IRO_NodeTable[fnode->succ[1]]) {
        target = IRO_NodeTable[fnode->succ[1]];
    } else {
        if (next != IRO_NodeTable[fnode->succ[1]]) {
            return NULL;
        }
        target = IRO_NodeTable[fnode->succ[0]];
        if (!target) {
            return NULL;
        }
    }
    if (!startswithlabel(target, label)) {
        return NULL;
    }
    if ((r = func_0058C140(next, var, target, label))) {
        return r;
    }
    return func_0058C000(next, var, target);
}

// 0x58BC10
int IRO_SimplifyConditionals(void) {
    IRONode* fnode;
    IRONode* end;
    int changed = FALSE;

    for (fnode = IRO_FirstNode; fnode;) {
        if (fnode->last->type == IROLinearIf &&
            (end = func_0058C260(fnode)) &&
            func_0058BE70(fnode, end)) {
            changed = TRUE;
            fnode = end->nextnode;
        } else {
            fnode = fnode->nextnode;
        }
    }
    if (changed) {
        IRO_ComputeSuccPred();
        IRO_ComputeDom();
    }
    IRO_CheckForUserBreak();
    return changed;
}
#else
int IRO_SimplifyConditionals(void) {
    IRONode* fnode;
    IRONode* start;
    IRONode* end;
    int changed = FALSE;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (fnode->last->type == IROLinearIf) {
            start = end = fnode;
            while (fnode->nextnode && fnode->nextnode->last->type == IROLinearIf) {
                end = fnode = fnode->nextnode;
            }
            if (start != end && ReduceConsecutiveIf(start, end)) {
                changed = TRUE;
            }
        }
    }

    if (changed) {
        IRO_ComputeSuccPred();
        IRO_ComputeDom();
    }
    IRO_CheckForUserBreak();
    return changed;
}
#endif

Boolean IRO_EvaluateDefinitePointers(Object* func) {
    IROLinear* nd;
    Boolean result;
    Boolean changed;
    Boolean changed2;
    IROLinear* nd2;
    IROListNode* scan;
    IROListNode* list;

    if (!copts.opt_pointer_analysis) {
        return FALSE;
    }

    result = FALSE;

    do {
        changed = FALSE;

        for (nd = IRO_FirstLinear; nd; nd = nd->next) {
            if (nd->type == IROLinearOp1Arg &&
                nd->nodetype == EINDIRECT &&
                !(nd->flags & IROLF_Assigned) &&
                nd->pointsToFunction &&
                !IRO_HasSideEffect(nd) &&
                PointerAnalysis_IsLinearNodePointerExprDefinite(func, nd)) {
                list = NULL;
                PointerAnalysis_LookupLinearNodePointerExpr(func, nd, &list);
                if (list) {
                    if (list->list.head && list->list.tail && !list->nextList) {
                        changed2 = IRO_LocateFather_Cut_And_Paste(nd, list->list.tail) != NULL;
                        if (changed2) {
                            IRO_PasteAfter(list->list.head, list->list.tail, nd);
                            for (nd2 = list->list.head; nd2 != list->list.tail->next;
                                 nd2 = nd2->next) {
                                if (nd2->type == IROLinearOperand && nd2->u.node->type == EOBJREF) {
                                    Object* obj = nd2->u.node->data.objref;
                                    if (Inline_IsObjectData(obj) || obj->datatype == DLOCAL) {
                                        IRO_FindVar(obj, TRUE, TRUE);
                                    } else {
                                        obj->varptr = NULL;
                                    }
                                }
                            }
                        }
                        changed |= changed2;
                    }

                    while (list) {
                        scan = list->nextList;
                        IRO_free(list);
                        list = scan;
                    }
                }
            }
        }

        result |= changed;
        IRO_CheckForUserBreak();
    } while (changed);

    return result;
}
#if MWCCPSP

#endif
#if MWCCPSP
// 3.0.1 (0x57B510): calls to known C library functions are looked up by name
// in a table of 480 descriptors (sorted, bsearch); a descriptor whose header
// is enabled may evaluate the call, giving replacement code or nothing.
// The table and the evaluators are still the original's (by address).
typedef struct IROStdHeader {
    char* name;
    Boolean enabled;
} IROStdHeader;
typedef struct IROStdFunc {
    char* name;
    IROStdHeader* header;
    void* handler;
    Boolean (*evaluate)(IROLinear* call, IROList* result);
} IROStdFunc;
extern IROStdFunc IRO_StdFuncs[480];
extern Boolean IRO_IsFunctionObject(Object* obj);
extern void IRO_ResetList(IROList* list);
extern Boolean CScope_IsStdNameSpace(NameSpace* nspace);

static int StdFuncCompare(const void* key, const void* elem) {
    return strcmp((const char*)key, ((const IROStdFunc*)elem)->name);
}

void IRO_EvaluateStandardFunctionCalls(IROLinear* linear) {
    Object* obj;
    char* name;
    IROStdFunc* entry;
    IROList list;

    for (; linear; linear = linear->next) {
        if (linear->type == IROLinearFunccall &&
            !(linear->u.funccall.functype->flags & FUNC_INTRINSIC) &&
            linear->u.funccall.callee->type == IROLinearOperand &&
            linear->u.funccall.callee->u.node->type == EOBJREF &&
            (obj = linear->u.funccall.callee->u.node->data.objref)) {
            if (!obj ||
                !obj->name ||
                !IRO_IsFunctionObject(obj) ||
                (obj->sclass != 0 && obj->sclass != TK_EXTERN) ||
                (obj->qual & 0x80000) ||
                (copts.cplusplus &&
                 !CScope_IsStdNameSpace(obj->nspace) &&
                 obj->nspace != cscope_root)) {
                name = NULL;
            } else {
                name = obj->name->name;
            }
            if (name) {
                entry = bsearch(name, IRO_StdFuncs, 480, sizeof(IROStdFunc), StdFuncCompare);
            } else {
                entry = NULL;
            }
            if (entry && entry->evaluate && entry->header && entry->header->enabled) {
                IRO_InitList(&list);
                if (entry->evaluate(linear, &list)) {
                    if (!list.tail) {
                        IRO_NopOut(linear);
                    } else {
                        IRO_PasteAfter(list.head, list.tail, linear);
                        IRO_LocateFather_Cut_And_Paste(linear, list.tail);
                    }
                }
                IRO_ResetList(&list);
            }
        }
    }
}

// The variable C library functions set on error (errno), named by a pragma.
extern char data_00608B58[256];
extern Boolean data_00608C58;
extern void func_0057B6B0(void); // the handler of functions that set only errno

// 0x57B6C0: the errno variable among the function's variables
VarRecord* func_0057B6C0(void) {
    VarRecord* var;
    Object* obj;

    for (var = IRO_FirstVar; var; var = var->next) {
        if ((obj = var->object) &&
            (obj->datatype == DDATA || obj->datatype == DABSOLUTE) &&
            obj->sclass != TK_STATIC &&
            (!copts.cplusplus ||
             CScope_IsStdNameSpace(obj->nspace) ||
             obj->nspace == cscope_root) &&
            obj->name &&
            !strcmp(obj->name->name, data_00608B58)) {
            return var;
        }
    }
    return NULL;
}

// 0x57B770: errno has a name
Boolean func_0057B770(void) {
    return data_00608C58 && strlen(data_00608B58) != 0;
}

// 0x57B7A0: FALSE for a known library function that changes nothing but errno
Boolean func_0057B7A0(Object* obj) {
    char* name;
    IROStdFunc* entry;

    if (!obj ||
        !obj->name ||
        !IRO_IsFunctionObject(obj) ||
        (obj->sclass != 0 && obj->sclass != TK_EXTERN) ||
        (obj->qual & 0x80000) ||
        (copts.cplusplus && !CScope_IsStdNameSpace(obj->nspace) && obj->nspace != cscope_root)) {
        name = NULL;
    } else {
        name = obj->name->name;
    }
    entry = name ? bsearch(name, IRO_StdFuncs, 480, sizeof(IROStdFunc), StdFuncCompare) : NULL;
    if (entry &&
        entry->header &&
        entry->header->enabled &&
        entry->handler == (void*)func_0057B6B0) {
        return FALSE;
    }
    return TRUE;
}
#endif
