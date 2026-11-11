#include "compiler/IrOptimizer.h"
#include "compiler/IroCSE.h"
#include "compiler/IroDump.h"
#include "compiler/IroEmptyLoop.h"
#include "compiler/IroEval.h"
#include "compiler/IroExprRegeneration.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroJump.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroLoop.h"
#include "compiler/IroPointerAnalysis.h"
#include "compiler/IroPropagate.h"
#include "compiler/IroRangePropagation.h"
#include "compiler/IroSubable.h"
#include "compiler/IroTransform.h"
#include "compiler/IroUnrollLoop.h"
#include "compiler/IROUseDef.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"
#if MWCCPSP
#include "compiler/IROUseDef.h"
#include "compiler/IroEval.h"
#endif
#include "compiler/CError.h"
#include "compiler/CompilerTools.h"
#include "compiler/CParser.h"
#include "compiler/enode.h"
#include "compiler/InlineAsm.h"
#include "compiler/InlineAsmPPC.h"
#include "compiler/objects.h"

Boolean DoScalarize;
Boolean DoLinearize;
Boolean EarlyReturn;
Boolean IRO_CPFirstTime;
Boolean VectorPhaseCalledFromUnroll;
Boolean IRO_Log;
static Boolean stIsSetup;
#if MWCCPSP
// IRO_Setup's hook table, one global each (addresses in config/PSP_3_0_1/symbols.manual.txt).
// The original initializes hooks 0, 1, 9, 10 and 20 to defaults (0x4C3EC0,
// 0x4C40A0, 0x4C4380, 0x4C43C0, 0x55EC40); the native build needs them too.
IROHook IRO_Hook0, IRO_Hook2, IRO_Hook3, IRO_Hook5, IRO_Hook6, IRO_Hook7, IRO_Hook8;
IROHook IRO_Hook9, IRO_Hook10, IRO_Hook11, IRO_Hook12, IRO_Hook14, IRO_Hook15;
IROHook IRO_Hook16, IRO_Hook17, IRO_Hook18, IRO_Hook19, IRO_Hook20;
void (*IRO_GetLogFileNameHook)(char* buf, int size);
Boolean (*IRO_IsAddressMultiplyHook)(IROLinear* linear);
void (*IRO_GetAsmEffectsHook)(Statement* stmt, IROAsmEffects* effects);
IROAsmEffects IRO_EmptyAsmEffects;
Boolean IRO_HasDivide;
Boolean IRO_HasCosCall;
Boolean IRO_HasSinCall;
#endif

static void CountRefToObject(Object* object, int depth) {
    static unsigned short LoopUsage[] = {1, 4, 16, 64};
    int index = depth;

    if (index > 3) {
        index = 3;
    }

    object->u.var.info->usage += LoopUsage[index];
    object->u.var.info->used = TRUE;
}

static void CountARef(IROLinear* node, int depth) {
    Object* object;

    object = node->u.node->data.objref;
    CError_ASSERT(78, object->datatype != DALIAS);

    if (object->datatype == DLOCAL && object->u.var.info) {
        CountRefToObject(object, depth);
        if ((node->flags & IROLF_Used) && (node->flags & IROLF_Assigned)) {
            CountRefToObject(object, depth);
        }

        if (!(node->flags & IROLF_Immind) && !object->u.var.info->noregister) {
            object->u.var.info->noregister = 2;
        }
    }
}

static void CountDoubleInd(IROLinear* node, int depth) {
    if (IRO_IsVariable(node)) {
        CountARef(node->u.monadic, depth);
    } else if (node->type == IROLinearOp2Arg) {
        if (node->nodetype == EADD) {
            CountDoubleInd(node->u.diadic.left, depth);
            CountDoubleInd(node->u.diadic.right, depth);
        } else if (IRO_IsAddressMultiply(node)) {
            if (IRO_IsVariable(node->u.diadic.left)) {
                CountARef(node->u.diadic.left->u.monadic, depth);
            }
        }
    }
}

static void CountUsage(void) {
    IRONode* fnode = IRO_FirstNode;
    IROLinear* node;

    if (IRO_FirstNode) {
        for (; fnode; fnode = fnode->nextnode) {
            for (node = fnode->first; node; node = node->next) {
                if (IS_LINEAR_ENODE(node, EOBJREF)) {
                    CountARef(node, fnode->loopdepth);
                } else if (IS_LINEAR_MONADIC(node, EINDIRECT)) {
                    CountDoubleInd(node->u.monadic, fnode->loopdepth);
                }

                if (node->type == IROLinearAsm) {
                    IAEffects effects;
                    int i;

                    CodeGen_GetAsmEffects(node->u.asm_stmt, &effects);
                    for (i = 0; i < effects.numoperands; i++) {
                        Object* object = effects.operands[i].object;
                        if (object->datatype == DLOCAL && object->u.var.info) {
                            CountRefToObject(object, fnode->loopdepth);
                            if (effects.operands[i].type == IAEffect_AddressOf &&
                                !object->u.var.info->noregister) {
                                object->u.var.info->noregister = 2;
                            }
                        }
                    }
                }

                if (node == fnode->last) {
                    break;
                }
            }
        }
    } else {
        for (node = IRO_FirstLinear; node; node = node->next) {
            if (IS_LINEAR_ENODE(node, EOBJREF)) {
                CountARef(node, 0);
            } else if (IS_LINEAR_MONADIC(node, EINDIRECT)) {
                CountDoubleInd(node->u.monadic, 0);
            }
        }
    }

    IRO_CheckForUserBreak();
}

#if MWCCPSP
// 3.0.1's driver: every pass has its own copts switch (set from the
// optimization level by IRO_SetOptimizerOptions, 0x4C4800), the pass loop
// runs only when one of them is on, and the passes are new (decompose and
// recompose of aggregates, conditional simplification, loop reversal, live
// range splitting...). Callees without a known name keep their address.
extern void IRO_ResetDumpTicks(void);
extern void IRO_DumpBeforePhase(char* name, Boolean force);
extern void IRO_CollapseLogicalSelections(Statement* stmt);
extern Statement* IRO_PreLinearize301(Statement* stmt);
extern void IRO_CollectAddressedLabels(IROLinear* first, IROLinear* last);
extern void IRO_DoTransformations301(IROLinear* first);
extern void IRO_EvaluateStandardFunctionCalls(IROLinear* first);
extern void IRO_SerializeAllAssignments(void);
extern void IRO_DecomposeAssignments(void);
extern void IRO_RangePropagateInFNodes(int unused);
extern void IRO_UseDef301(void);
extern Boolean IRO_DeadStore(void);
extern Boolean IRO_PropagateSelfAssignments(void);
extern void IRO_SimplifyIfThenElse(void);
extern Boolean IRO_PullOutInvariantAssignments(void);
extern Boolean IRO_RemoveAllEmptyLoops(void);
extern void IRO_LoopReversal(void);
extern void IRO_DoCommonLoopTransformations(void);
extern void func_0058EB60(void);
extern void func_00594C60(void);
extern void IRO_RecomposeAssignments(void);
extern void IRO_UnpromoteTypesLinear(IROLinear* first, IROLinear* last, Boolean flag);
extern void IRO_RepositionCode(void);
extern void IRO_TransformSelfAssignmentsToAssignments(IROLinear* first);
extern void IRO_CountUsage(void);
extern void IRO_ZapVarPtrs301(void);

extern void* data_00624664;
extern void* data_006242F4;
extern void* data_006242B8;
extern void* data_00625304;
extern void* data_00624C38;
extern void* data_006242BC;

Statement* IRO_Optimizer(Object* func, Statement* statements) {
    Boolean changed;
    Boolean c;
    int passCount;
    int pass;

    CError_ASSERT(165, stIsSetup);

    FunctionName = func;
    IRO_Depends = NULL;

    IRO_SetupForUserBreakChecking();
    IRO_ResetDumpTicks();

    IRO_Dump("Starting function %s\n", func ? func->name->name : "Init-code");
    IRO_Dump("--------------------------------------------------------------------------------\n");

    if (copts.opt_collapselogicalselectionintobittest) {
        if (copts.opt_treetransformation) {
            IRO_TransformTree(statements);
        }
        IRO_CollapseLogicalSelections(statements);
    }
    if (copts.opt_prelinearize) {
        statements = IRO_PreLinearize301(statements);
    }
    if (copts.opt_treetransformation) {
        IRO_TransformTree(statements);
    }

    IRO_Linearize(statements);

    CurStat = NULL;
    IRO_FirstExpr = NULL;
    IRO_LastExpr = NULL;
    IRO_FirstAssign = NULL;
    IRO_LastAssign = NULL;
    IRO_NumNodes = 0;
    IRO_FirstNode = NULL;
    IRO_LastNode = NULL;
    IRO_NodeTable = NULL;
    data_00624664 = NULL;
    data_006242F4 = NULL;
    data_006242B8 = NULL;
    data_00625304 = NULL;
    data_00624C38 = NULL;
    data_006242BC = NULL;

    IRO_CollectAddressedLabels(IRO_FirstLinear, IRO_LastLinear);
    if (copts.opt_arithtransformation) {
        IRO_DoTransformations301(IRO_FirstLinear);
    }

    IRO_BuildFlowgraph(IRO_FirstLinear);
    IRO_DumpAfterPhase("IRO_BuildflowGraph", FALSE);
    IRO_EvaluateStandardFunctionCalls(IRO_FirstLinear);
    IRO_DumpAfterPhase("IRO_EvaluateStandardFunctionCalls", FALSE);

    if (copts.opt_serializeassignments) {
        IRO_SerializeAllAssignments();
        IRO_DumpAfterPhase("IRO_SerializeAllAssignments", FALSE);
    }
    if (copts.opt_decomposeaggregates) {
        IRO_DecomposeAssignments();
        IRO_DumpAfterPhase("IRO_DecomposeAssignments", FALSE);
    }

    IRO_FindAllVars();
    if (copts.warn_uninitializedvar) {
        IRO_CheckInit();
    }

    if (copts.opt_optimizecontrolflow || copts.deadcode) {
        changed = FALSE;
        if (copts.opt_optimizecontrolflow) {
            changed = IRO_EvaluateConditionals();
            IRO_DumpAfterPhase("IRO_EvaluateConditionals", FALSE);
        }
        if (!DisableDueToAsm && copts.deadcode) {
            changed |= IRO_RemoveUnreachable();
            IRO_DumpAfterPhase("IRO_RemoveUnreachable", FALSE);
        }
        if (copts.opt_optimizecontrolflow) {
            changed |= IRO_RemoveRedundantJumps();
            IRO_DumpAfterPhase("IRO_RemoveRedundantJumps", FALSE);
        }
        if (copts.opt_optimizecontrolflow) {
            changed |= IRO_DoJumpChaining();
            IRO_DumpAfterPhase("IRO_DoJumpChaining", FALSE);
        }
        if (!DisableDueToAsm) {
            changed |= IRO_RemoveLabels();
            IRO_DumpAfterPhase("IRO_RemoveLabels()", FALSE);
        }
        if (changed) {
            IRO_BuildFlowgraph(IRO_FirstLinear);
            IRO_DumpAfterPhase("IRO_BuildflowGraph--1", FALSE);
        }
    }

    if (!DisableDueToAsm &&
        (copts.opt_arithtransformation ||
         copts.commonsubs ||
         copts.deadcode ||
         copts.deadstore ||
         copts.opt_foldconstants ||
         copts.unknownEE[0] ||
         copts.loopinvariants ||
         copts.opt_markcounterloops ||
         copts.opt_marknonregtemps ||
         copts.opt_optimizecontrolflow ||
         copts.opt_optimizenonregaccess ||
         copts.propagation ||
         copts.opt_removeemptyloops ||
         copts.opt_reverseloops ||
         copts.opt_rotateloops ||
         copts.opt_scalarize ||
         copts.lifetimes ||
         (copts.unrolling && !copts.optimizesize) ||
         (copts.strengthreduction && !copts.optimizesize))) {
        passCount = copts.multiplepasses ? 2 : 1;
        IRO_DumpBeforePhase("IRO pass loop", FALSE);
        for (pass = 0; pass < passCount; pass++) {
            IRO_Dump("*****************\n");
            IRO_Dump("Dumps for pass = %d\n", pass);
            IRO_Dump("*****************\n");

            if (pass == 0) {
                IRO_SetLoopDepth();
            }
            if (copts.opt_scalarize) {
                IRO_ScalarizeClassDataMembers();
                IRO_DumpAfterPhase("IRO_ScalarizeClassDataMembers", FALSE);
            }
            if (copts.propagation) {
                IRO_CopyAndConstantPropagation();
                IRO_DumpAfterPhase("IRO_CopyAndConstantPropagation", FALSE);
                IRO_ExpressionPropagation();
                IRO_DumpAfterPhase("IRO_ExpressionPropagation", FALSE);
                IRO_RangePropagateInFNodes(0);
                IRO_DumpAfterPhase("IRO_RangePropagateInFNodes", FALSE);
                IRO_UpdateFlagsOnInts(IRO_FirstLinear);
            }
            if (copts.deadstore || copts.propagation) {
                IRO_UseDef301();
                IRO_DumpAfterPhase("IRO_UseDef", FALSE);
                if (copts.deadstore) {
                    IRO_DeadStore();
                    IRO_DumpAfterPhase("IRO_DeadStore", FALSE);
                }
                if (copts.propagation) {
                    IRO_PropagateSelfAssignments();
                    IRO_DumpAfterPhase("IRO_PropagateSelfAssignments", FALSE);
                }
            }

            IRO_UpdateVars();
            if (copts.opt_foldconstants) {
                IRO_ConstantFolding();
                IRO_DumpAfterPhase("IRO_ConstantFolding", FALSE);
            }
            if (copts.opt_optimizecontrolflow) {
                IRO_EvaluateConditionals();
                IRO_DumpAfterPhase("IRO_EvaluateConditionals", FALSE);
            }
            if (copts.deadcode) {
                IRO_RemoveUnreachable();
                IRO_DumpAfterPhase("IRO_RemoveUnreachable", FALSE);
            }
            if (copts.opt_optimizecontrolflow) {
                IRO_SimplifyConditionals();
                IRO_DumpAfterPhase("IRO_SimplifyConditionals", FALSE);
            }
            if (copts.opt_optimizecontrolflow) {
                IRO_SimplifyIfThenElse();
                IRO_DumpAfterPhase("IRO_SimplifyIfThenElse", FALSE);
            }

            if (pass == 0) {
                c = FALSE;
                if (copts.loopinvariants) {
                    IRO_PullOutInvariantAssignments();
                    IRO_DumpAfterPhase("IRO_PullOutInvariantAssignments", FALSE);
                }
                if (copts.opt_removeemptyloops) {
                    c = IRO_RemoveAllEmptyLoops();
                    IRO_DumpAfterPhase("IRO_RemoveAllEmptyLoops", FALSE);
                }
                while (c) {
                    if (copts.loopinvariants) {
                        c = IRO_PullOutInvariantAssignments();
                        IRO_DumpAfterPhase("IRO_PullOutInvariantAssignments", FALSE);
                    }
                    if (c && copts.opt_removeemptyloops) {
                        c = IRO_RemoveAllEmptyLoops();
                        IRO_DumpAfterPhase("IRO_RemoveAllEmptyLoops", FALSE);
                    }
                }
                if (copts.opt_reverseloops) {
                    IRO_LoopReversal();
                    IRO_DumpAfterPhase("IRO_LoopReversal", FALSE);
                }
                if (copts.unrolling && !copts.optimizesize) {
                    IRO_LoopUnroller();
                    IRO_DumpAfterPhase("IRO_LoopUnroller", FALSE);
                }
                if (copts.loopinvariants ||
                    copts.opt_markcounterloops ||
                    copts.opt_marknonregtemps ||
                    copts.opt_optimizenonregaccess ||
                    copts.opt_rotateloops ||
                    (copts.strengthreduction && !copts.optimizesize)) {
                    IRO_DoCommonLoopTransformations();
                    IRO_SetLoopDepth();
                    IRO_DumpAfterPhase("IRO_DoCommonLoopTransformations", FALSE);
                } else if (copts.opt_removeemptyloops ||
                           copts.opt_reverseloops ||
                           (copts.unrolling && !copts.optimizesize)) {
                    IRO_SetLoopDepth();
                }
            }

            if (copts.propagation) {
                IRO_CopyAndConstantPropagation();
                if (copts.opt_foldconstants) {
                    IRO_ConstantFolding();
                }
                if (copts.opt_optimizecontrolflow) {
                    IRO_EvaluateConditionals();
                    IRO_SetLoopDepth();
                }
                IRO_DumpAfterPhase(
                    "Second pass:IRO_CopyAndConstantPropagation, IRO_ConstantFolding, IRO_EvaluateConditionals",
                    FALSE
                );
            }

            if (copts.commonsubs) {
                if (pass == 0) {
                    if (!copts.deadstore && !copts.propagation) {
                        IRO_UseDef301();
                    }
                    func_0058EB60();
                }
                IRO_CommonSubs();
                IRO_DumpAfterPhase("IRO_CommonSubs", FALSE);
            }

            IRO_UpdateFlagsOnInts(IRO_FirstLinear);
            IRO_UpdateVars();
            if (copts.opt_arithtransformation) {
                IRO_DoTransformations301(IRO_FirstLinear);
                IRO_DumpAfterPhase("IRO_DoTransformations", FALSE);
            }
            if (copts.opt_foldconstants) {
                IRO_ConstantFolding();
                IRO_DumpAfterPhase("IRO_ConstantFolding", FALSE);
            }

            do {
                do {
                    IRO_UpdateFlagsOnInts(IRO_FirstLinear);
                    changed = FALSE;
                    if (copts.deadcode) {
                        IRO_RemoveUnreachable();
                        IRO_DumpAfterPhase("IRO_RemoveUnreachable", FALSE);
                    }
                    if (copts.opt_optimizecontrolflow) {
                        changed = IRO_RemoveRedundantJumps();
                        IRO_DumpAfterPhase("IRO_RemoveRedundantJumps", FALSE);
                    }
                    changed |= IRO_RemoveLabels();
                    IRO_DumpAfterPhase("IRO_RemoveLabels", FALSE);
                    if (copts.opt_optimizecontrolflow) {
                        changed |= IRO_DoJumpChaining();
                        IRO_DumpAfterPhase("IRO_DoJumpChaining", FALSE);
                    }
                    if (copts.propagation) {
                        changed |= IRO_CopyAndConstantPropagation();
                        IRO_DumpAfterPhase("IRO_CopyAndConstantPropagation", FALSE);
                        if (copts.opt_foldconstants) {
                            IRO_ConstantFolding();
                            IRO_DumpAfterPhase("IRO_ConstantFolding", FALSE);
                        }
                    }
                    if (copts.deadstore || copts.propagation) {
                        IRO_UseDef301();
                        IRO_DumpAfterPhase("IRO_UseDef", FALSE);
                        if (copts.deadstore) {
                            changed |= IRO_DeadStore();
                            IRO_DumpAfterPhase("IRO_DeadStore", FALSE);
                        }
                        if (copts.propagation) {
                            changed |= IRO_PropagateSelfAssignments();
                            IRO_DumpAfterPhase("IRO_PropagateSelfAssignments", FALSE);
                        }
                    }
                    if (copts.opt_optimizecontrolflow) {
                        changed |= IRO_EvaluateConditionals();
                        IRO_DumpAfterPhase("IRO_EvaluateConditionals", FALSE);
                    }
                } while (changed);
                if (copts.opt_removeemptyloops) {
                    changed = IRO_RemoveAllEmptyLoops();
                    IRO_DumpAfterPhase("IRO_RemoveAllEmptyLoops", FALSE);
                }
            } while (changed);
        }

        if (copts.lifetimes) {
            IRO_UseDef301();
            IRO_SplitLifetimes();
            IRO_DumpAfterPhase("IRO_SplitLifetimes", FALSE);
        }
        if (copts.unknownEE[0]) {
            IRO_ComputeSuccPred();
            func_00594C60();
        }
        if (copts.opt_arithtransformation) {
            IRO_DoTransformations301(IRO_FirstLinear);
            IRO_DumpAfterPhase("IRO_DoTransformations", FALSE);
        }
    }

    if (!DisableDueToAsm && copts.opt_scalarize) {
        IRO_RewriteBitFieldTemps();
        IRO_DumpAfterPhase("IRO_RewriteBitFieldTemps", FALSE);
    }

    if (copts.opt_recomposeaggregates) {
        IRO_RecomposeAssignments();
        IRO_DumpAfterPhase("IRO_RecomposeAssignments", FALSE);
        if (copts.propagation) {
            IRO_CopyAndConstantPropagation();
            IRO_DumpAfterPhase("IRO_CopyAndConstantPropagation", FALSE);
        }
        if (copts.deadstore) {
            IRO_UseDef301();
            IRO_DumpAfterPhase("IRO_UseDef", FALSE);
            IRO_DeadStore();
            IRO_DumpAfterPhase("IRO_DeadStore", FALSE);
        }
    }

    if (!DisableDueToAsm && copts.opt_pointer_analysis && func) {
        IRO_AnalyzePointers(func);
    }

    if (!DisableDueToAsm) {
        IRO_RegenerateExpressions();
        IRO_DumpAfterPhase("IRO_RegenerateExpressions", FALSE);
    }

    if (copts.opt_unpromotetypes && !DisableDueToAsm) {
        IRO_UnpromoteTypesLinear(IRO_FirstLinear, IRO_LastLinear, TRUE);
        IRO_DumpAfterPhase("IRO_UnpromoteTypesLinear", FALSE);
    }

    if (copts.opt_arithtransformation) {
        IRO_DoTransformations301(IRO_FirstLinear);
        IRO_DumpAfterPhase("IRO_DoTransformations", FALSE);
    }

    if (copts.opt_optimizecontrolflow && copts.opt_repositioncode && !DisableDueToAsm) {
        IRO_BuildFlowgraph(IRO_FirstLinear);
        IRO_RepositionCode();
        IRO_DumpAfterPhase("IRO_RepositionCode", FALSE);
    }

    IRO_TransformSelfAssignmentsToAssignments(IRO_FirstLinear);
    IRO_CountUsage();
    IRO_SetLoopDepth();
    IRO_DumpBeforePhase("IRO_Delinearize", FALSE);

    statements = IRO_Delinearize(IRO_FirstNode, NULL);
    IRO_ZapVarPtrs301();
    freeoheap();
    return statements;
}
#else
Statement* IRO_Optimizer(Object* func, Statement* statements) {
    Boolean changed;
    int pass;
    int passCount;

    CError_ASSERT(234, stIsSetup);

    DisableDueToAsm = FALSE;
    FunctionName = func;
    DoScalarize = TRUE;
    DoLinearize = TRUE;
    EarlyReturn = FALSE;
    IRO_Depends = NULL;
    LoopOptimizerRun = FALSE;
    IRO_IsLeafFunction = TRUE;
    IRO_FunctionHasReturn = FALSE;

    IRO_SetupForUserBreakChecking();

    IRO_Dump("Starting function %s\n", func ? func->name->name : "Init-code");
    IRO_Dump("--------------------------------------------------------------------------------\n");

    if (DoLinearize) {
        IRO_PreLinearize(statements);
    }
    if (copts.optimizationlevel > 0) {
        IRO_TransformTree(statements);
    }

    VectorPhaseCalledFromUnroll = FALSE;

    IRO_Linearize(statements);

    CurStat = NULL;

    IRO_FirstExpr = NULL;
    IRO_LastExpr = NULL;
    IRO_FirstAssign = NULL;
    IRO_LastAssign = NULL;
    IRO_FirstVarUse = NULL;
    IRO_LastVarUse = NULL;
    IRO_FirstNode = NULL;
    IRO_LastNode = NULL;

    if (copts.optimizationlevel > 0) {
        IRO_DoTransformations();
    }

    IRO_BuildFlowgraph(IRO_FirstLinear);
    IRO_DumpAfterPhase("IRO_BuildflowGraph", FALSE);

    IRO_FindAllVars();
    if (copts.warn_uninitializedvar) {
        IRO_CheckInit();
    }

    if (!DisableDueToAsm && copts.optimizationlevel > 0 && copts.opt_pointer_analysis && func) {
        IRO_AnalyzePointers(func);
        if (copts.propagation && IRO_EvaluateDefinitePointers(func)) {
            IRO_UpdateFlagsOnInts();
            IRO_UpdateVars();
            IRO_DumpAfterPhase("IRO_EvaluateDefinitePointers", FALSE);
        }
    }

    if (copts.optimizationlevel > 0) {
        changed = IRO_EvaluateConditionals();

        if (!DisableDueToAsm) {
            changed |= IRO_RemoveUnreachable();
            IRO_DumpAfterPhase("IRO_RemoveUnreachable", FALSE);
        }

        changed |= IRO_RemoveRedundantJumps();
        IRO_DumpAfterPhase("IRO_RemoveRedundantJumps", FALSE);

        if (!DisableDueToAsm) {
            changed |= IRO_RemoveLabels();
            IRO_DumpAfterPhase("IRO_RemoveLabels()", FALSE);
        }

        if (changed) {
            IRO_BuildFlowgraph(IRO_FirstLinear);
            IRO_DumpAfterPhase("IRO_BuildflowGraph--1", FALSE);
        }
    }

    if (!DisableDueToAsm && copts.optimizationlevel > 0) {
        passCount = copts.multiplepasses ? 2 : 1;
        IRO_CPFirstTime = TRUE;
        for (pass = 0; pass < passCount; pass++) {
            IRO_Dump("*****************\n");
            IRO_Dump("Dumps for pass=%d\n", pass);
            IRO_Dump("*****************\n");

            if (DoScalarize) {
                IRO_ScalarizeClassDataMembers();
            }
            IRO_DumpAfterPhase("IRO_ScalarizeClassDataMembers", FALSE);

            if (copts.propagation) {
                IRO_CopyAndConstantPropagation();
                IRO_CPFirstTime = FALSE;
                IRO_ExpressionPropagation();
                IRO_DumpAfterPhase("Copy and constant propagation", FALSE);

                IRO_RangePropagateInFNode();
                IRO_DumpAfterPhase("IRO_RangePropagateInFNode", FALSE);
                IRO_UpdateFlagsOnInts();
            }

            IRO_DumpAfterPhase("IRO_ExpressionPropagation", FALSE);

            if (copts.deadstore || copts.propagation) {
                IRO_UseDef(copts.deadstore, copts.propagation);
            }
            IRO_DumpAfterPhase("after IRO_UseDef", FALSE);

            IRO_UpdateVars();
            IRO_ConstantFolding();
            IRO_DumpAfterPhase("IRO_ConstantFolding", FALSE);

            IRO_EvaluateConditionals();
            IRO_RemoveUnreachable();
            IRO_SimplifyConditionals();

            if (pass == 1 && copts.optimizationlevel > 2) {
                IRO_RenumberInts();
                IRO_DumpAfterPhase("Before IRO_FindEmptyLoops", FALSE);
                IRO_FindEmptyLoops();
                IRO_DumpAfterPhase("After IRO_FindEmptyLoops", FALSE);
                IRO_RenumberInts();
            }

            if (copts.unrolling && !copts.optimizesize && pass == 0) {
                IRO_DumpAfterPhase("Before IRO_LoopUnroller", FALSE);
                IRO_LoopUnroller();
                IRO_DumpAfterPhase("After IRO_LoopUnroller", FALSE);
                IRO_RenumberInts();
            }

            VectorPhaseCalledFromUnroll = FALSE;

            if (pass == 0 && (copts.loopinvariants || copts.strengthreduction)) {
                IRO_DumpAfterPhase("Before IRO_FindLoops", FALSE);
                IRO_FindLoops();
                LoopOptimizerRun = TRUE;
                IRO_SetLoopDepth();
            }
            IRO_DumpAfterPhase("After IRO_FindLoops", FALSE);

            if (copts.propagation) {
                IRO_CopyAndConstantPropagation();
                IRO_ConstantFolding();
                IRO_EvaluateConditionals();
            }

            IRO_DumpAfterPhase(
                "Second pass:IRO_CopyAndConstantPropagation, IRO_ConstantFolding, IRO_EvaluateConditionals",
                FALSE
            );

            if (copts.commonsubs) {
                IRO_FindExpressions(NULL, FALSE);
            }

            if (copts.commonsubs) {
                IRO_ComputeAvail();
                IRO_CommonSubs();
            }
            IRO_DumpAfterPhase("IRO_CommonSubs", FALSE);

            IRO_UpdateFlagsOnInts();
            IRO_UpdateVars();
            IRO_DoTransformations();
            IRO_ConstantFolding();

            do {
                IRO_UpdateFlagsOnInts();
                changed = FALSE;

                if (copts.deadcode) {
                    IRO_RemoveUnreachable();
                }
                IRO_DumpAfterPhase("IRO_RemoveUnreachable", FALSE);

                changed |= IRO_RemoveRedundantJumps();
                IRO_DumpAfterPhase("IRO_RemoveRedundantJumps", FALSE);

                changed |= IRO_RemoveLabels();
                IRO_DumpAfterPhase("IRO_RemoveLabels", FALSE);

                changed |= IRO_DoJumpChaining();
                IRO_DumpAfterPhase("IRO_DoJumpChaining", FALSE);

                if (copts.propagation) {
                    IRO_RenumberInts();
                    IRO_DumpAfterPhase("Before IRO_CopyAndConstantPropagation", FALSE);
                    changed |= IRO_CopyAndConstantPropagation();
                    IRO_DumpAfterPhase("After IRO_CopyAndConstantPropagation", FALSE);
                    IRO_ConstantFolding();
                }

                if (copts.deadstore || copts.propagation) {
                    changed |= IRO_UseDef(copts.deadstore, copts.propagation);
                }
                IRO_DumpAfterPhase("IRO_UseDef", FALSE);

                changed |= IRO_EvaluateConditionals();
                IRO_DumpAfterPhase("IRO_EvaluateConditionals", FALSE);
            } while (changed);
        }

        if (copts.lifetimes) {
            IRO_UseDef(FALSE, FALSE);
            IRO_SplitLifetimes();
        }

        IRO_DoTransformations();
        IRO_DumpAfterPhase("Before RebuildCondExpressions", FALSE);
    }

    IRO_RenumberInts();
    IRO_DumpAfterPhase("before IRO_RewriteBitFieldTemps", FALSE);
    IRO_RewriteBitFieldTemps();
    IRO_DumpAfterPhase("After IRO_RewriteBitFieldTemps", FALSE);

    if (!DisableDueToAsm) {
        IRO_RegenerateExpressions();
        IRO_DumpAfterPhase("IRO_RegenerateExpressions", FALSE);
    }

    CountUsage();

    IRO_DumpAfterPhase("After IRO_Optimizer", FALSE);

    statements = IRO_Delinearize(IRO_FirstNode, NULL);

    IRO_ZapVarPtrs();
    ResetVars();
    freeoheap();
    return statements;
}
#endif

#if MWCCPSP
// 3.0.1 (0x4C40B0): a new table between IsSubableOp and AssignmentOp in
// IRO_Setup's order; TRUE for the pure arithmetic operators (unary minus,
// ~, !, * / % + - << >>, & ^ |, min/max). Name not known.
Boolean IRO_IsArithmeticOp[IRO_MAXEXPR];

void IRO_InitializeIsArithmeticOpArray(void) {
    int i;

    for (i = 0; i < IRO_MAXEXPR; i++) {
        IRO_IsArithmeticOp[i] = FALSE;
    }

    IRO_IsArithmeticOp[EPOSTINC] = FALSE;
    IRO_IsArithmeticOp[EPOSTDEC] = FALSE;
    IRO_IsArithmeticOp[EPREINC] = FALSE;
    IRO_IsArithmeticOp[EPREDEC] = FALSE;
    IRO_IsArithmeticOp[EINDIRECT] = FALSE;
    IRO_IsArithmeticOp[EMONMIN] = TRUE;
    IRO_IsArithmeticOp[EBINNOT] = TRUE;
    IRO_IsArithmeticOp[ELOGNOT] = TRUE;
    IRO_IsArithmeticOp[EFORCELOAD] = FALSE;
    IRO_IsArithmeticOp[EMUL] = TRUE;
    IRO_IsArithmeticOp[EMULV] = TRUE;
    IRO_IsArithmeticOp[EDIV] = TRUE;
    IRO_IsArithmeticOp[EMODULO] = TRUE;
    IRO_IsArithmeticOp[EADDV] = TRUE;
    IRO_IsArithmeticOp[ESUBV] = TRUE;
    IRO_IsArithmeticOp[EADD] = TRUE;
    IRO_IsArithmeticOp[ESUB] = TRUE;
    IRO_IsArithmeticOp[ESHL] = TRUE;
    IRO_IsArithmeticOp[ESHR] = TRUE;
    IRO_IsArithmeticOp[ELESS] = FALSE;
    IRO_IsArithmeticOp[EGREATER] = FALSE;
    IRO_IsArithmeticOp[ELESSEQU] = FALSE;
    IRO_IsArithmeticOp[EGREATEREQU] = FALSE;
    IRO_IsArithmeticOp[EEQU] = FALSE;
    IRO_IsArithmeticOp[ENOTEQU] = FALSE;
    IRO_IsArithmeticOp[EAND] = TRUE;
    IRO_IsArithmeticOp[EXOR] = TRUE;
    IRO_IsArithmeticOp[EOR] = TRUE;
    IRO_IsArithmeticOp[ELAND] = FALSE;
    IRO_IsArithmeticOp[ELOR] = FALSE;
    IRO_IsArithmeticOp[EASS] = FALSE;
    IRO_IsArithmeticOp[EMULASS] = FALSE;
    IRO_IsArithmeticOp[EDIVASS] = FALSE;
    IRO_IsArithmeticOp[EMODASS] = FALSE;
    IRO_IsArithmeticOp[EADDASS] = FALSE;
    IRO_IsArithmeticOp[ESUBASS] = FALSE;
    IRO_IsArithmeticOp[ESHLASS] = FALSE;
    IRO_IsArithmeticOp[ESHRASS] = FALSE;
    IRO_IsArithmeticOp[EANDASS] = FALSE;
    IRO_IsArithmeticOp[EXORASS] = FALSE;
    IRO_IsArithmeticOp[EORASS] = FALSE;
    IRO_IsArithmeticOp[ECOMMA] = FALSE;
    IRO_IsArithmeticOp[EMIN] = TRUE;
    IRO_IsArithmeticOp[EMAX] = TRUE;
    IRO_IsArithmeticOp[EPMODULO] = FALSE;
    IRO_IsArithmeticOp[EROTL] = FALSE;
    IRO_IsArithmeticOp[EROTR] = FALSE;
    IRO_IsArithmeticOp[EBCLR] = FALSE;
    IRO_IsArithmeticOp[EBTST] = FALSE;
    IRO_IsArithmeticOp[EBSET] = FALSE;
    IRO_IsArithmeticOp[ETYPCON] = FALSE;
    IRO_IsArithmeticOp[EBITFIELD] = FALSE;
    IRO_IsArithmeticOp[EINTCONST] = FALSE;
    IRO_IsArithmeticOp[EFLOATCONST] = FALSE;
    IRO_IsArithmeticOp[ESTRINGCONST] = FALSE;
    IRO_IsArithmeticOp[ECOND] = FALSE;
    IRO_IsArithmeticOp[EFUNCCALL] = FALSE;
    IRO_IsArithmeticOp[EFUNCCALLP] = FALSE;
    IRO_IsArithmeticOp[EOBJREF] = FALSE;
    IRO_IsArithmeticOp[ENULLCHECK] = FALSE;
    IRO_IsArithmeticOp[EPRECOMP] = FALSE;
    IRO_IsArithmeticOp[ELABEL] = FALSE;
    IRO_IsArithmeticOp[ESCOPEBEGIN] = FALSE;
    IRO_IsArithmeticOp[ESCOPEEND] = FALSE;
    IRO_IsArithmeticOp[EMFPOINTER] = FALSE;
    IRO_IsArithmeticOp[ETEMP] = FALSE;
    IRO_IsArithmeticOp[ELOCOBJ] = FALSE;
    IRO_IsArithmeticOp[EARGOBJ] = FALSE;
    IRO_IsArithmeticOp[ESETCONST] = FALSE;
    IRO_IsArithmeticOp[ENEWEXCEPTION] = FALSE;
    IRO_IsArithmeticOp[ENEWEXCEPTIONARRAY] = FALSE;
    IRO_IsArithmeticOp[EINITTRYCATCH] = FALSE;
    IRO_IsArithmeticOp[EOBJACCESS] = FALSE;
    IRO_IsArithmeticOp[ETEMPLDEP] = FALSE;
    IRO_IsArithmeticOp[EPOINTERSTAR] = FALSE;
    IRO_IsArithmeticOp[EDOTSTAR] = FALSE;
    IRO_IsArithmeticOp[ECTORINIT] = FALSE;
    IRO_IsArithmeticOp[ESTMT] = FALSE;
    IRO_IsArithmeticOp[EINSTRUCTION] = FALSE;
    IRO_IsArithmeticOp[EDEFINE] = FALSE;
    IRO_IsArithmeticOp[EREUSE] = FALSE;
    IRO_IsArithmeticOp[EASSBLK] = FALSE;
    IRO_IsArithmeticOp[ECONDASS] = FALSE;
}

// The back end's default IsAddressMultiply hook (0x4C40A0).
Boolean IRO_DefaultIsAddressMultiply(IROLinear* linear) {
    return FALSE;
}

static Boolean ENodeArraysHaveBeenInitialized;

void IRO_Setup(IROHooks* hooks) {
    if (!stIsSetup) {
        IRO_Log = FALSE;
        if (!ENodeArraysHaveBeenInitialized) {
            IRO_InitializeNodeNamesArray();
            IRO_InitializeIsAssociativeENodeTypeArray();
            IRO_InitializeIsSubableOpArray();
            IRO_InitializeIsArithmeticOpArray();
            IRO_InitializeAssignmentOpArray();
            IRO_InitializeComplementaryOpArray();
            IRO_InitializeComplementaryOpLogicalArray();
            IRO_InitializeNonAssignmentOpArray();
            IRO_InitializeAssignmentFoldingFunctionArray();
            IRO_InitializeIRO_IsModifyOpArray();
            IRO_InitializeIRO_IsAssignOpArray();
            ENodeArraysHaveBeenInitialized = TRUE;
        }
        if (hooks) {
            IRO_Hook0 = hooks->hook0;
            IRO_IsAddressMultiplyHook = hooks->isAddressMultiply;
            IRO_Hook2 = hooks->hook2;
            IRO_Hook3 = hooks->hook3;
            IRO_GetAsmEffectsHook = hooks->getAsmEffects;
            IRO_Hook5 = hooks->hook5;
            IRO_Hook6 = hooks->hook6;
            IRO_Hook7 = hooks->hook7;
            IRO_Hook8 = hooks->hook8;
            IRO_Hook9 = hooks->hook9;
            IRO_Hook10 = hooks->hook10;
            IRO_Hook11 = hooks->hook11;
            IRO_Hook12 = hooks->hook12;
            IRO_GetLogFileNameHook = hooks->getLogFileName;
            IRO_Hook14 = hooks->hook14;
            IRO_Hook15 = hooks->hook15;
            IRO_Hook16 = hooks->hook16;
            IRO_Hook17 = hooks->hook17;
            IRO_Hook18 = hooks->hook18;
            IRO_Hook19 = hooks->hook19;
            IRO_Hook20 = hooks->hook20;
        }
        IRO_SetupDump();
        stIsSetup = TRUE;
    }
}
#else
void IRO_Setup(void) {
    static Boolean ENodeArraysHaveBeenInitialized;

    if (!stIsSetup) {
        IRO_Log = FALSE;
        IRO_SetupDump();
        if (!ENodeArraysHaveBeenInitialized) {
            IRO_InitializeNodeNamesArray();
            IRO_InitializeIsAssociativeENodeTypeArray();
            IRO_InitializeIsSubableOpArray();
            IRO_InitializeAssignmentOpArray();
            IRO_InitializeComplementaryOpArray();
            IRO_InitializeComplementaryOpLogicalArray();
            IRO_InitializeNonAssignmentOpArray();
            IRO_InitializeAssignmentFoldingFunctionArray();
            IRO_InitializeIRO_IsModifyOpArray();
            IRO_InitializeIRO_IsAssignOpArray();
            ENodeArraysHaveBeenInitialized = TRUE;
        }
        stIsSetup = TRUE;
    }
}
#endif

#if MWCCPSP
// 3.0.1 (0x4C4800): each optimizer switch from the optimization level
void IRO_UpdateOptimizerOptions(void) {
    copts.deadcode = '\0' < copts.optimizationlevel;
    copts.commonsubs = '\x01' < copts.optimizationlevel;
    copts.loopinvariants = '\x02' < copts.optimizationlevel;
    copts.propagation = copts.commonsubs;
    copts.deadstore = copts.commonsubs;
    copts.strengthreduction = copts.loopinvariants;
    copts.strengthreductionstrict = 1;
    copts.unknownEE[0] = 0;
    copts.lifetimes = copts.loopinvariants;
    copts.unrolling = copts.loopinvariants;
    copts.vectorizeloops = copts.loopinvariants;
    copts.multiplepasses = '\x03' < copts.optimizationlevel;
    copts.opt_scalarize = copts.deadcode;
    copts.opt_prelinearize = 1;
    copts.opt_treetransformation = copts.deadcode;
    copts.opt_arithtransformation = copts.deadcode;
    copts.opt_rebuildconditionals = copts.deadcode;
    copts.opt_rebuildlogicals = copts.deadcode;
    copts.opt_generateconditionalassignments = 0;
    copts.opt_removeemptyloops = copts.loopinvariants;
    copts.opt_alwaysdumpopts = 0;
    copts.opt_rotateloops = copts.loopinvariants;
    copts.opt_optimizenonregaccess = copts.loopinvariants;
    copts.opt_movepostops = 0;
    copts.opt_findoptimalunrollfactor = 1;
    copts.opt_findoptimalvectorunrollfactor = 1;
    copts.opt_unrollpostloop = 1;
    copts.opt_unrollvectorpostloop = 1;
    copts.opt_createvectortemps = 1;
    copts.opt_unpromotetypes = 0;
    copts.opt_dumptypes = 0;
    copts.opt_scalarizebitfields = 1;
    copts.opt_propagatemultiplydivideassignments = 1;
    copts.opt_moveinvariantsinaddressexpr = 1;
    copts.opt_convertfpconsts = 1;
    copts.opt_markcounterloops = 0;
    copts.opt_markvariablecounterloops = 0;
    copts.opt_pulloutconstants = 0;
    copts.opt_defineargorder = 0;
    copts.opt_useexpressioncostswhenlinearizingassignments = 0;
    copts.opt_intelcompareoplinearizationhack = 0;
    copts.opt_rearrangemultiplesubscripts = 0;
    copts.opt_returnloopdepthinstatementvalue = 0;
    copts.opt_marknonregtemps = 0;
    copts.opt_addgotounrolledlooptest = 0;
    copts.opt_addtogetherconstantpartsoftreeexpressions = 1;
    copts.opt_propagateequivalentscalartypes = 0;
    copts.opt_unpromoteenumtointconversions = 0;
    copts.opt_unpromotemultipliestochartype = 1;
    copts.opt_dospecialmultiplyunpromotion = 0;
    copts.opt_docrossconversionofinductionreference = 0;
    copts.opt_intrinsicfunctioncallscanhavesideeffects = 1;
    copts.opt_collapselogicalselectionintobittest = copts.commonsubs;
    copts.opt_collapselogicalselectionintobittest_uselonglong = 0;
    copts.opt_foldconstants = copts.deadcode;
    copts.opt_optimizecontrolflow = copts.deadcode;
    copts.opt_serializeassignments = 0;
    copts.opt_branchlessexpensivethandiv = 0;
    copts.opt_repositioncode = copts.deadcode;
    copts.opt_decomposeaggregates = copts.loopinvariants;
    copts.opt_recomposeaggregates = copts.loopinvariants;
    copts.opt_scalarizeliveranges = copts.loopinvariants;
}
#endif
#if MWCCPSP

#endif
void IRO_Cleanup(void) {
    if (stIsSetup) {
        IRO_CleanupDump();
        stIsSetup = FALSE;
    }
}

#if !MWCCPSP // 3.0.1: in the backend's CodeGen.c (src/psp/backend), different flags
void CodeGen_UpdateOptimizerOptions(void) {
    copts.deadcode = copts.optimizationlevel >= 1;
    copts.commonsubs = copts.propagation = copts.optimizationlevel >= 2;
    copts.loopinvariants = copts.strengthreduction = copts.lifetimes = copts.deadstore =
        copts.unrolling = copts.vectorizeloops = copts.optimizationlevel >= 3;
    copts.multiplepasses = copts.optimizationlevel >= 4;
}
#endif
