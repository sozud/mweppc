#include "compiler/common.h"
#include <stdarg.h>
#include "psp/BE.h"
#include "compiler/InlineAsmMIPS.h"
#include "compiler/CError.h"
#include "compiler/CInt64.h"
#include "compiler/CParser.h"
#include "compiler/CompilerTools.h"
#include "compiler/objects.h"

// mwccpsp 3.0.1 PCodeInfo.c (MIPS; 2.4.7's is PowerPC and stays out of the
// build's checks): operand queries and formatting.

// A branch's label operand.
MOperand* func_0048FAE0(MPCode* pc) {
    int i;
    MOperand* op;

    for (i = 0, op = pc->args; i < pc->argcount; i++, op++) {
        if (op->kind == 6) {
            return op;
        }
    }
    CError_Internal("PCodeInfo.c", 1509);
    return NULL;
}

// The opcode of a nop.
SInt16 func_004903F0(void) {
    return 0xBA;
}

// the opcode table (OpcodeDef in InlineAsmMIPS.h): format, nargs, and flags
// (6: loads and stores)

// How many bytes the load or store accesses.
SInt32 func_0048FBE0(UInt16 op) {
    if (!(data_006115F8[op].flags & 6)) {
        CError_FATAL(1384);
        return 0;
    }
    switch (op) {
        case 0x91:
        case 0x92:
        case 0xC3:
        case 0x561:
        case 0x568:
        case 0x58D:
            return 1;
        case 0x94:
        case 0x95:
        case 0xCA:
        case 0x569:
        case 0x56A:
        case 0x58E:
            return 2;
        case 0x9B:
        case 0x9D:
        case 0x9E:
        case 0x9F:
        case 0xA0:
        case 0xA1:
        case 0xA2:
        case 0xA3:
        case 0xC1:
        case 0xCB:
        case 0xCC:
        case 0xCD:
        case 0xCE:
        case 0xCF:
        case 0xD0:
        case 0xD1:
        case 0x56B:
        case 0x56D:
        case 0x56E:
        case 0x56F:
        case 0x570:
        case 0x571:
        case 0x572:
        case 0x573:
        case 0x574:
        case 0x585:
        case 0x58F:
        case 0x590:
        case 0x591:
        case 0x592:
        case 0x593:
        case 0x594:
        case 0x595:
        case 0x596:
            return 4;
        case 0x93:
        case 0x96:
        case 0x97:
        case 0x98:
        case 0x99:
        case 0x9A:
        case 0x9C:
        case 0xC2:
        case 0xC4:
        case 0xC5:
        case 0xC6:
        case 0xC7:
        case 0xC8:
        case 0xC9:
        case 0x562:
        case 0x563:
        case 0x564:
        case 0x565:
        case 0x566:
        case 0x567:
        case 0x56C:
        case 0x586:
        case 0x587:
        case 0x588:
        case 0x589:
        case 0x58A:
        case 0x58B:
        case 0x58C:
            return 8;
        case 0x10B:
        case 0x10D:
            return 4;
        case 0x10A:
        case 0x10C:
            return 8;
        case 0x164:
        case 0x173:
            return 8;
        case 0x41B:
        case 0x41C:
        case 0x5B3:
        case 0x5B5:
            return 4;
        case 0x4CC:
        case 0x4CD:
        case 0x4CE:
        case 0x4CF:
        case 0x4D0:
        case 0x5B4:
        case 0x5B6:
        case 0x5B7:
        case 0x5B8:
        case 0x5B9:
            return 16;
        default:
            CError_FATAL(1380);
            return 0;
    }
}

extern SInt16 data_00625E48; // move
extern SInt16 data_00625E74; // 16-byte register move

// An instruction copying register operand src to dst.
MPCode* func_00490280(MOperand* src, MOperand* dst) {
    if (src->kind == 0) {
        if (src->rclass == 0) {
            return makepcode(
                (src->flags & 0x2000) ? data_00625E74 : data_00625E48, dst->reg, src->reg
            );
        } else if (src->rclass == 1) {
            return makepcode((src->flags & 0x8000) ? 0x5B2 : 0x5B1, dst->reg, src->reg);
        } else if (src->rclass == 5) {
            return makepcode(0x2AF, 0xF, dst->reg, src->reg);
        } else if (src->rclass == 3) {
            return makepcode(0x281, dst->reg, src->reg, 0);
        }
    }
    CError_FATAL(979);
    return NULL;
}

// A copy of kind 1 (a register move) or 2..7 (0x59A / 0x599 with 8, 16 or
// 32).
MPCode* func_0048FDC0(int kind, MOperand* src, MOperand* dst) {
    switch (kind) {
        case 1:
            return func_00490280(src, dst);
        case 2:
            return makepcode(0x59A, dst->reg, src->reg, 8);
        case 3:
            return makepcode(0x599, dst->reg, src->reg, 8);
        case 4:
            return makepcode(0x59A, dst->reg, src->reg, 0x10);
        case 5:
            return makepcode(0x599, dst->reg, src->reg, 0x10);
        case 6:
            return makepcode(0x59A, dst->reg, src->reg, 0x20);
        case 7:
            return makepcode(0x599, dst->reg, src->reg, 0x20);
        default:
            CError_FATAL(1183);
            return NULL;
    }
}

extern SInt32 data_00624788;

// 0x48FB20: the move kind (func_0048FDC0's) of a load or store's value; 0
// when it has none.
int func_0048FB20(MPCode* pc) {
    switch (pc->op) {
        case 0x91:
        case 0x568:
            return 3;
        case 0x92:
        case 0x561:
            return 2;
        case 0x93:
        case 0x99:
        case 0x9A:
            return 1;
        case 0x94:
        case 0x569:
            return 5;
        case 0x95:
        case 0x56A:
            return 4;
        case 0x9D:
        case 0x56D:
            return (data_00624788 == 4) ? 1 : 7;
        case 0xA0:
        case 0x574:
            return (data_00624788 == 4) ? 1 : 6;
        default:
            return 0;
    }
}

extern SInt32 func_0048FBE0(UInt16 op); // bytes an opcode moves

// 0x48FFA0: how load can take the value store wrote (func_0048FDC0's kind,
// 0 when it cannot)
SInt32 func_0048FFA0(MPCode* store, MPCode* load) {
    SInt32 size = func_0048FBE0(store->op);

    if (size > 16) {
        return 0;
    }
    // copts +0x3E (named function_align16 for ObjGen) also allows the
    // 16-byte register pairs here
    if (copts.function_align16) {
        if (store->op == 0x1D2 && load->op == 0x1D1) {
            return 1;
        }
        if (store->op == 0x597 && load->op == 0x575) {
            return 1;
        }
    }
    if (size > 8) {
        return 0;
    }
    if (size > 4) {
        return 0;
    }
    if (store->op == 0xCD && load->op == 0xA1) {
        return 1;
    }
    if (store->op == 0x591 && load->op == 0x56E) {
        return 1;
    }
    if (store->op == 0xCB && load->op == 0x9D) {
        return 1;
    }
    if (store->op == 0x58F && load->op == 0x56D) {
        return 1;
    }
    if (store->op == 0xCB && load->op == 0x94) {
        return 5;
    }
    if (store->op == 0x58F && load->op == 0x569) {
        return 5;
    }
    if (store->op == 0xCA && load->op == 0x94) {
        return 5;
    }
    if (store->op == 0x58E && load->op == 0x569) {
        return 5;
    }
    if (store->op == 0xC3 && load->op == 0x91) {
        return 3;
    }
    if (store->op == 0x58D && load->op == 0x568) {
        return 3;
    }
    if (store->op == 0xCB && load->op == 0x91) {
        return 3;
    }
    if (store->op == 0x58F && load->op == 0x568) {
        return 3;
    }
    if (store->op == 0xCA && load->op == 0x91) {
        return 3;
    }
    if (store->op == 0x58E && load->op == 0x568) {
        return 3;
    }
    return 0;
}

extern SInt16 data_00625E3E; // frame register

// Whether the instruction just copies or makes a value it can be dropped or
// repeated for: x = $zero op imm, a nop-like 0x90 / 0x55E, or 0x55C off $gp or
// the frame register.
int func_004901D0(MPCode* pc) {
    if (pc->flags & 0x100) {
        return 0;
    }
    if (pc->unk38) {
        return 0;
    }
    if (pc->argcount == 3 &&
        (pc->args[2].kind == 2 || pc->args[2].kind == 3) &&
        pc->args[1].kind == 0 &&
        pc->args[1].rclass == 0 &&
        pc->args[1].reg == 0) {
        return 1;
    }
    switch (pc->op) {
        case 0x90:
        case 0x55E:
            return 1;
        case 0x55C:
            if (pc->args[2].kind == 0 && pc->args[1].rclass == 0) {
                if (pc->args[2].reg == 28) {
                    return 1;
                }
                if (pc->args[2].reg == data_00625E3E) {
                    return 1;
                }
            }
            return 0;
        default:
            return 0;
    }
}

// In a heavily weighted (loop) block, emit 0x5C3 with the configured value
// (from the CodeGen panel) unless optimizing for size.
void func_00490370(void) {
    CInt64 n;

    if (copts.optimizesize || !copts.mips_option3A) {
        return;
    }
    if (data_006246B4->weight < 8) {
        return;
    }
    if ((SInt8)copts.mips_option39 > 2) {
        CInt64_SetLong(&n, (SInt8)copts.mips_option39);
        emitpcode(0x5C3, n);
        data_006246B4->last->flags |= 0x100;
    }
}

extern struct Alias* make_alias(Object* obj, SInt32 offset, SInt32 size);
extern Boolean is_volatile_object(Object* obj);
extern Boolean is_const_object(Object* obj);

// vformatpcode: build an instruction from its opcode's operand format string
// and the arguments.
MPCode* func_00490400(UInt16 op, va_list args) {
    OpcodeDef* info;
    MPCode* pc;
    char pair;
    char masked;
    SInt32 count;
    const char* format;
    MOperand* arg;
    int effect;
    SInt32 mask;
    SInt32 size;

    info = &data_006115F8[op];
    format = info->format;
    count = info->nargs;
    pair = 0;
    masked = 0;
    if (*format == '#') {
        count += va_arg(args, int);
        format++;
    }
    size = PCODE_SIZE(count);
    pc = lalloc(size);
    memclrw(pc, size);
    pc->op = op;
    pc->argcount = count;
    pc->flags = info->flags;
    arg = pc->args;
    while (*format) {
        if (*format == ',' || *format == ';' || *format == '(') {
            format++;
        }
        effect = 1;
        if (*format == '=') {
            effect = 2;
            format++;
        } else if (*format == '+') {
            effect = 3;
            format++;
        }
        if (*format == '!') {
            effect |= 0x8000;
            format++;
            if (copts.mips_option1A) {
                effect |= 0x20;
            }
        }
        if (*format == '$') {
            effect |= 0x2000;
            format++;
        }
        if (*format == '2') {
            format++;
            pair = 1;
            if (*format == '+') {
                format++;
            }
            if (*format == '\'') {
                format++;
                masked = 1;
            }
        }
        switch (*format) {
            case ')':
                format++;
                break;
            case 'b':
            case 'e':
            case 'v':
                arg->kind = 2;
                arg->value = va_arg(args, int);
                format++;
                break;
            case 'P':
            case 'd':
            case 's':
            case 't':
                arg->kind = 0;
                arg->rclass = 0;
                arg->reg = va_arg(args, int);
                format++;
                arg->flags = effect;
                break;
            case '9':
                arg->kind = 0;
                arg->rclass = 3;
                arg->reg = va_arg(args, int);
                format++;
                arg->flags = effect;
                break;
            case 'D':
            case 'G':
            case 'N':
            case 'S':
            case 'T':
                arg->kind = 0;
                arg->rclass = pair ? (char)5 : (char)1;
                if (masked) {
                    mask = va_arg(args, int);
                    if (mask & 8) {
                        effect |= 0x800;
                    }
                    if (mask & 4) {
                        effect |= 0x400;
                    }
                    if (mask & 2) {
                        effect |= 0x200;
                    }
                    if (mask & 1) {
                        effect |= 0x100;
                    }
                    if (mask < 15) {
                        pc->flags &= ~0x4010;
                    }
                }
                arg->flags = effect;
                arg->reg = va_arg(args, int);
                format++;
                break;
            case 'J':
            case 'L':
                arg->kind = 0;
                arg->rclass = 6;
                arg->flags = effect;
                arg->reg = va_arg(args, int);
                format++;
                break;
            case 'j':
                arg->kind = 3;
                CInt64_SetLong((CInt64*)&arg->value, va_arg(args, int));
                format++;
                break;
            case 'i':
                arg->kind = 3;
                CInt64_SetLong((CInt64*)&arg->value, va_arg(args, int));
                format++;
                break;
            case '&':
            case '<':
            case '>':
            case '@':
            case 'M':
            case 'X':
                arg->kind = 2;
                arg->value = va_arg(args, int);
                if (*format == '&' && arg->value < 15) {
                    pc->flags &= ~0x4010;
                }
                format++;
                break;
            case 'h':
                arg->kind = 0;
                arg->rclass = 2;
                arg->reg = 0;
                format++;
                arg->flags = effect;
                break;
            case 'H':
                arg->kind = 0;
                arg->rclass = 2;
                arg->reg = 1;
                format++;
                arg->flags = effect;
                break;
            case 'Z':
                arg->kind = 0;
                arg->rclass = 2;
                arg->reg = 2;
                format++;
                arg->flags = effect;
                break;
            case 'A':
                arg->kind = 0;
                arg->rclass = 2;
                arg->reg = 2;
                format++;
                arg->flags = effect;
                break;
            case 'R':
                arg->kind = 0;
                arg->rclass = 2;
                arg->reg = 3;
                format++;
                arg->flags = effect;
                break;
            case 'Y':
                arg->kind = 0;
                arg->rclass = 2;
                arg->reg = 7;
                format++;
                arg->flags = effect;
                break;
            case '1':
                arg->kind = 0;
                arg->rclass = 2;
                arg->reg = 3;
                format++;
                arg->flags = effect;
                break;
            case 'o':
                count = va_arg(args, SInt32);
                if (count) {
                    if (count == -1) {
                        arg->kind = 6;
                        MOP_SETLABEL(*arg, va_arg(args, void*));
                    } else {
                        MOperand* memarg;
                        SInt32 meffect;

                        arg->kind = 4;
                        arg->value2 = count;
                        arg->value = va_arg(args, int);
                        memarg = arg;
                        if (format[1] != '(') {
                            CError_FATAL(357);
                        }
                        format += 2;
                        meffect = 1;
                        if (*format == '=') {
                            format++;
                            meffect = 2;
                        } else if (*format == '+') {
                            meffect = 3;
                            format++;
                        }
                        if (*format != 's' && *format != 't') {
                            CError_FATAL(367);
                        }
                        arg++;
                        arg->kind = 0;
                        arg->rclass = 0;
                        arg->reg = va_arg(args, int);
                        arg->flags = meffect;
                        format++;
                        if (pc->flags & 0x60006) {
                            if (((Object*)count)->datatype == DLOCAL && arg->reg != data_00625E3E) {
                                pc->alias = make_alias((Object*)count, 0, 0);
                            } else if (((Object*)count)->datatype == DLOCAL) {
                                pc->alias = make_alias(
                                    (Object*)count,
                                    memarg->value - ((Object*)MOP_OBJ(*memarg))->u.var.offset,
                                    func_0048FBE0(pc->op)
                                );
                            } else if (((Object*)count)->datatype == DDATA) {
                                pc->alias = make_alias((Object*)count, 0, 0);
                            } else {
                                CError_FATAL(386);
                            }
                            if (is_volatile_object((Object*)count)) {
                                pc->flags |= 0x80;
                            }
                            if (is_const_object((Object*)count)) {
                                pc->flags |= 0x40;
                            }
                        }
                    }
                } else {
                    arg->kind = 2;
                    arg->value = va_arg(args, int);
                    if (pc->flags & 0x60006) {
                        pc->flags |= 0x20;
                    }
                }
                format++;
                break;
            case 'k':
                arg->kind = 2;
                arg->value = va_arg(args, int);
                format++;
                break;
            case 'a':
            case 'q':
                CError_FATAL(413);
            case 'l':
            case 'p':
                arg->kind = 6;
                MOP_SETLABEL(*arg, va_arg(args, void*));
                format++;
                break;
            case 'Q':
                arg->kind = 0;
                arg->rclass = 2;
                arg->reg = 1;
                format++;
                arg->flags = effect;
                break;
            case 'K':
                switch (pc->op) {
                    case 0x68:
                    case 0x69:
                    case 0x6A:
                    case 0x6B:
                    case 0x1CD:
                    case 0x1CE:
                    case 0x1CF:
                    case 0x1D0:
                        arg->kind = 0;
                        if (pc->args[1].reg == 0x14) {
                            arg->rclass = 6;
                            arg->reg = 3;
                            arg->flags = effect;
                        } else if (pc->args[1].reg == 0x15) {
                            arg->rclass = 6;
                            arg->reg = 0;
                            arg->flags = effect;
                        } else if (pc->args[1].reg == 0x16) {
                            arg->rclass = 6;
                            arg->reg = 1;
                            arg->flags = effect;
                        } else {
                            arg->kind = 7;
                        }
                        break;
                    case 0x26C:
                    case 0x273:
                    case 0x27C:
                    case 0x296:
                    case 0x29D:
                    case 0x2B3:
                    case 0x2BA:
                    case 0x2C2:
                    case 0x2C9:
                    case 0x2D4:
                    case 0x2D8:
                    case 0x2DB:
                    case 0x2E2:
                        arg->kind = 0;
                        arg->rclass = 4;
                        arg->reg = 0x16;
                        arg->flags = effect;
                        break;
                    case 0x26B:
                    case 0x272:
                    case 0x295:
                    case 0x29C:
                    case 0x2A3:
                    case 0x2B2:
                    case 0x2B9:
                    case 0x2C1:
                    case 0x2C8:
                    case 0x2DA:
                    case 0x2E1:
                        arg->kind = 0;
                        arg->rclass = 4;
                        arg->reg = 0x15;
                        arg->flags = effect;
                        break;
                    case 0x2D1:
                    case 0x2D3:
                    case 0x2D5:
                        arg->kind = 0;
                        arg->rclass = 4;
                        arg->reg = 0x14;
                        arg->flags = effect;
                        break;
                    default:
                        CError_FATAL(517);
                }
                format++;
                break;
            case 'I':
                arg->kind = 3;
                *(CInt64*)&arg->value = va_arg(args, CInt64);
                format++;
                break;
            case 'B':
            case 'C':
            case 'c':
                arg->kind = 2;
                arg->value = va_arg(args, int);
                format++;
                break;
            case 'z':
            default:
                CError_FATAL(551);
        }
        arg++;
    }
    return pc;
}
