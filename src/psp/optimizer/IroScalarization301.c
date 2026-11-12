// mwccpsp 3.0.1: IroScalarization.c (0x57FAA0..0x5813B0, by its asserts)
// and IRO_ScalarizeClassDataMembers (0x57FA80). 2.4.7 kept this in IroVars.c.
//
// With #pragma opt_scalarizeliveranges (on with loop invariants), 3.0.1 first
// builds the per-bit definitions and uses (func_00578130), splits each
// variable's definitions and uses into webs (the definitions reaching a use,
// and the uses a definition reaches) and scalarizes the members accessed in
// each web on its own. The use-def records it builds replace every
// VarRecord's defs/uses lists (func_00579920 clears them first) and are not
// freed here. If the records cannot be built (too many), or the pragma is
// off, the 2.4.7 whole-variable pass runs (func_005804A0).
#include "compiler/common.h"
#include "compiler/BitVector.h"
#include "compiler/CError.h"
#include "compiler/CExpr.h"
#include "compiler/CFunc.h"
#include "compiler/CInt64.h"
#include "compiler/CParser.h"
#include "compiler/enode.h"
#include "compiler/Exceptions.h"
#include "compiler/IrOptimizer.h"
#include "compiler/IroDump.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroMalloc.h"
#include "compiler/IroPointerAnalysis.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"
#include "compiler/IROUseDef301.h"
#include "compiler/objects.h"
#include "compiler/types.h"

extern Boolean is_volatile_object(Object* obj);
extern Boolean IRO_FunctionCallMightThrowException(IROLinear* linear);
extern void func_0054FC80(ExceptionAction* action, void (*func)(Object*, void*), void* arg);
extern IROLinear* func_00558810(
    IROLinear* nd, Boolean* isBitfield
); // IROUseDef301.c: the assigned operand
extern Boolean func_00578130(void); // IROUseDef301.c: the per-bit definitions and uses
extern void RewriteIndDec(void); // IroVars.c (0x581470)

// 3.0.1's indirect records: 2.4.7's, naturally aligned, without the
// replacement (0x28 bytes, IRO_Alloc)
typedef struct IndirectRecordMatch301 {
    IROLinear* nd; // +0
    struct IndirectRecordMatch301* otherMatches; // +4
} IndirectRecordMatch301;

typedef struct IndirectRecord301 {
    IROLinear* linear; // +0
    UInt8 flags; // +4
    VarRecord* var; // +8
    SInt32 addend; // +0xC
    SInt32 size; // +0x10
    UInt32 startbit; // +0x14
    UInt32 endbit; // +0x18
    Type* type; // +0x1C
    struct IndirectRecord301* next; // +0x20
    IndirectRecordMatch301* matches; // +0x24
} IndirectRecord301;

enum {
    IRF301_Disabled = 1,
    IRF301_Bitfield = 2
};

Boolean data_00608C78; // the web has only definite, linear definitions and exact uses
IROUse301* data_00608C7C; // the web's uses (through x2C)
IRODef301* data_00608C80; // the web's definitions (through x30)
BitVector* data_00608C84; // the uses of no variable
BitVector* data_00608C88; // the uses not yet in a web
BitVector* data_00608C8C; // the variable's definitions not yet in a web
IndirectRecord301* data_00608C90; // the last record
IndirectRecord301* data_00608C94; // the records

#define FirstRecord data_00608C94
#define LastRecord data_00608C90

// 3.0.1 chains the web through def->x30 and use->x2C (32-bit fields no
// other pass reads); a table by record index keeps the same order and holds
// 64-bit pointers
static IRODef301** WebDefNext;
static IROUse301** WebUseNext;
#define DEF_WEBNEXT(def) WebDefNext[(def)->index]
#define USE_WEBNEXT(use) WebUseNext[(use)->index]

#define VAR_DEFS(var) (*(IRODef301**)&(var)->defs)
#define VAR_USES(var) (*(IROUse301**)&(var)->uses)

Boolean func_0057FAA0(void);
void func_005801A0(IRODef301* def, int depth);
void func_005802F0(IROUse301* use, int depth);
void func_0057FD30(VarRecord* webvar);
void func_005804A0(void);
Boolean func_005809C0(IROLinear* indirect);
Boolean func_00580B20(IROLinear* funccall);
void func_00580E40(IROLinear* nd, Boolean* resultFlag);
void func_00581040(IROLinear* nd, VarRecord* var, SInt32 addend, Type* type);
Boolean func_005810B0(
    IROLinear* nd, VarRecord* var, SInt32 addend, Type* type, UInt32 start_bit, UInt32 end_bit
);

// 0x57FA80
#if !MWCCPS2 // b210: IroVars.c has the dispatcher, the classic pass and the address checks
void IRO_ScalarizeClassDataMembers(void) {
    Boolean done = FALSE;

    if (copts.opt_scalarizeliveranges) {
        done = func_0057FAA0();
    }
    if (!done) {
        func_005804A0();
    }
}
#endif

// 0x57FAA0: scalarize per web; FALSE when the use-def records are not built
Boolean func_0057FAA0(void) {
    IROUse301* use;
    IRODef301* def;
    IRODef301* other;
    VarRecord* var;
    SInt32 numVars;

    RewriteIndDec();
    if (!func_00578130()) {
        return FALSE;
    }

    WebDefNext = IRO_malloc((data_00624664 + 1) * sizeof(IRODef301*));
    WebUseNext = IRO_malloc((data_006242F4 + 1) * sizeof(IROUse301*));
    Bv_AllocVector(&data_00608C8C, data_00624664);
    Bv_AllocVector(&data_00608C88, data_006242F4);
    Bv_AllocVector(&data_00608C84, data_006242F4);
    for (use = data_006242B8; use; use = use->globalnext) {
        if (!use->var) {
            Bv_SetBit(use->index, data_00608C84);
        }
    }

    // the temps made on the way come after the old variables
    var = IRO_FirstVar;
    numVars = IRO_NumVars;
    while (var && var->index <= numVars) {
        if (!is_volatile_object(var->object) && !var->usedAtCall) {
            Bv_Clear(data_00608C8C);
            for (def = VAR_DEFS(var); def; def = def->varnext) {
                Bv_SetBit(def->index, data_00608C8C);
            }
            for (use = VAR_USES(var); use; use = use->varnext) {
                Bv_SetBit(use->index, data_00608C88);
            }
            for (;;) {
                Bv_Or(data_00608C84, data_00608C88);
                data_00608C80 = NULL;
                data_00608C7C = NULL;
                data_00608C78 = TRUE;
                for (def = VAR_DEFS(var); def; def = def->varnext) {
                    if (Bv_IsBitSet(def->index, data_00608C8C)) {
                        break;
                    }
                }
                if (!def) {
                    break;
                }
                func_005801A0(def, 0);
                if (def->linear) {
                    for (other = def->linear->x24.aggDefs; other; other = other->linearnext) {
                        if (other != def && Bv_IsBitSet(other->index, data_00608C8C)) {
                            func_005801A0(other, 0);
                        }
                    }
                }
                if (data_00608C78) {
                    func_0057FD30(var);
                }
            }
        }
        var = var->next;
    }

    IRO_free(WebDefNext);
    WebDefNext = NULL;
    IRO_free(WebUseNext);
    WebUseNext = NULL;
    IRO_CheckForUserBreak();
    return TRUE;
}

// the copy of the variable's member into the temp at the function's start:
// *&temp = *addr (list ends with addr)
static inline void InitTempAtEntry301(Object* obj, Type* type, IROList* list) {
    IROLinear* tempRef;
    IROLinear* addr;
    IROLinear* tempInd;
    IROLinear* valueInd;
    IROLinear* assign;

    tempRef = IRO_NewLinear(IROLinearOperand);
    tempRef->nodetype = EOBJREF;
    tempRef->u.node = create_objectrefnode(obj);
    tempRef->rtype = IRO_UnqualifiedType(tempRef->u.node->rtype);
    tempRef->index = ++IRO_NumLinear;
    tempRef->flags |= IROLF_Ind | IROLF_Assigned;

    addr = list->tail;

    tempInd = IRO_NewLinear(IROLinearOp1Arg);
    tempInd->nodetype = EINDIRECT;
    tempInd->rtype = IRO_UnqualifiedType(type);
    tempInd->u.monadic = tempRef;
    tempInd->index = ++IRO_NumLinear;
    tempInd->flags |= IROLF_Assigned;

    valueInd = IRO_NewLinear(IROLinearOp1Arg);
    valueInd->nodetype = EINDIRECT;
    valueInd->rtype = IRO_UnqualifiedType(type);
    valueInd->u.monadic = addr;
    valueInd->index = ++IRO_NumLinear;
    valueInd->flags |= IROLF_Reffed;

    assign = IRO_NewLinear(IROLinearOp2Arg);
    assign->nodetype = EASS;
    assign->u.diadic.left = tempInd;
    assign->u.diadic.right = valueInd;
    assign->rtype = IRO_UnqualifiedType(type);
    assign->index = ++IRO_NumLinear;

    addr->next = valueInd;
    valueInd->next = tempRef;
    tempRef->next = tempInd;
    tempInd->next = assign;

    IRO_PasteAfter(list->head, assign, IRO_FirstLinear);
}

// one access of the record now goes to the temp
static inline void ReplaceMatch301(
    IndirectRecord301* rec,
    IndirectRecordMatch301* match,
    Object* obj,
    VarRecord* var,
    IROList* list2
) {
    IROLinear* father;

    if (match->nd->type == IROLinearOperand && match->nd->u.node->type == EOBJREF) {
        father = IRO_LocateFather(match->nd);
        if (rec->flags & IRF301_Bitfield) {
            father->flags |= IROLF_BitfieldIndirect;
            var->bitfieldType = (TypeBitfield*)IRO_UnqualifiedType(match->nd->rtype);
        }
        var->bitFieldReplacement = list2->tail;
        match->nd->u.node = create_objectrefnode(obj);
    } else {
        father = IRO_LocateFather(match->nd);
        if (rec->flags & IRF301_Bitfield) {
            father->flags |= IROLF_BitfieldIndirect;
            var->bitfieldType = (TypeBitfield*)IRO_UnqualifiedType(match->nd->rtype);
        }
        var->bitFieldReplacement = list2->tail;
        IRO_NopOut(father->u.monadic);
        father->u.monadic = IRO_NewLinear(IROLinearOperand);
        father->u.monadic->nodetype = EOBJREF;
        father->u.monadic->u.node = create_objectrefnode(obj);
        father->u.monadic->rtype = IRO_UnqualifiedType(match->nd->rtype);
        father->u.monadic->index = ++IRO_NumLinear;
        father->u.monadic->flags |= IROLF_Ind | IROLF_Reffed;
        IRO_NopOut(match->nd);
        IRO_PasteAfter(father->u.monadic, father->u.monadic, match->nd);
    }
}

// 0x57FD30: scalarize the accesses of the web (data_00608C80, data_00608C7C)
void func_0057FD30(VarRecord* webvar) {
    IRODef301* def;
    IROUse301* use;
    IROLinear* nd;
    IROLinear* ind;
    IROLinear* father;
    IndirectRecord301* rec;
    IndirectRecordMatch301* match;
    Object* obj;
    VarRecord* var;
    IROList list1;
    IROList list2;
    Boolean entryDef;

    FirstRecord = NULL;
    LastRecord = NULL;
    entryDef = FALSE;

    for (def = data_00608C80; def; def = DEF_WEBNEXT(def)) {
        nd = def->linear;
        if (!nd) {
            entryDef = TRUE;
            continue;
        }
        ind = nd->u.diadic.left;
        if (!(ind->u.monadic->flags & IROLF_Unk200000)) {
            ind->u.monadic->flags |= IROLF_Unk200000;
            if (IRO_IsAssignment(nd)) {
                func_005810B0(
                    ind, webvar, 0, ind->rtype, def->offset, def->offset + def->count - 1
                );
            } else {
                func_005810B0(ind, webvar, -1, ind->rtype, 0, 0);
            }
        }
    }

    for (use = data_00608C7C; use; use = USE_WEBNEXT(use)) {
        nd = use->linear;
        father = nd;
        do {
            father = IRO_LocateFather(father);
        } while (father && !(father->type == IROLinearOp1Arg && father->nodetype == EINDIRECT));
        if (!(father->u.monadic->flags & IROLF_Unk200000)) {
            father->u.monadic->flags |= IROLF_Unk200000;
            if (use->var && nd->type == IROLinearOperand) {
                func_005810B0(father, webvar, 0, father->rtype, use->x30, use->x30 + use->x34 - 1);
            } else {
                func_005810B0(father, webvar, -1, father->rtype, 0, 0);
            }
        }
    }

    for (rec = FirstRecord; rec; rec = rec->next) {
        for (match = rec->matches; match; match = match->otherMatches) {
            match->nd->flags &= ~IROLF_Unk200000;
        }
        if (rec->flags & IRF301_Disabled) {
            continue;
        }

        obj = create_temp_object(IRO_UnqualifiedType(rec->type));
        var = IRO_FindVar(obj, TRUE, TRUE);
        IRO_Dump("Splitting range for variable: %d temp:%s\n", webvar->index, obj->name->name);

        match = rec->matches;
        IRO_InitList(&list1);
        IRO_DuplicateExpr(match->nd, &list1);
        for (; match; match = match->otherMatches) {
            IRO_InitList(&list2);
            IRO_DuplicateExpr(match->nd, &list2);
            ReplaceMatch301(rec, match, obj, var, &list2);
        }

        if (entryDef) {
            InitTempAtEntry301(obj, rec->type, &list1);
        }
    }

    FirstRecord = NULL;
    LastRecord = NULL;
    IRO_CheckForUserBreak();
}

// 0x5801A0: add the definition to the web, and the uses it reaches
void func_005801A0(IRODef301* def, int depth) {
    IROUse301* use;
    IROLinear* assigned;

    DEF_WEBNEXT(def) = data_00608C80;
    data_00608C80 = def;
    Bv_ClearBit(def->index, data_00608C8C);
    if (data_00608C78) {
        if (!def->var || (def->linear && def->linear->type == IROLinearAsm) || !def->definite) {
            data_00608C78 = FALSE;
        }
    }

    if (!def->useCount && !def->linear) {
        return;
    }

    use = data_006242B8;
    if (def->linear && IRO_IsAssignment(def->linear)) {
        assigned = func_00558810(def->linear, NULL);
    } else {
        assigned = NULL;
    }
    for (; use; use = use->globalnext) {
        if (Bv_IsBitSet(use->index, data_00608C88)) {
            if (Bv_IsBitSet(def->index, use->reachingDefs) ||
                def->linear == use->linear ||
                assigned == use->linear) {
                func_005802F0(use, depth + 1);
            }
        }
    }
}

// 0x5802F0: add the use to the web, and the definitions reaching it
void func_005802F0(IROUse301* use, int depth) {
    IRODef301* def;
    IRODef301* other;

    USE_WEBNEXT(use) = data_00608C7C;
    data_00608C7C = use;
    Bv_ClearBit(use->index, data_00608C88);
    if (data_00608C78) {
        if (!use->var || (use->linear && use->linear->type == IROLinearAsm) || !use->x38) {
            data_00608C78 = FALSE;
        }
    }

    if (!use->var) {
        return;
    }

    for (def = VAR_DEFS(use->var); def; def = def->varnext) {
        if (!Bv_IsBitSet(def->index, data_00608C8C)) {
            continue;
        }
        if (Bv_IsBitSet(def->index, use->reachingDefs) ||
            def->linear == use->linear ||
            (def->linear &&
             IRO_IsAssignment(def->linear) &&
             func_00558810(def->linear, NULL) == use->linear)) {
            func_005801A0(def, depth + 1);
            if (def->linear) {
                for (other = def->linear->x24.aggDefs; other; other = other->linearnext) {
                    if (other != def && Bv_IsBitSet(other->index, data_00608C8C)) {
                        func_005801A0(other, depth + 1);
                    }
                }
            }
        }
    }
}

#if !MWCCPS2 // b210: IroVars.c (MWCCPS2) has these
// 0x5804A0: 2.4.7's pass over whole variables
void func_005804A0(void) {
    IROLinear* nd;
    IndirectRecord301* rec;
    IndirectRecordMatch301* match;
    ObjectList* scan;
    Object* obj;
    VarRecord* var;
    IROList list1;
    IROList list2;
    Boolean unsafe;
    SInt32 i;

    nd = IRO_FirstLinear;
    FirstRecord = NULL;
    LastRecord = NULL;
    RewriteIndDec();
    unsafe = FALSE;

    for (; nd; nd = nd->next) {
        if (nd->type == IROLinearOp1Arg && nd->nodetype == EINDIRECT) {
            func_00580E40(nd, &unsafe);
        } else if (nd->type == IROLinearAsm && IRO_GetAsmEffectsHook && nd->u.asm_stmt) {
            IROAsmEffects effects = IRO_EmptyAsmEffects;
            IRO_GetAsmEffectsHook(nd->u.asm_stmt, &effects);
            for (i = 0; i < effects.numoperands; i++) {
                if ((var = IRO_FindVar(effects.operands[i].object, FALSE, TRUE))) {
                    func_00581040(nd, var, -1, &stvoid);
                } else {
                    CError_FATAL(1094);
                }
            }
            if (effects.flag2 || effects.flag1) {
                return;
            }
            if (effects.mayThrow) {
                unsafe = TRUE;
            }
        }
        if (nd->type == IROLinearFunccall && !unsafe) {
            unsafe = func_00580B20(nd);
        }
    }

    if (unsafe) {
        for (rec = FirstRecord; rec; rec = rec->next) {
            if (rec->var->noregister) {
                rec->flags |= IRF301_Disabled;
            }
        }
    }

    for (rec = FirstRecord; rec; rec = rec->next) {
        if (rec->flags & IRF301_Disabled) {
            continue;
        }
        obj = create_temp_object(IRO_UnqualifiedType(rec->type));
        var = IRO_FindVar(obj, TRUE, TRUE);
        for (match = rec->matches; match; match = match->otherMatches) {
            IRO_InitList(&list1);
            IRO_InitList(&list2);
            IRO_DuplicateExpr(match->nd, &list1);
            IRO_DuplicateExpr(match->nd, &list2);
            ReplaceMatch301(rec, match, obj, var, &list2);
        }

        for (scan = arguments; scan; scan = scan->next) {
            if (scan->object == rec->var->object) {
                break;
            }
        }
        if (scan) {
            InitTempAtEntry301(obj, rec->type, &list1);
        }
    }

    IRO_CheckForUserBreak();
}

// 0x5809C0: 2.4.7's IndirectMightUseOrKillAnyAddressedVar
Boolean func_005809C0(IROLinear* indirect) {
    IROListNode* next;
    IROLinear* nd;
    Object* obj;
    IROListNode* resultList;
    Boolean result;
    IROListNode* scan;
    Boolean foundObjRef;
    VarRecord* var;

    result = FALSE;

    if (indirect &&
        indirect->type == IROLinearOp1Arg &&
        indirect->nodetype == EINDIRECT &&
        (indirect = indirect->u.monadic) &&
        copts.opt_pointer_analysis &&
        indirect->pointsToFunction &&
        FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupLinearNodePointerExpr(FunctionName, indirect, &resultList);
        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail) {
                    result = TRUE;
                    break;
                }

                foundObjRef = FALSE;
                for (nd = scan->list.head; nd != scan->list.tail->next; nd = nd->next) {
                    if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
                        foundObjRef = TRUE;
                        obj = nd->u.node->data.objref;
                        if (!obj) {
                            CError_FATAL(979);
                        }
                        var = IRO_FindVar(obj, TRUE, TRUE);
                        if (!var) {
                            CError_FATAL(981);
                        }
                        if (var->noregister) {
                            result = TRUE;
                        }
                    }
                }

                if (!foundObjRef) {
                    result = TRUE;
                }
                if (result) {
                    break;
                }
            }

            while (resultList) {
                next = resultList->nextList;
                IRO_free2(resultList);
                resultList = next;
            }
        } else {
            result = TRUE;
        }
    } else {
        result = TRUE;
    }

    return result;
}

// 0x580E00 (inside 0x580B20's extent): an object destroyed at the throw may be addressed
#line 567 "IroScalarization.c"
static void MarkAddressedAtThrow301(Object* obj, void* arg) {
    VarRecord* var = IRO_FindVar(obj, TRUE, TRUE);
    if (!var) {
        CError_FATAL(791);
    }
    if (var->noregister) {
        *(Boolean*)arg = TRUE;
    }
}
#line 577 "IroScalarization301.c"

// the call kills or uses an addressed variable among the list's objects
static inline Boolean AnyAddressedVar301(ObjectList* list, int line) {
    ObjectList* olist;
    VarRecord* var;
    Boolean result = FALSE;

    for (olist = list; olist; olist = olist->next) {
        if (!olist->object) {
            result = TRUE;
        } else {
            var = IRO_FindVar(olist->object, TRUE, TRUE);
            if (!var) {
                CError_FATAL(line);
            }
            if (var->noregister) {
                result = TRUE;
            }
        }
        if (result) {
            break;
        }
    }
    return result;
}

static inline void FreeObjectList301(ObjectList* list) {
    ObjectList* next;

    while (list) {
        next = list->next;
        IRO_free2(list);
        list = next;
    }
}

// 0x580B20: 2.4.7's FunctionCallMightUseOrKillAnyAddressedVar, and the
// objects destroyed when the call throws
Boolean func_00580B20(IROLinear* funccall) {
    IROListNode* nextList;
    IROLinear* funcnd;
    IROLinear* nd;
    IROListNode* scan;
    IROListNode* resultList;
    ObjectList* killList;
    Boolean result;
    Boolean foundObjRef;
    Object* obj;

    result = FALSE;

    funcnd = funccall->u.funccall.callee;
    if (funcnd && copts.opt_pointer_analysis && funcnd->pointsToFunction && FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupLinearNodePointerExpr(FunctionName, funcnd, &resultList);

        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail) {
                    result = TRUE;
                    break;
                }

                foundObjRef = FALSE;
                for (nd = scan->list.head; nd != scan->list.tail->next; nd = nd->next) {
                    if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
                        foundObjRef = TRUE;
                        obj = nd->u.node->data.objref;
                        if (!obj) {
                            CError_FATAL(840);
                        }

                        killList = NULL;
                        PointerAnalysis_GetFunctionKills(obj, funccall, &killList);
                        result = AnyAddressedVar301(killList, 852);
                        FreeObjectList301(killList);
                        if (result) {
                            break;
                        }

                        killList = NULL;
                        PointerAnalysis_GetFunctionDependencies(obj, funccall, &killList);
                        result = AnyAddressedVar301(killList, 880);
                        FreeObjectList301(killList);
                        if (result) {
                            break;
                        }
                    }
                }

                if (!foundObjRef) {
                    result = TRUE;
                }
                if (result) {
                    break;
                }
            }

            while (resultList) {
                nextList = resultList->nextList;
                IRO_free2(resultList);
                resultList = nextList;
            }
        } else {
            result = TRUE;
        }
    } else {
        result = TRUE;
    }

    if (!result && funccall->stmt && IRO_FunctionCallMightThrowException(funccall)) {
        func_0054FC80(funccall->stmt->dobjstack, MarkAddressedAtThrow301, &result);
    }

    return result;
}

static inline Boolean IsAggregate301(Object* obj) {
    return IS_TYPE_CLASS(obj->type) || IS_TYPE_STRUCT(obj->type) || IS_TYPE_ARRAY(obj->type);
}

// 0x580E40: 2.4.7's CheckAddress; the address may be any sum with one object
void func_00580E40(IROLinear* nd, Boolean* resultFlag) {
    IROAddrRecord301 rec;
    IROLinear* inner;
    IROElmList* elm;
    Object* obj;
    VarRecord* var;
    CInt64 sum;
    CInt64 tmp;
    SInt32 addend;
    Boolean result;

    inner = nd->u.monadic;
    if (copts.opt_scalarizebitfields &&
        inner->type == IROLinearOp1Arg &&
        inner->nodetype == EBITFIELD) {
        inner = inner->u.monadic;
    }

    result = FALSE;

    if (inner->type == IROLinearOperand && inner->u.node->type == EOBJREF) {
        obj = inner->u.node->data.objref;
        if (IsAggregate301(obj) && obj->datatype == DLOCAL) {
            var = IRO_FindVar(obj, FALSE, TRUE);
            if (!var) {
                CError_FATAL(720);
            }
            func_00581040(nd, var, 0, nd->rtype);
            result = TRUE;
        }
    } else if (inner->type == IROLinearOp2Arg && inner->nodetype == EADD) {
        func_005513A0(&rec, inner);
        func_005510D0(inner, &rec, IRO_malloc);
        if (rec.numObjRefs == 1) {
            obj = rec.objRefs->element->u.node->data.objref;
            if (obj && IsAggregate301(obj) && obj->datatype == DLOCAL) {
                var = IRO_FindVar(obj, FALSE, TRUE);
                addend = 0;
                if (rec.numMisc) {
                    addend = -1;
                } else if (rec.numInts) {
                    sum = cint64_zero;
                    for (elm = rec.ints; elm; elm = elm->next) {
                        sum = CInt64_Add(sum, elm->element->u.node->data.intval);
                    }
                    addend = sum.lo;
                    CInt64_SetULong(&tmp, sum.lo);
                    if (!CInt64_Equal(tmp, sum)) {
                        addend = -1;
                    }
                }
                func_00581040(nd, var, addend, IRO_UnqualifiedType(nd->rtype));
                result = TRUE;
            }
        }
        func_00551310(&rec, IRO_free);
    }

    if (!result && !*resultFlag) {
        *resultFlag = func_005809C0(nd);
    }
}

// 0x581040: the bits of the access
void func_00581040(IROLinear* nd, VarRecord* var, SInt32 addend, Type* type) {
    IROLinear* inner;
    TypeBitfield* tbitfield;
    UInt32 start_bit;
    UInt32 end_bit;

    inner = (nd->type == IROLinearOp1Arg) ? nd->u.monadic : NULL;
    if (inner && inner->type == IROLinearOp1Arg && inner->nodetype == EBITFIELD) {
        tbitfield = TYPE_BITFIELD(inner->rtype);
        start_bit = addend * 8 + (signed char)tbitfield->offset;
        end_bit = start_bit + (signed char)tbitfield->bitlength - 1;
    } else {
        start_bit = addend * 8;
        end_bit = start_bit + type->size * 8 - 1;
    }
    func_005810B0(nd, var, addend, type, start_bit, end_bit);
}

// 0x5810B0: 2.4.7's CheckAddressConsistency, given the bits
Boolean func_005810B0(
    IROLinear* nd, VarRecord* var, SInt32 addend, Type* type, UInt32 start_bit, UInt32 end_bit
) {
    IndirectRecord301* rp;
    IndirectRecord301* scan;
    IndirectRecordMatch301* match;
    IROLinear* inner;
    Boolean conflict;
    ObjectList* olist;

    conflict = FALSE;
    inner = (nd->type == IROLinearOp1Arg) ? nd->u.monadic : NULL;

    if (var->noregister && !copts.opt_pointer_analysis) {
        return FALSE;
    }

    if (nd->rtype && CParser_IsVolatile(nd->rtype, nd->nodeflags & 3)) { // ENODE_FLAG_QUALS
        addend = -1;
    }
    if (is_volatile_object(var->object)) {
        addend = -1;
    }

    for (rp = FirstRecord; rp; rp = rp->next) {
        if (rp->var->index == var->index) {
            if (rp->flags & IRF301_Disabled) {
                return FALSE;
            }

            if (addend == -1) {
                conflict = TRUE;
                break;
            }

            if (IRO_TypesEqual(rp->type, type) &&
                rp->startbit == start_bit &&
                rp->endbit == end_bit) {
                match = IRO_Alloc(sizeof(IndirectRecordMatch301));
                match->nd = inner;
                match->otherMatches = NULL;
                if (rp->matches) {
                    match->otherMatches = rp->matches;
                }
                rp->matches = match;
                return TRUE;
            }

            if (rp->startbit == start_bit &&
                rp->endbit == end_bit &&
                !IRO_TypesEqual(rp->type, type)) {
                conflict = TRUE;
                break;
            }

            if (rp->startbit >= start_bit && end_bit >= rp->startbit) {
                conflict = TRUE;
                break;
            }

            if (rp->startbit < start_bit && rp->endbit >= start_bit) {
                conflict = TRUE;
                break;
            }
        }
    }

    if (conflict) {
        for (scan = FirstRecord; scan; scan = scan->next) {
            if (scan->var->index == rp->var->index) {
                scan->flags |= IRF301_Disabled;
            }
        }
        if (addend >= 0) {
            return FALSE;
        }
    }

    rp = IRO_Alloc(sizeof(IndirectRecord301));
    rp->linear = nd;
    rp->flags = 0;
    rp->var = var;
    rp->addend = addend;
    rp->size = type->size;
    rp->type = IRO_UnqualifiedType(type);
    rp->next = NULL;
    rp->matches = IRO_Alloc(sizeof(IndirectRecordMatch301));
    rp->matches->nd = inner;
    rp->matches->otherMatches = NULL;
    rp->startbit = start_bit;
    rp->endbit = end_bit;

    if (nd->type == IROLinearOp1Arg &&
        nd->nodetype == EINDIRECT &&
        nd->u.monadic->type == IROLinearOp1Arg &&
        nd->u.monadic->nodetype == EBITFIELD) {
        rp->flags |= IRF301_Bitfield;
    }

    if (!(type->type == TYPEFLOAT ||
          (type->type == TYPEINT && TYPE_INTEGRAL(type)->integral < 0x11) ||
          type->type == TYPEPOINTER)) {
        rp->flags |= IRF301_Disabled;
    } else if (addend == -1) {
        rp->flags |= IRF301_Disabled;
    } else if (inner->flags & IROLF_InCallArg) {
        for (olist = arguments; olist; olist = olist->next) {
            if (olist->object == var->object) {
                break;
            }
        }
        if (olist) {
            rp->flags |= IRF301_Disabled;
        }
    }

    if (FirstRecord) {
        LastRecord->next = rp;
    } else {
        FirstRecord = rp;
    }
    LastRecord = rp;
    return TRUE;
}
#endif
