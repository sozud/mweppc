#ifndef COMPILER_IROUSEDEF301_H
#define COMPILER_IROUSEDEF301_H

// mwccpsp 3.0.1: the definition and use records of IROUseDef301.c. The
// aggregate pass (IRO_RecomposeAssignments) builds bigger ones, a record per
// bit of a small struct variable (0x57A1E0, 0x57A0E0); 3.0.1's IRO_UseDef
// allocates only the first 0x24 bytes.
#include "compiler/common.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroVars.h"
#include "compiler/BitVector.h"

typedef struct IRODef301 {
    SInt32 index; // +0
    IRONode* node; // +4
    IROLinear* linear; // +8
    VarRecord* var; // +0xC
    struct IRODef301* globalnext; // +0x10
    struct IRODef301* varnext; // +0x14
    SInt32 useCount; // +0x18
    Boolean global; // +0x1C
    Boolean noregister; // +0x1D
    Boolean definite; // +0x1E
    Boolean removable; // +0x1F
    Boolean hasVectors; // +0x20: x28 is the dead-store pass's vector
    BitVector* x24; // the aggregate pass: 0x57F400's result
    BitVector* x28;
    // the aggregate pass's records (0x48 bytes)
    struct IRODef301*
        linearnext; // +0x2C: the linear's next definition (IROLinear +0x24 holds the first)
    SInt32 x30;
    SInt32 x34; // +0x34: how many uses it reaches (the aggregate pass)
    struct IROUse301* x38; // +0x38: the use, when one
    SInt32 bit; // +0x3C: the bit of the variable this record defines
    SInt32 offset; // +0x40: the first bit the assignment defines
    SInt32 count; // +0x44: how many
} IRODef301;

typedef struct IROUse301 {
    SInt32 index; // +0
    IRONode* node; // +4
    IROLinear* linear; // +8
    VarRecord* var; // +0xC
    struct IROUse301* globalnext; // +0x10
    struct IROUse301* varnext; // +0x14
    BitVector* reachingDefs; // +0x18
    SInt32 reachingDefCount; // +0x1C
    Boolean hasVectors; // +0x20: x24 and x28 belong to later passes
    BitVector* x24;
    BitVector* x28;
    // the aggregate pass's records (0x3C bytes)
    SInt32 x2C;
    SInt32 x30; // +0x30
    SInt32 x34; // +0x34
    Boolean x38; // +0x38
} IROUse301;

extern IRODef301* data_00624C38; // the definitions
extern IRODef301* data_006242BC; // the last one
extern SInt32 data_00624664; // how many
extern IROUse301* data_006242B8; // the uses
extern IROUse301* data_00625304; // the last one
extern SInt32 data_006242F4; // how many

extern void func_0057B300(void);

#endif
