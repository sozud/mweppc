// mwccpsp 3.0.1 peephole framework (0x4AFBE0..0x4AFFF0; unit name not known).
// The patterns are in MIPS_Peephole.c, which its asserts name.
#include "compiler/common.h"
#include "compiler/CParser.h"
#include "compiler/CompilerTools.h"
#include "compiler/objects.h"
#include "psp/BE.h"

typedef struct PeepholeFunc {
    struct PeepholeFunc* next;
    int (*func)(MPCode* pc);
} PeepholeFunc;

extern SInt32 data_00625A98[7]; // registers used, per class
extern PeepholeFunc* data_005F0F10[]; // patterns per opcode (0x5C5)
extern MPCode** data_005F0EF4[7]; // defining instruction, per class and register
extern MPCode* data_005F0EF0; // stands for "defined before the block"

extern SInt16 func_004903F0(void); // the nop opcode
extern void func_004C3460(MPCodeBlock* block);
extern SInt32 func_004C3310(MPCode* pc);
extern void func_004C33C0(MPCode* pc);
extern void func_004C34A0(void);
void func_005469C0(void); // registers the patterns (MIPS_Peephole.c)

// 0x4AFD20
void func_004AFD20(UInt16 op, int (*func)(MPCode* pc)) {
    PeepholeFunc* p;

    p = oalloc(sizeof(PeepholeFunc));
    p->func = func;
    p->next = data_005F0F10[op];
    data_005F0F10[op] = p;
}

// 0x4AFD50: runs the patterns over a block, last instruction first
static int func_004AFD50(MPCodeBlock* block) {
    MPCode* pc;
    PeepholeFunc* p;
    int changed;

    changed = 0;
    func_004C3460(block);
    for (pc = block->last; pc; pc = pc->prev) {
        if (!func_004C3310(pc)) {
            for (p = data_005F0F10[pc->op]; p;) {
                if (p->func(pc)) {
                    changed = 1;
                    if (!pc->block) {
                        break;
                    }
                    p = data_005F0F10[pc->op];
                } else {
                    p = p->next;
                }
            }
            if (pc->block) {
                func_004C33C0(pc);
            }
        } else {
            deletepcode(pc);
        }
    }
    return changed;
}

// 0x4AFDD0: is the register used by an instruction between a and b?
int func_004AFDD0(MPCode* a, MPCode* b, MOperand* reg) {
    MPCode* pc;
    MOperand* op;
    int n;

    for (pc = a->prev; pc != b; pc = pc->prev) {
        for (op = pc->args, n = pc->argcount; n--; op++) {
            if (op->kind == reg->kind &&
                op->reg == reg->reg &&
                op->rclass == reg->rclass &&
                (op->flags & 1)) {
                return 1;
            }
        }
    }
    return 0;
}

// 0x4AFE40: ... defined by one?
int func_004AFE40(MPCode* a, MPCode* b, MOperand* reg) {
    MPCode* pc;
    MOperand* op;
    int n;

    for (pc = a->prev; pc != b; pc = pc->prev) {
        for (op = pc->args, n = pc->argcount; n--; op++) {
            if (op->kind == reg->kind &&
                op->reg == reg->reg &&
                op->rclass == reg->rclass &&
                (op->flags & 2)) {
                return 1;
            }
        }
    }
    return 0;
}

// 0x4AFEB0: every register use in the block learns its defining instruction
#if MWCCPS2
void func_004AFEB0(MPCodeBlock* block) { // b210: pipeline1.c calls it too
#else
static void func_004AFEB0(MPCodeBlock* block) {
#endif
    RegClass301 rclass;
    int i;
    MPCode* pc;
    MOperand* op;

    for (rclass = 0; rclass < 7; rclass++) {
        for (i = 0; i < data_00625A98[rclass]; i++) {
            data_005F0EF4[rclass][i] = data_005F0EF0;
        }
    }
    for (pc = block->first; pc; pc = pc->next) {
        for (i = 0, op = pc->args; i < pc->argcount; i++, op++) {
            if (op->kind == 0 && (op->flags & 1)) {
                MOP_SETOBJ(*op, data_005F0EF4[op->rclass][op->reg]);
            }
        }
        for (i = 0, op = pc->args; i < pc->argcount; i++, op++) {
            if (op->kind == 0 && (op->flags & 2)) {
                data_005F0EF4[op->rclass][op->reg] = pc;
            }
        }
    }
}

#if MWCCPS2
// b210 0x4BBC10: the def tables alone, for pipeline1.c
void ps2_peepinit_4BBC10(void) {
    RegClass301 rclass;

    for (rclass = 0; rclass < 7; rclass++) {
        data_005F0EF4[rclass] = oalloc(data_00625A98[rclass] * sizeof(MPCode*));
    }
    data_005F0EF0 = makepcode(func_004903F0());
}
#endif

// 0x4AFBE0: the peephole pass
void func_004AFBE0(Object* func) {
    RegClass301 rclass;
    int i;
    MPCodeBlock* block;

    for (rclass = 0; rclass < 7; rclass++) {
        data_005F0EF4[rclass] = oalloc(data_00625A98[rclass] * sizeof(MPCode*));
    }
    data_005F0EF0 = makepcode(func_004903F0());
    for (i = 0; i < BE_NUM_OPDEFS; i++) {
        data_005F0F10[i] = NULL;
    }
    func_005469C0();
    func_004C34A0();
    for (block = data_00624FD0; block; block = block->next) {
        if (!(block->flags2E & 0x180) && block->pcodecount) {
            do {
                func_004AFEB0(block);
            } while (func_004AFD50(block));
        }
    }
    freeoheap();
}

// 0x4AFF90..0x4AFFE0: listing hooks, empty in the release build.
int func_004AFF90() {
    return 0;
}

int func_004AFFA0() {
    return 0;
}

void func_004AFFB0() {}

void func_004AFFC0() {}

void func_004AFFD0() {}

void func_004AFFE0() {}
