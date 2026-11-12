// IRO_RepositionCode: 3.0.1's code positioning (mwccps2 b210 0x5972F0..0x599A80,
// b145 0x596D60..0x5994F0, mwccpsp 0x5815A0..0x583D30; the three are
// identical function for function, except that the b210/b145 helpers at
// 0x598470 and 0x599180 differ from mwccpsp's in relocations only).
//
// IRO_Optimizer calls it after IRO_BuildFlowgraph when opt_optimizecontrolflow
// and opt_repositioncode are on. It predicts the direction of every two-way
// branch with Ball and Larus's heuristics (loop branch, pointer, call, opcode,
// return, store, loop header, guard; each prediction is dumped as "...
// Heuristic predicts A to B."), lays the flowgraph nodes out so that the
// predicted successor follows its branch (two queues ordered by the old
// layout hold the nodes still to place: the first the unpredicted ones, the
// second the successors predicted against), and relinks the linears in that
// order, adding a label and a goto where a fallthrough is lost and dropping
// a goto that now jumps to the next node.
#include "compiler/common.h"
#include "compiler/BitVector.h"
#include "compiler/CError.h"
#include "compiler/CFunc.h"
#include "compiler/enode.h"
#include "compiler/IroDump.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroLoop301.h"
#include "compiler/IroMalloc.h"
#include "compiler/IrOptimizer.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"
#include "compiler/objects.h"
#include "compiler/types.h"
#include <string.h>

// IroLoop.c (3.0.1)
extern IRONaturalLoop* IRO_FindLoopHeaders301(void);
extern void func_00589A80(IRONaturalLoop* loops); // free the loop records
// IroUtil.c (3.0.1): the semantic expression compare
extern Boolean func_00555190(IROLinear* a, IROLinear* b);

// 3.0.1's BitVector.h tests a bit with an inline function (its out-of-line
// copy is IroBitVect.c's Bv_IsBitSet301)
HEADER_INLINE Boolean Bv_IsBitSet301(UInt32 bit, BitVector* bv) {
    int result = 0;

    if ((bit >> 5) < bv->size && (bv->data[bit >> 5] & (1 << (bit & 31)))) {
        result = 1;
    }
    return result;
}

#define RC_FREE(p) \
    if (p) { \
        IRO_free(p); \
        (p) = NULL; \
    }

// what the heuristics know about a node (0x10 bytes, by node index)
typedef struct RCNodeInfo {
    BitVector* pdom; // +0: the nodes that postdominate it
    BitVector* loops; // +4: the loops it belongs to (by loop number)
    Boolean isLoopHeader; // +8
    Boolean noPredict; // +9: the header of a recognized loop (its test is at the top)
    Boolean hasCall; // +0xA
    Boolean hasReturn; // +0xB: it ends in a return
    Boolean inQueue; // +0xC: in the queue of unpredicted nodes
    Boolean inPredQueue; // +0xD: in the queue of nodes predicted against
} RCNodeInfo;

typedef struct RCTable {
    UInt16 num; // +0
    UInt16 numloops; // +2
    RCNodeInfo* info; // +4
} RCTable;

// the work queues: a list kept in the order of the original layout, its
// entries at the node's original position in a table twice the node count
typedef struct RCEntry {
    IRONode* node;
    struct RCEntry* next;
    struct RCEntry* prev;
} RCEntry;

typedef struct RCQueue {
    RCEntry* head; // +0
    RCEntry* tail; // +4
    int size; // +8
    RCEntry* entries; // +0xC
    UInt16* position; // +0x10: a node's 1-based position in the original layout, by index
} RCQueue;

static void RepositionLinears(IRONode** order);
static void FixFallthrough(IRONode* node, IRONode* oldnext, IRONode* newnext);
static void ComputeNodeOrder(IRONode** order);
static int OrderNodes(
    IRONode* node, RCQueue* queue, RCQueue* predQueue, RCTable* tbl, IRONode** order, int count
);
static IRONode* PredictBranch(IRONode* node, RCTable* tbl, IRONode** other);
static IRONode* PointerHeuristic(IRONode* node, RCTable* tbl, IRONode** other);
static IRONode* StoreHeuristic(IRONode* node, RCTable* tbl, IRONode** other);
static IRONode* GuardHeuristic(IRONode* node, RCTable* tbl, IRONode** other);
static void GuardCollectVars(IROLinear* linear, Boolean isEntry, BitVector* used);
static IRONode* CallHeuristic(IRONode* node, RCTable* tbl, IRONode** other);
static IRONode* LoopHeuristic(IRONode* node, RCTable* tbl, IRONode** other);
static IRONode* OpcodeHeuristic(IRONode* node, RCTable* tbl, IRONode** other);
static IRONode* LoopBranchHeuristic(IRONode* node, RCTable* tbl, IRONode** other);
static void InitBranchInfo(RCTable* tbl, UInt16 num);
static void MarkLoopBody(IRONaturalLoop* loop, RCTable* tbl, UInt16 loopnum);
static void MarkLoopBodyPreds(IRONode* node, BitVector* body, RCTable* tbl, UInt16 loopnum);
static void ComputePostDominators(RCTable* tbl);
static void QueueInsert(RCQueue* queue, IRONode* node);

// b210 0x58BD40 (mwccpsp 0x5760B0): is b a successor of a?
__declspec(noinline) Boolean IsSucc(IRONode* a, IRONode* b) {
    UInt16 i;

    for (i = 0; i < a->numsucc; i++) {
        if (b->index == a->succ[i]) {
            return TRUE;
        }
    }
    return FALSE;
}

static inline Boolean RepositionAllowed(void) {
    IROLinear* linear;

    for (linear = IRO_FirstLinear; linear; linear = linear->next) {
        if (linear->type == IROLinearAsm && IRO_GetAsmEffectsHook && linear->u.asm_stmt) {
            IROAsmEffects effects = IRO_EmptyAsmEffects;
            IRO_GetAsmEffectsHook(linear->u.asm_stmt, &effects);
            if (effects.flag7) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

// b210 0x5972F0
void IRO_RepositionCode(void) {
    IRONode** order;
    size_t size;

    if (RepositionAllowed()) {
        size = IRO_NumNodes * 2 * sizeof(IRONode*);
        order = IRO_malloc(size);
        memset(order, 0, size);
        ComputeNodeOrder(order);
        RepositionLinears(order);
        IRO_free(order);
    }
}

// b210 0x5973D0: chain the linears of the nodes in the new order
static void RepositionLinears(IRONode** order) {
    int i;
    IRONode* node;
    IROLinear* last;

    for (i = 0; (node = order[i]); i++) {
        FixFallthrough(node, node->nextnode, order[i + 1]);
    }

    if ((node = order[0])) {
        IRO_FirstLinear = node->first;
        last = node->last;
        for (i = 1; (node = order[i]); i++) {
            last->next = node->first;
            last = node->last;
        }
        if (last) {
            last->next = NULL;
        }
        IRO_LastLinear = last;
    } else {
        IRO_FirstLinear = NULL;
        IRO_LastLinear = NULL;
    }

    IRO_BuildFlowgraph(IRO_FirstLinear);
}

// b210 0x597470: node no longer falls into oldnext but into newnext
static void FixFallthrough(IRONode* node, IRONode* oldnext, IRONode* newnext) {
    CLabel* label;
    IROLinear* linear;

    if (!oldnext || !IsSucc(node, oldnext)) {
        oldnext = NULL;
    }

    if (oldnext && oldnext != newnext) {
        if (oldnext->first && oldnext->first->type != IROLinearLabel) {
            label = IRO_NewLabel();
            label->stmt = (Statement*)oldnext;
            linear = IRO_NewLinear(IROLinearLabel);
            linear->index = IRO_NumLinear++;
            linear->u.label.label = label;
            linear->flags |= IROLF_StartsNode;
            IRO_Paste(linear, linear, oldnext->first);
        }
        linear = IRO_NewLinear(IROLinearGoto);
        linear->index = IRO_NumLinear++;
        linear->u.label.label = oldnext->first->u.label.label;
        IRO_PasteAfter(linear, linear, node->last);
    }

    if (!newnext || !IsSucc(node, newnext)) {
        newnext = NULL;
    }

    if (newnext &&
        node->last &&
        node->last->type == IROLinearGoto &&
        newnext->first &&
        newnext->first->type == IROLinearLabel &&
        node->last->u.label.label == newnext->first->u.label.label) {
        if (newnext->numpred == 1) {
            IRO_NopOut(newnext->first);
        }
        IRO_NopOut(node->last);
    }
}

static inline int AddNode(IRONode* node, IRONode** order, int count) {
    if (!node->marked) {
        order[count++] = node;
        node->marked = TRUE;
    }
    return count;
}

static inline IRONode* QueueRemove(RCQueue* queue) {
    RCEntry* entry;
    IRONode* node;

    if ((entry = queue->head)) {
        queue->head = entry->next;
        if (queue->head) {
            queue->head->prev = NULL;
        } else {
            queue->tail = NULL;
        }
        node = entry->node;
        entry->node = NULL;
        entry->next = NULL;
        entry->prev = NULL;
        return node;
    }
    return NULL;
}

static inline void QueueFree(RCQueue* queue) {
    queue->position = NULL;
    IRO_free(queue->entries);
    queue->entries = NULL;
    queue->size = 0;
    queue->tail = NULL;
    queue->head = NULL;
}

// b210 0x597580
static void ComputeNodeOrder(IRONode** order) {
    RCTable tbl;
    UInt16* position;
    int size;
    RCQueue* predQueue;
    RCQueue* queue;
    IRONode* node;
    int i;
    int count;

    size = IRO_NumNodes * 2;
    position = IRO_malloc(size * sizeof(UInt16));
    memset(position, 0, size * sizeof(UInt16));
    queue = IRO_malloc(sizeof(RCQueue));
    predQueue = IRO_malloc(sizeof(RCQueue));

    for (node = IRO_FirstNode, i = 1; node; node = node->nextnode, i++) {
        position[node->index] = i;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = size;
    queue->entries = IRO_malloc(size * sizeof(RCEntry));
    memset(queue->entries, 0, size * sizeof(RCEntry));
    queue->position = position;

    predQueue->head = NULL;
    predQueue->tail = NULL;
    predQueue->size = size;
    predQueue->entries = IRO_malloc(size * sizeof(RCEntry));
    memset(predQueue->entries, 0, size * sizeof(RCEntry));
    predQueue->position = position;

    InitBranchInfo(&tbl, size);

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        node->marked = FALSE;
    }

    count = OrderNodes(IRO_FirstNode, queue, predQueue, &tbl, order, 0);

    for (size = 0; (UInt16)size < tbl.num; size++) {
        if (tbl.info[size].pdom) {
            RC_FREE(tbl.info[size].pdom);
        }
        if (tbl.info[size].loops) {
            RC_FREE(tbl.info[size].loops);
        }
    }
    IRO_free(tbl.info);
    tbl.info = NULL;
    tbl.num = 0;

    QueueFree(predQueue);
    QueueFree(queue);
    IRO_free(predQueue);
    IRO_free(queue);
    IRO_free(position);

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (!node->marked) {
            count = AddNode(node, order, count);
        }
    }

    IRO_Dump("%s\n", "new node ordering: ");
    for (i = 0; i < IRO_NumNodes * 2; i++) {
        if (order[i]) {
            IRO_Dump(" %5ld\n", order[i]->index);
        }
    }
    IRO_Dump("\n");
}

#define RC_INFO(tbl, node) ((tbl)->info[(node)->index])
#define RC_QUEUED(tbl, node) (RC_INFO(tbl, node).inQueue || RC_INFO(tbl, node).inPredQueue)

// b210 0x597880: lay out the nodes from node on; returns the new count
static int OrderNodes(
    IRONode* node, RCQueue* queue, RCQueue* predQueue, RCTable* tbl, IRONode** order, int count
) {
    IRONode* only;
    IRONode* other;
    IRONode* succ;
    IRONode* next;
    IRONode* predicted;
    UInt16 numsucc;
    UInt16 numDominated;
    UInt16 i;
    UInt16 numpred;

    while (node) {
        numDominated = 0;
        only = NULL;
        numsucc = node->numsucc;
        for (i = 0; i < numsucc; i++) {
            succ = IRO_NodeTable[node->succ[i]];
            if (!succ->marked && Bv_IsBitSet301(node->index, succ->dom)) {
                only = succ;
                numDominated++;
            }
        }

        if (!RC_INFO(tbl, node).noPredict && numDominated == 0) {
            count = AddNode(node, order, count);
            for (i = 0; i < node->numsucc; i++) {
                succ = IRO_NodeTable[node->succ[i]];
                if (succ && !succ->marked && !RC_QUEUED(tbl, succ)) {
                    QueueInsert(queue, succ);
                    RC_INFO(tbl, succ).inQueue = TRUE;
                }
            }
            for (i = 0; i < node->numsucc; i++) {
                succ = IRO_NodeTable[node->succ[i]];
                if (!succ->marked) {
                    only = succ;
                    numDominated++;
                }
            }
            if (numDominated == 1) {
                numpred = 0;
                for (i = 0; i < only->numpred; i++) {
                    if (!IRO_NodeTable[only->pred[i]]->marked) {
                        numpred++;
                    }
                }
                if (!numpred) {
                    node = only;
                    continue;
                }
            }
        } else if (!RC_INFO(tbl, node).noPredict && numDominated == 1) {
            count = AddNode(node, order, count);
            for (i = 0; i < node->numsucc; i++) {
                succ = IRO_NodeTable[node->succ[i]];
                if (succ != only && !succ->marked && !RC_QUEUED(tbl, succ)) {
                    QueueInsert(queue, succ);
                    RC_INFO(tbl, succ).inQueue = TRUE;
                }
            }
            if (!only->marked) {
                node = only;
                continue;
            }
        } else if (!RC_INFO(tbl, node).noPredict &&
                   (predicted = PredictBranch(node, tbl, &other))) {
            if (node != predicted) {
                if (!other->marked && !RC_QUEUED(tbl, other)) {
                    QueueInsert(predQueue, other);
                    RC_INFO(tbl, other).inPredQueue = TRUE;
                }
                count = AddNode(node, order, count);
                for (i = 0; i < node->numsucc; i++) {
                    succ = IRO_NodeTable[node->succ[i]];
                    if (succ != predicted && !succ->marked && !RC_QUEUED(tbl, succ)) {
                        QueueInsert(queue, succ);
                        RC_INFO(tbl, succ).inQueue = TRUE;
                    }
                }
                if (!predicted->marked) {
                    node = predicted;
                    continue;
                }
            } else {
                count = AddNode(node, order, count);
                if (!other->marked) {
                    node = other;
                    continue;
                }
            }
        } else {
            if (!RC_QUEUED(tbl, node)) {
                QueueInsert(queue, node);
                RC_INFO(tbl, node).inQueue = TRUE;
            }
            for (i = 0; i < node->numsucc; i++) {
                succ = IRO_NodeTable[node->succ[i]];
                if (succ && !succ->marked && !RC_QUEUED(tbl, succ)) {
                    QueueInsert(queue, succ);
                    RC_INFO(tbl, succ).inQueue = TRUE;
                }
            }
        }

        do {
            next = QueueRemove(queue);
            if (next) {
                RC_INFO(tbl, next).inQueue = FALSE;
            }
        } while (next && next->marked == TRUE);

        if (next) {
            if (node == next) {
                count = AddNode(node, order, count);
                for (i = 0; i < node->numsucc; i++) {
                    succ = IRO_NodeTable[node->succ[i]];
                    if (succ && !succ->marked && !RC_QUEUED(tbl, succ)) {
                        QueueInsert(queue, succ);
                        RC_INFO(tbl, succ).inQueue = TRUE;
                    }
                }
                next = QueueRemove(queue);
                if (next) {
                    RC_INFO(tbl, next).inQueue = FALSE;
                }
            }
            node = next;
        } else {
            do {
                next = QueueRemove(predQueue);
                if (next) {
                    RC_INFO(tbl, next).inPredQueue = FALSE;
                }
            } while (next && next->marked == TRUE);

            if (node == next) {
                count = AddNode(node, order, count);
                for (i = 0; i < node->numsucc; i++) {
                    succ = IRO_NodeTable[node->succ[i]];
                    if (succ && !succ->marked && !RC_QUEUED(tbl, succ)) {
                        QueueInsert(queue, succ);
                        RC_INFO(tbl, succ).inQueue = TRUE;
                    }
                }
                next = QueueRemove(predQueue);
                if (next) {
                    RC_INFO(tbl, next).inPredQueue = FALSE;
                }
            }
            node = next;
        }
    }

    return count;
}

// the return heuristic: a successor that returns (or leads straight to a
// return) is predicted not taken
static inline Boolean LeadsToReturn(IRONode* node, RCTable* tbl) {
    IRONode* succ;
    Boolean result = TRUE;

    if (!RC_INFO(tbl, node).hasReturn &&
        !(node->numsucc == 1 &&
          (succ = IRO_NodeTable[node->succ[0]]) &&
          RC_INFO(tbl, succ).hasReturn)) {
        result = FALSE;
    }
    return result;
}

static inline IRONode* ReturnHeuristic(IRONode* node, RCTable* tbl, IRONode** other) {
    IRONode* succ0;
    IRONode* succ1;
    Boolean ret0;
    Boolean ret1;

    if (node->numsucc == 2) {
        succ0 = IRO_NodeTable[node->succ[0]];
        succ1 = IRO_NodeTable[node->succ[1]];
        ret0 = LeadsToReturn(succ0, tbl);
        ret1 = LeadsToReturn(succ1, tbl);
        if (ret0 != ret1) {
            if (ret0) {
                *other = succ0;
                return succ1;
            }
            *other = succ1;
            return succ0;
        }
    }
    return NULL;
}

// b210 0x598000: the first heuristic that predicts the branch at the end of
// node wins; returns the predicted successor and the other one in *other
static IRONode* PredictBranch(IRONode* node, RCTable* tbl, IRONode** other) {
    IRONode* result;

    *other = NULL;
    if ((result = LoopBranchHeuristic(node, tbl, other))) {
        IRO_Dump("Loop Branch Heuristic predicts %ld to %ld.\n", node->index, result->index);
        return result;
    }
    if ((result = PointerHeuristic(node, tbl, other))) {
        IRO_Dump("Pointer Heuristic predicts %ld to %ld.\n", node->index, result->index);
        return result;
    }
    if ((result = CallHeuristic(node, tbl, other))) {
        IRO_Dump("Call Heuristic predicts %ld to %ld.\n", node->index, result->index);
        return result;
    }
    if ((result = OpcodeHeuristic(node, tbl, other))) {
        IRO_Dump("Opcode Heuristic predicts %ld to %ld.\n", node->index, result->index);
        return result;
    }
    if ((result = ReturnHeuristic(node, tbl, other))) {
        IRO_Dump("Return Heuristic predicts %ld to %ld.\n", node->index, result->index);
        return result;
    }
    if ((result = StoreHeuristic(node, tbl, other))) {
        IRO_Dump("Store Heuristic predicts %ld to %ld.\n", node->index, result->index);
        return result;
    }
    if ((result = LoopHeuristic(node, tbl, other))) {
        IRO_Dump("Loop Heuristic predicts %ld to %ld.\n", node->index, result->index);
        return result;
    }
    if ((result = GuardHeuristic(node, tbl, other))) {
        IRO_Dump("Guard Heuristic predicts %ld to %ld.\n", node->index, result->index);
        return result;
    }
    return NULL;
}

// b210 0x598260: a pointer compared with NULL or with another pointer is
// predicted not null / not equal
static IRONode* PointerHeuristic(IRONode* node, RCTable* tbl, IRONode** other) {
    IROLinear* last;
    IROLinear* cond;
    IROLinear* left;
    IROLinear* right;
    IRONode* fall;
    IRONode* target;
    UInt8 type;
    UInt8 op;

    last = node->last;
    if (!last) {
        return NULL;
    }
    type = last->type;
    if (type != IROLinearIf && type != IROLinearIfNot) {
        return NULL;
    }

    cond = last->u.label.cond;
    op = cond->nodetype;
    fall = node->nextnode;
    if (node->succ[0] == fall->index) {
        target = IRO_NodeTable[node->succ[1]];
    } else {
        target = IRO_NodeTable[node->succ[0]];
    }

    if (cond->type == IROLinearOp2Arg) {
        left = cond->u.diadic.left;
        right = cond->u.diadic.right;
        if (left->rtype->type != TYPEPOINTER && right->rtype->type != TYPEPOINTER) {
            return NULL;
        }
        if (func_00555190(left, right) && !IRO_HasSideEffect(left) && !IRO_HasSideEffect(right)) {
            if (op == EEQU) {
                if (type == IROLinearIf) {
                    *other = fall;
                    return target;
                }
                *other = target;
                return fall;
            }
            if (op != ENOTEQU) {
                return NULL;
            }
            if (type == IROLinearIf) {
                *other = target;
                return fall;
            }
            *other = fall;
            return target;
        }
        if (op == EEQU) {
            if (type == IROLinearIf) {
                *other = target;
                return fall;
            }
            *other = fall;
            return target;
        }
        if (op != ENOTEQU) {
            return NULL;
        }
        if (type == IROLinearIf) {
            *other = fall;
            return target;
        }
        *other = target;
        return fall;
    }

    if (cond->type != IROLinearOp1Arg) {
        return NULL;
    }
    if (op == ELOGNOT &&
        cond->u.monadic &&
        cond->u.monadic->rtype->type == TYPEPOINTER &&
        IRO_IsVariable(cond->u.monadic)) {
        if (type == IROLinearIf) {
            *other = target;
            return fall;
        }
        *other = fall;
        return target;
    }
    if (cond->rtype->type == TYPEPOINTER && IRO_IsVariable(cond)) {
        if (type == IROLinearIf) {
            *other = fall;
            return target;
        }
        *other = target;
        return fall;
    }
    return NULL;
}

// does the node store to memory (an assignment to anything but a local
// register candidate)?
static inline Boolean NodeStores(IRONode* node) {
    IROLinear* linear;
    Object* obj;
    Boolean result = FALSE;

    if (node->first && node->last) {
        for (linear = node->first; linear != node->last->next; linear = linear->next) {
            if (IRO_IsAssignment(linear) && linear->u.monadic) {
                obj = IRO_IsVariable(linear->u.monadic);
                if (!obj ||
                    obj->datatype != DLOCAL ||
                    !obj->u.var.info ||
                    obj->u.var.info->noregister) {
                    result = TRUE;
                    break;
                }
            }
        }
    }
    return result;
}

// b210 0x598470: a successor that stores (and does not postdominate) is
// predicted not taken
static IRONode* StoreHeuristic(IRONode* node, RCTable* tbl, IRONode** other) {
    IRONode* succ0;
    IRONode* succ1;
    BitVector* pdom;
    Boolean pdom0;
    Boolean pdom1;
    Boolean store0;
    Boolean store1;

    if (node->numsucc != 2) {
        return NULL;
    }
    succ0 = IRO_NodeTable[node->succ[0]];
    succ1 = IRO_NodeTable[node->succ[1]];
    pdom = RC_INFO(tbl, node).pdom;
    pdom0 = Bv_IsBitSet301(succ0->index, pdom);
    pdom1 = Bv_IsBitSet(succ1->index, pdom);
    store0 = NodeStores(succ0);
    store1 = NodeStores(succ1);

    if (!pdom0 && store0) {
        if (pdom1 || !store1) {
            *other = succ0;
            return succ1;
        }
    } else if (!pdom1 && store1) {
        *other = succ1;
        return succ0;
    }
    return NULL;
}

// does the node use a variable of the condition (in used) before it
// assigns it?
static inline Boolean NodeUsesBeforeDef(IRONode* node, BitVector* used) {
    IROLinear* linear;
    Object* obj;
    UInt32 bit;
    Boolean result = FALSE;

    if (node->first && node->last) {
        for (linear = node->first; linear != node->last->next; linear = linear->next) {
            IRO_GetKills(linear);
            if ((obj = IRO_IsVariable(linear)) &&
                (!(linear->flags & IROLF_Assigned) || (linear->flags & IROLF_Used)) &&
                obj->varptr) {
                bit = obj->varptr->index;
                if (Bv_IsBitSet(bit, used) && !Bv_IsBitSet(bit, IRO_VarKills)) {
                    result = TRUE;
                    break;
                }
            }
        }
    }
    return result;
}

// b210 0x598630: a successor that uses an operand of the condition before
// defining it (and does not postdominate) is predicted taken
static IRONode* GuardHeuristic(IRONode* node, RCTable* tbl, IRONode** other) {
    IROLinear* last;
    IROLinear* cond;
    IRONode* succ0;
    IRONode* succ1;
    BitVector* pdom;
    BitVector* used;
    BitVector* saveKills;
    Boolean pdom0;
    Boolean pdom1;
    Boolean uses0;
    Boolean uses1;

    last = node->last;
    if (!last) {
        return NULL;
    }
    if (last->type != IROLinearIf && last->type != IROLinearIfNot) {
        return NULL;
    }

    cond = last->u.label.cond;
    succ0 = IRO_NodeTable[node->succ[0]];
    succ1 = IRO_NodeTable[node->succ[1]];
    pdom = RC_INFO(tbl, node).pdom;
    pdom0 = Bv_IsBitSet301(succ0->index, pdom);
    pdom1 = Bv_IsBitSet301(succ1->index, pdom);

    Bv_AllocVector(&used, IRO_NumVars + 1);
    saveKills = IRO_VarKills;
    Bv_AllocVector(&IRO_VarKills, IRO_NumVars + 1);
#if MWCCPS2
    IRO_WalkTree_ps2(cond, (IROWalkTreeArgFunc_ps2)GuardCollectVars, used);
#else
    (IRO_WalkTree)(cond, (IROWalkTreeFunc)GuardCollectVars, used);
#endif

    uses0 = NodeUsesBeforeDef(succ0, used);
    Bv_Clear(IRO_VarKills);
    uses1 = NodeUsesBeforeDef(succ1, used);

    RC_FREE(IRO_VarKills);
    IRO_VarKills = saveKills;
    RC_FREE(used);

    if (!pdom0 && uses0) {
        if (pdom1 || !uses1) {
            *other = succ1;
            return succ0;
        }
    } else if (!pdom1 && uses1) {
        *other = succ0;
        return succ1;
    }
    return NULL;
}

// b210 0x598970: collect the variables an expression uses
static void GuardCollectVars(IROLinear* linear, Boolean isEntry, BitVector* used) {
    Object* obj;

    if (isEntry &&
        (obj = IRO_IsVariable(linear)) &&
        (!(linear->flags & IROLF_Assigned) || (linear->flags & IROLF_Used)) &&
        obj->varptr) {
        Bv_SetBit(obj->varptr->index, used);
    }
}

// the node has flag set, or its only successor has it and the node dominates
// it (so it always leads there)
#define RC_LEADS_TO(tbl, node, flag, result) \
    do { \
        IRONode* _succ; \
        Boolean _dom = FALSE; \
        (result) = TRUE; \
        if (!RC_INFO(tbl, node).flag) { \
            Boolean _found = FALSE; \
            if ((node)->numsucc == 1 && \
                (_succ = IRO_NodeTable[(node)->succ[0]]) && \
                Bv_IsBitSet301((node)->index, _succ->dom)) { \
                _dom = TRUE; \
            } \
            if (_dom && RC_INFO(tbl, _succ).flag) { \
                _found = TRUE; \
            } \
            if (!_found) { \
                (result) = FALSE; \
            } \
        } \
    } while (0)

// b210 0x5989E0: a successor that calls (and does not postdominate) is
// predicted not taken
static IRONode* CallHeuristic(IRONode* node, RCTable* tbl, IRONode** other) {
    IRONode* succ0;
    IRONode* succ1;
    Boolean call0;
    Boolean call1;

    if (node->numsucc != 2) {
        return NULL;
    }
    succ0 = IRO_NodeTable[node->succ[0]];
    succ1 = IRO_NodeTable[node->succ[1]];
    RC_LEADS_TO(tbl, succ0, hasCall, call0);
    RC_LEADS_TO(tbl, succ1, hasCall, call1);
    if (call0 == call1) {
        return NULL;
    }
    if (call0) {
        if (!Bv_IsBitSet(succ0->index, RC_INFO(tbl, node).pdom)) {
            *other = succ0;
            return succ1;
        }
    } else {
        if (!Bv_IsBitSet(succ1->index, RC_INFO(tbl, node).pdom)) {
            *other = succ1;
            return succ0;
        }
    }
    return NULL;
}

// b210 0x598C30: a successor that is a loop header (or leads straight to
// one) and does not postdominate is predicted taken
static IRONode* LoopHeuristic(IRONode* node, RCTable* tbl, IRONode** other) {
    IRONode* succ0;
    IRONode* succ1;
    BitVector* pdom;
    Boolean pdom0;
    Boolean pdom1;
    Boolean head0;
    Boolean head1;

    if (node->numsucc != 2) {
        return NULL;
    }
    succ0 = IRO_NodeTable[node->succ[0]];
    succ1 = IRO_NodeTable[node->succ[1]];
    pdom = RC_INFO(tbl, node).pdom;
    pdom0 = Bv_IsBitSet301(succ0->index, pdom);
    pdom1 = Bv_IsBitSet301(succ1->index, pdom);
    RC_LEADS_TO(tbl, succ0, isLoopHeader, head0);
    RC_LEADS_TO(tbl, succ1, isLoopHeader, head1);

    if (!pdom0 && head0) {
        if (pdom1 || !head1) {
            *other = succ1;
            return succ0;
        }
    } else if (!pdom1 && head1) {
        *other = succ0;
        return succ1;
    }
    return NULL;
}

// b210 0x598E80: an integer compared below zero, or two floats compared for
// equality, is predicted false
static IRONode* OpcodeHeuristic(IRONode* node, RCTable* tbl, IRONode** other) {
    IROLinear* last;
    IROLinear* cond;
    IROLinear* left;
    IROLinear* right;
    IRONode* fall;
    IRONode* target;
    UInt8 type;
    UInt8 op;
    Boolean zeroRight;
    Boolean zero;

    last = node->last;
    if (!last) {
        return NULL;
    }
    type = last->type;
    if (type != IROLinearIf && type != IROLinearIfNot) {
        return NULL;
    }
    cond = last->u.label.cond;
    if (cond->type != IROLinearOp2Arg) {
        return NULL;
    }
    op = cond->nodetype;
    left = cond->u.diadic.left;
    right = cond->u.diadic.right;
    fall = node->nextnode;
    if (node->succ[0] == fall->index) {
        target = IRO_NodeTable[node->succ[1]];
    } else {
        target = IRO_NodeTable[node->succ[0]];
    }

    if (left->rtype->type == TYPEINT &&
        TYPE_INTEGRAL(left->rtype)->integral < IT_17 &&
        right->rtype->type == TYPEINT &&
        TYPE_INTEGRAL(right->rtype)->integral < IT_17) {
        zeroRight = IRO_IsConstantZero(right);
        if ((zero = IRO_IsConstantZero(left)) != zeroRight) {
            switch (op) {
                case ELESS:
                case ELESSEQU:
                    if ((type == IROLinearIf) == !zero) {
                        *other = target;
                        return fall;
                    }
                    *other = fall;
                    return target;
                case EGREATER:
                case EGREATEREQU:
                    if ((type == IROLinearIf) == !zero) {
                        *other = fall;
                        return target;
                    }
                    *other = target;
                    return fall;
                default:
                    return NULL;
            }
        }
    }

    if (left->rtype->type == TYPEFLOAT && right->rtype->type == TYPEFLOAT) {
        if (op == EEQU) {
            if (type == IROLinearIf) {
                *other = target;
                return fall;
            }
            *other = fall;
            return target;
        }
        if (op == ENOTEQU) {
            if (type == IROLinearIf) {
                *other = fall;
                return target;
            }
            *other = target;
            return fall;
        }
    }
    return NULL;
}

// b210 0x599050: a successor that leaves a loop the other stays in is
// predicted not taken
static IRONode* LoopBranchHeuristic(IRONode* node, RCTable* tbl, IRONode** other) {
    BitVector* loops;
    BitVector* loops0;
    BitVector* loops1;
    BitVector* bv;
    IRONode* succ0;
    IRONode* succ1;
    Boolean exits0;
    Boolean exits1;

    if (node->numsucc != 2) {
        return NULL;
    }
    loops = RC_INFO(tbl, node).loops;
    if (!loops || Bv_IsEmpty(loops)) {
        return NULL;
    }
    succ0 = IRO_NodeTable[node->succ[0]];
    succ1 = IRO_NodeTable[node->succ[1]];
    loops0 = RC_INFO(tbl, succ0).loops;
    loops1 = RC_INFO(tbl, succ1).loops;
    Bv_AllocVector(&bv, tbl->numloops);
    Bv_Copy(loops, bv);
    Bv_Minus(loops0, bv);
    exits0 = !Bv_IsEmpty(bv);
    Bv_Copy(loops, bv);
    Bv_Minus(loops1, bv);
    exits1 = !Bv_IsEmpty(bv);
    RC_FREE(bv);

    if (exits0 == exits1) {
        return NULL;
    }
    if (exits0) {
        *other = succ0;
        return succ1;
    }
    *other = succ1;
    return succ0;
}

// b210 0x599180
static void InitBranchInfo(RCTable* tbl, UInt16 num) {
    IRONaturalLoop* loops;
    IRONaturalLoop* loop;
    IRONode* node;
    IROLinear* linear;
    UInt16 count;
    UInt16 i;

    tbl->num = num;
    tbl->info = IRO_malloc(num * sizeof(RCNodeInfo));
    memset(tbl->info, 0, num * sizeof(RCNodeInfo));
    ComputePostDominators(tbl);

    loops = IRO_FindLoopHeaders301();
    count = 0;
    for (loop = loops; loop; loop = loop->next) {
        count++;
    }
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        Bv_AllocVector(&RC_INFO(tbl, node).loops, count);
    }
    tbl->numloops = count;

    for (loop = loops, i = 0; loop; loop = loop->next, i++) {
        RC_INFO(tbl, loop->header).isLoopHeader = TRUE;
        MarkLoopBody(loop, tbl, i);
        func_00587540(loop);
        func_00587270(loop);
        if (!(loop->flags & LP301_FORM_UNRECOGNIZABLE) &&
            loop->x10 == loop->header &&
            loop->x10 != loop->x20) {
            RC_INFO(tbl, loop->header).noPredict = TRUE;
        }
    }
    func_00589A80(loops);

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->first && node->last) {
            for (linear = node->first; linear != node->last->next; linear = linear->next) {
                if (linear->type == IROLinearFunccall) {
                    RC_INFO(tbl, node).hasCall = TRUE;
                }
            }
            if (node->last->type == IROLinearReturn) {
                RC_INFO(tbl, node).hasReturn = TRUE;
            }
        }
    }
}

// b210 0x5992D0: the loop's body (its header and every node that reaches a
// back edge without passing the header) gets bit loopnum
static void MarkLoopBody(IRONaturalLoop* loop, RCTable* tbl, UInt16 loopnum) {
    BitVector* body;
    IRONode* header;
    IRONode* pred;
    UInt16 i;

    if (loop->nodes) {
        RC_FREE(loop->nodes);
    }
    header = loop->header;
    Bv_AllocVector(&body, IRO_NumNodes + 1);
    Bv_Clear(body);
    Bv_SetBit(header->index, body);
    Bv_SetBit(loopnum, RC_INFO(tbl, header).loops);
    for (i = 0; i < header->numpred; i++) {
        pred = IRO_NodeTable[header->pred[i]];
        if (Bv_IsBitSet301(header->index, pred->dom)) {
            Bv_SetBit(pred->index, body);
            Bv_SetBit(loopnum, RC_INFO(tbl, pred).loops);
            if (pred != header) {
                MarkLoopBodyPreds(pred, body, tbl, loopnum);
            }
        }
    }
    loop->nodes = body;
}

// b210 0x5994E0
static void MarkLoopBodyPreds(IRONode* node, BitVector* body, RCTable* tbl, UInt16 loopnum) {
    IRONode* pred;
    UInt16 i;

    for (i = 0; i < node->numpred; i++) {
        pred = IRO_NodeTable[node->pred[i]];
        if (!Bv_IsBitSet301(pred->index, body)) {
            Bv_SetBit(pred->index, body);
            Bv_SetBit(loopnum, RC_INFO(tbl, pred).loops);
            MarkLoopBodyPreds(pred, body, tbl, loopnum);
        }
    }
}

// b210 0x5996A0: postdominators, from the exits the entry reaches
static void ComputePostDominators(RCTable* tbl) {
    IRONode* node;
    BitVector* bv;
    Boolean changed;
    UInt16 i;

    for (i = 0; i < tbl->num; i++) {
        if (tbl->info[i].pdom) {
            RC_FREE(tbl->info[i].pdom);
        }
    }

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->numsucc == 0 && Bv_IsBitSet(IRO_FirstNode->index, node->dom)) {
            Bv_AllocVector(&RC_INFO(tbl, node).pdom, IRO_NumNodes);
            Bv_SetBit(node->index, RC_INFO(tbl, node).pdom);
        }
    }
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (!RC_INFO(tbl, node).pdom) {
            Bv_AllocVector(&RC_INFO(tbl, node).pdom, IRO_NumNodes);
            Bv_Set(RC_INFO(tbl, node).pdom);
        }
    }

    Bv_AllocVector(&bv, IRO_NumNodes);
    do {
        changed = FALSE;
        for (node = IRO_FirstNode; node; node = node->nextnode) {
            if (node->numsucc) {
                Bv_Set(bv);
                for (i = 0; i < node->numsucc; i++) {
                    Bv_And(RC_INFO(tbl, IRO_NodeTable[node->succ[i]]).pdom, bv);
                }
                Bv_SetBit(node->index, bv);
            } else {
                Bv_Clear(bv);
                Bv_SetBit(node->index, bv);
            }
            if (!Bv_Compare(bv, RC_INFO(tbl, node).pdom)) {
                Bv_Copy(bv, RC_INFO(tbl, node).pdom);
                changed = TRUE;
            }
        }
    } while (changed);
}

// b210 0x599930: put node in the queue, after the nearest queued node that
// comes before it in the original layout (or before the nearest after it)
static void QueueInsert(RCQueue* queue, IRONode* node) {
    RCEntry* entry;
    RCEntry* near;
    UInt16 pos;
    UInt16 dist;
    UInt16 at;
    int dir;

    pos = queue->position[node->index];
    entry = &queue->entries[pos];
    entry->node = node;
    entry->next = NULL;
    entry->prev = NULL;

    near = NULL;
    for (dist = 1; dist < queue->size && !near; dist++) {
        for (dir = -1; dir <= 1; dir += 2) {
            at = pos + dir * dist;
            if (at < queue->size && queue->entries[at].node) {
                near = &queue->entries[at];
                break;
            }
        }
    }

    if (near) {
        if (near < entry) {
            entry->prev = near;
            entry->next = near->next;
            if (entry->next) {
                entry->next->prev = entry;
            } else {
                queue->tail = entry;
            }
            near->next = entry;
        } else {
            entry->next = near;
            entry->prev = near->prev;
            if (entry->prev) {
                entry->prev->next = entry;
            } else {
                queue->head = entry;
            }
            near->prev = entry;
        }
    } else {
        queue->head = entry;
        queue->tail = entry;
    }
}
