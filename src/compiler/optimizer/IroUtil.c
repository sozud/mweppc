#include "compiler/IroUtil.h"
#if MWCCPSP
#include "compiler/IrOptimizer.h"
#endif
#include "compiler/IroCSE.h"
#include "compiler/IroDump.h"
#include "compiler/IroFlowgraph.h"
#if MWCCPSP
#include "compiler/IroEval.h"
#endif
#include "compiler/IroLinearForm.h"
#if MWCCPSP
#include "compiler/IroMalloc.h"
#endif
#include "compiler/IroVars.h"
#include "compiler/CError.h"
#include "compiler/CException.h"
#include "compiler/CExpr.h"
#include "compiler/CFunc.h"
#include "compiler/CInt64.h"
#include "compiler/CMachine.h"
#include "compiler/CompilerTools.h"
#include "compiler/CParser.h"
#include "compiler/CPrep.h"
#include "compiler/enode.h"
#include "compiler/Exceptions.h"
#include "compiler/InlineAsmPPC.h"
#include "compiler/objects.h"
#include "compiler/types.h"
#include "driver/CCompiler.h"
#include "driver/COSToolsCLT.h"
#include "driver/CWPluginsPrivate.h"
#if MWCCPSP
#include <string.h>
#include <setjmp.h>
#include "compiler/BitVector.h"
#endif

static UInt32 IRO_LastUserBreakTick;
static IROLinear* ExprStart;
static IROLinear* ExprEnd;
static IRONode* IRO_Node;
static SInt32 FuncLevel;
Object* FunctionName;
Boolean IRO_IsLeafFunction;
Boolean IRO_FunctionHasReturn;
Boolean DisableDueToAsm;
Boolean LoopOptimizerRun;

Object* IRO_IsVariable(IROLinear* linear) {
    if (linear->type == IROLinearOp1Arg &&
        linear->nodetype == EINDIRECT &&
        linear->u.monadic->type == IROLinearOperand &&
        linear->u.monadic->u.node->type == EOBJREF) {
        return linear->u.monadic->u.node->data.objref;
    }

    return NULL;
}

Boolean IRO_IsConstant(IROLinear* linear) {
    if (linear->type == IROLinearOperand &&
        ENODE_IS3(linear->u.node, EINTCONST, EVECTOR128CONST, EFLOATCONST)) {
        return TRUE;
    }
    return FALSE;
}

Boolean IRO_IsPow2(IROLinear* linear, int* powvalue) {
    UInt32 desired;
    UInt32 value;
    UInt32 i;

    *powvalue = -1;
    if (linear->type == IROLinearOperand && linear->u.node->type == EINTCONST) {
        if (linear->u.node->data.intval.hi) {
            return FALSE;
        }

        desired = CInt64_GetULong(&linear->u.node->data.intval);
        value = 2;
        for (i = 1; i < 31; i++) {
            if (value == desired) {
                *powvalue = i;
                return TRUE;
            }
            value += value;
        }
    }

    return FALSE;
}

#if MWCCPSP
// 3.0.1 (0x555B00) also wants an integer type no wider than the
// floating-point kinds start (integral < 17).
Boolean IRO_IsIntConstant(IROLinear* linear) {
    if (linear->type == IROLinearOperand &&
        IS_TYPE_INT(linear->rtype) &&
        TYPE_INTEGRAL(linear->rtype)->integral < 17 &&
        linear->u.node &&
        linear->u.node->type == EINTCONST) {
        return TRUE;
    }
    return FALSE;
}
#else
Boolean IRO_IsIntConstant(IROLinear* linear) {
    if (linear->type == IROLinearOperand && ENODE_IS(linear->u.node, EINTCONST)) {
        return TRUE;
    }
    return FALSE;
}
#endif

Boolean IRO_IsFloatConstant(IROLinear* linear) {
    if (linear->type == IROLinearOperand && ENODE_IS(linear->u.node, EFLOATCONST)) {
        return TRUE;
    }
    return FALSE;
}

Boolean IRO_IsVector128Constant(IROLinear* linear) {
    if (linear->type == IROLinearOperand && ENODE_IS(linear->u.node, EVECTOR128CONST)) {
        return TRUE;
    }
    return FALSE;
}

Boolean IRO_IsAssignment(IROLinear* linear) {
    if (linear->type == IROLinearOp1Arg || linear->type == IROLinearOp2Arg) {
        if (IRO_IsAssignOp[linear->nodetype]) {
            return TRUE;
        }
    }
    return FALSE;
}

static Boolean IRO_OperandsSame(ENode* a, ENode* b) {
    if (a->type == b->type) {
        switch (a->type) {
            case EINTCONST:
                return CInt64_Equal(a->data.intval, b->data.intval);
            case ESTRINGCONST:
                return FALSE;
            case EFLOATCONST:
                return a->data.floatval.data.value == b->data.floatval.data.value;
            case EVECTOR128CONST:
                return (a->data.vector128val.ul[0] == b->data.vector128val.ul[0]) &&
                    (a->data.vector128val.ul[1] == b->data.vector128val.ul[1]) &&
                    (a->data.vector128val.ul[2] == b->data.vector128val.ul[2]) &&
                    (a->data.vector128val.ul[3] == b->data.vector128val.ul[3]);
            case EOBJREF:
                return a->data.objref == b->data.objref;
            default:
                break;
        }
    }

    return FALSE;
}

#if MWCCPSP
// 3.0.1 (0x55CC70): identical types first
Boolean IRO_TypesEqual(Type* a, Type* b) {
    if (a == b) {
        return TRUE;
    }
    if (IS_TYPE_BITFIELD(a)) {
        if (IS_TYPE_BITFIELD(b) &&
            TYPE_BITFIELD(a)->bitfieldtype == TYPE_BITFIELD(b)->bitfieldtype &&
            TYPE_BITFIELD(a)->offset == TYPE_BITFIELD(b)->offset &&
            TYPE_BITFIELD(a)->bitlength == TYPE_BITFIELD(b)->bitlength) {
            return TRUE;
        }
        return FALSE;
    }
    if (IS_TYPE_POINTER_ONLY(a) && IS_TYPE_POINTER_ONLY(b)) {
        return TRUE;
    }
    return is_typeequal(a, b) != 0;
}
#else
Boolean IRO_TypesEqual(Type* a, Type* b) {
    TypeBitfield* ba;
    TypeBitfield* bb;

    if (IS_TYPE_BITFIELD(a)) {
        if (IS_TYPE_BITFIELD(b)) {
            ba = TYPE_BITFIELD(a);
            bb = TYPE_BITFIELD(b);
            if (ba->bitfieldtype == bb->bitfieldtype &&
                ba->offset == bb->offset &&
                ba->bitlength == bb->bitlength) {
                return TRUE;
            }
        }
        return FALSE;
    }

    if (IS_TYPE_POINTER_ONLY(a) && IS_TYPE_POINTER_ONLY(b)) {
        return TRUE;
    }

    return !!is_typeequal(a, b);
}
#endif

Type* IRO_UnsignedType(Type* type) {
    TypeIntegral* t = TYPE_INTEGRAL(type);
    if (IS_TYPE_ENUM(t) || IS_TYPE_POINTER_ONLY(t)) {
        if (t->size == stunsignedchar.size) {
            return TYPE(&stunsignedchar);
        }
        if (t->size == stunsignedint.size) {
            return TYPE(&stunsignedint);
        }
        if (t->size == stunsignedshort.size) {
            return TYPE(&stunsignedshort);
        }
        if (t->size == stunsignedlong.size) {
            return TYPE(&stunsignedlong);
        }
        if (t->size == stunsignedlonglong.size) {
            return TYPE(&stunsignedlonglong);
        }
        CError_FATAL(281);
        return NULL;
    }

    if (!IS_TYPE_INT(t)) {
        CError_FATAL(287);
        return NULL;
    }

    if (t == &stbool || t == &stwchar) {
        return type;
    }

    if (t == &stchar || t == &stsignedchar || t == &stunsignedchar) {
        return TYPE(&stunsignedchar);
    }
    if (t == &stsignedshort || t == &stunsignedshort) {
        return TYPE(&stunsignedshort);
    }
    if (t == &stsignedint || t == &stunsignedint) {
        return TYPE(&stunsignedint);
    }
    if (t == &stsignedlong || t == &stunsignedlong) {
        return TYPE(&stunsignedlong);
    }
    if (t == &stsignedlonglong || t == &stunsignedlonglong) {
        return TYPE(&stunsignedlonglong);
    }

    CError_FATAL(319);
    return NULL;
}

Type* IRO_SignedType(Type* type) {
    if (IS_TYPE_ENUM(type) || IS_TYPE_POINTER_ONLY(type)) {
        if (type->size == stsignedchar.size) {
            return TYPE(&stsignedchar);
        }
        if (type->size == stsignedint.size) {
            return TYPE(&stsignedint);
        }
        if (type->size == stsignedshort.size) {
            return TYPE(&stsignedshort);
        }
        if (type->size == stsignedlong.size) {
            return TYPE(&stsignedlong);
        }
        if (type->size == stsignedlonglong.size) {
            return TYPE(&stsignedlonglong);
        }
        CError_FATAL(357);
        return NULL;
    }

    if (!IS_TYPE_INT(type)) {
        CError_FATAL(363);
        return NULL;
    }

    if (type == TYPE(&stbool) && type->size == stsignedchar.size) {
        return TYPE(&stsignedchar);
    }

    if (type == TYPE(&stwchar) && type->size == stsignedshort.size) {
        return TYPE(&stsignedshort);
    }

    if (type == TYPE(&stchar) || type == TYPE(&stsignedchar) || type == TYPE(&stunsignedchar)) {
        return TYPE(&stsignedchar);
    }
    if (type == TYPE(&stsignedshort) || type == TYPE(&stunsignedshort)) {
        return TYPE(&stsignedshort);
    }
    if (type == TYPE(&stsignedint) || type == TYPE(&stunsignedint)) {
        return TYPE(&stsignedint);
    }
    if (type == TYPE(&stsignedlong) || type == TYPE(&stunsignedlong)) {
        return TYPE(&stsignedlong);
    }
    if (type == TYPE(&stsignedlonglong) || type == TYPE(&stunsignedlonglong)) {
        return TYPE(&stsignedlonglong);
    }

    CError_FATAL(399);
    return NULL;
}

Boolean IRO_is_CPtypeequal(Type* a, Type* b) {
    if (IS_TYPE_POINTER_ONLY(a) && IS_TYPE_POINTER_ONLY(b)) {
        return TRUE;
    }

    return !!is_typeequal(a, b);
}

#if MWCCPSP
// 3.0.1 (0x555880): the node flags must agree too (two typecasts may differ in
// 0x80), and the operands compare inline: a float zero is never the same,
// strings are compared by contents
Boolean IRO_ExprsSame(IROLinear* a, IROLinear* b) {
    UInt16 mask;
    ENode* na;
    ENode* nb;

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
            return IRO_ExprsSame(a->u.monadic, b->u.monadic);
        case IROLinearOp2Arg:
            if (a->nodetype != b->nodetype) {
                return FALSE;
            }
            return IRO_ExprsSame(a->u.diadic.left, b->u.diadic.left) &&
                IRO_ExprsSame(a->u.diadic.right, b->u.diadic.right);
        case IROLinearOp3Arg:
            if (a->nodetype != b->nodetype) {
                return FALSE;
            }
            return IRO_ExprsSame(a->u.args3.a, b->u.args3.a) &&
                IRO_ExprsSame(a->u.args3.b, b->u.args3.b) &&
                IRO_ExprsSame(a->u.args3.c, b->u.args3.c);
        default:
            return FALSE;
    }
}
#else
Boolean IRO_ExprsSame(IROLinear* a, IROLinear* b) {
    if (a->type == b->type && IRO_TypesEqual(a->rtype, b->rtype)) {
        switch (a->type) {
            case IROLinearOperand:
                return IRO_OperandsSame(a->u.node, b->u.node);
            case IROLinearOp1Arg:
                if (a->nodetype == b->nodetype) {
                    return IRO_ExprsSame(a->u.monadic, b->u.monadic);
                }
                return FALSE;
            case IROLinearOp2Arg:
                if (a->nodetype == b->nodetype) {
                    return IRO_ExprsSame(a->u.diadic.left, b->u.diadic.left) &&
                        IRO_ExprsSame(a->u.diadic.right, b->u.diadic.right);
                }
                return FALSE;
            case IROLinearOp3Arg:
                if (a->nodetype == b->nodetype) {
                    return IRO_ExprsSame(a->u.args3.a, b->u.args3.a) &&
                        IRO_ExprsSame(a->u.args3.b, b->u.args3.b) &&
                        IRO_ExprsSame(a->u.args3.c, b->u.args3.c);
                }
                return FALSE;
            case IROLinearFunccall:
                return FALSE;
            default:
                return FALSE;
        }
    }

    return FALSE;
}
#endif

CLabel* IRO_NewLabel(void) {
    CLabel* label = newlabel();
    label->next = Labels;
    Labels = label;
    return label;
}

Boolean IRO_ExprsSameSemantically(IROLinear* a, IROLinear* b) {
    if (a->type == b->type && IRO_TypesEqual(a->rtype, b->rtype)) {
        Boolean swappedSame = FALSE;

        switch (a->type) {
            case IROLinearOperand:
                return IRO_OperandsSame(a->u.node, b->u.node);
            case IROLinearOp1Arg:
                if (a->nodetype == b->nodetype) {
                    return IRO_ExprsSameSemantically(a->u.monadic, b->u.monadic);
                }
                return FALSE;
            case IROLinearOp2Arg:
                if (a->nodetype == b->nodetype) {
                    switch (a->nodetype) {
                        case EMUL:
                        case EADD:
                        case EAND:
                        case EXOR:
                        case EOR:
                        case ELAND:
                        case ELOR:
                            if (!IRO_HasSideEffect(a)) {
                                swappedSame = IRO_ExprsSameSemantically(
                                                  a->u.diadic.left, b->u.diadic.right
                                              ) &&
                                    IRO_ExprsSameSemantically(a->u.diadic.right, b->u.diadic.left);
                            }
                            break;
                        default:
                            break;
                    }

                    return swappedSame ||
                        (IRO_ExprsSameSemantically(a->u.diadic.left, b->u.diadic.left) &&
                         IRO_ExprsSameSemantically(a->u.diadic.right, b->u.diadic.right));
                }
                return FALSE;
            case IROLinearOp3Arg:
                if (a->nodetype == b->nodetype) {
                    return IRO_ExprsSameSemantically(a->u.args3.a, b->u.args3.a) &&
                        IRO_ExprsSameSemantically(a->u.args3.b, b->u.args3.b) &&
                        IRO_ExprsSameSemantically(a->u.args3.c, b->u.args3.c);
                }
                return FALSE;
            case IROLinearFunccall:
                return FALSE;
            default:
                return FALSE;
        }
    }

    return FALSE;
}

IROLinear* IRO_FindPrecedAfter(IROLinear* a, IROLinear* iter) {
    IROLinear* prev = iter;
    while (iter && iter != a) {
        prev = iter;
        iter = iter->next;
    }

    if (iter) {
        return prev;
    }
    return iter;
}

IROLinear* IRO_FindPreced(IROLinear* a) {
    IROLinear* iter;
    IROLinear* prev;

    prev = iter = IRO_FirstLinear;
    while (iter && iter != a) {
        prev = iter;
        iter = iter->next;
    }

    if (iter) {
        return prev;
    }
    return iter;
}

static IROLinear* FindFirst(IROLinear* linear) {
    short i;

    switch (linear->type) {
        case IROLinearOperand:
            return linear;
        case IROLinearOp1Arg:
            return FindFirst(linear->u.monadic);
        case IROLinearOp2Arg:
            if (linear->u.diadic.right->index < linear->u.diadic.left->index) {
                return FindFirst(linear->u.diadic.right);
            } else {
                return FindFirst(linear->u.diadic.left);
            }
        case IROLinearOp3Arg:
            if (linear->u.args3.a->index < linear->u.args3.b->index) {
                if (linear->u.args3.a->index < linear->u.args3.c->index) {
                    return FindFirst(linear->u.args3.a);
                } else {
                    return FindFirst(linear->u.args3.c);
                }
            } else {
                if (linear->u.args3.b->index < linear->u.args3.c->index) {
                    return FindFirst(linear->u.args3.b);
                } else {
                    return FindFirst(linear->u.args3.c);
                }
            }
        case IROLinearFunccall:
            i = linear->u.funccall.argCount - 1;
            return FindFirst(linear->u.funccall.args[i]);
        default:
            CError_FATAL(641);
            return NULL;
    }
}

IROLinear* IRO_FindFirst(IROLinear* linear) {
    IRO_RenumberInts();
    return FindFirst(linear);
}

void IRO_CutAndPasteAfter(IROLinear* first, IROLinear* last, IROLinear* after) {
    IROLinear* preced = IRO_FindPreced(first);
#if MWCCPSP
    // 3.0.1 (0x554300): first must be in the list
    if (!preced) {
        CError_FATAL(2195);
    }
#endif
    preced->next = last->next;
    last->next = after->next;
    after->next = first;
}

Boolean IRO_IsConstantZero(IROLinear* linear) {
    return (IRO_IsIntConstant(linear) && CInt64_IsZero(&linear->u.node->data.intval)) ||
        (IRO_IsFloatConstant(linear) && CMach_FloatIsZero(linear->u.node->data.floatval));
}

Boolean IRO_IsConstantOne(IROLinear* linear) {
    return (IRO_IsIntConstant(linear) && CInt64_IsOne(&linear->u.node->data.intval)) ||
        (IRO_IsFloatConstant(linear) && CMach_FloatIsOne(linear->u.node->data.floatval));
}

Boolean IRO_IsConstantNegativeOne(IROLinear* linear) {
    CInt64 neg = CInt64_Neg(linear->u.node->data.intval);
    return (IRO_IsIntConstant(linear) && CInt64_IsOne(&neg)) ||
        (IRO_IsFloatConstant(linear) && CMach_FloatIsNegOne(linear->u.node->data.floatval));
}

static void ActNop(IROLinear* linear, Boolean isEntry) {
    if (!isEntry) {
        linear->type = IROLinearNop;
        linear->expr = NULL;
    }
}

void IRO_NopOut(IROLinear* linear) {
    IRO_WalkTree(linear, ActNop);
}

static Boolean NotNoppable(IROLinear* linear) {
    return (linear->type == IROLinearFunccall) ||
        IRO_IsAssignment(linear) ||
        ((linear->type == IROLinearOperand) && ENODE_IS(linear->u.node, EINSTRUCTION)) ||
        (linear->rtype &&
         CParser_IsVolatile(linear->rtype, linear->nodeflags & ENODE_FLAG_QUALS)) ||
        ((linear->type == IROLinearOperand) &&
         ENODE_IS(linear->u.node, EOBJREF) &&
         is_volatile_object(linear->u.node->data.objref));
}

static void ActNopNonSideEffects(IROLinear* linear, Boolean isEntry) {
    if (isEntry) {
        if (NotNoppable(linear)) {
            if (!FuncLevel) {
                linear->flags &= ~IROLF_Reffed;
            }
            FuncLevel++;
        }
    } else {
        if (NotNoppable(linear)) {
            FuncLevel--;
        } else if (!FuncLevel) {
            linear->type = IROLinearNop;
        }
    }
}

void IRO_NopNonSideEffects(IROLinear* linear, SInt32 level) {
    FuncLevel = level;
    IRO_WalkTree(linear, ActNopNonSideEffects);
}

void IRO_WalkTree(IROLinear* linear, IROWalkTreeFunc func) {
    int i;

    func(linear, TRUE);
    switch (linear->type) {
        case IROLinearOperand:
            break;
        case IROLinearOp1Arg:
            IRO_WalkTree(linear->u.monadic, func);
            break;
        case IROLinearOp2Arg:
            IRO_WalkTree(linear->u.diadic.left, func);
            IRO_WalkTree(linear->u.diadic.right, func);
            break;
        case IROLinearOp3Arg:
            IRO_WalkTree(linear->u.args3.a, func);
            IRO_WalkTree(linear->u.args3.b, func);
            IRO_WalkTree(linear->u.args3.c, func);
            break;
        case IROLinearFunccall:
            IRO_WalkTree(linear->u.funccall.callee, func);
            for (i = 0; i < linear->u.funccall.argCount; i++) {
                IRO_WalkTree(linear->u.funccall.args[i], func);
            }
            break;
        default:
            break;
    }
    func(linear, FALSE);
}

#if MWCCPSP

// 0x5531A0: 3.0.1's tree walk with an argument: the node before its
// operands when pre is set, and after them
void func_005531A0(IROLinear* linear, IROWalkTreeArgFunc func, Boolean pre, void* arg) {
    int i;

    if (pre) {
        func(linear, TRUE, arg);
    }
    switch (linear->type) {
        case IROLinearOp1Arg:
            func_005531A0(linear->u.monadic, func, pre, arg);
            break;
        case IROLinearOp2Arg:
            func_005531A0(linear->u.diadic.left, func, pre, arg);
            func_005531A0(linear->u.diadic.right, func, pre, arg);
            break;
        case IROLinearOp3Arg:
            func_005531A0(linear->u.args3.a, func, pre, arg);
            func_005531A0(linear->u.args3.b, func, pre, arg);
            func_005531A0(linear->u.args3.c, func, pre, arg);
            break;
        case IROLinearFunccall:
            func_005531A0(linear->u.funccall.callee, func, pre, arg);
            for (i = 0; i < linear->u.funccall.argCount; i++) {
                func_005531A0(linear->u.funccall.args[i], func, pre, arg);
            }
            break;
        default:
            break;
    }
    func(linear, FALSE, arg);
}

extern IROLinear* func_00552E90(IROLinear* nd); // IroSimplifyIfThenElse.c

// 0x554440: the label linear of label, from start on
IROLinear* func_00554440(CLabel* label, IROLinear* start) {
    IROLinear* nd;

    for (nd = start; nd; nd = nd->next) {
        if (nd->type == IROLinearLabel && nd->u.label.label == label) {
            return nd;
        }
    }
    CError_FATAL(2087);
    return NULL;
}

// 0x552790: *obj (the operand linked before the indirection; 3.0.1's
// IRO_TempReference without the list)
IROLinear* func_00552790(Object* obj) {
    IROLinear* objref;
    IROLinear* ind;

    objref = IRO_NewLinear(IROLinearOperand);
    objref->nodetype = EOBJREF;
    objref->u.node = create_objectrefnode(obj);
    objref->rtype = IRO_UnqualifiedType(objref->u.node->rtype);
    objref->index = ++IRO_NumLinear;
    objref->flags |= IROLF_Reffed | IROLF_Ind;
    ind = IRO_NewLinear(IROLinearOp1Arg);
    objref->next = ind;
    ind->nodetype = EINDIRECT;
    ind->rtype = IRO_UnqualifiedType(obj->type);
    ind->u.monadic = objref;
    ind->index = ++IRO_NumLinear;
    ind->next = NULL;
    return ind;
}

// 0x555C60: an integer or float constant operand
Boolean func_00555C60(IROLinear* nd) {
    return nd->type == IROLinearOperand &&
        (nd->u.node->type == EINTCONST || nd->u.node->type == EFLOATCONST);
}

// 0x554490: a new label linear (starting a node)
IROLinear* func_00554490(void) {
    IROLinear* nd = IRO_NewLinear(IROLinearLabel);
    CLabel* label;

    nd->index = IRO_NumLinear++;
    label = newlabel();
    label->next = Labels;
    Labels = label;
    nd->u.label.label = label;
    nd->flags |= IROLF_StartsNode;
    return nd;
}

// 0x552B00: copies of the statements first..last onto list
void func_00552B00(IROLinear* first, IROLinear* last, IROList* list) {
    IROLinear* nd;

    for (nd = first; nd; nd = nd->next) {
        if (nd->type != IROLinearNop && nd->type != IROLinearLabel && !(nd->flags & IROLF_Reffed)) {
            IRO_DuplicateExpr(nd, list);
        }
        if (nd == last) {
            break;
        }
    }
}

// 0x552EE0: func on the expression a statement linear holds
void func_00552EE0(IROLinear* nd, IROWalkTreeArgFunc func, void* arg) {
    IROLinear* root = func_00552E90(nd);

    if (root) {
        func(root, TRUE, arg);
    }
}

// 0x5530D0: the same after the operands only
void func_005530D0(IROLinear* linear, IROWalkTreeArgFunc func, void* arg) {
    int i;

    switch (linear->type) {
        case IROLinearOp1Arg:
            func_005531A0(linear->u.monadic, func, FALSE, arg);
            break;
        case IROLinearOp2Arg:
            func_005531A0(linear->u.diadic.left, func, FALSE, arg);
            func_005531A0(linear->u.diadic.right, func, FALSE, arg);
            break;
        case IROLinearOp3Arg:
            func_005531A0(linear->u.args3.a, func, FALSE, arg);
            func_005531A0(linear->u.args3.b, func, FALSE, arg);
            func_005531A0(linear->u.args3.c, func, FALSE, arg);
            break;
        case IROLinearFunccall:
            func_005531A0(linear->u.funccall.callee, func, FALSE, arg);
            for (i = 0; i < linear->u.funccall.argCount; i++) {
                func_005531A0(linear->u.funccall.args[i], func, FALSE, arg);
            }
            break;
        default:
            break;
    }
    func(linear, FALSE, arg);
}
#endif
#if MWCCPSP

#endif
void IRO_WalkTreeToPropagateFlags(IROLinear* linear, IROWalkTreeFunc func) {
    int i;

    switch (linear->type) {
        case IROLinearOperand:
            break;
        case IROLinearOp1Arg:
            IRO_WalkTreeToPropagateFlags(linear->u.monadic, func);
            break;
        case IROLinearOp2Arg:
            IRO_WalkTreeToPropagateFlags(linear->u.diadic.left, func);
            IRO_WalkTreeToPropagateFlags(linear->u.diadic.right, func);
            break;
        case IROLinearOp3Arg:
            IRO_WalkTreeToPropagateFlags(linear->u.args3.a, func);
            IRO_WalkTreeToPropagateFlags(linear->u.args3.b, func);
            IRO_WalkTreeToPropagateFlags(linear->u.args3.c, func);
            break;
        case IROLinearFunccall:
            IRO_WalkTreeToPropagateFlags(linear->u.funccall.callee, func);
            for (i = 0; i < linear->u.funccall.argCount; i++) {
                IRO_WalkTreeToPropagateFlags(linear->u.funccall.args[i], func);
            }
            break;
        default:
            break;
    }
    func(linear, FALSE);
}

void IRO_WalkInts(IROLinear* first, IROLinear* last, IROWalkTreeFunc func) {
    IROLinear* scan;

    for (scan = first; scan; scan = scan->next) {
        switch (scan->type) {
            case IROLinearBeginCatch:
            case IROLinearEndCatch:
            case IROLinearEndCatchDtor:
                IRO_WalkTree(scan->u.ctch.linear, func);
                break;
            case IROLinearOperand:
            case IROLinearOp1Arg:
            case IROLinearOp2Arg:
            case IROLinearOp3Arg:
            case IROLinearFunccall:
                IRO_WalkTree(scan, func);
                break;
            case IROLinearIf:
            case IROLinearIfNot:
                IRO_WalkTree(scan->u.label.cond, func);
                break;
            case IROLinearReturn:
                if (scan->u.monadic) {
                    IRO_WalkTree(scan->u.monadic, func);
                }
                break;
            case IROLinearSwitch:
                IRO_WalkTree(scan->u.swtch.cond, func);
                break;
            case IROLinearAsm:
                func(scan, TRUE);
                func(scan, FALSE);
                break;
            case IROLinearNop:
            case IROLinear18:
            case IROLinear19:
            case IROLinearEnd:
                break;
            default:
                break;
        }

        if (scan == last) {
            return;
        }
    }
}

void IRO_Cut(IROLinear* first, IROLinear* last) {
    IROLinear* scan;
    IROLinear* prev;
    IRONode* node;

    scan = first;
    while (1) {
        scan->stmt = NULL;
        if (scan == last) {
            break;
        }
        scan = scan->next;
    }

    prev = NULL;
    for (scan = IRO_FirstLinear; scan && scan != first; scan = scan->next) {
        prev = scan;
    }

#if MWCCPSP
    CError_ASSERT(2671, scan);
#else
    CError_ASSERT(960, scan);
#endif

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->first == first) {
            if (node->last == last) {
                node->first = node->last = NULL;
                break;
            } else {
                node->first = last->next;
                break;
            }
        } else if (node->last == last) {
            node->last = prev;
            break;
        }
    }

    if (prev) {
        prev->next = last->next;
    } else {
        IRO_FirstLinear = last->next;
    }
}

void IRO_Paste(IROLinear* first, IROLinear* last, IROLinear* before) {
    IROLinear* scan;
    IROLinear* prev;
    IRONode* node;

#if !MWCCPSP // 3.0.1 has no check
    CError_ASSERT(1011, before && before->type != IROLinearLabel);
#endif

    prev = NULL;
    for (scan = IRO_FirstLinear; scan && scan != before; scan = scan->next) {
        prev = scan;
    }

#if MWCCPSP
    CError_ASSERT(2736, scan);
#else
    CError_ASSERT(1025, scan);
#endif

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->first == before) {
            node->first = first;
            break;
        }
    }

    last->next = before;
    if (prev) {
        prev->next = first;
    } else {
        IRO_FirstLinear = first;
    }
}

void IRO_PasteAfter(IROLinear* first, IROLinear* last, IROLinear* after) {
    IRONode* node;

#if !MWCCPSP // 3.0.1 has no check
    switch (after->type) {
        case IROLinearGoto:
        case IROLinearIf:
        case IROLinearIfNot:
        case IROLinearSwitch:
            CError_FATAL(1069);
            break;
        default:
            break;
    }
#endif

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->last == after) {
            node->last = last;
            break;
        }
    }

    last->next = after->next;
    after->next = first;
}

static void FindStart(IROLinear* linear, Boolean isEntry) {
    IROLinear* scan;
    if (isEntry) {
        scan = linear;
        do {
            if (scan == ExprStart) {
                ExprStart = linear;
                return;
            }
            if (scan == ExprEnd) {
                return;
            }
        } while ((scan = scan->next));
    }
}

void IRO_ClipExpr(IROExpr* expr) {
    ExprStart = ExprEnd = expr->linear;
    IRO_WalkTree(expr->linear, FindStart);
    IRO_Cut(ExprStart, ExprEnd);
}

void IRO_ClipExprTree(IROLinear* linear) {
    ExprStart = ExprEnd = linear;
    IRO_WalkTree(linear, FindStart);
    IRO_Cut(ExprStart, ExprEnd);
}

static void SetNodeNumInExprList(IROLinear* linear, Boolean isEntry) {
    if (isEntry && linear->expr) {
        linear->expr->node = IRO_Node;
    }
}

void IRO_MoveExpression(IROExpr* expr, IROLinear* linear) {
    IRONode* node;
    IROLinear* scan;

    ExprStart = ExprEnd = expr->linear;
    IRO_WalkTree(expr->linear, FindStart);

    if (ExprStart == linear) {
        return;
    }

    IRO_Cut(ExprStart, ExprEnd);
    IRO_Paste(ExprStart, ExprEnd, linear);
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        for (scan = node->first; scan; scan = scan->next) {
            if (scan == expr->linear) {
                expr->node = node;
                break;
            }
            if (scan == node->last) {
                break;
            }
        }
    }

    IRO_Node = expr->node;
    IRO_WalkTree(expr->linear, SetNodeNumInExprList);
}

#if MWCCPSP
// 0x551ED0: temp = value, onto list
IROLinear* func_00551ED0(Object* temp, IROLinear* value, IROList* list) {
    IROLinear* objref;
    IROLinear* ind;
    IROLinear* ass;

    objref = IRO_NewLinear(IROLinearOperand);
    objref->nodetype = EOBJREF;
    objref->u.node = create_objectrefnode(temp);
    objref->rtype = IRO_UnqualifiedType(objref->u.node->rtype);
    objref->index = ++IRO_NumLinear;
    objref->flags |= IROLF_Reffed | IROLF_Assigned | IROLF_Ind;
    ind = IRO_NewLinear(IROLinearOp1Arg);
    ind->nodetype = EINDIRECT;
    ind->rtype = IRO_UnqualifiedType(value->rtype);
    ind->u.monadic = objref;
    ind->index = ++IRO_NumLinear;
    ind->flags |= IROLF_Reffed | IROLF_Assigned;
    ass = IRO_NewLinear(IROLinearOp2Arg);
    ass->nodetype = EASS;
    ass->u.diadic.left = ind;
    ass->u.diadic.right = value;
    ass->rtype = IRO_UnqualifiedType(value->rtype);
    ass->index = ++IRO_NumLinear;
    objref->next = ind;
    ind->next = ass;
    if (list->head) {
        list->tail->next = objref;
    } else {
        list->head = objref;
    }
    list->tail = objref;
    while (list->tail->next) {
        list->tail = list->tail->next;
    }
    return ass;
}

// 0x552CA0: cut an expression's tree out of the linear list
void func_00552CA0(IROExpr* expr) {
    ExprStart = ExprEnd = expr->linear;
    IRO_WalkTree(expr->linear, FindStart);
    IRO_Cut(ExprStart, ExprEnd);
}

// 0x552C80: walker callback, the expression records of a moved tree get its
// node
void func_00552C80(IROLinear* linear, Boolean isFirst, void* arg) {
    if (isFirst && linear->expr) {
        linear->expr->node = arg;
    }
}

// 0x552BC0: 3.0.1's IRO_MoveExpression (the node goes to the records through
// the walker's argument)
void func_00552BC0(IROExpr* expr, IROLinear* linear) {
    IRONode* node;
    IROLinear* scan;

    ExprStart = ExprEnd = expr->linear;
    IRO_WalkTree(expr->linear, FindStart);
    if (ExprStart == linear) {
        return;
    }
    IRO_Cut(ExprStart, ExprEnd);
    IRO_Paste(ExprStart, ExprEnd, linear);
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        for (scan = node->first; scan; scan = scan->next) {
            if (scan == expr->linear) {
                expr->node = node;
                break;
            }
            if (scan == node->last) {
                break;
            }
        }
    }
    func_005531A0(expr->linear, func_00552C80, TRUE, expr->node);
}

#endif
#if MWCCPSP

// the operand slot of scan that holds nd
static inline IROLinear** OperandSlot(IROLinear* scan, IROLinear* nd) {
    int i;

    switch (scan->type) {
        case IROLinearGotoExpr:
        case IROLinearReturn:
        case IROLinearOp1Arg:
            return scan->u.monadic == nd ? &scan->u.monadic : NULL;
        case IROLinearIf:
        case IROLinearIfNot:
            return scan->u.label.cond == nd ? &scan->u.label.cond : NULL;
        case IROLinearSwitch:
            return scan->u.swtch.cond == nd ? &scan->u.swtch.cond : NULL;
        case IROLinearOp2Arg:
            if (scan->u.diadic.left == nd) {
                return &scan->u.diadic.left;
            }
            return scan->u.diadic.right == nd ? &scan->u.diadic.right : NULL;
        case IROLinearOp3Arg:
            if (scan->u.args3.a == nd) {
                return &scan->u.args3.a;
            }
            if (scan->u.args3.b == nd) {
                return &scan->u.args3.b;
            }
            return scan->u.args3.c == nd ? &scan->u.args3.c : NULL;
        case IROLinearFunccall:
            if (scan->u.funccall.callee == nd) {
                return &scan->u.funccall.callee;
            }
            for (i = 0; i < scan->u.funccall.argCount; i++) {
                if (scan->u.funccall.args[i] == nd) {
                    return &scan->u.funccall.args[i];
                }
            }
            return NULL;
        case IROLinearOperand:
        case IROLinearNop:
        case IROLinearScope:
        case IROLinearGoto:
        case IROLinearLabel:
        case IROLinearEntry:
        case IROLinearExit:
        case IROLinearBeginCatch:
        case IROLinearEndCatch:
        case IROLinearEndCatchDtor:
        case IROLinearAsm:
        case IROLinearEnd:
            return NULL;
        default:
            CError_FATAL(3288);
            return NULL;
    }
}

// 0x551FC0: nd's use becomes replacement
void func_00551FC0(IROLinear* nd, IROLinear* replacement) {
    IROLinear* scan;
    IROLinear** ptr = NULL;
    int pass;

    scan = nd->next;
    for (pass = 0; pass < 2; pass++) {
        for (; scan; scan = scan->next) {
            if ((ptr = OperandSlot(scan, nd))) {
                goto found;
            }
        }
        scan = IRO_FirstLinear;
    }
    return;
found:
    if (!ptr || !*ptr) {
        CError_FATAL(3419);
    }
    *ptr = replacement;
    replacement->flags |= IROLF_Reffed;
}

// 0x552150: 3.0.1's IRO_ReplaceReference: nd's use becomes *obj, the
// reference pasted after 'after'
void func_00552150(IROLinear* nd, Object* obj, IROLinear* after) {
    IROLinear* scan;
    IROLinear** ptr = NULL;
    IROLinear* objref;
    IROLinear* ind;
    IROLinear* first;
    IROLinear* last;
    IRONode* node;
    int pass;

    scan = nd->next;
    for (pass = 0; pass < 2; pass++) {
        for (; scan; scan = scan->next) {
            if ((ptr = OperandSlot(scan, nd))) {
                goto found;
            }
        }
        scan = IRO_FirstLinear;
    }
    ptr = NULL;
    scan = NULL;
found:
    if (!scan) {
        IRO_Dump("Oh, oh, did not find reference to replace\n");
        return;
    }
    if (!ptr || !*ptr) {
        CError_FATAL(3355);
    }
    objref = IRO_NewLinear(IROLinearOperand);
    objref->nodetype = EOBJREF;
    objref->u.node = create_objectrefnode(obj);
    objref->rtype = IRO_UnqualifiedType(objref->u.node->rtype);
    objref->index = ++IRO_NumLinear;
    objref->flags |= IROLF_Reffed | IROLF_Ind;
    ind = IRO_NewLinear(IROLinearOp1Arg);
    objref->next = ind;
    ind->nodetype = EINDIRECT;
    ind->rtype = IRO_UnqualifiedType(obj->type);
    ind->u.monadic = objref;
    ind->index = ++IRO_NumLinear;
    ind->next = NULL;
    *ptr = ind;
    (*ptr)->rtype = nd->rtype;
    first = (*ptr)->u.monadic;
    for (last = first; last->next; last = last->next) {}
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->last == after) {
            node->last = last;
            break;
        }
    }
    last->next = after->next;
    after->next = first;
    last->flags |= IROLF_Reffed;
    if (nd->flags & IROLF_LoopInvariant) {
        last->flags |= IROLF_LoopInvariant;
    }
}

#endif
void IRO_InitList(IROList* list) {
    list->head = list->tail = NULL;
}
#if MWCCPSP

#endif
#if MWCCPSP
// 0x552B80: 3.0.1 empties a list it is done with through a copy of
// IRO_InitList
void IRO_ResetList(IROList* list) {
    list->head = list->tail = NULL;
}
#endif

void IRO_AddToList(IROLinear* linear, IROList* list) {
    if (list->head) {
        list->tail->next = linear;
    } else {
        list->head = linear;
    }

    list->tail = linear;
    while (list->tail->next) {
        list->tail = list->tail->next;
    }
}

IROLinear* IRO_FindLabelNode(CLabel* label, IROLinear* linear) {
    IROLinear* scan;

    for (scan = linear; scan; scan = scan->next) {
        if (scan->type == IROLinearLabel && scan->u.label.label == label) {
            break;
        }
    }

    CError_ASSERT(1253, scan);
    return scan;
}

void IRO_DuplicateExprRange(IROLinear* start, IROLinear* end, IROList* list) {
    IROLinear* scan;

    for (scan = start; scan; scan = scan->next) {
        if (scan->type != IROLinearNop && !(scan->flags & IROLF_Reffed)) {
            IRO_DuplicateExpr(scan, list);
        }
        if (scan == end) {
            break;
        }
    }
}

IROLinear* IRO_DuplicateExpr(IROLinear* linear, IROList* list) {
    IROLinear* copy;
    ENode* copynode;
    int i;

    copy = IRO_NewLinear(linear->type);
    *copy = *linear;

    copy->index = ++IRO_NumLinear;
    copy->next = NULL;
    copy->expr = NULL;

    switch (copy->type) {
        case IROLinearNop:
            break;
        case IROLinearOperand:
#if MWCCPSP
        case IROLinearScope: // 3.0.1 (0x5528A0) copies its ENode too
#endif
            copynode = lalloc(sizeof(ENode));
            *copynode = *linear->u.node;
            copy->u.node = copynode;
            break;
        case IROLinearOp1Arg:
            copy->u.monadic = IRO_DuplicateExpr(copy->u.monadic, list);
            break;
        case IROLinearOp2Arg:
            if (linear->flags & IROLF_RightFirst) {
                copy->u.diadic.right = IRO_DuplicateExpr(copy->u.diadic.right, list);
                copy->u.diadic.left = IRO_DuplicateExpr(copy->u.diadic.left, list);
            } else {
                copy->u.diadic.left = IRO_DuplicateExpr(copy->u.diadic.left, list);
                copy->u.diadic.right = IRO_DuplicateExpr(copy->u.diadic.right, list);
            }
            break;
        case IROLinearOp3Arg:
            copy->u.args3.a = IRO_DuplicateExpr(copy->u.args3.a, list);
            copy->u.args3.b = IRO_DuplicateExpr(copy->u.args3.b, list);
            copy->u.args3.c = IRO_DuplicateExpr(copy->u.args3.c, list);
            break;
        case IROLinearFunccall:
            copy->u.funccall.callee = IRO_DuplicateExpr(copy->u.funccall.callee, list);
#if MWCCPSP
            copy->u.funccall.args = IRO_Alloc(sizeof(IROLinear*) * copy->u.funccall.argCount);
#else
            copy->u.funccall.args = oalloc(sizeof(IROLinear*) * copy->u.funccall.argCount);
#endif
            for (i = 0; i < copy->u.funccall.argCount; i++) {
                copy->u.funccall.args[i] = IRO_DuplicateExpr(linear->u.funccall.args[i], list);
            }
            break;
        case IROLinearAsm:
#if MWCCPSP
            // 3.0.1: the asm statement is copied by the back end's hook 6
            // (FEInterface.c 0x43A4E0); without one the copy shares it
            if (IRO_Hook6 && linear->u.asm_stmt) {
                copy->u.asm_stmt = ((Statement * (*)(Statement*)) IRO_Hook6)(linear->u.asm_stmt);
            }
#else
            copy->u.asm_stmt = CodeGen_CopyAsmStat(linear->u.asm_stmt);
#endif
            break;
        default:
            break;
    }

    IRO_AddToList(copy, list);
    return copy;
}

IROLinear* IRO_TempReference(Object* obj, IROList* list) {
    IROLinear* op;
    IROLinear* ind;

    op = IRO_NewLinear(IROLinearOperand);
    op->u.node = create_objectrefnode(obj);
    op->rtype = op->u.node->data.objref->type;
    op->index = ++IRO_NumLinear;
    op->flags |= IROLF_Reffed | IROLF_Ind;
    IRO_AddToList(op, list);

    ind = IRO_NewLinear(IROLinearOp1Arg);
    ind->nodetype = EINDIRECT;
    ind->rtype = obj->type;
    ind->u.monadic = op;
    ind->index = ++IRO_NumLinear;
    ind->next = NULL;
    IRO_AddToList(ind, list);

    return ind;
}

inline IROLinear* LocateFatherHelper(IROLinear* linear, Boolean wrapAround, IROLinear*** refPtr) {
    IROLinear* scan;
    SInt32 index;
    int i;

    for (scan = linear->next, index = 0; index < (wrapAround ? 2 : 1); index++) {
        while (scan) {
            switch (scan->type) {
                case IROLinearIf:
                case IROLinearIfNot:
                    if (scan->u.label.cond == linear) {
                        if (refPtr) {
                            *refPtr = &scan->u.label.cond;
                        }
                        return scan;
                    }
                    break;
                case IROLinearReturn:
#if MWCCPSP
                case IROLinearGotoExpr: // 3.0.1 (0x5526C0): the target expression
#endif
                    if (scan->u.monadic == linear) {
                        if (refPtr) {
                            *refPtr = &scan->u.monadic;
                        }
                        return scan;
                    }
                    break;
                case IROLinearOp1Arg:
                    if (scan->u.monadic == linear) {
                        if (refPtr) {
                            *refPtr = &scan->u.monadic;
                        }
                        return scan;
                    }
                    break;
                case IROLinearSwitch:
                    if (scan->u.swtch.cond == linear) {
                        if (refPtr) {
                            *refPtr = &scan->u.swtch.cond;
                        }
                        return scan;
                    }
                    break;
                case IROLinearOp2Arg:
                    if (scan->u.diadic.left == linear) {
                        if (refPtr) {
                            *refPtr = &scan->u.diadic.left;
                        }
                        return scan;
                    }
                    if (scan->u.diadic.right == linear) {
                        if (refPtr) {
                            *refPtr = &scan->u.diadic.right;
                        }
                        return scan;
                    }
                    break;
                case IROLinearOp3Arg:
                    if (scan->u.args3.a == linear) {
                        if (refPtr) {
                            *refPtr = &scan->u.args3.a;
                        }
                        return scan;
                    }
                    if (scan->u.args3.b == linear) {
                        if (refPtr) {
                            *refPtr = &scan->u.args3.b;
                        }
                        return scan;
                    }
                    if (scan->u.args3.c == linear) {
                        if (refPtr) {
                            *refPtr = &scan->u.args3.c;
                        }
                        return scan;
                    }
                    break;
                case IROLinearFunccall:
                    if (scan->u.funccall.callee == linear) {
                        if (refPtr) {
                            *refPtr = &scan->u.funccall.callee;
                        }
                        return scan;
                    }
                    for (i = 0; i < scan->u.funccall.argCount; i++) {
                        if (scan->u.funccall.args[i] == linear) {
                            if (refPtr) {
                                *refPtr = &scan->u.funccall.args[i];
                            }
                            return scan;
                        }
                    }
                    break;
                case IROLinearNop:
                case IROLinearOperand:
                case IROLinearGoto:
                case IROLinearLabel:
                case IROLinearEntry:
                case IROLinearExit:
                case IROLinearBeginCatch:
                case IROLinearEndCatch:
                case IROLinearEndCatchDtor:
                case IROLinearAsm:
                case IROLinearEnd:
#if MWCCPSP
                case IROLinearScope:
#endif
                    break;
                default:
#if MWCCPSP
                    CError_FATAL(3288);
#else
                    CError_FATAL(1545);
#endif
            }
            scan = scan->next;
        }

        scan = IRO_FirstLinear;
    }

    if (refPtr) {
        *refPtr = NULL;
    }
    return NULL;
}

IROLinear* IRO_LocateFather(IROLinear* linear) {
    return LocateFatherHelper(linear, FALSE, NULL);
}

IROLinear* IRO_LocateFather_Cut_And_Paste(IROLinear* nd, IROLinear* replacement) {
    IROLinear** link;
    IROLinear* father = LocateFatherHelper(nd, FALSE, &link);
    if (father) {
#if MWCCPSP
        CError_ASSERT(3319, link && *link);
#else
        CError_ASSERT(1577, link && *link);
#endif
        IRO_NopOut(nd);
        *link = replacement;
    }
    return father;
}

IROLinear* IRO_LocateFather_Cut_And_Paste_Without_Nopping(IROLinear* nd, IROLinear* replacement) {
    IROLinear** link;
    IROLinear* father = LocateFatherHelper(nd, FALSE, &link);
    if (father) {
#if MWCCPSP
        CError_ASSERT(3335, link && *link);
#else
        CError_ASSERT(1594, link && *link);
#endif
        *link = replacement;
    }
    return father;
}

void IRO_ReplaceReference(IROLinear* nd, Object* obj, IROLinear* after) {
    IROLinear** ptr;
    IROList list;

    if (LocateFatherHelper(nd, TRUE, &ptr)) {
        CError_ASSERT(1614, ptr && *ptr);
        IRO_InitList(&list);
        *ptr = IRO_TempReference(obj, &list);
        IRO_PasteAfter(list.head, list.tail, after);
        if (nd->flags & IROLF_LoopInvariant) {
            list.tail->flags |= IROLF_LoopInvariant;
        }
    } else {
        IRO_Dump("Oh, oh, did not find reference to replace\n");
    }
}

void IRO_ReplaceReferenceWithNode(IROLinear* nd, IROLinear* replacement) {
    IROLinear** ptr;

    if (LocateFatherHelper(nd, TRUE, &ptr)) {
        CError_ASSERT(1673, ptr && *ptr);
        *ptr = replacement;
        replacement->flags |= IROLF_Reffed;
    } else {
        IRO_Dump("Oh, oh, did not find reference to replace\n");
    }
}

VarRecord* IRO_GetTemp(IROExpr* expr) {
    expr->temp = create_temp_object(expr->linear->rtype);
    return IRO_FindVar(expr->temp, TRUE, TRUE);
}

IROLinear* IRO_AssignToTemp(IROExpr* expr) {
    IROLinear* objref;
    IROLinear* ind;
    IROLinear* ass;

    objref = IRO_NewLinear(IROLinearOperand);
    objref->u.node = create_objectrefnode(expr->temp);
    objref->rtype = objref->u.node->data.objref->type;
    objref->index = ++IRO_NumLinear;
    objref->flags |= IROLF_Reffed | IROLF_Assigned | IROLF_Ind;

    ind = IRO_NewLinear(IROLinearOp1Arg);
    ind->nodetype = EINDIRECT;
    ind->rtype = expr->linear->rtype;
    ind->u.monadic = objref;
    ind->index = ++IRO_NumLinear;
    ind->flags |= IROLF_Reffed | IROLF_Assigned;

    ass = IRO_NewLinear(IROLinearOp2Arg);
    ass->nodetype = EASS;
    ass->u.diadic.left = ind;
    ass->u.diadic.right = expr->linear;
    ass->rtype = expr->linear->rtype;
    ass->index = ++IRO_NumLinear;

    objref->next = ind;
    ind->next = ass;
    IRO_PasteAfter(objref, ass, expr->linear);
    return ass;
}

IROLinear* IRO_FindStart(IROLinear* linear) {
    ExprStart = ExprEnd = linear;
    IRO_WalkTree(linear, FindStart);
    return ExprStart;
}

void IRO_DeleteCommaNode(IROLinear* linear, IROExpr* expr) {
    IROLinear* scan;
    for (scan = linear; scan; scan = scan->next) {
        if (scan->nodetype == ECOMMA) {
            if (scan != expr->linear) {
                IRO_NopOut(scan->u.diadic.left);
                IRO_LocateFather_Cut_And_Paste(scan, scan->u.diadic.right);
            } else {
                IRO_NopOut(scan->u.diadic.left);
                expr->linear = scan->u.diadic.right;
            }
        }
    }
}

void IRO_RemoveCommaNodeFromIR(void) {
    IRONode* node;
    IROLinear* linear;

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        linear = node->first;
        do {
            if (!linear) {
                break;
            }
            if (linear->nodetype == ECOMMA) {
                linear->u.diadic.left->flags = linear->u.diadic.left->flags & ~IROLF_Reffed;
                IRO_LocateFather_Cut_And_Paste_Without_Nopping(linear, linear->u.diadic.right);
                linear->type = IROLinearNop;
            }
            linear = linear->next;
        } while (linear != node->last);
    }
}

IROAddrRecord* IRO_InitAddrRecordPointer(IROLinear* linear) {
    IROAddrRecord* rec = oalloc(sizeof(IROAddrRecord));
    rec->numObjRefs = 0;
    rec->objRefs = NULL;
    rec->numMisc = 0;
    rec->misc = NULL;
    rec->numInts = 0;
    rec->ints = NULL;
    rec->unused = 0;
    rec->linear = linear;
    return rec;
}

#if MWCCPSP
// 0x5513A0
// 0x551310: free an address record's lists
void func_00551310(IROAddrRecord301* rec, void (*dealloc)(void*)) {
    IROElmList* elm;
    IROElmList* next;

    for (elm = rec->objRefs; elm; elm = next) {
        next = elm->next;
        dealloc(elm);
    }
    rec->numObjRefs = 0;
    rec->objRefs = NULL;
    for (elm = rec->misc; elm; elm = next) {
        next = elm->next;
        dealloc(elm);
    }
    rec->numMisc = 0;
    rec->misc = NULL;
    for (elm = rec->ints; elm; elm = next) {
        next = elm->next;
        dealloc(elm);
    }
    rec->numInts = 0;
    rec->ints = NULL;
    rec->unused = 0;
    rec->linear = NULL;
}

void func_005513A0(IROAddrRecord301* rec, IROLinear* linear) {
    rec->numObjRefs = 0;
    rec->objRefs = NULL;
    rec->numMisc = 0;
    rec->misc = NULL;
    rec->numInts = 0;
    rec->ints = NULL;
    rec->unused = 0;
    rec->linear = linear;
}

// 0x5512E0: push linear onto list
void func_005512E0(IROLinear* linear, IROElmList** list, IROAllocFunc alloc) {
    IROElmList* elm = alloc(sizeof(IROElmList));

    elm->element = linear;
    elm->next = NULL;
    if (*list) {
        elm->next = *list;
        *list = elm;
    } else {
        *list = elm;
    }
}

static inline void AddAddend301(IROLinear* nd, IROAddrRecord301* rec, IROAllocFunc alloc) {
    if (nd->type == IROLinearOp2Arg && nd->nodetype == EADD) {
        func_005510D0(nd, rec, alloc);
    } else if (IRO_IsIntConstant(nd)) {
        rec->numInts++;
        func_005512E0(nd, &rec->ints, alloc);
    } else if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
        rec->numObjRefs++;
        func_005512E0(nd, &rec->objRefs, alloc);
    } else {
        rec->numMisc++;
        func_005512E0(nd, &rec->misc, alloc);
    }
}

// 0x5510D0: sort the addends of a sum into rec
void func_005510D0(IROLinear* linear, IROAddrRecord301* rec, IROAllocFunc alloc) {
    AddAddend301(linear->u.diadic.left, rec, alloc);
    AddAddend301(linear->u.diadic.right, rec, alloc);
}
#endif
#if MWCCPSP

#endif
IROLinear* IRO_HasSideEffect(IROLinear* linear) {
    IROLinear* effect;

    if (!linear) {
        return linear;
    }

    if (linear->rtype && CParser_IsVolatile(linear->rtype, linear->nodeflags & ENODE_FLAG_QUALS)) {
        return linear;
    }

    switch (linear->type) {
        case IROLinearAsm:
            return linear;
        case IROLinearOperand:
            if (ENODE_IS(linear->u.node, EOBJREF) &&
                is_volatile_object(linear->u.node->data.objref)) {
                return linear;
            }
            return NULL;
        case IROLinearOp1Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                return linear;
            } else {
                return IRO_HasSideEffect(linear->u.monadic);
            }
        case IROLinearOp2Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                return linear;
            }
            effect = IRO_HasSideEffect(linear->u.diadic.left);
            if (!effect) {
                effect = IRO_HasSideEffect(linear->u.diadic.right);
            }
            return effect;
        case IROLinearOp3Arg:
            if ((effect = IRO_HasSideEffect(linear->u.args3.a))) {
                return effect;
            } else if ((effect = IRO_HasSideEffect(linear->u.args3.b))) {
                return effect;
            } else {
                return IRO_HasSideEffect(linear->u.args3.c);
            }
        case IROLinearFunccall:
            return linear;
        default:
            return linear;
    }
}

IROLinear* IRO_CheckSideEffect(IROLinear* linear) {
    IROLinear* result;
    IROLinear* effect1;
    IROLinear* effect2;
    IROLinear* effect3;

    if (!linear) {
        return linear;
    }

    if (linear->rtype && CParser_IsVolatile(linear->rtype, linear->nodeflags & ENODE_FLAG_QUALS)) {
        linear->flags &= ~IROLF_Reffed;
        return linear;
    }

    result = NULL;
    switch (linear->type) {
        case IROLinearAsm:
            linear->flags &= ~IROLF_Reffed;
            return linear;
        case IROLinearOperand:
            if (ENODE_IS(linear->u.node, EOBJREF) &&
                is_volatile_object(linear->u.node->data.objref)) {
                linear->flags &= ~IROLF_Reffed;
                return linear;
            }
            break;
        case IROLinearOp1Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                linear->flags &= ~IROLF_Reffed;
                return linear;
            }
            if ((result = IRO_CheckSideEffect(linear->u.monadic))) {
                if (linear->nodetype == EINDIRECT && linear->u.monadic->type == IROLinearOperand) {
                    linear->flags &= ~IROLF_Reffed;
                    return linear;
                }
            }
            break;
        case IROLinearOp2Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                linear->flags &= ~IROLF_Reffed;
                return linear;
            }
            effect1 = IRO_CheckSideEffect(linear->u.diadic.left);
            effect2 = IRO_CheckSideEffect(linear->u.diadic.right);
            if (effect1) {
                result = effect1;
            } else {
                result = effect2;
            }
            break;
        case IROLinearOp3Arg:
            effect1 = IRO_CheckSideEffect(linear->u.args3.a);
            effect2 = IRO_CheckSideEffect(linear->u.args3.b);
            effect3 = IRO_CheckSideEffect(linear->u.args3.c);
            result = effect1 ? effect1 : effect2 ? effect2 : effect3;
            break;
#if MWCCPSP
        case IROLinearScope: // 3.0.1 (0x551C20): kept like a call
#endif
        case IROLinearFunccall:
            linear->flags &= ~IROLF_Reffed;
            return linear;
        default:
            return linear;
    }

    linear->type = IROLinearNop;
    IRO_Dump("Nop out with side-effects checking at: %d\n", linear->index);
    return result;
}

void IRO_WalkExcActions(ExceptionAction* action, WalkObjFunc func) {
    while (action) {
        switch (action->type) {
            case EAT_DESTROYLOCAL:
                func(action->data.destroy_local.local);
                break;
            case EAT_DESTROYLOCALCOND:
                func(action->data.destroy_local_cond.local);
                func(action->data.destroy_local_cond.cond);
                break;
            case EAT_DESTROYLOCALOFFSET:
                func(action->data.destroy_local_offset.local);
                break;
            case EAT_DESTROYLOCALPOINTER:
                func(action->data.destroy_local_pointer.pointer);
                break;
            case EAT_DESTROYLOCALARRAY:
                func(action->data.destroy_local_array.localarray);
                break;
            case EAT_DESTROYPARTIALARRAY:
                func(action->data.destroy_partial_array.arraypointer);
                func(action->data.destroy_partial_array.arraycounter);
                func(action->data.destroy_partial_array.element_size);
                break;
            case EAT_DESTROYBASE:
                func(action->data.destroy_base.objectptr);
                break;
            case EAT_DESTROYMEMBER:
                func(action->data.destroy_member.objectptr);
                break;
            case EAT_DESTROYMEMBERCOND:
                func(action->data.destroy_member_cond.objectptr);
                func(action->data.destroy_member_cond.cond);
                break;
            case EAT_DESTROYMEMBERARRAY:
                func(action->data.destroy_member_array.objectptr);
                break;
            case EAT_DELETEPOINTER:
                func(action->data.delete_pointer.pointerobject);
                break;
            case EAT_DELETEPOINTERCOND:
                func(action->data.delete_pointer_cond.pointerobject);
                func(action->data.delete_pointer_cond.cond);
                break;
            default:
                break;
        }
        action = action->prev;
    }
}

#if MWCCPSP
// 0x54FC80: 3.0.1's walk over the objects of exception actions, with an
// argument; it also visits a partial array's destructor and skips
// specifications
void func_0054FC80(ExceptionAction* action, void (*func)(Object*, void*), void* arg) {
    for (; action; action = action->prev) {
        switch (action->type) {
            case EAT_DESTROYLOCAL:
                func(action->data.destroy_local.local, arg);
                break;
            case EAT_DESTROYLOCALCOND:
                func(action->data.destroy_local_cond.local, arg);
                func(action->data.destroy_local_cond.cond, arg);
                break;
            case EAT_DESTROYLOCALOFFSET:
                func(action->data.destroy_local_offset.local, arg);
                break;
            case EAT_DESTROYLOCALPOINTER:
                func(action->data.destroy_local_pointer.pointer, arg);
                break;
            case EAT_DESTROYLOCALARRAY:
                func(action->data.destroy_local_array.localarray, arg);
                break;
            case EAT_DESTROYPARTIALARRAY:
                func(action->data.destroy_partial_array.arraypointer, arg);
                func(action->data.destroy_partial_array.arraycounter, arg);
                func(action->data.destroy_partial_array.dtor, arg);
                func(action->data.destroy_partial_array.element_size, arg);
                break;
            case EAT_DESTROYMEMBER:
                func(action->data.destroy_member.objectptr, arg);
                break;
            case EAT_DESTROYMEMBERCOND:
                func(action->data.destroy_member_cond.objectptr, arg);
                func(action->data.destroy_member_cond.cond, arg);
                break;
            case EAT_DESTROYMEMBERARRAY:
                func(action->data.destroy_member_array.objectptr, arg);
                break;
            case EAT_DELETEPOINTER:
            case EAT_DELETELOCALPOINTER:
                func(action->data.delete_pointer.pointerobject, arg);
                break;
            case EAT_DELETEPOINTERCOND:
                func(action->data.delete_pointer_cond.pointerobject, arg);
                func(action->data.delete_pointer_cond.cond, arg);
                break;
            case EAT_CATCHBLOCK:
                if (action->data.catch_block.catch_object) {
                    func(action->data.catch_block.catch_object, arg);
                }
                func(action->data.catch_block.catch_info_object, arg);
                break;
            case EAT_ACTIVECATCHBLOCK:
                func(action->data.active_catch_block.catch_info_object, arg);
                break;
            case EAT_DESTROYBASE:
                func(action->data.destroy_base.objectptr, arg);
                break;
            case EAT_UNK18:
                func(action->data.destroy_local.local, arg);
                break;
            default:
                break;
        }
    }
}
#endif
#if MWCCPSP

#endif
#if MWCCPSP
// 3.0.1 (0x551650): asm statements may throw too, as the back end says;
// 0x551630 asks only about calls.
Boolean IRO_FunctionCallMightThrowException(IROLinear* linear) {
    Object* obj;

    if (linear->type == IROLinearFunccall) {
        obj = NULL;
        if (linear->u.funccall.callee->type == IROLinearOperand &&
            ENODE_IS(linear->u.funccall.callee->u.node, EOBJREF)) {
            obj = linear->u.funccall.callee->u.node->data.objref;
        }
        if (obj &&
            !CExcept_CanThrowException(
                obj, obj->datatype == DVFUNC && (linear->nodeflags & ENODE_FLAG_NONVIRTUAL) == 0
            )) {
            return FALSE;
        }
        return TRUE;
    }
    if (linear->type == IROLinearAsm && IRO_GetAsmEffectsHook && linear->u.asm_stmt) {
        IROAsmEffects effects = IRO_EmptyAsmEffects;
        IRO_GetAsmEffectsHook(linear->u.asm_stmt, &effects);
        if (effects.mayThrow) {
            return TRUE;
        }
    }
    return FALSE;
}

Boolean IRO_IsThrowingCall(IROLinear* linear) {
    if (linear->type != IROLinearFunccall) {
        return FALSE;
    }
    return IRO_FunctionCallMightThrowException(linear);
}
#else
Boolean IRO_FunctionCallMightThrowException(IROLinear* linear) {
    Object* obj;
    if (linear->type == IROLinearFunccall) {
        obj = NULL;
        if (linear->u.funccall.callee->type == IROLinearOperand &&
            ENODE_IS(linear->u.funccall.callee->u.node, EOBJREF)) {
            obj = linear->u.funccall.callee->u.node->data.objref;
        }
        if (!obj ||
            CExcept_CanThrowException(
                obj, obj->datatype == DVFUNC && (linear->nodeflags & ENODE_FLAG_NONVIRTUAL) == 0
            )) {
            return TRUE;
        }
    }
    return FALSE;
}
#endif

#if MWCCPSP
// 3.0.1 (0x551540): the constant linear has the EINTCONST node type, the
// unqualified type, the value truncated to the type, and is loop invariant.
IROLinear* IRO_NewIntConst(CInt64 val, Type* type) {
    ENode* node;
    IROLinear* linear;

    linear = IRO_NewLinear(IROLinearOperand);
    linear->nodetype = EINTCONST;
    linear->index = ++IRO_NumLinear;
    linear->rtype = IRO_UnqualifiedType(type);
    IRO_TruncateValueToType(&val, type);
    node = lalloc(sizeof(ENode));
    memclrw(node, sizeof(ENode));
    node->type = EINTCONST;
    node->data.intval = val;
    node->rtype = IRO_UnqualifiedType(type);
    linear->u.node = node;
    linear->flags |= IROLF_LoopInvariant;
    return linear;
}
#else
IROLinear* IRO_NewIntConst(CInt64 val, Type* type) {
    ENode* node;
    IROLinear* linear;

    node = IRO_NewENode(EINTCONST);
    node->data.intval = val;
    node->rtype = type;

    linear = IRO_NewLinear(IROLinearOperand);
    linear->index = ++IRO_NumLinear;
    linear->rtype = type;
    linear->u.node = node;
    return linear;
}
#endif

IROLinear* IRO_NewFloatConst(const Float val, Type* type) {
    ENode* node;
    IROLinear* linear;

    node = IRO_NewENode(EFLOATCONST);
    node->data.floatval = val;
    node->rtype = type;

    linear = IRO_NewLinear(IROLinearOperand);
    linear->index = ++IRO_NumLinear;
    linear->rtype = type;
    linear->u.node = node;
    return linear;
}

#if MWCCPSP
ENode* IRO_NewENode(ENodeType nodetype) {
    ENode* enode = lalloc(sizeof(ENode));
    memset(enode, 0, sizeof(ENode));
    enode->type = nodetype;
    return enode;
}

// 3.0.1 (IroType.c, 0x55CBA0): pointer, array and member-pointer types are
// copied with only the 0x200003 qualifier bits kept, recursively; other
// types are returned as they are.
Type* IRO_UnqualifiedType(Type* type) {
    if (!type) {
        return NULL;
    }
    switch (type->type) {
        case TYPEVOID:
        case TYPEINT:
        case TYPEFLOAT:
        case TYPEENUM:
        case TYPESTRUCT:
        case TYPEBITFIELD:
            return type;
        case TYPECLASS:
        case TYPETEMPLATE:
            return type;
        case TYPEFUNC:
            return type;
        case TYPEMEMBERPOINTER: {
            TypeMemberPointer* tm = galloc(sizeof(TypeMemberPointer));
            *tm = *TYPE_MEMBER_POINTER(type);
            tm->ty1 = IRO_UnqualifiedType(TYPE_MEMBER_POINTER(type)->ty1);
            tm->qual &= 0x200003;
            return (Type*)tm;
        }
        case TYPEPOINTER:
        case TYPEARRAY: {
            TypePointer* tp = galloc(sizeof(TypePointer));
            *tp = *TYPE_POINTER(type);
            tp->target = IRO_UnqualifiedType(TYPE_POINTER(type)->target);
            tp->qual &= 0x200003;
            return (Type*)tp;
        }
        case TYPEILLEGAL:
        case TYPELABEL:
        case TYPEOBJCID:
        case TYPETEMPLDEPEXPR:
            return type;
        default:
            CError_FATAL(143);
            return NULL;
    }
}

// 3.0.1 statement and node makers used by the pre-linearizer (0x54FBD0,
// 0x54FBA0, 0x5515E0).
Statement* IRO_NewStatement(int type, ENode* expr) {
    Statement* stmt = lalloc(sizeof(Statement));
    memset(stmt, 0, sizeof(Statement));
    stmt->type = type;
    stmt->expr = expr;
    return stmt;
}

void IRO_CopySourceInfo(Statement* dst, Statement* src) {
    dst->dobjstack = src->dobjstack;
    dst->sourcefilepath = src->sourcefilepath;
    dst->sourceoffset = src->sourceoffset;
    dst->value = src->value;
    dst->flags = src->flags;
}

// 0x551500: 3.0.1's floating constant node
ENode* func_00551500(Float val, Type* type) {
    ENode* enode = lalloc(sizeof(ENode));
    memset(enode, 0, sizeof(ENode));
    enode->type = EFLOATCONST;
    enode->data.floatval = val;
    enode->rtype = IRO_UnqualifiedType(type);
    return enode;
}

ENode* IRO_NewIntConstENode(CInt64 val, Type* type) {
    ENode* enode;

    IRO_TruncateValueToType(&val, type);
    enode = lalloc(sizeof(ENode));
    memset(enode, 0, sizeof(ENode));
    enode->type = EINTCONST;
    enode->data.intval = val;
    enode->rtype = IRO_UnqualifiedType(type);
    return enode;
}
#endif
#if MWCCPSP

// 3.0.1 (0x54F790): the labels whose address the function takes, for a goto
// through an expression: ELABEL operands, label objects of this function
// (the assert after that loop always fires; such objects do not reach the
// optimizer), and every live user label when a goto's target is computed.
IROAddressedLabel* IRO_AddressedLabels;
SInt32 IRO_NumAddressedLabels;

static IROAddressedLabel* NewAddressedLabel(CLabel* label) {
    IROAddressedLabel* al = IRO_Alloc(sizeof(IROAddressedLabel));
    memset(al, 0, sizeof(IROAddressedLabel));
    al->label = label;
    al->next = IRO_AddressedLabels;
    IRO_NumAddressedLabels++;
    IRO_AddressedLabels = al;
    return al;
}

void IRO_CollectAddressedLabels(IROLinear* first, IROLinear* last) {
    IROLinear* linear;
    CLabel* label;
    Object* obj;

    IRO_NumAddressedLabels = 0;
    IRO_AddressedLabels = NULL;
    for (linear = first; linear; linear = linear->next) {
        if (linear->type == IROLinearOperand &&
            linear->u.node &&
            linear->u.node->type == ELABEL &&
            linear->u.node->data.label) {
            NewAddressedLabel(linear->u.node->data.label);
        } else if (linear->type == IROLinearOperand &&
                   linear->u.node &&
                   linear->u.node->type == EOBJREF &&
                   (obj = linear->u.node->data.objref)->datatype == DLABEL) {
            for (label = Labels; label; label = label->next) {
                if (obj->u.label.function == FunctionName && label->uniquename == obj->name) {
                    NewAddressedLabel(label);
                }
            }
            CError_ASSERT(5493, label);
        } else if (linear->type == IROLinearGotoExpr &&
                   linear->u.monadic &&
                   linear->u.monadic->type == IROLinearOp1Arg &&
                   linear->u.monadic->u.monadic &&
                   linear->u.monadic->u.monadic->type != IROLinearOperand) {
            for (label = Labels; label; label = label->next) {
                if (label->stmt && label->uniquename->name[0] != '@') {
                    NewAddressedLabel(label);
                }
            }
        }
        if (linear == last) {
            break;
        }
    }
}

// 3.0.1 (0x54F970): where a goto can go. A computed goto asks the pointer
// analysis (when it ran) for the labels its target can be, else every
// addressed label. IRO_FreeGotoTargets (0x54F940) frees a computed list.
typedef struct PointsToEntry301 {
    IROLinear* x0;
    IROLinear* x4;
    struct PointsToEntry301* next;
} PointsToEntry301;
extern void PointerAnalysis_LookupGotoTargets(
    Object* func, IROLinear* linear, PointsToEntry301** list
);

IROAddressedLabel* IRO_GetGotoTargets(IROLinear* linear, UInt16* count) {
    IROAddressedLabel* result;
    IROAddressedLabel* al;
    PointsToEntry301* list;
    PointsToEntry301* p;
    PointsToEntry301* next;
    CLabel* label;
    Boolean fallback;

    if (linear->type == IROLinearGoto) {
        result = IRO_malloc(sizeof(IROAddressedLabel));
        memset(result, 0, sizeof(IROAddressedLabel));
        result->label = linear->u.label.label;
        *count = 1;
    } else if (linear->type == IROLinearGotoExpr) {
        fallback = FALSE;
        if (!copts.opt_pointer_analysis || !linear->u.monadic->pointsToFunction || !FunctionName) {
            fallback = TRUE;
        } else {
            list = NULL;
            PointerAnalysis_LookupGotoTargets(FunctionName, linear->u.monadic, &list);
            if (!list) {
                fallback = TRUE;
            } else {
                for (p = list; p; p = p->next) {
                    if (!p->x0 ||
                        p->x0 != p->x4 ||
                        p->x0->type != IROLinearOperand ||
                        p->x0->u.node->type != ELABEL) {
                        fallback = TRUE;
                    }
                }
                if (!fallback) {
                    *count = 0;
                    result = NULL;
                    for (p = list; p; p = p->next) {
                        label = p->x0->u.node->data.label;
                        CError_ASSERT(5392, label);
                        al = IRO_malloc(sizeof(IROAddressedLabel));
                        memset(al, 0, sizeof(IROAddressedLabel));
                        al->label = label;
                        al->next = result;
                        ++*count;
                        result = al;
                    }
                }
                while (list) {
                    next = list->next;
                    IRO_free2(list);
                    list = next;
                }
            }
        }
        if (fallback) {
            *count = IRO_NumAddressedLabels;
            result = IRO_AddressedLabels;
        }
    } else {
        CError_FATAL(5423);
    }
    return result;
}

void IRO_FreeGotoTargets(IROAddressedLabel* list) {
    IROAddressedLabel* next;

    if (list != IRO_AddressedLabels && list) {
        do {
            next = list->next;
            IRO_free(list);
            list = next;
        } while (next);
    }
}

// 0x54FB50
Boolean IRO_IsFunctionObject(Object* obj) {
    return (UInt8)(obj->datatype - DFUNC) <= 2;
}

// 3.0.1 replaces the IROAddrRecord walk of MarkSubs1/MarkAssigned with a
// count: the object references among the addends of an EADD tree (the last
// one found is returned) and the other non-constant addends. Name not known.
static void DecomposeAddressExpression(
    IROLinear* linear, IROLinear** objref, int* numObjRefs, int* numOther
) {
    IROLinear* op;

    op = linear->u.diadic.left;
    if (IS_LINEAR_DIADIC(op, EADD)) {
        DecomposeAddressExpression(op, objref, numObjRefs, numOther);
    } else if (!IRO_IsIntConstant(op)) {
        if (IS_LINEAR_ENODE(op, EOBJREF)) {
            ++*numObjRefs;
            *objref = linear->u.diadic.left;
        } else {
            ++*numOther;
        }
    }

    op = linear->u.diadic.right;
    if (IS_LINEAR_DIADIC(op, EADD)) {
        DecomposeAddressExpression(op, objref, numObjRefs, numOther);
    } else if (!IRO_IsIntConstant(op)) {
        if (IS_LINEAR_ENODE(op, EOBJREF)) {
            ++*numObjRefs;
            *objref = linear->u.diadic.right;
        } else {
            ++*numOther;
        }
    }
}

void IRO_FindAddressObjRefs(IROLinear* linear, IROLinear** objref, int* numObjRefs, int* numOther) {
    *objref = NULL;
    *numObjRefs = 0;
    *numOther = 0;
    DecomposeAddressExpression(linear, objref, numObjRefs, numOther);
}

#endif
Boolean IRO_IsAddressMultiply(IROLinear* linear) {
    return FALSE;
}

void IRO_SetupForUserBreakChecking(void) {
    IRO_LastUserBreakTick = 0;
}

void IRO_CheckForUserBreak(void) {
    if (IRO_LastUserBreakTick + 8 < (UInt32)COS_GetTicks()) {
        if (CWUserBreak(cparams.context)) {
            CError_UserBreak();
        }
        IRO_LastUserBreakTick = COS_GetTicks();
    }
}
#if MWCCPSP

// --- the bits of a variable an access reaches (0x5582F0..0x5587A0) ---------

// the address addr (an object's, plus constants, maybe a bitfield's) reaches
// bits *offset.. of var, *count of them (*count comes in as the access
// width); FALSE (with all of var) when the constant part is not known to be
// a small non-negative offset
static inline Boolean AddressBits301(
    IROLinear** paddr, VarRecord* var, SInt32* offset, SInt32* count
) {
    IROAddrRecord301 rec;
    IROLinear* addr = *paddr;
    IROElmList* elm;
    CInt64 value;
    SInt32 bitOffset = 0;
    SInt32 byteOffset = 0;
    SInt32 bits = *count;
    Boolean ok = TRUE;

    if (addr->type == IROLinearOp1Arg && addr->nodetype == EBITFIELD) {
        bitOffset = TYPE_BITFIELD(addr->rtype)->offset;
        bits = TYPE_BITFIELD(addr->rtype)->bitlength;
        addr = addr->u.monadic;
    }
    if (addr->type == IROLinearOp2Arg && addr->nodetype == EADD) {
        func_005513A0(&rec, addr);
        func_005510D0(addr, &rec, IRO_malloc);
        if (rec.numObjRefs == 1) {
            addr = rec.objRefs->element;
        }
        if (rec.numInts) {
            value = cint64_zero;
            for (elm = rec.ints; elm; elm = elm->next) {
                value = CInt64_Add(value, elm->element->u.node->data.intval);
            }
            if (CInt64_IsNegative(&value)) {
                ok = FALSE;
                bitOffset = 0;
                byteOffset = 0;
                bits = var->object->type->size * 8;
            } else {
                CInt64 max;
                CInt64_SetLong(&max, 0x7FFFFFFF);
                if (CInt64_LessEqual(value, max)) {
                    byteOffset = CInt64_GetULong(&value);
                } else {
                    ok = FALSE;
                    bitOffset = 0;
                    byteOffset = 0;
                    bits = var->object->type->size * 8;
                }
            }
        }
        if (rec.numMisc) {
            ok = FALSE;
            bitOffset = 0;
            bits = var->object->type->size * 8;
            byteOffset = 0;
        }
        func_00551310(&rec, IRO_free);
    }
    *paddr = addr;
    *offset = byteOffset * 8 + bitOffset;
    *count = bits;
    return ok;
}

// 0x558360
Boolean func_00558360(IROLinear* addr, VarRecord* var, SInt32* offset, SInt32* count) {
    Boolean ok = AddressBits301(&addr, var, offset, count);

    // 3.0.1 looks the address's variable up and drops it (a pure lookup, and
    // the address need not be an object reference)
    if (addr->type == IROLinearOperand) {
        IRO_FindVar(addr->u.node->data.objref, FALSE, TRUE);
    }
    return ok;
}

// 0x5582F0: the bits of var read through nd, an address of var inside an
// indirection
Boolean func_005582F0(IROLinear* nd, VarRecord* var, SInt32* offset, SInt32* count) {
    IROLinear* ind = nd;

    *count = var->object->type->size * 8;
    do {
        ind = IRO_LocateFather(ind);
    } while (ind && !(ind->type == IROLinearOp1Arg && ind->nodetype == EINDIRECT));
    if (ind) {
        *count = ind->rtype->size * 8;
        return func_00558360(ind->u.monadic, var, offset, count);
    }
    return FALSE;
}

// 0x558570: as 0x558360, for an address that comes down to an object
// reference; *exact says whether the bits are known
Boolean func_00558570(
    IROLinear* addr, VarRecord* var, SInt32* offset, SInt32* count, Boolean* exact
) {
    Boolean ok = AddressBits301(&addr, var, offset, count);

    if (addr->type == IROLinearOperand && addr->u.node->type == EOBJREF) {
        IRO_FindVar(addr->u.node->data.objref, FALSE, TRUE);
        *exact = ok;
        return TRUE;
    }
    return FALSE;
}

// 0x558520: the bits an assignment nd stores
Boolean func_00558520(
    IROLinear* nd, VarRecord* var, SInt32* offset, SInt32* count, Boolean* exact
) {
    IROLinear* addr;

    *count = nd->rtype->size * 8;
    addr = nd->u.diadic.left;
    if (addr->type == IROLinearOp1Arg && addr->nodetype == EINDIRECT) {
        addr = addr->u.monadic;
    }
    return func_00558570(addr, var, offset, count, exact);
}

extern IROLinear* IRO_FindAssignedObjRef(IROLinear* linear, Boolean* isBitfield); // IroVars.c
extern VarRecord* IRO_FindAssigned301(IROLinear* linear, Boolean* isBitfield);

// 0x558770: the variable an address refers to
VarRecord* func_00558770(IROLinear* addr, Boolean* isBitfield) {
    IROLinear* objref = IRO_FindAssignedObjRef(addr, isBitfield);
    VarRecord* var;

    if (objref && (var = IRO_FindVar(objref->u.node->data.objref, FALSE, TRUE))) {
        return var;
    }
    return NULL;
}

// 0x558750
VarRecord* func_00558750(IROLinear* addr) {
    return func_00558770(addr, NULL);
}

// 0x558760
VarRecord* func_00558760(IROLinear* nd) {
    return IRO_FindAssigned301(nd, NULL);
}

#endif
#if MWCCPSP
// --- 3.0.1 walks that pass an argument along -------------------------------

// IRO_WalkTree with an argument (3.0.1's IRO_WalkTree, 0x553A10, takes one)
static void WalkTreeArg301(IROLinear* linear, IROWalkTreeArgFunc func, void* arg) {
    int i;

    func(linear, TRUE, arg);
    switch (linear->type) {
        case IROLinearOperand:
            break;
        case IROLinearOp1Arg:
            WalkTreeArg301(linear->u.monadic, func, arg);
            break;
        case IROLinearOp2Arg:
            WalkTreeArg301(linear->u.diadic.left, func, arg);
            WalkTreeArg301(linear->u.diadic.right, func, arg);
            break;
        case IROLinearOp3Arg:
            WalkTreeArg301(linear->u.args3.a, func, arg);
            WalkTreeArg301(linear->u.args3.b, func, arg);
            WalkTreeArg301(linear->u.args3.c, func, arg);
            break;
        case IROLinearFunccall:
            WalkTreeArg301(linear->u.funccall.callee, func, arg);
            for (i = 0; i < linear->u.funccall.argCount; i++) {
                WalkTreeArg301(linear->u.funccall.args[i], func, arg);
            }
            break;
        default:
            break;
    }
    func(linear, FALSE, arg);
}

// 0x553000: IRO_WalkInts with an argument
void func_00553000(IROLinear* first, IROLinear* last, IROWalkTreeArgFunc func, void* arg) {
    IROLinear* scan;

    for (scan = first; scan; scan = scan->next) {
        switch (scan->type) {
            case IROLinearOperand:
            case IROLinearOp1Arg:
            case IROLinearOp2Arg:
            case IROLinearOp3Arg:
            case IROLinearScope:
            case IROLinearFunccall:
                WalkTreeArg301(scan, func, arg);
                break;
            case IROLinearGotoExpr:
                WalkTreeArg301(scan->u.monadic, func, arg);
                break;
            case IROLinearIf:
            case IROLinearIfNot:
                WalkTreeArg301(scan->u.label.cond, func, arg);
                break;
            case IROLinearReturn:
                if (scan->u.monadic) {
                    WalkTreeArg301(scan->u.monadic, func, arg);
                }
                break;
            case IROLinearSwitch:
                WalkTreeArg301(scan->u.swtch.cond, func, arg);
                break;
            case IROLinearBeginCatch:
            case IROLinearEndCatch:
            case IROLinearEndCatchDtor:
                WalkTreeArg301(scan->u.ctch.linear, func, arg);
                break;
            case IROLinearAsm:
                func(scan, TRUE, arg);
                func(scan, FALSE, arg);
                break;
            case IROLinearNop:
            case IROLinearGoto:
            case IROLinearLabel:
            case IROLinearEnd:
                break;
            default:
                CError_FATAL(2507);
                break;
        }
        if (scan == last) {
            return;
        }
    }
}

// a search through a statement that stops at the first match
typedef struct StmtSearch301 {
    jmp_buf buf;
    IROLinear* target; // 0x551B90's
    Boolean found;
} StmtSearch301;

static void SideEffectAction301(IROLinear* linear, Boolean isFirst, void* arg) {
    StmtSearch301* search = arg;

    if (isFirst) {
        switch (linear->type) {
            case IROLinearOperand:
            case IROLinearOp1Arg:
            case IROLinearOp2Arg:
            case IROLinearOp3Arg:
            case IROLinearScope:
            case IROLinearFunccall:
            case IROLinearAsm:
                if (IRO_HasSideEffect(linear)) {
                    search->found = TRUE;
                    longjmp(search->buf, 1);
                }
                break;
        }
    }
}

// 0x551B00: the statement has a side effect
Boolean func_00551B00(IROLinear* stmt) {
    StmtSearch301 search;

    memset(&search, 0, sizeof(search));
    search.found = FALSE;
    if (!setjmp(search.buf)) {
        func_00553000(stmt, stmt, SideEffectAction301, &search);
    }
    return search.found;
}

static void UseAction301(IROLinear* linear, Boolean isFirst, void* arg) {
    StmtSearch301* search = arg;

    if (isFirst && linear == search->target) {
        search->found = TRUE;
        longjmp(search->buf, 1);
    }
}

// 0x551B90: the statement holds use
Boolean func_00551B90(IROLinear* stmt, IROLinear* use) {
    StmtSearch301 search;

    memset(&search, 0, sizeof(search));
    search.target = use;
    search.found = FALSE;
    if (!setjmp(search.buf)) {
        func_00553000(stmt, stmt, UseAction301, &search);
    }
    return search.found;
}

extern void func_0056AB70(
    IROLinear* linear, Boolean alloc, Boolean stop, Boolean recurse
); // IroCSE.c

// a side effect in nd, or a read of a variable in kills
static inline void SideEffectOrUse301(
    IROLinear* nd, BitVector* kills, BitVector* uses, Boolean* result
) {
    if (IRO_HasSideEffect(nd)) {
        *result = TRUE;
    } else {
        Bv_Clear(uses);
        func_0056AB70(nd, FALSE, FALSE, TRUE);
        Bv_Or(IRO_Depends, uses);
        if (Bv_BitsInCommon(kills, uses)) {
            *result = TRUE;
        }
    }
}

// 0x5518A0: a part of nd evaluated in no fixed order with excluded has a
// side effect or reads a variable in kills
void func_005518A0(
    IROLinear* nd, IROLinear* excluded, BitVector* kills, BitVector* uses, Boolean* result
) {
    int i;

    switch (nd->type) {
        case IROLinearOp2Arg:
            if (nd->nodetype != ELAND && nd->nodetype != ELOR && nd->nodetype != ECOMMA) {
                if (nd->u.diadic.left != excluded) {
                    SideEffectOrUse301(nd->u.diadic.left, kills, uses, result);
                }
                if (nd->u.diadic.right != excluded) {
                    SideEffectOrUse301(nd->u.diadic.right, kills, uses, result);
                }
            }
            break;
        case IROLinearFunccall:
            if (nd->u.funccall.callee != excluded) {
                SideEffectOrUse301(nd->u.funccall.callee, kills, uses, result);
            }
            for (i = 0; !*result && i < nd->u.funccall.argCount; i++) {
                if (nd->u.funccall.args[i] != excluded) {
                    SideEffectOrUse301(nd->u.funccall.args[i], kills, uses, result);
                }
            }
            break;
        default:
            break;
    }
}
#endif
