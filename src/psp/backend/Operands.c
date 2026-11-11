#include "compiler/common.h"
#include <string.h>
#include "psp/BE.h"
#include "compiler/InlineAsmMIPS.h"
#include "compiler/CompilerTools.h"

// mwccpsp 3.0.1, around 0x47FD20..0x480710 (unit name not known): making
// instructions with their operands.

// the opcode table's flags (data_00611610 is data_006115F8[0].flags): OpcodeDef
// in InlineAsmMIPS.h

extern SInt32 data_00624D84; // source position for new instructions

#define IMPLICIT(r_class, r_reg, r_flags) \
    do { \
        implicit[n].kind = 0; \
        implicit[n].rclass = (r_class); \
        implicit[n].reg = (r_reg); \
        implicit[n].flags = (r_flags); \
        n++; \
    } while (0)

// An instruction with room for nargs operands, followed by the special
// registers the opcode reads (flags 1) or writes (flags 2) implicitly.
MPCode* func_00480420(int op, SInt16 nargs) {
    MOperand implicit[20];
    int n;
    UInt32 f2;
    MPCode* pc;
    int total;
    int size;
    int i;

    n = 0;
    f2 = data_006115F8[op].flags2;
    if (f2 & 0x20000) {
        IMPLICIT(2, 0, 2);
    }
    if (f2 & 0x80000) {
        IMPLICIT(2, 1, 2);
    }
    if (f2 & 0x10000) {
        IMPLICIT(2, 0, 1);
    }
    if (f2 & 0x40000) {
        IMPLICIT(2, 1, 1);
    }
    if (f2 & 0x800) {
        IMPLICIT(0, 31, 2);
    }
    if (f2 & 0x2000) {
        IMPLICIT(2, 4, 1);
    }
    if (f2 & 0x1000) {
        IMPLICIT(2, 4, 2);
    }
    if (f2 & 0x8000) {
        IMPLICIT(2, 3, 1);
    }
    if (f2 & 0x8000) {
        IMPLICIT(2, 5, 1);
    }
    if (f2 & 0x4000) {
        IMPLICIT(2, 3, 2);
    }
    if (f2 & 0x4000) {
        IMPLICIT(2, 5, 2);
    }

    total = nargs + n;
    size = PCODE_SIZE(total);
    pc = lalloc(size);
    memset(pc, 0, size);
    pc->argcount = total;
    pc->op = op;
    pc->unk20 = data_00624D84;
    for (i = total - 1; n; i--, n--) {
        pc->args[i] = implicit[n - 1];
    }
    pc->flags = data_006115F8[op].flags;
    return pc;
}
