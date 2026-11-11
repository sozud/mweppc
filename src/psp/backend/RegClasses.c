#include "compiler/common.h"
#include "compiler/CompilerTools.h"
#include "compiler/objects.h"
#include "psp/BE.h"
#include "compiler/CParser.h"
#include "compiler/CError.h"

// mwccpsp 3.0.1, 0x462050..0x4622E0: per-register-class bookkeeping (seven
// classes of up to 128 registers). The original unit name is not known; the
// functions sit just before ObjGen.c in the link. Tables are named by
// address until they are identified.

extern SInt32 data_00625A98[7];
extern SInt32 data_00624588[7];
extern SInt32 data_00624BCC[7];
extern SInt32 data_00625478[7]; // next free slot in the allocation order
extern SInt32 data_00624670[7]; // allocation order length
extern SInt32 data_00624744[7]; // registers in the class
extern SInt32 data_006241E0[7];
extern SInt32 data_00624C1C[7];
extern SInt32 data_0060EF70[7][128]; // allocation order
extern SInt32 data_0060E038[7][128];
extern UInt8 data_0060FEE8[7][128]; // register in use
extern SInt32 data_00625350;
extern SInt32 data_0062482C;
extern void bitvectorinitialize(UInt32* bv, UInt32 size, UInt32 initial);
extern char* data_006242D8[7]; // class names, for listings
extern char* data_00625214[7]; // register name formats
extern char data_00625EAE; // the frame pointer is used
extern SInt16 data_00625E3E; // frame register
void func_004B8C50(void);

// VarInfo's 3.0.1 layout is not rebuilt yet; these are its register fields.
#define VARINFO_FLAGS(vi) (*(UInt8*)&((VarInfo*)(vi))->flags) // +0x26
#define VARINFO_RCLASS(vi) (*(char*)&((VarInfo*)(vi))->rclass) // +0x27
#define VARINFO_REG(vi) (*(SInt16*)&((VarInfo*)(vi))->reg) // +0x28
#define OBJECT_VARINFO(obj) (*(void**)&((Object*)(obj))->u.var.info) // +0x28

void func_00462050(void) {
    RegClass301 rclass;

    for (rclass = 0; rclass < 7; rclass++) {
        data_00624588[rclass] = data_00625A98[rclass];
        data_00624BCC[rclass] = data_00624588[rclass];
    }
}

SInt32 func_00462090(char rclass) {
    SInt32 reg;

    reg = -1;
    if (data_00625478[rclass] < data_00624670[rclass]) {
        reg = data_0060EF70[rclass][data_00625478[rclass]++];
    }
    return reg;
}

UInt32* func_004620D0(char rclass) {
    UInt32* bv;
    SInt32 i;
    SInt32 reg;

    bv = oalloc(((data_00624744[rclass] + 31) >> 5) * 4);
    bitvectorinitialize(bv, data_00624744[rclass], 0);
    for (i = 0; i < data_006241E0[rclass]; i++) {
        reg = data_0060E038[rclass][i];
        if (!data_0060FEE8[rclass][reg]) {
            bv[reg >> 5] |= BIT32(reg);
        }
    }
    return bv;
}

SInt32 func_00462160(char rclass) {
    SInt32 count;
    SInt32 i;

    count = 0;
    for (i = 0; i < data_00624744[rclass]; i++) {
        if (!data_0060FEE8[rclass][i]) {
            count++;
        }
    }
    return count;
}

void func_004621A0(Object* obj, char rclass) {
    void* vi;
    SInt16 reg;

    vi = OBJECT_VARINFO(obj);
    reg = data_0060EF70[rclass][data_00625478[rclass]++];
    data_00624C1C[rclass]--;
    data_0060FEE8[rclass][reg] = 1;
    VARINFO_FLAGS(vi) |= 2;
    VARINFO_RCLASS(vi) = rclass;
    VARINFO_REG(vi) = reg;
}

void func_004621F0(void) {
    RegClass301 rclass;
    SInt32 i;

    for (rclass = 0; rclass < 7; rclass++) {
        for (i = 0; i < 128; i++) {
            data_0060FEE8[rclass][i] = 0;
        }
    }
    for (rclass = 0; rclass < 7; rclass++) {
        data_00625478[rclass] = 0;
    }
    data_00625350 = 1;
    func_004B8C50();
    for (rclass = 0; rclass < 7; rclass++) {
        data_00625A98[rclass] = data_00624744[rclass];
    }
    data_0062482C = 1;
}

void func_004622D0(void) {}

// The registers a function writes, per class, kept by name for callers
// (0x461DF0, 0x461E40).
typedef struct FuncRegUse {
    struct FuncRegUse* next;
    Object* func;
    UInt32 written[7];
} FuncRegUse; // 0x24

extern FuncRegUse* data_005EAD88[0x80];

void func_00461E40(UInt32* written) {
    RegClass301 i;
    MPCodeBlock* block;
    MPCode* pc;
    MOperand* op;
    UInt32 n;

    for (i = 0; i < 7; i++) {
        written[i] = 0;
    }
    for (block = data_00624FD0; block; block = block->next) {
        for (pc = block->first; pc; pc = pc->next) {
            for (n = pc->argcount, op = pc->args; n--; op++) {
                if (op->kind == 0 && (op->flags & 2)) {
                    written[(char)op->rclass] |= BIT32(op->reg);
                }
            }
        }
    }
}

// 0x461EE0 (ObjGen_Setup): forget every function's register use
void func_00461EE0(void) {
    int i;

    for (i = 0; i < 0x80; i++) {
        data_005EAD88[i] = NULL;
    }
}

void func_00461DF0(Object* func) {
    FuncRegUse* r = galloc(sizeof(FuncRegUse));

    r->func = func;
    func_00461E40(r->written);
    r->next = data_005EAD88[func->name->hashval & 0x7F];
    data_005EAD88[func->name->hashval & 0x7F] = r;
}

// The MIPS register classes: 0 integer, 1 floating point, 2 special
// (HI/LO, ...), 3/4 coprocessor 2 integer, 5 coprocessor 2 float, 6
// coprocessor 2 special. Marks the reserved registers (2), lists the
// callee-saved ones (data_0060EF70) and then the caller-saved allocatable
// ones (data_0060E038), which a call clobbers.
void func_004B8C50(void) {
    int i;
    int j;
    RegClass301 rclass;

    data_006242D8[0] = "GPR";
    data_00625214[0] = "r%ld";
    data_006242D8[1] = "FPR";
    data_00625214[1] = "f%ld";
    data_006242D8[2] = "SPECIAL";
    data_00625214[2] = "#%ld";
    data_006242D8[3] = "COPROC2_i";
    data_00625214[3] = "vi%ld";
    data_006242D8[4] = "COPROC2_ii";
    data_00625214[4] = "vi%ld";
    data_006242D8[5] = "COPROC2_f";
    data_00625214[5] = "vf%ld";
    data_006242D8[6] = "COPROC2_special";
    data_00625214[6] = "COPROC2_%ld";

    data_00624744[0] = 32;
    data_00624744[1] = 32;
    data_00624744[2] = 32;
    data_00624744[5] = 0x80;
    data_00624744[6] = 0x90;

    data_0060FEE8[0][0] = 2; // $zero
    data_0060FEE8[0][1] = 2; // $at
    data_0060FEE8[0][26] = 2; // $k0
    data_0060FEE8[0][27] = 2; // $k1
    data_0060FEE8[0][28] = 2; // $gp
    data_0060FEE8[0][29] = 2; // $sp
    data_0060FEE8[0][31] = 2; // $ra
    if (data_00625EAE) {
        data_0060FEE8[0][30] = 2; // $fp
        data_00625E3E = 30;
    } else {
        data_00625E3E = 29;
    }
    if (copts.mips_option08) {
        data_0060FEE8[0][23] = 2; // $s7 base register
    }

    data_0060FEE8[3][0] = 2;
    data_0060FEE8[4][0] = 2;
    for (i = 1; i < data_00624744[3]; i++) {
        data_0060FEE8[3][i] = 0;
    }
    data_00624670[3] = 0;
    data_00624C1C[3] = 0;
    for (i = 1; i < data_00624744[4]; i++) {
        data_0060FEE8[4][i] = 0;
    }
    data_00624670[4] = 0;
    data_00624C1C[4] = 0;
    for (i = 1; i < data_00624744[5]; i++) {
        data_0060FEE8[5][i] = 0;
    }
    data_0060FEE8[5][0] = 0;
    data_00624670[5] = 0;
    data_00624C1C[5] = 0;
    for (i = 0; i < data_00624744[6]; i++) {
#if MWCCPSP_NATIVE
        // 3.0.1 bug: class 6 has 0x90 registers, a row 0x80 bytes, so this
        // runs 16 bytes past the table (over member_fileoffset, which is
        // set before use). Natively the bytes land in the table's padding.
        (&data_0060FEE8[6][0])[i] = 2;
#else
        data_0060FEE8[6][i] = 2;
#endif
    }
    data_00624670[6] = 0;
    data_00624C1C[6] = 0;
    for (i = 0; i < data_00624744[1]; i++) {
        data_0060FEE8[1][i] = 0;
        if (!copts.floatsp32 && (i & 1)) {
            data_0060FEE8[1][i] = 2; // odd halves of double pairs
        }
    }
    for (i = 0; i < data_00624744[2]; i++) {
        data_0060FEE8[2][i] = 2;
    }

    // callee-saved: $s0-$s6, $s7 unless it is a base register, $fp unless
    // it is the frame pointer
    data_00624670[0] = 0;
    data_0060EF70[0][data_00624670[0]++] = 16;
    data_0060EF70[0][data_00624670[0]++] = 17;
    data_0060EF70[0][data_00624670[0]++] = 18;
    data_0060EF70[0][data_00624670[0]++] = 19;
    data_0060EF70[0][data_00624670[0]++] = 20;
    data_0060EF70[0][data_00624670[0]++] = 21;
    data_0060EF70[0][data_00624670[0]++] = 22;
    if (!copts.mips_option08) {
        data_0060EF70[0][data_00624670[0]++] = 23;
    }
    if (!data_00625EAE) {
        data_0060EF70[0][data_00624670[0]++] = 30;
    }
    data_00624C1C[0] = data_00624670[0];
    // callee-saved: $f20-$f31 that are usable
    data_00624670[1] = 0;
    for (i = 20; i <= 31; i++) {
        if (!data_0060FEE8[1][i]) {
            data_0060EF70[1][data_00624670[1]++] = i;
        }
    }
    data_00624C1C[1] = data_00624670[1];

    for (rclass = 0; rclass < 7; rclass++) {
        data_006241E0[rclass] = 0;
        for (i = 0; i < data_00624744[rclass]; i++) {
            if (data_0060FEE8[rclass][i]) {
                continue;
            }
            for (j = 0; j < data_00624670[rclass]; j++) {
                if (i == data_0060EF70[rclass][j]) {
                    goto next;
                }
            }
            data_0060E038[rclass][data_006241E0[rclass]++] = i;
next:;
        }
    }
}

// 0x461D60..0x462040 (same unit, before the functions above)

// 0x461D60: the registers a function with internal linkage was recorded
// to use, per class; -1 if not known
void func_00461D60(Object* func, SInt32* regs) {
    RegClass301 rclass;
    FuncRegUse* e;

    for (rclass = 0; rclass < 7; rclass++) {
        regs[rclass] = -1;
    }
    if (!CParser_HasInternalLinkage(func)) {
        return;
    }
    for (e = data_005EAD88[func->name->hashval & 0x7F]; e && e->func != func; e = e->next) {}
    if (e) {
        for (rclass = 0; rclass < 7; rclass++) {
            regs[rclass] = e->written[rclass];
        }
    }
}

// 0x461F50
SInt32 func_00461F50(void) {
    RegClass301 rclass;
    SInt32 total;

    total = 0;
    for (rclass = 0; rclass < 7; rclass++) {
        total += data_006241E0[rclass];
    }
    return total;
}

// 0x461F80
void func_00461F80(void) {
    RegClass301 rclass;

    for (rclass = 0; rclass < 7; rclass++) {
        if (data_00625A98[rclass] < data_00624588[rclass]) {
            data_00625A98[rclass] = data_00624588[rclass];
        } else {
            data_00624588[rclass] = data_00625A98[rclass];
        }
    }
}

// 0x461FC0
void func_00461FC0(void) {
    RegClass301 rclass;

    for (rclass = 0; rclass < 7; rclass++) {
        if (data_00625A98[rclass] < 0 || data_00625A98[rclass] > 0x7FF7) {
            CError_Error(0x14F);
        }
    }
    if (data_00625350) {
        return;
    }
    for (rclass = 0; rclass < 7; rclass++) {
        if (data_00624588[rclass] < data_00625A98[rclass]) {
            data_00624588[rclass] = data_00625A98[rclass];
        }
        if (data_00625A98[rclass] > 0x100) {
            data_00625A98[rclass] = data_00624BCC[rclass];
        }
    }
}
