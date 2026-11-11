#ifndef COMPILER_IROCSE_H
#define COMPILER_IROCSE_H

#include "compiler/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MWCCPSP
// 3.0.1: 0x34 bytes, natural alignment (IRO_AddExpression 0x575FE0)
struct IROExpr {
    Boolean flags; // +0
    SInt32 index; // +4
    IROLinear* linear; // +8
    Object* temp; // +0xC
    IRONode* node; // +0x10
    BitVector* depends; // +0x14
    UInt32 x18;
    IROExpr* use; // +0x1C
    Boolean x20;
    Boolean couldError; // +0x21
    Boolean notSubable; // +0x22
    IROLinear* factor; // +0x24
    VarRecord* var; // +0x28
    IROLinear* indOperand; // +0x2C
    IROExpr* next; // +0x30
};
#else
#pragma pack(push, 2)
struct IROExpr {
    Boolean flags;
    UInt16 index;
    IROLinear* linear;
    Object* temp;
    IRONode* node;
    BitVector* depends;
    IROExpr* use;
    Boolean couldError;
    Boolean notSubable;
    IROLinear* factor;
    VarRecord* var;
    IROLinear* indOperand;
    IROExpr* next;
};
#pragma pack(pop)
#endif

extern BitVector* IRO_Depends;
extern Boolean IRO_NotSubable;
extern Boolean IRO_IsVolatile;
extern Boolean IRO_CouldError;
extern IROExpr* IRO_FirstExpr;
extern IROExpr* IRO_LastExpr;
extern SInt32 IRO_NumExprs;

extern void IRO_FindDepends_NoAlloc(IROLinear* linear);
extern void IRO_FindDepends(IROLinear* linear);
extern void IRO_FindExpressions(BitVector* bv, Boolean unused);
extern void IRO_RemoveExpr(IROExpr* expr);
extern void IRO_ComputeAvail(void);
extern void IRO_CommonSubs(void);

#ifdef __cplusplus
}
#endif

#endif
