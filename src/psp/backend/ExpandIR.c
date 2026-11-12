#include "compiler/common.h"
#include "compiler/CInt64.h"
#include "compiler/CMachine.h"
#include "compiler/CompilerTools.h"
#include "compiler/enode.h"
#include "compiler/types.h"
#include "compiler/CParser.h"
#include "compiler/CFunc.h"
#include "compiler/Exceptions.h"
#include "compiler/objects.h"
#include "compiler/CMangler.h"
#include "compiler/CDecl.h"
#include "compiler/CExpr.h"
#include <string.h>

// mwccpsp 3.0.1 around 0x4A25E0..0x4A4300: rewriting the IR before code
// generation (expandIR32 and its helpers).

#define IS_SINGLE(t) \
    (TYPE_INTEGRAL(t)->integral == IT_FLOAT || ((t)->type == TYPEFLOAT && (t)->size == 4))

extern Float data_005F0E58; // 0.0
extern char data_00625EA9; // cleared when the function makes a real call
extern char data_00625EAE; // the frame pointer is used
extern struct ConstObjCache* data_00624DAC;
extern SInt32 data_00624BA0; // a catch block or exception specification
extern SInt32 data_006246CC; // the function has inline asm
extern SInt32 data_00624810;
extern SInt32 data_0062525C;
extern void func_0049E0B0(ExceptionAction* actions);
extern void func_004A2810(ENode* expr, Type* target, int ignored); // expandIR on an expression
extern ENode* func_004A4260(ENode* expr); // expr != 0

// the start of a 3.0.1 inline asm statement (scan->expr of ST_ASM)
typedef struct PspInlineAsm {
    UInt16 opcode;
    UInt16 unk2;
    UInt16 flags;
} PspInlineAsm;
extern void func_0045B8C0(ENode* expr, int intrinsic); // turn the call into that intrinsic

// ++x / --x becomes x += 1 / x -= 1 (the element size for pointers).
void func_004A3AA0(ENode* expr) {
    ENode* sub;
    ENode* one;
    Type* type;

    sub = expr->data.monadic;
    one = lalloc(sizeof(ENode));
    type = expr->rtype;
    if (type->type == TYPEFLOAT) {
        one->type = EFLOATCONST;
        one->cost = 0;
        one->rtype = type;
        one->data.floatval = CMach_CalcFloatConvertFromInt(TYPE(&stsignedint), cint64_one);
    } else if (type->type == TYPEPOINTER) {
        one->type = EINTCONST;
        one->cost = 0;
        one->rtype = TYPE(&stunsignedint);
        CInt64_SetULong(&one->data.intval, TPTR_TARGET(type)->size);
    } else {
        one->type = EINTCONST;
        one->cost = 0;
        one->rtype = type;
        one->data.intval = cint64_one;
    }
    expr->type = expr->type == EPREDEC ? ESUBASS : EADDASS;
    expr->data.diadic.left = sub;
    expr->data.diadic.right = one;
}

// expr != 0, as a condition.
ENode* func_004A4260(ENode* expr) {
    ENode* cmp;
    ENode* zero;

    cmp = lalloc(sizeof(ENode));
    zero = lalloc(sizeof(ENode));
    if (expr->rtype->type == TYPEFLOAT) {
        zero->type = EFLOATCONST;
        zero->cost = 0;
        zero->rtype = IS_SINGLE(expr->rtype) ? TYPE(&stfloat) : TYPE(&stdouble);
        zero->data.floatval = data_005F0E58;
    } else {
        zero->type = EINTCONST;
        zero->cost = 0;
        zero->rtype = TYPE(&stsignedint);
        zero->data.intval = cint64_zero;
    }
    cmp->type = ENOTEQU;
    cmp->cost = expr->cost;
    cmp->rtype = TYPE(&stsignedint);
    cmp->data.diadic.left = expr;
    cmp->data.diadic.right = zero;
    return cmp;
}

// Which intrinsic a call to the named function becomes, or -1. The square
// roots and fabsf only under fastmath; the string/abs ones only with
// inline_intrinsics.
int func_004A3930(char* name) {
    if (copts.fastmath && !strcmp(name, "sqrt")) {
        return 0x10;
    }
    if (copts.fastmath && !strcmp(name, "sqrtf")) {
        return 0x10;
    }
#if !MWCCPS2 || MWCCPS2_BUILD >= 210 // b145 has no fabsf intrinsic
    if (copts.fastmath && !strcmp(name, "fabsf")) {
        return 0xC;
    }
#endif
    if (!strcmp(name, "__clear")) {
        return 0x12;
    }
    if (!copts.inline_intrinsics) {
        return -1;
    }
    if (!strcmp(name, "strcpy")) {
        return 8;
    }
    if (!strcmp(name, "memcpy")) {
        return 9;
    }
    if (!strcmp(name, "abs")) {
        return 0xA;
    }
    if (!strcmp(name, "labs")) {
        return 0xB;
    }
    return -1;
}

// memcpy(dst, src, n) inline only for objects and a constant size; as
// written it tests the second argument twice, so it is always a real call.
void func_004A3840(ENode* expr) {
    ENodeList* args = expr->data.funccall.args;

    if (ENODE_IS(args->node, EOBJREF) &&
        ENODE_IS(args->next->node, EOBJREF) &&
        ENODE_IS(args->next->node, EINTCONST)) {
        func_0045B8C0(expr, 9);
    } else {
        expr->hascall = TRUE;
        data_00625EA9 = 0;
    }
}

// strcpy(obj, "literal") becomes a copy of strlen + 1 bytes (with read-only,
// unpooled strings).
void func_004A3880(ENode* expr) {
    ENodeList* args;
    ENode* str;
    ENode* size;

    if (!copts.poolstrings &&
        copts.readonly_strings &&
        ENODE_IS((args = expr->data.funccall.args)->node, EOBJREF) &&
        ENODE_IS((str = (args = args->next)->node), ESTRINGCONST)) {
        args->next = lalloc(sizeof(ENodeList));
        memclrw(args->next, sizeof(ENodeList));
        args = args->next;
        args->node = lalloc(sizeof(ENode));
        memclrw(args->node, sizeof(ENode));
        size = args->node;
        size->type = EINTCONST;
        size->cost = 0;
        size->rtype = TYPE(&stsignedint);
        CInt64_SetLong(&size->data.intval, strlen(str->data.string.data) + 1);
        func_0045B8C0(expr, 8);
    } else {
        expr->hascall = TRUE;
        data_00625EA9 = 0;
    }
}

// struct, class or array: a bit test on the type code
static inline int returns_aggregate(Type* type) {
    UInt8 t = type->type;
    UInt8 d;
    int structlike = 0;

    d = t - TYPESTRUCT;
    if (d <= 8 && ((1 << d) & 0x103)) {
        structlike = 1;
    }
    return structlike || (t == TYPEMEMBERPOINTER && type->size == 12);
}

static inline void check_actions(Statement* stmt) {
    ExceptionAction* action;

    func_0049E0B0(stmt->dobjstack);
    for (action = stmt->dobjstack; action; action = action->prev) {
        if (action->type == EAT_CATCHBLOCK || action->type == EAT_SPECIFICATION) {
            data_00624BA0 = 1;
            data_00625EAE = 1;
            return;
        }
    }
}

// expandIR32: rewrite every statement's expression before code generation,
// noting what the function uses (asm, the frame pointer, exceptions).
void func_004A25E0(Statement* stmt) {
    Statement* scan;
    ENode* expr;

    data_00624DAC = 0;
    if (copts.mips_option01) {
        data_00625EA9 = 0;
    } else {
        data_00625EA9 = 1;
    }
    data_00625EAE = 0;

    for (scan = stmt->next; scan; scan = scan->next) {
        curstmtvalue = scan->value;
        if (scan->dobjstack) {
            check_actions(scan);
        }

        switch (scan->type) {
            case ST_ASM:
                data_006246CC = 1;
                if (((PspInlineAsm*)scan->expr)->flags & 8) {
                    data_00624810 = 1;
                }
#if !MWCCPS2 || MWCCPS2_BUILD >= 210 // b145 has no data_0062525C
                if (((PspInlineAsm*)scan->expr)->opcode == 0x3B ||
                    ((PspInlineAsm*)scan->expr)->opcode == 0x3D) {
                    data_0062525C = 1;
                }
#endif
                break;
            case ST_EXPRESSION:
                func_004A2810(scan->expr, 0, 1);
                expr = scan->expr;
                if (ENODE_IS(expr, ETYPCON) && expr->rtype->type == TYPEVOID) {
                    scan->expr = expr->data.monadic;
                }
                break;
            case ST_IFGOTO:
                if (!(scan->expr->type >= ELESS && scan->expr->type <= ENOTEQU)) {
                    scan->expr = func_004A4260(scan->expr);
                }
                func_004A2810(scan->expr, 0, 0);
                break;
            case ST_IFNGOTO:
                if (!(scan->expr->type >= ELESS && scan->expr->type <= ENOTEQU)) {
                    scan->expr = func_004A4260(scan->expr);
                }
                func_004A2810(scan->expr, 0, 0);
                break;
            case ST_RETURN:
                if ((expr = scan->expr)) {
                    func_004A2810(expr, 0, expr->rtype && returns_aggregate(expr->rtype));
                }
                break;
            case ST_SWITCH:
                func_004A2810(scan->expr, 0, 0);
                break;
        }
    }
}

extern ObjectList* data_00624D48; // locals whose value is used (not only their address)
extern char data_00625EAD; // the object reference is under an indirection
extern SInt32 data_00624DEC; // 1: the function calls setjmp
extern void func_004A3B50(ENode* expr, ENodeType op); // x op= y (also ++/--) expanded
extern void func_004A3E00(ENode* expr, ENodeType cmp); // <? and >? as a comparison
extern void func_00504D90(ENode* expr, int unk); // a string constant as an object
extern Boolean func_004A4490(Type* type, Float value); // the constant can be built without memory
extern Object* func_004A43A0(Type* type, Float* value); // the pooled object for a float constant
extern Object* func_004A4320(Object* key);
extern Type* func_00464B40(Type* type);
extern Type* func_00464BE0(Type* type);
extern int func_0045B8E0(ENode* expr); // the intrinsic the call is, or 0

#define IS_COMPARE(e) \
    ((e)->type == ELOGNOT || \
     (e)->type == ELAND || \
     (e)->type == ELOR || \
     ((e)->type >= ELESS && (e)->type <= ENOTEQU))
#define IS_SINGLE(t) \
    (TYPE_INTEGRAL(t)->integral == IT_FLOAT || ((t)->type == TYPEFLOAT && (t)->size == 4))
#define IS_DOUBLE(t) \
    (TYPE_INTEGRAL(t)->integral == IT_DOUBLE || ((t)->type == TYPEFLOAT && (t)->size == 8))
#define IS_SOFTFLOAT(t) \
    ((IS_SINGLE(t) && !(copts.fp_library & 1)) || (IS_DOUBLE(t) && !(copts.fp_library & 2)))

static inline void note_local(Object* obj) {
    ObjectList* list;

    for (list = data_00624D48; list; list = list->next) {
        if (list->object == obj) {
            return;
        }
    }
    if (obj && obj->datatype == DLOCAL) {
        list = lalloc(sizeof(ObjectList));
        memclrw(list, sizeof(ObjectList));
        list->next = data_00624D48;
        list->object = obj;
        data_00624D48 = list;
    }
}

// expandIR on an expression: fills in cost and hascall, rewrites what the
// code generator does not handle directly.
void func_004A2810(ENode* expr, Type* target, int ignored) {
    ENode* sub;
    ENode* left;
    ENode* right;
    ENode* n;
    ENodeList* list;
    Object* obj;
    Type* type;
    TypeType tt;
    double d;
    SInt32 i;
    CInt64 c;
    Boolean isint;
    int id;
    char* name;

    expr->ignored = ignored;
    switch (expr->type) {
        case EINTCONST:
            expr->hascall = 0;
            expr->cost = 1;
            break;

        case EFLOATCONST:
            if (func_004A4490(expr->rtype, expr->data.floatval)) {
                return;
            }
            d = expr->data.floatval.data.value;
            if (d < 2147483648.0 && d >= -2147483648.0 && (double)(i = (SInt32)d) == d) {
                CInt64_SetLong(&c, i);
                isint = 1;
            } else {
                isint = 0;
            }
            if (isint) {
                n = lalloc(sizeof(ENode));
                memclrw(n, sizeof(ENode));
                n->type = EINTCONST;
                n->cost = 0;
                n->rtype = TYPE(&stsignedint);
                n->data.intval = c;
                expr->type = ETYPCON;
                expr->cost = 2;
                expr->data.monadic = n;
                break;
            }
            obj = func_004A43A0(expr->rtype, &expr->data.floatval);
            n = lalloc(sizeof(ENode));
            memclrw(n, sizeof(ENode));
            n->type = EOBJREF;
            n->cost = 0;
            n->data.objref = obj;
            n->rtype = CDecl_NewPointerType(expr->rtype);
            expr->type = EINDIRECT;
            expr->data.monadic = n;
            expr->cost = 1;
            expr->hascall = 0;
            break;

        case ESTRINGCONST:
            func_00504D90(expr, 1);
            func_004A2810(expr, NULL, 0);
            break;

        case EINDIRECT:
        case EFORCELOAD:
            sub = expr->data.monadic;
            data_00625EAD = 1;
            func_004A2810(sub, NULL, 0);
            expr->cost = expr->data.monadic->cost + 1;
            expr->hascall = sub->hascall;
            break;

        case EOBJREF:
            obj = expr->data.objref;
            if (!data_00625EAD && obj->datatype == DLOCAL) {
                note_local(obj);
            }
            data_00625EAD = 0;
            if (obj->datatype != DDATA && obj->datatype != DFUNC && obj->datatype == DABSOLUTE) {
                expr->type = EINTCONST;
                CInt64_SetLong(&expr->data.intval, obj->u.address);
            }
            expr->hascall = 0;
            expr->cost = 0;
            break;

        case EMIN:
            func_004A3E00(expr, ELESS);
            func_004A2810(expr, target, ignored);
            break;
        case EMAX:
            func_004A3E00(expr, EGREATER);
            func_004A2810(expr, target, ignored);
            break;

        case ECOND:
            while (expr->data.cond.cond->type == ELOGNOT) {
                expr->data.cond.cond = expr->data.cond.cond->data.monadic;
                n = expr->data.cond.expr1;
                expr->data.cond.expr1 = expr->data.cond.expr2;
                expr->data.cond.expr2 = n;
            }
            if (!IS_COMPARE(expr->data.cond.cond)) {
                expr->data.cond.cond = func_004A4260(expr->data.cond.cond);
            }
            left = expr->data.cond.expr1;
            right = expr->data.cond.expr2;
            func_004A2810(expr->data.cond.cond, NULL, 0);
            func_004A2810(left, NULL, ignored);
            func_004A2810(right, NULL, ignored);
            expr->hascall = expr->data.cond.cond->hascall | expr->data.cond.expr1->hascall |
                expr->data.cond.expr2->hascall;
            expr->cost = expr->data.cond.cond->cost + expr->data.cond.expr1->cost +
                expr->data.cond.expr2->cost + 3;
            if (expr->rtype->type == TYPESTRUCT || expr->rtype->type == TYPEARRAY) {
                if (left->type == EINDIRECT) {
                    expr->data.cond.expr1 = left->data.monadic;
                }
                if (right->type == EINDIRECT) {
                    expr->data.cond.expr2 = right->data.monadic;
                }
            }
            break;

        case EFUNCCALL:
        case EFUNCCALLP:
            if (func_0045B8E0(expr)) {
                expr->hascall = 0;
                id = expr->data.funccall.funcref->data.objref->u.func.u.intrinsicid;
                if (id == 0) {
                    data_00625EAE = 1;
                } else if (id == 8) {
                    func_004A3880(expr);
                } else if (id == 9) {
                } else if (id == 0x11) {
                    for (list = expr->data.funccall.args; list; list = list->next) {
                        if (list->node->type == ETYPCON) {
                            list->node->rtype = TYPE(&stfloat);
                        } else {
                            list->node =
                                CExpr_AssignmentPromotion(list->node, TYPE(&stfloat), 0, 1);
                        }
                    }
                    expr->rtype = TYPE(&stfloat);
                    expr->hascall = 1;
                    data_00625EA9 = 0;
                }
            } else if (expr->data.funccall.funcref->type == EOBJREF) {
                id = func_004A3930(
                    CMangler_GetLinkName(expr->data.funccall.funcref->data.objref)->name
                );
                if (id == 8) {
                    func_004A3880(expr);
                } else if (id == 9) {
                    func_004A3840(expr);
                } else if (id == 0x10) {
                    if (target || IS_SINGLE(expr->rtype)) {
                        list = expr->data.funccall.args;
                        if (list->node->type == ETYPCON) {
                            list->node->rtype = TYPE(&stfloat);
                        } else {
                            list->node =
                                CExpr_AssignmentPromotion(list->node, TYPE(&stfloat), 0, 1);
                        }
                        expr->rtype = TYPE(&stfloat);
                        func_0045B8C0(expr, 0x10);
                        expr->hascall = 1;
                        data_00625EA9 = 0;
                    } else {
                        data_00625EA9 = 0;
                        expr->hascall = 1;
                    }
                } else if (id == -1) {
                    if (!strcmp(
                            CMangler_GetLinkName(expr->data.funccall.funcref->data.objref)->name,
                            "__throw"
                        )) {
                        data_00625EAE = 1;
                    }
                    data_00625EA9 = 0;
                    expr->hascall = 1;
                } else {
                    func_0045B8C0(expr, id);
                }
            } else {
                data_00625EA9 = 0;
                expr->hascall = 1;
            }

            for (list = expr->data.funccall.args; list; list = list->next) {
                if (list->node->type == EOBJREF && list->node->data.objref->datatype == DLOCAL) {
                    note_local(list->node->data.objref);
                }
            }
            n = expr->data.funccall.funcref;
            if (n->type == EOBJREF &&
                !strcmp(CMangler_GetLinkName(n->data.objref)->name, "__setjmp")) {
                data_00624DEC |= 1;
            }
            n = expr->data.funccall.funcref;
            if (n->type == EINDIRECT && n->rtype && n->rtype->type == TYPEFUNC) {
                *n = *n->data.monadic;
            }
            n = expr->data.funccall.funcref;
            if (n->type == EOBJREF) {
                n->hascall = 0;
                obj = expr->data.funccall.funcref->data.objref;
                if (obj->datatype == 4 && (expr->data.funccall.funcref->flags & 0x80)) {
                    obj = galloc(sizeof(Object));
                    *obj = *expr->data.funccall.funcref->data.objref;
                    obj->datatype = DFUNC;
                    expr->data.funccall.funcref->data.objref = obj;
                }
            } else {
                func_004A2810(n, NULL, 0);
            }

            expr->cost = 10;
            for (list = expr->data.funccall.args; list; list = list->next) {
                n = list->node;
                if (n->type == ETYPCON &&
                    n->data.monadic->type == EINDIRECT &&
                    n->rtype == n->data.monadic->rtype) {
                    list->node = n->data.monadic;
                }
                func_004A2810(list->node, NULL, 0);
                expr->cost += list->node->cost;
            }
            type = expr->data.funccall.functype->functype;
            if (type &&
                (type->type == TYPEARRAY ||
                 type->type == TYPESTRUCT ||
                 type->type == TYPECLASS ||
                 (type->type == TYPEMEMBERPOINTER && type->size == 12))) {
                if (CMach_GetFunctionResultClass(expr->data.funccall.functype) == 1) {
                    right = expr->data.funccall.args->node;
                    n = lalloc(sizeof(ENode));
                    *n = *expr;
                    expr->type = ECOMMA;
                    expr->data.diadic.left = n;
                    expr->data.diadic.right = right;
                }
            }
            break;

        case ECOMMA:
            func_004A2810(expr->data.diadic.left, NULL, 1);
            func_004A2810(expr->data.diadic.right, NULL, ignored);
            expr->hascall = expr->data.diadic.left->hascall | expr->data.diadic.right->hascall;
            expr->cost = expr->data.diadic.left->cost + expr->data.diadic.right->cost;
            break;

        case ELAND:
        case ELOR:
            if (!IS_COMPARE(expr->data.diadic.left)) {
                expr->data.diadic.left = func_004A4260(expr->data.diadic.left);
            }
            if (!IS_COMPARE(expr->data.diadic.right)) {
                expr->data.diadic.right = func_004A4260(expr->data.diadic.right);
            }
            func_004A2810(expr->data.diadic.left, NULL, 0);
            func_004A2810(expr->data.diadic.right, NULL, 0);
            expr->hascall = expr->data.diadic.left->hascall | expr->data.diadic.right->hascall;
            expr->cost = expr->data.diadic.left->cost + expr->data.diadic.right->cost + 1;
            break;

        case EASS:
            right = expr->data.diadic.right;
            if (right->type == ETYPCON &&
                right->data.monadic->type == EINDIRECT &&
                right->rtype->type == right->data.monadic->rtype->type &&
                right->rtype->size == right->data.monadic->rtype->size) {
                expr->data.diadic.right = right->data.monadic;
            }
            func_004A2810(expr->data.diadic.left, NULL, 0);
            func_004A2810(expr->data.diadic.right, NULL, 0);
            if (expr->data.diadic.right->type == EFUNCCALL &&
                (func_00464B40(expr->rtype) || func_00464BE0(expr->rtype))) {
                break;
            }
            expr->cost = 1;
            type = expr->rtype;
            if (type->type == TYPESTRUCT ||
                type->type == TYPEARRAY ||
                type->type == TYPECLASS ||
                (type->type == TYPEMEMBERPOINTER && type->size == 12)) {
                if (expr->data.diadic.right->type != EFUNCCALL) {
                    expr->type = EASSBLK;
                    expr->cost += 10;
                    if (expr->data.diadic.left->type == EINDIRECT) {
                        expr->data.diadic.left = expr->data.diadic.left->data.monadic;
                    }
                    if (expr->data.diadic.right->type == EINDIRECT) {
                        expr->data.diadic.right = expr->data.diadic.right->data.monadic;
                    }
                }
            }
            expr->hascall = expr->data.diadic.left->hascall | expr->data.diadic.right->hascall;
            expr->cost += expr->data.diadic.left->cost + expr->data.diadic.right->cost;
            break;

        case EDIVASS:
            func_004A3B50(expr, EDIV);
            goto assignop;
        case EMULASS:
            func_004A3B50(expr, EMUL);
            goto assignop;
        case EADDASS:
            func_004A3B50(expr, EADD);
            goto assignop;
        case ESUBASS:
            func_004A3B50(expr, ESUB);
            goto assignop;
        case EMODASS:
            func_004A3B50(expr, EMODULO);
            goto assignop;
        case ESHLASS:
            func_004A3B50(expr, ESHL);
            goto assignop;
        case ESHRASS:
            func_004A3B50(expr, ESHR);
            goto assignop;
        case EANDASS:
            func_004A3B50(expr, EAND);
            goto assignop;
        case EXORASS:
            func_004A3B50(expr, EXOR);
            goto assignop;
        case EORASS:
            func_004A3B50(expr, EOR);
assignop:
            func_004A2810(expr->data.diadic.left, NULL, 0);
            func_004A2810(expr->data.diadic.right, NULL, 0);
            expr->hascall = expr->data.diadic.left->hascall | expr->data.diadic.right->hascall;
            break;

        case EDIV:
        case EMODULO:
            func_004A2810(expr->data.diadic.left, NULL, 0);
            func_004A2810(expr->data.diadic.right, NULL, 0);
            expr->hascall = expr->data.diadic.left->hascall | expr->data.diadic.right->hascall;
            expr->cost = expr->data.diadic.left->cost + expr->data.diadic.right->cost + 2;
            break;

        case ESHL:
        case ESHR:
        case ELESS:
        case EGREATER:
        case ELESSEQU:
        case EGREATEREQU:
        case EEQU:
        case ENOTEQU:
            func_004A2810(expr->data.diadic.left, NULL, 0);
            func_004A2810(expr->data.diadic.right, NULL, 0);
            expr->hascall = expr->data.diadic.left->hascall | expr->data.diadic.right->hascall;
            expr->cost = expr->data.diadic.left->cost + expr->data.diadic.right->cost + 1;
            break;

        case ESUB:
            if (expr->data.diadic.right->type == EINTCONST) {
                expr->type = EADD;
                expr->data.diadic.right->data.intval =
                    CInt64_Neg(expr->data.diadic.right->data.intval);
            }
        case EMUL:
        case EADD:
        case EAND:
        case EXOR:
        case EOR:
            func_004A2810(expr->data.diadic.left, target, 0);
            func_004A2810(expr->data.diadic.right, target, 0);
            expr->hascall = expr->data.diadic.left->hascall | expr->data.diadic.right->hascall;
            if (expr->data.diadic.left->type == EOBJREF &&
                expr->data.diadic.right->type == EINTCONST) {
                expr->cost = expr->data.diadic.left->cost;
            } else {
                expr->cost = expr->data.diadic.left->cost + expr->data.diadic.right->cost + 1;
            }
            break;

        case ETYPCON:
            type = expr->rtype;
            tt = type->type;
            sub = expr->data.monadic;
            if ((tt == TYPEINT || tt == TYPEENUM) &&
                type->size < 4 &&
                (sub->rtype->type == TYPEINT || sub->rtype->type == TYPEENUM)) {
                func_004A2810(sub, type, 0);
            } else if (copts.fastmath &&
                       sub->type == EMONMIN &&
                       tt == TYPEFLOAT &&
                       sub->rtype->type == TYPEFLOAT &&
                       sub->data.monadic->rtype->size >= type->size) {
                func_004A2810(sub, type, tt == TYPEVOID);
            } else if (copts.fastmath && IS_SINGLE(type) && sub->rtype->type == TYPEFLOAT) {
                if (sub->type == EFUNCCALL) {
                    func_004A2810(sub, type, tt == TYPEVOID);
                } else if (sub->type == EDIV && sub->data.diadic.right->type == EFUNCCALL &&
                           func_004A3930(CMangler_GetLinkName(sub->data.diadic.right->data.funccall.funcref->data.objref)->name) ==
                               0x10
#if !MWCCPS2 || MWCCPS2_BUILD >= 210 // b145 does not test the option
                           && copts.mips_option23
#endif
                           ) {
                    // x / sqrt(y) as one reciprocal square root
                    list = lalloc(sizeof(ENodeList));
                    list->next = sub->data.diadic.right->data.funccall.args;
                    list->node = sub->data.diadic.left;
                    sub->data.diadic.right->data.funccall.args = list;
                    sub = sub->data.diadic.right;
                    func_0045B8C0(sub, 0x11);
                    expr->data.monadic = sub;
                    func_004A2810(sub, NULL, expr->rtype->type == TYPEVOID);
                } else {
                    func_004A2810(sub, NULL, expr->rtype->type == TYPEVOID);
                }
            } else {
                func_004A2810(sub, NULL, tt == TYPEVOID);
            }
            expr->hascall = sub->hascall;
            expr->cost = sub->cost + 1;
            if (sub->rtype->type == TYPEFLOAT || expr->rtype->type == TYPEFLOAT) {
                expr->hascall = 1;
                data_00625EA9 = 0;
            }
            break;

        case EPOSTINC:
            if (!expr->ignored) {
                sub = expr->data.monadic;
                func_004A2810(sub, NULL, 0);
                expr->hascall = sub->hascall;
                break;
            }
        case EPREINC:
            expr->type = EPREINC;
            func_004A3AA0(expr);
            func_004A3B50(expr, EADD);
            goto assignop;

        case EPOSTDEC:
            if (!expr->ignored) {
                sub = expr->data.monadic;
                func_004A2810(sub, NULL, 0);
                expr->hascall = sub->hascall;
                break;
            }
        case EPREDEC:
            expr->type = EPREDEC;
            func_004A3AA0(expr);
            func_004A3B50(expr, ESUB);
            goto assignop;

        case EDEFINE:
            func_004A2810(expr->data.monadic, NULL, 0);
            expr->hascall = expr->data.monadic->hascall;
            expr->cost = expr->data.monadic->cost;
            break;

        case ELOGNOT:
            if (!IS_COMPARE(expr->data.monadic)) {
                expr->data.monadic = func_004A4260(expr->data.monadic);
            }
        case EMONMIN:
        case EBINNOT:
            sub = expr->data.monadic;
            func_004A2810(sub, target, 0);
            expr->hascall = sub->hascall;
            expr->cost = expr->data.monadic->cost + 1;
            if (copts.fastmath &&
                target &&
                expr->data.monadic->type == ETYPCON &&
                expr->data.monadic->rtype->type == TYPEFLOAT &&
                expr->data.monadic->data.monadic->rtype->type == TYPEFLOAT &&
                expr->data.monadic->data.monadic->rtype->size == target->size) {
                expr->data.monadic = expr->data.monadic->data.monadic;
                expr->rtype = target;
            }
            break;

        case EBITFIELD:
            sub = expr->data.monadic;
            func_004A2810(sub, NULL, 0);
            expr->cost = expr->data.monadic->cost + 3;
            expr->hascall = sub->hascall;
            break;

        case ENULLCHECK:
            func_004A2810(expr->data.nullcheck.nullcheckexpr, NULL, 0);
            func_004A2810(expr->data.nullcheck.condexpr, NULL, 0);
            expr->hascall = expr->data.nullcheck.nullcheckexpr->hascall |
                expr->data.nullcheck.condexpr->hascall;
            expr->cost =
                expr->data.nullcheck.nullcheckexpr->cost + expr->data.nullcheck.condexpr->cost + 2;
            break;

        case EPRECOMP:
            expr->hascall = 0;
            expr->cost = 0;
            break;

        case ELABEL:
            // as written: the object made for the label is not used
            func_004A4320((Object*)expr->data.label);
            n = lalloc(sizeof(ENode));
            n->type = EOBJREF;
            n->cost = 0;
            n->rtype = NULL;
            sub = lalloc(sizeof(ENode));
            sub->type = EINDIRECT;
            sub->cost = 1;
            sub->data.monadic = n;
            sub->rtype = NULL;
            expr->type = EINDIRECT;
            expr->cost = 1;
            expr->data.monadic = sub;
            expr->hascall = 0;
            break;
    }

    type = expr->rtype;
    if (type->type == TYPEFLOAT && IS_SOFTFLOAT(type)) {
        if ((expr->hascall |= (type->type == TYPEFLOAT))) {
            data_00625EA9 = 0;
        }
    }
}

// a copy of the node (not of its operands)
ENode* func_004A4070(ENode* expr) {
    ENode* copy = lalloc(sizeof(ENode));

    memclrw(copy, sizeof(ENode));
    *copy = *expr;
    return copy;
}

// a copy of the expression tree (operators only; leaves are copied as nodes)
ENode* func_004A40C0(ENode* expr) {
    ENode* copy = func_004A4070(expr);

    switch (expr->type) {
ENODE_CASE_MONADIC:
        copy->data.monadic = func_004A40C0(expr->data.monadic);
        break;
ENODE_CASE_DIADIC_ALL:
    case EMIN:
    case EMAX:
    case EPOINTERSTAR:
    case EDOTSTAR:
        copy->data.diadic.left = func_004A40C0(expr->data.diadic.left);
        copy->data.diadic.right = func_004A40C0(expr->data.diadic.right);
        break;
    }
    return copy;
}

// floats rank by bits; integers by size, unsigned above signed
static inline int type_rank(Type* type) {
    if (type->type == TYPEFLOAT) {
        return type->size * 8;
    }
    if (is_unsigned(type)) {
        return type->size * 2 + 1;
    }
    return type->size * 2;
}

// x op= y as x = x op y; an lvalue that is not a plain object is evaluated
// once (EDEFINE / EREUSE); the operation is done in the wider type.
void func_004A3B50(ENode* expr, ENodeType op) {
    ENode* left = expr->data.diadic.left;
    ENode* target = left->data.monadic;
    ENode* right = expr->data.diadic.right;
    Type* ltype = left->rtype;
    Type* rtype = right->rtype;
    ENode* n;
    ENode* c;
    ENode* def;
    ENode* reuse;

    // as written this only runs when both types are the same, so the ranks
    // are equal and nothing is stripped
    if (ltype == rtype && right->type == ETYPCON && type_rank(rtype) > type_rank(ltype)) {
        expr->data.diadic.right = right->data.monadic;
        right = expr->data.diadic.right;
    }

    n = lalloc(sizeof(ENode));
    n->type = op;
    n->cost = 0;
    n->rtype = (op == ESHL || op == ESHR) ? ltype : rtype;
    n->data.diadic.left = expr->data.diadic.left;
    n->data.diadic.right = right;
    expr->type = EASS;
    expr->data.diadic.left = target;
    expr->data.diadic.right = n;

    if (target->type != EOBJREF) {
        def = lalloc(sizeof(ENode));
        def->type = EDEFINE;
        def->cost = 0;
        def->rtype = ltype;
        def->data.diadic.right = NULL;
        reuse = lalloc(sizeof(ENode));
        reuse->type = EREUSE;
        reuse->cost = 0;
        reuse->rtype = ltype;
        reuse->data.monadic = def;
        if (target->type != EBITFIELD) {
            def->data.monadic = expr->data.diadic.left;
            expr->data.diadic.left = def;
            n->data.diadic.left->data.monadic = reuse;
        } else {
            def->data.monadic = target->data.monadic;
            target->data.monadic = def;
            c = lalloc(sizeof(ENode));
            *c = *target;
            c->data.monadic = reuse;
            n->data.diadic.left->data.monadic = c;
        }
    }

    if (ltype != rtype && ltype->type != TYPEPOINTER && type_rank(ltype) < type_rank(rtype)) {
        c = lalloc(sizeof(ENode));
        c->type = ETYPCON;
        c->cost = 0;
        c->rtype = ltype;
        c->data.monadic = expr->data.diadic.right;
        expr->data.diadic.right = c;
        c = lalloc(sizeof(ENode));
        c->type = ETYPCON;
        c->cost = 0;
        c->rtype = rtype;
        c->data.monadic = n->data.diadic.left;
        n->data.diadic.left = c;
    }
}

// One operand of <? / >?: an operand that is not a plain object is
// evaluated once, in the comparison (EDEFINE), and reused for the result.
// *slot is the conditional's arm, *cmpslot the comparison's operand.
static inline void minmax_operand(ENode* x, ENode* inner, ENode** slot, ENode** cmpslot) {
    ENode* def;
    ENode* reuse;
    ENode* t;
    ENode* p;

    def = lalloc(sizeof(ENode));
    memclrw(def, sizeof(ENode));
    def->type = EDEFINE;
    def->rtype = x->rtype;
    reuse = lalloc(sizeof(ENode));
    memclrw(reuse, sizeof(ENode));
    reuse->type = EREUSE;
    reuse->rtype = x->rtype;
    reuse->data.monadic = def;
    if (inner->type != EBITFIELD) {
        def->data.monadic = *cmpslot;
        *cmpslot = def;
        *slot = reuse;
    } else {
        *slot = func_004A40C0(x);
        def->data.monadic = inner->data.monadic;
        inner->data.monadic = def;
        t = lalloc(sizeof(ENode)); // as written: replaced right away
        memclrw(t, sizeof(ENode));
        t = func_004A40C0(inner);
        t->data.monadic = reuse;
        p = *slot;
        if (p->type != EBITFIELD) {
            while (p->data.monadic->type != EBITFIELD) {
                p = p->data.monadic;
            }
        }
        p->data.monadic = t;
    }
}

// a <? b and a >? b as (a cmp b) ? a : b
void func_004A3E00(ENode* expr, ENodeType cmp) {
    ENode* a = expr->data.diadic.left;
    ENode* b = expr->data.diadic.right;
    ENode* ainner = a->data.monadic;
    ENode* binner = b->data.monadic;
    ENode* c;

    expr->type = ECOND;
    c = lalloc(sizeof(ENode));
    memclrw(c, sizeof(ENode));
    c->type = cmp;
    c->rtype = TYPE(&stsignedint);
    c->data.diadic.left = a;
    c->data.diadic.right = b;
    expr->data.cond.cond = c;
    if (ainner->type == EOBJREF) {
        expr->data.cond.expr1 = func_004A40C0(a);
    }
    if (binner->type == EOBJREF) {
        expr->data.cond.expr2 = func_004A40C0(b);
    }
    if (ainner->type != EOBJREF) {
        minmax_operand(a, ainner, &expr->data.cond.expr1, &c->data.diadic.left);
    }
    if (binner->type != EOBJREF) {
        minmax_operand(b, binner, &expr->data.cond.expr2, &c->data.diadic.right);
    }
}
