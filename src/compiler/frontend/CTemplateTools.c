#include "compiler/CTemplateTools.h"
#include "compiler/CABI.h"
#include "compiler/CDecl.h"
#include "compiler/CError.h"
#include "compiler/CExpr.h"
#include "compiler/CFunc.h"
#include "compiler/CInline.h"
#include "compiler/CInt64.h"
#include "compiler/CMachine.h"
#include "compiler/CompilerTools.h"
#include "compiler/CParser.h"
#include "compiler/CScope.h"
#include "compiler/CTemplateClass.h"
#include "compiler/CTemplateNew.h"
#include "compiler/enode.h"
#include "compiler/objects.h"
#include "compiler/scopes.h"
#include "compiler/templates.h"
#include "compiler/types.h"
#include <string.h>

short ctempl_instdepth;

#if MWCCPSP
// 0x624220: #pragma template_depth(n)
SInt32 ctempl_maxinstdepth;

// 0x4AF440: the current instance stack; inside a function body that is not
// being instantiated, a point-of-instantiation entry for that function
TemplStack* CTemplTool_GetInstanceStack(void) {
    TemplStack* stack;

    if (!ctempl_curinstance && cscope_currentfunc) {
        stack = galloc(sizeof(TemplStack));
        stack->next = NULL;
        stack->u.func = cscope_currentfunc;
        stack->is_func = TRUE;
        stack->is_poi = TRUE;
        return stack;
    }
    return ctempl_curinstance;
}

inline static TemplStack* CTemplTool_GetInstanceStack_inl(void) {
    TemplStack* stack;

    if (!ctempl_curinstance && cscope_currentfunc) {
        stack = galloc(sizeof(TemplStack));
        stack->next = NULL;
        stack->u.func = cscope_currentfunc;
        stack->is_func = TRUE;
        stack->is_poi = TRUE;
        return stack;
    }
    return ctempl_curinstance;
}

// 0x4AF390: 3.0.1 allocates the entry and returns the stack to restore
TemplStack* CTemplTool_PushInstance(TypeClass* tmclass, Object* func) {
    TemplStack* saved;
    TemplStack* stack;

    saved = ctempl_curinstance;
    ctempl_curinstance = CTemplTool_GetInstanceStack_inl();

    stack = galloc(sizeof(TemplStack));
    if (tmclass) {
        stack->u.theclass = tmclass;
        stack->is_func = FALSE;
    } else {
        stack->u.func = func;
        stack->is_func = TRUE;
    }
    stack->is_poi = FALSE;
    stack->next = ctempl_curinstance;
    ctempl_curinstance = stack;

    if (++ctempl_instdepth >= ctempl_maxinstdepth) {
        CError_ErrorTerm(ERR_TEMPLATE_TOO_COMPLEX_RECURSIVE);
    }
    return saved;
}

// 0x4AF360
void CTemplTool_PopInstance(TemplStack* saved) {
    ctempl_curinstance = saved;
    if (--ctempl_instdepth < 0) {
        ctempl_instdepth = 0;
    }
}
#else
void CTemplTool_PushInstance(TemplStack* stack, TypeClass* tmclass, Object* func) {
    if (tmclass) {
        stack->u.theclass = tmclass;
        stack->is_func = FALSE;
    } else {
        stack->u.func = func;
        stack->is_func = TRUE;
    }
    stack->next = ctempl_curinstance;
    ctempl_curinstance = stack;

    if (++ctempl_instdepth >= 64) {
        CError_ErrorTerm(ERR_TEMPLATE_TOO_COMPLEX_RECURSIVE);
    }
}

void CTemplTool_PopInstance(TemplStack* stack) {
    CError_ASSERT(53, ctempl_curinstance == stack);

    ctempl_curinstance = stack->next;
    if (--ctempl_instdepth < 0) {
        ctempl_instdepth = 0;
    }
}
#endif

#if MWCCPSP
// 0x4AF140: a member pointer template argument from a name lookup result:
// keep the member whose type matches the parameter's
ENode* func_004AF140(ENode* expr, Type* type, UInt32 qual) {
    NameSpaceObjectList* list;
    NameSpaceObjectList* single;
    Type* mtype;

    CError_ASSERT(327, expr->type == EOBJACCESS);
    CError_ASSERT(328, IS_TYPE_MEMBERPOINTER(type));
    if (!copts.cpp_extensions &&
        (!expr->data.objaccess.addressTaken || !expr->data.objaccess.is_qualified)) {
        CError_Warning(ERR_ILLEGAL_IMPLICIT_MEMBER_POINTER_CONVERSION);
    }

    mtype = TYPE_MEMBER_POINTER(type)->ty1;
    for (list = expr->data.objaccess.list; list; list = list->next) {
        if (list->object->otype == OT_MEMBERVAR) {
            if (!is_typeequal(OBJ_MEMBER_VAR(list->object)->type, mtype)) {
                CError_Error(ERR_ILLEGAL_TYPE);
            }
            return expr;
        }
        if (list->object->otype == OT_OBJECT &&
            is_memberpointerequal(OBJECT(list->object)->type, mtype)) {
            if (expr->data.objaccess.list != list || list->next) {
                single = galloc(sizeof(NameSpaceObjectList));
                expr->data.objaccess.list = single;
                *single = *list;
                single->next = NULL;
            }
            break;
        }
    }
    if (!list) {
        CError_Error(ERR_ILLEGAL_IMPLICIT_MEMBER_POINTER_CONVERSION);
    }
    return expr;
}
#endif
Boolean CTemplTool_InitDeduceInfo(
    DeduceInfo* info, TemplParam* params, TemplArg* args, Boolean checkUndeduced
) {
    TemplArg* buffer;
    TemplParam* param;
    TemplArg* arg;
    int i;

    if (!params) {
        info->args = info->argBuffer;
        info->maxCount = 0;
        info->nindex = 0xFF;
        return TRUE;
    }

    memclrw(info, sizeof(DeduceInfo));

    i = 0;
    param = params;
    while (param) {
        i++;
        param = param->next;
    }

    if (i > 16) {
        buffer = lalloc(i * sizeof(TemplArg));
        memclrw(buffer, i * sizeof(TemplArg));
    } else {
        buffer = info->argBuffer;
    }

    info->args = buffer;
    info->maxCount = i;
    info->nindex = params->pid.nindex;

    i = 0;
    param = params;
    while (param) {
        buffer[i].pid = param->pid;
        i++;
        param = param->next;
    }

    i = 0;
    param = params;
    while (args) {
        if (!param || param->pid.type != args->pid.type) {
            return FALSE;
        }
        arg = args;
        buffer[i].data = arg->data;
        if (i > 0) {
            buffer[i - 1].next = &buffer[i];
        }
        buffer[i].next = NULL;
        buffer[i].is_deduced = TRUE;
        info->count++;

        if (param->pid.type == TPT_NONTYPE &&
            !CTemplTool_IsTemplateArgumentDependentType(param->data.paramdecl.type)) {
            if (CExpr_CanImplicitlyConvert(
                    buffer[i].data.paramdecl.expr,
                    param->data.paramdecl.type,
                    param->data.paramdecl.qual
                )) {
#if MWCCPSP
                ENode* expr = buffer[i].data.paramdecl.expr;
                if (ENODE_IS(expr, EINDIRECT)) {
                    if (!IS_TYPE_REFERENCE(param->data.paramdecl.type)) {
                        return FALSE;
                    }
                    expr = CExpr_AssignmentPromotion(
                        expr, param->data.paramdecl.type, param->data.paramdecl.qual, FALSE
                    );
                    if (!ENODE_IS(expr, EOBJREF)) {
                        return FALSE;
                    }
                    buffer[i].data.paramdecl.expr = expr;
                    buffer[i].data.paramdecl.is_ref = TRUE;
                } else if (ENODE_IS(expr, EOBJACCESS) &&
                           IS_TYPE_MEMBERPOINTER(param->data.paramdecl.type)) {
                    buffer[i].data.paramdecl.expr =
                        func_004AF140(expr, param->data.paramdecl.type, param->data.paramdecl.qual);
                } else {
                    buffer[i].data.paramdecl.expr = CExpr_AssignmentPromotion(
                        expr, param->data.paramdecl.type, param->data.paramdecl.qual, FALSE
                    );
                }
#else
                buffer[i].data.paramdecl.expr = CExpr_AssignmentPromotion(
                    buffer[i].data.paramdecl.expr,
                    param->data.paramdecl.type,
                    param->data.paramdecl.qual,
                    FALSE
                );
#endif
            } else {
                return FALSE;
            }
        }
        i++;
        args = args->next;
        param = param->next;
    }

    if (checkUndeduced) {
        for (param = params, i = 0; param; param = param->next, i++) {
            if (!buffer[i].is_deduced && !param->name) {
                switch (param->pid.type) {
                    case TPT_TYPE:
                        buffer[i].data.typeparam.type = &stvoid;
                        break;
                    case TPT_NONTYPE:
                        buffer[i].data.paramdecl.expr = nullnode();
                        break;
                    case TPT_TEMPLATE:
                    default:
#if MWCCPSP
                        CError_FATAL(482);
#else
                        CError_FATAL(208);
#endif
                }
                buffer[i].is_deduced = TRUE;
            }
        }
    }

    return TRUE;
}

void CTemplTool_InsertTemplateParameter(NameSpace* nspace, TemplParam* param) {
    Type* type;
    ObjType* obj;
    NameSpaceName* nsn;

    type = TYPE(CDecl_NewTemplDepType(TEMPLDEP_ARGUMENT));
    TYPE_TEMPLATE(type)->u.pid = param->pid;

    obj = galloc(sizeof(ObjType));
    memclrw(obj, sizeof(ObjType));
    obj->otype = OT_TYPE;
    obj->access = ACCESSPUBLIC;
    obj->type = type;

    for (nsn = nspace->tparams; nsn; nsn = nsn->next) {
        if (nsn->name == param->name) {
            CError_Error(ERR_IDENTIFIER_REDECLARED, param->name->name);
            return;
        }
    }

    nsn = galloc(sizeof(NameSpaceName));
    memclrw(nsn, sizeof(NameSpaceName));

    nsn->name = param->name;
    nsn->first.object = OBJ_BASE(obj);

    nsn->next = nspace->tparams;
    nspace->tparams = nsn;
}

TemplArg* CTemplTool_MakeTemplArgList(DeduceInfo* info) {
    TemplArg* args;
    TemplArg* last;
    int i;

    for (i = 0; i < info->maxCount; i++) {
        if (i) {
            last->next = galloc(sizeof(TemplArg));
            last = last->next;
        } else {
            args = last = galloc(sizeof(TemplArg));
        }

        *last = info->args[i];
#if MWCCPSP
        // 3.0.1 (0x4AED30): non-type arguments get a global copy
        if (last->pid.type == TPT_NONTYPE && last->data.paramdecl.expr) {
            last->data.paramdecl.expr =
                CInline_CopyExpression(last->data.paramdecl.expr, CopyModeGlobal);
        }
#endif
    }

    last->next = NULL;
    return args;
}

Boolean CTemplTool_IsIdenticalTemplArgList(TemplArg* args, TemplParam* params) {
    while (args) {
        if (!params) {
            return FALSE;
        }

#if MWCCPSP
        CError_ASSERT(636, params->pid.type == args->pid.type);
#else
        CError_ASSERT(297, params->pid.type == args->pid.type);
#endif

        switch (args->pid.type) {
            case TPT_TYPE:
                if (!IS_TYPE_TEMPLATE(args->data.typeparam.type) ||
                    TYPE_TEMPLATE(args->data.typeparam.type)->dtype != TEMPLDEP_ARGUMENT ||
                    TYPE_TEMPLATE(args->data.typeparam.type)->u.pid.index != params->pid.index ||
                    TYPE_TEMPLATE(args->data.typeparam.type)->u.pid.nindex != params->pid.nindex) {
                    return FALSE;
                }
                break;
            case TPT_NONTYPE:
                if (!ENODE_IS(args->data.paramdecl.expr, ETEMPLDEP) ||
                    args->data.paramdecl.expr->data.templdep.subtype != TDE_PARAM ||
                    args->data.paramdecl.expr->data.templdep.u.pid.index != params->pid.index ||
                    args->data.paramdecl.expr->data.templdep.u.pid.nindex != params->pid.nindex) {
                    return FALSE;
                }
                break;
            case TPT_TEMPLATE:
                if (!IS_TYPE_TEMPLATE(args->data.typeparam.type) ||
                    TYPE_TEMPLATE(args->data.typeparam.type)->dtype != TEMPLDEP_ARGUMENT ||
                    TYPE_TEMPLATE(args->data.typeparam.type)->u.pid.index != params->pid.index ||
                    TYPE_TEMPLATE(args->data.typeparam.type)->u.pid.nindex != params->pid.nindex) {
                    return FALSE;
                }
                break;
            default:
#if MWCCPSP
                CError_FATAL(670);
#else
                CError_FATAL(331);
#endif
        }

        args = args->next;
        params = params->next;
    }

    return params == NULL;
}

Type* CTemplTool_GetSelfRefTemplate(Type* type) {
    TemplClass* target;
    TemplClass* templ;
    TemplArg* args;

    CError_ASSERT(347, IS_TYPE_TEMPLATE(type));

    if (TYPE_TEMPLATE(type)->dtype == TEMPLDEP_TEMPLATE) {
        if (CTemplTool_IsIdenticalTemplArgList(
                TYPE_TEMPLATE(type)->u.templ.args, TYPE_TEMPLATE(type)->u.templ.templ->templ__params
            )) {
            return TYPE(TYPE_TEMPLATE(type)->u.templ.templ);
        }

        target = TYPE_TEMPLATE(type)->u.templ.templ;
        if (target->pspecs) {
            templ = target;
            if (CTemplClass_FindPartialTemplate(TYPE_TEMPLATE(type)->u.templ.args, &templ, &args) &&
                CTemplTool_IsIdenticalTemplArgList(args, templ->templ__params)) {
                return TYPE(templ);
            }
        }

        return NULL;
    }

    if (TYPE_TEMPLATE(type)->dtype == TEMPLDEP_QUALNAME) {
        Type* t = CTemplTool_GetSelfRefTemplate(TYPE(TYPE_TEMPLATE(type)->u.qual.type));
        if (t &&
            (t = CScope_GetLocalTagType(TYPE_CLASS(t)->nspace, TYPE_TEMPLATE(type)->u.qual.name)) &&
            IS_TYPE_CLASS(t) &&
            (TYPE_CLASS(t)->flags & CLASS_IS_TEMPL) &&
            !TEMPL_CLASS(t)->templ__params) {
            return t;
        }

        return NULL;
    }

    if (TYPE_TEMPLATE(type)->dtype == TEMPLDEP_QUALTEMPL) {
        Type* t;

        CError_ASSERT(
            389, TYPE_TEMPLATE(TYPE_TEMPLATE(type)->u.qualtempl.type)->dtype == TEMPLDEP_QUALNAME
        );

        t = CTemplTool_GetSelfRefTemplate(
            TYPE(TYPE_TEMPLATE(TYPE_TEMPLATE(type)->u.qualtempl.type)->u.qual.type)
        );
        if (t &&
            (t = CScope_GetLocalTagType(
                 TYPE_CLASS(t)->nspace,
                 TYPE_TEMPLATE(TYPE_TEMPLATE(type)->u.qualtempl.type)->u.qual.name
             )) &&
            IS_TYPE_CLASS(t) &&
            (TYPE_CLASS(t)->flags & CLASS_IS_TEMPL)) {
            TemplClass* tm = TEMPL_CLASS(t);
            if (CTemplTool_IsIdenticalTemplArgList(
                    TYPE_TEMPLATE(type)->u.qualtempl.args, tm->templ__params
                )) {
                return TYPE(tm);
            }
        }
    }

    return NULL;
}

TemplateFunction* CTemplTool_GetFuncTempl(Object* object) {
    while (object->datatype == DALIAS) {
        object = object->u.alias.object;
    }

#if MWCCPSP
    CError_ASSERT(173, IS_TEMPL_FUNC(object->type));
#else
    CError_ASSERT(416, IS_TEMPL_FUNC(object->type));
#endif
    return object->u.func.u.templ;
}

void CTemplTool_MergeDefaultArgs(TemplParam* dest, TemplParam* src) {
    while (1) {
        if (!dest) {
            CError_ASSERT(455, !src);
            return;
        }

        CError_ASSERT(458, src);
        CError_ASSERT(459, dest->pid.type == src->pid.type);

        switch (dest->pid.type) {
            case TPT_TYPE:
                if (!dest->data.typeparam.type && src->data.typeparam.type) {
                    dest->data = src->data;
                }
                break;
            case TPT_NONTYPE:
                if (!dest->data.paramdecl.defaultarg && src->data.paramdecl.defaultarg) {
                    dest->data = src->data;
                }
                break;
            case TPT_TEMPLATE:
                if (!dest->data.templparam.defaultarg && src->data.templparam.defaultarg) {
                    dest->data = src->data;
                }
                break;
            default:
                CError_FATAL(484);
        }

        dest = dest->next;
        src = src->next;
    }
}

static FuncArg* CTemplTool_GetFirstRealArg(TypeFunc* tfunc) {
    FuncArg* arg = tfunc->args;

    if (IS_TYPEFUNC_NONSTATIC_METHOD(tfunc)) {
#if MWCCPSP
        CError_ASSERT(835, arg);
#else
        CError_ASSERT(502, arg);
#endif
        arg = arg->next;
        if ((tfunc->flags & FUNC_IS_CTOR) &&
            (TYPE_METHOD(tfunc)->theclass->flags & CLASS_HAS_VBASES)) {
#if MWCCPSP
            CError_ASSERT(840, arg);
#else
            CError_ASSERT(507, arg);
#endif
            arg = arg->next;
        }
    }

    return arg;
}

void CTemplTool_MergeArgNames(TypeFunc* src, TypeFunc* dest) {
    FuncArg* destArg;
    FuncArg* srcArg;

#if MWCCPSP
    CError_ASSERT(857, IS_TYPE_FUNC(dest) && IS_TYPE_FUNC(src));
#else
    CError_ASSERT(524, IS_TYPE_FUNC(dest) && IS_TYPE_FUNC(src));
#endif

    srcArg = CTemplTool_GetFirstRealArg(src);
    destArg = CTemplTool_GetFirstRealArg(dest);

    while (1) {
        if (!srcArg || !destArg || srcArg == &elipsis || destArg == &elipsis) {
#if MWCCPSP
            CError_ASSERT(864, srcArg == destArg);
#else
            CError_ASSERT(531, srcArg == destArg);
#endif
            break;
        }

        destArg->name = srcArg->name;
        srcArg = srcArg->next;
        destArg = destArg->next;
    }

    if (IS_TYPEFUNC_NONSTATIC_METHOD(TYPE_FUNC(dest))) {
#if MWCCPSP
        CError_ASSERT(871, destArg = TYPE_FUNC(dest)->args);
#else
        CError_ASSERT(538, destArg = TYPE_FUNC(dest)->args);
#endif
        if (!destArg->name) {
            destArg->name = this_name_node;
        }
    }
}

Boolean CTemplTool_EqualParams(TemplParam* a, TemplParam* b, Boolean copyNames) {
    while (1) {
        if (!a) {
            return b == NULL;
        }
        if (!b) {
            return FALSE;
        }

        if (a->pid.type != b->pid.type) {
            return FALSE;
        }

        if (copyNames) {
            a->name = b->name;
        }

        switch (a->pid.type) {
            case TPT_TYPE:
                break;
            case TPT_NONTYPE:
                if (!is_typesame(a->data.paramdecl.type, b->data.paramdecl.type) ||
                    a->data.paramdecl.qual != b->data.paramdecl.qual) {
                    return FALSE;
                }
                break;
            case TPT_TEMPLATE:
                break;
            default:
#if MWCCPSP
                CError_FATAL(909);
#else
                CError_FATAL(576);
#endif
        }

        a = a->next;
        b = b->next;
    }
}

NameSpace* CTemplTool_SetupTemplateArgumentNameSpace(
    TemplParam* params, TemplArg* args, Boolean is_global
) {
    NameSpace* nspace;
    Boolean clear_global;
    ObjType* objType;
    Object* object;

    clear_global = FALSE;
    if (!is_global && trychain) {
        is_global = clear_global = TRUE;
    }

    nspace = CScope_NewListNameSpace(NULL, is_global);
    nspace->is_templ = TRUE;

    if (clear_global) {
        nspace->is_global = FALSE;
    }

    if (!params) {
        return nspace;
    }

    while (params) {
#if MWCCPSP
        CError_ASSERT(940, args);
#else
        CError_ASSERT(607, args);
#endif

        if (params->name) {
            switch (args->pid.type) {
                case TPT_TYPE:
                    if (is_global) {
                        objType = galloc(sizeof(ObjType));
                        memclrw(objType, sizeof(ObjType));
                    } else {
                        objType = lalloc(sizeof(ObjType));
                        memclrw(objType, sizeof(ObjType));
                    }
                    objType->otype = OT_TYPE;
                    objType->access = ACCESSPUBLIC;
                    objType->type = args->data.typeparam.type;
                    objType->qual = args->data.typeparam.qual;
                    CScope_AddObject(nspace, params->name, OBJ_BASE(objType));
                    break;
                case TPT_NONTYPE:
                    if (is_global) {
                        object = galloc(sizeof(Object));
                        memclrw(object, sizeof(Object));
                    } else {
                        object = lalloc(sizeof(Object));
                        memclrw(object, sizeof(Object));
                    }
                    object->otype = OT_OBJECT;
                    object->access = ACCESSPUBLIC;
                    object->nspace = nspace;
                    object->name = params->name;
                    object->type = args->data.paramdecl.expr->rtype;
                    object->qual = ENODE_QUALS(args->data.paramdecl.expr);
                    object->datatype = DEXPR;
                    object->u.expr = args->data.paramdecl.expr;
                    if (IS_TYPE_REFERENCE(params->data.paramdecl.type)) {
#if MWCCPSP
                        CError_ASSERT(984, IS_TYPE_POINTER_ONLY(object->u.expr->rtype));
#else
                        CError_ASSERT(652, IS_TYPE_POINTER_ONLY(object->u.expr->rtype));
#endif
                        object->u.expr = makemonadicnode(object->u.expr, EINDIRECT);
                        object->u.expr->rtype = TPTR_TARGET(params->data.paramdecl.type);
                    }
                    if (is_global) {
                        object->u.expr = CInline_CopyExpression(object->u.expr, CopyModeGlobal);
                    }
                    CScope_AddObject(nspace, params->name, OBJ_BASE(object));
                    break;
                case TPT_TEMPLATE:
                    if (is_global) {
                        objType = galloc(sizeof(ObjType));
                        memclrw(objType, sizeof(ObjType));
                    } else {
                        objType = lalloc(sizeof(ObjType));
                        memclrw(objType, sizeof(ObjType));
                    }
                    objType->otype = OT_TYPE;
                    objType->access = ACCESSPUBLIC;
                    objType->type = args->data.typeparam.type;
                    objType->qual = 0;
                    CScope_AddObject(nspace, params->name, OBJ_BASE(objType));
                    break;
                default:
#if MWCCPSP
                    CError_FATAL(1013);
#else
                    CError_FATAL(681);
#endif
            }
        }

        params = params->next;
        args = args->next;
    }

#if MWCCPSP
    CError_ASSERT(1017, !args);
#else
    CError_ASSERT(685, !args);
#endif

    return nspace;
}

void CTemplTool_SetupOuterTemplateArgumentNameSpace(NameSpace* nspace) {
    NameSpace* newns;

    while (nspace) {
        if (nspace->theclass && (nspace->theclass->flags & CLASS_IS_TEMPL_INST)) {
            newns = CTemplTool_SetupTemplateArgumentNameSpace(
                TEMPL_CLASS(TEMPL_CLASS_INST(nspace->theclass)->templ)->templ__params,
                TEMPL_CLASS_INST(nspace->theclass)->inst_args,
                FALSE
            );
            newns->parent = nspace->parent;
            nspace->parent = newns;
        }

        nspace = nspace->parent;
    }
}

NameSpace* CTemplTool_InsertTemplateArgumentNameSpace(
    TemplParam* params, TemplClassInst* inst, CScopeSave* save
) {
    NameSpace* nspace = CTemplTool_SetupTemplateArgumentNameSpace(params, inst->inst_args, FALSE);

    nspace->parent = inst->theclass.nspace->parent;
    inst->theclass.nspace->parent = nspace;

    CTemplTool_SetupOuterTemplateArgumentNameSpace(nspace);
    CScope_SetNameSpaceScope(inst->theclass.nspace, save);

    return nspace;
}

void CTemplTool_RemoveOuterTemplateArgumentNameSpace(NameSpace* nspace) {
    while (nspace->parent) {
        if (nspace->theclass &&
            (nspace->theclass->flags & CLASS_IS_TEMPL_INST) &&
            nspace->parent->is_templ) {
            nspace->parent = nspace->parent->parent;
        }
        nspace = nspace->parent;
    }
}

void CTemplTool_RemoveTemplateArgumentNameSpace(
    NameSpace* nspace, TemplClassInst* inst, CScopeSave* save
) {
    CTemplTool_RemoveOuterTemplateArgumentNameSpace(inst->theclass.nspace);
    CScope_RestoreScope(save);
}

Boolean CTemplTool_IsTemplateArgumentDependentType(Type* type) {
    FuncArg* arg;

    while (1) {
        switch (type->type) {
            case TYPETEMPLATE:
                return TRUE;
            case TYPEVOID:
            case TYPEINT:
            case TYPEFLOAT:
            case TYPEENUM:
            case TYPESTRUCT:
                return FALSE;
            case TYPECLASS:
                return (TYPE_CLASS(type)->flags & CLASS_IS_TEMPL) != 0;
            case TYPEMEMBERPOINTER:
                if (CTemplTool_IsTemplateArgumentDependentType(TYPE_MEMBER_POINTER(type)->ty1)) {
                    return TRUE;
                }
                type = TYPE_MEMBER_POINTER(type)->ty2;
                continue;
            case TYPEPOINTER:
            case TYPEARRAY:
                type = TPTR_TARGET(type);
                continue;
            case TYPEFUNC:
                for (arg = TYPE_FUNC(type)->args; arg && arg != &elipsis && arg != &oldstyle;
                     arg = arg->next) {
                    if (CTemplTool_IsTemplateArgumentDependentType(arg->type)) {
                        return TRUE;
                    }
                }
                type = TYPE_FUNC(type)->functype;
                continue;
            case TYPEBITFIELD:
                type = TYPE_BITFIELD(type)->bitfieldtype;
                continue;
            case TYPETEMPLDEPEXPR:
                return TRUE;
            default:
                CError_FATAL(822);
        }
    }
}

Boolean CTemplTool_IsTemplateArgumentDependentExpression(ENode* expr) {
#if MWCCPSP
    CError_ASSERT(1514, expr);
    if (IS_TYPE_TEMPLDEPEXPR(expr->rtype)) {
        return TRUE;
    }
    return FALSE;
#else
    if (!expr) {
        return FALSE;
    }
#endif

    return IS_TYPE_TEMPLDEPEXPR(expr->rtype);
}

Boolean CTemplTool_IsSameTemplate(TemplParam* params, TemplArg* args) {
    while (1) {
        if (!args) {
#if MWCCPSP
            CError_ASSERT(1616, !params);
#else
            CError_ASSERT(850, !params);
#endif
            return TRUE;
        }

#if MWCCPSP
        CError_ASSERT(1619, params && args->pid.type == params->pid.type);
#else
        CError_ASSERT(853, params && args->pid.type == params->pid.type);
#endif

        switch (args->pid.type) {
            case TPT_TYPE:
                if (!IS_TYPE_TEMPLATE(args->data.typeparam.type) ||
                    TYPE_TEMPLATE(args->data.typeparam.type)->dtype != TEMPLDEP_ARGUMENT ||
                    TYPE_TEMPLATE(args->data.typeparam.type)->u.pid.nindex != params->pid.nindex ||
                    TYPE_TEMPLATE(args->data.typeparam.type)->u.pid.index != params->pid.index ||
                    args->data.typeparam.qual != 0) {
                    return FALSE;
                }
                break;
            case TPT_NONTYPE:
                if (!ENODE_IS(args->data.paramdecl.expr, ETEMPLDEP) ||
                    args->data.paramdecl.expr->data.templdep.subtype != TDE_PARAM ||
                    args->data.paramdecl.expr->data.templdep.u.pid.nindex != params->pid.nindex ||
                    args->data.paramdecl.expr->data.templdep.u.pid.index != params->pid.index) {
                    return FALSE;
                }
                break;
            case TPT_TEMPLATE:
                if (!IS_TYPE_TEMPLATE(args->data.typeparam.type)) {
                    return FALSE;
                }
                break;
            default:
#if MWCCPSP
                CError_FATAL(1652);
#else
                CError_FATAL(886);
#endif
        }

        args = args->next;
        params = params->next;
    }
}

TemplClass* CTemplTool_IsTemplate(TypeTemplDep* ttd) {
    if (ttd->dtype == TEMPLDEP_QUALNAME && ttd->u.qual.type->dtype == TEMPLDEP_TEMPLATE) {
        ttd = ttd->u.qual.type;
    } else if (ttd->dtype != TEMPLDEP_TEMPLATE) {
        return NULL;
    }

    if (CTemplTool_IsSameTemplate(ttd->u.templ.templ->templ__params, ttd->u.templ.args)) {
        return ttd->u.templ.templ;
    } else {
        return NULL;
    }
}
#if MWCCPSP

#endif
#if MWCCPSP
// 3.0.1 (0x4ADEB0): is the expression's type template-dependent
Boolean CTemplTool_IsTemplDepExpr(ENode* expr) {
    CError_ASSERT(1411, expr);
    if (IS_TYPE_TEMPLDEPEXPR(expr->rtype)) {
        return TRUE;
    }
    return FALSE;
}
#endif

Type* CTemplTool_IsDependentTemplate(TemplClass* tmclass, TemplArg* args) {
    TemplParam* param;
    TemplArg* arg;
    Type* type;

    if (!tmclass->templ_parent || tmclass->inst_parent) {
        arg = args;
        param = tmclass->templ__params;
        while (1) {
            if (!arg) {
                CError_ASSERT(988, !param);
                return NULL;
            }

            CError_ASSERT(991, param && arg->pid.type == param->pid.type);

            switch (arg->pid.type) {
                case TPT_TYPE:
                    if (CTemplTool_IsTemplateArgumentDependentType(arg->data.typeparam.type)) {
                        goto done;
                    }
                    break;
                case TPT_NONTYPE:
                    if (CTemplTool_IsTemplateArgumentDependentExpression(
                            arg->data.paramdecl.expr
                        )) {
                        goto done;
                    }
                    break;
                case TPT_TEMPLATE:
                    if (!IS_TYPE_CLASS(arg->data.typeparam.type) &&
                        CTemplTool_IsTemplateArgumentDependentType(arg->data.typeparam.type)) {
                        goto done;
                    }
                    break;
                default:
                    CError_FATAL(1008);
                    goto done;
            }

            arg = arg->next;
            param = param->next;
        }
    }
done:
    if (cscope_current->theclass == TYPE_CLASS(tmclass) &&
        CTemplTool_IsSameTemplate(tmclass->templ__params, args)) {
        return TYPE(tmclass);
    }

    type = TYPE(CDecl_NewTemplDepType(TEMPLDEP_TEMPLATE));
    TYPE_TEMPLATE(type)->u.templ.templ = tmclass;
    TYPE_TEMPLATE(type)->u.templ.args = args;
    return type;
}

Boolean CTemplTool_EqualExprTypes(ENode* a, ENode* b) {
    Object* objA;
    Object* objB;

    if (!a || !b) {
        return FALSE;
    }
    if (a->type != b->type) {
        return FALSE;
    }

    switch (a->type) {
        case EINTCONST:
            return CInt64_Equal(a->data.intval, b->data.intval);
        case EOBJREF:
            objA = a->data.objref;
            while (objA->datatype == DALIAS) {
                objA = objA->u.alias.object;
            }
            objB = b->data.objref;
            while (objB->datatype == DALIAS) {
                objB = objB->u.alias.object;
            }
            return objA == objB;
        case EMEMBER:
            return a->data.emember->list == b->data.emember->list;
        case ETEMPLDEP:
            if (a->data.templdep.subtype != b->data.templdep.subtype) {
                return FALSE;
            }

            switch (a->data.templdep.subtype) {
                case TDE_PARAM:
                    return a->data.templdep.u.pid.nindex == b->data.templdep.u.pid.nindex &&
                        a->data.templdep.u.pid.index == b->data.templdep.u.pid.index;
                case TDE_SIZEOF:
                case TDE_ALIGNOF:
                    return is_typesame(
                        a->data.templdep.u.typeexpr.type, b->data.templdep.u.typeexpr.type
                    );
                case TDE_CAST:
                    return is_typesame(
                               a->data.templdep.u.cast.type, b->data.templdep.u.cast.type
                           ) &&
                        a->data.templdep.u.cast.qual == b->data.templdep.u.cast.qual;
                case TDE_QUALNAME:
                    return is_typesame(
                               TYPE(a->data.templdep.u.qual.type),
                               TYPE(b->data.templdep.u.qual.type)
                           ) &&
                        a->data.templdep.u.qual.name == b->data.templdep.u.qual.name;
                case TDE_OBJ:
                    return a->data.templdep.u.obj == b->data.templdep.u.obj;
                case TDE_ADDRESS_OF:
                    return CTemplTool_EqualExprTypes(
                        a->data.templdep.u.monadic, b->data.templdep.u.monadic
                    );
                default:
                    CError_FATAL(1086);
            }
        case EMUL:
        case EDIV:
        case EMODULO:
        case EADD:
        case ESUB:
        case ESHL:
        case ESHR:
        case ELESS:
        case EGREATER:
        case ELESSEQU:
        case EGREATEREQU:
        case EEQU:
        case ENOTEQU:
        case EAND:
        case EXOR:
        case EOR:
        case ELAND:
        case ELOR:
        case EROTL:
        case EROTR:
            return CTemplTool_EqualExprTypes(a->data.diadic.left, b->data.diadic.left) &&
                CTemplTool_EqualExprTypes(a->data.diadic.right, b->data.diadic.right);
        case EMONMIN:
        case EBINNOT:
        case ELOGNOT:
            return CTemplTool_EqualExprTypes(a->data.monadic, b->data.monadic);
        case ECOND:
            return CTemplTool_EqualExprTypes(a->data.cond.cond, b->data.cond.cond) &&
                CTemplTool_EqualExprTypes(a->data.cond.expr1, b->data.cond.expr1) &&
                CTemplTool_EqualExprTypes(a->data.cond.expr2, b->data.cond.expr2);
        default:
            CError_FATAL(1122);
            return FALSE;
    }
}

ENode* CTempl_MakeTemplDepExpr(ENode* left, ENodeType nt, ENode* right) {
#if MWCCPSP
    if (!ENODE_IS_TEMPLDEPEXPR(right)) {
#else
    if (!IS_TYPE_TEMPLDEPEXPR(right->rtype)) {
#endif
        right = pointer_generation(right);
        if (!ENODE_IS(right, EINTCONST)) {
            CError_Error(ERR_ILLEGAL_TEMPLATE_ARGUMENT_DEPENDENT_EXPRESSION);
            right = nullnode();
        }
    }

    if (left) {
#if MWCCPSP
        if (!ENODE_IS_TEMPLDEPEXPR(left)) {
#else
        if (!IS_TYPE_TEMPLDEPEXPR(left->rtype)) {
#endif
            left = pointer_generation(left);
            if (!ENODE_IS(left, EINTCONST)) {
                CError_Error(ERR_ILLEGAL_TEMPLATE_ARGUMENT_DEPENDENT_EXPRESSION);
                left = nullnode();
            }
        }

        left = makediadicnode(left, right, nt);
    } else {
        left = makemonadicnode(right, nt);
    }

    left->rtype = &sttemplexpr;
    return left;
}

void CTemplTool_CheckTemplArgType(Type* type) {
    while (IS_TYPE_POINTER_ONLY(type)) {
        type = TPTR_TARGET(type);
    }

    if (IS_TYPE_CLASS(type)) {
        if (IsTempName(TYPE_CLASS(type)->classname) ||
            CScope_IsInLocalNameSpace(TYPE_CLASS(type)->nspace)) {
            CError_Error(ERR_ILLEGAL_TEMPLATE_ARGUMENTS);
            return;
        }
    }
}

Boolean CTemplTool_EqualArgs(TemplArg* a, TemplArg* b) {
    while (a) {
        if (!b || a->pid.type != b->pid.type) {
            return FALSE;
        }

        switch (a->pid.type) {
            case TPT_TYPE:
                if (!is_typesame(a->data.typeparam.type, b->data.typeparam.type) ||
                    a->data.typeparam.qual != b->data.typeparam.qual) {
                    return FALSE;
                }
                break;
            case TPT_NONTYPE:
                if (!CTemplTool_EqualExprTypes(a->data.paramdecl.expr, b->data.paramdecl.expr)) {
                    return FALSE;
                }
                break;
            case TPT_TEMPLATE:
                if (!is_typesame(a->data.typeparam.type, b->data.typeparam.type)) {
                    return FALSE;
                }
                break;
            default:
#if MWCCPSP
                CError_FATAL(2049);
#else
                CError_FATAL(1215);
#endif
        }

        a = a->next;
        b = b->next;
    }

    if (b) {
        return FALSE;
    }

    return TRUE;
}

TemplArg* CTemplTool_MakeGlobalTemplArgCopy(TemplArg* args) {
    TemplArg* firstCopy;
    TemplArg* copy;

    firstCopy = NULL;
    while (args) {
        if (firstCopy) {
            copy->next = galloc(sizeof(TemplArg));
            copy = copy->next;
        } else {
            copy = galloc(sizeof(TemplArg));
            firstCopy = copy;
        }

        *copy = *args;
        if (copy->pid.type == TPT_NONTYPE && copy->data.paramdecl.expr) {
            copy->data.paramdecl.expr =
                CInline_CopyExpression(copy->data.paramdecl.expr, CopyModeGlobal);
        }

        args = args->next;
    }

    return firstCopy;
}

Boolean CTemplTool_TemplDepTypeCompare(TypeTemplDep* a, TypeTemplDep* b) {
    if (a == b) {
        return TRUE;
    }
    if (a->dtype != b->dtype) {
        return FALSE;
    }

    switch (a->dtype) {
        case TEMPLDEP_ARGUMENT:
            return a->u.pid.nindex == b->u.pid.nindex && a->u.pid.index == b->u.pid.index;
        case TEMPLDEP_QUALNAME:
            return CTemplTool_TemplDepTypeCompare(a->u.qual.type, b->u.qual.type) &&
                a->u.qual.name == b->u.qual.name;
        case TEMPLDEP_TEMPLATE:
            return a->u.templ.templ == b->u.templ.templ &&
                CTemplTool_EqualArgs(a->u.templ.args, b->u.templ.args);
        case TEMPLDEP_ARRAY:
            return is_typesame(a->u.array.type, b->u.array.type) &&
                CTemplTool_EqualExprTypes(a->u.array.index, b->u.array.index);
        case TEMPLDEP_QUALTEMPL:
            return CTemplTool_TemplDepTypeCompare(a->u.qualtempl.type, b->u.qualtempl.type) &&
                CTemplTool_EqualArgs(a->u.qualtempl.args, b->u.qualtempl.args);
        case TEMPLDEP_BITFIELD:
            return is_typesame(a->u.bitfield.type, b->u.bitfield.type) &&
                CTemplTool_EqualExprTypes(a->u.bitfield.size, b->u.bitfield.size);
        default:
            CError_FATAL(1286);
            return FALSE;
    }
}

Type* CTemplTool_DeduceArgDepType(TemplArg* args, Type* type, UInt32 qual, UInt32* resultQual) {
    TemplArg* arg;

    *resultQual = qual;

    if (IS_TYPE_TEMPLATE(type) && TYPE_TEMPLATE(type)->dtype == TEMPLDEP_ARGUMENT) {
        arg = args;
        while (1) {
            if (!arg) {
                return NULL;
            }

            if (arg->pid.index == TYPE_TEMPLATE(type)->u.pid.index &&
                arg->pid.nindex == TYPE_TEMPLATE(type)->u.pid.nindex) {
                break;
            }

            arg = arg->next;
        }

        CError_ASSERT(1314, arg->pid.type == TPT_TYPE);
        *resultQual |= arg->data.typeparam.qual;
        return arg->data.typeparam.type;
    }

    return NULL;
}

static TemplClassInst* CTemplTool_FindNestedClassInstance(
    TemplClass* templ, TemplClass* outer, TemplClassInst* outerInst
) {
    TemplClass* array[32];
    TemplClassInst* inst;
    int i;

    array[0] = templ;
    i = 0;
    while (1) {
        CError_ASSERT(1338, i < 32);
        CError_ASSERT(1339, templ = templ->templ_parent);

        if (templ == outer) {
            break;
        }

        CError_ASSERT(1341, templ->templ__params == NULL);

        array[++i] = templ;
    }

    while (1) {
        inst = array[i--]->instances;
        while (1) {
            CError_ASSERT(1350, inst);
            if (inst->parent == outerInst) {
                break;
            }
            inst = inst->next;
        }

        outerInst = inst;
        if (i < 0) {
            break;
        }

        if ((inst->theclass.flags & (CLASS_COMPLETED | CLASS_IS_TEMPL_INST)) ==
            CLASS_IS_TEMPL_INST) {
            CTempl_InstantiateTemplateClass(TYPE_CLASS(inst));
        }
    }

    return inst;
}

static TemplClassInst* CTemplTool_FindNestedClass(TemplClass* a, TemplClassInst* b, TemplClass* c) {
    TemplClass* scan;

    while (1) {
        for (scan = c->templ_parent; scan; scan = scan->templ_parent) {
            if (scan == a) {
                return CTemplTool_FindNestedClassInstance(c, a, b);
            }
        }

        a = a->templ_parent;
        if (!a) {
            break;
        }

        b = b->parent;
        CError_ASSERT(1377, b);
    }

    return NULL;
}

static Type* CTemplTool_FindTemplateInstance(TypeDeduce* deduce, TemplClass* templ) {
    TemplClass* scantempl;
    TemplClass* dtempl;
    TemplClassInst* scaninst;
    TemplClassInst* dinst;

    dtempl = deduce->tmclass;
    CError_ASSERT(1393, dtempl);

    dinst = deduce->inst;
    if (!dinst) {
        if (!dtempl->templ_parent || !dtempl->inst_parent) {
            return TYPE(templ);
        }

        dtempl = dtempl->templ_parent;
        dinst = TEMPL_CLASS_INST(deduce->tmclass->inst_parent);
    }

    scantempl = dtempl;
    scaninst = dinst;
    while (1) {
        if (scantempl == templ) {
            return TYPE(scaninst);
        }

        if (!scantempl->templ_parent && scantempl->pspec_owner) {
            scantempl = TEMPL_CLASS(scantempl->pspec_owner);
        }

        scantempl = scantempl->templ_parent;
        if (!scantempl) {
            break;
        }

        CError_ASSERT(1416, scaninst = scaninst->parent);
    }

    if (dtempl->flags & TEMPLCLASS_IS_SPECIALIZATION) {
        scantempl = TEMPL_CLASS(dtempl->theclass.nspace->theclass);
        CError_ASSERT(1422, scantempl->theclass.flags & CLASS_IS_TEMPL);
        scaninst = dinst;

        while (1) {
            if (scantempl == templ) {
                return TYPE(scaninst);
            }

            scantempl = scantempl->templ_parent;
            if (!scantempl) {
                break;
            }

            CError_ASSERT(1430, scaninst = scaninst->parent);
        }
    }

    if (!templ->templ__params && (scaninst = CTemplTool_FindNestedClass(dtempl, dinst, templ))) {
        return TYPE(scaninst);
    }

    CError_FATAL(1477);
    return NULL;
}

static ENodeList* CTemplTool_DeduceExprList(TypeDeduce* deduce, ENodeList* list) {
    ENodeList* resultList;
    ENodeList* last;

    resultList = NULL;
    while (list) {
        if (resultList) {
            last->next = lalloc(sizeof(ENodeList));
            last = last->next;
        } else {
            last = lalloc(sizeof(ENodeList));
            resultList = last;
        }

        *last = *list;
        last->node = CTemplTool_DeduceExpr(deduce, last->node);

        list = list->next;
    }

    return resultList;
}

#if MWCCPSP
extern ENode* CExpr_TypeOpValue(UInt8 kind, Type* type, ENode* expr); // 0x516D60
#endif
ENode* CTemplTool_DeduceExpr(TypeDeduce* deduce, ENode* expr) {
    TemplArg* arg;
    TemplClassInst* inst;
    ENode* newExpr;
    NameSpaceObjectList* nsObjectList;
    Type* type;
    NameResult pr;
    TStreamElement* saved;
    UInt32 qual;

    if (!CTemplTool_IsTemplateArgumentDependentExpression(expr)) {
#if MWCCPSP
        newExpr = CEXPR_COPYENODE(expr);
#else
        newExpr = lalloc(sizeof(ENode));
        *newExpr = *expr;
#endif
        return newExpr;
    }

    switch (expr->type) {
        case ETEMPLDEP:
            switch (expr->data.templdep.subtype) {
                case TDE_PARAM:
                    if (deduce->processingArgument &&
                        expr->data.templdep.u.pid.nindex == deduce->nindex) {
#if MWCCPSP
                        newExpr = CEXPR_COPYENODE(expr);
#else
                        newExpr = lalloc(sizeof(ENode));
                        *newExpr = *expr;
#endif
                        return newExpr;
                    }

                    for (arg = deduce->args; arg; arg = arg->next) {
                        if (arg->pid.index == expr->data.templdep.u.pid.index &&
                            arg->pid.nindex == expr->data.templdep.u.pid.nindex) {
                            CError_ASSERT(
                                1562, arg->pid.type == TPT_NONTYPE && arg->data.paramdecl.expr
                            );
#if MWCCPSP
                            newExpr = CEXPR_COPYENODE(arg->data.paramdecl.expr);
#else
                            newExpr = lalloc(sizeof(ENode));
                            *newExpr = *arg->data.paramdecl.expr;
#endif
                            return newExpr;
                        }
                    }

                    for (inst = deduce->inst; inst; inst = inst->parent) {
                        for (arg = inst->inst_args; arg; arg = arg->next) {
                            if (arg->pid.index == expr->data.templdep.u.pid.index &&
                                arg->pid.nindex == expr->data.templdep.u.pid.nindex) {
                                CError_ASSERT(
                                    1575, arg->pid.type == TPT_NONTYPE && arg->data.paramdecl.expr
                                );
#if MWCCPSP
                                newExpr = CEXPR_COPYENODE(arg->data.paramdecl.expr);
#else
                                newExpr = lalloc(sizeof(ENode));
                                *newExpr = *arg->data.paramdecl.expr;
#endif
                                return newExpr;
                            }
                        }
                    }

                    CError_FATAL(1582);

                case TDE_SIZEOF:
#if MWCCPSP
                    // 3.0.1: any type operator, on a type or an expression
                    if (expr->data.templdep.u.typeop.isexpr) {
                        newExpr = CTemplTool_DeduceExpr(
                            deduce, (ENode*)expr->data.templdep.u.typeop.operand
                        );
                        if (CTemplTool_IsTemplDepExpr(newExpr)) {
                            expr = CEXPR_COPYENODE(expr);
                            expr->data.templdep.u.typeop.operand = newExpr;
                            return expr;
                        }
                        return CExpr_TypeOpValue(expr->data.templdep.u.typeop.kind, NULL, newExpr);
                    }
                    qual = 0;
                    type = CTemplTool_DeduceTypeCopy(
                        deduce, TYPE(expr->data.templdep.u.typeop.operand), &qual
                    );
                    if (CTemplTool_IsTemplateArgumentDependentType(type)) {
                        newExpr = CEXPR_COPYENODE(expr);
                        newExpr->data.templdep.u.typeop.operand = type;
                        return newExpr;
                    }
                    CDecl_CompleteType(type);
                    return CExpr_TypeOpValue(expr->data.templdep.u.typeop.kind, type, NULL);
#endif
                    qual = 0;
                    type = CTemplTool_DeduceTypeCopy(
                        deduce, TYPE(expr->data.templdep.u.typeexpr.type), &qual
                    );
                    CDecl_CompleteType(type);

                    if (CTemplTool_IsTemplateArgumentDependentType(type)) {
#if MWCCPSP
                        newExpr = CEXPR_COPYENODE(expr);
#else
                        newExpr = lalloc(sizeof(ENode));
                        *newExpr = *expr;
#endif
                        newExpr->data.templdep.u.typeexpr.type = type;
                        return newExpr;
                    }

                    return intconstnode(CABI_GetSizeTType(), type->size);

                case TDE_ALIGNOF:
                    qual = 0;
                    type = CTemplTool_DeduceTypeCopy(
                        deduce, TYPE(expr->data.templdep.u.typeexpr.type), &qual
                    );
                    CDecl_CompleteType(type);

                    if (CTemplTool_IsTemplateArgumentDependentType(type)) {
#if MWCCPSP
                        newExpr = CEXPR_COPYENODE(expr);
#else
                        newExpr = lalloc(sizeof(ENode));
                        *newExpr = *expr;
#endif
                        newExpr->data.templdep.u.typeexpr.type = type;
                        return newExpr;
                    }

                    return intconstnode(CABI_GetSizeTType(), CMach_GetTypeAlign(type));

                case TDE_CAST:
                    qual = expr->data.templdep.u.cast.qual;
                    type =
                        CTemplTool_DeduceTypeCopy(deduce, expr->data.templdep.u.cast.type, &qual);
                    return CExpr_DoExplicitConversion(
                        type,
                        qual,
                        CTemplTool_DeduceExprList(deduce, expr->data.templdep.u.cast.args)
                    );

                case TDE_QUALNAME:
                    qual = 0;
                    type = CTemplTool_DeduceTypeCopy(
                        deduce, TYPE(expr->data.templdep.u.qual.type), &qual
                    );

                    if (IS_TYPE_CLASS(type)) {
                        CDecl_CompleteType(type);
                        if (CScope_FindQualifiedClassMember(
                                &pr, TYPE_CLASS(type), expr->data.templdep.u.qual.name
                            )) {
                            return pointer_generation(CExpr_MakeNameLookupResultExpr(&pr));
                        }

                        CError_Error(
                            ERR_NOT_STRUCT_UNION_CLASS_MEMBER, expr->data.templdep.u.qual.name->name
                        );
                    } else if (IS_TYPE_TEMPLATE(type) && !deduce->inst) {
#if MWCCPSP
                        newExpr = CEXPR_COPYENODE(expr);
#else
                        newExpr = lalloc(sizeof(ENode));
                        *newExpr = *expr;
#endif
                        newExpr->data.templdep.u.qual.type = TYPE_TEMPLATE(type);
                        return newExpr;
                    } else {
                        CError_Error(
                            ERR_ILLEGAL_USE_TEMPLATE_ARGUMENT_DEPENDENT_TYPE,
                            expr->data.templdep.u.qual.name->name
                        );
                    }

                    return nullnode();

                case TDE_OBJ:
                    type = CTemplTool_FindTemplateInstance(
                        deduce, TEMPL_CLASS(expr->data.templdep.u.obj->nspace->theclass)
                    );
                    CError_ASSERT(1651, type && IS_TYPE_CLASS(type));

                    nsObjectList = CScope_GetLocalObject(
                        TYPE_CLASS(type)->nspace, expr->data.templdep.u.obj->name
                    );
                    CError_ASSERT(1654, nsObjectList);

                    memclrw(&pr, sizeof(pr));
                    pr.object = nsObjectList->object;
                    return pointer_generation(CExpr_MakeNameLookupResultExpr(&pr));

                case TDE_SOURCEREF:
                    CError_LockErrorPos(expr->data.templdep.u.sourceref.token, &saved);
                    newExpr = CTemplTool_DeduceExpr(deduce, expr->data.templdep.u.sourceref.expr);
                    CError_UnlockErrorPos(&saved);
                    return newExpr;

                case TDE_ADDRESS_OF:
                    newExpr = CTemplTool_DeduceExpr(deduce, expr->data.templdep.u.monadic);
                    return getnodeaddress(newExpr, TRUE);

                default:
                    CError_FATAL(1671);
            }
        case EFUNCCALL:
            newExpr =
                CExpr_PointerGeneration(CTemplTool_DeduceExpr(deduce, expr->data.funccall.funcref));
            return CExpr_MakeFunctionCall(
                newExpr, CTemplTool_DeduceExprList(deduce, expr->data.funccall.args)
            );
        case ELOGNOT:
            return CExpr_New_ELOGNOT_Node(CTemplTool_DeduceExpr(deduce, expr->data.monadic));
        case EMONMIN:
            return CExpr_New_EMONMIN_Node(CTemplTool_DeduceExpr(deduce, expr->data.monadic));
        case EBINNOT:
            return CExpr_New_EBINNOT_Node(CTemplTool_DeduceExpr(deduce, expr->data.monadic));
        case EMUL:
        case EDIV:
        case EMODULO:
        case EADD:
        case ESUB:
        case ESHL:
        case ESHR:
        case ELESS:
        case EGREATER:
        case ELESSEQU:
        case EGREATEREQU:
        case EEQU:
        case ENOTEQU:
        case EAND:
        case EXOR:
        case EOR:
        case ELAND:
        case ELOR:
        case EROTL:
        case EROTR:
            return CExpr_NewDyadicNode(
                CTemplTool_DeduceExpr(deduce, expr->data.diadic.left),
                expr->type,
                CTemplTool_DeduceExpr(deduce, expr->data.diadic.right)
            );
        case ECOND:
            return CExpr_New_ECOND_Node(
                CTemplTool_DeduceExpr(deduce, expr->data.cond.cond),
                CTemplTool_DeduceExpr(deduce, expr->data.cond.expr1),
                CTemplTool_DeduceExpr(deduce, expr->data.cond.expr2)
            );
        default:
            CError_FATAL(1727);
        case EINTCONST:
#if MWCCPSP
            newExpr = CEXPR_COPYENODE(expr);
#else
            newExpr = lalloc(sizeof(ENode));
            *newExpr = *expr;
#endif
            return newExpr;
    }
}

ENode* CTemplTool_DeduceDefaultArg(Object* func, ENode* expr) {
    TypeDeduce deduce;

    memclrw(&deduce, sizeof(deduce));
    CError_ASSERT(1747, IS_TYPE_FUNC(func->type));

    if (func->u.func.inst) {
        deduce.args = ((TemplFuncInstance*)func->u.func.inst)->args;
    }

    if ((TYPE_FUNC(func->type)->flags & FUNC_METHOD) &&
        (TYPE_METHOD(func->type)->theclass->flags & CLASS_IS_TEMPL_INST)) {
        deduce.inst = TEMPL_CLASS_INST(TYPE_METHOD(func->type)->theclass);
        deduce.tmclass = deduce.inst->templ;
    }

    return CTemplTool_DeduceExpr(&deduce, expr);
}

static TemplClass* CTemplTool_FindNestedTemplateInstance(TypeDeduce* deduce, TemplClass* templ) {
    TemplClass* dtempl;
    TemplClassInst* dinst;
    Type* type;

    if (templ->inst_parent) {
        return templ;
    }

    CError_ASSERT(1776, (dtempl = deduce->tmclass) && (dinst = deduce->inst));

    while (1) {
        if (templ->templ_parent == dtempl &&
            (type = CScope_GetLocalTagType(dinst->theclass.nspace, templ->theclass.classname)) &&
            IS_TYPE_CLASS(type) &&
            (TYPE_CLASS(type)->flags & CLASS_IS_TEMPL) &&
            TEMPL_CLASS(type)->templ_parent == templ->templ_parent) {
            return TEMPL_CLASS(type);
        }

        dtempl = dtempl->templ_parent;
        if (!dtempl) {
            break;
        }

        dinst = dinst->parent;
        CError_ASSERT(1790, dinst);
    }

    return templ;
}

static Type* CTemplTool_DeduceClassInstanceCopy(
    TypeDeduce* deduce, TemplClass* templ, TemplArg* args
) {
    TemplArg* arg;
    TemplArg* deducedArgs;
    TemplArg* last;
    TemplParam* param;
    UInt32 qual;
    Type* type;

    if (templ->templ_parent) {
        templ = CTemplTool_FindNestedTemplateInstance(deduce, templ);
    }

    arg = args;
    deducedArgs = NULL;
    param = templ->templ__params;

    while (arg) {
        if (deducedArgs) {
            last->next = galloc(sizeof(TemplArg));
            last = last->next;
        } else {
            last = galloc(sizeof(TemplArg));
            deducedArgs = last;
        }

        *last = *arg;

        if (!param || param->pid.type != last->pid.type) {
            CError_Error(ERR_TEMPLATE_PARAMETER_ARGUMENT_LIST_MISMATCH);
            return &stvoid;
        }

        last->pid = param->pid;
        param = param->next;

        switch (last->pid.type) {
            case TPT_TYPE:
                last->data.typeparam.type = CTemplTool_DeduceTypeCopy(
                    deduce, last->data.typeparam.type, &last->data.typeparam.qual
                );
                break;

            case TPT_NONTYPE:
                if (!last->data.paramdecl.expr) {
                    CError_FATAL(1873);
                } else if (CTemplTool_IsTemplateArgumentDependentExpression(
                               last->data.paramdecl.expr
                           )) {
                    last->data.paramdecl.expr = pointer_generation(
                        CTemplTool_DeduceExpr(deduce, last->data.paramdecl.expr)
                    );
                    last->data.paramdecl.expr =
                        CInline_CopyExpression(last->data.paramdecl.expr, CopyModeGlobal);
                }
                break;

            case TPT_TEMPLATE:
                qual = 0;
                last->data.typeparam.type =
                    CTemplTool_DeduceTypeCopy(deduce, last->data.typeparam.type, &qual);
                break;

            default:
                CError_FATAL(1891);
        }

        arg = arg->next;
    }

    for (arg = deducedArgs; arg; arg = arg->next) {
        switch (arg->pid.type) {
            case TPT_TYPE:
                if (CTemplTool_IsTemplateArgumentDependentType(arg->data.typeparam.type)) {
                    break;
                }
                continue;

            case TPT_NONTYPE:
                if (CTemplTool_IsTemplateArgumentDependentExpression(arg->data.paramdecl.expr)) {
                    break;
                }

                switch (arg->data.paramdecl.expr->type) {
                    case EINTCONST:
#if !MWCCPSP // not a 3.0.1 node kind
                    case EOBJLIST:
#endif
                    case EMEMBER:
                        break;
                    case EOBJREF:
                        if (CParser_HasInternalLinkage2(arg->data.paramdecl.expr->data.objref)) {
                            CError_Error(ERR_TEMPLATE_NON_TYPE_ARGUMENT_OBJECTS_SHALL);
                        }
                        break;
                    default:
                        CError_Error(ERR_ILLEGAL_NON_TYPE_TEMPLATE_ARGUMENT);
                        arg->data.paramdecl.expr = nullnode();
                        break;
                }
                continue;

            case TPT_TEMPLATE:
                if (!IS_TYPE_CLASS(arg->data.typeparam.type) &&
                    CTemplTool_IsTemplateArgumentDependentType(arg->data.typeparam.type)) {
                    break;
                }
                continue;

            default:
                CError_FATAL(1937);
        }

        break;
    }

    if (arg) {
        type = TYPE(CDecl_NewTemplDepType(TEMPLDEP_TEMPLATE));
        TYPE_TEMPLATE(type)->u.templ.templ = templ;
        TYPE_TEMPLATE(type)->u.templ.args = deducedArgs;
        return type;
    }

    if ((type = CTemplTool_IsDependentTemplate(templ, deducedArgs))) {
        return type;
    }

    return TYPE(CTemplClass_GetInstance(templ, deducedArgs, NULL));
}

static TemplArg* CTemplTool_FindTemplArg(TemplArg* args, TemplParamID pid) {
    while (args) {
        if (args->pid.index == pid.index && args->pid.nindex == pid.nindex) {
            CError_ASSERT(1984, pid.type == args->pid.type);
            return args;
        }
        args = args->next;
    }

    return NULL;
}

static TemplArg* CTemplTool_DeduceTemplArg(TypeDeduce* deduce, TemplParamID pid) {
    TemplClass* tmclass;
    TemplClassInst* inst;
    TemplArg* arg;

    if ((arg = CTemplTool_FindTemplArg(deduce->args, pid))) {
        return arg;
    }

    tmclass = deduce->tmclass;
    CError_ASSERT(2008, tmclass);

    inst = deduce->inst;
    if (!inst) {
        CError_ASSERT(2011, tmclass->templ_parent && tmclass->inst_parent);
        inst = TEMPL_CLASS_INST(deduce->tmclass->inst_parent);
    }

    while (1) {
        if ((arg = CTemplTool_FindTemplArg(inst->inst_args, pid))) {
            return arg;
        }

        inst = inst->parent;
        CError_ASSERT(2022, inst);
    }
}

static Type* CTemplTool_DeduceArrayCopy(
    TypeDeduce* deduce, Type* type, ENode* index, UInt32* resultQual
) {
    if (CTemplTool_IsTemplateArgumentDependentType(type)) {
        type = CTemplTool_DeduceTypeCopy(deduce, type, resultQual);
    }

    index = CTemplTool_DeduceExpr(deduce, index);

    if (ENODE_IS(index, EINTCONST)) {
        if (CInt64_IsNegative(&index->data.intval)) {
            CError_Error(ERR_ILLEGAL_CONSTANT_EXPRESSION);
            index->data.intval = cint64_one;
        }

        if (!CDecl_CheckArrayIntegr(type)) {
            type = TYPE(&stsignedchar);
        }

        type = CDecl_NewArrayType(type, type->size * CInt64_GetULong(&index->data.intval));
    } else {
        if (!deduce->suppressErrors) {
            CError_Error(ERR_ILLEGAL_CONSTANT_EXPRESSION);
        }
    }

    return type;
}

static Type* CTemplTool_DeduceBitfieldCopy(
    TypeDeduce* deduce, Type* type, ENode* size, UInt32* resultQual
) {
    TypeBitfield* tbitfield;
    short sizeval;
    short maxsize;

    if (CTemplTool_IsTemplateArgumentDependentType(type)) {
        UInt32 qual = 0;
        type = CTemplTool_DeduceTypeCopy(deduce, type, &qual);
    }

    if (!IS_TYPE_INT_OR_ENUM(type)) {
        CError_Error(ERR_ILLEGAL_BITFIELD_DECLARATION);
        type = TYPE(&stunsignedint);
    }

    switch (type->size) {
        case 1:
            maxsize = 8;
            break;
        case 2:
            maxsize = 16;
            break;
        case 4:
            maxsize = 32;
            break;
        case 8:
            maxsize = 64;
            break;
        default:
            CError_Error(ERR_ILLEGAL_BITFIELD_DECLARATION);
            return type;
    }

    if (!ENODE_IS(size, EINTCONST)) {
        size = CTemplTool_DeduceExpr(deduce, size);
        if (!ENODE_IS(size, EINTCONST)) {
            CError_Error(ERR_ILLEGAL_CONSTANT_EXPRESSION);
            return type;
        }
    }

    sizeval = CInt64_GetULong(&size->data.intval);
    if (sizeval > maxsize || CInt64_IsNegative(&size->data.intval)) {
        CError_Error(ERR_ILLEGAL_BITFIELD_DECLARATION);
        sizeval = 1;
    }

    tbitfield = galloc(sizeof(TypeBitfield));
    memclrw(tbitfield, sizeof(TypeBitfield));

    tbitfield->type = TYPEBITFIELD;
    tbitfield->size = type->size;
    tbitfield->bitfieldtype = type;
    tbitfield->bitlength = sizeval;

    return TYPE(tbitfield);
}

static Type* CTemplTool_DeduceTemplDepType(
    TypeDeduce* deduce, TypeTemplDep* tdt, UInt32* resultQual
) {
    Type* type;
    UInt32 qual;

    qual = 0;

    if (deduce->processingClassTypes) {
        TemplArg* arg;

        type = CTemplTool_GetSelfRefTemplate(TYPE(tdt));
        if (type && type == TYPE(deduce->tmclass)) {
            return type;
        }

        switch (tdt->dtype) {
            case TEMPLDEP_ARGUMENT:
                return TYPE(tdt);

            case TEMPLDEP_QUALNAME:
                type = CTemplTool_DeduceTypeCopy(deduce, TYPE(tdt->u.qual.type), &qual);
                if (type == TYPE(tdt->u.qual.type)) {
                    return TYPE(tdt);
                }

                if (!IS_TYPE_CLASS(type)) {
                    TypeTemplDep* tdtCopy;
                    CError_ASSERT(2157, IS_TYPE_TEMPLATE(type));
                    tdtCopy = galloc(sizeof(TypeTemplDep));
                    *tdtCopy = *tdt;
                    tdtCopy->u.qual.type = TYPE_TEMPLATE(type);
                    return TYPE(tdtCopy);
                } else if ((type = CScope_GetType(
                                TYPE_CLASS(type)->nspace, tdt->u.qual.name, resultQual
                            ))) {
                    return type;
                } else {
                    CError_Error(ERR_NOT_STRUCT_UNION_CLASS_MEMBER, tdt->u.qual.name->name);
                    return TYPE(tdt);
                }

            case TEMPLDEP_TEMPLATE:
                for (arg = tdt->u.templ.args; arg; arg = arg->next) {
                    if (arg->pid.type == TPT_TYPE) {
                        arg->data.typeparam.type = CTemplTool_DeduceTypeCopy(
                            deduce, arg->data.typeparam.type, &arg->data.typeparam.qual
                        );
                    }
                }
                return TYPE(tdt);

            case TEMPLDEP_ARRAY:
                tdt->u.array.type = CTemplTool_DeduceTypeCopy(deduce, tdt->u.array.type, &qual);
                return TYPE(tdt);

            case TEMPLDEP_QUALTEMPL:
                tdt->u.qualtempl.type = TYPE_TEMPLATE(
                    CTemplTool_DeduceTemplDepType(deduce, tdt->u.qualtempl.type, &qual)
                );
                for (arg = tdt->u.qualtempl.args; arg; arg = arg->next) {
                    if (arg->pid.type == TPT_TYPE) {
                        arg->data.typeparam.type = CTemplTool_DeduceTypeCopy(
                            deduce, arg->data.typeparam.type, &arg->data.typeparam.qual
                        );
                    }
                }
                return TYPE(tdt);

            case TEMPLDEP_BITFIELD:
                tdt->u.bitfield.type =
                    CTemplTool_DeduceTypeCopy(deduce, tdt->u.bitfield.type, &qual);
                return TYPE(tdt);
        }
    } else {
        switch (tdt->dtype) {
            case TEMPLDEP_ARGUMENT:
                if (deduce->processingArgument && tdt->u.pid.nindex == deduce->nindex) {
                    return TYPE(tdt);
                }

                {
                    TemplArg* arg = CTemplTool_DeduceTemplArg(deduce, tdt->u.pid);
                    if (arg->pid.type == TPT_TEMPLATE) {
                        CError_ASSERT(
                            2222,
                            IS_TYPE_CLASS(arg->data.typeparam.type) &&
                                (TYPE_CLASS(arg->data.typeparam.type)->flags & CLASS_IS_TEMPL)
                        );
                        *resultQual = arg->data.typeparam.qual;
                        return arg->data.typeparam.type;
                    }

                    CError_ASSERT(2226, arg->pid.type == TPT_TYPE);
                    *resultQual = arg->data.typeparam.qual;
                    return arg->data.typeparam.type;
                }

            case TEMPLDEP_QUALNAME:
                type = CTemplTool_DeduceTypeCopy(deduce, TYPE(tdt->u.qual.type), &qual);
                if (IS_TYPE_CLASS(type)) {
                    CDecl_CompleteType(type);
                    if ((type = CScope_GetType(
                             TYPE_CLASS(type)->nspace, tdt->u.qual.name, resultQual
                         ))) {
                        return type;
                    } else {
                        CError_Error(ERR_NOT_STRUCT_UNION_CLASS_MEMBER, tdt->u.qual.name->name);
                    }
                } else {
                    if ((deduce->processingArgument || !deduce->inst) && IS_TYPE_TEMPLATE(type)) {
                        TypeTemplDep* tdtCopy = galloc(sizeof(TypeTemplDep));
                        *tdtCopy = *tdt;
                        tdtCopy->u.qual.type = TYPE_TEMPLATE(type);
                        return TYPE(tdtCopy);
                    } else {
                        CError_Error(
                            ERR_ILLEGAL_USE_TEMPLATE_ARGUMENT_DEPENDENT_TYPE, tdt->u.qual.name->name
                        );
                    }
                }
                return TYPE(&stsignedint);

            case TEMPLDEP_TEMPLATE:
                return CTemplTool_DeduceClassInstanceCopy(
                    deduce, tdt->u.templ.templ, tdt->u.templ.args
                );

            case TEMPLDEP_ARRAY:
                return CTemplTool_DeduceArrayCopy(
                    deduce, tdt->u.array.type, tdt->u.array.index, resultQual
                );

            case TEMPLDEP_QUALTEMPL:
                type = CTemplTool_DeduceTypeCopy(deduce, TYPE(tdt->u.qualtempl.type), &qual);
                if (!(IS_TYPE_CLASS(type) && (TYPE_CLASS(type)->flags & CLASS_IS_TEMPL))) {
                    CError_Error(ERR_DECLARATION_SYNTAX_ERROR);
                    return TYPE(&stsignedint);
                }
                return CTemplTool_DeduceClassInstanceCopy(
                    deduce, TEMPL_CLASS(type), tdt->u.qualtempl.args
                );

            case TEMPLDEP_BITFIELD:
                return CTemplTool_DeduceBitfieldCopy(
                    deduce, tdt->u.bitfield.type, tdt->u.bitfield.size, resultQual
                );
        }
    }

    CError_FATAL(2275);
    return NULL;
}

static Type* CTemplTool_DeduceTypeQualCopy(TypeDeduce* deduce, Type* type, UInt32* resultQual) {
    Type* innerType;
    UInt32 qual;
    UInt32 innerQual;
    TypePointer* newPtr;

    qual = *resultQual;

    if (IS_TYPE_POINTER_ONLY(type) && IS_TYPE_TEMPLATE(TPTR_TARGET(type))) {
        innerQual = 0;
        innerType =
            CTemplTool_DeduceTemplDepType(deduce, TYPE_TEMPLATE(TPTR_TARGET(type)), &innerQual);

        newPtr = galloc(sizeof(TypePointer));
        *newPtr = *TYPE_POINTER(type);

        if (IS_TYPE_POINTER_ONLY(innerType)) {
            newPtr->target = galloc(sizeof(TypePointer));
            *TYPE_POINTER(newPtr->target) = *TYPE_POINTER(innerType);
            *resultQual = innerQual & (Q_CONST | Q_VOLATILE);
            TPTR_QUAL(newPtr->target) |= qual & (Q_CONST | Q_VOLATILE);
        } else if (IS_TYPE_MEMBERPOINTER(innerType)) {
            newPtr->target = galloc(sizeof(TypeMemberPointer));
            *TYPE_MEMBER_POINTER(newPtr->target) = *TYPE_MEMBER_POINTER(innerType);
            *resultQual = innerQual & (Q_CONST | Q_VOLATILE);
            TYPE_MEMBER_POINTER(newPtr->target)->qual |= qual & (Q_CONST | Q_VOLATILE);
        } else {
            newPtr->target = innerType;
            *resultQual = (qual | innerQual) & (Q_CONST | Q_VOLATILE);
        }

        return TYPE(newPtr);
    }

    return CTemplTool_DeduceTypeCopy(deduce, type, resultQual);
}

FuncArg* CTemplTool_DeduceArgCopy(TypeDeduce* deduce, FuncArg* args) {
    FuncArg* resultArgs;
    FuncArg* last;

    if (args == &oldstyle || args == &elipsis) {
        return args;
    }

    resultArgs = NULL;

    while (args) {
        if (args == &elipsis) {
            last->next = args;
            break;
        }

        if (resultArgs) {
            last->next = galloc(sizeof(FuncArg));
            last = last->next;
        } else {
            last = galloc(sizeof(FuncArg));
            resultArgs = last;
        }

        *last = *args;
        last->type = CTemplTool_DeduceTypeQualCopy(deduce, last->type, &last->qual);
        CanCreateObject(last->type);

        args = args->next;
    }

    return resultArgs;
}

static ExceptSpecList* CTemplTool_DeduceExSpecCopy(TypeDeduce* deduce, ExceptSpecList* exspec) {
    ExceptSpecList* copy;

    copy = galloc(sizeof(ExceptSpecList));
    *copy = *exspec;

    if (copy->type && CTemplTool_IsTemplateArgumentDependentType(copy->type)) {
        copy->type = CTemplTool_DeduceTypeCopy(deduce, copy->type, &copy->qual);
    }

    if (copy->next) {
        copy->next = CTemplTool_DeduceExSpecCopy(deduce, copy->next);
    }

    return copy;
}

Type* CTemplTool_DeduceTypeCopy(TypeDeduce* deduce, Type* type, UInt32* resultQual) {
    TemplClassInst* inst;
    Type* deduced;
    UInt32 qual2;
    UInt32 qual;

    switch (type->type) {
        case TYPETEMPLATE:
            qual = 0;
            deduced = CTemplTool_DeduceTemplDepType(deduce, TYPE_TEMPLATE(type), &qual);
            if (*resultQual & (Q_CONST | Q_VOLATILE)) {
                if (IS_TYPE_POINTER_ONLY(deduced)) {
                    TypePointer* newPtr = galloc(sizeof(TypePointer));
                    *newPtr = *TYPE_POINTER(deduced);
                    newPtr->qual |= *resultQual & (Q_CONST | Q_VOLATILE);
                    *resultQual &= ~(Q_CONST | Q_VOLATILE);
                    deduced = TYPE(newPtr);
                } else if (IS_TYPE_MEMBERPOINTER(deduced)) {
                    TypeMemberPointer* newPtr = galloc(sizeof(TypeMemberPointer));
                    *newPtr = *TYPE_MEMBER_POINTER(deduced);
                    newPtr->qual |= *resultQual & (Q_CONST | Q_VOLATILE);
                    *resultQual &= ~(Q_CONST | Q_VOLATILE);
                    deduced = TYPE(newPtr);
                }
            }
            *resultQual |= qual;
            return deduced;

        case TYPEVOID:
        case TYPEINT:
        case TYPEFLOAT:
        case TYPESTRUCT:
            return type;

        case TYPEENUM:
            if (TYPE_ENUM(type)->nspace->theclass &&
                (TYPE_ENUM(type)->nspace->theclass->flags & CLASS_IS_TEMPL) &&
                !deduce->processingClassTypes) {
                CError_ASSERT(2471, TYPE_ENUM(type)->enumname);
                inst = TEMPL_CLASS_INST(CTemplTool_FindTemplateInstance(
                    deduce, TEMPL_CLASS(TYPE_ENUM(type)->nspace->theclass)
                ));
                CError_ASSERT(2473, inst && inst->theclass.type == TYPECLASS);

                CDecl_CompleteType(TYPE(inst));
                type = CScope_GetLocalTagType(inst->theclass.nspace, TYPE_ENUM(type)->enumname);
                CError_ASSERT(2477, type);
                return type;
            }
            return type;

        case TYPECLASS:
            if (!deduce->processingClassTypes) {
                if (TYPE_CLASS(type)->flags & CLASS_IS_TEMPL) {
                    return CTemplTool_FindTemplateInstance(deduce, TEMPL_CLASS(type));
                }

                if (TYPE_CLASS(type)->nspace->theclass &&
                    (TYPE_CLASS(type)->nspace->theclass->flags & CLASS_IS_TEMPL)) {
                    CError_ASSERT(2492, deduce->inst);
                    CError_ASSERT(2493, TYPE_CLASS(type)->classname);

                    type = CScope_GetLocalTagType(
                        deduce->inst->theclass.nspace, TYPE_CLASS(type)->classname
                    );
                    CError_ASSERT(2496, type);
                    return type;
                }
            }

            return type;

        case TYPEARRAY: {
            SInt32 elements;

            elements = TPTR_TARGET(type)->size;
            if (elements > 0) {
                elements = type->size / elements;
            }

            deduced = galloc(sizeof(TypePointer));
            *TYPE_POINTER(deduced) = *TYPE_POINTER(type);
            TPTR_TARGET(deduced) = CTemplTool_DeduceTypeCopy(deduce, TPTR_TARGET(type), resultQual);

            do {
                type = TPTR_TARGET(type);
            } while (IS_TYPE_ARRAY(type));

            if (IS_TYPE_TEMPLATE(type)) {
                CDecl_CompleteType(TPTR_TARGET(deduced));
                deduced->size = TPTR_TARGET(deduced)->size * elements;
            }

            return deduced;
        }

        case TYPEPOINTER:
            deduced = galloc(sizeof(TypePointer));
            *TYPE_POINTER(deduced) = *TYPE_POINTER(type);
            TPTR_TARGET(deduced) = CTemplTool_DeduceTypeCopy(deduce, TPTR_TARGET(type), resultQual);
            return deduced;

        case TYPEBITFIELD:
            deduced = galloc(sizeof(TypeBitfield));
            *TYPE_BITFIELD(deduced) = *TYPE_BITFIELD(type);
            TYPE_BITFIELD(deduced)->bitfieldtype =
                CTemplTool_DeduceTypeCopy(deduce, TYPE_BITFIELD(type)->bitfieldtype, resultQual);
            return deduced;

        case TYPEMEMBERPOINTER:
            deduced = galloc(sizeof(TypeMemberPointer));
            *TYPE_MEMBER_POINTER(deduced) = *TYPE_MEMBER_POINTER(type);
            TYPE_MEMBER_POINTER(deduced)->ty1 =
                CTemplTool_DeduceTypeCopy(deduce, TYPE_MEMBER_POINTER(type)->ty1, resultQual);
            qual2 = 0;
            TYPE_MEMBER_POINTER(deduced)->ty2 =
                CTemplTool_DeduceTypeCopy(deduce, TYPE_MEMBER_POINTER(type)->ty2, &qual2);

            if (!IS_TYPE_CLASS(TYPE_MEMBER_POINTER(deduced)->ty2) &&
                !deduce->processingClassTypes &&
                !deduce->processingArgument &&
                !deduce->suppressErrors) {
                CError_Error(ERR_ILLEGAL_TEMPLATE_ARGUMENTS);
                return TYPE_MEMBER_POINTER(deduced)->ty1;
            }
            return deduced;

        case TYPEFUNC:
            if (TYPE_FUNC(type)->flags & FUNC_METHOD) {
                qual2 = 0;
                deduced = galloc(sizeof(TypeMemberFunc));
                *TYPE_METHOD(deduced) = *TYPE_METHOD(type);
                TYPE_METHOD(deduced)->funcid = 0;
                TYPE_METHOD(deduced)->theclass = TYPE_CLASS(
                    CTemplTool_DeduceTypeQualCopy(deduce, TYPE(TYPE_METHOD(type)->theclass), &qual2)
                );
                CError_ASSERT(2556, IS_TYPE_CLASS(TYPE_METHOD(deduced)->theclass));
            } else {
                deduced = galloc(sizeof(TypeFunc));
                *TYPE_FUNC(deduced) = *TYPE_FUNC(type);
            }

            TYPE_FUNC(deduced)->flags &= ~FUNC_IS_TEMPL;

            qual2 = TYPE_FUNC(type)->qual;
            TYPE_FUNC(deduced)->functype =
                CTemplTool_DeduceTypeQualCopy(deduce, TYPE_FUNC(type)->functype, &qual2);
            TYPE_FUNC(deduced)->qual = qual2;

            TYPE_FUNC(deduced)->args = CTemplTool_DeduceArgCopy(deduce, TYPE_FUNC(type)->args);
            if (TYPE_FUNC(type)->exspecs) {
                TYPE_FUNC(deduced)->exspecs =
                    CTemplTool_DeduceExSpecCopy(deduce, TYPE_FUNC(type)->exspecs);
            }

            CDecl_SetResultReg(TYPE_FUNC(deduced));
            return deduced;

        case TYPETEMPLDEPEXPR:
            CError_Error(ERR_UNIMPLEMENTED_C_FEATURE);
            return &stvoid;

        default:
            CError_FATAL(2580);
            return NULL;
    }
}

Type* CTemplTool_ResolveMemberSelfRefs(TemplClass* templ, Type* type, UInt32* resultQual) {
    TypeDeduce deduce;

    memclrw(&deduce, sizeof(deduce));
    deduce.tmclass = templ;
    deduce.processingClassTypes = TRUE;

    if (IS_TYPE_FUNC(type)) {
        TYPE_FUNC(type)->functype =
            CTemplTool_DeduceTypeCopy(&deduce, TYPE_FUNC(type)->functype, &TYPE_FUNC(type)->qual);
        TYPE_FUNC(type)->args = CTemplTool_DeduceArgCopy(&deduce, TYPE_FUNC(type)->args);
        CDecl_SetResultReg(TYPE_FUNC(type));
    } else {
        type = CTemplTool_DeduceTypeCopy(&deduce, type, resultQual);
    }

    return type;
}

Boolean CTemplTool_IsSameTemplateType(Type* a, Type* b) {
    return CTemplTool_GetSelfRefTemplate(b) == a;
}
#if MWCCPSP

// 3.0.1: a class as its template, checked (0x4AF5D0)
TemplClass* CTemplTool_GetTemplClass(TypeClass* tclass) {
    if (!tclass) {
        return NULL;
    }
    CError_ASSERT(118, tclass->flags & CLASS_IS_TEMPL);
    return (TemplClass*)tclass;
}

#if MWCCPS2
// b210 0x4BB2C0 (b219 0x4AF6A0, real name unknown): TemplClass and
// TemplClassInst embed a template header at +0x30 whose byte at +0xC marks a
// class template; this maps a header back to its class template
TemplClass* CTemplTool_TemplClassFromHeader(void* header) {
    if (!header) {
        return NULL;
    }
    CError_ASSERT(68, ((UInt8*)header)[0xC]);
    return (TemplClass*)((char*)header - 0x30);
}

// b210 0x4BB290 (b219 0x4AF670): an instance's header points at its class
// template's header
TemplClassInst* CTemplTool_TemplClassInstFromHeader(void* header) {
    if (!header) {
        return NULL;
    }
    CError_ASSERT(80, (*(UInt8**)header)[0xC]);
    return (TemplClassInst*)((char*)header - 0x30);
}
#endif

// 3.0.1: the template of a class template instance, checked (0x4AF4D0; the
// exe reaches it through a template header embedded in TemplClass)
#if MWCCPS2
inline static TemplClassInst* CTemplTool_GetTemplClassInst_inl(TypeClass* tclass) {
    if (!tclass) {
        return NULL;
    }
    CError_ASSERT(133, tclass->flags & CLASS_IS_TEMPL_INST);
    return (TemplClassInst*)tclass;
}

inline static TemplClass* CTemplTool_TemplClassFromHeader_inl(void* header) {
    if (!header) {
        return NULL;
    }
    CError_ASSERT(68, ((UInt8*)header)[0xC]);
    return (TemplClass*)((char*)header - 0x30);
}

// b210 0x4BB150 (b219 0x4AF530): a type as a class template instance, or NULL
TemplClassInst* func_004AF530(Type* type) {
    if (type == NULL) {
        return NULL;
    }
    if (IS_TYPE_CLASS(type) && (TYPE_CLASS(type)->flags & CLASS_IS_TEMPL_INST)) {
        return CTemplTool_GetTemplClassInst_inl(TYPE_CLASS(type));
    }
    return NULL;
}

// b210 0x4BB230 (b219 0x4AF610): checks that a header belongs to an instance
void* func_004AF610(void* header) {
    if (!header) {
        return NULL;
    }
    CError_ASSERT(104, !(*(UInt8**)header)[0xC]);
    return header;
}

// b210 0x4BB260 (b219 0x4AF640): checks that a header is not a class template's
void* func_004AF640(void* header) {
    if (!header) {
        return NULL;
    }
    CError_ASSERT(92, !((UInt8*)header)[0xC]);
    return header;
}

// b210 0x4BB2F0 (b219 0x4AF6D0): the nearest enclosing header that belongs to
// a class template instance, as its class
TemplClass* func_004AF6D0(void* header) {
    for (;;) {
        CError_ASSERT(51, header);
        if ((*(UInt8**)header)[0xC]) {
            return (TemplClass*)((char*)header - 0x30);
        }
        header = ((void**)header)[1];
    }
}

// b210: the instance's header points at its class template's header
TemplClass* CTemplTool_GetInstTemplate(TypeClass* tclass) {
    return CTemplTool_TemplClassFromHeader_inl(CTemplTool_GetTemplClassInst_inl(tclass)->parent);
}
#else
TemplClass* CTemplTool_GetInstTemplate(TypeClass* tclass) {
    CError_ASSERT(133, tclass->flags & CLASS_IS_TEMPL_INST);
    return TEMPL_CLASS_INST(tclass)->templ;
}
#endif

// 3.0.1: a class as its template instance, checked (0x4AF590)
TemplClassInst* CTemplTool_GetTemplClassInst(TypeClass* tclass) {
    if (!tclass) {
        return NULL;
    }
    CError_ASSERT(133, tclass->flags & CLASS_IS_TEMPL_INST);
    return (TemplClassInst*)tclass;
}

#endif
#if MWCCPSP
// 0x4AF260: the class being defined, if it is a class template
TemplClass* func_004AF260(void) {
    TypeClass* tclass;

    if (cscope_currentclass && (cscope_currentclass->flags & CLASS_IS_TEMPL)) {
        tclass = cscope_currentclass;
        CError_ASSERT(118, !tclass || (tclass->flags & CLASS_IS_TEMPL));
        return TEMPL_CLASS(tclass);
    }
    return NULL;
}
#endif
