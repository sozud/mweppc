// mwccpsp 3.0.1: hoisting loop-invariant assignments out of loops
// (IRO_PullOutInvariantAssignments and its helpers, 0x5920D0..0x5940E0).
// Not in the 2.4.7 source.
//
//     for (i = 0; i < n; i++) { k = a * b; p[i] = k; }
// becomes
//     k = a * b; for (i = 0; i < n; i++) { p[i] = k; }
#include "compiler/common.h"
#include "compiler/BitVector.h"
#include "compiler/CError.h"
#include "compiler/CParser.h"
#include "compiler/enode.h"
#include "compiler/objects.h"
#include "compiler/IroCSE.h"
#include "compiler/IroDump.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroLoop301.h"
#include "compiler/IroMalloc.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"

extern void func_0056AB70(
    IROLinear* linear, Boolean alloc, Boolean stop, Boolean recurse
); // IroCSE.c
extern Type* func_0055A650(Type* type);

// a candidate assignment (0x10 bytes)
typedef struct IROInvariantRec {
    UInt8 done; // +0: moved, or found not movable
    UInt8 invariant; // +1: both sides are loop-invariant
    IROLinear* linear; // +4
    IRONode* node; // +8
    struct IROInvariantRec* next; // +0xC
} IROInvariantRec;

// IroFlowgraph.h's node queue (a ring of IRO_NumNodes indexes)
typedef struct IRONodeQueue {
    UInt16* data;
    UInt16 count;
    UInt16 head;
} IRONodeQueue;

static inline void Enqueue(IRONodeQueue* q, UInt16 index) {
    if (q->count >= IRO_NumNodes) {
        CError_FATAL(127);
    } else {
        q->data[(q->head + q->count) % IRO_NumNodes] = index;
        q->count++;
    }
}

static inline UInt16 Dequeue(IRONodeQueue* q) {
    UInt16 index = 0xFFFF;

    if (q->count) {
        index = q->data[q->head];
        q->head = (q->head + 1) % IRO_NumNodes;
        q->count--;
    }
    return index;
}

// an assignment (not a modify op) of a non-volatile value
static inline Boolean IsPlainAssignment(IROLinear* nd) {
    return IRO_IsAssignment(nd) &&
        !IRO_IsModifyOp[nd->nodetype] &&
        nd->rtype &&
        !CParser_IsVolatile(nd->rtype, nd->nodeflags & ENODE_FLAG_QUALS);
}

// 0x593D60: an assignment to an invariant place whose value reads nothing
// the loop changes and nothing the place's address reads
Boolean func_00593D60(IROLinear* nd, IRONaturalLoop* loop) {
    Boolean result = FALSE;
    Boolean bad;
    IROLinear* left;
    IROLinear* right;
    BitVector* depends;
    VarRecord* var;

    if (IsPlainAssignment(nd)) {
        right = (nd->type == IROLinearOp1Arg) ? NULL : nd->u.diadic.right;
        left = nd->u.diadic.left;
        if (right &&
            left &&
            (left->u.monadic->flags & IROLF_LoopInvariant) &&
            !IRO_HasSideEffect(right) &&
            !IRO_HasSideEffect(left)) {
            func_0056AB70(right, FALSE, FALSE, TRUE);
            depends = IRO_Depends;
            func_0056AB70(left, TRUE, FALSE, TRUE);
            if (!Bv_BitsInCommon(depends, IRO_Depends)) {
                bad = FALSE;
                if (depends->size > 0 && (depends->data[0] & 1)) {
                    bad = TRUE;
                } else if (loop->xC->size > 0 && (loop->xC->data[0] & 1) && !Bv_IsEmpty(depends)) {
                    bad = TRUE;
                } else {
                    for (var = IRO_FirstVar; var; var = var->next) {
                        if (Bv_IsBitSet(var->index, depends) &&
                            Bv_IsBitSet(var->index, loop->xC) &&
                            var->inductionState != 2) {
                            bad = TRUE;
                            break;
                        }
                    }
                }
                result = !bad;
            }
            if (IRO_Depends) {
                IRO_free(IRO_Depends);
            }
            IRO_Depends = depends;
        }
    }
    return result;
}

// 0x593C30: the node's candidate assignments, appended to head/tail
void func_00593C30(
    IRONode* node, IRONaturalLoop* loop, IROInvariantRec** head, IROInvariantRec** tail
) {
    IROLinear* nd;
    IROLinear* left;
    IROLinear* right;
    IROInvariantRec* rec;
    Boolean invariant;

    for (nd = node->first; nd; nd = nd->next) {
        if (!(nd->flags & IROLF_Reffed)) {
            invariant = FALSE;
            if (IsPlainAssignment(nd)) {
                right = (nd->type == IROLinearOp1Arg) ? NULL : nd->u.diadic.right;
                left = nd->u.diadic.left;
                if (right &&
                    left &&
                    (right->flags & IROLF_LoopInvariant) &&
                    (left->u.monadic->flags & IROLF_LoopInvariant) &&
                    !IRO_HasSideEffect(right) &&
                    !IRO_HasSideEffect(left)) {
                    invariant = TRUE;
                }
            }
            if (invariant || func_00593D60(nd, loop)) {
                rec = IRO_malloc(sizeof(IROInvariantRec));
                rec->done = FALSE;
                rec->invariant = invariant;
                rec->linear = nd;
                rec->node = node;
                rec->next = NULL;
                nd->flags |= IROLF_Unk200000;
                if (*tail) {
                    (*tail)->next = rec;
                } else {
                    *head = rec;
                }
                *tail = rec;
            }
        }
        if (nd == node->last) {
            break;
        }
    }
}

// 0x593A90: breadth first through the loop from the queued nodes,
// collecting the candidates of the nodes every pass reaches
void func_00593A90(
    IRONodeQueue* q, IRONaturalLoop* loop, IROInvariantRec** head, IROInvariantRec** tail
) {
    IRONode* node;
    IRONode* succ;
    UInt16 i;

    while (q->count) {
        node = IRO_NodeTable[Dequeue(q)];
        if (node->mustreach1) {
            func_00593C30(node, loop, head, tail);
        }
        for (i = 0; i < node->numsucc; i++) {
            succ = IRO_NodeTable[node->succ[i]];
            if (Bv_IsBitSet(succ->index, loop->nodes) && !succ->marked) {
                Enqueue(q, succ->index);
                succ->marked = TRUE;
            }
        }
    }
}

// 0x5939A0: the loop's candidate assignments, from its header on
void func_005939A0(IRONaturalLoop* loop, IROInvariantRec** head, IROInvariantRec** tail) {
    IRONode* node;
    IRONodeQueue q;

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        node->marked = FALSE;
    }
    Bv_AllocVector(&IRO_Depends, IRO_NumVars + 1);
    q.data = IRO_malloc(IRO_NumNodes * sizeof(UInt16));
    q.count = 0;
    q.head = 0;
    Enqueue(&q, loop->header->index);
    loop->header->marked = TRUE;
    func_00593A90(&q, loop, head, tail);
    IRO_free(q.data);
    if (IRO_Depends) {
        IRO_free(IRO_Depends);
        IRO_Depends = NULL;
    }
}

// 0x593920: the candidates not done, by invariant and by whether their node
// runs on every iteration (unk3F)
void func_00593920(
    IROInvariantRec* list, int* invEvery, int* invSome, int* otherEvery, int* otherSome
) {
    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;

    for (; list; list = list->next) {
        if (!list->done) {
            if (list->invariant) {
                if (list->node->unk3F) {
                    a++;
                } else {
                    b++;
                }
            } else if (list->node->unk3F) {
                c++;
            } else {
                d++;
            }
        }
    }
    *invEvery = a;
    *invSome = b;
    *otherEvery = c;
    *otherSome = d;
}

// --- what a candidate kills, against the rest of the loop ------------------

extern void func_0056A2E0(IROLinear* linear, Boolean isEntry); // IroVars.c: kills of one linear

// clear the nodes' marks and queue the loop header
static inline void StartQueue(IRONodeQueue* q, IRONaturalLoop* loop) {
    IRONode* node;

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        node->marked = FALSE;
    }
    q->data = IRO_malloc(IRO_NumNodes * sizeof(UInt16));
    q->count = 0;
    q->head = 0;
    Enqueue(q, loop->header->index);
    loop->header->marked = TRUE;
}

// queue node's unmarked successors in the loop
static inline void QueueSuccessors(IRONodeQueue* q, IRONaturalLoop* loop, IRONode* node) {
    IRONode* succ;
    UInt16 i;

    for (i = 0; i < node->numsucc; i++) {
        succ = IRO_NodeTable[node->succ[i]];
        if (Bv_IsBitSet(succ->index, loop->nodes) && !succ->marked) {
            Enqueue(q, succ->index);
            succ->marked = TRUE;
        }
    }
}

// 0x5930A0: walker callback, the kills of a statement's expression
void func_005930A0(IROLinear* nd, Boolean isFirst, void* arg) {
    if (isFirst) {
        IRO_WalkTree(nd, func_0056A2E0);
    }
}

// 0x592FF0: a statement of node (not the candidate or another candidate)
// kills what the candidate kills
Boolean func_00592FF0(IRONode* node, IROInvariantRec* rec, BitVector* kills) {
    IROLinear* nd;

    for (nd = node->first; nd; nd = nd->next) {
        if (!(nd->flags & IROLF_Reffed) && !(nd->flags & IROLF_Unk200000) && nd != rec->linear) {
            if (nd->type == IROLinearAsm) {
                IRO_WalkTree(nd, func_0056A2E0);
            } else {
                func_00552EE0(nd, func_005930A0, NULL);
            }
            if (Bv_BitsInCommon(IRO_VarKills, kills)) {
                return TRUE;
            }
        }
        if (nd == node->last) {
            break;
        }
    }
    return FALSE;
}

// 0x592E80
Boolean func_00592E80(
    IRONodeQueue* q, IROInvariantRec* rec, IRONaturalLoop* loop, BitVector* kills
) {
    IRONode* node;
    Boolean found = FALSE;

    while (q->count && !found) {
        node = IRO_NodeTable[Dequeue(q)];
        found = func_00592FF0(node, rec, kills);
        QueueSuccessors(q, loop, node);
    }
    return found;
}

// --- what the loop reads of what a candidate kills ---------------------------

// the linear only writes its target (its value is not used)
static inline Boolean WritesOnly(IROLinear* nd) {
    return (IRO_IsAssignment(nd) && nd->u.monadic && !(nd->u.monadic->flags & IROLF_Used)) ||
        ((nd->flags & IROLF_Assigned) && !(nd->flags & IROLF_Used));
}

void func_005938A0(IROLinear* nd, Boolean isFirst, void* arg);

// the variables nd reads, added to used
static inline void AddUses(IROLinear* nd, BitVector* used) {
    if (WritesOnly(nd)) {
        func_00552F00(nd, func_005938A0, used);
    } else {
        func_0056AB70(nd, FALSE, FALSE, TRUE);
        Bv_Or(IRO_Depends, used);
    }
}

// 0x5938A0: walker callback, the variables an expression reads
void func_005938A0(IROLinear* nd, Boolean isFirst, void* arg) {
    if (isFirst) {
        AddUses(nd, arg);
    }
}

// the statement nd reads what kills holds
static inline Boolean ReadsKills(IROLinear* nd, BitVector* kills, BitVector* used) {
    if (nd->type == IROLinearAsm) {
        AddUses(nd, used);
    } else {
        func_00552EE0(nd, func_005938A0, used);
    }
    return Bv_BitsInCommon(used, kills);
}

// 0x593370: a statement of node other than the candidate reads what it kills
Boolean func_00593370(IRONode* node, IROInvariantRec* rec, BitVector* kills, BitVector* used) {
    IROLinear* nd;

    for (nd = node->first; nd; nd = nd->next) {
        if (!(nd->flags & IROLF_Reffed) && nd != rec->linear && ReadsKills(nd, kills, used)) {
            return TRUE;
        }
        if (nd == node->last) {
            break;
        }
    }
    return FALSE;
}

// 0x5931F0
Boolean func_005931F0(
    IRONodeQueue* q, IROInvariantRec* rec, IRONaturalLoop* loop, BitVector* kills, BitVector* used
) {
    IRONode* node;
    Boolean found = FALSE;

    while (q->count && !found) {
        node = IRO_NodeTable[Dequeue(q)];
        found = func_00593370(node, rec, kills, used);
        QueueSuccessors(q, loop, node);
    }
    return found;
}

// 0x5930C0: the loop reads what the (not invariant) candidate kills
Boolean func_005930C0(IROInvariantRec* rec, IRONaturalLoop* loop, BitVector* kills) {
    BitVector* used;
    IRONodeQueue q;
    Boolean found;

    Bv_AllocVector(&used, IRO_NumVars + 1);
    Bv_AllocVector(&IRO_Depends, IRO_NumVars + 1);
    StartQueue(&q, loop);
    found = func_005931F0(&q, rec, loop, kills, used);
    IRO_free(q.data);
    if (IRO_Depends) {
        IRO_free(IRO_Depends);
        IRO_Depends = NULL;
    }
    if (used) {
        IRO_free(used);
        used = NULL;
    }
    return found;
}

// 0x593780: a statement of node before the candidate reads what it kills
Boolean func_00593780(IRONode* node, IROInvariantRec* rec, BitVector* kills, BitVector* used) {
    IROLinear* nd;
    Boolean stop = FALSE;

    for (nd = node->first; nd && !stop; nd = nd->next) {
        if (!(nd->flags & IROLF_Reffed)) {
            if (nd == rec->linear) {
                stop = TRUE;
            } else if (ReadsKills(nd, kills, used)) {
                return TRUE;
            }
        }
        if (nd == node->last) {
            break;
        }
    }
    return FALSE;
}

// 0x5935A0: as 0x5931F0, up to the candidate's node (not past the nodes it
// dominates)
Boolean func_005935A0(
    IRONodeQueue* q, IROInvariantRec* rec, IRONaturalLoop* loop, BitVector* kills, BitVector* used
) {
    IRONode* node;
    IRONode* succ;
    UInt16 i;
    Boolean found = FALSE;

    while (q->count && !found) {
        node = IRO_NodeTable[Dequeue(q)];
        found = func_00593780(node, rec, kills, used);
        for (i = 0; i < node->numsucc; i++) {
            succ = IRO_NodeTable[node->succ[i]];
            if (Bv_IsBitSet(succ->index, loop->nodes) &&
                !succ->marked &&
                (!Bv_IsBitSet(rec->node->index, succ->dom) || succ == rec->node)) {
                Enqueue(q, succ->index);
                succ->marked = TRUE;
            }
        }
    }
    return found;
}

// 0x593470: the loop reads what the invariant candidate kills before it
Boolean func_00593470(IROInvariantRec* rec, IRONaturalLoop* loop, BitVector* kills) {
    BitVector* used;
    IRONodeQueue q;
    Boolean found;

    Bv_AllocVector(&used, IRO_NumVars + 1);
    Bv_AllocVector(&IRO_Depends, IRO_NumVars + 1);
    StartQueue(&q, loop);
    found = func_005935A0(&q, rec, loop, kills, used);
    IRO_free(q.data);
    if (IRO_Depends) {
        IRO_free(IRO_Depends);
        IRO_Depends = NULL;
    }
    if (used) {
        IRO_free(used);
        used = NULL;
    }
    return found;
}

// another statement of the loop kills what the candidate kills
static inline Boolean OtherKills(IROInvariantRec* rec, IRONaturalLoop* loop, BitVector* kills) {
    IRONodeQueue q;
    Boolean found;

    Bv_AllocVector(&IRO_VarKills, IRO_NumVars + 1);
    StartQueue(&q, loop);
    found = func_00592E80(&q, rec, loop, kills);
    IRO_free(q.data);
    if (IRO_VarKills) {
        IRO_free(IRO_VarKills);
        IRO_VarKills = NULL;
    }
    return found;
}

// 0x592C10: the candidate cannot move: the loop reads what it kills (before
// it, when invariant) or kills it elsewhere
Boolean func_00592C10(IROInvariantRec* rec, IRONaturalLoop* loop) {
    BitVector* kills;
    int result;

    Bv_AllocVector(&IRO_VarKills, IRO_NumVars + 1);
    IRO_GetKills(rec->linear);
    kills = IRO_VarKills;
    if (rec->invariant) {
        result = func_00593470(rec, loop, kills) != 0;
        if (!result) {
            result = OtherKills(rec, loop, kills) != 0;
        }
    } else {
        result = func_005930C0(rec, loop, kills) != 0;
        if (!result) {
            result = OtherKills(rec, loop, kills) != 0;
        }
    }
    if (kills) {
        IRO_free(kills);
    }
    return result;
}

// --- moving a candidate past the loop -----------------------------------------

// 0x592BA0: the kills of the statements of node (after the candidate, in
// its node)
Boolean func_00592BA0(IRONode* node, IROInvariantRec* rec) {
    IROLinear* nd;

    nd = (node == rec->node) ? rec->linear : node->first;
    for (; nd; nd = nd->next) {
        if (!(nd->flags & IROLF_Reffed) && !(nd->flags & IROLF_Unk200000) && nd != rec->linear) {
            if (nd->type == IROLinearAsm) {
                IRO_WalkTree(nd, func_0056A2E0);
            } else {
                func_00552EE0(nd, func_005930A0, NULL);
            }
        }
        if (nd == node->last) {
            break;
        }
    }
    return FALSE;
}

// 0x592A10: from the candidate's node to the loop test
void func_00592A10(IRONodeQueue* q, IROInvariantRec* rec, IRONaturalLoop* loop) {
    IRONode* node;

    while (q->count) {
        node = IRO_NodeTable[Dequeue(q)];
        func_00592BA0(node, rec);
        if (node != loop->x10) {
            QueueSuccessors(q, loop, node);
        }
    }
}

// 0x592900: the variables killed from the candidate to the loop test
void func_00592900(IROInvariantRec* rec, IRONaturalLoop* loop, BitVector* out) {
    IRONode* node;
    IRONodeQueue q;

    Bv_AllocVector(&IRO_VarKills, IRO_NumVars + 1);
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        node->marked = FALSE;
    }
    q.data = IRO_malloc(IRO_NumNodes * sizeof(UInt16));
    q.count = 0;
    q.head = 0;
    Enqueue(&q, rec->node->index);
    rec->node->marked = TRUE;
    func_00592A10(&q, rec, loop);
    IRO_free(q.data);
    Bv_Copy(IRO_VarKills, out);
    if (IRO_VarKills) {
        IRO_free(IRO_VarKills);
        IRO_VarKills = NULL;
    }
}

// 0x5926C0: src (a copy of the candidate) onto dst for after the loop, a
// counter that changes after the candidate read as counter - step
void func_005926C0(
    IROInvariantRec* rec, IRONaturalLoop* loop, IROList* src, IROList* dst, Boolean* moved
) {
    BitVector* killed;
    IROLinear* nd;
    IROLinear* next;
    IROLinear* add;
    IROLinear* saved;
    Object* obj;
    VarRecord* var;
    IROLoopInd301* ind;
    Type* type;
    Boolean keep;

    Bv_AllocVector(&killed, IRO_NumVars + 1);
    func_00592900(rec, loop, killed);
    for (nd = src->head; nd; nd = next) {
        next = nd->next;
        keep = TRUE;
        if ((obj = IRO_IsVariable(nd)) &&
            (var = obj->varptr) &&
            var->inductionState == 2 &&
            (ind = func_00588C90(loop, var)) &&
            Bv_IsBitSet(var->index, killed)) {
            nd->flags |= IROLF_Reffed;
            nd->next = NULL;
            IRO_AddToList(nd, dst);
            add = IRO_NewLinear(IROLinearOp2Arg);
            add->nodetype = ESUB;
            add->index = ++IRO_NumLinear;
            add->rtype = IRO_UnqualifiedType(nd->rtype);
            if (CInt64_IsZero(&ind->step)) {
                if (ind->stepExpr) {
                    IRO_DuplicateExpr(ind->stepExpr, dst);
                    dst->tail->flags |= IROLF_Reffed;
                    if (ind->flags & 4) {
                        add->nodetype = EADD;
                    }
                } else {
                    CError_FATAL(861);
                }
            } else {
                type = nd->rtype;
                if (type->type == TYPEPOINTER) {
                    type = func_0055A650(type);
                }
                IRO_AddToList(
                    IRO_NewIntConst(
                        func_00583FD0(ind, IRO_UnqualifiedType(type)), IRO_UnqualifiedType(type)
                    ),
                    dst
                );
                dst->tail->flags |= IROLF_Reffed;
            }
            add->u.diadic.right = dst->tail;
            IRO_AddToList(add, dst);
            saved = nd->next;
            nd->next = next;
            if (IRO_LocateFather_Cut_And_Paste_Without_Nopping(nd, add)) {
                add->flags |= IROLF_Reffed;
            }
            nd->next = saved;
            add->u.diadic.left = nd;
            keep = FALSE;
        }
        if (keep) {
            nd->next = NULL;
            IRO_AddToList(nd, dst);
        }
    }
    if (killed) {
        IRO_free(killed);
        killed = NULL;
    }
    *moved = TRUE;
}

// --- the per-loop driver -------------------------------------------------------

// 0x5922C0: hoist the loop's invariant assignments into its preheader, and
// move the assignments whose value is only needed after the loop to its exit
Boolean func_005922C0(IRONaturalLoop* loop) {
    Boolean changed = FALSE;
    Boolean ok;
    Boolean again;
    Boolean sideEffect;
    Boolean made;
    Boolean moved;
    IROInvariantRec* head;
    IROInvariantRec* tail;
    IROInvariantRec* rec;
    IROInvariantRec* next;
    IROLinear* pre;
    IROLinear* at;
    IRONode* exitNode;
    IROList list;
    IROList out;
    int invEvery;
    int invSome;
    int otherEvery;
    int otherSome;

    func_00587540(loop);
    if (!func_0058A4F0(loop)) {
        IRO_Dump(
            "LoopCanHaveItsInvariantAssignmentsHoisted:No because no predecessor outside the loop\n"
        );
        ok = FALSE;
    } else if (loop->flags & 0x200000) {
        IRO_Dump(
            "LoopCanHaveItsInvariantAssignmentsHoisted:No because loop form was not recognized\n"
        );
        ok = FALSE;
    } else {
        ok = TRUE;
    }
    if (!ok) {
        return changed;
    }

    made = FALSE;
    head = NULL;
    tail = NULL;
    func_005939A0(loop, &head, &tail);
    sideEffect = IRO_HasSideEffect(loop->cond) != NULL;
    do {
        again = FALSE;
        for (rec = head; rec; rec = rec->next) {
            if (!rec->done && ((sideEffect && !rec->node->unk3F) || func_00592C10(rec, loop))) {
                rec->done = TRUE;
                again = TRUE;
                rec->linear->flags &= ~IROLF_Unk200000;
            }
        }
    } while (again);

    func_00593920(head, &invEvery, &invSome, &otherEvery, &otherSome);
    if (invSome > 0 || invEvery > 0) {
        func_0058A430(loop, loop->header, loop->x18, &pre, &made);
        ok = TRUE;
        if (invSome > 0) {
            func_00589F20(loop, &exitNode, &at);
            if (exitNode && at) {
                ok = func_0058A030(loop, &pre, exitNode, NULL, NULL);
            } else {
                pre = NULL;
            }
        }
        if (pre && ok) {
            for (rec = head; rec; rec = rec->next) {
                if (rec->invariant && !rec->done) {
                    IRO_InitList(&list);
                    IRO_DuplicateExpr(rec->linear, &list);
                    IRO_NopOut(rec->linear);
                    IRO_Paste(list.head, list.tail, pre);
                    changed = TRUE;
                }
            }
        }
    }
    if (otherSome > 0 || otherEvery > 0) {
        func_00589F20(loop, &exitNode, &at);
        if (exitNode && at) {
            ok = TRUE;
            if (otherSome > 0) {
                func_0058A430(loop, loop->header, loop->x18, &pre, &made);
                if (loop->preheader && pre && loop->cond) {
                    ok = func_0058A030(loop, &pre, exitNode, NULL, NULL);
                    if (ok) {
                        loop->exitSuccessor = func_0058A5D0(loop);
                        func_00589F20(loop, &exitNode, &at);
                        ok = exitNode != NULL;
                        if (exitNode) {
                            ok = at != NULL;
                        }
                    }
                } else {
                    ok = FALSE;
                }
            }
            if (exitNode && at) {
                for (rec = head; rec; rec = rec->next) {
                    if (!rec->invariant && !rec->done && (ok || rec->node->unk3F)) {
                        IRO_InitList(&list);
                        IRO_InitList(&out);
                        IRO_DuplicateExpr(rec->linear, &list);
                        func_005926C0(rec, loop, &list, &out, &moved);
                        if (moved) {
                            IRO_NopOut(rec->linear);
                            IRO_Paste(out.head, out.tail, at);
                            changed = TRUE;
                        }
                    }
                }
            }
        }
    }
    for (rec = head; rec; rec = next) {
        next = rec->next;
        if (!rec->done) {
            rec->linear->flags &= ~IROLF_Unk200000;
        }
        IRO_free(rec);
    }
    return changed;
}
