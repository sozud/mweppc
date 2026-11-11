#ifndef COMPILER_IROPTIMIZER_H
#define COMPILER_IROPTIMIZER_H

#include "compiler/common.h"

#ifdef __cplusplus
extern "C" {
#endif

extern Boolean DoLinearize;
extern Boolean VectorPhaseCalledFromUnroll;
extern Boolean IRO_Log;

extern Statement* IRO_Optimizer(Object* func, Statement* statements);
#if MWCCPSP
// 3.0.1: the back end hands IRO_Setup a table of 21 hooks (copied into
// separate globals, see IrOptimizer.c). Only some are identified.
typedef struct IROAsmOperand {
    UInt8 type; // 3: address taken (IRO_FindAllVars)
    struct Object* object;
    SInt32 offset;
    SInt32 size;
} IROAsmOperand;
typedef struct IROAsmEffects {
    UInt8 optimizationBarrier; // +0
    UInt8 flag1;
    UInt8 flag2;
    UInt8 unmodeledControlFlow; // +3
    UInt8 mayThrow; // +4: IRO_FindAllVars then handles the exception actions
    UInt8 noFallthrough; // +5 (IRO_ComputeSuccPred)
    UInt8 reachBarrier; // +6: IRO_FindAsmBarriers marks the node
    UInt8 flag7;
    SInt32 x8;
    SInt32 numoperands; // +0xC
    SInt32 numlabels;
    IROAsmOperand operands[16]; // +0x14
    struct CLabel* labels[16];
} IROAsmEffects; // 0x154 bytes
typedef void (*IROHook)(void);
typedef struct IROHooks {
    IROHook hook0;
    Boolean (*isAddressMultiply)(IROLinear* linear);
    IROHook hook2;
    IROHook hook3;
    void (*getAsmEffects)(Statement* stmt, IROAsmEffects* effects);
    IROHook hook5;
    IROHook hook6;
    IROHook hook7;
    IROHook hook8;
    IROHook hook9;
    IROHook hook10;
    IROHook hook11;
    IROHook hook12;
    void (*getLogFileName)(char* buf, int size);
    IROHook hook14;
    IROHook hook15;
    IROHook hook16;
    IROHook hook17;
    IROHook hook18;
    IROHook hook19;
    IROHook hook20;
} IROHooks;
extern void (*IRO_GetAsmEffectsHook)(Statement* stmt, IROAsmEffects* effects);
extern IROHook IRO_Hook0, IRO_Hook2, IRO_Hook3, IRO_Hook5, IRO_Hook6, IRO_Hook7, IRO_Hook8;
extern IROHook IRO_Hook9, IRO_Hook10, IRO_Hook11, IRO_Hook12, IRO_Hook14, IRO_Hook15;
extern IROHook IRO_Hook16, IRO_Hook17, IRO_Hook18, IRO_Hook19, IRO_Hook20;
extern IROAsmEffects IRO_EmptyAsmEffects;
extern void (*IRO_GetLogFileNameHook)(char* buf, int size);
extern void IRO_Setup(IROHooks* hooks);
#else
extern void IRO_Setup(void);
#endif
#if MWCCPSP
extern void IRO_Cleanup(void);
#endif
extern void CodeGen_UpdateOptimizerOptions(void);

#ifdef __cplusplus
}
#endif

#endif
