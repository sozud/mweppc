#ifndef COMPILER_IROLOOP301_H
#define COMPILER_IROLOOP301_H

// mwccpsp 3.0.1's loop records (IroLoop.c, IroLoopInvariants.c)
#include "compiler/common.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"
#include "compiler/IroCSE.h"

// 3.0.1 keeps a record per natural loop (0x70 bytes): the loop headers are
// found first (a predecessor that the node dominates), then each loop's body
// is collected and every node in it gets one more level; the counter-loop
// pass may look at the records before they are freed.
typedef struct IRONaturalLoop {
    UInt32 flags; // +0: 0x80 up by one, 0x200 up, 0x1000 down by one, 0x2000 down
    UInt32 x4;
    BitVector* nodes; // +8
    BitVector* xC;
    IRONode* x10;
    IRONode* header; // +0x14
    IRONode* x18; // the predecessor outside the loop placed just before the header
    IRONode* preheader; // +0x1C: the only predecessor outside the loop
    IRONode* x20; // +0x20: the loop's one node among x10's successors
    IROLinear* cond; // +0x24: the loop's test
    IROElmList* x28; // +0x28: the linears of the loop test (0x587540)
    void* induction; // +0x2C: the IROLoopInd301 that counts the loop
    IROElmList* x30; // +0x30: the linears of the induction updates (0x587540)
    Boolean hasCount; // +0x34: the trip count is known (0x584670)
    CInt64 count; // +0x38: the trip count when constant, else -1
    IROList countExpr; // +0x40: the trip count expression when not
    IROList countConds; // +0x48: the conditions the count needs
    UInt32 x50; // +0x50: the loop's linears that do something
    BitVector* x54; // the nodes with a successor outside the loop (exits)
    BitVector* x58; // the nodes with the header as successor (tails)
    UInt16 numExits; // +0x5C
    UInt16 numTails; // +0x5E
    IRONode* exitSuccessor; // +0x60: an exit's successor outside the loop
    IRONode* tail; // +0x64
    void* inductions; // +0x68: IROLoopInd301 list
    struct IRONaturalLoop* next; // +0x6C
} IRONaturalLoop;

// 3.0.1's loop flags (IRONaturalLoop.flags), named by the loop flag dump
// (0x583D30)
enum {
    LP301_INDUCTION_AT_LEFT = 0x1,
    LP301_HAS_CALLS = 0x2,
    LP301_HAS_CNTRLFLOW = 0x4,
    LP301_HAS_NONASSIGN = 0x8,
    LP301_INDUCTION_NOT_FOUND = 0x10,
    LP301_IF_EXPR_NONCANONICAL = 0x20,
    LP301_HAS_MULTIPLE_INDUCTIONS = 0x40,
    LP301_STEP_IS_ONE = 0x80,
    LP301_HEADER_FOLLOWS_UPDATE = 0x100,
    LP301_STEP_IS_POS = 0x200,
    LP301_IND_USED_IN_LOOP = 0x400,
    LP301_HAS_MULTIPLE_EXITS = 0x800,
    LP301_STEP_IS_NEG_ONE = 0x1000,
    LP301_STEP_IS_NEG = 0x2000,
    LP301_HAS_ASM = 0x4000,
    LP301_WHILE_LOOP = 0x8000,
    LP301_RECURSIVE_LOOP = 0x10000,
    LP301_IS_REDUCTION_CAND = 0x20000,
    LP301_HAS_NONCANONICAL_STEP = 0x40000,
    LP301_INNER_LOOP = 0x80000,
    LP301_TO_BE_VISITED = 0x100000,
    LP301_FORM_UNRECOGNIZABLE = 0x200000
};

// 3.0.1's record of an induction variable of a natural loop (0x44 bytes)
typedef struct IROLoopInd301 {
    UInt8 flags; // 4: it goes down
    VarRecord* var; // +4
    IRONode* fnode; // +8
    IROLinear* nd; // +0xC: the update
    IROLinear* init; // +0x10: the assignment before the loop
    IRONode* initNode; // +0x14
    UInt8 x18; // +0x18: the final value is known (0x584020)
    CInt64 finalValue; // +0x1C: the value after the loop, when constant
    IROList finalList; // +0x24: else this expression, its tail (0x584020)
    IROList finalConds; // +0x2C: the conditions the final value needs
    CInt64 step; // +0x34, when constant
    IROLinear* stepExpr; // +0x3C, else
    struct IROLoopInd301* next; // +0x40
} IROLoopInd301;

extern IROLoopInd301* func_00588C90(IRONaturalLoop* loop, VarRecord* var);
extern CInt64 func_00583FD0(IROLoopInd301* ind, Type* type);
extern void func_00587540(IRONaturalLoop* loop);
extern Boolean func_0058A4F0(IRONaturalLoop* loop);
extern void func_0058A430(
    IRONaturalLoop* loop, IRONode* header, IRONode* pred, IROLinear** at, Boolean* made
);
extern void func_00589F20(IRONaturalLoop* loop, IRONode** exitNode, IROLinear** at);
extern Boolean func_0058A030(
    IRONaturalLoop* loop, IROLinear** pre, IRONode* exitNode, IRONode** newPre, void* unused
);
extern IRONode* func_0058A5D0(IRONaturalLoop* loop);
extern void func_00583D30(UInt32 flags);
extern void func_00587270(IRONaturalLoop* loop);
extern Boolean func_00588690(IRONaturalLoop* loop, IROLinear* nd);
extern Boolean func_00588740(IRONaturalLoop* loop, IROLinear* nd);
extern void func_005886C0(IRONaturalLoop* loop);
extern void func_00588770(IRONaturalLoop* loop);
extern void func_00588700(IRONaturalLoop* loop);
extern void func_005887B0(IRONaturalLoop* loop);
extern IROLinear* func_00588900(IRONaturalLoop* loop);
extern void func_00588AE0(IRONaturalLoop* loop, IROLoopInd301* ind);
extern Boolean func_00584020(IRONaturalLoop* loop, IROLoopInd301* ind, Boolean withConds);

#endif
