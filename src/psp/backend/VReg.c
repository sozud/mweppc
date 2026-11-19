#include "compiler/mwcc_core.h"
#include "compiler/common.h"
#include <string.h>
#include "psp/BE.h"
#include "compiler/CError.h"
#include "compiler/CInt64.h"
#include "compiler/CFunc.h"
#include "compiler/ub_probe.h"

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
#if MWCC_CORE == MWCC_CORE_B52
#undef CError_B52_SHIFT
#define CError_B52_SHIFT (-20)
#endif
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
#if MWCC_CORE == MWCC_CORE_B52
#undef CError_B52_SHIFT
#define CError_B52_SHIFT (0)
#endif

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

// b198 leaves its operands' rclass/flags as stack residue (func_004C3C60 is exact
// there: 0x4D006F, 0x4D00A4): natively a defined value, or the original's when replaying
#if MWCCPSP_NATIVE && MWCCPS2 && MWCCPS2_BUILD >= 198 && MWCCPS2_BUILD < 210
#include <stdio.h>
#include <stdlib.h>
// Classification aid: MWCCPS2_B119_RESIDUE names a file of the original's
// residue words, one "kind rclass flags" hex triple per kind-4 (memory) or
// kind-3 (immediate) operand in order
// (tools/mwccps2/b119/residue/resid.py --seq). Replaying them shows whether a
// case fails only because of the residue.
static int b119_residue_used, b119_residue_short;
static FILE* b119_residue_file;
static void b119_residue_report(void) {
    unsigned rc, fl;
    int left = 0;
    unsigned k;
    while (b119_residue_file && fscanf(b119_residue_file, "%x %x %x", &k, &rc, &fl) == 3) {
        left++;
    }
    if (b119_residue_short || left) {
        char path[4096];
        FILE* r;
        snprintf(path, sizeof(path), "%s.mis", getenv("MWCCPS2_B119_RESIDUE"));
        if ((r = fopen(path, "w"))) {
            fprintf(
                r,
                "%d operands, %d past the end, %d left over\n",
                b119_residue_used,
                b119_residue_short,
                left
            );
            fclose(r);
        }
    }
}
static void b119_replay_residue(MOperand* op, int kind) {
    static FILE* f;
    static int opened;
    unsigned k, rc, fl;
    if (!opened) {
        const char* path = getenv("MWCCPS2_B119_RESIDUE");
        opened = 1;
        f = path ? fopen(path, "r") : NULL;
        if (f) {
            atexit(b119_residue_report);
        }
    }
    b119_residue_used++;
    if (f && fscanf(f, "%x %x %x", &k, &rc, &fl) == 3) {
        if ((int)k != kind) {
            b119_residue_short++; // out of step: count it as misaligned
        }
        op->rclass = rc;
        op->flags = fl;
    } else if (f) {
        b119_residue_short++;
    }
    b119_residue_file = f;
}
#endif
// The instruction operand for a VOperand.
MOperand func_004C3C60(VOperand* v) {
    MOperand op;
    MOperand imm;

#if MWCC_CORE == MWCC_CORE_B52
    // b52 (0x4ADF30, 0x4ADF00) leaves a memory or immediate operand's arg
    // byte as stack residue, and a reader distinguishes it (forced to 0xFF
    // the original differs on 179 corpus cases, forced to 0 on none): the
    // native takes 0, the original's usual value
    memset(&op, 0, sizeof(op));
    memset(&imm, 0, sizeof(imm));

#endif
    switch (v->kind) {
        case 2:
        case 4:
            op.kind = 4;
#if MWCCPSP_NATIVE && MWCCPS2 && MWCCPS2_BUILD >= 198 && MWCCPS2_BUILD < 210
            // b198 leaves rclass/flags as stack residue (often the high half of a heap or
            // code pointer); copypropagatestouse tests flags & 8 on every operand
            op.rclass = 0;
            op.flags = mwcc_ub_fill("operand", 0);
            b119_replay_residue(&op, 4);
#endif
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
#if MWCCPSP_NATIVE && MWCCPS2 && MWCCPS2_BUILD >= 198 && MWCCPS2_BUILD < 210
            // and an immediate's ([esp+0x19..0x1b] at 0x4D00A4)
            imm.rclass = 0;
            imm.flags = mwcc_ub_fill("operand", 0);
#endif
            *(CInt64*)&imm.value = v->imm;
            op = imm;
#if MWCCPSP_NATIVE && MWCCPS2 && MWCCPS2_BUILD >= 198 && MWCCPS2_BUILD < 210
            b119_replay_residue(&op, 3);
#endif
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

#if MWCC_CORE == MWCC_CORE_B52
    // b52 (0x4ADF30, 0x4ADF00) leaves a memory or immediate operand's arg
    // byte as stack residue, and a reader distinguishes it (forced to 0xFF
    // the original differs on 179 corpus cases, forced to 0 on none): the
    // native takes 0, the original's usual value
    memset(&op, 0, sizeof(op));

#endif
    op.kind = 3;
    *(CInt64*)&op.value = value;
    return op;
}

MOperand func_004C3D90(SInt32 value) {
    MOperand op;
#if MWCC_CORE == MWCC_CORE_B52

    // b52 (0x4ADF30, 0x4ADF00) leaves a memory or immediate operand's arg
    // byte as stack residue, and a reader distinguishes it (forced to 0xFF
    // the original differs on 179 corpus cases, forced to 0 on none): the
    // native takes 0, the original's usual value
    memset(&op, 0, sizeof(op));
#endif

    op.kind = 3;
    CInt64_SetLong((CInt64*)&op.value, value);
    return op;
}

// A new virtual register of the class, as a definition.
#if MWCCPS2 && !MWCC_CORE
// mwccps2 (b198 new_register_arg, b210): an if-chain (the PS2 b198 decompilation's)
MOperand func_004C3DD0(UInt8 rclass) {
    MOperand op;

    op.kind = 0;
    op.rclass = rclass;
    op.flags = 2;
    if (rclass == 0) {
        op.reg = data_00625A98[0]++;
    } else if (rclass == 1) {
        op.reg = data_00625A98[1]++;
    } else if (rclass == 5) {
        op.reg = data_00625A98[5]++;
    } else {
        CError_Internal("VReg.c", CError_B145_LINE(60));
    }
    return op;
}
#else
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
#endif

#if MWCCPS2 && !MWCC_CORE && !MWCCPSP_NATIVE
// mwccps2 (b198 0x4D02D0, b210): int parameters, the class before the register
// (the PS2 b198 decompilation's fn_004D02D0); the native keeps the SInt16 prototype
// its callers declare
MOperand func_004C3E70(int reg, int rclass, int flags) {
    MOperand op;

    op.kind = 0;
    op.rclass = rclass;
    op.reg = reg;
    op.flags = flags;
    return op;
}
#else
MOperand func_004C3E70(SInt16 reg, UInt8 rclass, UInt16 flags) {
    MOperand op;

    op.kind = 0;
    op.rclass = rclass;
    op.flags = flags;
    op.reg = reg;
    return op;
}
#endif

Boolean func_004C40A0(void) {
    return 0;
}
