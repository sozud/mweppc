#include "compiler/common.h"
#include "psp/BE.h"
#include "compiler/CInt64.h"
#include "compiler/CParser.h"
#include "compiler/CError.h"
#include "compiler/objects.h"

// mwccpsp 3.0.1 BE_macro.c (named by its assertions): expansion of the
// assembler macros that remain after register allocation.

#if MWCCPS2
// b210 (0x49AFC0, 0x49B060): 64-bit registers, so zext/sext shift by
// 64 - bits with dsll/dsll32 and dsrl(32)/dsra(32); no seb/seh on the R5900.
__declspec(noinline) void func_0048F6D0(MPCode* pc) {
    SInt32 bits = pc->args[2].value;
    UInt32 mask;
    UInt32 shift;
    MPCode* p;

    if (bits >= 32) {
        mask = -1;
    } else if (bits < 32) {
        mask = (1 << bits) - 1;
    }
    if (mask <= 0xFFFF) {
        pc->op = 7; // andi
        pc->args[2].kind = 3;
        CInt64_SetLong((CInt64*)&pc->args[2].value, mask);
    } else {
        shift = 64 - bits;
        if (shift >= 32) {
            pc->op = 0x85; // dsll32, then dsrl32
            p = makepcode(0x8B, pc->args[0].reg, pc->args[0].reg, shift - 32);
            shift += -32;
        } else {
            pc->op = 0x84; // dsll, then dsrl
            p = makepcode(0x8A, pc->args[0].reg, pc->args[0].reg, shift);
        }
        pc->args[2].value = shift;
        insertpcodeafter(pc, p);
    }
}

__declspec(noinline) void func_0048F750(MPCode* pc) {
    SInt32 shift = 64 - pc->args[2].value;
    MPCode* p;

    if (shift >= 32) {
        pc->op = 0x85; // dsll32, then dsra32
        p = makepcode(0x88, pc->args[0].reg, pc->args[0].reg, shift - 32);
        pc->args[2].value = shift - 32;
    } else {
        pc->op = 0x84; // dsll, then dsra
        p = makepcode(0x87, pc->args[0].reg, pc->args[0].reg, shift);
        pc->args[2].value = shift;
    }
    insertpcodeafter(pc, p);
}
#else
// zext rd, rs, bits
void func_0048F6D0(MPCode* pc) {
    SInt32 bits = pc->args[2].value;
    UInt32 mask;

    mask = (bits >= 32) ? -1 : (BIT32(bits)) - 1;
    if (mask <= 0xFFFF) {
        pc->op = 7; // andi
        pc->args[2].kind = 3;
        CInt64_SetLong((CInt64*)&pc->args[2].value, mask);
    } else {
        MPCode* p;

        pc->op = 0xD2; // sll, then srl
        p = makepcode(0xDA, pc->args[0].reg, pc->args[0].reg, 32 - bits);
        pc->args[2].value = 32 - bits;
        insertpcodeafter(pc, p);
    }
}

// sext rd, rs, bits
void func_0048F750(MPCode* pc) {
    SInt32 bits = pc->args[2].value;
    SInt32 shift = 32 - bits;

    if (bits == 16) {
        pc->op = 0x3F3; // seh
        pc->args[2].kind = 7;
        pc->argcount = 2;
    } else if (bits == 8) {
        pc->op = 0x3F2; // seb
        pc->args[2].kind = 7;
        pc->argcount = 2;
    } else {
        MPCode* p;

        pc->op = 0xD2; // sll, then sra
        p = makepcode(0xD8, pc->args[0].reg, pc->args[0].reg, shift);
        pc->args[2].value = shift;
        insertpcodeafter(pc, p);
    }
}
#endif

extern char data_00625EAE; // the frame pointer is used
extern char data_00625EA4; // the function ends in a tail call
extern SInt32 data_0062421C; // stack alignment
extern SInt32 data_00624FCC; // locals start
extern SInt32 data_0062597C; // frame size
extern SInt32 data_005F0BDC;
extern void func_0048F7C0(int start, MPCode* at, SInt32* x); // save/restore (interrupt)
extern void func_0048EF70(Boolean save, MPCode* at); // save/restore registers
extern void func_0048B5D0(MPCode** ppc, Boolean flag); // expand a macro

typedef struct FrameInfo {
    UInt8 valid; // +0
    UInt8 framereg; // +1
    UInt8 pad[2];
    SInt32 size; // +4
} FrameInfo;

extern FrameInfo data_0060EF38; // the frame of the function being finished
extern SInt16 data_00625E3E; // frame register

// .end: the epilogue.
void func_0048ED20(MPCode* pc) {
    MPCode* move;
    MPCode* p;
    CInt64 size;
    SInt32 n;

    if (data_00625EAE) {
        move = makepcode(0xAA, 29, 30); // move $sp, $fp
        insertpcodebefore(pc, move);
        move->flags |= 0x100;
    }
    if (copts.interrupt) {
        func_0048F7C0(0, pc, &data_005F0BDC);
    } else {
        func_0048EF70(0, pc);
    }
#if MWCCPS2 && MWCCPS2_BUILD < 210
    // b145 records the frame here too
    data_0060EF38.valid = 1;
    data_0060EF38.framereg = data_00625E3E;
    n = ((data_00624FCC + (data_0062421C - 1)) & ~(data_0062421C - 1)) + data_0062597C;
    data_0060EF38.size = n;
#else
    n = ((data_00624FCC + (data_0062421C - 1)) & ~(data_0062421C - 1)) + data_0062597C;
#endif
    if (n) {
        CInt64_SetLong(&size, n);
        p = makepcode(MACRO_OP(0x01), 29, 29, size); // addiu $sp, $sp, size
        p->flags |= 0x100;
        insertpcodebefore(pc, p);
        func_0048B5D0(&p, 0);
    }
    if (copts.interrupt) {
        p = makepcode(0x8F); // eret
        p->flags |= 0x80000;
        insertpcodebefore(pc, p);
    } else if (!data_00625EA4) {
        p = makepcode(0x3B, 31); // jr $ra
        p->flags |= 0x80000;
        insertpcodebefore(pc, p);
    }
    deletepcode(pc);
}

#if !MWCCPS2 || MWCCPS2_BUILD >= 210 // b145 expands .start inline (func_0048B5D0)
// .start: the prologue.
void func_0048EE70(MPCode* pc) {
    MPCode* p;
    CInt64 size;
    SInt32 n;

    data_0060EF38.valid = 1;
    data_0060EF38.framereg = data_00625E3E;
    n = ((data_00624FCC + (data_0062421C - 1)) & ~(data_0062421C - 1)) + data_0062597C;
    data_0060EF38.size = n;
    if (data_0062597C + data_00624FCC) {
        CInt64_SetLong(&size, -n);
        p = makepcode(MACRO_OP(0x01), 29, 29, size); // addiu $sp, $sp, -size
        p->flags |= 0x100;
        insertpcodebefore(pc, p);
        func_0048B5D0(&p, 0);
    }
    if (copts.interrupt) {
        func_0048F7C0(1, pc, &data_005F0BDC);
    } else {
        func_0048EF70(1, pc);
    }
    if (data_00625EAE) {
        insertpcodebefore(pc, makepcode(0xAA, 30, 29)); // move $fp, $sp
    }
    deletepcode(pc);
}
#endif

typedef struct SavedReg {
    UInt8 kind; // +0: 4 saved in the frame, 1 return address
    UInt8 reg; // +1
    UInt8 pad[2];
    SInt32 offset; // +4
    UInt8 base; // +8: frame register
    UInt8 pad2[7];
} SavedReg; // 0x10

typedef struct UsedRegs3 {
    SInt32 codewords;
    UInt32 intregs;
    UInt32 fpregs;
} UsedRegs3;

extern UsedRegs3* data_006241D8;
extern SInt32 data_00624650; // locals size
extern SInt32 data_005F0BD4; // padding
extern SInt32 data_00624788; // size of a saved integer register
extern UInt16 data_00625E52; // load-word opcode
extern UInt16 data_00625E5C; // store-word opcode
extern SavedReg data_00610438[32]; // integer registers saved
extern SavedReg data_00609820[32]; // FP registers saved as doubles
extern SavedReg data_0060A020[32]; // FP registers saved as singles
extern SInt32 data_00624DB8; // lowest integer save slot
extern SInt32 data_00624D70; // where $gp is saved
extern SInt32 data_00624B54; // lowest FP save slot

// Save (in the prologue) or restore (in the epilogue) the callee-saved
// registers the function uses.
#if MWCCPS2
// b210 saves the integer registers as quadwords (sq/lq, $ra with sw/lw),
// unaligned, and pads the FP saves to a multiple of four
void func_0048EF70(Boolean save, MPCode* at) {
    SInt32 offset;
    SInt32 qop;
    SInt32 wop;
    int r;
    int n;
    UInt32 fp;

    offset = data_0062597C - data_00624650 - data_005F0BD4;
    if (save) {
        wop = data_00625E5C;
        data_00610438[31].kind = 1;
        data_00610438[31].reg = 31;
        data_00610438[31].offset = 0;
        data_00624DB8 = 0;
        qop = 0x1D2; // sq
        data_00624B54 = 0;
    } else {
        wop = data_00625E52;
        qop = 0x1D1; // lq
    }

    if (data_006241D8->intregs & 0xC0FF0000) {
        for (r = 31; r >= 0; r--) {
            if (copts.mips_option08 && r == 23) {
                continue;
            }
            if ((data_006241D8->intregs & (BIT32(r))) && ((BIT32(r)) & 0xC0FF0000)) {
                offset -= 16;
                if (r == 31) {
                    insertpcodebefore(at, makepcode((SInt16)wop, r, 0, offset, 29));
                } else {
                    insertpcodebefore(at, makepcode((SInt16)qop, r, 0, offset, 29));
                }
                if (save) {
                    data_00610438[r].kind = 4;
                    data_00610438[r].base = data_00625E3E;
                    data_00610438[r].offset = offset;
                    data_00624DB8 = offset;
                    if (r == 28) {
                        data_00624D70 = offset;
                    }
                }
            }
        }
    }

    fp = data_006241D8->fpregs;
    if (fp & 0xFFF00000) {
        n = 0;
        for (r = 31; r >= 0; r--) {
            if ((fp & (BIT32(r))) && ((BIT32(r)) & 0xFFF00000)) {
                n++;
            }
        }
        while (n & 3) {
            n++;
            offset -= 4;
        }
        for (r = 31; r >= 0; r--) {
            if (!(data_006241D8->fpregs & (BIT32(r))) || !((BIT32(r)) & 0xFFF00000)) {
                continue;
            }
            if (copts.mips_isa >= 2 && (copts.fp_library & 2)) {
                if (r & 1) {
                    continue;
                }
                offset -= 8;
                insertpcodebefore(at, makepcode(save ? 0xC5 : 0x96, r, 0, offset, 29)); // sdc1/ldc1
                if (!save) {
                    continue;
                }
                data_00609820[r].kind = 4;
                data_00609820[r].base = data_00625E3E;
                data_00609820[r].offset = offset;
            } else {
                offset -= 4;
                insertpcodebefore(at, makepcode(save ? 0xCD : 0xA1, r, 0, offset, 29)); // swc1/lwc1
                if (!save) {
                    continue;
                }
                data_0060A020[r].kind = 4;
                data_0060A020[r].base = data_00625E3E;
                data_0060A020[r].offset = offset;
            }
            data_00624B54 = offset;
        }
    }
}
#else
void func_0048EF70(Boolean save, MPCode* at) {
    SInt32 offset;
    UInt16 op;
    UInt16 raop;
    SInt32 size;
    int r;
    int n;
    UInt32 fp;

    offset = data_0062597C - data_00624650 - data_005F0BD4;
    if (save) {
        raop = data_00625E5C;
        data_00610438[31].kind = 1;
        data_00610438[31].reg = 31;
        data_00610438[31].offset = 0;
        data_00624DB8 = 0;
        op = data_00625E5C;
        data_00624B54 = 0;
    } else {
        raop = data_00625E52;
        op = data_00625E52;
    }
    size = data_00624788;

    if (data_006241D8->intregs & 0xC0FF0000) {
        offset = (offset + 15) & ~15;
        for (r = 31; r >= 0; r--) {
            if (copts.mips_option08 && r == 23) {
                continue;
            }
            if ((data_006241D8->intregs & (BIT32(r))) && ((BIT32(r)) & 0xC0FF0000)) {
                offset -= size;
                if (r == 31) {
                    insertpcodebefore(at, makepcode((SInt16)op, r, 0, offset, 29));
                } else {
                    insertpcodebefore(at, makepcode((SInt16)raop, r, 0, offset, 29));
                }
                if (save) {
                    data_00610438[r].kind = 4;
                    data_00610438[r].base = data_00625E3E;
                    data_00610438[r].offset = offset;
                    data_00624DB8 = offset;
                    if (r == 28) {
                        data_00624D70 = offset;
                    }
                }
            }
        }
    }

    fp = data_006241D8->fpregs;
    if (fp & 0xFFF00000) {
        n = 0;
        for (r = 31; r >= 0; r--) {
            if ((fp & (BIT32(r))) && ((BIT32(r)) & 0xFFF00000)) {
                n++;
            }
        }
        while (n & 1) {
            n++;
            offset -= 4;
        }
        for (r = 31; r >= 0; r--) {
            if (!(data_006241D8->fpregs & (BIT32(r))) || !((BIT32(r)) & 0xFFF00000)) {
                continue;
            }
            if (copts.mips_isa >= 2 && (copts.fp_library & 2)) {
                if (r & 1) {
                    continue;
                }
                offset -= 8;
                insertpcodebefore(at, makepcode(save ? 0xC5 : 0x96, r, 0, offset, 29)); // sdc1/ldc1
                if (!save) {
                    continue;
                }
                data_00609820[r].kind = 4;
                data_00609820[r].base = data_00625E3E;
                data_00609820[r].offset = offset;
            } else {
                offset -= 4;
                insertpcodebefore(at, makepcode(save ? 0xCD : 0xA1, r, 0, offset, 29)); // swc1/lwc1
                if (!save) {
                    continue;
                }
                data_0060A020[r].kind = 4;
                data_0060A020[r].base = data_00625E3E;
                data_0060A020[r].offset = offset;
            }
            data_00624B54 = offset;
        }
    }
}
#endif

// ---------------------------------------------------------------------------
// The macro expander (0x48B5D0; functions.csv splits it at 0x48D310).

extern SInt32 data_00624D3C; // expansion depth
extern SInt32 data_00624D70;
extern UInt16 data_00625CE4[]; // macro opcode -> instruction (from 0x518)
extern UInt16 data_00625B86[]; // ... -> the alternative instruction
extern UInt16 data_00625E48; // the move opcode
extern Object* data_005F0BF8; // the function being expanded
extern CInt64 cint64_zero;
extern CInt64 cint64_one;
extern const CInt64 data_005D0074; // 0x7FF8
extern MOperand func_004C3E70(SInt16 reg, UInt8 rclass, UInt16 flags);
extern MOperand func_004C3D90(SInt32 value);
extern MPCode* func_00480420(int op, SInt16 nargs);

#define MACRO_FILE "BE_macro.c"
#define REAL(op) data_00625CE4[(op) - BE_NUM_OPCODES]
#define ALT(op) data_00625B86[(op) - BE_NUM_OPCODES]
#define IMM(o) (*(CInt64*)&(o).value)
#define HI16(v) (((UInt32)(v) >> 16) + (((v) & 0x8000) >> 15))
#define AT_DEF() func_004C3E70(1, 0, 2)
#if MWCCPS2
#define SELF (&pc) // b210 recurses on its local copy, never on ppc
#define LI_HI(o) ((o).unk0C) // the 128-bit constant's bits 32..63
#define LI_LO(o) ((o).value_mid[0]) // ... and its low word
#define OP_ADDIU16 0x78 // daddiu
#define MACRO_ADDIU MACRO_OP(0x30) // daddiu macro
#else
#define SELF ppc
#define LI_HI(o) ((o).value)
#define LI_LO(o) ((o).value2)
#define OP_ADDIU16 3
#define MACRO_ADDIU MACRO_OP(0x01)
#endif
#define AT_USE() func_004C3E70(1, 0, 1)
#define ZERO_USE() func_004C3E70(0, 0, 1)

// Split a misaligned word or doubleword access into its left/right halves.
static void split_unaligned2(MPCode** ppc, Boolean flag, UInt16 first, UInt16 second, SInt32 bump) {
    MPCode* pc = *ppc;
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
    func_0048B5D0(&p, flag);
    func_0048B5D0(ppc, flag);
}

// lui + %lo pair through $at for a memory operand; pc becomes the %lo half.
static MPCode* at_hi(MPCode* pc, SInt32 value) {
    MPCode* p = makepcode(0x90, 1, HI16(value));

    p->unk38 = 5;
    p->flags &= ~0x4000;
    p->relobj = (Object*)MOP_OBJ(pc->args[1]);
    return p;
}

// b145's asserts sit 18 lines earlier, the flag checks (607) 22 lines earlier
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -18
#if MWCCPS2 && MWCCPS2_BUILD < 210
#define FLAG_LINE 585
#else
#define FLAG_LINE 607
#endif
void func_0048B5D0(MPCode** ppc, Boolean flag) {
    MPCode* pc = *ppc;
    MPCode* p;
    MPCode* q;
    UInt16 op;
    SInt16 real;
    SInt16 alt;
    SInt16 reg;
    SInt32 hi;
    SInt32 lo;
    SInt32 value;
    ObjAddress addr;
    MOperand tmp;
#if MWCCPS2 && MWCCPS2_BUILD < 210
    CInt64 size;
#endif

    op = pc->op;
    if (++data_00624D3C >= 10) {
        CError_Internal(MACRO_FILE, CError_B145_LINE(671));
    }
    switch (op) {
        case MACRO_OP(0xA3): // .start
#if MWCCPS2 && MWCCPS2_BUILD < 210
            // b145: func_0048EE70 inline, without recording the frame (.end does)
            if (data_0062597C + data_00624FCC) {
                CInt64_SetLong(
                    &size,
                    -(((data_00624FCC + (data_0062421C - 1)) & ~(data_0062421C - 1)) +
                      data_0062597C)
                );
                p = makepcode(MACRO_OP(0x01), 29, 29, size); // addiu $sp, $sp, -size
                p->flags |= 0x100;
                insertpcodebefore(pc, p);
                func_0048B5D0(&p, 0);
            }
            if (copts.interrupt) {
                func_0048F7C0(1, pc, &data_005F0BDC);
            } else {
                func_0048EF70(1, pc);
            }
            if (data_00625EAE) {
                insertpcodebefore(pc, makepcode(0xAA, 30, 29)); // move $fp, $sp
            }
            deletepcode(pc);
#else
            func_0048EE70(pc);
#endif
            break;
        case MACRO_OP(0xA4): // .end
            func_0048ED20(pc);
            break;
        case MACRO_OP(0xA5):
            deletepcode(pc);
            break;
        case MACRO_OP(0x98): // .restoreGP
            p = makepcode(0x9D, 28, 0, data_00624D70, 29);
            insertpcodebefore(pc, p);
            deletepcode(pc);
            break;
        case MACRO_OP(0x81): // sext
#if MWCCPS2 && MWCCPS2_BUILD < 210
            // b145: func_0048F750 inline
            value = 64 - pc->args[2].value;
            if (value >= 32) {
                pc->op = 0x85; // dsll32, then dsra32
                p = makepcode(0x88, pc->args[0].reg, pc->args[0].reg, value - 32);
                value += -32;
            } else {
                pc->op = 0x84; // dsll, then dsra
                p = makepcode(0x87, pc->args[0].reg, pc->args[0].reg, value);
            }
            pc->args[2].value = value;
            insertpcodeafter(pc, p);
#else
            func_0048F750(pc);
#endif
            break;
        case MACRO_OP(0x82): // zext
            func_0048F6D0(pc);
            break;
        case MACRO_OP(0x94): // sne with an immediate: addiu $t, s, -imm; sltu d, $zero, $t
            p = makepcode(MACRO_ADDIU, 1, pc->args[1].reg, CInt64_Neg(IMM(pc->args[2])));
            insertpcodebefore(pc, p);
            pc->op = 0xD7;
            pc->args[1] = ZERO_USE();
            pc->args[2] = p->args[0];
            pc->args[2].flags = 1;
            func_0048B5D0(&p, flag);
            break;
        case MACRO_OP(0x93): // sne with registers
            if (pc->args[2].reg == 0) {
                pc->args[2] = pc->args[1];
                pc->args[1] = ZERO_USE();
                pc->op = 0xD7;
            } else if (pc->args[1].reg != 0) {
                pc->op = 0xE4;
                insertpcodeafter(pc, makepcode(0xD7, pc->args[0].reg, 0, pc->args[0].reg));
            } else {
                pc->op = 0xD7;
            }
            break;
        case MACRO_OP(0x83): // seq with registers
            pc->op = 0xE4;
            goto seq_tail;
        case MACRO_OP(0x84): // seq with an immediate
            pc->op = MACRO_OP(0x97);
            func_0048B5D0(SELF, flag);
seq_tail:
            insertpcodeafter(pc, makepcode(0xD6, pc->args[0].reg, pc->args[0].reg, 1));
            break;
        case MACRO_OP(0x45): // la of a label
la_label:
            p = makepcode(0x90, pc->args[0].reg, 0);
            insertpcodebefore(pc, p);
            q = func_00480420(3, 3);
            q->args[0] = pc->args[0];
            q->args[0].flags = 2;
            q->args[1] = pc->args[0];
            q->args[1].flags = 0x11;
            q->args[2] = pc->args[1];
            insertpcodebefore(pc, q);
            p->unk38 = 5;
            p->flags &= ~0x4000;
            p->relobj = data_005F0BF8;
            p->alias = pc->alias;
            q->unk38 = 6;
            p->relobj = data_005F0BF8; // sic: p again
            q->alias = pc->alias;
            p->pair = q;
            q->pair = p;
            if (copts.mips_option08) {
                insertpcodebefore(pc, makepcode(4, pc->args[0].reg, pc->args[0].reg, 23));
            }
            deletepcode(pc);
            break;
        case MACRO_OP(0x44): // la
            if (pc->args[1].kind == 6) {
                goto la_label;
            }
            ObjGen_GetObjectAddress((Object*)MOP_OBJ(pc->args[1]), &addr);
            reg = pc->args[0].reg;
            value = pc->args[1].value;
            if (addr.basereg == 28) {
                if (addr.near) {
                    p = makepcode(3, reg, 28, value);
                    p->unk38 = addr.gpreg ? 8 : 7;
                    p->alias = pc->alias;
                    p->relobj = (Object*)MOP_OBJ(pc->args[1]);
                    goto replace_with_p;
                }
                p = at_hi(pc, value);
                p->alias = pc->alias;
                insertpcodebefore(pc, p);
                q = makepcode(3, reg, 1, (UInt16)value);
                q->unk38 = 6;
                q->relobj = (Object*)MOP_OBJ(pc->args[1]);
                q->alias = pc->alias;
                insertpcodebefore(pc, q);
                p->pair = q;
                q->pair = p;
                p = makepcode(4, pc->args[0].reg, q->args[0].reg, 28);
                p->alias = pc->alias;
                insertpcodebefore(pc, p);
                deletepcode(pc);
            } else if (addr.basereg == 23) {
                p = makepcode(0x90, reg, HI16(value));
                insertpcodebefore(pc, p);
                q = makepcode(3, pc->args[0].reg, pc->args[0].reg, (UInt16)pc->args[1].value);
                insertpcodebefore(pc, q);
                p->unk38 = 5;
                p->flags &= ~0x4000;
                p->relobj = (Object*)MOP_OBJ(pc->args[1]);
                p->alias = pc->alias;
                q->unk38 = 6;
                q->relobj = (Object*)MOP_OBJ(pc->args[1]);
                q->alias = pc->alias;
                p->pair = q;
                q->pair = p;
                p = makepcode(4, pc->args[0].reg, pc->args[0].reg, 23);
                p->alias = pc->alias;
                insertpcodebefore(pc, p);
                deletepcode(pc);
            } else if (addr.basereg == 0) {
                if (addr.near) {
                    p = makepcode(3, pc->args[0], 0, pc->args[1].value);
                    p->unk38 = addr.gpreg ? 8 : 7;
                    p->relobj = (Object*)MOP_OBJ(pc->args[1]);
                    p->alias = pc->alias;
                    goto replace_with_p;
                }
                p = makepcode(0x90, reg, HI16(value));
                insertpcodebefore(pc, p);
                q = makepcode(3, pc->args[0].reg, pc->args[0].reg, (UInt16)pc->args[1].value);
                insertpcodebefore(pc, q);
                p->unk38 = 5;
                p->flags &= ~0x4000;
                p->relobj = (Object*)MOP_OBJ(pc->args[1]);
                p->alias = pc->alias;
                q->unk38 = 6;
                q->relobj = (Object*)MOP_OBJ(pc->args[1]);
                q->alias = pc->alias;
                p->pair = q;
                q->pair = p;
                deletepcode(pc);
            } else if (addr.basereg == data_00625E3E ||
#if MWCCPS2
                       (pc->args[2].reg <= 0x20 && pc->args[2].reg != 0)) {
#else
                       ((UInt16)pc->args[2].reg <= 0x20 && pc->args[2].reg != 0)) {
#endif
                // base register + offset
                p = func_00480420(MACRO_OP(0x01), 3);
                p->args[0] = pc->args[0];
                p->args[1] = pc->args[2];
                p->args[2] = func_004C3D90(pc->args[1].value);
                p->unk38 = pc->unk38;
                p->relobj = (Object*)MOP_OBJ(pc->args[1]);
                p->alias = pc->alias;
                insertpcodebefore(pc, p);
                deletepcode(pc);
                func_0048B5D0(&p, flag);
            } else {
                CError_Internal(MACRO_FILE, CError_B145_LINE(903));
            }
            break;
        case MACRO_OP(0x46): // li
            hi = LI_HI(pc->args[1]);
            lo = LI_LO(pc->args[1]);
            reg = pc->args[0].reg;
            if (copts.mips_isa >= 3 &&
                ((hi != 0 && hi != -1) ||
                 (hi == -1 && !(lo & 0x80000000)) ||
                 (hi == 0 && (lo & 0x80000000)))) {
                // a 64-bit constant, 16 bits at a time
                if (pc->unk38) {
                    CError_Internal(MACRO_FILE, CError_B145_LINE(914));
                }
                if (flag) {
                    CError_Internal(MACRO_FILE, FLAG_LINE);
                }
                insertpcodebefore(pc, makepcode(0xBF, reg, 0, (UInt16)(hi >> 16)));
                insertpcodebefore(pc, makepcode(0x84, pc->args[0].reg, pc->args[0].reg, 16));
                if ((UInt16)hi) {
                    insertpcodebefore(
                        pc, makepcode(0xBF, pc->args[0].reg, pc->args[0].reg, (UInt16)hi)
                    );
                }
                insertpcodebefore(pc, makepcode(0x84, pc->args[0].reg, pc->args[0].reg, 16));
                lo = LI_LO(pc->args[1]);
                if ((lo >> 16) & 0xFFFF) {
                    insertpcodebefore(
                        pc, makepcode(0xBF, pc->args[0].reg, pc->args[0].reg, (lo >> 16) & 0xFFFF)
                    );
                }
                insertpcodebefore(pc, makepcode(0x84, pc->args[0].reg, pc->args[0].reg, 16));
                if ((UInt16)lo) {
                    insertpcodebefore(
                        pc, makepcode(0xBF, pc->args[0].reg, pc->args[0].reg, (UInt16)lo)
                    );
                }
                deletepcode(pc);
                break;
            }
            if (pc->unk38 == 2) {
                // relocated: lui $at, %hi; addiu reg, $at, %lo
                if (flag) {
                    CError_Internal(MACRO_FILE, FLAG_LINE);
                }
                p = makepcode(0x90, 1, HI16(lo));
                p->unk38 = 5;
                p->flags &= ~0x4000;
                p->relobj = NULL;
                p->alias = pc->alias;
                insertpcodebefore(pc, p);
                q = makepcode(3, pc->args[0].reg, 1, (UInt16)lo);
                q->unk38 = 6;
                q->relobj = NULL;
                q->alias = pc->alias;
                insertpcodebefore(pc, q);
                deletepcode(pc);
                p->pair = q;
                q->pair = p;
                break;
            }
            if (lo == 0) {
                p = makepcode((SInt16)data_00625E48, pc->args[0].reg, 0);
            } else if (lo >= -0x8000 && lo < 0x8000) {
                p = func_00480420(OP_ADDIU16, 3);
                p->args[0] = pc->args[0];
                p->args[1] = ZERO_USE();
                p->args[2] = pc->args[1];
            } else if (lo >= 0 && lo < 0x10000) {
                p = func_00480420(0xBF, 3);
                p->args[0] = pc->args[0];
                p->args[1] = ZERO_USE();
                p->args[2] = pc->args[1];
            } else if ((UInt16)lo) {
                if (flag) {
                    CError_Internal(MACRO_FILE, FLAG_LINE);
                }
                insertpcodebefore(pc, makepcode(0x90, 1, (UInt16)(lo >> 16)));
                insertpcodebefore(pc, makepcode(0xBF, pc->args[0].reg, 1, lo & 0xFFFF));
                deletepcode(pc);
                break;
            } else {
                pc->op = 0x90;
                pc->args[1] = func_004C3D90((UInt16)(lo >> 16));
                break;
            }
replace_with_p:
            if (flag) {
                *ppc = p;
                break;
            }
            insertpcodebefore(pc, p);
            deletepcode(pc);
            break;
        case MACRO_OP(0x31):
        case MACRO_OP(0x33):
        case MACRO_OP(0x35):
        case MACRO_OP(0x36):
        case MACRO_OP(0x38):
        case MACRO_OP(0x3A):
        case MACRO_OP(0x40):
        case MACRO_OP(0x41):
        case MACRO_OP(0x5F):
        case MACRO_OP(0x60):
        case MACRO_OP(0x61):
        case MACRO_OP(0x62):
        case MACRO_OP(0x63):
        case MACRO_OP(0x65):
        case MACRO_OP(0x67):
        case MACRO_OP(0x6A):
        case MACRO_OP(0x6B):
            // multiply/divide into HI/LO, then move the result
            alt = ALT(op);
            p = func_00480420((SInt16)REAL(op), 2);
            p->args[0] = pc->args[1];
            p->args[1] = pc->args[2];
            q = func_00480420(alt, 1);
            q->args[0] = pc->args[0];
            insertpcodebefore(pc, p);
            insertpcodebefore(pc, q);
            deletepcode(pc);
            break;
        case MACRO_OP(0x42):
        case MACRO_OP(0x43):
        case MACRO_OP(0x95):
        case MACRO_OP(0x96): // sub immediate: add the negation
            pc->op = REAL(op);
            IMM(pc->args[2]) = CInt64_Neg(IMM(pc->args[2]));
            func_0048B5D0(SELF, flag);
            break;
        case MACRO_OP(0x48):
        case MACRO_OP(0x6C): // l.d, s.d
            if (copts.mips_isa >= 2 && (copts.fp_library & 2)) {
                pc->op = REAL(op);
                func_0048B5D0(SELF, flag);
                break;
            }
            // two single-word accesses
            real = REAL(op);
            p = func_00480420(real, 3);
            p->flags = pc->flags;
            p->relobj = (Object*)MOP_OBJ(pc->args[1]);
            p->unk38 = pc->unk38;
            p->alias = pc->alias;
            p->args[0] = pc->args[0];
            if (!copts.littleendian) {
                p->args[0].flags |= 0x4000;
            } else {
                pc->args[0].flags |= 0x4000;
            }
            p->args[1] = pc->args[1];
            p->args[2] = pc->args[2];
            insertpcodebefore(pc, p);
            pc->op = real;
            pc->args[1].value += 4;
            func_0048B5D0(&p, flag);
            func_0048B5D0(SELF, flag);
            break;
        case MACRO_OP(0x8E):
        case MACRO_OP(0x90): // sle with an immediate: slt with imm + 1
            pc->op = REAL(op);
            IMM(pc->args[2]) = CInt64_Add(IMM(pc->args[2]), cint64_one);
            func_0048B5D0(SELF, flag);
            break;
        case MACRO_OP(0x00):
        case MACRO_OP(0x01):
        case MACRO_OP(0x2F):
        case MACRO_OP(0x30): // add immediate
            real = REAL(op);
            alt = ALT(op);
            if (!pc->unk38 && CInt64_IsInRange(IMM(pc->args[2]), 2)) {
                pc->op = real;
                break;
            }
            if (!pc->unk38 &&
                CInt64_Greater(IMM(pc->args[2]), cint64_zero) &&
                CInt64_IsInRange(CInt64_Sub(IMM(pc->args[2]), data_005D0074), 2)) {
                p = func_00480420(real, 3);
                p->args[0] = pc->args[0];
                p->args[1] = pc->args[0];
                p->args[1].flags = 1;
                p->args[2] = pc->args[2];
                IMM(p->args[2]) = data_005D0074;
                IMM(pc->args[2]) = CInt64_Sub(IMM(pc->args[2]), data_005D0074);
                pc->op = real;
                insertpcodeafter(pc, p);
                break;
            }
            if (!pc->unk38 &&
                CInt64_Less(IMM(pc->args[2]), cint64_zero) &&
                CInt64_IsInRange(CInt64_Add(IMM(pc->args[2]), data_005D0074), 2)) {
                p = func_00480420(real, 3);
                p->args[0] = pc->args[0];
                p->args[1] = pc->args[0];
                p->args[1].flags = 1;
                p->args[2] = pc->args[2];
                IMM(p->args[2]) = CInt64_Neg(data_005D0074);
                IMM(pc->args[2]) = CInt64_Add(IMM(pc->args[2]), data_005D0074);
                pc->op = real;
                insertpcodeafter(pc, p);
                break;
            }
            goto li_at_arg2;
        case MACRO_OP(0x91):
        case MACRO_OP(0x92): // slt with an immediate
            real = REAL(op);
            alt = ALT(op);
            if (CInt64_IsInRange(IMM(pc->args[2]), 2) && !pc->unk38) {
                pc->op = real;
                break;
            }
li_at_arg2:
            // li $at, imm; then the register form
            p = func_00480420(MACRO_OP(0x46), 2);
            p->args[0] = AT_DEF();
            p->args[1] = pc->args[2];
            insertpcodebefore(pc, p);
            p->unk38 = pc->unk38;
            p->alias = pc->alias;
            pc->op = alt;
            pc->args[2] = AT_USE();
            pc->unk38 = 0;
            pc->alias = NULL;
            func_0048B5D0(&p, flag);
            break;
        case MACRO_OP(0x55):
        case MACRO_OP(0x77): // lw, sw
            if (pc->args[1].value % 4 == 0) {
                goto memory;
            }
            CError_Internal(MACRO_FILE, CError_B145_LINE(1203));
            if (pc->op == MACRO_OP(0x55)) {
                if (copts.littleendian) {
                    split_unaligned2(SELF, flag, MACRO_OP(0x5B), MACRO_OP(0x5A), 3);
                } else {
                    split_unaligned2(SELF, flag, MACRO_OP(0x5A), MACRO_OP(0x5B), 3);
                }
            } else if (copts.littleendian) {
                split_unaligned2(SELF, flag, MACRO_OP(0x7E), MACRO_OP(0x7D), 3);
            } else {
                split_unaligned2(SELF, flag, MACRO_OP(0x7D), MACRO_OP(0x7E), 3);
            }
            break;
        case MACRO_OP(0x4A):
        case MACRO_OP(0x6F): // ld, sd
            if (pc->args[1].value % 8 == 0) {
                goto memory;
            }
            CError_Internal(MACRO_FILE, CError_B145_LINE(1244));
            if (pc->op == MACRO_OP(0x4A)) {
                if (copts.littleendian) {
                    split_unaligned2(SELF, flag, MACRO_OP(0x4F), MACRO_OP(0x4E), 7);
                } else {
                    split_unaligned2(SELF, flag, MACRO_OP(0x4E), MACRO_OP(0x4F), 7);
                }
            } else if (copts.littleendian) {
                split_unaligned2(SELF, flag, MACRO_OP(0x74), MACRO_OP(0x73), 7);
            } else {
                split_unaligned2(SELF, flag, MACRO_OP(0x73), MACRO_OP(0x74), 7);
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
#if !MWCCPS2 // b210 has no VFPU macros
        case MACRO_OP(0x9B):
        case MACRO_OP(0x9C):
        case MACRO_OP(0x9D):
        case MACRO_OP(0x9E):
        case MACRO_OP(0x9F):
        case MACRO_OP(0xA0):
        case MACRO_OP(0xA1):
#endif
memory:
            ObjGen_GetObjectAddress((Object*)MOP_OBJ(pc->args[1]), &addr);
            if (addr.basereg == 28) {
                if (addr.near) {
                    pc->op = REAL(op);
                    pc->unk38 = addr.gpreg ? 8 : 7;
                    pc->relobj = (Object*)MOP_OBJ(pc->args[1]);
                    break;
                }
                p = makepcode(0x90, 1, HI16(pc->args[1].value));
                p->unk38 = 5;
                p->flags &= ~0x4000;
                p->relobj = pc->relobj;
                insertpcodebefore(pc, p);
                insertpcodebefore(pc, makepcode(4, pc->args[0].reg, 28, pc->args[0].reg));
                pc->op = REAL(op);
                pc->args[2] = p->args[0];
                pc->args[2].flags = 1;
                pc->args[1].value &= 0xFFFF;
                pc->relobj = (Object*)MOP_OBJ(pc->args[1]);
                pc->unk38 = 6;
                p->pair = pc;
                pc->pair = p;
                break;
            }
            if (addr.basereg == data_00625E3E && pc->args[2].reg != 28) {
                value = pc->args[1].value;
                if (value > 0x7FFF || value < -0x8000) {
                    p = makepcode(0x90, 1, HI16(value));
                    insertpcodebefore(pc, p);
                    insertpcodebefore(
                        pc, makepcode(4, p->args[0].reg, pc->args[2].reg, p->args[0].reg)
                    );
                    pc->args[1].value &= 0xFFFF;
                    pc->args[2] = p->args[0];
                    pc->args[2].flags = 1;
                }
                pc->op = REAL(op);
                break;
            }
            if (addr.basereg == 23) {
                p = makepcode(0x90, 1, HI16(pc->args[1].value));
                p->unk38 = 5;
                p->flags &= ~0x4000;
                p->relobj = (Object*)MOP_OBJ(pc->args[1]);
                insertpcodebefore(pc, p);
                insertpcodebefore(pc, makepcode(4, p->args[0].reg, 23, p->args[0].reg));
                pc->op = REAL(op);
                pc->args[2] = p->args[0];
                pc->args[2].flags = 1;
                pc->args[1].value &= 0xFFFF;
                pc->unk38 = 6;
                pc->relobj = (Object*)MOP_OBJ(pc->args[1]);
                p->pair = pc;
                pc->pair = p;
                break;
            }
            if (addr.basereg == -1) {
                value = pc->args[1].value;
                if (value > 0x7FFF || value < -0x8000) {
                    p = makepcode(0x90, 1, HI16(value));
                    insertpcodebefore(pc, p);
                    if (pc->args[2].reg) {
                        insertpcodebefore(
                            pc, makepcode(4, p->args[0].reg, pc->args[2].reg, p->args[0].reg)
                        );
                    }
                    pc->args[1].value &= 0xFFFF;
                    pc->args[2] = p->args[0];
                    pc->args[2].flags = 1;
                }
                pc->op = REAL(op);
                break;
            }
            if (addr.near) {
                pc->op = REAL(op);
                pc->unk38 = 1;
                pc->relobj = (Object*)MOP_OBJ(pc->args[1]);
                break;
            }
            p = makepcode(0x90, 1, HI16(pc->args[1].value));
            p->unk38 = 5;
            p->flags &= ~0x4000;
            p->relobj = (Object*)MOP_OBJ(pc->args[1]);
            insertpcodebefore(pc, p);
            pc->op = REAL(op);
            pc->args[2] = p->args[0];
            pc->args[2].flags = 1;
            pc->args[1].value &= 0xFFFF;
            pc->unk38 = 6;
            pc->relobj = (Object*)MOP_OBJ(pc->args[1]);
            p->pair = pc;
            pc->pair = p;
            break;
#if !MWCCPS2
        case MACRO_OP(0xA2): // lvi.s
            if (CInt64_IsZero((CInt64*)&pc->args[1].value)) {
                pc->op = 0x40F;
                pc->args[1] = pc->args[0];
                pc->args[0] = func_004C3E70(0, 0, 2);
                pc->args[0].flags = 1;
                break;
            }
            p = func_00480420(MACRO_OP(0x46), 2);
            p->args[0] = AT_DEF();
            p->args[1] = pc->args[1];
            insertpcodebefore(pc, p);
            pc->op = 0x40F;
            pc->args[1] = pc->args[0];
            pc->args[0] = p->args[0];
            pc->args[0].flags = 1;
            func_0048B5D0(&p, flag);
            break;
#endif
        case MACRO_OP(0x02):
        case MACRO_OP(0x69):
        case MACRO_OP(0x97): // and/or/xor with an immediate
            if (CInt64_IsInURange(IMM(pc->args[2]), 2)) {
                pc->op = REAL(op);
                break;
            }
            p = func_00480420(MACRO_OP(0x46), 2);
            p->args[0] = AT_DEF();
            p->args[1] = pc->args[2];
            insertpcodebefore(pc, p);
            pc->op = ALT(op);
            pc->args[2] = p->args[0];
            pc->args[2].flags = 1;
            func_0048B5D0(&p, flag);
            break;
        case MACRO_OP(0x8A):
        case MACRO_OP(0x8C): // sgt with an immediate
            real = REAL(op);
            p = func_00480420(MACRO_OP(0x46), 2);
            p->args[0] = AT_DEF();
            p->args[1] = pc->args[2];
            insertpcodebefore(pc, p);
            pc->op = real;
            pc->args[2] = pc->args[1];
            pc->args[1] = AT_USE();
            func_0048B5D0(&p, flag);
            break;
        case MACRO_OP(0x85):
        case MACRO_OP(0x87):
        case MACRO_OP(0x89):
        case MACRO_OP(0x8B): // sge/sgt with registers: swap
            real = REAL(op);
            pc->op = real;
            tmp = pc->args[1];
            pc->args[1] = pc->args[2];
            pc->args[2] = tmp;
            if (real >= BE_NUM_OPCODES) {
                func_0048B5D0(SELF, flag);
            }
            break;
        case MACRO_OP(0x0D):
        case MACRO_OP(0x0E):
        case MACRO_OP(0x11):
        case MACRO_OP(0x12):
        case MACRO_OP(0x15):
        case MACRO_OP(0x16):
        case MACRO_OP(0x19):
        case MACRO_OP(0x1A):
            // bgt/ble with registers: swap
            pc->op = REAL(op);
            tmp = pc->args[0];
            pc->args[0] = pc->args[1];
            pc->args[1] = tmp;
            func_0048B5D0(SELF, flag);
            break;
        case MACRO_OP(0x8D):
        case MACRO_OP(0x8F): // sle with registers: swap, then xori 1
            real = REAL(op);
            pc->op = real;
            tmp = pc->args[1];
            pc->args[1] = pc->args[2];
            pc->args[2] = tmp;
            insertpcodeafter(pc, makepcode(0xE5, pc->args[0].reg, pc->args[0].reg, 1));
            if (real >= BE_NUM_OPCODES) {
                func_0048B5D0(SELF, flag);
            }
            break;
        case MACRO_OP(0x86):
        case MACRO_OP(0x88): // sge with an immediate: slt, then xori 1
            real = REAL(op);
            pc->op = real;
            insertpcodeafter(pc, makepcode(0xE5, pc->args[0].reg, pc->args[0].reg, 1));
            if (real >= BE_NUM_OPCODES) {
                func_0048B5D0(SELF, flag);
            }
            break;
        case MACRO_OP(0x05):
        case MACRO_OP(0x06):
        case MACRO_OP(0x09):
        case MACRO_OP(0x0A):
        case MACRO_OP(0x1D):
        case MACRO_OP(0x1E):
        case MACRO_OP(0x21):
        case MACRO_OP(0x22):
            // bge/blt with registers: slt $at, then branch on $at
            real = REAL(op);
            alt = ALT(op);
            p = func_00480420(real, 3);
            p->args[0] = AT_DEF();
            p->args[1] = pc->args[0];
            p->args[2] = pc->args[1];
            insertpcodebefore(pc, p);
            pc->op = alt;
            pc->args[0] = p->args[0];
            pc->args[0].flags = 1;
            pc->args[1] = ZERO_USE();
            break;
        case MACRO_OP(0x18): // blel
            if (CInt64_IsZero((CInt64*)&pc->args[1].value)) {
                pc->op = 0x23;
                goto compare_zero;
            }
            goto compare_plus_one;
        case MACRO_OP(0x17): // ble
            if (CInt64_IsZero((CInt64*)&pc->args[1].value)) {
                pc->op = 0x22;
                goto compare_zero;
            }
            goto compare_plus_one;
        case MACRO_OP(0x0F):
        case MACRO_OP(0x10):
        case MACRO_OP(0x13):
        case MACRO_OP(0x14):
        case MACRO_OP(0x1B):
        case MACRO_OP(0x1C):
compare_plus_one:
            // bgt/ble with an immediate: slt $at, s, imm + 1
            real = REAL(op);
            alt = ALT(op);
            p = func_00480420(real, 3);
            p->args[0] = AT_DEF();
            p->args[1] = pc->args[0];
            p->args[2] = pc->args[1];
            IMM(p->args[2]) = CInt64_Add(IMM(p->args[2]), cint64_one);
            pc->op = alt;
            pc->args[0] = p->args[0];
            pc->args[0].flags = 1;
            pc->args[1] = ZERO_USE();
            insertpcodebefore(pc, p);
            func_0048B5D0(&p, flag);
            break;
        case MACRO_OP(0x07): // bge
            if (CInt64_IsZero((CInt64*)&pc->args[1].value)) {
                pc->op = 0x1C;
                goto compare_zero;
            }
            goto compare_imm;
        case MACRO_OP(0x08): // bgel
            if (CInt64_IsZero((CInt64*)&pc->args[1].value)) {
                pc->op = 0x1D;
                goto compare_zero;
            }
            goto compare_imm;
        case MACRO_OP(0x1F): // blt
            if (CInt64_IsZero((CInt64*)&pc->args[1].value)) {
                pc->op = 0x24;
                goto compare_zero;
            }
            goto compare_imm;
        case MACRO_OP(0x20): // bltl
            if (CInt64_IsZero((CInt64*)&pc->args[1].value)) {
                pc->op = 0x25;
                goto compare_zero;
            }
        case MACRO_OP(0x0B):
        case MACRO_OP(0x0C):
        case MACRO_OP(0x23):
        case MACRO_OP(0x24):
compare_imm:
            // slt $at, s, imm; branch on $at
            real = REAL(op);
            alt = ALT(op);
            p = func_00480420(real, 3);
            p->args[0] = AT_DEF();
            p->args[1] = pc->args[0];
            p->args[2] = pc->args[1];
            insertpcodebefore(pc, p);
            pc->op = alt;
            pc->args[0] = p->args[0];
            pc->args[0].flags = 1;
            pc->args[1] = ZERO_USE();
            if (real >= BE_NUM_OPCODES) {
                func_0048B5D0(&p, flag);
            }
            break;
compare_zero:
            pc->args[1] = pc->args[2];
            pc->argcount--;
            break;
        case MACRO_OP(0x32):
        case MACRO_OP(0x34):
        case MACRO_OP(0x37):
        case MACRO_OP(0x39):
        case MACRO_OP(0x3B):
        case MACRO_OP(0x3D):
        case MACRO_OP(0x3F):
        case MACRO_OP(0x64):
        case MACRO_OP(0x66):
        case MACRO_OP(0x68):
            // multiply/divide by an immediate
            if (CInt64_NotEqual(*(CInt64*)&pc->args[1].value, cint64_zero)) {
                p = func_00480420(MACRO_OP(0x46), 2);
                p->args[0] = AT_DEF();
                p->args[1] = pc->args[2];
                insertpcodebefore(pc, p);
                func_0048B5D0(&p, flag);
                pc->args[2] = AT_USE();
            } else {
                pc->args[2] = ZERO_USE();
            }
            q = func_00480420((SInt16)REAL(op), 3);
            q->args[0] = pc->args[0];
            q->args[1] = pc->args[1];
            q->args[2] = pc->args[2];
            insertpcodebefore(pc, q);
            deletepcode(pc);
            func_0048B5D0(&q, flag);
            break;
        case MACRO_OP(0x03):
        case MACRO_OP(0x04):
        case MACRO_OP(0x25):
        case MACRO_OP(0x26): // beq/bne with an immediate
            alt = ALT(op);
            if (CInt64_NotEqual(*(CInt64*)&pc->args[1].value, cint64_zero)) {
                p = func_00480420(MACRO_OP(0x46), 2);
                p->args[0] = AT_DEF();
                p->args[1] = pc->args[1];
                insertpcodebefore(pc, p);
                func_0048B5D0(&p, flag);
                pc->args[1] = p->args[0];
                pc->args[1].flags = 1;
            } else {
                pc->args[1] = ZERO_USE();
            }
            pc->op = alt;
            break;
        case MACRO_OP(0x99): // fcopy
            pc->op = 0xF9;
            break;
        case MACRO_OP(0x9A): // dcopy
            if (copts.fp_library & 2) {
                pc->op = 0xF8;
                break;
            }
            insertpcodebefore(pc, makepcode(0xF9, pc->args[0].reg + 1, pc->args[1].reg + 1));
            pc->op = 0xF9;
            break;
        case MACRO_OP(0x27):
        case MACRO_OP(0x28):
        case MACRO_OP(0x29):
        case MACRO_OP(0x2A):
        case MACRO_OP(0x2B):
        case MACRO_OP(0x2C): // traps
            if (CInt64_IsInRange(*(CInt64*)&pc->args[1].value, 2) && !pc->unk38) {
                pc->op = REAL(op);
                break;
            }
            p = func_00480420(MACRO_OP(0x46), 2);
            p->args[0] = AT_DEF();
            p->args[1] = pc->args[1];
            insertpcodebefore(pc, p);
            p->unk38 = pc->unk38;
            p->alias = pc->alias;
            pc->op = ALT(op);
            pc->args[1] = p->args[0];
            pc->args[1].flags = 1;
            pc->unk38 = 0;
            pc->alias = NULL;
            func_0048B5D0(&p, flag);
            break;
        case MACRO_OP(0x3C):
        case MACRO_OP(0x3E):
        case MACRO_OP(0x47):
        default:
            break;
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#undef FLAG_LINE
