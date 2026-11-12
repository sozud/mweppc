// mwccpsp 3.0.1: range propagation inside flowgraph nodes
// (IRO_RangePropagateInFNodes, 0x58ED50..0x590CA4; IroRangePropagation.c),
// a rewrite of 2.4.7's ERfold: every expression linear gets a range (the
// IROLinear field at +0x28); an assignment to an integer variable records the
// variable's range for the rest of the node; comparisons whose operand ranges
// decide them fold; a value whose range is a single number becomes that
// constant, and x & c goes when x's range has no bit outside c.
#include "compiler/common.h"
#include "compiler/BitVector.h"
#include "compiler/CError.h"
#include "compiler/CFunc.h"
#include "compiler/CInt64.h"
#include "compiler/CMachine.h"
#include "compiler/CParser.h"
#include "compiler/enode.h"
#include "compiler/Exceptions.h"
#include "compiler/IroDump.h"
#include "compiler/IroEval.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroMalloc.h"
#include "compiler/IroPointerAnalysis.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"
#include "compiler/objects.h"
#include "compiler/types.h"

extern Boolean is_volatile_object(Object* obj);
extern Boolean IRO_FunctionCallMightThrowException(IROLinear* linear);
extern void func_0054FC80(ExceptionAction* action, void (*func)(Object*, void*), void* arg);
extern Boolean func_00555B30(ENode* expr);
extern CInt64 cint64_negone;

typedef enum ERangeType {
    ERangeConstant,
    ERangeBounded,
    ERangeComputed,
    ERangeUnknown
} ERangeType;

typedef struct ERange {
    UInt8 type;
    CInt64 upper;
    CInt64 lower;
} ERange;

typedef struct ERecord {
    Object* object;
    ERange range;
    struct ERecord* next;
} ERecord;

ERecord* data_00608CB0; // the variables with a known range in this node

static inline Boolean IsIntType(Type* type) {
    return type->type == TYPEINT && TYPE_INTEGRAL(type)->integral < IT_17;
}

static inline Boolean Known(ERange* range) {
    return range && range->type != ERangeUnknown;
}

static inline Boolean IsNegative(CInt64 value) {
    return CInt64_IsNegative(&value);
}

static inline ERecord* FindRecord(Object* obj) {
    ERecord* record;

    for (record = data_00608CB0; record && record->object != obj; record = record->next) {}
    return record;
}

static inline void InvalidateAll(void) {
    ERecord* record;

    for (record = data_00608CB0; record; record = record->next) {
        record->range.type = ERangeUnknown;
    }
}

// --- IroType.c ------------------------------------------------------------

// 0x55A670: the largest value of an integral type
#if MWCCPS2
__declspec(noinline) // b210 calls it (FoldLinear)
#endif
CInt64 func_0055A670(Type* type) {
    CInt64 value;
    CInt64 bits;

    if (is_unsigned(type)) {
        value = cint64_negone;
    } else {
        CInt64_SetLong(&bits, type->size * 8 - 1);
        value = CInt64_Sub(CInt64_Shl(cint64_one, bits), cint64_one);
    }
    IRO_TruncateValueToType(&value, type);
    return value;
}

// 0x55A710: the smallest value of an integral type
#if MWCCPS2
__declspec(noinline) // b210 calls it (FoldLinear)
#endif
CInt64 func_0055A710(Type* type) {
    CInt64 value;
    CInt64 bits;

    if (is_unsigned(type)) {
        value = cint64_zero;
    } else {
        CInt64_SetLong(&bits, type->size * 8 - 1);
        value = CInt64_Shl(cint64_one, bits);
        IRO_TruncateValueToType(&value, type);
    }
    return value;
}

#if MWCCPS2
#line 115 "IroRangePropagation.c" // b210 names this unit in its asserts
#endif
// --- IroRangePropagation.c ------------------------------------------------

// 0x590C80: a range that does not fit the type, or whose bounds cross, is
// unknown
void func_00590C80(ERange* range, Type* type) {
    CInt64 typeSize;
    CInt64 three;
    CInt64 work;
    CInt64 work2;

    if (!range) {
        return;
    }
    if (!IsIntType(type)) {
        range->type = ERangeUnknown;
        return;
    }
    CInt64_SetLong(&typeSize, type->size);
    CInt64_SetLong(&three, 3);
    if (is_unsigned(type)) {
        if (type->size >= 8) {
            range->type = ERangeUnknown;
        } else {
            work = CInt64_Sub(CInt64_Shl(cint64_one, CInt64_Shl(typeSize, three)), cint64_one);
            if (CInt64_GreaterU(range->upper, work)) {
                range->type = ERangeUnknown;
            } else if (CInt64_GreaterU(range->lower, range->upper)) {
                range->type = ERangeUnknown;
            }
        }
    } else {
        if (type->size >= 8) {
            range->type = ERangeUnknown;
        } else {
            work = CInt64_Sub(
                CInt64_Shl(cint64_one, CInt64_Sub(CInt64_Shl(typeSize, three), cint64_one)),
                cint64_one
            );
            work2 = CInt64_Shl(cint64_negone, CInt64_Sub(CInt64_Shl(typeSize, three), cint64_one));
            if (CInt64_Greater(range->upper, work) || CInt64_Less(range->lower, work2)) {
                range->type = ERangeUnknown;
            } else if (CInt64_Greater(range->lower, range->upper)) {
                range->type = ERangeUnknown;
            }
        }
    }
}

typedef Boolean (*CInt64Compare)(CInt64 a, CInt64 b);

// 0x590200: the value of a comparison its operands' ranges decide
Boolean func_00590200(IROLinear* nd, CInt64* result) {
    ERange* a = nd->u.diadic.left->range;
    ERange* b = nd->u.diadic.right->range;
    ENodeType op;
    Boolean uns;
    CInt64 x;
    CInt64 y;
    CInt64Compare cmp;

    if (!a || a->type == ERangeUnknown) {
        return FALSE;
    }
    if (!b || b->type == ERangeUnknown) {
        return FALSE;
    }
    op = nd->nodetype;
    uns = is_unsigned(nd->u.diadic.left->rtype);
    switch (op) {
        case ELESS:
        case EGREATER:
        case ELESSEQU:
        case EGREATEREQU:
            switch (op) {
                case ELESS: // always true?
                    x = a->upper;
                    y = b->lower;
                    cmp = uns ? CInt64_LessU : CInt64_Less;
                    break;
                case EGREATER:
                    x = a->lower;
                    y = b->upper;
                    cmp = uns ? CInt64_GreaterU : CInt64_Greater;
                    break;
                case ELESSEQU:
                    x = a->upper;
                    y = b->lower;
                    cmp = uns ? CInt64_LessEqualU : CInt64_LessEqual;
                    break;
                case EGREATEREQU:
                    x = a->lower;
                    y = b->upper;
                    cmp = uns ? CInt64_GreaterEqualU : CInt64_GreaterEqual;
                    break;
            }
            if (cmp(x, y)) {
                *result = cint64_one;
                return TRUE;
            }
            switch (op) {
                case ELESS: // always false?
                    x = a->lower;
                    y = b->upper;
                    cmp = uns ? CInt64_GreaterEqualU : CInt64_GreaterEqual;
                    break;
                case EGREATER:
                    x = a->upper;
                    y = b->lower;
                    cmp = uns ? CInt64_LessEqualU : CInt64_LessEqual;
                    break;
                case ELESSEQU:
                    x = a->lower;
                    y = b->upper;
                    cmp = uns ? CInt64_GreaterU : CInt64_Greater;
                    break;
                case EGREATEREQU:
                    x = a->upper;
                    y = b->lower;
                    cmp = uns ? CInt64_LessU : CInt64_Less;
                    break;
            }
            if (cmp(x, y)) {
                *result = cint64_zero;
                return TRUE;
            }
            return FALSE;
        case EEQU:
        case ENOTEQU:
            if (CInt64_Equal(a->lower, b->upper) && CInt64_Equal(a->upper, b->lower)) {
                *result = (op == EEQU) ? cint64_one : cint64_zero;
                return TRUE;
            }
            cmp = uns ? CInt64_LessU : CInt64_Less;
            if (cmp(a->upper, b->lower) || cmp(b->upper, a->lower)) {
                *result = (op == ENOTEQU) ? cint64_one : cint64_zero;
                return TRUE;
            }
            return FALSE;
        default:
            CError_FATAL(693);
            return FALSE;
    }
}

#if MWCCPS2
// unique link names: IroRangePropagation.c (2.4.7) has statics with the same
// names, and the hybrid links replacements by base name
#define InvalidateRangesForKillsByFunctionCall InvalidateRangesForKillsByFunctionCall301
#define SetRangesForKillsByIndirectAssignment SetRangesForKillsByIndirectAssignment301
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

static inline void FreeResultList(IROListNode* resultList) {
    IROListNode* next;

    while (resultList) {
        next = resultList->nextList;
        IRO_free2(resultList);
        resultList = next;
    }
}

static inline void FreeObjectList(ObjectList* list) {
    ObjectList* next;

    while (list) {
        next = list->next;
        IRO_free2(list);
        list = next;
    }
}

// 0x5908C0: the record of an exception action's object
static void InvalidateObject(Object* obj, void* arg) {
    ERecord* record;

    if ((record = FindRecord(obj))) {
        record->range.type = ERangeUnknown;
    }
}

// 0x5905F0: what a call may change
static void InvalidateRangesForKillsByFunctionCall(IROLinear* nd) {
    IROListNode* resultList;
    IROListNode* scan;
    IROLinear* scannd;
    IROLinear* callee;
    Object* obj;
    ObjectList* killList;
    ObjectList* olist;
    ERecord* record;
    Boolean failed;
    Boolean foundObjRef;

    failed = FALSE;
    callee = nd->u.funccall.callee;
    if (callee && copts.opt_pointer_analysis && callee->pointsToFunction && FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupLinearNodePointerExpr(FunctionName, callee, &resultList);
        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail) {
                    failed = TRUE;
                    break;
                }
                foundObjRef = FALSE;
                for (scannd = scan->list.head; scannd != scan->list.tail->next;
                     scannd = scannd->next) {
                    if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                        foundObjRef = TRUE;
                        obj = scannd->u.node->data.objref;
                        CError_ASSERT(463, obj != NULL);
                        killList = NULL;
                        PointerAnalysis_GetFunctionKills(obj, nd, &killList);
                        for (olist = killList; olist; olist = olist->next) {
                            if (!olist->object) {
                                failed = TRUE;
                                break;
                            }
                        }
                        FreeObjectList(killList);
                        if (failed) {
                            break;
                        }
                    }
                }
                if (!foundObjRef) {
                    failed = TRUE;
                }
                if (failed) {
                    break;
                }
            }
            if (!failed) {
                for (scan = resultList; scan; scan = scan->nextList) {
                    for (scannd = scan->list.head; scannd != scan->list.tail->next;
                         scannd = scannd->next) {
                        if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                            obj = scannd->u.node->data.objref;
                            killList = NULL;
                            PointerAnalysis_GetFunctionKills(obj, nd, &killList);
                            for (olist = killList; olist; olist = olist->next) {
                                if ((record = FindRecord(olist->object))) {
                                    record->range.type = ERangeUnknown;
                                }
                            }
                            FreeObjectList(killList);
                        }
                    }
                }
            }
            FreeResultList(resultList);
        } else {
            failed = TRUE;
        }
    } else {
        failed = TRUE;
    }
    if (!failed && nd->stmt && IRO_FunctionCallMightThrowException(nd)) {
        func_0054FC80(nd->stmt->dobjstack, InvalidateObject, NULL);
    }
    if (failed) {
        InvalidateAll();
    }
}

// the record of a variable stored with the given range: in a record of its
// own, unknown unless the store has the variable's size, and after a
// post-increment or post-decrement the stored value
#if MWCCPS2
static inline // b210 inlines it into SetRangesForKillsByIndirectAssignment
#else
static
#endif
    void RecordStore(Object* obj, ERange* range, IROLinear* nd, Boolean bitfield, Boolean multi) {
    ERecord* record;

    if (!(record = FindRecord(obj))) {
        record = IRO_Alloc(sizeof(ERecord));
        record->object = obj;
        record->range = *range;
        record->next = data_00608CB0;
        data_00608CB0 = record;
    } else {
        record->range = *range;
    }
    if (record->range.type != ERangeUnknown) {
        if (multi || obj->type->size != nd->rtype->size) {
            record->range.type = ERangeUnknown;
        } else if (nd->nodetype == EPOSTINC) {
            record->range.upper = CInt64_Add(record->range.upper, cint64_one);
            record->range.lower = CInt64_Add(record->range.lower, cint64_one);
        } else if (nd->nodetype == EPOSTDEC) {
            record->range.upper = CInt64_Sub(record->range.upper, cint64_one);
            record->range.lower = CInt64_Sub(record->range.lower, cint64_one);
        }
        func_00590C80(&record->range, obj->type);
        if (bitfield) {
            record->range.type = ERangeUnknown;
        }
    }
}

// 0x5908F0: a store through a pointer
static void SetRangesForKillsByIndirectAssignment(IROLinear* nd) {
    IROListNode* resultList;
    IROListNode* scan;
    IROLinear* scannd;
    IROLinear* inner;
    IROLinear* ptr;
    Object* obj;
    ERange* range;
    Boolean failed;
    Boolean multi;

    failed = FALSE;
    multi = FALSE;
    inner = nd->u.monadic;
    if (inner &&
        inner->type == IROLinearOp1Arg &&
        inner->nodetype == EINDIRECT &&
        (ptr = inner->u.monadic) &&
        copts.opt_pointer_analysis &&
        ptr->pointsToFunction &&
        FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupLinearNodePointerExpr(FunctionName, ptr, &resultList);
        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail || !ListHasObject(scan)) {
                    failed = TRUE;
                    break;
                }
            }
            if (!failed) {
                if (resultList && resultList->nextList) {
                    multi = TRUE;
                }
                for (scan = resultList; scan; scan = scan->nextList) {
                    for (scannd = scan->list.head; scannd != scan->list.tail->next;
                         scannd = scannd->next) {
                        if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                            obj = scannd->u.node->data.objref;
                            CError_ASSERT(326, obj != NULL);
                            range = nd->range;
                            if (is_volatile_object(obj)) {
                                range->type = ERangeUnknown;
                            }
                            func_00590C80(range, obj->type);
                            if (scannd->rtype->type == TYPEBITFIELD) {
                                range->type = ERangeUnknown;
                            }
                            RecordStore(obj, range, nd, scannd->rtype->type == TYPEBITFIELD, multi);
                        }
                    }
                }
            }
            FreeResultList(resultList);
        } else {
            failed = TRUE;
        }
    } else {
        failed = TRUE;
    }
    if (failed) {
        InvalidateAll();
    }
}

// 0x58EE30: IRO_WalkTree callback
static void NopOutWithRange(IROLinear* linear, Boolean isEntry) {
    if (!isEntry) {
        linear->type = IROLinearNop;
        linear->expr = NULL;
        linear->range = NULL;
    }
}

// 0x58EE60: nd's range, from its operands'
static void FoldLinear(IROLinear* nd) {
    ERange* range;
    ERange* a;
    ERange* b;
    IROLinear* left;
    IROLinear* right;
    IROLinear* constant;
    Object* obj;
    ERecord* record;
    CInt64 value;
    CInt64 work;
    CInt64 mask;
    CInt64 upper;
    CInt64 lower;
    UInt32 bits;
    Boolean uns;
    Boolean bitfield;

    nd->range = IRO_Alloc(sizeof(ERange));
    nd->range->type = ERangeUnknown;
    range = nd->range;

    switch (nd->type) {
        case IROLinearOperand:
            switch (nd->u.node->type) {
                case EINTCONST:
                    if (func_00555B30(nd->u.node)) {
                        range->type = ERangeConstant;
                        range->lower = nd->u.node->data.intval;
                        range->upper = range->lower;
                    }
                    break;
                case EINSTRUCTION:
                    InvalidateAll();
                    break;
            }
            break;

        case IROLinearOp1Arg:
            left = nd->u.monadic;
            switch (nd->nodetype) {
                case EPOSTINC:
                case EPOSTDEC:
                    if (IsIntType(nd->rtype) && Known(left->range)) {
                        range->type = ERangeComputed;
                        range->upper = left->range->upper;
                        range->lower = left->range->lower;
                    }
                    break;
                case EPREINC:
                    if (IsIntType(nd->rtype) && Known(left->range)) {
                        range->type = ERangeComputed;
                        range->upper = CInt64_Add(left->range->upper, cint64_one);
                        range->lower = CInt64_Add(left->range->lower, cint64_one);
                    }
                    break;
                case EPREDEC:
                    if (IsIntType(nd->rtype) && Known(left->range)) {
                        range->type = ERangeComputed;
                        range->upper = CInt64_Sub(left->range->upper, cint64_one);
                        range->lower = CInt64_Sub(left->range->lower, cint64_one);
                    }
                    break;
                case EINDIRECT:
                    if (IsIntType(nd->rtype) &&
                        left->type == IROLinearOperand &&
                        left->u.node->type == EOBJREF &&
                        (obj = left->u.node->data.objref)) {
                        record = FindRecord(obj);
                        if (record &&
                            !is_volatile_object(obj) &&
                            nd->rtype->size == obj->type->size) {
                            *range = record->range;
                        }
                    }
                    break;
                case ELOGNOT:
                    range->type = ERangeBounded;
                    range->upper = cint64_one;
                    range->lower = cint64_zero;
                    break;
            }
            break;

        case IROLinearOp2Arg:
            left = nd->u.diadic.left;
            right = nd->u.diadic.right;
            switch (nd->nodetype) {
                case EMUL:
                case EMULV:
                case EMULASS:
                    if (IsIntType(nd->rtype) && Known(left->range) && Known(right->range)) {
                        range->type = ERangeComputed;
                        if (is_unsigned(nd->rtype)) {
                            range->upper = CInt64_MulU(left->range->upper, right->range->upper);
                            range->lower = CInt64_MulU(left->range->lower, right->range->lower);
                        } else {
                            range->upper = CInt64_Mul(left->range->upper, right->range->upper);
                            range->lower = CInt64_Mul(left->range->lower, right->range->lower);
                        }
                    }
                    break;
                case EDIV:
                case EDIVASS:
                    if (IsIntType(nd->rtype) &&
                        Known(left->range) &&
                        Known(right->range) &&
                        !CInt64_IsZero(&right->range->lower) &&
                        !CInt64_IsZero(&right->range->upper)) {
                        range->type = ERangeComputed;
                        if (is_unsigned(nd->rtype)) {
                            range->upper = CInt64_DivU(left->range->upper, right->range->lower);
                            range->lower = CInt64_DivU(left->range->lower, right->range->upper);
                        } else {
                            range->upper = CInt64_Div(left->range->upper, right->range->lower);
                            range->lower = CInt64_Div(left->range->lower, right->range->upper);
                        }
                    }
                    break;
                case EMODULO:
                case EMODASS:
                    if (IsIntType(nd->rtype) && Known(left->range) && Known(right->range)) {
                        uns = is_unsigned(nd->rtype);
                        a = left->range;
                        b = right->range;
                        if (CInt64_IsOne(&b->lower) && CInt64_IsOne(&b->upper)) {
                            range->lower = cint64_zero;
                            range->upper = cint64_zero;
                        }
                        if (!uns &&
                            CInt64_Equal(b->lower, cint64_negone) &&
                            CInt64_Equal(b->upper, cint64_negone)) {
                            range->lower = cint64_zero;
                            range->upper = cint64_zero;
                            break;
                        }
                        if (!uns && IsNegative(b->lower)) {
                            // |divisor| bounds the result
                            lower = b->lower;
                            upper = b->upper;
                            if (IsNegative(upper)) {
                                upper = CInt64_Neg(upper);
                            }
                            if (IsNegative(lower) && CInt64_Greater(CInt64_Neg(lower), upper)) {
                                upper = CInt64_Neg(lower);
                            }
                            range->upper = CInt64_Sub(upper, cint64_one);
                            range->lower = CInt64_Neg(range->upper);
                            break;
                        }
                        if (!uns && IsNegative(a->lower)) {
                            range->upper = CInt64_Sub(b->upper, cint64_one);
                            range->lower = CInt64_Neg(range->upper);
                            break;
                        }
                        range->lower = cint64_zero;
                        if (CInt64_Less(a->upper, b->upper)) {
                            range->upper = a->upper;
                        } else {
                            range->upper = CInt64_Sub(b->upper, cint64_one);
                        }
                    }
                    break;
                case EADDV:
                case EADD:
                case EADDASS:
                    if (IsIntType(nd->rtype) && Known(left->range) && Known(right->range)) {
                        range->type = ERangeComputed;
                        range->upper = CInt64_Add(left->range->upper, right->range->upper);
                        range->lower = CInt64_Add(left->range->lower, right->range->lower);
                    }
                    break;
                case ESUBV:
                case ESUB:
                case ESUBASS:
                    if (IsIntType(nd->rtype) && Known(left->range) && Known(right->range)) {
                        range->type = ERangeComputed;
                        range->upper = CInt64_Sub(left->range->upper, right->range->lower);
                        range->lower = CInt64_Sub(left->range->lower, right->range->upper);
                    }
                    break;
                case ESHL:
                case ESHLASS:
                    if (IsIntType(nd->rtype) &&
                        Known(left->range) &&
                        Known(right->range) &&
                        (is_unsigned(nd->rtype) ||
                         (!IsNegative(right->range->upper) && !IsNegative(right->range->lower)))) {
                        range->type = ERangeComputed;
                        range->upper = CInt64_Shl(left->range->upper, right->range->upper);
                        range->lower = CInt64_Shl(left->range->lower, right->range->lower);
                    }
                    break;
                case ESHR:
                case ESHRASS:
                    if (IsIntType(nd->rtype) &&
                        Known(left->range) &&
                        Known(right->range) &&
                        (is_unsigned(nd->rtype) ||
                         (!IsNegative(right->range->upper) && !IsNegative(right->range->lower)))) {
                        range->type = ERangeComputed;
                        if (is_unsigned(nd->rtype)) {
                            range->upper = CInt64_ShrU(left->range->upper, right->range->lower);
                            range->lower = CInt64_ShrU(left->range->lower, right->range->upper);
                        } else {
                            range->upper = CInt64_Shr(left->range->upper, right->range->lower);
                            range->lower = CInt64_Shr(left->range->lower, right->range->upper);
                        }
                    }
                    break;
                case ELESS:
                case EGREATER:
                case ELESSEQU:
                case EGREATEREQU:
                case EEQU:
                case ENOTEQU:
                    range->type = ERangeConstant;
                    if (func_00590200(nd, &value)) {
                        range->upper = value;
                        range->lower = value;
                    } else {
                        range->upper = cint64_one;
                        range->lower = cint64_zero;
                    }
                    break;
                case EAND:
                case EANDASS:
                    if (!IRO_IsIntConstant(right)) {
                        break;
                    }
                    value = right->u.node->data.intval;
                    range->type = ERangeBounded;
                    range->upper = value;
                    range->lower = cint64_zero;
                    IRO_TruncateValueToType(&range->upper, nd->rtype);
                    if (!is_unsigned(nd->rtype) && IsNegative(range->upper)) {
                        range->lower = func_0055A710(nd->rtype);
                        range->upper = CInt64_And(range->upper, func_0055A670(nd->rtype));
                    }
                    if (Known(left->range) &&
                        CInt64_LessEqualU(left->range->upper, value) &&
                        CInt64_LessEqualU(left->range->lower, value)) {
                        // the bits left's upper bound may have
                        work = left->range->upper;
                        bits = 0;
                        while (
                            CInt64_NotEqual((work = CInt64_ShrU(work, cint64_one)), cint64_zero)) {
                            bits++;
                        }
                        if (CInt64_NotEqual(left->range->upper, cint64_zero)) {
                            bits++;
                        }
                        CInt64_SetULong(&work, bits);
                        mask = CInt64_Sub(CInt64_Shl(cint64_one, work), cint64_one);
                        if (!CInt64_NotEqual(cint64_zero, CInt64_And(CInt64_Inv(value), mask)) &&
                            !IRO_HasSideEffect(left)) {
                            a = left->range;
                            IRO_Dump(
                                "eliminating redundant EAND %ld; upperBound==0x%lx, lowerBound==0x%lx, "
                                "constant==0x%lx\n",
                                nd->index,
                                CInt64_GetULong(&a->upper),
                                CInt64_GetULong(&a->lower),
                                CInt64_GetULong(&value)
                            );
                            IRO_WalkTree(right, NopOutWithRange);
                            nd->type = IROLinearNop;
                            nd->expr = NULL;
                            nd->range = NULL;
                            if (!IRO_LocateFather_Cut_And_Paste(nd, left)) {
                                left->flags &= ~IROLF_Reffed;
                                if (IRO_IsVariable(left)) {
                                    IRO_WalkTree(left, NopOutWithRange);
                                }
                            }
                            nd = left;
                            range = left->range;
                        }
                    }
                    break;
                case ELAND:
                case ELOR:
                    range->type = ERangeBounded;
                    range->upper = cint64_one;
                    range->lower = cint64_zero;
                    break;
                case EASS:
                    if (IsIntType(nd->rtype)) {
                        *range = *right->range;
                    }
                    break;
                case EMIN:
                    if (IsIntType(nd->rtype) && Known(left->range) && Known(right->range)) {
                        range->type = ERangeComputed;
                        if (is_unsigned(nd->rtype)) {
                            range->upper = CInt64_LessU(left->range->upper, right->range->upper)
                                ? left->range->upper
                                : right->range->upper;
                            range->lower = CInt64_LessU(left->range->lower, right->range->lower)
                                ? left->range->lower
                                : right->range->lower;
                        } else {
                            range->upper = CInt64_Less(left->range->upper, right->range->upper)
                                ? left->range->upper
                                : right->range->upper;
                            range->lower = CInt64_Less(left->range->lower, right->range->lower)
                                ? left->range->lower
                                : right->range->lower;
                        }
                    }
                    break;
                case EMAX:
                    if (IsIntType(nd->rtype) && Known(left->range) && Known(right->range)) {
                        range->type = ERangeComputed;
                        if (is_unsigned(nd->rtype)) {
                            range->upper = CInt64_GreaterU(left->range->upper, right->range->upper)
                                ? left->range->upper
                                : right->range->upper;
                            range->lower = CInt64_GreaterU(left->range->lower, right->range->lower)
                                ? left->range->lower
                                : right->range->lower;
                        } else {
                            range->upper = CInt64_Greater(left->range->upper, right->range->upper)
                                ? left->range->upper
                                : right->range->upper;
                            range->lower = CInt64_Greater(left->range->lower, right->range->lower)
                                ? left->range->lower
                                : right->range->lower;
                        }
                    }
                    break;
            }
            break;

        case IROLinearFunccall:
            InvalidateRangesForKillsByFunctionCall(nd);
            break;
    }

    if ((nd->type == IROLinearOp1Arg || nd->type == IROLinearOp2Arg) &&
        IRO_IsAssignOp[nd->nodetype] &&
        IsIntType(nd->rtype)) {
        left = nd->u.monadic;
        if ((obj = IRO_IsVariable(left))) {
            if (is_volatile_object(obj)) {
                range->type = ERangeUnknown;
            }
            func_00590C80(range, obj->type);
            bitfield =
                left->type == IROLinearOp1Arg && left->u.monadic->rtype->type == TYPEBITFIELD;
            if (bitfield) {
                range->type = ERangeUnknown;
            }
            RecordStore(obj, range, nd, bitfield, FALSE);
        } else {
            SetRangesForKillsByIndirectAssignment(nd);
        }
    }

    if (range) {
        func_00590C80(range, nd->rtype);
        if (CParser_IsVolatile(nd->rtype, nd->nodeflags & ENODE_FLAG_QUALS)) {
            range->type = ERangeUnknown;
        }
        if (range->type != ERangeUnknown &&
            CInt64_Equal(range->upper, range->lower) &&
            IsIntType(nd->rtype) &&
            !(nd->flags & IROLF_Assigned) &&
            !IRO_HasSideEffect(nd) &&
            !IRO_IsIntConstant(nd)) {
            IRO_Dump(
                "replacing LinearNode %ld with CInt64 0x%lx\n",
                nd->index,
                CInt64_GetULong(&range->upper)
            );
            IRO_WalkTree(nd, NopOutWithRange);
            constant = IRO_NewIntConst(range->upper, nd->rtype);
            IRO_PasteAfter(constant, constant, nd);
            if (!IRO_LocateFather_Cut_And_Paste(nd, constant)) {
                constant->flags &= ~IROLF_Reffed;
            }
        }
    }
}

// 0x58ED50
#if MWCCPS2
// b210 inlines a Boolean bit test (unsigned index)
static inline Boolean IsBitSet210(UInt32 bit, BitVector* bv) {
    return (bit >> 5) < bv->size && (bv->data[bit >> 5] & (1 << (bit & 31)));
}
#endif

void IRO_RangePropagateInFNodes(BitVector* nodes) {
    IRONode* fnode;
    IROLinear* nd;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
#if MWCCPS2
        if (nodes && !IsBitSet210(fnode->index, nodes)) {
#else
        if (nodes && !Bv_IsBitSet(fnode->index, nodes)) {
#endif
            continue;
        }
        data_00608CB0 = NULL;
        for (nd = fnode->first; nd && nd != fnode->last->next; nd = nd->next) {
            nd->range = NULL;
            switch (nd->type) {
                case IROLinearOperand:
                case IROLinearOp1Arg:
                case IROLinearOp2Arg:
                case IROLinearOp3Arg:
                case IROLinearScope:
                case IROLinearFunccall:
                    FoldLinear(nd);
                    break;
                case IROLinearAsm:
                    InvalidateAll();
                    break;
            }
        }
        data_00608CB0 = NULL;
    }
    IRO_CheckForUserBreak();
    for (nd = IRO_FirstLinear; nd; nd = nd->next) {
        nd->range = NULL;
    }
}
