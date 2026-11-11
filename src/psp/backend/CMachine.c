#include "compiler/common.h"
#include "compiler/CError.h"
#include "compiler/CParser.h"
#include "compiler/objects.h"
#include "compiler/types.h"
#include "compiler/CMachine.h"
#include "compiler/CClass.h"
#include "compiler/CInt64.h"
#include "compiler/tokens.h"
#include "compiler/ScanFloat.h"
#include "compiler/CompilerTools.h"
#include <stdio.h>
#include <string.h>
#ifndef __MWERKS__
#include "x87.h" // native: x87 extended-precision constant folding in software
#endif

// mwccpsp 3.0.1 CMachine.c (MIPS)

extern SInt32 data_00624FD4; // maximum alignment, if any

SInt16 CMach_GetTypeAlign(Type* type) {
    SInt32 align;

restart:
    switch (type->type) {
        case TYPEVOID:
            return 4;
        case TYPEENUM:
            type = TYPE_ENUM(type)->enumtype;
        case TYPEINT:
            switch (TYPE_INTEGRAL(type)->integral) {
                case IT_BOOL:
                case IT_CHAR:
                case IT_SCHAR:
                case IT_UCHAR:
                case IT_WCHAR_T:
                case IT_SHORT:
                case IT_USHORT:
                case IT_INT:
                case IT_UINT:
                case IT_LONG:
                case IT_ULONG:
                case IT_LONGLONG:
                case IT_ULONGLONG:
                case 18:
                case 19:
                    align = type->size;
                    break;
                default:
                    CError_Internal("CMachine.c", 957);
            }
            break;
        case TYPEFLOAT:
            switch (TYPE_INTEGRAL(type)->integral) {
                case 17:
                    return 16;
                default:
                    align = (type->size > 4) ? 8 : 4;
            }
            break;
        case TYPESTRUCT:
            align = TYPE_STRUCT(type)->align;
            break;
        case TYPECLASS:
            align = TYPE_CLASS(type)->align;
            break;
        case TYPEFUNC:
            return 0;
        case TYPEBITFIELD:
            type = TYPE_BITFIELD(type)->bitfieldtype;
            goto restart;
        case TYPETEMPLATE:
            return 1;
        case TYPEMEMBERPOINTER:
        case TYPEPOINTER:
            align = 4;
            break;
        case TYPEARRAY:
            type = TYPE_POINTER(type)->target;
            goto restart;
        default:
            CError_Internal("CMachine.c", 997);
            return 0;
    }
    if (data_00624FD4 && align > data_00624FD4) {
        align = data_00624FD4;
    }
    return align;
}

// The alignment an __attribute__((aligned)) qualifier asks for, or 0.
int func_00466720(UInt32 qual) {
    int align = 0;

    switch ((SInt32)(qual & Q_ALIGNED_MASK)) {
        case 0:
            break;
        case Q_ALIGNED_1:
            align = 1;
            break;
        case Q_ALIGNED_2:
            align = 2;
            break;
        case Q_ALIGNED_4:
            align = 4;
            break;
        case Q_ALIGNED_8:
            align = 8;
            break;
        case Q_ALIGNED_16:
            align = 0x10;
            break;
        case Q_ALIGNED_32:
            align = 0x20;
            break;
        case Q_ALIGNED_64:
            align = 0x40;
            break;
        case Q_ALIGNED_128:
            align = 0x80;
            break;
        case Q_ALIGNED_256:
            align = 0x100;
            break;
        case Q_ALIGNED_512:
            align = 0x200;
            break;
        case Q_ALIGNED_1024:
            align = 0x400;
            break;
        case Q_ALIGNED_2048:
            align = 0x800;
            break;
        case Q_ALIGNED_4096:
            align = 0x1000;
            break;
        case Q_ALIGNED_8192:
            align = 0x2000;
            break;
        default:
            CError_Internal("CMachine.c", 340);
    }
    return align;
}

// The alignment of a variable.
SInt16 func_00464E20(Object* obj) {
    Type* type = obj->type;
    SInt16 align;

    if ((align = func_00466720(obj->qual))) {
        return align;
    }
    align = CMach_GetTypeAlign(type);
    if (type->type == TYPESTRUCT || type->type == TYPECLASS || type->type == TYPEARRAY) {
        align = (align < 4) ? 4 : align;
    }
    if (type->size >= 16 && copts.function_align16) {
        align = (align <= 16) ? 16 : align;
    }
    if (type->size > 4) {
        if (align <= ((copts.mips_isa >= 3) ? 8 : 4)) {
            align = (copts.mips_isa >= 3) ? 8 : 4;
        }
    }
    if (type->size == 4) {
        align = (align < 4) ? 4 : align;
    }
    return align;
}

void CMach_Configure(void) {
    stwchar.size = copts.ushort_wchar_t ? stunsignedshort.size : stsignedint.size;
    if (copts.mips_long64) {
        stsignedlong.size = 8;
        stunsignedlong.size = 8;
    }
    data_00624FD4 = 0;
}

static SInt32 cmach_structstart;
static SInt16 cmach_curbfsize;
static SInt32 cmach_structoffset;

extern UInt8 func_00436B30(Type* type); // how a value of the type is returned

Boolean CMach_IsUnsignedType(Type* type) {
    switch ((char)type->type) {
        case TYPEINT:
            switch (TYPE_INTEGRAL(type)->integral) {
                case IT_BOOL:
                case IT_UCHAR:
                case IT_WCHAR_T:
                case IT_USHORT:
                case IT_UINT:
                case IT_ULONG:
                case IT_ULONGLONG:
                    return 1;
                case IT_CHAR:
                    return copts.unsigned_char;
                default:
                    return 0;
            }
        case TYPEPOINTER:
            return 1;
        default:
            return 0;
    }
}

Boolean CMach_GetFunctionResultClass(TypeFunc* tfunc) {
    if (func_00436B30(tfunc->functype) == 1) {
        return 1;
    }
    return 0;
}

SInt32 CMach_StructLayoutGetOffset(Type* type, UInt32 qual) {
    SInt32 align;
    SInt32 mask;
    SInt16 pad;
    SInt32 offset;
    UInt32 q;

    if (type->type == TYPEMEMBERPOINTER || type->type == TYPEPOINTER) {
        q = 0;
    } else {
        q = CParser_GetTypeQualifiers(type, qual);
    }
    cmach_curbfsize = 0;
    align = func_00466720(q);
    offset = cmach_structoffset;
    if (align <= 0 && (align = CMach_GetTypeAlign(type)) <= 1) {
        pad = 0;
    } else {
        mask = align - 1;
        pad = (align - (offset & mask)) & mask;
    }
    offset = pad + cmach_structoffset;
    cmach_structoffset = offset + type->size;
    return offset;
}

SInt32 CMach_StructLayoutGetCurSize(void) {
    return cmach_structoffset;
}

void CMach_StructLayoutInitOffset(SInt32 offset) {
    cmach_structoffset = offset;
    cmach_curbfsize = 0;
    cmach_structstart = offset;
}

// 0x464F60: GCC-style bitfield layout. A field never straddles a boundary
// of its declared type's size; a zero-width field aligns to it. Returns the
// byte offset of the field's storage unit; tbitfield->offset gets the bit
// offset within it.
SInt32 CMach_StructLayoutBitfield(TypeBitfield* tbitfield, UInt32 qual) {
    int align;
    int basesize;
    int bits;
    int unit;
    int a;
    int pos;
    int off;
    SInt16 pad;
    UInt32 mask;
    SInt32 offset;

    align = func_00466720(qual);
    unit = 0;
    switch (tbitfield->bitfieldtype->size) {
        case 1:
        case 2:
        case 4:
        case 8:
        case 16:
            basesize = tbitfield->bitfieldtype->size;
            bits = basesize * 8;
            break;
        default:
            CError_FATAL(1208);
    }

    a = basesize;
    if (align) {
        cmach_curbfsize = 0;
        offset = cmach_structoffset;
        a = align;
        if (align <= 0 && (a = CMach_GetTypeAlign(tbitfield->bitfieldtype)) <= 1) {
            pad = 0;
        } else {
            mask = a - 1;
            pad = (a - (offset & mask)) & mask;
        }
        cmach_structoffset += pad;
        a = align;
    }

    if (!cmach_curbfsize && cmach_structoffset) {
        cmach_structstart = cmach_structoffset;
    }

    if (!tbitfield->bitlength) {
        cmach_structstart += a - 1;
        cmach_structstart &= ~(a - 1);
        if (cmach_structoffset < cmach_structstart) {
            cmach_structoffset = cmach_structstart;
        }
        cmach_curbfsize = 0;
        return cmach_structstart;
    }

    if (cmach_structstart & (a - 1)) {
        while (cmach_curbfsize >= 8) {
            cmach_structstart++;
            cmach_curbfsize -= 8;
            if (cmach_structoffset < cmach_structstart) {
                cmach_structoffset = cmach_structstart;
            }
        }
    }

    pos = cmach_structstart * 8 + cmach_curbfsize;
    if (pos / bits != (pos + (UInt8)tbitfield->bitlength - 1) / bits) {
        do {
            if (cmach_curbfsize < 1) {
                cmach_curbfsize = 1;
            }
            cmach_structstart += (cmach_curbfsize + 7) / 8;
            cmach_curbfsize = 0;
            if (cmach_structoffset < cmach_structstart) {
                cmach_structoffset = cmach_structstart;
            }
        } while ((cmach_structstart * 8) / bits !=
                 (cmach_structstart * 8 + (UInt8)tbitfield->bitlength - 1) / bits);
    }

    off = cmach_curbfsize;
    cmach_curbfsize += (UInt8)tbitfield->bitlength;
    cmach_structoffset = (cmach_curbfsize + 7) / 8 + cmach_structstart;
    for (; off >= bits; off -= bits) {
        unit += basesize;
    }
    tbitfield->offset = off;
    return cmach_structstart + unit;
}

// 0x464EE0: is a function result of this type returned through memory?
// Aggregates and the 12-byte member function pointers are.
Boolean Type_RequiresMemoryReturn(Type* type) {
    if (IS_TYPE_CLASS(type) && !CClass_IsPODClass(TYPE_CLASS(type))) {
        return TRUE;
    }
    if (type &&
        (IS_TYPE_ARRAY(type) ||
         IS_TYPE_STRUCT(type) ||
         IS_TYPE_CLASS(type) ||
         (IS_TYPE_MEMBERPOINTER(type) && type->size == 12))) {
        return TRUE;
    }
    return FALSE;
}

// 0x4652D0: the padding to put before a member of this type at offset
SInt16 CMach_MemberAlignValue(Type* type, SInt32 offset) {
    SInt16 align = CMach_GetTypeAlign(type);
    if (align <= 1) {
        return 0;
    }
    return (align - 1) & (align - (offset & (align - 1)));
}

// The alignment of a struct: its most aligned member.
int CMach_GetStructAlign(TypeStruct* tstruct) {
    int best;
    int align;
    StructMember* member;

    best = 1;
    if ((tstruct->size <= 1 && copts.mwabi_bitfields) || tstruct->size < 1) {
        return 1;
    }
    for (member = tstruct->members; member; member = member->next) {
        if (member->type->type == TYPEMEMBERPOINTER || member->type->type == TYPEPOINTER) {
            align = CMach_GetTypeAlign(member->type);
        } else if (!(align = func_00466720(member->qual))) {
            align = CMach_GetTypeAlign(member->type);
        }
        if (align > best) {
            best = align;
        }
    }
    return best;
}

// 0x465380: the alignment of a class: its most aligned base or member, and at
// least a pointer's if it has a vtable pointer
SInt16 CMach_GetClassAlign(TypeClass* tclass) {
    int best;
    int align;
    Boolean hasvptr;
    ClassList* base;
    ObjMemberVar* ivar;

    best = 1;
    for (base = tclass->bases; base; base = base->next) {
        if (base->base->align > best) {
            best = base->base->align;
        }
    }
    for (ivar = tclass->ivars; ivar; ivar = ivar->next) {
        if (ivar->type->type == TYPEMEMBERPOINTER || ivar->type->type == TYPEPOINTER) {
            align = CMach_GetTypeAlign(ivar->type);
        } else if (!(align = func_00466720(ivar->qual))) {
            align = CMach_GetTypeAlign(ivar->type);
        }
        if (align > best) {
            best = align;
        }
    }
    if (tclass->vtable) {
        hasvptr = TRUE;
    } else {
        hasvptr = FALSE;
        for (base = tclass->bases; base; base = base->next) {
            if (base->is_virtual) {
                hasvptr = TRUE;
                break;
            }
        }
    }
    if (hasvptr && best <= 4) {
        best = 4;
    }
    return best;
}

// 2.4.7's, plus the 3.0.1 64-bit conversions and 16-byte check; division
// by zero is an error here.
CInt64 CMach_CalcIntDiadic(Type* type, CInt64 left, short op, CInt64 right) {
    if (is_unsigned(type)) {
        switch (type->size) {
            case 1:
                CInt64_ConvertUInt8(&left);
                CInt64_ConvertUInt8(&right);
                break;
            case 2:
                CInt64_ConvertUInt16(&left);
                CInt64_ConvertUInt16(&right);
                break;
            case 4:
                CInt64_ConvertUInt32(&left);
                CInt64_ConvertUInt32(&right);
                break;
            case 8:
                CInt64_ConvertUInt64(&left);
                CInt64_ConvertUInt64(&right);
                break;
            case 16:
                CError_Error(ERR_ILLEGAL_DATA_SIZE);
                break;
            default:
                CError_FATAL(388);
        }

        switch (op) {
            case '*':
                left = CInt64_MulU(left, right);
                break;
            case '/':
                if (CInt64_IsZero(&right)) {
                    CError_Error(ERR_DIVISION_BY_0);
                } else {
                    left = CInt64_DivU(left, right);
                }
                break;
            case '%':
                if (CInt64_IsZero(&right)) {
                    CError_Error(ERR_DIVISION_BY_0);
                } else {
                    left = CInt64_ModU(left, right);
                }
                break;
            case '+':
                left = CInt64_Add(left, right);
                break;
            case '-':
                left = CInt64_Sub(left, right);
                break;
            case TK_SHL:
                left = CInt64_Shl(left, right);
                break;
            case TK_SHR:
                left = CInt64_ShrU(left, right);
                break;
            case '<':
                CInt64_SetLong(&left, CInt64_LessU(left, right));
                break;
            case '>':
                CInt64_SetLong(&left, CInt64_GreaterU(left, right));
                break;
            case TK_LESS_EQUAL:
                CInt64_SetLong(&left, CInt64_LessEqualU(left, right));
                break;
            case TK_GREATER_EQUAL:
                CInt64_SetLong(&left, CInt64_GreaterEqualU(left, right));
                break;
            case TK_LOGICAL_EQ:
                CInt64_SetLong(&left, CInt64_Equal(left, right));
                break;
            case TK_LOGICAL_NE:
                CInt64_SetLong(&left, CInt64_NotEqual(left, right));
                break;
            case '&':
                left = CInt64_And(left, right);
                break;
            case '^':
                left = CInt64_Xor(left, right);
                break;
            case '|':
                left = CInt64_Or(left, right);
                break;
            case TK_LOGICAL_AND:
                CInt64_SetLong(&left, (!CInt64_IsZero(&left) && !CInt64_IsZero(&right)));
                break;
            case TK_LOGICAL_OR:
                CInt64_SetLong(&left, (!CInt64_IsZero(&left) || !CInt64_IsZero(&right)));
                break;
            default:
                CError_Error(ERR_UNEXPECTED_TOKEN);
        }

        switch (type->size) {
            case 1:
                CInt64_ConvertUInt8(&left);
                break;
            case 2:
                CInt64_ConvertUInt16(&left);
                break;
            case 4:
                CInt64_ConvertUInt32(&left);
                break;
            case 8:
                CInt64_ConvertUInt64(&left);
                break;
        }
    } else {
        switch (type->size) {
            case 1:
                CInt64_ConvertInt8(&left);
                CInt64_ConvertInt8(&right);
                break;
            case 2:
                CInt64_ConvertInt16(&left);
                CInt64_ConvertInt16(&right);
                break;
            case 4:
                CInt64_ConvertInt32(&left);
                CInt64_ConvertInt32(&right);
                break;
            case 8:
                CInt64_ConvertInt64(&left);
                CInt64_ConvertInt64(&right);
                break;
            case 16:
                CError_Error(ERR_ILLEGAL_DATA_SIZE);
                break;
            default:
                CError_FATAL(462);
        }

        switch (op) {
            case '*':
                left = CInt64_Mul(left, right);
                break;
            case '/':
                if (CInt64_IsZero(&right)) {
                    CError_Error(ERR_DIVISION_BY_0);
                } else {
                    left = CInt64_Div(left, right);
                }
                break;
            case '%':
                if (CInt64_IsZero(&right)) {
                    CError_Error(ERR_DIVISION_BY_0);
                } else {
                    left = CInt64_Mod(left, right);
                }
                break;
            case '+':
                left = CInt64_Add(left, right);
                break;
            case '-':
                left = CInt64_Sub(left, right);
                break;
            case TK_SHL:
                left = CInt64_Shl(left, right);
                break;
            case TK_SHR:
                left = CInt64_Shr(left, right);
                break;
            case '<':
                CInt64_SetLong(&left, CInt64_Less(left, right));
                break;
            case '>':
                CInt64_SetLong(&left, CInt64_Greater(left, right));
                break;
            case TK_LESS_EQUAL:
                CInt64_SetLong(&left, CInt64_LessEqual(left, right));
                break;
            case TK_GREATER_EQUAL:
                CInt64_SetLong(&left, CInt64_GreaterEqual(left, right));
                break;
            case TK_LOGICAL_EQ:
                CInt64_SetLong(&left, CInt64_Equal(left, right));
                break;
            case TK_LOGICAL_NE:
                CInt64_SetLong(&left, CInt64_NotEqual(left, right));
                break;
            case '&':
                left = CInt64_And(left, right);
                break;
            case '^':
                left = CInt64_Xor(left, right);
                break;
            case '|':
                left = CInt64_Or(left, right);
                break;
            case TK_LOGICAL_AND:
                CInt64_SetLong(&left, (!CInt64_IsZero(&left) && !CInt64_IsZero(&right)));
                break;
            case TK_LOGICAL_OR:
                CInt64_SetLong(&left, (!CInt64_IsZero(&left) || !CInt64_IsZero(&right)));
                break;
            default:
                CError_Error(ERR_UNEXPECTED_TOKEN);
        }

        switch (type->size) {
            case 1:
                CInt64_ConvertInt8(&left);
                break;
            case 2:
                CInt64_ConvertInt16(&left);
                break;
            case 4:
                CInt64_ConvertInt32(&left);
                break;
            case 8:
                CInt64_ConvertInt64(&left);
                break;
        }
    }

    return left;
}

// 3.0.1 converts 8-byte values as well (0x465AF0)
CInt64 CMach_CalcIntMonadic(Type* type, short op, CInt64 val) {
    if (is_unsigned(type)) {
        switch (type->size) {
            case 1:
                CInt64_ConvertUInt8(&val);
                break;
            case 2:
                CInt64_ConvertUInt16(&val);
                break;
            case 4:
                CInt64_ConvertUInt32(&val);
                break;
            case 8:
                CInt64_ConvertUInt64(&val);
                break;
            default:
                CError_FATAL(529);
        }

        switch (op) {
            case '-':
                val = CInt64_Neg(val);
                break;
            case '~':
                val = CInt64_Inv(val);
                break;
            case '!':
                val = CInt64_Not(val);
                break;
            default:
                CError_Error(ERR_UNEXPECTED_TOKEN);
        }

        switch (type->size) {
            case 1:
                CInt64_ConvertUInt8(&val);
                break;
            case 2:
                CInt64_ConvertUInt16(&val);
                break;
            case 4:
                CInt64_ConvertUInt32(&val);
                break;
            case 8:
                CInt64_ConvertUInt64(&val);
                break;
        }
    } else {
        switch (type->size) {
            case 1:
                CInt64_ConvertInt8(&val);
                break;
            case 2:
                CInt64_ConvertInt16(&val);
                break;
            case 4:
                CInt64_ConvertInt32(&val);
                break;
            case 8:
                CInt64_ConvertInt64(&val);
                break;
            default:
                CError_FATAL(567);
        }

        switch (op) {
            case '-':
                val = CInt64_Neg(val);
                break;
            case '~':
                val = CInt64_Inv(val);
                break;
            case '!':
                val = CInt64_Not(val);
                break;
            default:
                CError_Error(ERR_UNEXPECTED_TOKEN);
        }

        switch (type->size) {
            case 1:
                CInt64_ConvertInt8(&val);
                break;
            case 2:
                CInt64_ConvertInt16(&val);
                break;
            case 4:
                CInt64_ConvertInt32(&val);
                break;
            case 8:
                CInt64_ConvertInt64(&val);
                break;
        }
    }

    return val;
}

// 3.0.1 (0x465A60 down to 0x465510; addresses from the reverse link order)
CInt64 CMach_CalcIntConvertFromFloat(Type* type, Float fval) {
    CInt64 result;

    if (is_unsigned(type)) {
        CInt64_ConvertUFromLongDouble(&result, fval.data.value);
        switch (type->size) {
            case 1:
                CInt64_ConvertUInt8(&result);
                break;
            case 2:
                CInt64_ConvertUInt16(&result);
                break;
            case 4:
                CInt64_ConvertUInt32(&result);
                break;
            case 8:
                CInt64_ConvertUInt64(&result);
                break;
            case 16:
                break;
            default:
                CError_FATAL(623);
        }
    } else {
        CInt64_ConvertFromLongDouble(&result, fval.data.value);
        switch (type->size) {
            case 1:
                CInt64_ConvertInt8(&result);
                break;
            case 2:
                CInt64_ConvertInt16(&result);
                break;
            case 4:
                CInt64_ConvertInt32(&result);
                break;
            case 8:
                CInt64_ConvertInt64(&result);
                break;
            case 16:
                break;
            default:
                CError_FATAL(641);
        }
    }

    return result;
}

extern void func_00455A20(void* mem, void* src, int len);

void CMach_InitIntMem(Type* type, CInt64 val, void* mem) {
    UInt32 lg;

    switch (type->type) {
        case TYPEINT:
            switch (type->size) {
                case 1:
                    lg = val.lo;
                    *(UInt8*)mem = *(UInt8*)&lg;
                    break;
                case 2:
                    lg = val.lo;
                    *(UInt16*)mem = *(UInt16*)&lg;
                    break;
                case 4:
                    lg = val.lo;
                    memcpy(mem, &lg, 4);
                    break;
                case 8:
                    CTool_EndianConvertWord64(val, mem);
                    break;
                case 16:
                    func_00455A20(mem, &val, 16);
                    break;
                default:
                    CError_FATAL(704);
            }
            break;
        default:
            CError_FATAL(709);
    }
}

// 3.0.1 inlines this into CalcFloatDiadic/Monadic, which link before it
Float CMach_CalcFloatConvert(Type* type, Float fval) {
    switch (type->size) {
        case 4:
            fval.data.value = (float)fval.data.value;
            break;
        case 8:
            fval.data.value = (double)fval.data.value;
            break;
        default:
            CError_FATAL(818);
    }
    return fval;
}

Float CMach_CalcFloatDiadic(Type* type, Float left, short op, Float right) {
    switch (op) {
        case '+':
#ifdef __MWERKS__
            left.data.value += right.data.value;
#else
            left.data.value = X87_Arithmetic(left.data.value, X87_ADD, right.data.value);
#endif
            break;
        case '-':
#ifdef __MWERKS__
            left.data.value -= right.data.value;
#else
            left.data.value = X87_Arithmetic(left.data.value, X87_SUB, right.data.value);
#endif
            break;
        case '*':
#ifdef __MWERKS__
            left.data.value *= right.data.value;
#else
            left.data.value = X87_Arithmetic(left.data.value, X87_MUL, right.data.value);
#endif
            break;
        case '/':
#ifdef __MWERKS__
            left.data.value /= right.data.value;
#else
            left.data.value = X87_Arithmetic(left.data.value, X87_DIV, right.data.value);
#endif
            break;
        default:
            CError_FATAL(728);
    }

    return CMach_CalcFloatConvert(type, left);
}

Float CMach_CalcFloatMonadic(Type* type, short op, Float fval) {
    if (op != '-') {
        CError_FATAL(741);
    }

    fval.data.value = -fval.data.value;
    return CMach_CalcFloatConvert(type, fval);
}

Boolean CMach_CalcFloatDiadicBool(Type* type, Float left, short op, Float right) {
    switch (op) {
        case TK_LOGICAL_EQ:
            return left.data.value == right.data.value;
        case TK_LOGICAL_NE:
            return left.data.value != right.data.value;
        case TK_LESS_EQUAL:
            return left.data.value <= right.data.value;
        case TK_GREATER_EQUAL:
            return left.data.value >= right.data.value;
        case '>':
            return left.data.value > right.data.value;
        case '<':
            return left.data.value < right.data.value;
        default:
            CError_FATAL(763);
            return FALSE;
    }
}

char* CMach_FloatScan(char* input, Float* result, Boolean* fail) {
    double resultval;
    char* outpos;

    outpos = ScanFloat(input, &resultval, fail);
    if (*fail) {
        result->data.value = 0.0;
    } else {
        result->data.value = resultval;
    }

    return outpos;
}

Float CMach_CalcFloatConvertFromInt(Type* type, CInt64 val) {
    Float result;

    if (is_unsigned(type)) {
        result.data.value = CInt64_ConvertUToLongDouble(&val);
    } else {
        result.data.value = CInt64_ConvertToLongDouble(&val);
    }

    return result;
}

Boolean CMach_FloatIsZero(Float fval) {
    return fval.data.value == 0.0;
}

Boolean CMach_FloatIsOne(Float fval) {
    return fval.data.value == 1.0;
}

extern const double data_005CCB50; // -1.0 (ours would land in writable .data)

Boolean CMach_FloatIsNegOne(Float fval) {
    return data_005CCB50 == fval.data.value;
}

void CMach_InitFloatMem(Type* type, Float val, void* mem) {
    float f;

    if (type->type == TYPEFLOAT) {
        switch (type->size) {
            case 4:
                f = val.data.value;
                *(float*)mem = f;
                break;
            case 8:
                *(Float*)mem = val;
                break;
            default:
                CError_FATAL(883);
        }
        return;
    }
    CError_FATAL(888);
}

void CMach_PrintFloat(char* buf, Float val) {
    sprintf(buf, "%g", val.data.value);
}

extern char volatileasm; // 0x625E8E (inline asm)
#include "compiler/CInline.h"

// 0x464D50 / 0x464CE0: an inline function's back-end options are a
// snapshot of the first 0x144 bytes of copts plus volatileasm; restoring
// puts back the first 0x140 bytes and volatileasm
void CMach_GetBackEndOptions(struct CInlineBackendOptions* options) {
    int i;

    for (i = 0; i < 0x140; i += 0x10) {
        *(UInt32*)(options->copts + i) = *(UInt32*)((UInt8*)&copts + i);
        *(UInt32*)(options->copts + i + 4) = *(UInt32*)((UInt8*)&copts + i + 4);
        *(UInt32*)(options->copts + i + 8) = *(UInt32*)((UInt8*)&copts + i + 8);
        *(UInt32*)(options->copts + i + 12) = *(UInt32*)((UInt8*)&copts + i + 12);
    }
    *(UInt32*)(options->copts + 0x140) = *(UInt32*)((UInt8*)&copts + 0x140);
    options->volatileasm = volatileasm;
}

void CMach_SetBackEndOptions(struct CInlineBackendOptions* options) {
    int i;

    for (i = 0; i < 0x140; i += 0x10) {
        *(UInt32*)((UInt8*)&copts + i) = *(UInt32*)(options->copts + i);
        *(UInt32*)((UInt8*)&copts + i + 4) = *(UInt32*)(options->copts + i + 4);
        *(UInt32*)((UInt8*)&copts + i + 8) = *(UInt32*)(options->copts + i + 8);
        *(UInt32*)((UInt8*)&copts + i + 12) = *(UInt32*)(options->copts + i + 12);
    }
    volatileasm = options->volatileasm;
}

extern SInt32 data_00624788; // word size (InstrSelection.c)
extern Boolean CClass_ReferenceArgument(TypeClass* tclass);

// 0x464C30: a class's only member variable, if it is a float and no base
// class has member variables
static Type* CMach_ClassSingleFloat(TypeClass* tclass) {
    ObjMemberVar* ivar;
    ClassList* base;

    if (!(ivar = tclass->ivars)) {
        return NULL;
    }
    if (ivar->next) {
        return NULL;
    }
    for (base = tclass->bases; base; base = base->next) {
        if (base->base->ivars && base->base->ivars->access < 3) {
            return NULL;
        }
    }
    if (ivar->type->type == TYPEFLOAT) {
        return ivar->type;
    }
    return NULL;
}

// 0x464BE0: the type of a struct's (or class's) only member, if a float
Type* func_00464BE0(Type* type) {
    StructMember* member;
    Type* t;

    if (type->type == TYPESTRUCT) {
        member = TYPE_STRUCT(type)->members;
        if (!member) {
            t = NULL;
        } else if (!member->next) {
            t = member->type;
            if (t->type != TYPEFLOAT) {
                t = NULL;
            }
        } else {
            t = NULL;
        }
        return t;
    }
    if (type->type != TYPECLASS) {
        return NULL;
    }
    return CMach_ClassSingleFloat(TYPE_CLASS(type));
}

// 0x464B40: as above, but only for float sizes the floating-point library
// passes in registers (copts.fp_library bit 0: single, bit 1: double)
Type* func_00464B40(Type* type) {
    StructMember* member;
    Type* t;

    if (type->type == TYPESTRUCT) {
        member = TYPE_STRUCT(type)->members;
        if (!member) {
            t = NULL;
        } else if (!member->next) {
            t = member->type;
            if (t->type != TYPEFLOAT) {
                t = NULL;
            }
        } else {
            t = NULL;
        }
    } else if (type->type == TYPECLASS) {
        t = CMach_ClassSingleFloat(TYPE_CLASS(type));
    } else {
        t = NULL;
    }

    if (t &&
        t->type == TYPEFLOAT &&
        (((TYPE_INTEGRAL(t)->integral == IT_FLOAT || t->size == 4) && (copts.fp_library & 1)) ||
         ((TYPE_INTEGRAL(t)->integral == IT_DOUBLE || t->size == 8) && (copts.fp_library & 2)))) {
        return t;
    }
    return NULL;
}

// 0x464C90: aggregates larger than a word are passed by address
Boolean CMach_PassResultInHiddenArg(Type* type) {
    if (type &&
        (type->type == TYPEARRAY ||
         type->type == TYPESTRUCT ||
         type->type == TYPECLASS ||
         (type->type == TYPEMEMBERPOINTER && type->size == 12)) &&
        data_00624788 < type->size) {
        return TRUE;
    }
    if (type->type == TYPECLASS && CClass_ReferenceArgument(TYPE_CLASS(type))) {
        return TRUE;
    }
    return FALSE;
}
