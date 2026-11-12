// mwccpsp 3.0.1 Scheduler.c (0x4B5210..0x4B63A0): list scheduling of each
// basic block, as in 2.4.7, with three 3.0.1 additions: an instruction class
// (DGNode.kind) that breaks ties instead of the opcode rank, extra
// serialization chains for the HI/LO-style special registers and for three
// instruction flags, and register pressure: while registers are still
// virtual, the pressure of every class is tracked as instructions issue,
// and when some class is full the instruction that frees the most is taken.
// The machine model (latencies, issue rules) is still the original code.
#include "compiler/common.h"
#include "compiler/CError.h"
#include "compiler/CompilerTools.h"
#include "psp/BE.h"

typedef struct DGNode {
    struct DGNode* next;
    struct DGNode* prev;
    struct DGSuccessor* successors;
    MPCode* instr;
    UInt16 latency; // +0x10
    UInt16 kind; // +0x12: func_004B5EA0
    UInt16 earliest; // +0x14
    UInt16 latest; // +0x16
    UInt16 height; // +0x18
    short predCount; // +0x1A
} DGNode; // 0x1C

typedef struct DGSuccessor {
    struct DGSuccessor* next;
    DGNode* node;
    UInt16 latency;
} DGSuccessor; // 0xC

typedef struct DGNodeList {
    struct DGNodeList* next;
    DGNode* node;
} DGNodeList;

typedef struct MachineInfo {
    int issue_width; // +0x00
    int renames_registers; // +0x04
    int (*latency)(MPCode* instr); // +0x08
    void (*initialize)(void); // +0x0C
    int (*can_issue)(MPCode* instr); // +0x10
    void (*issue)(MPCode* instr); // +0x14
    void (*advance_clock)(void); // +0x18
    void (*dependence)(MPCode* a, MPCode* b); // +0x1C: told of each new edge (may be NULL)
} MachineInfo;

extern SInt32 data_00625A98[7]; // next virtual register of each class
extern SInt32 data_00624744[7]; // first virtual register of the class (after the physical ones)
extern UInt8 data_0060FEE8[7][BE_MAX_CLASS_REGS]; // physical register use (2: dedicated)
extern MPCodeBlock* data_00624FD0; // pcbasicblocks
extern SInt32 data_0062482C; // registers are still virtual
extern SInt32 func_00462160(char rclass); // registers available in the class
extern void func_004C34A0(void);
extern void gather_alias_info(void);
extern Boolean may_alias(MPCode* a, MPCode* b);

static MachineInfo* MI;
static DGNode* head;
static DGNodeList** register_uses[7];
static DGNodeList** register_defs[7];
static DGNodeList** special_uses; // 32 special registers (kind 1 operands)
static DGNodeList** special_defs;
static DGNodeList* memory_uses;
static DGNodeList* memory_defs;
static DGNodeList* chain200; // instructions flagged 0x200, 0x400, 0x800 are kept in order
static DGNodeList* chain400;
static DGNodeList* chain800;
static DGNodeList* side_effects;
static DGNodeList* volatile_refs;
static DGNode* defaultsuccessor;
static UInt16 criticalpath;
static SInt32* refcount[7]; // uses left of each register in the block
static SInt32 avail[7]; // registers available per class
static SInt32 live[7]; // registers live per class

#define DEDICATED(rclass, reg) \
    ((reg) < data_00624744[(rclass)] && data_0060FEE8[(rclass)][(reg)] == 2)
#define BITSET(set, i) (SHR32((set)[(i) >> 5], (i)) & 1)

static void addsuccessor(DGNode* a, DGNode* b, Boolean hasLatency);
static void findsuccessors(DGNode* node);
static DGNode* selectinstruction(DGNode* nodes, UInt16 counter);
static void initresources(void);
static UInt16 func_004B5EA0(MPCode* instr);
static int func_004B5F30(MPCode* instr);
static void func_004B60F0(MPCode* instr);
static void func_004B61E0(MPCodeBlock* block);
static void scheduleblock(MPCodeBlock* block);

void func_004B5210(void) {
    MPCodeBlock* block;

    gather_alias_info();
    func_004C34A0();
    for (block = data_00624FD0; block; block = block->next) {
        if (block->pcodecount >= 2 && !(block->flags2E & 0x11B)) {
            scheduleblock(block);
            block->flags2E |= 8;
        }
    }
    freeoheap();
}

void func_004B5250(MachineInfo* mi) {
    MI = mi;
}

static void scheduleblock(MPCodeBlock* block) {
    UInt16 counter;
    UInt16 i;
    DGNode* node;
    DGNode* n;
    DGSuccessor* succ;
    MPCode* instr;

    MI->initialize();
    initresources();
    func_004B61E0(block);
    defaultsuccessor = NULL;
    head = NULL;

    for (instr = block->last; instr; instr = instr->prev) {
        node = oalloc(sizeof(DGNode));
        node->next = NULL;
        node->prev = NULL;
        node->successors = NULL;
        node->instr = instr;
        node->latency = node->height = MI->latency(instr);
        node->kind = func_004B5EA0(instr);
        node->earliest = 0;
        node->latest = 0;
        node->predCount = 0;
        findsuccessors(node);
        if (instr->flags & 1) {
            defaultsuccessor = node;
        }
        if (head) {
            head->prev = node;
        }
        node->next = head;
        head = node;
    }

    for (n = head; n; n = n->next) {
        n->latest = criticalpath - n->height;
    }

    block->first = block->last = NULL;
    block->pcodecount = 0;

    counter = 0;
    while (head) {
        for (i = 0; i < MI->issue_width; i++) {
            if (!head) {
                break;
            }
            node = selectinstruction(head, counter);
            if (!node) {
                break;
            }
            instr = node->instr;
            if (node->successors) {
                for (succ = node->successors; succ; succ = succ->next) {
                    succ->node->predCount--;
                    if (succ->node->earliest < counter + succ->latency) {
                        succ->node->earliest = counter + succ->latency;
                    }
                }
            }
            appendpcode(block, instr);
            MI->issue(instr);
            func_004B60F0(instr);
            if (node->prev) {
                node->prev->next = node->next;
            } else {
                head = node->next;
            }
            if (node->next) {
                node->next->prev = node->prev;
            }
        }
        MI->advance_clock();
        counter++;
    }
}

// is some register class full?
static inline Boolean overpressure(void) {
    RegClass301 rclass;

    for (rclass = 0; rclass < RC301_MAX; rclass++) {
        if (live[rclass] >= avail[rclass]) {
            return TRUE;
        }
    }
    return FALSE;
}

static DGNode* selectinstruction(DGNode* nodes, UInt16 counter) {
    DGNode* node;
    DGNode* candidate;
    int nodeUncovers;
    int candidateUncovers;
    int best;
    DGSuccessor* succ;

    node = nodes;
    while (node) {
        if (node->predCount == 0 && node->earliest <= counter && MI->can_issue(node->instr)) {
            break;
        }
        node = node->next;
    }

    if (!node) {
        return NULL;
    }

    if (data_0062482C && overpressure()) {
        // the instruction that frees the most registers
        best = func_004B5F30(node->instr);
        for (candidate = node->next; candidate; candidate = candidate->next) {
            if (candidate->predCount == 0 &&
                candidate->earliest <= counter &&
                MI->can_issue(candidate->instr) &&
                func_004B5F30(candidate->instr) < best) {
                best = func_004B5F30(candidate->instr);
                node = candidate;
            }
        }
        return node;
    }

    for (candidate = node->next; candidate; candidate = candidate->next) {
        if (candidate->predCount == 0 &&
            candidate->earliest <= counter &&
            MI->can_issue(candidate->instr) &&
            (node->latest > counter || candidate->latest <= counter)) {
            if (node->latest > counter && candidate->latest <= counter) {
                node = candidate;
                continue;
            }

            candidateUncovers = 0;
            for (succ = candidate->successors; succ; succ = succ->next) {
                if (succ->node->predCount == 1) {
                    candidateUncovers++;
                }
            }
            nodeUncovers = 0;
            for (succ = node->successors; succ; succ = succ->next) {
                if (succ->node->predCount == 1) {
                    nodeUncovers++;
                }
            }
            if (nodeUncovers > candidateUncovers) {
                continue;
            }
            if (nodeUncovers < candidateUncovers) {
                node = candidate;
                continue;
            }

            if (node->height > candidate->height) {
                continue;
            }
            if (node->height < candidate->height) {
                node = candidate;
                continue;
            }

            if (data_0062482C) {
                if (node->kind < candidate->kind) {
                    continue;
                }
                if (node->kind > candidate->kind) {
                    node = candidate;
                }
            }
        }
    }

    return node;
}

static inline void addtolist(DGNodeList** list, DGNode* node) {
    DGNodeList* entry = oalloc(sizeof(DGNodeList));
    entry->node = node;
    entry->next = *list;
    *list = entry;
}

static inline void serializelist(DGNode* node, DGNodeList* list) {
    while (list) {
        if (list->node != node) {
            addsuccessor(node, list->node, FALSE);
        }
        list = list->next;
    }
}

static void serialize_store(DGNode* node);
static void serializeregister(
    int special, DGNode* node, DGNodeList** defs, DGNodeList** uses, int isWrite
);

static void findsuccessors(DGNode* node) {
    MPCode* instr;
    MOperand* op;
    int i;
    DGNodeList* list;
    DGNode* n;

    instr = node->instr;
    for (i = 0, op = instr->args; i < instr->argcount; i++, op++) {
        switch (op->kind) {
            case 0:
                if (op->flags & 2) {
                    serializeregister(
                        0,
                        node,
                        &register_defs[op->rclass][op->reg],
                        &register_uses[op->rclass][op->reg],
                        op->flags & 2
                    );
                }
                break;
            case 1:
                serializeregister(
                    1, node, &special_defs[op->reg], &special_uses[op->reg], op->flags & 2
                );
                break;
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                break;
            default:
                CError_FATAL(771);
        }
    }
    for (i = 0, op = instr->args; i < instr->argcount; i++, op++) {
        switch (op->kind) {
            case 0:
                if (op->flags & 1) {
                    serializeregister(
                        0,
                        node,
                        &register_defs[op->rclass][op->reg],
                        &register_uses[op->rclass][op->reg],
                        op->flags & 2
                    );
                }
                break;
            case 1:
                serializeregister(
                    1, node, &special_defs[op->reg], &special_uses[op->reg], op->flags & 2
                );
                break;
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                break;
            default:
                CError_FATAL(794);
        }
    }

    if (instr->flags & 0x20002) {
        for (list = memory_defs; list; list = list->next) {
            if (may_alias(node->instr, list->node->instr)) {
                addsuccessor(node, list->node, TRUE);
            }
        }
        addtolist(&memory_uses, node);
    } else if (instr->flags & 0x40004) {
        serialize_store(node);
    }

    if (instr->flags & 0x80) {
        serializelist(node, volatile_refs);
        addtolist(&volatile_refs, node);
    }
    if (instr->flags & 0x200) {
        serializelist(node, chain800);
        addtolist(&chain200, node);
    }
    if (instr->flags & 0x400) {
        serializelist(node, chain200);
        addtolist(&chain400, node);
    }
    if (instr->flags & 0x800) {
        serializelist(node, chain400);
        addtolist(&chain800, node);
    }
    if (instr->flags & 0x100108) {
        for (n = head; n; n = n->next) {
            addsuccessor(node, n, FALSE);
        }
        addtolist(&side_effects, node);
    }
    if (side_effects) {
        serializelist(node, side_effects);
    }
    if (!node->successors && defaultsuccessor) {
        addsuccessor(node, defaultsuccessor, FALSE);
    }
    if (node->height > criticalpath) {
        criticalpath = node->height;
    }
}

static void serialize_store(DGNode* node) {
    DGNodeList* list;

    for (list = memory_uses; list; list = list->next) {
        if (may_alias(node->instr, list->node->instr)) {
            addsuccessor(node, list->node, TRUE);
        }
    }
    for (list = memory_defs; list; list = list->next) {
        if (may_alias(node->instr, list->node->instr)) {
            addsuccessor(node, list->node, TRUE);
        }
    }
    addtolist(&memory_defs, node);
    if (node->instr->flags & 0x40000) {
        addtolist(&memory_uses, node);
    }
}

static void serializeregister(
    int special, DGNode* node, DGNodeList** defs, DGNodeList** uses, int isWrite
) {
    DGNodeList* list;

    if (isWrite) {
        for (list = *uses; list; list = list->next) {
            if (list->node != node) {
                addsuccessor(node, list->node, TRUE);
            }
        }
        for (list = *defs; list; list = list->next) {
            if (list->node != node) {
                addsuccessor(node, list->node, special || !MI->renames_registers);
            }
        }
        if (*defs) {
            (*defs)->node = node;
        } else {
            addtolist(defs, node);
        }
        *uses = NULL;
    } else {
        for (list = *defs; list; list = list->next) {
            if (list->node != node) {
                addsuccessor(node, list->node, special || !MI->renames_registers);
            }
        }
        addtolist(uses, node);
    }
}

static void addsuccessor(DGNode* a, DGNode* b, Boolean hasLatency) {
    DGSuccessor* succ;

    if (MI->dependence) {
        MI->dependence(a->instr, b->instr);
    }
    for (succ = a->successors; succ; succ = succ->next) {
        if (succ->node == b) {
            return;
        }
    }
    succ = oalloc(sizeof(DGSuccessor));
    succ->node = b;
    succ->next = a->successors;
    a->successors = succ;
    succ->latency = hasLatency ? a->latency : 0;
    b->predCount++;
    if (b->height + succ->latency > a->height) {
        a->height = b->height + succ->latency;
    }
}

static void initresources(void) {
    RegClass301 rclass;
    int i;

    for (rclass = 0; rclass < 7; rclass++) {
        register_uses[rclass] = oalloc(sizeof(DGNodeList*) * data_00625A98[rclass]);
        register_defs[rclass] = oalloc(sizeof(DGNodeList*) * data_00625A98[rclass]);
        for (i = 0; i < data_00625A98[rclass]; i++) {
            register_uses[rclass][i] = register_defs[rclass][i] = NULL;
        }
    }
    special_uses = oalloc(sizeof(DGNodeList*) * 32);
    special_defs = oalloc(sizeof(DGNodeList*) * 32);
    for (i = 0; i < 32; i++) {
        special_uses[i] = special_defs[i] = NULL;
    }
    memory_uses = memory_defs = NULL;
    chain200 = chain400 = chain800 = NULL;
    side_effects = NULL;
    volatile_refs = NULL;
    criticalpath = 0;
}

// the instruction's class: 0 a move, 3 a load, 1 no register result, 4 a
// result from no registers, 2 the rest
static UInt16 func_004B5EA0(MPCode* instr) {
    MOperand* op;
    int n;
    int defs;
    int uses;

    if (instr->flags & 0x10) {
        return 0;
    }
    if (instr->flags & 2) {
        return 3;
    }
    defs = uses = 0;
    for (op = instr->args, n = instr->argcount; n--; op++) {
        if (op->kind == 0) {
            if (op->flags & 2) {
                defs++;
            }
            if (op->flags & 1) {
                uses++;
            }
        }
    }
    if (!defs) {
        return 1;
    }
    if (!uses) {
        return 4;
    }
    return 2;
}

// how much issuing the instruction would raise the register pressure
// (lower is better)
static int func_004B5F30(MPCode* instr) {
    MOperand* op;
    int n;
    int count;
    int prio;
    SInt32 d;

    prio = 0x10;
    for (op = instr->args, n = instr->argcount; n--; op++) {
        if (op->kind == 0 && (op->flags & 1)) {
            refcount[op->rclass][op->reg]--;
        }
    }
    count = instr->argcount;
    for (op = instr->args, n = count; n--; op++) {
        if (op->kind == 0 &&
            (op->flags & 1) &&
            !DEDICATED(op->rclass, op->reg) &&
            refcount[op->rclass][op->reg] == 0) {
            d = live[op->rclass] - avail[op->rclass];
            prio--;
            if (d <= 1) {
                d = 1;
            }
            prio -= d;
        }
    }
    for (op = instr->args, n = count; n--; op++) {
        if (op->kind == 0 &&
            (op->flags & 2) &&
            !DEDICATED(op->rclass, op->reg) &&
            refcount[op->rclass][op->reg] &&
            !(op->flags & 1)) {
            d = live[op->rclass] - avail[op->rclass];
            if (d <= 1) {
                d = 1;
            }
            prio += d;
        }
    }
    for (op = instr->args, n = count; n--; op++) {
        if (op->kind == 0 && (op->flags & 1)) {
            refcount[op->rclass][op->reg]++;
        }
    }
    return prio;
}

// the pressure after the instruction issued
static void func_004B60F0(MPCode* instr) {
    MOperand* op;
    int n;

    for (op = instr->args, n = instr->argcount; n--; op++) {
        if (op->kind == 0 &&
            (op->flags & 1) &&
            !DEDICATED(op->rclass, op->reg) &&
            --refcount[op->rclass][op->reg] == 0) {
            live[op->rclass]--;
        }
    }
    for (op = instr->args, n = instr->argcount; n--; op++) {
        if (op->kind == 0 &&
            (op->flags & 2) &&
            !DEDICATED(op->rclass, op->reg) &&
            refcount[op->rclass][op->reg] &&
            !(op->flags & 1)) {
            live[op->rclass]++;
        }
    }
}

// the pressure at the block's start, and how often each register is used
static void func_004B61E0(MPCodeBlock* block) {
    RegClass301 rclass;
    int i;
    MPCode* pc;
    MOperand* op;
    int n;

    for (rclass = 0; rclass < 7; rclass++) {
        avail[rclass] = func_00462160(rclass);
        live[rclass] = 0;
        for (i = 0; i < data_00625A98[rclass]; i++) {
            if (BITSET(data_0062541C[rclass].blocks[block->number].livein, i) &&
                !DEDICATED(rclass, i)) {
                live[rclass]++;
            }
        }
    }
    for (rclass = 0; rclass < 7; rclass++) {
        refcount[rclass] = oalloc(sizeof(SInt32) * data_00625A98[rclass]);
        for (i = 0; i < data_00625A98[rclass]; i++) {
            refcount[rclass][i] =
                BITSET(data_0062541C[rclass].blocks[block->number].liveout, i) ? 1 : 0;
        }
    }
    for (pc = block->first; pc; pc = pc->next) {
        for (op = pc->args, n = pc->argcount; n--; op++) {
            if (op->kind == 0 && (op->flags & 1)) {
                refcount[op->rclass][op->reg]++;
            }
        }
    }
}
