// mwccpsp 3.0.1: the statement-tree transformations run before linearizing
// (IRO_TransformTree, 0x56E0A0..0x56F320; IroTransform.c). Over 2.4.7:
// - under copts.opt_movepostops (set by the PSP target, 0x439EA0) an
//   expression statement's x++ / x-- on a plain non-volatile variable is
//   moved out into a new expression statement right after it, and the use
//   in place becomes a copy of the load of x (0x56E1F0). A statement that is
//   just `i++` thus becomes `i;` followed by `i++;`.
// - integer-constant tests go through func_00555B30 (an EINTCONST of an
//   integral type) and integer types must be integral (below IT_17);
// - CombineConstants is gated by copts.opt_addtogetherconstantpartsoftree-
//   expressions, skips sums with a volatile part, builds its constants with
//   IRO_NewIntConstENode, keeps pointer types on the sums, copies the old
//   root's flags and frees its addend list.
// It also builds an (unused) ENode pattern into data_00608AC8 first.
#include "compiler/common.h"
#include "compiler/CExpr.h"
#include "compiler/CFunc.h"
#include "compiler/CInt64.h"
#include "compiler/CMachine.h"
#include "compiler/CParser.h"
#include "compiler/enode.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroMalloc.h"
#include "compiler/IroTransform.h"
#include "compiler/IroUtil.h"
#include "compiler/objects.h"
#include "compiler/types.h"

extern Boolean is_volatile_object(Object* obj);
extern Boolean func_00555B30(ENode* expr); // an integer constant of an integral type

ENode* data_00608AC8; // the pattern built on entry; nothing reads it
UInt8 data_00608AE8; // ExprType
ENode* data_00608AEC; // IndirectRef
Boolean data_00608AF0; // FirstTime
CInt64 data_00608AF4; // OperandConst
Object* data_00608AFC; // OperandObject
#if MWCCPS2 && MWCCPSP_NATIVE
// IroTransform.c defines them under their names (one b210 address each):
// natively one storage, not two
#define data_00608B00 LastAddend
#define data_00608B04 FirstAddend
extern ENodeList* LastAddend;
extern ENodeList* FirstAddend;
#else
ENodeList* data_00608B00; // LastAddend
ENodeList* data_00608B04; // FirstAddend
#endif

static inline Boolean IsIntType(Type* type) {
    return type->type == TYPEINT && TYPE_INTEGRAL(type)->integral < IT_17;
}

// monadic in 3.0.1's numbering: EPOSTINC..EFORCELOAD, ETYPCON, EBITFIELD
static inline Boolean IsMonadic(UInt8 t) {
    return t <= EFORCELOAD || t == ETYPCON || t == EBITFIELD;
}

// diadic: EMUL..EBSET, EPOINTERSTAR, EDOTSTAR
static inline Boolean IsDiadic(UInt8 t) {
    return (t >= EMUL && t <= EBSET) || t == EPOINTERSTAR || t == EDOTSTAR;
}

// 0x56E1F0: move post-increments/decrements of plain variables out of the
// expression of `stmt` into new statements after it
void func_0056E1F0(Statement* stmt, ENode** pexpr) {
    ENode* expr = *pexpr;
    UInt8 t = expr->type;
    ENode* ind;
    ENode* ref;
    ENode* copy;
    Statement* newstmt;

    if (IsMonadic(t)) {
        if (t != EFORCELOAD) {
            func_0056E1F0(stmt, &expr->data.monadic);
        }
        expr = *pexpr;
        if (expr->type > EPOSTDEC) {
            return;
        }
        ind = expr->data.monadic;
        if (ind->type != EINDIRECT || !ind->rtype) {
            return;
        }
        if (CParser_IsVolatile(ind->rtype, ind->flags & 3)) {
            return;
        }
        ref = ind->data.monadic;
        if (ref->type != EOBJREF || is_volatile_object(ref->data.objref)) {
            return;
        }

        newstmt = IRO_NewStatement(ST_EXPRESSION, *pexpr);
        IRO_CopySourceInfo(newstmt, stmt);
        newstmt->next = stmt->next;
        stmt->next = newstmt;

        copy = IRO_NewENode(EINDIRECT);
        *copy = *ind;
        copy->data.monadic = IRO_NewENode(EOBJREF);
        *copy->data.monadic = *ind->data.monadic;
        *pexpr = copy;
    } else if (IsDiadic(t)) {
        if (t != ECOND && t != ECOMMA && t != ELOR && t != ELAND && t != ENULLCHECK) {
            func_0056E1F0(stmt, &expr->data.diadic.left);
            func_0056E1F0(stmt, &(*pexpr)->data.diadic.right);
        }
    }
}

// 0x56E480: RemoveRedundantBitOperations
Boolean func_0056E480(ENode* expr) {
    Boolean a;
    Boolean b;

    if (expr->type == data_00608AE8) {
        a = func_0056E480(expr->data.diadic.left);
        b = func_0056E480(expr->data.diadic.right);
        return a && b;
    }

    if (expr->type == EINDIRECT) {
        if (expr->data.monadic->type == EOBJREF) {
            if (!data_00608AFC) {
                data_00608AFC = expr->data.monadic->data.objref;
                data_00608AEC = expr;
                return TRUE;
            }
            return expr->data.monadic->data.objref == data_00608AFC;
        }
        return FALSE;
    }

    if (func_00555B30(expr)) {
        if (data_00608AF0) {
            data_00608AF4 = expr->data.intval;
            data_00608AF0 = FALSE;
        } else if (data_00608AE8 == EAND) {
            data_00608AF4 = CInt64_And(expr->data.intval, data_00608AF4);
        } else if (data_00608AE8 == EOR) {
            data_00608AF4 = CInt64_Or(expr->data.intval, data_00608AF4);
        } else if (data_00608AE8 == EXOR) {
            data_00608AF4 = CInt64_Xor(expr->data.intval, data_00608AF4);
        }
        return TRUE;
    }

    return FALSE;
}

// 0x56E370: TransformExprTree2 (x & c1 & c2 chains over one variable)
void func_0056E370(ENode* expr) {
    ENodeList* list;
    UInt8 t = expr->type;

    if (IsMonadic(t)) {
        func_0056E370(expr->data.monadic);
    } else if (IsDiadic(t)) {
        func_0056E370(expr->data.diadic.left);
        func_0056E370(expr->data.diadic.right);
    } else if (t == ECOND) {
        func_0056E370(expr->data.cond.cond);
        func_0056E370(expr->data.cond.expr1);
        func_0056E370(expr->data.cond.expr2);
    } else if (t == EFUNCCALL || t == EFUNCCALLP) {
        func_0056E370(expr->data.funccall.funcref);
        for (list = expr->data.funccall.args; list; list = list->next) {
            func_0056E370(list->node);
        }
    } else if (t == ENULLCHECK) {
        func_0056E370(expr->data.nullcheck.nullcheckexpr);
        func_0056E370(expr->data.nullcheck.condexpr);
    }

    t = expr->type;
    if ((t == EAND || t == EXOR || t == EOR) &&
        (t == expr->data.diadic.left->type || t == expr->data.diadic.right->type)) {
        data_00608AFC = NULL;
        data_00608AE8 = expr->type;
        data_00608AF0 = TRUE;
        data_00608AEC = NULL;
        if (func_0056E480(expr)) {
            expr->data.diadic.left = data_00608AEC;
            expr->data.diadic.right->type = EINTCONST;
            expr->data.diadic.right->data.intval = data_00608AF4;
        }
    }
}

// 0x56E690: FoldConstantsinAssociativeExprs
void func_0056E690(ENode* expr) {
    UInt8 nodetype1;
    UInt8 nodetype2;
    UInt8 nodetype3;
    Boolean changed;
    short op;
    CInt64 val1;
    CInt64 val2;
    CInt64 typeBits;
    CInt64 sum;

    if (!(expr->type == EADD ||
          expr->type == EMUL ||
          expr->type == EAND ||
          expr->type == EOR ||
          expr->type == EXOR ||
          expr->type == ESHL ||
          expr->type == ESHR)) {
        return;
    }
    if (!IsIntType(expr->rtype) || !func_00555B30(expr->data.diadic.right)) {
        return;
    }

    do {
        changed = FALSE;

        if (expr->data.diadic.left->type == expr->type &&
            func_00555B30(expr->data.diadic.left->data.diadic.right)) {
            val1 = expr->data.diadic.right->data.intval;
            val2 = expr->data.diadic.left->data.diadic.right->data.intval;
            switch (expr->type) {
                case EADD:
                case ESHL:
                    op = '+';
                    break;
                case ESHR:
                    op = '+';
                    if (!is_unsigned(expr->rtype)) {
                        CInt64_SetLong(&typeBits, expr->rtype->size * 8);
                        if (CInt64_GreaterEqualU(val1, typeBits) ||
                            CInt64_GreaterEqualU(val2, typeBits)) {
                            return;
                        }
                        sum = CMach_CalcIntDiadic(expr->rtype, val1, '+', val2);
                        if (CInt64_GreaterEqualU(sum, typeBits)) {
                            val1 = CInt64_Sub(typeBits, cint64_one);
                            val2 = cint64_zero;
                        }
                    }
                    break;
                case EMUL:
                    op = '*';
                    break;
                case EAND:
                    op = '&';
                    break;
                case EOR:
                    op = '|';
                    break;
                case EXOR:
                    op = '^';
                    break;
                default:
                    return;
            }

            expr->data.diadic.right->data.intval = CMach_CalcIntDiadic(expr->rtype, val1, op, val2);
            expr->data.diadic.left = expr->data.diadic.left->data.diadic.left;
            changed = TRUE;
        } else if (((nodetype1 = expr->type) == EAND || nodetype1 == EOR) &&
                   ((nodetype2 = expr->data.diadic.left->type) == EAND || nodetype2 == EOR) &&
                   ((nodetype3 = expr->data.diadic.left->data.diadic.left->type) == EAND ||
                    nodetype3 == EOR) &&
                   func_00555B30(expr->data.diadic.left->data.diadic.left->data.diadic.right)) {
            if (CInt64_Equal(
                    expr->data.diadic.right->data.intval,
                    expr->data.diadic.left->data.diadic.left->data.diadic.right->data.intval
                )) {
                if (nodetype1 == nodetype3) {
                    expr->data.diadic.left->data.diadic.left =
                        expr->data.diadic.left->data.diadic.left->data.diadic.left;
                    changed = TRUE;
                } else if (nodetype2 == nodetype3) {
                    *expr = *expr->data.diadic.right;
                } else {
                    expr->data.diadic.left = expr->data.diadic.left->data.diadic.right;
                    changed = TRUE;
                }
            }
        }
    } while (changed);
}

// 0x56E5E0: TransformExprTree1 (fold constants bottom-up)
void func_0056E5E0(ENode* expr) {
    ENodeList* list;
    UInt8 t = expr->type;

    if (IsMonadic(t)) {
        func_0056E5E0(expr->data.monadic);
    } else if (IsDiadic(t)) {
        func_0056E5E0(expr->data.diadic.left);
        func_0056E5E0(expr->data.diadic.right);
    } else if (t == ECOND) {
        func_0056E5E0(expr->data.cond.cond);
        func_0056E5E0(expr->data.cond.expr1);
        func_0056E5E0(expr->data.cond.expr2);
    } else if (t == EFUNCCALL || t == EFUNCCALLP) {
        func_0056E5E0(expr->data.funccall.funcref);
        for (list = expr->data.funccall.args; list; list = list->next) {
            func_0056E5E0(list->node);
        }
    } else if (t == ENULLCHECK) {
        func_0056E5E0(expr->data.nullcheck.nullcheckexpr);
        func_0056E5E0(expr->data.nullcheck.condexpr);
    }

    func_0056E690(expr);
}

// 0x56EEC0: an EADD tree with a volatile-typed sum in it
Boolean func_0056EEC0(ENode* expr) {
    if (expr->type == EADD &&
        ((expr->rtype && CParser_IsVolatile(expr->rtype, expr->flags & 3)) ||
         func_0056EEC0(expr->data.diadic.left) ||
         func_0056EEC0(expr->data.diadic.right))) {
        return TRUE;
    }
    return FALSE;
}

#if MWCCPS2
// b210 has no AppendAddend: every use in 0x56F0F0 (and its inlined
// recursion) is expanded in place; a macro keeps CW from leaving calls
#define AppendAddend(expr) \
    do { \
        ENodeList* list_ = IRO_malloc(sizeof(ENodeList)); \
        list_->node = (expr); \
        list_->next = NULL; \
        if (data_00608B04) { \
            data_00608B00->next = list_; \
        } else { \
            data_00608B04 = list_; \
        } \
        data_00608B00 = list_; \
    } while (0)
#else
static void AppendAddend(ENode* expr) {
    ENodeList* list = IRO_malloc(sizeof(ENodeList));

    list->node = expr;
    list->next = NULL;
    if (data_00608B04) {
        data_00608B00->next = list;
    } else {
        data_00608B04 = list;
    }
    data_00608B00 = list;
}
#endif

// 0x56F0F0: AddAddend, the leaves of an EADD tree onto the addend list
void func_0056F0F0(ENode* expr) {
    if (expr->type == EADD) {
        func_0056F0F0(expr->data.diadic.left);
        func_0056F0F0(expr->data.diadic.right);
    } else {
        AppendAddend(expr);
    }
}

static inline Type* SumType(ENode* left, Type* other) {
    return left->rtype->type == TYPEPOINTER ? left->rtype : other;
}

// 0x56EB50: CombineConstants (an address sum: variable first, constant parts
// added together at the end)
ENode* func_0056EB50(ENode* expr) {
    ENode* addend;
    ENodeList* el;
    ENodeList* next;
    ENode* result;
    ENode* var;
    Type* type;
    ENode* tmp;
    ENodeList* prev;
    CInt64 val;

    if (func_0056EEC0(expr)) {
        return expr;
    }

    data_00608B04 = data_00608B00 = NULL;
    func_0056F0F0(expr->data.diadic.left);
    func_0056F0F0(expr->data.diadic.right);

    prev = NULL;
    var = NULL;
    for (el = data_00608B04; el; el = el->next) {
        addend = el->node;
        if (addend->type == EOBJREF) {
            var = addend;
            if (prev) {
                prev->next = el->next;
            } else {
                data_00608B04 = el->next;
            }
            break;
        }
        prev = el;
    }

    if (!var) {
        prev = NULL;
        for (el = data_00608B04; el; el = el->next) {
            addend = el->node;
            if (addend->type == EINDIRECT) {
                var = addend;
                if (prev) {
                    prev->next = el->next;
                } else {
                    data_00608B04 = el->next;
                }
                break;
            }
            prev = el;
        }
    }

    CInt64_SetLong(&val, 0);
    prev = NULL;
    type = NULL;

    for (el = data_00608B04; el; el = el->next) {
        addend = el->node;
        if (func_00555B30(addend)) {
            if (!type || type->size < addend->rtype->size) {
                type = addend->rtype;
            }
            val = CInt64_Add(val, addend->data.intval);
            if (prev) {
                prev->next = el->next;
            } else {
                data_00608B04 = el->next;
            }
        } else if (addend->type == EMUL && func_00555B30(addend->data.diadic.right)) {
            if (!type || type->size < addend->rtype->size) {
                type = addend->rtype;
            }
            tmp = addend->data.diadic.left;
            if (tmp->type == EADD && func_00555B30(tmp->data.diadic.right)) {
                val = CInt64_Add(
                    val,
                    CInt64_MulU(
                        tmp->data.diadic.right->data.intval, addend->data.diadic.right->data.intval
                    )
                );
                addend->data.diadic.left = tmp->data.diadic.left;
            }
            prev = el;
        } else {
            prev = el;
        }
    }

    result = NULL;
    if (var) {
        result = var;
        if (!CInt64_IsZero(&val)) {
            result = IRO_NewENode(EADD);
            result->data.diadic.left = var;
            result->data.diadic.right = IRO_NewIntConstENode(val, type);
            result->rtype = SumType(var, type);
            result->cost = 1;
            CInt64_SetLong(&val, 0);
        }
    }

    for (el = data_00608B04; el; el = el->next) {
        addend = el->node;
        if (result) {
            tmp = IRO_NewENode(EADD);
            tmp->data.diadic.left = result;
            tmp->data.diadic.right = addend;
            tmp->cost = result->cost + 1;
            tmp->rtype = SumType(result, addend->rtype);
            result = tmp;
        } else {
            result = addend;
        }
    }

    if (!CInt64_IsZero(&val)) {
        tmp = IRO_NewENode(EADD);
        tmp->data.diadic.left = result;
        tmp->data.diadic.right = IRO_NewIntConstENode(val, type);
        tmp->cost = result->cost + 1;
        tmp->rtype = SumType(result, type);
        result = tmp;
    }

    result->flags = expr->flags;

    for (el = data_00608B04; el; el = next) {
        next = el->next;
        IRO_free(el);
    }

    return result;
}

// 0x56EA70: TransformExprNode
ENode* func_0056EA70(ENode* expr) {
    ENode* left;

    switch (expr->type) {
        case EINDIRECT:
            if (copts.opt_addtogetherconstantpartsoftreeexpressions &&
                expr->data.monadic->type == EADD) {
                expr->data.monadic = func_0056EB50(expr->data.monadic);
            }
            break;

        case EMUL:
        case EADD:
        case EAND:
        case EXOR:
        case EOR:
            if (IsIntType(expr->rtype) &&
                !func_00555B30(expr->data.diadic.right) &&
                func_00555B30(expr->data.diadic.left)) {
                left = expr->data.diadic.left;
                expr->data.diadic.left = expr->data.diadic.right;
                expr->data.diadic.right = left;
            }
            break;

        case EEQU:
        case ENOTEQU:
            if (IsIntType(expr->rtype) &&
                !func_00555B30(expr->data.diadic.right) &&
                func_00555B30(expr->data.diadic.left)) {
                left = expr->data.diadic.left;
                expr->data.diadic.left = expr->data.diadic.right;
                expr->data.diadic.right = left;
            }
            if (func_00555B30(expr->data.diadic.right) && expr->data.diadic.left->type == EBINNOT) {
                left = expr->data.diadic.left;
                expr->data.diadic.left = left->data.monadic;
                left->data.monadic = expr->data.diadic.right;
                expr->data.diadic.right = left;
            }
            break;

        default:
            break;
    }

    return expr;
}

// 0x56E9A0: TransformExprTree
ENode* func_0056E9A0(ENode* expr) {
    ENodeList* list;
    UInt8 t = expr->type;

    if (IsMonadic(t)) {
        expr->data.monadic = func_0056E9A0(expr->data.monadic);
    } else if (IsDiadic(t)) {
        expr->data.diadic.left = func_0056E9A0(expr->data.diadic.left);
        expr->data.diadic.right = func_0056E9A0(expr->data.diadic.right);
    } else if (t == ECOND) {
        func_0056E9A0(expr->data.cond.cond);
        func_0056E9A0(expr->data.cond.expr1);
        func_0056E9A0(expr->data.cond.expr2);
    } else if (t == EFUNCCALL || t == EFUNCCALLP) {
        func_0056E9A0(expr->data.funccall.funcref);
        for (list = expr->data.funccall.args; list; list = list->next) {
            func_0056E9A0(list->node);
        }
    } else if (t == ENULLCHECK) {
        func_0056E9A0(expr->data.nullcheck.nullcheckexpr);
        func_0056E9A0(expr->data.nullcheck.condexpr);
    }

    return func_0056EA70(expr);
}

// 0x56E0A0
void IRO_TransformTree(Statement* statements) {
    Statement* stmt;
    Statement* next;

    // (x < 0u) || (x >= 1u) over a variable, built and never used
    data_00608AC8 = CExpr_NewENode(ELOR);
    data_00608AC8->data.diadic.left = CExpr_NewENode(ELESS);
    data_00608AC8->data.diadic.left->data.diadic.left = CExpr_NewENode(EINDIRECT);
    data_00608AC8->data.diadic.left->data.diadic.left->data.monadic = CExpr_NewENode(EOBJREF);
    data_00608AC8->data.diadic.left->data.diadic.left->data.monadic->data.objref = NULL;
    data_00608AC8->data.diadic.left->data.diadic.right = intconstnode(TYPE(&stunsignedlong), 0);
    data_00608AC8->data.diadic.right = CExpr_NewENode(EGREATEREQU);
    data_00608AC8->data.diadic.right->data.diadic.left = CExpr_NewENode(EINDIRECT);
    data_00608AC8->data.diadic.right->data.diadic.left->data.monadic = CExpr_NewENode(EOBJREF);
    data_00608AC8->data.diadic.right->data.diadic.right = intconstnode(TYPE(&stunsignedlong), 1);

    for (stmt = statements; stmt; stmt = next) {
        next = stmt->next; // read first: statements split off below are not visited
        switch (stmt->type) {
            case ST_EXPRESSION:
            case ST_SWITCH:
            case ST_IFGOTO:
            case ST_IFNGOTO:
            case ST_RETURN:
                if (stmt->expr) {
                    if (copts.opt_movepostops && stmt->type == ST_EXPRESSION) {
                        func_0056E1F0(stmt, &stmt->expr);
                    }
                    stmt->expr = func_0056E9A0(stmt->expr);
                    func_0056E370(stmt->expr);
                    func_0056E5E0(stmt->expr);
                }
                break;
            default:
                break;
        }
    }

    IRO_CheckForUserBreak();
}
