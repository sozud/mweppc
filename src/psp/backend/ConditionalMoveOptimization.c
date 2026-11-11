// mwccpsp 3.0.1 conditional-move optimization (ConditionalMoveOptimization.c,
// 0x4B0010..0x4B11D0). Not in the 2.4.7 sources.
//
// The early pass (before register allocation) turns a conditional branch
// over a small triangle or diamond of integer code into straight-line code:
// the branch condition is computed into a new register with slt/slti/sltu/
// sltiu/xor/xori, the arms' instructions are hoisted above the branch
// (results that are live out are renamed and committed with movn/movz) and
// the arm blocks are deleted.
//
// The late pass (after register allocation) handles a branch over a single
// instruction (or two arms of one instruction each) by replacing it with
// movn/movz, or movf/movt for the FP condition branches.
#include "compiler/common.h"
#include "psp/BE.h"
#include "compiler/CInt64.h"
#include "compiler/CParser.h"
#include "compiler/CError.h"

extern SInt32 data_00625A98[7]; // next virtual register of each class
extern void func_004C34A0(void); // liveness
extern MOperand* func_0048FAE0(MPCode* pc); // a branch's label operand
extern void func_0046FD50(MPCode* anchor, VarArgShort op, ...); // emit before anchor
extern Boolean func_0046F5F0(MPCodeBlock* from, MPCodeBlock* to); // can merge
extern void func_0046F400(MPCodeBlock* from, MPCodeBlock* to); // merge

#define LABEL_BLOCK(op) (((MPCodeLabel*)(op)->ptr)->block)
#define RESULTIDX(pc) (((pc)->flags & 0x80000000) ? 1 : 0)
#define IMM(o) (*(CInt64*)&(o).value)
#define LIVEOUT(block, reg) \
    ((data_0062541C[0].blocks[(block)->number].liveout[(reg) >> 5] >> ((reg) & 31)) & 1)

UInt8 func_004B0850(MPCode* pc);

// Rename a dead copy away: a `move d, s` in `arm` whose d is live out of the
// arm but whose s is live out of neither the arm nor `block` is deleted after
// every earlier use of s in the arm is renamed to d.
void func_004B06E0(MPCodeBlock* arm, MPCodeBlock* block) {
    MPCode* pc;
    MPCode* next;
    MPCode* scan;
    SInt32 d;
    SInt32 s;
    SInt32 i;

    for (pc = arm->first; pc; pc = next) {
        next = pc->next;
        if ((UInt16)(pc->op - 0xAA) <= 1 && (pc->flags & 0x10)) {
            d = pc->args[0].reg;
            s = pc->args[1].reg;
            if (LIVEOUT(arm, d) && !LIVEOUT(block, s) && !LIVEOUT(arm, s)) {
                for (scan = arm->first; scan != pc; scan = scan->next) {
                    for (i = scan->argcount - 1; i >= 0; i--) {
                        if (scan->args[i].kind == 0 &&
                            scan->args[i].rclass == 0 &&
                            scan->args[i].reg == s) {
                            scan->args[i].reg = d;
                        }
                    }
                }
                deletepcode(pc);
            }
        }
    }
}

// Hoist the instructions of `arm` above the branch `br`; results live out of
// the arm are committed with `op` (movn/movz) on the condition `cond`.
void func_004B05D0(MPCodeBlock* arm, MPCodeBlock* join, MPCode* br, SInt32 cond, UInt16 op) {
    MPCode* pc;
    MPCode* next;
    SInt32 reg;
    SInt32 old;

    for (pc = arm->first; pc; pc = next) {
        next = pc->next;
        switch (func_004B0850(pc)) {
            case 1: // live copy: a conditional move instead
                func_0046FD50(br, (SInt16)op, pc->args[0].reg, pc->args[1].reg, cond);
                deletepcode(pc);
                break;
            case 2: // dead copy
            case 4: // dead result
                deletepcode(pc);
                insertpcodebefore(br, pc);
                break;
            case 3: // live result: compute into a new register, then move
                reg = data_00625A98[0];
                old = pc->args[0].reg;
                data_00625A98[0]++;
                pc->args[0].reg = reg;
                deletepcode(pc);
                insertpcodebefore(br, pc);
                func_0046FD50(br, (SInt16)op, old, reg, cond);
                break;
            case 5: // load of zero: move $zero
                func_0046FD50(br, (SInt16)op, pc->args[0].reg, 0, cond);
                deletepcode(pc);
                break;
            case 6: // the arm's jump: left alone
                break;
            default:
                CError_FATAL(0x233);
                break;
        }
    }
}

// Cost of hoisting a block's instructions (7 or more: not possible).
SInt32 func_004B0800(MPCodeBlock* block) {
    SInt32 cost = 0;
    MPCode* pc = block->first;
    UInt8 kind;

    while (pc && cost <= 6) {
        kind = func_004B0850(pc);
        pc = pc->next;
        switch (kind) {
            case 1:
            case 4:
            case 5:
                cost += 1;
                break;
            case 3:
                cost += 2;
                break;
            case 2:
            case 6:
                break;
            default:
                cost = 7;
                break;
        }
    }
    return cost;
}

// Classify an instruction for hoisting: 0 not possible, 1 copy (result live
// out), 2 copy (dead), 3 other (result live out), 4 other (dead), 5 load of
// zero (li r, 0) with a live result, 6 unconditional jump.
UInt8 func_004B0850(MPCode* pc) {
    SInt32 i;
    UInt8 kind;

    if (!pc) {
        CError_FATAL(0x153);
        return 0;
    }
    if (pc->flags & 0x106) {
        return 0;
    }
    if (pc->argcount == 0) {
        return 0;
    }
    for (i = pc->argcount - 1; i >= 0; i--) {
        kind = pc->args[i].kind;
        if ((UInt8)(kind - 4) <= 1) {
            return 0;
        }
        if (kind == 0) {
            if (pc->args[i].rclass != 0) {
                return 0;
            }
            if (i == 0) {
                if (!(pc->args[i].flags & 2)) {
                    return 0;
                }
            } else {
                if (pc->args[i].flags & 2) {
                    return 0;
                }
            }
        } else if (i == 0 && pc->op != 0x39) {
            return 0;
        }
    }

    switch (pc->op) {
        case 2:
        case 3:
            if (!LIVEOUT(pc->block, pc->args[0].reg)) {
                return 4;
            }
            if (pc->args[1].reg == 0 && CInt64_IsZero(&IMM(pc->args[2]))) {
                return 5;
            }
            return 3;
        case 0x39:
            return 6;
        case 0xAA:
        case 0xAB:
            return LIVEOUT(pc->block, pc->args[0].reg) ? 1 : 2;
        case 1:
        case 4:
        case 6:
        case 7:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x86:
        case 0x87:
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0x8C:
        case 0x8D:
        case 0x8E:
        case 0x90:
        case 0xB8:
        case 0xB9:
        case 0xBC:
        case 0xBD:
        case 0xBE:
        case 0xBF:
        case 0xD2:
        case 0xD3:
        case 0xD4:
        case 0xD5:
        case 0xD6:
        case 0xD7:
        case 0xDA:
        case 0xDB:
        case 0xDC:
        case 0xDD:
        case 0xE4:
        case 0xE5:
        case 0x3EE:
        case 0x3EF:
        case 0x3F0:
        case 0x3F1:
        case 0x3F2:
        case 0x3F3:
        case 0x3FB:
        case 0x3FC:
        case 0x3FD:
        case 0x3FE:
        case 0x3FF:
        case 0x402:
        case 0x404:
            return LIVEOUT(pc->block, pc->args[0].reg) ? 3 : 4;
        default:
            return 0;
    }
}

void BE_EarlyConditionalMoves(void) {
    MPCodeBlock* block;
    MPCodeBlock* a;
    MPCodeBlock* b;
    MPCodeBlock* join;
    MPBlockLink* link;
    MPCode* br;
    MPCode* keepA;
    MPCode* keepB;
    CInt64 lim;
    UInt16 cmpop;
    SInt32 newreg;
    SInt32 src;
    SInt32 third;
    SInt32 cost;
    Boolean swap;
    Boolean merged;
    Boolean again = 0;

    func_004C34A0();
    block = data_00624FD0;
    while (block) {
        swap = 0;
        merged = 0;
        br = block->last;
        if (!br) {
            goto next;
        }
        switch (br->op) {
            case 0x18: // beq: xor
                swap = 1;
            case 0x28: // bne
                cmpop = 0xE4;
                src = br->args[0].reg;
                third = br->args[1].reg;
                break;
            case 0x1C: // bgez: slt r, 0
                swap = 1;
            case 0x24: // bltz
                cmpop = 0xD4;
                third = 0;
                src = br->args[0].reg;
                break;
            case 0x20: // bgtz: slti r, 1
                swap = 1;
            case 0x22: // blez
                cmpop = 0xD5;
                src = br->args[0].reg;
                third = 1;
                break;
            case 0x51B: // beq imm: xori
                swap = 1;
            case 0x53D: // bne imm
                CInt64_SetLong(&lim, 0xFFFF);
                if (CInt64_GreaterU(IMM(br->args[1]), lim)) {
                    goto next;
                }
                cmpop = 0xE5;
                src = br->args[0].reg;
                third = br->args[1].value2;
                break;
            case 0x51D: // bge: slt
                swap = 1;
            case 0x535: // blt
                cmpop = 0xD4;
                src = br->args[0].reg;
                third = br->args[1].reg;
                break;
            case 0x51F: // bge imm: slti
                swap = 1;
            case 0x537: // blt imm
                if (!CInt64_IsInRange(IMM(br->args[1]), 2)) {
                    goto next;
                }
                cmpop = 0xD5;
                src = br->args[0].reg;
                third = br->args[1].value2;
                break;
            case 0x521: // bgeu: sltu
                swap = 1;
            case 0x539: // bltu
                cmpop = 0xD7;
                src = br->args[0].reg;
                third = br->args[1].reg;
                break;
            case 0x523: // bgeu imm: sltiu
                swap = 1;
            case 0x53B: // bltu imm
                if (!CInt64_IsInRange(IMM(br->args[1]), 2)) {
                    goto next;
                }
                cmpop = 0xD6;
                src = br->args[0].reg;
                third = br->args[1].value2;
                break;
            default:
                goto next;
        }
        newreg = data_00625A98[0]++;

        b = LABEL_BLOCK(func_0048FAE0(br));
        link = block->successors;
        a = link->block;
        if (a == b && link->next) {
            b = link->next->block;
        }
        if (!a->predecessors || a->predecessors->nextpred) {
            goto next;
        }
        if (!b->predecessors || b->predecessors->nextpred) {
            goto next;
        }
        link = a->successors;
        if (link && !link->next && link->block == b) {
            join = b; // triangle
        } else {
            if (!link || link->next) {
                goto next;
            }
            if (!b->successors || b->successors->next) {
                goto next;
            }
            join = link->block;
            if (join != b->successors->block) {
                goto next;
            }
        }

        if (cmpop) {
            // (the original expands a macro opcode >= 0x518 here; none is
            // ever emitted)
            func_0046FD50(br, (SInt16)cmpop, newreg, src, third);
        }
        cost = func_004B0800(a);
        cost += (join == b) ? 0 : func_004B0800(b);
        if (cost > 6) {
            goto next;
        }

        if (join == b) {
            func_004B06E0(a, block);
            func_004B05D0(a, join, br, newreg, swap ? 0x111 : 0x110);
        } else {
            if (again) {
                swap = !swap;
            }
            func_004B06E0(a, block);
            func_004B06E0(b, block);
            func_004B05D0(a, join, br, newreg, swap ? 0x111 : 0x110);
            func_004B05D0(b, join, br, newreg, swap ? 0x110 : 0x111);
        }
        deletepcode(br);

        if (b == join) {
            keepB = NULL;
        } else {
            keepB = b->first;
            unlinkblocks(b, join);
            unlinkblocks(block, b);
            deleteblock(b);
        }
        keepA = a->first;
        if (keepA) {
            deletepcode(keepA);
        }
        unlinkblocks(a, join);
        unlinkblocks(block, a);
        deleteblock(a);
        linkblocks(block, join);
        if (func_0046F5F0(block, join)) {
            func_0046F400(block, join);
            merged = 1;
        } else if (keepA) {
            appendpcode(block, keepA);
        } else if (keepB) {
            appendpcode(block, keepB);
        } else {
            CError_FATAL(0x3B9);
        }

next:
        if (merged) {
            func_004C34A0();
            again = 1;
        } else {
            block = block->next;
            again = 0;
        }
    }
    deleteunreachableblocks();
    func_004C34A0();
}

// Late pass: the conditional move replacing `br` over the instruction `pc`.
MPCode* func_004B0C40(MPCode* br, MPCode* pc, Boolean flag) {
    SInt32 dst = 0;
    SInt32 src = 0;
    SInt32 cond;
    Boolean isd;
    Boolean gpr;
    SInt16 op;

    if (pc->flags & 0x10) {
        src = pc->args[1].reg;
        dst = pc->args[0].reg;
    }
    isd = pc->op == 0xF8;
    gpr = pc->args[RESULTIDX(pc)].rclass == 0;
    switch (br->op) {
        case 0x18: // beq
        case 0x1A: // beqz
            cond = br->args[0].reg;
            if (gpr) {
                op = flag ? 0x110 : 0x111;
            } else if (isd) {
                op = flag ? 0x118 : 0x11A;
            } else {
                op = flag ? 0x119 : 0x11B;
            }
            return makepcode(op, dst, src, cond);
        case 0x28: // bne
        case 0x2A: // bnez
            cond = br->args[0].reg;
            if (gpr) {
                op = flag ? 0x111 : 0x110;
            } else if (isd) {
                op = flag ? 0x11A : 0x118;
            } else {
                op = flag ? 0x11B : 0x119;
            }
            return makepcode(op, dst, src, cond);
        case 0x0C: // bc1f
            if (gpr) {
                op = flag ? 0x113 : 0x112;
            } else if (isd) {
                op = flag ? 0x116 : 0x114;
            } else {
                op = flag ? 0x117 : 0x115;
            }
            return makepcode(op, dst, src, 0);
        case 0x0E: // bc1t
            if (gpr) {
                op = flag ? 0x112 : 0x113;
            } else if (isd) {
                op = flag ? 0x114 : 0x116;
            } else {
                op = flag ? 0x115 : 0x117;
            }
            return makepcode(op, dst, src, 0);
        default:
            CError_FATAL(0xE8);
            return NULL;
    }
}

// Replace the branch `br` by a conditional move of `arm`'s instruction, then
// `other`'s instructions (if any), then a branch to `join`.
void func_004B0AD0(
    MPCode* br, Boolean flag, MPCodeBlock* arm, MPCodeBlock* other, MPCodeBlock* join
) {
    MPCode* first = arm->first;
    MPCodeBlock* block = br->block;
    MPBlockLink* link;
    MPCodeBlock* from;
    MPCode* pc;

    for (link = block->successors; link; link = link->next) {
        unlinkblocks(block, link->block);
    }
    for (link = join->predecessors; link; link = link->nextpred) {
        from = link->from;
        if (from->number == br->block->number ||
            from->number == arm->number ||
            (other && from->number == other->number)) {
            unlinkblocks(from, join);
        }
    }
    appendpcode(block, func_004B0C40(br, first, flag));
    if (other) {
        for (pc = other->first; pc; pc = pc->next) {
            if (pc->op != 0x39 && pc->op != 0x38A && pc->op != 0x38B) {
                appendpcode(block, copypcode(pc));
            }
        }
    }
    if (other && (other->last->flags & 1)) {
        appendpcode(block, copypcode(other->last));
        for (link = other->successors; link; link = link->next) {
            linkblocks(block, link->block);
        }
    } else if (arm->last->flags & 1) {
        appendpcode(block, copypcode(arm->last));
        for (link = arm->successors; link; link = link->next) {
            linkblocks(block, arm); // (sic: the arm, once per successor)
        }
    }
    pcbranch(block, join->labels, 0);
    deletepcode(br);
    deleteunreachableblocks();
}

// Does block (pc reads its first instruction) end in a jump, or (with one
// instruction) is that instruction a jump writing `reg`?
Boolean func_004B1030(MPCodeBlock* block, SInt32 reg) {
    MPCode* first = block->first;
    UInt16 op;

    if (!first) {
        return 0;
    }
    if (first->flags & 2) {
        return 0;
    }
    if (block->pcodecount == 1) {
        op = block->last->op;
        if (op == 0x39 || (UInt16)(op - 0x38A) <= 1) {
            if (reg == first->args[RESULTIDX(first)].reg) {
                return 1;
            }
        }
    }
    op = first->op;
    return op == 0x39 || (UInt16)(op - 0x38A) <= 1;
}

// Is block a single instruction with a result (optionally followed by a
// jump) that does not write `reg` and can be conditionally moved?
Boolean func_004B10D0(MPCodeBlock* block, SInt32 reg) {
    MPCode* first = block->first;
    UInt16 op;

    if (block->pcodecount != 1) {
        if (block->pcodecount != 2) {
            return 0;
        }
        op = block->last->op;
        if (op != 0x39 && (UInt16)(op - 0x38A) > 1) {
            return 0;
        }
    }
    if (!(first->flags & 0x10)) {
        return 0;
    }
    if (reg == first->args[RESULTIDX(first)].reg) {
        return 0;
    }
    if (!copts.conditional_move || copts.mips_isa >= 4) {
        return 1;
    }
    return first->args[RESULTIDX(first)].rclass == 0;
}

// Is pc a branch the late pass can turn into a conditional move?
Boolean func_004B1170(MPCode* pc) {
    UInt16 op = pc->op;

    if (op == 0x1A || op == 0x2A) {
        return 1;
    }
    if (op == 0x18 || op == 0x28) {
        if (pc->args[1].reg == 0) {
            return 1;
        }
    }
    if (!copts.conditional_move || copts.mips_isa >= 4) {
        if (op == 0x0E || op == 0x0C) {
            return 1;
        }
    }
    return 0;
}

void BE_LateConditionalMoves(void) {
    MPCodeBlock* block;
    MPCodeBlock* fall;
    MPCodeBlock* target;
    MPCodeBlock* join;
    MPBlockLink* link;
    MPCode* br;

    for (block = data_00624FD0; block; block = block->next) {
        br = block->last;
        if (!br || !func_004B1170(br)) {
            continue;
        }
        target = LABEL_BLOCK(func_0048FAE0(br));
        link = block->successors;
        fall = link->block;
        if (fall == target && link->next) {
            fall = link->next->block;
        }
        if (!fall->predecessors || fall->predecessors->nextpred) {
            continue;
        }
        if (!target->predecessors || target->predecessors->nextpred) {
            continue;
        }
        link = fall->successors;
        if (link && !link->next && link->block == target) {
            join = target;
        } else {
            if (!link || link->next) {
                continue;
            }
            if (!target->successors || target->successors->next) {
                continue;
            }
            join = link->block;
            if (join != target->successors->block) {
                continue;
            }
        }

        if (join == target) {
            if (func_004B10D0(fall, br->args[0].reg)) {
                func_004B0AD0(br, 1, fall, NULL, join);
            }
        } else if (func_004B10D0(fall, br->args[0].reg)) {
            if (func_004B1030(target, fall->first->args[0].reg)) {
                func_004B0AD0(br, 1, fall, target, join);
            }
        } else if (func_004B10D0(target, br->args[0].reg)) {
            if (func_004B1030(fall, target->first->args[0].reg)) {
                func_004B0AD0(br, 0, target, fall, join);
            }
        }
    }
    deleteunreachableblocks();
}
