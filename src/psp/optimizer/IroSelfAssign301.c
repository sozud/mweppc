// mwccpsp 3.0.1: folding a self-assignment (x op= c) into the expression that
// uses its result (0x566AC0..0x568330; IroPropagate.c by its assert).
// 3.0.1's folders compute in CInt64 and either truncate the result to the
// constant's type or refuse a result that does not fit it; the shift/multiply
// rewrites check the same way, and a refused fold falls through to building
// "x op c" instead of storing a wrapped constant.
#include "compiler/common.h"
#include "compiler/CError.h"
#include "compiler/CInt64.h"
#include "compiler/CParser.h"
#include "compiler/IroEval.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IROUseDef.h"
#include "compiler/IroUtil.h"
#include "compiler/enode.h"
#include "compiler/types.h"

typedef Boolean (*AssignmentFoldingFunc)(
    CInt64 a, CInt64 b, Type* type, Type* type2, CInt64* result
);
extern AssignmentFoldingFunc data_006078C8[IRO_MAXEXPR]; // IROUseDef.c's AssignmentFoldingFunction

extern CInt64 func_0055A670(Type* type); // largest value of the type (IroRange301.c)
extern CInt64 func_0055A710(Type* type); // smallest value of the type (IroRange301.c)
extern Type* func_0055A620(Type* type); // integral promotion (IroLogicalSelection.c)
extern IROLinear* IRO_MakeTypeConversion(IROLinear* linear, Type* type, IROList* list);

// inlined everywhere: does v fit type (unsigned or signed compare by type)
static Boolean IroSelfAssign_FitsType(CInt64 v, Type* type) {
    if (is_unsigned(type)) {
        return CInt64_LessEqualU(v, func_0055A670(type));
    }
    if (CInt64_GreaterEqual(v, func_0055A710(type)) && CInt64_LessEqual(v, func_0055A670(type))) {
        return TRUE;
    }
    return FALSE;
}

// 0x555B60: is linear an integer constant 2^n (n >= 0, so 1 counts with
// n = 0); 64-bit, unlike 2.4.7's IRO_IsPow2. *powvalue is 0 on failure.
#if MWCCPS2
// b210 inlines IRO_IsIntConstant here
static inline Boolean IsIntConstant210(IROLinear* linear) {
    if (linear->type == IROLinearOperand &&
        IS_TYPE_INT(linear->rtype) &&
        TYPE_INTEGRAL(linear->rtype)->integral < 17 &&
        linear->u.node &&
        linear->u.node->type == EINTCONST) {
        return TRUE;
    }
    return FALSE;
}
#define POW2_ISINTCONST IsIntConstant210
#else
#define POW2_ISINTCONST IRO_IsIntConstant
#endif
#if MWCCPS2
__declspec(noinline) // b210 calls it (UpdateUseForOtherSelfAssignment)
#endif
Boolean func_00555B60(IROLinear* linear, int* powvalue) {
    CInt64 value;
    int n;

    *powvalue = -1;
    if (POW2_ISINTCONST(linear)) {
        value = linear->u.node->data.intval;
        n = 0;
        if (!CInt64_IsZero(&value)) {
            do {
                if (CInt64_Equal(CInt64_And(value, cint64_one), cint64_one)) {
                    if (CInt64_IsOne(&value)) {
                        *powvalue = n;
                        return TRUE;
                    }
                    break;
                }
                value = CInt64_ShrU(value, cint64_one);
                n++;
            } while (!CInt64_IsZero(&value));
        }
    }
    *powvalue = 0;
    return FALSE;
}

// 0x567FC0
Boolean IROUseDef_FoldOr(CInt64 a, CInt64 b, Type* type, Type* type2, CInt64* result) {
    CInt64 v = CInt64_Or(a, b);
    IRO_TruncateValueToType(&v, type);
    *result = v;
    return TRUE;
}

// 0x568010
Boolean IROUseDef_FoldXor(CInt64 a, CInt64 b, Type* type, Type* type2, CInt64* result) {
    CInt64 v = CInt64_Xor(a, b);
    IRO_TruncateValueToType(&v, type);
    *result = v;
    return TRUE;
}

// 0x568060
Boolean IROUseDef_FoldAnd(CInt64 a, CInt64 b, Type* type, Type* type2, CInt64* result) {
    CInt64 v = CInt64_And(a, b);
    IRO_TruncateValueToType(&v, type);
    *result = v;
    return TRUE;
}

// 0x5680B0: shift counts add; for a signed shifted type two in-range counts
// saturate at width - 1. type2 is the self-assignment's type.
Boolean IROUseDef_FoldShr(CInt64 a, CInt64 b, Type* type, Type* type2, CInt64* result) {
    CInt64 v;
    CInt64 lim;

    v = CInt64_Add(a, b);
    if (!is_unsigned(type2)) {
        CInt64_SetLong(&lim, type2->size * 8);
        if (CInt64_LessU(a, lim) && CInt64_LessU(b, lim) && CInt64_GreaterEqualU(v, lim)) {
            v = CInt64_Sub(lim, cint64_one);
        }
    }
    if (IroSelfAssign_FitsType(v, type)) {
        *result = v;
        return TRUE;
    }
    return FALSE;
}

// 0x568210
Boolean IROUseDef_FoldAdd(CInt64 a, CInt64 b, Type* type, Type* type2, CInt64* result) {
    CInt64 v = CInt64_Add(a, b);
    IRO_TruncateValueToType(&v, type);
    *result = v;
    return TRUE;
}

// 0x568260
Boolean IROUseDef_FoldShl(CInt64 a, CInt64 b, Type* type, Type* type2, CInt64* result) {
    CInt64 v = CInt64_Add(a, b);
    if (IroSelfAssign_FitsType(v, type)) {
        *result = v;
        return TRUE;
    }
    return FALSE;
}

// 0x568320
Boolean IROUseDef_FoldMul(CInt64 a, CInt64 b, Type* type, Type* type2, CInt64* result) {
    CInt64 v = CInt64_Mul(a, b);
    if (IroSelfAssign_FitsType(v, type)) {
        *result = v;
        return TRUE;
    }
    return FALSE;
}

// 0x566AC0: a is a use of the variable b (x op= c) defined; fold c into a's
// father "a op2 c2" when possible, else replace a by "a op c".
// The original inlines the folders into the switch cases.
#if MWCCPS2 && MWCCPS2_BUILD < 210
#define B145_M6 -6 // b145 asserts
#else
#define B145_M6
#endif
void UpdateUseForOtherSelfAssignment(IROLinear* a, IROLinear* b, Type* type) {
    CInt64 val;
    CInt64 fatherval;
    CInt64 folded;
    CInt64 v;
    CInt64 t;
    IROLinear* father;
    IROLinear* right;
    ENode* constnode;
    Type* ctype;
    AssignmentFoldingFunc fold;
    int powvalue;
    Type* promoted;
    IROLinear* operand;
    IROLinear* repl;
    IROLinear* newdiadic;
    IROList list;

    val = IRO_GetSelfAssignmentVal(b);
    if ((father = IRO_LocateFather(a)) &&
        father->type == IROLinearOp2Arg &&
        (right = father->u.diadic.right) &&
        IRO_IsIntConstant(right)) {
        ctype = right->rtype;
        constnode = right->u.node;
        fatherval = constnode->data.intval;

        fold = data_006078C8[b->nodetype];
        if (fold &&
            (father->nodetype == b->nodetype ||
             father->nodetype == IRO_NonAssignmentOp[b->nodetype]) &&
            fold(fatherval, val, ctype, b->rtype, &folded)) {
            constnode->data.intval = folded;
            return;
        }

        switch (b->nodetype) {
            case EMULASS:
                if (father->nodetype == ESHL || father->nodetype == ESHLASS) {
                    // x *= 2^n; x << c  ->  x << (n + c)
                    Boolean ispow2 = func_00555B60(b->u.diadic.right, &powvalue);
                    CInt64_SetLong(&v, powvalue);
                    if (ispow2 &&
                        powvalue > 0 &&
                        IROUseDef_FoldShl(v, fatherval, ctype, b->rtype, &folded)) {
                        constnode->data.intval = folded;
                        return;
                    }
                    // x * (c' * (1 << c))
                    t = CInt64_Shl(cint64_one, fatherval);
                    if (IroSelfAssign_FitsType(t, ctype) &&
                        IROUseDef_FoldMul(t, val, ctype, b->rtype, &folded)) {
                        father->nodetype = (father->nodetype == ESHL) ? EMUL : EMULASS;
                        constnode->data.intval = folded;
                        return;
                    }
                }
                break;
            case EDIVASS:
                if ((father->nodetype == ESHR || father->nodetype == ESHRASS) &&
                    is_unsigned(father->rtype)) {
                    Boolean ispow2 = func_00555B60(b->u.diadic.right, &powvalue);
                    CInt64_SetLong(&v, powvalue);
                    if (ispow2 &&
                        powvalue > 0 &&
                        IROUseDef_FoldShl(v, fatherval, ctype, b->rtype, &folded)) {
                        constnode->data.intval = folded;
                        return;
                    }
                    t = CInt64_Shl(cint64_one, fatherval);
                    if (IroSelfAssign_FitsType(t, ctype) &&
                        IROUseDef_FoldMul(t, val, ctype, b->rtype, &folded)) {
                        father->nodetype = (father->nodetype == ESHR) ? EDIV : EDIVASS;
                        constnode->data.intval = folded;
                        return;
                    }
                }
                break;
            case ESHLASS:
                if (father->nodetype == EMUL || father->nodetype == EMULASS) {
                    Boolean ispow2 = func_00555B60(right, &powvalue);
                    CInt64_SetLong(&v, powvalue);
                    if (ispow2 &&
                        powvalue > 0 &&
                        IROUseDef_FoldShl(v, val, ctype, b->rtype, &folded)) {
                        father->nodetype = (father->nodetype == EMUL) ? ESHL : ESHLASS;
                        constnode->data.intval = folded;
                        return;
                    }
                    t = CInt64_Shl(cint64_one, val);
                    if (IroSelfAssign_FitsType(t, ctype) &&
                        IROUseDef_FoldMul(fatherval, t, ctype, b->rtype, &folded)) {
                        constnode->data.intval = folded;
                        return;
                    }
                } else if (father->nodetype == ESHR || father->nodetype == ESHRASS) {
                    if (CInt64_Equal(fatherval, val) && is_unsigned(father->rtype)) {
                        father->nodetype = (father->nodetype == ESHR) ? EAND : EANDASS;
#if MWCCPS2 // the 128-bit CInt64
                        if (father->rtype->size * 8 > 128) {
                            CError_Internal("IroPropagate.c", CError_B145_LINE(2672) B145_M6);
                        }
                        CInt64_SetLong(&v, 128 - father->rtype->size * 8);
#else
                        if (father->rtype->size * 8 > 64) {
                            CError_Internal("IroPropagate.c", CError_B145_LINE(2672) B145_M6);
                        }
                        CInt64_SetLong(&v, 64 - father->rtype->size * 8);
#endif
                        v = CInt64_Add(val, v);
                        folded = CInt64_ShrU(cint64_negone, v);
                        IRO_TruncateValueToType(&folded, ctype);
                        constnode->data.intval = folded;
                        return;
                    }
                }
                break;
            case ESHRASS:
                if ((father->nodetype == EDIV || father->nodetype == EDIVASS) &&
                    is_unsigned(father->rtype)) {
                    Boolean ispow2 = func_00555B60(right, &powvalue);
                    CInt64_SetLong(&v, powvalue);
                    if (ispow2 &&
                        powvalue > 0 &&
                        IROUseDef_FoldShr(v, val, ctype, b->rtype, &folded)) {
                        father->nodetype = (father->nodetype == EDIV) ? ESHR : ESHRASS;
                        constnode->data.intval = folded;
                        return;
                    }
                    t = CInt64_Shl(cint64_one, val);
                    if (IroSelfAssign_FitsType(t, ctype) &&
                        IROUseDef_FoldMul(fatherval, t, ctype, b->rtype, &folded)) {
                        constnode->data.intval = folded;
                        return;
                    }
                } else if (father->nodetype == ESHL || father->nodetype == ESHLASS) {
                    if (CInt64_Equal(fatherval, val)) {
                        father->nodetype = (father->nodetype == ESHL) ? EAND : EANDASS;
                        folded = CInt64_Shl(cint64_negone, val);
                        IRO_TruncateValueToType(&folded, ctype);
                        constnode->data.intval = folded;
                        return;
                    }
                }
                break;
            default:
                break;
        }
    }

    IRO_InitList(&list);
    promoted = type;
    if ((type->type == TYPEINT && TYPE_INTEGRAL(type)->integral < 17) || type->type == TYPEENUM) {
        promoted = func_0055A620(type);
    }
    operand = a;
    if (promoted != type) {
        operand = IRO_MakeTypeConversion(a, promoted, &list);
    }
    repl = IRO_NewIntConst(val, promoted);
    IRO_AddToList(repl, &list);
    newdiadic = IRO_NewLinear(IROLinearOp2Arg);
    newdiadic->index = ++IRO_NumLinear;
    newdiadic->nodetype = IRO_NonAssignmentOp[b->nodetype];
    newdiadic->u.diadic.left = operand;
    newdiadic->u.diadic.right = repl;
    newdiadic->rtype = IRO_UnqualifiedType(promoted);
    IRO_AddToList(newdiadic, &list);
    if (promoted != type) {
        newdiadic = IRO_MakeTypeConversion(newdiadic, type, &list);
    }
    IRO_LocateFather_Cut_And_Paste_Without_Nopping(a, newdiadic);
    IRO_PasteAfter(list.head, list.tail, a);
}
