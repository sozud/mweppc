#include "compiler/common.h"
#include <stdio.h>
#include <stdlib.h>
#include "psp/BE.h"
#include "compiler/CParser.h"
#include "compiler/CInt64.h"
#include "compiler/objects.h"
#include "compiler/types.h"

// mwccpsp 3.0.1 stack frame layout (0x48F260; unit name not known): sizes
// the register save areas and the frame once registers are colored, then
// expands the frame pseudo-instructions.

typedef struct UsedRegs {
    SInt32 codewords;
    UInt32 intregs;
    UInt32 fpregs;
} UsedRegs;

#define ALIGN_FRAME(n) (((n) + (data_0062421C - 1)) & ~(data_0062421C - 1))

extern UsedRegs* data_006241D8;
extern SInt32 data_0062421C; // stack alignment
extern SInt32 data_00624650; // locals size
extern SInt32 data_0062597C; // frame size
extern char data_00625EAE; // the frame pointer is used
extern SInt32 data_00625478[7]; // registers used, per class ...
extern SInt32 data_0060EF70[7][0x80]; // ... and which
extern SInt32 data_00624788; // size of a saved integer register
extern SInt32 data_00625018; // integer save area
extern SInt32 data_006250AC; // floating-point save area
extern SInt32 data_00624DB0;
extern SInt32 data_005F0BD4; // padding
extern SInt32 data_006242B4; // outgoing argument area
extern char data_005F0BFC; // a frame is needed
extern MPCodeBlock* data_00624C58;
extern SInt32 data_00624D3C;
extern SInt32 data_005F0BDC;
extern SInt32 func_0048F9D0(Object* func, SInt32* x); // interrupt frame size
extern SInt32 data_00624FCC; // locals start
extern char data_00625EA4;
extern SInt16 data_00625E3E; // frame register

// Frame-relative operands get their final offsets; any use of the frame
// register means the function needs a frame.
void func_0048F630(MPCode* pc) {
    int i;
    SInt32 pad;

    pad = ALIGN_FRAME(data_00624FCC) - data_00624FCC;
    if (data_00625EA4 && (pc->flags & 8)) {
        return;
    }
    for (i = 0; i < pc->argcount; i++) {
        if (pc->args[i].kind == 0) {
            if (data_00625E3E == pc->args[i].reg) {
                data_005F0BFC = 1;
            }
        } else if (pc->args[i].kind == 4 &&
                   pc->args[i].value2 &&
                   ((Object*)MOP_OBJ(pc->args[i]))->datatype == DLOCAL) {
            if (pc->args[i].value >= 0) {
                pc->args[i].value += pad;
            }
            pc->args[i].value += data_0062597C;
        }
    }
}
extern void func_0048B5D0(MPCode** pc, int flag);
extern void func_004584C0(CInt64* dst, SInt32 value); // CInt64_SetLong
extern CInt64 func_00458340(CInt64 a, CInt64 b); // CInt64_Add

void func_0048F260(Object* func) {
    int i;
    MPCodeBlock* block;
    MPCode* pc;
    CInt64 delta;

    if (!copts.interrupt) {
        data_0062597C = data_00624650 = ALIGN_FRAME(data_00624650);
    } else {
        data_0062597C = func_0048F9D0(func, &data_005F0BDC);
    }
    if (data_00625EAE) {
        data_006241D8->intregs |= 0x40000000;
    }
    for (i = 0; i < data_00625478[0]; i++) {
        data_006241D8->intregs |= BIT32(data_0060EF70[0][i]);
    }
    for (i = 0; i < data_00625478[1]; i++) {
        data_006241D8->fpregs |= BIT32(data_0060EF70[1][i]);
        if (!copts.floatsp32) {
            data_006241D8->fpregs |= BIT32((data_0060EF70[1][i] + 1));
        }
    }

    for (i = 0; i < 32; i++) {
        if (copts.mips_option08 && i == 23) {
            continue;
        }
        if ((data_006241D8->intregs & (BIT32(i))) && ((BIT32(i)) & 0xC0FF0000)) {
            data_00625018 += data_00624788;
        }
    }
    if (data_00625018) {
        data_00625018 = ALIGN_FRAME(data_00625018);
    }
    if (copts.fp_library & 1) {
        for (i = 0; i < 32; i++) {
            if ((data_006241D8->fpregs & (BIT32(i))) && ((BIT32(i)) & 0xFFF00000)) {
                data_006250AC += 4;
            }
        }
        if (data_006250AC) {
            data_006250AC = ALIGN_FRAME(data_006250AC);
        }
    }
    data_00624DB0 += data_006250AC + data_00625018;
    if (data_00624DB0 != ALIGN_FRAME(data_00624DB0)) {
        data_005F0BD4 = (data_0062421C == 16) ? 8 : 4;
    } else {
        data_005F0BD4 = 0;
    }
    data_0062597C += data_006242B4 + data_00624DB0 + data_005F0BD4;

    data_005F0BFC = 0;
    for (block = data_00624FD0; block; block = block->next) {
        data_00624C58 = block;
        for (pc = block->first; pc; pc = pc->next) {
            func_0048F630(pc);
        }
    }
    if (!data_005F0BFC) {
        if ((data_006241D8->intregs & 0xC0FF0000) || (data_006241D8->fpregs & 0xFFF00000)) {
            data_005F0BFC = 1;
        }
    }
    if (!data_005F0BFC) {
        data_0062597C = 0;
        data_00625018 = 0;
        data_006250AC = 0;
    }

    for (block = data_00624FD0; block; block = block->next) {
        data_00624C58 = block;
        for (pc = block->first; pc; pc = pc->next) {
            if (pc->unk38 == 0x11) {
                // an offset into the outgoing argument area
                func_004584C0(&delta, data_006242B4);
                *(CInt64*)&pc->args[2].value = func_00458340(*(CInt64*)&pc->args[2].value, delta);
                pc->unk38 = 0;
            }
            if (pc->op >= 0x518) {
                data_00624D3C = 0;
                func_0048B5D0(&pc, 0);
            }
        }
    }
}

extern SInt16 func_00464E20(Object* obj); // alignment of a variable
#define OBJECT_FRAMEOFFSET(obj) (*(SInt32*)&((Object*)(obj))->u.var.offset) // +0x30

// Give a local a slot below the frame's other locals.
void func_004A4660(Object* obj) {
    SInt16 align = func_00464E20(obj);

    data_00624650 += obj->type->size;
    data_00624650 += align - 1;
    data_00624650 &= ~(align - 1);
    OBJECT_FRAMEOFFSET(obj) = -data_00624650;
}
