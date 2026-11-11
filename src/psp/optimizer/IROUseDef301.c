// mwccpsp 3.0.1: definitions and uses of variables (IROUseDef.c by its
// asserts, 0x57A360..0x57B510). 3.0.1 split 2.4.7's IRO_UseDef: this pass
// only finds the definitions, the uses and which definitions reach which
// use; dead stores (IRO_DeadStore) and the propagation of self-assignments
// (IRO_PropagateSelfAssignments) are passes of their own that read the result.
// The records are 0x24 bytes from IRO_malloc with 32-bit counts, and what a
// call, a store through a pointer or a counter loop's bounds may read comes
// from the depends walker (func_0056AB70).
#include "compiler/common.h"
#include "compiler/BitVector.h"
#include "compiler/CError.h"
#include "compiler/CFunc.h"
#include "compiler/CParser.h"
#include "compiler/IroDump.h"
#include "compiler/IroEval.h"
#include "compiler/enode.h"
#include "compiler/Exceptions.h"
#include "compiler/IrOptimizer.h"
#include "compiler/IroCSE.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroMalloc.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"
#include "compiler/objects.h"
#include "compiler/types.h"

extern Boolean IRO_FunctionCallMightThrowException(IROLinear* linear);
extern VarRecord* IRO_FindAssigned301(IROLinear* linear, Boolean* isBitfield);
extern void func_0054FC80(ExceptionAction* action, void (*func)(Object*, void*), void* arg);
extern Boolean is_volatile_object(Object* obj);
extern void func_0056AB70(IROLinear* linear, Boolean alloc, Boolean stop, Boolean recurse);

#include "compiler/IROUseDef301.h"

IRODef301* data_00624C38; // the definitions
IRODef301* data_006242BC; // the last one
SInt32 data_00624664; // how many
IROUse301* data_006242B8; // the uses
IROUse301* data_00625304; // the last one
SInt32 data_006242F4; // how many
BitVector* data_00608B4C; // the definitions that reach some use
BitVector* data_00608B50; // the definitions reaching the current point

#define FirstDef data_00624C38
#define LastDef data_006242BC
#define NumDefs data_00624664
#define FirstUse data_006242B8
#define LastUse data_00625304
#define NumUses data_006242F4
#define MightReachAnyUse data_00608B4C
#define Reaches data_00608B50

// 3.0.1 allocates 0x24 bytes; x24/x28 are reached only for records later
// passes give vectors to
#if MWCCPSP_NATIVE
#define RECORD_SIZE(type) sizeof(type)
#else
#define RECORD_SIZE(type) 0x24
#endif

#define VAR_DEFS(var) (*(IRODef301**)&(var)->defs)
#define VAR_USES(var) (*(IROUse301**)&(var)->uses)

// 0x57B300: free the records of the last run
void func_0057B300(void) {
    IRODef301* def;
    IRODef301* nextdef;
    IROUse301* use;
    IROUse301* nextuse;

    for (def = FirstDef; def; def = nextdef) {
        nextdef = def->globalnext;
        if (def->hasVectors && def->x28) {
            IRO_free(def->x28);
            def->x28 = NULL;
        }
        IRO_free(def);
    }
    for (use = FirstUse; use; use = nextuse) {
        nextuse = use->globalnext;
        if (use->reachingDefs) {
            IRO_free(use->reachingDefs);
            use->reachingDefs = NULL;
        }
        if (use->hasVectors && use->x24) {
            IRO_free(use->x24);
            use->x24 = NULL;
        }
        if (use->hasVectors && use->x28) {
            IRO_free(use->x28);
            use->x28 = NULL;
        }
        IRO_free(use);
    }
    MightReachAnyUse = NULL;
    NumDefs = 0;
    NumUses = 0;
    FirstUse = NULL;
    LastUse = NULL;
    FirstDef = NULL;
    LastDef = NULL;
}

// 0x57B400
void func_0057B400(VarRecord* var, IROLinear* linear, IRONode* node) {
    IROUse301* use = IRO_malloc(RECORD_SIZE(IROUse301));
    use->index = NumUses++;
    use->node = node;
    use->linear = linear;
    use->var = var;
    use->globalnext = NULL;
    use->reachingDefs = NULL;
    use->reachingDefCount = 0;
    use->hasVectors = FALSE;
    if (FirstUse) {
        LastUse->globalnext = use;
    } else {
        FirstUse = use;
    }
    LastUse = use;
    use->varnext = VAR_USES(var);
    VAR_USES(var) = use;
}

// 0x57B470
void func_0057B470(VarRecord* var, IROLinear* linear, IRONode* node, Boolean definite) {
    IRODef301* def = IRO_malloc(RECORD_SIZE(IRODef301));
    def->index = NumDefs++;
    def->node = node;
    def->linear = linear;
    def->var = var;
    def->globalnext = NULL;
    def->useCount = 0;
    def->global = var->object->datatype == DDATA || var->object->datatype == DABSOLUTE;
    def->noregister = var->noregister;
    def->definite = definite;
    def->removable = FALSE;
    def->hasVectors = FALSE;
    if (FirstDef) {
        LastDef->globalnext = def;
    } else {
        FirstDef = def;
    }
    LastDef = def;
    def->varnext = VAR_DEFS(var);
    VAR_DEFS(var) = def;
}

// 0x57B090
void func_0057B090(void) {
    VarRecord* var;
    IROLinear* linear;
    IRONode* node;
    Object* obj;
    Boolean isBitfield;
    int i;

    func_0057B300();
    for (var = IRO_FirstVar; var; var = var->next) {
        var->defs = NULL;
        var->uses = NULL;
        var->usedAtCall = FALSE;
    }
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        for (linear = node->first; linear; linear = linear->next) {
            if (linear->type == IROLinearOperand && linear->u.node->type == EOBJREF) {
                obj = linear->u.node->data.objref;
                if ((linear->flags & IROLF_Ind) &&
                    (!(linear->flags & IROLF_Assigned) || (linear->flags & IROLF_Used))) {
                    if ((var = IRO_FindVar(obj, FALSE, TRUE))) {
                        func_0057B400(var, linear, node);
                    }
                }
            }
            if (IRO_IsAssignment(linear) && (var = IRO_FindAssigned301(linear, &isBitfield))) {
                func_0057B470(
                    var, linear, node, linear->rtype->size == var->object->type->size && !isBitfield
                );
            }
            if (linear->type == IROLinearAsm && IRO_GetAsmEffectsHook && linear->u.asm_stmt) {
                IROAsmEffects effects = IRO_EmptyAsmEffects;
                IRO_GetAsmEffectsHook(linear->u.asm_stmt, &effects);
                for (i = 0; i < effects.numoperands; i++) {
                    var = IRO_FindVar(effects.operands[i].object, FALSE, TRUE);
                    switch (effects.operands[i].type) {
                        case 0: // read
                            func_0057B400(var, linear, node);
                            break;
                        case 1: // written
                            func_0057B470(
                                var,
                                linear,
                                node,
                                effects.operands[i].offset == 0 &&
                                    effects.operands[i].size == var->object->type->size
                            );
                            break;
                        case 2: // clobbered
                            func_0057B470(var, linear, node, FALSE);
                            break;
                        case 4: // read and written
                            func_0057B400(var, linear, node);
                            func_0057B470(
                                var,
                                linear,
                                node,
                                effects.operands[i].offset == 0 &&
                                    effects.operands[i].size == var->object->type->size
                            );
                            break;
                    }
                }
            }
            if (linear == node->last) {
                break;
            }
        }
    }
    for (var = IRO_FirstVar; var; var = var->next) {
        func_0057B470(var, NULL, NULL, TRUE);
    }
}

// 0x57AFD0: an exception action's object is used at the call
static void MarkUsedAtCall301(Object* obj, void* arg) {
    VarRecord* var;

    if ((var = IRO_FindVar(obj, FALSE, TRUE))) {
        var->usedAtCall = TRUE;
    }
}

// 0x57AFF0: an exception action's object is used by the asm statement
static void MarkUses301(Object* obj, void* arg) {
    VarRecord* var;
    IRODef301* def;

    if ((var = IRO_FindVar(obj, FALSE, TRUE))) {
        for (def = VAR_DEFS(var); def; def = def->varnext) {
            if (Bv_IsBitSet(def->index, Reaches)) {
                Bv_SetBit(def->index, MightReachAnyUse);
                def->useCount++;
            }
        }
        var->usedAtCall = TRUE;
    }
}

// the definitions reaching here of the variables in IRO_Depends reach a use
static inline void MarkDependsReached(void) {
    IRODef301* def;

    for (def = FirstDef; def; def = def->globalnext) {
        if (def->var &&
            def->var->index &&
            Bv_IsBitSet(def->var->index, IRO_Depends) &&
            Bv_IsBitSet(def->index, Reaches)) {
            Bv_SetBit(def->index, MightReachAnyUse);
            def->useCount++;
        }
    }
}

// 0x57A360
void IRO_UseDef301(void) {
    IRODef301* def;
    IRODef301* d;
    IRODef301* gdef;
    IROUse301* guse;
    IROUse301* use;
    IRONode* node;
    IROLinear* nd;
    VarRecord* var;
    IROCounterLoopInfo* loop;
    BitVector* tmp;
    BitVector* work;
    BitVector* globals;
    Boolean changed;
    UInt16 i;

    func_0057B090();
    IRO_CheckForUserBreak();

    for (use = FirstUse; use; use = use->globalnext) {
        Bv_AllocVector(&use->reachingDefs, NumDefs);
    }
    Bv_AllocVector(&MightReachAnyUse, NumDefs);
    IRO_CheckForUserBreak();

    gdef = FirstDef;
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        Bv_AllocVector(&node->in, NumDefs);
        Bv_AllocVector(&node->gen, NumDefs);
        Bv_AllocVector(&node->kill, NumDefs);
        Bv_AllocVector(&node->out, NumDefs);
        for (nd = node->first; nd; nd = nd->next) {
            while (gdef && gdef->linear == nd) {
                Bv_SetBit(gdef->index, node->gen);
                if (gdef->definite) {
                    for (d = VAR_DEFS(gdef->var); d; d = d->varnext) {
                        if (d != gdef) {
                            Bv_SetBit(d->index, node->kill);
                            Bv_ClearBit(d->index, node->gen);
                        }
                    }
                }
                gdef = gdef->globalnext;
            }
            if (nd == node->last) {
                break;
            }
        }
        if (node->numpred) {
            Bv_Set(node->in);
        }
        Bv_Copy(node->gen, node->out);
        IRO_CheckForUserBreak();
    }

    Bv_AllocVector(&work, NumDefs);
    do {
        changed = FALSE;
        for (node = IRO_FirstNode; node; node = node->nextnode) {
            Bv_Clear(node->in);
            if (node->numpred) {
                for (i = 0; i < node->numpred; i++) {
                    Bv_Or(IRO_NodeTable[node->pred[i]]->out, node->in);
                }
            } else if (node == IRO_FirstNode) {
                for (var = IRO_FirstVar; var; var = var->next) {
                    Bv_SetBit(VAR_DEFS(var)->index, node->in);
                }
            }
            Bv_Copy(node->in, work);
            Bv_Minus(node->kill, work);
            Bv_Or(node->gen, work);
            if (!Bv_Compare(work, node->out)) {
                Bv_Copy(work, node->out);
                changed = TRUE;
            }
        }
        IRO_CheckForUserBreak();
    } while (changed);
    if (work) {
        IRO_free(work);
        work = NULL;
    }

    Bv_AllocVector(&globals, NumDefs);
    for (d = FirstDef; d; d = d->globalnext) {
        if (d->global || d->noregister) {
            Bv_SetBit(d->index, globals);
        }
    }
    Bv_AllocVector(&tmp, NumDefs);
    Bv_AllocVector(&IRO_Depends, IRO_NumVars + 1);
    gdef = FirstDef;
    guse = FirstUse;
    Bv_AllocVector(&Reaches, NumDefs);

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        Bv_Copy(node->in, Reaches);
        for (nd = node->first;; nd = nd->next) {
            while (guse && guse->linear == nd) {
                for (def = VAR_DEFS(guse->var); def; def = def->varnext) {
                    if (Bv_IsBitSet(def->index, Reaches)) {
                        Bv_SetBit(def->index, guse->reachingDefs);
                        Bv_SetBit(def->index, MightReachAnyUse);
                        def->useCount++;
                        guse->reachingDefCount++;
                    }
                }
                guse = guse->globalnext;
            }

            if (nd->type == IROLinearFunccall ||
                (nd->type == IROLinearOp1Arg && nd->nodetype == EINDIRECT && !IRO_IsVariable(nd))) {
                func_0056AB70(nd, FALSE, FALSE, FALSE);
                for (def = FirstDef; def; def = def->globalnext) {
                    if (def->var &&
                        def->var->index &&
                        Bv_IsBitSet(def->var->index, IRO_Depends) &&
                        Bv_IsBitSet(def->index, Reaches)) {
                        def->useCount++;
                        Bv_SetBit(def->index, MightReachAnyUse);
                    }
                }
                if (nd->type == IROLinearFunccall &&
                    nd->stmt &&
                    IRO_FunctionCallMightThrowException(nd)) {
                    func_0054FC80(nd->stmt->dobjstack, MarkUsedAtCall301, NULL);
                }
            }
            if (nd->type == IROLinearAsm && IRO_GetAsmEffectsHook && nd->u.asm_stmt) {
                IROAsmEffects effects = IRO_EmptyAsmEffects;
                IRO_GetAsmEffectsHook(nd->u.asm_stmt, &effects);
                if (effects.flag2 || effects.flag1) {
                    Bv_Or(Reaches, MightReachAnyUse);
                    for (def = FirstDef; def; def = def->globalnext) {
                        if (Bv_IsBitSet(def->index, Reaches)) {
                            def->useCount++;
                        }
                    }
                }
                if (effects.mayThrow) {
                    Bv_Copy(Reaches, tmp);
                    Bv_And(globals, tmp);
                    Bv_Or(tmp, MightReachAnyUse);
                    for (def = FirstDef; def; def = def->globalnext) {
                        if (Bv_IsBitSet(def->index, globals) && Bv_IsBitSet(def->index, Reaches)) {
                            def->useCount++;
                        }
                    }
                    if (nd->stmt) {
                        func_0054FC80(nd->stmt->dobjstack, MarkUses301, NULL);
                    }
                }
            } else if (nd->type == IROLinearReturn || nd->type == IROLinearEnd) {
                for (def = FirstDef; def; def = def->globalnext) {
                    if (def->global && Bv_IsBitSet(def->index, Reaches)) {
                        Bv_SetBit(def->index, MightReachAnyUse);
                        def->useCount++;
                    }
                }
            }
            if (nd->type != IROLinearNop &&
                (nd->flags & IROLF_CounterLoop) &&
                (loop = nd->u.label.loop)) {
                if (loop->xC) {
                    func_0056AB70(loop->xC, FALSE, FALSE, TRUE);
                    MarkDependsReached();
                }
                if (loop->x1C) {
                    func_0056AB70(loop->x1C, FALSE, FALSE, TRUE);
                    MarkDependsReached();
                }
            }

            while (gdef && gdef->linear == nd) {
                if (gdef->definite) {
                    for (d = VAR_DEFS(gdef->var); d; d = d->varnext) {
                        Bv_ClearBit(d->index, Reaches);
                    }
                }
                Bv_SetBit(gdef->index, Reaches);
                gdef = gdef->globalnext;
            }

            if (nd == node->last) {
                break;
            }
        }
    }

    if (Reaches) {
        IRO_free(Reaches);
        Reaches = NULL;
    }
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        if (node->out) {
            IRO_free(node->out);
            node->out = NULL;
        }
        node->out = NULL;
        if (node->kill) {
            IRO_free(node->kill);
            node->kill = NULL;
        }
        node->kill = NULL;
        if (node->gen) {
            IRO_free(node->gen);
            node->gen = NULL;
        }
        node->gen = NULL;
        if (node->in) {
            IRO_free(node->in);
            node->in = NULL;
        }
        node->in = NULL;
    }
    if (IRO_Depends) {
        IRO_free(IRO_Depends);
        IRO_Depends = NULL;
    }
    if (tmp) {
        IRO_free(tmp);
        tmp = NULL;
    }
    if (globals) {
        IRO_free(globals);
        globals = NULL;
    }
    IRO_CheckForUserBreak();
}

// --- dead stores (3.0.1 IRO_DeadStore, 0x577C90..0x578130) ----------------

extern Boolean IRO_TransformSelfAssignmentToAssignment(IROLinear* nd); // 0x54FD60
extern Boolean IRO_IsModifyOp[];

// 0x577EF0: the use is inside an assignment to the same variable whose sides
// have no side effects
static Boolean UseOfVarIsInsideASimpleDefOfVar301(IROUse301* use) {
    VarRecord* var;
    VarRecord* varAss;
    IROLinear* nd;
    IROLinear* rhs;

    if ((var = use->var)) {
        if ((nd = use->linear) && (nd->flags & IROLF_InCallArg)) {
            return FALSE;
        }
        while (nd && (nd->flags & IROLF_Reffed)) {
            nd = nd->next;
        }
        if (nd &&
            IRO_IsAssignment(nd) &&
            (varAss = IRO_FindAssigned301(nd, NULL)) &&
            varAss == var &&
            !IRO_HasSideEffect(nd->u.diadic.left) &&
            (!(rhs = (nd->type == IROLinearOp1Arg) ? NULL : nd->u.diadic.right) ||
             !IRO_HasSideEffect(rhs))) {
            return TRUE;
        }
    }
    return FALSE;
}

// 0x577E40
static Boolean AllLoopDefUsesAreInSimpleLoopDefs301(IRODef301* def) {
    VarRecord* var;
    IRODef301* scan;
    IROUse301* use;

    if (!(var = def->var)) {
        return FALSE;
    }
    for (scan = VAR_DEFS(var); scan; scan = scan->varnext) {
        if (scan->node && scan->node->loopdepth) {
            for (use = VAR_USES(var); use; use = use->varnext) {
                if (Bv_IsBitSet(scan->index, use->reachingDefs)) {
                    if (!use->node ||
                        !use->node->loopdepth ||
                        !UseOfVarIsInsideASimpleDefOfVar301(use)) {
                        return FALSE;
                    }
                }
            }
        }
    }
    return TRUE;
}

// 0x577DC0: a local's definitions in loops whose every use is in such a
// definition go together
static void MarkRemovableLoopDefs301(void) {
    IRODef301* def;
    IRODef301* scan;

    for (def = FirstDef; def; def = def->globalnext) {
        if (!def->global &&
            !def->noregister &&
            def->var &&
            !def->var->usedAtCall &&
            def->node &&
            def->node->loopdepth &&
            !def->removable &&
            AllLoopDefUsesAreInSimpleLoopDefs301(def)) {
            for (scan = VAR_DEFS(def->var); scan; scan = scan->varnext) {
                if (scan->node && scan->node->loopdepth) {
                    scan->removable = TRUE;
                }
            }
        }
    }
}

// 0x577F90: a dead assignment whose value is used is replaced by its value
static Boolean EliminateReffedDeadStore301(IRODef301* def) {
    IROLinear* nd;
    Object* obj;
    VarRecord* var;
    Boolean result;
    Boolean isAssignment;
    Boolean isPostIncOrDec;

    nd = def->linear;
    result = IRO_LocateFather(nd) != NULL;
    if (result) {
        isAssignment = IRO_IsAssignment(nd);
        if (isAssignment == TRUE &&
            (obj = IRO_IsVariable(nd->u.diadic.left)) &&
            (nd->u.diadic.left->flags & IROLF_BitfieldIndirect)) {
            if (nd->nodetype == EASS && IRO_IsIntConstant(nd->u.diadic.right)) {
                var = IRO_FindVar(obj, FALSE, TRUE);
                IRO_TruncateBitfieldValueToType(
                    &nd->u.diadic.right->u.node->data.intval, nd->rtype, var->bitfieldType
                );
            } else {
                result = FALSE;
            }
        }
    }
    isPostIncOrDec =
        nd->type == IROLinearOp1Arg && (nd->nodetype == EPOSTINC || nd->nodetype == EPOSTDEC);
    if (result && isAssignment && IRO_IsModifyOp[nd->nodetype]) {
        result = IRO_TransformSelfAssignmentToAssignment(nd);
    }
    result = result && nd->type == IROLinearOp2Arg && nd->nodetype == EASS;
    if (result) {
        def->useCount = 0;
        IRO_Dump("Removing referenced dead assignment %d\n", nd->index);
        if (isPostIncOrDec) {
            IRO_NopNonSideEffects(nd->u.diadic.right, 0);
            nd->type = IROLinearNop;
            IRO_LocateFather_Cut_And_Paste_Without_Nopping(nd, nd->u.diadic.left);
        } else {
            IRO_NopNonSideEffects(nd->u.diadic.left, 0);
            nd->type = IROLinearNop;
            IRO_LocateFather_Cut_And_Paste_Without_Nopping(nd, nd->u.diadic.right);
        }
    }
    return result;
}

// 0x577C90
Boolean IRO_DeadStore(void) {
    IRODef301* def;
    Boolean result = FALSE;

    if (copts.deadstore) {
        MarkRemovableLoopDefs301();
        for (def = FirstDef; def; def = def->globalnext) {
            if (def->linear &&
                (!Bv_IsBitSet(def->index, MightReachAnyUse) || def->removable) &&
                (copts.opt_pointer_analysis || !def->noregister) &&
                def->linear->type != IROLinearAsm &&
                (!def->linear->rtype ||
                 !CParser_IsVolatile(
                     def->linear->rtype, def->linear->nodeflags & ENODE_FLAG_QUALS
                 )) &&
                !is_volatile_object(def->var->object)) {
                if (!(def->linear->flags & IROLF_Reffed)) {
                    def->useCount = 0;
                    IRO_NopNonSideEffects(def->linear, -1);
                    def->linear->type = IROLinearNop;
                    IRO_Dump("Removing dead assignment %d\n", def->linear->index);
                    result = TRUE;
                } else {
                    result = EliminateReffedDeadStore301(def);
                }
            }
        }
    }
    IRO_CheckForUserBreak();
    return result;
}

// --- self-assignment propagation (3.0.1 IRO_PropagateSelfAssignments) ----

extern CInt64 IRO_GetSelfAssignmentVal(IROLinear* linear);
extern void UpdateUse(IROLinear* linear, CInt64 val, Type* type); // 0x567C50
extern void UpdateUseForOtherSelfAssignment(IROLinear* a, IROLinear* b, Type* type); // 0x566AC0

static inline Boolean IsIncOrDec301(IROLinear* nd) {
    switch (nd->type) {
        case IROLinearOp1Arg:
            return nd->nodetype <= EPREDEC && !(nd->u.monadic->flags & IROLF_BitfieldIndirect);
        case IROLinearOp2Arg:
            return (nd->nodetype == EADDASS || nd->nodetype == ESUBASS) &&
                IRO_IsIntConstant(nd->u.diadic.right) &&
                !(nd->u.diadic.left->flags & IROLF_BitfieldIndirect);
    }
    return FALSE;
}

// *= and /= only with #pragma opt_propagatemultiplydivideassignments
static inline Boolean IsOtherSelfAssignment301(IROLinear* nd) {
    return nd->type == IROLinearOp2Arg &&
        IRO_IsIntConstant(nd->u.diadic.right) &&
        !(nd->u.diadic.left->flags & IROLF_BitfieldIndirect) &&
        ((nd->nodetype >= ESHLASS && nd->nodetype <= EORASS) ||
         (copts.opt_propagatemultiplydivideassignments &&
          (nd->nodetype == EMULASS || nd->nodetype == EDIVASS)));
}

static inline Boolean IsIntEnumOrPointer(Type* type) {
    return (type->type == TYPEINT && TYPE_INTEGRAL(type)->integral < IT_17) ||
        type->type == TYPEENUM ||
        type->type == TYPEPOINTER;
}

// the uses of the result after a definition's propagation reach what the
// variable's value before it reached
static inline void TakeOverReachingDefs(IRODef301* def, IROUse301* use) {
    IROUse301* use2;

    for (use2 = VAR_USES(def->var); use2; use2 = use2->varnext) {
        if (use2->linear == def->linear->u.diadic.left->u.monadic) {
            use->reachingDefCount = use2->reachingDefCount;
            Bv_Copy(use2->reachingDefs, use->reachingDefs);
            break;
        }
    }
}

static inline void RemoveDef(IRODef301* def) {
    def->useCount = 0;
    IRO_NopNonSideEffects(def->linear, -1);
    def->linear->type = IROLinearNop;
    IRO_Dump("Removing deadddd assignment %d\n", def->linear->index);
}

// 0x566760: x++ (or x += c) whose every use reads only this definition is
// folded into those uses
static Boolean PropagateIncsAndDecs301(void) {
    IRODef301* def;
    IROUse301* use;
    IROLinear* father;
    CInt64 val;
    SInt32 i;
    Boolean result = FALSE;

    for (def = FirstDef; def; def = def->globalnext) {
        if (!(def->linear &&
              def->definite &&
              IsIncOrDec301(def->linear) &&
              !(def->linear->flags & IROLF_Reffed) &&
              IsIntEnumOrPointer(def->linear->rtype) &&
              IRO_TypesEqual(def->linear->rtype, def->var->object->type) &&
              !is_volatile_object(def->var->object))) {
            continue;
        }
        for (use = VAR_USES(def->var), i = 0; use; use = use->varnext) {
            if (Bv_IsBitSet(def->index, use->reachingDefs)) {
                if (use->reachingDefCount == 1 &&
                    use->linear->type == IROLinearOperand &&
                    (father = IRO_LocateFather(use->linear)) &&
                    IRO_IsVariable(father) &&
                    IRO_TypesEqual(def->linear->rtype, father->rtype)) {
                    if (use->linear->flags & IROLF_Assigned) {
                        if (!((father = IRO_LocateFather(father)) &&
                              IsIncOrDec301(father) &&
                              !(father->flags & IROLF_Reffed))) {
                            goto nextDef;
                        }
                    }
                    i++;
                } else {
                    goto nextDef;
                }
            }
        }
        if (i == def->useCount) {
            for (use = VAR_USES(def->var); use; use = use->varnext) {
                if (Bv_IsBitSet(def->index, use->reachingDefs)) {
                    IRO_Dump(
                        "Propagating inc/dec from %d to %d\n",
                        def->linear->index,
                        use->linear->index
                    );
                    father = IRO_LocateFather(use->linear);
                    val = IRO_GetSelfAssignmentVal(def->linear);
                    UpdateUse(father, val, IRO_UnqualifiedType(father->rtype));
                    result = TRUE;
                    TakeOverReachingDefs(def, use);
                }
            }
            RemoveDef(def);
        }
nextDef:;
    }
    return result;
}

// 0x566410: the same for x <<= c, x &= c and the like
static Boolean PropagateOtherSelfAssignments301(void) {
    IRODef301* def;
    IROUse301* use;
    IROLinear* father;
    SInt32 i;
    Boolean result = FALSE;

    for (def = FirstDef; def; def = def->globalnext) {
        if (!(def->linear &&
              def->definite &&
              IsOtherSelfAssignment301(def->linear) &&
              !(def->linear->flags & IROLF_Reffed) &&
              ((def->linear->rtype->type == TYPEINT &&
                TYPE_INTEGRAL(def->linear->rtype)->integral < IT_17) ||
               def->linear->rtype->type == TYPEENUM) &&
              IRO_TypesEqual(def->linear->rtype, def->var->object->type) &&
              !is_volatile_object(def->var->object))) {
            continue;
        }
        for (use = VAR_USES(def->var), i = 0; use; use = use->varnext) {
            if (Bv_IsBitSet(def->index, use->reachingDefs)) {
                if (use->reachingDefCount == 1 &&
                    use->linear->type == IROLinearOperand &&
                    (father = IRO_LocateFather(use->linear)) &&
                    IRO_IsVariable(father) &&
                    IRO_TypesEqual(def->linear->rtype, father->rtype)) {
                    if (use->linear->flags & IROLF_Assigned) {
                        if (!((father = IRO_LocateFather(father)) &&
                              IsOtherSelfAssignment301(father) &&
                              father->nodetype == def->linear->nodetype &&
                              !(father->flags & IROLF_Reffed))) {
                            goto nextDef;
                        }
                    }
                    i++;
                } else {
                    goto nextDef;
                }
            }
        }
        if (i == def->useCount) {
            for (use = VAR_USES(def->var); use; use = use->varnext) {
                if (Bv_IsBitSet(def->index, use->reachingDefs)) {
                    IRO_Dump(
                        "Propagating self assignment from %d to %d\n",
                        def->linear->index,
                        use->linear->index
                    );
                    father = IRO_LocateFather(use->linear);
                    UpdateUseForOtherSelfAssignment(father, def->linear, father->rtype);
                    result = TRUE;
                    TakeOverReachingDefs(def, use);
                }
            }
            RemoveDef(def);
        }
nextDef:;
    }
    return result;
}

// 0x5663E0
Boolean IRO_PropagateSelfAssignments(void) {
    Boolean result = FALSE;

    if (copts.propagation) {
        while (PropagateIncsAndDecs301() || PropagateOtherSelfAssignments301()) {
            result = TRUE;
        }
    }
    IRO_CheckForUserBreak();
    return result;
}

// --- IroLifetime.c (3.0.1): splitting a variable's independent live ranges ---

extern IROLinear* IRO_FindAssignedObjRef(IROLinear* linear, Boolean* isBitfield); // IroVars.c
extern int Bv_IsBitSet301(UInt32 bit, BitVector* bv); // IroBitVect.c

BitVector* data_00608CCC; // the definitions not yet in a range
BitVector* data_00608CC8; // the uses not yet in a range
BitVector* data_00608CC4; // the current range's definitions
BitVector* data_00608CC0; // the current range's uses

// 0x558810: the operand a definition assigns
IROLinear* func_00558810(IROLinear* nd, Boolean* isBitfield) {
    IROLinear* target;

    if (isBitfield) {
        *isBitfield = FALSE;
    }
    if (!nd) {
        return NULL;
    }
    target = nd->u.monadic;
    if (target->type == IROLinearOp1Arg && target->nodetype == EINDIRECT) {
        target = target->u.monadic;
    }
    return IRO_FindAssignedObjRef(target, isBitfield);
}

// 0x594AF0: def joins the range, with the uses it reaches and their other
// definitions (2.4.7's AddDefToRange, with AddUseToRange inlined)
void func_00594AF0(IRODef301* def, int depth) {
    IROUse301* use;
    IRODef301* other;
    IROLinear* assigned;

    Bv_SetBit(def->index, data_00608CC4);
    Bv_ClearBit(def->index, data_00608CCC);
    assigned = func_00558810(def->linear, NULL);
    for (use = (IROUse301*)def->var->uses; use; use = use->varnext) {
        if (Bv_IsBitSet(use->index, data_00608CC8) &&
            (Bv_IsBitSet(def->index, use->reachingDefs) || assigned == use->linear)) {
            Bv_SetBit(use->index, data_00608CC0);
            Bv_ClearBit(use->index, data_00608CC8);
            for (other = (IRODef301*)use->var->defs; other; other = other->varnext) {
                if (Bv_IsBitSet301(other->index, data_00608CCC) &&
                    (Bv_IsBitSet301(other->index, use->reachingDefs) ||
                     func_00558810(other->linear, NULL) == use->linear)) {
                    func_00594AF0(other, depth + 2);
                }
            }
        }
    }
}

// 0x594910: the range gets a variable of its own
void func_00594910(VarRecord* var) {
    Object* newObj;
    IRODef301* def;
    IROUse301* use;
    IROLinear* assigned;
    IROLinear* nd;

    IRO_Dump("Splitting range for variable: %d\n", var->index);
    IRO_DumpBits("Def set: ", data_00608CC4);
    IRO_DumpBits("Use set: ", data_00608CC0);
    IRO_DumpBits("All defs: ", data_00608CCC);
    IRO_DumpBits("All uses: ", data_00608CC8);
    newObj = create_temp_object(IRO_UnqualifiedType(var->object->type));
    IRO_FindVar(newObj, TRUE, TRUE);
    for (def = (IRODef301*)var->defs; def; def = def->varnext) {
        if (Bv_IsBitSet(def->index, data_00608CC4) && def->linear) {
            assigned = func_00558810(def->linear, NULL);
            if (!assigned ||
                assigned->type != IROLinearOperand ||
                assigned->u.node->type != EOBJREF ||
                assigned->u.node->data.objref != var->object) {
                CError_FATAL(1041);
            }
            assigned->u.node->data.objref = newObj;
        }
    }
    for (use = (IROUse301*)var->uses; use; use = use->varnext) {
        if (Bv_IsBitSet(use->index, data_00608CC0)) {
            nd = use->linear;
            if (nd->type != IROLinearOperand || nd->u.node->type != EOBJREF) {
                CError_FATAL(1064);
            }
            if (nd->u.node->data.objref == var->object) {
                nd->u.node->data.objref = newObj;
            } else if (nd->u.node->data.objref != newObj) {
                CError_FATAL(1076);
            }
        }
    }
}

// 0x5946E0
void IRO_SplitLifetimes(void) {
    VarRecord* var;
    SInt32 numVars;
    IRODef301* def;
    IROUse301* use;
    Boolean firstRange;

    Bv_AllocVector(&data_00608CCC, NumDefs);
    Bv_AllocVector(&data_00608CC8, NumUses);
    Bv_AllocVector(&data_00608CC4, NumDefs);
    Bv_AllocVector(&data_00608CC0, NumUses);
    var = IRO_FirstVar;
    numVars = IRO_NumVars;
    for (; var && var->index <= numVars; var = var->next) {
        if (var->object->datatype != DLOCAL ||
            is_volatile_object(var->object) ||
            var->usedAtCall ||
            var->noregister ||
            var->bitfieldType) {
            continue;
        }
        Bv_Clear(data_00608CCC);
        Bv_Clear(data_00608CC8);
        for (def = (IRODef301*)var->defs; def; def = def->varnext) {
            if (def->linear && def->linear->type == IROLinearAsm) {
                goto skipThisVar;
            }
            Bv_SetBit(def->index, data_00608CCC);
        }
        for (use = (IROUse301*)var->uses; use; use = use->varnext) {
            if (use->linear && use->linear->type == IROLinearAsm) {
                goto skipThisVar;
            }
            Bv_SetBit(use->index, data_00608CC8);
        }
        firstRange = TRUE;
        for (;;) {
            Bv_Clear(data_00608CC4);
            Bv_Clear(data_00608CC0);
            for (def = (IRODef301*)var->defs; def && !Bv_IsBitSet(def->index, data_00608CCC);
                 def = def->varnext) {}
            if (!def) {
                break;
            }
            func_00594AF0(def, 0);
            if (Bv_IsEmpty(data_00608CCC)) {
                break;
            }
            if (!firstRange) {
                func_00594910(var);
            }
            firstRange = FALSE;
        }
skipThisVar:;
    }
    IRO_CheckForUserBreak();
}

// --- the aggregate pass: a record per bit (IRO_RecomposeAssignments) -------

UInt8* data_00608B40; // per variable: a small struct the aggregate pass follows

extern VarRecord* func_00558750(IROLinear* addr); // IroUtil.c
extern VarRecord* func_00558760(IROLinear* nd); // IroUtil.c
extern VarRecord* func_00558770(IROLinear* addr, Boolean* isBitfield);
extern Boolean func_005582F0(IROLinear* nd, VarRecord* var, SInt32* offset, SInt32* count);
extern Boolean func_00558520(
    IROLinear* nd, VarRecord* var, SInt32* offset, SInt32* count, Boolean* exact
);

// the variable is one the aggregate pass follows
static inline Boolean AggVar301(VarRecord* var) {
    SInt32 size = var->object->type->size;
    return size <= 99 && size >= 0 && data_00608B40[var->index];
}

// 0x57A0E0: a use of bits offset..offset+count-1 of var (any variable when
// var is NULL); FALSE when there are too many
Boolean func_0057A0E0(
    VarRecord* var, IROLinear* linear, IRONode* node, Boolean exact, SInt32 offset, SInt32 count
) {
    IROUse301* use;

    if (var && !AggVar301(var)) {
        return TRUE;
    }
    use = IRO_malloc(sizeof(IROUse301));
    memset(use, 0, sizeof(IROUse301));
    use->index = NumUses++;
    use->node = node;
    use->linear = linear;
    use->var = var;
    use->globalnext = NULL;
    use->varnext = NULL;
    use->reachingDefs = NULL;
    use->reachingDefCount = 0;
    use->hasVectors = TRUE;
    use->x24 = NULL;
    use->x28 = NULL;
    use->x30 = offset;
    use->x34 = count;
    use->x38 = exact;
    if (FirstUse) {
        LastUse->globalnext = use;
    } else {
        FirstUse = use;
    }
    LastUse = use;
    if (var) {
        use->varnext = VAR_USES(var);
        VAR_USES(var) = use;
    } else {
        use->varnext = NULL;
    }
    return NumUses <= 1400;
}

// 0x57A1E0: definitions of bits offset..offset+count-1 of var, a record per
// bit; FALSE when there are too many
Boolean func_0057A1E0(
    VarRecord* var, IROLinear* linear, IRONode* node, Boolean definite, SInt32 offset, SInt32 count
) {
    IRODef301* def;
    SInt32 bit;
    SInt32 end;

    if (!AggVar301(var)) {
        return TRUE;
    }
    end = offset + count;
    for (bit = offset; bit < end; bit++) {
        def = IRO_malloc(sizeof(IRODef301));
        memset(def, 0, sizeof(IRODef301));
        def->index = NumDefs++;
        def->node = node;
        def->linear = linear;
        def->var = var;
        def->globalnext = NULL;
        def->varnext = NULL;
        def->useCount = 0;
        def->global = var->object->datatype == DDATA || var->object->datatype == DABSOLUTE;
        def->noregister = var->noregister;
        def->definite = definite;
        def->removable = FALSE;
        def->hasVectors = TRUE;
        def->x24 = NULL;
        def->x28 = NULL;
        def->x34 = 0;
        def->x38 = NULL;
        def->bit = bit;
        def->offset = offset;
        def->count = count;
        if (FirstDef) {
            LastDef->globalnext = def;
        } else {
            FirstDef = def;
        }
        LastDef = def;
        def->varnext = VAR_DEFS(var);
        VAR_DEFS(var) = def;
        if (linear) {
            def->linearnext = linear->x24.aggDefs;
            linear->x24.aggDefs = def;
        } else {
            def->linearnext = NULL;
        }
    }
    return NumDefs <= 0x2BC0;
}

// 0x579920: the per-bit definitions and uses (0x57B090's work for the
// aggregate pass); FALSE when there are too many
Boolean func_00579920(void) {
    VarRecord* var;
    IROLinear* nd;
    IRONode* node;
    SInt32 offset;
    SInt32 count;
    Boolean exact;
    int i;

    func_0057B300();
    for (var = IRO_FirstVar; var; var = var->next) {
        var->defs = NULL;
        var->uses = NULL;
        var->usedAtCall = FALSE;
    }
    for (nd = IRO_FirstLinear; nd; nd = nd->next) {
        nd->x24.aggDefs = NULL;
    }
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        for (nd = node->first; nd; nd = nd->next) {
            if (IRO_IsAssignment(nd)) {
                if ((var = func_00558760(nd)) && func_00558520(nd, var, &offset, &count, &exact)) {
                    if (!func_0057A1E0(var, nd, node, exact, offset, count)) {
                        return FALSE;
                    }
                }
            } else if (nd->type == IROLinearAsm && IRO_GetAsmEffectsHook && nd->u.asm_stmt) {
                IROAsmEffects effects = IRO_EmptyAsmEffects;
                IRO_GetAsmEffectsHook(nd->u.asm_stmt, &effects);
                for (i = 0; i < effects.numoperands; i++) {
                    var = IRO_FindVar(effects.operands[i].object, FALSE, TRUE);
                    switch (effects.operands[i].type) {
                        case 1: // written
                        case 4: // read and written
                            exact = effects.operands[i].offset == 0 &&
                                effects.operands[i].size == var->object->type->size;
                            if (!func_0057A1E0(
                                    var,
                                    nd,
                                    node,
                                    exact,
                                    effects.operands[i].offset * 8,
                                    effects.operands[i].size * 8
                                )) {
                                return FALSE;
                            }
                            break;
                        case 2: // clobbered
                            if (!func_0057A1E0(
                                    var,
                                    nd,
                                    node,
                                    FALSE,
                                    effects.operands[i].offset * 8,
                                    effects.operands[i].size * 8
                                )) {
                                return FALSE;
                            }
                            break;
                    }
                }
            }
            if (nd == node->last) {
                break;
            }
        }
    }
    for (var = IRO_FirstVar; var; var = var->next) {
        if (!func_0057A1E0(var, NULL, NULL, TRUE, 0, var->object->type->size * 8)) {
            return FALSE;
        }
    }
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        for (nd = node->first; nd; nd = nd->next) {
            if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
                if ((nd->flags & IROLF_Ind) &&
                    (!(nd->flags & IROLF_Assigned) || (nd->flags & IROLF_Used)) &&
                    (var = func_00558750(nd))) {
                    exact = func_005582F0(nd, var, &offset, &count);
                    if (!func_0057A0E0(var, nd, node, exact, offset, count)) {
                        return FALSE;
                    }
                }
            } else if (nd->type == IROLinearFunccall ||
                       (nd->type == IROLinearOp1Arg &&
                        nd->nodetype == EINDIRECT &&
                        !func_00558770(nd->u.monadic, NULL))) {
                if (!func_0057A0E0(NULL, nd, node, FALSE, 0, 0)) {
                    return FALSE;
                }
            } else if (nd->type == IROLinearAsm && IRO_GetAsmEffectsHook && nd->u.asm_stmt) {
                IROAsmEffects effects = IRO_EmptyAsmEffects;
                IRO_GetAsmEffectsHook(nd->u.asm_stmt, &effects);
                if (effects.flag2 || effects.flag1 || effects.mayThrow) {
                    if (!func_0057A0E0(NULL, nd, node, FALSE, 0, 0)) {
                        return FALSE;
                    }
                }
                for (i = 0; i < effects.numoperands; i++) {
                    var = IRO_FindVar(effects.operands[i].object, FALSE, TRUE);
                    switch (effects.operands[i].type) {
                        case 0: // read
                        case 4: // read and written
                            if (!func_0057A0E0(
                                    var,
                                    nd,
                                    node,
                                    FALSE,
                                    effects.operands[i].offset * 8,
                                    effects.operands[i].size * 8
                                )) {
                                return FALSE;
                            }
                            break;
                    }
                }
            } else if (nd->type == IROLinearReturn || nd->type == IROLinearEnd) {
                if (!func_0057A0E0(NULL, nd, node, FALSE, 0, 0)) {
                    return FALSE;
                }
            }
            if (nd == node->last) {
                break;
            }
        }
    }
    IRO_CheckForUserBreak();
    return TRUE;
}

// 0x579890
Boolean func_00579890(void) {
    IRODef301* def;
    IROUse301* use;
    Boolean ok;

    ok = func_00579920();
    if (ok) {
        for (def = FirstDef; def; def = def->globalnext) {
            Bv_AllocVector(&def->x28, NumDefs);
        }
        for (use = FirstUse; use; use = use->globalnext) {
            Bv_AllocVector(&use->x24, NumDefs);
            Bv_AllocVector(&use->x28, NumDefs);
            Bv_AllocVector(&use->reachingDefs, NumDefs);
        }
    } else {
        func_0057B300();
    }
    IRO_CheckForUserBreak();
    return ok;
}

// per node, for the aggregate pass: the definitions not yet used at its
// start (in), its end (out), and those it makes and uses (0x10 bytes)
typedef struct IROAggNodeSets {
    BitVector* in; // +0
    BitVector* out; // +4
    BitVector* gen; // +8
    BitVector* kill; // +0xC
} IROAggNodeSets;

IROAggNodeSets* data_00608B44;
BitVector* data_00608B48; // the definitions not yet used here
BitVector* data_00608B50; // the definitions reaching here

// the variable is accessed in part somewhere
static inline void MarkPartial301(VarRecord* var, SInt32 offset, SInt32 count) {
    if (offset != 0 || count != var->object->type->size * 8) {
        data_00608B40[var->index] = TRUE;
    }
}

// 0x579EA0: the variables accessed in part (data_00608B40)
void func_00579EA0(void) {
    VarRecord* var;
    IROLinear* nd;
    IRONode* node;
    SInt32 offset;
    SInt32 count;
    Boolean exact;
    int i;

    data_00608B40 = IRO_malloc(IRO_NumVars + 1);
    memset(data_00608B40, 0, IRO_NumVars + 1);
    for (node = IRO_FirstNode; node; node = node->nextnode) {
        for (nd = node->first; nd; nd = nd->next) {
            if (nd->type == IROLinearOperand &&
                nd->u.node->type == EOBJREF &&
                (nd->flags & IROLF_Ind) &&
                (!(nd->flags & IROLF_Assigned) || (nd->flags & IROLF_Used)) &&
                (var = func_00558750(nd))) {
                exact = func_005582F0(nd, var, &offset, &count);
                MarkPartial301(var, offset, count);
            }
            if (IRO_IsAssignment(nd)) {
                if ((var = func_00558760(nd)) && func_00558520(nd, var, &offset, &count, &exact)) {
                    MarkPartial301(var, offset, count);
                }
            } else if (nd->type == IROLinearAsm && IRO_GetAsmEffectsHook && nd->u.asm_stmt) {
                IROAsmEffects effects = IRO_EmptyAsmEffects;
                IRO_GetAsmEffectsHook(nd->u.asm_stmt, &effects);
                for (i = 0; i < effects.numoperands; i++) {
                    var = IRO_FindVar(effects.operands[i].object, FALSE, TRUE);
                    switch (effects.operands[i].type) {
                        case 0:
                        case 1:
                        case 2:
                        case 4:
                            if (effects.operands[i].offset != 0 ||
                                effects.operands[i].size != var->object->type->size) {
                                data_00608B40[var->index] = TRUE;
                            }
                            break;
                    }
                }
            }
            if (nd == node->last) {
                break;
            }
        }
    }
    IRO_CheckForUserBreak();
}

// def is used up in node: it is no longer waiting for a use
static inline void AggKill301(IRODef301* def, IRONode* node) {
    Bv_SetBit(def->index, data_00608B44[node->index].kill);
    Bv_ClearBit(def->index, data_00608B44[node->index].gen);
}

// 0x579750: an exception action's object is used by the asm statement
static void AggKillAtThrow301(Object* obj, void* arg) {
    IRONode* node = arg;
    VarRecord* var;
    IRODef301* def;

    if ((var = IRO_FindVar(obj, FALSE, TRUE))) {
        for (def = VAR_DEFS(var); def; def = def->varnext) {
            AggKill301(def, node);
        }
        var->usedAtCall = TRUE;
    }
}

// 0x5797E0: the same while walking the linears (0x578130)
static void AggUseAtThrow301(Object* obj, void* arg) {
    VarRecord* var;
    IRODef301* def;

    if ((var = IRO_FindVar(obj, FALSE, TRUE))) {
        for (def = VAR_DEFS(var); def; def = def->varnext) {
            if (Bv_IsBitSet(def->index, data_00608B50)) {
                def->useCount++;
            }
            if (Bv_IsBitSet(def->index, data_00608B48)) {
                Bv_ClearBit(def->index, data_00608B48);
                def->x34++;
                def->x38 = NULL;
            }
        }
        var->usedAtCall = TRUE;
    }
}

// 0x578D90: reaching definitions (the nodes' in/out) and definitions not
// yet used (data_00608B44) per node
void func_00578D90(void) {
    IRODef301* gdef = FirstDef;
    IROUse301* guse = FirstUse;
    IRODef301* d;
    IRONode* node;
    IRONode* pred;
    IROLinear* nd;
    BitVector* tmp;
    IROAggNodeSets* sets;
    SInt32 lo;
    SInt32 hi;
    UInt16 k;
    Boolean changed;

    for (node = IRO_FirstNode; node; node = node->nextnode) {
        Bv_AllocVector(&node->in, NumDefs);
        Bv_AllocVector(&node->gen, NumDefs);
        Bv_AllocVector(&node->kill, NumDefs);
        Bv_AllocVector(&node->out, NumDefs);
        Bv_AllocVector(&data_00608B44[node->index].in, NumDefs);
        Bv_AllocVector(&data_00608B44[node->index].gen, NumDefs);
        Bv_AllocVector(&data_00608B44[node->index].kill, NumDefs);
        Bv_AllocVector(&data_00608B44[node->index].out, NumDefs);
        for (nd = node->first; nd; nd = nd->next) {
            for (; gdef && gdef->linear == nd; gdef = gdef->globalnext) {
                Bv_SetBit(gdef->index, node->gen);
                Bv_SetBit(gdef->index, data_00608B44[node->index].gen);
                if (gdef->definite) {
                    for (d = VAR_DEFS(gdef->var); d; d = d->varnext) {
                        if (d != gdef && d->bit == gdef->bit) {
                            Bv_SetBit(d->index, node->kill);
                            Bv_SetBit(d->index, data_00608B44[node->index].kill);
                            Bv_ClearBit(d->index, node->gen);
                            Bv_ClearBit(d->index, data_00608B44[node->index].gen);
                        }
                    }
                }
            }
            for (; guse && guse->linear == nd; guse = guse->globalnext) {
                if (guse->var) {
                    lo = guse->x30;
                    hi = lo + guse->x34;
                    for (d = VAR_DEFS(guse->var); d; d = d->varnext) {
                        if (d->bit >= lo && d->bit < hi) {
                            AggKill301(d, node);
                        }
                    }
                } else if (nd->type == IROLinearFunccall ||
                           (nd->type == IROLinearOp1Arg &&
                            nd->nodetype == EINDIRECT &&
                            !func_00558770(nd->u.monadic, NULL))) {
                    func_0056AB70(nd, FALSE, FALSE, FALSE);
                    for (d = FirstDef; d; d = d->globalnext) {
                        if (d->var && d->var->index && Bv_IsBitSet(d->var->index, IRO_Depends)) {
                            AggKill301(d, node);
                        }
                    }
                } else if (nd->type == IROLinearAsm && IRO_GetAsmEffectsHook && nd->u.asm_stmt) {
                    IROAsmEffects effects = IRO_EmptyAsmEffects;
                    IRO_GetAsmEffectsHook(nd->u.asm_stmt, &effects);
                    if (effects.flag2) {
                        for (d = FirstDef; d; d = d->globalnext) {
                            AggKill301(d, node);
                        }
                    }
                    if (effects.flag1) {
                        for (d = FirstDef; d; d = d->globalnext) {
                            AggKill301(d, node);
                        }
                    }
                    if (effects.mayThrow) {
                        for (d = FirstDef; d; d = d->globalnext) {
                            if (d->global || d->noregister) {
                                AggKill301(d, node);
                            }
                        }
                        if (nd->stmt) {
                            func_0054FC80(nd->stmt->dobjstack, AggKillAtThrow301, node);
                        }
                    }
                } else if (nd->type == IROLinearReturn || nd->type == IROLinearEnd) {
                    for (d = FirstDef; d; d = d->globalnext) {
                        if (d->global) {
                            AggKill301(d, node);
                        }
                    }
                }
            }
            if (nd == node->last) {
                break;
            }
        }
        if (node->numpred) {
            Bv_Set(node->in);
            Bv_Set(data_00608B44[node->index].in);
        }
        Bv_Copy(node->gen, node->out);
        Bv_Copy(data_00608B44[node->index].gen, data_00608B44[node->index].out);
        IRO_CheckForUserBreak();
    }

    Bv_AllocVector(&tmp, NumDefs);
    do {
        changed = FALSE;
        for (node = IRO_FirstNode; node; node = node->nextnode) {
            sets = &data_00608B44[node->index];
            Bv_Clear(node->in);
            if (node->numpred) {
                Bv_Set(sets->in);
                for (k = 0; k < node->numpred; k++) {
                    pred = IRO_NodeTable[node->pred[k]];
                    Bv_Or(pred->out, node->in);
                    Bv_And(data_00608B44[node->pred[k]].out, data_00608B44[node->index].in);
                }
            } else if (node == IRO_FirstNode) {
                Bv_Clear(data_00608B44[node->index].in);
                for (d = FirstDef; d; d = d->globalnext) {
                    if (!d->linear) {
                        Bv_SetBit(d->index, node->in);
                        Bv_SetBit(d->index, data_00608B44[node->index].in);
                    }
                }
            }
            Bv_Copy(node->in, tmp);
            Bv_Minus(node->kill, tmp);
            Bv_Or(node->gen, tmp);
            if (!Bv_Compare(tmp, node->out)) {
                Bv_Copy(tmp, node->out);
                changed = TRUE;
            }
            Bv_Copy(data_00608B44[node->index].in, tmp);
            Bv_Minus(data_00608B44[node->index].kill, tmp);
            Bv_Or(data_00608B44[node->index].gen, tmp);
            if (!Bv_Compare(tmp, data_00608B44[node->index].out)) {
                Bv_Copy(tmp, data_00608B44[node->index].out);
                changed = TRUE;
            }
        }
        IRO_CheckForUserBreak();
    } while (changed);
    if (tmp) {
        IRO_free(tmp);
        tmp = NULL;
    }
}

// a reaching definition of the use
static inline void AggReach301(IRODef301* def, IROUse301* use) {
    if (Bv_IsBitSet(def->index, data_00608B50)) {
        Bv_SetBit(def->index, use->reachingDefs);
        def->useCount++;
        use->reachingDefCount++;
    }
}

// a definition the use is the first to use
static inline void AggFirstUse301(IRODef301* def, IROUse301* use) {
    if (Bv_IsBitSet(def->index, data_00608B48)) {
        Bv_ClearBit(def->index, data_00608B48);
        def->x34++;
        def->x38 = use;
    }
}

// 0x578130: the per-bit definitions and uses, which definitions reach each
// use (x24, reachingDefs) and are still unused there (x28), and the uses
// that first use each definition (x34, x38); FALSE when there are too many
Boolean func_00578130(void) {
    IRODef301* gdef;
    IROUse301* guse;
    IRODef301* d;
    IRONode* node;
    IROLinear* nd;
    SInt32 lo;
    SInt32 hi;
    Boolean ok;

    func_00579EA0();
    ok = func_00579890();
    if (ok) {
        data_00608B44 = IRO_malloc(IRO_NumNodes * sizeof(IROAggNodeSets));
        memset(data_00608B44, 0, IRO_NumNodes * sizeof(IROAggNodeSets));
        Bv_AllocVector(&IRO_Depends, IRO_NumVars + 1);
        func_00578D90();
        gdef = FirstDef;
        guse = FirstUse;
        Bv_AllocVector(&data_00608B50, NumDefs);
        Bv_AllocVector(&data_00608B48, NumDefs);
        for (node = IRO_FirstNode; node; node = node->nextnode) {
            Bv_Copy(node->in, data_00608B50);
            Bv_Copy(data_00608B44[node->index].in, data_00608B48);
            for (nd = node->first;; nd = nd->next) {
                for (; guse && guse->linear == nd; guse = guse->globalnext) {
                    Bv_Copy(data_00608B50, guse->x24);
                    Bv_Copy(data_00608B48, guse->x28);
                    if (guse->var) {
                        lo = guse->x30;
                        hi = lo + guse->x34;
                        for (d = VAR_DEFS(guse->var); d; d = d->varnext) {
                            if (Bv_IsBitSet(d->index, data_00608B50) &&
                                d->bit >= lo &&
                                d->bit < hi) {
                                Bv_SetBit(d->index, guse->reachingDefs);
                                d->useCount++;
                                guse->reachingDefCount++;
                            }
                            if (Bv_IsBitSet(d->index, data_00608B48) &&
                                d->bit >= lo &&
                                d->bit < hi) {
                                Bv_ClearBit(d->index, data_00608B48);
                                d->x34++;
                                d->x38 = guse;
                            }
                        }
                    } else if (nd->type == IROLinearFunccall ||
                               (nd->type == IROLinearOp1Arg &&
                                nd->nodetype == EINDIRECT &&
                                !func_00558770(nd->u.monadic, NULL))) {
                        func_0056AB70(nd, FALSE, FALSE, FALSE);
                        for (d = FirstDef; d; d = d->globalnext) {
                            if (d->var &&
                                d->var->index &&
                                Bv_IsBitSet(d->var->index, IRO_Depends)) {
                                AggReach301(d, guse);
                            }
                            if (d->var &&
                                d->var->index &&
                                Bv_IsBitSet(d->var->index, IRO_Depends)) {
                                AggFirstUse301(d, guse);
                            }
                        }
                        if (nd->type == IROLinearFunccall &&
                            nd->stmt &&
                            IRO_FunctionCallMightThrowException(nd)) {
                            func_0054FC80(nd->stmt->dobjstack, MarkUsedAtCall301, NULL);
                        }
                    } else if (nd->type == IROLinearAsm &&
                               IRO_GetAsmEffectsHook &&
                               nd->u.asm_stmt) {
                        IROAsmEffects effects = IRO_EmptyAsmEffects;
                        IRO_GetAsmEffectsHook(nd->u.asm_stmt, &effects);
                        if (effects.flag2) {
                            for (d = FirstDef; d; d = d->globalnext) {
                                AggReach301(d, guse);
                                AggFirstUse301(d, guse);
                            }
                        }
                        if (effects.flag1) {
                            for (d = FirstDef; d; d = d->globalnext) {
                                AggReach301(d, guse);
                                AggFirstUse301(d, guse);
                            }
                        }
                        if (effects.mayThrow) {
                            for (d = FirstDef; d; d = d->globalnext) {
                                if (d->global || d->noregister) {
                                    AggReach301(d, guse);
                                }
                                if (d->global || d->noregister) {
                                    AggFirstUse301(d, guse);
                                }
                            }
                            if (nd->stmt) {
                                func_0054FC80(nd->stmt->dobjstack, AggUseAtThrow301, NULL);
                            }
                        }
                    } else if (nd->type == IROLinearReturn || nd->type == IROLinearEnd) {
                        for (d = FirstDef; d; d = d->globalnext) {
                            if (d->global) {
                                AggReach301(d, guse);
                            }
                            if (d->global) {
                                AggFirstUse301(d, guse);
                            }
                        }
                    }
                }
                for (; gdef && gdef->linear == nd; gdef = gdef->globalnext) {
                    Bv_Copy(data_00608B50, gdef->x28);
                    if (gdef->definite) {
                        for (d = VAR_DEFS(gdef->var); d; d = d->varnext) {
                            if (d->bit == gdef->bit) {
                                Bv_ClearBit(d->index, data_00608B50);
                                Bv_ClearBit(d->index, data_00608B48);
                            }
                        }
                    }
                    Bv_SetBit(gdef->index, data_00608B50);
                    Bv_SetBit(gdef->index, data_00608B48);
                }
                if (nd == node->last) {
                    break;
                }
            }
        }
        if (data_00608B50) {
            IRO_free(data_00608B50);
            data_00608B50 = NULL;
        }
        if (data_00608B48) {
            IRO_free(data_00608B48);
            data_00608B48 = NULL;
        }
        for (node = IRO_FirstNode; node; node = node->nextnode) {
            if (node->out) {
                IRO_free(node->out);
                node->out = NULL;
            }
            node->out = NULL;
            if (node->kill) {
                IRO_free(node->kill);
                node->kill = NULL;
            }
            node->kill = NULL;
            if (node->gen) {
                IRO_free(node->gen);
                node->gen = NULL;
            }
            node->gen = NULL;
            if (node->in) {
                IRO_free(node->in);
                node->in = NULL;
            }
            node->in = NULL;
            if (data_00608B44[node->index].out) {
                IRO_free(data_00608B44[node->index].out);
                data_00608B44[node->index].out = NULL;
            }
            data_00608B44[node->index].out = NULL;
            if (data_00608B44[node->index].kill) {
                IRO_free(data_00608B44[node->index].kill);
                data_00608B44[node->index].kill = NULL;
            }
            data_00608B44[node->index].kill = NULL;
            if (data_00608B44[node->index].gen) {
                IRO_free(data_00608B44[node->index].gen);
                data_00608B44[node->index].gen = NULL;
            }
            data_00608B44[node->index].gen = NULL;
            if (data_00608B44[node->index].in) {
                IRO_free(data_00608B44[node->index].in);
                data_00608B44[node->index].in = NULL;
            }
            data_00608B44[node->index].in = NULL;
        }
        if (IRO_Depends) {
            IRO_free(IRO_Depends);
            IRO_Depends = NULL;
        }
        IRO_free(data_00608B44);
        data_00608B44 = NULL;
    }
    IRO_free(data_00608B40);
    data_00608B40 = NULL;
    IRO_CheckForUserBreak();
    return ok;
}
