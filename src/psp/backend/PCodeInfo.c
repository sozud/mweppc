#include "compiler/mwcc_core.h"
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
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
#if MWCC_CORE == MWCC_CORE_B52
#undef CError_B52_SHIFT
#define CError_B52_SHIFT (-418)
#endif
MOperand* func_0048FAE0(MPCode* pc) {
    int i;
    MOperand* op;

    for (i = 0, op = pc->args; i < pc->argcount; i++, op++) {
        if (op->kind == 6) {
            return op;
        }
    }
    CError_Internal("PCodeInfo.c", CError_B145_LINE(1509));
    return NULL;
}
#if MWCC_CORE == MWCC_CORE_B52
#undef CError_B52_SHIFT
#define CError_B52_SHIFT (0)
#endif
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// The opcode of a nop.
SInt16 func_004903F0(void) {
    return 0xBA;
}

// the opcode table (OpcodeDef in InlineAsmMIPS.h): format, nargs, and flags
// (6: loads and stores)

// How many bytes the load or store accesses.
#if MWCCPS2
// b210 (0x49B4F0): its own opcode numbering (64- and 128-bit loads/stores; the
// case lists are read from the original's compare chain); func_00490400 calls it
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
BE_NOINLINE SInt32 func_0048FBE0(UInt16 op) {
#else
__declspec(noinline) SInt32 func_0048FBE0(UInt16 op) {
#endif
    if (!(data_006115F8[op].flags & 6)) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        // b69 0x49AA80 returns 0 for an opcode that does not access memory
        // (LocalCSE.c's is128bitop 0x49AC60 asks for every copied value);
        // only the switch's default asserts
#else
        CError_FATAL(1384);
#endif
        return 0;
    }
    switch (op) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0x91):
        case OPC(0x92):
        case OPC(0xC3):
        case OPC(0x453):
        case OPC(0x45A):
        case OPC(0x47F):
#else
        case 0x91:
        case 0x92:
        case 0xC3:
        case 0x453:
        case 0x45A:
        case 0x47F:
#endif
            return 1;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0x94):
        case OPC(0x95):
        case OPC(0xCA):
        case OPC(0x45B):
        case OPC(0x45C):
        case OPC(0x480):
#else
        case 0x94:
        case 0x95:
        case 0xCA:
        case 0x45B:
        case 0x45C:
        case 0x480:
#endif
            return 2;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0x9B):
        case OPC(0x9D):
        case OPC(0x9E):
        case OPC(0x9F):
        case OPC(0xA0):
        case OPC(0xA1):
        case OPC(0xA2):
        case OPC(0xA3):
        case OPC(0xC1):
        case OPC(0xCB):
        case OPC(0xCC):
        case OPC(0xCD):
        case OPC(0xCE):
        case OPC(0xCF):
        case OPC(0xD0):
        case OPC(0xD1):
        case OPC(0x45D):
        case OPC(0x45F):
        case OPC(0x460):
        case OPC(0x461):
        case OPC(0x462):
        case OPC(0x463):
        case OPC(0x464):
        case OPC(0x465):
        case OPC(0x466):
        case OPC(0x477):
        case OPC(0x481):
        case OPC(0x482):
        case OPC(0x483):
        case OPC(0x484):
        case OPC(0x485):
        case OPC(0x486):
        case OPC(0x487):
        case OPC(0x488):
#else
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
        case 0x45D:
        case 0x45F:
        case 0x460:
        case 0x461:
        case 0x462:
        case 0x463:
        case 0x464:
        case 0x465:
        case 0x466:
        case 0x477:
        case 0x481:
        case 0x482:
        case 0x483:
        case 0x484:
        case 0x485:
        case 0x486:
        case 0x487:
        case 0x488:
#endif
            return 4;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0x93):
        case OPC(0x96):
        case OPC(0x97):
        case OPC(0x98):
        case OPC(0x99):
        case OPC(0x9A):
        case OPC(0x9C):
        case OPC(0xC2):
        case OPC(0xC4):
        case OPC(0xC5):
        case OPC(0xC6):
        case OPC(0xC7):
        case OPC(0xC8):
        case OPC(0xC9):
        case OPC(0x454):
        case OPC(0x455):
        case OPC(0x456):
        case OPC(0x457):
        case OPC(0x458):
        case OPC(0x459):
        case OPC(0x45E):
        case OPC(0x478):
        case OPC(0x479):
        case OPC(0x47A):
        case OPC(0x47B):
        case OPC(0x47C):
        case OPC(0x47D):
        case OPC(0x47E):
#else
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
        case 0x454:
        case 0x455:
        case 0x456:
        case 0x457:
        case 0x458:
        case 0x459:
        case 0x45E:
        case 0x478:
        case 0x479:
        case 0x47A:
        case 0x47B:
        case 0x47C:
        case 0x47D:
        case 0x47E:
#endif
            return 8;
            // b210 keeps these in groups of their own (separate return blocks)
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0x10B):
        case OPC(0x10D):
#else
        case 0x10B:
        case 0x10D:
#endif
            return 4;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0x10A):
        case OPC(0x10C):
#else
        case 0x10A:
        case 0x10C:
#endif
            return 8;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0x164):
        case OPC(0x173):
#else
        case 0x164:
        case 0x173:
#endif
            return 8;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0x1D1):
        case OPC(0x1D2):
        case OPC(0x263):
        case OPC(0x268):
        case OPC(0x284):
        case OPC(0x285):
        case OPC(0x286):
        case OPC(0x287):
        case OPC(0x289):
        case OPC(0x28A):
        case OPC(0x28B):
        case OPC(0x28C):
        case OPC(0x467):
        case OPC(0x468):
        case OPC(0x489):
        case OPC(0x48A):
#else
        case 0x1D1:
        case 0x1D2:
        case 0x263:
        case 0x268:
        case 0x284:
        case 0x285:
        case 0x286:
        case 0x287:
        case 0x289:
        case 0x28A:
        case 0x28B:
        case 0x28C:
        case 0x467:
        case 0x468:
        case 0x489:
        case 0x48A:
#endif
            return 16;
        default:
            CError_FATAL(1380);
            return 0;
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#else
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
SInt32 func_0048FBE0(UInt16 op) {
    if (!(data_006115F8[op].flags & 6)) {
        CError_FATAL(1384);
        return 0;
    }
    switch (op) {
        case 0x91:
        case 0x92:
        case 0xC3:
        case MACRO_OP(0x49):
        case MACRO_OP(0x50):
        case MACRO_OP(0x75):
            return 1;
        case 0x94:
        case 0x95:
        case 0xCA:
        case MACRO_OP(0x51):
        case MACRO_OP(0x52):
        case MACRO_OP(0x76):
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
        case MACRO_OP(0x53):
        case MACRO_OP(0x55):
        case MACRO_OP(0x56):
        case MACRO_OP(0x57):
        case MACRO_OP(0x58):
        case MACRO_OP(0x59):
        case MACRO_OP(0x5A):
        case MACRO_OP(0x5B):
        case MACRO_OP(0x5C):
        case MACRO_OP(0x6D):
        case MACRO_OP(0x77):
        case MACRO_OP(0x78):
        case MACRO_OP(0x79):
        case MACRO_OP(0x7A):
        case MACRO_OP(0x7B):
        case MACRO_OP(0x7C):
        case MACRO_OP(0x7D):
        case MACRO_OP(0x7E):
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
        case MACRO_OP(0x4A):
        case MACRO_OP(0x4B):
        case MACRO_OP(0x4C):
        case MACRO_OP(0x4D):
        case MACRO_OP(0x4E):
        case MACRO_OP(0x4F):
        case MACRO_OP(0x54):
        case MACRO_OP(0x6E):
        case MACRO_OP(0x6F):
        case MACRO_OP(0x70):
        case MACRO_OP(0x71):
        case MACRO_OP(0x72):
        case MACRO_OP(0x73):
        case MACRO_OP(0x74):
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
        case MACRO_OP(0x9B):
        case MACRO_OP(0x9D):
            return 4;
        case 0x4CC:
        case 0x4CD:
        case 0x4CE:
        case 0x4CF:
        case 0x4D0:
        case MACRO_OP(0x9C):
        case MACRO_OP(0x9E):
        case MACRO_OP(0x9F):
        case MACRO_OP(0xA0):
        case MACRO_OP(0xA1):
            return 16;
        default:
            CError_FATAL(1380);
            return 0;
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#endif

extern SInt16 data_00625E48; // move
extern SInt16 data_00625E74; // 16-byte register move

// An instruction copying register operand src to dst.
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 3 // b145 asserts
MPCode* func_00490280(MOperand* src, MOperand* dst) {
    if (src->kind == 0) {
        if (src->rclass == 0) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            return makepcode(
                (src->flags & 0x2000) ? data_00625E74 : data_00625E48, dst->reg, src->reg
            );
#else
            return makepcode(
                (src->flags & 0x2000) ? data_00625E74 : data_00625E48, dst->reg, src->reg
            );
#endif
        } else if (src->rclass == 1) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            return makepcode(
                (src->flags & 0x8000) ? MACRO_OP(0x9A) : MACRO_OP(0x99), dst->reg, src->reg
            );
#else
            return makepcode(
                (src->flags & 0x8000) ? MACRO_OP(0x9A) : MACRO_OP(0x99), dst->reg, src->reg
            );
#endif
        } else if (src->rclass == 5) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            return makepcode(OPC(0x2AF), 0xF, dst->reg, src->reg);
#else
            return makepcode(0x2AF, 0xF, dst->reg, src->reg);
#endif
        } else if (src->rclass == 3) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            return makepcode(OPC(0x281), dst->reg, src->reg, 0);
#else
            return makepcode(0x281, dst->reg, src->reg, 0);
#endif
        }
    }
    CError_FATAL(979);
    return NULL;
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// A copy of kind 1 (a register move) or 2..7 (0x59A / 0x599 with 8, 16 or
// 32).
MPCode* func_0048FDC0(int kind, MOperand* src, MOperand* dst) {
    switch (kind) {
        case 1:
            return func_00490280(src, dst);
        case 2:
            return makepcode(MACRO_OP(0x82), dst->reg, src->reg, 8);
        case 3:
            return makepcode(MACRO_OP(0x81), dst->reg, src->reg, 8);
        case 4:
            return makepcode(MACRO_OP(0x82), dst->reg, src->reg, 0x10);
        case 5:
            return makepcode(MACRO_OP(0x81), dst->reg, src->reg, 0x10);
        case 6:
            return makepcode(MACRO_OP(0x82), dst->reg, src->reg, 0x20);
        case 7:
            return makepcode(MACRO_OP(0x81), dst->reg, src->reg, 0x20);
        default:
            CError_FATAL(1183);
            return NULL;
    }
}

extern SInt32 data_00624788;

// 0x48FB20: the move kind (func_0048FDC0's) of a load or store's value; 0
// when it has none.
#if MWCCPS2
int func_0048FB20(MPCode* pc) {
    switch (pc->op) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0x91):
#else
        case 0x91:
#endif
        case MACRO_OP(0x50):
            return 3;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0x92):
#else
        case 0x92:
#endif
        case MACRO_OP(0x49):
            return 2;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0x94):
#else
        case 0x94:
#endif
        case MACRO_OP(0x51):
            return 5;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0x95):
#else
        case 0x95:
#endif
        case MACRO_OP(0x52):
            return 4;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0x9D):
#else
        case 0x9D:
#endif
        case MACRO_OP(0x55):
            return (UInt8)((data_00624788 == 4) ? (UInt8)1 : (UInt8)7);
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0xA0):
#else
        case 0xA0:
#endif
        case MACRO_OP(0x5C):
            return (UInt8)((data_00624788 == 4) ? (UInt8)1 : (UInt8)6);
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0x93):
        case OPC(0x99):
        case OPC(0x9A):
#else
        case 0x93:
        case 0x99:
        case 0x9A:
#endif
            return 1;
        default:
            return 0;
    }
}
#else
int func_0048FB20(MPCode* pc) {
    switch (pc->op) {
        case 0x91:
        case MACRO_OP(0x50):
            return 3;
        case 0x92:
        case MACRO_OP(0x49):
            return 2;
        case 0x93:
        case 0x99:
        case 0x9A:
            return 1;
        case 0x94:
        case MACRO_OP(0x51):
            return 5;
        case 0x95:
        case MACRO_OP(0x52):
            return 4;
        case 0x9D:
        case MACRO_OP(0x55):
            return (data_00624788 == 4) ? 1 : 7;
        case 0xA0:
        case MACRO_OP(0x5C):
            return (data_00624788 == 4) ? 1 : 6;
        default:
            return 0;
    }
}
#endif

#undef CError_B145_SHIFT
#define CError_B145_SHIFT 17 // b145 asserts
extern SInt32 func_0048FBE0(UInt16 op); // bytes an opcode moves

// 0x48FFA0: how load can take the value store wrote (func_0048FDC0's kind,
// 0 when it cannot)
#if MWCCPS2 && MWCCPS2_BUILD < 198
#include "compiler/Alias.h"
// b145 (0x49AF40): the store opcode whose list holds the stores a load
// (flags 0x20002) can read; other instructions keep their own opcode
UInt16 storeopcode(MPCode* pcode) {
    UInt16 op = pcode->op;

    if (pcode->flags & 0x20002) {
        switch (pcode->op) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0x91):
#else
            case 0x91:
#endif
                op = 0xC3;
                break;
            case MACRO_OP(0x50):
                op = MACRO_OP(0x75);
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0x94):
#else
            case 0x94:
#endif
                op = 0xCA;
                break;
            case MACRO_OP(0x51):
                op = MACRO_OP(0x76);
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0x9D):
#else
            case 0x9D:
#endif
                op = 0xCB;
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0x9E):
#else
            case 0x9E:
#endif
                op = 0xD0;
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0x9F):
#else
            case 0x9F:
#endif
                op = 0xD1;
                break;
            case MACRO_OP(0x55):
                op = MACRO_OP(0x77);
                break;
            case MACRO_OP(0x5A):
                op = MACRO_OP(0x7D);
                break;
            case MACRO_OP(0x5B):
                op = MACRO_OP(0x7E);
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0xA1):
#else
            case 0xA1:
#endif
                op = 0xCD;
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0xA2):
#else
            case 0xA2:
#endif
                op = 0xCE;
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0xA3):
#else
            case 0xA3:
#endif
                op = 0xCF;
                break;
            case MACRO_OP(0x56):
            case MACRO_OP(0x57):
                op = MACRO_OP(0x79);
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0x93):
#else
            case 0x93:
#endif
                op = 0xC4;
                break;
            case MACRO_OP(0x4A):
                op = MACRO_OP(0x6F);
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0x99):
#else
            case 0x99:
#endif
                op = 0xC8;
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0x9A):
#else
            case 0x9A:
#endif
                op = 0xC9;
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0x9C):
#else
            case 0x9C:
#endif
                op = 0xC8;
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0x96):
#else
            case 0x96:
#endif
                op = 0xC5;
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0x97):
#else
            case 0x97:
#endif
                op = 0xC6;
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0x98):
#else
            case 0x98:
#endif
                op = 0xC7;
                break;
            case MACRO_OP(0x4F):
                op = MACRO_OP(0x74);
                break;
            case MACRO_OP(0x4B):
                op = MACRO_OP(0x70);
                break;
            case MACRO_OP(0x4C):
                op = MACRO_OP(0x71);
                break;
            case MACRO_OP(0x4D):
                op = MACRO_OP(0x72);
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0x1D1):
#else
            case 0x1D1:
#endif
                op = 0x1D2;
                break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
            case OPC(0x263):
#else
            case 0x263:
#endif
                op = 0x268;
                break;
            case MACRO_OP(0x5D):
                op = MACRO_OP(0x7F);
                break;
        }
    }
    return op;
}

// b145 (0x49B490): whether load can take the value store wrote, by the
// store's alias size (no copy kinds yet)
SInt32 func_0048FFA0(MPCode* store, MPCode* load) {
    SInt32 size;

    if (!store->alias) {
        return 0;
    }
    size = store->alias->size;
    if (copts.mips_option41) {
        if (size > 16) {
            return 0;
        }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        if ((store->op == OPC(0x1D2) && load->op == OPC(0x1D1)) ||
            (store->op == MACRO_OP(0x7F) && load->op == MACRO_OP(0x5D))) {
#else
        if ((store->op == 0x1D2 && load->op == 0x1D1) ||
            (store->op == MACRO_OP(0x7F) && load->op == MACRO_OP(0x5D))) {
#endif
            return 1;
        }
    }
    if (size > 8) {
        return 0;
    }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    if ((store->op == OPC(0xC4) && load->op == OPC(0x93)) ||
        (store->op == MACRO_OP(0x6F) && load->op == MACRO_OP(0x4A))) {
#else
    if ((store->op == 0xC4 && load->op == 0x93) ||
        (store->op == MACRO_OP(0x6F) && load->op == MACRO_OP(0x4A))) {
#endif
        return 1;
    }
    if (size == 4) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        if ((store->op == OPC(0xCB) && load->op == OPC(0x9D)) ||
            (store->op == MACRO_OP(0x77) && load->op == MACRO_OP(0x55))) {
#else
        if ((store->op == 0xCB && load->op == 0x9D) ||
            (store->op == MACRO_OP(0x77) && load->op == MACRO_OP(0x55))) {
#endif
            return 1;
        }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        if ((store->op == OPC(0xCD) && load->op == OPC(0xA1)) ||
            (store->op == MACRO_OP(0x79) && load->op == MACRO_OP(0x56))) {
#else
        if ((store->op == 0xCD && load->op == 0xA1) ||
            (store->op == MACRO_OP(0x79) && load->op == MACRO_OP(0x56))) {
#endif
            return 1;
        }
    }
    if (size == 1) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        if ((store->op == OPC(0xC3) && load->op == OPC(0x91)) ||
            (store->op == MACRO_OP(0x75) && load->op == MACRO_OP(0x50))) {
#else
        if ((store->op == 0xC3 && load->op == 0x91) ||
            (store->op == MACRO_OP(0x75) && load->op == MACRO_OP(0x50))) {
#endif
            return 1;
        }
    }
    return 0;
}
#elif MWCCPS2
// b210 (0x49B8C0): copts+0x41 for the quadword pairs, a doubleword pair, and
// the pairs with the same result tested together
SInt32 func_0048FFA0(MPCode* store, MPCode* load) {
    SInt32 size = func_0048FBE0(store->op);

    if (size > 16) {
        return 0;
    }
    if (copts.mips_option41) {
        if ((store->op == 0x1D2 && load->op == 0x1D1) ||
            (store->op == MACRO_OP(0x7F) && load->op == MACRO_OP(0x5D))) {
            return 1;
        }
    }
    if (size > 8) {
        return 0;
    }
    if ((store->op == 0xC4 && load->op == 0x93) ||
        (store->op == MACRO_OP(0x6F) && load->op == MACRO_OP(0x4A))) {
        return 1;
    }
    if (size > 4) {
        return 0;
    }
    if ((store->op == 0xCD && load->op == 0xA1) ||
        (store->op == MACRO_OP(0x79) && load->op == MACRO_OP(0x56))) {
        return 1;
    }
    if ((store->op == 0xCB && load->op == 0x9D) ||
        (store->op == MACRO_OP(0x77) && load->op == MACRO_OP(0x55))) {
        return 1;
    }
    if ((store->op == 0xCB && load->op == 0x94) ||
        (store->op == MACRO_OP(0x77) && load->op == MACRO_OP(0x51))) {
        return 5;
    }
    if ((store->op == 0xCA && load->op == 0x94) ||
        (store->op == MACRO_OP(0x76) && load->op == MACRO_OP(0x51))) {
#if MWCCPS2_BUILD < 210
        return 1; // b198 (0x49B660) copies these two pairs whole
#else
        return 5;
#endif
    }
    if ((store->op == 0xC3 && load->op == 0x91) ||
        (store->op == MACRO_OP(0x75) && load->op == MACRO_OP(0x50))) {
#if MWCCPS2_BUILD < 210
        return 1;
#else
        return 3;
#endif
    }
    if ((store->op == 0xCB && load->op == 0x91) ||
        (store->op == MACRO_OP(0x77) && load->op == MACRO_OP(0x50))) {
        return 5; // mwccpsp: 3
    }
    if ((store->op == 0xCA && load->op == 0x91) ||
        (store->op == MACRO_OP(0x76) && load->op == MACRO_OP(0x50))) {
        return 3;
    }
    return 0;
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#else
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
        if (store->op == MACRO_OP(0x7F) && load->op == MACRO_OP(0x5D)) {
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
    if (store->op == MACRO_OP(0x79) && load->op == MACRO_OP(0x56)) {
        return 1;
    }
    if (store->op == 0xCB && load->op == 0x9D) {
        return 1;
    }
    if (store->op == MACRO_OP(0x77) && load->op == MACRO_OP(0x55)) {
        return 1;
    }
    if (store->op == 0xCB && load->op == 0x94) {
        return 5;
    }
    if (store->op == MACRO_OP(0x77) && load->op == MACRO_OP(0x51)) {
        return 5;
    }
    if (store->op == 0xCA && load->op == 0x94) {
        return 5;
    }
    if (store->op == MACRO_OP(0x76) && load->op == MACRO_OP(0x51)) {
        return 5;
    }
    if (store->op == 0xC3 && load->op == 0x91) {
        return 3;
    }
    if (store->op == MACRO_OP(0x75) && load->op == MACRO_OP(0x50)) {
        return 3;
    }
    if (store->op == 0xCB && load->op == 0x91) {
        return 3;
    }
    if (store->op == MACRO_OP(0x77) && load->op == MACRO_OP(0x50)) {
        return 3;
    }
    if (store->op == 0xCA && load->op == 0x91) {
        return 3;
    }
    if (store->op == MACRO_OP(0x76) && load->op == MACRO_OP(0x50)) {
        return 3;
    }
    return 0;
}
#endif

extern SInt16 data_00625E3E; // frame register

// Whether the instruction just copies or makes a value it can be dropped or
// repeated for: x = $zero op imm, a nop-like 0x90 / 0x55E, or 0x55C off $gp or
// the frame register.
#if MWCCPS2 && !MWCC_CORE
// mwccps2 (b198 is_location_independent, b210): the register and the opcode in locals,
// the stack-pointer test first in the switch (the PS2 b198 decompilation's)
int func_004901D0(MPCode* pc) {
    UInt16 op;
    SInt32 reg;

    if (pc->flags & 0x100) {
        return 0;
    }
    if (pc->unk38) {
        return 0;
    }
    if (pc->argcount == 3 &&
        (UInt8)(pc->args[2].kind - 2) <= 1 &&
        pc->args[1].kind == 0 &&
        pc->args[1].rclass == 0) {
        reg = pc->args[1].reg;
        if (reg == 0) {
            return 1;
        }
    }
    op = pc->op;
    switch (op) {
        case MACRO_OP(0x44):
            if (pc->args[2].kind == 0 &&
                pc->args[1].rclass == 0 &&
                ((UInt16)pc->args[2].reg == 28 ||
                 (UInt16)pc->args[2].reg == (UInt16)data_00625E3E)) {
                return 1;
            }
            return 0;
        case 0x90:
        case MACRO_OP(0x46):
            return 1;
    }
    return 0;
}
#else
int func_004901D0(MPCode* pc) {
    if (pc->flags & 0x100) {
        return 0;
    }
    if (pc->unk38) {
        return 0;
    }
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    if (pc->argcount == 3 &&
        (pc->args[2].kind == 2 || pc->args[2].kind == 3) &&
        pc->args[1].kind == 0 &&
        pc->args[1].rclass == 0 &&
        pc->args[1].reg == 0) {
#else
    if (pc->argcount == 3 &&
        (pc->args[2].kind == 2 || pc->args[2].kind == 3) &&
        pc->args[1].kind == 0 &&
        pc->args[1].rclass == 0 &&
        pc->args[1].reg == 0) {
#endif
        return 1;
    }
    switch (pc->op) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        case OPC(0x90):
#else
        case 0x90:
#endif
        case MACRO_OP(0x46):
            return 1;
        case MACRO_OP(0x44):
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
#endif

// In a heavily weighted (loop) block, emit 0x5C3 with the configured value
// (from the CodeGen panel) unless optimizing for size.
void func_00490370(void) {
    CInt64 n;

    if (copts.optimizesize || !copts.mips_option3A) {
#if MWCC_CORE != MWCC_CORE_B52
        return;
    }
    if (data_006246B4->weight < 8) {
#endif
        return;
    }
    if ((SInt8)copts.mips_option39 > 2) {
        CInt64_SetLong(&n, (SInt8)copts.mips_option39);
        emitpcode(BE_NUM_OPDEFS - 2, n);
        data_006246B4->last->flags |= 0x100;
    }
}

extern struct Alias* make_alias(Object* obj, SInt32 offset, SInt32 size);
extern Boolean is_volatile_object(Object* obj);
extern Boolean is_const_object(Object* obj);

// vformatpcode: build an instruction from its opcode's operand format string
// and the arguments.
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 1 // b145 asserts
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
#if MWCCPS2 && MWCCPS2_BUILD < 198
#if !MWCC_CORE || MWCC_CORE > MWCC_CORE_B52
                arg->flags = effect;
#endif
                format++;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                arg->flags = effect;
#else
                pc->flags = 0x100; // b145
#endif
#else
                format++;
                arg->flags = effect;
#endif
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
#if MWCCPS2
                arg->reg = 3; // mwccps2 b210's numbering
#else
                arg->reg = 2;
#endif
                format++;
                arg->flags = effect;
                break;
            case 'R':
                arg->kind = 0;
                arg->rclass = 2;
#if MWCCPS2
                arg->reg = 4; // mwccps2 b210's numbering
#else
                arg->reg = 3;
#endif
                format++;
                arg->flags = effect;
                break;
            case 'Y':
                arg->kind = 0;
                arg->rclass = 2;
#if MWCCPS2
                arg->reg = 6; // mwccps2 b210's numbering
#else
                arg->reg = 7;
#endif
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
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                                pc->alias = make_alias(
                                    (Object*)count,
                                    memarg->value - ((Object*)MOP_OBJ(*memarg))->u.var.offset,
                                    func_0048FBE0(pc->op)
                                );
#else
                                pc->alias = make_alias(
                                    (Object*)count,
                                    memarg->value - ((Object*)MOP_OBJ(*memarg))->u.var.offset,
                                    func_0048FBE0(pc->op)
                                );
#endif
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
#if MWCCPS2
                arg->reg = 2; // mwccps2 b210's numbering
#else
                arg->reg = 1;
#endif
                format++;
                arg->flags = effect;
                break;
            case 'K':
                switch (pc->op) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                    case OPC(0x68):
                    case OPC(0x69):
                    case OPC(0x6A):
                    case OPC(0x6B):
                    case OPC(0x1CD):
                    case OPC(0x1CE):
                    case OPC(0x1CF):
                    case OPC(0x1D0):
#else
                    case 0x68:
                    case 0x69:
                    case 0x6A:
                    case 0x6B:
                    case 0x1CD:
                    case 0x1CE:
                    case 0x1CF:
                    case 0x1D0:
#endif
                        arg->kind = 0;
#if MWCCPS2
#define K_REG(psp) ((psp) + 1) // mwccps2 b210 numbers these one higher
#else
#define K_REG(psp) (psp)
#endif
                        if (pc->args[1].reg == 0x14) {
                            arg->rclass = 6;
                            arg->reg = K_REG(3);
                            arg->flags = effect;
                        } else if (pc->args[1].reg == 0x15) {
                            arg->rclass = 6;
                            arg->reg = K_REG(0);
                            arg->flags = effect;
                        } else if (pc->args[1].reg == 0x16) {
                            arg->rclass = 6;
                            arg->reg = K_REG(1);
                            arg->flags = effect;
                        } else {
                            arg->kind = 7;
                        }
                        break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                    case OPC(0x26C):
                    case OPC(0x273):
                    case OPC(0x27C):
                    case OPC(0x296):
                    case OPC(0x29D):
                    case OPC(0x2B3):
                    case OPC(0x2BA):
                    case OPC(0x2C2):
                    case OPC(0x2C9):
                    case OPC(0x2D4):
                    case OPC(0x2D8):
                    case OPC(0x2DB):
                    case OPC(0x2E2):
#else
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
#endif
                        arg->kind = 0;
                        arg->rclass = 4;
                        arg->reg = 0x16;
                        arg->flags = effect;
                        break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                    case OPC(0x26B):
                    case OPC(0x272):
                    case OPC(0x295):
                    case OPC(0x29C):
                    case OPC(0x2A3):
                    case OPC(0x2B2):
                    case OPC(0x2B9):
                    case OPC(0x2C1):
                    case OPC(0x2C8):
                    case OPC(0x2DA):
                    case OPC(0x2E1):
#else
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
#endif
                        arg->kind = 0;
                        arg->rclass = 4;
                        arg->reg = 0x15;
                        arg->flags = effect;
                        break;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
                    case OPC(0x2D1):
                    case OPC(0x2D3):
                    case OPC(0x2D5):
#else
                    case 0x2D1:
                    case 0x2D3:
                    case 0x2D5:
#endif
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
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
