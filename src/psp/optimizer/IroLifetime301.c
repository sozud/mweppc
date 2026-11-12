// mwccpsp 3.0.1: the variables live at each linear (func_00594C60 and its
// helpers, 0x594C60..0x5963C0; IroLifetime.c). A backward liveness pass over
// the flowgraph:
//
//   1. per node, in (+0x1C) gets the variables used before any definition in
//      the node, gen (+0x24) the variables the node defines (out, +0x20, is
//      cleared; kill, +0x28, is zeroed);
//   2. out = the union of the successors' in, in |= out - gen, to a fixpoint;
//   3. the linears are renumbered, then each node is walked from its last
//      linear back (the linear list is reversed meanwhile, so `next` goes
//      backwards), keeping the live set in data_00608CD0; every variable's
//      lifetime vector (VarRecord +8, `unused`) gets the bit of each linear the
//      variable is live at, and the bit of a dead store.
//
// IroEmptyLoop301.c asks whether an induction is live at a loop's exit.
#include "compiler/common.h"
#include "compiler/BitVector.h"
#include "compiler/CError.h"
#include "compiler/CFunc.h"
#include "compiler/CParser.h"
#include "compiler/enode.h"
#include "compiler/Exceptions.h"
#include "compiler/IrOptimizer.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroMalloc.h"
#include "compiler/IroPointerAnalysis.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"
#include "compiler/objects.h"
#include "compiler/types.h"

// IROAsmOperand.type (InlineAsm.h's IAEffect order; not visible to the PSP build)
#ifndef IAEffect_Read
enum {
    IAEffect_Read = 0,
    IAEffect_Write = 1,
    IAEffect_Clobber = 2,
    IAEffect_AddressOf = 3,
    IAEffect_ReadWrite = 4
};
#endif

extern VarRecord* IRO_FindAssigned301(IROLinear* linear, Boolean* isBitfield);
extern Boolean IRO_FunctionCallMightThrowException(IROLinear* linear);
extern void Bv_AllocVectorLocal(BitVector** bv, UInt32 size);

BitVector* data_00608CD0; // the variables live at the current linear

// 0x595D60: an exception action of the list names obj
Boolean func_00595D60(ExceptionAction* action, Object* obj) {
    for (; action; action = action->prev) {
        switch (action->type) {
            case EAT_DESTROYLOCAL:
                if (action->data.destroy_local.local == obj) {
                    return TRUE;
                }
                break;
            case EAT_DESTROYLOCALCOND:
                if (action->data.destroy_local_cond.local == obj ||
                    action->data.destroy_local_cond.cond == obj) {
                    return TRUE;
                }
                break;
            case EAT_DESTROYLOCALOFFSET:
                if (action->data.destroy_local_offset.local == obj) {
                    return TRUE;
                }
                break;
            case EAT_DESTROYLOCALPOINTER:
                if (action->data.destroy_local_pointer.pointer == obj) {
                    return TRUE;
                }
                break;
            case EAT_DESTROYLOCALARRAY:
                if (action->data.destroy_local_array.localarray == obj) {
                    return TRUE;
                }
                break;
            case EAT_DESTROYPARTIALARRAY:
                if (action->data.destroy_partial_array.arraypointer == obj ||
                    action->data.destroy_partial_array.arraycounter == obj ||
                    action->data.destroy_partial_array.dtor == obj ||
                    action->data.destroy_partial_array.element_size == obj) {
                    return TRUE;
                }
                break;
            case EAT_DESTROYMEMBER:
                if (action->data.destroy_member.objectptr == obj) {
                    return TRUE;
                }
                break;
            case EAT_DESTROYMEMBERCOND:
                if (action->data.destroy_member_cond.objectptr == obj ||
                    action->data.destroy_member_cond.cond == obj) {
                    return TRUE;
                }
                break;
            case EAT_DESTROYMEMBERARRAY:
                if (action->data.destroy_member_array.objectptr == obj) {
                    return TRUE;
                }
                break;
            case EAT_DELETEPOINTER:
                if (action->data.delete_pointer.pointerobject == obj) {
                    return TRUE;
                }
                break;
            case EAT_DELETEPOINTERCOND:
                if (action->data.delete_pointer_cond.pointerobject == obj ||
                    action->data.delete_pointer_cond.cond == obj) {
                    return TRUE;
                }
                break;
            case EAT_DESTROYBASE:
                if (action->data.destroy_base.objectptr == obj) {
                    return TRUE;
                }
                break;
            default:
                break;
        }
    }
    return FALSE;
}

// a call (or an asm that may throw) can read var: a global, an addressed
// variable, or one an exception action of a throwing call names
static inline Boolean CallMayRead(IROLinear* nd, VarRecord* var) {
    if (Inline_IsObjectData(var->object) || var->noregister) {
        return TRUE;
    }
    return nd->stmt &&
        IRO_FunctionCallMightThrowException(nd) &&
        func_00595D60(nd->stmt->dobjstack, var->object);
}

// 0x595880: out gets the variables the call may read, but those in skip
// (when given): its dependencies from pointer analysis when every target is
// known, otherwise every variable a call can read
void func_00595880(IROLinear* call, BitVector* skip, BitVector* out) {
    IROLinear* callee;
    IROListNode* resultList;
    IROListNode* scan;
    IROListNode* nextScan;
    IROLinear* scannd;
    Object* obj;
    ObjectList* depsList;
    ObjectList* olist;
    VarRecord* var;
    UInt32 index;
    Boolean unknown;
    Boolean foundObjRef;

    unknown = FALSE;
    callee = call->u.funccall.callee;
    if (callee && copts.opt_pointer_analysis && callee->pointsToFunction && FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupLinearNodePointerExpr(FunctionName, callee, &resultList);
        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail) {
                    unknown = TRUE;
                    break;
                }
                foundObjRef = FALSE;
                for (scannd = scan->list.head; scannd != scan->list.tail->next;
                     scannd = scannd->next) {
                    if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                        foundObjRef = TRUE;
                        obj = scannd->u.node->data.objref;
                        CError_ASSERT(327, obj != NULL);
                        depsList = NULL;
                        PointerAnalysis_GetFunctionDependencies(obj, call, &depsList);
                        for (olist = depsList; olist; olist = olist->next) {
                            if (!olist->object) {
                                unknown = TRUE;
                                break;
                            }
                        }
                        while (depsList) {
                            olist = depsList->next;
                            IRO_free2(depsList);
                            depsList = olist;
                        }
                        if (unknown) {
                            break;
                        }
                    }
                }
                if (!foundObjRef) {
                    unknown = TRUE;
                }
                if (unknown) {
                    break;
                }
            }

            if (!unknown) {
                for (scan = resultList; scan; scan = scan->nextList) {
                    for (scannd = scan->list.head; scannd != scan->list.tail->next;
                         scannd = scannd->next) {
                        if (scannd->type == IROLinearOperand && scannd->u.node->type == EOBJREF) {
                            obj = scannd->u.node->data.objref;
                            depsList = NULL;
                            PointerAnalysis_GetFunctionDependencies(obj, call, &depsList);
                            for (olist = depsList; olist; olist = olist->next) {
                                var = IRO_FindVar(olist->object, TRUE, TRUE);
                                CError_ASSERT(378, var != NULL);
                                index = var->index;
                                CError_ASSERT(380, index != 0);
                                if ((!skip || !Bv_IsBitSet(index, skip)) &&
                                    CallMayRead(call, var)) {
                                    Bv_SetBit(index, out);
                                }
                            }
                            while (depsList) {
                                olist = depsList->next;
                                IRO_free2(depsList);
                                depsList = olist;
                            }
                        }
                    }
                }
            }

            while (resultList) {
                nextScan = resultList->nextList;
                IRO_free2(resultList);
                resultList = nextScan;
            }
        } else {
            unknown = TRUE;
        }
    } else {
        unknown = TRUE;
    }

    if (unknown) {
        for (var = IRO_FirstVar; var; var = var->next) {
            index = var->index;
            if ((!skip || !Bv_IsBitSet(index, skip)) && CallMayRead(call, var)) {
                Bv_SetBit(index, out);
            }
        }
    }
}

// the value of an assignment is used: so is the variable it assigns
static inline void UseAssignedValue(IROLinear* nd) {
    Object* obj;
    VarRecord* var;

    if (nd->type == IROLinearOp2Arg &&
        nd->nodetype == EASS &&
        (obj = IRO_IsVariable(nd->u.diadic.left)) &&
        (var = IRO_FindVar(obj, FALSE, TRUE))) {
        Bv_SetBit(var->index, data_00608CD0);
    }
}

// 0x595E30: the variables nd reads become live
void func_00595E30(IROLinear* nd) {
    Object* obj;
    VarRecord* var;
    SInt32 i;

    if (!(nd->flags & IROLF_Assigned) || (nd->flags & IROLF_Used)) {
        if ((obj = IRO_IsVariable(nd)) && (var = IRO_FindVar(obj, FALSE, TRUE))) {
            Bv_SetBit(var->index, data_00608CD0);
        }
    }

    switch (nd->type) {
        case IROLinearOperand:
            if (nd->u.node->type == EOBJREF &&
                (nd->flags & IROLF_Ind) &&
                (!(nd->flags & IROLF_Assigned) || (nd->flags & IROLF_Used))) {
                if ((var = IRO_FindVar(nd->u.node->data.objref, FALSE, TRUE))) {
                    Bv_SetBit(var->index, data_00608CD0);
                }
            }
            break;
        case IROLinearOp1Arg:
        case IROLinearGotoExpr:
            func_00595E30(nd->u.monadic);
            UseAssignedValue(nd->u.monadic);
            break;
        case IROLinearOp2Arg:
            if (nd->nodetype != ECOMMA) {
                func_00595E30(nd->u.diadic.left);
                UseAssignedValue(nd->u.diadic.left);
            }
            func_00595E30(nd->u.diadic.right);
            UseAssignedValue(nd->u.diadic.right);
            break;
        case IROLinearFunccall:
            func_00595E30(nd->u.funccall.callee);
            UseAssignedValue(nd->u.funccall.callee);
            for (i = 0; i < nd->u.funccall.argCount; i++) {
                UseAssignedValue(nd->u.funccall.args[i]);
            }
            break;
        case IROLinearIf:
        case IROLinearIfNot:
            func_00595E30(nd->u.label.cond);
            UseAssignedValue(nd->u.label.cond);
            break;
        case IROLinearReturn:
            if (nd->u.monadic) {
                func_00595E30(nd->u.monadic);
                UseAssignedValue(nd->u.monadic);
            }
            break;
        case IROLinearSwitch:
            func_00595E30(nd->u.swtch.cond);
            UseAssignedValue(nd->u.swtch.cond);
            break;
        default:
            break;
    }
}

// an asm operand writes all of the variable
static inline Boolean AsmWritesWhole(IROAsmOperand* op) {
    return op->offset == 0 && op->object->type->size == op->size;
}

// 0x594C60: the variables live at each linear (VarRecord +8)
void func_00594C60(void) {
    IRONode* node;
    IROLinear* nd;
    IROLinear* rev;
    IROLinear* next;
    VarRecord* var;
    BitVector* vec;
    BitVector* tmp;
    UInt32 index;
    UInt16 i;
    SInt32 j;
    Boolean changed;

    // 1. the uses before a definition (in) and the definitions (gen) per node
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        Bv_AllocVector(&node->in, IRO_NumVars + 1);
        Bv_AllocVector(&node->gen, IRO_NumVars + 1);
        Bv_AllocVector(&node->out, IRO_NumVars + 1);
        node->kill = NULL;
        for (nd = node->first; nd; nd = nd->next) {
            if (nd->type == IROLinearFunccall) {
                func_00595880(nd, node->gen, node->in);
            } else if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
                if (!(nd->flags & IROLF_Assigned) || (nd->flags & IROLF_Used)) {
                    if ((var = IRO_FindVar(nd->u.node->data.objref, FALSE, TRUE)) &&
                        !Bv_IsBitSet(var->index, node->gen)) {
                        Bv_SetBit(var->index, node->in);
                    }
                }
            } else if (IRO_IsAssignment(nd)) {
                if ((var = IRO_FindAssigned301(nd, NULL)) &&
                    var->object->type->size == nd->rtype->size) {
                    Bv_SetBit(var->index, node->gen);
                }
            } else if (nd->type == IROLinearAsm && IRO_GetAsmEffectsHook && nd->u.asm_stmt) {
                IROAsmEffects effects = IRO_EmptyAsmEffects;
                IRO_GetAsmEffectsHook(nd->u.asm_stmt, &effects);
                index = 0;
                for (j = 0; j < effects.numoperands; j++) {
                    if ((var = IRO_FindVar(effects.operands[j].object, FALSE, TRUE))) {
                        index = var->index;
                    }
                    switch (effects.operands[j].type) {
                        case IAEffect_Read:
                            if (var && !Bv_IsBitSet(index, node->gen)) {
                                Bv_SetBit(index, node->in);
                            }
                            break;
                        case IAEffect_Write:
                            if (var && AsmWritesWhole(&effects.operands[j])) {
                                Bv_SetBit(index, node->gen);
                            }
                            break;
                        case IAEffect_ReadWrite:
                            if (var) {
                                if (!Bv_IsBitSet(index, node->gen)) {
                                    Bv_SetBit(index, node->in);
                                }
                                if (AsmWritesWhole(&effects.operands[j])) {
                                    Bv_SetBit(index, node->gen);
                                }
                            }
                            break;
                        default:
                            break;
                    }
                }
                if (effects.flag2) {
                    Bv_Set(node->in);
                    Bv_Minus(node->in, node->gen);
                }
                if (effects.mayThrow) {
                    for (var = IRO_FirstVar; var; var = var->next) {
                        if (!Bv_IsBitSet(var->index, node->gen) && CallMayRead(nd, var)) {
                            Bv_SetBit(var->index, node->in);
                        }
                    }
                }
            }
            if (nd == node->last) {
                break;
            }
        }
    }

    // 2. out = the successors' in; in |= out - gen
    Bv_AllocVector(&tmp, IRO_NumVars + 1);
    do {
        changed = FALSE;
        for (node = IRO_FirstNode; node; node = node->nextnode) {
            Bv_Clear(tmp);
            for (i = 0; i < node->numsucc; i++) {
                Bv_Or(IRO_NodeTable[node->succ[i]]->in, tmp);
            }
            if (!Bv_Compare(tmp, node->out)) {
                Bv_Copy(tmp, node->out);
                changed = TRUE;
                Bv_Minus(node->gen, tmp);
                Bv_Or(tmp, node->in);
            }
        }
    } while (changed);

    // 3. renumber the linears, reversing the list for the backward walk
    IRO_NumLinear = 0;
    rev = NULL;
    while (IRO_FirstLinear) {
        IRO_FirstLinear->index = IRO_NumLinear++;
        next = IRO_FirstLinear->next;
        IRO_FirstLinear->next = rev;
        rev = IRO_FirstLinear;
        IRO_FirstLinear = next;
    }

    for (var = IRO_FirstVar; var; var = var->next) {
        vec = (BitVector*)var->unused;
        if (!vec || vec->size * 32 < IRO_NumLinear + 1) {
            Bv_AllocVectorLocal(&vec, ((SInt32)IRO_NumLinear >> 2) + IRO_NumLinear + 1);
            var->unused = vec;
        } else {
            Bv_Clear(vec);
        }
    }

    Bv_AllocVector(&data_00608CD0, IRO_NumVars + 1);
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        Bv_Copy(node->out, data_00608CD0);
        for (nd = node->last; nd; nd = nd->next) {
            if (IRO_IsAssignment(nd) && !(nd->flags & IROLF_Used)) {
                if ((var = IRO_FindAssigned301(nd, NULL)) &&
                    var->object->type->size == nd->rtype->size) {
                    index = var->index;
                    if (!copts.deadstore || (nd->flags & IROLF_Reffed)) {
                        if (!Bv_IsBitSet(index, data_00608CD0)) {
                            // a store nothing reads
                            Bv_SetBit(nd->index, (BitVector*)var->unused);
                        }
                    }
                    Bv_ClearBit(index, data_00608CD0);
                }
            } else if (nd->type == IROLinearAsm && IRO_GetAsmEffectsHook && nd->u.asm_stmt) {
                IROAsmEffects effects = IRO_EmptyAsmEffects;
                IRO_GetAsmEffectsHook(nd->u.asm_stmt, &effects);
                index = 0;
                for (j = effects.numoperands - 1; j >= 0; j--) {
                    if ((var = IRO_FindVar(effects.operands[j].object, FALSE, TRUE))) {
                        index = var->index;
                    }
                    switch (effects.operands[j].type) {
                        case IAEffect_Read:
                            if (var) {
                                Bv_SetBit(index, data_00608CD0);
                            }
                            break;
                        case IAEffect_Write:
                            if (var && AsmWritesWhole(&effects.operands[j])) {
                                Bv_ClearBit(index, data_00608CD0);
                            }
                            break;
                        case IAEffect_ReadWrite:
                            // as in the binary: the use first, then the
                            // definition kills it
                            if (var) {
                                Bv_SetBit(index, data_00608CD0);
                                if (AsmWritesWhole(&effects.operands[j])) {
                                    Bv_ClearBit(index, data_00608CD0);
                                }
                            }
                            break;
                        default:
                            break;
                    }
                }
                if (effects.flag2) {
                    Bv_Set(data_00608CD0);
                }
                if (effects.mayThrow) {
                    for (var = IRO_FirstVar; var; var = var->next) {
                        if (CallMayRead(nd, var)) {
                            Bv_SetBit(var->index, data_00608CD0);
                        }
                    }
                }
            }
            if (nd->type == IROLinearFunccall) {
                func_00595880(nd, NULL, data_00608CD0);
            }
            func_00595E30(nd);
            for (var = IRO_FirstVar; var; var = var->next) {
                if (Bv_IsBitSet(var->index, data_00608CD0)) {
                    Bv_SetBit(nd->index, (BitVector*)var->unused);
                }
            }
            if (nd == node->first) {
                break;
            }
        }
    }

    // the list back in order
    while (rev) {
        next = rev->next;
        rev->next = IRO_FirstLinear;
        IRO_FirstLinear = rev;
        rev = next;
    }

    IRO_CheckForUserBreak();
}
