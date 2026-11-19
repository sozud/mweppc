// IroSerializeAssignments.c: pulls assignments and calls out of expressions that are
// referenced inside a statement, so that each runs once, in order (mwccps2 b210 0x5A22F0,
// b198 0x5A1F80). From the PS2 b198 decompilation's IroSerializeAssignments.c, taken as a
// hint.
#include "compiler/CFunc.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"

#if MWCCPS2 && !MWCC_CORE
extern IROLinear* func_00551ED0(
    Object* temp, IROLinear* value, IROList* list
); // IroUtil.c: temp = value
extern IROLinear* func_00552790(Object* obj); // IroUtil.c: *&obj
// b210 0x5A22F0: an assignment referenced from inside its statement is done before the
// statement; what used it reads the variable (or, for x++/x--, the value saved before)
void ps2_serialize_assignment_5A22F0(IROLinear* expr, IROLinear* before) {
    IROList first;
    IROList duplicated;
    IROList trailing;
    Object* temporary;
    IROLinear* node;
    UInt8 type;

    IRO_InitList(&first);
    IRO_InitList(&duplicated);
    IRO_InitList(&trailing);
    type = expr->type;
    node = expr->u.monadic;
    if (type == IROLinearOp1Arg && (expr->nodetype == EPOSTINC || expr->nodetype == EPOSTDEC)) {
        temporary = create_temp_object(IRO_UnqualifiedType(node->rtype));
        IRO_FindVar(temporary, TRUE, TRUE);
        IRO_DuplicateExpr(node, &first);
        func_00551ED0(temporary, first.tail, &first);
        IRO_Paste(first.head, first.tail, before);
    }
    IRO_DuplicateExpr(expr, &duplicated);
    duplicated.tail->flags &= ~IROLF_Reffed;
    IRO_Paste(duplicated.head, duplicated.tail, before);
    if (expr->type == IROLinearOp1Arg &&
        (expr->nodetype == EPOSTINC || expr->nodetype == EPOSTDEC)) {
        node = func_00552790(temporary);
        node->flags |= IROLF_Reffed;
        IRO_PasteAfter(node->u.monadic, node, expr);
        IRO_LocateFather_Cut_And_Paste(expr, node);
    } else {
        IRO_DuplicateExpr(node, &trailing);
        IRO_PasteAfter(trailing.head, trailing.tail, expr);
        IRO_LocateFather_Cut_And_Paste(expr, trailing.tail);
    }
}

// b210 0x5A2410: one statement's nodes (start..end); if a referenced node assigns, every
// referenced assignment is moved out and every referenced call goes through a temporary
void ps2_serialize_statement_5A2410(IROLinear* start, IROLinear* end) {
    Boolean serialize = FALSE;
    IROLinear* node;
    IROList list;
    Type* type;
    Object* temporary;
    IROLinear* reference;

    for (node = start; node != end->next; node = node->next) {
        if ((node->flags & IROLF_Reffed) && IRO_IsAssignment(node)) {
            serialize = TRUE;
            break;
        }
    }
    if (serialize) {
        for (node = start; node != end->next; node = node->next) {
            if (node->flags & IROLF_Reffed) {
                if (!IRO_IsAssignment(node)) {
                    if (node->type == IROLinearFunccall) {
                        IRO_InitList(&list);
                        IRO_DuplicateExpr(node, &list);
                        type = IRO_UnqualifiedType(node->rtype);
                        temporary = create_temp_object(type);
                        IRO_FindVar(temporary, TRUE, TRUE);
                        func_00551ED0(temporary, list.tail, &list);
                        IRO_Paste(list.head, list.tail, start);
                        reference = func_00552790(temporary);
                        reference->flags |= IROLF_Reffed;
                        IRO_PasteAfter(reference->u.monadic, reference, node);
                        IRO_LocateFather_Cut_And_Paste(node, reference);
                    }
                } else {
                    ps2_serialize_assignment_5A22F0(node, start);
                }
            }
        }
    }
}

// b210 0x5A2520: every statement of every block
void IRO_SerializeAllAssignments(void) {
    IRONode* fnode;
    IROLinear* start;
    IROLinear* node;
    IROLinear* first;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        first = fnode->first;
        start = first;
        node = first;
        if (first != fnode->last->next) {
            do {
                if (!(node->flags & IROLF_Reffed)) {
                    ps2_serialize_statement_5A2410(start, node);
                    start = node->next;
                }
                node = node->next;
            } while (node != fnode->last->next);
        }
    }
    IRO_UpdateFlagsOnInts(IRO_FirstLinear);
}
#endif
