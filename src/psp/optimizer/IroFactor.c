// mwccpsp 3.0.1: factoring common multipliers out of an integer + - chain
// (IroTransform.c by its asserts, 0x56BBB0..0x56D490). Not in the 2.4.7 source.
//
//     a*x + b*x - x*c     becomes  x * (a + b - c)
//
// The chain is split into addends (FactorItem: a copy of the addend and
// whether it is subtracted). Each multiplier of an addend gets a FactorTerm,
// paired with a term for its negation: `uses` lists the addends the multiplier
// (or its negation) divides, `other` those that only hold the negation. While
// some term divides two or more addends, the one dividing the most (and
// negated in the fewest) is factored out of them.
#include "compiler/common.h"
#include "compiler/CInt64.h"
#include "compiler/enode.h"
#include "compiler/IroDump.h"
#include "compiler/IroEval.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroMalloc.h"
#include "compiler/IroUtil.h"
#include "compiler/types.h"

extern void IRO_ResetList(IROList* list); // 0x552B80

// a list cell; an empty list is a cell with a NULL head
typedef struct FactorCons {
    void* head;
    struct FactorCons* tail;
} FactorCons;

typedef struct FactorItem {
    IROList list; // its expression is list.tail
    Boolean negate; // subtracted
    UInt8 pad[3]; // 3.0.1 allocates 0xC
} FactorItem;

typedef struct FactorTerm {
    IROList list; // its expression is list.tail
    struct FactorTerm* partner;
    FactorCons* uses;
    FactorCons* other;
} FactorTerm;

#define ITEM_EXPR(x) (((FactorItem*)(x))->list.tail)

static inline Boolean IsIntType(Type* type) {
    return type->type == TYPEINT && TYPE_INTEGRAL(type)->integral < IT_17;
}

static inline Boolean IsIntAddSub(IROLinear* nd) {
    return nd->type == IROLinearOp2Arg &&
        (nd->nodetype == EADD || nd->nodetype == ESUB) &&
        IsIntType(nd->rtype);
}

static inline Boolean IsIntMul(IROLinear* nd) {
    return nd->type == IROLinearOp2Arg && nd->nodetype == EMUL && IsIntType(nd->rtype);
}

// cheap to negate: -x, a - b or a constant
static inline Boolean IsCheapNeg(IROLinear* nd) {
    if (!IsIntType(nd->rtype)) {
        return 0;
    }
    if ((nd->type == IROLinearOp1Arg && nd->nodetype == EMONMIN) ||
        (nd->type == IROLinearOp2Arg && nd->nodetype == ESUB)) {
        return 1;
    }
    return IRO_IsIntConstant(nd);
}

static inline void Push(FactorCons* list, void* x) {
    FactorCons* cell;

    if (list->head) {
        cell = IRO_malloc(sizeof(FactorCons));
        cell->head = NULL;
        cell->tail = NULL;
        cell->head = list->head;
        cell->tail = list->tail;
        list->tail = cell;
    }
    list->head = x;
}

static inline FactorCons* NewCons(void) {
    FactorCons* cell = IRO_malloc(sizeof(FactorCons));
    cell->head = NULL;
    cell->tail = NULL;
    return cell;
}

// the cell whose element has x's expression
static inline void* FindItem(FactorCons* list, void* x) {
    FactorCons* c;
    void* y;

    for (c = list; c && (y = c->head); c = c->tail) {
        if (ITEM_EXPR(y) == ITEM_EXPR(x)) {
            return y;
        }
    }
    return NULL;
}

static IROLinear* NewDiadic(ENodeType op, IROLinear* left, IROLinear* right) {
    IROLinear* nd = IRO_NewLinear(IROLinearOp2Arg);
    nd->index = ++IRO_NumLinear;
    nd->nodetype = op;
    nd->u.diadic.left = left;
    nd->u.diadic.right = right;
    nd->rtype = IRO_UnqualifiedType(left->rtype);
    return nd;
}

static IROLinear* NewMonadic(ENodeType op, IROLinear* operand) {
    IROLinear* nd = IRO_NewLinear(IROLinearOp1Arg);
    nd->index = ++IRO_NumLinear;
    nd->nodetype = op;
    nd->u.monadic = operand;
    nd->rtype = IRO_UnqualifiedType(operand->rtype);
    return nd;
}

Boolean func_0056C090(FactorTerm* term, IROLinear* expr, IROList* list);
void func_0056C9E0(FactorCons* terms, FactorItem* item, IROLinear* expr);
void func_0056CAC0(FactorCons* terms, FactorItem* item, IROLinear* expr);
UInt8 func_0056CDF0(IROLinear* nd);
IROLinear* func_0056C410(IROLinear* nd, UInt8 kind, IROList* list);
void func_0056D130(FactorCons* terms, FactorTerm* term);
void func_0056D2A0(FactorCons* list, FactorItem* item);
void func_0056D390(FactorCons* list, FactorItem* item);

// 0x56C000: the term dividing the most addends (at least two), then the one
// negated in the fewest
FactorTerm* func_0056C000(FactorCons* terms) {
    FactorTerm* best = NULL;
    int bestUses = 2;
    int bestOther = 0x7FFFFFFF;
    FactorCons* c;
    FactorCons* p;
    FactorTerm* term;
    int uses;
    int other;

    for (c = terms; c && (term = c->head); c = c->tail) {
        uses = 0;
        for (p = term->uses; p && p->head; p = p->tail) {
            uses++;
        }
        if (uses < bestUses) {
            continue;
        }
        other = 0;
        for (p = term->other; p && p->head; p = p->tail) {
            other++;
        }
        if (uses > bestUses || other < bestOther) {
            bestUses = uses;
            bestOther = other;
            best = term;
        }
    }
    return best;
}

// 0x56C090: expr without one multiplier term, appended to list; FALSE (and a
// copy of expr) when expr is not a product
Boolean func_0056C090(FactorTerm* term, IROLinear* expr, IROList* list) {
    IROLinear* left;
    IROLinear* right;
    IROLinear* a;
    IROLinear* b;

    if (IsIntMul(expr)) {
        left = expr->u.diadic.left;
        right = expr->u.diadic.right;
        if (IRO_ExprsSame(term->list.tail, left)) {
            IRO_DuplicateExpr(right, list);
            return 1;
        }
        if (IRO_ExprsSame(term->list.tail, right)) {
            IRO_DuplicateExpr(left, list);
            return 1;
        }
        if (func_0056C090(term, left, list)) {
            a = list->tail;
            IRO_DuplicateExpr(right, list);
            b = list->tail;
        } else {
            IRO_DuplicateExpr(left, list);
            a = list->tail;
            func_0056C090(term, right, list);
            b = list->tail;
        }
        IRO_AddToList(NewDiadic(EMUL, a, b), list);
        return 1;
    }
    IRO_DuplicateExpr(expr, list);
    return 0;
}

// an addend, as written (negate FALSE) or negated, appended to list; TRUE
// when it still has to be subtracted
static inline Boolean AppendAddend(FactorItem* item, IROList* list) {
    UInt8 kind;

    if (!item->negate) {
        IRO_DuplicateExpr(item->list.tail, list);
        return 0;
    }
    if ((kind = func_0056CDF0(item->list.tail))) {
        func_0056C410(item->list.tail, kind, list);
        return 0;
    }
    IRO_DuplicateExpr(item->list.tail, list);
    return 1;
}

// 0x56C1E0: the sum of the addends, appended to list; it starts with one
// that is added, or else cheap to negate
IROLinear* func_0056C1E0(FactorCons* items, IROList* list) {
    FactorItem* first = NULL;
    FactorCons* c;
    FactorItem* item;
    IROLinear* sum;
    IROLinear* right;
    Boolean sub;

    for (c = items; c && (item = c->head); c = c->tail) {
        if (!item->negate) {
            first = item;
            break;
        }
    }
    if (!first) {
        for (c = items; c && (item = c->head); c = c->tail) {
            if (func_0056CDF0(item->list.tail)) {
                first = item;
                break;
            }
        }
    }
    if (!first) {
        first = items->head;
    }

    sub = AppendAddend(first, list);
    sum = list->tail;
    if (sub) {
        IRO_AddToList(NewMonadic(EMONMIN, sum), list);
        sum = list->tail;
    }
    for (c = items; c && (item = c->head); c = c->tail) {
        if (item == first) {
            continue;
        }
        sub = AppendAddend(item, list);
        right = list->tail;
        IRO_AddToList(NewDiadic(sub ? ESUB : EADD, sum, right), list);
        sum = list->tail;
    }
    return sum;
}

// 0x56C410: -nd, appended to list, the way func_0056CDF0 found
IROLinear* func_0056C410(IROLinear* nd, UInt8 kind, IROList* list) {
    IROLinear* a;
    IROLinear* b;
    CInt64 value;

    switch (kind) {
        case 0: // -(nd)
            a = IRO_DuplicateExpr(nd, list);
            IRO_AddToList(NewMonadic(EMONMIN, a), list);
            break;
        case 1: // -(-a) = a
            IRO_DuplicateExpr(nd->u.monadic, list);
            break;
        case 2: // -(a - b) = b - a
            a = IRO_DuplicateExpr(nd->u.diadic.right, list);
            b = IRO_DuplicateExpr(nd, list)->u.diadic.left;
            IRO_AddToList(NewDiadic(ESUB, a, b), list);
            break;
        case 3:
            value = CInt64_Neg(nd->u.node->data.intval);
            IRO_TruncateValueToType(&value, nd->rtype);
            IRO_AddToList(IRO_NewIntConst(value, nd->rtype), list);
            break;
        case 4: // -(a + b) = -a - b
            a = func_0056C410(nd->u.diadic.left, func_0056CDF0(nd->u.diadic.left), list);
            b = IRO_DuplicateExpr(nd->u.diadic.right, list);
            IRO_AddToList(NewDiadic(ESUB, a, b), list);
            break;
        case 5: // -(a + b) = -b - a
            a = func_0056C410(nd->u.diadic.right, func_0056CDF0(nd->u.diadic.right), list);
            b = IRO_DuplicateExpr(nd->u.diadic.left, list);
            IRO_AddToList(NewDiadic(ESUB, a, b), list);
            break;
        case 6: // -(a * b) = -a * b
            a = func_0056C410(nd->u.diadic.left, func_0056CDF0(nd->u.diadic.left), list);
            b = IRO_DuplicateExpr(nd->u.diadic.right, list);
            IRO_AddToList(NewDiadic(EMUL, a, b), list);
            break;
        case 7: // -(a * b) = a * -b
            kind = func_0056CDF0(nd->u.diadic.right);
            a = IRO_DuplicateExpr(nd->u.diadic.left, list);
            b = func_0056C410(nd->u.diadic.right, kind, list);
            IRO_AddToList(NewDiadic(EMUL, a, b), list);
            break;
    }
    return list->tail;
}

// 0x56D0E0
Boolean func_0056D0E0(IROLinear* nd) {
    return IsIntAddSub(nd);
}

// 0x56C970: one addend of the chain
void func_0056C970(FactorCons* items, FactorCons* terms, IROLinear* expr, Boolean negate) {
    FactorItem* item = IRO_malloc(sizeof(FactorItem));
    IRO_InitList(&item->list);
    IRO_DuplicateExpr(expr, &item->list);
    item->negate = negate;
    Push(items, item);
    func_0056CAC0(terms, item, expr);
}

static inline void SplitOperand(
    FactorCons* items, FactorCons* terms, IROLinear* nd, Boolean negate
);

// 0x56C6C0: split the chain at nd into addends
void func_0056C6C0(FactorCons* items, FactorCons* terms, IROLinear* nd, Boolean negate) {
    SplitOperand(items, terms, nd->u.diadic.left, negate);
    SplitOperand(items, terms, nd->u.diadic.right, (nd->nodetype == ESUB) ^ negate);
}

static inline void SplitOperand(
    FactorCons* items, FactorCons* terms, IROLinear* nd, Boolean negate
) {
    IROLinear* a;
    IROLinear* b;

    if (IsIntAddSub(nd)) {
        a = nd->u.diadic.left;
        b = nd->u.diadic.right;
        if (func_0056D0E0(a)) {
            func_0056C6C0(items, terms, a, negate);
        } else {
            func_0056C970(items, terms, a, negate);
        }
        negate = (nd->nodetype == ESUB) ^ negate;
        if (func_0056D0E0(b)) {
            func_0056C6C0(items, terms, b, negate);
        } else {
            func_0056C970(items, terms, b, negate);
        }
    } else {
        func_0056C970(items, terms, nd, negate);
    }
}

// 0x56C9E0: forget an addend in the terms of its multipliers; a term (and its
// partner) dividing nothing more goes
void func_0056C9E0(FactorCons* terms, FactorItem* item, IROLinear* expr) {
    FactorCons* c;
    FactorTerm* term;
    FactorTerm* partner;

    if (IsIntMul(expr)) {
        func_0056C9E0(terms, item, expr->u.diadic.left);
        func_0056C9E0(terms, item, expr->u.diadic.right);
        return;
    }
    for (c = terms; c && c->head; c = c->tail) {
        if (IRO_ExprsSame(((FactorTerm*)c->head)->list.tail, expr)) {
            break;
        }
    }
    term = c ? c->head : NULL;
    if (term) {
        func_0056D2A0(term->other, item);
        func_0056D2A0(term->uses, item);
        partner = term->partner;
        func_0056D2A0(partner->other, item);
        func_0056D2A0(partner->uses, item);
        if (!term->uses->head) {
            func_0056D130(terms, term);
            func_0056D130(terms, partner);
        }
    }
}

static inline FactorTerm* NewTerm(FactorCons* terms, IROLinear* expr) {
    FactorTerm* term = IRO_malloc(sizeof(FactorTerm));
    IRO_InitList(&term->list);
    IRO_DuplicateExpr(expr, &term->list);
    term->partner = NULL;
    term->uses = NewCons();
    term->other = NewCons();
    Push(terms, term);
    return term;
}

// 0x56CAC0: note each multiplier of an addend
void func_0056CAC0(FactorCons* terms, FactorItem* item, IROLinear* expr) {
    FactorCons* c;
    FactorTerm* term;
    FactorTerm* partner;
    IROList list;
    IROLinear* neg;
    UInt8 kind;

    if (IsIntMul(expr)) {
        func_0056CAC0(terms, item, expr->u.diadic.left);
        func_0056CAC0(terms, item, expr->u.diadic.right);
        return;
    }
    for (c = terms; c && c->head; c = c->tail) {
        if (IRO_ExprsSame(((FactorTerm*)c->head)->list.tail, expr)) {
            break;
        }
    }
    term = c ? c->head : NULL;
    if (!term) {
        term = NewTerm(terms, expr);
        kind = func_0056CDF0(expr);
        IRO_InitList(&list);
        neg = func_0056C410(expr, kind, &list);
        partner = NewTerm(terms, neg);
        IRO_ResetList(&list);
        term->partner = partner;
        partner->partner = term;
    }
    partner = term->partner;
    if (!FindItem(partner->uses, item)) {
        Push(partner->uses, item);
        if (!FindItem(partner->other, item)) {
            Push(partner->other, item);
        }
    }
    if (!FindItem(term->uses, item)) {
        Push(term->uses, item);
    }
    func_0056D2A0(term->other, item);
}

// 0x56CDF0: how func_0056C410 negates nd; 0 wraps it in a unary minus
UInt8 func_0056CDF0(IROLinear* nd) {
    if (!IsIntType(nd->rtype)) {
        return 0;
    }
    if (nd->type == IROLinearOp1Arg && nd->nodetype == EMONMIN) {
        return 1;
    }
    if (nd->type == IROLinearOp2Arg && nd->nodetype == ESUB) {
        return 2;
    }
    if (IRO_IsIntConstant(nd)) {
        return 3;
    }
    if (IsIntAddSub(nd)) {
        if (IsCheapNeg(nd->u.diadic.left)) {
            return 4;
        }
        if (IsCheapNeg(nd->u.diadic.right)) {
            return 5;
        }
    }
    if (IsIntMul(nd)) {
        if (IsCheapNeg(nd->u.diadic.left)) {
            return 6;
        }
        if (IsCheapNeg(nd->u.diadic.right)) {
            return 7;
        }
    }
    return 0;
}

// unlink cell c (prev before it) from its list
static inline void Unlink(FactorCons* c, FactorCons* prev) {
    FactorCons* next;

    if (prev) {
        prev->tail = c->tail;
        c->head = NULL;
        c->tail = NULL;
        IRO_free(c);
    } else if ((next = c->tail)) {
        c->head = next->head;
        c->tail = c->tail->tail;
        next->head = NULL;
        next->tail = NULL;
        IRO_free(next);
    } else {
        c->head = NULL;
    }
}

// 0x56D130: remove a term and free it
void func_0056D130(FactorCons* terms, FactorTerm* term) {
    FactorCons* prev = NULL;
    FactorCons* c;
    FactorTerm* x;

    for (c = terms; c && (x = c->head); prev = c, c = c->tail) {
        if (x->list.tail == term->list.tail) {
            IRO_ResetList(&x->list);
            while (x->uses && x->uses->head) {
                func_0056D2A0(x->uses, x->uses->head);
            }
            IRO_free(x->uses);
            while (x->other && x->other->head) {
                func_0056D2A0(x->other, x->other->head);
            }
            IRO_free(x->other);
            IRO_free(c->head);
            Unlink(c, prev);
            return;
        }
    }
}

// 0x56D2A0: remove an addend
void func_0056D2A0(FactorCons* list, FactorItem* item) {
    FactorCons* prev = NULL;
    FactorCons* c;

    for (c = list; c && c->head; prev = c, c = c->tail) {
        if (ITEM_EXPR(c->head) == item->list.tail) {
            Unlink(c, prev);
            return;
        }
    }
}

// 0x56D390: remove an addend and free it
void func_0056D390(FactorCons* list, FactorItem* item) {
    FactorCons* prev = NULL;
    FactorCons* c;

    for (c = list; c && c->head; prev = c, c = c->tail) {
        if (ITEM_EXPR(c->head) == item->list.tail) {
            IRO_ResetList(&((FactorItem*)c->head)->list);
            IRO_free(c->head);
            Unlink(c, prev);
            return;
        }
    }
}

// 0x56BCC0: factor term out of the addends it divides
void func_0056BCC0(FactorCons* items, FactorCons* terms, FactorTerm* term) {
    IROList list;
    FactorCons quotients = {NULL, NULL};
    FactorCons done = {NULL, NULL};
    FactorCons* c;
    FactorCons* p;
    FactorItem* item;
    FactorItem* x;
    FactorItem* q;
    FactorTerm* by;
    IROLinear* expr;
    IROLinear* sum;
    IROLinear* product;
    Boolean viaPartner;

    for (c = term->uses; c && (item = c->head); c = c->tail) {
        IRO_InitList(&list);
        for (p = term->other; p && (x = p->head); p = p->tail) {
            if (x->list.tail == item->list.tail) {
                break;
            }
        }
        if (!p) {
            x = NULL;
        }
        viaPartner = x != NULL;
        by = viaPartner ? term->partner : term;
        expr = item->list.tail;
        if (IRO_ExprsSame(by->list.tail, expr)) {
            IRO_AddToList(IRO_NewIntConst(cint64_one, expr->rtype), &list);
        } else {
            func_0056C090(by, expr, &list);
        }
        expr = list.tail;
        viaPartner = item->negate ^ viaPartner;
        q = IRO_malloc(sizeof(FactorItem));
        IRO_InitList(&q->list);
        IRO_DuplicateExpr(expr, &q->list);
        q->negate = viaPartner;
        Push(&quotients, q);
        Push(&done, item);
        IRO_ResetList(&list);
    }

    IRO_InitList(&list);
    sum = func_0056C1E0(&quotients, &list);
    while (quotients.head) {
        func_0056D390(&quotients, quotients.head);
    }
    IRO_DuplicateExpr(term->list.tail, &list);
    expr = list.tail;
    IRO_AddToList(NewDiadic(EMUL, expr, sum), &list);
    product = list.tail;

    for (c = &done; c && (item = c->head); c = c->tail) {
        func_0056C9E0(terms, item, item->list.tail);
        func_0056D390(items, item);
    }
    while (done.head) {
        func_0056D2A0(&done, done.head);
    }

    q = IRO_malloc(sizeof(FactorItem));
    IRO_InitList(&q->list);
    IRO_DuplicateExpr(product, &q->list);
    q->negate = 0;
    Push(items, q);
    func_0056CAC0(terms, q, product);
    IRO_ResetList(&list);
}

// 0x56BBB0: factor the + - chain at nd
void func_0056BBB0(IROLinear* nd) {
    FactorCons terms = {NULL, NULL};
    IROList list;
    FactorCons items = {NULL, NULL};
    FactorTerm* term;
    Boolean changed;

    IRO_InitList(&list);
    func_0056C6C0(&items, &terms, nd, 0);
    changed = 0;
    do {
        term = func_0056C000(&terms);
        if (term) {
            func_0056BCC0(&items, &terms, term);
            changed = 1;
        }
    } while (term);
    if (changed) {
        IRO_Dump("factoring expression %d\n", nd->index);
        IRO_NopOut(nd);
        func_0056C1E0(&items, &list);
        IRO_PasteAfter(list.head, list.tail, nd);
        IRO_LocateFather_Cut_And_Paste(nd, list.tail);
    }
    IRO_ResetList(&list);
    while (terms.head) {
        func_0056D130(&terms, terms.head);
    }
    while (items.head) {
        func_0056D390(&items, items.head);
    }
}
