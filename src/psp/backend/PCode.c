#include "compiler/mwcc_core.h"
#include "compiler/common.h"
#include <stdarg.h>
#include "psp/BE.h"
#include "compiler/CError.h"
#include "compiler/CompilerTools.h"
#include "compiler/Alias.h"
#include "compiler/CFunc.h"

// mwccpsp 3.0.1 PCode.c (0x46F260..0x46FFB0): instruction lists, basic
// blocks and the control-flow graph. The functions follow 2.4.7's PCode.c in
// source order (the original links them in reverse); new in 3.0.1 is a hash
// table of edges keyed on (from, to), so an edge is made once and can be
// removed.

extern Statement* data_00625070; // current_statement (sourceoffset at +0x18)
extern MPCode* func_00490400(UInt16 op, va_list args); // vformatpcode
extern MPBlockLink* data_005EDCE0[0x7CD]; // edges by hash
extern SInt32 data_006246F4; // edges made
static UInt16 pclabelcount;

#define EDGE_HASH(from, to) ((((from)->number << 8) ^ (to)->number) % 0x7CD)

void initpcode(void) {
    int i;

    for (i = 0; i < 0x7CD; i++) {
        data_005EDCE0[i] = NULL;
    }
    data_00624FD0 = data_006246B4 = NULL;
    pclabelcount = data_00624D40 = data_006246F4 = 0;
    data_0062478C = 1;
    initialize_aliases();
}

// Defined first: emitpcode inlines it.
void appendpcode(MPCodeBlock* block, MPCode* pc) {
    if (block->first) {
        pc->next = NULL;
        pc->prev = block->last;
        block->last->next = pc;
        block->last = pc;
    } else {
        block->first = block->last = pc;
        pc->next = pc->prev = NULL;
    }
    pc->block = block;
    block->pcodecount++;
}

MPCode* makepcode(VarArgShort op, ...) {
    MPCode* pc;
    va_list list;

    va_start(list, op);
    pc = func_00490400(op, list);
    pc->unk20 = data_00625070 ? data_00625070->sourceoffset : -1;
    return pc;
}

void emitpcode(VarArgShort op, ...) {
    MPCode* pc;
    va_list list;

    va_start(list, op);
    pc = func_00490400(op, list);
    if (!pc->unk20) {
        pc->unk20 = data_00625070 ? data_00625070->sourceoffset : -1;
    }
    appendpcode(data_006246B4, pc);
}

// emitpcode before an instruction (insertpcodebefore written out).
void func_0046FD50(MPCode* anchor, VarArgShort op, ...) {
    MPCode* pc;
    MPCodeBlock* block;
    va_list list;

    va_start(list, op);
    pc = func_00490400(op, list);
    if (!pc->unk20) {
        pc->unk20 = data_00625070 ? data_00625070->sourceoffset : -1;
    }
    block = anchor->block;
    if (anchor->prev) {
        anchor->prev->next = pc;
    } else {
        block->first = pc;
    }
    pc->unk24 = anchor->unk24;
    pc->next = anchor;
    pc->prev = anchor->prev;
    anchor->prev = pc;
    pc->unk20 = anchor->unk20;
    pc->block = block;
    block->pcodecount++;
    block->flags2E &= ~8;
}

typedef struct Quad {
    UInt32 w[4];
} Quad;

#if MWCCPS2
typedef struct MPCodeHead {
    UInt32 w[offsetof(MPCode, args) / 4];
} MPCodeHead; // a pcode without its operands
#endif

MPCode* copypcode(MPCode* pc) {
    MPCode* copy = lalloc(PCODE_SIZE(pc->argcount));
    int i;

#if MWCCPS2 // b210/b198: one structure assignment (a copy loop)
    *(MPCodeHead*)copy = *(MPCodeHead*)pc;
#else
    for (i = 0; i < (int)offsetof(MPCode, args); i += sizeof(Quad)) {
        *(Quad*)((char*)copy + i) = *(Quad*)((char*)pc + i);
    }
#endif
    for (i = 0; i < pc->argcount; i++) {
        copy->args[i] = pc->args[i];
    }
    return copy;
}

MPCodeLabel* makepclabel(void) {
    MPCodeLabel* label;

    label = lalloc(sizeof(MPCodeLabel));
    memclrw(label, sizeof(MPCodeLabel));
    label->index = pclabelcount++;
    return label;
}

MPCodeBlock* makepcblock(void) {
    MPCodeBlock* block;

    block = lalloc(sizeof(MPCodeBlock));
    memclrw(block, sizeof(MPCodeBlock));
    block->unk20 = -1;
    block->weight = data_0062478C;
    block->number = data_00624D40++;
    if (data_006246B4) {
        data_006246B4->next = block;
        block->prev = data_006246B4;
    } else {
        data_00624FD0 = block;
    }
    data_006246B4 = block;
    return block;
}

void pclabel(MPCodeBlock* block, MPCodeLabel* label) {
    MPPendingBranch* p;

    for (p = (MPPendingBranch*)label->block; p; p = p->next) {
        linkblocks(p->from, block)->flags |= p->flags;
    }
    label->block = block;
    label->resolved = 1;
    label->next = block->labels;
    block->labels = label;
}

#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
#undef CError_B69_SHIFT
#define CError_B69_SHIFT (-2)
#endif
void pcbranch(MPCodeBlock* block, MPCodeLabel* label, UInt16 flags) {
    MPPendingBranch* p;

    if (label->resolved) {
#if MWCCPS2 // b210/b198: the link in a local
        MPBlockLink* link = linkblocks(block, label->block);
        link->flags |= flags;
#else
        linkblocks(block, label->block)->flags |= flags;
#endif
    } else {
        p = lalloc(sizeof(MPPendingBranch));
        memclrw(p, sizeof(MPPendingBranch));
        p->from = block;
        p->flags = flags;
        p->next = (MPPendingBranch*)label->block;
        label->block = (MPCodeBlock*)p;
    }
}
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
#undef CError_B69_SHIFT
#define CError_B69_SHIFT 0
#endif

// Place a label: the current block ends (falling through to it) unless it
// is still empty.
void func_0046FA90(MPCodeLabel* label) {
    if (data_006246B4->pcodecount) {
        pcbranch(data_006246B4, label, 0);
        makepcblock();
    }
    pclabel(data_006246B4, label);
}

void func_0046FA50(void) {
    func_0046FA90(makepclabel());
}

#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
#if MWCC_CORE != MWCC_CORE_B52
// b69 (0x498680): the first block is never deleted and the last one keeps
// data_006246B4; not inlined into deleteunreachableblocks
#ifdef __MWERKS__
#pragma auto_inline off
#endif
#endif
#endif
void deleteblock(MPCodeBlock* block) {
#if MWCC_CORE == MWCC_CORE_B52
    if (block->prev) {
#endif
#if !MWCC_CORE || MWCC_CORE > MWCC_CORE_B52
        if (block->prev) {
#endif
            block->prev->next = block->next;
#if MWCC_CORE == MWCC_CORE_B52
        } else {
            data_00624FD0 = block->next;
        }
#endif
#if !MWCC_CORE || MWCC_CORE > MWCC_CORE_B52
    } else {
        data_00624FD0 = block->next;
    }
#endif
    if (block->next) {
        block->next->prev = block->prev;
#if MWCC_CORE == MWCC_CORE_B52
    } else {
        data_006246B4 = block->prev;
#endif
#if !MWCC_CORE || MWCC_CORE > MWCC_CORE_B52
    }
    else {
        data_006246B4 = block->prev;
#endif
    }
    block->flags2E |= 0x20;
}
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
#if MWCC_CORE != MWCC_CORE_B52
#ifdef __MWERKS__
#pragma auto_inline reset
#endif
#endif
#endif

void deleteunreachableblocks(void) {
    MPCodeBlock* block;

    computedepthfirstordering();
    for (block = data_00624FD0->next; block; block = block->next) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        // b69 (0x4986A0): a noreorder block (0x4000) is kept, unreached or not
        // (the delay slot of an asm `b` under .set noreorder)
        if (!(block->flags2E & 4) && !(block->flags2E & 0x4000)) {
#else
        if (!(block->flags2E & 4) && !(block->flags2E & 0x190)) {
#endif
            deleteblock(block);
        }
    }
}

void deletepcode(MPCode* pc) {
    MPCodeBlock* block = pc->block;

    if (pc->prev) {
        pc->prev->next = pc->next;
    } else {
        block->first = pc->next;
    }
    if (pc->next) {
        pc->next->prev = pc->prev;
    } else {
        block->last = pc->prev;
    }
    pc->block = NULL;
    block->pcodecount--;
    block->flags2E &= ~8;
}

void insertpcodebefore(MPCode* anchor, MPCode* pc) {
    MPCodeBlock* block = anchor->block;

    if (anchor->prev) {
        anchor->prev->next = pc;
    } else {
        block->first = pc;
    }
    pc->unk24 = anchor->unk24;
    pc->next = anchor;
    pc->prev = anchor->prev;
    anchor->prev = pc;
    pc->unk20 = anchor->unk20;
    pc->block = block;
    block->pcodecount++;
    block->flags2E &= ~8;
}

void insertpcodeafter(MPCode* anchor, MPCode* pc) {
    MPCodeBlock* block = anchor->block;

    if (anchor->next) {
        anchor->next->prev = pc;
    } else {
        block->last = pc;
    }
    pc->unk24 = anchor->next ? anchor->next->unk24 : 0;
    pc->prev = anchor;
    pc->next = anchor->next;
    anchor->next = pc;
    pc->unk20 = anchor->unk20;
    pc->block = block;
    block->pcodecount++;
    block->flags2E &= ~8;
}

void setpcodeflags(UInt32 flags) {
    data_006246B4->last->flags |= flags;
}

typedef struct DFSEntry {
    MPCodeBlock* block;
    MPBlockLink* succ; // next successor to visit
} DFSEntry;

extern SInt32 data_005EDCD8; // next free slot (counting down)

// Order the blocks by reverse postorder of a depth-first walk from the entry.
#if MWCCPS2 && !MWCC_CORE
// mwccps2 (b198 0x47C460, b210): a signed index that starts at zero and a while loop (the
// PS2 b198 decompilation's)
void computedepthfirstordering(void) {
    MPCodeBlock* block;
    MPBlockLink* link;
    DFSEntry* stack;
    SInt32 sp;

    data_00625418 = lalloc(sizeof(MPCodeBlock*) * data_00624D40);
    data_005EDCD8 = data_00624D40;
    for (block = data_00624FD0; block; block = block->next) {
        block->flags2E &= ~4;
    }
    stack = oalloc(sizeof(DFSEntry) * data_00624D40);
    sp = 0;
    data_00624FD0->flags2E |= 4;
    stack[sp].block = data_00624FD0;
    stack[sp].succ = data_00624FD0->successors;
    sp++;
    while (sp) {
        if ((link = stack[sp - 1].succ)) {
            stack[sp - 1].succ = link->next;
            block = link->block;
            if (!(block->flags2E & 4)) {
                block->flags2E |= 4;
                stack[sp].block = block;
                stack[sp].succ = block->successors;
                sp++;
            }
        } else {
            block = stack[--sp].block;
            data_00625418[--data_005EDCD8] = block;
        }
    }

    while (data_005EDCD8) {
        data_00625418[--data_005EDCD8] = NULL;
    }
}
#else
void computedepthfirstordering(void) {
    MPCodeBlock* block;
    MPBlockLink* link;
    DFSEntry* stack;
    UInt32 sp;

    data_00625418 = lalloc(data_00624D40 * sizeof(MPCodeBlock*));
    data_005EDCD8 = data_00624D40;
    for (block = data_00624FD0; block; block = block->next) {
        block->flags2E &= ~4;
    }
    stack = oalloc(data_00624D40 * sizeof(DFSEntry));
    data_00624FD0->flags2E |= 4;
    stack[0].block = data_00624FD0;
    stack[0].succ = data_00624FD0->successors;
    sp = 1;
    do {
        if ((link = stack[sp - 1].succ)) {
            stack[sp - 1].succ = link->next;
            block = link->block;
            if (!(block->flags2E & 4)) {
                block->flags2E |= 4;
                stack[sp].block = block;
                stack[sp].succ = block->successors;
                sp++;
            }
        } else {
            sp--;
            data_00625418[--data_005EDCD8] = stack[sp].block;
        }
    } while (sp);
    while (data_005EDCD8) {
        data_00625418[--data_005EDCD8] = NULL;
    }
}
#endif

// Put a new block on the fall-through edge from -> to.
MPCodeBlock* func_0046F660(MPCodeBlock* from, MPCodeBlock* to) {
    MPCodeBlock* block;

    if (from->next != to) {
        CError_Internal("PCode.c", CError_B145_LINE(611));
    }
    block = lalloc(sizeof(MPCodeBlock));
    memclrw(block, sizeof(MPCodeBlock));
    block->unk20 = -1;
    block->number = data_00624D40++;
    block->weight = from->weight;
    pclabel(block, makepclabel());
    from->next = block;
    block->prev = from;
    block->next = to;
    to->prev = block;
    unlinkblocks(from, to);
    linkblocks(from, block);
    linkblocks(block, to);
    return block;
}

// Whether `to` can be merged into `from`: it follows `from`, is `from`'s only
// successor and has no other predecessor.
static int onesuccessor(MPCodeBlock* block) {
    return block->successors && !block->successors->next;
}

static int onepredecessor(MPCodeBlock* block) {
    return block->predecessors && !block->predecessors->nextpred;
}

Boolean func_0046F5F0(MPCodeBlock* from, MPCodeBlock* to) {
    int result = 0;
    int onepred = 0;
    int onesucc = 0;

    if (from->next == to && onesuccessor(from)) {
        onesucc = 1;
    }
    if (onesucc && onepredecessor(to)) {
        onepred = 1;
    }
    if (onepred && from->successors->block == to) {
        result = 1;
    }
    return result;
}

// Merge `to` into `from`, dropping `from`'s branch.
void func_0046F400(MPCodeBlock* from, MPCodeBlock* to) {
    MPCode* pc;
    MPCode* next;
    MPBlockLink* e;
    MPCodeBlock* target;

    if (!func_0046F5F0(from, to)) {
        CError_FATAL(826);
    }
    if (from->last && (from->last->flags & 1)) {
        deletepcode(from->last);
    }
    for (pc = to->first; pc; pc = next) {
        next = pc->next;
        deletepcode(pc);
        appendpcode(from, pc);
    }
    e = to->successors;
    for (;;) {
        if (!e) {
            break;
        }
        target = e->block;
        linkblocks(from, target);
        unlinkblocks(to, target);
        e = e->next;
    }
    unlinkblocks(from, to);
    deleteblock(to);
}

MPBlockLink* linkblocks(MPCodeBlock* from, MPCodeBlock* to) {
    int h = EDGE_HASH(from, to);
    MPBlockLink* e;

    for (e = data_005EDCE0[h]; e; e = e->hashnext) {
        if (e->from == from && e->block == to) {
            return e;
        }
    }
    e = lalloc(sizeof(MPBlockLink));
    memclrw(e, sizeof(MPBlockLink));
    e->from = from;
    e->block = to;
    e->index = data_006246F4++;
    e->hash = h;
    e->hashnext = data_005EDCE0[h];
    data_005EDCE0[h] = e;
    e->next = from->successors;
    from->successors = e;
    e->nextpred = to->predecessors;
    to->predecessors = e;
    return e;
}

static MPBlockLink* findlink(MPCodeBlock* from, MPCodeBlock* to) {
    MPBlockLink* e;

    for (e = data_005EDCE0[EDGE_HASH(from, to)]; e; e = e->hashnext) {
        if (e->from == from && e->block == to) {
            return e;
        }
    }
    return NULL;
}

void unlinkblocks(MPCodeBlock* from, MPCodeBlock* to) {
    MPBlockLink* e;
    MPBlockLink** pp;

    e = findlink(from, to);
    if (!e) {
        CError_Internal("PCode.c", CError_B145_LINE(934));
    }
    for (pp = &to->predecessors; *pp != e; pp = &(*pp)->nextpred) {}
    *pp = e->nextpred;
    for (pp = &from->successors; *pp != e; pp = &(*pp)->next) {}
    *pp = e->next;
    for (pp = &data_005EDCE0[e->hash]; *pp != e; pp = &(*pp)->hashnext) {}
    *pp = e->hashnext;
}
