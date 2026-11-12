#include "compiler/common.h"
#include "compiler/BitVectors.h"
#include "psp/BE.h"
#include "compiler/CParser.h"

// mwccpsp 3.0.1 register liveness (0x4C3190..; the original unit name is not
// known).

typedef struct ExitLive {
    RegClass301 rclass;
    char pad;
    SInt16 reg;
} ExitLive;
typedef struct ExitLiveList {
    SInt32 count;
    ExitLive entries[1];
} ExitLiveList;

extern SInt32 data_00625A98[7];
extern SInt32 data_00624D40; // number of blocks
extern MPCodeBlock* data_00624C18; // exit block
extern MPCodeBlock** data_00625418; // blocks by number, for reverse-order sweeps
extern void memclrw(void* p, SInt32 size);
extern ExitLiveList* data_00603F88; // registers live at function exit
extern void* oalloc(SInt32 size);
extern void* lalloc(SInt32 size);
void func_004C3970(char rclass);
void func_004C37C0(char rclass);
void func_004C3650(void);

// Compute use/def/live-in/live-out of every block for every register class.
void func_004C34A0(void) {
    RegClass301 rclass;
    SInt32 n;
    SInt32 size;
    SInt32 i;
    BlockLiveness* bl;
    ExitLive* e;

    computedepthfirstordering();
    for (rclass = 0; rclass < 7; rclass++) {
        n = data_00625A98[rclass];
        data_0062541C[rclass].n = n;
        data_0062541C[rclass].blocks = oalloc(data_00624D40 * sizeof(BlockLiveness));
        size = ((UInt32)(n + 31) >> 5) * 4;
        data_0062541C[rclass].current = oalloc(size);
        for (i = 0; i < data_00624D40; i++) {
            bl = &data_0062541C[rclass].blocks[i];
            bitvectorinitialize(bl->use = oalloc(size), n, 0);
            bitvectorinitialize(bl->def = oalloc(size), n, 0);
            bitvectorinitialize(bl->livein = oalloc(size), n, 0);
            bitvectorinitialize(bl->liveout = oalloc(size), n, 0);
        }
    }

    func_004C3970(-1);
    for (i = 0; i < data_00603F88->count; i++) {
        e = &data_00603F88->entries[i];
        bitvectorsetbit(e->reg, data_0062541C[e->rclass].blocks[data_00624C18->number].use);
    }
    for (rclass = 0; rclass < 7; rclass++) {
        func_004C37C0(rclass);
    }
    func_004C3650();
}

// Start a backwards walk of a block: current = live-out.
void func_004C3460(MPCodeBlock* block) {
    RegClass301 rclass;
    ClassLiveness* cl;

    for (rclass = 0, cl = data_0062541C; rclass < 7; rclass++, cl++) {
        bitvectorcopy(cl->current, cl->blocks[block->number].liveout, cl->n);
    }
}

// Is pc dead? (no side effects, no live result; only when optimizing)
SInt32 func_004C3310(MPCode* pc) {
#if MWCCPS2
    MOperand* op;
    SInt32 k;
#else
    SInt32 k;
    MOperand* op;
#endif

    if (pc->flags & 0x14098D) {
        return 0;
    }
    if (pc->block->flags2E & 3) {
        return 0;
    }
    for (k = pc->argcount, op = pc->args; k--; op++) {
        if (op->kind == 0 &&
            (op->flags & 2) &&
            bitvectorgetbit(op->reg, data_0062541C[op->rclass].current)) {
            return 0;
        }
    }
    if (copts.optimizationlevel > 0) {
        return 1;
    }
    return 0;
}

// Step the live sets backwards over pc: kill its results, add its operands.
void func_004C33C0(MPCode* pc) {
    SInt32 k;
    MOperand* op;

    for (k = pc->argcount, op = pc->args; k--; op++) {
        if (op->kind == 0 && (op->flags & 2)) {
            bitvectorclearbit(op->reg, data_0062541C[op->rclass].current);
        }
    }
    for (k = pc->argcount, op = pc->args; k--; op++) {
        if (op->kind == 0 && (op->flags & 1)) {
            bitvectorsetbit(op->reg, data_0062541C[op->rclass].current);
        }
    }
}

// Solve liveness for one register class (backwards, to a fixed point).
void func_004C37C0(char rclass) {
    ClassLiveness* cl;
    BlockLiveness* blocks;
    BlockLiveness* bl;
    UInt32 n;
    UInt32 words;
    UInt32 i;
    UInt32 w;
    SInt32 k;
    SInt32 changed;
    UInt32 v;
    MPCodeBlock* block;
    MPBlockLink* s;
    UInt32* in;
    UInt32* out;
    UInt32* use;
    UInt32* def;

    cl = &data_0062541C[rclass];
    n = cl->n;
    blocks = cl->blocks;
    words = (n + 31) >> 5;
    do {
        changed = 0;
        for (i = data_00624D40; i;) {
            i--;
            block = data_00625418[i];
            if (!block) {
                continue;
            }
            bl = &blocks[block->number];
            if ((s = block->successors)) {
                bitvectorcopy(bl->liveout, blocks[s->block->number].livein, n);
                for (s = s->next; s; s = s->next) {
                    bitvectorunion(bl->liveout, blocks[s->block->number].livein, n);
                }
            } else {
                for (k = 0; k < data_00603F88->count; k++) {
                    if (rclass == data_00603F88->entries[k].rclass) {
                        bitvectorsetbit(
                            data_00603F88->entries[k].reg,
                            data_0062541C[rclass].blocks[block->number].liveout
                        );
                    }
                }
            }
            out = bl->liveout;
            in = bl->livein;
            use = bl->use;
            def = bl->def;
            for (w = 0; w < words; w++, in++, out++, use++, def++) {
                v = (~*def & *out) | *use;
                if (v != *in) {
                    *in = v;
                    changed = 1;
                }
            }
        }
    } while (changed);
}

typedef struct BlockFlagFlow {
    SInt32 gen; // a 0x400 instruction with no 0x200 before it
    SInt32 kill; // a 0x200 instruction with no 0x400 before it
    SInt32 in;
    SInt32 out;
} BlockFlagFlow;

// One-bit backward dataflow over the 0x400/0x200 instruction flags; marks
// blocks (0x40) and 0x200 instructions (0x800) that can reach a 0x400 one.
void func_004C3650(void) {
    BlockFlagFlow* info;
    BlockFlagFlow* bi;
    MPCodeBlock* block;
    MPCode* pc;
    MPBlockLink* s;
    UInt32 i;
    SInt32 changed;
    SInt32 v;
    SInt32 live;

    info = oalloc(data_00624D40 * sizeof(BlockFlagFlow));
    memclrw(info, data_00624D40 * sizeof(BlockFlagFlow));
    for (block = data_00624FD0; block; block = block->next) {
        bi = &info[block->number];
        for (pc = block->first; pc; pc = pc->next) {
            if ((pc->flags & 0x400) && !bi->kill) {
                bi->gen = 1;
            }
            if ((pc->flags & 0x200) && !bi->gen) {
                bi->kill = 1;
            }
        }
    }

    do {
        changed = 0;
        for (i = data_00624D40; i;) {
            i--;
            block = data_00625418[i];
            if (!block) {
                continue;
            }
            bi = &info[block->number];
            if ((s = block->successors)) {
                bi->out = info[s->block->number].in;
                for (s = s->next; s; s = s->next) {
                    bi->out |= info[s->block->number].in;
                }
            }
            v = ((bi->kill == 0) & bi->out) | bi->gen;
            if (v != bi->in) {
                bi->in = v;
                changed = 1;
            }
        }
    } while (changed);

    for (block = data_00624FD0; block; block = block->next) {
        live = info[block->number].out;
        if (live) {
            block->flags2E |= 0x40;
        } else {
            block->flags2E &= ~0x40;
        }
        for (pc = block->last; pc; pc = pc->prev) {
            if (pc->flags & 0x200) {
                if (live) {
                    pc->flags |= 0x800;
                } else {
                    pc->flags &= ~0x800;
                }
                live = 0;
            }
            if (pc->flags & 0x400) {
                live = 1;
            }
        }
    }
}

// Per-block use (read before written) and def (written before read) sets,
// for one register class or, with -1, all of them.
void func_004C3970(char rclass) {
    MPCodeBlock* block;
    MPCode* pc;
    MOperand* op;
    SInt32 k;
    BlockLiveness* bl;

    for (block = data_00624FD0; block; block = block->next) {
        for (pc = block->first; pc; pc = pc->next) {
            for (k = pc->argcount, op = pc->args; k--; op++) {
                if (op->kind == 0 && (rclass == -1 || op->rclass == rclass) && (op->flags & 1)) {
                    bl = &data_0062541C[op->rclass].blocks[block->number];
                    if (!bitvectorgetbit(op->reg, bl->def)) {
                        bitvectorsetbit(op->reg, bl->use);
                    }
                }
            }
            for (k = pc->argcount, op = pc->args; k--; op++) {
                if (op->kind == 0 && (rclass == -1 || op->rclass == rclass) && (op->flags & 2)) {
                    bl = &data_0062541C[op->rclass].blocks[block->number];
                    if (!bitvectorgetbit(op->reg, bl->use)) {
                        bitvectorsetbit(op->reg, bl->def);
                    }
                }
            }
        }
    }
}

// Liveness for a single register class.
void func_004C3190(char rclass) {
    ClassLiveness* cl;
    SInt32 n;
    SInt32 size;
    SInt32 i;
    SInt32 k;
    BlockLiveness* bl;

    computedepthfirstordering();
    cl = &data_0062541C[rclass];
    n = data_00625A98[rclass];
    cl->n = n;
    cl->blocks = oalloc(data_00624D40 * sizeof(BlockLiveness));
    size = ((UInt32)(n + 31) >> 5) * 4;
    cl->current = oalloc(size);
    for (i = 0; i < data_00624D40; i++) {
        bl = &cl->blocks[i];
        bitvectorinitialize(bl->use = oalloc(size), n, 0);
        bitvectorinitialize(bl->def = oalloc(size), n, 0);
        bitvectorinitialize(bl->livein = oalloc(size), n, 0);
        bitvectorinitialize(bl->liveout = oalloc(size), n, 0);
    }
    func_004C3970(rclass);
    for (k = 0; k < data_00603F88->count; k++) {
        if (rclass == data_00603F88->entries[k].rclass) {
            bitvectorsetbit(data_00603F88->entries[k].reg, cl->blocks[data_00624C18->number].use);
        }
    }
    func_004C37C0(rclass);
    func_004C3650();
}

// computedepthfirstordering (0x46F750) is in PCode.c.
