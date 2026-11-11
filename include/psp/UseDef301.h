#ifndef PSP_USEDEF301_H
#define PSP_USEDEF301_H
// mwccpsp 3.0.1: the back end's use-def chains (UseDefChains301.c)
#include "compiler/common.h"
#include "compiler/BitVectors.h"
#include "psp/BE.h"

typedef struct UseOrDef301 {
    MPCode* pcode; // +0
    UInt8 kind; // +4: 0 register, 4 memory
    char rclass; // +5: register class; memory: 0 direct, 1 indirect
    SInt16 reg; // +6
    Object* object; // +8: memory
} UseOrDef301; // 0xC

typedef struct RegUseOrDef301 {
    struct RegUseOrDef301* next;
    int id;
} RegUseOrDef301;

typedef struct ObjectUseDef301 {
    struct ObjectUseDef301* next; // +0
    struct ObjectUseDef301* leftchild; // +4
    struct ObjectUseDef301* rightchild; // +8
    Object* object; // +0xC
    RegUseOrDef301* uses; // +0x10
    RegUseOrDef301* defs; // +0x14
    short directdefs;
    short directuses;
    short indirectdefs;
    short indirectuses;
} ObjectUseDef301; // 0x20

typedef struct UseDefInfo301 {
    int usestart; // +0
    int usecount; // +4
    int defstart; // +8
    int defcount; // +0xC
    BitVectorRange* defgen; // +0x10
    CompressedBitVector* defkill; // +0x14
    UInt32* defin; // +0x18
    UInt32* defout; // +0x1C
    BitVectorRange* usegen; // +0x20
    CompressedBitVector* usekill; // +0x24
    UInt32* usein; // +0x28
    UInt32* useout; // +0x2C
} UseDefInfo301; // 0x30

extern int data_00625374; // number_of_Defs
extern int data_00625AB8; // number_of_Uses
extern UseOrDef301* data_00624B58; // Defs
extern UseOrDef301* data_00624768; // Uses
extern RegUseOrDef301** data_00624BFC[7]; // reg_Defs
extern RegUseOrDef301** data_006247F4[7]; // reg_Uses
extern UseDefInfo301* data_00625988; // usedefinfo, by block number

#endif
