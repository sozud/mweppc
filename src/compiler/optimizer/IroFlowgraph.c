#if MWCCPSP
#include "compiler/IrOptimizer.h"
#include "compiler/IroMalloc.h"
#endif
#include "compiler/IroFlowgraph.h"
#if MWCCPSP
#include "compiler/IROUseDef301.h"
#endif
#include "compiler/IroCSE.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroPropagate.h"
#include "compiler/IROUseDef.h"
#include "compiler/IroUtil.h"
#include "compiler/BitVector.h"
#include "compiler/CError.h"
#include "compiler/CFunc.h"
#include "compiler/CompilerTools.h"
#include "compiler/enode.h"
#include "compiler/Exceptions.h"
#include "compiler/InlineAsmPPC.h"
#include "compiler/Switch.h"
#include <string.h>

UInt16 IRO_NumNodes;
IRONode* IRO_FirstNode;
IRONode* IRO_LastNode;
IRONode* IRO_EndNode;
IRONode** IRO_NodeTable;
BitVector* IRO_VarKills;
BitVector* IRO_Avail;
BitVector* IRO_FuncKills;
BitVector* IRO_ExprKills;

#if !MWCCPSP
static IRONode* StartNode(IROLinear* linear) {
    IRONode* node = oalloc(sizeof(IRONode));

    node->index = IRO_NumNodes;
    node->numsucc = 0;
    node->succ = NULL;
    node->numpred = 0;
    node->pred = NULL;
    node->first = linear;
    node->last = linear;
    node->in = NULL;
    node->out = NULL;
    node->gen = NULL;
    node->kill = NULL;
    node->unused = 0;
    node->copyOut = NULL;
    node->dom = NULL;
    node->nextnode = NULL;
    node->reachable = FALSE;
    node->visited = FALSE;
    node->mustreach = FALSE;
    node->referenced = FALSE;
    node->loopdepth = 0;
    node->marked = FALSE;
    node->addressed = NULL;

    IRO_NumNodes++;
    if (!IRO_FirstNode) {
        IRO_FirstNode = node;
    } else {
        IRO_LastNode->nextnode = node;
    }
    IRO_LastNode = node;
    return node;
}
#endif

static void AddSucc(IRONode* a, IRONode* b) {
    a->succ[a->numsucc++] = b->index;
    b->numpred++;
}

static void AddLabelSucc(IRONode* node, CLabel* label) {
    IRONode* targetnode = (IRONode*)label->stmt;
    if (targetnode) {
        AddSucc(node, targetnode);
        targetnode->referenced = TRUE;
    } else {
        CError_FATAL(126);
    }
}

static void AddSwitchSucc(IRONode* node) {
    SwitchInfo* info = node->last->u.swtch.info;
    SwitchCase* curcase = info->cases;
    SInt32 i = 1;

    while (curcase) {
        i++;
        curcase = curcase->next;
    }

    node->succ = oalloc(sizeof(UInt16) * i);
    for (curcase = info->cases; curcase; curcase = curcase->next) {
        AddLabelSucc(node, curcase->label);
    }
    AddLabelSucc(node, info->defaultlabel);
}

static void AddPred(UInt32 a, UInt32 b) {
    IRONode* node = IRO_NodeTable[a];
    node->pred[node->numpred++] = b;
}

#if MWCCPSP
// 3.0.1: successors are not added twice; a node's successor array is sized
// for its exception edges too (added last by AddThrowSuccs); computed gotos
// take their targets from IRO_GetGotoTargets; nodes reached from asm labels
// and catch/specification labels count as referenced.
static inline void AddSucc301(IRONode* a, IRONode* b) {
    UInt16 i;

    for (i = 0; i < a->numsucc; i++) {
        if (b->index == a->succ[i]) {
            return;
        }
    }
    a->succ[a->numsucc++] = b->index;
    b->numpred++;
}

static inline void AddLabelSucc301(IRONode* node, CLabel* label) {
    IRONode* target = (IRONode*)label->stmt;
    if (target) {
        AddSucc301(node, target);
        target->referenced = TRUE;
    } else {
        CError_FATAL(141);
    }
}

static void AddThrowSuccs(IRONode* node, IROLinear* linear, UInt16 count) {
    ExceptionAction* action;

    for (action = linear->stmt ? linear->stmt->dobjstack : NULL; action; action = action->prev) {
        if (action->type == EAT_CATCHBLOCK) {
            AddLabelSucc301(node, action->data.catch_block.catch_label);
        } else if (action->type == EAT_SPECIFICATION) {
            AddLabelSucc301(node, action->data.specification.unexp_label);
        }
    }
}

static void AddSwitchSucc301(IRONode* node, UInt16 count) {
    SwitchInfo* info = node->last->u.swtch.info;
    SwitchCase* curcase;
    UInt16 i = 1;

    for (curcase = info->cases; curcase; curcase = curcase->next) {
        i++;
    }
    node->succ = IRO_malloc((count + i) * sizeof(UInt16));
    for (curcase = info->cases; curcase; curcase = curcase->next) {
        AddLabelSucc301(node, curcase->label);
    }
    AddLabelSucc301(node, info->defaultlabel);
}

extern CLabel* func_0052B3F0(Statement* asmStmt);

void IRO_ComputeSuccPred(void) {
    CLabel* label;
    IRONode* node;
    IRONode* succ;
    IROLinear* linear;
    ExceptionAction* action;
    UInt16 count;
    UInt16 n;
    UInt16 i;

    for (label = Labels; label; label = label->next) {
        label->stmt = NULL;
    }

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        node->referenced = FALSE;
        node->numsucc = 0;
        node->numpred = 0;
        node->reachable = FALSE;
        node->visited = FALSE;
        if (node->first && node->first->type == IROLinearLabel) {
            node->first->u.label.label->stmt = (Statement*)node;
        }
    }

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (!node->first) {
            if (node->nextnode) {
                node->succ = IRO_malloc(sizeof(UInt16));
                AddSucc301(node, node->nextnode);
            }
        } else {
            linear = node->last;
next_linear:
            count = 0;
            if (IRO_IsThrowingCall(linear)) {
                count = 0;
                for (action = linear->stmt ? linear->stmt->dobjstack : NULL; action;
                     action = action->prev) {
                    if (action->type == EAT_CATCHBLOCK || action->type == EAT_SPECIFICATION) {
                        count++;
                    }
                }
            }
            switch (linear->type) {
                case IROLinearOp2Arg:
                    if (linear->nodetype == ECOMMA) {
                        linear = linear->u.diadic.right;
                        goto next_linear;
                    }
                    goto fallthrough;
                case IROLinearGoto:
                    node->succ = IRO_malloc((count + 1) * sizeof(UInt16));
                    AddLabelSucc301(node, linear->u.label.label);
                    break;
                case IROLinearGotoExpr: {
                    IROAddressedLabel* list;
                    IROAddressedLabel* al;

                    list = IRO_GetGotoTargets(linear, &n);
                    if (!list || !n) {
                        CError_FATAL(324);
                    }
                    node->succ = IRO_malloc((count + n) * sizeof(UInt16));
                    for (al = list; al; al = al->next) {
                        AddLabelSucc301(node, al->label);
                        n--;
                    }
                    if (n) {
                        CError_FATAL(338);
                    }
                    IRO_FreeGotoTargets(list);
                    break;
                }
                case IROLinearIf:
                case IROLinearIfNot:
                    node->succ = IRO_malloc((count + 2) * sizeof(UInt16));
                    AddSucc301(node, node->nextnode);
                    AddLabelSucc301(node, linear->u.label.label);
                    break;
                case IROLinearReturn:
                    node->succ = IRO_malloc(count * sizeof(UInt16));
                    break;
                case IROLinearSwitch:
                    AddSwitchSucc301(node, count);
                    break;
                case IROLinearAsm:
                    if (IRO_GetAsmEffectsHook && linear->u.asm_stmt) {
                        IROAsmEffects effects = IRO_EmptyAsmEffects;

                        IRO_GetAsmEffectsHook(linear->u.asm_stmt, &effects);
                        n = !effects.noFallthrough + effects.numlabels;
                        node->succ = IRO_malloc((count + n) * sizeof(UInt16));
                        if (!effects.noFallthrough) {
                            AddSucc301(node, node->nextnode);
                        }
                        for (i = 0; i < effects.numlabels; i++) {
                            AddLabelSucc301(node, effects.labels[i]);
                        }
                    }
                    break;
                case IROLinearEnd:
                    node->succ = IRO_malloc(count * sizeof(UInt16));
                    break;
                default:
fallthrough:
                    if (node->nextnode) {
                        node->succ = IRO_malloc((count + 1) * sizeof(UInt16));
                        AddSucc301(node, node->nextnode);
                    } else if (count) {
                        node->succ = IRO_malloc(count * sizeof(UInt16));
                    }
                    break;
            }
            if (count) {
                AddThrowSuccs(node, linear, count);
            }
        }
    }

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->numpred) {
            node->pred = IRO_malloc(node->numpred * sizeof(UInt16));
        } else {
            node->pred = NULL;
        }
        node->numpred = 0;
    }

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        for (i = 0; i < node->numsucc; i++) {
            succ = IRO_NodeTable[node->succ[i]];
            succ->pred[succ->numpred++] = node->index;
        }
    }

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        for (linear = node->first; linear; linear = linear->next) {
            if (linear->type == IROLinearAsm &&
                linear->u.asm_stmt &&
                (label = func_0052B3F0(linear->u.asm_stmt)) &&
                label->stmt) {
                ((IRONode*)label->stmt)->referenced = TRUE;
            }
            for (action = linear->stmt ? linear->stmt->dobjstack : NULL; action;
                 action = action->prev) {
                if (action->type == EAT_CATCHBLOCK) {
                    if (action->data.catch_block.catch_label &&
                        action->data.catch_block.catch_label->stmt) {
                        ((IRONode*)action->data.catch_block.catch_label->stmt)->referenced = TRUE;
                    }
                } else if (action->type == EAT_SPECIFICATION) {
                    if (action->data.specification.unexp_label &&
                        action->data.specification.unexp_label->stmt) {
                        ((IRONode*)action->data.specification.unexp_label->stmt)->referenced = TRUE;
                    }
                }
            }
            if (node->first->type == IROLinearLabel &&
                (linear->type == IROLinearBeginCatch ||
                 linear->type == IROLinearEndCatch ||
                 linear->type == IROLinearEndCatchDtor)) {
                node->referenced = TRUE;
            }
            if (linear == node->last) {
                break;
            }
        }
    }
}
#else
void IRO_ComputeSuccPred(void) {
    CLabel* label;
    IRONode* node;
    SInt32 count;
    ExceptionAction* action;
    IAEffects effects;
    UInt16 i;
    IROLinear* linear;

    for (label = Labels; label; label = label->next) {
        label->stmt = NULL;
    }

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        node->referenced = FALSE;
        node->numsucc = 0;
        node->numpred = 0;
        node->reachable = FALSE;
        node->visited = FALSE;
        if (node->first && node->first->type == IROLinearLabel) {
            node->first->u.label.label->stmt = (Statement*)node;
        }
    }

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (!node->first) {
            if (node->nextnode) {
                node->succ = oalloc(sizeof(UInt16));
                AddSucc(node, node->nextnode);
            }
        } else {
            linear = node->last;
next_linear:
            switch (linear->type) {
                case IROLinearReturn:
                case IROLinearEnd:
                    break;
                case IROLinearGoto:
                    node->succ = oalloc(sizeof(UInt16));
                    AddLabelSucc(node, linear->u.label.label);
                    break;
                case IROLinearIf:
                case IROLinearIfNot:
                    node->succ = oalloc(sizeof(UInt16) * 2);
                    AddSucc(node, node->nextnode);
                    AddLabelSucc(node, linear->u.label.label);
                    break;
                case IROLinearSwitch:
                    AddSwitchSucc(node);
                    break;
                case IROLinearFunccall:
                    count = 1;
                    if (linear->stmt && IRO_FunctionCallMightThrowException(linear)) {
                        for (action = linear->stmt->dobjstack; action; action = action->prev) {
                            if (action->type == EAT_CATCHBLOCK ||
                                action->type == EAT_SPECIFICATION) {
                                count++;
                            }
                        }
                    }
                    node->succ = oalloc(sizeof(UInt16) * count);
                    AddSucc(node, node->nextnode);
                    if (linear->stmt && IRO_FunctionCallMightThrowException(linear)) {
                        for (action = linear->stmt->dobjstack; action; action = action->prev) {
                            if (action->type == EAT_CATCHBLOCK) {
                                AddLabelSucc(node, action->data.catch_block.catch_label);
                            } else if (action->type == EAT_SPECIFICATION) {
                                AddLabelSucc(node, action->data.specification.unexp_label);
                            }
                        }
                    }
                    break;
                case IROLinearAsm:
                    CodeGen_GetAsmEffects(linear->u.asm_stmt, &effects);
                    node->succ =
                        oalloc(sizeof(UInt16) * (!effects.noFallthrough + effects.numlabels));
                    if (!effects.noFallthrough) {
                        AddSucc(node, node->nextnode);
                    }
                    for (i = 0; i < effects.numlabels; i++) {
                        AddLabelSucc(node, effects.labels[i]);
                    }
                    break;
                case IROLinearOp2Arg:
                    if (linear->nodetype == ECOMMA) {
                        linear = linear->u.diadic.right;
                        goto next_linear;
                    }
                default:
                    if (node->nextnode) {
                        node->succ = oalloc(sizeof(UInt16));
                        AddSucc(node, node->nextnode);
                    }
            }
        }
    }

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->numpred) {
            node->pred = oalloc(sizeof(UInt16) * node->numpred);
        } else {
            node->pred = NULL;
        }
        node->numpred = 0;
    }

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        for (i = 0; i < node->numsucc; i++) {
            AddPred(node->succ[i], node->index);
        }
    }

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->first && node->first->type == IROLinearLabel) {
            IROLinear* linear = node->first;
            do {
                if (linear->type == IROLinearBeginCatch ||
                    linear->type == IROLinearEndCatch ||
                    linear->type == IROLinearEndCatchDtor) {
                    node->referenced = TRUE;
                    break;
                }
            } while (linear != node->last && (linear = linear->next));
        }
    }
}
#endif

#if MWCCPSP
// 3.0.1 frees the previous dominator sets first (0x576EB0)
void IRO_ComputeDom(void) {
    BitVector* bv;
    IRONode* node;
    int repeat;
    UInt16 i;

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->dom) {
            if (node->dom) {
                IRO_free(node->dom);
                node->dom = NULL;
            }
            node->dom = NULL;
        }
    }

    Bv_AllocVector(&IRO_FirstNode->dom, IRO_NumNodes);
    Bv_SetBit(IRO_FirstNode->index, IRO_FirstNode->dom);
    for (node = IRO_FirstNode->nextnode; node; node = node->nextnode) {
        Bv_AllocVector(&node->dom, IRO_NumNodes);
        Bv_Set(node->dom);
    }

    Bv_AllocVector(&bv, IRO_NumNodes);

    do {
        repeat = FALSE;
        for (node = IRO_FirstNode->nextnode; node; node = node->nextnode) {
            if (node->numpred) {
                Bv_Set(bv);
                for (i = 0; i < node->numpred; i++) {
                    Bv_And(IRO_NodeTable[node->pred[i]]->dom, bv);
                }
                Bv_SetBit(node->index, bv);
            } else {
                Bv_Clear(bv);
                Bv_SetBit(node->index, bv);
            }

            if (!Bv_Compare(bv, node->dom)) {
                Bv_Copy(bv, node->dom);
                repeat = TRUE;
            }
        }
    } while (repeat);
}
#else
void IRO_ComputeDom(void) {
    BitVector* bv;
    IRONode* node;
    int repeat;
    SInt32 i;

    Bv_AllocVector(&IRO_FirstNode->dom, IRO_NumNodes);
    Bv_SetBit(IRO_FirstNode->index, IRO_FirstNode->dom);
    for (node = IRO_FirstNode->nextnode; node; node = node->nextnode) {
        Bv_AllocVector(&node->dom, IRO_NumNodes);
        Bv_Set(node->dom);
    }

    Bv_AllocVector(&bv, IRO_NumNodes);

    do {
        repeat = FALSE;
        for (node = IRO_FirstNode->nextnode; node; node = node->nextnode) {
            if (node->numpred) {
                Bv_Set(bv);
                for (i = 0; i < node->numpred; i++) {
                    Bv_And(IRO_NodeTable[node->pred[i]]->dom, bv);
                }
                Bv_SetBit(node->index, bv);
            } else {
                Bv_Clear(bv);
                Bv_SetBit(node->index, bv);
            }

            if (!Bv_Compare(bv, node->dom)) {
                Bv_Copy(bv, node->dom);
                repeat = TRUE;
            }
        }
    } while (repeat);
}
#endif

#if MWCCPSP
// 3.0.1: nodes are pool-allocated (StartNode301) and the previous
// graph is freed first; a call that may throw into a catch or exception
// specification ends its node; asm statements with labels end theirs (the
// back end's hook says); the node table is indexed by node number.
extern void ObjectSet_Term(struct ObjectSet* set);
extern Boolean IRO_FunctionCallMightThrowException(IROLinear* linear);

static void StartNode301(IROLinear* linear) {
    IRONode* node;

    node = IRO_malloc(sizeof(IRONode));
    memset(node, 0, sizeof(IRONode));
    node->index = IRO_NumNodes++;
    node->nextnode = NULL;
    node->numsucc = 0;
    node->succ = NULL;
    node->numpred = 0;
    node->pred = NULL;
    node->first = linear;
    node->last = linear;
    node->in = NULL;
    node->out = NULL;
    node->gen = NULL;
    node->kill = NULL;
    node->copyOut = NULL;
    node->dom = NULL;
    node->nextnode = NULL;
    node->reachable = FALSE;
    node->visited = FALSE;
    node->mustreach = FALSE;
    node->unk3F = FALSE;
    node->referenced = FALSE;
    node->loopdepth = 0;
    node->marked = FALSE;
    node->addressed = NULL;
    if (IRO_FirstNode) {
        IRO_LastNode->nextnode = node;
    } else {
        IRO_FirstNode = node;
    }
    IRO_LastNode = node;
}

void IRO_FreeFlowGraphNode(IRONode* node) {
    IRO_NumNodes--;
    if (node->succ) {
        IRO_free(node->succ);
    }
    if (node->pred) {
        IRO_free(node->pred);
    }
    if (node->in) {
        IRO_free(node->in);
        node->in = NULL;
    }
    if (node->out) {
        IRO_free(node->out);
        node->out = NULL;
    }
    if (node->gen) {
        IRO_free(node->gen);
        node->gen = NULL;
    }
    if (node->kill) {
        IRO_free(node->kill);
        node->kill = NULL;
    }
    if (node->copyOut) {
        IRO_free(node->copyOut);
        node->copyOut = NULL;
    }
    if (node->dom) {
        IRO_free(node->dom);
        node->dom = NULL;
    }
    if (node->addressed) {
        ObjectSet_Term(node->addressed);
        ((UInt32*)node->addressed)[0] = 0;
        ((UInt32*)node->addressed)[1] = 0;
        ((UInt32*)node->addressed)[0] = -1;
        ((UInt32*)node->addressed)[1] = -1;
        IRO_free2(node->addressed);
    }
    IRO_free(node);
}

// 3.0.1 (0x576550): reachability that also flows out of nodes holding an asm
// statement the back end calls a barrier (IRONode +0x43)
void IRO_FindAsmBarriers(void) {
    IRONode* fnode;
    IRONode* succ;
    IROLinear* linear;
    Boolean changed;
    UInt16 i;

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        fnode->hasAsmBarrier = FALSE;
    }
    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (fnode->last) {
            for (linear = fnode->first;
                 !fnode->hasAsmBarrier && linear && linear != fnode->last->next;
                 linear = linear->next) {
                if (linear->type == IROLinearAsm && IRO_GetAsmEffectsHook && linear->u.asm_stmt) {
                    IROAsmEffects effects = IRO_EmptyAsmEffects;
                    IRO_GetAsmEffectsHook(linear->u.asm_stmt, &effects);
                    if (effects.reachBarrier) {
                        fnode->hasAsmBarrier = TRUE;
                    }
                }
            }
        }
    }

    IRO_FirstNode->reachable = TRUE;
    do {
        changed = FALSE;
        for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
            if ((fnode->reachable || fnode->hasAsmBarrier) && !fnode->visited) {
                for (i = 0; i < fnode->numsucc; i++) {
                    succ = IRO_NodeTable[fnode->succ[i]];
                    if (!succ->reachable) {
                        changed = TRUE;
                        succ->reachable = TRUE;
                    }
                }
                fnode->visited = TRUE;
            }
        }
    } while (changed);
}

void IRO_BuildFlowgraph(IROLinear* linear) {
    IROLinear* scan;
    IROLinear* nw;
    CLabel* label;
    ExceptionAction* action;
    IRONode* node;
    IRONode* next;
    Boolean endsNode;
    int count;
    SInt32 i;

    if (IRO_FirstNode) {
        for (node = IRO_FirstNode; node; node = next) {
            next = node->nextnode;
            IRO_FreeFlowGraphNode(node);
        }
    }
    IRO_NumNodes = 0;
    IRO_FirstNode = NULL;
    IRO_LastNode = NULL;

    for (label = Labels; label; label = label->next) {
        label->stmt = NULL;
    }

    for (scan = linear; scan; scan = scan->next) {
        StartNode301(scan);
        if (scan->type == IROLinearLabel) {
            scan->u.label.label->stmt = (Statement*)IRO_LastNode;
        }

        endsNode = FALSE;
        while (!endsNode && scan->next && !(scan->next->flags & IROLF_StartsNode)) {
            if (IRO_FunctionCallMightThrowException(scan)) {
                count = 0;
                for (action = scan->stmt ? scan->stmt->dobjstack : NULL; action;
                     action = action->prev) {
                    if (action->type == EAT_CATCHBLOCK || action->type == EAT_SPECIFICATION) {
                        count++;
                    }
                }
                if (count) {
                    endsNode = TRUE;
                }
            }
            switch (scan->type) {
                case IROLinearFunccall:
                    if (endsNode) {
                        goto split;
                    }
                    break;
                case IROLinearGoto:
                case IROLinearGotoExpr:
                case IROLinearReturn:
                case IROLinearEntry:
                case IROLinearExit:
                case IROLinearEnd:
                    endsNode = TRUE;
                    break;
                case IROLinearIf:
                case IROLinearIfNot:
                case IROLinearSwitch:
                    endsNode = TRUE;
split:
                    if (scan->next->type == IROLinearLabel) {
                        nw = IRO_NewLinear(IROLinearNop);
                        nw->index = ++IRO_NumLinear;
                        nw->next = scan->next;
                        scan->next = nw;
                    }
                    break;
                case IROLinearAsm:
                    if (IRO_GetAsmEffectsHook && scan->u.asm_stmt) {
                        IROAsmEffects effects = IRO_EmptyAsmEffects;
                        IRO_GetAsmEffectsHook(scan->u.asm_stmt, &effects);
                        if (effects.numlabels) {
                            endsNode = TRUE;
                        }
                    }
                    break;
            }
            if (!endsNode) {
                scan = scan->next;
            }
        }

        IRO_LastNode->last = scan;
    }

    if (IRO_NodeTable) {
        IRO_free(IRO_NodeTable);
    }
    IRO_NodeTable = NULL;
    IRO_NodeTable = IRO_malloc(IRO_NumNodes * sizeof(IRONode*));
    memset(IRO_NodeTable, 0, IRO_NumNodes * sizeof(IRONode*));
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        IRO_NodeTable[node->index] = node;
    }

    IRO_ComputeSuccPred();
    IRO_ComputeDom();
    IRO_CheckForUserBreak();
}

// 0x576E30: the index -> node table again, after the loop passes add nodes
// (name not known)
void IRO_RebuildNodeTable(void) {
    IRONode* node;

    if (IRO_NodeTable) {
        IRO_free(IRO_NodeTable);
    }
    IRO_NodeTable = NULL;
    IRO_NodeTable = IRO_malloc(IRO_NumNodes * sizeof(IRONode*));
    memset(IRO_NodeTable, 0, IRO_NumNodes * sizeof(IRONode*));
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        IRO_NodeTable[node->index] = node;
    }
}
#else
void IRO_BuildFlowgraph(IROLinear* linear) {
    IROLinear* scan;
    CLabel* label;
    ExceptionAction* action;
    IAEffects effects;
    IRONode* node;
    SInt32 i;
    int endsNode;

    for (label = Labels; label; label = label->next) {
        label->stmt = NULL;
    }

    scan = linear;
    IRO_NumNodes = 0;
    IRO_FirstNode = IRO_LastNode = IRO_EndNode = NULL;
    while (scan) {
        StartNode(scan);
        if (scan->type == IROLinearLabel) {
            scan->u.label.label->stmt = (Statement*)IRO_LastNode;
        }

        endsNode = FALSE;
        while (!endsNode && scan->next && !(scan->next->flags & IROLF_StartsNode)) {
            switch (scan->type) {
                case IROLinearGoto:
                case IROLinearReturn:
                case IROLinearEntry:
                case IROLinearExit:
                case IROLinearEnd:
                    endsNode = TRUE;
                    break;
                case IROLinearIf:
                case IROLinearIfNot:
                case IROLinearSwitch:
                    endsNode = TRUE;
skip:
                    if (scan->next->type == IROLinearLabel) {
                        IROLinear* nw = IRO_NewLinear(IROLinearNop);
                        nw->index = ++IRO_NumLinear;
                        nw->next = scan->next;
                        scan->next = nw;
                    }
                    break;
                case IROLinearFunccall:
                    if (scan->stmt && IRO_FunctionCallMightThrowException(scan)) {
                        for (action = scan->stmt->dobjstack; action; action = action->prev) {
                            if (action->type == EAT_CATCHBLOCK ||
                                action->type == EAT_SPECIFICATION) {
                                endsNode = TRUE;
                                goto skip;
                            }
                        }
                    }
                    break;
                case IROLinearAsm:
                    CodeGen_GetAsmEffects(scan->u.asm_stmt, &effects);
                    if (effects.numlabels) {
                        endsNode = TRUE;
                    }
                    break;
                default:
                    break;
            }
            if (!endsNode) {
                scan = scan->next;
            }
        }

        if (scan->type == IROLinearEnd) {
            IRO_EndNode = IRO_LastNode;
        }
        IRO_LastNode->last = scan;
        scan = scan->next;
    }

    IRO_NodeTable = oalloc(IRO_NumNodes * sizeof(IRONode*));
    memset(IRO_NodeTable, 0, IRO_NumNodes * sizeof(IRONode*));
    for (node = IRO_FirstNode, i = 0; node; node = node->nextnode) {
        IRO_NodeTable[i++] = node;
    }

    IRO_ComputeSuccPred();
    IRO_ComputeDom();
    IRO_CheckForUserBreak();
}
#endif

IRONode* IRO_NewFlowGraphNode(void) {
#if MWCCPSP
    IRONode* node = IRO_malloc(sizeof(IRONode));
#else
    IRONode* node = oalloc(sizeof(IRONode));
#endif
    memset(node, 0, sizeof(IRONode));
    node->index = IRO_NumNodes;
    IRO_NumNodes++;
    node->nextnode = NULL;
    return node;
}

IRONode* IRO_MergeFlowGraphNodes(IRONode* a, IRONode* b) {
    IRONode* succ;
    Boolean found;
    UInt32 i;
    UInt32 j;
    UInt32 k;

    if (a->nextnode == b && a->last && b->first && a->last->next == b->first) {
        if (b->first->type == IROLinearLabel) {
            IRO_NopOut(b->first);
        }

        a->nextnode = b->nextnode;
        a->last = b->last;

        for (i = 0; i < a->numsucc; i++) {
            if (b->index == a->succ[i]) {
                for (j = i; j < a->numsucc; j++) {
                    if ((j + 1) < a->numsucc) {
                        a->succ[j] = a->succ[j + 1];
                    } else {
                        a->succ[j] = 0;
                    }
                }
                a->numsucc--;
                break;
            }
        }

        for (i = 0; i < b->numsucc; i++) {
            succ = IRO_NodeTable[b->succ[i]];
            for (j = 0; j < a->numsucc; j++) {
                if (b->succ[i] == a->succ[j]) {
                    break;
                }
            }

            if (j == a->numsucc) {
                AddSucc(a, succ);
                succ->numpred--;
            }

            found = FALSE;
            for (j = 0; j < succ->numpred; j++) {
                if (a->index == succ->pred[j]) {
                    found = TRUE;
                    break;
                }
            }

            for (j = 0; j < succ->numpred; j++) {
                if (b->index == succ->pred[j]) {
                    if (!found) {
                        succ->pred[j] = a->index;
                    } else {
                        for (k = j; k < succ->numpred; k++) {
                            if ((k + 1) < succ->numpred) {
                                succ->pred[k] = succ->pred[k + 1];
                            } else {
                                succ->pred[k] = 0;
                            }
                        }
                        succ->numpred--;
                    }
                    break;
                }
            }
        }

        b->numsucc = b->numpred = 0;
        b->first = b->last = NULL;
        b->nextnode = NULL;
        b->reachable = FALSE;
        b->visited = FALSE;
        b->mustreach = FALSE;
#if MWCCPSP
        b->unk3F = FALSE;
#endif
        b->referenced = FALSE;
        b->loopdepth = 0;

        if (IRO_LastNode == b) {
            IRO_LastNode = a;
        }

        if (IRO_FirstExpr && IRO_LastExpr) {
            IROExpr* expr;
            for (expr = IRO_FirstExpr; expr && expr != IRO_LastExpr->next; expr = expr->next) {
                if (expr->node == b) {
                    expr->node = a;
                }
            }
        }

        if (IRO_FirstAssign && IRO_LastAssign) {
            IROAssign* assign;
            for (assign = IRO_FirstAssign; assign && assign != IRO_LastAssign->next;
                 assign = assign->next) {
                if (assign->node == b) {
                    assign->node = a;
                }
            }
        }

#if MWCCPSP
        // 3.0.1 (0x5766B0): IRO_UseDef301's uses
        if (data_006242B8 && data_00625304) {
            IROUse301* use;
            for (use = data_006242B8; use && use != data_00625304->globalnext;
                 use = use->globalnext) {
                if (use->node == b) {
                    use->node = a;
                }
            }
        }
#else
        if (IRO_FirstVarUse && IRO_LastVarUse) {
            IROUse* use;
            for (use = IRO_FirstVarUse; use && use != IRO_LastVarUse->globalnext;
                 use = use->globalnext) {
                if (use->node == b) {
                    use->node = a;
                }
            }
        }
#endif

        IRO_NodeTable[b->index] = NULL;
        return a;
    }

    return NULL;
}
