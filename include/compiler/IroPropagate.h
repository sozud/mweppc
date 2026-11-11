#ifndef COMPILER_IROPROPAGATE_H
#define COMPILER_IROPROPAGATE_H

#include "compiler/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !MWCCPSP_NATIVE
#pragma pack(push, 2)
#endif
#if MWCCPSP
// 3.0.1: 0x2C bytes, 32-bit counts (IRO_ExpressionPropagation 0x5683E0)
struct IROAssign {
    IROLinear* linear; // +0
    SInt32 index; // +4
    SInt32 varIndex; // +8
    IROLinear* linear2; // +0xC
    BitVector* depends; // +0x10
    Object* varObj; // +0x14
    VarRecord* var; // +0x18
    IROAssign* next; // +0x1C
    IROAssign* prev; // +0x20
    SInt32 useCount; // +0x24
    IRONode* node; // +0x28
};
#else
struct IROAssign {
    IROLinear* linear;
    UInt16 index;
    UInt16 varIndex;
    IROLinear* linear2;
    BitVector* depends;
    Object* varObj;
    VarRecord* var;
    IROAssign* next;
    IROAssign* prev;
    UInt16 useCount;
    IRONode* node;
};
#endif
#if !MWCCPSP_NATIVE
#pragma pack(pop)
#endif

extern IROAssign* IRO_FirstAssign;
extern IROAssign* IRO_LastAssign;

extern int IRO_IsRegable(Object* obj);
extern Boolean IRO_CopyAndConstantPropagation(void);
extern void IRO_ExpressionPropagation(void);

#ifdef __cplusplus
}
#endif

#endif
