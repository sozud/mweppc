// mwccpsp 3.0.1 SchedMachine.c (0x4B48A0..0x4B5210, the name from its
// statics): the scheduler's machine model for the PSP's Allegrex. Five
// functional units, each busy for its instruction's latency; the unit is
// 1 for loads and otherwise the opcode table's +0x28 field.
#include "compiler/common.h"
#include "compiler/CParser.h"
#include "compiler/InlineAsmMIPS.h"
#include "psp/BE.h"

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

static int (*oplatency)(MPCode* instr); // 0x603DC0
static SInt32 finished[5]; // 0x603E54: clock when the unit's last instruction finished
static MPCode* lastinstr[5]; // 0x603E68
static SInt32 busycount[5]; // 0x603E7C
static MPCode* busy[5]; // 0x603E90
static SInt32 clock; // 0x603EA4

#define UNIT(pc) (((pc)->flags & 2) ? 1 : (data_006115F8[(pc)->op].unk28 & 0xFFF))

// 0x4B4EC0: the latency of an opcode
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
        case 0x577:
        case 0x578:
        case 0x579:
        case 0x57C:
        case 0x57D:
        case 0x57E:
        case 0x57F:
        case 0x580:
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
        case 0x552:
        case 0x553:
        case 0x554:
        case 0x555:
        case 0x556:
        case 0x557:
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
        case 0x54D:
        case 0x54F:
        case 0x550:
        case 0x551:
        case 0x582:
        case 0x583:
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
        case 0x549:
        case 0x54A:
        case 0x54B:
        case 0x54C:
        case 0x558:
        case 0x559:
            return 68;
        case 0x124:
            return 121;
        default:
            return 1;
    }
}

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
        case 0x577:
        case 0x57C:
        case 0x57D:
        case 0x57E:
        case 0x57F:
        case 0x580:
            return 5;
        default:
            return oplatency(instr);
    }
}

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
}

#if MWCCPSP_NATIVE
// The models CodeGen hands the scheduler: with and without the delay-slot
// dependence hook.
MachineInfo data_005D082C = {
    1, 0, func_004B51B0, func_004B4DE0, func_004B4B50, func_004B4AE0, func_004B4A90, func_004B48A0
};
MachineInfo data_005D084C = {
    1, 0, func_004B51B0, func_004B4DE0, func_004B4B50, func_004B4AE0, func_004B4A90, NULL
};
#endif
