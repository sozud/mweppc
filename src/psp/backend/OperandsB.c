#include "compiler/common.h"
#include "psp/BE.h"
#include "compiler/CError.h"
#include "compiler/objects.h"
#include "compiler/CInt64.h"
#include "compiler/CParser.h"
#include "compiler/types.h"
#include "compiler/CMachine.h"

#line 1 "Operands.c"
// mwccpsp 3.0.1 around 0x4A46A0..0x4A7400: operand helpers and call setup.
// The assertions here name the unit "Operands.c"; our Operands.c holds the
// instruction makers at 0x47FD20 (unit name unknown).

extern SInt32 data_00624DB0;
extern SInt32 data_006242B4;
extern SInt32 data_00624650; // locals' frame size
extern SInt32 data_00625018;
extern SInt32 data_006250AC;
extern UInt16 data_00625E48; // move (read unsigned here)
extern UInt8 data_00625EA9;
extern UInt32 data_005F0E68[]; // registers a runtime call clobbers
extern VOperand func_0047FAA0(
    UInt16 op, int fpr, Boolean pair
); // the result registers after a call
extern void func_004A8310(Object* func, int unk, UInt32* masks); // branch_subroutine

// Reset the frame layout for a new function.
void func_004A46A0(void) {
    data_00624DB0 = 0;
    data_006242B4 = 0;
    data_00624650 = 0;
    data_00625018 = 0;
    data_006250AC = 0;
}

// The integer result of a call.
VOperand func_004A46E0(Boolean pair) {
    return func_0047FAA0(data_00625E48, 0, pair != 0);
}

// Call a runtime routine.
void func_004A4A20(Object* rtfunc) {
    data_00625EA9 = 0;
    func_004A8310(rtfunc, 0, data_005F0E68);
}

// The register an object is addressed from.
SInt16 func_004A5360(Object* obj) {
    ObjAddress addr;

    ObjGen_GetObjectAddress(obj, &addr);
    return addr.basereg;
}

// The high register of a pair.
VOperand func_004A7390(VOperand* src) {
    VOperand v;

    CError_ASSERT(54, src->kind != 3);
    v.kind = src->kind;
    v.reg = src->regHi;
    return v;
}

extern Type data_005CB170; // the type an address is loaded as
extern void func_004A64C0(VOperand* op, Type* type, short copy); // make the operand a value

// Make the operand a memory reference (kind 4) and note whether the
// expression is volatile or const.
void func_004A72B0(VOperand* op, ENode* expr) {
    switch (op->kind) {
        case 4:
            if (op->kind) {
                func_004A64C0(op, &data_005CB170, 0);
            }
        case 0:
        case 1:
            op->offset = 0;
            op->object = 0;
            op->kind = 4;
            break;
        case 2:
            op->kind = 4;
            break;
        case 3:
            if (CInt64_IsInRange(op->imm, 4) || CInt64_IsInURange(op->imm, 4)) {
                op->reg = 0;
                op->offset = op->imm.lo;
            } else {
                CError_FATAL(90);
            }
            op->object = 0;
            op->kind = 4;
            break;
        default:
            CError_FATAL(96);
    }
    op->flags = 0;
    if (expr) {
        if (CParserIsVolatileExpr(expr)) {
            op->flags |= 0x80;
        }
        if (CParserIsConstExpr(expr)) {
            op->flags |= 0x40;
        }
    }
}

// Make op a value of type unless it already is one.
void func_004A5A60(VOperand* op, Type* type) {
    if (op->kind != 0) {
        func_004A64C0(op, type, 1);
    }
}

#define IS_SINGLE(t) \
    (TYPE_INTEGRAL(t)->integral == IT_FLOAT || ((t)->type == TYPEFLOAT && (t)->size == 4))
#define IS_DOUBLE(t) \
    (TYPE_INTEGRAL(t)->integral == IT_DOUBLE || ((t)->type == TYPEFLOAT && (t)->size == 8))
#define IS_SOFTFLOAT(t) \
    ((IS_SINGLE(t) && !(copts.fp_library & 1)) || (IS_DOUBLE(t) && !(copts.fp_library & 2)))

// Where a call's result of this type comes back: general registers (a pair
// for a double) with the float library, else the float result register.
VOperand func_004A4710(Type* type) {
    Boolean pair = IS_DOUBLE(type);

    if (type->type != TYPEFLOAT || !IS_SOFTFLOAT(type)) {
        return func_0047FAA0(IS_SINGLE(type) ? 0x5B1 : 0x5B2, 1, 0);
    }
    return func_0047FAA0(data_00625E48, 0, pair != 0);
}

// Move an argument into $a0 + n - 1; a double in two float registers goes
// to a general register pair in the target's word order.
void func_004A47E0(VOperand* op, Boolean pair, int n) {
    int reg = 4;

    if (pair) {
        reg += (n - 1) * 2;
        if (copts.littleendian) {
            emitpcode(0xAA, reg, op->reg);
            emitpcode(0xAA, reg + 1, op->regHi);
        } else {
            emitpcode(0xAA, reg + 1, op->reg);
            emitpcode(0xAA, reg, op->regHi);
        }
        data_005F0E68[0] |= 3 << reg;
    } else {
        reg += n - 1;
        emitpcode((SInt16)data_00625E48, reg, op->reg);
        data_005F0E68[0] |= BIT32(reg);
    }
}

// Pass an argument of this type in the n-th argument register: general
// registers for integers and float-library floats, else $f12 + 2(n - 1).
void func_004A4890(VOperand* op, Type* type, int n) {
    Boolean pair = IS_DOUBLE(type);
    int reg;

    if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
        reg = 4;
        if (pair) {
            reg += (n - 1) * 2;
            if (copts.littleendian) {
                emitpcode(0xAA, reg, op->reg);
                emitpcode(0xAA, reg + 1, op->regHi);
            } else {
                emitpcode(0xAA, reg, op->regHi);
                emitpcode(0xAA, reg + 1, op->reg);
            }
            data_005F0E68[0] |= 3 << reg;
        } else {
            reg += n - 1;
            emitpcode((SInt16)data_00625E48, reg, op->reg);
            data_005F0E68[0] |= BIT32(reg);
        }
    } else {
        reg = 12;
        reg += (n - 1) * 2;
        if (pair) {
            emitpcode(0x5B2, reg, op->reg);
            data_005F0E68[1] |= 3 << reg;
        } else {
            emitpcode(0x5B1, reg, op->reg);
            data_005F0E68[1] |= BIT32(reg);
        }
    }
}

extern SInt32 data_00625A98[7]; // next virtual register, per class
extern VOperand func_004C3B10(MOperand op);
extern VOperand func_0047F9A0(VOperand* src); // copy into a new register

#define IS_HARDFLOAT(t) \
    ((IS_SINGLE(t) && (copts.fp_library & 1)) || (IS_DOUBLE(t) && (copts.fp_library & 2)))

// Copy a value of this type into new virtual registers: a register pair for
// long long and float-library doubles, a float register for hardware floats.
VOperand func_004A7050(VOperand* src, Type* type) {
    MPCode* pc;
    MPCode* pc2;
    VOperand v;

    if (((type->type == TYPEINT || type->type == TYPEENUM) && type->size == 8) ||
        (type->type == TYPEFLOAT &&
         IS_SOFTFLOAT(type) &&
         type->type == TYPEFLOAT &&
         IS_DOUBLE(type))) {
        pc = makepcode(0xAA, data_00625A98[0]++, src->reg);
        appendpcode(data_006246B4, pc);
        pc2 = makepcode(0xAA, data_00625A98[0]++, src->regHi);
        appendpcode(data_006246B4, pc2);
        v = func_004C3B10(pc->args[0]);
        v.regHi = pc2->args[0].reg;
    } else if (type->type == TYPEFLOAT && IS_HARDFLOAT(type)) {
        pc = makepcode(IS_SINGLE(type) ? 0x5B1 : 0x5B2, data_00625A98[1]++, src->reg);
        v = func_004C3B10(pc->args[0]);
        appendpcode(data_006246B4, pc);
    } else {
        return func_0047F9A0(src);
    }
    return v;
}

extern VOperand func_00479C30(UInt16 op, VOperand* src, SInt32 bits); // sign or zero extend
extern VOperand func_0047EBB0(VOperand* src, SInt32 shift, Type* type); // src << shift
extern VOperand func_0047F9F0(CInt64 value, Type* type); // load a constant
extern VOperand func_0047E650(VOperand* src, UInt32 mask); // src & mask

// Store value into the bitfield bf of word (in a register); past bit 31
// the mask clearing the field is a 64-bit constant.
void func_004A5380(VOperand value, VOperand* word, TypeBitfield* bf) {
    Type* type = bf->bitfieldtype;
    int width = bf->bitlength;
    int top;
    int shift;
    int offset;
    CInt64 n;
    CInt64 mask;
    VOperand v;
    VOperand m;
    int reg;

    v = func_00479C30(0x59A, &value, width);
    if (!copts.littleendian) {
        top = bf->bitfieldtype->size * 8 - bf->offset;
        shift = top - width;
        v = func_0047EBB0(&v, shift, type);
        if (top > 31) {
            CInt64_SetLong(&n, width);
            mask = CInt64_Sub(CInt64_Shl(cint64_one, n), cint64_one);
            CInt64_SetLong(&n, shift);
            mask = CInt64_Inv(CInt64_Shl(mask, n));
            m = func_0047F9F0(mask, TYPE(&stsignedlonglong));
            reg = data_00625A98[0]++;
            emitpcode(6, reg, word->reg, m.reg);
            word->reg = reg;
        } else {
            *word = func_0047E650(word, ~(((BIT32(width)) - 1) << shift));
        }
    } else {
        offset = bf->offset;
        v = func_0047EBB0(&v, offset, type);
        if (width + offset > 31) {
            CInt64_SetLong(&n, width);
            mask = CInt64_Sub(CInt64_Shl(cint64_one, n), cint64_one);
            CInt64_SetLong(&n, offset);
            mask = CInt64_Inv(CInt64_Shl(mask, n));
            m = func_0047F9F0(mask, TYPE(&stsignedlonglong));
            reg = data_00625A98[0]++;
            emitpcode(6, reg, word->reg, m.reg);
            word->reg = reg;
        } else {
            *word = func_0047E650(word, ~(((BIT32(width)) - 1) << offset));
        }
    }
    reg = data_00625A98[0]++;
    emitpcode(0xBE, reg, word->reg, v.reg);
    *word = func_004C3B10(data_006246B4->last->args[0]);
}

extern SInt32 data_00624788; // register size in bytes
extern VOperand func_0047EA50(VOperand* src, SInt32 shift, Type* type); // src >> shift

// The value of a bitfield assignment: the field taken back out of the word
// (shifted up to the top of a register, then down, signed or not by type).
void func_004A5730(VOperand word, TypeBitfield* bf, VOperand* out) {
    Type* type = bf->bitfieldtype;
    int width = bf->bitlength;
    Boolean uns = is_unsigned(type);
    int top;
    int offset;
    int n;

    if (!copts.littleendian) {
        top = type->size * 8 - bf->offset;
        n = data_00624788 * 8 - top;
        if (n > 0 && top > 0) {
            *out = func_0047EBB0(&word, n, type);
        } else {
            *out = word;
        }
        n = data_00624788 * 8 - width;
        if (n > 0) {
            *out = func_0047EA50(out, n, type);
        }
    } else if ((offset = bf->offset) != 0) {
        n = data_00624788 * 8 - (width + offset);
        if (n > 0) {
            *out = func_0047EBB0(&word, n, type);
        } else {
            *out = word;
        }
        n = data_00624788 * 8 - width;
        if (n > 0) {
            *out = func_0047EA50(out, n, type);
        }
    } else {
        if (data_00624788 * 8 - width > 0) {
            *out = func_00479C30(uns ? 0x59A : 0x599, &word, width);
        } else {
            *out = word;
        }
    }
}

extern SInt32 data_00624744[7]; // first virtual register of each class
extern VOperand func_004C3BA0(SInt32 reg, UInt8 kind);
extern MPCode* func_00480420(int op, SInt16 nargs);
extern UInt16 func_0047FD20(Type* type, SInt32* fpr); // load opcode for the type
extern VOperand func_0047F2D0(VOperand* src); // the address in a register
extern MOperand func_004C3DD0(char rclass); // a new virtual register
extern MOperand func_004C3C60(VOperand* v);
extern MOperand func_004C3E70(UInt16 reg, UInt8 rclass, UInt16 flags);
extern struct Alias* func_0053BD10(VOperand* mem, MPCode* pc);
extern VarInfo* Registers_GetVarInfo(Object* obj);

#define IS_LONGLONG(t) (((t)->type == TYPEINT || (t)->type == TYPEENUM) && (t)->size == 8)

extern SInt16 data_00625E74; // 16-byte register move

// Move src of class sclass into dst of class dclass (a new register when dst
// is negative); returns dst. Classes: 0 general, 1 floating point.
static inline int emit_regmove(char dclass, int dst, char sclass, int src, int size) {
    switch (dclass) {
        case 0:
            if (sclass == 0 && (dst == src || dst < 0)) {
                return dst;
            }
            if (dst < 0) {
                dst = data_00625A98[0]++;
            }
            switch (sclass) {
                case 0:
                    emitpcode(size == 16 ? data_00625E74 : (SInt16)data_00625E48, dst, src);
                    break;
                case 1:
                    emitpcode(size == 8 ? 0x7F : 0xA5, dst, src);
                    break;
                default:
                    CError_FATAL(186);
            }
            break;
        case 1:
            if (sclass == 1 && (dst == src || dst < 0)) {
                return dst;
            }
            if (dst < 0) {
                dst = data_00625A98[1]++;
            }
            switch (sclass) {
                case 0:
                    emitpcode(size == 8 ? 0xB1 : 0xAD, src, dst);
                    break;
                case 1:
                    emitpcode(size == 8 ? 0xF8 : 0xF9, dst, src);
                    break;
                default:
                    CError_FATAL(226);
            }
            break;
        default:
            CError_FATAL(320);
    }
    return dst;
}

// Coerce_to_register: make op a value in a register (a new one if copy is
// set): constants are loaded, addresses computed, memory read (a register
// pair for 8-byte integers and doubles, the home register for a local that
// lives in one, the field for a bitfield).
void func_004A64C0(VOperand* op, Type* type, short copy) {
    UInt32 pcflags = 0;
    MPCode* pc2 = NULL;
    RegClass301 rclass;
    int reg;
    VarInfo* vi;
    MPCode* pc;
    SInt32 fpr;
    UInt16 opc;
    VOperand v;

    switch (op->kind) {
        case 0:
        case 1:
            return;
        case 5:
            // emit_regmove(rclass, -1, op->kind, ...) with no move for a
            // coprocessor register: only the assertions are left
            rclass = type->type == TYPEFLOAT ? 1 : 0;
            switch (rclass) {
                case 0:
                    reg = data_00625A98[0]++;
                    CError_FATAL(186);
                    break;
                case 1:
                    reg = data_00625A98[1]++;
                    CError_FATAL(226);
                    break;
                default:
                    CError_FATAL(186);
            }
            *op = func_004C3BA0(reg, rclass);
            return;
        case 3:
            *op = func_0047F9F0(op->imm, type);
            if (copy) {
                *op = func_004A7050(op, type);
            }
            return;
        case 2:
            if (op->offset == 0 && op->reg >= data_00624744[0]) {
                *op = func_004C3B10(func_004C3E70(op->reg, 0, 1));
            } else {
                *op = func_0047F2D0(op);
            }
            return;

        case 4:
            vi = NULL;
            if (!op->object) {
                pcflags |= 0x20;
            }
            if (op->object && is_volatile_object((Object*)op->object)) {
                pcflags |= 0x80;
            }
            pcflags |= op->flags;
            if (op->object &&
                !(((Object*)op->object)->qual & 0x10000) &&
                ((Object*)op->object)->datatype == DLOCAL) {
                vi = Registers_GetVarInfo((Object*)op->object);
            }
            if (vi && vi->reg) {
                if ((int)vi->rclass == 1) {
                    if (copy) {
                        pc =
                            makepcode(IS_SINGLE(type) ? 0x5B1 : 0x5B2, data_00625A98[1]++, vi->reg);
                        *op = func_004C3B10(pc->args[0]);
                        appendpcode(data_006246B4, pc);
                    } else {
                        *op = func_004C3BA0(vi->reg, 1);
                    }
                } else if (IS_LONGLONG(type) || (type->type == TYPEFLOAT && IS_DOUBLE(type))) {
                    if (copy) {
                        pc = makepcode(0xAA, data_00625A98[0]++, vi->reg);
                        appendpcode(data_006246B4, pc);
                        pc2 = makepcode(0xAA, data_00625A98[0]++, vi->regHi);
                        appendpcode(data_006246B4, pc2);
                        *op = func_004C3B10(pc->args[0]);
                        op->regHi = pc2->args[0].reg;
                    } else {
                        *op = func_004C3BA0(vi->reg, 0);
                        op->regHi = vi->regHi;
                    }
                } else {
                    if (copy) {
                        pc = makepcode((SInt16)data_00625E48, data_00625A98[0]++, vi->reg);
                        *op = func_004C3B10(pc->args[0]);
                        appendpcode(data_006246B4, pc);
                    } else {
                        *op = func_004C3BA0(vi->reg, 0);
                    }
                }
                return;
            }

            opc = func_0047FD20(type, &fpr);
            if (fpr == 0 && (IS_LONGLONG(type) || (type->type == TYPEFLOAT && IS_DOUBLE(type)))) {
                pc = func_00480420(opc, 3);
                pc->args[0] = func_004C3DD0(fpr);
                pc2 = func_00480420(opc, 3);
                pc2->args[0] = func_004C3DD0(0);
                if (copts.littleendian) {
                    pc->args[1] = func_004C3C60(op);
                    op->offset += 4;
                    pc2->args[1] = func_004C3C60(op);
                    op->offset -= 4;
                } else {
                    op->offset += 4;
                    pc->args[1] = func_004C3C60(op);
                    op->offset -= 4;
                    pc2->args[1] = func_004C3C60(op);
                }
                pc->args[2] = func_004C3E70(op->reg, 0, 1);
                pc2->args[2] = func_004C3E70(op->reg, 0, 1);
                if (op->object) {
                    pc->alias = func_0053BD10(op, pc);
                    pc2->alias = func_0053BD10(op, pc2);
                }
            } else {
                pc = func_00480420(opc, 3);
                pc->args[0] = func_004C3DD0(fpr);
                if (opc == ((copts.mips_isa >= 2 && (copts.fp_library & 2)) ? 0x563 : 0x560)) {
                    pc->args[0].flags |= 0x8000;
                    if (copts.mips_option1A) {
                        pc->args[0].flags |= 0x20;
                    }
                }
                pc->args[1] = func_004C3C60(op);
                pc->args[2] = func_004C3E70(op->reg, 0, 1);
                if (op->object) {
                    pc->alias = func_0053BD10(op, pc);
                }
            }

            v = func_004C3B10(pc->args[0]);
            pc->flags |= pcflags;
            appendpcode(data_006246B4, pc);
            if (pc2) {
                pc2->flags |= pcflags;
                appendpcode(data_006246B4, pc2);
                v.regHi = pc2->args[0].reg;
            }
            if (op->unk18) {
                func_004A5730(v, (TypeBitfield*)op->unk18, op);
            } else {
                *op = v;
            }
            if (!pc->alias) {
                pc->flags |= 0x20;
                if (pc2) {
                    pc->flags |= 0x20;
                }
            }
            break;
        default:

            CError_FATAL(226);
    }
}

extern UInt16 func_0047FFD0(Type* type); // store opcode for the type
extern void func_00479710(UInt16 op, VOperand* reg, VOperand* mem); // store reg to mem
extern Type* func_00464B40(Type* type); // the struct's single floating-point member
extern Type* func_00464BE0(Type* type); // the type of the struct's only member

static inline int is_hardfloat(Type* type) {
    int single = IS_SINGLE(type);
    int hard = 0;

    if (single && (copts.fp_library & 1)) {
        hard = 1;
    }
    return hard || (IS_DOUBLE(type) && (copts.fp_library & 2));
}

static inline VOperand high_half(VOperand* v) {
    VOperand hi;

    hi.kind = v->kind;
    hi.reg = v->regHi;
    return hi;
}

// Store src (of type) to dst: into the home register of a local that lives
// in one, else to memory (two stores for 8-byte integers and float-library
// doubles).
void func_004A5A90(VOperand src, VOperand* dst, Type* type) {
    UInt32 pcflags = 0;
    VarInfo* vi;
    VarInfo* svi;
    int hard;
    RegClass301 rclass;
    UInt16 opc;
    Type* t;
    VOperand v;
    VOperand hi;
    MPCode* pc;
    MPCode* pc2;
    int reg;

    if (dst->object && ((Object*)dst->object)->datatype == DLOCAL) {
        vi = Registers_GetVarInfo((Object*)dst->object);
    } else {
        vi = NULL;
    }
    if (src.object && ((Object*)src.object)->datatype == DLOCAL) {
        svi = Registers_GetVarInfo((Object*)src.object);
    }
    if (!dst->object) {
        pcflags |= 0x20;
    }
    if (dst->object && is_volatile_object((Object*)dst->object)) {
        pcflags |= 0x80;
    }

    switch (dst->kind) {
        case 2:
        case 4:
            if (vi && vi->reg) {
                if (type->type == TYPEFLOAT && IS_HARDFLOAT(type)) {
                    emitpcode(IS_SINGLE(type) ? 0x5B1 : 0x5B2, vi->reg, src.reg);
                    return;
                }
                if (IS_LONGLONG(type) || (type->type == TYPEFLOAT && IS_DOUBLE(type))) {
                    if (vi->reg != src.regHi) {
                        emitpcode((SInt16)data_00625E48, vi->reg, src.reg);
                        emitpcode((SInt16)data_00625E48, vi->regHi, src.regHi);
                    } else if (vi->regHi != src.reg) {
                        emitpcode((SInt16)data_00625E48, vi->regHi, src.regHi);
                        emitpcode((SInt16)data_00625E48, vi->reg, src.reg);
                    } else {
                        reg = data_00625A98[0]++;
                        emitpcode((SInt16)data_00625E48, reg, src.regHi);
                        emitpcode((SInt16)data_00625E48, vi->reg, src.reg);
                        emitpcode((SInt16)data_00625E48, vi->regHi, reg);
                    }
                    return;
                }

                opc = func_0047FFD0(type);
                switch (opc) {
                    case 0x587:
                    case 0x58F:
                    case 0x597:
                        v = src;
                        break;
                    case 0x58D:
                        if (src.reg == 0 || type->type == TYPEENUM) {
                            v = src;
                        } else {
                            v = func_00479C30(is_unsigned(type) ? 0x59A : 0x599, &src, 8);
                        }
                        break;
                    case 0x58E:
                        if (src.reg == 0 || type->type == TYPEENUM) {
                            v = src;
                        } else {
                            v = func_00479C30(is_unsigned(type) ? 0x59A : 0x599, &src, 16);
                        }
                        break;
                    default:
                        CError_FATAL(653);
                }

                hard = 0;
                if (type->type == TYPEFLOAT && is_hardfloat(type)) {
                    hard = 1;
                }
                rclass = hard ? 1 : 0;
                emit_regmove(rclass, vi->reg, rclass, v.reg, type->size);
                return;
            }

            if (type->type == TYPESTRUCT || type->type == TYPECLASS) {
                if ((t = func_00464B40(type))) {
                    opc = func_0047FFD0(t);
                } else if ((t = func_00464BE0(type))) {
                    opc = func_0047FFD0(t);
                } else {
                    opc = func_0047FFD0(type);
                }
            } else {
                opc = func_0047FFD0(type);
            }
            break;
        default:
            CError_FATAL(791);
    }

    if (IS_LONGLONG(type) ||
        (type->type == TYPEFLOAT &&
         IS_SOFTFLOAT(type) &&
         type->type == TYPEFLOAT &&
         IS_DOUBLE(type))) {
        if (src.kind == 3) {
            CError_FATAL(54);
        }
        hi = high_half(&src);
        pc = func_00480420(opc, 3);
        pc->args[0] = func_004C3C60(&src);
        pc2 = func_00480420(opc, 3);
        pc2->args[0] = func_004C3C60(&hi);
        if (copts.littleendian) {
            pc->args[1] = func_004C3C60(dst);
            dst->offset += 4;
            pc2->args[1] = func_004C3C60(dst);
            dst->offset -= 4;
        } else {
            dst->offset += 4;
            pc->args[1] = func_004C3C60(dst);
            dst->offset -= 4;
            pc2->args[1] = func_004C3C60(dst);
        }
        pc->args[2] = func_004C3E70(dst->reg, 0, 1);
        pc2->args[2] = func_004C3E70(dst->reg, 0, 1);
        if (pc->args[1].value2) {
            pc->alias = func_0053BD10(dst, pc);
            pc2->alias = func_0053BD10(dst, pc2);
        }
        pc->flags |= pcflags;
        appendpcode(data_006246B4, pc);
        if (pc2) {
            pc2->flags |= pcflags;
            appendpcode(data_006246B4, pc2);
        }
    } else {
        func_00479710(opc, &src, dst);
    }
}
