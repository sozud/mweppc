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

#define REAL_OP(op) data_00625CE4[(op) - BE_NUM_OPCODES]
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
        case MACRO_OP(0x47):
#if MWCCPS2 // the 128-bit constant
            p = makepcode(MACRO_OP(0x46), NEWREG(), *(CInt64*)&pc->args[1].value);
#elif MWCCPSP_NATIVE
        {
            // makepcode reads an 'I' operand as one CInt64; two ints only
            // line up with that on a 32-bit stack
            CInt64 v;
            v.hi = pc->args[1].value;
            v.lo = pc->args[1].value2;
            p = makepcode(MACRO_OP(0x46), NEWREG(), v);
        }
#else
            p = makepcode(MACRO_OP(0x46), NEWREG(), pc->args[1].value, pc->args[1].value2);
#endif
            pc->op = 0xAD;
            pc->args[1] = pc->args[0];
            pc->args[0] = p->args[0];
            pc->args[0].flags = 1;
            insertpcodebefore(pc, p);
            return;
        case MACRO_OP(0x55):
        case MACRO_OP(0x77):
            if (pc->args[1].value % 4) {
                if (pc->op == MACRO_OP(0x55)) {
                    if (copts.littleendian) {
                        split_unaligned(pc, MACRO_OP(0x5B), MACRO_OP(0x5A), 3);
                    } else {
                        split_unaligned(pc, MACRO_OP(0x5A), MACRO_OP(0x5B), 3);
                    }
                } else {
                    if (copts.littleendian) {
                        split_unaligned(pc, MACRO_OP(0x7E), MACRO_OP(0x7D), 3);
                    } else {
                        split_unaligned(pc, MACRO_OP(0x7D), MACRO_OP(0x7E), 3);
                    }
                }
            }
            break;
        case MACRO_OP(0x4A):
        case MACRO_OP(0x6F):
            if (pc->args[1].value % 8) {
                if (pc->op == MACRO_OP(0x4A)) {
                    if (copts.littleendian) {
                        split_unaligned(pc, MACRO_OP(0x4F), MACRO_OP(0x4E), 7);
                    } else {
                        split_unaligned(pc, MACRO_OP(0x4E), MACRO_OP(0x4F), 7);
                    }
                } else {
                    if (copts.littleendian) {
                        split_unaligned(pc, MACRO_OP(0x74), MACRO_OP(0x73), 7);
                    } else {
                        split_unaligned(pc, MACRO_OP(0x73), MACRO_OP(0x74), 7);
                    }
                }
            }
            break;
        case MACRO_OP(0x2D):
        case MACRO_OP(0x2E):
        case MACRO_OP(0x49):
        case MACRO_OP(0x4B):
        case MACRO_OP(0x4C):
        case MACRO_OP(0x4D):
        case MACRO_OP(0x4E):
        case MACRO_OP(0x4F):
        case MACRO_OP(0x50):
        case MACRO_OP(0x51):
        case MACRO_OP(0x52):
        case MACRO_OP(0x53):
        case MACRO_OP(0x54):
        case MACRO_OP(0x56):
        case MACRO_OP(0x57):
        case MACRO_OP(0x58):
        case MACRO_OP(0x59):
        case MACRO_OP(0x5A):
        case MACRO_OP(0x5B):
        case MACRO_OP(0x5C):
        case MACRO_OP(0x5D):
        case MACRO_OP(0x5E):
        case MACRO_OP(0x6D):
        case MACRO_OP(0x6E):
        case MACRO_OP(0x70):
        case MACRO_OP(0x71):
        case MACRO_OP(0x72):
        case MACRO_OP(0x73):
        case MACRO_OP(0x74):
        case MACRO_OP(0x75):
        case MACRO_OP(0x76):
        case MACRO_OP(0x78):
        case MACRO_OP(0x79):
        case MACRO_OP(0x7A):
        case MACRO_OP(0x7B):
        case MACRO_OP(0x7C):
        case MACRO_OP(0x7D):
        case MACRO_OP(0x7E):
        case MACRO_OP(0x7F):
        case MACRO_OP(0x80):
#if !MWCCPS2 // the VFPU macros (b210 has none)
        case MACRO_OP(0x9B):
        case MACRO_OP(0x9C):
        case MACRO_OP(0x9D):
        case MACRO_OP(0x9E):
        case MACRO_OP(0x9F):
        case MACRO_OP(0xA0):
        case MACRO_OP(0xA1):
#endif
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
