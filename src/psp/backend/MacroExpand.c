#include "compiler/common.h"
#include "psp/BE.h"
#include "compiler/CParser.h"
#include "compiler/objects.h"

// mwccpsp 3.0.1 macro expansion of memory operands (0x48AEF0..0x48B5D0;
// the unit's name is not known). Loads and stores are made with a symbolic
// operand (object + offset); here they get a real address: $gp- or
// $s7-relative, frame-relative, or a lui/%lo pair, and misaligned word and
// doubleword accesses are split into their left/right halves.

extern SInt32 data_00625A98[7]; // next virtual register, per class
extern UInt16 data_00625CE4[]; // macro opcode -> machine opcode, from 0x518
extern SInt16 data_00625E3E; // frame register
extern MPCode* func_00480420(int op, SInt16 nargs);
extern struct Alias* make_alias(Object* obj, SInt32 offset, SInt32 size);

extern Object* data_005F0BF8; // the function being expanded
extern MPCodeBlock* data_00624C58; // block being expanded

#define REAL_OP(op) data_00625CE4[(op) - 0x518]
#define HA(v) (((UInt32)(v) >> 16) + (((v) & 0x8000) >> 15))
#define NEWREG() (data_00625A98[0]++)

// A misaligned access becomes the left and right instructions.
static void split_unaligned(MPCode* pc, UInt16 first, UInt16 second, SInt32 bump) {
    MPCode* p;

    pc->op = first;
    p = func_00480420(second, 3);
    p->flags = pc->flags;
    p->args[0] = pc->args[0];
    p->args[0].flags |= 1;
    p->args[1] = pc->args[1];
    p->args[2] = pc->args[2];
    p->unk38 = pc->unk38;
    p->alias = pc->alias;
    insertpcodeafter(pc, p);
    p->args[1].value += bump;
}

// lui reg, %hi(object+offset): the first half of a pair.
static MPCode* make_hi(MPCode* pc) {
    MPCode* lui = makepcode(0x90, NEWREG(), HA(pc->args[1].value));

    lui->unk38 = 5;
    lui->flags &= ~0x4000;
    lui->relobj = (Object*)MOP_OBJ(pc->args[1]);
    if (pc->flags & 6) {
        lui->alias = make_alias((Object*)MOP_OBJ(pc->args[1]), 0, 0);
    }
    insertpcodebefore(pc, lui);
    return lui;
}

// pc becomes the %lo half, based on the lui's register.
static void make_lo(MPCode* pc, MPCode* lui) {
    pc->op = REAL_OP(pc->op);
    pc->args[2] = lui->args[0];
    pc->args[2].flags = 1;
    pc->args[1].value &= 0xFFFF;
    pc->unk38 = 6;
    pc->relobj = (Object*)MOP_OBJ(pc->args[1]);
    lui->pair = pc;
    pc->pair = lui;
}

void func_0048AF30(MPCode* pc) {
    ObjAddress addr;
    MPCode* p;
    MPCode* lui;
    SInt32 offset;

    switch (pc->op) {
        case 0x55F:
            p = makepcode(0x55E, NEWREG(), pc->args[1].value, pc->args[1].value2);
            pc->op = 0xAD;
            pc->args[1] = pc->args[0];
            pc->args[0] = p->args[0];
            pc->args[0].flags = 1;
            insertpcodebefore(pc, p);
            return;
        case 0x56D:
        case 0x58F:
            if (pc->args[1].value % 4) {
                if (pc->op == 0x56D) {
                    if (copts.littleendian) {
                        split_unaligned(pc, 0x573, 0x572, 3);
                    } else {
                        split_unaligned(pc, 0x572, 0x573, 3);
                    }
                } else {
                    if (copts.littleendian) {
                        split_unaligned(pc, 0x596, 0x595, 3);
                    } else {
                        split_unaligned(pc, 0x595, 0x596, 3);
                    }
                }
            }
            break;
        case 0x562:
        case 0x587:
            if (pc->args[1].value % 8) {
                if (pc->op == 0x562) {
                    if (copts.littleendian) {
                        split_unaligned(pc, 0x567, 0x566, 7);
                    } else {
                        split_unaligned(pc, 0x566, 0x567, 7);
                    }
                } else {
                    if (copts.littleendian) {
                        split_unaligned(pc, 0x58C, 0x58B, 7);
                    } else {
                        split_unaligned(pc, 0x58B, 0x58C, 7);
                    }
                }
            }
            break;
        case 0x545:
        case 0x546:
        case 0x561:
        case 0x563:
        case 0x564:
        case 0x565:
        case 0x566:
        case 0x567:
        case 0x568:
        case 0x569:
        case 0x56A:
        case 0x56B:
        case 0x56C:
        case 0x56E:
        case 0x56F:
        case 0x570:
        case 0x571:
        case 0x572:
        case 0x573:
        case 0x574:
        case 0x575:
        case 0x576:
        case 0x585:
        case 0x586:
        case 0x588:
        case 0x589:
        case 0x58A:
        case 0x58B:
        case 0x58C:
        case 0x58D:
        case 0x58E:
        case 0x590:
        case 0x591:
        case 0x592:
        case 0x593:
        case 0x594:
        case 0x595:
        case 0x596:
        case 0x597:
        case 0x598:
        case 0x5B3:
        case 0x5B4:
        case 0x5B5:
        case 0x5B6:
        case 0x5B7:
        case 0x5B8:
        case 0x5B9:
            // loads and stores with a symbolic address
            break;
        default:
            return;
    }

    if (pc->flags & 0x200000) {
        return;
    }
    ObjGen_GetObjectAddress((Object*)MOP_OBJ(pc->args[1]), &addr);
    if (addr.basereg == 28) {
        if (addr.near) {
            pc->op = REAL_OP(pc->op);
            pc->unk38 = addr.gpreg ? 8 : 7;
            pc->relobj = (Object*)MOP_OBJ(pc->args[1]);
        } else {
            // $gp-relative but out of reach: lui + addu $gp + %lo
            lui = make_hi(pc);
            insertpcodebefore(pc, makepcode(4, pc->args[0].reg, 28, pc->args[0].reg));
            make_lo(pc, lui);
        }
        return;
    }
    if (addr.basereg == data_00625E3E && pc->args[2].reg != 28) {
        return;
    }
    if (addr.basereg == 23) {
        lui = make_hi(pc);
        insertpcodebefore(pc, makepcode(4, lui->args[0].reg, 23, lui->args[0].reg));
        make_lo(pc, lui);
        return;
    }
    if (addr.basereg == -1) {
        // frame or absolute offset: fine if it fits in 16 bits
        offset = pc->args[1].value;
        if (offset > 0x7FFF || offset < -0x8000) {
            lui = makepcode(0x90, NEWREG(), HA(offset));
            insertpcodebefore(pc, lui);
            if (pc->args[2].reg) {
                insertpcodebefore(
                    pc, makepcode(4, lui->args[0].reg, pc->args[2].reg, lui->args[0].reg)
                );
            }
            pc->args[1].value &= 0xFFFF;
            pc->args[2] = lui->args[0];
            pc->args[2].flags = 1;
        }
        pc->op = REAL_OP(pc->op);
        return;
    }
    if (addr.near) {
        pc->op = REAL_OP(pc->op);
        pc->unk38 = 1;
        return;
    }
    make_lo(pc, make_hi(pc));
}

// Expand every instruction of the function.
void func_0048AEF0(Object* func) {
    MPCodeBlock* block;
    MPCode* pc;

    data_005F0BF8 = func;
    for (block = data_00624FD0; block; block = block->next) {
        data_00624C58 = block;
        for (pc = block->first; pc; pc = pc->next) {
            func_0048AF30(pc);
        }
    }
}
