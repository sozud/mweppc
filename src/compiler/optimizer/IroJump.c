#include "compiler/IroJump.h"
#include "compiler/IroDump.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroUtil.h"
#include "compiler/CFunc.h"
#include "compiler/Exceptions.h"
#include "compiler/Switch.h"

#if MWCCPSP
// 3.0.1 (0x58D6E0): CheckChain; a goto that ends a loop (IROLF_EndLoop) is
// not followed
Boolean func_0058D6E0(CLabel** labelptr) {
    IRONode* node;
    IROLinear* linear;
    CLabel* lab;
    CLabel* label;

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->first &&
            node->first->type == IROLinearLabel &&
            node->first->u.label.label == *labelptr) {
            label = *labelptr;
            for (linear = node->first->next, lab = NULL;
                 linear && (linear->type == IROLinearLabel || linear->type == IROLinearNop);
                 linear = linear->next) {
                if (linear->type == IROLinearLabel) {
                    lab = linear->u.label.label;
                }
            }

            if (linear->type == IROLinearGoto &&
                !(linear->flags & IROLF_EndLoop) &&
                label != linear->u.label.label) {
                *labelptr = linear->u.label.label;
                IRO_Dump("Chaining goto at %d\n", linear->index);
                return TRUE;
            }

            if (lab && label != lab) {
                *labelptr = lab;
            }
            return FALSE;
        }
    }

    return FALSE;
}

// 3.0.1 (0x58D620): as 2.4.7, but a loop-ending goto is not rechained
Boolean IRO_DoJumpChaining(void) {
    IRONode* node;
    IROLinear* linear;
    Boolean changed;
    SwitchInfo* info;
    SwitchCase* curcase;
    Boolean result;

    result = FALSE;
    do {
        changed = FALSE;
        for (node = IRO_FirstNode; node; node = node->nextnode) {
            if (node->first) {
                linear = node->last;
                switch (linear->type) {
                    case IROLinearGoto:
                        if (!(linear->flags & IROLF_EndLoop)) {
                            if (func_0058D6E0(&linear->u.label.label)) {
                                changed = TRUE;
                            }
                        }
                        break;
                    case IROLinearIf:
                    case IROLinearIfNot:
                        if (func_0058D6E0(&linear->u.label.label)) {
                            changed = TRUE;
                        }
                        break;
                    case IROLinearSwitch:
                        info = linear->u.swtch.info;
                        for (curcase = info->cases; curcase; curcase = curcase->next) {
                            if (func_0058D6E0(&curcase->label)) {
                                changed = TRUE;
                            }
                        }
                        if (func_0058D6E0(&info->defaultlabel)) {
                            changed = TRUE;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
        result |= changed;
        IRO_CheckForUserBreak();
    } while (changed);

    // 3.0.1 recomputes the graph once anything was chained
    if (result) {
        IRO_ComputeSuccPred();
        IRO_ComputeDom();
    }

    return result;
}
#else
static Boolean CheckChain(CLabel** labelptr) {
    IRONode* node;

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->first &&
            node->first->type == IROLinearLabel &&
            node->first->u.label.label == *labelptr) {
            IROLinear* linear;
            CLabel* lab;
            for (linear = node->first->next, lab = NULL;
                 linear && (linear->type == IROLinearLabel || linear->type == IROLinearNop);
                 linear = linear->next) {
                if (linear->type == IROLinearLabel) {
                    lab = linear->u.label.label;
                }
            }

            if (linear->type == IROLinearGoto && *labelptr != linear->u.label.label) {
                *labelptr = linear->u.label.label;
                IRO_Dump("Chaining goto at %d\n", linear->index);
                return TRUE;
            }

            if (lab && *labelptr != lab) {
                *labelptr = lab;
            }
            return FALSE;
        }
    }

    return FALSE;
}

Boolean IRO_DoJumpChaining(void) {
    IRONode* node;
    IROLinear* linear;
    Boolean changed;
    SwitchInfo* info;
    SwitchCase* curcase;
    Boolean result;

    result = FALSE;
    do {
        changed = FALSE;
        for (node = IRO_FirstNode; node; node = node->nextnode) {
            if (node->first) {
                linear = node->last;
                switch (linear->type) {
                    case IROLinearGoto:
                        if (CheckChain(&linear->u.label.label)) {
                            changed = TRUE;
                        }
                        break;
                    case IROLinearIf:
                    case IROLinearIfNot:
                        if (CheckChain(&linear->u.label.label)) {
                            changed = TRUE;
                        }
                        break;
                    case IROLinearSwitch:
                        info = linear->u.swtch.info;
                        for (curcase = info->cases; curcase; curcase = curcase->next) {
                            if (CheckChain(&curcase->label)) {
                                changed = TRUE;
                            }
                        }
                        if (CheckChain(&info->defaultlabel)) {
                            changed = TRUE;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
        result |= changed;
        IRO_CheckForUserBreak();
    } while (changed);

    return result;
}

#endif
#if MWCCPSP

#endif
void IRO_MakeReachable(IRONode* node) {
    UInt16 i;
    Boolean changed;
    IRONode* scan;

    node->reachable = TRUE;
    do {
        changed = FALSE;
        for (scan = IRO_FirstNode; scan; scan = scan->nextnode) {
            if (scan->reachable && !scan->visited) {
                for (i = 0; i < scan->numsucc; i++) {
                    if (!IRO_NodeTable[scan->succ[i]]->reachable) {
                        changed = TRUE;
                        IRO_NodeTable[scan->succ[i]]->reachable = TRUE;
                    }
                }
                scan->visited = TRUE;
            }
        }
    } while (changed);
}

#if MWCCPSP
extern void IRO_FindAsmBarriers(void); // IroFlowgraph.c (0x576550)

// 3.0.1 (0x5763B0): reachability comes from IRO_FindAsmBarriers, and a node
// holding an asm barrier is never removed
Boolean IRO_RemoveUnreachable(void) {
    IRONode* node;
    IRONode* prevNode;
    IROLinear* scan;
    IROLinear* linear;
    Boolean result;
    ExceptionAction** actptr;
    ExceptionAction* act;

    result = FALSE;
    IRO_ComputeSuccPred();
    IRO_FindAsmBarriers();
    prevNode = IRO_FirstNode;
    for (node = IRO_FirstNode->nextnode; node; node = node->nextnode) {
        if (node->first && !node->reachable && !node->hasAsmBarrier) {
            IRO_Dump("Removing unreachable code at: %d\n", node->index);
            prevNode->nextnode = node->nextnode;
            prevNode->last->next = node->last->next;
            result = TRUE;
            for (linear = node->first;
                 linear && linear->type == IROLinearLabel && linear != node->last->next;
                 linear = linear->next) {
                for (scan = IRO_FirstLinear; scan; scan = scan->next) {
                    if (scan->stmt) {
                        scan->stmt->marked = FALSE;
                    }
                }

                for (scan = IRO_FirstLinear; scan; scan = scan->next) {
                    if (scan->stmt && !scan->stmt->marked) {
                        scan->stmt->marked = TRUE;
                        for (actptr = &scan->stmt->dobjstack; (act = *actptr);
                             actptr = &act->prev) {
                            if ((act->type == EAT_CATCHBLOCK &&
                                 act->data.catch_block.catch_label == linear->u.label.label) ||
                                (act->type == EAT_SPECIFICATION &&
                                 act->data.specification.unexp_label == linear->u.label.label)) {
                                *actptr = act->prev;
                            }
                        }
                    }
                }
            }
            if (node == IRO_LastNode) {
                IRO_LastNode = prevNode;
            }
        } else {
            prevNode = node;
        }
    }

    if (result) {
        IRO_ComputeSuccPred();
        IRO_ComputeDom();
    }

    IRO_CheckForUserBreak();
    return result;
}
#else
Boolean IRO_RemoveUnreachable(void) {
    IRONode* node;
    IRONode* prevNode;
    IROLinear* scan;
    IROLinear* linear;
    Boolean result;
    ExceptionAction** actptr;
    ExceptionAction* act;

    result = FALSE;
    IRO_ComputeSuccPred();
    IRO_MakeReachable(IRO_FirstNode);
    prevNode = IRO_FirstNode;
    for (node = IRO_FirstNode->nextnode; node; node = node->nextnode) {
        if (node->first && !node->reachable) {
            IRO_Dump("Removing unreachable code at: %d\n", node->index);
            prevNode->nextnode = node->nextnode;
            prevNode->last->next = node->last->next;
            result = TRUE;
            for (linear = node->first;
                 linear && linear->type == IROLinearLabel && linear != node->last->next;
                 linear = linear->next) {
                for (scan = IRO_FirstLinear; scan; scan = scan->next) {
                    if (scan->stmt) {
                        scan->stmt->marked = FALSE;
                    }
                }

                for (scan = IRO_FirstLinear; scan; scan = scan->next) {
                    if (scan->stmt && !scan->stmt->marked) {
                        scan->stmt->marked = TRUE;
                        for (actptr = &scan->stmt->dobjstack; (act = *actptr);
                             actptr = &act->prev) {
                            if ((act->type == EAT_CATCHBLOCK &&
                                 act->data.catch_block.catch_label == linear->u.label.label) ||
                                (act->type == EAT_SPECIFICATION &&
                                 act->data.specification.unexp_label == linear->u.label.label)) {
                                *actptr = act->prev;
                            }
                        }
                    }
                }
            }
            if (node == IRO_LastNode) {
                IRO_LastNode = prevNode;
            }
        } else {
            prevNode = node;
        }
    }

    if (result) {
        IRO_ComputeSuccPred();
        IRO_ComputeDom();
    }

    IRO_CheckForUserBreak();
    return result;
}
#endif

#if MWCCPSP
// 3.0.1 (0x58D410): as 2.4.7, but a goto that ends a loop (IROLF_EndLoop) is
// neither removed as a goto-next nor folded into a branch around it
Boolean IRO_RemoveRedundantJumps(void) {
    IRONode* node;
    IROLinear* linear;
    IROLinear* scan;
    IROLinear* scan2;
    SwitchInfo* info;
    SwitchCase* curcase;
    Boolean result;

    result = FALSE;
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->first) {
            linear = node->last;
            switch (linear->type) {
                case IROLinearGoto:
                    if (linear->flags & IROLF_EndLoop) {
                        break;
                    }
                    scan = linear->next;
                    while (scan &&
                           ((scan->type == IROLinearNop) ||
                            (scan->type == IROLinearLabel &&
                             scan->u.label.label != linear->u.label.label))) {
                        scan = scan->next;
                    }
                    while (scan && scan->type == IROLinearLabel) {
                        if (scan->u.label.label == linear->u.label.label) {
                            IRO_Dump("Removing goto next at %d\n", linear->index);
                            linear->type = IROLinearNop;
                            result = TRUE;
                            break;
                        }
                        scan = scan->next;
                    }
                    break;

                case IROLinearIf:
                case IROLinearIfNot:
                    scan = linear->next;
                    while (scan && scan->type == IROLinearNop) {
                        scan = scan->next;
                    }
                    if (scan && scan->type == IROLinearGoto && !(scan->flags & IROLF_EndLoop)) {
                        scan2 = scan->next;
                        while (scan2 &&
                               ((scan2->type == IROLinearNop) ||
                                (scan2->type == IROLinearLabel &&
                                 scan2->u.label.label != linear->u.label.label))) {
                            scan2 = scan2->next;
                        }

                        if (scan2 &&
                            scan2->type == IROLinearLabel &&
                            scan2->u.label.label == linear->u.label.label) {
                            if (linear->type == IROLinearIf) {
                                linear->type = IROLinearIfNot;
                            } else {
                                linear->type = IROLinearIf;
                            }

                            linear->u.label.label = scan->u.label.label;
                            scan->type = IROLinearNop;
                            IRO_Dump("Removing branch around goto at %d\n", linear->index);
                            result = TRUE;
                        }
                    }

                    scan2 = linear->next;
                    while (scan2 &&
                           ((scan2->type == IROLinearNop) ||
                            (scan2->type == IROLinearLabel &&
                             scan2->u.label.label != linear->u.label.label))) {
                        scan2 = scan2->next;
                    }
                    while (scan2 && scan2->type == IROLinearLabel) {
                        if (scan2->u.label.label == linear->u.label.label) {
                            IRO_Dump("Removing If/IfNot_Goto next at %d\n", linear->index);
                            linear->type = IROLinearNop;
                            IRO_CheckSideEffect(linear->u.label.cond);
                            result = TRUE;
                            break;
                        }
                        scan2 = scan2->next;
                    }
                    break;

                case IROLinearSwitch:
                    info = linear->u.swtch.info;
                    curcase = info->cases;
                    while (curcase && curcase->label == info->defaultlabel) {
                        curcase = curcase->next;
                    }
                    if (!curcase) {
                        IRO_Dump("Removing Switch next at %d\n", linear->index);
                        IRO_CheckSideEffect(linear->u.swtch.cond);
                        linear->type = IROLinearGoto;
                        linear->u.label.label = info->defaultlabel;
                        result = TRUE;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    if (result) {
        IRO_ComputeSuccPred();
        IRO_ComputeDom();
    }

    IRO_CheckForUserBreak();
    return result;
}
#else
Boolean IRO_RemoveRedundantJumps(void) {
    IRONode* node;
    IROLinear* linear;
    IROLinear* scan;
    IROLinear* scan2;
    SwitchInfo* info;
    SwitchCase* curcase;
    Boolean result;

    result = FALSE;
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->first) {
            linear = node->last;
            switch (linear->type) {
                case IROLinearGoto:
                    scan = linear->next;
                    while (scan &&
                           ((scan->type == IROLinearNop) ||
                            (scan->type == IROLinearLabel &&
                             scan->u.label.label != linear->u.label.label))) {
                        scan = scan->next;
                    }
                    while (1) {
                        if (!scan) {
                            break;
                        }
                        if (scan->type != IROLinearLabel) {
                            break;
                        }
                        if (scan->u.label.label == linear->u.label.label) {
                            IRO_Dump("Removing goto next at %d\n", linear->index);
                            linear->type = IROLinearNop;
                            result = TRUE;
                            break;
                        }
                        scan = scan->next;
                    }
                    break;

                case IROLinearIf:
                case IROLinearIfNot:
                    scan = linear->next;
                    while (scan && scan->type == IROLinearNop) {
                        scan = scan->next;
                    }
                    if (scan && scan->type == IROLinearGoto) {
                        scan2 = scan->next;
                        while (scan2 &&
                               ((scan2->type == IROLinearNop) ||
                                (scan2->type == IROLinearLabel &&
                                 scan2->u.label.label != linear->u.label.label))) {
                            scan2 = scan2->next;
                        }

                        if (scan2 &&
                            scan2->type == IROLinearLabel &&
                            scan2->u.label.label == linear->u.label.label) {
                            if (linear->type == IROLinearIf) {
                                linear->type = IROLinearIfNot;
                            } else {
                                linear->type = IROLinearIf;
                            }

                            linear->u.label.label = scan->u.label.label;
                            scan->type = IROLinearNop;
                            IRO_Dump("Removing branch around goto at %d\n", linear->index);
                            result = TRUE;
                        }
                    }

                    scan2 = linear->next;
                    while (scan2 &&
                           ((scan2->type == IROLinearNop) ||
                            (scan2->type == IROLinearLabel &&
                             scan2->u.label.label != linear->u.label.label))) {
                        scan2 = scan2->next;
                    }
                    while (1) {
                        if (!scan2) {
                            break;
                        }
                        if (scan2->type != IROLinearLabel) {
                            break;
                        }
                        if (scan2->u.label.label == linear->u.label.label) {
                            IRO_Dump("Removing If/IfNot_Goto next at %d\n", linear->index);
                            linear->type = IROLinearNop;
                            IRO_CheckSideEffect(linear->u.label.cond);
                            result = TRUE;
                            break;
                        }
                        scan2 = scan2->next;
                    }
                    break;

                case IROLinearSwitch:
                    info = linear->u.swtch.info;
                    curcase = info->cases;
                    while (curcase && curcase->label == info->defaultlabel) {
                        curcase = curcase->next;
                    }
                    if (!curcase) {
                        IRO_Dump("Removing Switch next at %d\n", linear->index);
                        IRO_CheckSideEffect(linear->u.swtch.cond);
                        linear->type = IROLinearGoto;
                        linear->u.label.label = info->defaultlabel;
                        result = TRUE;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    if (result) {
        IRO_ComputeSuccPred();
        IRO_ComputeDom();
    }

    IRO_CheckForUserBreak();
    return result;
}

#endif
#if MWCCPSP

#endif
Boolean IRO_RemoveLabels(void) {
    Boolean result;
    IRONode* node;

    result = FALSE;
    IRO_ComputeSuccPred();
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->first && node->first->type == IROLinearLabel && !node->referenced) {
            node->first->type = IROLinearNop;
            node->first->flags &= ~IROLF_StartsNode;
            result = TRUE;
        }
    }

    IRO_CheckForUserBreak();
    return result;
}
