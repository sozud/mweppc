#include "compiler/common.h"
#include "compiler/CError.h"
#include "compiler/CParser.h"
#include "compiler/types.h"
#include "psp/BE.h"
#include "compiler/CInt64.h"
#include "compiler/Alias.h"
#include "compiler/objects.h"
#include "compiler/RegisterInfo.h"
#include "compiler/CFunc.h"

// mwccpsp 3.0.1 mips.c (named by its assertions): target-specific choices.

#define IS_SINGLE(t) \
    (TYPE_INTEGRAL(t)->integral == IT_FLOAT || ((t)->type == TYPEFLOAT && (t)->size == 4))
#define IS_DOUBLE(t) \
    (TYPE_INTEGRAL(t)->integral == IT_DOUBLE || ((t)->type == TYPEFLOAT && (t)->size == 8))
#define IN_FPR(t) \
    (((IS_SINGLE(t) && (copts.fp_library & 1))) || (IS_DOUBLE(t) && (copts.fp_library & 2)))

// The load opcode for a value of the type; *fpr is 1 when it goes to a
// floating-point register (5 for 128-bit values).
UInt16 func_0047FD20(Type* type, SInt32* fpr) {
    *fpr = 0;
    if (type->type == TYPEENUM) {
        switch (type->size) {
            case 1:
                return 0x568;
            case 2:
                return 0x569;
            case 4:
                return 0x56D;
            case 8:
                return 0x56D;
            default:
                CError_Internal("mips.c", 174);
                return 0;
        }
    }
    if (type->type == TYPESTRUCT || type->type == TYPECLASS || type->type == TYPEARRAY) {
        if (type->size == 1) {
            return 0x568;
        }
        if (type->size == 2) {
            return 0x569;
        }
        if (type->size <= 4) {
            return 0x56D;
        }
        if (type->size <= 8) {
            return 0x562;
        }
        CError_Internal("mips.c", 185);
        return 0;
    }
    if (type->type == TYPEMEMBERPOINTER || type->type == TYPEPOINTER) {
        return 0x56D;
    }
    switch (TYPE_INTEGRAL(type)->integral) {
        case IT_BOOL:
            return 0x561;
        case IT_CHAR:
            return copts.unsigned_char ? 0x561 : 0x568;
        case IT_SCHAR:
            return 0x568;
        case IT_UCHAR:
            return 0x561;
        case IT_WCHAR_T:
            return copts.ushort_wchar_t ? 0x56A : 0x56D;
        case IT_SHORT:
            return 0x569;
        case IT_USHORT:
            return 0x56A;
        case IT_INT:
        case IT_UINT:
        case IT_LONG:
        case IT_ULONG:
            if (type->size == 4) {
                return 0x56D;
            }
            CError_Internal("mips.c", 220);
            return 0;
        case IT_LONGLONG:
        case IT_ULONGLONG:
            return 0x56D;
        case IT_FLOAT:
            if (type->type == TYPEFLOAT && IN_FPR(type)) {
                *fpr = 1;
                return 0x56E;
            }
            return 0x56D;
        case IT_SHORTDOUBLE:
        case IT_DOUBLE:
        case IT_LONGDOUBLE:
            if (type->type == TYPEFLOAT && IN_FPR(type)) {
                *fpr = 1;
                return (copts.mips_isa >= 2 && (copts.fp_library & 2)) ? 0x563 : 0x560;
            }
            return 0x56D;
        case 17:
            *fpr = 5;
            return 0x5B3;
        default:
            CError_Internal("mips.c", 259);
            return 0;
    }
}

// The store opcode for a value of the type.
UInt16 func_0047FFD0(Type* type) {
    if (type->type == TYPEENUM) {
        switch (type->size) {
            case 1:
                return 0x58D;
            case 2:
                return 0x58E;
            case 4:
                return 0x58F;
            case 8:
                return 0x58F;
            default:
                CError_Internal("mips.c", 92);
                return 0;
        }
    }
    if (type->type == TYPESTRUCT || type->type == TYPECLASS) {
        switch (type->size) {
            case 1:
                return 0x58D;
            case 2:
                return 0x58E;
            case 4:
                return 0x58F;
            case 8:
                return 0x587;
            default:
                CError_Internal("mips.c", 103);
                return 0;
        }
    }
    if (type->type == TYPEMEMBERPOINTER || type->type == TYPEPOINTER) {
        return 0x58F;
    }
    switch (TYPE_INTEGRAL(type)->integral) {
        case IT_BOOL:
            return 0x58D;
        case IT_CHAR:
            return 0x58D;
        case IT_SCHAR:
            return 0x58D;
        case IT_UCHAR:
            return 0x58D;
        case IT_WCHAR_T:
            return copts.ushort_wchar_t ? 0x58E : 0x58F;
        case IT_SHORT:
            return 0x58E;
        case IT_USHORT:
            return 0x58E;
        case IT_INT:
        case IT_UINT:
        case IT_LONG:
        case IT_ULONG:
            if (type->size == 4) {
                return 0x58F;
            }
            CError_Internal("mips.c", 136);
            return 0x58F;
        case IT_LONGLONG:
            return 0x58F;
        case IT_ULONGLONG:
            return 0x58F;
        case IT_FLOAT:
            if (type->type == TYPEFLOAT && IN_FPR(type)) {
                return 0x591;
            }
            return 0x58F;
        case IT_SHORTDOUBLE:
        case IT_DOUBLE:
        case IT_LONGDOUBLE:
            if (type->type == TYPEFLOAT && IN_FPR(type)) {
                return (copts.mips_isa >= 2 && (copts.fp_library & 2)) ? 0x588 : 0x584;
            }
            return 0x58F;
        case 17:
            return 0x5B5;
        default:
            CError_Internal("mips.c", 160);
            return 0;
    }
}

// The opcode table (PCodeInfo), 0x30 bytes per opcode.
typedef struct OpcodeDef {
    char* name; // +0x00
    char* alias; // +0x04: another name for the opcode
    char* format; // +0x08: operand format string
    UInt8 nargs; // +0x0C
    UInt8 size; // +0x0D
    UInt8 pad0E[2];
    UInt32 unk10; // +0x10
    UInt8 pad14[4];
    UInt32 flags; // +0x18
    UInt32 flags2; // +0x1C: implicit operands, from the format
    char* intrinsic; // +0x20: runtime routine name, if the op can be one
    SInt32 unk24; // +0x24
    SInt32 unk28; // +0x28
    UInt16 unk2C; // +0x2C
} OpcodeDef;

extern OpcodeDef data_006115F8[];
extern char* data_0062554C[]; // macro opcodes (from 0x518): format
extern UInt16 data_00625CE4[]; // ... machine opcode
extern UInt16 data_00625B86[]; // ...
extern SInt32 data_005EFC18; // opcodes with an extra entry
extern UInt16 data_00610C90[];

// isspace() as 3.0.1's x86 MSL does it (through the thread's locale); the
// native build uses the host's.
#ifdef __MWERKS__
extern char* _GetThreadLocalData(int create);
#define MW_ISSPACE(c) \
    (((c) >= 0 && (c) < 0x100) \
         ? ((*(UInt16**)(*(char**)(_GetThreadLocalData(1) + 0x1BC) + 8))[(c)] & 8) \
         : 0)
#else
#include <ctype.h>
#define MW_ISSPACE(c) isspace(c)
#endif

// The special registers an operand format reads and writes. A format is a
// list of operands, each a letter with prefixes: '=' written, '+' read and
// written, '!' and '$' variants, '2' a register pair.
UInt32 func_00479040(char* format) {
    UInt32 implicit = 0;
    Boolean pair = 0;
    int access;

    while (*format) {
        access = 1;
        if (*format == '=') {
            access++;
            format++;
        }
        if (*format == '+') {
            access = 3;
            format++;
        }
        if (*format == '!') {
            access |= 0x8000;
            format++;
        }
        if (*format == '$') {
            access |= 0x2000;
            format++;
        }
        if (*format == '2') {
            format++;
            pair = 1;
            if (*format == '+') {
                format++;
            }
        }
        if (*format == '\'') {
            format++;
        }
        switch (*format) {
            case '0':
                if (access & 1) {
                    implicit |= 0x2000;
                }
                if (access & 2) {
                    implicit |= 0x1000;
                }
                break;
            case '1':
                if (access & 1) {
                    implicit |= 0x8000;
                }
                if (access & 2) {
                    implicit |= 0x4000;
                }
                break;
            case '3':
                if (access & 1) {
                    implicit |= 0x8000;
                }
                if (access & 2) {
                    implicit |= 0x4000;
                }
                break;
            case 'r':
                if (access & 2) {
                    implicit |= 0x800;
                }
                break;
            case 's':
                if (access & 1) {
                    implicit |= 2;
                }
                break;
            case 't':
                if (access & 1) {
                    implicit |= 1;
                }
                break;
            case 'H':
            case 'h':
                if (access & 1) {
                    implicit |= 0x10000;
                }
                if (access & 2) {
                    implicit |= 0x20000;
                }
                break;
            case 'R':
                if (!pair && (access & 1)) {
                    implicit |= 0x40;
                }
                break;
            case 'S':
                if (access & 1) {
                    implicit |= 0x100;
                }
                break;
            case 'N':
            case 'T':
                if (access & 1) {
                    implicit |= 0x10;
                }
                break;
            case 'E':
                if (access & 1) {
                    implicit |= 0xA000;
                }
                if (access & 2) {
                    implicit |= 0x5000;
                }
                break;
            case 'G':
                if (access & 1) {
                    implicit |= 0xA000;
                }
                if (access & 2) {
                    implicit |= 0x5000;
                }
                break;
            case '&':
            case '(':
            case ')':
            case ',':
            case '-':
            case ';':
            case '<':
            case '>':
            case '@':
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'F':
            case 'I':
            case 'J':
            case 'K':
            case 'L':
            case 'M':
            case 'O':
            case 'P':
            case 'Q':
            case 'U':
            case 'V':
            case 'W':
            case 'X':
            case 'Y':
            case 'Z':
            case '[':
            case ']':
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
            case 'i':
            case 'j':
            case 'k':
            case 'l':
            case 'o':
            case 'p':
            case 'q':
            case 'u':
            case 'v':
            case 'x':
            case 'z':
            case '|':
            case '}':
            case '9':
                break;
            default:
                CError_Internal("mips.c", 2347);
        }
        format++;
    }
    return implicit;
}

// The number of operands a VFPU format describes.
int func_00479220(char* format) {
    int n = 0;

    while (*format) {
        if (!MW_ISSPACE(*format)) {
            switch (*format) {
                case 'r':
                    n++;
                    format++;
                    break;
                case 's':
                    n++;
                    format++;
                    break;
                case 'd':
                case 'i':
                case 'l':
                case 'm':
                case 't':
                case 'w':
                    n++;
                    break;
                case 'p':
                    format++;
                    break;
                case '#':
                case '(':
                case ')':
                case ',':
                case ';':
                case '<':
                case '=':
                case '>':
                case '@':
                    break;
                default:
                    CError_Internal("mips.c", 2152);
            }
        }
        format++;
    }
    return n;
}

// The number of operands a format describes.
int func_004792B0(char* format) {
    int n = 0;

    while (*format) {
        switch (*format) {
            case '&':
            case '0':
            case '1':
            case '3':
            case '<':
            case '>':
            case '@':
            case 'B':
            case 'H':
            case 'P':
            case 'X':
            case 'Y':
            case 'Z':
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'h':
            case 'i':
            case 'j':
            case 'k':
            case 'l':
            case 'p':
            case 'q':
            case 'r':
            case 's':
            case 't':
            case 'u':
            case 'v':
            case 'x':
            case 'z':
                n++;
                break;
            case '9':
            case 'D':
            case 'E':
            case 'G':
            case 'J':
            case 'K':
            case 'L':
            case 'M':
            case 'N':
            case 'O':
            case 'R':
            case 'S':
            case 'T':
            case 'V':
            case 'W':
                n++;
                break;
            case 'U':
                n++;
                break;
            case 'A':
                n += 2;
                break;
            case 'o':
                n++;
                break;
            case 'C':
            case 'F':
            case 'I':
            case 'Q':
            case 'f':
                n++;
                break;
            case '!':
            case '$':
            case '\'':
            case '(':
            case ')':
            case '+':
            case ',':
            case '-':
            case '2':
            case ';':
            case '=':
            case '[':
            case ']':
            case '|':
            case '}':
                break;
            default:
                CError_Internal("mips.c", 2099);
        }
        format++;
    }
    return n;
}

// Define a macro opcode and the machine opcode it becomes.
void func_00478E30(int op, char* name, char* format, UInt16 real, UInt16 alt, UInt16 unk2C) {
    data_006115F8[op].name = name;
    data_006115F8[op].alias = 0;
    data_006115F8[op].format = format;
    data_006115F8[op].unk10 = 0;
    data_006115F8[op].flags2 = func_00479040(format);
    data_006115F8[op].intrinsic = 0;
    data_006115F8[op].unk24 = 0;
    data_006115F8[op].size = 0;
    data_006115F8[op].nargs = func_004792B0(format);
    data_006115F8[op].unk2C = unk2C;
    data_0062554C[op - 0x518] = format;
    data_00625CE4[op - 0x518] = real;
    data_00625B86[op - 0x518] = alt;
}

// Define a machine opcode. 0x37E..0x3E5 are the VFPU's.
void func_00478ED0(
    int op,
    char* name,
    char* format,
    UInt32 unk10,
    UInt32 flags,
    char* intrinsic,
    SInt32 unk28,
    UInt16 unk2C
) {
    UInt8 size;

    data_006115F8[op].name = name;
    data_006115F8[op].alias = 0;
    data_006115F8[op].format = format;
    data_006115F8[op].unk10 = unk10;
    data_006115F8[op].flags2 = (op >= 0x37E && op <= 0x3E5) ? 0 : func_00479040(format);
    data_006115F8[op].unk28 = unk28;
    data_006115F8[op].unk2C = unk2C;
    data_006115F8[op].flags = flags;
    if ((UInt16)op >= 0x37E && (UInt16)op <= 0x3E5) {
        if (data_006115F8[(UInt16)op].flags & 0x1000) {
            switch ((UInt16)op) {
                case 0x3CD:
                case 0x3D6:
                case 0x3D8:
                case 0x3DB:
                case 0x3DE:
                    size = 4;
                    break;
                default:
                    size = 2;
            }
        } else if (data_006115F8[(UInt16)op].unk10 & 0xFFFF0000) {
            size = 4;
        } else {
            size = 2;
        }
    } else {
        size = 4;
    }
    data_006115F8[op].size = size;
    data_006115F8[op].nargs =
        (op >= 0x37E && op <= 0x3E5) ? func_00479220(format) : func_004792B0(format);
    if (intrinsic) {
        data_006115F8[op].intrinsic = intrinsic;
        data_006115F8[op].unk24 = data_005EFC18;
        data_00610C90[data_005EFC18++] = op;
    }
}

extern SInt32 data_00625A98[7]; // next virtual register, per class
extern VOperand func_004C3BA0(
    SInt32 reg, UInt8 kind
); // VReg.c takes a short; callers here pass an int

// A new virtual register holding the constant (0, $zero, for zero).
SInt32 func_0047F680(SInt32 value) {
    SInt32 reg;
    SInt32 tmp;
    SInt32 lo;

    if (!value) {
        return 0;
    }
    reg = data_00625A98[0]++;
    if (value >= -0x8000 && value < 0x8000) {
        emitpcode(3, reg, 0, value);
    } else if (value >= 0 && value < 0x10000) {
        emitpcode(0xBF, reg, 0, value);
    } else if (!(lo = value & 0xFFFF)) {
        emitpcode(0x90, reg, (UInt16)(value >> 16));
    } else {
        emitpcode(0x90, tmp = data_00625A98[0]++, (UInt16)(value >> 16));
        emitpcode(0xBF, reg, tmp, lo);
    }
    return reg;
}

// The constant as an integer register operand.
VOperand func_0047F660(SInt32 value) {
    return func_004C3BA0(func_0047F680(value), 0);
}

extern SInt16 data_00625E48; // move
extern VOperand func_004C3B10(MOperand op);

// Move a value to the result register: $v0 (and $v1), or $f0.
void func_0047FC80(VOperand op, int opcode, int fpr, Boolean pair) {
    SInt32 dst;

    dst = fpr == 0 ? 2 : 0;
    if (pair) {
        if (copts.littleendian) {
            emitpcode(opcode, dst, op.reg);
            emitpcode(opcode, dst + 1, op.regHi);
        } else {
            emitpcode(opcode, dst + 1, op.reg);
            emitpcode(opcode, dst, op.regHi);
        }
    } else {
        emitpcode(opcode, dst, op.reg);
    }
}

// Copy the operand's register into a new virtual register.
VOperand func_0047F9A0(VOperand* src) {
    emitpcode(data_00625E48, data_00625A98[0]++, src->reg);
    return func_004C3B10(data_006246B4->last->args[0]);
}

// A constant in a register (a pair for long long).
VOperand func_0047F9F0(CInt64 value, Type* type) {
    VOperand v;

    if ((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8) {
        v = func_004C3BA0(func_0047F680(value.lo), 0);
        v.regHi = func_0047F680(value.hi);
        return v;
    }
    return func_004C3BA0(func_0047F680(value.lo), 0);
}

extern MPCode* func_00480420(int op, SInt16 nargs);
extern MOperand func_004C3DD0(char rclass); // a new virtual register (VReg.c: UInt8)
extern MOperand func_004C3C60(VOperand* v);
extern MOperand func_004C3D50(CInt64 value);

// dst = a op b in a new register; a constant b becomes the immediate.
VOperand func_0047F740(UInt16 op, VOperand* a, VOperand* b, char rclass) {
    MPCode* pc;
    CInt64 imm;

    if (b->kind == 3) {
        imm = b->imm;
        pc = func_00480420(op, 3);
        pc->args[0] = func_004C3DD0(0);
        pc->args[1] = func_004C3C60(a);
        pc->args[2] = func_004C3D50(imm);
        appendpcode(data_006246B4, pc);
        return func_004C3B10(pc->args[0]);
    } else {
        pc = func_00480420(op, 3);
        pc->args[0] = func_004C3DD0(rclass);
        pc->args[1] = func_004C3C60(a);
        pc->args[2] = func_004C3C60(b);
        appendpcode(data_006246B4, pc);
        return func_004C3B10(pc->args[0]);
    }
}

// After a call: the result ($v0/$v1 or $f0/$f1) in new virtual registers.
VOperand func_0047FAA0(SInt16 op, int fpr, Boolean pair) {
    MPCode* pc;
    SInt32 src;
    VOperand v;
    VOperand v2;

    src = fpr == 0 ? 2 : 0;
    pc = makepcode(op, fpr == 0 ? data_00625A98[0]++ : data_00625A98[1]++, src);
    appendpcode(data_006246B4, pc);
    v = func_004C3B10(pc->args[0]);
    if (pair) {
        pc = makepcode(op, fpr == 0 ? data_00625A98[0]++ : data_00625A98[1]++, src + 1);
        appendpcode(data_006246B4, pc);
        v2 = func_004C3B10(pc->args[0]);
        if (copts.littleendian) {
            v.regHi = v2.reg;
            if (!fpr) {
                v.kind = 0;
            }
            return v;
        } else {
            v2.regHi = v.reg;
            if (!fpr) {
                v2.kind = 0;
            }
            return v2;
        }
    }
    return v;
}

#define OBJECT_FRAMEOFFSET(obj) (*(SInt32*)&((Object*)(obj))->u.var.offset) // +0x30

// The address the operand describes, in a new register.
VOperand func_0047F2D0(VOperand* src) {
    MPCode* pc;
    VOperand tmp;
    VOperand result;
    Object* obj;

    if (!src->object && src->kind == 2 && src->offset <= 0x7FFF && src->offset >= -0x8000) {
        pc = makepcode(3, data_00625A98[0]++, src->reg, src->offset);
        appendpcode(data_006246B4, pc);
        return func_004C3B10(pc->args[0]);
    }
    tmp = func_004C3BA0(src->reg, 0);
    result = func_0047F740(0x55C, src, &tmp, 0);
    if ((obj = (Object*)src->object)) {
        if (obj->datatype == DLOCAL && Registers_GetVarInfo(obj)) {
            data_006246B4->last->alias = make_alias(
                (Object*)src->object, src->offset - OBJECT_FRAMEOFFSET((Object*)src->object), 1
            );
        } else {
            data_006246B4->last->alias = make_alias((Object*)src->object, src->offset, 1);
        }
    } else {
        data_006246B4->last->alias = NULL;
    }
    return result;
}

// src | mask in a new register.
VOperand func_0047E580(VOperand* src, UInt32 mask) {
    MPCode* pc;

    if (!mask) {
        return *src;
    }
    if (!(mask & 0xFFFF0000)) {
        pc = makepcode(0xBF, data_00625A98[0]++, src->reg, (UInt16)mask);
    } else {
#if MWCCPSP_NATIVE // 3.0.1 evaluates arguments right to left
        {
            SInt32 k = func_0047F680(mask);
            pc = makepcode(0xBE, data_00625A98[0]++, src->reg, k);
        }
#else
        pc = makepcode(0xBE, data_00625A98[0]++, src->reg, func_0047F680(mask));
#endif
    }
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

extern void func_004C5890(int max, SInt32 value); // warn: shift count too large

#define IS_TYPE_LL(t) (((t)->type == TYPEINT || (t)->type == TYPEENUM) && (t)->size == 8)
#define IS_TYPE_128(t) (((t)->type == TYPEINT || (t)->type == TYPEENUM) && (t)->size == 16)

// src << shift in a new register.
VOperand func_0047EBB0(VOperand* src, SInt32 shift, Type* type) {
    SInt32 dst;

    if (!src->reg || !shift) {
        return *src;
    }
    dst = data_00625A98[0]++;
    if (shift > 31) {
        func_004C5890(31, shift);
        shift = 31;
    }
    if (!type || IS_TYPE_LL(type) || !(IS_TYPE_LL(type) || IS_TYPE_128(type))) {
        if (shift <= 31) {
            emitpcode(0xD2, dst, src->reg, shift);
        }
    }
    return func_004C3BA0(dst, 0);
}

// src >> shift in a new register: logical for unsigned types.
VOperand func_0047EA50(VOperand* src, SInt32 shift, Type* type) {
    MPCode* pc;
    Boolean isunsigned;

    isunsigned = type && is_unsigned(type);
    if (!src->reg || !shift) {
        return *src;
    }
    if (shift > 31) {
        func_004C5890(31, shift);
        shift = 31;
    }
    if (!type || IS_TYPE_LL(type) || !(IS_TYPE_LL(type) || IS_TYPE_128(type))) {
        if (shift <= 31) {
            pc = makepcode(isunsigned ? 0xDA : 0xD8, data_00625A98[0]++, src->reg, shift);
        }
    }
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

VOperand func_0047EF00(VOperand* src, SInt32 value, Type* type);

// src + a constant: a wide long long constant goes through registers.
VOperand func_0047ECC0(VOperand* src, CInt64* value, Type* type) {
    VOperand v;

    if (IS_TYPE_LL(type) && !CInt64_IsInRange(*value, 4)) {
        v = func_0047F9F0(*value, type);
        return func_0047F740(0x79, src, &v, 0);
    }
    return func_0047EF00(src, value->lo, type);
}

extern MOperand func_004C3D90(SInt32 value);

// src + value in a new register.
VOperand func_0047EF00(VOperand* src, SInt32 value, Type* type) {
    MPCode* pc;
    VOperand tmp;

    if (!value) {
        return *src;
    } else if (type && IS_TYPE_LL(type)) {
        CError_FATAL(774);
    } else if (value >= -0x8000 && value < 0x8000) {
        pc = func_00480420(3, 3);
        pc->args[0] = func_004C3DD0(0);
        pc->args[1] = func_004C3C60(src);
        pc->args[2] = func_004C3D90(value);
        appendpcode(data_006246B4, pc);
        return func_004C3B10(pc->args[0]);
    } else if (value >= 0x8000 && value < 0xFFFF) {
        tmp = func_0047EF00(src, 0x7FFF, NULL);
        return func_0047EF00(&tmp, value - 0x7FFF, NULL);
    } else if (value < -0x8000 && value >= -0x10000) {
        tmp = func_0047EF00(src, -0x8000, NULL);
        return func_0047EF00(&tmp, value + 0x8000, NULL);
    } else {
        tmp = func_004C3BA0(func_0047F680(value), 0);
        return func_0047F740(4, src, &tmp, 0);
    }
}

// src ^ value in a new register (folded for a constant).
VOperand func_0047E3D0(VOperand* src, SInt32 value) {
    MPCode* pc;
    VOperand v;
    CInt64 c;

    if (src->kind == 3) {
        v = *src;
        CInt64_SetLong(&c, value);
        v.imm = CInt64_Xor(src->imm, c);
        return v;
    }
    if (!value) {
        return *src;
    }
    if (value == -1) {
        pc = makepcode(0xBC, data_00625A98[0]++, src->reg, 0);
    } else if (!(value & 0xFFFF0000)) {
        pc = makepcode(0xE5, data_00625A98[0]++, src->reg, (UInt16)value);
    } else {
#if MWCCPSP_NATIVE // 3.0.1 evaluates arguments right to left
        {
            SInt32 k = func_0047F680(value);
            pc = makepcode(0xE4, data_00625A98[0]++, src->reg, k);
        }
#else
        pc = makepcode(0xE4, data_00625A98[0]++, src->reg, func_0047F680(value));
#endif
    }
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

// The low width bits of src, with the Allegrex ext instruction.
inline VOperand extract_low(VOperand* src, SInt32 width) {
    MPCode* pc;

    pc = makepcode(0x59A, data_00625A98[0]++, src->reg, width);
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

inline VOperand clear_high(VOperand* src, SInt32 n) {
    SInt32 width;

    if (!n) {
        return *src;
    }
    width = 32 - n;
    if (width > 32) {
        CError_FATAL(1714);
    }
    return extract_low(src, width);
}

// src & mask in a new register.
VOperand func_0047E650(VOperand* src, UInt32 mask) {
    MPCode* pc;
    SInt32 n;

    if (!mask || !src->reg) {
        return func_004C3BA0(0, 0);
    }
    if (mask == 0xFF) {
        return extract_low(src, 8);
    }
    if (mask == 0xFFFF) {
        return extract_low(src, 16);
    }
    if (mask == 0xFFFFFFFF) {
        return *src;
    }
    if (!(mask & 0xFFFF0000)) {
        pc = makepcode(7, data_00625A98[0]++, src->reg, (UInt16)mask);
    } else {
        switch (mask) {
            case 0x7FFFFFFF:
                n = 1;
                break;
            case 0x3FFFFFFF:
                n = 2;
                break;
            case 0x1FFFFFFF:
                n = 3;
                break;
            case 0x0FFFFFFF:
                n = 4;
                break;
            case 0x07FFFFFF:
                n = 5;
                break;
            case 0x03FFFFFF:
                n = 6;
                break;
            case 0x01FFFFFF:
                n = 7;
                break;
            case 0x00FFFFFF:
                n = 8;
                break;
            case 0x007FFFFF:
                n = 9;
                break;
            case 0x003FFFFF:
                n = 10;
                break;
            case 0x001FFFFF:
                n = 11;
                break;
            case 0x000FFFFF:
                n = 12;
                break;
            case 0x0007FFFF:
                n = 13;
                break;
            case 0x0003FFFF:
                n = 14;
                break;
            case 0x0001FFFF:
                n = 15;
                break;
            default:
#if MWCCPSP_NATIVE // 3.0.1 evaluates arguments right to left
            {
                SInt32 k = func_0047F680(mask);
                pc = makepcode(6, data_00625A98[0]++, src->reg, k);
            }
#else
                pc = makepcode(6, data_00625A98[0]++, src->reg, func_0047F680(mask));
#endif
                goto emit;
        }
        return clear_high(src, n);
    }
emit:
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

// dst = op a in a new register of the class.
VOperand func_0047F900(UInt16 op, VOperand* a, char rclass) {
    MPCode* pc;

    pc = func_00480420(op, 2);
    pc->args[0] = func_004C3DD0(rclass);
    pc->args[1] = func_004C3C60(a);
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

// src < value (signed), as 0 or 1 in a new register.
VOperand func_0047D480(VOperand* src, SInt32 value) {
    MPCode* pc;
    VOperand tmp;

    if (value == (SInt32)0x80000000) {
        return func_004C3BA0(func_0047F680(0), 0);
    }
    if (value >= -0x8000 && value < 0x8000) {
        pc = func_00480420(0xD5, 3);
        pc->args[0] = func_004C3DD0(0);
        pc->args[1] = func_004C3C60(src);
        pc->args[2] = func_004C3D90(value);
        appendpcode(data_006246B4, pc);
        return func_004C3B10(pc->args[0]);
    }
    tmp = func_004C3BA0(func_0047F680(value), 0);
    return func_0047F740(0xD4, src, &tmp, 0);
}

// src < a constant: a wide long long constant goes through registers.
VOperand func_0047D240(VOperand* src, CInt64* value, Type* type) {
    VOperand v;

    if (IS_TYPE_LL(type) && !CInt64_IsInRange(*value, 4)) {
        v = func_0047F9F0(*value, type);
        return func_0047F740(0xD4, src, &v, 0);
    }
    return func_0047D480(src, value->lo);
}

// src < value (unsigned), as 0 or 1 in a new register.
VOperand func_0047CF50(VOperand* src, SInt32 value) {
    MPCode* pc;
    VOperand tmp;

    if (!value) {
        return func_004C3BA0(func_0047F680(0), 0);
    }
    if (value >= -0x8000 && value < 0x8000) {
        pc = func_00480420(0xD6, 3);
        pc->args[0] = func_004C3DD0(0);
        pc->args[1] = func_004C3C60(src);
        pc->args[2] = func_004C3D90(value);
        appendpcode(data_006246B4, pc);
        return func_004C3B10(pc->args[0]);
    }
    tmp = func_004C3BA0(func_0047F680(value), 0);
    return func_0047F740(0xD7, src, &tmp, 0);
}

// src == value, as 0 or 1 in a new register.
VOperand func_0047E210(VOperand* src, SInt32 value) {
    MPCode* pc;
    VOperand t;

    if (value) {
        t = func_0047E3D0(src, value);
        pc = func_00480420(0xD6, 3);
        pc->args[0] = func_004C3DD0(0);
        pc->args[1] = func_004C3C60(&t);
        pc->args[2] = func_004C3D90(1);
        appendpcode(data_006246B4, pc);
        return func_004C3B10(pc->args[0]);
    } else {
        pc = func_00480420(0xD6, 3);
        pc->args[0] = func_004C3DD0(0);
        pc->args[1] = func_004C3C60(src);
        pc->args[2] = func_004C3D90(1);
        appendpcode(data_006246B4, pc);
        return func_004C3B10(pc->args[0]);
    }
}

// src == a constant: a wide long long constant goes through registers.
VOperand func_0047DFC0(VOperand* src, CInt64* value, Type* type) {
    VOperand v;

    if (IS_TYPE_LL(type) && !CInt64_IsInRange(*value, 4)) {
        v = func_0047F9F0(*value, type);
        return func_0047F740(0x59B, src, &v, 0);
    }
    return func_0047E210(src, value->lo);
}

extern VOperand func_00479E40(
    UInt16 op, VOperand* src, CInt64 value
); // an operation with a constant

#ifdef __MWERKS__
#pragma dont_inline on
#endif
// src <= value (signed), as 0 or 1 in a new register.
VOperand func_0047D9A0(VOperand* src, SInt32 value) {
    if (value == 0x7FFFFFFF) {
        return func_004C3BA0(func_0047F680(1), 0);
    }
    return func_0047D480(src, value + 1);
}
#ifdef __MWERKS__
#pragma dont_inline reset
#endif

#ifdef __MWERKS__
#pragma dont_inline on
#endif
// 0x47D870: src <= value (unsigned), as 0 or 1 in a new register.
VOperand func_0047D870(VOperand* src, SInt32 value) {
    if (value == -1) {
        return func_004C3BA0(func_0047F680(1), 0);
    }
    return func_0047CF50(src, value + 1);
}
#ifdef __MWERKS__
#pragma dont_inline reset
#endif

// src <= a constant (signed): a wide long long constant compares as < value + 1.
VOperand func_0047D8B0(VOperand* src, CInt64* value, Type* type) {
    CInt64 next;

    if (IS_TYPE_LL(type) && !CInt64_IsInRange(*value, 4)) {
        if (CInt64_Equal(*value, cint64_max)) {
            return func_004C3BA0(func_0047F680(1), 0);
        }
        next = CInt64_Add(*value, cint64_one);
        return func_0047D240(src, &next, type);
    }
    return func_0047D9A0(src, value->lo);
}

// 0x47CC70: src >= value (unsigned), as 0 or 1 in a new register:
// !(src < value), and always 1 for 0
VOperand func_0047CC70(VOperand* src, SInt32 value) {
    VOperand lt;

    if (value == 0) {
        return func_004C3BA0(func_0047F680(1), 0);
    }
    lt = func_0047CF50(src, value);
    return func_0047E3D0(&lt, 1);
}

// 0x47C740: src > value (unsigned), as 0 or 1 in a new register: sltu from
// $zero for 0, never for -1, else src >= value + 1
#ifdef __MWERKS__
#pragma dont_inline on
#endif
VOperand func_0047C740(VOperand* src, SInt32 value) {
    MPCode* pc;
    VOperand zero;

    zero = func_004C3BA0(0, 0);
    if (value == 0) {
        pc = func_00480420(0xD7, 3);
        pc->args[0] = func_004C3DD0(0);
        pc->args[1] = func_004C3C60(&zero);
        if (src->kind == 3) {
            pc->args[2] = func_004C3D50(src->imm);
        } else {
            pc->args[2] = func_004C3C60(src);
        }
        appendpcode(data_006246B4, pc);
        return func_004C3B10(pc->args[0]);
    }
    if (value == -1) {
        return func_004C3BA0(func_0047F680(0), 0);
    }
    return func_0047CC70(src, value + 1);
}
#ifdef __MWERKS__
#pragma dont_inline reset
#endif

// 0x47C9D0: src > value (signed), as 0 or 1 in a new register: slt from
// $zero for 0, else !(src <= value)
VOperand func_0047C9D0(VOperand* src, SInt32 value) {
    MPCode* pc;
    VOperand zero;
    VOperand le;

    zero = func_004C3BA0(0, 0);
    if (value == 0) {
        pc = func_00480420(0xD4, 3);
        pc->args[0] = func_004C3DD0(0);
        pc->args[1] = func_004C3C60(&zero);
        if (src->kind == 3) {
            pc->args[2] = func_004C3D50(src->imm);
        } else {
            pc->args[2] = func_004C3C60(src);
        }
        appendpcode(data_006246B4, pc);
        return func_004C3B10(pc->args[0]);
    }
    le = func_0047D9A0(src, value);
    return func_0047E3D0(&le, 1);
}

VOperand func_0047CD10(VOperand* src, CInt64* value, Type* type);

// 0x47D780: src <= a constant (unsigned): a wide long long constant compares
// as < value + 1. (3.0.1 tests for cint64_min here, not the unsigned maximum.)
VOperand func_0047D780(VOperand* src, CInt64* value, Type* type) {
    CInt64 next;

    if (IS_TYPE_LL(type) && !CInt64_IsInRange(*value, 4)) {
        if (CInt64_Equal(*value, cint64_min)) {
            return func_004C3BA0(func_0047F680(1), 0);
        }
        next = CInt64_Add(*value, cint64_one);
        return func_0047CD10(src, &next, type);
    }
    return func_0047D870(src, value->lo);
}

// 0x47C700: src > a constant (unsigned): !(src <= value).
VOperand func_0047C700(VOperand* src, CInt64* value, Type* type) {
    VOperand le;

    le = func_0047D780(src, value, type);
    return func_0047E3D0(&le, 1);
}

// src > a constant (signed): !(src <= value).
VOperand func_0047C990(VOperand* src, CInt64* value, Type* type) {
    VOperand le;

    le = func_0047D8B0(src, value, type);
    return func_0047E3D0(&le, 1);
}

// src != value, as 0 or 1 in a new register: 0 < (src ^ value), unsigned.
VOperand func_0047DC40(VOperand* src, SInt32 value) {
    MPCode* pc;
    VOperand zero;
    VOperand x;
    VOperand zero2;
    CInt64 imm;

    zero = func_004C3BA0(0, 0);
    if (!value) {
        if (src->kind == 3) {
            imm = src->imm;
            pc = func_00480420(0xD7, 3);
            pc->args[0] = func_004C3DD0(0);
            pc->args[1] = func_004C3C60(&zero);
            pc->args[2] = func_004C3D50(imm);
            appendpcode(data_006246B4, pc);
            return func_004C3B10(pc->args[0]);
        } else {
            pc = func_00480420(0xD7, 3);
            pc->args[0] = func_004C3DD0(0);
            pc->args[1] = func_004C3C60(&zero);
            pc->args[2] = func_004C3C60(src);
            appendpcode(data_006246B4, pc);
            return func_004C3B10(pc->args[0]);
        }
    }

    x = func_0047E3D0(src, value);
    zero2 = func_004C3BA0(0, 0);
    if (x.kind == 3) {
        return func_00479E40(0xD7, &zero2, x.imm);
    }
    pc = func_00480420(0xD7, 3);
    pc->args[0] = func_004C3DD0(0);
    pc->args[1] = func_004C3C60(&zero2);
    pc->args[2] = func_004C3C60(&x);
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

// src != a constant: a wide long long constant goes through registers.
VOperand func_0047D9F0(VOperand* src, CInt64* value, Type* type) {
    MPCode* pc;
    VOperand v;
    CInt64 imm;

    if (IS_TYPE_LL(type) && !CInt64_IsInRange(*value, 4)) {
        v = func_0047F9F0(*value, type);
        if (v.kind == 3) {
            imm = v.imm;
            pc = func_00480420(0x5AB, 3);
            pc->args[0] = func_004C3DD0(0);
            pc->args[1] = func_004C3C60(src);
            pc->args[2] = func_004C3D50(imm);
        } else {
            pc = func_00480420(0x5AB, 3);
            pc->args[0] = func_004C3DD0(0);
            pc->args[1] = func_004C3C60(src);
            pc->args[2] = func_004C3C60(&v);
        }
        appendpcode(data_006246B4, pc);
        return func_004C3B10(pc->args[0]);
    }
    return func_0047DC40(src, value->lo);
}

// --- branches ------------------------------------------------------------------

extern UInt8 data_00625E83; // something branches to the return label
extern CLabel* data_006242A4; // returnlabel
extern MOperand func_004C3C00(CLabel* label);

// End the block with a conditional branch to label; the fall-through
// starts a new block.

// Branch on a register compared with zero.
void func_00479540(UInt16 op, VOperand* src, CLabel* label) {
    MPCode* pc;
    MPCodeLabel* fall;

    fall = makepclabel();
    if (data_006242A4 == label) {
        data_00625E83 = 1;
    }
    if (!label->pclabel) {
        label->pclabel = (struct PCodeLabel*)makepclabel();
    }
    pc = func_00480420(op, 2);
    pc->args[0] = func_004C3C60(src);
    pc->args[1] = func_004C3C00(label);
    appendpcode(data_006246B4, pc);
    pcbranch(data_006246B4, (MPCodeLabel*)label->pclabel, 0);
    pcbranch(data_006246B4, fall, 0);
    makepcblock();
    pclabel(data_006246B4, fall);
}

// Branch on two registers compared.
void func_00479430(UInt16 op, VOperand* a, VOperand* b, CLabel* label) {
    MPCode* pc;
    MPCodeLabel* fall;

    fall = makepclabel();
    if (data_006242A4 == label) {
        data_00625E83 = 1;
    }
    pc = func_00480420(op, 3);
    pc->args[0] = func_004C3C60(a);
    pc->args[1] = func_004C3C60(b);
    pc->args[2] = func_004C3C00(label);
    appendpcode(data_006246B4, pc);
    pcbranch(data_006246B4, (MPCodeLabel*)label->pclabel, 0);
    pcbranch(data_006246B4, fall, 0);
    makepcblock();
    pclabel(data_006246B4, fall);
}

// a branch on a compared with imm: op3 against the constant in a new
// register, or op2 against zero
static void casebranch(VOperand* a, SInt32 imm, CLabel* label, UInt16 op3, UInt16 op2) {
    MPCode* pc;
    MPCodeLabel* fall;
    VOperand c;

    if (imm) {
        c = func_004C3BA0(func_0047F680(imm), 0);
        fall = makepclabel();
        if (data_006242A4 == label) {
            data_00625E83 = 1;
        }
        pc = func_00480420(op3, 3);
        pc->args[0] = func_004C3C60(a);
        pc->args[1] = func_004C3C60(&c);
        pc->args[2] = func_004C3C00(label);
    } else {
        fall = makepclabel();
        if (data_006242A4 == label) {
            data_00625E83 = 1;
        }
        if (!label->pclabel) {
            label->pclabel = (struct PCodeLabel*)makepclabel();
        }
        pc = func_00480420(op2, 2);
        pc->args[0] = func_004C3C60(a);
        pc->args[1] = func_004C3C00(label);
    }
    appendpcode(data_006246B4, pc);
    pcbranch(data_006246B4, (MPCodeLabel*)label->pclabel, 0);
    pcbranch(data_006246B4, fall, 0);
    makepcblock();
    pclabel(data_006246B4, fall);
}

// 0x47B6A0: a switch case: branch to label when a equals imm (beq / beqz)
void func_0047B6A0(VOperand* a, SInt32 imm, CLabel* label) {
    casebranch(a, imm, label, 0x18, 0x1A);
}

// 0x47B480: branch to label when a differs from imm (bne / bnez)
void func_0047B480(VOperand* a, SInt32 imm, CLabel* label) {
    casebranch(a, imm, label, 0x28, 0x2A);
}

extern VOperand func_0047CF50(VOperand* src, SInt32 value);
extern VOperand func_0047D240(VOperand* src, CInt64* value, Type* type);
extern VOperand func_0047E3D0(VOperand* src, SInt32 value);
extern VOperand func_0047F9F0(CInt64 value, Type* type);
extern MOperand func_004C3DD0(char rclass);
extern MOperand func_004C3D50(CInt64 value);
extern VOperand func_004C3B10(MOperand op);

// 0x47CCD0: src >= value, signed: !(src < value)
VOperand func_0047CCD0(VOperand* src, CInt64* value, Type* type) {
    VOperand t;

    t = func_0047D240(src, value, type);
    return func_0047E3D0(&t, 1);
}

// 0x47CD10: src < value, unsigned; a 64-bit constant that does not fit 32
// bits gets an sltu against it
VOperand func_0047CD10(VOperand* src, CInt64* value, Type* type) {
    MPCode* pc;
    VOperand c;
    CInt64 k;

    if (!((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8) ||
        CInt64_IsInRange(*value, 4)) {
        return func_0047CF50(src, value->lo);
    }
    c = func_0047F9F0(*value, type);
    if (c.kind == 3) {
        k = c.imm;
        pc = func_00480420(0xD7, 3);
        pc->args[0] = func_004C3DD0(0);
        pc->args[1] = func_004C3C60(src);
        pc->args[2] = func_004C3D50(k);
    } else {
        pc = func_00480420(0xD7, 3);
        pc->args[0] = func_004C3DD0(0);
        pc->args[1] = func_004C3C60(src);
        pc->args[2] = func_004C3C60(&c);
    }
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

// Branch on a register compared with an immediate.
void func_00479310(UInt16 op, VOperand* a, SInt32 imm, CLabel* label) {
    MPCode* pc;
    MPCodeLabel* fall;
    VOperand c;

    c.kind = 3;
    CInt64_SetLong(&c.imm, imm);
    fall = makepclabel();
    if (data_006242A4 == label) {
        data_00625E83 = 1;
    }
    pc = func_00480420(op, 3);
    pc->args[0] = func_004C3C60(a);
    pc->args[1] = func_004C3C60(&c);
    pc->args[2] = func_004C3C00(label);
    appendpcode(data_006246B4, pc);
    pcbranch(data_006246B4, (MPCodeLabel*)label->pclabel, 0);
    pcbranch(data_006246B4, fall, 0);
    makepcblock();
    pclabel(data_006246B4, fall);
}

// op dst, src, shift in a new register.
VOperand func_00479D10(SInt16 op, VOperand* src, SInt32 shift) {
    MPCode* pc;

    pc = makepcode(op, data_00625A98[0]++, src->reg, shift);
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

// Sign (0x599) or zero (0x59A) extend from the low bits.
VOperand func_00479C30(SInt16 op, VOperand* src, SInt32 bits) {
    MPCode* pc;

    CError_ASSERT(1711, op == 0x599 || op == 0x59A);
    if (bits == 32) {
        return *src;
    }
    if (bits > 32) {
        CError_FATAL(1714);
    }
    pc = makepcode(op, data_00625A98[0]++, src->reg, bits);
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

extern MOperand func_004C3E70(SInt16 reg, UInt8 rclass, UInt16 flags);
extern struct Alias* func_0053BD10(VOperand* mem, MPCode* pc);

// A load or store between reg and the memory operand.
void func_00479710(UInt16 op, VOperand* reg, VOperand* mem) {
    MPCode* pc;
    UInt32 flags;
    VOperand m;

    flags = 4;
    if (mem->object) {
        if (is_volatile_object((Object*)mem->object)) {
            flags |= 0x80;
        }
        if (is_const_object((Object*)mem->object)) {
            flags |= 0x40;
        }
    } else {
        flags |= 0x20;
    }
    m = *mem;
    pc = func_00480420(op, 3);
    pc->args[0] = func_004C3C60(reg);
    pc->args[1] = func_004C3C60(&m);
    pc->args[2] = func_004C3E70(mem->reg, 0, 1);
    if (pc->op == 0x597 || pc->op == 0x1D2 || pc->op == 0x268) {
        pc->args[1].flags |= 0x2000;
    }
    if (m.object) {
        pc->alias = func_0053BD10(mem, pc);
    }
    pc->flags |= flags;
    appendpcode(data_006246B4, pc);
}

// A load from the memory operand into a new register of class rclass.
VOperand func_00479870(UInt16 op, VOperand* mem, char rclass) {
    MPCode* pc;
    UInt32 flags;
    VOperand m;
    VOperand result;

    flags = 2;
    m = *mem;
    if (mem->object) {
        if (is_volatile_object((Object*)mem->object)) {
            flags |= 0x80;
        }
        if (is_const_object((Object*)mem->object)) {
            flags |= 0x40;
        }
    } else {
        flags |= 0x20;
    }
    pc = func_00480420(op, 3);
    pc->args[0] = func_004C3DD0(rclass);
    pc->args[1] = func_004C3C60(&m);
    pc->args[2] = func_004C3E70(mem->reg, 0, 1);
    if (pc->op == 0x575 || pc->op == 0x1D1) {
        pc->args[0].flags |= 0x2000;
    }
    if (m.object) {
        pc->alias = func_0053BD10(mem, pc);
    }
    pc->flags = flags;
    appendpcode(data_006246B4, pc);
    result = func_004C3B10(pc->args[0]);
    return result;
}

// dst = src op imm in a new register.
VOperand func_00479D70(UInt16 op, VOperand* src, SInt32 imm) {
    MPCode* pc;

    pc = func_00480420(op, 3);
    pc->args[0] = func_004C3DD0(0);
    pc->args[1] = func_004C3C60(src);
    pc->args[2] = func_004C3D90(imm);
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

// dst = src op value (a 64-bit immediate) in a new register.
VOperand func_00479E40(UInt16 op, VOperand* src, CInt64 value) {
    MPCode* pc;

    pc = func_00480420(op, 3);
    pc->args[0] = func_004C3DD0(0);
    pc->args[1] = func_004C3C60(src);
    pc->args[2] = func_004C3D50(value);
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

// Move an integer register to a new FP register (mtc1).
VOperand func_0047A060(VOperand* src) {
    emitpcode(0xAD, src->reg, data_00625A98[1]++);
    setpcodeflags(0x4000);
    return func_004C3B10(data_006246B4->last->args[1]);
}

// Branch on the FP condition (bc1t/bc1f).
void func_00479B00(SInt16 op, MPCodeLabel* label) {
    MPCodeLabel* fall;

    fall = makepclabel();
    emitpcode(op, label);
    pcbranch(data_006246B4, label, 0);
    pcbranch(data_006246B4, fall, 0);
    makepcblock();
    pclabel(data_006246B4, fall);
}

void func_0047B8C0(VOperand* a, SInt32 imm, CLabel* label);
void func_0047BF50(VOperand* a, SInt32 imm, CLabel* label);

// Branch when a < imm; a long long constant compares both halves.
void func_0047A170(VOperand* a, CInt64* imm, Type* type, CLabel* label) {
    SInt32 hi;
    SInt32 lo;

    if (IS_TYPE_LL(type) && !CInt64_IsInRange(*imm, 4)) {
        hi = imm->hi;
        lo = imm->lo;
        if (is_unsigned(type)) {
            func_0047B8C0(a, hi, label);
        } else {
            func_0047BF50(a, hi, label);
        }
        func_0047B8C0(a, lo, label);
        return;
    }
    if (is_unsigned(type)) {
        func_0047B8C0(a, imm->lo, label);
    } else {
        func_0047BF50(a, imm->lo, label);
    }
}

void func_0047C290(VOperand* a, SInt32 imm, CLabel* label);
void func_0047C4C0(VOperand* a, SInt32 imm, CLabel* label);

// 0x47A0C0: branch when a <= imm; a long long constant compares both halves.
void func_0047A0C0(VOperand* a, CInt64* imm, Type* type, CLabel* label) {
    SInt32 hi;
    SInt32 lo;

    if (IS_TYPE_LL(type) && !CInt64_IsInRange(*imm, 4)) {
        hi = imm->hi;
        lo = imm->lo;
        if (is_unsigned(type)) {
            func_0047B8C0(a, hi, label);
        } else {
            func_0047BF50(a, hi, label);
        }
        func_0047C290(a, lo, label);
        return;
    }
    if (is_unsigned(type)) {
        func_0047C290(a, imm->lo, label);
    } else {
        func_0047C4C0(a, imm->lo, label);
    }
}

// A one-operand branch to label.
void func_00479A50(UInt16 op, CLabel* label) {
    MPCode* pc;
    MPCodeLabel* fall;

    fall = makepclabel();
    if (data_006242A4 == label) {
        data_00625E83 = 1;
    }
    pc = func_00480420(op, 1);
    pc->args[0] = func_004C3C00(label);
    appendpcode(data_006246B4, pc);
    pcbranch(data_006246B4, (MPCodeLabel*)label->pclabel, 0);
    pcbranch(data_006246B4, fall, 0);
    makepcblock();
    pclabel(data_006246B4, fall);
}

// Branch when a < imm (unsigned).
void func_0047B8C0(VOperand* a, SInt32 imm, CLabel* label) {
    VOperand t;

    if (imm == 0) {
        return;
    }
    if (imm == 1) {
        func_00479540(0x1A, a, label);
        return;
    }
    t = func_0047CF50(a, imm);
    func_00479540(0x2A, &t, label);
}

// Branch when a < imm (signed).
void func_0047BF50(VOperand* a, SInt32 imm, CLabel* label) {
    VOperand t;

    if (imm == 0) {
        func_00479540(0x24, a, label);
    } else if (imm == 1) {
        func_00479540(0x22, a, label);
    } else if (imm != (SInt32)0x80000000) {
        t = func_0047D480(a, imm);
        func_00479540(0x2A, &t, label);
    }
}

extern void func_00480710(MPCodeLabel* label); // branch_always

// 0x47C290: branch when a <= imm (unsigned).
void func_0047C290(VOperand* a, SInt32 imm, CLabel* label) {
    VOperand t;

    if (imm == 0) {
        func_00479540(0x1A, a, label);
    } else if (imm == -1) {
        func_00480710((MPCodeLabel*)label->pclabel);
    } else {
        t = func_0047CF50(a, imm + 1);
        func_00479540(0x2A, &t, label);
    }
}

// 0x47C4C0: branch when a <= imm (signed).
void func_0047C4C0(VOperand* a, SInt32 imm, CLabel* label) {
    VOperand t;

    if (imm == 0) {
        func_00479540(0x22, a, label);
    } else if (imm == 0x7FFFFFFF) {
        func_00480710((MPCodeLabel*)label->pclabel);
    } else {
        t = func_0047D480(a, imm + 1);
        func_00479540(0x2A, &t, label);
    }
}

// Branch when a < b (unsigned).
void func_0047AE80(VOperand* a, VOperand* b, CLabel* label) {
    VOperand t;

    t = func_0047F740(0xD7, a, b, 0);
    func_00479540(0x2A, &t, label);
}

// Branch when a < b (signed).
void func_0047B180(VOperand* a, VOperand* b, CLabel* label) {
    VOperand t;

    t = func_0047F740(0xD4, a, b, 0);
    func_00479540(0x2A, &t, label);
}

// Branch when a (op) imm, for beq/bne against a constant.
inline void branch_eq_const(
    UInt16 op, UInt16 zop, VOperand* a, CInt64* imm, Type* type, CLabel* label
) {
    VOperand t;

    if (IS_TYPE_LL(type) && !CInt64_IsInRange(*imm, 4)) {
        t = func_0047F9F0(*imm, type);
        func_00479430(op, a, &t, label);
    } else if (!imm->lo) {
        func_00479540(zop, a, label);
    } else {
        t = func_004C3BA0(func_0047F680(imm->lo), 0);
        func_00479430(op, a, &t, label);
    }
}

// Branch when a == imm.
void func_0047A850(VOperand* a, CInt64* imm, Type* type, CLabel* label) {
    branch_eq_const(0x18, 0x1A, a, imm, type, label);
}

// Branch when a != imm.
void func_0047A220(VOperand* a, CInt64* imm, Type* type, CLabel* label) {
    branch_eq_const(0x28, 0x2A, a, imm, type, label);
}
