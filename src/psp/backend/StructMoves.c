// mwccpsp 3.0.1 StructMoves.c (0x52DE30..0x52FD90): block copies for
// structure assignment, argument passing and memcpy. Small blocks are
// copied by straight-line loads and stores, bigger ones by a loop of the
// widest access the alignment allows plus the remainder.
#include "compiler/common.h"
#include "compiler/CParser.h"
#include "compiler/CompilerTools.h"
#include "compiler/CError.h"
#include "compiler/CFunc.h"
#include "compiler/CInt64.h"
#include "compiler/CMachine.h"
#include "psp/BE.h"
#include "psp/ObjGen.h"

extern void func_004A64C0(VOperand* op, Type* type, short copy); // into a register

// the operand into a register (if it is not one)
#define ENSURE_REG(op) \
    do { \
        if ((op)->kind) { \
            func_004A64C0((op), TYPE(&void_ptr), 0); \
        } \
    } while (0)
// a register operand made an indirection through it
#define MAKE_INDIRECT(op) \
    do { \
        if ((op)->kind) { \
            CError_FATAL(55); \
        } else { \
            (op)->kind = 2; \
            (op)->object = 0; \
            (op)->unk18 = 0; \
            (op)->offset = 0; \
            (op)->imm = cint64_zero; \
        } \
    } while (0)

extern void func_00479710(UInt16 op, VOperand* reg, VOperand* mem); // store a register
extern VOperand func_00479870(UInt16 op, VOperand* mem, char rclass); // load into a new register
extern MOperand func_004C3E70(UInt16 reg, UInt8 rclass, UInt16 flags); // a register operand
extern MOperand func_004C3C60(VOperand* v); // a memory operand
extern VOperand func_0047F660(SInt32 value); // a constant in a register
extern void func_00479540(UInt16 op, VOperand* src, CLabel* label); // compare with zero and branch
extern void func_0046FA90(MPCodeLabel* label); // place a label
extern void func_00490370(void); // end the block
extern MPCode* func_00480420(int op, SInt16 nargs);
extern SInt32 data_00625A98[7]; // next virtual register of each class
extern SInt16 data_00625E48; // the move opcode
static void func_0052E3B0(
    UInt16 loadop,
    UInt16 storeop,
    SInt32 step,
    VOperand* dstEnd,
    VOperand* srcEnd,
    VOperand* dst,
    VOperand* src,
    SInt32 len
);
// an address operand into a register
#define COERCE_ADDRESS(op) \
    do { \
        if ((op)->kind == 2) { \
            ENSURE_REG(op); \
        } else if ((op)->kind == 4) { \
            ENSURE_REG(op); \
            MAKE_INDIRECT(op); \
        } else if ((op)->kind == 3) { \
            ENSURE_REG(op); \
            MAKE_INDIRECT(op); \
        } \
    } while (0)

static void func_0052FA10(VOperand* dst, VOperand* src, SInt32 len);
static void func_0052FAC0(VOperand* op);
static void func_0052EC70(VOperand* dst, VOperand* src, SInt32 len);
#if MWCCPS2
static void ps2_move8_53E8F0(VOperand* dst, VOperand* src, SInt32 len);
#endif
static void func_0052F250(VOperand* dst, VOperand* src, SInt32 len);
static void func_0052F600(VOperand* dst, VOperand* src, SInt32 len);
static void func_0052F860(VOperand* dst, VOperand* src, SInt32 len);

// move_block: copies len bytes from src to dst, both aligned to align
void func_0052DE30(VOperand* dst, VOperand* src, SInt32 len, SInt32 align) {
    VOperand s;
    VOperand d;
    int factor;
    UInt16 op;

    s = *src;
    d = *dst;
    factor = copts.optimizesize ? 1 : 3;

    if (src->kind == 1) {
        // a floating-point register to memory
        if (len != 4) {
            op = (copts.mips_isa >= 2 && (copts.fp_library & 2)) ? MACRO_OP(0x70) : MACRO_OP(0x6C);
        } else {
            op = MACRO_OP(0x79);
        }
        func_00479710(op, src, dst);
        return;
    }

#if MWCCPS2
    // b210 tests copts+0x41 for the quadword path (the header's
    // function_align16 placement is unverified) and has a doubleword path
    if (copts.mips_option41 && align >= 16) {
#else
    if (copts.function_align16 && align >= 16) {
#endif
        if (len <= (char)copts.mips_option33 * factor * 8) {
            func_0052EC70(&d, &s, len);
        } else {
            VOperand d2 = d;
            VOperand s2 = s;
            func_0052E3B0(MACRO_OP(0x5D), MACRO_OP(0x7F), 16, &d2, &s2, &d, &s, len);
            func_0052DE30(&d2, &s2, len & 15, 16);
        }
#if MWCCPS2
    } else if (copts.mips_isa >= 3 && align >= 8) {
        if (len <= (char)copts.mips_option33 * factor * 8) {
            ps2_move8_53E8F0(&d, &s, len);
        } else {
            VOperand d2 = d;
            VOperand s2 = s;
            func_0052E3B0(MACRO_OP(0x4A), MACRO_OP(0x6F), 8, &d2, &s2, &d, &s, len);
            func_0052DE30(&d2, &s2, len & 7, 8);
        }
#endif
    } else if (align >= 4) {
        if (len <= (char)copts.mips_option33 * factor * 4) {
            func_0052F250(&d, &s, len);
        } else {
            VOperand d2 = d;
            VOperand s2 = s;
            func_0052E3B0(MACRO_OP(0x55), MACRO_OP(0x77), 4, &d2, &s2, &d, &s, len);
            func_0052FA10(&d2, &s2, len & 3);
        }
    } else if (align == 2) {
        if (len <= (char)copts.mips_option33 * 2) {
            func_0052F600(&d, &s, len);
        } else {
            VOperand d2 = d;
            VOperand s2 = s;
            func_0052E3B0(MACRO_OP(0x51), MACRO_OP(0x76), 2, &d2, &s2, &d, &s, len);
            func_0052FA10(&d2, &s2, len & 1);
        }
    } else {
        if (len <= (char)copts.mips_option33) {
            func_0052F860(&d, &s, len);
        } else {
            VOperand d2 = d;
            VOperand s2 = s;
            func_0052E3B0(MACRO_OP(0x50), MACRO_OP(0x75), 1, &d2, &s2, &d, &s, len);
        }
    }
}

// the 0..3 bytes left after a word loop (or any length, a word at most)
static void func_0052FA10(VOperand* dst, VOperand* src, SInt32 len) {
    VOperand tmp;
    UInt16 loadop;
    UInt16 storeop;

    if (len == 0) {
        return;
    } else if (len == 1) {
        loadop = MACRO_OP(0x49);
        storeop = MACRO_OP(0x75);
    } else if (len == 2) {
        loadop = MACRO_OP(0x51);
        storeop = MACRO_OP(0x76);
    } else if (len == 3) {
        func_0052FA10(dst, src, 2);
        dst->offset += 2;
        src->offset += 2;
        func_0052FA10(dst, src, 1);
        return;
    } else {
        loadop = MACRO_OP(0x55);
        storeop = MACRO_OP(0x77);
    }
    tmp = func_00479870(loadop, src, 0);
    func_00479710(storeop, &tmp, dst);
}

// the copy loop: len / step iterations (two accesses each when that is even
// and at least 6) of loadop/storeop through pointer registers, which
// dstEnd/srcEnd describe afterwards (for the remainder)
static void func_0052E3B0(
    UInt16 loadop,
    UInt16 storeop,
    SInt32 step,
    VOperand* dstEnd,
    VOperand* srcEnd,
    VOperand* dst,
    VOperand* src,
    SInt32 len
) {
    SInt32 rtmp;
    SInt32 rtmp2;
    SInt32 rcount;
    CLabel* label;
    SInt32 rdst;
    SInt32 rsrc;
    Boolean unroll;
    SInt32 count;
    VOperand counter;
    VOperand smem;
    VOperand dmem;
    MPCode* pc;

    label = galloc(sizeof(CLabel));
    rsrc = data_00625A98[0]++;
    rdst = data_00625A98[0]++;
    rcount = data_00625A98[0]++;
    rtmp = data_00625A98[0]++;
    label->pclabel = (struct PCodeLabel*)makepclabel();
    smem.kind = 2;
    smem.offset = 0;
    smem.object = 0;
    dmem.kind = 2;
    dmem.offset = 0;
    dmem.object = 0;
    srcEnd->reg = rsrc;
    srcEnd->offset = 0;
    srcEnd->object = 0;
    srcEnd->unk18 = 0;
    dstEnd->reg = rdst;
    dstEnd->offset = 0;
    dstEnd->object = 0;
    dstEnd->unk18 = 0;
    COERCE_ADDRESS(src);
    COERCE_ADDRESS(dst);

    count = len / step;
    unroll = count >= 6 && !(count & 1);
    if (unroll) {
        count /= 2;
        rtmp2 = data_00625A98[0]++;
    }
    counter = func_0047F660(count);
    emitpcode(data_00625E48, rcount, counter.reg);
    counter.reg = rcount;
    emitpcode(data_00625E48, rsrc, src->reg);
    emitpcode(data_00625E48, rdst, dst->reg);
    func_00490370();
    func_0046FA90((MPCodeLabel*)label->pclabel);

    pc = func_00480420(loadop, 3);
    pc->flags |= 0x20;
    pc->args[0] = func_004C3E70(rtmp, 0, 0x12);
    pc->args[1] = func_004C3C60(&smem);
    pc->args[2] = func_004C3E70(rsrc, 0, 0x11);
    if (pc->op == MACRO_OP(0x5D) || pc->op == 0x1D1) {
        pc->args[0].flags |= 0x2000;
    }
    appendpcode(data_006246B4, pc);
    if (unroll) {
        pc = func_00480420(loadop, 3);
        pc->flags |= 0x20;
        pc->args[0] = func_004C3E70(rtmp2, 0, 0x12);
        if (pc->op == MACRO_OP(0x5D) || pc->op == 0x1D1) {
            pc->args[0].flags |= 0x2000;
        }
        smem.offset += step;
        pc->args[1] = func_004C3C60(&smem);
        pc->args[2] = func_004C3E70(rsrc, 0, 0x11);
        appendpcode(data_006246B4, pc);
        pc = makepcode(3, srcEnd->reg, srcEnd->reg, step * 2);
    } else {
        pc = makepcode(3, srcEnd->reg, srcEnd->reg, step);
    }
#if MWCCPS2 // b210 marks the destination and source registers
    pc->args[0].flags |= 0x10;
    pc->args[1].flags |= 0x10;
#else
    pc->args[1].flags |= 0x10;
    pc->args[2].flags |= 0x10;
#endif
    appendpcode(data_006246B4, pc);
    pc = makepcode(3, counter.reg, counter.reg, -1);
#if MWCCPS2 // b210 marks the destination and source registers
    pc->args[0].flags |= 0x10;
    pc->args[1].flags |= 0x10;
#else
    pc->args[1].flags |= 0x10;
    pc->args[2].flags |= 0x10;
#endif
    appendpcode(data_006246B4, pc);

    pc = func_00480420(storeop, 3);
    pc->flags |= 0x20;
    pc->args[0] = func_004C3E70(rtmp, 0, 0x11);
    if (pc->op == MACRO_OP(0x7F) || pc->op == 0x1D2) {
        pc->args[0].flags |= 0x2000;
    }
    pc->args[1] = func_004C3C60(&dmem);
    pc->args[2] = func_004C3E70(rdst, 0, 0x11);
    appendpcode(data_006246B4, pc);
    if (unroll) {
        pc = func_00480420(storeop, 3);
        pc->flags |= 0x20;
        pc->args[0] = func_004C3E70(rtmp2, 0, 0x11);
        if (pc->op == MACRO_OP(0x5D) || pc->op == 0x1D1) {
            pc->args[0].flags |= 0x2000;
        }
        dmem.offset += step;
        pc->args[1] = func_004C3C60(&dmem);
        pc->args[2] = func_004C3E70(rdst, 0, 0x11);
        appendpcode(data_006246B4, pc);
        pc = makepcode(3, dstEnd->reg, dstEnd->reg, step * 2);
    } else {
        pc = makepcode(3, dstEnd->reg, dstEnd->reg, step);
    }
#if MWCCPS2 // b210 marks the destination and source registers
    pc->args[0].flags |= 0x10;
    pc->args[1].flags |= 0x10;
#else
    pc->args[1].flags |= 0x10;
    pc->args[2].flags |= 0x10;
#endif
    appendpcode(data_006246B4, pc);
    func_00479540(0x20, &counter, label);
}

// straight-line copy of 16-byte aligned blocks (quadwords, then doubles,
// words, halfwords and bytes), mips_option33 registers at a time
static void func_0052EC70(VOperand* dst, VOperand* src, SInt32 len) {
    VOperand regs[6];
    VOperand s;
    VOperand d;
    int i;
    SInt32 n;

    s = *src;
    d = *dst;
    func_0052FAC0(&s);
    func_0052FAC0(&d);
    while (len > 0) {
        n = len;
        for (i = 0; i < (char)copts.mips_option33; i++) {
            if (n >= 16) {
                regs[i] = func_00479870(MACRO_OP(0x5D), &s, 0);
                s.offset += 16;
                n -= 16;
            } else if (n >= 8) {
                if (!copts.nofpregforblkmv && (copts.fp_library & 2)) {
                    regs[i] = func_00479870(MACRO_OP(0x4B), &s, 1);
                } else {
                    regs[i] = func_00479870(MACRO_OP(0x4A), &s, 0);
                }
                s.offset += 8;
                n -= 8;
            } else if (n >= 4) {
                if (!copts.nofpregforblkmv && (copts.fp_library & 1)) {
                    regs[i] = func_00479870(MACRO_OP(0x56), &s, 1);
                } else {
                    regs[i] = func_00479870(MACRO_OP(0x55), &s, 0);
                }
                s.offset += 4;
                n -= 4;
            } else if (n >= 2) {
                regs[i] = func_00479870(MACRO_OP(0x51), &s, 0);
                s.offset += 2;
                n -= 2;
            } else if (n > 0) {
                regs[i] = func_00479870(MACRO_OP(0x49), &s, 0);
                s.offset++;
                n--;
            }
        }
        n = len;
        for (i = 0; i < (char)copts.mips_option33; i++) {
            if (n >= 16) {
                func_00479710(MACRO_OP(0x7F), &regs[i], &d);
                d.offset += 16;
                n -= 16;
                len -= 16;
            } else if (n >= 8) {
                if (!copts.nofpregforblkmv && (copts.fp_library & 2)) {
                    func_00479710(MACRO_OP(0x70), &regs[i], &d);
                } else {
                    func_00479710(MACRO_OP(0x6F), &regs[i], &d);
                }
                d.offset += 8;
                n -= 8;
                len -= 8;
            } else if (n >= 4) {
                if (!copts.nofpregforblkmv && (copts.fp_library & 1)) {
                    func_00479710(MACRO_OP(0x79), &regs[i], &d);
                } else {
                    func_00479710(MACRO_OP(0x77), &regs[i], &d);
                }
                d.offset += 4;
                n -= 4;
                len -= 4;
            } else if (n >= 2) {
                func_00479710(MACRO_OP(0x76), &regs[i], &d);
                d.offset += 2;
                n -= 2;
                len -= 2;
            } else if (n > 0) {
                func_00479710(MACRO_OP(0x75), &regs[i], &d);
                d.offset++;
                n--;
                len--;
            }
        }
    }
}

#if MWCCPS2
// 0x53E8F0: a doubleword-aligned block move (ld/sd and smaller)
static void ps2_move8_53E8F0(VOperand* dst, VOperand* src, SInt32 len) {
    VOperand regs[6];
    VOperand s;
    VOperand d;
    int i;
    SInt32 n;

    s = *src;
    d = *dst;
    func_0052FAC0(&s);
    func_0052FAC0(&d);
    while (len > 0) {
        n = len;
        for (i = 0; i < (char)copts.mips_option33; i++) {
            if (n >= 8) {
                if (!copts.nofpregforblkmv && (copts.fp_library & 2)) {
                    regs[i] = func_00479870(MACRO_OP(0x4B), &s, 1);
                } else {
                    regs[i] = func_00479870(MACRO_OP(0x4A), &s, 0);
                }
                s.offset += 8;
                n -= 8;
            } else if (n >= 4) {
                if (!copts.nofpregforblkmv && (copts.fp_library & 1)) {
                    regs[i] = func_00479870(MACRO_OP(0x56), &s, 1);
                } else {
                    regs[i] = func_00479870(MACRO_OP(0x55), &s, 0);
                }
                s.offset += 4;
                n -= 4;
            } else if (n >= 2) {
                regs[i] = func_00479870(MACRO_OP(0x51), &s, 0);
                s.offset += 2;
                n -= 2;
            } else if (n > 0) {
                regs[i] = func_00479870(MACRO_OP(0x49), &s, 0);
                s.offset++;
                n--;
            }
        }
        n = len;
        for (i = 0; i < (char)copts.mips_option33; i++) {
            if (n >= 8) {
                if (!copts.nofpregforblkmv && (copts.fp_library & 2)) {
                    func_00479710(MACRO_OP(0x70), &regs[i], &d);
                } else {
                    func_00479710(MACRO_OP(0x6F), &regs[i], &d);
                }
                d.offset += 8;
                n -= 8;
                len -= 8;
            } else if (n >= 4) {
                if (!copts.nofpregforblkmv && (copts.fp_library & 1)) {
                    func_00479710(MACRO_OP(0x79), &regs[i], &d);
                } else {
                    func_00479710(MACRO_OP(0x77), &regs[i], &d);
                }
                d.offset += 4;
                n -= 4;
                len -= 4;
            } else if (n >= 2) {
                func_00479710(MACRO_OP(0x76), &regs[i], &d);
                d.offset += 2;
                n -= 2;
                len -= 2;
            } else if (n > 0) {
                func_00479710(MACRO_OP(0x75), &regs[i], &d);
                d.offset++;
                n--;
                len--;
            }
        }
    }
}
#endif

// straight-line word copy, mips_option33 registers at a time (words through
// FP registers when the FPU option allows)
static void func_0052F250(VOperand* dst, VOperand* src, SInt32 len) {
    VOperand regs[6];
    VOperand s;
    VOperand d;
    int i;
    SInt32 n;

    s = *src;
    d = *dst;
    func_0052FAC0(&s);
    func_0052FAC0(&d);
    while (len > 0) {
        n = len;
        for (i = 0; i < (char)copts.mips_option33; i++) {
            if (n >= 4) {
                if (!copts.nofpregforblkmv && (copts.fp_library & 1)) {
                    regs[i] = func_00479870(MACRO_OP(0x56), &s, 1);
                } else {
                    regs[i] = func_00479870(MACRO_OP(0x55), &s, 0);
                }
                s.offset += 4;
                n -= 4;
            } else if (n >= 2) {
                regs[i] = func_00479870(MACRO_OP(0x51), &s, 0);
                s.offset += 2;
                n -= 2;
            } else if (n > 0) {
                regs[i] = func_00479870(MACRO_OP(0x49), &s, 0);
                s.offset++;
                n--;
            }
        }
        n = len;
        for (i = 0; i < (char)copts.mips_option33; i++) {
            if (n >= 4) {
                if (!copts.nofpregforblkmv && (copts.fp_library & 1)) {
                    func_00479710(MACRO_OP(0x79), &regs[i], &d);
                } else {
                    func_00479710(MACRO_OP(0x77), &regs[i], &d);
                }
                d.offset += 4;
                n -= 4;
                len -= 4;
            } else if (n >= 2) {
                func_00479710(MACRO_OP(0x76), &regs[i], &d);
                d.offset += 2;
                n -= 2;
                len -= 2;
            } else if (n > 0) {
                func_00479710(MACRO_OP(0x75), &regs[i], &d);
                d.offset++;
                n--;
                len--;
            }
        }
    }
}

// straight-line halfword copy (a last odd byte by itself)
static void func_0052F600(VOperand* dst, VOperand* src, SInt32 len) {
    VOperand regs[6];
    VOperand s;
    VOperand d;
    int i;
    SInt32 n;

    s = *src;
    d = *dst;
    n = len;
    func_0052FAC0(&s);
    func_0052FAC0(&d);
    for (i = 0; i < (char)copts.mips_option33; i++) {
        if (n >= 2) {
            regs[i] = func_00479870(MACRO_OP(0x51), &s, 0);
            s.offset += 2;
            n -= 2;
        } else if (n > 0) {
            regs[i] = func_00479870(MACRO_OP(0x49), &s, 0);
            s.offset++;
            n--;
        }
    }
    for (i = 0; i < (char)copts.mips_option33; i++) {
        if (len >= 2) {
            func_00479710(MACRO_OP(0x76), &regs[i], &d);
            d.offset += 2;
            len -= 2;
        } else if (len > 0) {
            func_00479710(MACRO_OP(0x75), &regs[i], &d);
            d.offset++;
            len--;
        }
    }
}

// straight-line byte copy: up to mips_option33 loads, then the stores
static void func_0052F860(VOperand* dst, VOperand* src, SInt32 len) {
    VOperand regs[6];
    VOperand s;
    VOperand d;
    int i;
    SInt32 n;

    s = *src;
    d = *dst;
    n = len;
    func_0052FAC0(&s);
    func_0052FAC0(&d);
    for (i = 0; i < (char)copts.mips_option33; i++) {
        if (n > 0) {
            regs[i] = func_00479870(MACRO_OP(0x49), &s, 0);
            s.offset++;
            n--;
        }
    }
    for (i = 0; i < (char)copts.mips_option33; i++) {
        if (len > 0) {
            func_00479710(MACRO_OP(0x75), &regs[i], &d);
            d.offset++;
            len--;
        }
    }
}

// makes the operand an indirection through a register ($gp-relative
// objects stay as they are)
static void func_0052FAC0(VOperand* op) {
    ObjAddress addr;

    if (op->kind == 0) {
        MAKE_INDIRECT(op);
    } else if (op->kind == 2) {
        if (op->object) {
            ObjGen_GetObjectAddress((Object*)op->object, &addr);
            if (addr.basereg == 28 && addr.near) {
                op->kind = 4;
                return;
            }
        }
        COERCE_ADDRESS(op);
        MAKE_INDIRECT(op);
    } else if (op->kind == 4) {
        ENSURE_REG(op);
        MAKE_INDIRECT(op);
    } else if (op->kind == 3) {
        ENSURE_REG(op);
        MAKE_INDIRECT(op);
    } else {
        CError_FATAL(114);
    }
}
