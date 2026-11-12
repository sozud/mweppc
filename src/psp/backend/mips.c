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

#if MWCCPS2
// b210 (0x48EA50): 64-bit loads (ld) for 8-byte values, 128-bit (lq) for
// 16-byte aggregates and the 128-bit integer; doubles outside the FPU are a
// 64-bit load.
UInt16 func_0047FD20(Type* type, SInt32* fpr) {
    *fpr = 0;
    if (type->type == TYPEENUM) {
        if (type->size == 1) {
            return MACRO_OP(0x50);
        }
        if (type->size == 2) {
            return MACRO_OP(0x51);
        }
        if (type->size == 4) {
            return MACRO_OP(0x55);
        }
        if (type->size == 8) {
            return MACRO_OP(0x4A);
        }
        CError_Internal("mips.c", CError_B145_LINE(168));
        return 0;
    }
    if (type->type == TYPESTRUCT || type->type == TYPECLASS || type->type == TYPEARRAY) {
        if (type->size == 1) {
            return MACRO_OP(0x50);
        }
        if (type->size == 2) {
            return MACRO_OP(0x51);
        }
        if (type->size <= 4) {
            return MACRO_OP(0x55);
        }
        if (type->size <= 8) {
            return MACRO_OP(0x4A);
        }
        if (type->size <= 16) {
            return MACRO_OP(0x5D);
        }
        CError_Internal("mips.c", CError_B145_LINE(179));
        return 0;
    }
    if (type->type == TYPEMEMBERPOINTER || type->type == TYPEPOINTER) {
        return MACRO_OP(0x55);
    }
    switch (TYPE_INTEGRAL(type)->integral) {
        case IT_BOOL:
            return MACRO_OP(0x49);
        case IT_CHAR:
            return copts.unsigned_char ? MACRO_OP(0x49) : MACRO_OP(0x50);
        case IT_SCHAR:
            return MACRO_OP(0x50);
        case IT_UCHAR:
            return MACRO_OP(0x49);
        case IT_WCHAR_T:
            return MACRO_OP(0x52);
        case IT_SHORT:
            return MACRO_OP(0x51);
        case IT_USHORT:
            return MACRO_OP(0x52);
        case IT_INT:
        case IT_UINT:
        case IT_LONG:
        case IT_ULONG:
            if (type->size == 4) {
                return MACRO_OP(0x55);
            }
            if (type->size == 8) {
                return MACRO_OP(0x4A);
            }
            CError_Internal("mips.c", CError_B145_LINE(208));
            return 0;
        case IT_LONGLONG:
        case IT_ULONGLONG:
            return MACRO_OP(0x4A);
        case IT_FLOAT:
            if (type->type == TYPEFLOAT && IN_FPR(type)) {
                *fpr = 1;
                return MACRO_OP(0x56);
            }
            return MACRO_OP(0x55);
        case IT_SHORTDOUBLE:
        case IT_DOUBLE:
        case IT_LONGDOUBLE:
            if (type->type == TYPEFLOAT && IN_FPR(type)) {
                *fpr = 1;
                return (copts.mips_isa >= 2 && (copts.fp_library & 2)) ? MACRO_OP(0x4B)
                                                                       : MACRO_OP(0x48);
            }
            return MACRO_OP(0x4A);
        case 17:
            *fpr = 1;
            return (copts.mips_isa >= 2 && (copts.fp_library & 2)) ? MACRO_OP(0x4B)
                                                                   : MACRO_OP(0x48);
        case 18:
            return MACRO_OP(0x5D);
        default:
            CError_Internal("mips.c", CError_B145_LINE(247));
            return 0;
    }
}

// b210 (0x48ED30): the store side of func_0047FD20
UInt16 func_0047FFD0(Type* type) {
    if (type->type == TYPEENUM) {
        if (type->size == 1) {
            return MACRO_OP(0x75);
        }
        if (type->size == 2) {
            return MACRO_OP(0x76);
        }
        if (type->size == 4) {
            return MACRO_OP(0x77);
        }
        if (type->size == 8) {
            return MACRO_OP(0x6F);
        }
        CError_Internal("mips.c", CError_B145_LINE(92));
        return 0;
    }
    if (type->type == TYPESTRUCT || type->type == TYPECLASS) {
        if (type->size == 1) {
            return MACRO_OP(0x75);
        }
        if (type->size == 2) {
            return MACRO_OP(0x76);
        }
        if (type->size == 4) {
            return MACRO_OP(0x77);
        }
        if (type->size == 8) {
            return MACRO_OP(0x6F);
        }
        if (type->size == 16) {
            return MACRO_OP(0x7F);
        }
        CError_Internal("mips.c", CError_B145_LINE(103));
        return 0;
    }
    if (type->type == TYPEMEMBERPOINTER || type->type == TYPEPOINTER) {
        return MACRO_OP(0x77);
    }
    switch (TYPE_INTEGRAL(type)->integral) {
        case IT_BOOL:
            return MACRO_OP(0x75);
        case IT_CHAR:
            return MACRO_OP(0x75);
        case IT_SCHAR:
            return MACRO_OP(0x75);
        case IT_UCHAR:
            return MACRO_OP(0x75);
        case IT_WCHAR_T:
            return MACRO_OP(0x76);
        case IT_SHORT:
            return MACRO_OP(0x76);
        case IT_USHORT:
            return MACRO_OP(0x76);
        case IT_INT:
        case IT_UINT:
        case IT_LONG:
        case IT_ULONG:
            if (type->size == 4) {
                return MACRO_OP(0x77);
            }
            if (type->size == 8) {
                return MACRO_OP(0x6F);
            }
            CError_Internal("mips.c", CError_B145_LINE(130));
        case IT_LONGLONG:
            return MACRO_OP(0x6F);
        case IT_ULONGLONG:
            return MACRO_OP(0x6F);
        case IT_FLOAT:
            return (type->type == TYPEFLOAT && IN_FPR(type)) ? MACRO_OP(0x79) : MACRO_OP(0x77);
        case IT_SHORTDOUBLE:
        case IT_DOUBLE:
        case IT_LONGDOUBLE:
            return (type->type == TYPEFLOAT && IN_FPR(type))
                ? ((copts.mips_isa >= 2 && (copts.fp_library & 2)) ? MACRO_OP(0x70)
                                                                   : MACRO_OP(0x6C))
                : MACRO_OP(0x6F);
        case 17:
            return (copts.mips_isa >= 2 && (copts.fp_library & 2)) ? MACRO_OP(0x70)
                                                                   : MACRO_OP(0x6C);
        case 18:
            return MACRO_OP(0x7F);
        default:
            CError_Internal("mips.c", CError_B145_LINE(154));
            return 0;
    }
}
#else
// The load opcode for a value of the type; *fpr is 1 when it goes to a
// floating-point register (5 for 128-bit values).
UInt16 func_0047FD20(Type* type, SInt32* fpr) {
    *fpr = 0;
    if (type->type == TYPEENUM) {
        switch (type->size) {
            case 1:
                return MACRO_OP(0x50);
            case 2:
                return MACRO_OP(0x51);
            case 4:
                return MACRO_OP(0x55);
            case 8:
                return MACRO_OP(0x55);
            default:
                CError_Internal("mips.c", CError_B145_LINE(174));
                return 0;
        }
    }
    if (type->type == TYPESTRUCT || type->type == TYPECLASS || type->type == TYPEARRAY) {
        if (type->size == 1) {
            return MACRO_OP(0x50);
        }
        if (type->size == 2) {
            return MACRO_OP(0x51);
        }
        if (type->size <= 4) {
            return MACRO_OP(0x55);
        }
        if (type->size <= 8) {
            return MACRO_OP(0x4A);
        }
        CError_Internal("mips.c", CError_B145_LINE(185));
        return 0;
    }
    if (type->type == TYPEMEMBERPOINTER || type->type == TYPEPOINTER) {
        return MACRO_OP(0x55);
    }
    switch (TYPE_INTEGRAL(type)->integral) {
        case IT_BOOL:
            return MACRO_OP(0x49);
        case IT_CHAR:
            return copts.unsigned_char ? MACRO_OP(0x49) : MACRO_OP(0x50);
        case IT_SCHAR:
            return MACRO_OP(0x50);
        case IT_UCHAR:
            return MACRO_OP(0x49);
        case IT_WCHAR_T:
            return copts.ushort_wchar_t ? MACRO_OP(0x52) : MACRO_OP(0x55);
        case IT_SHORT:
            return MACRO_OP(0x51);
        case IT_USHORT:
            return MACRO_OP(0x52);
        case IT_INT:
        case IT_UINT:
        case IT_LONG:
        case IT_ULONG:
            if (type->size == 4) {
                return MACRO_OP(0x55);
            }
            CError_Internal("mips.c", CError_B145_LINE(220));
            return 0;
        case IT_LONGLONG:
        case IT_ULONGLONG:
            return MACRO_OP(0x55);
        case IT_FLOAT:
            if (type->type == TYPEFLOAT && IN_FPR(type)) {
                *fpr = 1;
                return MACRO_OP(0x56);
            }
            return MACRO_OP(0x55);
        case IT_SHORTDOUBLE:
        case IT_DOUBLE:
        case IT_LONGDOUBLE:
            if (type->type == TYPEFLOAT && IN_FPR(type)) {
                *fpr = 1;
                return (copts.mips_isa >= 2 && (copts.fp_library & 2)) ? MACRO_OP(0x4B)
                                                                       : MACRO_OP(0x48);
            }
            return MACRO_OP(0x55);
        case 17:
            *fpr = 5;
            return MACRO_OP(0x9B);
        default:
            CError_Internal("mips.c", CError_B145_LINE(259));
            return 0;
    }
}

// The store opcode for a value of the type.
UInt16 func_0047FFD0(Type* type) {
    if (type->type == TYPEENUM) {
        switch (type->size) {
            case 1:
                return MACRO_OP(0x75);
            case 2:
                return MACRO_OP(0x76);
            case 4:
                return MACRO_OP(0x77);
            case 8:
                return MACRO_OP(0x77);
            default:
                CError_Internal("mips.c", CError_B145_LINE(92));
                return 0;
        }
    }
    if (type->type == TYPESTRUCT || type->type == TYPECLASS) {
        switch (type->size) {
            case 1:
                return MACRO_OP(0x75);
            case 2:
                return MACRO_OP(0x76);
            case 4:
                return MACRO_OP(0x77);
            case 8:
                return MACRO_OP(0x6F);
            default:
                CError_Internal("mips.c", CError_B145_LINE(103));
                return 0;
        }
    }
    if (type->type == TYPEMEMBERPOINTER || type->type == TYPEPOINTER) {
        return MACRO_OP(0x77);
    }
    switch (TYPE_INTEGRAL(type)->integral) {
        case IT_BOOL:
            return MACRO_OP(0x75);
        case IT_CHAR:
            return MACRO_OP(0x75);
        case IT_SCHAR:
            return MACRO_OP(0x75);
        case IT_UCHAR:
            return MACRO_OP(0x75);
        case IT_WCHAR_T:
            return copts.ushort_wchar_t ? MACRO_OP(0x76) : MACRO_OP(0x77);
        case IT_SHORT:
            return MACRO_OP(0x76);
        case IT_USHORT:
            return MACRO_OP(0x76);
        case IT_INT:
        case IT_UINT:
        case IT_LONG:
        case IT_ULONG:
            if (type->size == 4) {
                return MACRO_OP(0x77);
            }
            CError_Internal("mips.c", CError_B145_LINE(136));
            return MACRO_OP(0x77);
        case IT_LONGLONG:
            return MACRO_OP(0x77);
        case IT_ULONGLONG:
            return MACRO_OP(0x77);
        case IT_FLOAT:
            if (type->type == TYPEFLOAT && IN_FPR(type)) {
                return MACRO_OP(0x79);
            }
            return MACRO_OP(0x77);
        case IT_SHORTDOUBLE:
        case IT_DOUBLE:
        case IT_LONGDOUBLE:
            if (type->type == TYPEFLOAT && IN_FPR(type)) {
                return (copts.mips_isa >= 2 && (copts.fp_library & 2)) ? MACRO_OP(0x70)
                                                                       : MACRO_OP(0x6C);
            }
            return MACRO_OP(0x77);
        case 17:
            return MACRO_OP(0x9D);
        default:
            CError_Internal("mips.c", CError_B145_LINE(160));
            return 0;
    }
}
#endif

#if MWCCPS2
// b210 0x48EFD0 (asserts name PCodeUtilities.c): a load just emitted from a
// pointer of smaller alignment becomes an unaligned pair (ldl/ldr, lwl/lwr)
extern MPCode* func_00480420(int op, SInt16 nargs);
extern SInt16 data_00625E3E; // the stack pointer register

void func_00480230(SInt16 align, void* unk) {
    MPCode* pc;
    MPCode* p;
    UInt16 first;
    UInt16 second;
    UInt16 op;

    pc = data_006246B4->last;
    op = pc->op;
    switch (op) {
        case MACRO_OP(0x5D):
            CError_Internal("PCodeUtilities.c", CError_B145_LINE(577));
            break;
        case MACRO_OP(0x4A):
            if (pc->args[2].reg == data_00625E3E || pc->args[2].reg == 0x1C) {
                return;
            }
            if (align < 8) {
                if (copts.littleendian) {
                    first = MACRO_OP(0x4F);
                    second = MACRO_OP(0x4E);
                } else {
                    first = MACRO_OP(0x4E);
                    second = MACRO_OP(0x4F);
                }
                pc->op = first;
                p = func_00480420(second, 3);
                p->flags = pc->flags;
                p->args[0] = pc->args[0];
                p->args[0].flags |= 1;
                p->args[1] = pc->args[1];
                p->args[2] = pc->args[2];
                p->alias = pc->alias;
                insertpcodeafter(pc, p);
                p->args[1].value += 7;
            }
            break;
        case MACRO_OP(0x55):
            if (pc->args[2].reg == data_00625E3E || pc->args[2].reg == 0x1C) {
                return;
            }
            if (align < 4) {
                if (copts.littleendian) {
                    first = MACRO_OP(0x5B);
                    second = MACRO_OP(0x5A);
                } else {
                    first = MACRO_OP(0x5A);
                    second = MACRO_OP(0x5B);
                }
                pc->op = first;
                p = func_00480420(second, 3);
                p->flags = pc->flags;
                p->args[0] = pc->args[0];
                p->args[0].flags |= 1;
                p->args[1] = pc->args[1];
                p->args[2] = pc->args[2];
                p->alias = pc->alias;
                insertpcodeafter(pc, p);
                p->args[1].value += 3;
            }
            break;
    }
}
#endif

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
                CError_Internal("mips.c", CError_PS2_LINE(2347 - 12)); // mwccps2: 0x91F
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
                    CError_Internal("mips.c", CError_B145_LINE(2152));
            }
        }
        format++;
    }
    return n;
}

#undef CError_PS2_SHIFT
#define CError_PS2_SHIFT -12 // mwccps2: its assert is at line 0x827
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
                CError_Internal("mips.c", CError_PS2_LINE(2099));
        }
        format++;
    }
    return n;
}
#undef CError_PS2_SHIFT
#define CError_PS2_SHIFT 0

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
    data_0062554C[op - BE_NUM_OPCODES] = format;
    data_00625CE4[op - BE_NUM_OPCODES] = real;
    data_00625B86[op - BE_NUM_OPCODES] = alt;
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

#if MWCCPS2
VOperand ps2_const64_48DBF0(SInt32 hi, SInt32 lo, Type* type);

// b210 (0x48E3D0): a constant in a register. A long long takes one 64-bit
// register (built by ps2_const64_48DBF0 unless it fits 32 bits); a 128-bit
// constant joins its two 64-bit halves with 0x21A (pcpyld).
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 3 // b145 asserts
VOperand func_0047F9F0(CInt64 value, Type* type) {
    VOperand hi;
    VOperand lo;
    SInt32 reg;

    if ((type->type == TYPEINT || type->type == TYPEENUM) &&
        (type->size == 8 || type->size == 16)) {
        if (!((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 16) &&
            CInt64_IsInRange(value, 4)) {
            return func_004C3BA0(func_0047F680(value.lo), 0);
        }
        if (!((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 16) &&
            CInt64_IsInRange(value, 8)) {
            return ps2_const64_48DBF0(value.mid[1], value.lo, type);
        }
        if (CInt64_IsInRange(value, 16)) {
            if (value.hi || value.mid[0]) {
                hi = ps2_const64_48DBF0(value.hi, value.mid[0], type);
            }
            if (value.mid[1] || value.lo) {
                lo = ps2_const64_48DBF0(value.mid[1], value.lo, type);
            }
            if (!value.hi && !value.mid[0] && !value.mid[1] && !value.lo) {
                emitpcode(0x21A, reg = data_00625A98[0]++, 0, 0);
            } else if (!value.hi && !value.mid[0]) {
                emitpcode(0x21A, reg = data_00625A98[0]++, 0, lo.reg);
            } else if (!value.mid[1] && !value.lo) {
                emitpcode(0x21A, reg = data_00625A98[0]++, hi.reg, 0);
            } else {
                emitpcode(0x21A, reg = data_00625A98[0]++, hi.reg, lo.reg);
            }
            return func_004C3BA0(reg, 0);
        }
        {
            VOperand zero = {0};

            CError_FATAL(389);
            return zero;
        }
    }
    return func_004C3BA0(func_0047F680(value.lo), 0);
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#else
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
#endif

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

#if MWCCPS2
extern MOperand func_004C3D90(SInt32 value);
// b210 inlines func_0047F740 into func_0047F2D0, func_0047ECC0 and func_0047EF00
inline static VOperand ps2_binop_inline(UInt16 op, VOperand* a, VOperand* b, char rclass) {
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
// b210 has no call to func_0047F740 inside this unit: every use is inline
#define func_0047F740 ps2_binop_inline
#endif

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
#if MWCCPS2
    result = ps2_binop_inline(MACRO_OP(0x44), src, &tmp, 0);
#else
    result = func_0047F740(MACRO_OP(0x44), src, &tmp, 0);
#endif
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

extern void func_004C5890(int code, ...); // MIPSError_Warning (31: shift count too large)

#define IS_TYPE_LL(t) (((t)->type == TYPEINT || (t)->type == TYPEENUM) && (t)->size == 8)
#define IS_TYPE_128(t) (((t)->type == TYPEINT || (t)->type == TYPEENUM) && (t)->size == 16)

// src << shift in a new register.
VOperand func_0047EBB0(VOperand* src, SInt32 shift, Type* type) {
    SInt32 dst;

    if (!src->reg || !shift) {
        return *src;
    }
    dst = data_00625A98[0]++;
#if MWCCPS2
    // b210 (0x48C510): 64-bit registers shift with dsll (0x84) and dsll32
    // (0x85) past 31
    if ((type && (IS_TYPE_LL(type) || IS_TYPE_128(type))) || shift > 31) {
        if (shift > 31) {
            emitpcode(0x85, dst, src->reg, shift - 32);
        } else {
            emitpcode(0x84, dst, src->reg, shift);
        }
    } else {
        emitpcode(0xD2, dst, src->reg, shift);
    }
#else
    if (shift > 31) {
        func_004C5890(31, shift);
        shift = 31;
    }
    if (!type || IS_TYPE_LL(type) || !(IS_TYPE_LL(type) || IS_TYPE_128(type))) {
        if (shift <= 31) {
            emitpcode(0xD2, dst, src->reg, shift);
        }
    }
#endif
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
#if MWCCPS2
    // b210 (0x48C350): 64-bit registers shift with dsrl/dsra (0x87/0x8A)
    // and dsrl32/dsra32 (0x88/0x8B) past 31
    if ((type && (IS_TYPE_LL(type) || IS_TYPE_128(type))) || shift > 31) {
        if (shift <= 31) {
            pc = makepcode(isunsigned ? 0x8A : 0x87, data_00625A98[0]++, src->reg, shift);
        } else if (shift > 31) {
            pc = makepcode(isunsigned ? 0x8B : 0x88, data_00625A98[0]++, src->reg, shift - 32);
        }
    } else {
        pc = makepcode(isunsigned ? 0xDA : 0xD8, data_00625A98[0]++, src->reg, shift);
    }
#else
    if (shift > 31) {
        func_004C5890(31, shift);
        shift = 31;
    }
    if (!type || IS_TYPE_LL(type) || !(IS_TYPE_LL(type) || IS_TYPE_128(type))) {
        if (shift <= 31) {
            pc = makepcode(isunsigned ? 0xDA : 0xD8, data_00625A98[0]++, src->reg, shift);
        }
    }
#endif
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

#if MWCCPS2
// b210 inlines its operand helpers into the 64-bit constant loader: these
// are func_0047F740 (op 0xBE, or) and func_0047E580 as inline copies.
inline static VOperand ps2_or_inline(VOperand* a, VOperand* b) {
    MPCode* pc;
    CInt64 imm;

    if (!b->reg) {
        return *a;
    }
    if (b->kind == 3) {
        imm = b->imm;
        pc = func_00480420(0xBE, 3);
        pc->args[0] = func_004C3DD0(0);
        pc->args[1] = func_004C3C60(a);
        pc->args[2] = func_004C3D50(imm);
        appendpcode(data_006246B4, pc);
        return func_004C3B10(pc->args[0]);
    } else {
        pc = func_00480420(0xBE, 3);
        pc->args[0] = func_004C3DD0(0);
        pc->args[1] = func_004C3C60(a);
        pc->args[2] = func_004C3C60(b);
        appendpcode(data_006246B4, pc);
        return func_004C3B10(pc->args[0]);
    }
}

inline static VOperand ps2_ormask_inline(VOperand* src, UInt32 mask) {
    MPCode* pc;

    if (!mask) {
        return *src;
    }
    if (!(mask & 0xFFFF0000)) {
        pc = makepcode(0xBF, data_00625A98[0]++, src->reg, mask);
    } else {
#if MWCCPSP_NATIVE // 3.0.1 evaluates arguments right to left
        SInt32 k = func_0047F680(mask);
        pc = makepcode(0xBE, data_00625A98[0]++, src->reg, k);
#else
        pc = makepcode(0xBE, data_00625A98[0]++, src->reg, func_0047F680(mask));
#endif
    }
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

// b210 (0x48DBF0): a 64-bit constant in a register. Values that sign-extend
// from 32 bits take one register; otherwise the high word is shifted up by
// 32 and the low word (built as two halves when its sign bit is set) or'ed in.
VOperand ps2_const64_48DBF0(SInt32 hi, SInt32 lo, Type* type) {
    VOperand t;
    VOperand k;

    if (hi == 0 && lo <= 0x7FFFFFFFU) {
        return func_004C3BA0(func_0047F680(lo), 0);
    }
    if (hi == -1 && lo >= 0x80000000) {
        return func_004C3BA0(func_0047F680(lo), 0);
    }

    t = func_004C3BA0(func_0047F680(hi), 0);
    t = func_0047EBB0(&t, 32, type);
    if (lo > 0) {
        k = func_004C3BA0(func_0047F680(lo), 0);
        k = ps2_or_inline(&k, &t);
    } else if (lo != 0) {
        k = func_004C3BA0(func_0047F680((UInt16)(lo >> 16)), 0);
        k = func_0047EBB0(&k, 16, type);
        k = ps2_ormask_inline(&k, lo & 0xFFFF);
        k = ps2_or_inline(&k, &t);
    } else {
        k = t;
    }
    return k;
}
#endif

VOperand func_0047EF00(VOperand* src, SInt32 value, Type* type);

// src + a constant: a wide long long constant goes through registers.
VOperand func_0047ECC0(VOperand* src, CInt64* value, Type* type) {
    VOperand v;

    if (IS_TYPE_LL(type) && !CInt64_IsInRange(*value, 4)) {
        v = func_0047F9F0(*value, type);
#if MWCCPS2
        return ps2_binop_inline(0x79, src, &v, 0);
#else
        return func_0047F740(0x79, src, &v, 0);
#endif
    }
    return func_0047EF00(src, value->lo, type);
}

extern MOperand func_004C3D90(SInt32 value);

#if MWCCPS2
// src + value in a new register: b210 adds to a long long with daddiu/daddu
// (64-bit registers) where mwccpsp stops.
VOperand func_0047EF00(VOperand* src, SInt32 value, Type* type) {
    MPCode* pc;
    VOperand tmp;

    if (!value) {
        return *src;
    } else if (type && IS_TYPE_LL(type)) {
        if (value >= -0x8000 && value < 0x8000) {
            pc = func_00480420(0x78, 3); // daddiu
            pc->args[0] = func_004C3DD0(0);
            pc->args[1] = func_004C3C60(src);
            pc->args[2] = func_004C3D90(value);
            appendpcode(data_006246B4, pc);
            return func_004C3B10(pc->args[0]);
        } else if (value >= 0x8000 && value < 0xFFFF) {
            tmp = func_0047EF00(src, 0x7FFF, type);
            return func_0047EF00(&tmp, value - 0x7FFF, type);
        } else if (value < -0x8000 && value >= -0x10000) {
            tmp = func_0047EF00(src, -0x8000, type);
            return func_0047EF00(&tmp, value + 0x8000, type);
        } else {
            tmp = func_004C3BA0(func_0047F680(value), 0);
            return ps2_binop_inline(0x79, src, &tmp, 0); // daddu
        }
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
        return ps2_binop_inline(4, src, &tmp, 0);
    }
}
#else
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
#endif

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

    pc = makepcode(MACRO_OP(0x82), data_00625A98[0]++, src->reg, width);
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

#if MWCCPS2
// b210: no assert; a full-width mask (64 bits) keeps the operand
inline VOperand clear_high(VOperand* src, SInt32 n) {
    SInt32 width;

    width = 32 - n;
    if (width == 64) {
        return *src;
    }
    return extract_low(src, width);
}
#else
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
#endif

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
#if MWCCPS2 && MWCCPS2_BUILD < 210
        // b145 clears the high word: dsll32 then dsrl32 (by 0, i.e. 32)
        n = data_00625A98[0]++;
        emitpcode(0x85, n, src->reg, 0);
        pc = makepcode(0x8B, data_00625A98[0]++, n, 0);
        goto emit;
#else
        return *src;
#endif
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
#if MWCCPS2 // b210 calls it from every user
__declspec(noinline)
#endif
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
#if MWCCPS2 // b210 calls it from every user
__declspec(noinline)
#endif
VOperand func_0047D240(VOperand* src, CInt64* value, Type* type) {
    VOperand v;

    if (IS_TYPE_LL(type) && !CInt64_IsInRange(*value, 4)) {
        v = func_0047F9F0(*value, type);
        return func_0047F740(0xD4, src, &v, 0);
    }
    return func_0047D480(src, value->lo);
}

// src < value (unsigned), as 0 or 1 in a new register.
#if MWCCPS2 // b210 calls it from every user
__declspec(noinline)
#endif
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
#if MWCCPS2 // b210 calls it from every user
__declspec(noinline)
#endif
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
        return func_0047F740(MACRO_OP(0x83), src, &v, 0);
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
#if MWCCPS2 // b210 calls it from every user
__declspec(noinline)
#endif
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
#if MWCCPS2 // b210 calls it from every user
__declspec(noinline)
#endif
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
            pc = func_00480420(MACRO_OP(0x93), 3);
            pc->args[0] = func_004C3DD0(0);
            pc->args[1] = func_004C3C60(src);
            pc->args[2] = func_004C3D50(imm);
        } else {
            pc = func_00480420(MACRO_OP(0x93), 3);
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
#if MWCCPS2
// b210 has no call to func_00479540 inside this unit: every use is inline
inline static void ps2_branch_inline(UInt16 op, VOperand* src, CLabel* label) {
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
#define func_00479540 ps2_branch_inline
#endif

#if MWCCPS2
// b219 0x479640 / b210 0x486600: an unconditional branch (op) to label that
// ends the block
void func_00479640(UInt16 op, CLabel* label) {
    MPCode* pc;
    MPCodeLabel* fall;

    fall = makepclabel();
    if (data_006242A4 == label) {
        data_00625E83 = 1;
    }
    if (label->pclabel == NULL) {
        label->pclabel = (struct PCodeLabel*)makepclabel();
    }
    pc = func_00480420(op, 1);
    pc->args[0] = func_004C3C00(label);
    appendpcode(data_006246B4, pc);
    pcbranch(data_006246B4, (MPCodeLabel*)label->pclabel, 0);
    pcbranch(data_006246B4, fall, 0);
    makepcblock();
    pclabel(data_006246B4, fall);
}
#endif

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
#if MWCCPS2 // b210 inlines it into both case branches, with both branch helpers inline
inline static void ps2_regbranch_inline(UInt16 op, VOperand* a, VOperand* b, CLabel* label) {
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
inline static void ps2_zbranch_inline(UInt16 op, VOperand* src, CLabel* label) {
    MPCode* pc;
    MPCodeLabel* fall;

    fall = makepclabel();
    if (data_006242A4 == label) {
        data_00625E83 = 1;
    }
    if (label->pclabel == NULL) {
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
inline
#endif
    static void casebranch(VOperand* a, SInt32 imm, CLabel* label, UInt16 op3, UInt16 op2) {
#if MWCCPS2
    VOperand c;

    if (imm) {
        c = func_004C3BA0(func_0047F680(imm), 0);
        ps2_regbranch_inline(op3, a, &c, label);
    } else {
        ps2_zbranch_inline(op2, a, label);
    }
#else
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
#endif
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

#if MWCCPS2
VOperand func_0047CD10(VOperand* src, CInt64* value, Type* type);

// b219 0x47CC30 / b210 0x489E00: src >= value, unsigned: !(src < value)
VOperand func_0047CC30(VOperand* src, CInt64* value, Type* type) {
    VOperand t;

    t = func_0047CD10(src, value, type);
    return func_0047E3D0(&t, 1);
}
#endif

// 0x47CCD0: src >= value, signed: !(src < value)
VOperand func_0047CCD0(VOperand* src, CInt64* value, Type* type) {
    VOperand t;

    t = func_0047D240(src, value, type);
    return func_0047E3D0(&t, 1);
}

// 0x47CD10: src < value, unsigned; a 64-bit constant that does not fit 32
// bits gets an sltu against it
#if MWCCPS2 // b210 calls it from every user
__declspec(noinline)
#endif
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
#if MWCCPS2
#undef CError_PS2_SHIFT
#define CError_PS2_SHIFT -12 // mwccps2: 0x6A3
// b210 (0x486D10): 64-bit registers, so only a 64-bit "extension" is a copy
VOperand func_00479C30(SInt16 op, VOperand* src, SInt32 bits) {
    MPCode* pc;

    CError_ASSERT(1711, op == MACRO_OP(0x81) || op == MACRO_OP(0x82));
    if (bits == 64) {
        return *src;
    }
    pc = makepcode(op, data_00625A98[0]++, src->reg, bits);
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}
#undef CError_PS2_SHIFT
#define CError_PS2_SHIFT 0
#else
VOperand func_00479C30(SInt16 op, VOperand* src, SInt32 bits) {
    MPCode* pc;

    CError_ASSERT(1711, op == MACRO_OP(0x81) || op == MACRO_OP(0x82));
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
#endif

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
    if (pc->op == MACRO_OP(0x7F) || pc->op == 0x1D2 || pc->op == 0x268) {
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
    if (pc->op == MACRO_OP(0x5D) || pc->op == 0x1D1) {
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
#if MWCCPS2
extern VOperand ps2_const128_48E3D0(CInt64 value, Type* type); // load a constant (128-bit CInt64)
void func_0047AE80(VOperand* a, VOperand* b, CLabel* label);
void func_0047B180(VOperand* a, VOperand* b, CLabel* label);
#endif
void func_0047A170(VOperand* a, CInt64* imm, Type* type, CLabel* label) {
    SInt32 hi;
    SInt32 lo;
#if MWCCPS2
    VOperand v;

    // b210 (0x487560): a long long constant goes to a register and the
    // compare is register-register
    if (IS_TYPE_LL(type) && !CInt64_IsInRange(*imm, 4)) {
        v = ps2_const128_48E3D0(*imm, type);
        if (is_unsigned(type)) {
            func_0047AE80(a, &v, label);
        } else {
            func_0047B180(a, &v, label);
        }
        return;
    }
    if (is_unsigned(type)) {
        func_0047B8C0(a, imm->lo, label);
    } else {
        func_0047BF50(a, imm->lo, label);
    }
    return;
#endif

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

#if MWCCPS2 // the accepted beq/bne-constant equivalents keep their call
#undef func_00479540
#endif
// Branch when a (op) imm, for beq/bne against a constant.
inline void branch_eq_const(
    UInt16 op, UInt16 zop, VOperand* a, CInt64* imm, Type* type, CLabel* label
) {
    VOperand t;

    if (IS_TYPE_LL(type) && !CInt64_IsInRange(*imm, 4)) {
#if MWCCPS2 // b210 loads the constant with its 128-bit loader (0x48E3D0)
        t = ps2_const128_48E3D0(*imm, type);
#else
        t = func_0047F9F0(*imm, type);
#endif
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
#if MWCCPS2
#define func_00479540 ps2_branch_inline
#endif

extern MOperand func_004C3D90(SInt32 value);

#if !MWCCPS2 // b210: written above (func_00479640, b210 0x486600)
// 0x479640: end the block with a branch (op) to label alone; the
// fall-through starts a new block
void func_00479640(UInt16 op, CLabel* label) {
    MPCode* pc;
    MPCodeLabel* fall;

    fall = makepclabel();
    if (data_006242A4 == label) {
        data_00625E83 = 1;
    }
    if (!label->pclabel) {
        label->pclabel = (struct PCodeLabel*)makepclabel();
    }
    pc = func_00480420(op, 1);
    pc->args[0] = func_004C3C00(label);
    appendpcode(data_006246B4, pc);
    pcbranch(data_006246B4, (MPCodeLabel*)label->pclabel, 0);
    pcbranch(data_006246B4, fall, 0);
    makepcblock();
    pclabel(data_006246B4, fall);
}
#endif

// 0x479B60: op new, src, imm with relocation kind flags; the new register
VOperand func_00479B60(UInt16 op, VOperand* src, SInt32 imm, SInt32 flags) {
    MPCode* pc;

    pc = func_00480420(op, 3);
    pc->args[0] = func_004C3DD0(0);
    pc->args[1] = func_004C3C60(src);
    pc->unk38 = flags;
    pc->args[2] = func_004C3D90(imm);
    appendpcode(data_006246B4, pc);
    return func_004C3B10(pc->args[0]);
}

// 0x479F10: copy src to a new integer register (op 0xA5, kept: 0x4000)
VOperand func_00479F10(VOperand* src) {
    emitpcode(0xA5, data_00625A98[0]++, src->reg);
    setpcodeflags(0x4000);
    return func_004C3B10(data_006246B4->last->args[0]);
}

#if !MWCCPS2 // b210's twin (0x487210, ps2_tofpr64_487210) moves with dmtc1
// 0x479F70: move src to a new double-precision FP register: two moves of its
// halves when doubles are kept in register pairs (mips_option0F), else one
VOperand func_00479F70(VOperand* src) {
    SInt32 reg;

    reg = data_00625A98[1]++;
    if (copts.mips_option0F) {
        emitpcode(0xAD, src->reg, reg);
        data_006246B4->last->args[1].flags |= 0x8000;
        setpcodeflags(0x100);
        emitpcode(0xAD, src->regHi, reg);
        data_006246B4->last->args[1].flags |= 0xC000;
        setpcodeflags(0x100);
    } else {
        emitpcode(0xB1, src->reg, reg);
    }
    return func_004C3B10(data_006246B4->last->args[1]);
}
#endif

// 0x47BAF0: branch when a > imm (unsigned): never above 0xFFFFFFFF, bnez
// for 0, else beqz on a < imm + 1
void func_0047BAF0(VOperand* a, SInt32 imm, CLabel* label) {
    VOperand t;

    if (imm == -1) {
        return;
    }
    if (imm == 0) {
        func_00479540(0x2A, a, label);
        return;
    }
    t = func_0047CF50(a, imm + 1);
    func_00479540(0x1A, &t, label);
}

// 0x47BD20: branch when a > imm (signed): never above 0x7FFFFFFF, bgtz for
// 0, else beqz on a < imm + 1
void func_0047BD20(VOperand* a, SInt32 imm, CLabel* label) {
    VOperand t;

    if (imm == 0x7FFFFFFF) {
        return;
    }
    if (imm == 0) {
        func_00479540(0x20, a, label);
        return;
    }
    t = func_0047D480(a, imm + 1);
    func_00479540(0x1A, &t, label);
}

#if !MWCCPS2 // b210: written above (func_0047CC30, b210 0x489E00)
// 0x47CC30: src >= value, unsigned: !(src < value)
VOperand func_0047CC30(VOperand* src, CInt64* value, Type* type) {
    VOperand t;

    t = func_0047CD10(src, value, type);
    return func_0047E3D0(&t, 1);
}
#endif
