#include "compiler/mwcc_core.h"
#include "compiler/common.h"
#include "psp/BE.h"
#include "compiler/InlineAsmMIPS.h"
#include "compiler/CMangler.h"
#include "compiler/CParser.h"
#include "compiler/objects.h"
#include "compiler/CompilerTools.h"

// mwccpsp 3.0.1 branch delay slots (around 0x4B7D80; the unit name is a
// guess). Unscheduled code gets a nop after every branch; a branch to the
// block that follows is deleted.

extern SInt16 func_004903F0(void); // the nop opcode
extern MOperand* func_0048FAE0(MPCode* pc); // a branch's label operand
extern void func_004C34A0(void);
void func_004B7E20(MPCodeBlock* block, Boolean special);
MPCode* func_004B88B0(MPCodeBlock* block);
extern void func_00446C10(void);
extern void BE_DumpPCode(char* name, char* label);
void func_004B83A0(MPCodeBlock* block);

#define LABEL_BLOCK(op) (((MPCodeLabel*)(op)->ptr)->block)

static MPCodeBlock* nextnonempty(MPCodeBlock* block) {
    MPCodeBlock* next;

    for (next = block->next; next; next = next->next) {
        if (next->pcodecount) {
            return next;
        }
    }
    return NULL;
}

#if MWCCPS2
extern void ps2_padloop_4C2960(MPCodeBlock* block); // b210 only: #pragma padloop
#endif
#if MWCCPS2 && !MWCC_CORE
// mwccps2 (b198 0x004C3DA0, b210): the opcode and flags in locals, the padding as an else (the PS2
// b198 decompilation's fill_block_delay_slot_simple)
void func_004B8240(MPCodeBlock* block) {
    MPCode* last;
    MPCode* nop;
    MPCodeBlock* target;
    MPCodeBlock* next;
    UInt16 opcode;
    UInt32 flags;

    target = NULL;
    if (block->flags2E & 0x4000) {
        return;
    }
    last = block->last;
    if (!last || !((flags = last->flags) & 0x20000000)) {
        return;
    }
    opcode = last->op;
    if ((opcode == 0x39 || opcode == 0x38A || opcode == 0x38B || (flags & 0x10000000)) &&
        last->unk38 == 0 &&
        !(flags & 0x800000)) {
        target = LABEL_BLOCK(func_0048FAE0(last));
        next = nextnonempty(block);
        if (next && target && target->number == next->number) {
            deletepcode(last);
            return;
        }
    }
    if (last->prev && (last->prev->flags & 0x40000000)) {
        if (target && target->number == last->block->number && target->pcodecount == 2) {
            nop = makepcode(func_004903F0());
            insertpcodebefore(last, nop);
        }
        return;
    }
    nop = makepcode(func_004903F0());
    nop->unk20 = last->unk20;
    nop->flags |= 0x40000000;
    insertpcodebefore(last, nop);
    if (target && target->number == last->block->number && target->pcodecount <= 7) {
        if (copts.padloop) {
            ps2_padloop_4C2960(block);
        } else {
            nop = makepcode(func_004903F0());
            insertpcodebefore(last, nop);
        }
    }
}
#else
void func_004B8240(MPCodeBlock* block) {
    MPCode* last;
    MPCodeBlock* target;
    MPCodeBlock* next;
    MPCode* nop;

    target = NULL;
    if (block->flags2E & 0x4000) {
        return;
    }
    last = block->last;
    if (!last || !(last->flags & 0x20000000)) {
        return;
    }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    if ((last->op == OPC(0x39) ||
         last->op == OPC(0x38A) ||
         last->op == OPC(0x38B) ||
         (last->flags & 0x10000000)) &&
        !last->unk38 &&
        !(last->flags & 0x800000)) {
#else
    if ((
            last->op == 0x39 || last->op == 0x38A || last->op == 0x38B || (last->flags & 0x10000000)
        ) &&
        !last->unk38 &&
        !(last->flags & 0x800000)) {
#endif
        target = LABEL_BLOCK(func_0048FAE0(last));
        next = nextnonempty(block);
        if (next && target && target->number == next->number) {
            // a jump to the next block
            deletepcode(last);
            return;
        }
    }
    if (last->prev && (last->prev->flags & 0x40000000)) {
        if (target && target->number == last->block->number && target->pcodecount == 2) {
            insertpcodebefore(last, makepcode(func_004903F0()));
        }
        return;
    }
    nop = makepcode(func_004903F0());
    nop->unk20 = last->unk20;
    nop->flags |= 0x40000000;
    insertpcodebefore(last, nop);
    if (target && target->number == last->block->number && target->pcodecount <= 7) {
#if MWCCPS2
        // b210: #pragma padloop pads the short loop instead
        if (copts.padloop) {
            ps2_padloop_4C2960(block);
            return;
        }
#endif
        insertpcodebefore(last, makepcode(func_004903F0()));
    }
}
#endif

// The branch optimizer after scheduling (0x4B7A50..0x4B7D80): branches to
// a block that only jumps go straight to its target, a jump to the next
// block goes, and a block reached only from a jump is pulled up into it.
// Per-opcode information (PCodeInfo.c), 0x30 bytes each.
// the opcode table's flags (data_00611610 is data_006115F8[0].flags): OpcodeDef
// in InlineAsmMIPS.h

extern void func_0046F9B0(void); // recompute the predecessor lists

static Boolean changed;
static void func_004B7AE0(MPCodeBlock* block);
static int func_004B7D10(MPCodeBlock* block);

#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
#define IS_JUMP(pc) ((pc)->op == OPC(0x39) || (pc)->op == OPC(0x38A) || (pc)->op == OPC(0x38B))
#else
#define IS_JUMP(pc) ((pc)->op == 0x39 || (pc)->op == 0x38A || (pc)->op == 0x38B)
#endif

void func_004B7A50(void) {
    MPCodeBlock* block;

    do {
        changed = FALSE;
        for (block = data_00624FD0; block; block = block->next) {
            if (!(block->flags2E & 0x80) && !(block->flags2E & 0x4000)) {
                func_004B7AE0(block);
                if (changed) {
                    break;
                }
            }
        }
        if (changed) {
            for (block = data_00624FD0; block; block = block->next) {
                block->predecessors = NULL;
            }
            func_0046F9B0();
        }
    } while (changed);
}

#if MWCCPS2 && !(MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52)
// b210/b198 (simplify_block, 0x4C35C0)
static void func_004B7AE0(MPCodeBlock* block) {
    MPCode* last;
    MPCode* first;
    MPCode* pc;
    MPCode* next;
    MOperand* op;
    MOperand* targetOp;
    MPCodeBlock* target;
    MPBlockLink* link;
    MPBlockLink* edge;
    int moved;

    last = block->last;
    moved = 0;
    if (!last) {
        return;
    }
    if (last->unk38 == 5 || last->unk38 == 6) {
        return;
    }
    if (last->flags & 0x10000000) {
        op = func_0048FAE0(last);
        target = LABEL_BLOCK(op);
        first = target->first;
        if (first && IS_JUMP(first)) {
            targetOp = func_0048FAE0(first);
            *op = *targetOp;
            edge = block->successors;
            for (;;) {
                if (edge->block->number == target->number) {
                    if (edge->block == LABEL_BLOCK(op)) {
                        return;
                    }
                    edge->block = LABEL_BLOCK(op);
                    changed = TRUE;
                    return;
                }
                edge = edge->next;
            }
        }
    }
    link = block->successors;
    if (!link || link->next) {
        return;
    }
    target = link->block;
    if (target->number == block->number) {
        return;
    }
    if (IS_JUMP(last) && block->next && target->number == block->next->number) {
        deletepcode(last);
        changed = TRUE;
        return;
    }
    if (!func_004B7D10(target)) {
        return;
    }
    if (data_006115F8[last->op].flags & 8) {
        return;
    }
    if (last->flags & 1) {
        return;
    }
    link = target->predecessors;
    if (link && !link->nextpred) {
        if (link->from->number != block->number) {
            return;
        }
        if ((pc = target->first) != NULL) {
            moved = 1;
            for (; pc; pc = next) {
                next = pc->next;
                deletepcode(pc);
                appendpcode(block, pc);
            }
        }
    }
    if (moved) {
        changed = TRUE;
        block->successors = target->successors;
    }
}
#else
static void func_004B7AE0(MPCodeBlock* block) {
    MPCode* last;
    MOperand* op;
    MPCodeBlock* target;
    MPBlockLink* link;
    MPCode* pc;
    MPCode* next;
    int moved;
#if MWCCPS2
    MPCodeBlock* lb;
#endif

    last = block->last;
    moved = 0;
    if (!last) {
        return;
    }
    if (last->unk38 == 5 || last->unk38 == 6) {
        return;
    }
    if (last->flags & 0x10000000) {
        op = func_0048FAE0(last);
        target = LABEL_BLOCK(op);
        if (target->first && IS_JUMP(target->first)) {
            *op = *func_0048FAE0(target->first);
#if MWCCPS2
            link = block->successors;
            while (1) {
                lb = link->block;
                if (lb->number != target->number) {
                    link = link->next;
                } else {
                    break;
                }
            }
            if (lb != LABEL_BLOCK(op)) {
#else
            for (link = block->successors; link->block->number != target->number;
                 link = link->next) {}
            if (link->block != LABEL_BLOCK(op)) {
#endif
                link->block = LABEL_BLOCK(op);
                changed = TRUE;
            }
            return;
        }
    }
    link = block->successors;
    if (!link || link->next) {
        return;
    }
    target = link->block;
    if (target->number == block->number) {
        return;
    }
    if (IS_JUMP(last) && block->next && target->number == block->next->number) {
        deletepcode(last);
        changed = TRUE;
        return;
    }
    if (!func_004B7D10(target)) {
        return;
    }
    if (data_006115F8[last->op].flags & 8) {
        return;
    }
    if (last->flags & 1) {
        return;
    }
    link = target->predecessors;
    if (link && !link->nextpred && link->from->number == block->number && target->first) {
        moved = 1;
        for (pc = target->first; pc; pc = next) {
            next = pc->next;
            deletepcode(pc);
            appendpcode(block, pc);
        }
    }
    if (moved) {
        changed = TRUE;
        block->successors = target->successors;
    }
}
#endif

// does the block end in a jump (or a return) to another block?
static int func_004B7D10(MPCodeBlock* block) {
    MPCode* last;

    last = block->last;
    if (!last) {
        return 0;
    }
    if (!block->successors) {
        return 0;
    }
    if (!block->successors->block || block->successors->block->number == block->number) {
        return 0;
    }
    if (IS_JUMP(last)) {
        return 1;
    }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    if (last->op == OPC(0x3B) && last->args[0].reg == 31) {
#else
    if (last->op == 0x3B && last->args[0].reg == 31) {
#endif
        return 1;
    }
    return 0;
}

#define IS_UNCOND_JUMP(op) ((op) == 0x39 || (op) == 0x38A || (op) == 0x38B)

// A jump the delay-slot filler can fold into a branch: a plain 0x39 (no
// register), possibly after a filled delay slot (0xBA).
static MPCode* foldablejump(MPCode* pc, Boolean remove) {
    MPCode* next;

#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    if (pc->op == OPC(0x39) && pc->args[0].value2 == 0) {
#else
    if (pc->op == 0x39 && pc->args[0].value2 == 0) {
#endif
        if (remove) {
            deletepcode(pc);
        }
        return pc;
    }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    if (pc->op == OPC(0xBA) &&
        (pc->flags & 0x40000000) &&
        (next = pc->next) &&
        next->op == OPC(0x39) &&
#else
    if (pc->op == 0xBA &&
        (pc->flags & 0x40000000) &&
        (next = pc->next) &&
        next->op == 0x39 &&
#endif
        next->args[0].value2 == 0) {
        if (remove) {
            deletepcode(pc);
            deletepcode(next);
        }
        return next;
    }
    return NULL;
}

// 0x4B7E20: fills the delay slot of a scheduled block's branch: with an
// instruction of the block, else (optimizing, not special) with the
// target's first instruction (moved when the branch is its only way in,
// copied otherwise; conditional branches become branch-likely), else a nop.
#if MWCCPS2
#include "compiler/CError.h"
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
#include "compiler/BitVectors.h"
#endif

// b210 (0x4C3C50): the same delay-slot filler; the folded jump test is
// inline, and on the way out a short loop (the branch back to its own
// block, at most 7 instructions) is padded under #pragma padloop
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52

// 0x4B1220: may pc be moved up into a delay slot on the fall-through path?
static Boolean b69_movable(MPCode* pc) {
    if (!pc) {
        return 0;
    }
    if (pc->flags & 0x8000000) {
        return 0;
    }
    if (pc->flags & 4) {
        return 0;
    }
    if (pc->flags & 2) {
        return 0;
    }
    if (pc->flags & 0x40000000) {
        return 0;
    }
    return 1;
}

// 0x4B1280: is the register pc writes live into or out of block?
static Boolean b69_writeslive(MPCode* pc, MPCodeBlock* block) {
    MOperand* op = &pc->args[(pc->flags & 0x80000000) ? 1 : 0];
    BlockLiveness* bl = &data_0062541C[op->rclass].blocks[block->number];

    if (bitvectorgetbit(op->reg, bl->liveout) || bitvectorgetbit(op->reg, bl->livein)) {
        return 1;
    }
    return 0;
}

// 0x4B1470: does an instruction of block use op's register with a flag in
// mask? (b69 compares the kind and number, not the class)
static Boolean b69_blockuses(MPCodeBlock* block, MOperand* op, int mask) {
    MPCode* pc;
    MOperand* arg;
    int i;

    for (pc = block->first; pc; pc = pc->next) {
        for (i = 0, arg = pc->args; i < pc->argcount; i++, arg++) {
            if (op->kind == arg->kind && op->reg == arg->reg && (arg->flags & mask)) {
                return 1;
            }
        }
    }
    return 0;
}

// 0x4B14D0: does block have a memory operand?
static Boolean b69_blockhasmem(MPCodeBlock* block) {
    MPCode* pc;
    MOperand* arg;
    int i;

    for (pc = block->first; pc; pc = pc->next) {
        for (i = 0, arg = pc->args; i < pc->argcount; i++, arg++) {
            if (arg->kind == 4) {
                return 1;
            }
        }
    }
    return 0;
}

// 0x4B1510: is pc independent of everything block does?
static Boolean b69_independent(MPCode* pc, MPCodeBlock* block) {
    MOperand* arg;
    int i;

    for (i = 0, arg = pc->args; i < pc->argcount; i++, arg++) {
        switch (arg->kind) {
            case 0:
                if ((arg->flags & 2) && b69_blockuses(block, arg, 3)) {
                    return 0;
                }
                if ((arg->flags & 1) && b69_blockuses(block, arg, 2)) {
                    return 0;
                }
                break;
            case 1:
                return 0;
            case 2:
            case 3:
            case 6:
                break;
            case 4:
                if (b69_blockhasmem(block)) {
                    return 0;
                }
                break;
            default:
                CError_Internal("BE_schedule.c", 615);
                break;
        }
    }
    return 1;
}

#endif
// b198 (0x4C3C66) copies a target's first instruction marked 6 into a
// branch-likely slot too: only 5 stays out, tested before the move
#if MWCCPS2 && MWCCPS2_BUILD >= 198 && MWCCPS2_BUILD < 210
#define B198_SLOT_COPY 1
#else
#define B198_SLOT_COPY 0
#endif
void func_004B7E20(MPCodeBlock* block, Boolean special) {
    MPCode* br;
    MPCode* pc;
    MPCode* first;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    MPCode* fallfirst;
#else
    MPCode* next;
    MPCode* jump;
#endif
    MPCodeBlock* target = NULL;
    MPCodeBlock* follow;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    MPCodeBlock* fall;
    MPBlockLink* succ;
#endif
    MPBlockLink* pred;
#if !MWCC_CORE || MWCC_CORE > MWCC_CORE_B52
    int i;
#endif

    if (block->flags2E & 0x4000) {
        return;
    }
    if (!block->pcodecount) {
        return;
    }
    br = block->last;
    if (!(br->flags & 0x20000000)) {
        return;
    }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    if ((IS_UNCOND_JUMP(br->op) || (br->flags & 0x10000000)) &&
        br->unk38 == 0 &&
        !(br->flags & 0x800000)) {
#else
    if ((IS_UNCOND_JUMP(br->op) || (br->flags & 0x10000000)) &&
        br->unk38 == 0 &&
        !(br->flags & 0x800000)) {
#endif
        target = LABEL_BLOCK(func_0048FAE0(br));
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        follow = nextnonempty(block);
#else
        for (follow = block->next; follow; follow = follow->next) {
            if (follow->pcodecount) {
                break;
            }
        }
#endif
        if (follow && target->number == follow->number) {
            deletepcode(br);
            return;
        }
    }
    if ((pc = func_004B88B0(block))) {
        insertpcodebefore(br, pc);
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        return;
#else
        goto done;
#endif
    }
    if (special || !copts.no_load_delay_nops) {
        goto nop;
    }
    if (IS_UNCOND_JUMP(br->op)) {
        target = LABEL_BLOCK(func_0048FAE0(br));
        first = target->first;
        if (!first || (first->flags & 0x48000000)) {
            goto nop;
        }
        pred = target->predecessors;
        if (pred && !pred->nextpred) {
            deletepcode(first);
            insertpcodebefore(br, first);
            first->flags |= 0x40000000;
        } else {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            if (first->unk38 == 5) {
#else
            if (first->unk38 == 5 || first->unk38 == 6) {
#endif
                goto nop;
            }
            pc = copypcode(first);
            insertpcodebefore(br, pc);
            pc->flags |= 0x40000000;
            br->flags |= 0x4000000;
            first->flags |= 0x8000000;
        }
        freeoheap();
        func_004C34A0();
        return;
    }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    if (!(br->flags & 0x10000000)) {
#else
    if (!(br->flags & 0x10000000) || copts.no_branch_likely || (br->flags & 0x800000)) {
#endif
        goto nop;
    }
    target = LABEL_BLOCK(func_0048FAE0(br));
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    succ = block->successors;
    fall = succ->block;
    if (fall->number == target->number && succ->next) {
        fall = succ->next->block;
#else
    first = target->first;
    if (!first || (first->flags & 0x48000100)) {
        goto nop;
#endif
    }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    fallfirst = fall->first;
    first = target->first;
    if (first && !(first->flags & 0x48000106) && !b69_writeslive(first, block)) {
#endif
#if MWCCPS2 && MWCCPS2_BUILD >= 198 && MWCCPS2_BUILD < 210
        // b198 (0x4C3AB7) keeps an instruction marked 5 out of a branch-likely slot
        if (first->unk38 == 5) {
            goto nop;
        }
#endif
        pred = target->predecessors;
        if (pred && !pred->nextpred) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            goto move;
#else
        br->op++;
        br->flags |= 0x800000;
        next = first->next;
        deletepcode(first);
        insertpcodebefore(br, first);
        first->flags |= 0x40000000;
        if (!next) {
            goto done;
#endif
        }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        for (; pred; pred = pred->nextpred) {
            if (pred->from->number != block->number && pred->from->number != fall->number) {
                goto copy;
#else
        if (next->op == 0x39 && next->args[0].value2 == 0) {
            jump = next;
            deletepcode(next);
        } else if (next->op == 0xBA &&
                   (next->flags & 0x40000000) &&
                   (jump = next->next) &&
                   jump->op == 0x39 &&
                   jump->args[0].value2 == 0) {
            deletepcode(next);
            deletepcode(jump);
        } else {
            jump = NULL;
#endif
            }
#if !MWCC_CORE || MWCC_CORE > MWCC_CORE_B52
            if (!jump) {
                goto done;
#endif
            }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            if (fall->predecessors &&
                !fall->predecessors->nextpred &&
                b69_independent(first, fall)) {
                goto move;
#else
    for (i = 0; i < br->argcount; i++) {
        if (br->args[i].kind == 6) {
            MOP_SETLABEL(br->args[i], jump->args[0].ptr);
            goto done;
#endif
            }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
copy:
            if (first->unk38 == 5) {
#else
        CError_ASSERT(572, i < br->argcount);
    }
    goto done;
}
if (!B198_SLOT_COPY && (first->unk38 == 5 || first->unk38 == 6)) {
#endif
                goto nop;
            }
            pc = copypcode(first);
            insertpcodebefore(br, pc);
            pc->flags |= 0x40000000;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            br->flags |= 0x4000000;
#else
br->op++;
br->flags |= 0x4800000;
#endif
            first->flags |= 0x8000000;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            freeoheap();
            func_004C34A0();
            return;
#else
jump = first->next;
if (!jump) {
    goto done;
#endif
        }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        if (fallfirst &&
            !(fallfirst->flags & 0x48000106) &&
            fall->predecessors &&
            !fall->predecessors->nextpred &&
            b69_movable(fallfirst) &&
            !b69_writeslive(fallfirst, block)) {
            first = fallfirst;
            goto move;
#else
if (jump->op == 0x39 && jump->args[0].value2 == 0) {
} else if (jump->op == 0xBA &&
           (jump->flags & 0x40000000) &&
           (jump = jump->next) &&
           jump->op == 0x39 &&
           jump->args[0].value2 == 0) {
} else {
    jump = NULL;
#endif
        }
#if !MWCC_CORE || MWCC_CORE > MWCC_CORE_B52
        if (!jump) {
            goto done;
        }
        br->flags &= ~0x4000000;
        for (i = 0; i < br->argcount; i++) {
            if (br->args[i].kind == 6) {
                MOP_SETLABEL(br->args[i], jump->args[0].ptr);
                if ((first = ((MPCodeLabel*)jump->args[0].ptr)->block->first)) {
                    first->flags |= 0x8000000;
                }
                goto done;
            }
            CError_ASSERT(643, i < br->argcount);
        }
        goto done;
#endif

nop:
        pc = makepcode(func_004903F0());
        pc->flags |= 0x40000000;
        insertpcodebefore(br, pc);
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
#if MWCC_CORE == MWCC_CORE_B52
        if (copts.padloop &&
            target &&
            br->block->number == target->number &&
            target->pcodecount <= 7) {
#else
        if (copts.padloop &&
            target &&
            br->block->number == target->number &&
            target->pcodecount == 2) {
#endif
#else
done : if (copts.padloop &&
           target &&
           br->block->number == target->number &&
           target->pcodecount <= 7) {
#endif
            ps2_padloop_4C2960(block);
        }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        return;

move:
        deletepcode(first);
        insertpcodebefore(br, first);
        first->flags |= 0x40000000;
        freeoheap();
        func_004C34A0();
#endif
    }
#else
void func_004B7E20(MPCodeBlock* block, Boolean special) {
    MPCode* br;
    MPCode* pc;
    MPCode* first;
    MPCode* next;
    MPCode* jump;
    MPCodeBlock* target;
    MPCodeBlock* follow;
    MPBlockLink* pred;
    int i;

    if (block->flags2E & 0x4000) {
        return;
    }
    if (!block->pcodecount) {
        return;
    }
    br = block->last;
    if (!(br->flags & 0x20000000)) {
        return;
    }
    if ((IS_UNCOND_JUMP(br->op) || (br->flags & 0x10000000)) &&
        br->unk38 == 0 &&
        !(br->flags & 0x800000)) {
        target = LABEL_BLOCK(func_0048FAE0(br));
        for (follow = block->next; follow; follow = follow->next) {
            if (follow->pcodecount) {
                break;
            }
        }
        if (follow && target->number == follow->number) {
            deletepcode(br);
            return;
        }
    }
    if ((pc = func_004B88B0(block))) {
        insertpcodebefore(br, pc);
        return;
    }
    if (special || !copts.no_load_delay_nops) {
        goto nop;
    }
    if (IS_UNCOND_JUMP(br->op)) {
        target = LABEL_BLOCK(func_0048FAE0(br));
        first = target->first;
        if (!first || (first->flags & 0x48000000)) {
            goto nop;
        }
        pred = target->predecessors;
        if (pred && !pred->nextpred) {
            deletepcode(first);
            insertpcodebefore(br, first);
            first->flags |= 0x40000000;
        } else {
            if (first->unk38 == 5 || first->unk38 == 6) {
                goto nop;
            }
            pc = copypcode(first);
            insertpcodebefore(br, pc);
            pc->flags |= 0x40000000;
            br->flags |= 0x4000000;
            first->flags |= 0x8000000;
        }
        freeoheap();
        func_004C34A0();
        return;
    }
    if (!(br->flags & 0x10000000) || copts.no_branch_likely || (br->flags & 0x800000)) {
        goto nop;
    }
    target = LABEL_BLOCK(func_0048FAE0(br));
    first = target->first;
    if (!first || (first->flags & 0x48000100)) {
        goto nop;
    }
    pred = target->predecessors;
    if (pred && !pred->nextpred) {
        br->op++;
        br->flags |= 0x800000;
        next = first->next;
        deletepcode(first);
        insertpcodebefore(br, first);
        first->flags |= 0x40000000;
        if (!next || !(jump = foldablejump(next, 1))) {
            return;
        }
        for (i = 0; i < br->argcount; i++) {
            if (br->args[i].kind == 6) {
                MOP_SETLABEL(br->args[i], jump->args[0].ptr);
                return;
            }
        }
        return;
    }
    if (first->unk38 == 5 || first->unk38 == 6) {
        goto nop;
    }
    pc = copypcode(first);
    insertpcodebefore(br, pc);
    pc->flags |= 0x40000000;
    br->op++;
    br->flags |= 0x4800000;
    first->flags |= 0x8000000;
    next = first->next;
    if (!next || !(jump = foldablejump(next, 0))) {
        return;
    }
    br->flags &= ~0x4000000;
    for (i = 0; i < br->argcount; i++) {
        if (br->args[i].kind == 6) {
            MOP_SETLABEL(br->args[i], jump->args[0].ptr);
            if ((first = ((MPCodeLabel*)jump->args[0].ptr)->block->first)) {
                first->flags |= 0x8000000;
            }
            return;
        }
    }
    return;

nop:
    pc = makepcode(func_004903F0());
    pc->flags |= 0x40000000;
    insertpcodebefore(br, pc);
}
#endif

    void BE_InsertNops(Object * func, int scheduled, Boolean special) {
        MPCodeBlock* block;

        if (!scheduled) {
            for (block = data_00624FD0; block; block = block->next) {
                func_004B8240(block);
            }
        } else {
            func_004C34A0();
            for (block = data_00624FD0; block; block = block->next) {
                func_004B7E20(block, special);
            }
            func_00446C10();
        }
        if (copts.debuglisting) {
            BE_DumpPCode(CMangler_GetLinkName(func)->name, "After Branch Delay Slot");
        }
        for (block = data_00624FD0; block; block = block->next) {
            if (!(block->flags2E & 0x4000)) {
                func_004B83A0(block);
            }
        }
    }

    // Does user read op's register? Floating-point registers are compared by
    // even/odd pair.
    int func_004B8810(MOperand * op, MPCode * user) {
        MOperand* arg;
        RegClass301 rclass;
        int reg;
        int n;
        int i;

        rclass = op->rclass;
        arg = user->args;
        reg = op->reg;
        if (rclass == 1 && (reg & 1)) {
            reg &= ~1;
        }
        n = (data_006115F8[user->op].flags & 8) ? 2 : user->argcount;
        for (i = 0; i < n; i++, arg++) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            if (arg->kind == 0 &&
                arg->rclass == rclass &&
                reg == (arg->reg & ((arg->rclass == 1) ? ~1 : ~0)) &&
#else
        if (arg->kind == 0 &&
            arg->rclass == rclass &&
            reg == (arg->reg & ((arg->rclass == 1) ? ~1 : ~0)) &&
#endif
                (arg->flags & 1)) {
                return 1;
            }
        }
        return 0;
    }

    // Does pc write a register that user reads?
    static int feeds(MPCode * pc, MPCode * user) {
        MOperand* op;
        int n;
        int i;

        op = pc->args;
        n = (data_006115F8[pc->op].flags & 8) ? 2 : pc->argcount;
        for (i = 0; i < n; i++, op++) {
            if (op->kind == 0 && (op->flags & 2) && func_004B8810(op, user)) {
                return 1;
            }
        }
        return 0;
    }

    // Pipeline hazards in unscheduled code: a nop between an instruction and a
    // next one that reads its result too early.
    void func_004B83A0(MPCodeBlock * block) {
        MPCode* pc;
        MPCode* next;
        MPCode* last;
        MPCode* delay;
        int nops;
        int extra;
        int i;

        last = block->last;
        pc = block->first;
        if (last && (last->flags & 0x20000000)) {
            // take the branch's delay slot out while looking
            delay = last->prev;
            if (delay && (delay->flags & 0x40000000)) {
                deletepcode(delay);
            }
        } else {
            delay = NULL;
        }

        for (; pc; pc = next) {
            nops = 0;
            extra = nops;
            next = pc->next;
            if (pc->op >= MACRO_OP(0x00) || !next) {
                continue;
            }
            if (pc->flags & 2) {
                if (feeds(pc, next) && !copts.no_load_delay_nops) {
                    nops++;
                }
            } else if (data_006115F8[pc->op].flags2 & 0xA000) {
                if (feeds(next, pc)) {
                    nops++;
                }
            } else if (data_006115F8[pc->op].flags2 & 0x4000) {
#if MWCCPS2
                // b210 tests the hazard but never pads it
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
#if MWCC_CORE != MWCC_CORE_B52
                if (feeds(pc, next) && copts.mips_isa < 4) {
                    nops++;
                }
#endif
#endif
#else
            if (feeds(pc, next) && copts.mips_isa < 4) {
                nops++;
            }
#endif
            } else if (data_006115F8[pc->op].flags2 & 0x5000) {
                if (feeds(pc, next)) {
                    nops++;
                }
#if MWCCPS2
            } else if (data_006115F8[pc->op].flags2 & 0x20000) {
                // b210: an instruction two ahead (or the delay slot) of kind
                // 0x10000 needs a nop after the next one (or after this one)
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                // b69: the next one of that kind needs two after this one
                if (data_006115F8[next->op].flags2 & 0x10000) {
                    nops += 2;
                } else
#endif
                    if (next->next && (data_006115F8[next->next->op].flags2 & 0x10000)) {
                    extra++;
                } else if (!next->next && delay && (data_006115F8[delay->op].flags2 & 0x10000)) {
                    nops++;
                }
            } else if (data_006115F8[pc->op].flags2 & 0x80000) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                if (data_006115F8[next->op].flags2 & 0x40000) {
                    nops += 2;
                } else
#endif
                    if (next->next && (data_006115F8[next->next->op].flags2 & 0x40000)) {
                    extra++;
                } else if (!next->next && delay && (data_006115F8[delay->op].flags2 & 0x40000)) {
                    nops++;
                }
#endif
            } else if (pc->flags & 0x1000000) {
                if (feeds(pc, next)) {
                    nops++;
                }
            }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            if ((copts.mips_flags02 & 0x2000) &&
                (UInt16)(pc->op - OPC(0xB4)) <= 1 &&
                (UInt16)(next->op - OPC(0xB4)) <= 3 &&
                !nops) {
#else
        if ((copts.mips_flags02 & 0x2000) &&
            (UInt16)(pc->op - 0xB4) <= 1 &&
            (UInt16)(next->op - 0xB4) <= 3 &&
            !nops) {
#endif
                nops++;
            }
            for (i = 0; i < nops; i++) {
                insertpcodeafter(pc, makepcode(func_004903F0()));
            }
            for (i = 0; i < extra; i++) {
                insertpcodeafter(next, makepcode(func_004903F0()));
            }
        }

        // the last real instruction before the block's closing pseudo-ops
        last = block->last;
        if (last && last->op >= MACRO_OP(0xA3)) {
            for (pc = last->prev; pc && pc->op >= MACRO_OP(0x00); pc = pc->prev) {}
        } else {
            pc = last;
        }
        if (!pc) {
            return;
        }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
#if MWCC_CORE == MWCC_CORE_B52
        if ((!copts.no_load_delay_nops && (pc->flags & 2)) ||
            (data_006115F8[pc->op].flags2 & 0x5000)) {
#else
    // b69 pads a closing load whatever no_load_delay_nops says
    if ((pc->flags & 2) || (data_006115F8[pc->op].flags2 & 0x5000)) {
#endif
#else
    if ((!copts.no_load_delay_nops && (pc->flags & 2)) || (data_006115F8[pc->op].flags2 & 0x5000)) {
#endif
            insertpcodeafter(pc, makepcode(func_004903F0()));
        }
        if (delay) {
            insertpcodebefore(last, delay);
        }
    }

    // Take an instruction off the end of the block for a delay slot: one marked
    // before, else the earliest of the movable ones at the end.
    MPCode* func_004B88B0(MPCodeBlock * block) {
        MPCode* pc;
        MPCode* cand = NULL;

        for (pc = block->last; pc; pc = pc->prev) {
            if (pc->flags & 0x40000000) {
                deletepcode(pc);
                return pc;
            }
        }
        for (pc = block->last; pc; pc = pc->prev) {
            if (pc->flags & 0xA000000) {
                continue;
            }
            if (!(pc->flags & 2)) {
                break;
            }
            cand = pc;
        }
        if (!pc) {
            pc = cand;
        }
        if (pc) {
            pc->flags |= 0x40000000;
            deletepcode(pc);
        }
        return pc;
    }
