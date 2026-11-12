// mwccpsp 3.0.1: the MIPS back end's loop detection (0x54A9E0..0x54B9B0).
// 2.4.7's LoopDetection.c finds loops the same way (dominators, back edges,
// natural loops, preheaders), but 3.0.1 keeps them in its own tree of
// Loop301 nodes, numbers them, and records per-class register pressure.
#include "compiler/common.h"
#include "compiler/BitVectors.h"
#include "compiler/CParser.h"
#include "compiler/CompilerTools.h"
#include "psp/BE.h"
#include "psp/Loop301.h"

extern SInt32 data_00624D40; // pcblockcount
extern SInt32 func_00462160(char rclass);
extern void func_004C3460(MPCodeBlock* block);
extern MPCodeBlock* func_0046F660(MPCodeBlock* from, MPCodeBlock* to);
extern void func_004AFFC0(); // listing hook

Loop301* data_006258BC; // the loop forest
SInt32 data_00624BA8; // loops numbered so far
SInt32 data_00607668; // bits in a block set (3 * pcblockcount)
MPCodeBlock** data_0060766C; // findnaturalloop's stack
SInt32 data_00607670; // loop headers found
BlockList301* data_00607674; // loop headers
UInt32** data_00607678; // dominators, by block number
RegPressure301 data_00624E04[7];

#define GETBIT(vec, n) (((vec)[(n) >> 5] >> ((n) & 31)) & 1)
#define SETBIT(vec, n) ((vec)[(n) >> 5] |= 1u << ((n) & 31))
#define CLRBIT(vec, n) ((vec)[(n) >> 5] &= ~(1u << ((n) & 31)))
#define VECBYTES(n) (4 * (((n) + 31) >> 5))

static Loop301* newloop(UInt8 kind) {
    Loop301* loop;

    loop = lalloc(sizeof(Loop301));
    memclrw(loop, sizeof(Loop301));
    loop->kind = kind;
    loop->id = data_00624BA8++;
    return loop;
}

static UInt32* newblockset(void) {
    UInt32* vec;

    vec = lalloc(VECBYTES(data_00607668));
    bitvectorinitialize(vec, data_00607668, 0);
    return vec;
}

static void addblock(SInt32* count, BlockList301** list, UInt32* vec, MPCodeBlock* block) {
    BlockList301* bl;

    (*count)++;
    bl = lalloc(sizeof(BlockList301));
    bl->block = block;
    bl->next = *list;
    *list = bl;
    SETBIT(vec, block->number);
}

// 0x54A9E0: op is read in first..last before it is written.
Boolean func_0054A9E0(MPCode* first, MPCode* last, MOperand* op) {
    MPCode* stop;
    MPCode* pc;
    MOperand* o;
    int n;
    Boolean read;
    Boolean written;

    written = 0;
    read = 0;
    stop = last->next;
    for (pc = first; pc != stop; pc = pc->next) {
        for (n = pc->argcount, o = pc->args; n; n--, o++) {
            if (o->kind == op->kind && o->reg == op->reg && o->rclass == op->rclass) {
                if (o->flags & 2) {
                    written = 1;
                } else {
                    read = 1;
                }
            }
        }
        if (read) {
            return 1;
        }
        if (written) {
            return 0;
        }
    }
    return 0;
}

// 0x54AD40
static void walkloop(Loop301* loop, void (*func)(Loop301*), Boolean preorder) {
    Loop301* child;

    switch (loop->kind) {
        case LOOP301_LOOP:
        case LOOP301_ROOT:
            if (preorder) {
                func(loop);
            }
            for (child = loop->children; child; child = child->next) {
                walkloop(child, func, preorder);
            }
            if (!preorder) {
                func(loop);
            }
            break;
        case LOOP301_BLOCK:
            func(loop);
            break;
    }
}

// 0x54AD10
void func_0054AD10(void (*func)(Loop301*)) {
    Loop301* loop;

    for (loop = data_006258BC; loop; loop = loop->next) {
        walkloop(loop, func, 0);
    }
}

// 0x54B8A0: steps the live sets back over pc.
static void stepback(MPCode* pc) {
    MOperand* op;
    int n;
    UInt32* live;

    for (n = pc->argcount, op = pc->args; n; n--, op++) {
        if (op->kind == 0 && (op->flags & 2)) {
            live = data_0062541C[op->rclass].current;
            if (GETBIT(live, op->reg)) {
                data_00624E04[op->rclass].count--;
            }
            CLRBIT(live, op->reg);
        }
    }
    for (n = pc->argcount, op = pc->args; n; n--, op++) {
        if (op->kind == 0 && (op->flags & 1)) {
            live = data_0062541C[op->rclass].current;
            if (!GETBIT(live, op->reg)) {
                data_00624E04[op->rclass].count++;
            }
            SETBIT(live, op->reg);
        }
    }
}

// 0x54B760: the register pressure of every block
static void computepressure(void) {
    char rclass;
    MPCodeBlock* block;
    MPCode* pc;

    for (rclass = 0; rclass < 7; rclass++) {
        data_00624E04[rclass].perblock = oalloc(data_00624D40 * sizeof(SInt32));
        data_00624E04[rclass].avail = func_00462160(rclass);
    }
    for (block = data_00624FD0; block; block = block->next) {
        func_004C3460(block);
        for (rclass = 0; rclass < 7; rclass++) {
            data_00624E04[rclass].count =
                bitvectorcount(data_0062541C[rclass].current, data_0062541C[rclass].n);
        }
        for (rclass = 0; rclass < 7; rclass++) {
            data_00624E04[rclass].perblock[block->number] = data_00624E04[rclass].count;
        }
        for (pc = block->last; pc; pc = pc->prev) {
            stepback(pc);
            for (rclass = 0; rclass < 7; rclass++) {
                if ((UInt32)data_00624E04[rclass].perblock[block->number] <
                    (UInt32)data_00624E04[rclass].count) {
                    data_00624E04[rclass].perblock[block->number] = data_00624E04[rclass].count;
                }
            }
        }
    }
}

// 0x54AAD0
static void looppressure(Loop301* loop) {
    char rclass;
    BlockList301* bl;
    SInt32 v;

    switch (loop->kind) {
        case LOOP301_BLOCK:
            for (rclass = 0; rclass < 7; rclass++) {
                loop->pressure[rclass] = data_00624E04[rclass].perblock[loop->header->number];
            }
            break;
        case LOOP301_LOOP:
        case LOOP301_ROOT:
            for (rclass = 0; rclass < 7; rclass++) {
                loop->pressure[rclass] = 0;
            }
            for (bl = loop->blocks; bl; bl = bl->next) {
                for (rclass = 0; rclass < 7; rclass++) {
                    v = data_00624E04[rclass].perblock[bl->block->number];
                    if ((UInt32)v > (UInt32)loop->pressure[rclass]) {
                        loop->pressure[rclass] = v;
                    }
                }
            }
            break;
    }
}

// 0x54AAB0
void func_0054AAB0(void) {
    computepressure();
    func_0054AD10(looppressure);
}

#if MWCCPS2
// compiler/optimizer/LoopDetection.c defines these names too; unique names
// let config/replace.txt (base names) list this unit's copies
#define computedominators computedominators301
#define findloopheaders findloopheaders301
#define addlooptolist addlooptolist301
#endif
// 0x54B5B0: computedominators
static void computedominators(void) {
    SInt32 n;
    SInt32 i;
    int changed;
    UInt32* vec;
    MPCodeBlock* block;
    MPBlockLink* link;

    n = data_00624D40;
    data_00607678 = oalloc(n * sizeof(UInt32*));
    for (i = 0; i < data_00624D40; i++) {
        data_00607678[i] = oalloc(VECBYTES(n));
    }
    vec = oalloc(VECBYTES(n));
    bitvectorinitialize(data_00607678[data_00624FD0->number], n, 0);
    data_00607678[data_00624FD0->number][0] |= 1;
    for (block = data_00624FD0->next; block; block = block->next) {
        bitvectorinitialize(data_00607678[block->number], n, 0xFFFFFFFF);
    }
    computedepthfirstordering();
    do {
        changed = 0;
        for (i = 0; i < data_00624D40; i++) {
            block = data_00625418[i];
            if (!block || block->number == data_00624FD0->number) {
                continue;
            }
            link = block->predecessors;
            bitvectorcopy(vec, data_00607678[link->from->number], n);
            for (link = link->nextpred; link; link = link->nextpred) {
                bitvectorintersect(vec, data_00607678[link->from->number], n);
            }
            SETBIT(vec, block->number);
            if (bitvectorchanged(data_00607678[block->number], vec, n)) {
                changed = 1;
            }
        }
    } while (changed);
}

// 0x54B520: findloopheaders
static BlockList301* findloopheaders(void) {
    MPCodeBlock* block;
    MPBlockLink* link;
    BlockList301* bl;

    data_00607674 = NULL;
    data_00607670 = 0;
    for (block = data_00624FD0->next; block; block = block->next) {
        for (link = block->predecessors; link; link = link->nextpred) {
            if (GETBIT(data_00607678[link->from->number], block->number)) {
                bl = oalloc(sizeof(BlockList301));
                bl->block = block;
                bl->next = data_00607674;
                data_00607674 = bl;
                data_00607670++;
                break;
            }
        }
    }
    return data_00607674;
}

#if MWCCPS2
// compiler/optimizer/LoopDetection.c has a findnaturalloop too; a unique
// name lets config/replace.txt (base names) list this one
#define findnaturalloop findnaturalloop301
#endif
// 0x54B2E0: findnaturalloop
static void findnaturalloop(Loop301* loop) {
    int sp;
    MPCodeBlock* header;
    MPCodeBlock* block;
    MPBlockLink* link;
    BlockList301* bl;
    Boolean isexit;

    sp = 0;
    header = loop->header;
    if (!GETBIT(loop->membervec, header->number)) {
        addblock(&loop->nblocks, &loop->blocks, loop->membervec, header);
    }
    for (link = loop->header->predecessors; link; link = link->nextpred) {
        block = link->from;
        if (GETBIT(data_00607678[block->number], loop->header->number) && block != loop->header) {
            if (!GETBIT(loop->membervec, block->number)) {
                addblock(&loop->nblocks, &loop->blocks, loop->membervec, block);
            }
            data_0060766C[sp++] = block;
        }
    }
    while (sp) {
        block = data_0060766C[--sp];
        for (link = block->predecessors; link; link = link->nextpred) {
            if (!GETBIT(loop->membervec, link->from->number)) {
                addblock(&loop->nblocks, &loop->blocks, loop->membervec, link->from);
                data_0060766C[sp++] = link->from;
            }
        }
    }
    for (bl = loop->blocks; bl; bl = bl->next) {
        block = bl->block;
        isexit = 0;
        for (link = block->successors; link; link = link->next) {
            if (!GETBIT(loop->membervec, link->block->number)) {
                isexit = 1;
                break;
            }
        }
        if (isexit && !GETBIT(loop->exitvec, block->number)) {
            addblock(&loop->nexits, &loop->exits, loop->exitvec, block);
        }
    }
    if (loop->nexits == 0) {
        loop->flags |= 1;
    }
}

// 0x54B250: addlooptolist
static void addlooptolist(Loop301* loop, Loop301** list) {
    Loop301** scan;
    Loop301* l;

    scan = list;
    for (l = *scan; l; l = *scan) {
        if (GETBIT(l->membervec, loop->header->number)) {
            loop->parent = l;
            addlooptolist(loop, &l->children);
            return;
        }
        if (GETBIT(loop->membervec, l->header->number)) {
            *scan = l->next;
            l->parent = loop;
            l->next = loop->children;
            loop->children = l;
        } else {
            scan = &l->next;
        }
    }
    loop->next = *list;
    *list = loop;
}

// 0x54B150: the loop's preheader, made when its single outside predecessor
// falls through into the header.
static void insertpreheader(Loop301* loop) {
    MPCodeBlock* header;
    MPCodeBlock* pre;
    MPBlockLink* link;
    MPBlockLink* succ;
    Loop301* p;

    header = loop->header;
    pre = NULL;
    for (link = header->predecessors; link; link = link->nextpred) {
        if (!GETBIT(loop->membervec, link->from->number)) {
            if (pre) {
                return;
            }
            pre = link->from;
        }
    }
    if (pre) {
        succ = pre->successors;
        if (succ && !succ->next) {
            // one successor
        } else if (succ && succ->next && !succ->next->next && pre->next == header) {
            // two, falling through into the header
        } else {
            return;
        }
        if (pre->next == header) {
            pre = func_0046F660(pre, header);
            for (p = loop->parent; p; p = p->parent) {
                if (!GETBIT(p->membervec, pre->number)) {
                    addblock(&p->nblocks, &p->blocks, p->membervec, pre);
                }
            }
        }
    }
    loop->preheader = pre;
    if (!loop->children) {
        loop->flags |= 2;
    }
}

// 0x54B060: the root, holding every block
static void makeroot(void) {
    Loop301* root;
    Loop301* child;
    MPCodeBlock* block;

    root = newloop(LOOP301_ROOT);
    root->children = data_006258BC;
    data_006258BC = root;
    for (child = root->children; child; child = child->next) {
        child->parent = root;
    }
    root->nblocks = 0;
    root->blocks = NULL;
    root->membervec = newblockset();
    for (block = data_00624FD0; block; block = block->next) {
        if (!GETBIT(root->membervec, block->number)) {
            addblock(&root->nblocks, &root->blocks, root->membervec, block);
        }
    }
}

// 0x54AF30: a LOOP301_BLOCK leaf for each block not in an inner loop
static void addblockleaves(Loop301* loop) {
    UInt32* vec;
    BlockList301* bl;
    Loop301* child;
    Loop301** tail;
    Loop301* leaf;

    vec = oalloc(VECBYTES(data_00624D40));
    bitvectorinitialize(vec, data_00624D40, 0);
    for (bl = loop->blocks; bl; bl = bl->next) {
        SETBIT(vec, bl->block->number);
    }
    for (child = loop->children; child; child = child->next) {
        for (bl = child->blocks; bl; bl = bl->next) {
            CLRBIT(vec, bl->block->number);
        }
    }
    for (tail = &loop->children; *tail; tail = &(*tail)->next) {}
    for (bl = loop->blocks; bl; bl = bl->next) {
        if (GETBIT(vec, bl->block->number)) {
            leaf = newloop(LOOP301_BLOCK);
            leaf->header = bl->block;
            leaf->parent = loop;
            *tail = leaf;
            tail = &leaf->next;
        }
    }
}

// 0x54AB90: findloopsinflowgraph
Loop301* func_0054AB90(Boolean blocks) {
    Loop301* loop;

    data_006258BC = NULL;
    data_00624BA8 = 0;
    data_00607668 = data_00624D40 * 3;
    computedominators();
    if (findloopheaders()) {
        data_0060766C = oalloc(data_00624D40 * sizeof(MPCodeBlock*));
        for (; data_00607674; data_00607674 = data_00607674->next) {
            loop = newloop(LOOP301_LOOP);
            loop->header = data_00607674->block;
            loop->nblocks = 0;
            loop->blocks = NULL;
            loop->membervec = newblockset();
            loop->nexits = 0;
            loop->exits = NULL;
            loop->exitvec = newblockset();
            findnaturalloop(loop);
            addlooptolist(loop, &data_006258BC);
        }
        if (copts.debuglisting) {
            func_004AFFC0();
        }
        func_0054AD10(insertpreheader);
    }
    if (blocks) {
        makeroot();
        func_0054AD10(addblockleaves);
    }
    freeoheap();
    return data_006258BC;
}
