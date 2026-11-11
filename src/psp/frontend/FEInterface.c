// FEInterface.c (by the unit's asserts): the PSP target's optimizer switches
// on top of the level's defaults (0x439EA0)
#include "compiler/common.h"
#include "compiler/CParser.h"
#include <string.h>

extern void IRO_UpdateOptimizerOptions(void);

void func_00439EA0(void) {
    IRO_UpdateOptimizerOptions();
    copts.opt_decomposeaggregates = copts.optimizationlevel >= 3;
    copts.opt_movepostops = TRUE;
    copts.opt_pulloutconstants = TRUE;
    copts.opt_repositioncode = FALSE;
}

// The optimizer's hook table for this target (0x439E20). Hook 1 and 10 are
// the optimizer's own defaults.
#include "compiler/IrOptimizer.h"
#include "compiler/CError.h"
#include "compiler/CFunc.h"
#include "compiler/CompilerTools.h"
#include "compiler/InlineAsmMIPS.h"
#include "compiler/objects.h"
#include "compiler/RegisterInfo.h"
#include "compiler/types.h"
#include "compiler/enode.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroUtil.h"
#include <stddef.h>

extern Boolean IRO_IsArithmeticOp[]; // IrOptimizer.c
extern Boolean func_004323D0(CInt64 value); // CodeGen.c: fits an immediate
extern Boolean func_005575F0(Object* obj); // IRO_IsRegable
extern Boolean func_00555C60(IROLinear* nd); // IroUtil.c: an int or float constant
Boolean func_004C3EC0(IROLinear* linear);
extern Boolean IRO_DefaultIsAddressMultiply(IROLinear* linear);
extern SInt32 func_0048FBE0(UInt16 op); // bytes an opcode loads or stores
extern void func_004C5940(SInt32 err, ...); // an inline assembler error
extern UInt8 data_00625FCF; // the previous asm instruction was a branch

// 0x4C5A00: is obj one of the function's arguments
Boolean func_004C5A00(Object* obj) {
    ObjectList* list;

    for (list = arguments; list; list = list->next) {
        if (list->object == obj) {
            return TRUE;
        }
    }
    return FALSE;
}

// a variable that fits a register (an operand the optimizer can follow)
#define REGISTER_TYPE(t) \
    ((t)->type == TYPEINT || \
     (t)->type == TYPEENUM || \
     ((t)->type == TYPEMEMBERPOINTER && (t)->size == 4) || \
     (t)->type == TYPEFLOAT)

// effects->operands[n] for obj
static void asm_operand(
    IROAsmEffects* effects, UInt8 type, Object* obj, SInt32 offset, SInt32 size
) {
    IROAsmOperand* o = &effects->operands[effects->numoperands];

    o->type = type;
    o->object = obj;
    o->offset = offset;
    o->size = size;
    effects->numoperands++;
}

// 0x439F30 (hook 4): what an inline asm instruction reads, writes and does
// to control flow, for the optimizer
static void func_00439F30(Statement* stmt, IROAsmEffects* effects) {
    MAsmInstruction* instr = (MAsmInstruction*)stmt->expr;
    OpcodeDef* def = &data_006115F8[instr->opcode];
    MAsmOperand* op;
    Object* obj;
    Type* type;
    IROAsmOperand* last;
    SInt32 i;

    effects->numoperands = 0;
    effects->numlabels = 0;
    effects->flag1 = 0;
    effects->flag2 = 0;
    effects->unmodeledControlFlow = 0;
    effects->mayThrow = 0;
    effects->optimizationBarrier = 0;
    effects->noFallthrough = 0;
    effects->reachBarrier = 0;
    effects->flag7 = 0;
    *(UInt8*)&effects->x8 = 0;

    if (instr->flags & 4) {
        effects->reachBarrier = 1;
    }
    if (def->flags & 0x100) {
        effects->reachBarrier = 1;
    }
    if (def->flags & 0x20) {
        if (def->flags & 2) {
            effects->flag1 = 1;
        }
        if (def->flags & 4) {
            effects->flag2 = 1;
        }
    }
    if (def->flags & 8) {
        effects->mayThrow = 1;
    }
    if (data_00625FCF == 1) {
        effects->reachBarrier = 1;
        data_00625FCF = 0;
    }
    if (instr->opcode == 0x39 ||
        instr->opcode == 0x38A ||
        instr->opcode == 0x38B ||
        (UInt32)(instr->opcode - 0x3A) <= 3) {
        effects->noFallthrough = 1;
        data_00625FCF = 1;
    }
    if (def->flags & 2) {
        effects->flag7 = 1;
    }

    for (i = 0, op = instr->ops; i < instr->nops; i++, op++) {
        switch (op->type) {
            case 0:
                if (def->flags & 2) {
                    effects->flag2 = 1;
                } else if (def->flags & 4) {
                    effects->flag1 = 1;
                }
                break;
            case 1: // a register variable
                if (!(obj = op->u.reg.object)) {
                    break;
                }
                if (op->effect & 1) {
                    type = obj->type;
                    if (REGISTER_TYPE(type)) {
                        last = effects->numoperands ? &effects->operands[effects->numoperands - 1]
                                                    : NULL;
                        if (last && last->object == obj && (last->type == 1 || last->type == 4)) {
                            last->type = 4;
                        } else {
                            asm_operand(
                                effects, 0, op->u.reg.object, 0, op->u.reg.object->type->size
                            );
                        }
                    } else {
                        effects->flag2 = 1;
                    }
                }
                if (op->effect & 2) {
                    obj = op->u.reg.object;
                    type = obj->type;
                    if (REGISTER_TYPE(type) || type->type == TYPEPOINTER) {
                        last = effects->numoperands ? &effects->operands[effects->numoperands - 1]
                                                    : NULL;
                        if (last && last->object == obj && (last->type == 0 || last->type == 4)) {
                            last->type = 4;
                        } else {
                            asm_operand(effects, 1, op->u.reg.object, 0, obj->type->size);
                        }
                    } else {
                        effects->flag1 = 1;
                    }
                }
                break;
            case 2:
            case 3: // memory
                if (!op->u.obj.obj) {
                    break;
                }
                if (def->flags & 2) {
                    asm_operand(
                        effects, 0, op->u.obj.obj, op->u.obj.offset, func_0048FBE0(instr->opcode)
                    );
                }
                if (def->flags & 4) {
                    asm_operand(
                        effects, 1, op->u.obj.obj, op->u.obj.offset, func_0048FBE0(instr->opcode)
                    );
                }
                break;
            case 4:
                effects->labels[effects->numlabels++] = op->u.lab.label;
                break;
            case 5:
            case 6:
                op->u.reg.object = NULL;
                func_004C5940(0x1E);
                break;
            default:
                effects->flag2 = 1;
                effects->flag1 = 1;
                break;
        }
        CError_ASSERT(378, (UInt32)effects->numoperands <= 16);
        CError_ASSERT(381, (UInt32)effects->numlabels <= 16);
    }
    if ((def->flags & 9) && !effects->numlabels) {
        effects->unmodeledControlFlow = 1;
    }
}

// 0x43A3C0 (hook 5): in an asm statement, use the variable expr refers to
// in place of obj (copy propagation into inline assembly)
static void func_0043A3C0(Statement* stmt, Object* obj, ENode* expr) {
    MAsmInstruction* instr = (MAsmInstruction*)stmt->expr;
    Object* repl;
    Type* type;
    MAsmOperand* op;
    SInt32 i;

    if (obj->type->type == TYPEFLOAT || expr->type != EOBJREF) {
        return;
    }
    repl = expr->data.objref;
    if (func_004C5A00(repl) || !Registers_GetVarInfo(repl)) {
        return;
    }
    if (Registers_GetVarInfo(repl) && Registers_GetVarInfo(repl)->noregister) {
        return;
    }
    if (obj->otype != repl->otype || obj->datatype != repl->datatype) {
        return;
    }
    for (i = 0, op = instr->ops; i < instr->nops; i++, op++) {
        switch (op->type) {
            case 1:
                if (op->u.reg.object != obj) {
                    break;
                }
                type = repl->type;
                if (type->type == TYPEINT ||
                    type->type == TYPEENUM ||
                    type->type == TYPEPOINTER ||
                    (type->type == TYPEMEMBERPOINTER && type->size == 4)) {
                    op->u.reg.object = repl;
                } else if (type->type == TYPEFLOAT && op->rclass == 1) {
                    op->u.reg.object = repl;
                }
                break;
            case 2:
            case 3:
                if (op->u.obj.obj == obj) {
                    op->u.obj.obj = repl;
                }
                break;
        }
    }
}

// 0x43A4E0 (hook 6): a copy of an asm statement and its instruction
static Statement* func_0043A4E0(Statement* stmt) {
    MAsmInstruction* instr;
    Statement* copy;
    SInt32 size;

    if (!stmt) {
        return NULL;
    }
    instr = (MAsmInstruction*)stmt->expr;
    copy = lalloc(sizeof(Statement));
    *copy = *stmt;
    size = offsetof(MAsmInstruction, ops) + instr->nops * sizeof(MAsmOperand);
    copy->expr = lalloc(size);
    memcpy(copy->expr, instr, size);
    return copy;
}
extern IROLinear* func_004C46C0(IROLinear* target, IROLinear* sum, Boolean* inLeft); // IroLoop.c

static inline Boolean SameType301(Type* a, Type* b) {
    return a == b || IRO_TypesEqual(a, b);
}

static inline Boolean IsAddOrSub(IROLinear* nd) {
    return nd->type == IROLinearOp2Arg && (nd->nodetype == EADD || nd->nodetype == ESUB);
}

// 0x4C4590: target = target + ... sums put target's addend first (2.4.7's
// ReorderOperandsForReductionPattern)
void func_004C4590(IROLinear* target, IROLinear* sum) {
    IROLinear* left = sum->u.diadic.left;
    IROLinear* right = sum->u.diadic.right;
    IROLinear* addend;
    IROLinear* swap;
    IROList list;
    Boolean inLeft;

    if (sum->nodetype != EADD) {
        return;
    }
    if (!(addend = func_004C46C0(target, sum, &inLeft)) || addend == left) {
        return;
    }
    if (inLeft && IsAddOrSub(left) && SameType301(addend->rtype, right->rtype)) {
        swap = left;
        sum->u.diadic.left = right;
        left = right;
        sum->u.diadic.right = swap;
        right = swap;
        inLeft = FALSE;
    }
    if (!inLeft && IsAddOrSub(right) && SameType301(addend->rtype, left->rtype)) {
        IRO_InitList(&list);
        IRO_DuplicateExpr(left, &list);
        IRO_NopOut(left);
        IRO_PasteAfter(list.head, list.tail, addend);
        if (IRO_LocateFather_Cut_And_Paste_Without_Nopping(addend, list.tail)) {
            IRO_InitList(&list);
            IRO_DuplicateExpr(addend, &list);
            IRO_NopOut(addend);
            IRO_Paste(list.head, list.tail, sum);
            sum->u.diadic.left = list.tail;
        }
    }
}

// a narrowing integer conversion of the same kind is looked through
static inline IROLinear* NarrowedValue(IROLinear* right) {
    IROLinear* inner;

    if (right->type == IROLinearOp1Arg && right->nodetype == ETYPCON) {
        inner = right->u.monadic;
        if (right->rtype->type == inner->rtype->type &&
            right->rtype->type == TYPEINT &&
            TYPE_INTEGRAL(right->rtype)->integral < IT_17 &&
            right->rtype->size < inner->rtype->size) {
            return inner;
        }
    }
    return right;
}

static inline Boolean IsInvariantIndirect(IROLinear* nd) {
    return nd->type == IROLinearOp1Arg &&
        nd->nodetype == EINDIRECT &&
        (nd->u.monadic->flags & IROLF_LoopInvariant);
}

static inline Boolean IsReductionOp(IROLinear* nd) {
    return nd->type == IROLinearOp2Arg &&
        (nd->nodetype == EADD ||
         nd->nodetype == ESUB ||
         nd->nodetype == EMUL ||
         nd->nodetype == EDIV);
}

// 0x4C43C0: an assignment that accumulates into an invariant place (*p = *p +
// x, *p += x): the back end's reduction candidate test (IRO_Hook10; 2.4.7's
// IsAssignmentReductionCandidate)
Boolean func_004C43C0(IROLinear* nd) {
    IROLinear* left;
    IROLinear* right;
    IROLinear* operand;

    if (nd->type != IROLinearOp2Arg) {
        return FALSE;
    }
    if (nd->nodetype == EASS) {
        left = nd->u.diadic.left;
        right = nd->u.diadic.right;
        if (!IsInvariantIndirect(left)) {
            return FALSE;
        }
        right = NarrowedValue(right);
        if (!IsAddOrSub(right)) {
            return FALSE;
        }
        func_004C4590(left, right);
        operand = right->u.diadic.left;
        if (operand->type == IROLinearOp1Arg && operand->nodetype == ETYPCON) {
            operand = operand->u.monadic;
        }
        return IsInvariantIndirect(operand) &&
            IRO_ExprsSame(left, operand) &&
            IsReductionOp(right->u.diadic.right);
    }
    if (nd->nodetype == EADDASS || nd->nodetype == ESUBASS) {
        right = NarrowedValue(nd->u.diadic.right);
        return IsInvariantIndirect(nd->u.diadic.left) && IsReductionOp(right);
    }
    return FALSE;
}
extern void func_00432420(void);

// a register local
static inline Boolean IsRegisterLocal(Object* obj) {
    return obj->datatype == DLOCAL && obj->u.var.info && !obj->u.var.info->noregister;
}

// 0x4C3EC0: the back end's subable expression test (IRO_Hook0)
Boolean func_004C3EC0(IROLinear* nd) {
    IROLinear* x;
    Object* obj;
    ENodeType op;

    switch (nd->type) {
        case IROLinearOperand:
            if (nd->u.node &&
                nd->u.node->type == EOBJREF &&
                IRO_Hook12 &&
                ((Boolean (*)(ENode*))IRO_Hook12)(nd->u.node)) {
                return TRUE;
            }
            if (!copts.opt_pulloutconstants) {
                return FALSE;
            }
            if (IRO_IsIntConstant(nd) && func_004323D0(nd->u.node->data.intval)) {
                return TRUE;
            }
            if (IRO_IsFloatConstant(nd)) {
                return TRUE;
            }
            return FALSE;
        case IROLinearOp1Arg:
            op = nd->nodetype;
            if (IRO_IsArithmeticOp[op]) {
                return TRUE;
            }
            if (op == EINDIRECT && !(nd->flags & IROLF_Assigned)) {
                if (nd->flags & IROLF_Ind) {
                    x = nd->u.monadic;
                    if (x->type == IROLinearOperand &&
                        x->u.node->type == EOBJREF &&
                        IsRegisterLocal(x->u.node->data.objref)) {
                        return FALSE;
                    }
                    return TRUE;
                }
                if (!IRO_IsVariable(nd)) {
                    return TRUE;
                }
                return !func_005575F0(nd->u.monadic->u.node->data.objref);
            }
            if (op == ETYPCON &&
                nd->rtype->type == TYPEINT &&
                TYPE_INTEGRAL(nd->rtype)->integral < IT_17 &&
                nd->rtype->size >= nd->u.monadic->rtype->size) {
                return TRUE;
            }
            return FALSE;
        case IROLinearOp2Arg:
            if ((nd->nodetype == EADD || nd->nodetype == ESUB) &&
                func_00555C60(nd->u.diadic.right)) {
                obj = IRO_IsVariable(nd->u.diadic.left);
                if (obj && obj->datatype == DLOCAL) {
                    if (obj->u.var.info && !obj->u.var.info->noregister) {
                        return FALSE;
                    }
                } else if (nd->u.diadic.left->type == IROLinearOperand &&
                           nd->u.diadic.left->u.node->type == EOBJREF) {
                    return FALSE;
                }
            }
            return IRO_IsArithmeticOp[nd->nodetype];
        default:
            return FALSE;
    }
}

static Boolean FE_Hook0(IROLinear* linear) {
    if (!func_004C3EC0(linear)) {
        return FALSE;
    }
    return TRUE;
}

static Boolean FE_Hook14(void) {
    return FALSE;
}

static Boolean FE_Hook12(void) {
    return FALSE;
}

static Boolean FE_Hook16(void) {
    return FALSE;
}

static Boolean FE_Hook3(void) {
    return FALSE;
}

void func_00439E20(void) {
    IROHooks hooks;

    memset(&hooks, 0, sizeof(hooks));
    hooks.hook14 = (IROHook)FE_Hook14;
    hooks.hook3 = (IROHook)FE_Hook3;
    hooks.hook12 = (IROHook)FE_Hook12;
    hooks.hook0 = (IROHook)FE_Hook0;
    hooks.isAddressMultiply = IRO_DefaultIsAddressMultiply;
    hooks.getAsmEffects = func_00439F30;
    hooks.hook5 = (IROHook)func_0043A3C0;
    hooks.hook6 = (IROHook)func_0043A4E0;
    hooks.hook10 = (IROHook)func_004C43C0;
    hooks.hook11 = (IROHook)func_00432420;
    hooks.hook16 = (IROHook)FE_Hook16;
    IRO_Setup(&hooks);
}
