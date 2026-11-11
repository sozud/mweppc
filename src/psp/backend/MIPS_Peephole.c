// mwccpsp 3.0.1 MIPS_Peephole.c: the peephole patterns (0x5469C0..0x54AAB0;
// the framework that runs them is Peephole.c, far away at 0x4AFBE0).
// Each pattern gets the instruction it was registered for and returns
// nonzero if it changed something. Register uses carry their defining
// instruction in value2 (set up by MIPS_Peephole.c).
#include "compiler/common.h"
#include "compiler/CError.h"
#include "compiler/CInt64.h"
#include "compiler/CParser.h"
#include "compiler/CompilerTools.h"
#include "compiler/objects.h"
#include "psp/BE.h"
#include "compiler/InlineAsmMIPS.h"

extern void func_004AFD20(UInt16 op, int (*func)(MPCode* pc));
extern int func_004AFDD0(MPCode* a, MPCode* b, MOperand* reg); // used between
extern int func_004AFE40(MPCode* a, MPCode* b, MOperand* reg); // defined between

// the opcode table's flags (data_00611610 is data_006115F8[0].flags): OpcodeDef
// in InlineAsmMIPS.h

typedef struct RegBits {
    UInt32* bits;
    UInt32 unk04;
    UInt32 unk08;
} RegBits;

extern RegBits data_00625424[7]; // registers live out of the function, per class
extern UInt16 data_00625E48; // the move opcode
extern UInt16 data_00625E74; // the 64-bit move opcode
extern SInt32 func_0048FBE0(UInt16 op); // bytes a memory instruction accesses
extern SInt32 func_0048FFA0(MPCode* a, MPCode* b);
extern MPCode* func_0048FDC0(SInt32 kind, MOperand* a, MOperand* b);
extern Boolean may_alias(MPCode* a, MPCode* b);
extern MPCode* func_00480420(int op, SInt16 nargs);

// the instruction defining a register use
#define DEF(o) ((MPCode*)MOP_OBJ(o))
#define COPY_OP(d, s) \
    ((d).value2 = (s).value2, (d) = (s)) // the struct copy carries the native fields
#define MAKE_MOVE(pc) ((pc)->op = data_00625E48, (pc)->flags |= 0x10, (pc)->argcount = 2)
#define REG_LIVE(o) (SHR32(data_00625424[(char)(o).rclass].bits[(o).reg >> 5], (o).reg) & 1)
#define SET_LIVE(o) (data_00625424[(char)(o).rclass].bits[(o).reg >> 5] |= BIT32((o).reg))
#define SAME_REG(a, b) \
    ((a)->kind == (b)->kind && (a)->reg == (b)->reg && (a)->rclass == (b)->rclass)

static inline void SET_OP(MPCode* pc, UInt16 op) {
    UInt32 flags;

    flags = pc->flags;
    pc->op = op;
    pc->flags = (flags & 0x180E0) | data_006115F8[op].flags;
}

// the other form of opcode change
#define SET_OP2(pc, o) \
    do { \
        UInt32 flags_ = (pc)->flags & 0x180E0; \
        (pc)->op = (o); \
        (pc)->flags = data_006115F8[(o)].flags | flags_; \
    } while (0)

// turns pc into a nop (0xBA)
static inline void NOP_OUT(MPCode* pc) {
    UInt32 flags;

    flags = pc->flags & 0x180E0;
    pc->op = 0xBA;
    pc->flags = data_006115F8[0xBA].flags | flags;
    pc->argcount = 0;
}

// is the register defined after def (up to pc), or by def itself?
static inline Boolean Redefined(MPCode* pc, MPCode* def, MOperand* reg) {
    MOperand* op;
    int n;

    if (func_004AFE40(pc, def, reg)) {
        return 1;
    }
    for (op = def->args, n = def->argcount; n--; op++) {
        if (SAME_REG(op, reg) && (op->flags & 2)) {
            return 1;
        }
    }
    return 0;
}

// is the register used or defined by an instruction between a and b?
static inline Boolean TouchedBetween(MPCode* a, MPCode* b, MOperand* reg) {
    MPCode* pc;
    MOperand* op;
    int n;

    for (pc = a->prev; pc != b; pc = pc->prev) {
        for (op = pc->args, n = pc->argcount; n--; op++) {
            if (SAME_REG(op, reg) && (op->flags & 3)) {
                return 1;
            }
        }
    }
    return 0;
}

// 0x5469C0: the patterns, per opcode. data_0062482C is set while registers
// are still virtual (before colouring).
extern int func_00547340(MPCode* pc);
extern int func_00547360(MPCode* pc);
extern int func_005473D0(MPCode* pc);
extern int func_005473F0(MPCode* pc);
extern int func_00547600(MPCode* pc);
extern int func_00547820(MPCode* pc);
extern int func_00547A40(MPCode* pc);
extern int func_00547AE0(MPCode* pc);
extern int func_00547B60(MPCode* pc);
extern int func_00547BF0(MPCode* pc);
extern int func_00547C60(MPCode* pc);
extern int func_00547D60(MPCode* pc);
extern int func_00547E70(MPCode* pc);
extern int func_00547EE0(MPCode* pc);
extern int func_00547F50(MPCode* pc);
extern int func_00547FC0(MPCode* pc);
extern int func_00548030(MPCode* pc);
extern int func_00548060(MPCode* pc);
extern int func_00548230(MPCode* pc);
extern int func_005483A0(MPCode* pc);
extern int func_00548510(MPCode* pc);
extern int func_005486E0(MPCode* pc);
extern int func_00548900(MPCode* pc);
extern int func_005489A0(MPCode* pc);
extern int func_00548A60(MPCode* pc);
extern int func_00548B00(MPCode* pc);
extern int func_00548BC0(MPCode* pc);
extern int func_00548D10(MPCode* pc);
extern int func_00548D30(MPCode* pc);
extern int func_00548D50(MPCode* pc);
extern int func_00548D70(MPCode* pc);
extern int func_00548F60(MPCode* pc);
extern int func_00549400(MPCode* pc);
extern int func_00549440(MPCode* pc);
extern int func_005494B0(MPCode* pc);
extern int func_005494E0(MPCode* pc);
extern int func_00549640(MPCode* pc);
extern int func_005496A0(MPCode* pc);
extern int func_005497E0(MPCode* pc);
extern int func_00549870(MPCode* pc);
extern int func_00549910(MPCode* pc);
extern int func_005499A0(MPCode* pc);
extern int func_00549A40(MPCode* pc);
extern int func_00549B40(MPCode* pc);
extern int func_00549C40(MPCode* pc);
extern int func_00549D50(MPCode* pc);
extern int func_00549DA0(MPCode* pc);
extern int func_0054A560(MPCode* pc);
extern int func_0054A5F0(MPCode* pc);
extern int func_0054A690(MPCode* pc);
extern int func_0054A760(MPCode* pc);
extern int func_0054A800(MPCode* pc);
extern int func_0054A910(MPCode* pc);
extern UInt16 data_00625AD0;
extern UInt16 data_00625B58;
extern SInt32 data_0062482C;

void func_005469C0(void) {
    func_004AFD20(data_00625B58, func_00549B40);
    func_004AFD20(data_00625AD0, func_00549A40);
    func_004AFD20(0x96, func_005486E0);
    func_004AFD20(0x563, func_005486E0);
    func_004AFD20(0x56E, func_005486E0);
    func_004AFD20(0xA1, func_005486E0);
    func_004AFD20(0xCB, func_00548510);
    func_004AFD20(0x58F, func_00548510);
    func_004AFD20(0xCA, func_00548510);
    func_004AFD20(0x58E, func_00548510);
    func_004AFD20(0xC3, func_00548510);
    func_004AFD20(0x58D, func_00548510);
    if (data_0062482C) {
        func_004AFD20(0x599, func_00549400);
        func_004AFD20(0x599, func_00549DA0);
        func_004AFD20(0x599, func_0054A5F0);
        func_004AFD20(0x59A, func_00549400);
        func_004AFD20(0x59A, func_00549DA0);
        func_004AFD20(0x59A, func_0054A690);
        func_004AFD20(0x59A, func_00549D50);
        func_004AFD20(0xD2, func_0054A910);
        func_004AFD20(0xD2, func_0054A560);
        func_004AFD20(0xD8, func_0054A800);
        func_004AFD20(0x7, func_0054A760);
        func_004AFD20(0x51A, func_0054A760);
        func_004AFD20(0x58D, func_005499A0);
        func_004AFD20(0xC3, func_005499A0);
        func_004AFD20(0x58D, func_00549910);
        func_004AFD20(0xC3, func_00549910);
        func_004AFD20(0x58E, func_00549870);
        func_004AFD20(0xCA, func_00549870);
        func_004AFD20(0x58E, func_005497E0);
        func_004AFD20(0xCA, func_005497E0);
        func_004AFD20(0xBE, func_005494B0);
        func_004AFD20(0xBC, func_005494E0);
        func_004AFD20(0xE5, func_00549C40);
        func_004AFD20(0x5AF, func_00549C40);
        func_004AFD20(0x568, func_005496A0);
        func_004AFD20(0x561, func_005496A0);
        func_004AFD20(0x569, func_005496A0);
        func_004AFD20(0x56A, func_005496A0);
        func_004AFD20(0x56D, func_005496A0);
        func_004AFD20(0x574, func_005496A0);
        func_004AFD20(0x562, func_005496A0);
        func_004AFD20(0x56E, func_005496A0);
        func_004AFD20(0x563, func_005496A0);
        func_004AFD20(0x560, func_005496A0);
        func_004AFD20(0x58D, func_005496A0);
        func_004AFD20(0x58E, func_005496A0);
        func_004AFD20(0x58F, func_005496A0);
        func_004AFD20(0x587, func_005496A0);
        func_004AFD20(0x591, func_005496A0);
        func_004AFD20(0x588, func_005496A0);
        func_004AFD20(0x584, func_005496A0);
        func_004AFD20(0x91, func_005496A0);
        func_004AFD20(0x92, func_005496A0);
        func_004AFD20(0x94, func_005496A0);
        func_004AFD20(0x95, func_005496A0);
        func_004AFD20(0x9D, func_005496A0);
        func_004AFD20(0xA0, func_005496A0);
        func_004AFD20(0x93, func_005496A0);
        func_004AFD20(0xA1, func_005496A0);
        func_004AFD20(0x96, func_005496A0);
        func_004AFD20(0xC3, func_005496A0);
        func_004AFD20(0xCA, func_005496A0);
        func_004AFD20(0xCB, func_005496A0);
        func_004AFD20(0xC4, func_005496A0);
        func_004AFD20(0xCD, func_005496A0);
        func_004AFD20(0xC5, func_005496A0);
        func_004AFD20(0x568, func_00549640);
        func_004AFD20(0x561, func_00549640);
        func_004AFD20(0x569, func_00549640);
        func_004AFD20(0x56A, func_00549640);
        func_004AFD20(0x56D, func_00549640);
        func_004AFD20(0x574, func_00549640);
        func_004AFD20(0x562, func_00549640);
        func_004AFD20(0x56E, func_00549640);
        func_004AFD20(0x563, func_00549640);
        func_004AFD20(0x560, func_00549640);
        func_004AFD20(0x58D, func_00549640);
        func_004AFD20(0x58E, func_00549640);
        func_004AFD20(0x58F, func_00549640);
        func_004AFD20(0x587, func_00549640);
        func_004AFD20(0x591, func_00549640);
        func_004AFD20(0x588, func_00549640);
        func_004AFD20(0x584, func_00549640);
        func_004AFD20(0x591, func_00547FC0);
        func_004AFD20(0xCD, func_00547F50);
        func_004AFD20(0x58D, func_00548BC0);
        func_004AFD20(0xC3, func_00548BC0);
        func_004AFD20(0x58E, func_00548BC0);
        func_004AFD20(0xCA, func_00548BC0);
        func_004AFD20(0x58F, func_00548BC0);
        func_004AFD20(0xCB, func_00548BC0);
        func_004AFD20(0x587, func_00548BC0);
        func_004AFD20(0xC4, func_00548BC0);
        func_004AFD20(0x599, func_00549440);
        func_004AFD20(0xA5, func_00547AE0);
        func_004AFD20(0xAD, func_00547A40);
        func_004AFD20(0xAD, func_00548060);
        func_004AFD20(0xA5, func_00547C60);
        func_004AFD20(0x1, func_00548F60);
        func_004AFD20(0x4, func_00548F60);
        func_004AFD20(0xDC, func_00548F60);
        func_004AFD20(0xDD, func_00548F60);
        func_004AFD20(0x59A, func_00548230);
        func_004AFD20(0x51A, func_005483A0);
        func_004AFD20(0x7, func_005483A0);
        if (copts.mips_isa < 3) {
            return;
        }
        func_004AFD20(0x588, func_00547EE0);
        func_004AFD20(0x584, func_00547EE0);
        func_004AFD20(0xC5, func_00547E70);
        func_004AFD20(0x7F, func_00547D60);
        func_004AFD20(0x7F, func_00547BF0);
        func_004AFD20(0xB1, func_00547B60);
        func_004AFD20(0xB1, func_00548030);
        if (copts.mips_isa < 4) {
            return;
        }
        if (copts.mips_option11) {
            func_004AFD20(0xFB, func_00547820);
            func_004AFD20(0xFA, func_00547600);
            func_004AFD20(0xED, func_005473F0);
            func_004AFD20(0xEC, func_005473D0);
            func_004AFD20(0x103, func_00547360);
            func_004AFD20(0x102, func_00547340);
        }
        func_004AFD20(0x56E, func_00548D70);
        func_004AFD20(0xA1, func_00548D70);
        func_004AFD20(0x560, func_00548D30);
        func_004AFD20(0x563, func_00548D30);
        func_004AFD20(0x96, func_00548D30);
        func_004AFD20(0x591, func_00548D50);
        func_004AFD20(0xCD, func_00548D50);
        func_004AFD20(0x584, func_00548D10);
        func_004AFD20(0x588, func_00548D10);
        func_004AFD20(0xC5, func_00548D10);
    } else {
        func_004AFD20(0x58D, func_00548A60);
        func_004AFD20(0xC3, func_00548A60);
        func_004AFD20(0x58D, func_00548B00);
        func_004AFD20(0xC3, func_00548B00);
        func_004AFD20(0x58E, func_00548900);
        func_004AFD20(0xCA, func_00548900);
        func_004AFD20(0x58E, func_005489A0);
        func_004AFD20(0xCA, func_005489A0);
        func_004AFD20(0x59A, func_00549DA0);
        func_004AFD20(0x599, func_00549DA0);
    }
}

static int func_00547380(MPCode* pc, UInt16 op, UInt16 newop1, UInt16 newop2);
static int func_00547410(MPCode* pc, UInt16 op, UInt16 newop);

int func_00547340(MPCode* pc) {
    return func_00547380(pc, 0xB4, 0x11C, 0x120);
}

int func_00547360(MPCode* pc) {
    return func_00547380(pc, 0xB5, 0x11D, 0x121);
}

static int func_00547470(MPCode* pc, UInt16 op, int k, MPCode* def);

// either source of pc made by op: fold it into a three-operand instruction
static int func_00547380(MPCode* pc, UInt16 op, UInt16 newop1, UInt16 newop2) {
    MPCode* def1;
    MPCode* def2;

    def1 = DEF(pc->args[1]);
    def2 = DEF(pc->args[2]);
    if (def1->op == op) {
        return func_00547470(pc, newop1, 2, def1);
    }
    if (def2->op == op) {
        return func_00547470(pc, newop2, 1, def2);
    }
    return 0;
}

int func_005473D0(MPCode* pc) {
    return func_00547410(pc, 0xB4, 0x10E);
}

int func_005473F0(MPCode* pc) {
    return func_00547410(pc, 0xB5, 0x10F);
}

static int func_00547410(MPCode* pc, UInt16 op, UInt16 newop) {
    MPCode* def1;
    MPCode* def2;

    def1 = DEF(pc->args[1]);
    def2 = DEF(pc->args[2]);
    if (def1->op == op) {
        return func_00547470(pc, newop, 2, def1);
    }
    if (def2->op == op) {
        return func_00547470(pc, newop, 1, def2);
    }
    return 0;
}

static int func_00547470(MPCode* pc, UInt16 op, int k, MPCode* def) {
    if (Redefined(pc, def, &def->args[1])) {
        return 0;
    }
    if (Redefined(pc, def, &def->args[2])) {
        return 0;
    }
    SET_LIVE(def->args[1]);
    SET_LIVE(def->args[2]);
    insertpcodebefore(
        pc, makepcode(op, pc->args[0].reg, pc->args[k].reg, def->args[1].reg, def->args[2].reg)
    );
    NOP_OUT(pc);
    return 1;
}

// def computes pc's source; let def write pc's destination instead (as op)
static inline int MergeInto(MPCode* pc, MPCode* def, UInt16 op) {
    if (func_004AFDD0(pc, def, &def->args[0])) {
        return 0;
    }
    SET_OP(def, op);
    COPY_OP(def->args[0], pc->args[0]);
    NOP_OUT(pc);
    return 1;
}

int func_00547600(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[1]);
    if (def->op == 0x10E) {
        return MergeInto(pc, def, 0x11E);
    } else if (def->op == 0x11C) {
        return MergeInto(pc, def, 0x120);
    } else if (def->op == 0x11E) {
        return MergeInto(pc, def, 0x10E);
    } else if (def->op == 0x120) {
        return MergeInto(pc, def, 0x11C);
    }
    return 0;
}

int func_00547820(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[1]);
    if (def->op == 0x10F) {
        return MergeInto(pc, def, 0x11F);
    } else if (def->op == 0x11D) {
        return MergeInto(pc, def, 0x121);
    } else if (def->op == 0x11F) {
        return MergeInto(pc, def, 0x10F);
    } else if (def->op == 0x121) {
        return MergeInto(pc, def, 0x11D);
    }
    return 0;
}

int func_00547A40(MPCode* pc) {
    MPCode* def;

    if (pc->flags & 0x100) {
        return 0;
    }
    def = DEF(pc->args[0]);
    if (def->op != 0xA5) {
        return 0;
    }
    if (func_004AFE40(pc, def, &def->args[1])) {
        return 0;
    }
    COPY_OP(pc->args[0], pc->args[1]);
    COPY_OP(pc->args[1], def->args[1]);
    MAKE_MOVE(pc);
    pc->op = 0x5B1;
    return 1;
}

int func_00547AE0(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[1]);
    if (pc->flags & 0x100) {
        return 0;
    }
    if (def->op != 0xAD) {
        return 0;
    }
    if (func_004AFE40(pc, def, &def->args[0])) {
        return 0;
    }
    COPY_OP(pc->args[1], def->args[0]);
    MAKE_MOVE(pc);
    return 1;
}

int func_00547B60(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[0]);
    if (def->op != 0x7F) {
        return 0;
    }
    if (func_004AFE40(pc, def, &def->args[1])) {
        return 0;
    }
    COPY_OP(pc->args[0], pc->args[1]);
    COPY_OP(pc->args[1], def->args[1]);
    MAKE_MOVE(pc);
    pc->op = 0x5B2;
    return 1;
}

int func_00547BF0(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[1]);
    if (def->op != 0xB1) {
        return 0;
    }
    if (func_004AFE40(pc, def, &def->args[0])) {
        return 0;
    }
    COPY_OP(pc->args[1], def->args[0]);
    MAKE_MOVE(pc);
    return 1;
}

int func_00547C60(MPCode* pc) {
    MPCode* def;
    UInt16 op;

    def = DEF(pc->args[1]);
    if (def->op == 0xA1) {
        op = 0x9D;
    } else if (def->op == 0x56E) {
        op = 0x56D;
    } else {
        return 0;
    }
    if (func_004AFDD0(pc, def, &def->args[0])) {
        return 0;
    }
    if (func_004AFDD0(pc, def, &pc->args[0])) {
        return 0;
    }
    if (REG_LIVE(def->args[0])) {
        return 0;
    }
    SET_OP(def, op);
    COPY_OP(def->args[0], pc->args[0]);
    NOP_OUT(pc);
    return 1;
}

int func_00547D60(MPCode* pc) {
    MPCode* def;
    UInt16 op;

    def = DEF(pc->args[1]);
    if (def->op == 0x96) {
        op = 0x93;
    } else if (def->op == 0x563 || def->op == 0x560) {
        op = 0x562;
    } else {
        return 0;
    }
    if (func_004AFDD0(pc, def, &def->args[0])) {
        return 0;
    }
    if (func_004AFDD0(pc, def, &pc->args[0])) {
        return 0;
    }
    if (REG_LIVE(def->args[0])) {
        return 0;
    }
    SET_OP(def, op);
    COPY_OP(def->args[0], pc->args[0]);
    NOP_OUT(pc);
    return 1;
}

// pc's first operand made by a defop: take def's operand directly
static inline int StoreOfLoad(MPCode* pc, UInt16 defop, UInt16 op) {
    MPCode* def;

    def = DEF(pc->args[0]);
    if (def->op != defop) {
        return 0;
    }
    if (func_004AFE40(pc, def, &def->args[0])) {
        return 0;
    }
    SET_OP(pc, op);
    COPY_OP(pc->args[0], def->args[0]);
    return 1;
}

int func_00547E70(MPCode* pc) {
    return StoreOfLoad(pc, 0xB1, 0xC4);
}

int func_00547EE0(MPCode* pc) {
    return StoreOfLoad(pc, 0xB1, 0x587);
}

int func_00547F50(MPCode* pc) {
    return StoreOfLoad(pc, 0xAD, 0xCB);
}

int func_00547FC0(MPCode* pc) {
    return StoreOfLoad(pc, 0xAD, 0x58F);
}

static int func_00548090(MPCode* pc, UInt16 op1, UInt16 op2, UInt16 newop1, UInt16 newop2);

int func_00548030(MPCode* pc) {
    return func_00548090(pc, 0x93, 0x562, 0x96, 0x563);
}

int func_00548060(MPCode* pc) {
    return func_00548090(pc, 0x9D, 0x56D, 0xA1, 0x56E);
}

static int func_00548090(MPCode* pc, UInt16 op1, UInt16 op2, UInt16 newop1, UInt16 newop2) {
    MPCode* def;
    UInt16 newop;

    def = DEF(pc->args[0]);
    if (!def) {
        return 0;
    }
    if (def->op == op1) {
        newop = newop1;
    } else if (def->op == op2) {
        newop = newop2;
    } else {
        return 0;
    }
    if (func_004AFDD0(pc, def, &def->args[0])) {
        return 0;
    }
    if (REG_LIVE(def->args[0])) {
        return 0;
    }
    if (TouchedBetween(pc, def, &pc->args[1])) {
        return 0;
    }
    def->op = newop;
    def->args[0] = pc->args[1];
    MOP_SETOBJ(def->args[0], def);
    def->flags |= pc->flags & 0x100;
    deletepcode(pc);
    return 1;
}

int func_00549400(MPCode* pc) {
    switch (DEF(pc->args[1])->op) {
        case 0xD4:
        case 0xD5:
        case 0xD6:
        case 0xD7:
        case 0x59B:
        case 0x59C:
        case 0x59D:
        case 0x59E:
        case 0x59F:
        case 0x5A0:
        case 0x5A1:
        case 0x5A2:
        case 0x5A3:
        case 0x5A4:
        case 0x5A5:
        case 0x5A6:
        case 0x5A7:
        case 0x5A8:
        case 0x5A9:
        case 0x5AA:
        case 0x5AB:
        case 0x5AC:
            MAKE_MOVE(pc);
            return 1;
    }
    return 0;
}

int func_00549440(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[1]);
    if (pc->args[2].value != 32) {
        return 0;
    }
    switch (def->op) {
        case 0x88:
            if (def->args[2].value != 0) {
                return 0;
            }
            break;
        case 0xD2:
        case 0xD3:
        case 0x189:
        case 0x18A:
        case 0x18D:
        case 0x18E:
            break;
        default:
            return 0;
    }
    MAKE_MOVE(pc);
    return 1;
}

int func_005494B0(MPCode* pc) {
    if (pc->args[2].reg != 0) {
        return 0;
    }
    MAKE_MOVE(pc);
    return 1;
}

int func_005494E0(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[1]);
    if (def->op != 0xBE) {
        return 0;
    }
    if (pc->args[2].reg != 0) {
        return 0;
    }
    if (Redefined(pc, def, &def->args[1])) {
        return 0;
    }
    if (Redefined(pc, def, &def->args[2])) {
        return 0;
    }
    COPY_OP(pc->args[1], def->args[1]);
    COPY_OP(pc->args[2], def->args[2]);
    return 1;
}

int func_00549640(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[2]);
    if (def->op != data_00625E48) {
        return 0;
    }
    if (func_004AFE40(pc, def, &def->args[1])) {
        return 0;
    }
    COPY_OP(pc->args[2], def->args[1]);
    return 1;
}

// a 32-bit-or-less right shift (0xD8/0xDA) then a mask of the low bits:
// one bit-field extract (0x3EE)
int func_00548230(MPCode* pc) {
    MPCode* def;
    MPCode* p;
    SInt32 size;
    SInt32 shift;

    def = DEF(pc->args[1]);
    if (def->op != 0xD8 && def->op != 0xDA) {
        return 0;
    }
    if (Redefined(pc, def, &def->args[1])) {
        return 0;
    }
    size = pc->args[2].value;
    if (size <= 0 || size > 32) {
        return 0;
    }
    shift = def->args[2].value;
    if (shift < 0 || shift >= 32) {
        return 0;
    }
    if (shift + size <= 0 || shift + size > 32) {
        return 0;
    }
    p = makepcode(0x3EE, pc->args[0].reg, def->args[1].reg, shift, size);
    COPY_OP(p->args[1], def->args[1]);
    SET_LIVE(p->args[1]);
    insertpcodeafter(pc, p);
    deletepcode(pc);
    return 1;
}

// the same with an and-immediate (7/0x51A) of 2^n-1
int func_005483A0(MPCode* pc) {
    MPCode* def;
    MPCode* p;
    SInt32 size;
    SInt32 shift;

    def = DEF(pc->args[1]);
    if (def->op != 0xD8 && def->op != 0xDA) {
        return 0;
    }
    if (Redefined(pc, def, &def->args[1])) {
        return 0;
    }
    size = getbit(pc->args[2].value2 + 1);
    if (size <= 0 || size > 16) {
        return 0;
    }
    shift = def->args[2].value;
    if (shift < 0 || shift >= 32) {
        return 0;
    }
    if (shift + size <= 0 || shift + size > 32) {
        return 0;
    }
    p = makepcode(0x3EE, pc->args[0].reg, def->args[1].reg, shift, size);
    COPY_OP(p->args[1], def->args[1]);
    SET_LIVE(p->args[1]);
    insertpcodeafter(pc, p);
    deletepcode(pc);
    return 1;
}

// a store followed by stores to the same place: combine them
int func_00548510(MPCode* pc) {
    MPCode* p;
    MPCode* q;
    MOperand* op;
    int n;
    SInt32 kind;
    int changed;

    changed = 0;
    func_0048FBE0(pc->op);
    if (pc->flags & 0x180) {
        return 0;
    }
    for (p = pc->next; p; p = p->next) {
        if (p->flags & 0x180) {
            return 0;
        }
        if (p->flags & 2) {
            if (p->args[2].reg == pc->args[2].reg &&
                p->args[1].kind == 4 &&
                p->args[1].value2 == pc->args[1].value2 &&
                p->args[1].value == pc->args[1].value &&
                (kind = func_0048FFA0(pc, p))) {
                q = func_0048FDC0(kind, &pc->args[0], &p->args[0]);
                insertpcodebefore(p, q);
                deletepcode(p);
                p = q;
                changed = 1;
            }
        } else if (p->flags & 4) {
            if (p->args[1].value2 == pc->args[1].value2 || may_alias(p, pc)) {
                return 0;
            }
        }
        for (op = p->args, n = p->argcount; n--; op++) {
            if (op->kind == 0 && (op->flags & 2)) {
                if (op->kind == 0 &&
                    op->rclass == 0 &&
                    (op->reg == pc->args[1].reg || op->reg == pc->args[2].reg)) {
                    return 0;
                }
                if (op->rclass == pc->args[0].rclass && op->reg == pc->args[0].reg) {
                    return 0;
                }
            }
        }
    }
    return changed ? 1 : 0;
}

static int func_005487E0(MPCode* store, MPCode* load);

// a load of what the previous instruction stored: a move
int func_005486E0(MPCode* pc) {
    MPCode* p;
    int kind;

    p = pc->prev;
    if (!p) {
        return 0;
    }
    if (!p->block) {
        return 0;
    }
    if (!(p->flags & 4)) {
        return 0;
    }
    kind = func_005487E0(p, pc);
    if (!kind) {
        return 0;
    }
    if (pc->args[2].reg != p->args[2].reg) {
        return 0;
    }
    if (pc->args[1].value2 == p->args[1].value2 && pc->args[1].value == p->args[1].value) {
        if (kind == 1) {
            pc->op = data_00625E48;
        } else if (kind == 2) {
            pc->op = data_00625E74;
        } else if (kind == 3) {
            pc->op = 0x5B1;
        } else if (kind == 4) {
            pc->op = 0x5B2;
        }
        COPY_OP(pc->args[1], p->args[0]);
        pc->flags &= ~2;
        pc->flags |= 0x10;
        pc->argcount = 2;
        pc->unk38 = 0;
        return 1;
    }
    return 0;
}

// which move a store/load pair becomes (0: none)
static int func_005487E0(MPCode* store, MPCode* load) {
    if (store->op == 0x597 && load->op == 0x575) {
        return 2;
    }
    if (store->op == 0x1D2 && load->op == 0x1D1) {
        return 2;
    }
    if (store->op == 0x587 && load->op == 0x562) {
        return 1;
    }
    if (store->op == 0xC4 && load->op == 0x93) {
        return 1;
    }
    if (store->op == 0x58F && load->op == 0x56D) {
        return 1;
    }
    if (store->op == 0xCB && load->op == 0x9D) {
        return 1;
    }
    if (store->op == 0xC5 && load->op == 0x96) {
        return 4;
    }
    if (store->op == 0xCD && load->op == 0xA1) {
        return 3;
    }
    if (store->op == 0x588 && load->op == 0x563) {
        return 4;
    }
    if (store->op == 0x591 && load->op == 0x56E) {
        return 3;
    }
    return 0;
}

// sign or zero extensions of values already extended, or masked
extern CInt64 data_005CCD70; // 0xFF
extern CInt64 data_005CCD78; // 0xFFFF

int func_00548900(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[0]);
    if (!def->block) {
        return 0;
    }
    if (def->op != 0x599 && def->op != 0x59A) {
        return 0;
    }
    if (def->args[2].value != 16) {
        return 0;
    }
    if (def->args[0].reg != def->args[1].reg) {
        return 0;
    }
    if (REG_LIVE(pc->args[0])) {
        return 0;
    }
    pc->flags |= def->flags & 0x100;
    deletepcode(def);
    return 1;
}

int func_005489A0(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[0]);
    if (!def->block) {
        return 0;
    }
    if (def->op != 7 && def->op != 0x51A) {
        return 0;
    }
    if (!CInt64_Equal(*(CInt64*)&def->args[2].value, data_005CCD78)) {
        return 0;
    }
    if (def->args[0].reg != def->args[1].reg) {
        return 0;
    }
    if (REG_LIVE(pc->args[0])) {
        return 0;
    }
    pc->flags |= def->flags & 0x100;
    deletepcode(def);
    return 1;
}

int func_00548A60(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[0]);
    if (!def->block) {
        return 0;
    }
    if (def->op != 0x599 && def->op != 0x59A) {
        return 0;
    }
    if (def->args[2].value != 8) {
        return 0;
    }
    if (def->args[0].reg != def->args[1].reg) {
        return 0;
    }
    if (REG_LIVE(pc->args[0])) {
        return 0;
    }
    pc->flags |= def->flags & 0x100;
    deletepcode(def);
    return 1;
}

int func_00548B00(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[0]);
    if (!def->block) {
        return 0;
    }
    if (def->op != 7 && def->op != 0x51A) {
        return 0;
    }
    if (!CInt64_Equal(*(CInt64*)&def->args[2].value, data_005CCD70)) {
        return 0;
    }
    if (def->args[0].reg != def->args[1].reg) {
        return 0;
    }
    if (REG_LIVE(pc->args[0])) {
        return 0;
    }
    pc->flags |= def->flags & 0x100;
    deletepcode(def);
    return 1;
}

// a store to what an earlier store (with no use between) wrote: delete that one
int func_00548BC0(MPCode* pc) {
    MPCode* p;
    MPCode* q;

    if (pc->flags & 0x20) {
        return 0;
    }
    for (p = pc->prev; p; p = p->prev) {
        if ((p->flags & 0x40004) && !(p->flags & 0x20) && pc->alias == p->alias) {
            break;
        }
    }
    if (!p) {
        return 0;
    }
    if (p->args[1].value != pc->args[1].value) {
        return 0;
    }
    if (pc->args[2].kind == 0 && pc->args[2].reg != p->args[2].reg) {
        return 0;
    }
    if (pc->args[2].kind == 0 &&
        pc->args[2].reg == p->args[2].reg &&
        pc->args[2].value2 != p->args[2].value2) {
        return 0;
    }
    for (q = pc->prev; q != p; q = q->prev) {
        if (q->flags & 0x60002) {
            if (may_alias(pc, q)) {
                return 0;
            }
        } else if (q->flags & 8) {
            if (may_alias(pc, q)) {
                return 0;
            }
        }
    }
    if (p->flags & 0x80) {
        return 0;
    }
    if (func_0048FBE0(p->op) > func_0048FBE0(pc->op)) {
        return 0;
    }
    pc->flags |= p->flags & 0x100;
    deletepcode(p);
    return 1;
}

static int func_00548D90(MPCode* pc, UInt16 op);

int func_00548D10(MPCode* pc) {
    return func_00548D90(pc, 0x10C);
}

int func_00548D30(MPCode* pc) {
    return func_00548D90(pc, 0x10A);
}

int func_00548D50(MPCode* pc) {
    return func_00548D90(pc, 0x10D);
}

int func_00548D70(MPCode* pc) {
    return func_00548D90(pc, 0x10B);
}

// a memory access at offset 0 from a sum (4): the indexed form
static int func_00548D90(MPCode* pc, UInt16 op) {
    MPCode* def;

    def = DEF(pc->args[2]);
    if (def->op != 4) {
        return 0;
    }
    if (pc->unk38 != 0) {
        return 0;
    }
    if (pc->args[1].value != 0) {
        return 0;
    }
    if (Redefined(pc, def, &def->args[1])) {
        return 0;
    }
    if (Redefined(pc, def, &def->args[2])) {
        return 0;
    }
    SET_OP(pc, op);
    SET_LIVE(def->args[1]);
    SET_LIVE(def->args[2]);
    COPY_OP(pc->args[1], def->args[1]);
    COPY_OP(pc->args[2], def->args[2]);
    return 1;
}

// is *a before *b in its block? if *b comes first and swapping is allowed,
// swaps them
static Boolean func_005493A0(MPCode** a, MPCode** b, int canswap) {
    MPCodeBlock* block;
    MPCode* p;

    block = (*a)->block;
    if (!block) {
        return 0;
    }
    for (p = block->first; p; p = p->next) {
        if (p == *a) {
            return 1;
        }
        if (p == *b) {
            if (!canswap) {
                return 0;
            }
            *b = *a;
            *a = p;
            return 1;
        }
    }
    return 0;
}

static Boolean func_0054A9E0(MPCode* from, MPCode* to, MOperand* reg);

// does q define LO?
static inline Boolean DefinesLO(MPCode* q) {
    MOperand* op;
    int i;

    if (!q) {
        return 0;
    }
    for (i = 0, op = q->args; i < q->argcount; i++, op++) {
        if (op->kind == 0 && op->rclass == 2 && (op->flags & 2) && !op->reg) {
            return 1;
        }
    }
    return 0;
}

static inline Boolean LOClobbered(MPCode* from, MPCode* to) {
    MPCode* q;

    for (q = from->next; q != to; q = q->next) {
        if (DefinesLO(q)) {
            return 1;
        }
    }
    return 0;
}

// an add or subtract of a product (or of LO): multiply-accumulate
int func_00548F60(MPCode* pc) {
    MPCode* d2;
    UInt16 op;
    UInt16 extra;
    MPCode* d1;
    MPCode* q;

    if (!copts.int_multiply_add) {
        return 0;
    }
    d1 = DEF(pc->args[1]);
    d2 = DEF(pc->args[2]);
    if (!func_005493A0(&d1, &d2, pc->flags & 0x2000)) {
        return 0;
    }
    switch (d1->op) {
        case 0x577:
            extra = 0xB6;
            break;
        case 0x57F:
            extra = 0xB7;
            break;
        case 0xA9:
            extra = 0;
            if (d1->args[0].reg == pc->args[1].reg) {
                if (!REG_LIVE(pc->args[1])) {
                    return 0;
                }
            } else if (d1->args[0].reg == pc->args[2].reg) {
                if (!REG_LIVE(pc->args[2])) {
                    return 0;
                }
            } else {
                return 0;
            }
            if (!d1->next || !pc->prev || func_0054A9E0(d1->next, pc->prev, &d1->args[0])) {
                return 0;
            }
            break;
        default:
            return 0;
    }
    if (d2->op != 0x577 && d2->op != 0x57F) {
        return 0;
    }
    if (pc->op == 1 || pc->op == 4) {
        if (d2->op == 0x577) {
            op = 0x400;
        } else {
            op = 0x401;
        }
    } else if (pc->op == 0xDC || pc->op == 0xDD) {
        if (d2->op == 0x577) {
            op = 0x405;
        } else {
            op = 0x406;
        }
    } else {
        return 0;
    }
    if (LOClobbered(d1, d2)) {
        return 0;
    }
    if (pc->args[0].reg != d2->args[0].reg &&
        pc->next &&
        func_0054A9E0(pc->next, pc->block->last, &d2->args[0])) {
        return 0;
    }
    if (pc->prev != d2 && func_0054A9E0(d2->next, pc->prev, &d2->args[0])) {
        return 0;
    }
    if (extra != 0) {
        q = func_00480420(extra, 3);
        q->args[0] = d1->args[1];
        q->args[1] = d1->args[2];
        q->args[2].kind = 0;
        q->args[2].rclass = 2;
        q->args[2].reg = 0;
        q->args[2].flags = 2;
        insertpcodeafter(d1, q);
    }
    q = func_00480420(op, 3);
    q->args[0] = d2->args[1];
    q->args[1] = d2->args[2];
    q->args[2].kind = 0;
    q->args[2].rclass = 2;
    q->args[2].reg = 0;
    q->args[2].flags = 3;
    insertpcodeafter(d2, q);
    SET_OP2(pc, 0xA9);
    pc->argcount = 2;
    pc->args[1].kind = 0;
    pc->args[1].rclass = 2;
    pc->args[1].reg = 0;
    pc->args[1].flags = 1;
    deletepcode(d1);
    deletepcode(d2);
    return 1;
}

// is reg read between from and to (inclusive) before being redefined?
static Boolean func_0054A9E0(MPCode* from, MPCode* to, MOperand* reg) {
    MPCode* end;
    MOperand* op;
    int n;
    Boolean defined;
    Boolean used;

    defined = 0;
    used = 0;
    end = to->next;
    for (; from != end; from = from->next) {
        for (op = from->args, n = from->argcount; n--; op++) {
            if (SAME_REG(op, reg)) {
                if (op->flags & 2) {
                    defined = 1;
                } else {
                    used = 1;
                }
            }
        }
        if (defined && !used) {
            return 0;
        }
        if (used) {
            return 1;
        }
    }
    return 0;
}

// a memory access through a register just made by an add-immediate (3):
// fold the constant into the offset
int func_005496A0(MPCode* pc) {
    MPCode* def;
    SInt32 offset;

    def = DEF(pc->args[2]);
    if (def->op != 3) {
        return 0;
    }
    if (def->args[1].rclass == 0 && def->args[1].reg == 0) {
        return 0;
    }
    if (pc->unk38 != 0 || def->unk38 != 0) {
        return 0;
    }
    if (def->args[0].reg == def->args[1].reg) {
        return 0;
    }
    if (Redefined(pc, def, &def->args[1])) {
        return 0;
    }
    if (!CInt64_IsInRange(*(CInt64*)&def->args[2].value, 2)) {
        return 0;
    }
    offset = def->args[2].value2 + pc->args[1].value;
    if (offset > 0x7FFF || offset < -0x8000) {
        return 0;
    }
    COPY_OP(pc->args[2], def->args[1]);
    pc->args[1].value = offset;
    return 1;
}

// a halfword or byte store of a masked or extended value: store the value
int func_005497E0(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[0]);
    if (def->op != 7 && def->op != 0x51A) {
        return 0;
    }
    if (func_004AFE40(pc, def, &def->args[1])) {
        return 0;
    }
    if (!CInt64_Equal(*(CInt64*)&def->args[2].value, data_005CCD78)) {
        return 0;
    }
    COPY_OP(pc->args[0], def->args[1]);
    return 1;
}

int func_00549870(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[0]);
    if (def->op != 0x59A && def->op != 0x599) {
        return 0;
    }
    if (func_004AFE40(pc, def, &def->args[1])) {
        return 0;
    } else if (def->args[2].value == 16) {
        if (def->args[0].reg == def->args[1].reg) {
            MAKE_MOVE(def);
            return 1;
        }
        COPY_OP(pc->args[0], def->args[1]);
        return 1;
    } else {
        return 0;
    }
}

int func_00549910(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[0]);
    if (def->op != 7 && def->op != 0x51A) {
        return 0;
    }
    if (func_004AFE40(pc, def, &def->args[1])) {
        return 0;
    }
    if (!CInt64_Equal(*(CInt64*)&def->args[2].value, data_005CCD70)) {
        return 0;
    }
    COPY_OP(pc->args[0], def->args[1]);
    return 1;
}

int func_005499A0(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[0]);
    if (def->op != 0x59A && def->op != 0x599) {
        return 0;
    }
    if (func_004AFE40(pc, def, &def->args[1])) {
        return 0;
    } else if (def->args[2].value == 8) {
        if (def->args[0].reg == def->args[1].reg) {
            MAKE_MOVE(def);
            return 1;
        }
        COPY_OP(pc->args[0], def->args[1]);
        return 1;
    } else {
        return 0;
    }
}

// an extension (data_00625AD0, by args[2] bits) of an extension
int func_00549A40(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[1]);
    if (def->op != data_00625AD0) {
        return 0;
    }
    if (Redefined(pc, def, &def->args[1])) {
        return 0;
    }
    if (pc->args[2].value < def->args[2].value) {
        COPY_OP(pc->args[1], def->args[1]);
        return 1;
    }
    MAKE_MOVE(pc);
    return 1;
}

int func_00549B40(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[1]);
    if (def->op != data_00625B58) {
        return 0;
    }
    if (pc->args[2].value >= def->args[2].value) {
        MAKE_MOVE(pc);
        return 1;
    }
    if (pc->args[2].value < def->args[2].value) {
        if (Redefined(pc, def, &def->args[1])) {
            return 0;
        }
        COPY_OP(pc->args[1], def->args[1]);
        return 1;
    }
    return 0;
}

// the same logical immediate applied twice
int func_00549C40(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[1]);
    if (def->op != 0xE5 && def->op != 0x5AF) {
        return 0;
    }
    if (CInt64_NotEqual(*(CInt64*)&def->args[2].value, *(CInt64*)&pc->args[2].value)) {
        return 0;
    }
    if (Redefined(pc, def, &def->args[1])) {
        return 0;
    }
    COPY_OP(pc->args[1], def->args[1]);
    MAKE_MOVE(pc);
    return 1;
}

// a right shift (0x59A) of a value masked to fewer bits: a move
int func_00549D50(MPCode* pc) {
    MPCode* def;
    int bits;

    def = DEF(pc->args[1]);
    if (def->op != 7) {
        return 0;
    }
    bits = getbit(def->args[2].value2 + 1);
    if (bits <= 0) {
        return 0;
    }
    if (pc->args[2].value <= bits) {
        return 0;
    }
    MAKE_MOVE(pc);
    return 1;
}

// a sign (0x599) or zero (0x59A) extension from 8, 16 or 32 bits of a loaded
// value: load only those bits (at the right end of the word) and move
#define NARROW(newop, be) \
    do { \
        def->op = (newop); \
        MAKE_MOVE(pc); \
        def->args[1].value += copts.littleendian ? 0 : (be); \
        return 1; \
    } while (0)
#define RELOAD(newop) \
    do { \
        def->op = (newop); \
        MAKE_MOVE(pc); \
        return 1; \
    } while (0)

int func_00549DA0(MPCode* pc) {
    UInt16 b2;
    int used;
    UInt16 w2;
    UInt16 w;
    UInt16 h;
    UInt16 h2;
    SInt32 bits;
    MPCode* def;
    UInt16 b;
    int live;

    def = DEF(pc->args[1]);
    if (!(def->flags & 2)) {
        return 0;
    }
    live = REG_LIVE(def->args[0]);
    bits = pc->args[2].value;
    if (bits != 8 && bits != 16 && bits != 32) {
        return 0;
    }
    used = func_004AFDD0(pc, def, &def->args[0]);
    if (pc->op == 0x599) {
        b = 0x91;
        b2 = 0x568;
        h = 0x94;
        h2 = 0x569;
        w = 0x9D;
        w2 = 0x56D;
    } else if (pc->op == 0x59A) {
        b = 0x92;
        b2 = 0x561;
        h = 0x95;
        h2 = 0x56A;
        w = 0xA0;
        w2 = 0x574;
    } else {
        CError_Internal("MIPS_Peephole.c", 575);
    }
    if (bits == 8) {
        if (def->op == b || def->op == b2) {
            MAKE_MOVE(pc);
            return 1;
        }
        if (used || live) {
            return 0;
        }
        if (def->op == 0x91 || def->op == 0x92) {
            if (live) {
                return 0;
            }
            RELOAD(b);
        } else if (def->op == 0x568 || def->op == 0x561) {
            RELOAD(b2);
        } else if (def->op == 0x94 || def->op == 0x95) {
            NARROW(b, 1);
        } else if (def->op == 0x569 || def->op == 0x56A) {
            NARROW(b2, 1);
        } else if (def->op == 0x9D || def->op == 0xA0) {
            NARROW(b, 3);
        } else if (def->op == 0x56D || def->op == 0x574) {
            NARROW(b2, 3);
        } else if (def->op == 0x93) {
            NARROW(b, 7);
        } else if (def->op == 0x562) {
            NARROW(b2, 7);
        }
    } else if (bits == 16) {
        if (def->op == h || def->op == h2 || def->op == b || def->op == b2) {
            MAKE_MOVE(pc);
            return 1;
        }
        if (used || live) {
            return 0;
        }
        if (def->op == 0x94 || def->op == 0x95) {
            RELOAD(h);
        } else if (def->op == 0x569 || def->op == 0x56A) {
            RELOAD(h2);
        } else if (def->op == 0x9D || def->op == 0xA0) {
            NARROW(h, 2);
        } else if (def->op == 0x56D || def->op == 0x574) {
            NARROW(h2, 2);
        } else if (def->op == 0x93) {
            NARROW(h, 6);
        } else if (def->op == 0x562) {
            NARROW(h2, 6);
        }
    } else if (bits == 32) {
        if (def->op == w ||
            def->op == w2 ||
            def->op == h ||
            def->op == h2 ||
            def->op == b ||
            def->op == b2) {
            MAKE_MOVE(pc);
            return 1;
        }
        if (used || live) {
            return 0;
        }
        if (def->op == 0xA0 || def->op == 0x9D) {
            RELOAD(w);
        } else if (def->op == 0x574 || def->op == 0x56D) {
            RELOAD(w2);
        } else if (def->op == 0x93) {
            NARROW(w, 4);
        } else if (def->op == 0x562) {
            NARROW(w2, 4);
        }
    }
    return 0;
}

// constant folding: shifts, extensions and masks of a constant (0x55E li,
// or an add-immediate to $zero)
int func_0054A560(MPCode* pc) {
    MPCode* def;
    CInt64 value;
    CInt64 shift;

    def = DEF(pc->args[1]);
    if (def->op == 0x55E) {
        value = *(CInt64*)&def->args[1].value;
    } else if (def->op == 3 && def->args[1].reg == 0) {
        value = *(CInt64*)&def->args[2].value;
    } else {
        return 0;
    }
    pc->op = 0x55E;
    pc->args[1].kind = 3;
    CInt64_SetLong(&shift, pc->args[2].value);
    *(CInt64*)&pc->args[1].value = CInt64_Shl(value, shift);
    pc->argcount = 2;
    return 1;
}

int func_0054A5F0(MPCode* pc) {
    MPCode* def;
    CInt64 value;

    def = DEF(pc->args[1]);
    if (def->op == 0x55E) {
        value = *(CInt64*)&def->args[1].value;
    } else if ((def->op == 3 || def->op == 0x78) && def->args[1].reg == 0) {
        value = *(CInt64*)&def->args[2].value;
    } else {
        return 0;
    }
    if (!CInt64_IsInRange(value, pc->args[2].value / 8)) {
        return 0;
    }
    MAKE_MOVE(pc);
    return 1;
}

int func_0054A690(MPCode* pc) {
    MPCode* def;
    CInt64 value;
    CInt64 bits;

    def = DEF(pc->args[1]);
    if (def->op == 0x55E) {
        value = *(CInt64*)&def->args[1].value;
    } else if ((def->op == 3 || def->op == 0x78) && def->args[1].reg == 0) {
        value = *(CInt64*)&def->args[2].value;
    } else {
        return 0;
    }
    pc->op = 0x55E;
    pc->args[1].kind = 3;
    CInt64_SetLong(&bits, pc->args[2].value);
    *(CInt64*)&pc->args[1].value =
        CInt64_And(value, CInt64_Sub(CInt64_Shl(cint64_one, bits), cint64_one));
    pc->argcount = 2;
    return 1;
}

int func_0054A760(MPCode* pc) {
    MPCode* def;
    CInt64 value;

    def = DEF(pc->args[1]);
    if (def->op == 0x55E) {
        value = *(CInt64*)&def->args[1].value;
    } else if ((def->op == 3 || def->op == 0x78) && def->args[1].reg == 0) {
        value = *(CInt64*)&def->args[2].value;
    } else {
        return 0;
    }
    pc->op = 0x55E;
    pc->args[1] = pc->args[2];
    *(CInt64*)&pc->args[1].value = CInt64_And(*(CInt64*)&pc->args[2].value, value);
    pc->argcount = 2;
    return 1;
}

// an arithmetic right shift (0xD8) of a left shift (0xD2) by the same
// amount: a sign extension
int func_0054A800(MPCode* pc) {
    MPCode* def;

    def = DEF(pc->args[1]);
    if (def->op != 0xD2) {
        return 0;
    }
    if (Redefined(pc, def, &def->args[1])) {
        return 0;
    }
    if (pc->args[2].value != def->args[2].value) {
        return 0;
    }
    SET_OP2(pc, 0x599);
    pc->args[2].kind = 2;
    pc->args[2].value = 32 - def->args[2].value;
    COPY_OP(pc->args[1], def->args[1]);
    return 1;
}

// a left shift of a left shift
int func_0054A910(MPCode* pc) {
    MPCode* def;
    SInt32 shift;

    def = DEF(pc->args[1]);
    if (def->op != 0xD2) {
        return 0;
    }
    if (Redefined(pc, def, &def->args[1])) {
        return 0;
    }
    shift = pc->args[2].value + def->args[2].value;
    if (shift >= 31) {
        return 0;
    }
    COPY_OP(pc->args[1], def->args[1]);
    pc->args[2].value = shift;
    return 1;
}
