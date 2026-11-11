#include "compiler/common.h"
#include "psp/BE.h"
#include "compiler/CInt64.h"
#include "compiler/CParser.h"
#include "compiler/CError.h"
#include "compiler/objects.h"

// mwccpsp 3.0.1 BE_macro.c (named by its assertions): expansion of the
// assembler macros that remain after register allocation.

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

extern char data_00625EAE; // the frame pointer is used
extern char data_00625EA4; // the function ends in a tail call
extern SInt32 data_0062421C; // stack alignment
extern SInt32 data_00624FCC; // locals start
extern SInt32 data_0062597C; // frame size
extern SInt32 data_005F0BDC;
extern void func_0048F7C0(int start, MPCode* at, SInt32* x); // save/restore (interrupt)
extern void func_0048EF70(Boolean save, MPCode* at); // save/restore registers
extern void func_0048B5D0(MPCode** ppc, Boolean flag); // expand a macro

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
    n = ((data_00624FCC + (data_0062421C - 1)) & ~(data_0062421C - 1)) + data_0062597C;
    if (n) {
        CInt64_SetLong(&size, n);
        p = makepcode(0x519, 29, 29, size); // addiu $sp, $sp, size
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

typedef struct FrameInfo {
    UInt8 valid; // +0
    UInt8 framereg; // +1
    UInt8 pad[2];
    SInt32 size; // +4
} FrameInfo;

extern FrameInfo data_0060EF38; // the frame of the function being finished
extern SInt16 data_00625E3E; // frame register

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
        p = makepcode(0x519, 29, 29, size); // addiu $sp, $sp, -size
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
#define REAL(op) data_00625CE4[(op) - 0x518]
#define ALT(op) data_00625B86[(op) - 0x518]
#define IMM(o) (*(CInt64*)&(o).value)
#define HI16(v) (((UInt32)(v) >> 16) + (((v) & 0x8000) >> 15))
#define AT_DEF() func_004C3E70(1, 0, 2)
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

    op = pc->op;
    if (++data_00624D3C >= 10) {
        CError_Internal(MACRO_FILE, 671);
    }
    switch (op) {
        case 0x5BB: // .start
            func_0048EE70(pc);
            break;
        case 0x5BC: // .end
            func_0048ED20(pc);
            break;
        case 0x5BD:
            deletepcode(pc);
            break;
        case 0x5B0: // .restoreGP
            p = makepcode(0x9D, 28, 0, data_00624D70, 29);
            insertpcodebefore(pc, p);
            deletepcode(pc);
            break;
        case 0x599: // sext
            func_0048F750(pc);
            break;
        case 0x59A: // zext
            func_0048F6D0(pc);
            break;
        case 0x5AC: // sne with an immediate: addiu $t, s, -imm; sltu d, $zero, $t
            p = makepcode(0x519, 1, pc->args[1].reg, CInt64_Neg(IMM(pc->args[2])));
            insertpcodebefore(pc, p);
            pc->op = 0xD7;
            pc->args[1] = ZERO_USE();
            pc->args[2] = p->args[0];
            pc->args[2].flags = 1;
            func_0048B5D0(&p, flag);
            break;
        case 0x5AB: // sne with registers
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
        case 0x59B: // seq with registers
            pc->op = 0xE4;
            goto seq_tail;
        case 0x59C: // seq with an immediate
            pc->op = 0x5AF;
            func_0048B5D0(ppc, flag);
seq_tail:
            insertpcodeafter(pc, makepcode(0xD6, pc->args[0].reg, pc->args[0].reg, 1));
            break;
        case 0x55D: // la of a label
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
        case 0x55C: // la
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
                       ((UInt16)pc->args[2].reg <= 0x20 && pc->args[2].reg != 0)) {
                // base register + offset
                p = func_00480420(0x519, 3);
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
                CError_Internal(MACRO_FILE, 903);
            }
            break;
        case 0x55E: // li
            hi = pc->args[1].value;
            lo = pc->args[1].value2;
            reg = pc->args[0].reg;
            if (copts.mips_isa >= 3 &&
                ((hi != 0 && hi != -1) ||
                 (hi == -1 && !(lo & 0x80000000)) ||
                 (hi == 0 && (lo & 0x80000000)))) {
                // a 64-bit constant, 16 bits at a time
                if (pc->unk38) {
                    CError_Internal(MACRO_FILE, 914);
                }
                if (flag) {
                    CError_Internal(MACRO_FILE, 607);
                }
                insertpcodebefore(pc, makepcode(0xBF, reg, 0, (UInt16)(hi >> 16)));
                insertpcodebefore(pc, makepcode(0x84, pc->args[0].reg, pc->args[0].reg, 16));
                if ((UInt16)hi) {
                    insertpcodebefore(
                        pc, makepcode(0xBF, pc->args[0].reg, pc->args[0].reg, (UInt16)hi)
                    );
                }
                insertpcodebefore(pc, makepcode(0x84, pc->args[0].reg, pc->args[0].reg, 16));
                lo = pc->args[1].value2;
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
                    CError_Internal(MACRO_FILE, 607);
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
                p = func_00480420(3, 3);
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
                    CError_Internal(MACRO_FILE, 607);
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
        case 0x549:
        case 0x54B:
        case 0x54D:
        case 0x54E:
        case 0x550:
        case 0x552:
        case 0x558:
        case 0x559:
        case 0x577:
        case 0x578:
        case 0x579:
        case 0x57A:
        case 0x57B:
        case 0x57D:
        case 0x57F:
        case 0x582:
        case 0x583:
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
        case 0x55A:
        case 0x55B:
        case 0x5AD:
        case 0x5AE: // sub immediate: add the negation
            pc->op = REAL(op);
            IMM(pc->args[2]) = CInt64_Neg(IMM(pc->args[2]));
            func_0048B5D0(ppc, flag);
            break;
        case 0x560:
        case 0x584: // l.d, s.d
            if (copts.mips_isa >= 2 && (copts.fp_library & 2)) {
                pc->op = REAL(op);
                func_0048B5D0(ppc, flag);
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
            func_0048B5D0(ppc, flag);
            break;
        case 0x5A6:
        case 0x5A8: // sle with an immediate: slt with imm + 1
            pc->op = REAL(op);
            IMM(pc->args[2]) = CInt64_Add(IMM(pc->args[2]), cint64_one);
            func_0048B5D0(ppc, flag);
            break;
        case 0x518:
        case 0x519:
        case 0x547:
        case 0x548: // add immediate
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
        case 0x5A9:
        case 0x5AA: // slt with an immediate
            real = REAL(op);
            alt = ALT(op);
            if (CInt64_IsInRange(IMM(pc->args[2]), 2) && !pc->unk38) {
                pc->op = real;
                break;
            }
li_at_arg2:
            // li $at, imm; then the register form
            p = func_00480420(0x55E, 2);
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
        case 0x56D:
        case 0x58F: // lw, sw
            if (pc->args[1].value % 4 == 0) {
                goto memory;
            }
            CError_Internal(MACRO_FILE, 1203);
            if (pc->op == 0x56D) {
                if (copts.littleendian) {
                    split_unaligned2(ppc, flag, 0x573, 0x572, 3);
                } else {
                    split_unaligned2(ppc, flag, 0x572, 0x573, 3);
                }
            } else if (copts.littleendian) {
                split_unaligned2(ppc, flag, 0x596, 0x595, 3);
            } else {
                split_unaligned2(ppc, flag, 0x595, 0x596, 3);
            }
            break;
        case 0x562:
        case 0x587: // ld, sd
            if (pc->args[1].value % 8 == 0) {
                goto memory;
            }
            CError_Internal(MACRO_FILE, 1244);
            if (pc->op == 0x562) {
                if (copts.littleendian) {
                    split_unaligned2(ppc, flag, 0x567, 0x566, 7);
                } else {
                    split_unaligned2(ppc, flag, 0x566, 0x567, 7);
                }
            } else if (copts.littleendian) {
                split_unaligned2(ppc, flag, 0x58C, 0x58B, 7);
            } else {
                split_unaligned2(ppc, flag, 0x58B, 0x58C, 7);
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
        case 0x5BA: // lvi.s
            if (CInt64_IsZero((CInt64*)&pc->args[1].value)) {
                pc->op = 0x40F;
                pc->args[1] = pc->args[0];
                pc->args[0] = func_004C3E70(0, 0, 2);
                pc->args[0].flags = 1;
                break;
            }
            p = func_00480420(0x55E, 2);
            p->args[0] = AT_DEF();
            p->args[1] = pc->args[1];
            insertpcodebefore(pc, p);
            pc->op = 0x40F;
            pc->args[1] = pc->args[0];
            pc->args[0] = p->args[0];
            pc->args[0].flags = 1;
            func_0048B5D0(&p, flag);
            break;
        case 0x51A:
        case 0x581:
        case 0x5AF: // and/or/xor with an immediate
            if (CInt64_IsInURange(IMM(pc->args[2]), 2)) {
                pc->op = REAL(op);
                break;
            }
            p = func_00480420(0x55E, 2);
            p->args[0] = AT_DEF();
            p->args[1] = pc->args[2];
            insertpcodebefore(pc, p);
            pc->op = ALT(op);
            pc->args[2] = p->args[0];
            pc->args[2].flags = 1;
            func_0048B5D0(&p, flag);
            break;
        case 0x5A2:
        case 0x5A4: // sgt with an immediate
            real = REAL(op);
            p = func_00480420(0x55E, 2);
            p->args[0] = AT_DEF();
            p->args[1] = pc->args[2];
            insertpcodebefore(pc, p);
            pc->op = real;
            pc->args[2] = pc->args[1];
            pc->args[1] = AT_USE();
            func_0048B5D0(&p, flag);
            break;
        case 0x59D:
        case 0x59F:
        case 0x5A1:
        case 0x5A3: // sge/sgt with registers: swap
            real = REAL(op);
            pc->op = real;
            tmp = pc->args[1];
            pc->args[1] = pc->args[2];
            pc->args[2] = tmp;
            if (real >= 0x518) {
                func_0048B5D0(ppc, flag);
            }
            break;
        case 0x525:
        case 0x526:
        case 0x529:
        case 0x52A:
        case 0x52D:
        case 0x52E:
        case 0x531:
        case 0x532:
            // bgt/ble with registers: swap
            pc->op = REAL(op);
            tmp = pc->args[0];
            pc->args[0] = pc->args[1];
            pc->args[1] = tmp;
            func_0048B5D0(ppc, flag);
            break;
        case 0x5A5:
        case 0x5A7: // sle with registers: swap, then xori 1
            real = REAL(op);
            pc->op = real;
            tmp = pc->args[1];
            pc->args[1] = pc->args[2];
            pc->args[2] = tmp;
            insertpcodeafter(pc, makepcode(0xE5, pc->args[0].reg, pc->args[0].reg, 1));
            if (real >= 0x518) {
                func_0048B5D0(ppc, flag);
            }
            break;
        case 0x59E:
        case 0x5A0: // sge with an immediate: slt, then xori 1
            real = REAL(op);
            pc->op = real;
            insertpcodeafter(pc, makepcode(0xE5, pc->args[0].reg, pc->args[0].reg, 1));
            if (real >= 0x518) {
                func_0048B5D0(ppc, flag);
            }
            break;
        case 0x51D:
        case 0x51E:
        case 0x521:
        case 0x522:
        case 0x535:
        case 0x536:
        case 0x539:
        case 0x53A:
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
        case 0x530: // blel
            if (CInt64_IsZero((CInt64*)&pc->args[1].value)) {
                pc->op = 0x23;
                goto compare_zero;
            }
            goto compare_plus_one;
        case 0x52F: // ble
            if (CInt64_IsZero((CInt64*)&pc->args[1].value)) {
                pc->op = 0x22;
                goto compare_zero;
            }
            goto compare_plus_one;
        case 0x527:
        case 0x528:
        case 0x52B:
        case 0x52C:
        case 0x533:
        case 0x534:
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
        case 0x51F: // bge
            if (CInt64_IsZero((CInt64*)&pc->args[1].value)) {
                pc->op = 0x1C;
                goto compare_zero;
            }
            goto compare_imm;
        case 0x520: // bgel
            if (CInt64_IsZero((CInt64*)&pc->args[1].value)) {
                pc->op = 0x1D;
                goto compare_zero;
            }
            goto compare_imm;
        case 0x537: // blt
            if (CInt64_IsZero((CInt64*)&pc->args[1].value)) {
                pc->op = 0x24;
                goto compare_zero;
            }
            goto compare_imm;
        case 0x538: // bltl
            if (CInt64_IsZero((CInt64*)&pc->args[1].value)) {
                pc->op = 0x25;
                goto compare_zero;
            }
        case 0x523:
        case 0x524:
        case 0x53B:
        case 0x53C:
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
            if (real >= 0x518) {
                func_0048B5D0(&p, flag);
            }
            break;
compare_zero:
            pc->args[1] = pc->args[2];
            pc->argcount--;
            break;
        case 0x54A:
        case 0x54C:
        case 0x54F:
        case 0x551:
        case 0x553:
        case 0x555:
        case 0x557:
        case 0x57C:
        case 0x57E:
        case 0x580:
            // multiply/divide by an immediate
            if (CInt64_NotEqual(*(CInt64*)&pc->args[1].value, cint64_zero)) {
                p = func_00480420(0x55E, 2);
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
        case 0x51B:
        case 0x51C:
        case 0x53D:
        case 0x53E: // beq/bne with an immediate
            alt = ALT(op);
            if (CInt64_NotEqual(*(CInt64*)&pc->args[1].value, cint64_zero)) {
                p = func_00480420(0x55E, 2);
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
        case 0x5B1: // fcopy
            pc->op = 0xF9;
            break;
        case 0x5B2: // dcopy
            if (copts.fp_library & 2) {
                pc->op = 0xF8;
                break;
            }
            insertpcodebefore(pc, makepcode(0xF9, pc->args[0].reg + 1, pc->args[1].reg + 1));
            pc->op = 0xF9;
            break;
        case 0x53F:
        case 0x540:
        case 0x541:
        case 0x542:
        case 0x543:
        case 0x544: // traps
            if (CInt64_IsInRange(*(CInt64*)&pc->args[1].value, 2) && !pc->unk38) {
                pc->op = REAL(op);
                break;
            }
            p = func_00480420(0x55E, 2);
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
        case 0x554:
        case 0x556:
        case 0x55F:
        default:
            break;
    }
}
