#ifndef COMPILER_IROLINEARFORM_H
#define COMPILER_IROLINEARFORM_H

#include "compiler/common.h"
#include "compiler/enode.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !MWCCPSP
#pragma pack(push, 2)
#endif
typedef struct IROLinearIRSave {
    IROLinear* firstLinear;
    IROLinear* lastLinear;
    UInt32 numLinear;
    Statement* curStat;
    Boolean disableDueToAsm;
    Boolean isLeafFunction;
    Boolean functionHasReturn;
    void* nullCheckList;
    Statement* currStmt;
    Statement* prevStmt;
} IROLinearIRSave;

#if MWCCPSP
// 3.0.1 numbering, from IRO_Linearize (statement kinds), MarkAllSubExprs and
// TravExprToUpdateFlags (0x563dd0, 0x5620f0): the expression kinds come
// first and a goto through an expression (ST_GOTOEXPR) has its own kind.
typedef enum IROLinearType {
    IROLinearNop, // 0
    IROLinearOperand, // 1
    IROLinearOp1Arg, // 2
    IROLinearOp2Arg, // 3
    IROLinearOp3Arg, // 4
    IROLinearScope, // 5: an ESCOPEBEGIN/ESCOPEEND operand, kept only when no optimization runs
    IROLinearFunccall, // 6
    IROLinearGoto, // 7
    IROLinearGotoExpr, // 8: u.monadic is the target expression
    IROLinearIf, // 9
    IROLinearIfNot, // 10
    IROLinearReturn, // 11
    IROLinearLabel, // 12
    IROLinearSwitch, // 13
    IROLinearEntry, // 14
    IROLinearExit, // 15
    IROLinearBeginCatch, // 16
    IROLinearEndCatch, // 17
    IROLinearEndCatchDtor, // 18
    IROLinearAsm, // 19
    IROLinear18, // 20 (fatal in TravExprToUpdateFlags' caller)
    IROLinear19, // 21
    IROLinearEnd // 22
} IROLinearType;
#else
typedef enum IROLinearType {
    IROLinearNop,
    IROLinearOperand,
    IROLinearOp1Arg,
    IROLinearOp2Arg,
    IROLinearGoto,
    IROLinearIf,
    IROLinearIfNot,
    IROLinearReturn,
    IROLinearLabel,
    IROLinearSwitch,
    IROLinearOp3Arg,
    IROLinearFunccall,
    IROLinearEntry,
    IROLinearExit,
    IROLinearBeginCatch,
    IROLinearEndCatch,
    IROLinearEndCatchDtor,
    IROLinearAsm,
    IROLinear18,
    IROLinear19,
    IROLinearEnd
} IROLinearType;
#endif

enum {
    IROLF_StartsNode = 0x1,
    IROLF_Reffed = 0x2,
    IROLF_Assigned = 0x4,
    IROLF_ReplaceCSE = 0x8,
    IROLF_Used = 0x10,
    IROLF_Ind = 0x20,
    IROLF_Subs = 0x40,
    IROLF_LoopInvariant = 0x100,
    IROLF_BeginLoop = 0x200,
    IROLF_EndLoop = 0x400,
    IROLF_Ris = 0x800,
    IROLF_Immind = 0x1000,
    IROLF_VecOp = 0x2000,
    IROLF_InCallArg = 0x4000,
    IROLF_RightFirst = 0x8000,
    IROLF_VecOpBase = 0x10000,
    IROLF_CounterLoop = 0x40000,
    IROLF_BitfieldIndirect = 0x80000,
#if MWCCPSP
    IROLF_CouldError = 0x100000,
    IROLF_Unk200000 = 0x200000 // 3.0.1: cleared with the per-pass flags (TravExprToUpdateFlags)
#else
    IROLF_CouldError = 0x100000
#endif
};

#if MWCCPSP
// 3.0.1: 0x44 bytes, natural alignment (IRO_NewLinear is inlined into
// IRO_Linearize: oalloc(0x44), cleared with rep stosd).
struct IROLinear {
    IROLinearType type; // +0
    ENodeType nodetype; // +1
    SInt32 flags; // +4
    UInt16 nodeflags; // +8
    UInt32 index; // +0xC
    Statement* stmt; // +0x10
    Type* rtype; // +0x14
    void* unk18; // +0x18 unidentified (IRO_NewLinear clears it)
    IROExpr* expr; // +0x1C (ActNop, IRO_DuplicateExpr clear it)
    struct PointsToFunction* pointsToFunction; // +0x20
    union { // +0x24
        Boolean evaluated; // pointer analysis
        struct IRODef301* aggDefs; // the aggregate pass's definitions by this linear (0x57A1E0)
    } x24;
    struct ERange* range; // +0x28 (IRO_RangePropagateInFNodes)
    union { // +0x2C
        ENode* node;
        IROLinear* monadic;
        struct {
            IROLinear* left;
            IROLinear* right;
        } diadic;
        struct {
            IROLinear* a;
            IROLinear* b;
            IROLinear* c;
        } args3;
        struct {
            char ispascal; // +0x2C
            short argCount; // +0x2E
            IROLinear** args; // +0x30
            IROLinear* callee; // +0x34
            TypeFunc* functype;
            struct LocationSetSet* returnedLocs;
        } funccall;
        Statement* asm_stmt;
        struct {
            CLabel* label;
            IROLinear* cond;
            struct IROCounterLoopInfo* loop; // +0x34: the test of a counter loop (3.0.1)
        } label;
        struct {
            SwitchInfo* info;
            IROLinear* cond;
        } swtch;
        struct {
            IROLinear* linear;
            int unused1;
            int unused2;
        } ctch;
        struct {
            IROLinear* operand;
            Boolean counterUpdate; // +0x30: a counter loop's update, dropped by IRO_Delinearize
        } op1x;
        struct {
            IROLinear* left;
            IROLinear* right;
            Boolean counterUpdate; // +0x34
        } op2x;
        struct {
            Statement* stmt; // NULL: emit expr (an ENode) as an expression statement
            ENode* expr;
        } asmx;
    } u;
    IROLinear* next; // +0x40
};

// 3.0.1: what the loop passes record for a counter loop, and what
// IRO_Delinearize hands the back end in an ST_BEGINLOOP statement.
typedef struct IROCounterLoopInfo {
    UInt8 kind; // +0: 1 = the counter range is constant
    IROLinear* x4;
    IROLinear* x8;
    IROLinear* xC; // a constant or an expression
    CInt64 x10;
    IROLinear* x18;
    IROLinear* x1C;
} IROCounterLoopInfo;

typedef struct StmtLoopInfo {
    UInt32 flags; // 1: x20 is valid
    ENode* x4;
    ENode* cond;
    Statement* test; // the loop's if/ifnot statement
    ENode* x10;
    CInt64 x14; // xC when it is constant
    ENode* x1C; // xC otherwise
    CInt64 x20;
    ENode* x28;
    ENode* x2C;
} StmtLoopInfo; // 0x30 bytes
#else
struct IROLinear {
    IROLinearType type;
    ENodeType nodetype;
    SInt32 flags;
    UInt16 nodeflags;
    UInt16 index;
    Statement* stmt;
    Type* rtype;
    IROExpr* expr;
    struct ERange* range;
    struct PointsToFunction* pointsToFunction;
    Boolean evaluated;
    union {
        ENode* node;
        IROLinear* monadic;
        struct {
            IROLinear* left;
            IROLinear* right;
        } diadic;
        struct {
            IROLinear* a;
            IROLinear* b;
            IROLinear* c;
        } args3;
        struct {
            char ispascal;
            short argCount;
            IROLinear** args;
            IROLinear* callee;
            TypeFunc* functype;
            struct LocationSetSet* returnedLocs;
        } funccall;
        Statement* asm_stmt;
        struct {
            CLabel* label;
            IROLinear* cond;
        } label;
        struct {
            SwitchInfo* info;
            IROLinear* cond;
        } swtch;
        struct {
            IROLinear* linear;
            int unused1;
            int unused2;
        } ctch;
    } u;
    IROLinear* next;
};
#endif
#if !MWCCPSP
#pragma pack(pop)
#endif

extern IROLinear* IRO_FirstLinear;
extern IROLinear* IRO_LastLinear;
extern UInt32 IRO_NumLinear;
#if MWCCPSP
// 3.0.1: the linearizer's globals, saved around a nested run (0x561390, 0x561320)
typedef struct IROLinearState {
    IROLinear* firstLinear;
    IROLinear* lastLinear;
    UInt32 numLinear;
    Statement* curStat;
    Boolean disableDueToAsm;
    Boolean x11; // data_00625ECA
    void* nullCheckList;
    Statement* currStmt;
    Statement* prevStmt;
    Statement* firstStmt;
    void* catchObjects;
} IROLinearState;
extern void func_00561390(IROLinearState* state);
extern void func_00561320(IROLinearState* state);
#endif
extern Statement* CurStat;

extern IROLinear* IRO_NewLinear(IROLinearType type);
extern void IRO_PreLinearize(Statement* stmt);
extern void IRO_Linearize(Statement* stmt);
extern ENode* IRO_NewENode(ENodeType nodetype);
extern Statement* IRO_Delinearize(IRONode* node, IROLinear* linear);
extern void IRO_RenumberInts(void);
#if MWCCPSP
extern void IRO_UpdateFlagsOnInts(IROLinear* first); // 3.0.1 walks the list it is given
#else
extern void IRO_UpdateFlagsOnInts(void);
#endif
extern void IRO_SaveLinearIR(IROLinearIRSave* save);
extern void IRO_RestoreLinearIR(IROLinearIRSave* save);

#define IS_LINEAR_ENODE(_linear, _nodetype) \
    (((_linear)->type == IROLinearOperand) && ((_linear)->u.node->type) == (_nodetype))
#define IS_LINEAR_MONADIC(_linear, _nodetype) \
    (((_linear)->type == IROLinearOp1Arg) && ((_linear)->nodetype) == (_nodetype))
#define IS_LINEAR_MONADIC_2(_linear, _nodetype1, _nodetype2) \
    (((_linear)->type == IROLinearOp1Arg) && \
     (((_linear)->nodetype) == (_nodetype1) || ((_linear)->nodetype) == (_nodetype2)))
#define IS_LINEAR_DIADIC(_linear, _nodetype) \
    (((_linear)->type == IROLinearOp2Arg) && ((_linear)->nodetype) == (_nodetype))
#define IS_LINEAR_DIADIC_2(_linear, _nodetype1, _nodetype2) \
    (((_linear)->type == IROLinearOp2Arg) && \
     (((_linear)->nodetype) == (_nodetype1) || ((_linear)->nodetype) == (_nodetype2)))
#define IS_LINEAR_DIADIC_3(_linear, _nodetype1, _nodetype2, _nodetype3) \
    (((_linear)->type == IROLinearOp2Arg) && \
     (((_linear)->nodetype) == (_nodetype1) || \
      ((_linear)->nodetype) == (_nodetype2) || \
      ((_linear)->nodetype) == (_nodetype3)))

#ifdef __cplusplus
}
#endif

#endif
