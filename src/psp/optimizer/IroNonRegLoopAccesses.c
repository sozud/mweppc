// mwccpsp 3.0.1: loads and stores in a loop through a loop-invariant
// address of a variable that is not in a register (IroNonRegLoopAccesses.c,
// 0x59C4F0..0x59CE30): when every access in the loop goes through the same
// address, the variable is kept in a temporary for the loop. Not in the
// 2.4.7 source.
#include "compiler/common.h"
#include "compiler/BitVector.h"
#include "compiler/CError.h"
#include "compiler/enode.h"
#include "compiler/objects.h"
#include "compiler/IroCSE.h"
#include "compiler/IroDump.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IrOptimizer.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroLoop301.h"
#include "compiler/IroMalloc.h"
#include "compiler/IroPropagate.h"
#include "compiler/IroUtil.h"
#include "compiler/CFunc.h"
#include "compiler/CParser.h"
#include "compiler/IroVars.h"

extern void func_0056AB70(
    IROLinear* linear, Boolean alloc, Boolean stop, Boolean recurse
); // IroCSE.c
extern IROLinear* func_00552790(Object* obj); // IroUtil.c: *obj

enum {
    ACCESS_VALID = 0,
    ACCESS_INVALID = 1,
    ACCESS_POSSIBLY_VALID = 2
};

// an object accessed in the loop (0x18 bytes)
typedef struct IROLoopAccess {
    UInt8 kind; // +0
    IROLinear* linear; // +4: the first access
    IROElmList* uses; // +8: the accesses
    IROLinear* addr; // +0xC: their address
    Object* obj; // +0x10
    struct IROLoopAccess* next; // +0x14
} IROLoopAccess;

typedef struct IROLoopAccessList {
    IROLoopAccess* head;
    IROLoopAccess* tail;
} IROLoopAccessList;

// 0x5575F0: obj is a local that may live in a register (3.0.1's
// IRO_IsRegable, IroUtil.c)
#if MWCCPS2
__declspec(noinline) // b210 calls it (func_0059CBF0)
#endif
Boolean func_005575F0(Object* obj) {
    if (obj->datatype == DLOCAL && obj->u.var.info) {
        return obj->u.var.info->noregister == 0;
    }
    return FALSE;
}

// 0x59C4F0: keeps access's variable in a new temporary for the loop: loaded
// at `at` before the loop, stored back at the loop's exit when the loop
// writes it (and it is not read-only), and every access replaced by the
// temporary.
void func_0059C4F0(IRONaturalLoop* loop, IROLoopAccess* access, IROLinear* at) {
    Object* temp;
    VarRecord* var;
    IROList list;
    IROLinear* ass;
    IROLinear* where;
    IROElmList* use;

    temp = create_temp_object(access->linear->rtype);
    IRO_FindVar(temp, TRUE, TRUE);
    var = IRO_FindVar(access->obj, FALSE, TRUE);
    if (!var) {
        CError_FATAL(568);
    }

    IRO_InitList(&list);
    ass = IRO_NewLinear(IROLinearOp2Arg);
    ass->index = ++IRO_NumLinear;
    ass->rtype = IRO_UnqualifiedType(access->linear->rtype);
    ass->nodetype = EASS;
    ass->u.diadic.left = func_00552790(temp);
    IRO_AddToList(ass->u.diadic.left->u.monadic, &list);
    ass->u.diadic.left->flags |= 0x26;
    ass->u.diadic.left->u.monadic->flags |= 0x24;
    ass->u.diadic.right = IRO_DuplicateExpr(access->linear, &list);
    IRO_AddToList(ass, &list);
    IRO_Paste(list.head, list.tail, at);

    if (Bv_IsBitSet(var->index, loop->xC) &&
        !CParser_IsReadOnly(access->obj->type, access->obj->qual)) {
        IRO_InitList(&list);
        ass = IRO_NewLinear(IROLinearOp2Arg);
        ass->index = ++IRO_NumLinear;
        ass->rtype = IRO_UnqualifiedType(access->linear->rtype);
        ass->nodetype = EASS;
        ass->u.diadic.left = IRO_DuplicateExpr(access->linear, &list);
        ass->u.diadic.left->flags |= 0x24;
        ass->u.diadic.left->u.monadic->flags |= 0x24;
        ass->u.diadic.right = func_00552790(temp);
        ass->u.diadic.right->flags |= 2;
        IRO_AddToList(ass->u.diadic.right->u.monadic, &list);
        IRO_AddToList(ass, &list);
        where = loop->exitSuccessor->first;
        if (where->type == IROLinearLabel) {
            IRO_PasteAfter(list.head, list.tail, where);
        } else {
            IRO_Paste(list.head, list.tail, where);
        }
    }

    for (use = access->uses; use; use = use->next) {
        IRO_Dump("Replacing Loop Motion Candidate Reference at Int = %d\n", use->element->index);
        IRO_InitList(&list);
        IRO_AddToList(func_00552790(temp)->u.monadic, &list);
        list.tail->flags |= 2;
        IRO_Paste(list.head, list.tail, use->element);
        IRO_LocateFather_Cut_And_Paste(use->element, list.tail);
    }
}

// 0x59CBF0: a new record for obj, accessed by nd (at addr); every: nd's node
// runs on every iteration; bad: nd may write or fault
void func_0059CBF0(
    IROLinear* nd, IROLinear* addr, Object* obj, Boolean every, Boolean bad, IROLoopAccessList* list
) {
    IROLoopAccess* access;
    IROElmList* elm;

    if (func_005575F0(obj)) {
        return;
    }
    access = IRO_Alloc(sizeof(IROLoopAccess));
    if (!list->head) {
        list->head = access;
    } else {
        list->tail->next = access;
        access = list->tail->next;
    }
    list->tail = access;
    access->kind = ACCESS_POSSIBLY_VALID;
    access->linear = nd;
    access->addr = addr;
    access->obj = obj;
    access->next = NULL;
    access->uses = NULL;
    if (obj->varptr) {
        obj->varptr->unk24 = access;
    }
    elm = IRO_Alloc(sizeof(IROElmList));
    elm->element = nd;
    elm->next = NULL;
    if (access->uses) {
        elm->next = access->uses;
        access->uses = elm;
    } else {
        access->uses = elm;
    }
    if (every) {
        access->kind = ACCESS_VALID;
    } else if (bad) {
        access->kind = ACCESS_INVALID;
    }
}

// 0x59CB00: the variables another statement reads cannot stay in temporaries
void func_0059CB00(IROLinear* nd, BitVector* bv, IROLoopAccessList* list) {
    VarRecord* var;

    if ((nd->type == IROLinearOp1Arg && nd->nodetype == EINDIRECT) ||
        nd->type == IROLinearFunccall ||
        (IRO_Hook15 && ((Boolean (*)(IROLinear*, int))IRO_Hook15)(nd, 0)) ||
        nd->type == IROLinearAsm) {
        Bv_Clear(bv);
        func_0056AB70(nd, FALSE, FALSE, FALSE);
        Bv_Copy(IRO_Depends, bv);
        if (!Bv_IsEmpty(bv)) {
            for (var = IRO_FirstVar; var; var = var->next) {
                if (Bv_IsBitSet(var->index, bv)) {
                    if (var->unk24) {
                        ((IROLoopAccess*)var->unk24)->kind = ACCESS_INVALID;
                    } else {
                        func_0059CBF0(nd, NULL, var->object, FALSE, TRUE, list);
                    }
                }
            }
        }
    }
}

// 0x59C960: an access through the invariant address of one object
void func_0059C960(IROLinear* nd, Boolean every, BitVector* bv, IROLoopAccessList* list) {
    IROAddrRecord301 rec;
    IROLinear* addr = nd->u.monadic;
    IROLoopAccess* access;
    IROElmList* elm;
    Object* obj;
    Boolean bad;

    func_005513A0(&rec, addr);
    if (addr->type == IROLinearOperand && addr->u.node->type == EOBJREF) {
        rec.numObjRefs = 1;
        func_005512E0(addr, &rec.objRefs, IRO_Alloc);
    } else if (addr->type == IROLinearOp2Arg && addr->nodetype == EADD) {
        func_005510D0(addr, &rec, IRO_Alloc);
    }
    if (rec.numObjRefs != 1) {
        func_0059CB00(nd, bv, list);
        return;
    }
    bad = TRUE;
    if (!(nd->flags & IROLF_CouldError) && (nd->flags & IROLF_Reffed)) {
        bad = FALSE;
    }
    obj = rec.objRefs->element->u.node->data.objref;
    for (access = list->head; access && access->obj != obj; access = access->next) {}
    if (!access) {
        func_0059CBF0(nd, rec.linear, obj, every, bad, list);
        return;
    }
    if (access->kind == ACCESS_INVALID) {
        return;
    }
    if (!IRO_ExprsSame(addr, access->addr)) {
        access->kind = ACCESS_INVALID;
    } else {
        for (elm = access->uses; elm && elm->element != nd; elm = elm->next) {}
        if (!elm) {
            elm = IRO_Alloc(sizeof(IROElmList));
            elm->element = nd;
            elm->next = NULL;
            if (access->uses) {
                elm->next = access->uses;
                access->uses = elm;
            } else {
                access->uses = elm;
            }
        }
    }
    if (!every && bad) {
        access->kind = ACCESS_INVALID;
        return;
    }
    if (access->kind == ACCESS_POSSIBLY_VALID && (every || bad)) {
        access->kind = ACCESS_VALID;
    }
}

// 0x59C770: the loop's accesses to objects not in registers (twice over)
void func_0059C770(BitVector* nodes, IROLoopAccessList* list) {
    VarRecord* var;
    BitVector* bv;
    IRONode* node;
    IROLinear* nd;
    int pass;

    for (var = IRO_FirstVar; var; var = var->next) {
        var->unk24 = NULL;
    }
    Bv_AllocVector(&bv, IRO_NumVars + 1);
    Bv_AllocVector(&IRO_Depends, IRO_NumVars + 1);
    for (pass = 0; pass < 2; pass++) {
        for (node = IRO_FirstNode; node; node = node->nextnode) {
            if (Bv_IsBitSet(node->index, nodes)) {
                for (nd = node->first; nd && nd != node->last->next; nd = nd->next) {
                    if (nd->type == IROLinearOp1Arg &&
                        nd->nodetype == EINDIRECT &&
                        (nd->u.monadic->flags & IROLF_LoopInvariant) &&
                        !IRO_HasSideEffect(nd)) {
                        func_0059C960(nd, node->unk3F, bv, list);
                    } else {
                        func_0059CB00(nd, bv, list);
                    }
                }
            }
        }
    }
    if (IRO_Depends) {
        IRO_free(IRO_Depends);
        IRO_Depends = NULL;
    }
    if (bv) {
        IRO_free(bv);
        bv = NULL;
    }
}

// 0x59CCD0
void func_0059CCD0(IRONaturalLoop* loop, Boolean* changed) {
    IROLinear* at;
    IROLoopAccessList list;
    IROLoopAccess* access;
    Boolean ok;

    if (!func_0058A4F0(loop)) {
        IRO_Dump("No predecessor outside the loop\n");
        return;
    }
    func_0058A430(loop, loop->header, loop->x18, &at, changed);
    ok = loop->preheader != NULL;
    if (ok && loop->numExits == 1) {
        if (loop->exitSuccessor->numpred != 1) {
            loop->exitSuccessor = func_0058A5D0(loop);
            if (!loop->exitSuccessor) {
                ok = FALSE;
            }
        }
        if (ok) {
            list.head = NULL;
            list.tail = NULL;
            func_0059C770(loop->nodes, &list);
            for (access = list.head; access; access = access->next) {
                if (access->kind == ACCESS_POSSIBLY_VALID) {
                    access->kind = ACCESS_INVALID;
                }
                IRO_Dump("Loop Motion Candidate:: Int = %d", access->linear->index);
                switch (access->kind) {
                    case ACCESS_VALID:
                        IRO_Dump("<valid>");
                        break;
                    case ACCESS_INVALID:
                        IRO_Dump("<invalid>");
                        break;
                    case ACCESS_POSSIBLY_VALID:
                        IRO_Dump("<possibly_valid>");
                        break;
                    default:
                        CError_FATAL(186);
                        break;
                }
                IRO_Dump("\n");
                if (access->kind == ACCESS_VALID) {
                    func_0059C4F0(loop, access, at);
                }
            }
        }
    }
    IRO_UpdateFlagsOnInts(IRO_FirstLinear);
}
