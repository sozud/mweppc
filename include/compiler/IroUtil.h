#ifndef COMPILER_IROUTIL_H
#define COMPILER_IROUTIL_H

#if MWCCPSP
#include <stddef.h>
#endif
#include "compiler/common.h"
#include "compiler/CInt64.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 2)
struct IROList {
    IROLinear* head;
    IROLinear* tail;
};
struct IROListNode {
    IROList list;
    IROListNode* nextList;
};
struct IROElmList {
    IROLinear* element;
    IROElmList* next;
};
struct IROAddrRecord {
    IROLinear* linear;
    UInt16 numObjRefs;
    IROElmList* objRefs;
    UInt16 numMisc;
    IROElmList* misc;
    UInt16 numInts;
    IROElmList* ints;
    int unused;
};
#pragma pack(pop)

extern Object* FunctionName;
extern Boolean IRO_IsLeafFunction;
extern Boolean IRO_FunctionHasReturn;
extern Boolean DisableDueToAsm;
extern Boolean LoopOptimizerRun;

extern Object* IRO_IsVariable(IROLinear* linear);
extern Boolean IRO_IsConstant(IROLinear* linear);
extern Boolean IRO_IsPow2(IROLinear* linear, int* powvalue);
extern Boolean IRO_IsIntConstant(IROLinear* linear);
extern Boolean IRO_IsFloatConstant(IROLinear* linear);
extern Boolean IRO_IsAssignment(IROLinear* linear);
extern Boolean IRO_TypesEqual(Type* a, Type* b);
extern Type* IRO_UnsignedType(Type* type);
extern Boolean IRO_is_CPtypeequal(Type* a, Type* b);
extern Boolean IRO_ExprsSame(IROLinear* a, IROLinear* b);
extern CLabel* IRO_NewLabel(void);
extern Boolean IRO_ExprsSameSemantically(IROLinear* a, IROLinear* b);
extern IROLinear* IRO_FindFirst(IROLinear* linear);
extern void IRO_CutAndPasteAfter(IROLinear* first, IROLinear* last, IROLinear* after);
extern Boolean IRO_IsConstantZero(IROLinear* linear);
extern Boolean IRO_IsConstantOne(IROLinear* linear);
extern Boolean IRO_IsConstantNegativeOne(IROLinear* linear);
extern void IRO_NopOut(IROLinear* linear);
extern void IRO_NopNonSideEffects(IROLinear* linear, SInt32 level);
typedef void (*IROWalkTreeFunc)(IROLinear* linear, Boolean isEntry);
extern void IRO_WalkTree(IROLinear* linear, IROWalkTreeFunc func);
extern void IRO_WalkInts(IROLinear* first, IROLinear* last, IROWalkTreeFunc func);
extern void IRO_Paste(IROLinear* first, IROLinear* last, IROLinear* before);
extern void IRO_PasteAfter(IROLinear* first, IROLinear* last, IROLinear* after);
extern void IRO_ClipExpr(IROExpr* expr);
extern void IRO_MoveExpression(IROExpr* expr, IROLinear* linear);
extern void IRO_InitList(IROList* list);
#if MWCCPSP
extern void IRO_ResetList(IROList* list); // 3.0.1, 0x552B80
#endif
extern void IRO_AddToList(IROLinear* linear, IROList* list);
extern IROLinear* IRO_FindLabelNode(CLabel* label, IROLinear* linear);
extern void IRO_DuplicateExprRange(IROLinear* start, IROLinear* end, IROList* list);
extern IROLinear* IRO_DuplicateExpr(IROLinear* linear, IROList* list);
extern IROLinear* IRO_TempReference(Object* obj, IROList* list);
extern IROLinear* IRO_LocateFather(IROLinear* linear);
extern IROLinear* IRO_LocateFather_Cut_And_Paste(IROLinear* nd, IROLinear* replacement);
extern IROLinear* IRO_LocateFather_Cut_And_Paste_Without_Nopping(
    IROLinear* nd, IROLinear* replacement
);
extern void IRO_ReplaceReference(IROLinear* nd, Object* obj, IROLinear* after);
extern void IRO_ReplaceReferenceWithNode(IROLinear* nd, IROLinear* replacement);
extern VarRecord* IRO_GetTemp(IROExpr* expr);
extern IROLinear* IRO_AssignToTemp(IROExpr* expr);
extern IROLinear* IRO_FindStart(IROLinear* linear);
extern IROAddrRecord* IRO_InitAddrRecordPointer(IROLinear* linear);
#if MWCCPSP
// 3.0.1's address record: the caller supplies it and the allocator for its lists
typedef struct IROAddrRecord301 {
    IROLinear* linear;
    int numObjRefs; // +4
    IROElmList* objRefs; // +8
    int numMisc; // +0xC
    IROElmList* misc; // +0x10
    int numInts; // +0x14
    IROElmList* ints; // +0x18
    int unused; // +0x1C
} IROAddrRecord301;
typedef void* (*IROAllocFunc)(size_t size);
extern void func_005513A0(IROAddrRecord301* rec, IROLinear* linear);
extern void func_005510D0(IROLinear* linear, IROAddrRecord301* rec, IROAllocFunc alloc);
extern void func_005512E0(IROLinear* linear, IROElmList** list, IROAllocFunc alloc);
typedef void (*IROWalkTreeArgFunc)(IROLinear* linear, Boolean isEntry, void* arg);
extern void func_00552EE0(IROLinear* nd, IROWalkTreeArgFunc func, void* arg);
extern void func_00552F00(IROLinear* nd, IROWalkTreeArgFunc func, void* arg);
extern IROLinear* func_00554490(void);
extern void func_00552B00(IROLinear* first, IROLinear* last, IROList* list);
#endif
extern IROLinear* IRO_HasSideEffect(IROLinear* linear);
extern IROLinear* IRO_CheckSideEffect(IROLinear* linear);
typedef void (*WalkObjFunc)(Object* obj);
extern void IRO_WalkExcActions(ExceptionAction* action, WalkObjFunc func);
extern Boolean IRO_FunctionCallMightThrowException(IROLinear* linear);
extern IROLinear* IRO_NewIntConst(CInt64 val, Type* type);
extern IROLinear* IRO_NewFloatConst(const Float val, Type* type);
extern Boolean IRO_IsAddressMultiply(IROLinear* linear);
#if MWCCPSP
typedef struct IROAddressedLabel {
    struct IROAddressedLabel* next;
    CLabel* label;
    UInt32 unused[4];
} IROAddressedLabel;
extern IROAddressedLabel* IRO_AddressedLabels;
extern SInt32 IRO_NumAddressedLabels;
extern IROAddressedLabel* IRO_GetGotoTargets(IROLinear* linear, UInt16* count);
extern void IRO_FreeGotoTargets(IROAddressedLabel* list);
extern Boolean IRO_IsThrowingCall(IROLinear* linear);
extern Boolean IRO_FunctionCallMightThrowException(IROLinear* linear);
extern Type* IRO_UnqualifiedType(Type* type); // IroType.c 0x55CBA0
extern Statement* IRO_NewStatement(int type, ENode* expr);
extern void IRO_CopySourceInfo(Statement* dst, Statement* src);
extern ENode* IRO_NewIntConstENode(CInt64 val, Type* type);
extern Boolean IRO_HasDivide; // cleared by IRO_Linearize (unidentified)
extern Boolean IRO_HasCosCall;
extern Boolean IRO_HasSinCall;
extern void IRO_FindAddressObjRefs(
    IROLinear* linear, IROLinear** objref, int* numObjRefs, int* numOther
);
// set by IRO_Setup from the back end's hook table (second entry)
extern Boolean (*IRO_IsAddressMultiplyHook)(IROLinear* linear);
#endif
extern void IRO_SetupForUserBreakChecking(void);
extern void IRO_CheckForUserBreak(void);

HEADER_INLINE CInt64 IRO_MakeULong(UInt32 i) {
    CInt64 val;
    CInt64_SetULong(&val, i);
    return val;
}

HEADER_INLINE CInt64 IRO_MakeLong(SInt32 i) {
    CInt64 val;
    val.lo = i;
    val.hi = (i < 0) ? -1 : 0;
    return val;
}

#ifdef __cplusplus
}
#endif

#endif
