// mwccpsp 3.0.1: collapsing chains of comparisons into bit tests
// (#pragma opt_collapselogicalselectionintobittest; IroTransform.c by its
// asserts, 0x56D490..0x56E0A0), and the ENode tree helpers it uses (IroUtil.c,
// IroType.c). None of this is in the 2.4.7 source.
//
//     x == 1 || x == 3 || x == 4      becomes  (x - 1) <= 3 && !!(0xD & (1 << (x - 1)))
//     x != 1 && x != 3 && x != 4      becomes  (x - 1) > 3 || !!!(0xD & (1 << (x - 1)))
//
// when the values fall in few enough 32-bit (or, with ..._uselonglong, 64-bit)
// windows to be worth it.
#include "compiler/common.h"
#include "compiler/CError.h"
#include "compiler/CExpr.h"
#include "compiler/CFunc.h"
#include "compiler/CInt64.h"
#include "compiler/CMachine.h"
#include "compiler/CParser.h"
#include "compiler/CompilerTools.h"
#include "compiler/enode.h"
#include "compiler/IroDump.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"
#include "compiler/objects.h"
#include "compiler/types.h"
#include "compiler/tokens.h"
#include <string.h>

extern Boolean is_volatile_object(Object* obj);
extern void IRO_CheckForUserBreak(void);
extern CInt64 cint64_one;

typedef Boolean (*ENodeWalkFunc)(ENode** ep, int phase, void* arg);

// --- IroUtil.c ------------------------------------------------------------

#if MWCCPS2
#line 38 "IroUtil.c" // b210 names these in its asserts
#endif
// 0x54F380: visit the tree at *ep; phase 1 before a node's operands (a FALSE
// return skips them), phase 2 after, as flags asks
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -58 // b145 asserts
#if !MWCCPS2
#line 39 "IroUtil.c"
#endif
void func_0054F380(ENode** ep, ENodeWalkFunc func, int flags, void* arg) {
    ENode* expr;
    ENodeList* list;

    if ((flags & 1) && !func(ep, 1, arg)) {
        return;
    }
    expr = *ep;
    CError_ASSERT(5820, expr);
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
            func_0054F380(&expr->data.monadic, func, flags, arg);
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
        case ELAND:
        case ELOR:
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
        case ECOMMA:
        case EMIN:
        case EMAX:
        case EPMODULO:
        case EROTL:
        case EROTR:
        case EBCLR:
        case EBTST:
        case EBSET:
        case EPOINTERSTAR:
        case EDOTSTAR:
            func_0054F380(&expr->data.diadic.left, func, flags, arg);
            func_0054F380(&expr->data.diadic.right, func, flags, arg);
            break;
        case ECOND:
        case ECONDASS:
            func_0054F380(&expr->data.cond.cond, func, flags, arg);
            func_0054F380(&expr->data.cond.expr1, func, flags, arg);
            func_0054F380(&expr->data.cond.expr2, func, flags, arg);
            break;
        case ENULLCHECK:
            func_0054F380(&expr->data.nullcheck.nullcheckexpr, func, flags, arg);
            break;
        case EFUNCCALL:
        case EFUNCCALLP:
            func_0054F380(&expr->data.funccall.funcref, func, flags, arg);
            for (list = expr->data.funccall.args; list; list = list->next) {
                func_0054F380(&list->node, func, flags, arg);
            }
            break;
        case EINTCONST:
        case EFLOATCONST:
        case ESTRINGCONST:
        case EOBJREF:
        case EPRECOMP:
        case ELABEL:
        case ESCOPEBEGIN:
        case ESCOPEEND:
        case EINSTRUCTION:
            break;
        default:
            CError_FATAL(5880);
    }
    if (flags & 2) {
        func(ep, 2, arg);
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#if !MWCCPS2
#line 142 "IroLogicalSelection.c"
#endif

// 0x552F00: func(operand, TRUE, arg) for each operand of an IROLinear
#include "compiler/IroLinearForm.h"
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -32 // b145 asserts
void func_00552F00(
    IROLinear* nd, void (*func)(IROLinear* linear, Boolean isEntry, void* arg), void* arg
) {
    SInt32 i;

    switch (nd->type) {
        case IROLinearOp1Arg:
        case IROLinearBeginCatch:
        case IROLinearEndCatch:
        case IROLinearEndCatchDtor:
        case IROLinearGotoExpr:
            func(nd->u.monadic, TRUE, arg);
            break;
        case IROLinearOp2Arg:
            func(nd->u.diadic.left, TRUE, arg);
            func(nd->u.diadic.right, TRUE, arg);
            break;
        case IROLinearOp3Arg:
            func(nd->u.args3.a, TRUE, arg);
            func(nd->u.args3.b, TRUE, arg);
            func(nd->u.args3.c, TRUE, arg);
            break;
        case IROLinearFunccall:
            func(nd->u.funccall.callee, TRUE, arg);
            for (i = 0; i < nd->u.funccall.argCount; i++) {
                func(nd->u.funccall.args[i], TRUE, arg);
            }
            break;
        case IROLinearIf:
        case IROLinearIfNot:
            func(nd->u.label.cond, TRUE, arg);
            break;
        case IROLinearReturn:
            if (nd->u.monadic) {
                func(nd->u.monadic, TRUE, arg);
            }
            break;
        case IROLinearSwitch:
            func(nd->u.swtch.cond, TRUE, arg);
            break;
        case IROLinearEntry:
        case IROLinearExit:
        case IROLinear18:
        case IROLinear19:
            CError_FATAL(2581);
            break;
        default:
            break;
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

#if MWCCPS2
#line 194 "IroLogicalSelection.c"
#endif
static Boolean HasSideEffectNode(ENode** ep, int phase, void* arg) {
#if MWCCPS2
    ENode* expr = *ep;
    Boolean* result = (Boolean*)arg;

    if (!(expr->flags & 2)) {
        if (expr->type == EOBJREF && is_volatile_object(expr->data.objref)) {
            goto found;
        }
        if (!IRO_IsAssignOp[expr->type] && (UInt8)(expr->type - EFUNCCALL) > 1) {
            return TRUE;
        }
    }
found:
    *result = TRUE;
    return FALSE;
#else
    ENode* expr = *ep;

    if (!(expr->flags & 2)) {
        if (expr->type == EOBJREF && is_volatile_object(expr->data.objref)) {
            goto found;
        }
        if (!IRO_IsAssignOp[expr->type] && (UInt8)(expr->type - EFUNCCALL) > 1) {
            return TRUE;
        }
    }
found:
    *(Boolean*)arg = TRUE;
    return FALSE;
#endif
}

// 0x54F2F0: does the tree assign, call or read a volatile
#if MWCCPS2
__declspec(noinline) // b210 calls it (func_0056D580)
#endif
Boolean func_0054F2F0(ENode* expr) {
    Boolean found = FALSE;

    if (expr) {
        func_0054F380(&expr, HasSideEffectNode, 1, &found);
    }
    return found;
}

// 0x54F500: are two trees the same expression. As in 3.0.1, a ?: matches
// when any of its operands does, and a floating-point zero matches nothing.
Boolean func_0054F500(ENode* a, ENode* b) {
    UInt16 mask;
    ENodeList* la;
    ENodeList* lb;

#if MWCCPS2 && MWCCPS2_BUILD < 210 // b145 masks the same flags for casts
    mask = 0xFDFF;
#else
    mask = (a->type == ETYPCON && b->type == ETYPCON) ? 0xFD7F : 0xFDFF;
#endif
    if (a->type != b->type || (a->flags & mask) != (b->flags & mask)) {
        return FALSE;
    }
    if (!IRO_TypesEqual(a->rtype, b->rtype)) {
        return FALSE;
    }
    switch (a->type) {
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
            return func_0054F500(a->data.monadic, b->data.monadic);
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
        case ELAND:
        case ELOR:
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
        case ECOMMA:
        case EMIN:
        case EMAX:
        case EPMODULO:
        case EROTL:
        case EROTR:
        case EBCLR:
        case EBTST:
        case EBSET:
        case EPOINTERSTAR:
        case EDOTSTAR:
            return func_0054F500(a->data.diadic.left, b->data.diadic.left) &&
                func_0054F500(a->data.diadic.right, b->data.diadic.right);
        case ECOND:
            return func_0054F500(a->data.cond.cond, b->data.cond.cond) ||
                func_0054F500(a->data.cond.expr1, b->data.cond.expr1) ||
                func_0054F500(a->data.cond.expr2, b->data.cond.expr2);
        case ENULLCHECK:
            return func_0054F500(
                       a->data.nullcheck.nullcheckexpr, b->data.nullcheck.nullcheckexpr
                   ) &&
                func_0054F500(a->data.nullcheck.condexpr, b->data.nullcheck.condexpr);
        case EINTCONST:
            return CInt64_Equal(a->data.intval, b->data.intval);
        case EFLOATCONST:
            if (CMach_FloatIsZero(a->data.floatval)) {
                return FALSE;
            }
            return CMach_CalcFloatDiadicBool(
                a->rtype, a->data.floatval, TK_LOGICAL_EQ, b->data.floatval
            );
        case ESTRINGCONST:
            if (a->data.string.size != b->data.string.size) {
                return FALSE;
            }
            return !memcmp(a->data.string.data, b->data.string.data, a->data.string.size);
        case EOBJREF:
            return a->data.objref == b->data.objref;
        case ELABEL:
        case ESCOPEBEGIN:
        case ESCOPEEND:
            return a->data.label == b->data.label;
        case EPRECOMP:
            return a->data.precompid == b->data.precompid;
        case EFUNCCALL:
        case EFUNCCALLP:
            if (!func_0054F500(a->data.funccall.funcref, b->data.funccall.funcref)) {
                return FALSE;
            }
            for (la = a->data.funccall.args, lb = b->data.funccall.args; la;
                 la = la->next, lb = lb->next) {
                if (!func_0054F500(la->node, lb->node)) {
                    return FALSE;
                }
            }
            return TRUE;
        default:
            return FALSE;
    }
}

static Boolean CopyNode(ENode** ep, int phase, void* arg) {
    ENode* copy;
    UInt8 type;

#if MWCCPS2
    if (phase & 1) {
        type = (*ep)->type;
        copy = lalloc(sizeof(ENode));
        memset(copy, 0, sizeof(ENode));
        copy->type = type;
        *copy = **ep;
        *ep = copy;
    }
    return TRUE;
#else
    if (!(phase & 1)) {
        return TRUE;
    }
    type = (*ep)->type;
    copy = lalloc(sizeof(ENode));
    memclrw(copy, sizeof(ENode));
    copy->type = type;
    *copy = **ep;
    *ep = copy;
    return TRUE;
#endif
}

// 0x552810: a copy of the tree (argument lists are shared)
ENode* func_00552810(ENode* expr) {
#if MWCCPS2
    ENode* copy = expr;

    func_0054F380(&copy, CopyNode, 1, NULL);
    return copy;
#else
    func_0054F380(&expr, CopyNode, 1, NULL);
    return expr;
#endif
}

// 0x555B30: an integer constant of an integral type
#if MWCCPS2
__declspec(noinline) // b210 calls it (func_0056DF60)
#endif
Boolean func_00555B30(ENode* expr) {
    Type* type = expr->rtype;

    if (type &&
        type->type == TYPEINT &&
        TYPE_INTEGRAL(type)->integral < IT_17 &&
        expr->type == EINTCONST) {
        return TRUE;
    }
    return FALSE;
}

// --- IroType.c ------------------------------------------------------------

#if MWCCPS2
#line 409 "IroType.c" // b210 names these in its asserts
#endif
// 0x55CA90: the unsigned type of the same size, from the type's own size up
Type* func_0055CA90(Type* type) {
    if (!(type->type == TYPEENUM ||
          type->type == TYPEPOINTER ||
          (type->type == TYPEINT && TYPE_INTEGRAL(type)->integral < IT_17))) {
fail:
        CError_FATAL(245);
        return NULL;
    }
    if (type == TYPE(&stbool) ||
        type == TYPE(&stwchar) ||
        type == TYPE(&stchar) ||
        type == TYPE(&stsignedchar) ||
        type == TYPE(&stunsignedchar)) {
        goto from_char;
    }
    if (type == TYPE(&stsignedshort) || type == TYPE(&stunsignedshort)) {
        goto from_short;
    }
    if (type == TYPE(&stsignedint) || type == TYPE(&stunsignedint)) {
        goto from_int;
    }
    if (type == TYPE(&stsignedlong) || type == TYPE(&stunsignedlong)) {
        goto from_long;
    }
    if (type == TYPE(&stsignedlonglong) || type == TYPE(&stunsignedlonglong)) {
        goto from_longlong;
    }
from_char:
    if (type->size == stunsignedchar.size) {
        return TYPE(&stunsignedchar);
    }
from_short:
    if (type->size == stunsignedshort.size) {
        return TYPE(&stunsignedshort);
    }
from_int:
    if (type->size == stunsignedint.size) {
        return TYPE(&stunsignedint);
    }
from_long:
    if (type->size == stunsignedlong.size) {
        return TYPE(&stunsignedlong);
    }
from_longlong:
    if (type->size == stunsignedlonglong.size) {
        return TYPE(&stunsignedlonglong);
    }
    goto fail;
}

// 0x55C980: the signed type of the same size, from the type's own size up
Type* func_0055C980(Type* type) {
    if (!(type->type == TYPEENUM ||
          type->type == TYPEPOINTER ||
          (type->type == TYPEINT && TYPE_INTEGRAL(type)->integral < IT_17))) {
fail:
        CError_FATAL(305);
        return NULL;
    }
    if (type == TYPE(&stbool) ||
        type == TYPE(&stwchar) ||
        type == TYPE(&stchar) ||
        type == TYPE(&stsignedchar) ||
        type == TYPE(&stunsignedchar)) {
        goto from_char;
    }
    if (type == TYPE(&stsignedshort) || type == TYPE(&stunsignedshort)) {
        goto from_short;
    }
    if (type == TYPE(&stsignedint) || type == TYPE(&stunsignedint)) {
        goto from_int;
    }
    if (type == TYPE(&stsignedlong) || type == TYPE(&stunsignedlong)) {
        goto from_long;
    }
    if (type == TYPE(&stsignedlonglong) || type == TYPE(&stunsignedlonglong)) {
        goto from_longlong;
    }
from_char:
    if (type->size == stsignedchar.size) {
        return TYPE(&stsignedchar);
    }
from_short:
    if (type->size == stsignedshort.size) {
        return TYPE(&stsignedshort);
    }
from_int:
    if (type->size == stsignedint.size) {
        return TYPE(&stsignedint);
    }
from_long:
    if (type->size == stsignedlong.size) {
        return TYPE(&stsignedlong);
    }
from_longlong:
    if (type->size == stsignedlonglong.size) {
        return TYPE(&stsignedlonglong);
    }
    goto fail;
}

#if MWCCPS2
#line 503 "IroLogicalSelection.c"
#endif
// 0x55A620: the integral promotion of type (an enum by its base type)
Type* func_0055A620(Type* type) {
    if (type->type == TYPEENUM) {
        type = TYPE_ENUM(type)->enumtype;
    }
    if (TYPE_INTEGRAL(type)->integral >= IT_INT) {
        return type;
    }
    if (is_unsigned(type)) {
        return TYPE(&stunsignedint);
    }
    return TYPE(&stsignedint);
}

// --- IroTransform.c -------------------------------------------------------

typedef struct BitTestValue {
    CInt64 value;
    struct BitTestValue* next;
} BitTestValue;

// the comparisons of one && or || chain against one expression
typedef struct BitTestSet {
    ENode* op; // +0x00: the && or || node
    ENode* expr; // +0x04: the expression compared
    CInt64 min; // +0x08
    CInt64 max; // +0x10
    BitTestValue* values; // +0x18: ascending, no duplicates
    SInt32 count; // +0x1C: comparisons
    Boolean wrapped; // +0x20: min and max swapped after an overflow
} BitTestSet;

// 0x56DF60: is cmp (an operand of op) "expr == value" (for ||) or
// "expr != value" (for &&); a truth test of an indirection counts as a
// comparison with zero
Boolean func_0056DF60(ENode* op, ENode* cmp, ENode** expr, CInt64* value) {
    ENode* inner;

    if (op->type == ELOR) {
        if (cmp->type == EEQU) {
            goto compare;
        }
        if (cmp->type == ELOGNOT) {
            for (inner = cmp->data.monadic; inner->type == ETYPCON; inner = inner->data.monadic) {}
            if (inner->type == EINDIRECT) {
                if (expr) {
                    *expr = cmp;
                }
                if (value) {
                    *value = cint64_zero;
                }
                return TRUE;
            }
        }
        return FALSE;
    }
    if (op->type != ELAND) {
        return FALSE;
    }
    if (cmp->type == ENOTEQU) {
        goto compare;
    }
    for (inner = cmp; inner->type == ETYPCON; inner = inner->data.monadic) {}
    if (inner->type == EINDIRECT) {
        if (expr) {
            *expr = cmp->data.monadic;
        }
        if (value) {
            *value = cint64_zero;
        }
        return TRUE;
    }
    return FALSE;

compare:
    if (func_00555B30(cmp->data.diadic.left)) {
        if (expr) {
            *expr = cmp->data.diadic.right;
        }
        if (value) {
            *value = cmp->data.diadic.left->data.intval;
        }
        return TRUE;
    }
    if (func_00555B30(cmp->data.diadic.right)) {
        if (expr) {
            *expr = cmp->data.diadic.left;
        }
        if (value) {
            *value = cmp->data.diadic.right->data.intval;
        }
        return TRUE;
    }
    return FALSE;
}

// 0x56DE70: add a comparison of expr with value, if expr is the set's
Boolean func_0056DE70(BitTestSet* set, ENode* expr, CInt64 value) {
    BitTestValue* node;
    BitTestValue** link;

    if (!func_0054F500(expr, set->expr)) {
        return FALSE;
    }
    if (CInt64_Less(value, set->min)) {
        set->min = value;
    } else if (CInt64_Greater(value, set->max)) {
        set->max = value;
    }
    node = lalloc(sizeof(BitTestValue));
    node->value = value;
    for (link = &set->values; *link && CInt64_Greater(value, (*link)->value);
         link = &(*link)->next) {}
    if (*link) {
        if (!CInt64_Equal(value, (*link)->value)) {
            node->next = *link;
            *link = node;
        }
    } else {
        *link = node;
        node->next = NULL;
    }
    set->count++;
    return TRUE;
}

// 0x56DD80: when max - min overflows, swap them and move the negative
// values after the others
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -19 // b145 asserts
void func_0056DD80(BitTestSet* set) {
    CInt64 tmp;
    BitTestValue* last;
    BitTestValue* first;

    CError_ASSERT(4911, !set->wrapped);
    if (!CInt64_Less(CInt64_Sub(set->max, set->min), cint64_zero)) {
        return;
    }
    set->wrapped = TRUE;
    tmp = set->min;
    set->min = set->max;
    set->max = tmp;
    for (last = set->values; last->next; last = last->next) {}
    first = set->values;
    while (CInt64_Less(first->value, cint64_zero)) {
        last->next = first;
        last = first;
        first = first->next;
        set->values = first;
        last->next = NULL;
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// 0x56DCE0: how many width-bit windows the values need
SInt32 func_0056DCE0(BitTestSet* set, SInt32 width) {
    BitTestValue* v;
    CInt64 start;
    CInt64 diff;
    CInt64 limit;
    SInt32 ranges = 0;

    v = set->values;
    while (v) {
        start = v->value;
        while (v) {
            diff = CInt64_Sub(v->value, start);
            CInt64_SetLong(&limit, width);
            if (!CInt64_LessU(diff, limit)) {
                break;
            }
            v = v->next;
        }
        if (v && CInt64_IsZero(&diff)) {
            v = v->next;
        }
        ranges++;
    }
    return ranges;
}

// 0x56DC10: IsLogicalSelectionBitTestBeneficial; *wide is set when the
// 64-bit form is allowed and needs fewer windows
#if MWCCPS2
__declspec(noinline) // b210 calls it (func_0056D580)
#endif
Boolean func_0056DC10(BitTestSet* set, Boolean* wide) {
    SInt32 n32;
    SInt32 n64;

    n32 = func_0056DCE0(set, stunsignedint.size * 8);
    if (copts.opt_collapselogicalselectionintobittest_uselonglong) {
        n64 = func_0056DCE0(set, stunsignedlonglong.size * 8);
        *wide = n64 * 3 < n32 * 2;
        if (*wide) {
            if (n64 * 2 >= set->count) {
                IRO_Dump(
                    "IsLogicalSelectionBitTestBeneficial: no (%d vals, %d llranges)\n",
                    set->count,
                    n64
                );
                return FALSE;
            }
            IRO_Dump(
                "IsLogicalSelectionBitTestBeneficial: yes (%d vals, %d llranges)\n", set->count, n64
            );
            return TRUE;
        }
    }
    if (n32 * 2 >= set->count) {
        IRO_Dump("IsLogicalSelectionBitTestBeneficial: no (%d vals, %d ranges)\n", set->count, n32);
        return FALSE;
    }
    IRO_Dump("IsLogicalSelectionBitTestBeneficial: yes (%d vals, %d ranges)\n", set->count, n32);
    return TRUE;
}

// 0x56D990: the test for the window first..last (mask: the values present,
// bit 0 for first)
ENode* func_0056D990(
    ENode* unused, BitTestSet* set, Type* type, Boolean wide, CInt64 first, CInt64 last, CInt64 mask
) {
    Type* utype;
    ENode* value;
    ENode* range;
    ENode* bits;
    CInt64 span;
    CInt64 full;

    utype = wide ? TYPE(&stunsignedlonglong) : TYPE(&stunsignedint);

    value = makemonadicnode(func_00552810(set->expr), ETYPCON);
    value->rtype = IRO_UnqualifiedType(type);
    if (type->type == TYPEINT && TYPE_INTEGRAL(type)->integral == IT_BOOL) {
        CError_FATAL(5095);
    }
    range = makediadicnode(
        makediadicnode(value, IRO_NewIntConstENode(first, type), ESUB),
        IRO_NewIntConstENode(CInt64_Sub(last, first), type),
        set->op->type == ELOR ? ELESSEQU : EGREATER
    );
    range->rtype = CParser_GetBoolType();

    value = makemonadicnode(func_00552810(set->expr), ETYPCON);
    value->rtype = type;
    if (type->type == TYPEINT && TYPE_INTEGRAL(type)->integral == IT_BOOL) {
        CError_FATAL(5109);
    }
    span = CInt64_Sub(last, first);
    if (span.lo + 1 < 64) {
        CInt64 n;
        CInt64_SetLong(&n, span.lo + 1);
        full = CInt64_Inv(CInt64_Shl(CInt64_Inv(cint64_zero), n));
    } else {
        full = CInt64_Inv(cint64_zero);
    }
    if (CInt64_Equal(mask, full)) {
        return range;
    }

    bits = makediadicnode(
        IRO_NewIntConstENode(mask, utype),
        makediadicnode(
            IRO_NewIntConstENode(cint64_one, utype),
            makediadicnode(value, IRO_NewIntConstENode(first, type), ESUB),
            ESHL
        ),
        EAND
    );
    bits = makemonadicnode(bits, ELOGNOT);
    bits->rtype = CParser_GetBoolType();
    bits = makemonadicnode(bits, ELOGNOT);
    bits->rtype = CParser_GetBoolType();
    if (set->op->type == ELAND) {
        bits = makemonadicnode(bits, ELOGNOT);
    }
    return makediadicnode(range, bits, set->op->type == ELOR ? ELAND : ELOR);
}

// 0x56D720: the tests for all the windows, joined with the chain's operator
ENode* func_0056D720(BitTestSet* set, Boolean wide) {
    Type* utype;
    SInt32 width;
    ENode* result;
    ENode* test;
    BitTestValue* v;
    CInt64 first;
    CInt64 last;
    CInt64 mask;
    CInt64 diff;
    CInt64 limit;
    Boolean big;

    utype = func_0055CA90(set->expr->rtype);
    width = (wide ? stunsignedlonglong.size : stunsignedint.size) * 8;
    result = NULL;
    v = set->values;
    while (v) {
        first = last = v->value;
        mask = cint64_zero;
        while (v) {
            diff = CInt64_Sub(v->value, first);
            CInt64_SetLong(&limit, width);
            if (!CInt64_LessU(diff, limit)) {
                break;
            }
            last = v->value;
            mask = CInt64_Or(mask, CInt64_Shl(cint64_one, diff));
            v = v->next;
        }
        if (CInt64_Equal(last, first)) {
            test = makediadicnode(
                func_00552810(set->expr),
                IRO_NewIntConstENode(first, set->expr->rtype),
                set->op->type == ELOR ? EEQU : ENOTEQU
            );
            test->rtype = CParser_GetBoolType();
            if (v && CInt64_IsZero(&diff)) {
                v = v->next;
            }
        } else {
            if (wide) {
                CInt64_SetLong(&limit, stunsignedint.size * 8);
                big = CInt64_GreaterEqualU(CInt64_Sub(last, first), limit);
            } else {
                big = FALSE;
            }
            test = func_0056D990(result, set, utype, big, first, last, mask);
        }
        result = result ? makediadicnode(result, test, set->op->type) : test;
    }
    return result;
}

#if MWCCPS2
extern Type* func_0055C960(void); // IroType.c: the type of a comparison
#endif

// 0x56D580: from op (whose right operand compares expr with value), take in
// the comparisons of the same expression to its left; *rest is what is left
// of the chain (NULL when all of it was taken)
ENode* func_0056D580(ENode* op, ENode** rest, ENode* expr, CInt64 value) {
    BitTestSet set;
    ENode* parent;
    ENode* node;
    ENode* test;
    Boolean wide;

    memset(&set, 0, sizeof(set));
#if MWCCPSP_NATIVE
    wide = FALSE; // left unset by 3.0.1 when the 64-bit form is not allowed
#endif
    if (func_0054F2F0(expr)) {
        return op;
    }
    set.op = op;
    set.expr = expr;
    set.min = set.max = value;
    set.values = lalloc(sizeof(BitTestValue));
    set.values->value = value;
    set.values->next = NULL;
    set.count = 1;
    *rest = op;

    node = op;
    for (;;) {
        parent = node;
        node = node->data.diadic.left;
        if (node->type == set.op->type) {
            if (func_0056DF60(node, node->data.diadic.right, &expr, &value) &&
                func_0056DE70(&set, expr, value)) {
                continue;
            }
            *rest = node;
        } else if (func_0056DF60(set.op, node, &expr, &value) && func_0056DE70(&set, expr, value)) {
            *rest = NULL;
        } else {
            *rest = node;
        }
        break;
    }

    func_0056DD80(&set);
    if (!func_0056DC10(&set, &wide)) {
        return set.op;
    }
    test = func_0056D720(&set, wide);
    if (*rest == node) {
        test = makediadicnode(parent->data.diadic.left, test, set.op->type);
#if MWCCPS2
        test->rtype = func_0055C960(); // b210 calls IroType.c's copy
#else
        test->rtype = CParser_GetBoolType();
#endif
    }
    return test;
}

#if MWCCPS2 // b210 builds this from IroTransform.c (its assert names it)
#line 878 "IroTransform.c"
#endif
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -19 // b145 asserts
static Boolean CollapseLogicalSelection(ENode** ep, int phase, void* arg) {
    ENode* op = *ep;
    ENode* expr;
    ENode* rest;
    CInt64 value;

    if (op->flags & 2) {
        return FALSE;
    }
#if MWCCPS2
    if ((UInt8)(op->type - ELAND) > 1 || !func_0056DF60(op, op->data.diadic.right, &expr, &value)) {
        return TRUE;
    }
#else
    if (op->type != ELAND && op->type != ELOR) {
        return TRUE;
    }
    if (!func_0056DF60(op, op->data.diadic.right, &expr, &value)) {
        return TRUE;
    }
#endif
#if MWCCPSP_NATIVE
    rest = op; // 3.0.1 reads it uninitialized when the expression has side effects
#endif
    *ep = func_0056D580(op, &rest, expr, value);
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -19 // b145 asserts
    CError_ASSERT(5386, *ep);
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
    return rest != NULL;
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#if MWCCPS2
#line 909 "IroLogicalSelection.c"
#endif

// 0x56D490
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -19 // b145 asserts
void IRO_CollapseLogicalSelections(Statement* stmt) {
    Statement* next;

    for (; stmt; stmt = next) {
        next = stmt->next;
        switch (stmt->type) {
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
                if (stmt->expr) {
                    func_0054F380(&stmt->expr, CollapseLogicalSelection, 1, NULL);
                }
                break;
        }
    }
    IRO_CheckForUserBreak();
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
