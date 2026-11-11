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

typedef struct ObjDataReloc {
    struct ObjDataReloc* next;
    struct Object* target;
    SInt32 offset;
    SInt32 addend;
} ObjDataReloc;

extern SInt32 data_00624DF4; // largest alignment in the function
extern SInt32 data_00603F70; // current byte offset
extern MSwitchTable* data_00624724; // switch tables to emit
extern SInt32* data_006241D8; // receives the instruction count
extern void func_004584C0(SInt32* out, SInt32 fill);
extern void* lalloc(SInt32 size);
extern void ObjGen_EmitData(struct Object* obj, void* data, ObjDataReloc* relocs, SInt32 size);

// a switch table object's entries (label operands) and size in bytes
#define OBJECT_SWITCH_LABELS(obj) ((MOperand**)(obj)->u.data.u.switchtable.data)
#define OBJECT_SWITCH_SIZE(obj) ((obj)->u.data.u.switchtable.size)
#define SWITCH_LABEL_OFFSET(l) (((MPCodeLabel*)(l)->ptr)->block->codeoffset)

extern SInt32 data_00625208; // index of the instruction being encoded
extern void func_004C5940(SInt32 err);
extern void func_004C5890(SInt32 max, SInt32 value);

// A label's code offset in bytes.
#define LABEL_OFFSET(v) (((MPCodeLabel*)(v))->block->codeoffset)
// A register operand: the second register of a pair is one higher.
#define REG(a) ((SInt16)(a)->value + (((a)->flags & 0x4000) ? 1 : 0))

void BE_EncodeInstruction(MPCode* pc, SInt32 pass) {
    char* fmt;
    SInt32 argi;
    UInt8 wide;
    UInt8 tick;
    SInt32 v;
    SInt32 hi;
    UInt16 op;
    MOperand* arg;

    argi = 0;
    fmt = data_006115F8[pc->op].format;
    pc->word = data_006115F8[pc->op].unk10;

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

            arg = &pc->args[argi];
            switch (*fmt) {
                case '&':
                    pc->word |= arg->value << 21;
                    argi++;
                    break;
                case '<':
                    if (wide) {
                        pc->word |= (arg->value & 7) << 18;
                    } else {
                        v = arg->value;
                        if (v > 31) {
                            func_004C5890(31, v);
                        }
                        pc->word |= (v & 31) << 6;
                    }
                    argi++;
                    break;
                case '>':
                    if (wide) {
                        pc->word |= (arg->value & 7) << 16;
                    } else {
                        v = arg->value;
                        if (v > 31) {
                            func_004C5890(31, v);
                        }
                        pc->word |= (v & 31) << 6;
                    }
                    argi++;
                    break;
                case '@':
                    if (pc->op == 0x3EF) {
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
                        pc->word |= (UInt16)arg->value2;
                    }
                    argi++;
                    break;
                case 'e':
                    pc->word |= arg->value << 8;
                    argi++;
                    break;
                case 'i':
                    pc->word |= (UInt16)arg->value2;
                    argi++;
                    break;
                case 'k':
                    if (wide) {
                        pc->word |= (UInt8)arg->value2 << 16;
                    } else {
                        pc->word |= (arg->value2 & 31) << 16;
                    }
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
                    if (wide) {
                        pc->word |= v & 0xFFFC;
                    } else {
                        pc->word |= (UInt16)v;
                    }
                    argi++;
                    break;
                case 'p':
                    if (pass == 2) {
                        if (pc->unk38 != 10) {
                            if (arg->kind == 2) {
                                v = arg->value2;
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
                    pc->word |= (SInt16)arg->value << 11;
                    argi++;
                    break;
                case 's':
                    if (wide) {
                        pc->word |= ((SInt16)arg->value << 21) & 0x7E00000;
                    } else {
                        pc->word |= (SInt16)arg->value << 21;
                    }
                    argi++;
                    break;
                case 'P':
                    pc->word |= (SInt16)arg->value * 2;
                    argi++;
                    break;
                case 'u':
                    pc->word |= (UInt16)(arg->value2 >> 16);
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
                        pc->word |= (SInt16)arg->value << 6;
                        argi++;
                        break;
                    }
                    CError_FATAL(341);
                case 'z':
                    CError_FATAL(344);
                    break;
                case 'D':
                    if (wide) {
                        pc->word |= REG(arg) & 0x7F;
                    } else {
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
                    if (wide) {
                        pc->word |= (REG(arg) & 0x7F) << 8;
                    } else {
                        pc->word |= REG(arg) << 11;
                    }
                    argi++;
                    break;
                case 'T':
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
                    pc->word |= (SInt16)arg->value << 16;
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
                    if (wide) {
                        pc->word |= (SInt16)arg->value << 16;
                    } else {
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
                    if (pass == 2 && arg->kind == 6) {
                        pc->word = LABEL_OFFSET(arg->ptr);
                    }
                    argi++;
                    break;
                case 'I':
                    if (!wide) {
                        pc->word |= arg->value2;
                    }
                    argi++;
                    break;
                case 'Q':
                    argi++;
                    break;
                case 'F':
                    CError_FATAL(516);
                    break;
                case 'f':
                    CError_FATAL(524);
                    break;
                case 'l':
                    if (pass == 2 && arg->kind == 6) {
                        pc->word = LABEL_OFFSET(arg->ptr);
                    }
                    break;
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
            }
            fmt++;
        } while (*fmt);
    }

    op = pc->op;
    if (op >= 0x2EF && op <= 0x32E) {
        pc->word |= ((pc->word & 0xF800) >> 11) << 21;
    } else if (op >= 0x32F && op <= 0x337) {
        pc->word |= ((pc->word & 0x1F0000) >> 16) << 21;
    } else if (op >= 0x338 && op <= 0x353) {
        pc->word |= ((pc->word & 0xF800) >> 11) << 16;
    } else if (op >= 0x354 && op <= 0x37D) {
        pc->word |= ((pc->word & 0x7C0) >> 6) << 11;
    }
}

// Final pass for one instruction: branches whose target is out of 16-bit
// range become the long form, then instructions with label operands are
// encoded again with the final offsets.
void BE_EncodeFinal(MPCode* pc) {
    SInt32 d;
    char* label;

    if (pc->flags & 1) {
        if (pc->op == 0x39) {
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
    } else if (pc->op == 0x2EE && pc->args[0].kind == 6) {
        BE_EncodeInstruction(pc, 2);
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
            } else if (pc->op < 0x518) {
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
    if (pc->op < 0x518) {
        data_00625208++;
        data_00603F70 += 4;
    } else if (pc->op < 0x5C5) {
        func_004C5940(15);
    }
}

// Lay out the function (offsets, alignment padding), emit its switch tables,
// then encode every instruction with the final offsets.
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
    SInt32 fillvalue[2];
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
                    case 0x5BF:
                    case 0x5C1:
                    case 0x5C3:
                        fill = 0;
                        break;
                    case 0x5BE:
                        b = (UInt8)pc->args[1].value2;
                        fill = b | (b << 16) | (b << 24) | (b << 8);
                        break;
                    case 0x5C0:
                        b = (UInt16)pc->args[1].value2;
                        fill = b | (b << 16);
                        break;
                    case 0x5C2:
                        fill = pc->args[1].value2;
                        break;
                    default:
                        CError_FATAL(718);
                        BE_CountInstruction(pc);
                        pc = pc->next;
                        continue;
                }
                func_004584C0(fillvalue, fill);
                if (pc->args[0].value2) {
                    align = BIT32(pc->args[0].value2);
                    if (align > data_00624DF4) {
                        data_00624DF4 = align;
                    }
                    while (data_00603F70 & (align - 1)) {
                        if (pc == block->first) {
                            block->codeoffset += 4;
                        }
                        nop = makepcode(0x2ED, fillvalue[0], fillvalue[1]);
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
