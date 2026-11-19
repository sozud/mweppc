#include "compiler/mwcc_core.h"
#include "compiler/common.h"
#include "compiler/CError.h"
#include "psp/BE.h"
#include "compiler/InlineAsmMIPS.h"
#include "compiler/objects.h"
#include <string.h>

// mwccpsp 3.0.1 BE_binary.c: turns MIPS PCode into instruction words.

// the opcode table (OpcodeDef in InlineAsmMIPS.h): the operand syntax
// ("d,s,t") and, in unk10, the opcode bits

typedef struct MSwitchTable {
    struct MSwitchTable* next;
    struct Object* object; // the table: label pointers at +0x28, size at +0x2C
} MSwitchTable;

extern SInt32 data_00624DF4; // largest alignment in the function
extern SInt32 data_00603F70; // current byte offset
extern MSwitchTable* data_00624724; // switch tables to emit
extern SInt32* data_006241D8; // receives the instruction count
extern void func_004584C0(SInt32* out, SInt32 fill);
extern void* lalloc(SInt32 size);

// a switch table object's entries (label operands) and size in bytes
#define OBJECT_SWITCH_LABELS(obj) ((MOperand**)(obj)->u.data.u.switchtable.data)
#define OBJECT_SWITCH_SIZE(obj) ((obj)->u.data.u.switchtable.size)
#define SWITCH_LABEL_OFFSET(l) (((MPCodeLabel*)(l)->ptr)->block->codeoffset)

extern SInt32 data_00625208; // index of the instruction being encoded
extern void func_004C5940(int code, ...); // MIPSError_Error
extern void func_004C5890(int code, ...); // MIPSError_Warning

// A label's code offset in bytes.
#define LABEL_OFFSET(v) (((MPCodeLabel*)(v))->block->codeoffset)
// A register operand: the second register of a pair is one higher.
#if MWCCPS2
static inline SInt32 ps2_reg(MOperand* a) {
    SInt32 r;

    r = (SInt16)a->value;
    if (a->flags & 0x4000) {
        r++;
    }
    return r;
}
#define REG(a) ps2_reg(a)
#else
#define REG(a) ((SInt16)(a)->value + (((a)->flags & 0x4000) ? 1 : 0))
#endif

// b210: an operand's 128-bit constant keeps its low word at +0x10
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
// value at +2 (3.0.1 and b210 read the value's low half)
#define ENC_REG(a) ((SInt16)(a)->value)

// b210: an operand's 128-bit constant keeps its low word at +0x10
#endif
#if MWCCPS2
#define ENC_IMM(a) ((a)->value_mid[0])
#else
#define ENC_IMM(a) ((a)->value2)
#endif

#if MWCCPS2 && MWCCPS2_BUILD >= 198 && MWCCPS2_BUILD < 210
// b198 (0x4C2C40, the PS2 b198 decompilation's): its operand cases written as
// b198 has them (its own locals, a for loop; no label word for 'A'/'U', no 'l')
#define B198_FATAL(line) CError_Internal(__FILE__, line)
void BE_EncodeInstruction(MPCode* pc, SInt32 pass) {
    OpcodeDef* info;
    SInt32 value;
    int index;
    char* format;
    Boolean quoted;
    Boolean paired;

    index = 0;
    info = &data_006115F8[pc->op];
    format = info->format;
    pc->word = info->unk10;
    for (; *format; format++) {
        if (*format == ';') {
            break;
        }
        if (*format == '=' || *format == '+') {
            format++;
        }
        if (*format == '!') {
            format++;
        }
        if (*format == '$') {
            format++;
        }
        paired = FALSE;
        quoted = FALSE;
        if (*format == '2') {
            format++;
            if (*format == '+') {
                format++;
            }
            paired = TRUE;
        }
        if (*format == '\'') {
            format++;
            quoted = TRUE;
        }
        switch (*format) {
            case '&':
                pc->word |= pc->args[index].value << 21;
                index++;
                break;
            case '<': {
                int reg;
                reg = pc->args[index].value;
                if (reg > 31) {
                    func_004C5890(31, reg);
                }
                pc->word |= (reg & 31) << 6;
                index++;
                break;
            }
            case '>': {
                int reg;
                reg = pc->args[index].value;
                if (reg > 31) {
                    func_004C5890(31, reg);
                }
                pc->word |= (reg & 31) << 6;
                index++;
                break;
            }
            case '@': {
                int reg;
                if (pc->op == 0x3EF) {
                    reg = pc->args[index].value + (((UInt32)pc->word >> 6) & 31);
                } else {
                    reg = pc->args[index].value;
                }
                pc->word |= ((reg - 1) & 31) << 11;
                index++;
                break;
            }
            case 'a':
                if (pass == 2 && pc->args[index].kind == 6) {
                    value = LABEL_OFFSET(pc->args[index].ptr) / 4;
                    if (pc->flags & 0x4000000) {
                        value++;
                    }
                    pc->word |= value & 0x3FFFFFF;
                }
                break;
            case 'b':
                if (paired && pass == 2 && pc->args[index].kind == 6) {
                    value = LABEL_OFFSET(pc->args[index].ptr) / 8;
                    pc->word |= (value & 0x7FFF) << 6;
                }
                index++;
                break;
            case 'q':
                value = data_00625208 * 4;
                if (!(value & 0x8000) && (value & 0xFFFF0000)) {
                    func_004C5940(14);
                }
                if (value & 0xFFFF0000) {
                    func_004C5940(14);
                }
                pc->word |= value & 0xFFFF;
                break;
            case 'c':
                value = pc->args[index].value << 6;
                pc->word |= value & 0x7FFFF;
                break;
            case 'j':
                if (pc->args[index].kind == 6) {
                    if (pass == 2) {
                        UInt32 high;
                        value = LABEL_OFFSET(pc->args[index].ptr);
                        pc->word |= value & 0xFFFF;
                        high = value >> 16;
                        if (value & 0x8000) {
                            high++;
                        }
                        pc->pair->word |= high & 0xFFFF;
                    }
                } else {
                    pc->word |= (UInt16)ENC_IMM(&pc->args[index]);
                }
                index++;
                break;
            case 'i': {
                SInt32 immediate = ENC_IMM(&pc->args[index]);
                if (paired) {
                    pc->word |= (immediate >> 3) << 6;
                } else {
                    pc->word |= immediate & 0xFFFF;
                }
                index++;
                break;
            }
            case 'k':
                pc->word |= (ENC_IMM(&pc->args[index]) & 31) << 16;
                index++;
                break;
            case 'o':
                if (pc->args[index].kind == 4) {
                    value = pc->args[index].value;
                } else if (pc->args[index].kind == 2) {
                    value = pc->args[index].value;
                } else if (pc->args[index].kind == 6) {
                    B198_FATAL(250);
                } else {
                    B198_FATAL(252);
                }
                pc->word |= value & 0xFFFF;
                index++;
                break;
            case 'p':
                if (pass == 2) {
                    if (pc->unk38 != 10) {
                        if (pc->args[index].kind == 2) {
                            value = ENC_IMM(&pc->args[index]);
                        } else {
                            value = LABEL_OFFSET(pc->args[index].ptr) / 4 - data_00625208 - 1;
                            if (pc->flags & 0x4000000) {
                                value++;
                            }
                            if ((SInt16)value != value) {
                                func_004C5940(14);
                            }
                        }
                    }
                    pc->word |= value & 0xFFFF;
                }
                index++;
                break;
            case 'd':
                pc->word |= (SInt16)pc->args[index].value << 11;
                index++;
                break;
            case 's':
                pc->word |= (SInt16)pc->args[index].value << 21;
                index++;
                break;
            case 'P':
                pc->word |= (SInt16)pc->args[index].value * 2;
                index++;
                break;
            case 'u':
                value = ENC_IMM(&pc->args[index]) >> 16;
                pc->word |= value & 0xFFFF;
                index++;
                break;
            case 'v':
                pc->word |= pc->args[index].value & 15;
                index++;
                break;
            case 'C':
                value = pc->args[index].value;
                pc->word |= value;
                break;
            case 'B':
                value = pc->args[index].value;
                pc->word |= (value & 0xFFFFF) << 6;
                break;
            case 'x':
                if (paired) {
                    pc->word |= (SInt16)pc->args[index].value << 6;
                    index++;
                    break;
                }
                B198_FATAL(341);
            case 'z':
                B198_FATAL(344);
                break;
            case 'D': {
                int reg;
                reg = (SInt16)pc->args[index].value;
                if (pc->args[index].flags & 0x4000) {
                    reg++;
                }
                pc->word |= reg << 6;
                index++;
                break;
            }
            case 'J': {
                int reg;
                reg = (SInt16)pc->args[index].value;
                if (pc->args[index].flags & 0x4000) {
                    reg++;
                }
                pc->word |= reg & 0xFF;
                index++;
                break;
            }
            case 'L': {
                int reg;
                reg = (SInt16)pc->args[index].value;
                if (pc->args[index].flags & 0x4000) {
                    reg++;
                }
                pc->word |= (reg & 0xFF) << 8;
                index++;
                break;
            }
            case 'M': {
                int reg;
                reg = (SInt16)pc->args[index].value;
                if (pc->args[index].flags & 0x4000) {
                    reg++;
                }
                pc->word |= (reg & 0xFF) << 16;
                index++;
                break;
            }
            case 'R': {
                int reg;
                if (!paired) {
                    reg = (SInt16)pc->args[index].value;
                    if (pc->args[index].flags & 0x4000) {
                        reg++;
                    }
                    pc->word |= reg << 21;
                }
                index++;
                break;
            }
            case 'S': {
                int reg;
                if (paired && quoted) {
                    pc->word &= ~0x600000;
                    if (pc->args[index].flags & 0x400) {
                        pc->word |= 0x200000;
                    } else if (pc->args[index].flags & 0x200) {
                        pc->word |= 0x400000;
                    } else if (pc->args[index].flags & 0x100) {
                        pc->word |= 0x600000;
                    }
                }
                reg = (SInt16)pc->args[index].value;
                if (pc->args[index].flags & 0x4000) {
                    reg++;
                }
                pc->word |= reg << 11;
                index++;
                break;
            }
            case 'N':
            case 'T': {
                int reg;
                if (paired && quoted) {
                    pc->word &= ~0x1800000;
                    if (pc->args[index].flags & 0x400) {
                        pc->word |= 0x800000;
                    } else if (pc->args[index].flags & 0x200) {
                        pc->word |= 0x1000000;
                    } else if (pc->args[index].flags & 0x100) {
                        pc->word |= 0x1800000;
                    }
                }
                reg = (SInt16)pc->args[index].value;
                if (pc->args[index].flags & 0x4000) {
                    reg++;
                }
                pc->word |= reg << 16;
                index++;
                break;
            }
            case 'X':
                pc->word |= (pc->args[index].value & 31) << 16;
                index++;
                break;
            case 't': {
                int reg;
                pc->word |= (SInt16)pc->args[index].value << 16;
                index++;
                break;
            }
            case 'W': {
                int reg;
                reg = (SInt16)pc->args[index].value;
                if (pc->args[index].flags & 0x4000) {
                    reg++;
                }
                pc->word |= reg << 16;
                index++;
                break;
            }
            case 'V': {
                int reg;
                reg = (SInt16)pc->args[index].value;
                if (pc->args[index].flags & 0x4000) {
                    reg++;
                }
                pc->word |= reg << 11;
                index++;
                break;
            }
            case 'E': {
                int reg;
                reg = (SInt16)pc->args[index].value;
                if (pc->args[index].flags & 0x4000) {
                    reg++;
                }
                pc->word |= reg << 16;
                index++;
                break;
            }
            case 'G': {
                int reg;
                reg = (SInt16)pc->args[index].value;
                if (pc->args[index].flags & 0x4000) {
                    reg++;
                }
                pc->word |= reg << 11;
                index++;
                break;
            }
            case 'A':
            case 'U':
                index++;
                break;
            case 'I':
                if (!paired) {
                    pc->word |= ENC_IMM(&pc->args[index]);
                }
                index++;
                break;
            case 'Q':
                index++;
                break;
            case 'F':
                B198_FATAL(509);
                break;
            case 'f':
                B198_FATAL(517);
                break;
            case 'l':
                B198_FATAL(521);
                break;
            default:
                B198_FATAL(537);
                break;
            case '(':
            case ')':
            case ',':
            case '-':
            case '9':
            case '|':
            case '}':
                break;
        }
    }
    if (pc->op >= 0x2EF && pc->op <= 0x32E) {
        pc->word |= (((UInt32)pc->word & 0xF800) >> 11) << 21;
    } else if (pc->op >= 0x32F && pc->op <= 0x337) {
        pc->word |= (((UInt32)pc->word & 0x1F0000) >> 16) << 21;
    } else if (pc->op >= 0x338 && pc->op <= 0x353) {
        pc->word |= (((UInt32)pc->word & 0xF800) >> 11) << 16;
    } else if (pc->op >= 0x354 && pc->op <= 0x37D) {
        pc->word |= (((UInt32)pc->word & 0x7C0) >> 6) << 11;
    }
}
#undef B198_FATAL
#else
void BE_EncodeInstruction(MPCode* pc, SInt32 pass) {
    char* fmt;
    SInt32 argi;
    UInt8 wide;
    UInt8 tick;
    SInt32 v;
    SInt32 hi;
    UInt16 op;
#if MWCCPS2 // b210 indexes the operand in every case
#define arg (&pc->args[argi])
#else
    MOperand* arg;
#endif

    argi = 0;
#if MWCCPS2
    {
        OpcodeDef* def = &data_006115F8[pc->op];
        fmt = def->format;
        pc->word = def->unk10;
    }
#else
    fmt = data_006115F8[pc->op].format;
    pc->word = data_006115F8[pc->op].unk10;
#endif

    if (*fmt) {
        do {
            if (*fmt == ';') {
                break;
            }
            if (*fmt == '=' || *fmt == '+') {
                fmt++;
            }
            if (*fmt == '!') {
                fmt++;
            }
            if (*fmt == '$') {
                fmt++;
            }
            wide = 0;
            tick = 0;
            if (*fmt == '2') {
                fmt++;
                if (*fmt == '+') {
                    fmt++;
                }
                wide = 1;
            }
            if (*fmt == '\'') {
                fmt++;
                tick = 1;
            }

#if !MWCCPS2
            arg = &pc->args[argi];
#endif
            switch (*fmt) {
                case '&':
                    pc->word |= arg->value << 21;
                    argi++;
                    break;
                case '<':
#if !MWCCPS2
                    if (wide) {
                        pc->word |= (arg->value & 7) << 18;
                    } else
#endif
                    {
                        v = arg->value;
                        if (v > 31) {
                            func_004C5890(31, v);
                        }
                        pc->word |= (v & 31) << 6;
                    }
                    argi++;
                    break;
                case '>':
#if !MWCCPS2
                    if (wide) {
                        pc->word |= (arg->value & 7) << 16;
                    } else
#endif
                    {
                        v = arg->value;
                        if (v > 31) {
                            func_004C5890(31, v);
                        }
                        pc->word |= (v & 31) << 6;
                    }
                    argi++;
                    break;
                case '@':
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                    if (pc->op == OPC(0x3EF)) {
#else
                    if (pc->op == 0x3EF) {
#endif
                        v = ((pc->word >> 6) & 31) + arg->value;
                    } else {
                        v = arg->value;
                    }
                    pc->word |= ((v - 1) & 31) << 11;
                    argi++;
                    break;
                case 'a':
                    if (pass == 2 && arg->kind == 6) {
                        v = LABEL_OFFSET(arg->ptr) / 4;
                        if (pc->flags & 0x4000000) {
                            v++;
                        }
                        pc->word |= v & 0x3FFFFFF;
                    }
                    break;
                case 'b':
                    if (wide && pass == 2 && arg->kind == 6) {
                        v = LABEL_OFFSET(arg->ptr) / 8;
                        pc->word |= (v & 0x7FFF) << 6;
                    }
                    argi++;
                    break;
                case 'q':
                    v = data_00625208 * 4;
                    if (!(v & 0x8000) && (v & 0xFFFF0000)) {
                        func_004C5940(14);
                    }
                    if (v & 0xFFFF0000) {
                        func_004C5940(14);
                    }
                    pc->word |= (UInt16)v;
                    break;
                case 'c':
                    v = arg->value << 6;
                    pc->word |= v & 0x7FFFF;
                    break;
                case 'j':
                    if (arg->kind == 6) {
                        if (pass == 2) {
                            v = LABEL_OFFSET(arg->ptr);
                            pc->word |= (UInt16)v;
                            hi = v >> 16;
                            if (v & 0x8000) {
                                hi++;
                            }
                            pc->pair->word |= (UInt16)hi;
                        }
                    } else {
                        pc->word |= (UInt16)ENC_IMM(arg);
                    }
                    argi++;
                    break;
#if !MWCCPS2
                case 'e':
                    pc->word |= arg->value << 8;
                    argi++;
                    break;
#endif
                case 'i':
#if MWCCPS2
                    v = ENC_IMM(arg);
                    if (wide) {
                        pc->word |= (v >> 3) << 6;
                    } else {
                        pc->word |= (UInt16)v;
                    }
#else
                    pc->word |= (UInt16)ENC_IMM(arg);
#endif
                    argi++;
                    break;
                case 'k':
#if MWCCPS2
                    pc->word |= (ENC_IMM(arg) & 31) << 16;
#else
                    if (wide) {
                        pc->word |= (UInt8)arg->value2 << 16;
                    } else {
                        pc->word |= (arg->value2 & 31) << 16;
                    }
#endif
                    argi++;
                    break;
                case 'o':
                    if (arg->kind == 4) {
                        v = arg->value;
                    } else if (arg->kind == 2) {
                        v = arg->value;
                    } else if (arg->kind == 6) {
                        CError_FATAL(250);
                    } else {
                        CError_FATAL(252);
                    }
#if !MWCCPS2
                    if (wide) {
                        pc->word |= v & 0xFFFC;
                    } else
#endif
                    {
                        pc->word |= (UInt16)v;
                    }
                    argi++;
                    break;
                case 'p':
                    if (pass == 2) {
                        if (pc->unk38 != 10) {
                            if (arg->kind == 2) {
                                v = ENC_IMM(arg);
                            } else {
                                v = LABEL_OFFSET(arg->ptr) / 4 - data_00625208 - 1;
                                if (pc->flags & 0x4000000) {
                                    v++;
                                }
                                if ((SInt16)v != v) {
                                    func_004C5940(14);
                                }
                            }
                        }
                        pc->word |= (UInt16)v;
                    }
                    argi++;
                    break;
                case 'd':
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                    pc->word |= ENC_REG(arg) << 11;
#else
                    pc->word |= (SInt16)arg->value << 11;
#endif
                    argi++;
                    break;
                case 's':
#if !MWCCPS2
                    if (wide) {
                        pc->word |= ((SInt16)arg->value << 21) & 0x7E00000;
                    } else
#endif
                    {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                        pc->word |= ENC_REG(arg) << 21;
#else
                    pc->word |= (SInt16)arg->value << 21;
#endif
                    }
                    argi++;
                    break;
                case 'P':
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                    pc->word |= ENC_REG(arg) * 2;
#else
                    pc->word |= (SInt16)arg->value * 2;
#endif
                    argi++;
                    break;
                case 'u':
                    pc->word |= (UInt16)(ENC_IMM(arg) >> 16);
                    argi++;
                    break;
                case 'v':
                    pc->word |= arg->value & 15;
                    argi++;
                    break;
                case 'C':
                    pc->word |= arg->value;
                    break;
                case 'B':
                    pc->word |= (arg->value & 0xFFFFF) << 6;
                    break;
                case 'x':
                    if (wide) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                        pc->word |= ENC_REG(arg) << 6;
#else
                        pc->word |= (SInt16)arg->value << 6;
#endif
                        argi++;
                        break;
                    }
                    CError_FATAL(341);
                case 'z':
                    CError_FATAL(344);
                    break;
                case 'D':
#if !MWCCPS2
                    if (wide) {
                        pc->word |= REG(arg) & 0x7F;
                    } else
#endif
                    {
                        pc->word |= REG(arg) << 6;
                    }
                    argi++;
                    break;
                case 'J':
                    pc->word |= (UInt8)REG(arg);
                    argi++;
                    break;
                case 'L':
                    pc->word |= (UInt8)REG(arg) << 8;
                    argi++;
                    break;
                case 'M':
                    pc->word |= (UInt8)REG(arg) << 16;
                    argi++;
                    break;
                case 'R':
                    if (!wide) {
                        pc->word |= REG(arg) << 21;
                    }
                    argi++;
                    break;
                case 'S':
#if MWCCPS2 // b210: a variant selector in bits 21-22 (cf. 'N')
                    if (wide && tick) {
                        pc->word &= 0xFF9FFFFF;
                        if (arg->flags & 0x400) {
                            pc->word |= 0x200000;
                        } else if (arg->flags & 0x200) {
                            pc->word |= 0x400000;
                        } else if (arg->flags & 0x100) {
                            pc->word |= 0x600000;
                        }
                    }
                    pc->word |= REG(arg) << 11;
#else
                    if (wide) {
                        pc->word |= (REG(arg) & 0x7F) << 8;
                    } else {
                        pc->word |= REG(arg) << 11;
                    }
#endif
                    argi++;
                    break;
                case 'T':
#if !MWCCPS2
                    if (wide) {
                        v = REG(arg);
                        if ((UInt16)(pc->op - 0x4CC) <= 4) {
                            pc->word &= ~1;
                            pc->word |= (v & 0x20) >> 5;
                        } else {
                            pc->word &= ~3;
                            pc->word |= (v & 0x60) >> 5;
                        }
                        pc->word |= (v & 31) << 16;
                        argi++;
                        break;
                    }
#endif
                case 'N':
                    if (wide && tick) {
                        pc->word &= 0xFE7FFFFF;
                        if (arg->flags & 0x400) {
                            pc->word |= 0x800000;
                        } else if (arg->flags & 0x200) {
                            pc->word |= 0x1000000;
                        } else if (arg->flags & 0x100) {
                            pc->word |= 0x1800000;
                        }
                    }
                    pc->word |= REG(arg) << 16;
                    argi++;
                    break;
                case 'X':
                    pc->word |= (arg->value & 31) << 16;
                    argi++;
                    break;
                case 't':
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                    pc->word |= ENC_REG(arg) << 16;
#else
                    pc->word |= (SInt16)arg->value << 16;
#endif
                    argi++;
                    break;
                case 'W':
                    pc->word |= REG(arg) << 16;
                    argi++;
                    break;
                case 'V':
                    pc->word |= REG(arg) << 11;
                    argi++;
                    break;
                case 'E':
#if !MWCCPS2
                    if (wide) {
                        pc->word |= (SInt16)arg->value << 16;
                    } else
#endif
                    {
                        pc->word |= REG(arg) << 16;
                    }
                    argi++;
                    break;
                case 'G':
                    pc->word |= REG(arg) << 11;
                    argi++;
                    break;
                case 'A':
                case 'U':
#if !MWCCPS2 || MWCCPS2_BUILD >= 198 // b145 encodes no label word here
                    if (pass == 2 && arg->kind == 6) {
                        pc->word = LABEL_OFFSET(arg->ptr);
                    }
#endif
                    argi++;
                    break;
                case 'I':
                    if (!wide) {
                        pc->word |= ENC_IMM(arg);
                    }
                    argi++;
                    break;
                case 'Q':
                    argi++;
                    break;
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -5 // b145 asserts
                case 'F':
                    CError_FATAL(516);
                    break;
                case 'f':
                    CError_FATAL(524);
                    break;
                case 'l':
#if MWCCPS2 && MWCCPS2_BUILD < 198
                    CError_FATAL(528); // b145: no 'l' operands
#else
                    if (pass == 2 && arg->kind == 6) {
                        pc->word = LABEL_OFFSET(arg->ptr);
                    }
#endif
                    break;
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -11 // b145 asserts
                case '(':
                case ')':
                case ',':
                case '-':
                case '9':
                case '|':
                case '}':
                    break;
                default:
                    CError_FATAL(550);
                    break;
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
            }
            fmt++;
        } while (*fmt);
    }

    op = pc->op;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    if (op >= OPC(0x2EF) && op <= OPC(0x32E)) {
#else
    if (op >= 0x2EF && op <= 0x32E) {
#endif
        pc->word |= ((pc->word & 0xF800) >> 11) << 21;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    } else if (op >= OPC(0x32F) && op <= OPC(0x337)) {
#else
    } else if (op >= 0x32F && op <= 0x337) {
#endif
        pc->word |= ((pc->word & 0x1F0000) >> 16) << 21;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    } else if (op >= OPC(0x338) && op <= OPC(0x353)) {
#else
    } else if (op >= 0x338 && op <= 0x353) {
#endif
        pc->word |= ((pc->word & 0xF800) >> 11) << 16;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    } else if (op >= OPC(0x354) && op <= OPC(0x37D)) {
#else
    } else if (op >= 0x354 && op <= 0x37D) {
#endif
        pc->word |= ((pc->word & 0x7C0) >> 6) << 11;
    }
}
#endif

// Final pass for one instruction: branches whose target is out of 16-bit
// range become the long form, then instructions with label operands are
// encoded again with the final offsets.
#undef arg

void BE_EncodeFinal(MPCode* pc) {
    SInt32 d;
    char* label;

    if (pc->flags & 1) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        if (pc->op == OPC(0x39)) {
#else
        if (pc->op == 0x39) {
#endif
            label = pc->args[0].ptr;
            if (label && ((MPCodeLabel*)label)->block) {
                d = LABEL_OFFSET(label) / 4 - data_00625208 - 1;
                if (d < -0x8000 || d > 0x7FFF) {
                    pc->op = 0x3A;
                    pc->unk38 = 4;
                }
            }
        }
        BE_EncodeInstruction(pc, 2);
    } else if (pc->op == 3 && pc->args[2].kind == 6) {
        BE_EncodeInstruction(pc, 2);
#if !MWCCPS2 || MWCCPS2_BUILD >= 210 // b145 and b198 have no second case
    } else if (pc->op == 0x2EE && pc->args[0].kind == 6) {
        BE_EncodeInstruction(pc, 2);
#endif
    }
    data_00625208++;
}

// First pass: encode every instruction; an instruction flagged 0x40000000 is
// encoded last in its block.
void BE_EncodeAll(void) {
    MPCodeBlock* block;
    MPCode* pc;
    MPCode* last;

    for (block = data_00624FD0; block; block = block->next) {
        last = NULL;
        for (pc = block->first; pc; pc = pc->next) {
            if (pc->flags & 0x40000000) {
                last = pc;
            } else if (pc->op < BE_NUM_OPCODES) {
                BE_EncodeInstruction(pc, 1);
            }
        }
        if (last) {
            BE_EncodeInstruction(last, 1);
        }
    }
}

static void BE_CountInstruction(MPCode* pc) {
    pc->offset = data_00603F70;
    if (pc->op < BE_NUM_OPCODES) {
        data_00625208++;
        data_00603F70 += 4;
    } else if (pc->op < MACRO_OP(0xAD)) {
        func_004C5940(15);
    }
}

// Lay out the function (offsets, alignment padding), emit its switch tables,
// then encode every instruction with the final offsets.
// b210: an operand's 128-bit constant keeps its low word at +0x10
#if MWCCPS2
#define LAYOUT_IMM(op) ((op).value_mid[0])
#else
#define LAYOUT_IMM(op) ((op).value2)
#endif

#undef CError_B145_SHIFT
#define CError_B145_SHIFT -17 // b145 asserts
#if MWCC_CORE == MWCC_CORE_B52
#undef CError_B52_SHIFT
#define CError_B52_SHIFT (-182)
#endif
void BE_LayoutAndEncode(struct Object* func) {
    MPCodeBlock* block;
    MPCode* pc;
    MPCode* deferred;
    MPCode* cur;
    MPCode* nop;
    MSwitchTable* table;
    struct Object* tobj;
    ObjDataReloc* relocs;
    ObjDataReloc* r;
    MOperand** labels;
    void* buf;
    SInt32 fill;
#if MWCCPS2
    CInt64 fillvalue;
#else
    SInt32 fillvalue[2];
#endif
    SInt32 align;
    SInt32 n;
    SInt32 off;
    UInt32 b;

    data_00624DF4 = 0;
    data_00625208 = 0;
    data_00603F70 = data_00625208;
    for (block = data_00624FD0; block; block = block->next) {
        block->codeoffset = data_00603F70;
        deferred = NULL;
        pc = block->first;
        while (pc) {
            if (pc->flags & 0x40000000) {
                deferred = pc;
                pc = pc->next;
            } else if (pc->flags & 0x100000) {
                cur = pc;
                switch (pc->op) {
                    case MACRO_OP(0xA7):
                    case MACRO_OP(0xA9):
                    case MACRO_OP(0xAB):
                        fill = 0;
                        break;
                    case MACRO_OP(0xA6):
                        b = (UInt8)LAYOUT_IMM(pc->args[1]);
                        fill = b | (b << 16) | (b << 24) | (b << 8);
                        break;
                    case MACRO_OP(0xA8):
                        b = (UInt16)LAYOUT_IMM(pc->args[1]);
                        fill = b | (b << 16);
                        break;
                    case MACRO_OP(0xAA):
                        fill = LAYOUT_IMM(pc->args[1]);
                        break;
                    default:
                        CError_FATAL(718);
                        BE_CountInstruction(pc);
                        pc = pc->next;
                        continue;
                }
#if MWCCPS2
                CInt64_SetLong(&fillvalue, fill);
#else
                func_004584C0(fillvalue, fill);
#endif
                if (LAYOUT_IMM(pc->args[0])) {
                    align = BIT32(LAYOUT_IMM(pc->args[0]));
                    if (align > data_00624DF4) {
                        data_00624DF4 = align;
                    }
                    while (data_00603F70 & (align - 1)) {
#if MWCC_CORE == MWCC_CORE_B52
                        if (pc == block->first) {
                            block->codeoffset += 4;
                        }
#endif
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                        nop = makepcode(OPC(0x2ED), fillvalue);
#else
                        if (pc == block->first) {
                            block->codeoffset += 4;
                        }
#endif
#if MWCCPS2
#if !MWCC_CORE || MWCC_CORE > MWCC_CORE_B52
                        nop = makepcode(0x2ED, fillvalue);
#endif
#else
                        nop = makepcode(0x2ED, fillvalue[0], fillvalue[1]);
#endif
                        insertpcodebefore(pc, nop);
                        BE_CountInstruction(nop);
                    }
                }
                pc = pc->next;
                deletepcode(cur);
            } else {
                BE_CountInstruction(pc);
                pc = pc->next;
            }
        }
        if (deferred) {
            BE_CountInstruction(deferred);
        }
    }

    for (table = data_00624724; table; table = table->next) {
        tobj = table->object;
        labels = OBJECT_SWITCH_LABELS(tobj);
        n = OBJECT_SWITCH_SIZE(tobj) / 4;
        relocs = NULL;
        off = 0;
        while (n) {
            r = lalloc(sizeof(ObjDataReloc));
            r->addend = SWITCH_LABEL_OFFSET(*labels);
            r->target = func;
            r->offset = off;
            off += 4;
            r->next = relocs;
            labels++;
            n--;
            relocs = r;
        }
        buf = lalloc(OBJECT_SWITCH_SIZE(tobj));
        memset(buf, 0, OBJECT_SWITCH_SIZE(tobj));
        ObjGen_EmitData(tobj, buf, relocs, OBJECT_SWITCH_SIZE(tobj));
    }
    data_00624724 = NULL;

    data_00625208 = 0;
    for (block = data_00624FD0; block; block = block->next) {
        deferred = NULL;
        for (pc = block->first; pc; pc = pc->next) {
            if (pc->flags & 0x40000000) {
                deferred = pc;
            } else {
                BE_EncodeFinal(pc);
            }
        }
        if (deferred) {
            BE_EncodeFinal(deferred);
        }
    }
    *data_006241D8 = data_00625208;
}
#if MWCC_CORE == MWCC_CORE_B52
#undef CError_B52_SHIFT
#define CError_B52_SHIFT (0)
#endif
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

#if MWCCPS2
// b210 #pragma padloop (copts+0x40): a short loop (fewer than seven
// instructions from its branch target to its end) is padded with nops up to
// seven, the last of which is marked to stay last (0x40000000).
#include "psp/Loop301.h"

extern MOperand* func_0048FAE0(MPCode* pc); // a branch's label operand
extern void func_0054AD10(void (*func)(Loop301*));

static inline Boolean ps2_isbranch(MPCode* pc) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    return (pc->flags & 0x10000000) ||
        pc->op == OPC(0x39) ||
        pc->op == OPC(0x38A) ||
        pc->op == OPC(0x38B);
#else
    return (pc->flags & 0x10000000) || pc->op == 0x39 || pc->op == 0x38A || pc->op == 0x38B;
#endif
}

// 0x4C2960
void ps2_padloop_4C2960(MPCodeBlock* block) {
    MPCode* last;
    int n;
    int count;
    MPCodeBlock* target;
    MPCodeBlock* b;
    MPCode* q;

    n = 0;
    count = 0;
    last = block->last;
    if (block->flags2E & 0x4000) {
        return;
    }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    if (!last ||
        (!(last->flags & 0x10000000) &&
         last->op != OPC(0x39) &&
         last->op != OPC(0x38A) &&
         last->op != OPC(0x38B))) {
#else
    if (!last ||
        (!(last->flags & 0x10000000) &&
         last->op != 0x39 &&
         last->op != 0x38A &&
         last->op != 0x38B)) {
#endif
        return;
    }
    {
        target = ((MPCodeLabel*)func_0048FAE0(last)->ptr)->block;
        b = target;
        for (;;) {
            if (b->number == block->number) {
                break;
            }
            b = b->next;
            if (!b) {
                break;
            }
            n++;
            if (n > 2 || n < -2) {
                return;
            }
        }
        b = block;
        while (target != b) {
            count += b->pcodecount;
#if MWCC_CORE == MWCC_CORE_B52
            for (q = b->first; q; q = q->next) {
                if (q->flags & 0x100000) {
                    count--;
                }
            }
#endif
#if !MWCC_CORE || MWCC_CORE > MWCC_CORE_B52
            for (q = b->first; q; q = q->next) {
                if (q->flags & 0x100000) {
                    count--;
                }
            }
#endif
            b = b->prev;
            if (!b) {
                break;
            }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
#if MWCC_CORE != MWCC_CORE_B52
            if (b->last && (b->last->flags & 0x100000)) {
                count--;
            }
#endif
#endif
            if (count >= 7) {
                return;
            }
        }
        if (b) {
            count += b->pcodecount;
#if MWCC_CORE == MWCC_CORE_B52
            for (q = b->first; q; q = q->next) {
                if (q->flags & 0x100000) {
                    count--;
#endif
#if !MWCC_CORE || MWCC_CORE > MWCC_CORE_B52
                    for (q = b->first; q; q = q->next) {
                        if (q->flags & 0x100000) {
                            count--;
#endif
                        }
#if MWCC_CORE == MWCC_CORE_B52
                    }
                }
#endif
#if !MWCC_CORE || MWCC_CORE > MWCC_CORE_B52
            }
        }
#endif
        if (count >= 7) {
            return;
        }
        q = last->prev;
        if (q && (q->flags & 0x40000000)) {
            q->flags &= ~0x40000000;
        }
        while (count < 7) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            insertpcodebefore(last, makepcode(OPC(0xBA)));
#else
            insertpcodebefore(last, makepcode(0xBA));
#endif
            count++;
        }
        if (last->prev) {
            last->prev->flags |= 0x40000000;
        }
    }
}

// 0x4C2AA0
static void ps2_padloop_cb(Loop301* loop) {
    if (!loop->exits) {
        return;
    }
    ps2_padloop_4C2960(loop->exits->block);
}

// 0x4C2950
void ps2_padloop_4C2950(Object* func) {
    func_0054AD10(ps2_padloop_cb);
}
// b210 #pragma dividefix (copts+0x3E): R5900 multiply/divide hazards. A
// div/mult (0xF3, 0x101, 0x125, 0x366, 0x37A) right after an mflo/mfhi
// (0xDF) gets a nop between them; in a block ending in a branch-likely
// (flags & 1) it needs three more instructions after it, and near the top
// of a block that follows its predecessor it needs three before it.
static MPCodeBlock* ps2_dividefix_prev; // 0x61B8E8

// 0x4C4960
static void ps2_dividefix_block(MPCodeBlock* block) {
    MPCode* pc;
    UInt8 follows;
    UInt8 likely;
    int lastmf;
    int i;

    pc = block->last;
    follows = 0;
    if (block->prev == ps2_dividefix_prev) {
        follows = 1;
    }
    likely = pc->flags & 1;
    lastmf = -1;
    for (pc = block->first, i = 1; pc; pc = pc->next, i++) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        if (pc->op == OPC(0xF3) ||
            pc->op == OPC(0x366) ||
            pc->op == OPC(0x101) ||
            pc->op == OPC(0x37A) ||
            pc->op == OPC(0x125)) {
#else
        if (pc->op == 0xF3 ||
            pc->op == 0x366 ||
            pc->op == 0x101 ||
            pc->op == 0x37A ||
            pc->op == 0x125) {
#endif
            if (lastmf == i - 1) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                insertpcodebefore(pc, makepcode(OPC(0xBA)));
#else
                insertpcodebefore(pc, makepcode(0xBA));
#endif
                i++;
            }
            if (likely && block->pcodecount - i <= 3) {
                while (block->pcodecount - i <= 3) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                    insertpcodeafter(pc, makepcode(OPC(0xBA)));
#else
                    insertpcodeafter(pc, makepcode(0xBA));
#endif
                }
            }
            if (follows && i < 3) {
                while (i < 3) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                    insertpcodebefore(pc, makepcode(OPC(0xBA)));
#else
                    insertpcodebefore(pc, makepcode(0xBA));
#endif
                    i++;
                }
            }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        } else if (pc->op == OPC(0xDF)) {
#else
        } else if (pc->op == 0xDF) {
#endif
            lastmf = i;
        }
    }
}

// 0x4C4910
void ps2_dividefix_4C4910(Object* func) {
    MPCodeBlock* block;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    MPCodeBlock* b69_dead;
#endif

    ps2_dividefix_prev = NULL;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    for (b69_dead = block = data_00624FD0; block; b69_dead = block = block->next) {
#if MWCC_CORE == MWCC_CORE_B52
        if (block->pcodecount && !(block->flags2E & 0x80) && !(block->flags2E & 0x4000)) {
#else
        if (block->pcodecount && !(block->flags2E & 0x4000)) {
#endif
#else
    for (block = data_00624FD0; block; block = block->next) {
        if (block->pcodecount && !(block->flags2E & 0x80) && !(block->flags2E & 0x4000)) {
#endif
            ps2_dividefix_block(block);
        }
        ps2_dividefix_prev = block;
    }
}
#endif
