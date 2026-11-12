// mwccpsp 3.0.1 SchedMachine.c (0x4B48A0..0x4B5210, the name from its
// statics): the scheduler's machine model for the PSP's Allegrex. Five
// functional units, each busy for its instruction's latency; the unit is
// 1 for loads and otherwise the opcode table's +0x28 field.
#include "compiler/common.h"
#include "compiler/CParser.h"
#include "compiler/InlineAsmMIPS.h"
#include "psp/BE.h"
// no CError.h here (its declarations would renumber statics): its b145 line macro
#ifndef CError_B145_LINE
#define CError_B145_SHIFT 0
#if MWCCPS2 && MWCCPS2_BUILD < 210
#define CError_B145_LINE(line) ((line) + CError_B145_SHIFT)
#else
#define CError_B145_LINE(line) (line)
#endif
#endif

typedef struct MachineInfo {
    int issue_width;
    int renames_registers;
    int (*latency)(MPCode* instr);
    void (*initialize)(void);
    int (*can_issue)(MPCode* instr);
    void (*issue)(MPCode* instr);
    void (*advance_clock)(void);
    void (*dependence)(MPCode* a, MPCode* b);
} MachineInfo;

#if MWCCPS2
// mwccps2 b210 replaces this unit's model with MIPSSimulation.c (the R5900:
// two-issue, six units, two two-slot pipelines); see the end of the file.
// Its data (0x61B720..0x61B808):
static int (*oplatency)(MPCode* instr); // 0x61B720
static SInt32 pipelat[2]; // 0x61B724
static SInt32 pipeflag[2]; // 0x61B72C
static SInt32 pipecount[2][2]; // 0x61B734
static MPCode* pipeinstr[2][2]; // 0x61B744
static SInt32 finished[6]; // 0x61B754
static MPCode* lastinstr[6]; // 0x61B76C
static SInt32 busycount[6]; // 0x61B784
static MPCode* busy[6]; // 0x61B79C
static SInt32 clock; // 0x61B804
#else
static int (*oplatency)(MPCode* instr); // 0x603DC0
static SInt32 finished[5]; // 0x603E54: clock when the unit's last instruction finished
static MPCode* lastinstr[5]; // 0x603E68
static SInt32 busycount[5]; // 0x603E7C
static MPCode* busy[5]; // 0x603E90
static SInt32 clock; // 0x603EA4
#endif

#define UNIT(pc) (((pc)->flags & 2) ? 1 : (data_006115F8[(pc)->op].unk28 & 0xFFF))

// 0x4B4EC0: the latency of an opcode
#if MWCCPS2
// b210 MIPSSimulation.c 0x4C0310: the same latencies as mwccpsp's, case groups
// in another order with their own returns (the block layout follows them)
static int func_004B4EC0(MPCode* instr) {
    switch (instr->op) {
        case 0x7C:
        case 0x7D:
        case MACRO_OP(0x35):
        case MACRO_OP(0x37):
        case MACRO_OP(0x38):
        case MACRO_OP(0x39):
        case MACRO_OP(0x6A):
        case MACRO_OP(0x6B):
            return 36;
        case 0xB6:
        case 0xB7:
#if MWCCPS2_BUILD >= 210 // b145 has no latency for these
        case 0x400:
        case 0x401:
        case 0x405:
        case 0x406:
#endif
        case MACRO_OP(0x5F):
        case MACRO_OP(0x64):
        case MACRO_OP(0x65):
        case MACRO_OP(0x66):
        case MACRO_OP(0x67):
        case MACRO_OP(0x68):
            return 3;
        case 0x7A:
        case 0x7B:
        case MACRO_OP(0x31):
        case MACRO_OP(0x32):
        case MACRO_OP(0x33):
        case MACRO_OP(0x34):
        case MACRO_OP(0x40):
        case MACRO_OP(0x41):
            return 68;
        case 0x80:
        case 0x81:
        case MACRO_OP(0x3A):
        case MACRO_OP(0x3B):
        case MACRO_OP(0x3C):
        case MACRO_OP(0x3D):
        case MACRO_OP(0x3E):
        case MACRO_OP(0x3F):
            return 6;
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
        case 0x4C:
        case 0x4D:
        case 0x4E:
        case 0x4F:
        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57:
        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x5B:
        case 0x5C:
        case 0x5D:
        case 0x5E:
        case 0x5F:
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
            return 2;
        case 0xEC:
        case 0xED:
        case 0x102:
        case 0x103:
            return 4;
        case 0x6C:
            return 6;
        case 0x6D:
            return 2;
        case 0x6E:
        case 0x6F:
        case 0x70:
            return 6;
        case 0x71:
            return 3;
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
            return 6;
        case 0xF2:
            return 59;
        case 0xF3:
            return 31;
        case 0xB4:
            return 6;
        case 0xB5:
            return 5;
        case 0xEE:
        case 0xEF:
        case 0xF0:
        case 0xF1:
        case 0xF4:
        case 0xF5:
        case 0xF6:
        case 0xF7:
        case 0xFC:
        case 0xFD:
        case 0xFE:
        case 0xFF:
        case 0x104:
        case 0x105:
        case 0x106:
        case 0x107:
            return 6;
        case 0x100:
            return 59;
        case 0x101:
            return 31;
        case 0x112:
        case 0x113:
        case 0x114:
        case 0x115:
        case 0x116:
        case 0x117:
        case 0x118:
        case 0x119:
        case 0x11A:
        case 0x11B:
            return 2;
        case 0x10F:
        case 0x11D:
        case 0x11F:
        case 0x121:
            return 9;
        case 0x10E:
        case 0x11C:
        case 0x11E:
        case 0x120:
            return 10;
        case 0x123:
            return 31;
        case 0x122:
            return 59;
        case 0x125:
            return 61;
        case 0x124:
            return 121;
        case 0x174:
        case 0x175:
        case 0x176:
        case MACRO_OP(0x60):
        case MACRO_OP(0x61):
            return 3;
        case 0x17C:
        case 0x17D:
        case 0x17E:
        case 0x17F:
        case 0x180:
        case 0x181:
            return 3;
        case 0x184:
        case 0x185:
            return 34;
        case 0x182:
        case 0x183:
            return 68;
        default:
            return 1;
    }
}
#else
static int func_004B4EC0(MPCode* instr) {
    switch (instr->op) {
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
        case 0x4C:
        case 0x4D:
        case 0x4E:
        case 0x4F:
        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57:
        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x5B:
        case 0x5C:
        case 0x5D:
        case 0x5E:
        case 0x5F:
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x6D:
        case 0x112:
        case 0x113:
        case 0x114:
        case 0x115:
        case 0x116:
        case 0x117:
        case 0x118:
        case 0x119:
        case 0x11A:
        case 0x11B:
            return 2;
        case 0x71:
        case 0xB6:
        case 0xB7:
        case 0x174:
        case 0x175:
        case 0x176:
        case 0x17C:
        case 0x17D:
        case 0x17E:
        case 0x17F:
        case 0x180:
        case 0x181:
        case 0x400:
        case 0x401:
        case 0x405:
        case 0x406:
        case MACRO_OP(0x5F):
        case MACRO_OP(0x60):
        case MACRO_OP(0x61):
        case MACRO_OP(0x64):
        case MACRO_OP(0x65):
        case MACRO_OP(0x66):
        case MACRO_OP(0x67):
        case MACRO_OP(0x68):
            return 3;
        case 0xEC:
        case 0xED:
        case 0x102:
        case 0x103:
            return 4;
        case 0xB5:
            return 5;
        case 0x6C:
        case 0x6E:
        case 0x6F:
        case 0x70:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x80:
        case 0x81:
        case 0xB4:
        case 0xEE:
        case 0xEF:
        case 0xF0:
        case 0xF1:
        case 0xF4:
        case 0xF5:
        case 0xF6:
        case 0xF7:
        case 0xFC:
        case 0xFD:
        case 0xFE:
        case 0xFF:
        case 0x104:
        case 0x105:
        case 0x106:
        case 0x107:
        case MACRO_OP(0x3A):
        case MACRO_OP(0x3B):
        case MACRO_OP(0x3C):
        case MACRO_OP(0x3D):
        case MACRO_OP(0x3E):
        case MACRO_OP(0x3F):
            return 6;
        case 0x10F:
        case 0x11D:
        case 0x11F:
        case 0x121:
            return 9;
        case 0x10E:
        case 0x11C:
        case 0x11E:
        case 0x120:
            return 10;
        case 0xF3:
        case 0x101:
        case 0x123:
            return 31;
        case 0x184:
        case 0x185:
            return 34;
        case 0x7C:
        case 0x7D:
        case MACRO_OP(0x35):
        case MACRO_OP(0x37):
        case MACRO_OP(0x38):
        case MACRO_OP(0x39):
        case MACRO_OP(0x6A):
        case MACRO_OP(0x6B):
            return 36;
        case 0xF2:
        case 0x100:
        case 0x122:
            return 59;
        case 0x125:
            return 61;
        case 0x7A:
        case 0x7B:
        case 0x182:
        case 0x183:
        case MACRO_OP(0x31):
        case MACRO_OP(0x32):
        case MACRO_OP(0x33):
        case MACRO_OP(0x34):
        case MACRO_OP(0x40):
        case MACRO_OP(0x41):
            return 68;
        case 0x124:
            return 121;
        default:
            return 1;
    }
}

#endif

#if MWCCPS2
// 0x4C0600: b210's operation latency (R5900 multiply/divide, MMI and COP2)
static int ps2_oplatency(MPCode* instr) {
    UInt16 op;
    int r;

    op = instr->op;
    switch (op) {
        case 0x7C:
        case 0x7D:
        case 0x1D3:
        case 0x1D4:
        case MACRO_OP(0x35):
        case MACRO_OP(0x36):
        case MACRO_OP(0x37):
        case MACRO_OP(0x38):
        case MACRO_OP(0x39):
        case MACRO_OP(0x6A):
        case MACRO_OP(0x6B):
            return 37;
        case 0xB6:
        case 0xB7:
        case 0x1D5:
        case 0x1D6:
        case 0x1D7:
        case 0x1D8:
        case 0x1DB:
        case 0x1DC:
        case MACRO_OP(0x5F):
        case MACRO_OP(0x64):
        case MACRO_OP(0x65):
        case MACRO_OP(0x66):
        case MACRO_OP(0x67):
        case MACRO_OP(0x68):
            return 4;
        case 0x22C:
        case 0x22D:
        case 0x231:
        case 0x232:
        case 0x233:
        case 0x23F:
        case 0x240:
        case 0x244:
        case 0x245:
        case 0x246:
            return 4;
        case 0x21C:
        case 0x21D:
        case 0x21E:
            return 37;
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            return 2;
    }
    if (op > 0x21E && op <= 0x2E7) {
        switch (op) {
            case 0x27C:
            case 0x2D8:
                r = 7;
                break;
            case 0x281:
            case 0x282:
            case 0x283:
            case 0x288:
            case 0x28D:
            case 0x2D2:
            case 0x2D5:
                r = 1;
                break;
            case 0x2D4:
                r = 13;
                break;
            case 0x2E7:
                r = 3;
                break;
            default:
                r = 4;
                break;
        }
        return r;
    }
    return func_004B4EC0(instr);
}
#endif

// 0x4B51B0: latency
int func_004B51B0(MPCode* instr) {
    if (instr->flags & 2) {
        return 3;
    }
    switch (instr->op) {
        case 0xB6:
        case 0xB7:
        case 0x400:
        case 0x401:
        case 0x405:
        case 0x406:
        case MACRO_OP(0x5F):
        case MACRO_OP(0x64):
        case MACRO_OP(0x65):
        case MACRO_OP(0x66):
        case MACRO_OP(0x67):
        case MACRO_OP(0x68):
            return 5;
        default:
            return oplatency(instr);
    }
}

#if MWCCPS2
// b210 MIPSSimulation.c 0x4BF6A0: the units an instruction needs, as a mask
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 10 // b145 asserts
static int ps2_UnitMask(MPCode* instr) {
    SInt32 f;
    SInt32 unit;

    f = data_006115F8[instr->op].unk28;
    unit = f & 0xFFF;
    switch (f & 0xFFF0000) {
        case 0x1000000:
            return 0x1000000;
        case 0x400000:
            return 0x100000;
        case 0x800000:
            return 0x800000;
        case 0x100000:
            return 0x100000;
        case 0x200000:
            return 0x200000;
        case 0x30000:
            return 0x30000;
        case 0x80000:
            return 0x80000;
        case 0x40000:
            return 0x40000;
        case 0x10000:
            return 0x10000;
        case 0x20000:
            return 0x20000;
    }
    switch (unit) {
        case 0:
            if ((instr->flags & 2) || (instr->flags & 4)) {
                return 0x40000;
            }
            if (instr->flags & 1) {
                return 0x80000;
            }
            return 0x30000;
        case 1:
            return 0x40000;
        case 3:
            return 0x100000;
        case 4:
            return 0x200000;
        default:
            CError_Internal("MIPSSimulation.c", CError_B145_LINE(1812));
            return 0x30000;
        case 2:
            break;
    }
    CError_Internal("MIPSSimulation.c", CError_B145_LINE(1815));
    return 0;
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// 0x4BF7F0: can_issue
int func_004B4B50(MPCode* instr) {
    SInt32 f;

    if (ps2_UnitMask(instr) == 0x30000) {
        if (busy[0] && busy[1]) {
            return 0;
        }
    } else if (ps2_UnitMask(instr) == 0x10000) {
        f = data_006115F8[instr->op].unk28;
        if ((f & 0x10000) && (f & 0x10000000)) {
            if (pipeinstr[0][0] && pipeinstr[0][1]) {
                return 0;
            }
            if (pipeinstr[0][0] && !pipeinstr[0][1] && pipecount[0][0] > 2) {
                return 0;
            }
            if (!pipeinstr[0][0] && pipeinstr[0][1] && pipecount[0][1] > 2) {
                return 0;
            }
            if (busy[0]) {
                return 0;
            }
        } else if ((f & 0x10000) && (f & 0x20000000)) {
            if (pipeflag[0]) {
                return 0;
            }
            if (busy[0]) {
                return 0;
            }
        } else if (busy[0]) {
            return 0;
        }
    } else if (ps2_UnitMask(instr) == 0x20000) {
        f = data_006115F8[instr->op].unk28;
        if ((f & 0x20000) && (f & 0x10000000)) {
            if (pipeinstr[1][0] && pipeinstr[1][1]) {
                return 0;
            }
            if (pipeinstr[1][0] && !pipeinstr[1][1] && pipecount[1][0] > 2) {
                return 0;
            }
            if (!pipeinstr[1][0] && pipeinstr[1][1] && pipecount[1][1] > 2) {
                return 0;
            }
            if (busy[1]) {
                return 0;
            }
        } else if ((f & 0x20000) && (f & 0x20000000)) {
            if (pipeflag[1]) {
                return 0;
            }
            if (busy[1]) {
                return 0;
            }
        } else if (busy[1]) {
            return 0;
        }
    } else if (ps2_UnitMask(instr) == 0x40000) {
        if (busy[2]) {
            return 0;
        }
    } else if (ps2_UnitMask(instr) == 0x80000) {
        if (busy[3]) {
            return 0;
        }
    } else if (ps2_UnitMask(instr) == 0x100000) {
        if (busy[0] || busy[4]) {
            return 0;
        }
    } else if (ps2_UnitMask(instr) == 0x200000) {
        if (busy[0] || busy[5]) {
            return 0;
        }
    } else if (ps2_UnitMask(instr) == 0x400000) {
        if (busy[2] || busy[4]) {
            return 0;
        }
    } else if (ps2_UnitMask(instr) == 0x800000) {
        if (busy[2] || busy[5]) {
            return 0;
        }
    } else if (ps2_UnitMask(instr) == 0x1000000) {
        if (busy[0] || busy[1]) {
            return 0;
        }
    }
    return 1;
}

// 0x4BFB00: issue
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 10 // b145 asserts
void func_004B4AE0(MPCode* instr) {
    SInt32 f;

    if (ps2_UnitMask(instr) == 0x30000) {
        if (busy[0]) {
            busy[1] = instr;
            busycount[1] = oplatency(instr);
        } else {
            busy[0] = instr;
            busycount[0] = oplatency(instr);
        }
    } else if (ps2_UnitMask(instr) == 0x10000) {
        f = data_006115F8[instr->op].unk28;
        if ((f & 0x10000) && (f & 0x10000000)) {
            if (busy[0]) {
                CError_Internal("MIPSSimulation.c", CError_B145_LINE(1488));
            } else {
                busy[0] = instr;
                busycount[0] = 1;
                if (pipeinstr[0][0]) {
                    pipeinstr[0][1] = instr;
                    pipecount[0][1] = oplatency(instr);
                } else {
                    pipeinstr[0][0] = instr;
                    pipecount[0][0] = oplatency(instr);
                }
            }
        } else if ((f & 0x10000) && (f & 0x20000000)) {
            busy[0] = instr;
            busycount[0] = 1;
            pipelat[0] = oplatency(instr);
        } else {
            busy[0] = instr;
            busycount[0] = oplatency(instr);
        }
    } else if (ps2_UnitMask(instr) == 0x20000) {
        f = data_006115F8[instr->op].unk28;
        if ((f & 0x20000) && (f & 0x10000000)) {
            if (busy[1]) {
                CError_Internal("MIPSSimulation.c", CError_B145_LINE(1517));
            } else {
                busy[1] = instr;
                busycount[1] = 1;
                if (pipeinstr[1][0]) {
                    pipeinstr[1][1] = instr;
                    pipecount[1][1] = oplatency(instr);
                } else {
                    pipeinstr[1][0] = instr;
                    pipecount[1][0] = oplatency(instr);
                }
            }
        } else if ((f & 0x20000) && (f & 0x20000000)) {
            busy[1] = instr;
            busycount[1] = 1;
            pipelat[1] = oplatency(instr);
        } else {
            busy[1] = instr;
            busycount[1] = oplatency(instr);
        }
    } else if (ps2_UnitMask(instr) == 0x40000) {
        busy[2] = instr;
        busycount[2] = oplatency(instr);
    } else if (ps2_UnitMask(instr) == 0x80000) {
        busy[3] = instr;
        busycount[3] = oplatency(instr);
    } else if (ps2_UnitMask(instr) == 0x100000) {
        busy[0] = instr;
        busycount[0] = 1;
        busy[4] = instr;
        busycount[4] = oplatency(instr);
    } else if (ps2_UnitMask(instr) == 0x200000) {
        busy[0] = instr;
        busycount[0] = 1;
        busy[5] = instr;
        busycount[5] = oplatency(instr);
    } else if (ps2_UnitMask(instr) == 0x400000) {
        busy[2] = instr;
        busycount[2] = oplatency(instr);
        busy[4] = instr;
        busycount[4] = oplatency(instr);
    } else if (ps2_UnitMask(instr) == 0x800000) {
        busy[2] = instr;
        busycount[2] = oplatency(instr);
        busy[5] = instr;
        busycount[5] = oplatency(instr);
    } else if (ps2_UnitMask(instr) == 0x1000000) {
        busy[0] = instr;
        busycount[0] = oplatency(instr);
        busy[1] = instr;
        busycount[1] = oplatency(instr);
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

static int ps2_oplatency(MPCode* instr);

// 0x4BFEC0: initialize
void func_004B4DE0(void) {
    int i;
    int j;

    clock = 0;
    for (i = 0; i < 6; i++) {
        busy[i] = NULL;
        busycount[i] = 0;
        lastinstr[i] = NULL;
        finished[i] = 0;
    }
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            pipecount[i][j] = 0;
            pipeinstr[i][j] = NULL;
        }
    }
    for (i = 0; i < 2; i++) {
        pipelat[i] = 0;
        pipeflag[i] = 0;
    }
    oplatency = ps2_oplatency;
}

// 0x4C0030: advance_clock
void func_004B4A90(void) {
    int i;
    int j;

    clock++;
    for (i = 0; i < 6; i++) {
        if (busycount[i] != 0) {
            busycount[i]--;
        }
        if (busycount[i] == 0) {
            lastinstr[i] = busy[i];
            finished[i] = clock;
            busy[i] = NULL;
        }
    }
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            if (pipecount[i][j] > 0) {
                pipecount[i][j]--;
            }
            if (pipecount[i][j] == 0) {
                pipeinstr[i][j] = NULL;
            }
        }
    }
    for (i = 0; i < 2; i++) {
        if (pipelat[i] > 0) {
            pipelat[i]--;
        }
        if (pipelat[i] == 0) {
            pipeflag[i] = 0;
        }
    }
}

#else
// 0x4B4DE0: initialize
void func_004B4DE0(void) {
    int i;

    clock = 0;
    for (i = 0; i < 5; i++) {
        busy[i] = NULL;
        busycount[i] = 0;
        lastinstr[i] = NULL;
        finished[i] = 0;
    }
    oplatency = func_004B4EC0;
}

// 0x4B4B50: can_issue
int func_004B4B50(MPCode* instr) {
    int unit;
    MPCode* last;

    unit = UNIT(instr);
    if (busy[unit]) {
        return 0;
    }
    last = lastinstr[unit];
    if ((instr->flags & 2) && last && (last->flags & 4) && clock < finished[unit] + 1) {
        return 0;
    }
    if ((instr->op == 0xA8 || instr->op == 0xA9) &&
        last &&
        (last->op == 0xB6 ||
         last->op == 0xB7 ||
         last->op == 0x400 ||
         last->op == 0x401 ||
         last->op == 0x405 ||
         last->op == 0x406) &&
        clock < finished[unit]) {
        return 0;
    }
    return 1;
}

// 0x4B4AE0: issue
void func_004B4AE0(MPCode* instr) {
    busy[UNIT(instr)] = instr;
    busycount[UNIT(instr)] = oplatency(instr);
}

// 0x4B4A90: advance_clock
void func_004B4A90(void) {
    int i;

    clock++;
    for (i = 0; i < 5; i++) {
        if (--busycount[i] == 0) {
            lastinstr[i] = busy[i];
            finished[i] = clock;
            busy[i] = NULL;
        }
    }
}

#endif

// 0x4B48A0: dependence; marks a (0x2000000) when it must not go in b's delay slot
void func_004B48A0(MPCode* a, MPCode* b) {
    UInt16 op;
    MOperand* o;
    MOperand* p;
    int i;
    int j;

    if (!(b->flags & 0x20000000)) {
        a->flags |= 0x2000000;
        return;
    }
    if (((a->flags & 2) || (data_006115F8[a->op].flags2 & 0x5000)) && copts.mips_isa < 2) {
        a->flags |= 0x2000000;
        return;
    }
    op = b->op;
    if (data_006115F8[op].flags & 8) {
        for (i = 0, o = a->args; i < a->argcount; i++, o++) {
            if (o->kind == 0 && o->rclass == 0) {
                if (o->reg == 0x1F) {
                    a->flags |= 0x2000000;
                    return;
                }
                if (op == 0x3B && b->args[0].reg == o->reg && (o->flags & 2)) {
                    a->flags |= 0x2000000;
                    return;
                }
                if (op == 0x3D && b->args[1].reg == o->reg && (o->flags & 2)) {
                    a->flags |= 0x2000000;
                    return;
                }
            }
        }
        return;
    }
#if MWCCPS2
    if (op != 0x3A) {
        if (op != 0x39) {
            for (i = 0, o = a->args; i < a->argcount; i++, o++) {
                if (o->kind == 0 && (o->flags & 2)) {
                    for (j = 0, p = b->args; j < b->argcount; j++, p++) {
                        if (p->kind == 0 && p->rclass == o->rclass && p->reg == o->reg) {
                            a->flags |= 0x2000000;
                            return;
                        }
                    }
                }
            }
        }
    }
#else
    if (op == 0x3A || op == 0x39) {
        return;
    }
    for (i = 0, o = a->args; i < a->argcount; i++, o++) {
        if (o->kind == 0 && (o->flags & 2)) {
            for (j = 0, p = b->args; j < b->argcount; j++, p++) {
                if (p->kind == 0 && p->rclass == o->rclass && p->reg == o->reg) {
                    a->flags |= 0x2000000;
                    return;
                }
            }
        }
    }
#endif
}

#if MWCCPSP_NATIVE
// The models CodeGen hands the scheduler: with and without the delay-slot
// dependence hook.
#if MWCCPS2
#define SCHED_ISSUE_WIDTH 2 // b210 (0x5E9070/0x5E9090): the R5900 issues two instructions a cycle
#else
#define SCHED_ISSUE_WIDTH 1
#endif
MachineInfo data_005D082C = {
    SCHED_ISSUE_WIDTH,
    0,
    func_004B51B0,
    func_004B4DE0,
    func_004B4B50,
    func_004B4AE0,
    func_004B4A90,
    func_004B48A0
};
MachineInfo data_005D084C = {
    SCHED_ISSUE_WIDTH,
    0,
    func_004B51B0,
    func_004B4DE0,
    func_004B4B50,
    func_004B4AE0,
    func_004B4A90,
    NULL
};
#endif
