#include "compiler/common.h"
#include <string.h>
#include "psp/BE.h"
#include "compiler/CError.h"
#include "compiler/CInt64.h"
#include "compiler/CFunc.h"

// mwccpsp 3.0.1 VReg.c (named by its assertions; 0x4C3B00..0x4C40B0):
// making operands. A VOperand (0x24 bytes) describes a value for the code
// generator (a register, a memory location or a constant); an MOperand is an
// instruction operand.

// a CLabel's pclabel is at +0x10

extern SInt32 data_00625A98[7]; // next virtual register, per class
extern void* data_00603F88; // the registers live at function exit (Liveness.c)

void func_004C3B00(void* value) {
    data_00603F88 = value;
}

// The VOperand for a register operand.
VOperand func_004C3B10(MOperand op) {
    VOperand v;

    memset(&v, 0, sizeof(v));
    switch ((char)op.rclass) {
        case 0:
            v.kind = 0;
            break;
        case 1:
            v.kind = 1;
            break;
        case 5:
            v.kind = 5;
            break;
        default:
            CError_Internal("VReg.c", CError_B145_LINE(165));
    }
    v.reg = op.reg;
    return v;
}

VOperand func_004C3BA0(SInt16 reg, UInt8 kind) {
    VOperand v;

    memset(&v, 0, sizeof(v));
    v.kind = kind;
    v.reg = reg;
    return v;
}

// The operand for a branch to a label (made on first use).
MOperand func_004C3C00(CLabel* label) {
    MOperand op;

    memset(&op, 0, sizeof(op));
    op.kind = 6;
    if (!label->pclabel) {
        label->pclabel = (struct PCodeLabel*)makepclabel();
    }
    MOP_SETLABEL(op, label->pclabel);
    return op;
}

// The instruction operand for a VOperand.
MOperand func_004C3C60(VOperand* v) {
    MOperand op;
    MOperand imm;

    switch (v->kind) {
        case 2:
        case 4:
            op.kind = 4;
            op.value = v->offset;
            MOP_SETOBJ(op, v->object);
            break;
        case 0:
            op.kind = 0;
            op.rclass = 0;
            op.reg = v->reg;
            op.flags = 1;
            break;
        case 3:
            imm.kind = 3;
            *(CInt64*)&imm.value = v->imm;
            op = imm;
            break;
        case 1:
            op.kind = 0;
            op.rclass = 1;
            op.reg = v->reg;
            op.flags = 1;
            break;
        case 5:
            op.kind = 0;
            op.rclass = 5;
            op.reg = v->reg;
            op.flags = 1;
            break;
        default:
            CError_Internal("VReg.c", CError_B145_LINE(122));
    }
    return op;
}

MOperand func_004C3D50(CInt64 value) {
    MOperand op;

    op.kind = 3;
    *(CInt64*)&op.value = value;
    return op;
}

MOperand func_004C3D90(SInt32 value) {
    MOperand op;

    op.kind = 3;
    CInt64_SetLong((CInt64*)&op.value, value);
    return op;
}

// A new virtual register of the class, as a definition.
MOperand func_004C3DD0(UInt8 rclass) {
    MOperand op;

    op.kind = 0;
    op.rclass = rclass;
    op.flags = 2;
    switch (rclass) {
        case 0:
            op.reg = data_00625A98[0]++;
            break;
        case 1:
            op.reg = data_00625A98[1]++;
            break;
        case 5:
            op.reg = data_00625A98[5]++;
            break;
        default:
            CError_Internal("VReg.c", CError_B145_LINE(60));
    }
    return op;
}

MOperand func_004C3E70(SInt16 reg, UInt8 rclass, UInt16 flags) {
    MOperand op;

    op.kind = 0;
    op.rclass = rclass;
    op.flags = flags;
    op.reg = reg;
    return op;
}

Boolean func_004C40A0(void) {
    return 0;
}
