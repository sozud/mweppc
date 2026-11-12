#ifndef PSP_OBJGEN_H
#define PSP_OBJGEN_H

#include "compiler/common.h"

// How an object is addressed (ObjGen_GetObjectAddress).
typedef struct ObjAddress {
    SInt32 basereg; // 0 (absolute), 23 ($s7), 28 ($gp), the frame register, or -1
    SInt32 near; // 16-bit offset from basereg
    UInt8 gpreg;
} ObjAddress;

// A relocation in a function's code (CodeGen.c collects them).
typedef struct ObjCodeReloc {
    struct ObjCodeReloc* next;
    Object* target;
    SInt32 offset;
    SInt32 unk0C;
    SInt16 type; // R_MIPS_*
} ObjCodeReloc; // 0x14

// A relocation in an object's data (ObjGen_EmitData).
typedef struct ObjDataReloc {
    struct ObjDataReloc* next;
    Object* target;
    SInt32 offset;
    SInt32 addend;
} ObjDataReloc; // 0x10

extern void ObjGen_EmitData(Object* obj, void* data, ObjDataReloc* relocs, SInt32 size);
extern void ObjGen_GetObjectAddress(Object* obj, ObjAddress* addr);

#endif
