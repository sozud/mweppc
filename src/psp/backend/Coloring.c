#include "compiler/common.h"
#include "psp/BE.h"
#include "compiler/CFunc.h"
#include "compiler/objects.h"
#include "compiler/CodeGen.h"
#include "compiler/CompilerTools.h"
#include "compiler/CError.h"
#include "compiler/CParser.h"
#include "compiler/BitVectors.h"
#include "compiler/CMachine.h"

// mwccpsp 3.0.1 register coloring (functions near BE_binary.c in the link;
// the original unit name is not known). So far: the simplify step.

typedef struct IGEdge {
    struct IGEdge* hashnext; // data_005F3DBC chain
    struct IGEdge* next_lo; // +0x04: next edge for the lower-numbered end
    struct IGEdge* next_hi; // +0x08: next edge for the higher-numbered end
#if MWCCPS2
    SInt16 lo; // +0x0C (b210 compares them as signed shorts)
    SInt16 hi; // +0x0E
#else
    UInt16 lo; // +0x0C
    UInt16 hi; // +0x0E
#endif
} IGEdge;

typedef struct IGNode {
    struct IGNode* next; // coloring stack / candidate list
    struct Object* object; // +0x04: the variable, if any
    MPCode* def; // +0x08: the only instruction defining it, if one
    SInt32 cost; // +0x0C spill cost
    SInt16 index; // +0x10
    SInt16 color; // +0x12: register, or -1
    UInt16 flags; // +0x14: 2 removed, 4 precolored?, 0x80/0x400 never spill, 0x200 register pair
    SInt16 degree; // +0x16
    SInt16 nedges; // +0x18
    UInt8 pad1A[2];
    IGEdge* edges; // +0x1C
} IGNode; // 0x20

extern char data_00625ECC; // register class being colored
extern IGNode* data_0062520C; // interference graph nodes
extern SInt32 data_00624744[7]; // first node of the class (after the physical registers)
extern SInt32 data_00625A98[7]; // nodes in the class
extern SInt32 func_00462160(char rclass); // registers available in the class
void func_0054C170(void);

// Take a node out of the graph: lower its neighbours' degrees (a pair counts
// twice) and push it on the coloring stack.
#define REMOVE_NODE(node, stack) \
    do { \
        IGEdge* _e; \
        IGNode* _n; \
        for (_e = (node)->edges; _e; _e = ((node)->index < _e->hi) ? _e->next_lo : _e->next_hi) { \
            _n = &data_0062520C[((node)->index < _e->hi) ? _e->hi : _e->lo]; \
            if (((node)->flags & 0x200) || (_n->flags & 0x200)) { \
                _n->degree -= 2; \
            } else { \
                _n->degree -= 1; \
            } \
        } \
        (node)->flags |= 2; \
        (node)->next = (stack); \
        (stack) = (node); \
    } while (0)

// Move every node of degree < k onto the stack; collect the others in spill.
#define SIMPLIFY(stack, spill, k) \
    do { \
        SInt32 _changed; \
        UInt32 _i; \
        IGNode* _node; \
        do { \
            (spill) = NULL; \
            _changed = 0; \
            for (_i = data_00624744[data_00625ECC]; _i < data_00625A98[data_00625ECC]; _i++) { \
                _node = &data_0062520C[_i]; \
                if (_node->flags & 6) { \
                    continue; \
                } \
                if (_node->degree < (k)) { \
                    REMOVE_NODE(_node, stack); \
                    _changed = 1; \
                } else { \
                    _node->next = (spill); \
                    (spill) = _node; \
                } \
            } \
        } while (_changed); \
    } while (0)

// Chaitin-style simplify: returns the coloring stack. When only high-degree
// nodes remain, the one with the lowest cost/degree is pushed as a spill
// candidate. The original compares these ratios in x87 extended precision;
// the native build must do the same to choose identically.
IGNode* BE_SimplifyGraph(void) {
    SInt32 k;
    IGNode* stack;
    IGNode* spill;
    IGNode* node;
    IGNode* pick;
    float best;
    float cur;

    k = func_00462160(data_00625ECC);
    stack = NULL;
    SIMPLIFY(stack, spill, k);
    if (spill) {
        func_0054C170();
    }

    best = 0.0f;
    cur = 0.0f;
    while (spill) {
        pick = spill;
        if (spill->flags & 0x80) {
            cur = 3.4028234663852886e38f;
        } else if (spill->flags & 0x400) {
            cur = 1.7014117331926443e38f;
        } else {
            cur = (float)spill->cost / (float)spill->degree;
        }
        best = cur;
        for (node = spill->next; node; node = node->next) {
            if (node->flags & 0x80) {
                cur = 3.4028234663852886e38f;
            } else if (node->flags & 0x400) {
                cur = 1.7014117331926443e38f;
            } else {
                cur = (float)node->cost / (float)node->degree;
            }
            if (cur < best) {
                best = cur;
                pick = node;
            }
        }
        REMOVE_NODE(pick, stack);
        SIMPLIFY(stack, spill, k);
    }
    return stack;
}

extern SInt32 data_00625478[7]; // first register of the class's allocation order
extern SInt32 data_006241E0[7];
extern SInt16 data_00603F68;
extern SInt32 data_006242FC; // spill rounds so far
extern SInt32 data_00609810;
extern SInt32 data_00609814;
extern SInt32 data_00609818;
extern SInt32 data_0060981C;
extern SInt32 data_0062482C;
void func_004B3740(void); // build the interference graph
SInt32 BE_AssignColors(IGNode* node);
void BE_ApplyColoring(void);
void func_0054B9B0(void);
extern void func_00446C10(void);
extern void func_004AFFB0(SInt32 a, SInt32 b, SInt32 c, SInt32 d);

void BE_ColorRegisters(SInt32 arg) {
    RegClass301 rclass;
    SInt32 failed;

    for (rclass = 0; rclass < 7; rclass++) {
        data_00625ECC = rclass;
        if (data_00625A98[rclass] == data_00624744[rclass]) {
            continue;
        }
        data_00609810 = data_00609818 = data_00609814 = data_0060981C = 0;
        data_00603F68 = data_00625478[rclass];
        data_006242FC = 0;
        do {
            if (data_006242FC > 10) {
                CError_FATAL(408);
            }
#if MWCCPS2
            ((void (*)(SInt32))func_004B3740)(arg); // b210 passes the argument along (unused)
#else
            func_004B3740();
#endif
            failed = BE_AssignColors(BE_SimplifyGraph()) == 0;
            if (failed) {
                func_0054B9B0();
                data_006242FC++;
            } else {
                BE_ApplyColoring();
            }
            func_00446C10();
        } while (failed);
        data_00625A98[rclass] = data_00624744[rclass];
    }

    if (copts.debuglisting &&
        (data_00609810 > 0 || data_00609818 > 0 || data_00609814 > 0 || data_0060981C > 0)) {
        func_004AFFB0(data_00609810, data_00609814, data_00609818, data_0060981C);
    }
    data_0062482C = 0;
}

extern void* oalloc(SInt32 size);
#if MWCCPS2
extern UInt32 func_004620D0(char rclass); // free registers of the class (one word)
#else
extern UInt32* func_004620D0(char rclass); // free registers of the class
#endif
extern SInt32 func_00462090(char rclass); // next register from the allocation order
extern int bitvectorisempty(UInt32* a, int len);
#define OBJECT_VARINFO(obj) (*(char**)&((Object*)(obj))->u.var.info) // +0x28

// Pop the coloring stack, giving each node a free register. Returns 0 if some
// node could not be colored (it is marked for spilling).
#if MWCCPS2
// b210: register sets are one word (classes have at most 32 registers)
SInt32 BE_AssignColors(IGNode* node) {
    UInt32 avail;
    IGEdge* e;
    IGNode* nb;
    SInt32 r;
    SInt32 reg;
    SInt32 ok;
    UInt32 freeregs;

    data_00625478[data_00625ECC] = data_00603F68;
    ok = 1;
    freeregs = func_004620D0(data_00625ECC);
    while (node) {
        avail = freeregs;
        for (e = node->edges; e; e = (node->index < e->hi) ? e->next_lo : e->next_hi) {
            nb = &data_0062520C[(node->index < e->hi) ? e->hi : e->lo];
            if (nb->color != -1) {
                if (nb->flags & 0x200) {
                    avail &= ~(3 << nb->color);
                } else {
                    avail &= ~(1 << nb->color);
                }
            }
        }
        if (avail) {
            if (node->flags & 0x200) {
                for (r = 0; r < data_00624744[data_00625ECC]; r += 2) {
                    if ((avail & (3 << r)) == (3 << r)) {
                        node->color = r;
                        break;
                    }
                }
            } else {
                for (r = 0; r < data_00624744[data_00625ECC]; r++) {
                    if ((1 << r) & avail) {
                        node->color = r;
                        break;
                    }
                }
            }
        }
        if (node->color == -1) {
            reg = (SInt16)func_00462090(data_00625ECC);
            if (reg != -1) {
                freeregs |= 1 << reg;
                continue; // try this node again with one more register
            }
        }
        if (node->color == -1) {
            node->flags |= 1;
            ok = 0;
        }
        node = node->next;
    }
    return ok;
}
#else
SInt32 BE_AssignColors(IGNode* node) {
    SInt32 k;
    SInt32 ok;
    UInt32* avail;
    UInt32* freeregs;
    IGEdge* e;
    IGNode* nb;
    SInt32 r;
    SInt16 reg;

    k = func_00462160(data_00625ECC);
    ok = 1;
    data_00625478[data_00625ECC] = data_00603F68;
    avail = oalloc(((k + 31) >> 5) * 4);
    freeregs = func_004620D0(data_00625ECC);
    while (node) {
        bitvectorcopy(avail, freeregs, k);
        for (e = node->edges; e; e = (node->index < e->hi) ? e->next_lo : e->next_hi) {
            nb = &data_0062520C[(node->index < e->hi) ? e->hi : e->lo];
            if (nb->color != -1) {
                if (nb->flags & 0x200) {
                    bitvectorclearbit(nb->color, avail);
                    bitvectorclearbit(nb->color + 1, avail);
                } else {
                    bitvectorclearbit(nb->color, avail);
                }
            }
        }
        if (!bitvectorisempty(avail, k)) {
            if (node->flags & 0x200) {
                for (r = 0; r < data_00624744[data_00625ECC]; r += 2) {
                    if (bitvectorgetbit(r, avail) && bitvectorgetbit(r + 1, avail)) {
                        node->color = r;
                        break;
                    }
                }
            } else {
                for (r = 0; r < data_00624744[data_00625ECC]; r++) {
                    if (bitvectorgetbit(r, avail)) {
                        node->color = r;
                        break;
                    }
                }
            }
        }
        if (node->color == -1) {
            reg = func_00462090(data_00625ECC);
            if (reg != -1) {
                bitvectorsetbit(reg, freeregs);
                continue; // try this node again with one more register
            }
        }
        if (node->color == -1) {
            node->flags |= 1;
            ok = 0;
        }
        node = node->next;
    }
    return ok;
}
#endif

// Rewrite register operands of the current class to their colors, drop
// moves that became no-ops, and record each variable's register.
#if MWCCPS2
#define COLOR_VU0OP(op) ((op) > 0x21E && (op) <= 0x2E7)
#endif
void BE_ApplyColoring(void) {
    MPCodeBlock* block;
    MPCode* pc;
    MOperand* op;
    SInt32 n;
    UInt32 i;
    IGNode* node;

    for (block = data_00624FD0; block; block = block->next) {
        for (pc = block->first; pc; pc = pc->next) {
            n = pc->argcount;
            op = pc->args;
            while (n--) {
                if (op->kind == 0 && op->rclass == data_00625ECC) {
                    op->reg = data_0062520C[op->reg].color;
                }
                op++;
            }
#if MWCCPS2
            // b210: VU0 instructions (0x21F..0x2E7) have the dest field first
            if ((pc->flags & 0x10) &&
                data_00625ECC == (COLOR_VU0OP(pc->op) ? &pc->args[2] : &pc->args[1])->rclass &&
                (COLOR_VU0OP(pc->op) ? &pc->args[2] : &pc->args[1])->reg ==
                    (COLOR_VU0OP(pc->op) ? &pc->args[1] : &pc->args[0])->reg) {
                deletepcode(pc);
            }
#else
            if ((pc->flags & 0x10) &&
                pc->args[1].rclass == data_00625ECC &&
                pc->args[1].reg == pc->args[0].reg) {
                deletepcode(pc);
            }
#endif
        }
    }

    for (i = data_00624744[data_00625ECC]; i < data_00625A98[data_00625ECC]; i++) {
        node = &data_0062520C[i];
        if (node->object && !(node->flags & 1)) {
#if MWCCPS2
            if (node->flags & 0x10) {
                ((VarInfo*)OBJECT_VARINFO(node->object))->regHi = node->color;
            } else {
                ((VarInfo*)OBJECT_VARINFO(node->object))->reg = node->color;
            }
#else
            if (node->flags & 0x10) {
                *(SInt16*)(OBJECT_VARINFO(node->object) + 0x2A) = node->color;
            } else {
                *(SInt16*)(OBJECT_VARINFO(node->object) + 0x28) = node->color;
            }
#endif
        }
    }
}

extern SInt16* data_005F3DA8; // node -> representative, for coalescing
extern void* lalloc(SInt32 size);
void func_004B42D0(void);
void func_004B3EE0(void);
SInt32 func_004B3B80(void);
extern void func_004B3910(void);
extern void func_004B3A70(void);
extern void func_004B3800(void);
extern void func_004C34A0(void);
extern void func_004C3460(MPCodeBlock* block);
extern SInt32 func_004C3310(MPCode* pc); // nonzero if pc is dead
extern void func_004C33C0(MPCode* pc);

// Coalesce copies in every register class.
void func_004B3650(void) {
    UInt32 n;
    UInt32 i;

    for (data_00625ECC = 0; data_00625ECC < 7; data_00625ECC++) {
        n = data_00625A98[data_00625ECC];
        if (n == data_00624744[data_00625ECC]) {
            continue;
        }
        data_005F3DA8 = lalloc(n * 2);
        for (i = 0; i < n; i++) {
            data_005F3DA8[i] = i;
        }
        do {
            func_00446C10();
            func_004B42D0();
            func_004B3EE0();
        } while (func_004B3B80());
        func_004B3910();
        func_004B3A70();
        func_00446C10();
    }
}

// Build the interference graph of the current class.
void func_004B3740(void) {
    UInt32 n;
    UInt32 i;

    n = data_00625A98[data_00625ECC];
    data_005F3DA8 = lalloc(n * 2);
    for (i = 0; i < n; i++) {
        data_005F3DA8[i] = i;
    }
    do {
        func_00446C10();
        func_004B42D0();
        func_004B3EE0();
    } while (func_004B3B80());
    func_004B3910();
    func_004B3A70();
    func_004B3800();
}

// Remove instructions whose results are never used, until nothing changes.
void func_004B35D0(void) {
    SInt32 changed;
    MPCodeBlock* block;
    MPCode* pc;

    do {
        changed = 0;
        func_004C34A0();
        for (block = data_00624FD0; block; block = block->next) {
            if (!(block->flags2E & 0x180) && block->pcodecount) {
                func_004C3460(block);
                for (pc = block->last; pc; pc = pc->prev) {
                    if (func_004C3310(pc)) {
                        changed = 1;
                        deletepcode(pc);
                    } else {
                        func_004C33C0(pc);
                    }
                }
            }
        }
        func_00446C10();
    } while (changed);
}

extern SInt32 func_004901D0(MPCode* pc); // keep this single definition?
#define VARINFO_REG(vi) (*(SInt16*)&((VarInfo*)(vi))->reg) // +0x28
#define VARINFO_REGHI(vi) (*(SInt16*)&((VarInfo*)(vi))->regHi) // +0x2A
#define VARINFO_RCLASS(vi) (*(UInt8*)&((VarInfo*)(vi))->rclass) // +0x27
#define VARINFO_FLAGS(vi) (*(UInt8*)&((VarInfo*)(vi))->flags) // +0x26

// Record each virtual register's single defining instruction.
void func_004B3800(void) {
    RegClass301 rclass;
    UInt32 n;
    UInt32 i;
    MPCodeBlock* block;
    MPCode* pc;
    MOperand* op;
    SInt32 k;
    IGNode* node;

    rclass = data_00625ECC;
    n = data_00625A98[rclass];
    for (block = data_00624FD0; block; block = block->next) {
        for (pc = block->last; pc; pc = pc->prev) {
            k = pc->argcount;
            op = pc->args;
            while (k--) {
                if (op->kind == 0 &&
                    op->rclass == data_00625ECC &&
                    (op->flags & 2) &&
                    op->reg >= data_00624744[rclass]) {
                    node = &data_0062520C[op->reg];
                    if (!(node->flags & 0x30) && !(node->flags & 0x40)) {
                        if (!node->def) {
                            node->def = pc;
                        } else {
                            node->def = NULL;
                            node->flags |= 0x40;
                        }
                    }
                }
                op++;
            }
        }
    }
    for (i = 0; i < n; i++) {
        node = &data_0062520C[i];
        if (node->def && !func_004901D0(node->def)) {
            node->def = NULL;
        }
    }
}

static void attach_variables(ObjectList* list) {
    Object* obj;
    char* vi;

    for (; list; list = list->next) {
        obj = list->object;
        vi = OBJECT_VARINFO(obj);
        if (obj->datatype == DLOCAL && VARINFO_REG(vi) && VARINFO_RCLASS(vi) == data_00625ECC) {
            data_0062520C[VARINFO_REG(vi)].object = obj;
            if (obj->datatype == DLOCAL && VARINFO_REGHI(OBJECT_VARINFO(obj))) {
                data_0062520C[VARINFO_REG(vi)].flags |= 0x20;
                data_0062520C[VARINFO_REGHI(vi)].object = obj;
                data_0062520C[VARINFO_REGHI(vi)].flags |= 0x10;
            }
        }
    }
}

// Physical registers color themselves; attach register variables to nodes.
void func_004B3910(void) {
    UInt32 i;

    for (i = 0; i < data_00624744[data_00625ECC]; i++) {
        data_0062520C[i].color = i;
    }
    attach_variables(arguments);
    attach_variables(locals);
}

// After coalescing: flag merged nodes, color the ones merged into a physical
// register, and mark variables whose node absorbed others.
#if MWCCPS2 // b210 builds this one from InterferenceGraph.c (its assert names it)
#line 580 "InterferenceGraph.c"
#endif
void func_004B3A70(void) {
    UInt32 n;
    UInt32 i;
    SInt16 r;
    IGNode* node;

    n = data_00625A98[data_00625ECC];
    for (i = 0; i < n; i++) {
        node = &data_0062520C[i];
        if (i != data_005F3DA8[i]) {
            r = i;
            node->flags |= 4;
            if ((UInt32)r > data_00625A98[data_00625ECC]) {
                CError_FATAL(400);
            }
            while (r != data_005F3DA8[r]) {
                r = data_005F3DA8[r];
            }
            data_0062520C[r].flags |= 8;
            if ((UInt32)r < data_00624744[data_00625ECC]) {
                node->color = r;
            }
        }
    }
    for (i = 0; i < n; i++) {
        node = &data_0062520C[i];
        if (node->object && (node->flags & 8)) {
            VARINFO_FLAGS(OBJECT_VARINFO(node->object)) |= 0x10;
        }
    }
}
#if MWCCPS2
#line 614 "Coloring.c"
#endif

extern IGEdge* data_005F3DBC[0x4000]; // edge hash table

// Allocate and reset the nodes of the current class; clear the edge table.
void func_004B42D0(void) {
    UInt32 n;
    UInt32 i;
    IGNode* node;

    n = data_00625A98[data_00625ECC];
    data_0062520C = oalloc(n * sizeof(IGNode));
    for (i = 0; i < n; i++) {
        node = &data_0062520C[i];
        node->next = NULL;
        node->object = NULL;
        node->def = NULL;
        node->cost = 0;
        node->index = i;
        node->color = -1;
        node->flags = 0;
        node->degree = 0;
        node->nedges = 0;
        node->edges = NULL;
    }
    for (i = 0; i < 0x4000; i++) {
        data_005F3DBC[i] = NULL;
    }
}

extern UInt32 data_005F3DAC;
extern UInt32 data_005F3DB0; // size of the call-argument register set
extern SInt32* data_005F3DB4; // its dense array
extern UInt32* data_005F3DB8; // its index array
extern SInt32 data_0060E038[7][BE_MAX_CLASS_REGS]; // caller-saved registers of each class
extern void func_004C3190(char rclass); // compute liveness
void func_004B46E0(UInt32 a, UInt32 b);
extern SInt32 func_004B8990(void);
extern SInt32 func_00461F50(void);

#define IN_SET(r, dense, sparse, count) ((sparse)[r] < (count) && (dense)[(sparse)[r]] == (r))

// Build interference edges for the current class from liveness, walking
// each block backwards with a sparse set of live registers.
void func_004B3EE0(void) {
    RegClass301 rclass;
    UInt32 n;
    UInt32 nphys;
    UInt32 i;
    UInt32 j;
    UInt32* dense;
    UInt32* sparse;
    UInt32 count;
    UInt32 idx;
    UInt32 last;
    UInt32* liveout;
    MPCodeBlock* block;
    MPCode* pc;
    MOperand* op;
    SInt32 k;
    SInt32 r;

    rclass = data_00625ECC;
    n = data_00625A98[rclass];
    nphys = data_00624744[rclass];
    data_005F3DAC = n;
    data_005F3DB0 = 0;
    data_005F3DB4 = oalloc(n * 4);
    data_005F3DB8 = oalloc(n * 4);
    data_005F3DB0 = 0;
    for (i = 0; i < nphys; i++) {
        for (j = 0; j < nphys; j++) {
            if (i != j) {
                func_004B46E0(i, j);
            }
        }
    }

    func_004C3190(data_00625ECC);
    dense = oalloc(n * 4);
    sparse = oalloc(n * 4);
    for (block = data_00624FD0; block; block = block->next) {
        liveout = data_0062541C[data_00625ECC].blocks[block->number].liveout;
        count = 0;
        for (i = 0; i < n; i++) {
            if (bitvectorgetbit(i, liveout)) {
                sparse[i] = count;
                dense[count++] = i;
            }
        }

        for (pc = block->last; pc; pc = pc->prev) {
            for (k = pc->argcount, op = pc->args; k--; op++) {
                if (op->kind == 0 && op->rclass == data_00625ECC && (op->flags & 2)) {
                    for (j = 0; j < count; j++) {
#if MWCCPS2
                        if (!(pc->flags & 0x10) ||
                            (COLOR_VU0OP(pc->op) ? &pc->args[2] : &pc->args[1])->reg !=
                                (SInt32)dense[j]) {
#else
                        if (!(pc->flags & 0x10) || pc->args[1].reg != (SInt32)dense[j]) {
#endif
                            func_004B46E0(op->reg, dense[j]);
                        }
                    }
                }
            }
            for (k = pc->argcount, op = pc->args; k--; op++) {
                if (op->kind == 0 && op->rclass == data_00625ECC && (op->flags & 2)) {
                    r = op->reg;
                    idx = sparse[r];
                    if (idx < count && dense[idx] == r) {
                        last = dense[--count];
                        dense[idx] = last;
                        sparse[last] = idx;
                    }
                }
            }
            for (k = pc->argcount, op = pc->args; k--; op++) {
                if (op->kind == 0 && op->rclass == data_00625ECC && (op->flags & 1)) {
                    r = op->reg;
                    if (!IN_SET(r, dense, sparse, count)) {
                        op->flags |= 4; // not live below: last use
                    }
                    r = op->reg;
                    if (!IN_SET(r, dense, sparse, count)) {
                        sparse[r] = count;
                        dense[count++] = r;
                    }
                }
            }
            if (pc->flags & 8) { // call: argument registers
                for (i = func_00461F50() + 2 - func_004B8990(), op = &pc->args[i];
                     i < (UInt32)pc->argcount;
                     i++, op++) {
                    if (op->rclass == data_00625ECC) {
                        for (j = 0; j < data_006241E0[data_00625ECC]; j++) {
                            func_004B46E0(op->reg, data_0060E038[data_00625ECC][j]);
                        }
                        r = op->reg;
                        if (!IN_SET(r, data_005F3DB4, data_005F3DB8, data_005F3DB0)) {
                            data_005F3DB8[r] = data_005F3DB0;
                            data_005F3DB4[data_005F3DB0++] = r;
                        }
                    }
                }
            }
        }
    }
}

// Add an interference edge between nodes a and b (once).
void func_004B46E0(UInt32 a, UInt32 b) {
    UInt32 h;
    IGEdge* e;
    IGNode* na;
    IGNode* nb;

    if (a < b) {
        h = ((((b - 1) * b) >> 1) + a) & 0x3FFF;
        for (e = data_005F3DBC[h]; e; e = e->hashnext) {
            if ((SInt16)e->lo == a && (SInt16)e->hi == b) {
                return;
            }
        }
        na = &data_0062520C[a];
        nb = &data_0062520C[b];
        e = oalloc(sizeof(IGEdge));
        e->hashnext = data_005F3DBC[h];
        data_005F3DBC[h] = e;
        e->next_lo = na->edges;
        na->edges = e;
        e->next_hi = nb->edges;
        nb->edges = e;
        e->lo = a;
        e->hi = b;
        na->nedges++;
        nb->nedges++;
        na->degree++;
        nb->degree++;
        if ((na->flags & 0x200) || (nb->flags & 0x200)) {
            na->degree++;
            nb->degree++;
        }
    } else if (a > b) {
        h = ((((a - 1) * a) >> 1) + b) & 0x3FFF;
        for (e = data_005F3DBC[h]; e; e = e->hashnext) {
            if ((SInt16)e->hi == a && (SInt16)e->lo == b) {
                return;
            }
        }
        na = &data_0062520C[a];
        nb = &data_0062520C[b];
        e = oalloc(sizeof(IGEdge));
        e->hashnext = data_005F3DBC[h];
        data_005F3DBC[h] = e;
        e->next_lo = nb->edges;
        nb->edges = e;
        e->next_hi = na->edges;
        na->edges = e;
        e->lo = b;
        e->hi = a;
        na->nedges++;
        nb->nedges++;
        na->degree++;
        nb->degree++;
        if ((na->flags & 0x200) || (nb->flags & 0x200)) {
            na->degree++;
            nb->degree++;
        }
    }
}

SInt32 func_004B4650(UInt32 a, UInt32 b);
Boolean func_004B89A0(SInt32 a, SInt32 b);
extern void func_004AFFD0(SInt32 keep, SInt32 gone);

// Representative of a coalesced node.
static SInt16 coalesce_find(SInt16 r) {
    if ((UInt32)r > data_00625A98[data_00625ECC]) {
        CError_FATAL(400);
    }
    while (r != data_005F3DA8[r]) {
        r = data_005F3DA8[r];
    }
    return r;
}

// Merge the operands of register copies that do not interfere; returns
// nonzero if anything was merged.
SInt32 func_004B3B80(void) {
    SInt32 changed;
    MPCodeBlock* block;
    MPCode* pc;
    MOperand* op;
    SInt32 k;
    SInt16 src;
    SInt16 dst;
    SInt16 keep;
    SInt16 gone;
    IGNode* node;
    IGEdge* e;
#if MWCCPS2
    int vu;
#endif

    changed = 0;
    for (block = data_00624FD0; block; block = block->next) {
        for (pc = block->first; pc; pc = pc->next) {
#if MWCCPS2
            // b210 (0x4BE880): a VU0 copy (0x21F..0x2E7) has its operands
            // one slot later
            if (!(pc->flags & 0x10)) {
                continue;
            }
            vu = pc->op > 0x21E && pc->op <= 0x2E7;
            if (pc->args[vu ? 2 : 1].rclass != data_00625ECC) {
                continue;
            }
            src = coalesce_find(pc->args[vu ? 2 : 1].reg);
            dst = coalesce_find(pc->args[vu ? 1 : 0].reg);
#else
            if (!(pc->flags & 0x10) || pc->args[1].rclass != data_00625ECC) {
                continue;
            }
            src = coalesce_find(pc->args[1].reg);
            dst = coalesce_find(pc->args[0].reg);
#endif
            if (src == dst) {
                deletepcode(pc);
                continue;
            }
            if (func_004B4650(src, dst) || func_004B89A0(src, dst)) {
                continue;
            }
            keep = (src < dst) ? src : dst;
            gone = (src > dst) ? src : dst;
            if (IN_SET(gone, data_005F3DB4, data_005F3DB8, data_005F3DB0)) {
                continue;
            }
            data_005F3DA8[gone] = keep;
            node = &data_0062520C[gone];
            for (e = node->edges; e; e = (node->index < e->hi) ? e->next_lo : e->next_hi) {
                func_004B46E0(keep, (node->index < e->hi) ? e->hi : e->lo);
            }
            deletepcode(pc);
            changed = 1;
            if (copts.debuglisting) {
                func_004AFFD0(keep, gone);
            }
        }
    }

    for (block = data_00624FD0; block; block = block->next) {
        for (pc = block->first; pc; pc = pc->next) {
            for (k = pc->argcount, op = pc->args; k--; op++) {
                if (op->kind == 0 &&
                    op->rclass == data_00625ECC &&
                    op->reg != data_005F3DA8[op->reg]) {
                    op->reg = coalesce_find(op->reg);
                }
            }
        }
    }
    return changed;
}

// Do nodes a and b interfere?
SInt32 func_004B4650(UInt32 a, UInt32 b) {
    IGEdge* e;

    if (a < b) {
        for (e = data_005F3DBC[((((b - 1) * b) >> 1) + a) & 0x3FFF]; e; e = e->hashnext) {
            if ((SInt16)e->lo == a && (SInt16)e->hi == b) {
                return 1;
            }
        }
    } else if (a > b) {
        for (e = data_005F3DBC[((((a - 1) * a) >> 1) + b) & 0x3FFF]; e; e = e->hashnext) {
            if ((SInt16)e->hi == a && (SInt16)e->lo == b) {
                return 1;
            }
        }
    }
    return 0;
}

// Never coalesce into or out of $zero (register 0 of the GPR class).
#if !MWCCPS2 // mwccps2 has it in RegisterInfo.c
Boolean func_004B89A0(SInt32 a, SInt32 b) {
    if (data_00625ECC == 0) {
        return (a == 0) || (b == 0);
    }
    return 0;
}
#endif

// Spill costs of the current class: uses and definitions weighted by the
// block's loop weight.
void func_0054C170(void) {
    MPCodeBlock* block;
    MPCode* pc;
    MOperand* op;
    SInt32 k;
    SInt32 weight;
    IGNode* node;

    for (block = data_00624FD0; block; block = block->next) {
        weight = copts.optimizesize ? 1 : block->weight;
        for (pc = block->first; pc; pc = pc->next) {
            for (k = pc->argcount, op = pc->args; k--; op++) {
                if (op->kind == 0 && op->rclass == data_00625ECC && (op->flags & 1)) {
                    node = &data_0062520C[op->reg];
                    if (op->flags & 0x10) {
                        node->flags |= 0x400;
                    }
                    if (op->flags & 0x40) {
                        node->flags |= 0x80;
                    }
                    if (node->def || copts.optimizesize) {
                        node->cost += weight;
                    } else {
                        node->cost += weight * 2;
                    }
                }
            }
            for (k = pc->argcount, op = pc->args; k--; op++) {
                if (op->kind == 0 && op->rclass == data_00625ECC && (op->flags & 2)) {
                    node = &data_0062520C[op->reg];
                    if (op->flags & 0x10) {
                        node->flags |= 0x400;
                    }
                    if (op->flags & 0x40) {
                        node->flags |= 0x80;
                    }
                    if (node->def || (pc->flags & 0x8000)) {
                        node->cost -= weight;
                    } else {
                        node->cost += weight;
                    }
                }
            }
        }
    }
}

void func_0054BB30(void);
extern UInt16 func_0047FFD0(Type* type); // store opcode for a type
extern UInt16 func_0047FD20(Type* type, SInt32* extra); // load opcode for a type
extern MPCode* func_00480420(int op, SInt16 nargs);
extern SInt32 func_0048FBE0(UInt16 op); // bytes an opcode moves
extern struct Alias* func_004817A0(Object* obj, SInt32 offset, SInt32 size); // make_alias
extern SInt16 data_00625E3E; // frame register
#define OBJECT_FRAMEOFFSET(obj) (*(SInt32*)&((Object*)(obj))->u.var.offset) // +0x30

// Byte offset of the node's half within a register-pair variable.
#define PAIR_OFFSET(node, off) \
    do { \
        off = 0; \
        if ((node)->flags & 0x10) { \
            off = copts.littleendian ? 4 : 0; \
        } else if ((node)->flags & 0x20) { \
            off = copts.littleendian ? 0 : 4; \
        } \
    } while (0)

static void frame_operands(MPCode* pc, Object* obj, SInt32 off) {
    pc->args[1].kind = 4;
    MOP_SETOBJ(pc->args[1], obj);
    pc->args[1].value = OBJECT_FRAMEOFFSET(obj) + off;
    pc->alias =
        func_004817A0((Object*)MOP_OBJ(pc->args[1]), pc->args[1].value, func_0048FBE0(pc->op));
    pc->args[2].kind = 0;
    pc->args[2].rclass = 0;
    pc->args[2].reg = data_00625E3E;
    pc->args[2].flags = 1;
}

// A store of reg to the node's stack slot.
MPCode* func_004B8A00(SInt16 reg, IGNode* node) {
    MPCode* pc;
    Object* obj = node->object;
    Type* type = obj->type;
    SInt32 off;
    UInt16 op;

    PAIR_OFFSET(node, off);
    op = func_0047FFD0(type);
    if (data_00625ECC == 5 && op == MACRO_OP(0x7F)) {
        op = MACRO_OP(0x80);
    }
    pc = func_00480420(op, 3);
    pc->args[0].kind = 0;
    pc->args[0].rclass = data_00625ECC;
    pc->args[0].reg = reg;
    pc->args[0].flags = 1;
    if (pc->op == MACRO_OP(0x7F)) {
        pc->args[0].flags |= 0x2000;
    }
    frame_operands(pc, obj, off);
    return pc;
}

// A load of reg from the node's stack slot.
MPCode* func_004B8B00(SInt16 reg, IGNode* node) {
    Type* type = node->object->type;
    SInt32 off;
    SInt32 extra;
    UInt16 op;
    MPCode* pc;

    PAIR_OFFSET(node, off);
    op = func_0047FD20(type, &extra);
    if (op == MACRO_OP(0x5D) && data_00625ECC == 5) {
        op = MACRO_OP(0x5E);
    }
    pc = func_00480420(op, 3);
    pc->args[0].kind = 0;
    pc->args[0].rclass = data_00625ECC;
    pc->args[0].reg = reg;
    pc->args[0].flags = 2;
    if (pc->op == MACRO_OP(0x5D)) {
        pc->args[0].flags |= 0x2000;
    }
#if MWCCPS2
    if (pc->op ==
        (UInt16)((copts.mips_isa >= 2 && (copts.fp_library & 2)) ? MACRO_OP(0x4B)
                                                                 : MACRO_OP(0x48))) {
#else
    if (pc->op ==
        ((copts.mips_isa >= 2 && (copts.fp_library & 2)) ? MACRO_OP(0x4B) : MACRO_OP(0x48))) {
#endif
        pc->args[0].flags |= 0x8000;
        if (copts.mips_option1A) {
            pc->args[0].flags |= 0x20;
        }
    }
    frame_operands(pc, node->object, off);
    return pc;
}

#if !MWCCPS2 // mwccps2 has it in RegisterInfo.c
// The type a spill slot of the current class holds.
Type* func_004B89D0(IGNode* node) {
    if (data_00625ECC != 0) {
        if (copts.floatsp32) {
            return TYPE(&stfloat);
        }
        return TYPE(&stlongdouble);
    }
    return TYPE(&stunsignedlong);
}
#else
Type* func_004B89D0(IGNode* node);
#endif
#define SPILL_ASSERT(line, cond) \
    do { \
        if (!(cond)) { \
            CError_Internal("Spill.c", (line)); \
        } \
    } while (0)

// Reload rematerializable value: a copy of the defining instruction, writing reg.
static MPCode* remat(MPCode* def, SInt16 reg) {
    MPCode* copy = copypcode(def);
    SPILL_ASSERT(0x76, copy->args[(copy->flags & 0x80000000) ? 1 : 0].kind == 0);
    copy->args[(copy->flags & 0x80000000) ? 1 : 0].reg = reg;
    return copy;
}

// Rewrite a register move whose source or destination was spilled.
void func_0054BD20(MPCodeBlock* block, MPCode* pc) {
#if MWCCPS2
    // b210 (0x55E0A0): a VU0 instruction (0x21F..0x2E7) has its operands
    // one slot later
    int vu = pc->op > 0x21E && pc->op <= 0x2E7;
    SInt16 src = pc->args[vu ? 2 : 1].reg;
    SInt16 dst = pc->args[vu ? 1 : 0].reg;
#else
    SInt16 src = pc->args[1].reg;
    SInt16 dst = pc->args[0].reg;
#endif
    IGNode* srcnode = &data_0062520C[src];
    IGNode* dstnode = &data_0062520C[dst];
    SInt16 tmp;

    if (!(srcnode->flags & 1)) {
        insertpcodebefore(pc, func_004B8A00(src, dstnode));
        data_00609814++;
        data_0060981C += block->weight;
    } else if (!(dstnode->flags & 1)) {
        if (srcnode->def) {
            insertpcodebefore(pc, remat(srcnode->def, dst));
        } else {
            insertpcodebefore(pc, func_004B8B00(dst, srcnode));
            data_00609810++;
            data_00609818 += block->weight * 2;
        }
    } else {
        tmp = data_00625A98[data_00625ECC]++;
        if (srcnode->def) {
            insertpcodebefore(pc, remat(srcnode->def, tmp));
        } else {
            insertpcodebefore(pc, func_004B8B00(tmp, srcnode));
            data_00609810++;
            data_00609818 += block->weight * 2;
        }
        insertpcodebefore(pc, func_004B8A00(tmp, dstnode));
        data_00609814++;
        data_0060981C += block->weight;
    }
    deletepcode(pc);
}

// Give each spilled register pc reads or writes a fresh temporary, loaded
// before pc and stored after it.
void func_0054BF20(MPCodeBlock* block, MPCode* pc) {
    SInt32 limit = data_00625A98[data_00625ECC];
    SInt32 i;
    SInt32 j;
    MOperand* op;
    MOperand* op2;
    SInt16 reg;
    IGNode* node;
    SInt16 tmp;
    SInt32 uses;
    SInt32 defs;

    for (i = 0, op = pc->args; i < pc->argcount; i++, op++) {
        if (op->kind != 0 || op->rclass != data_00625ECC || op->reg >= limit) {
            continue;
        }
        reg = op->reg;
        node = &data_0062520C[reg];
        if (!(node->flags & 1)) {
            continue;
        }
        tmp = data_00625A98[data_00625ECC]++;
        defs = 0;
        uses = 0;
        for (j = i, op2 = op; j < pc->argcount; j++, op2++) {
            if (op2->kind == 0 && op2->rclass == data_00625ECC && op2->reg == reg) {
                if (op2->flags & 1) {
                    uses++;
                }
                if (op2->flags & 2) {
                    defs++;
                }
                op2->reg = tmp;
                op2->flags |= 0x40;
            }
        }
        if (uses) {
            if (node->def) {
                insertpcodebefore(pc, remat(node->def, tmp));
            } else {
                insertpcodebefore(pc, func_004B8B00(tmp, node));
                data_00609810++;
                data_00609818 += block->weight * 2;
            }
        }
        if (defs) {
            if (node->def || (pc->flags & 0x8000)) {
                deletepcode(pc);
            } else {
                insertpcodeafter(pc, func_004B8A00(tmp, node));
                data_00609814++;
                data_0060981C += block->weight;
            }
        }
    }
}

#define IS_SPILLED(op) \
    ((op)->kind == 0 && (op)->rclass == data_00625ECC && (data_0062520C[(op)->reg].flags & 1))

// Rewrite every instruction that uses a spilled register of the class.
void func_0054B9B0(void) {
    MPCodeBlock* block;
    MPCode* pc;
    MPCode* next;
    MOperand* op;
    MOperand* src;
    MOperand* dst;
    SInt32 k;
    SInt32 n;
    SInt32 i;
    SInt32 spilled;

    func_0054BB30();
    for (block = data_00624FD0; block; block = block->next) {
        for (pc = block->first; pc; pc = next) {
            next = pc->next;
            n = pc->argcount;
            spilled = 0;
            for (k = n, op = pc->args; k--; op++) {
                if (IS_SPILLED(op)) {
                    spilled = 1;
                    break;
                }
            }
            if (!spilled) {
                continue;
            }
            if (pc->flags & 0x10) {
                func_0054BD20(block, pc);
                continue;
            }
            if (pc->flags & 8) {
                i = func_00461F50() + 2 - func_004B8990();
                for (dst = src = &pc->args[i]; i < n; i++, src++) {
                    if (IS_SPILLED(src)) {
                        pc->argcount--;
                    } else {
                        *dst++ = *src;
                    }
                }
            }
            func_0054BF20(block, pc);
        }
    }
}

extern void func_004A4660(Object* obj); // give a local a stack frame slot
extern void func_004AFFE0(SInt32 index, SInt32 cost, SInt32 nedges, Object* obj);
#define VARINFO_23(vi) (*(UInt8*)&((VarInfo*)(vi))->used) // +0x23

// Give every spilled node of the class a stack location.
void func_0054BB30(void) {
    UInt32 i;
    UInt32 n;
    IGNode* node;
    Object* obj;
    Type* type;
    Type* t2;

    n = data_00625A98[data_00625ECC];
    for (i = data_00624744[data_00625ECC]; i < n; i++) {
        node = &data_0062520C[i];
        if (!(node->flags & 1)) {
            continue;
        }
        if (node->def) {
            if (node->object) {
                VARINFO_23(OBJECT_VARINFO(node->object)) = 0;
            }
            continue;
        }

        if (node->flags & 0x20) {
            if (!(VARINFO_FLAGS(OBJECT_VARINFO(node->object)) & 1)) {
                func_004A4660(node->object);
            }
            data_0062520C[i + 1].flags |= 1;
        } else if (node->flags & 0x10) {
            if (!(data_0062520C[i - 1].flags & 1)) {
                data_0062520C[i - 1].flags |= 1;
                if (!(VARINFO_FLAGS(OBJECT_VARINFO(node->object)) & 1)) {
                    func_004A4660(node->object);
                }
            }
        } else if ((obj = node->object)) {
            if (!(VARINFO_FLAGS(OBJECT_VARINFO(obj)) & 1)) {
                if (VARINFO_FLAGS(OBJECT_VARINFO(obj)) & 0x10) {
                    type = obj->type;
                    t2 = func_004B89D0(node);
                    if (type->size < t2->size) {
                        node->object->type = t2;
                        func_004A4660(node->object);
                        node->object->type = type;
                        VARINFO_FLAGS(OBJECT_VARINFO(node->object)) |= 8;
                        goto done;
                    }
                }
                func_004A4660(obj);
            }
        } else {
            t2 = func_004B89D0(node);
            obj = lalloc(sizeof(Object));
            memclrw(obj, sizeof(Object));
            obj->otype = OT_OBJECT;
            obj->name = CParser_GetUniqueName();
            obj->type = t2;
            obj->datatype = DLOCAL;
            OBJECT_VARINFO(obj) = (char*)CodeGen_GetNewVarInfo();
            node->object = obj;
            func_004A4660(node->object);
        }
done:
        VARINFO_REG(OBJECT_VARINFO(node->object)) = 0;
        VARINFO_REGHI(OBJECT_VARINFO(node->object)) = 0;
        if (copts.debuglisting) {
            func_004AFFE0(i, node->cost, node->nedges, node->object);
        }
    }
}
