#include "compiler/common.h"
#include "psp/BE.h"
#include "compiler/Alias.h"
#include "compiler/CError.h"
#include "compiler/objects.h"
#include "compiler/CParser.h"
#include "compiler/RegisterInfo.h"

// mwccpsp 3.0.1 PCodeUtilities.c (named by its assertions): emitting
// common instruction shapes.

#define OBJECT_FRAMEOFFSET(obj) (*(SInt32*)&((Object*)(obj))->u.var.offset) // +0x30

extern SInt16 data_00625E3E; // frame register
extern MPCode* func_00480420(int op, SInt16 nargs);
extern SInt32 func_0048FBE0(UInt16 op); // bytes the memory instruction accesses

inline MOperand reg_operand(SInt16 reg) {
    MOperand op;
    op.kind = 0;
    op.rclass = 0;
    op.reg = reg;
    return op;
}

inline MOperand fpr_operand(SInt16 reg) {
    MOperand op;
    op.kind = 0;
    op.rclass = 1;
    op.reg = reg;
    return op;
}

extern SInt16 data_00625E3E; // the frame register
extern MPCodeBlock* data_006246B4; // the block being generated

// 0x480230: the access just made may be misaligned: split a doubleword load
// or a word store into its left and right halves (unless it is relative to
// the frame or $gp, or is aligned enough)
void func_00480230(SInt16 align, void* unk) {
    MPCode* pc;
    MPCode* npc;
    SInt16 base;

    pc = data_006246B4->last;
    switch (pc->op) {
        case 0x562:
            base = pc->args[2].reg;
            if (base == data_00625E3E || base == 28 || align >= 8) {
                return;
            }
            pc->op = copts.littleendian ? 0x567 : 0x566;
            npc = func_00480420(copts.littleendian ? 0x566 : 0x567, 3);
            npc->flags = pc->flags;
            npc->args[0] = pc->args[0];
            npc->args[0].flags |= 1;
            npc->args[1] = pc->args[1];
            npc->args[2] = pc->args[2];
            npc->alias = pc->alias;
            insertpcodeafter(pc, npc);
            npc->args[1].value += 7;
            break;
        case 0x56D:
            base = pc->args[2].reg;
            if (base == data_00625E3E || base == 28 || align >= 4) {
                return;
            }
            pc->op = copts.littleendian ? 0x573 : 0x572;
            npc = func_00480420(copts.littleendian ? 0x572 : 0x573, 3);
            npc->flags = pc->flags;
            npc->args[0] = pc->args[0];
            npc->args[0].flags |= 1;
            npc->args[1] = pc->args[1];
            npc->args[2] = pc->args[2];
            npc->alias = pc->alias;
            insertpcodeafter(pc, npc);
            npc->args[1].value += 3;
            break;
        case 0x575:
            CError_FATAL(0x241);
            break;
    }
}

// branch_always: an unconditional branch ends the block.
void func_00480710(MPCodeLabel* label) {
    SInt16 op = 0x39;
    emitpcode(op, label);
    pcbranch(data_006246B4, label, 0);
    makepcblock();
}

extern MOperand func_004C3D90(SInt32 value); // an immediate operand

// dst = src op imm
void func_00480740(int op, SInt16 dst, SInt16 src, SInt32 imm) {
    MPCode* pc;

    pc = func_00480420(op, 3);
    pc->args[0].kind = 0;
    pc->args[0].rclass = 0;
    pc->args[0].reg = dst;
    pc->args[0].flags = 2;
    pc->args[1].kind = 0;
    pc->args[1].rclass = 0;
    pc->args[1].reg = src;
    pc->args[1].flags = 1;
    pc->args[2] = func_004C3D90(imm);
    appendpcode(data_006246B4, pc);
}

// Store register reg to obj+offset addressed from base.
MPCode* func_004808E0(SInt16 op, SInt16 reg, SInt16 base, Object* obj, SInt32 offset) {
    MPCode* pc;

    pc = func_00480420(op, 3);
    pc->args[0] = reg_operand(reg);
    pc->args[0].flags = 1;
    pc->args[1].kind = 4;
    MOP_SETOBJ(pc->args[1], obj);
    pc->args[1].value = offset;
    pc->args[2] = reg_operand(base);
    pc->args[2].flags = 1;
    if (obj) {
        if (obj->datatype == DLOCAL && pc->args[2].reg != data_00625E3E) {
            pc->alias = make_alias(obj, 0, 0);
        } else if (obj->datatype == DLOCAL) {
            pc->alias = make_alias(obj, offset - OBJECT_FRAMEOFFSET(obj), func_0048FBE0(pc->op));
        } else if (obj->datatype == DDATA) {
            pc->alias = make_alias(obj, 0, 0);
        } else {
            CError_FATAL(222);
        }
    }
    if (!pc->alias) {
        pc->flags |= 0x20;
    }
    appendpcode(data_006246B4, pc);
    return pc;
}

extern SInt16 data_00625E48; // move

// Load register reg from obj+offset addressed from base; a local that lives
// in a register is just moved.
void func_00480B40(SInt16 op, SInt16 reg, SInt16 base, Object* obj, SInt32 offset) {
    MPCode* pc;
    VarInfo* vi;
    SInt16 home;

    if (obj && obj->datatype == DLOCAL) {
        vi = Registers_GetVarInfo(obj);
    } else {
        vi = NULL;
    }
    if (vi && (home = vi->reg)) {
        emitpcode(data_00625E48, reg, home);
        return;
    }

    pc = func_00480420(op, 3);
    pc->args[0] = reg_operand(reg);
    pc->args[0].flags = 2;
    if (op == (UInt16)((copts.mips_isa >= 2 && (copts.fp_library & 2)) ? 0x563 : 0x560)) {
        pc->args[0].flags |= 0x8000;
        if (copts.mips_option1A) {
            pc->args[0].flags |= 0x20;
        }
    }
    pc->args[1].kind = 4;
    MOP_SETOBJ(pc->args[1], obj);
    pc->args[1].value = offset;
    pc->args[2] = reg_operand(base);
    pc->args[2].flags = 1;
    if (obj) {
        if (obj->datatype == DLOCAL && pc->args[2].reg != data_00625E3E) {
            pc->alias = make_alias(obj, 0, 0);
        } else if (obj->datatype == DLOCAL) {
            pc->alias = make_alias(obj, offset - OBJECT_FRAMEOFFSET(obj), func_0048FBE0(pc->op));
        } else if (obj->datatype == DDATA) {
            pc->alias = make_alias(obj, 0, 0);
        } else {
            CError_FATAL(96);
        }
    }
    if (!pc->alias) {
        pc->flags |= 0x20;
    }
    if (obj && is_volatile_object(obj)) {
        pc->flags |= 0x80;
    }
    if (obj && is_const_object(obj)) {
        pc->flags |= 0x40;
    }
    appendpcode(data_006246B4, pc);
}

// Store floating-point register reg to obj+offset addressed from base.
MPCode* func_004807C0(SInt16 op, SInt16 reg, SInt16 base, Object* obj, SInt32 offset) {
    MPCode* pc;

    pc = func_00480420(op, 3);
    pc->args[0] = fpr_operand(reg);
    pc->args[0].flags = 1;
    pc->args[1].kind = 4;
    MOP_SETOBJ(pc->args[1], obj);
    pc->args[1].value = offset;
    pc->args[2] = reg_operand(base);
    pc->args[2].flags = 1;
    if (obj) {
        if (obj->datatype == DLOCAL && pc->args[2].reg != data_00625E3E) {
            pc->alias = make_alias(obj, 0, 0);
        } else if (obj->datatype == DLOCAL) {
            pc->alias = make_alias(obj, offset - OBJECT_FRAMEOFFSET(obj), func_0048FBE0(pc->op));
        } else if (obj->datatype == DDATA) {
            pc->alias = make_alias(obj, 0, 0);
        } else {
            CError_FATAL(261);
        }
    }
    if (!pc->alias) {
        pc->flags |= 0x20;
    }
    appendpcode(data_006246B4, pc);
    return pc;
}

// Load floating-point register reg from obj+offset addressed from base.
void func_00480A00(SInt16 op, SInt16 reg, SInt16 base, Object* obj, SInt32 offset) {
    MPCode* pc;

    pc = func_00480420(op, 3);
    pc->args[0].kind = 0;
    pc->args[0].rclass = 1;
    pc->args[0].reg = reg;
    pc->args[0].flags = 2;
    if (op == (UInt16)((copts.mips_isa >= 2 && (copts.fp_library & 2)) ? 0x563 : 0x560)) {
        pc->args[0].flags |= 0x8000;
        if (copts.mips_option1A) {
            pc->args[0].flags |= 0x20;
        }
    }
    pc->args[1].kind = 4;
    MOP_SETOBJ(pc->args[1], obj);
    pc->args[1].value = offset;
    pc->args[2].kind = 0;
    pc->args[2].rclass = 0;
    pc->args[2].reg = base;
    pc->args[2].flags = 1;
    if (obj) {
        if (obj->datatype == DLOCAL && pc->args[2].reg != data_00625E3E) {
            pc->alias = make_alias(obj, 0, 0);
        } else if (obj->datatype == DLOCAL) {
            pc->alias = make_alias(obj, offset - OBJECT_FRAMEOFFSET(obj), func_0048FBE0(pc->op));
        } else if (obj->datatype == DDATA) {
            pc->alias = make_alias(obj, 0, 0);
        } else {
            CError_FATAL(148);
        }
    }
    if (obj && is_volatile_object(obj)) {
        pc->flags |= 0x80;
    }
    if (obj && is_const_object(obj)) {
        pc->flags |= 0x40;
    }
    appendpcode(data_006246B4, pc);
}
