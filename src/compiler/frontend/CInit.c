#include "compiler/CInit.h"
#if MWCCPSP
#include "compiler/Exceptions.h"
#include "compiler/scopes.h"
#endif
#include "compiler/CABI.h"
#include "compiler/CClass.h"
#include "compiler/CDecl.h"
#include "compiler/CError.h"
#include "compiler/CException.h"
#include "compiler/CExpr.h"
#include "compiler/CFunc.h"
#include "compiler/CInline.h"
#include "compiler/CInt64.h"
#include "compiler/CMachine.h"
#include "compiler/CodeGen.h"
#include "compiler/CompilerTools.h"
#include "compiler/CParser.h"
#include "compiler/CPrec.h"
#include "compiler/CPrep.h"
#include "compiler/CPrepTokenizer.h"
#include "compiler/CScope.h"
#include "compiler/enode.h"
#include "compiler/objects.h"
#include "compiler/ObjGen_PPC_EABI.h"
#include "compiler/tokens.h"
#include "compiler/types.h"
#include <string.h>

typedef void (*ExprCB)(Type* type, ENode* expr, Boolean flag);

typedef struct InitInfo {
    Object* obj;
    OLinkList* list;
    void* unused;
    SInt32 expr_offset;
    ExprCB expr_cb;
    Boolean expr_cb_called;
    Boolean has_runtime_init;
    Boolean use_register_cb;
    InitExprRegisterCB init_expr_register_cb;
    Object* auto_obj;
    InsertExprCB insert_expr_cb;
    RegisterObjectCB register_object_cb;
    char* buffer;
    SInt32 size;
    SInt32 bufferSize;
    struct InitInfo* next;
} InitInfo;

TempNodeCB cinit_tempnodefunc;
InitInfo* cinit_initinfo;
static PooledString* cinit_stringlist;
static PooledString* cinit_pooledstringlist;
static PooledString* cinit_pooledwstringlist;
static ObjectList* cinit_tentative;
static TypeClass* cinit_loop_class;
static ENodeList* cinit_fdtnode;
static Boolean cinit_fdtambig;

#if !MWCCPSP // 3.0.1: natural alignment (PooledString is 0x18 bytes)
#pragma pack(push, 2)
#endif
typedef struct CInit_NonConst {
    struct CInit_NonConst* next;
    Type* type;
    ENode* expr;
    SInt32 offset;
} CInit_NonConst;

typedef struct CInit_DataBuffer {
    struct CInit_DataBuffer* next;
    struct CInit_DataBuffer* root;
    char* buffer;
    SInt32 offset;
    SInt32 size;
    SInt32 bufferSize;
    SInt32 flexsize;
    CInit_NonConst* nonconst;
    OLinkList* list;
    Boolean allow_nonconst;
} CInit_DataBuffer;

typedef enum {
    StageStart,
    StageOpen,
    StageExpr,
    StageClose,
    StageEnd
} Stage;

typedef struct CInit_ParseState {
    ENode* expr;
    ENode myexpr;
    Stage stage;
    Boolean is_vector;
    SInt32 parens;
    Type* type;
} CInit_ParseState;
#if !MWCCPSP
#pragma pack(pop)
#endif

static void CInit_InitType(
    CInit_DataBuffer* db, CInit_ParseState* ps, Type* type, UInt32 qual, Boolean errorflag
);
static void CInit_Type(Type* type, UInt32 qual, Boolean isTopLevel);
#if MWCCPSP

#endif
#if MWCCPSP
extern struct CInitDeferredData* data_00624D38;
#endif

void CInit_Init(void) {
#if MWCCPSP
    data_00624D38 = NULL;
    cinit_stringlist = NULL;
    cinit_pooledstringlist = NULL;
    cinit_pooledwstringlist = NULL;
    cinit_tentative = NULL;
#else
    cinit_tempnodefunc = NULL;
    cinit_initinfo = NULL;
    cinit_stringlist = NULL;
    cinit_pooledstringlist = NULL;
    cinit_pooledwstringlist = NULL;
    cinit_tentative = NULL;
#endif
}

static void CInit_SetupInitInfo(InitInfo* info, Object* obj) {
    memclrw(info, sizeof(InitInfo));
    info->obj = obj;
    info->next = cinit_initinfo;
    cinit_initinfo = info;
}

static void CInit_CleanupInitInfo(InitInfo* info) {
    cinit_initinfo = info->next;
}

static void CInit_SetupInitInfoBuffer(Type* type) {
    SInt32 size = type->size;
    cinit_initinfo->size = size;

    if (!size) {
        size = 512;
    } else if (size & 1) {
        size++;
    }

    cinit_initinfo->buffer = lalloc(size);
    cinit_initinfo->bufferSize = size;
    memclrw(cinit_initinfo->buffer, size);
}

static void CInit_SetData(void* data, SInt32 offset, SInt32 size) {
    SInt32 end;
    char* buffer;

    end = offset + size;
    if (end > cinit_initinfo->size) {
        cinit_initinfo->size = end;
    }

    if (end > cinit_initinfo->bufferSize) {
        if (cinit_initinfo->obj->type->size == 0) {
            if (end < 8000) {
                end += 0x400;
            } else {
                end += 0x4000;
            }
        }
        if (end & 1) {
            end++;
        }

        buffer = lalloc(end);
        memclrw(buffer, end);
        memcpy(buffer, cinit_initinfo->buffer, cinit_initinfo->bufferSize);
        cinit_initinfo->buffer = buffer;
        cinit_initinfo->bufferSize = end;
    }

    if (data) {
        memcpy(cinit_initinfo->buffer + offset, data, size);
    }
}

typedef struct CInit_Initializer {
    struct CInit_Initializer* next;
    struct CInit_Initializer* sublist;
    ENode* expr;
    TStreamElement element;
} CInit_Initializer;

static CInit_Initializer* CInit_ParseInitializerList(void) {
    CInit_Initializer* head;
    CInit_Initializer* tail;
    CInit_Initializer* item;

    if ((tk = lex()) == '}') {
        return NULL;
    }

    head = NULL;
    do {
        if (head) {
            item = lalloc(sizeof(CInit_Initializer));
            tail->next = item;
            tail = item;
        }
        if (!head) {
            head = tail = lalloc(sizeof(CInit_Initializer));
        }
        tail->next = NULL;

        if (tk == '{') {
            tail->element = *CPrep_CurStreamElement();
            tail->sublist = CInit_ParseInitializerList();
            tail->expr = NULL;
            tk = lex();
        } else {
            tail->sublist = NULL;
            tail->expr = conv_assignment_expression();
            tail->element = *CPrep_CurStreamElement();
        }

        if (tk == '}') {
            return head;
        }

        if (tk != ',') {
            CError_Error(ERR_COMMA_EXPECTED);
            return head;
        }
    } while ((tk = lex()) != '}');

    return head;
}

static CInit_Initializer* CInit_ParseInitializerClause(void) {
    CInit_Initializer* init;

    init = lalloc(sizeof(CInit_Initializer));
    init->next = NULL;
    if (tk != '{') {
        init->sublist = NULL;
        init->expr = conv_assignment_expression();
        init->element = *CPrep_CurStreamElement();
    } else {
        init->element = *CPrep_CurStreamElement();
        init->expr = NULL;
        init->sublist = CInit_ParseInitializerList();
        tk = lex();
    }

    return init;
}

static ENode* CInit_ParseInitializer(ENode* expr) {
    CInt64 save_int;
    Float save_float;
    SInt32 save_size;
    short t;

    switch (tk) {
        case TK_INTCONST:
        case TK_FLOATCONST:
            save_int = tkintconst;
            save_float = tkfloatconst;
            save_size = tksize;
            t = lookahead();
            tkintconst = save_int;
            tkfloatconst = save_float;
            tksize = save_size;

            switch (t) {
                case ',':
                case ';':
                case '}':
#if MWCCPSP // 3.0.1 (0x50AD80): the initializer list parser passes NULL
                    if (!expr) {
                        expr = lalloc(sizeof(ENode));
                    }
#endif
                    memclrw(expr, sizeof(ENode));
                    switch (tk) {
                        case TK_INTCONST:
                            expr->type = EINTCONST;
                            expr->rtype = atomtype();
                            expr->data.intval = tkintconst;
                            break;
                        case TK_FLOATCONST:
                            expr->type = EFLOATCONST;
                            expr->rtype = atomtype();
                            expr->data.floatval = tkfloatconst;
                            break;
                    }
                    tk = lex();
                    CPrep_TokenStreamFlush();
                    return expr;
            }
    }

    expr = assignment_expression();
    CPrep_TokenStreamFlush();
    return expr;
}

static Stage CInit_ParseNextInit(CInit_ParseState* ps) {
    DeclInfo di;
    short t;

    ps->expr = NULL;
    if (tk == ';') {
        ps->stage = StageEnd;
        return ps->stage;
    }
    switch (ps->stage) {
        case StageStart:
            if (ps->is_vector) {
                if (tk == '(') {
                    tk = lex();
                    CParser_GetDeclSpecs(&di, TRUE);
                    ps->type = di.thetype;
                    if (tk == ')') {
                        tk = lex();
                    } else {
                        CError_Error(ERR_RPAREN_EXPECTED);
                    }

                    if (tk == '(') {
                        tk = lex();
                    } else {
                        CError_Error(ERR_LPAREN_EXPECTED);
                    }
                    ps->parens++;
                    t = lookahead();
                    if (t == TK_UU_VECTOR ||
                        (t == TK_IDENTIFIER && !strcmp("vector", tkidentifier->name))) {
                        CInit_ParseNextInit(ps);
                    }
                    ps->stage = StageOpen;
                    return ps->stage;
                }
            } else {
                if (tk == '{') {
                    tk = lex();
                    ps->parens = 0;
                    ps->stage = StageOpen;
                    return ps->stage;
                }
            }
            ps->expr = CInit_ParseInitializer(&ps->myexpr);
            ps->stage = StageExpr;
            return ps->stage;
        case StageOpen:
            break;
        case StageExpr:
        case StageClose:
            if (tk == ',') {
                tk = lex();
                break;
            }
            if (ps->parens) {
                if (tk != ')') {
                    CError_Error(ERR_ILLEGAL_INITIALIZATION);
                }
                if (ps->parens > 1) {
                    ps->parens--;
                    tk = lex();
                    CInit_ParseNextInit(ps);
                }
            } else {
                if (tk != '}') {
                    CError_Error(ERR_ILLEGAL_INITIALIZATION);
                }
            }
            ps->stage = StageClose;
            return ps->stage;
        default:
            CError_FATAL(389);
    }

    switch (tk) {
        case '{':
            tk = lex();
            ps->stage = StageOpen;
            return ps->stage;
        case '}':
            ps->stage = StageClose;
            return ps->stage;
        case '(':
            if (ps->is_vector) {
                tk = lex();
                ps->stage = StageOpen;
                return ps->stage;
            }
        case ')':
            if (ps->is_vector && ps->parens) {
                if (ps->parens > 1) {
                    ps->parens--;
                    tk = lex();
                    CInit_ParseNextInit(ps);
                }
                ps->stage = StageClose;
                return ps->stage;
            }
        default:
            ps->expr = CInit_ParseInitializer(&ps->myexpr);
            ps->stage = StageExpr;
            return ps->stage;
    }
}

static void CInit_CloseInitList(void) {
    if (tk == ',' && copts.cplusplus) {
        tk = lex();
    }

    if (tk != '}') {
        CError_ErrorSkip(ERR_RBRACE_EXPECTED);
    } else {
        tk = lex();
    }
}

static Boolean CInit_IsAllZero(char* buf, SInt32 size) {
    SInt32 i;

    if (copts.explicit_zero_data) {
        return FALSE;
    }

    for (i = 0; i < size; i++) {
        if (buf[i]) {
            return FALSE;
        }
    }

    return TRUE;
}

static Boolean CInit_ClassNeedsConstruction(TypeClass* tclass) {
    return CClass_Constructor(tclass) || CClass_Destructor(tclass);
}

static Boolean CInit_IsSimpleStructArrayInit(Type* type) {
    switch (type->type) {
        case TYPESTRUCT:
            return TRUE;
        case TYPEARRAY:
            while (IS_TYPE_ARRAY(type)) {
                type = TYPE_POINTER(type)->target;
            }
            if (!IS_TYPE_CLASS(type)) {
                return TRUE;
            }
        case TYPECLASS:
            return !CInit_ClassNeedsConstruction(TYPE_CLASS(type));
        default:
            return FALSE;
    }
}

static Boolean CInit_IsSimpleInit(Type* type) {
    switch (type->type) {
        case TYPEPOINTER:
            return (TYPE_POINTER(type)->qual & Q_REFERENCE) == 0;
        case TYPEARRAY:
            while (IS_TYPE_ARRAY(type)) {
                type = TYPE_POINTER(type)->target;
            }
            if (!IS_TYPE_CLASS(type)) {
                return TRUE;
            }
        case TYPECLASS:
            return !CInit_ClassNeedsConstruction(TYPE_CLASS(type));
        default:
            return TRUE;
    }
}

static Object* CInit_GetInitObject(Object* obj) {
    if (obj->datatype == DALIAS) {
#if MWCCPSP
        CError_ASSERT(928, !obj->u.alias.offset);
#else
        CError_ASSERT(521, !obj->u.alias.offset);
#endif
        obj = obj->u.alias.object;
    }
    return obj;
}

static Object* CInit_CreateStaticDataObject(Type* type, UInt32 qual, HashNameNode* name) {
    Object* obj;
    DeclInfo di;

    memclrw(&di, sizeof(DeclInfo));
    di.thetype = type;
    di.name = name ? name : CParser_GetUniqueName();
    di.qual = qual;
    di.storageclass = TK_STATIC;
    di.is_extern_c = TRUE;

    obj = CParser_NewGlobalDataObject(&di);
    obj->nspace = cscope_root;
    return obj;
}

static Type* CInit_GetRegMemType(void) {
    return CDecl_NewStructType(void_ptr.size * 3, CMach_GetTypeAlign(TYPE(&void_ptr)));
}

static Object* CInit_CreateStaticData(Type* type) {
    Object* obj = CInit_CreateStaticDataObject(type, 0, NULL);
    CodeGen_UpdateObject(obj);
    CInit_DeclareData(obj, NULL, NULL, obj->type->size);
    return obj;
}

static void CInit_InitNonConst(CInit_DataBuffer* db, Type* type, ENode* expr) {
    CInit_NonConst* entry;
    CInit_NonConst* scan;
    MWVector128* vec;

#if MWCCPSP // no vector types in 3.0.1
    if (db->root->allow_nonconst) {
#else
    if (db->root->allow_nonconst || IS_TYPE_VECTOR(type)) {
        if (IS_TYPE_VECTOR(type) && ENODE_IS(expr, EVECTOR128CONST)) {
            vec = (MWVector128*)(db->buffer + db->size);
            *vec = expr->data.vector128val;
            CMach_InitVectorMem(type, *vec, vec, TRUE);
        }
#endif

        entry = lalloc(sizeof(CInit_NonConst));
        memclrw(entry, sizeof(CInit_NonConst));
        entry->next = NULL;
        entry->type = type;
        entry->expr = expr;
        entry->offset = db->offset + db->size;
        if ((scan = db->root->nonconst)) {
            while (scan->next) {
                scan = scan->next;
            }
            scan->next = entry;
        } else {
            db->root->nonconst = entry;
        }
    } else {
        CError_Error(ERR_ILLEGAL_CONSTANT_EXPRESSION);
    }
}

static CInit_DataBuffer* CInit_GrowBuffer(CInit_DataBuffer* db, SInt32 size) {
    CInit_DataBuffer* newbuf;

    newbuf = lalloc(sizeof(CInit_DataBuffer));
    memclrw(newbuf, sizeof(CInit_DataBuffer));
    newbuf->root = db->root;
    newbuf->buffer = lalloc(size);
    newbuf->offset = db->offset + db->size;
    newbuf->bufferSize = size;
    db->next = newbuf;
    memset(newbuf->buffer, 0, newbuf->bufferSize);
    return newbuf;
}

Boolean CInit_RelocInitCheck(ENode* expr, Object** objptr, CInt64* valptr, Boolean allowLocal) {
    Object* objcheck1;
    Object* objcheck2;
    CInt64 valcheck1;
    CInt64 valcheck2;

    *objptr = NULL;
#if MWCCPSP
    CInt64_SetLong(valptr, 0);
#else
    valptr->lo = 0;
    valptr->hi = 0;
#endif

    while (1) {
        switch (expr->type) {
            case EINTCONST:
                *valptr = expr->data.intval;
                return TRUE;
            case EOBJREF:
                objcheck1 = CInit_GetInitObject(expr->data.objref);
                if (objcheck1->datatype == DLOCAL && !allowLocal) {
                    return FALSE;
                }
                *objptr = objcheck1;
                return TRUE;
            case ESTRINGCONST:
                CInit_RewriteString(expr, FALSE);
                continue;
            case ETYPCON:
                do {
                    if (expr->rtype->size != expr->data.monadic->rtype->size) {
                        return FALSE;
                    }
                    expr = expr->data.monadic;
                    if (!IS_TYPE_POINTER_ONLY(expr->rtype) && !IS_TYPE_INT(expr->rtype)) {
                        return FALSE;
                    }
                } while (ENODE_IS(expr, ETYPCON));
                continue;
            case EADD:
                if (!CInit_RelocInitCheck(
                        expr->data.diadic.left, &objcheck1, &valcheck1, allowLocal
                    )) {
                    return FALSE;
                }
                if (!CInit_RelocInitCheck(
                        expr->data.diadic.right, &objcheck2, &valcheck2, allowLocal
                    )) {
                    return FALSE;
                }

                if (objcheck1) {
                    if (objcheck2) {
                        return FALSE;
                    }
                    *objptr = objcheck1;
                } else {
                    *objptr = objcheck1;
                }

#if MWCCPSP
                *valptr = CMach_CalcIntDiadic(TYPE_UNSIGNED_OFFSET, valcheck1, '+', valcheck2);
#else
                *valptr = CMach_CalcIntDiadic(TYPE(&stunsignedlong), valcheck1, '+', valcheck2);
#endif
                return TRUE;
            case ESUB:
                if (!CInit_RelocInitCheck(
                        expr->data.diadic.left, &objcheck1, &valcheck1, allowLocal
                    )) {
                    return FALSE;
                }
                if (!CInit_RelocInitCheck(
                        expr->data.diadic.right, &objcheck2, &valcheck2, allowLocal
                    )) {
                    return FALSE;
                }

                if (objcheck2) {
                    return FALSE;
                }

                *objptr = objcheck1;
#if MWCCPSP
                *valptr = CMach_CalcIntDiadic(TYPE_UNSIGNED_OFFSET, valcheck1, '-', valcheck2);
#else
                *valptr = CMach_CalcIntDiadic(TYPE(&stunsignedlong), valcheck1, '-', valcheck2);
#endif
                return TRUE;
            default:
                return FALSE;
        }
    }
}

static void CInit_InitTypePointer(
    CInit_DataBuffer* db, ENode* expr, TypePointer* tptr, UInt32 qual
) {
    Object* obj;
    CInt64 val;
    OLinkList* list;

    expr = CExpr_AssignmentPromotion(expr, TYPE(tptr), qual & (Q_CONST | Q_VOLATILE), TRUE);
    if (IS_TYPE_POINTER_ONLY(expr->rtype) || ENODE_IS(expr, EINTCONST)) {
        if (CInit_RelocInitCheck(expr, &obj, &val, FALSE)) {
            if (obj) {
                list = lalloc(sizeof(OLinkList));
                list->next = db->root->list;
                list->obj = obj;
                list->addend = CInt64_GetULong(&val);
                list->offset = db->offset + db->size;
                db->root->list = list;
            } else {
#if MWCCPSP
                CMach_InitIntMem(TYPE(&stunsignedint), val, db->buffer + db->size);
#else
                CMach_InitIntMem(TYPE(&stunsignedlong), val, db->buffer + db->size);
#endif
            }
        } else {
            CInit_InitNonConst(db, TYPE(tptr), expr);
        }
    } else {
        CError_Error(ERR_ILLEGAL_INITIALIZATION);
    }
}

static void CInit_InitTypeInt(CInit_DataBuffer* db, ENode* expr, TypeIntegral* tint, UInt32 qual) {
    expr = CExpr_AssignmentPromotion(expr, TYPE(tint), qual & (Q_CONST | Q_VOLATILE), TRUE);
    if (IS_TYPE_INT(expr->rtype)) {
        if (ENODE_IS(expr, EINTCONST)) {
            CMach_InitIntMem(TYPE(tint), expr->data.intval, db->buffer + db->size);
        } else if (ENODE_IS(expr, ETYPCON) &&
                   IS_TYPE_POINTER_ONLY(expr->data.monadic->rtype) &&
                   expr->rtype->size == 4 &&
                   (copts.cplusplus || !copts.ANSIstrict)) {
#if MWCCPSP // 3.0.1: the operand's own qualifiers
            CInit_InitTypePointer(
                db,
                expr->data.monadic,
                TYPE_POINTER(expr->data.monadic->rtype),
                ENODE_QUALS(expr->data.monadic)
            );
#else
            CInit_InitTypePointer(
                db, expr->data.monadic, TYPE_POINTER(expr->data.monadic->rtype), qual
            );
#endif
        } else {
            CInit_InitNonConst(db, TYPE(tint), expr);
        }
    } else {
        CError_Error(ERR_ILLEGAL_INITIALIZATION);
    }
}

static void CInit_InitTypeFloat(CInit_DataBuffer* s, ENode* expr, TypeIntegral* tint, UInt32 qual) {
    expr = CExpr_AssignmentPromotion(expr, TYPE(tint), qual & (Q_CONST | Q_VOLATILE), TRUE);
    if (IS_TYPE_FLOAT(expr->rtype)) {
        if (ENODE_IS(expr, EFLOATCONST)) {
            CMach_InitFloatMem(TYPE(tint), expr->data.floatval, s->buffer + s->size);
        } else {
            CInit_InitNonConst(s, TYPE(tint), expr);
        }
    } else {
        CError_Error(ERR_ILLEGAL_INITIALIZATION);
    }
}

static void CInit_InitTypeEnum(CInit_DataBuffer* db, ENode* expr, TypeEnum* tenum, UInt32 qual) {
    expr = CExpr_AssignmentPromotion(expr, TYPE(tenum), qual & (Q_CONST | Q_VOLATILE), TRUE);
    if (IS_TYPE_ENUM(expr->rtype)) {
        if (ENODE_IS(expr, EINTCONST)) {
            CMach_InitIntMem(tenum->enumtype, expr->data.intval, db->buffer + db->size);
        } else {
            CInit_InitNonConst(db, TYPE(tenum), expr);
        }
    } else {
        CError_Error(ERR_ILLEGAL_INITIALIZATION);
    }
}

static void CInit_InitTypeMemberPointer(
    CInit_DataBuffer* db, ENode* expr, TypeMemberPointer* tmptr, UInt32 qual
) {
    expr = CExpr_AssignmentPromotion(expr, TYPE(tmptr), qual & (Q_CONST | Q_VOLATILE), TRUE);
    if (ENODE_IS(expr, EINTCONST)) {
        CMach_InitIntMem(TYPE(&stsignedlong), expr->data.intval, db->buffer + db->size);
    } else {
        CInit_InitNonConst(db, TYPE(tmptr), expr);
    }
}

static void CInit_SetBitfield(TypeBitfield* tbitfield, UInt8* buffer, CInt64 val) {
    int i;
    int pos;
    int step;

    if (copts.littleendian) {
        pos = tbitfield->offset;
        step = 1;
    } else {
        pos = tbitfield->bitlength + tbitfield->offset - 1;
        step = -1;
    }
    for (i = 0; i < tbitfield->bitlength; i++, pos += step) {
        if (CInt64_GetULong(&val) & 1) {
            if (copts.littleendian) {
                buffer[pos >> 3] |= 1 << (pos & 7);
            } else {
                buffer[pos >> 3] |= 0x80 >> (pos & 7);
            }
        }
        val = CInt64_ShrU(val, cint64_one);
    }
}

static void CInit_InitTypeBitfield(
    CInit_DataBuffer* db, ENode* expr, TypeBitfield* tbitfield, UInt32 qual
) {
    Type* inner;

    inner = tbitfield->bitfieldtype;
    if (IS_TYPE_ENUM(inner)) {
        inner = TYPE_ENUM(inner)->enumtype;
    }
    expr = CExpr_AssignmentPromotion(expr, inner, qual & (Q_CONST | Q_VOLATILE), TRUE);

    if (IS_TYPE_INT(expr->rtype)) {
        if (ENODE_IS(expr, EINTCONST)) {
            CInit_SetBitfield(tbitfield, (UInt8*)db->buffer + db->size, expr->data.intval);
        } else {
            CInit_InitNonConst(db, TYPE(tbitfield), expr);
        }
    } else {
        CError_Error(ERR_ILLEGAL_INITIALIZATION);
    }
}

static void CInit_InitTypeArray(
    CInit_DataBuffer* db, CInit_ParseState* ps, TypePointer* tptr, UInt32 qual, Boolean errorflag
) {
    SInt32 targetsize;
    SInt32 start;
    SInt32 i;
    Boolean braced;
    Boolean is_zero_size;
    SInt32 size;
    Boolean is_char_ptr;
    Boolean is_wchar_ptr;

    is_zero_size = tptr->size == 0;
    if (!(targetsize = tptr->target->size)) {
        CError_Error(ERR_DATA_TYPE_INCOMPLETE);
        return;
    }

    is_char_ptr = IS_TYPE_INT(tptr->target) && (tptr->target->size == 1);
    is_wchar_ptr = IS_TYPE_INT(tptr->target) && (tptr->target->size == stwchar.size);
    switch (ps->stage) {
        case StageOpen:
            braced = TRUE;
            if (CInit_ParseNextInit(ps) == StageClose) {
                if (is_zero_size) {
                    CError_Error(ERR_ILLEGAL_INITIALIZATION);
                }
                tk = lex();
                return;
            }
            break;
        case StageExpr:
            braced = FALSE;
            break;
        default:
            break;
    }
    switch (ps->stage) {
        case StageOpen:
        case StageExpr:
            break;
        default:
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            return;
    }

    if (ps->stage == StageExpr) {
        ps->expr = pointer_generation(ps->expr);
    }

    if (ps->stage == StageExpr &&
        ENODE_IS(ps->expr, ESTRINGCONST) &&
        (is_char_ptr || is_wchar_ptr)) {
        if (IS_TYPE_POINTER_ONLY(ps->expr->rtype) &&
            tptr->target->size != TYPE_POINTER(ps->expr->rtype)->target->size) {
            CError_Warning(ERR_ILLEGAL_INITIALIZATION);
        }
        size = ps->expr->data.string.size;
        if (is_zero_size) {
            tptr->size = ps->expr->data.string.size;
            if (db->bufferSize < size) {
                db = CInit_GrowBuffer(db, size);
            }
            memcpy(db->buffer, ps->expr->data.string.data, size);
            db->size = size;
        } else {
            if (ps->expr->data.string.size > tptr->size) {
                if (copts.cplusplus || (ps->expr->data.string.size - 1) > tptr->size) {
                    CError_Error(ERR_TOO_MANY_INITIALIZERS);
                }
                ps->expr->data.string.size = tptr->size;
                size = tptr->size;
            }
            memcpy(db->buffer + db->size, ps->expr->data.string.data, size);
        }
    } else {
        if (!braced && errorflag) {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            return;
        }

        start = db->size;
        i = 0;
        while (1) {
            if (is_zero_size) {
                size = (i + 1) * targetsize;
                db->size = start + size - targetsize - db->offset;
                if (db->size + targetsize > db->bufferSize) {
                    db = CInit_GrowBuffer(db, targetsize * 16);
                }
                CInit_InitType(db, ps, tptr->target, qual, FALSE);
                tptr->size = size;
                db->size = start + size - db->offset;
            } else {
                if (tptr->size <= i * targetsize) {
                    i--;
                    CError_Error(ERR_TOO_MANY_INITIALIZERS);
                }
                db->size = start + i * targetsize;
                CInit_InitType(db, ps, tptr->target, qual, FALSE);
                if (!braced && tptr->size <= (i + 1) * targetsize) {
                    break;
                }
            }

            switch (CInit_ParseNextInit(ps)) {
                case StageOpen:
                case StageExpr:
                    break;
                case StageClose:
                    if (braced) {
                        tk = lex();
                    }
                    return;
                default:
                    CError_Error(ERR_RBRACE_EXPECTED);
                    return;
            }

            i++;
        }
    }

    if (braced) {
        switch (CInit_ParseNextInit(ps)) {
            case StageClose:
                tk = lex();
                return;
            case StageExpr:
                CError_Error(ERR_TOO_MANY_INITIALIZERS);
                return;
            default:
                CError_Error(ERR_RBRACE_EXPECTED);
                return;
        }
    }
}

static void CInit_InitTypeStruct(
    CInit_DataBuffer* db,
    CInit_ParseState* ps,
    const TypeStruct* tstruct,
    UInt32 qual,
    Boolean errorflag
) {
    StructMember* member;
    SInt32 start;
    Boolean braced;
    SInt32 count;
    TypePointer arraytype;
    int i;

    count = 0;
    if (ps->type) {
        tstruct = TYPE_STRUCT(ps->type);
    }

    if (!(member = tstruct->members)) {
        CError_Error(ERR_DATA_TYPE_INCOMPLETE);
        return;
    }

    switch (ps->stage) {
        case StageOpen:
            braced = TRUE;
            if (CInit_ParseNextInit(ps) == StageClose) {
                tk = lex();
                return;
            }
            break;
        case StageExpr:
            braced = FALSE;
            break;
        default:
            break;
    }

    switch (ps->stage) {
        case StageOpen:
        case StageExpr:
            break;
        default:
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            return;
    }

    if (!braced && ps->stage == StageExpr && (errorflag || ps->expr->rtype == TYPE(tstruct))) {
        ps->expr = CExpr_AssignmentPromotion(ps->expr, TYPE(tstruct), qual, TRUE);
        if (IS_TYPE_STRUCT(ps->expr->rtype)) {
            CInit_InitNonConst(db, TYPE(tstruct), ps->expr);
        }
        return;
    }

    start = db->size;
    while (1) {
        db->size = start + member->offset;
        if (!member->type->size) {
            if (!errorflag || !IS_TYPE_ARRAY(member->type)) {
                CError_Error(ERR_TOO_MANY_INITIALIZERS);
                if (!IS_TYPE_ARRAY(member->type)) {
                    return;
                }
            }

            arraytype = *TYPE_POINTER(member->type);
            CInit_InitTypeArray(db, ps, &arraytype, member->qual, TRUE);
            db->flexsize = arraytype.size;
        } else {
            CInit_InitType(db, ps, member->type, member->qual, FALSE);
        }

        count++;
        if (IS_TYPESTRUCT_VECTOR(tstruct) && ps->expr) {
            CError_ASSERT(1218, !ENODE_IS(ps->expr, EVECTOR128CONST));
        }

        do {
            member = member->next;
        } while (member && (member->qual & Q_WEAK));

        if (!member || tstruct->stype == STRUCT_TYPE_UNION) {
            if (braced) {
                switch (CInit_ParseNextInit(ps)) {
                    case StageClose:
                        if (IS_TYPESTRUCT_VECTOR(tstruct)) {
                            CMach_InitVectorMem(
                                TYPE(tstruct),
                                *(MWVector128*)(db->buffer + start),
                                (MWVector128*)(db->buffer + start),
                                FALSE
                            );
                        }
                        tk = lex();
                        return;
                    case StageExpr:
                        CError_Error(ERR_TOO_MANY_INITIALIZERS);
                        return;
                    default:
                        CError_Error(ERR_RBRACE_EXPECTED);
                        return;
                }
            }
            return;
        } else {
            switch (CInit_ParseNextInit(ps)) {
                case StageOpen:
                case StageExpr:
                    continue;
                case StageClose:
                    if (braced) {
                        tk = lex();
                    }
                    if (IS_TYPESTRUCT_VECTOR(tstruct)) {
                        switch (TYPE_STRUCT(tstruct)->stype) {
                            case STRUCT_VECTOR_UCHAR:
                            case STRUCT_VECTOR_SCHAR:
                            case STRUCT_VECTOR_BCHAR:
                                if (count != 16) {
                                    if (count == 1) {
                                        UInt8 *p, val;
                                        p = (UInt8*)db->buffer;
                                        val = p[0];
                                        for (i = 1; i < 16; i++) {
                                            p[i] = val;
                                        }
                                    } else {
                                        CError_Error(ERR_ILLEGAL_INITIALIZATION);
                                    }
                                }
                                break;
                            case STRUCT_VECTOR_USHORT:
                            case STRUCT_VECTOR_SSHORT:
                            case STRUCT_VECTOR_BSHORT:
                            case STRUCT_VECTOR_PIXEL:
                                if (count != 8) {
                                    if (count == 1) {
                                        SInt16 *p, val;
                                        p = (SInt16*)db->buffer;
                                        val = p[0];
                                        for (i = 1; i < 8; i++) {
                                            p[i] = val;
                                        }
                                    } else {
                                        CError_Error(ERR_ILLEGAL_INITIALIZATION);
                                    }
                                }
                                break;
                            case STRUCT_VECTOR_UINT:
                            case STRUCT_VECTOR_SINT:
                            case STRUCT_VECTOR_BINT:
                            case STRUCT_VECTOR_FLOAT:
                                if (count != 4) {
                                    if (count == 1) {
                                        UInt32 *p, val;
                                        p = (UInt32*)db->buffer;
                                        val = p[0];
                                        for (i = 1; i < 4; i++) {
                                            p[i] = val;
                                        }
                                    } else {
                                        CError_Error(ERR_ILLEGAL_INITIALIZATION);
                                    }
                                }
                                break;
                        }
                    }
                    return;
                default:
                    CError_Error(ERR_ILLEGAL_INITIALIZATION);
                    return;
            }
        }
    }
}

static ObjMemberVar* CInit_FindNextMember(ObjMemberVar* ivar) {
    ObjMemberVar* scan = ivar;
    while (1) {
        scan = scan->next;
        if (!scan) {
            return NULL;
        }
        if (!scan->anonunion) {
            return scan;
        }
        if (scan->offset > ivar->offset) {
            return scan;
        }
        if (IS_TYPE_BITFIELD(scan->type) &&
            IS_TYPE_BITFIELD(ivar->type) &&
            TYPE_BITFIELD(scan->type)->offset != TYPE_BITFIELD(ivar->type)->offset) {
            return scan;
        }
    }
}

static void CInit_InitTypeClass(
    CInit_DataBuffer* db, CInit_ParseState* ps, TypeClass* tclass, UInt32 qual, Boolean errorflag
) {
    ObjMemberVar* ivar;
    SInt32 start;
    Boolean braced;
    UInt32 last_offset;
    TypePointer arraytype;

    if (tclass->bases || tclass->vtable) {
        CError_Error(ERR_ILLEGAL_INITIALIZATION);
        return;
    }

    switch (ps->stage) {
        case StageOpen:
            braced = TRUE;
            if (CInit_ParseNextInit(ps) == StageClose) {
                tk = lex();
                return;
            }
            break;
        case StageExpr:
            braced = FALSE;
            break;
        default:
            break;
    }

    switch (ps->stage) {
        case StageOpen:
        case StageExpr:
            break;
        default:
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            return;
    }

    if (!braced &&
        ps->stage == StageExpr &&
        (errorflag ||
         ps->expr->rtype == TYPE(tclass) ||
         CExpr_CanImplicitlyConvert(ps->expr, TYPE(tclass), 0))) {
        ps->expr = CExpr_AssignmentPromotion(ps->expr, TYPE(tclass), qual, TRUE);
        if (IS_TYPE_CLASS(ps->expr->rtype)) {
            CInit_InitNonConst(db, TYPE(tclass), ps->expr);
        }
        return;
    }

    for (ivar = tclass->ivars; ivar; ivar = ivar->next) {
        if (ivar->access != ACCESSPUBLIC) {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            break;
        }
    }

    if (!(ivar = tclass->ivars)) {
        CError_Error(ERR_TOO_MANY_INITIALIZERS);
        return;
    }
    start = db->size;
    while (1) {
        db->size = start + ivar->offset;
        if (!ivar->type->size) {
            if (!errorflag || !IS_TYPE_ARRAY(ivar->type)) {
                CError_Error(ERR_TOO_MANY_INITIALIZERS);
                if (!IS_TYPE_ARRAY(ivar->type)) {
                    return;
                }
            }

            arraytype = *TYPE_POINTER(ivar->type);
            CInit_InitTypeArray(db, ps, &arraytype, ivar->qual, TRUE);
            db->flexsize = arraytype.size;
        } else {
            CInit_InitType(db, ps, ivar->type, ivar->qual, FALSE);
        }

        last_offset = ivar->offset;
        if (!(ivar = CInit_FindNextMember(ivar)) ||
            (tclass->mode == CLASS_MODE_UNION && ivar->offset == last_offset)) {
            if (braced) {
                switch (CInit_ParseNextInit(ps)) {
                    case StageClose:
                        tk = lex();
                        return;
                    case StageExpr:
                        CError_Error(ERR_TOO_MANY_INITIALIZERS);
                        return;
                    default:
                        CError_Error(ERR_RBRACE_EXPECTED);
                        return;
                }
            }
            return;
        } else {
            switch (CInit_ParseNextInit(ps)) {
                case StageOpen:
                case StageExpr:
                    continue;
                case StageClose:
                    if (braced) {
                        tk = lex();
                    }
                    return;
                default:
                    CError_Error(ERR_ILLEGAL_INITIALIZATION);
                    return;
            }
        }
    }
}

static void CInit_InitType(
    CInit_DataBuffer* db, CInit_ParseState* ps, Type* type, UInt32 qual, Boolean errorflag
) {
    Boolean braced;

    switch (type->type) {
        case TYPEVOID:
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            return;
        case TYPEINT:
        case TYPEFLOAT:
        case TYPEENUM:
        case TYPEBITFIELD:
        case TYPEMEMBERPOINTER:
        case TYPEPOINTER:
            switch (ps->stage) {
                case StageOpen:
                    braced = TRUE;
                    CInit_ParseNextInit(ps);
                    break;
                case StageExpr:
                    braced = FALSE;
                    break;
                default:
                    break;
            }
            if (ps->stage != StageExpr) {
                CError_Error(ERR_ILLEGAL_INITIALIZATION);
                return;
            }

            switch (type->type) {
                case TYPEINT:
                    CInit_InitTypeInt(db, ps->expr, TYPE_INTEGRAL(type), qual);
                    break;
                case TYPEFLOAT:
                    CInit_InitTypeFloat(db, ps->expr, TYPE_INTEGRAL(type), qual);
                    break;
                case TYPEENUM:
                    CInit_InitTypeEnum(db, ps->expr, TYPE_ENUM(type), qual);
                    break;
                case TYPEPOINTER:
                    CInit_InitTypePointer(db, ps->expr, TYPE_POINTER(type), qual);
                    break;
                case TYPEMEMBERPOINTER:
                    CInit_InitTypeMemberPointer(db, ps->expr, TYPE_MEMBER_POINTER(type), qual);
                    break;
                case TYPEBITFIELD:
                    CInit_InitTypeBitfield(db, ps->expr, TYPE_BITFIELD(type), qual);
                    break;
                default:
                    CError_FATAL(1542);
            }

            if (braced) {
                switch (CInit_ParseNextInit(ps)) {
                    case StageClose:
                        tk = lex();
                        break;
                    case StageExpr:
                        CError_Error(ERR_TOO_MANY_INITIALIZERS);
                        break;
                    default:
                        CError_Error(ERR_RBRACE_EXPECTED);
                }
            }
            break;
        case TYPESTRUCT:
            CInit_InitTypeStruct(db, ps, TYPE_STRUCT(type), qual, errorflag);
            break;
        case TYPEARRAY:
            CInit_InitTypeArray(db, ps, TYPE_POINTER(type), qual, errorflag);
            break;
        case TYPECLASS:
            CInit_InitTypeClass(db, ps, TYPE_CLASS(type), qual, errorflag);
            break;
        default:
            CError_FATAL(1573);
    }
}

static void CInit_InitData(CInit_DataBuffer* db, Type* type, UInt32 qual, Boolean allowNonConst) {
    CInit_ParseState ps;
    SInt32 size;
    CInit_DataBuffer* chunk;
    char* joined;

    locklheap();
    memclrw(db, sizeof(CInit_DataBuffer));
    db->root = db;
    if (type->size == 0) {
        if (IS_TYPE_ARRAY(type)) {
            db->bufferSize = 16 * TYPE_POINTER(type)->target->size;
        } else {
            CError_Error(ERR_DATA_TYPE_INCOMPLETE);
        }
    } else {
        db->bufferSize = type->size;
    }

    db->buffer = lalloc(db->bufferSize);
    memset(db->buffer, 0, db->bufferSize);
    db->allow_nonconst = allowNonConst;

    ps.stage = StageStart;
    ps.is_vector = FALSE;
    if (IS_TYPE_VECTOR(type)) {
        ps.is_vector = TRUE;
        db->allow_nonconst = TRUE;
    }
    if (IS_TYPE_ARRAY(type) && IS_TYPE_VECTOR(TYPE_POINTER(type)->target)) {
        db->allow_nonconst = TRUE;
    }

    ps.type = NULL;
    ps.parens = 0;
    CInit_ParseNextInit(&ps);
    CInit_InitType(db, &ps, type, qual, TRUE);

    if ((size = type->size + db->flexsize)) {
        if (db->next) {
            joined = lalloc(size);
            for (chunk = db; chunk; chunk = chunk->next) {
                CError_ASSERT(1647, (chunk->offset + chunk->size) <= size);
                memcpy(joined + chunk->offset, chunk->buffer, chunk->size);
            }
            db->buffer = joined;
        }
    } else {
        CError_Error(ERR_ILLEGAL_INITIALIZATION);
    }

    db->size = size;
    db->next = NULL;
    unlocklheap();
}

static ENode* CInit_InitConcat(ENode* prev, ENode* base, SInt32 offset, Type* type, ENode* value) {
    ENode* assign;
    ENode* addr;
    ENode* lvalue;

#if MWCCPSP
    addr = CEXPR_COPYENODE(base);
#else
    addr = lalloc(sizeof(ENode));
    *addr = *base;
#endif
    if (offset) {
#if MWCCPSP
        addr = makediadicnode(addr, intconstnode(TYPE_UNSIGNED_OFFSET, offset), EADD);
#else
        addr = makediadicnode(addr, intconstnode(TYPE(&stunsignedlong), offset), EADD);
#endif
    }

    if (IS_TYPE_BITFIELD(type)) {
        lvalue = makemonadicnode(addr, EBITFIELD);
        lvalue->rtype = type;
        lvalue = makemonadicnode(lvalue, EINDIRECT);
        lvalue->rtype = TYPE_BITFIELD(type)->bitfieldtype;
    } else {
        lvalue = makemonadicnode(addr, EINDIRECT);
        lvalue->rtype = type;
    }

    assign = makediadicnode(lvalue, value, EASS);
    if (!prev) {
        return assign;
    } else {
        prev = makediadicnode(prev, assign, ECOMMA);
        prev->rtype = assign->rtype;
        return prev;
    }
}

static ENode* CInit_RegisterDtorObject(Type* type, Object* dtor, ENode* objexpr) {
    ENode* expr;

    if (copts.no_static_dtors) {
        return objexpr;
    }

    expr = lalloc(sizeof(ENode));
    expr->type = EFUNCCALL;
    expr->cost = 4;
    expr->flags = 0;
    expr->rtype = CDecl_NewPointerType(type);
    expr->data.funccall.funcref = create_objectrefnode(Xgreg_func);
    expr->data.funccall.functype = TYPE_FUNC(Xgreg_func->type);
    expr->data.funccall.args = lalloc(sizeof(ENodeList));
    expr->data.funccall.args->node = objexpr;
    expr->data.funccall.args->next = lalloc(sizeof(ENodeList));
    expr->data.funccall.args->next->node =
        create_objectrefnode(CABI_GetDestructorObject(dtor, CABIDestroyComplete));
    expr->data.funccall.args->next->next = lalloc(sizeof(ENodeList));
    expr->data.funccall.args->next->next->node =
        create_objectrefnode(CInit_CreateStaticData(CInit_GetRegMemType()));
    expr->data.funccall.args->next->next->next = NULL;

    return expr;
}

static Boolean CInit_ConstructGlobalObject(
    Object* obj, TypeClass* tclass, ENode* valueexpr, SInt32 offset, Boolean parseInit
) {
    NameSpaceObjectList* ctor;
    Object* dtor;
    ENodeList* list;
    ENode* expr;
    Boolean ctorflag;

    ctor = CClass_Constructor(tclass);
    dtor = CClass_Destructor(tclass);
    if (!ctor && !dtor) {
        return FALSE;
    }

    if (parseInit && !ctor && tk == '=' && lookahead() == '{') {
        return FALSE;
    }

    if (parseInit && tk == '(') {
        tk = lex();
        list = CExpr_ScanExpressionList(TRUE);
        if (tk == ')') {
            tk = lex();
        } else {
            CError_Error(ERR_RPAREN_EXPECTED);
        }
    } else if (valueexpr) {
        list = lalloc(sizeof(ENodeList));
        list->node = valueexpr;
        list->next = NULL;
    } else {
        list = NULL;
    }

    expr = create_objectrefnode(obj);
    if (offset) {
#if MWCCPSP
        expr = makediadicnode(expr, intconstnode(TYPE_UNSIGNED_OFFSET, offset), EADD);
#else
        expr = makediadicnode(expr, intconstnode(TYPE(&stunsignedlong), offset), EADD);
#endif
    }

    if (ctor) {
        ctorflag = TRUE;
        if (tk == '=') {
            ctorflag = FALSE;
            if (list) {
                CError_Error(ERR_ILLEGAL_INITIALIZATION);
            }

            list = lalloc(sizeof(ENodeList));
            list->next = NULL;
            tk = lex();
            list->node = conv_assignment_expression();
        }

        expr = CExpr_ConstructObject(tclass, expr, list, FALSE, TRUE, FALSE, TRUE, ctorflag);
        if (expr->rtype->type != TYPEPOINTER) {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            return TRUE;
        }
    } else {
        if (list) {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
        }
    }

    if (dtor) {
        expr = CInit_RegisterDtorObject(TYPE(tclass), dtor, expr);
    }

    if (cinit_initinfo->use_register_cb) {
        cinit_initinfo->init_expr_register_cb(expr);
    } else {
        InitExpr_Register(expr, obj);
    }

    return TRUE;
}

static Boolean CInit_ConstructAutoObject(
    TypeClass* tclass, ENode* expr, SInt32 offset, Boolean parseInit
) {
    ENodeList* list;
    ENode* objexpr;
    NameSpaceObjectList* ctor;
    Object* dtor;
    Boolean ctorflag;

    ctor = CClass_Constructor(tclass);
    dtor = CClass_Destructor(tclass);
    if (!ctor && !dtor) {
        return FALSE;
    }

    if (dtor) {
        CClass_CheckStaticAccess(NULL, tclass, dtor->access);
    }

    if (parseInit && !ctor && tk == '=' && lookahead() == '{') {
        return FALSE;
    }

    if (parseInit && tk == '(') {
        tk = lex();
        list = CExpr_ScanExpressionList(TRUE);
        if (tk == ')') {
            tk = lex();
        } else {
            CError_Error(ERR_RPAREN_EXPECTED);
        }
    } else if (expr) {
        list = lalloc(sizeof(ENodeList));
        list->node = expr;
        list->next = NULL;
    } else {
        list = NULL;
    }

    if (ctor) {
        ctorflag = TRUE;
        if (tk == '=') {
            if (list) {
                CError_Error(ERR_ILLEGAL_INITIALIZATION);
            }
            list = lalloc(sizeof(ENodeList));
            list->next = NULL;
            tk = lex();
            list->node = conv_assignment_expression();
            ctorflag = FALSE;
        }

        if (!dtor) {
            objexpr = create_objectrefnode(cinit_initinfo->auto_obj);
            if (offset) {
                objexpr =
#if MWCCPSP
                    makediadicnode(objexpr, intconstnode(TYPE_UNSIGNED_OFFSET, offset), EADD);
#else
                    makediadicnode(objexpr, intconstnode(TYPE(&stunsignedlong), offset), EADD);
#endif
            }
        } else {
            objexpr = cinit_initinfo->register_object_cb(
                TYPE(tclass), cinit_initinfo->auto_obj, offset, FALSE
            );
        }

        objexpr = CExpr_ConstructObject(tclass, objexpr, list, FALSE, TRUE, FALSE, TRUE, ctorflag);
        if (!IS_TYPE_POINTER_ONLY(objexpr->rtype)) {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            return TRUE;
        }
        objexpr = makemonadicnode(objexpr, EINDIRECT);
        objexpr->rtype = TYPE_POINTER(objexpr->rtype)->target;
        cinit_initinfo->insert_expr_cb(objexpr);
    } else {
        if (list) {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
        }
        if (dtor) {
            objexpr = cinit_initinfo->register_object_cb(
                TYPE(tclass), cinit_initinfo->auto_obj, offset, FALSE
            );
        }
        cinit_initinfo->insert_expr_cb(objexpr);
    }

    return TRUE;
}

static void CInit_ExprPointer(TypePointer* tptr, ENode* expr) {
    Object* obj;
    CInt64 val;
    OLinkList* list;

    if (CInit_RelocInitCheck(expr, &obj, &val, FALSE)) {
        if (obj) {
            list = lalloc(sizeof(OLinkList));
            list->next = cinit_initinfo->list;
            list->obj = obj;
            list->addend = CInt64_GetULong(&val);
            list->offset = cinit_initinfo->expr_offset;
            cinit_initinfo->list = list;
        } else {
            CMach_InitIntMem(
#if MWCCPSP
                TYPE_UNSIGNED_OFFSET, val, cinit_initinfo->buffer + cinit_initinfo->expr_offset
#else
                TYPE(&stunsignedlong), val, cinit_initinfo->buffer + cinit_initinfo->expr_offset
#endif
            );
        }
    } else if (cinit_initinfo->expr_cb) {
        cinit_initinfo->expr_cb(TYPE(tptr), expr, FALSE);
        cinit_initinfo->expr_cb_called = TRUE;
    } else {
        CError_Error(ERR_ILLEGAL_CONSTANT_EXPRESSION);
    }

    cinit_initinfo->expr_offset += 4;
}

static void CInit_ExprInt(TypeIntegral* tint, ENode* expr) {
    if (ENODE_IS(expr, EINTCONST)) {
        CMach_InitIntMem(
            TYPE(tint), expr->data.intval, cinit_initinfo->buffer + cinit_initinfo->expr_offset
        );
    } else if (ENODE_IS(expr, ETYPCON) &&
               IS_TYPE_POINTER_ONLY(expr->data.monadic->rtype) &&
               expr->rtype->size == 4 &&
               (copts.cplusplus || !copts.ANSIstrict)) {
        CInit_ExprPointer(TYPE_POINTER(expr->data.monadic->rtype), expr->data.monadic);
    } else if (cinit_initinfo->expr_cb) {
        cinit_initinfo->expr_cb(TYPE(tint), expr, FALSE);
        cinit_initinfo->expr_cb_called = TRUE;
    } else {
        CError_Error(ERR_ILLEGAL_CONSTANT_EXPRESSION);
    }

    cinit_initinfo->expr_offset += tint->size;
}

static void CInit_ExprFloat(TypeIntegral* tint, ENode* expr) {
    if (ENODE_IS(expr, EFLOATCONST)) {
        CMach_InitFloatMem(
            TYPE(tint), expr->data.floatval, cinit_initinfo->buffer + cinit_initinfo->expr_offset
        );
    } else if (cinit_initinfo->expr_cb) {
        cinit_initinfo->expr_cb(TYPE(tint), expr, FALSE);
        cinit_initinfo->expr_cb_called = TRUE;
    } else {
        CError_Error(ERR_ILLEGAL_CONSTANT_EXPRESSION);
    }

    cinit_initinfo->expr_offset += tint->size;
}

static void CInit_ExprEnum(TypeEnum* tenum, ENode* expr) {
    if (ENODE_IS(expr, EINTCONST)) {
        CMach_InitIntMem(
            tenum->enumtype, expr->data.intval, cinit_initinfo->buffer + cinit_initinfo->expr_offset
        );
    } else if (cinit_initinfo->expr_cb) {
        cinit_initinfo->expr_cb(TYPE(tenum), expr, FALSE);
        cinit_initinfo->expr_cb_called = TRUE;
    } else {
        CError_Error(ERR_ILLEGAL_CONSTANT_EXPRESSION);
    }

    cinit_initinfo->expr_offset += tenum->size;
}

static void CInit_ExprMemberPointer(TypeMemberPointer* tmptr, ENode* expr) {
    if (ENODE_IS(expr, EINTCONST)) {
        CMach_InitIntMem(
            TYPE(&stsignedlong),
            expr->data.intval,
            cinit_initinfo->buffer + cinit_initinfo->expr_offset
        );
    } else if (cinit_initinfo->expr_cb) {
        cinit_initinfo->expr_cb(TYPE(tmptr), expr, FALSE);
        cinit_initinfo->expr_cb_called = TRUE;
    } else {
        CError_Error(ERR_ILLEGAL_CONSTANT_EXPRESSION);
    }

    cinit_initinfo->expr_offset += tmptr->size;
}

static void CInit_TypeExpr(Type* type, ENode* expr) {
    switch (type->type) {
        case TYPEINT:
            CInit_ExprInt(TYPE_INTEGRAL(type), expr);
            return;
        case TYPEFLOAT:
            CInit_ExprFloat(TYPE_INTEGRAL(type), expr);
            return;
        case TYPEENUM:
            CInit_ExprEnum(TYPE_ENUM(type), expr);
            return;
        case TYPEPOINTER:
            CInit_ExprPointer(TYPE_POINTER(type), expr);
            return;
        case TYPEMEMBERPOINTER:
            CInit_ExprMemberPointer(TYPE_MEMBER_POINTER(type), expr);
            return;
        case TYPESTRUCT:
        case TYPECLASS:
            if (cinit_initinfo->expr_cb) {
                cinit_initinfo->expr_cb(type, expr, FALSE);
                cinit_initinfo->expr_cb_called = TRUE;
            } else {
                CError_Error(ERR_ILLEGAL_CONSTANT_EXPRESSION);
            }
            return;
        case TYPEARRAY:
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            return;
        default:
            CError_FATAL(2082);
    }
}

static void CInit_Bitfield(TypeBitfield* tbitfield) {
    Boolean in_block;
    ENode* expr;
    ENode myexpr;

    in_block = tk == '{';
    if (in_block) {
        tk = lex();
    }

    expr = CInit_ParseInitializer(&myexpr);
    expr = CExpr_AssignmentPromotion(
        expr,
        IS_TYPE_ENUM(tbitfield->bitfieldtype) ? TYPE_ENUM(tbitfield->bitfieldtype)->enumtype
                                              : tbitfield->bitfieldtype,
        0,
        TRUE
    );
    if (ENODE_IS(expr, EINTCONST)) {
        CInit_SetBitfield(
            tbitfield,
            (UInt8*)cinit_initinfo->buffer + cinit_initinfo->expr_offset,
            expr->data.intval
        );
    } else {
        CError_Error(ERR_ILLEGAL_CONSTANT_EXPRESSION);
    }

    if (in_block) {
        CInit_CloseInitList();
    }
}

static void CInit_Array(TypePointer* tptr, UInt32 qual, Boolean isTopLevel) {
    SInt32 start;
    SInt32 i;
    SInt32 targetsize1;
    SInt32 targetsize2;
    Boolean in_block;
    Boolean is_char_ptr;
    Boolean needs_construction;
    Boolean is_wchar_ptr;

    targetsize1 = tptr->target->size;
    targetsize2 = tptr->target->size;
    if (!tptr->target->size) {
        if (!IS_TYPE_ARRAY(tptr->target)) {
            CError_Error(ERR_DATA_TYPE_INCOMPLETE);
            return;
        }
        targetsize1 = targetsize2 = TYPE_POINTER(tptr->target)->size;
        if (!targetsize1) {
            CError_Error(ERR_DATA_TYPE_INCOMPLETE);
            return;
        }
    }

    is_char_ptr = IS_TYPE_INT(tptr->target) && (tptr->target->size == 1);
    is_wchar_ptr = IS_TYPE_INT(tptr->target) && (tptr->target->size == stwchar.size);

    in_block = TRUE;
    if (isTopLevel &&
        !(tk == TK_STRING && is_char_ptr) &&
        !(tk == TK_STRING_WIDE && is_wchar_ptr)) {
        if (tk != '{') {
            CError_ErrorSkip(ERR_LBRACE_EXPECTED);
            return;
        }
        tk = lex();
    } else {
        if (tk == '{') {
            tk = lex();
        } else {
            in_block = FALSE;
        }
    }

    if ((tk == TK_STRING && is_char_ptr) || (tk == TK_STRING_WIDE && is_wchar_ptr)) {
        if (tptr->size) {
            if (tksize > tptr->size) {
                if (copts.cplusplus || (tksize - (is_wchar_ptr ? stwchar.size : 1)) > tptr->size) {
                    CError_Error(ERR_TOO_MANY_INITIALIZERS);
                }
                tksize = tptr->size;
            }
            memcpy(cinit_initinfo->buffer + cinit_initinfo->expr_offset, tkstring, tksize);
        } else {
            tptr->size = tksize;
            CInit_SetData(tkstring, cinit_initinfo->expr_offset, tptr->size);
        }
        cinit_initinfo->expr_offset += tptr->size;
        tk = lex();
        if (in_block) {
            CInit_CloseInitList();
        }
        return;
    }

    if (IS_TYPE_CLASS(tptr->target) && CInit_ClassNeedsConstruction(TYPE_CLASS(tptr->target))) {
        needs_construction = TRUE;
    } else {
        needs_construction = FALSE;
    }

    start = cinit_initinfo->expr_offset;
    i = 0;
    while (1) {
        if (tk == '}') {
innerloop:
            if (tptr->size) {
                if (needs_construction) {
                    while (tptr->size > (i * targetsize1)) {
                        cinit_initinfo->expr_offset = start + i * targetsize2;
                        if (cinit_initinfo->expr_cb) {
                            cinit_initinfo->expr_cb(tptr->target, NULL, TRUE);
                            cinit_initinfo->expr_cb_called = TRUE;
                        } else {
                            CError_Error(ERR_ILLEGAL_INITIALIZATION);
                        }
                        i++;
                    }
                }
            } else {
                tptr->size = i * targetsize1;
            }
            cinit_initinfo->expr_offset = start + tptr->size;
            if (in_block) {
                tk = lex();
            }
            return;
        }

        if (tptr->size == 0) {
            cinit_initinfo->expr_offset = start + i * targetsize2;
            CInit_SetData(NULL, cinit_initinfo->expr_offset, targetsize2);
            if (needs_construction) {
                if (cinit_initinfo->expr_cb) {
                    cinit_initinfo->expr_cb(tptr->target, conv_assignment_expression(), TRUE);
                    cinit_initinfo->expr_cb_called = TRUE;
                } else {
                    CError_Error(ERR_ILLEGAL_INITIALIZATION);
                }
            } else {
                CInit_Type(tptr->target, qual, FALSE);
            }
        } else {
            if (tptr->size <= i * targetsize1) {
                i--;
                CError_Error(ERR_TOO_MANY_INITIALIZERS);
            }

            cinit_initinfo->expr_offset = start + i * targetsize2;
            if (needs_construction) {
                if (cinit_initinfo->expr_cb) {
                    cinit_initinfo->expr_cb(tptr->target, conv_assignment_expression(), TRUE);
                    cinit_initinfo->expr_cb_called = TRUE;
                } else {
                    CError_Error(ERR_ILLEGAL_INITIALIZATION);
                }
            } else {
                CInit_Type(tptr->target, qual, FALSE);
            }

            if (!in_block) {
                if (tptr->size <= (i + 1) * targetsize1) {
                    return;
                }
            }
        }

        if (tk != '}') {
            if (tk != ',') {
                CError_ErrorSkip(ERR_DECLARATION_SYNTAX_ERROR);
                in_block = FALSE;
                i++;
                goto innerloop;
            }
            tk = lex();
        }
        i++;
    }
}

static void CInit_Struct(TypeStruct* tstruct, Boolean isTopLevel) {
    StructMember* member;
    SInt32 start;
    Boolean in_block;

    if (!(member = tstruct->members)) {
        CError_Error(ERR_DATA_TYPE_INCOMPLETE);
        return;
    }

    if (tstruct->stype == STRUCT_TYPE_UNION) {
        if (tk == '{') {
            tk = lex();
            CInit_Type(member->type, member->qual, FALSE);
            if (tk == '}') {
                tk = lex();
            }
        } else {
            CInit_Type(member->type, member->qual, FALSE);
        }
        return;
    }

    if (IS_TYPE_VECTOR(tstruct) && tk != '{') {
        CInit_TypeExpr(
            TYPE(tstruct),
            CExpr_AssignmentPromotion(conv_assignment_expression(), TYPE(tstruct), 0, TRUE)
        );
        return;
    }

    if (tk != '{') {
        if (isTopLevel) {
            CError_ErrorSkip(ERR_LBRACE_EXPECTED);
        }
        in_block = FALSE;
    } else {
        in_block = TRUE;
        tk = lex();
    }

    start = cinit_initinfo->expr_offset;
    while (1) {
        if (tk == '}') {
            break;
        }

        cinit_initinfo->expr_offset = start + member->offset;
        if (!member->type->size && IS_TYPE_ARRAY(member->type)) {
            CError_Error(ERR_TOO_MANY_INITIALIZERS);
            break;
        }

        CInit_Type(member->type, member->qual, FALSE);
        if (tk == '}') {
            break;
        }

        if (tk != ',') {
            CError_Error(ERR_DECLARATION_SYNTAX_ERROR);
            break;
        }

        do {
            member = member->next;
        } while (member && (member->qual & Q_WEAK));

        if (!member) {
            if (!in_block) {
                break;
            }
            if ((tk = lex()) != '}') {
                CError_Error(ERR_TOO_MANY_INITIALIZERS);
                break;
            }
        } else {
            tk = lex();
        }
    }

    cinit_initinfo->expr_offset = start + tstruct->size;
    if (tk == '}' && in_block) {
        tk = lex();
    }
}

static void CInit_Class(TypeClass* tclass, Boolean isTopLevel) {
    ObjMemberVar* ivar;
    SInt32 start;
    Boolean in_block;

    if (tk == '{') {
        in_block = TRUE;
        tk = lex();
    } else {
        in_block = FALSE;
    }

    if (tclass->bases || tclass->vtable) {
        CError_Error(ERR_ILLEGAL_INITIALIZATION);
        return;
    }

    for (ivar = tclass->ivars; ivar; ivar = ivar->next) {
        if (ivar->access != ACCESSPUBLIC) {
            break;
        }
    }

    if (!ivar && !CClass_Constructor(tclass) && (!CClass_Destructor(tclass) || in_block)) {
        if ((ivar = tclass->ivars)) {
            start = cinit_initinfo->expr_offset;
            while (1) {
                if (tk == '}') {
                    break;
                }

                if (!ivar->type->size && IS_TYPE_ARRAY(ivar->type)) {
                    CError_Error(ERR_TOO_MANY_INITIALIZERS);
                    break;
                }

                cinit_initinfo->expr_offset = start + ivar->offset;
                CInit_Type(ivar->type, ivar->qual, FALSE);

                if (tk == '}') {
                    break;
                }

                if (tk != ',') {
                    CError_Error(ERR_DECLARATION_SYNTAX_ERROR);
                    break;
                }

                do {
                    ivar = ivar->next;
                } while (ivar && ivar->anonunion);

                if (!ivar) {
                    if (!in_block) {
                        break;
                    }
                    if ((tk = lex()) != '}') {
                        CError_Error(ERR_TOO_MANY_INITIALIZERS);
                        break;
                    }
                } else {
                    tk = lex();
                }
            }
        } else {
            if (in_block && tk != '}') {
                CError_Error(ERR_TOO_MANY_INITIALIZERS);
            }
        }
    } else {
        if (in_block) {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
        }
        CInit_TypeExpr(
            TYPE(tclass),
            CExpr_AssignmentPromotion(conv_assignment_expression(), TYPE(tclass), 0, TRUE)
        );
    }

    cinit_initinfo->expr_offset = start + tclass->size;
    if (tk == '}' && in_block) {
        tk = lex();
    }
}

static void CInit_Type(Type* type, UInt32 qual, Boolean isTopLevel) {
    ENode* expr;
    ENode myexpr;

    switch (type->type) {
        case TYPEVOID:
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            return;
        case TYPEINT:
        case TYPEFLOAT:
        case TYPEENUM:
        case TYPEPOINTER:
        case TYPEMEMBERPOINTER:
            if (tk == '{') {
                tk = lex();
                expr = CInit_ParseInitializer(&myexpr);
                expr = CExpr_AssignmentPromotion(expr, type, qual & (Q_CONST | Q_VOLATILE), TRUE);
                CInit_CloseInitList();
            } else {
                expr = CInit_ParseInitializer(&myexpr);
                expr = CExpr_AssignmentPromotion(expr, type, qual & (Q_CONST | Q_VOLATILE), TRUE);
            }
            CInit_TypeExpr(type, expr);
            return;
        case TYPEBITFIELD:
            CInit_Bitfield(TYPE_BITFIELD(type));
            return;
        case TYPEARRAY:
            CInit_Array(TYPE_POINTER(type), qual, isTopLevel);
            return;
        case TYPESTRUCT:
            CInit_Struct(TYPE_STRUCT(type), isTopLevel);
            return;
        case TYPECLASS:
            CInit_Class(TYPE_CLASS(type), isTopLevel);
            return;
        default:
            CError_FATAL(2482);
    }
}

static void CInit_GlobalStaticInit(Type* type, ENode* valueexpr, Boolean isClass) {
    ENode* expr;
    ENode* lvalue;

    cinit_initinfo->has_runtime_init = TRUE;
    if (isClass) {
        CInit_ConstructGlobalObject(
            cinit_initinfo->obj, TYPE_CLASS(type), valueexpr, cinit_initinfo->expr_offset, FALSE
        );
    } else {
        expr = create_objectrefnode(cinit_initinfo->obj);
        if (!IS_TYPE_POINTER_ONLY(expr->rtype)) {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            return;
        }
        TYPE_POINTER(expr->rtype)->target = type;

        if (cinit_initinfo->expr_offset) {
            expr = makediadicnode(
#if MWCCPSP
                expr, intconstnode(TYPE_UNSIGNED_OFFSET, cinit_initinfo->expr_offset), EADD
#else
                expr, intconstnode(TYPE(&stunsignedlong), cinit_initinfo->expr_offset), EADD
#endif
            );
        }

        lvalue = makemonadicnode(expr, EINDIRECT);
        lvalue->rtype = type;
        expr = makediadicnode(lvalue, valueexpr, EASS);
        if (cinit_initinfo->use_register_cb) {
            cinit_initinfo->init_expr_register_cb(expr);
        } else {
            InitExpr_Register(expr, cinit_initinfo->obj);
        }
    }
}

static void CInit_AutoInit(Type* type, ENode* valueexpr, Boolean isClass) {
    ENode* expr;
    ENode* lvalue;
    TypePointer* copy;

    if (isClass) {
        CInit_ConstructAutoObject(TYPE_CLASS(type), valueexpr, cinit_initinfo->expr_offset, FALSE);
    } else {
        if (IS_TYPE_ARRAY(type) && (type->size & 1)) {
            copy = galloc(sizeof(TypePointer));
            *copy = *TYPE_POINTER(type);
            type = TYPE(copy);
            type->size++;
        }
        expr = create_objectrefnode(cinit_initinfo->auto_obj);
        if (!IS_TYPE_POINTER_ONLY(expr->rtype)) {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            return;
        }
        TYPE_POINTER(expr->rtype)->target = type;

        if (cinit_initinfo->expr_offset) {
            expr = makediadicnode(
#if MWCCPSP
                expr, intconstnode(TYPE_UNSIGNED_OFFSET, cinit_initinfo->expr_offset), EADD
#else
                expr, intconstnode(TYPE(&stunsignedlong), cinit_initinfo->expr_offset), EADD
#endif
            );
        }

        lvalue = makemonadicnode(expr, EINDIRECT);
        lvalue->rtype = type;
        expr = makediadicnode(lvalue, valueexpr, EASS);
        if (!copts.cplusplus) {
            CError_Error(ERR_ILLEGAL_CONSTANT_EXPRESSION);
        }
        cinit_initinfo->insert_expr_cb(expr);
    }
}

static SInt32 CInit_AdjustObjectDataSize(Object* obj) {
    if (obj->type->size <= 1) {
        return obj->type->size;
    }
    if (obj->type->size & 1) {
        return obj->type->size + 1;
    } else {
        return obj->type->size;
    }
}

static ENode* CInit_GenericData(
    Object* obj, Type* type, UInt32 qual, ExprCB expr_cb, Boolean isGlobal
) {
    ENode* expr;
    Object* target;
    ENode* stripped;
    Type* inner;
    TypePointer* copy;
    SInt32 size;
    Boolean lastflag;
    SInt16 cv;

    cinit_initinfo->expr_cb = expr_cb;
    expr = NULL;

    if (tk == '(') {
        if (IS_TYPE_ARRAY(type)) {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
        }
        tk = lex();
        expr = CExpr_AssignmentPromotion(assignment_expression(), type, qual, TRUE);
        if (tk != ')') {
            CError_ErrorSkip(ERR_RPAREN_EXPECTED);
        } else {
            tk = lex();
        }
        goto jump_ahead;
    }

    tk = lex();
    switch (type->type) {
        case TYPECLASS:
            if (tk == '{' && CClass_Constructor(TYPE_CLASS(type))) {
                CError_Error(ERR_ILLEGAL_INITIALIZATION);
            }
        case TYPESTRUCT:
            if (tk != '{') {
                goto generic_type;
            }
        case TYPEARRAY:
            if (!obj) {
                if (IS_TYPE_ARRAY(type)) {
                    inner = type;
                    while (IS_TYPE_ARRAY(inner)) {
                        inner = TYPE_POINTER(inner)->target;
                    }

                    if (IS_TYPE_CLASS(inner) && CInit_ClassNeedsConstruction(TYPE_CLASS(inner))) {
                        CInit_SetupInitInfoBuffer(type);
                        cinit_initinfo->obj = cinit_initinfo->auto_obj;
                        CInit_Type(type, cinit_initinfo->obj->qual, TRUE);
                        return NULL;
                    }
                    if (type->size & 1) {
                        copy = galloc(sizeof(TypePointer));
                        *copy = *TYPE_POINTER(type);
                        type = TYPE(copy);
                        type->size++;
                    }
                }

                cinit_initinfo->obj = obj = CInit_CreateStaticDataObject(type, qual, NULL);
                expr = create_objectnode(obj);
                cinit_initinfo->auto_obj = obj;
            }
            CInit_SetupInitInfoBuffer(type);
            CInit_Type(type, obj->qual, TRUE);
            if (obj->type->size != (size = cinit_initinfo->size)) {
                CError_FATAL(2639);
            }
            if (cinit_initinfo->list || !CInit_IsAllZero(cinit_initinfo->buffer, size)) {
                CInit_DeclareData(
                    obj, cinit_initinfo->buffer, cinit_initinfo->list, size = obj->type->size
                );
            } else {
                CInit_DeclareData(obj, NULL, NULL, size = obj->type->size);
            }
            return expr;
        case TYPEINT:
        case TYPEFLOAT:
        case TYPEENUM:
        case TYPEPOINTER:
        case TYPEMEMBERPOINTER:
generic_type:
            if (obj) {
                cv = obj->qual & Q_CV;
            } else {
                cv = cinit_initinfo->auto_obj->qual & Q_CV;
            }

            if (tk == '{') {
                tk = lex();
                expr = assignment_expression();
                CInit_CloseInitList();
            } else {
                expr = assignment_expression();
            }
            expr = CExpr_AssignmentPromotion(expr, type, cv, TRUE);
jump_ahead:
            if (obj == NULL) {
                target = cinit_initinfo->auto_obj;
            } else {
                target = obj;
            }

            if (is_const_object(target)) {
                switch (target->type->type) {
                    case TYPEINT:
                    case TYPEENUM:
                        if (ENODE_IS(expr, EINTCONST)) {
                            target->u.data.u.intconst = expr->data.intval;
                            goto inline_data;
                        }
                        break;
                    case TYPEFLOAT:
                        if (ENODE_IS(expr, EFLOATCONST)) {
                            target->u.data.u.floatconst = galloc(sizeof(Float));
                            *target->u.data.u.floatconst =
                                CMach_CalcFloatConvert(target->type, expr->data.floatval);
                            goto inline_data;
                        }
                        break;
                    case TYPEPOINTER:
                        stripped = expr;
                        while (ENODE_IS(stripped, ETYPCON)) {
                            stripped = stripped->data.monadic;
                        }
                        if (!ENODE_IS(stripped, EINTCONST)) {
                            break;
                        }
                        target->u.data.u.intconst = stripped->data.intval;
inline_data:
                        target->qual |= Q_INLINE_DATA;
                        if (!obj) {
                            target->sclass = TK_STATIC;
                            target->datatype = DDATA;
                            target->u.data.linkname = CParser_AppendUniqueName(target->name->name);
                        } else if (target->sclass != TK_STATIC ||
                                   (target->flags & OBJECT_REFERENCED)) {
                            CInit_ExportConst(target);
                        }
                        return NULL;
                    default:
                        break;
                }
            }

            if (!obj || (isGlobal && copts.cplusplus)) {
                if (obj) {
                    IsCompleteType(obj->type);
                    CError_ASSERT(2750, obj->type->size == type->size);
                    CInit_DeclareData(obj, NULL, NULL, obj->type->size);
                }
                return expr;
            }

            CInit_SetupInitInfoBuffer(type);
            CInit_TypeExpr(type, expr);
            CError_ASSERT(2759, obj->type->size == cinit_initinfo->size);

            IsCompleteType(obj->type);
            CInit_AdjustObjectDataSize(obj);
            lastflag = !cinit_initinfo->has_runtime_init && is_const_object(target);
            if (cinit_initinfo->list || !CInit_IsAllZero(cinit_initinfo->buffer, obj->type->size)) {
                if (lastflag) {
                    CInit_DeclareReadOnlyData(
                        obj, cinit_initinfo->buffer, cinit_initinfo->list, obj->type->size
                    );
                } else {
                    CInit_DeclareData(
                        obj, cinit_initinfo->buffer, cinit_initinfo->list, obj->type->size
                    );
                }
            } else {
                if (lastflag) {
                    CInit_DeclareReadOnlyData(obj, NULL, NULL, obj->type->size);
                } else {
                    CInit_DeclareData(obj, NULL, NULL, obj->type->size);
                }
            }
            break;

        default:
            CError_FATAL(2779);
    }

    return NULL;
}

void CInit_ExportConst(Object* obj) {
    char buffer[64];

    if (obj->flags & OBJECT_DEFINED) {
        return;
    }

    switch (obj->type->type) {
        case TYPEINT:
            CMach_InitIntMem(obj->type, obj->u.data.u.intconst, buffer);
            break;
        case TYPEENUM:
            CMach_InitIntMem(TYPE_ENUM(obj->type)->enumtype, obj->u.data.u.intconst, buffer);
            break;
        case TYPEPOINTER:
#if MWCCPSP
            CMach_InitIntMem(TYPE_UNSIGNED_OFFSET, obj->u.data.u.intconst, buffer);
#else
            CMach_InitIntMem(TYPE(&stunsignedlong), obj->u.data.u.intconst, buffer);
#endif
            break;
        case TYPEFLOAT:
            CMach_InitFloatMem(obj->type, *obj->u.data.u.floatconst, buffer);
            break;
        default:
            CError_FATAL(2810);
    }

    if (is_const_object(obj)) {
        CInit_DeclareReadOnlyData(obj, buffer, NULL, obj->type->size);
    } else {
        CInit_DeclareData(obj, buffer, NULL, obj->type->size);
    }
}

static ENode* CInit_ClassInitLoopCallBack(ENode* expr) {
    return CExpr_ConstructObject(cinit_loop_class, expr, NULL, FALSE, TRUE, FALSE, TRUE, TRUE);
}

Statement* CInit_ConstructClassArray(
    Statement* stmt, TypeClass* tclass, Object* ctor, Object* dtor, ENode* firstarg, SInt32 count
) {
    ENode* dtor_expr;

#if MWCCPSP
    // 0x5098C0: without a destructor, 3.0.1 constructs the elements in a loop
    if (!dtor) {
        cinit_loop_class = tclass;
        stmt = CFunc_GenerateLoop(
            stmt,
            CDecl_NewPointerType(TYPE(tclass)),
            firstarg,
            makediadicnode(
                CInline_CopyExpression(firstarg, CopyModeLocal),
                intconstnode(TYPE(&stunsignedint), count * tclass->size),
                EADD
            ),
            intconstnode(TYPE(&stunsignedint), tclass->size),
            NULL,
            (ENode * (*)(ENode*, ENode*)) CInit_ClassInitLoopCallBack
        );
        return stmt;
    }
#endif
#if MWCCPSP

#endif
    if (stmt) {
        stmt = CFunc_InsertStatement(ST_EXPRESSION, stmt);
    } else {
        stmt = CFunc_AppendStatement(ST_EXPRESSION);
    }

    if (dtor) {
        dtor_expr = create_objectrefnode(CABI_GetDestructorObject(dtor, CABIDestroyComplete));
    } else {
        dtor_expr = nullnode();
    }

    stmt->expr = CExpr_FuncCallSix(
        carr_func,
        firstarg,
        create_objectrefnode(ctor),
        dtor_expr,
#if MWCCPSP
        intconstnode(TYPE(&stunsignedlong), tclass->size),
        intconstnode(TYPE(&stunsignedlong), count),
#else
        intconstnode(TYPE(&stunsignedlong), tclass->size),
        intconstnode(TYPE(&stunsignedlong), count),
#endif
        NULL
    );

    return stmt;
}

static void CInit_InitializeClassArray(Object* obj, TypeClass* tclass, Boolean isGlobal) {
    Object* ctor;
    Object* dtor;
    SInt32 count;
    SInt32 i;
    ENode* expr;
    SInt32 offset;
    Statement* stmt;
    TypeFunc* tfunc;
    ENode* dtor_expr;
    Object* funcobj;

    dtor = CClass_Destructor(tclass);
    count = obj->type->size / tclass->size;
    if (CClass_Constructor(tclass)) {
        ctor = CClass_DefaultConstructor(tclass);
        if (!ctor) {
            ctor = CClass_DummyDefaultConstructor(tclass);
            if (!ctor) {
                CError_Error(ERR_CLASS_NO_DEFAULT_CONSTRUCTOR);
                return;
            }
        }
    } else {
        ctor = NULL;
    }

    if (count <= 1 || (!isGlobal && count <= 8)) {
        if (isGlobal) {
            for (i = 0; i < count; i++) {
                CInit_ConstructGlobalObject(obj, tclass, NULL, i * tclass->size, FALSE);
            }
        } else {
            for (i = 0; i < count; i++) {
                offset = i * tclass->size;
                expr = create_objectrefnode(obj);
                if (offset) {
#if MWCCPSP
                    expr = makediadicnode(expr, intconstnode(TYPE_UNSIGNED_OFFSET, offset), EADD);
#else
                    expr = makediadicnode(expr, intconstnode(TYPE(&stunsignedlong), offset), EADD);
#endif
                }
                if (ctor) {
                    expr =
                        CExpr_ConstructObject(tclass, expr, NULL, FALSE, TRUE, FALSE, TRUE, TRUE);
                }
                cinit_initinfo->insert_expr_cb(expr);
                if (dtor) {
                    CExcept_RegisterDestructorObject(obj, offset, dtor, TRUE);
                }
            }
            if (dtor) {
                stmt = CFunc_AppendStatement(ST_EXPRESSION);
                stmt->expr = nullnode();
            }
        }
    } else {
        if (ctor) {
            if (!isGlobal && !dtor) {
                CInit_ConstructClassArray(
                    NULL, tclass, ctor, dtor, create_objectrefnode(obj), count
                );
                expr = nullnode();
            } else {
                if (dtor) {
                    dtor_expr =
                        create_objectrefnode(CABI_GetDestructorObject(dtor, CABIDestroyComplete));
                } else {
                    dtor_expr = nullnode();
                }
                expr = CExpr_FuncCallSix(
                    carr_func,
                    create_objectrefnode(obj),
                    create_objectrefnode(ctor),
                    dtor_expr,
#if MWCCPSP
                    intconstnode(TYPE_UNSIGNED_OFFSET, tclass->size),
                    intconstnode(TYPE_UNSIGNED_OFFSET, count),
#else
                    intconstnode(TYPE(&stunsignedlong), tclass->size),
                    intconstnode(TYPE(&stunsignedlong), count),
#endif
                    NULL
                );
            }
        } else {
            expr = nullnode();
        }

        if (isGlobal) {
            if (dtor && !copts.no_static_dtors) {
                tfunc = galloc(sizeof(TypeFunc));
                memclrw(tfunc, sizeof(TypeFunc));
                tfunc->type = TYPEFUNC;
                tfunc->functype = &stvoid;
                CDecl_SetFuncFlags(tfunc, 1);

                funcobj = CParser_NewCompilerDefFunctionObject();
                funcobj->name = CParser_AppendUniqueName("__arraydtor");
                funcobj->type = TYPE(tfunc);
                funcobj->sclass = TK_STATIC;
                funcobj->qual = Q_INLINE;

                CParser_RegisterSingleExprFunction(
                    funcobj,
                    funccallexpr(
                        darr_func,
                        create_objectrefnode(obj),
                        create_objectrefnode(CABI_GetDestructorObject(dtor, CABIDestroyComplete)),
                        intconstnode(TYPE(&stsignedlong), tclass->size),
                        intconstnode(TYPE(&stsignedlong), count)
                    )
                );

                expr = makediadicnode(expr, nullnode(), ECOMMA);
                expr->rtype = TYPE(&void_ptr);

                expr = funccallexpr(
                    Xgreg_func,
                    expr,
                    create_objectrefnode(funcobj),
                    create_objectrefnode(CInit_CreateStaticData(CInit_GetRegMemType())),
                    NULL
                );
            }
            if (cinit_initinfo->use_register_cb) {
                cinit_initinfo->init_expr_register_cb(expr);
            } else {
                InitExpr_Register(expr, obj);
            }
        } else {
            stmt = CFunc_AppendStatement(ST_EXPRESSION);
            stmt->expr = expr;
            if (dtor) {
                CExcept_RegisterLocalArray(stmt, obj, dtor, count, tclass->size);
                stmt = CFunc_AppendStatement(ST_EXPRESSION);
                stmt->expr = nullnode();
            }
        }
    }
}

static ENode* CInit_AutoTempNode(Type* type, Boolean assignID) {
    ENode* node;
    node = CExpr_NewETEMPNode(type, FALSE);
    if (IS_TYPE_CLASS(type) && CClass_Destructor(TYPE_CLASS(type))) {
        node->data.temp.needs_dtor = TRUE;
    }
    return node;
}

static ENode* CInit_GlobalTempNode(Type* type, Boolean assignID) {
    Object* dtor;
    ENode* funcnode;
    ENode* node;

    node = create_objectrefnode(CInit_CreateStaticData(type));
    if (IS_TYPE_CLASS(type) &&
        (dtor = CClass_Destructor(TYPE_CLASS(type))) &&
        !copts.no_static_dtors) {
        if (assignID) {
            CError_Error(ERR_UNIMPLEMENTED_C_FEATURE);
        }

        funcnode = galloc(sizeof(ENode));
        funcnode->type = EFUNCCALL;
        funcnode->cost = 200;
        funcnode->flags = 0;
        funcnode->rtype = CDecl_NewPointerType(type);
        funcnode->data.funccall.funcref = create_objectrefnode(Xgreg_func);
        funcnode->data.funccall.functype = TYPE_FUNC(Xgreg_func->type);
        funcnode->data.funccall.args = lalloc(sizeof(ENodeList));
        funcnode->data.funccall.args->node = node;
        funcnode->data.funccall.args->next = lalloc(sizeof(ENodeList));
        funcnode->data.funccall.args->next->node =
            create_objectrefnode(CABI_GetDestructorObject(dtor, CABIDestroyComplete));
        funcnode->data.funccall.args->next->next = lalloc(sizeof(ENodeList));
        funcnode->data.funccall.args->next->next->node =
            create_objectrefnode(CInit_CreateStaticData(CInit_GetRegMemType()));
        funcnode->data.funccall.args->next->next->next = NULL;
        node = funcnode;
    }
    return node;
}

static void CInit_RefInit(Type* type, ENode* expr, Boolean unused) {
    ENode* objexpr;

    objexpr = create_objectrefnode(cinit_initinfo->obj);
    if (!IS_TYPE_POINTER_ONLY(objexpr->rtype)) {
        CError_Error(ERR_ILLEGAL_INITIALIZATION);
        return;
    }
    TYPE_POINTER(objexpr->rtype)->target = type;

    if (cinit_initinfo->expr_offset) {
        objexpr = makediadicnode(
#if MWCCPSP
            objexpr, intconstnode(TYPE_UNSIGNED_OFFSET, cinit_initinfo->expr_offset), EADD
#else
            objexpr, intconstnode(TYPE(&stunsignedlong), cinit_initinfo->expr_offset), EADD
#endif
        );
    }

    objexpr = makemonadicnode(objexpr, EINDIRECT);
    objexpr->rtype = type;

    objexpr = makediadicnode(objexpr, expr, EASS);
    if (cinit_initinfo->use_register_cb) {
        cinit_initinfo->init_expr_register_cb(objexpr);
    } else {
        InitExpr_Register(objexpr, cinit_initinfo->obj);
    }
}

static Boolean CInit_IsDtorTemp(ENode* expr) {
    return ENODE_IS(expr, ETEMP) && expr->data.temp.needs_dtor;
}

static void CInit_FindDtorTemp(ENode* expr) {
    ENodeList* list;

    while (ENODE_IS(expr, ECOMMA)) {
        expr = expr->data.diadic.right;
    }

    if (IS_TYPE_POINTER_ONLY(expr->rtype) && IS_TYPE_CLASS(TYPE_POINTER(expr->rtype)->target)) {
        switch (expr->type) {
            case ETYPCON:
                CInit_FindDtorTemp(expr->data.monadic);
                return;
            case EADD:
            case ESUB:
                CInit_FindDtorTemp(expr->data.diadic.left);
                CInit_FindDtorTemp(expr->data.diadic.right);
                return;
            case EFUNCCALL:
            case EFUNCCALLP:
                if ((list = expr->data.funccall.args)) {
                    if (CInit_IsDtorTemp(list->node) ||
                        ((list = list->next) && CInit_IsDtorTemp(list->node))) {
                        if (!cinit_fdtnode) {
                            cinit_fdtnode = list;
                        } else {
                            cinit_fdtambig = TRUE;
                        }
                    }
                }
                return;
            default:
                break;
        }
    }
}

static void CInit_RefTempTransform(Type* type, ENode* expr) {
    Object* obj;

    CError_ASSERT(3167, IS_TYPE_POINTER_ONLY(type));

    if (IS_TYPE_CLASS(TYPE_POINTER(type)->target)) {
        cinit_fdtnode = NULL;
        cinit_fdtambig = FALSE;
        CInit_FindDtorTemp(expr);
        if (cinit_fdtnode) {
            CError_ASSERT(3175, !cinit_fdtambig);
            obj = create_temp_object(cinit_fdtnode->node->data.temp.type);
            cinit_initinfo->register_object_cb(cinit_fdtnode->node->data.temp.type, obj, 0, FALSE);
            cinit_fdtnode->node = create_objectrefnode(obj);
        }
    }
}

static Boolean CInit_InitReference(Object* obj, Boolean isLocal) {
    ENode* expr;
    SInt32 size;

    if (tk == '=') {
        if (isLocal) {
            cinit_tempnodefunc = CInit_AutoTempNode;
        } else {
            cinit_tempnodefunc = CInit_GlobalTempNode;
        }
        tk = lex();
        expr = assignment_expression();
        expr = CExpr_AssignmentPromotion(expr, obj->type, obj->qual & (Q_CONST | Q_VOLATILE), TRUE);
        cinit_tempnodefunc = NULL;

        if (isLocal) {
            CInit_RefTempTransform(obj->type, expr);
            expr = makediadicnode(create_objectnode2(obj), expr, EASS);
            cinit_initinfo->insert_expr_cb(expr);
        } else {
            cinit_initinfo->expr_cb = CInit_RefInit;
            CInit_SetupInitInfoBuffer(obj->type);
            CInit_ExprPointer(TYPE_POINTER(obj->type), expr);
            CError_ASSERT(3216, obj->type->size == cinit_initinfo->size);

            if (cinit_initinfo->list || !CInit_IsAllZero(cinit_initinfo->buffer, obj->type->size)) {
                IsCompleteType(obj->type);
                CInit_DeclareData(
                    obj, cinit_initinfo->buffer, cinit_initinfo->list, size = obj->type->size
                );
            } else {
                IsCompleteType(obj->type);
                CInit_DeclareData(obj, NULL, NULL, size = obj->type->size);
            }
        }

        return TRUE;
    }

    return FALSE;
}

ENode* CInit_AutoObject(Object* obj, Type* type, UInt32 qual) {
    CInit_DataBuffer db;
    CInit_NonConst* entry;
    ENode* expr;
    ENode* indirect_expr;
    ENode* obj_expr;
    ENode* const_expr;
    Type* newtype;
    Object* newobj;

    CInit_InitData(
        &db, type, qual, copts.cplusplus || copts.gcc_extensions || copts.c9x || obj == NULL
    );
    if (!obj && !cscope_currentfunc) {
        obj = CParser_NewCompilerDefDataObject();
        obj->name = CParser_GetUniqueName();
        obj->type = type;
        obj->qual = qual;
        obj->sclass = TK_STATIC;
        CScope_AddGlobalObject(obj);
    }

    if (IS_TYPE_VECTOR(type) && !db.nonconst) {
        if (obj) {
            obj_expr = create_objectrefnode(obj);
        } else {
            obj_expr = CExpr_NewETEMPNode(type, TRUE);
        }

        const_expr = CExpr_NewENode(EVECTOR128CONST);
        const_expr->rtype = type;
        const_expr->data.vector128val = *((MWVector128*)db.buffer);

        indirect_expr = makemonadicnode(obj_expr, EINDIRECT);
        indirect_expr->rtype = type;

        expr = makediadicnode(indirect_expr, const_expr, EASS);
        if (!obj) {
            ENode* objval = lalloc(sizeof(ENode));
            *objval = *obj_expr;
            objval = makemonadicnode(objval, EINDIRECT);
            objval->rtype = type;
            objval->flags = qual & ENODE_FLAG_QUALS;
            expr = makecommaexpression(expr, objval);
        }
        return expr;
    }

    if (db.flexsize) {
        type = CDecl_NewStructType(type->size + db.flexsize, CMach_GetTypeAlign(type));
        if (obj) {
            obj->type = type;
        }
    }

    if (obj) {
        obj_expr = create_objectrefnode(obj);
    } else {
        obj_expr = CExpr_NewETEMPNode(type, TRUE);
    }

    if (IS_TYPE_ARRAY((newtype = type))) {
        newtype = CDecl_NewStructType(type->size, CMach_GetTypeAlign(type));
    }
    newobj = CInit_CreateStaticDataObject(newtype, 0, NULL);
    if (db.list || !CInit_IsAllZero(db.buffer, db.size)) {
        CInit_DeclareReadOnlyData(newobj, db.buffer, db.list, db.size);
    } else {
        CInit_DeclareReadOnlyData(newobj, NULL, NULL, db.size);
    }

    indirect_expr = makemonadicnode(obj_expr, EINDIRECT);
    indirect_expr->rtype = newtype;
    expr = makediadicnode(indirect_expr, create_objectnode(newobj), EASS);

    for (entry = db.nonconst; entry; entry = entry->next) {
        expr = CInit_InitConcat(expr, obj_expr, entry->offset, entry->type, entry->expr);
    }

    if (!obj) {
        ENode* objval = lalloc(sizeof(ENode));
        *objval = *obj_expr;
        objval = makemonadicnode(objval, EINDIRECT);
        objval->rtype = type;
        objval->flags = qual & ENODE_FLAG_QUALS;
        expr = makecommaexpression(expr, objval);
    } else if (IS_TYPE_ARRAY(type)) {
        expr = makecommaexpression(expr, create_objectnode(obj));
    }

    return expr;
}

static void CInit_GlobalObject(Object* obj) {
    CInit_DataBuffer db;
    CInit_NonConst* entry;
    ENode* obj_expr;
    ENode* expr;

    CInit_InitData(&db, obj->type, obj->qual, copts.cplusplus);
    obj_expr = create_objectrefnode(obj);

    IsCompleteType(obj->type);

    if (!db.nonconst && is_const_object(obj)) {
        CInit_DeclareReadOnlyData(obj, db.buffer, db.list, db.size);
    } else {
        CInit_DeclareData(obj, db.buffer, db.list, db.size);
    }

    if (db.nonconst) {
        entry = db.nonconst;
        expr = NULL;
        while (entry) {
            if (!ENODE_IS(entry->expr, EVECTOR128CONST)) {
                expr = CInit_InitConcat(expr, obj_expr, entry->offset, entry->type, entry->expr);
            }
            entry = entry->next;
        }

        if (expr) {
            InitExpr_Register(expr, obj);
        }
    }
}

void CInit_InitializeAutoData(Object* obj, InsertExprCB insert_cb, RegisterObjectCB register_cb) {
    Type* type = obj->type;
    Type* inner;
    Boolean is_simple;
    InitInfo initinfo;
    ENode* expr;

    switch (type->type) {
        case TYPESTRUCT:
            is_simple = TRUE;
            break;
        case TYPEARRAY:
            while (IS_TYPE_ARRAY(type)) {
                type = TYPE_POINTER(type)->target;
            }
            if (!IS_TYPE_CLASS(type)) {
                is_simple = TRUE;
                break;
            }
        case TYPECLASS:
            is_simple = !CInit_ClassNeedsConstruction(TYPE_CLASS(type));
            break;
        default:
            is_simple = FALSE;
    }

    if (is_simple) {
        if (tk == '=' || (tk == '(' && copts.cplusplus)) {
            if (tk == '(') {
                tk = lex();
                expr = conv_assignment_expression();
                if (tk != ')') {
                    CError_Error(ERR_RPAREN_EXPECTED);
                }
                tk = lex();
            } else if (tk == '=' && ((tk = lex()) == '{' || IS_TYPE_ARRAY(obj->type))) {
                insert_cb(CInit_AutoObject(obj, obj->type, obj->qual));
                return;
            } else {
                expr = conv_assignment_expression();
            }
            expr = CExpr_AssignmentPromotion(
                expr, obj->type, obj->qual & (Q_CONST | Q_VOLATILE), TRUE
            );
            insert_cb(makediadicnode(create_objectnode2(obj), expr, EASS));
            return;
        } else if (copts.cplusplus && is_const_object(obj)) {
            CError_Error(ERR_CONST_AMPERSAND_VARIABLE_NEEDS_INITIALIZER);
        }

        return;
    }

    CInit_SetupInitInfo(&initinfo, obj);
    initinfo.auto_obj = obj;
    initinfo.insert_expr_cb = insert_cb;
    initinfo.register_object_cb = register_cb;

    if (IS_TYPE_CLASS(obj->type) &&
        CInit_ConstructAutoObject(TYPE_CLASS(obj->type), NULL, 0, TRUE)) {
        CInit_CleanupInitInfo(&initinfo);
        return;
    }

    if (IS_TYPE_REFERENCE(obj->type) && CInit_InitReference(obj, TRUE)) {
        CInit_CleanupInitInfo(&initinfo);
        return;
    }

    if (tk != '=' && (tk != '(' || !copts.cplusplus)) {
        if (IS_TYPE_ARRAY(obj->type)) {
            inner = obj->type;
            while (IS_TYPE_ARRAY(inner)) {
                inner = TYPE_POINTER(inner)->target;
            }
            if (IS_TYPE_CLASS(inner)) {
                if (CInit_ClassNeedsConstruction(TYPE_CLASS(inner))) {
                    CInit_InitializeClassArray(obj, TYPE_CLASS(inner), FALSE);
                    CInit_CleanupInitInfo(&initinfo);
                    return;
                }
                CFunc_CheckClassCtors(TYPE_CLASS(inner));
            }
        }

        if (IS_TYPE_CLASS(obj->type)) {
            CFunc_CheckClassCtors(TYPE_CLASS(obj->type));
        }

        if ((IS_TYPE_REFERENCE(obj->type) || is_const_object(obj)) && copts.cplusplus) {
            CError_Error(ERR_CONST_AMPERSAND_VARIABLE_NEEDS_INITIALIZER);
        }
    } else {
        if (obj->type->size || IS_TYPE_ARRAY(obj->type)) {
            if ((expr = CInit_GenericData(NULL, obj->type, obj->qual, CInit_AutoInit, FALSE))) {
                insert_cb(makediadicnode(create_objectnode2(obj), expr, EASS));
            }
        } else {
            CError_Error(ERR_DATA_TYPE_INCOMPLETE);
        }
    }

    if (IS_TYPE_CLASS(obj->type) && CClass_Destructor(TYPE_CLASS(obj->type))) {
        register_cb(obj->type, obj, 0, FALSE);
    }

    CInit_CleanupInitInfo(&initinfo);
}

void CInit_InitializeStaticData(Object* obj, InitExprRegisterCB cb) {
    ENode* expr;
    Type* type;
    InitInfo initinfo;
    CInit_DataBuffer db;
    CInit_NonConst* entry;
    ENode* obj_expr;

    if (CInit_IsSimpleStructArrayInit(obj->type)) {
        if (tk == '=' || (tk == '(' && copts.cplusplus)) {
            if (tk == '=') {
                tk = lex();
            }
            CInit_InitData(&db, obj->type, obj->qual, copts.cplusplus);

            IsCompleteType(obj->type);

            if (!db.nonconst && is_const_object(obj)) {
                CInit_DeclareReadOnlyData(obj, db.buffer, db.list, db.size);
            } else {
                CInit_DeclareData(obj, db.buffer, db.list, db.size);
            }

            if (db.nonconst) {
                obj_expr = create_objectrefnode(obj);
                entry = db.nonconst;
                expr = NULL;
                while (entry) {
                    expr =
                        CInit_InitConcat(expr, obj_expr, entry->offset, entry->type, entry->expr);
                    entry = entry->next;
                }
                cb(expr);
            }
        } else {
            if (copts.cplusplus && is_const_object(obj)) {
                CError_Error(ERR_CONST_AMPERSAND_VARIABLE_NEEDS_INITIALIZER);
            }

            if (is_const_object(obj)) {
                CInit_DeclareReadOnlyData(obj, NULL, NULL, obj->type->size);
            } else {
                CInit_DeclareData(obj, NULL, NULL, obj->type->size);
            }
        }
        return;
    }

    CInit_SetupInitInfo(&initinfo, obj);
    initinfo.use_register_cb = TRUE;
    initinfo.init_expr_register_cb = cb;

    if (IS_TYPE_CLASS(obj->type) &&
        CInit_ConstructGlobalObject(obj, TYPE_CLASS(obj->type), NULL, 0, TRUE)) {
        IsCompleteType(obj->type);
        CInit_DeclareData(obj, NULL, NULL, obj->type->size);
        CInit_CleanupInitInfo(&initinfo);
        return;
    }

    if (IS_TYPE_REFERENCE(obj->type) && CInit_InitReference(obj, FALSE)) {
        CInit_CleanupInitInfo(&initinfo);
        return;
    }

    if (tk != '=' && (tk != '(' || !copts.cplusplus)) {
        if (IsCompleteType(obj->type)) {
            CInit_DeclareData(obj, NULL, NULL, obj->type->size);
        }

        if (IS_TYPE_ARRAY(obj->type)) {
            type = obj->type;
            while (IS_TYPE_ARRAY(type)) {
                type = TYPE_POINTER(type)->target;
            }
            if (IS_TYPE_CLASS(type)) {
                if (CInit_ClassNeedsConstruction(TYPE_CLASS(type))) {
                    CInit_InitializeClassArray(obj, TYPE_CLASS(type), TRUE);
                    CInit_CleanupInitInfo(&initinfo);
                    return;
                }
                CFunc_CheckClassCtors(TYPE_CLASS(type));
            }
        }

        if (IS_TYPE_CLASS(obj->type)) {
            CFunc_CheckClassCtors(TYPE_CLASS(obj->type));
        }

        if ((IS_TYPE_REFERENCE(obj->type) || is_const_object(obj)) && copts.cplusplus) {
            CError_Error(ERR_CONST_AMPERSAND_VARIABLE_NEEDS_INITIALIZER);
        }
    } else {
        if (obj->type->size || IS_TYPE_ARRAY(obj->type)) {
            if ((expr =
                     CInit_GenericData(obj, obj->type, obj->qual, CInit_GlobalStaticInit, TRUE))) {
                cb(makediadicnode(create_objectnode2(obj), expr, EASS));
            }
        } else {
            CError_Error(ERR_DATA_TYPE_INCOMPLETE);
        }
    }

    CInit_CleanupInitInfo(&initinfo);
}

#if MWCCPSP
extern void func_00449980(Object* obj); // __attribute__((...)) after a declarator
struct CInitRecord;
extern struct CInitRecord* CInit_ParseInitRecord(Type* type, Boolean parseList);
void CInit_StaticObject(Object* obj, struct CInitRecord* rec, Statement* stmt, Boolean isLocal);

// 0x505380: a global data object's initializer (3.0.1: parsed into a record
// and handed to CInit_StaticObject)
void CInit_InitializeData(Object* obj) {
    struct CInitRecord* rec;
    CInt64 val;

    if (tk == ':') {
        if ((obj->flags & OBJECT_DEFINED) ||
            (obj->qual & Q_INLINE_DATA) ||
            obj->datatype == DABSOLUTE) {
            CError_Error(ERR_DATA_OBJECT_REDEFINED, obj);
        }
        tk = lex();
        obj->datatype = DABSOLUTE;
        val = CExpr_IntegralConstExpr();
        obj->u.address = CInt64_GetULong(&val);
        return;
    }

    if (tk != '=' && (tk != '(' || !copts.cplusplus)) {
        if (obj->sclass == TK_EXTERN) {
            return;
        }
        if (!copts.cplusplus) {
            if (IsCompleteType(obj->type)) {
                ObjectList* list;
                for (list = cinit_tentative; list; list = list->next) {
                    if (list->object == obj) {
                        break;
                    }
                }
                if (!list) {
                    list = galloc(sizeof(ObjectList));
                    list->object = obj;
                    list->next = cinit_tentative;
                    cinit_tentative = list;
                    obj->qual |= Q_TENTATIVE;
                }
            }
            return;
        }
    }

    if ((obj->flags & OBJECT_DEFINED) ||
        (obj->qual & Q_INLINE_DATA) ||
        obj->datatype == DABSOLUTE) {
        CError_Error(ERR_DATA_OBJECT_REDEFINED, obj);
    }
    rec = CInit_ParseInitRecord(obj->type, FALSE);
    if (tk == TK_UU_ATTRIBUTE_UU) {
        func_00449980(obj);
    }
    CInit_StaticObject(obj, rec, NULL, FALSE);
    if (!(obj->qual & Q_INLINE_DATA)) {
        obj->flags |= OBJECT_DEFINED;
    }
}
#else
void CInit_InitializeData(Object* obj) {
    Object* dtor;
    ObjectList* list;
    InitInfo initinfo;
    CInt64 val;
    Boolean needs_construction;
    Type* type;

    if (tk == ':') {
        tk = lex();
        obj->datatype = DABSOLUTE;
        val = CExpr_IntegralConstExpr();
        obj->u.address = CInt64_GetULong(&val);
        return;
    }

    if (tk != '=' && (tk != '(' || !copts.cplusplus)) {
        if (obj->sclass == TK_EXTERN) {
            return;
        }
        if (!copts.cplusplus) {
            if (IsCompleteType(obj->type)) {
                for (list = cinit_tentative; list; list = list->next) {
                    if (list->object == obj) {
                        break;
                    }
                }
                if (!list) {
                    list = galloc(sizeof(ObjectList));
                    list->object = obj;
                    list->next = cinit_tentative;
                    cinit_tentative = list;
                    obj->qual |= Q_TENTATIVE;
                }
            }

            return;
        }

        if (obj->flags & OBJECT_DEFINED) {
            CError_Error(ERR_DATA_OBJECT_REDEFINED, obj);
        }
        obj->flags |= OBJECT_DEFINED;

        needs_construction = FALSE;
        if (IS_TYPE_ARRAY(obj->type)) {
            type = obj->type;
            while (IS_TYPE_ARRAY(type)) {
                type = TYPE_POINTER(type)->target;
            }
            if (IS_TYPE_CLASS(type)) {
                if (CInit_ClassNeedsConstruction(TYPE_CLASS(type))) {
                    CInit_SetupInitInfo(&initinfo, obj);
                    CInit_InitializeClassArray(obj, TYPE_CLASS(type), TRUE);
                    CInit_CleanupInitInfo(&initinfo);
                    needs_construction = TRUE;
                } else {
                    CFunc_CheckClassCtors(TYPE_CLASS(type));
                }
            }
        } else {
            if (IS_TYPE_CLASS(obj->type)) {
                if (CInit_ClassNeedsConstruction(TYPE_CLASS(obj->type))) {
                    CInit_SetupInitInfo(&initinfo, obj);
                    CInit_ConstructGlobalObject(obj, TYPE_CLASS(obj->type), NULL, 0, FALSE);
                    CInit_CleanupInitInfo(&initinfo);
                    needs_construction = TRUE;
                } else {
                    CFunc_CheckClassCtors(TYPE_CLASS(obj->type));
                }
            }
        }

        if (!needs_construction && copts.cplusplus) {
            if (IS_TYPE_REFERENCE(obj->type) || is_const_object(obj)) {
                CError_Error(ERR_CONST_AMPERSAND_VARIABLE_NEEDS_INITIALIZER);
            }
        }
        if (IsCompleteType(obj->type)) {
            CInit_DeclareData(obj, NULL, NULL, obj->type->size);
        }
        return;
    }

    if (obj->flags & OBJECT_DEFINED) {
        CError_Error(ERR_DATA_OBJECT_REDEFINED, obj);
    }

    if (CInit_IsSimpleStructArrayInit(obj->type)) {
        if (tk == '=') {
            tk = lex();
        } else {
            CError_Error(ERR_DECLARATION_SYNTAX_ERROR);
        }
        CInit_GlobalObject(obj);
        return;
    }

    CInit_SetupInitInfo(&initinfo, obj);
    if (IS_TYPE_CLASS(obj->type) &&
        CInit_ConstructGlobalObject(obj, TYPE_CLASS(obj->type), NULL, 0, TRUE)) {
        IsCompleteType(obj->type);
        CInit_DeclareData(obj, NULL, NULL, obj->type->size);
        CInit_CleanupInitInfo(&initinfo);
        return;
    }

    if (IS_TYPE_REFERENCE(obj->type) && CInit_InitReference(obj, FALSE)) {
        CInit_CleanupInitInfo(&initinfo);
        return;
    }

    if (obj->type->size == 0 && !IS_TYPE_ARRAY(obj->type)) {
        CError_Error(ERR_DATA_TYPE_INCOMPLETE);
        CInit_CleanupInitInfo(&initinfo);
        return;
    }

    if (copts.cplusplus) {
        CInit_GenericData(obj, obj->type, obj->qual, &CInit_GlobalStaticInit, FALSE);
    } else {
        CInit_GenericData(obj, obj->type, obj->qual, NULL, FALSE);
    }

    if (IS_TYPE_CLASS(obj->type) && (dtor = CClass_Destructor(TYPE_CLASS(obj->type)))) {
        ENode* expr = CInit_RegisterDtorObject(obj->type, dtor, create_objectrefnode(obj));
        InitExpr_Register(expr, obj);
    }

    CInit_CleanupInitInfo(&initinfo);
}
#endif

Object* CInit_DeclareString(char* data, SInt32 size, Boolean ispascal, Boolean iswide) {
    PooledString* str;
    Object* obj;
    PooledString* scan;

    if (!copts.dont_reuse_strings) {
        for (scan = cinit_stringlist; scan; scan = scan->next) {
            if (scan->size == size &&
                scan->ispascal == ispascal &&
                scan->iswide == iswide &&
                !memcmp(scan->data, data, size)) {
                return scan->obj;
            }
        }
    }

    obj = CParser_NewCompilerDefDataObject();
    obj->name = CParser_GetUniqueName();
    if (iswide) {
        obj->type = CDecl_NewArrayType(CParser_GetWCharType(), size);
    } else {
        obj->type = CDecl_NewArrayType(ispascal ? TYPE(&stunsignedchar) : TYPE(&stchar), size);
    }
    obj->sclass = TK_STATIC;
    CScope_AddGlobalObject(obj);

    if (copts.readonly_strings) {
        CInit_DeclareReadOnlyData(obj, data, NULL, obj->type->size);
    } else {
        CInit_DeclareData(obj, data, NULL, obj->type->size);
    }

    str = galloc(sizeof(PooledString));
    str->next = cinit_stringlist;
    cinit_stringlist = str;
    str->obj = obj;
    str->offset = 0;
    str->size = size;
    str->ispascal = ispascal;
    str->iswide = iswide;
    str->data = galloc(size);
    memcpy(str->data, data, size);

    return obj;
}

PooledString* CInit_DeclarePooledString(char* data, SInt32 size, Boolean ispascal) {
    PooledString* str;
    Object* obj;
    SInt32 offset;

    if (!copts.dont_reuse_strings) {
        for (str = cinit_pooledstringlist; str; str = str->next) {
            if (str->size == size && str->ispascal == ispascal && !memcmp(str->data, data, size)) {
                return str;
            }
        }
    }

    if (cinit_pooledstringlist) {
        obj = cinit_pooledstringlist->obj;
        offset = cinit_pooledstringlist->offset + cinit_pooledstringlist->size;
    } else {
        obj = CInit_CreateStaticDataObject(
            CDecl_NewArrayType(ispascal ? TYPE(&stunsignedchar) : TYPE(&stchar), size),
            0,
            GetHashNameNodeExport("@stringBase0")
        );
#if !MWCCPSP
        PPC_EABI_SetSection(obj, 0, copts.readonly_strings, TRUE);
#endif
        offset = 0;
    }

    str = galloc(sizeof(PooledString));
    str->next = cinit_pooledstringlist;
    cinit_pooledstringlist = str;
    str->obj = obj;
    str->offset = offset;
    str->size = size;
    str->ispascal = ispascal;
    str->data = galloc(size);
    memcpy(str->data, data, size);
    return str;
}

PooledString* CInit_DeclarePooledWString(char* data, SInt32 size) {
    PooledString* str;
    Object* obj;
    SInt32 offset;

    if (!copts.dont_reuse_strings) {
        for (str = cinit_pooledwstringlist; str; str = str->next) {
            if (str->size == size && !memcmp(str->data, data, size)) {
                return str;
            }
        }
    }

    if (cinit_pooledwstringlist) {
        obj = cinit_pooledwstringlist->obj;
        offset = cinit_pooledwstringlist->offset + cinit_pooledwstringlist->size;
    } else {
        obj = CInit_CreateStaticDataObject(
            CDecl_NewArrayType(CParser_GetWCharType(), size),
            0,
            GetHashNameNodeExport("@wstringBase0")
        );
        offset = 0;
    }

    str = galloc(sizeof(PooledString));
    str->next = cinit_pooledwstringlist;
    cinit_pooledwstringlist = str;
    str->obj = obj;
    str->offset = offset;
    str->size = size;
    str->ispascal = FALSE;
    str->data = galloc(size);
    memcpy(str->data, data, size);
    return str;
}

void CInit_RewriteString(ENode* expr, Boolean unused) {
    PooledString* str;
    Boolean is_wide;

#if MWCCPSP
    // 0x504D90: the copts byte at 0x069 allows strings in a precompiled
    // header; nothing is pooled while precompiling
    if (cparamblkptr->precompile == 1 && !copts.cpp_extensions) {
        CError_Error(ERR_ILLEGAL_USE_PRECOMPILED_HEADER);
    }

    CError_ASSERT(6937, expr->rtype->type == TYPEPOINTER);

    is_wide = TYPE_POINTER(expr->rtype)->target->size != 1;
    if (copts.poolstrings && cparamblkptr->precompile == 0) {
#else
    if (cparamblkptr->precompile == 1) {
        CError_Error(ERR_ILLEGAL_USE_PRECOMPILED_HEADER);
    }

    CError_ASSERT(4229, expr->rtype->type == TYPEPOINTER);

    is_wide = TYPE_POINTER(expr->rtype)->target->size != 1;
    if (copts.poolstrings) {
#endif
        if (is_wide) {
            str = CInit_DeclarePooledWString(expr->data.string.data, expr->data.string.size);
        } else {
            str = CInit_DeclarePooledString(
                expr->data.string.data, expr->data.string.size, expr->data.string.ispascal
            );
        }

        if (str->offset) {
            expr->type = EADD;
#if MWCCPSP
            expr->data.diadic.right = intconstnode(TYPE_UNSIGNED_OFFSET, str->offset);
#else
            expr->data.diadic.right = intconstnode(TYPE(&stunsignedlong), str->offset);
#endif
            expr->data.diadic.left = create_objectrefnode(str->obj);
            expr->cost = 1;
        } else {
            expr->type = EOBJREF;
            expr->data.objref = str->obj;
        }
    } else {
        expr->type = EOBJREF;
        expr->data.objref = CInit_DeclareString(
            expr->data.string.data, expr->data.string.size, expr->data.string.ispascal, is_wide
        );
    }
}

void CInit_DeclarePooledStrings(void) {
    SInt32 size;
    char* buffer;
    PooledString* str;

    size = 0;
    for (str = cinit_pooledstringlist; str; str = str->next) {
        size += str->size;
    }

    if (size) {
        cinit_pooledstringlist->obj->type = CDecl_NewArrayType(TYPE(&stchar), size);
        buffer = galloc(size);
        for (str = cinit_pooledstringlist; str; str = str->next) {
            memcpy(buffer + str->offset, str->data, str->size);
        }

        if (copts.readonly_strings) {
            CInit_DeclareReadOnlyData(cinit_pooledstringlist->obj, buffer, NULL, size);
        } else {
            CInit_DeclareData(cinit_pooledstringlist->obj, buffer, NULL, size);
        }
    }

    size = 0;
    for (str = cinit_pooledwstringlist; str; str = str->next) {
        size += str->size;
    }

    if (size) {
        cinit_pooledwstringlist->obj->type = CDecl_NewArrayType(CParser_GetWCharType(), size);
        buffer = galloc(size);
        for (str = cinit_pooledwstringlist; str; str = str->next) {
            memcpy(buffer + str->offset, str->data, str->size);
        }

        if (copts.readonly_strings) {
            CInit_DeclareReadOnlyData(cinit_pooledwstringlist->obj, buffer, NULL, size);
        } else {
            CInit_DeclareData(cinit_pooledwstringlist->obj, buffer, NULL, size);
        }
    }
}

#if MWCCPSP
// 3.0.1: data declared while precompiling (and string literals) is kept in
// a list and emitted at the end of the unit (CInit_DefineTentativeData).
typedef struct CInitDeferredData {
    struct CInitDeferredData* next;
    Object* obj;
    void* data;
    OLinkList* list;
    SInt32 size; // negative: read-only
} CInitDeferredData;

CInitDeferredData* data_00624D38;

#pragma inline_depth(6)
static OLinkList* CInit_CopyOLinkList(OLinkList* list) {
    OLinkList* copy;

    if (!list) {
        return NULL;
    }
    copy = galloc(sizeof(OLinkList));
    *copy = *list;
    copy->next = CInit_CopyOLinkList(copy->next);
    return copy;
}

static void CInit_DeferData(
    Object* obj, void* data, OLinkList* list, SInt32 size, Boolean copy, Boolean is_readonly
) {
    CInitDeferredData* d;

    d = galloc(sizeof(CInitDeferredData));
    d->obj = obj;
    d->data = data;
    d->list = CInit_CopyOLinkList(list);
    d->size = size;
    d->next = data_00624D38;
    data_00624D38 = d;
    if (data && copy) {
        d->data = galloc(obj->type->size);
        memcpy(d->data, data, obj->type->size);
    }
}
#endif
#if MWCCPSP

#endif
static void declaredata(
    Object* obj, void* data, OLinkList* list, SInt32 size, Boolean is_readonly
) {
    OLinkList* scan;
    UInt32 qual;

    qual = obj->qual;

    if (cparamblkptr->precompile == 1) {
#if MWCCPSP
        if (obj->sclass != TK_STATIC &&
            !(qual & (Q_WEAK | Q_IMPLICIT_WEAK)) &&
            !(copts.cpp_extensions && is_const_object(obj))) {
            CError_Error(ERR_ILLEGAL_USE_PRECOMPILED_HEADER);
        }
        CInit_DeferData(obj, data, list, size, TRUE, is_readonly);
        for (scan = list; scan; scan = scan->next) {
            CInline_ObjectAddrRef(scan->obj);
        }
        CInline_ObjectAddrRef(obj);
#else
        PreComp_StaticData(obj, data, list, size);
#endif
        return;
    }

    obj->flags |= OBJECT_DEFINED;
    if (!fatalerrors) {
        for (scan = list; scan; scan = scan->next) {
            CInline_ObjectAddrRef(scan->obj);
        }
        if (copts.filesyminfo) {
            CPrep_SetSourceFile(&cparser_fileoffset);
        }
        if (is_readonly) {
            ObjGen_DeclareReadOnlyData(obj, data, list, size);
        } else {
            ObjGen_DeclareData(obj, data, list, size);
        }
        obj->qual = qual;
    }
}

void CInit_DeclareData(Object* obj, void* data, OLinkList* list, SInt32 size) {
    declaredata(obj, data, list, size, FALSE);
}

void CInit_DeclareReadOnlyData(Object* obj, void* data, OLinkList* list, SInt32 size) {
    declaredata(obj, data, list, size, TRUE);
}

void CInit_DefineTentativeData(void) {
    ObjectList* list;

    for (list = cinit_tentative; list; list = list->next) {
        if (!(list->object->flags & OBJECT_DEFINED)) {
            CInit_DeclareData(list->object, NULL, NULL, list->object->type->size);
        }
    }

    cinit_tentative = NULL;
#if MWCCPSP
    if (cparamblkptr->precompile != 1) {
        CInitDeferredData* d;

        for (d = data_00624D38; d; d = d->next) {
            if (d->obj->flags & OBJECT_REFERENCED) {
                if (d->size < 0) {
                    declaredata(d->obj, d->data, d->list, -d->size, TRUE);
                } else {
                    declaredata(d->obj, d->data, d->list, d->size, FALSE);
                }
            }
        }
        data_00624D38 = NULL;
    }
#endif
}

#if !MWCCPSP
#pragma pack(2)
#endif
#if MWCCPSP

#endif
#if MWCCPSP
// ---------------------------------------------------------------------------
// 3.0.1 initializer engine (being decompiled; the 2.4.7 code above is still
// what our own callers use). An initializer is parsed into a record first
// (CInit_ParseInitRecord) and handed to the static/auto initializers.
// ---------------------------------------------------------------------------

typedef struct CInitDesignator {
    ENode* first; // [first] or GCC's [first ... last]
    ENode* last;
    HashNameNode* member; // .member
} CInitDesignator;

typedef struct CInitItem {
    struct CInitItem* next;
    struct CInitItem* sublist;
    ENode* expr;
    CInitDesignator* designator; // C99 designated initializer (an entry of its own)
    TStreamElement element;
} CInitItem;

typedef enum CInitRecordKind {
    CIR_EXPR, // = expression
    CIR_ARGS, // (expression list)
    CIR_LIST, // = { ... }
    CIR_BRACE // = { left for the caller
} CInitRecordKind;

typedef struct CInitRecord {
    void* data; // ENode*, ENodeList* or CInitItem*
    UInt8 kind;
} CInitRecord;

static Boolean CInit_ParseDesignator(CInitDesignator* desig);
static CInitItem* CInit_ParseInitList(void);

// 0x506530: an object declared without an initializer
void CInit_CheckUninitialized(Object* obj, Boolean hasInit) {
    Type* type;

    if (hasInit || obj->sclass == TK_EXTERN) {
        return;
    }

    type = obj->type;
    switch (type->type) {
        case TYPEPOINTER:
            if (TYPE_POINTER(type)->qual & Q_REFERENCE) {
                CError_Error(ERR_CONST_AMPERSAND_VARIABLE_NEEDS_INITIALIZER);
                return;
            }
            break;
        case TYPEARRAY:
            do {
                type = TPTR_TARGET(type);
            } while (type->type == TYPEARRAY);
            if (type->type != TYPECLASS) {
                break;
            }
        case TYPECLASS:
            if (CClass_Constructor(TYPE_CLASS(type)) != NULL) {
                return;
            }
            return;
    }

    if (copts.cplusplus && is_const_object(obj)) {
        CError_Error(ERR_CONST_AMPERSAND_VARIABLE_NEEDS_INITIALIZER);
    }
}

// 0x50A960
CInitRecord* CInit_ParseInitRecord(Type* type, Boolean parseList) {
    CInitRecord* rec;

    if (tk == '=') {
        rec = lalloc(sizeof(CInitRecord));
        memclrw(rec, sizeof(CInitRecord));
        tk = lex();
        if (tk == '{') {
            if (!parseList) {
                rec->kind = CIR_BRACE;
            } else {
                rec->data = CInit_ParseInitList();
                rec->kind = CIR_LIST;
            }
            tk = lex();
        } else {
            rec->data = assignment_expression();
            rec->kind = CIR_EXPR;
        }
        return rec;
    }

    if (tk == '(') {
        rec = lalloc(sizeof(CInitRecord));
        memclrw(rec, sizeof(CInitRecord));
        tk = lex();
        rec->data = CExpr_ScanExpressionList(TRUE);
        rec->kind = CIR_ARGS;
        if (rec->data == NULL) {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            return NULL;
        }
        tk = lex();
        return rec;
    }

    return NULL;
}

// 0x50AA30
static CInitItem* CInit_ParseInitList(void) {
    CInitItem* head;
    CInitItem* tail;
    CInitDesignator desig;

    if ((tk = lex()) == '}') {
        return NULL;
    }

    head = NULL;
    do {
next_item:
        if (head) {
            tail = tail->next = lalloc(sizeof(CInitItem));
        } else {
            head = tail = lalloc(sizeof(CInitItem));
        }
        tail->next = NULL;
        tail->expr = NULL;
        tail->sublist = NULL;
        tail->designator = NULL;

        switch (tk) {
            case '{':
                tail->element = *CPrep_CurStreamElement();
                tail->sublist = CInit_ParseInitList();
                tk = lex();
                break;
            case '.':
            case '[':
                if (!copts.c9x && !copts.gcc_extensions) {
                    goto bad_designator;
                }
                tail->element = *CPrep_CurStreamElement();
                if (!CInit_ParseDesignator(&desig)) {
                    goto bad_designator;
                }
                tail->designator = lalloc(sizeof(CInitDesignator));
                *tail->designator = desig;
                goto next_item;
bad_designator:
                CError_Error(ERR_ILLEGAL_INITIALIZATION);
                break;
            default:
                tail->expr = pointer_generation(CInit_ParseInitializer(NULL));
                tail->element = *CPrep_CurStreamElement();
                break;
        }

        if (tk == '}') {
            return head;
        }
        if (tk != ',') {
            CError_Error(ERR_COMMA_EXPECTED);
            return head;
        }
    } while ((tk = lex()) != '}');

    return head;
}

// 0x50AC10: .member, [index] or [first ... last], then = or another designator
static Boolean CInit_ParseDesignator(CInitDesignator* desig) {
    desig->first = NULL;
    desig->last = NULL;
    desig->member = NULL;

    switch (tk) {
        case '.':
            if ((tk = lex()) != TK_IDENTIFIER) {
                CError_Error(ERR_IDENTIFIER_EXPECTED);
                return FALSE;
            }
            desig->member = tkidentifier;
            tk = lex();
            break;
        case '[':
            tk = lex();
            desig->first = CExpr_IntegralConstOrDepExpr();
            if (tk == TK_ELLIPSIS) {
                tk = lex();
                desig->last = CExpr_IntegralConstOrDepExpr();
                if (tk == ']') {
                    tk = lex();
                } else {
                    CError_Error(ERR_RBRACKET_EXPECTED);
                }
                if (tk != '=') {
                    CError_Error(ERR_EXPRESSION_SYNTAX_ERROR);
                }
            } else if (tk == ']') {
                tk = lex();
            } else {
                CError_Error(ERR_RBRACKET_EXPECTED);
            }
            break;
        default:
            return FALSE;
    }

    if (tk == '=') {
        tk = lex();
        if (tk == '.' || tk == '[') {
            CError_Error(ERR_EXPRESSION_SYNTAX_ERROR);
        }
    } else if (tk != '.' && tk != '[') {
        CError_Error(ERR_EXPRESSION_SYNTAX_ERROR);
    }
    return TRUE;
}

#pragma auto_inline off
// 0x50AEC0: declare a static object's data; returns the stores for the
// parts that are not constant
static ENode* CInit_DeclareBuffer(Object* obj, CInit_DataBuffer* db) {
    ENode* expr;
    CInit_NonConst* nc;

    expr = NULL;
    if (db->buffer && !db->list && CInit_IsAllZero(db->buffer, db->size)) {
        db->buffer = NULL;
    }

    declaredata(obj, db->buffer, db->list, db->size, FALSE);

    if (db->nonconst) {
        expr = NULL;
        for (nc = db->nonconst; nc; nc = nc->next) {
            expr =
                CInit_InitConcat(expr, create_objectrefnode(obj), nc->offset, nc->type, nc->expr);
        }
    }
    return expr;
}
#pragma auto_inline reset

// the state an object's initialization works in
typedef struct CInitInfo {
    Object* obj;
    Statement* stmt; // insert after this statement, or append when NULL
    Object* guard; // static locals: the guard flag
    CLabel* label; // ... and the label that skips the initialization
    Object* reftemp; // the temporary a local reference was bound to (when keepRefTemp)
    UInt8 kind; // CII_*
    Boolean keepRefTemp;
} CInitInfo;

enum {
    CII_AUTO, // statements in the function
    CII_STATIC, // registered as static initialization (InitExpr_Register)
    CII_LOCALSTATIC // statements guarded by a flag
};

// a position in an initializer: a list item or a lone expression
typedef struct CInitCursor {
    void* cur; // CInitItem* or ENode*
    Boolean islist;
} CInitCursor;

extern Object* CABI_CreateGuard(Object* obj);
extern ENode* CABI_GuardTest(Object* guard, Boolean flag);
extern ENode* CABI_GuardSet(
    Object* guard, Boolean flag
); // the flag is unused (CABI.c defines it with one parameter)

#define CURSOR_ITEM(c) ((CInitItem*)(c)->cur)
static ENode* CInit_GetExpr(CInitCursor* cursor);
static void CInit_FinishBuffer(CInit_DataBuffer* db, Type* type, UInt32 qual);
static ENode* CInit_AutoDataExpr(
    CInit_DataBuffer* db, Object* obj, Type* type, UInt32 qual, Boolean commaObj
);
static void CInit_InitClassObject(
    CInitInfo* ii, SInt32 offset, TypeClass* tclass, ENodeList* args, Boolean flag
);
static ENode* CInit_RegisterDtor(CInitInfo* ii, SInt32 offset, Object* dtor);
static void CInit_EmitExpr(CInitInfo* ii, ENode* expr);
static void CInit_InitClassArray(CInitInfo* ii, TypeClass* tclass);
static void CInit_InitClassArrayList(
    CInitInfo* ii,
    SInt32 offset,
    CInitCursor* cursor,
    TypePointer* tarray,
    UInt32 qual,
    Boolean inList
);
extern void func_00500ED0(Type* a, Type* b, Type* c, AccessType access, Object* obj, void* unk);
extern ENode* func_00518C10(
    ENode* objexpr, ENode* arg, TypeClass* tclass, void* unk, Boolean flag1, Boolean flag2
);

static void CInit_InitTypeGeneric(
    CInit_DataBuffer* db, CInitCursor* cursor, Type* type, UInt32 qual
);
static void CInit_InitStruct(
    CInit_DataBuffer* db, CInitCursor* cursor, TypeStruct* tstruct, UInt32 qual, Boolean inList
);
static void CInit_InitClass(
    CInit_DataBuffer* db, CInitCursor* cursor, TypeClass* tclass, UInt32 qual, Boolean inList
);
static void CInit_InitArray(
    CInit_DataBuffer* db, CInitCursor* cursor, TypePointer* tarray, UInt32 qual, Boolean inList
);

extern ENode* CExpr_BindToReference(
    ENode* expr, Type* type, UInt32 qual, Boolean noRvalue
); // 0x538A10: direct reference binding, or NULL
extern int CExpr_IsReferenceCompatible(
    Type* typeA, UInt32 qualA, Type* typeB, UInt32 qualB
); // 0x53B1E0

// 0x5065C0: ii->obj (a reference) = expr, through a temporary when the
// expression cannot be bound directly
static ENode* CInit_ReferenceExpr(CInitInfo* ii, ENode* expr) {
    Type* target;
    Object* temp;
    Object* saved;
    ENodeList* args;
    ENode* result;
    ENode* assign;

    target = TPTR_TARGET(ii->obj->type);
    if (!(result = CExpr_BindToReference(expr, target, ii->obj->qual, TRUE))) {
        if (ii->kind == CII_AUTO) {
            temp = CParser_NewLocalDataObject(NULL, TRUE);
            temp->name = CParser_GetUniqueName();
            temp->type = target;
            temp->qual = ii->obj->qual;
            CFunc_SetupLocalVarInfo(temp);
            if (ii->keepRefTemp) {
                ii->reftemp = temp;
            }
        } else {
            temp = CParser_NewGlobalDataObject(NULL);
            temp->name = CParser_GetUniqueName();
            temp->nspace = cscope_root;
            temp->type = target;
            temp->qual = ii->obj->qual;
            declaredata(temp, NULL, NULL, target->size, FALSE);
        }

        saved = ii->obj;
        ii->obj = temp;
        if (IS_TYPE_CLASS(target) &&
            IS_TYPE_CLASS(expr->rtype) &&
            CExpr_IsReferenceCompatible(
                target, temp->qual, expr->rtype, expr->flags & ENODE_FLAG_QUALS
            )) {
            // a derived class object: construct it, bind to its base
            temp->type = expr->rtype;
            args = lalloc(sizeof(ENodeList));
            args->next = NULL;
            args->node = expr;
            CInit_InitClassObject(ii, 0, TYPE_CLASS(expr->rtype), args, FALSE);
            result = CClass_ClassPointerCast(
                CExpr_MakeObjRefNode(temp, TRUE),
                TYPE_CLASS(temp->type),
                TYPE_CLASS(target),
                TRUE,
                TRUE,
                TRUE
            );
        } else if (IS_TYPE_CLASS(target) &&
                   (CClass_Constructor(TYPE_CLASS(target)) ||
                    CClass_Destructor(TYPE_CLASS(target)))) {
            args = lalloc(sizeof(ENodeList));
            args->next = NULL;
            args->node = expr;
            CInit_InitClassObject(ii, 0, TYPE_CLASS(target), args, FALSE);
            result = CExpr_MakeObjRefNode(temp, TRUE);
        } else {
            assign = makediadicnode(
                create_objectnode2(temp),
                CExpr_AssignmentPromotion(expr, target, temp->qual, TRUE),
                EASS
            );
            result = makediadicnode(assign, CExpr_MakeObjRefNode(temp, TRUE), ECOMMA);
            result->rtype = result->data.diadic.right->rtype;
        }
        ii->obj = saved;
    }

    return makediadicnode(create_objectnode2(ii->obj), result, EASS);
}

// 0x506830: initialize a buffer for an object of type `type`
static void CInit_InitBuffer(
    CInit_DataBuffer* db, CInitCursor* cursor, Type* type, UInt32 qual, Boolean allowNonConst
) {
    TStreamElement* saved;

    locklheap();
    memclrw(db, sizeof(CInit_DataBuffer));
    db->root = db;
    if (type->size) {
        db->bufferSize = type->size;
    } else if (IS_TYPE_ARRAY(type)) {
        db->bufferSize = TPTR_TARGET(type)->size << 4;
    } else {
        CError_Error(ERR_DATA_TYPE_INCOMPLETE);
    }

    if ((UInt32)db->bufferSize >= 0x40000000) {
        CError_Error(ERR_ILLEGAL_INITIALIZATION);
        db->bufferSize = 100;
    }

    db->buffer = lalloc(db->bufferSize);
    memset(db->buffer, 0, db->bufferSize);
    db->allow_nonconst = allowNonConst;

    switch (type->type) {
        case TYPEARRAY:
            CInit_InitArray(db, cursor, TYPE_POINTER(type), qual, FALSE);
            break;
        case TYPESTRUCT:
            CInit_InitStruct(db, cursor, TYPE_STRUCT(type), qual, FALSE);
            break;
        case TYPECLASS:
            CInit_InitClass(db, cursor, TYPE_CLASS(type), qual, FALSE);
            break;
        default:
            CInit_InitTypeGeneric(db, cursor, type, qual);
            if (!(cursor->islist ? cursor->cur == NULL : (cursor->cur == NULL && tk == '}'))) {
                if (cursor->islist) {
                    if (CURSOR_ITEM(cursor)) {
                        CError_LockErrorPos(&CURSOR_ITEM(cursor)->element, &saved);
                    } else {
                        saved = cerror_locktoken;
                    }
                }
                CError_Error(ERR_TOO_MANY_INITIALIZERS);
                if (cursor->islist) {
                    CError_UnlockErrorPos(&saved);
                }
            }
    }

    CInit_FinishBuffer(db, type, qual);
    unlocklheap();
}

// cursor operations (inlined everywhere in 3.0.1)
static Boolean CInit_CursorAtEnd(CInitCursor* cursor) {
    if (cursor->islist) {
        return cursor->cur == NULL;
    }
    return cursor->cur == NULL && tk == '}';
}

// a value (or a designator) comes next
static Boolean CInit_CursorHasValue(CInitCursor* cursor) {
    if (cursor->islist) {
        return CURSOR_ITEM(cursor) &&
            (CURSOR_ITEM(cursor)->expr || CURSOR_ITEM(cursor)->designator);
    }
    if (cursor->cur) {
        return TRUE;
    }
    return tk != '{' && tk != '}' && tk != ';' && tk != '.' && tk != '[';
}

// an expression (not a designator or a brace) comes next
static Boolean CInit_CursorHasExpr(CInitCursor* cursor) {
    if (cursor->islist) {
        return CURSOR_ITEM(cursor) && CURSOR_ITEM(cursor)->expr;
    }
    if (cursor->cur) {
        return TRUE;
    }
    return tk != '{' && tk != '}' && tk != ';' && tk != '.' && tk != '[';
}

// a braced sublist comes next
static Boolean CInit_CursorIsBrace(CInitCursor* cursor) {
    if (cursor->islist) {
        return CURSOR_ITEM(cursor) &&
            !CURSOR_ITEM(cursor)->designator &&
            (CURSOR_ITEM(cursor)->sublist || !CURSOR_ITEM(cursor)->expr);
    }
    return cursor->cur == NULL && tk == '{';
}

static void CInit_CursorEnter(CInitCursor* cursor, CInitCursor* sub) {
    if (cursor->islist) {
        CError_ASSERT(1712, cursor->cur);
        sub->cur = CURSOR_ITEM(cursor)->sublist;
        sub->islist = TRUE;
    } else {
        CError_ASSERT(1717, tk == '{');
        sub->cur = NULL;
        sub->islist = FALSE;
        tk = lex();
    }
}

static void CInit_CursorNext(CInitCursor* cursor) {
    if (cursor->islist) {
        cursor->cur = CURSOR_ITEM(cursor)->next;
    } else {
        if (tk == ',') {
            tk = lex();
        } else if (tk != '}') {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
        }
        cursor->cur = NULL;
    }
}

static void CInit_CursorLock(CInitCursor* cursor, TStreamElement** saved) {
    if (cursor->islist) {
        if (CURSOR_ITEM(cursor)) {
            CError_LockErrorPos(&CURSOR_ITEM(cursor)->element, saved);
        } else {
            *saved = cerror_locktoken;
        }
    }
}

static void CInit_CursorUnlock(CInitCursor* cursor, TStreamElement** saved) {
    if (cursor->islist) {
        CError_UnlockErrorPos(saved);
    }
}

static void CInit_CursorLeave(CInitCursor* sub) {
    TStreamElement* saved;

    if (sub->islist) {
        if (sub->cur) {
            CInit_CursorLock(sub, &saved);
            CError_Error(ERR_TOO_MANY_INITIALIZERS);
            CInit_CursorUnlock(sub, &saved);
            sub->cur = NULL;
        }
    } else if (tk == '}') {
        tk = lex();
    } else {
        CError_Error(ERR_RBRACE_EXPECTED);
    }
}

// 0x5069C0: initialize one object of type `type` at the buffer position
static void CInit_InitTypeGeneric(
    CInit_DataBuffer* db, CInitCursor* cursor, Type* type, UInt32 qual
) {
    TStreamElement* saved;
    TStreamElement* subsaved;
    CInitCursor sub;
    ENode* expr;
    Type* inner;

    if (CInit_CursorAtEnd(cursor)) {
        return;
    }

    CInit_CursorLock(cursor, &saved);

    switch (type->type) {
        case TYPEVOID:
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            break;

        case TYPEINT:
        case TYPEFLOAT:
        case TYPEENUM:
        case TYPEBITFIELD:
        case TYPEMEMBERPOINTER:
        case TYPEPOINTER:
            if (CInit_CursorHasValue(cursor)) {
                expr = CInit_GetExpr(cursor);
            } else {
                if (!CInit_CursorIsBrace(cursor)) {
                    CError_Error(ERR_ILLEGAL_INITIALIZATION);
                    break;
                }
                CInit_CursorEnter(cursor, &sub);
                CInit_CursorLock(&sub, &subsaved);
                if (!CInit_CursorHasValue(&sub)) {
                    CError_Error(ERR_ILLEGAL_INITIALIZATION);
                    break;
                }
                expr = CInit_GetExpr(&sub);
                CInit_CursorNext(&sub);
                if (!CInit_CursorAtEnd(&sub)) {
                    CError_Error(ERR_TOO_MANY_INITIALIZERS);
                }
                CInit_CursorLeave(&sub);
            }

            switch (type->type) {
                case TYPEINT:
                    CInit_InitTypeInt(db, expr, TYPE_INTEGRAL(type), qual);
                    break;
                case TYPEFLOAT:
                    expr =
                        CExpr_AssignmentPromotion(expr, type, qual & (Q_CONST | Q_VOLATILE), TRUE);
                    if (!IS_TYPE_FLOAT(expr->rtype)) {
                        CError_Error(ERR_ILLEGAL_INITIALIZATION);
                    } else if (ENODE_IS(expr, EFLOATCONST)) {
                        CMach_InitFloatMem(type, expr->data.floatval, db->buffer + db->size);
                    } else {
                        CInit_InitNonConst(db, type, expr);
                    }
                    break;
                case TYPEENUM:
                    expr =
                        CExpr_AssignmentPromotion(expr, type, qual & (Q_CONST | Q_VOLATILE), TRUE);
                    if (!IS_TYPE_ENUM(expr->rtype)) {
                        CError_Error(ERR_ILLEGAL_INITIALIZATION);
                    } else if (ENODE_IS(expr, EINTCONST)) {
                        CMach_InitIntMem(
                            TYPE_ENUM(type)->enumtype, expr->data.intval, db->buffer + db->size
                        );
                    } else {
                        CInit_InitNonConst(db, type, expr);
                    }
                    break;
                case TYPEPOINTER:
                    CInit_InitTypePointer(db, expr, TYPE_POINTER(type), qual);
                    break;
                case TYPEMEMBERPOINTER:
                    expr =
                        CExpr_AssignmentPromotion(expr, type, qual & (Q_CONST | Q_VOLATILE), TRUE);
                    if (ENODE_IS(expr, EINTCONST)) {
                        CMach_InitIntMem(
                            TYPE(&stsignedint), expr->data.intval, db->buffer + db->size
                        );
                    } else {
                        CInit_InitNonConst(db, type, expr);
                    }
                    break;
                case TYPEBITFIELD:
                    inner = TYPE_BITFIELD(type)->bitfieldtype;
                    if (IS_TYPE_ENUM(inner)) {
                        inner = TYPE_ENUM(inner)->enumtype;
                    }
                    expr =
                        CExpr_AssignmentPromotion(expr, inner, qual & (Q_CONST | Q_VOLATILE), TRUE);
                    if (!IS_TYPE_INT(expr->rtype)) {
                        CError_Error(ERR_ILLEGAL_INITIALIZATION);
                    } else if (ENODE_IS(expr, EINTCONST)) {
                        CInit_SetBitfield(
                            TYPE_BITFIELD(type), (UInt8*)db->buffer + db->size, expr->data.intval
                        );
                    } else {
                        CInit_InitNonConst(db, type, expr);
                    }
                    break;
                default:
                    CError_FATAL(3049);
            }

            CInit_CursorNext(cursor);
            break;

        case TYPESTRUCT:
            if (CInit_CursorIsBrace(cursor)) {
                CInit_CursorEnter(cursor, &sub);
                CInit_InitStruct(db, &sub, TYPE_STRUCT(type), qual, FALSE);
                CInit_CursorLeave(&sub);
                CInit_CursorNext(cursor);
            } else {
                CInit_InitStruct(db, cursor, TYPE_STRUCT(type), qual, TRUE);
            }
            break;

        case TYPEARRAY:
            if (CInit_CursorIsBrace(cursor)) {
                CInit_CursorEnter(cursor, &sub);
                CInit_InitArray(db, &sub, TYPE_POINTER(type), qual, FALSE);
                CInit_CursorLeave(&sub);
                CInit_CursorNext(cursor);
            } else {
                CInit_InitArray(db, cursor, TYPE_POINTER(type), qual, TRUE);
            }
            break;

        case TYPECLASS:
            if (CInit_CursorIsBrace(cursor)) {
                CInit_CursorEnter(cursor, &sub);
                CInit_InitClass(db, &sub, TYPE_CLASS(type), qual, FALSE);
                CInit_CursorLeave(&sub);
                CInit_CursorNext(cursor);
            } else {
                CInit_InitClass(db, cursor, TYPE_CLASS(type), qual, TRUE);
            }
            break;

        default:
            CError_FATAL(3097);
    }

    CInit_CursorUnlock(cursor, &saved);
}

// a designator in front of the next value; consumes it
static Boolean CInit_CursorDesignator(CInitCursor* cursor, CInitDesignator* desig) {
    if (cursor->islist) {
        if (CURSOR_ITEM(cursor) && CURSOR_ITEM(cursor)->designator) {
            *desig = *CURSOR_ITEM(cursor)->designator;
            CInit_CursorNext(cursor);
            return TRUE;
        }
        return FALSE;
    }
    if (cursor->cur == NULL) {
        return CInit_ParseDesignator(desig);
    }
    return FALSE;
}

// the member a .member designator names (C99/GCC); `member` otherwise
static StructMember* CInit_DesignatedMember(
    CInitCursor* cursor, TypeStruct* tstruct, StructMember* member
) {
    CInitDesignator desig;

    if ((copts.c9x || copts.gcc_extensions) && CInit_CursorDesignator(cursor, &desig)) {
        if (desig.member) {
            member = tstruct->members;
            do {
                if (member->name == desig.member) {
                    return member;
                }
            } while ((member = member->next));
            CError_Error(ERR_NOT_STRUCT_UNION_CLASS_MEMBER, desig.member->name);
        } else {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
        }
    }
    return member;
}

// the data member a .member designator names (C99/GCC); `ivar` otherwise
static ObjMemberVar* CInit_DesignatedIvar(
    CInitCursor* cursor, TypeClass* tclass, ObjMemberVar* ivar
) {
    CInitDesignator desig;

    if ((copts.c9x || copts.gcc_extensions) && CInit_CursorDesignator(cursor, &desig)) {
        if (desig.member) {
            ivar = tclass->ivars;
            do {
                if (ivar->name == desig.member) {
                    return ivar;
                }
            } while ((ivar = ivar->next));
            CError_Error(ERR_NOT_STRUCT_UNION_CLASS_MEMBER, desig.member->name);
        } else {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
        }
    }
    return ivar;
}

// 0x5075B0: the data members of an aggregate class (C++ struct/union);
// `inList` when the values continue the enclosing list
static void CInit_InitClass(
    CInit_DataBuffer* db, CInitCursor* cursor, TypeClass* tclass, UInt32 qual, Boolean inList
) {
    TStreamElement* saved;
    CInitCursor sub;
    ObjMemberVar* ivar;
    ObjMemberVar* prev;
    ENode* expr;
    SInt32 base;
    UInt32 prevoffset;
    Boolean ok;
    TypePointer flex;

    CInit_CursorLock(cursor, &saved);

    if (tclass->bases || tclass->vtable) {
        CError_Error(ERR_ILLEGAL_INITIALIZATION);
        CInit_CursorUnlock(cursor, &saved);
        return;
    }

    // initialized from a class-valued expression
    if (!CInit_CursorAtEnd(cursor) && CInit_CursorHasExpr(cursor)) {
        expr = CInit_GetExpr(cursor);
        if (expr->rtype == TYPE(tclass) || CExpr_CanImplicitlyConvert(expr, TYPE(tclass), 0)) {
            expr = CExpr_AssignmentPromotion(expr, TYPE(tclass), qual, TRUE);
            if (IS_TYPE_CLASS(expr->rtype)) {
                CInit_InitNonConst(db, TYPE(tclass), expr);
                ok = TRUE;
            } else {
                ok = FALSE;
            }
        } else {
            ok = FALSE;
        }
        if (ok) {
            CInit_CursorNext(cursor);
            CInit_CursorUnlock(cursor, &saved);
            return;
        }
    }

    for (ivar = tclass->ivars; ivar; ivar = ivar->next) {
        if (ivar->access != ACCESSPUBLIC) {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            CInit_CursorUnlock(cursor, &saved);
            return;
        }
    }

    if (!(ivar = tclass->ivars)) {
        if (!CInit_CursorAtEnd(cursor)) {
            CError_Error(ERR_TOO_MANY_INITIALIZERS);
        }
        CInit_CursorUnlock(cursor, &saved);
        return;
    }

    if (CInit_CursorAtEnd(cursor)) {
        goto done;
    }

    base = db->size;
    for (;;) {
        ivar = CInit_DesignatedIvar(cursor, tclass, ivar);
        if (!ivar) {
            CInit_CursorUnlock(cursor, &saved);
            return;
        }

        db->size = base + ivar->offset;
        if (!ivar->type->size) {
            // a flexible array member takes the rest of the values
            if (!inList && IS_TYPE_ARRAY(ivar->type)) {
                flex = *TYPE_POINTER(ivar->type);
                if (CInit_CursorIsBrace(cursor)) {
                    CInit_CursorEnter(cursor, &sub);
                    CInit_InitArray(db, &sub, &flex, ivar->qual, TRUE);
                    CInit_CursorLeave(&sub);
                    CInit_CursorNext(cursor);
                    if (!CInit_CursorAtEnd(cursor)) {
                        CError_Error(ERR_ILLEGAL_INITIALIZATION);
                    }
                } else {
                    CInit_InitArray(db, cursor, &flex, ivar->qual, TRUE);
                }
                db->flexsize = flex.size;
                goto done;
            }
            CError_Error(ERR_TOO_MANY_INITIALIZERS);
            CInit_CursorUnlock(cursor, &saved);
            return;
        }

        CInit_InitTypeGeneric(db, cursor, ivar->type, ivar->qual);
        if (CInit_CursorAtEnd(cursor)) {
            goto done;
        }

        // the next member, skipping the other members of an anonymous union
        prevoffset = ivar->offset;
        prev = ivar;
        while ((ivar = ivar->next) &&
               ivar->anonunion &&
               ivar->offset <= prev->offset &&
               !(IS_TYPE_BITFIELD(ivar->type) &&
                 IS_TYPE_BITFIELD(prev->type) &&
                 TYPE_BITFIELD(ivar->type)->offset != TYPE_BITFIELD(prev->type)->offset)) {}

        ivar = CInit_DesignatedIvar(cursor, tclass, ivar);
        if (!ivar || (tclass->mode == CLASS_MODE_UNION && ivar->offset == prevoffset)) {
            break;
        }
    }

    if (!CInit_CursorAtEnd(cursor) && !inList) {
        CError_Error(ERR_TOO_MANY_INITIALIZERS);
    }
done:
    CInit_CursorUnlock(cursor, &saved);
}

// 0x507E60: the members of a struct or union; `inList` when the values
// continue the enclosing list (no braces of their own)
static void CInit_InitStruct(
    CInit_DataBuffer* db, CInitCursor* cursor, TypeStruct* tstruct, UInt32 qual, Boolean inList
) {
    TStreamElement* saved;
    StructMember* member;
    ENode* expr;
    SInt32 base;
    Boolean ok;
    TypePointer flex;

    CInit_CursorLock(cursor, &saved);

    if (!(member = tstruct->members)) {
        CError_Error(ERR_DATA_TYPE_INCOMPLETE);
        CInit_CursorUnlock(cursor, &saved);
        return;
    }

    if (CInit_CursorAtEnd(cursor)) {
        if (!copts.gcc_extensions) {
            CError_Warning(ERR_ILLEGAL_INITIALIZATION);
        }
        CInit_CursorUnlock(cursor, &saved);
        return;
    }

    // GCC: a struct initialized from a struct-valued expression
    if (db->allow_nonconst && copts.gcc_extensions && CInit_CursorHasExpr(cursor)) {
        expr = CInit_GetExpr(cursor);
        if (expr->rtype == TYPE(tstruct)) {
            expr = CExpr_AssignmentPromotion(expr, TYPE(tstruct), qual, TRUE);
            if (IS_TYPE_STRUCT(expr->rtype)) {
                CInit_InitNonConst(db, TYPE(tstruct), expr);
                ok = TRUE;
            } else {
                ok = FALSE;
            }
        } else {
            ok = FALSE;
        }
        if (ok) {
            CInit_CursorNext(cursor);
            CInit_CursorUnlock(cursor, &saved);
            return;
        }
    }

    base = db->size;
    for (;;) {
        member = CInit_DesignatedMember(cursor, tstruct, member);
        if (!member) {
            CInit_CursorUnlock(cursor, &saved);
            return;
        }

        db->size = base + member->offset;
        if (!member->type->size) {
            // a flexible array member takes the rest of the values
            if (!inList && IS_TYPE_ARRAY(member->type)) {
                flex = *TYPE_POINTER(member->type);
                CInit_InitArray(db, cursor, &flex, member->qual, TRUE);
                db->flexsize = flex.size;
                goto done;
            }
            CError_Error(ERR_TOO_MANY_INITIALIZERS);
            CInit_CursorUnlock(cursor, &saved);
            return;
        }

        CInit_InitTypeGeneric(db, cursor, member->type, member->qual);
        if (CInit_CursorAtEnd(cursor)) {
            goto done;
        }

        do {
            member = member->next;
        } while (member && (member->qual & Q_WEAK));
        if (!member) {
            member = CInit_DesignatedMember(cursor, tstruct, member);
        }
        if (!member || tstruct->stype == STRUCT_TYPE_UNION) {
            break;
        }
    }

    if (!CInit_CursorAtEnd(cursor) && !inList) {
        CError_Error(ERR_TOO_MANY_INITIALIZERS);
    }
done:
    CInit_CursorUnlock(cursor, &saved);
}

static void CInit_InitString(CInit_DataBuffer* db, ENode* expr, TypePointer* tarray, UInt32 qual);
static SInt32 CInit_InitRange(
    CInit_DataBuffer* db,
    CInitCursor* cursor,
    TypePointer* tarray,
    UInt32 qual,
    SInt32 elemsize,
    SInt32 base,
    SInt32 first,
    SInt32 last,
    Boolean unsized
);

// an [index] or [first ... last] designator (C99/GCC); consumes it
static inline Boolean CInit_DesignatedIndex(CInitCursor* cursor, SInt32* first, SInt32* last) {
    CInitDesignator desig;

    if ((copts.c9x || copts.gcc_extensions) && CInit_CursorDesignator(cursor, &desig)) {
        if (!desig.first) {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
        } else if (ENODE_IS(desig.first, EINTCONST) &&
                   (*first = *last = desig.first->data.intval.lo, *first >= 0) &&
                   (!desig.last ||
                    (ENODE_IS(desig.last, EINTCONST) &&
                     (*last = desig.last->data.intval.lo, *last > *first)))) {
            return TRUE;
        } else {
            CError_Error(ERR_ILLEGAL_CONSTANT_EXPRESSION);
        }
    }
    return FALSE;
}

// 0x508400: an array of classes with constructors (or of such arrays) from
// a braced list; each element is constructed from its value
static void CInit_InitClassArrayList(
    CInitInfo* ii,
    SInt32 offset,
    CInitCursor* cursor,
    TypePointer* tarray,
    UInt32 qual,
    Boolean inList
) {
    TStreamElement* saved;
    CInitCursor sub;
    ENodeList* args;
    Type* target;
    SInt32 elemsize;
    SInt32 count;
    SInt32 i;

    CInit_CursorLock(cursor, &saved);

    if (!(elemsize = tarray->target->size)) {
        CError_Error(ERR_DATA_TYPE_INCOMPLETE);
        CInit_CursorUnlock(cursor, &saved);
        return;
    }

    i = 0;
    count = tarray->size / elemsize;
    for (;;) {
        if (!tarray->size) {
            if (CInit_CursorAtEnd(cursor)) {
                if (i > 0 && ii->obj->type == TYPE(tarray)) {
                    tarray->size = offset;
                } else {
                    CError_Error(ERR_ILLEGAL_INITIALIZATION);
                }
                CInit_CursorUnlock(cursor, &saved);
                return;
            }
        } else if (i >= count) {
            if (!CInit_CursorAtEnd(cursor) && !inList) {
                CError_Error(ERR_TOO_MANY_INITIALIZERS);
            }
            CInit_CursorUnlock(cursor, &saved);
            return;
        }

        target = tarray->target;
        if (IS_TYPE_CLASS(target)) {
            if (CInit_CursorAtEnd(cursor)) {
                CInit_InitClassObject(ii, offset, TYPE_CLASS(target), NULL, FALSE);
            } else if (CInit_CursorIsBrace(cursor)) {
                CError_Error(ERR_ILLEGAL_INITIALIZATION);
                CInit_CursorUnlock(cursor, &saved);
                return;
            } else {
                args = lalloc(sizeof(ENodeList));
                args->next = NULL;
                args->node = CInit_GetExpr(cursor);
                CInit_InitClassObject(ii, offset, TYPE_CLASS(target), args, FALSE);
                CInit_CursorNext(cursor);
            }
        } else {
            if (!IS_TYPE_ARRAY(target)) {
                CError_FATAL(2536);
            }
            if (!CInit_CursorAtEnd(cursor) && CInit_CursorIsBrace(cursor)) {
                CInit_CursorEnter(cursor, &sub);
                CInit_InitClassArrayList(
                    ii, offset, &sub, TYPE_POINTER(tarray->target), qual, FALSE
                );
                CInit_CursorLeave(&sub);
                CInit_CursorNext(cursor);
            } else {
                CInit_InitClassArrayList(
                    ii, offset, cursor, TYPE_POINTER(tarray->target), qual, TRUE
                );
            }
        }

        i++;
        offset += elemsize;
    }
}

// 0x5088E0: the elements of an array; an array of unknown size grows
// (in chunks of 16 elements) and gets its size from the initializer
static void CInit_InitArray(
    CInit_DataBuffer* db, CInitCursor* cursor, TypePointer* tarray, UInt32 qual, Boolean inList
) {
    TStreamElement* saved;
    ENode* expr;
    Type* target;
    SInt32 elemsize;
    SInt32 base;
    SInt32 index;
    SInt32 growsize;
    SInt32 first;
    SInt32 last;
    SInt32 end;
    SInt32 pos;
    Boolean unsized;

    CInit_CursorLock(cursor, &saved);
    unsized = tarray->size == 0;

    // char/wchar_t arrays from a string literal
    if (CInit_CursorHasExpr(cursor)) {
        expr = CInit_GetExpr(cursor);
        if (ENODE_IS(expr, ESTRINGCONST) &&
            IS_TYPE_INT(target = tarray->target) &&
            (target->size == 1 || target->size == stwchar.size)) {
            CInit_InitString(db, expr, tarray, qual);
            CInit_CursorNext(cursor);
            if (!CInit_CursorAtEnd(cursor) && !inList) {
                CError_Error(ERR_ILLEGAL_INITIALIZATION);
            }
            CInit_CursorUnlock(cursor, &saved);
            return;
        }
    }

    if (!(elemsize = tarray->target->size)) {
        CError_Error(ERR_DATA_TYPE_INCOMPLETE);
        CInit_CursorUnlock(cursor, &saved);
        return;
    }

    base = db->size;
    growsize = elemsize << 4;
    for (index = 0; !CInit_CursorAtEnd(cursor); index++) {
        if (CInit_DesignatedIndex(cursor, &first, &last)) {
            index = last;
            if (last > first) {
                index = CInit_InitRange(
                            db, cursor, tarray, qual, elemsize, base, first, last, unsized
                        ) -
                    1;
                continue;
            }
        }

        if (unsized) {
            end = (index + 1) * elemsize;
            pos = base + end;
            db->size = pos - elemsize - db->offset;
            if (db->size + elemsize > db->bufferSize) {
                db = CInit_GrowBuffer(db, growsize);
            }
            CInit_InitTypeGeneric(db, cursor, tarray->target, qual);
            tarray->size = end;
            db->size = pos - db->offset;
            continue;
        }

        if (tarray->size <= index * elemsize) {
            index--;
            CError_Error(ERR_TOO_MANY_INITIALIZERS);
        }
        db->size = base + index * elemsize;
        CInit_InitTypeGeneric(db, cursor, tarray->target, qual);
        if (tarray->size > (index + 1) * elemsize) {
            continue;
        }

        if (!CInit_DesignatedIndex(cursor, &first, &last)) {
            break;
        }
        index = last;
        if (last > first) {
            index = CInit_InitRange(db, cursor, tarray, qual, elemsize, base, first, last, unsized);
        }
        index--;
    }

    if (!CInit_CursorAtEnd(cursor) && !inList) {
        CError_Error(ERR_TOO_MANY_INITIALIZERS);
    }
    CInit_CursorUnlock(cursor, &saved);
}

// 0x508FB0: GCC's [first ... last] = value, for scalar elements; returns
// the index after the range
static SInt32 CInit_InitRange(
    CInit_DataBuffer* db,
    CInitCursor* cursor,
    TypePointer* tarray,
    UInt32 qual,
    SInt32 elemsize,
    SInt32 base,
    SInt32 first,
    SInt32 last,
    Boolean unsized
) {
    ENode* expr;
    ENode* val;
    Type* target;
    SInt32 end;

    if (!CInit_CursorHasExpr(cursor)) {
        CError_Error(ERR_ILLEGAL_INITIALIZATION);
        return last;
    }

    expr = CInit_GetExpr(cursor);
    CInit_CursorNext(cursor);

    for (; first <= last; first++) {
        if (unsized) {
            end = (first + 1) * elemsize;
            db->size = base + end - elemsize - db->offset;
            if (db->size + elemsize > db->bufferSize) {
                db = CInit_GrowBuffer(db, elemsize << 4);
            }
        } else {
            if (tarray->size <= first * elemsize) {
                first--;
                CError_Error(ERR_TOO_MANY_INITIALIZERS);
            }
            db->size = base + first * elemsize;
        }

        target = tarray->target;
        switch (target->type) {
            case TYPEINT:
                CInit_InitTypeInt(db, expr, TYPE_INTEGRAL(target), qual);
                break;
            case TYPEFLOAT:
                val = CExpr_AssignmentPromotion(expr, target, qual & (Q_CONST | Q_VOLATILE), TRUE);
                if (!IS_TYPE_FLOAT(val->rtype)) {
                    CError_Error(ERR_ILLEGAL_INITIALIZATION);
                } else if (ENODE_IS(val, EFLOATCONST)) {
                    CMach_InitFloatMem(target, val->data.floatval, db->buffer + db->size);
                } else {
                    CInit_InitNonConst(db, target, val);
                }
                break;
            case TYPEENUM:
                val = CExpr_AssignmentPromotion(expr, target, qual & (Q_CONST | Q_VOLATILE), TRUE);
                if (!IS_TYPE_ENUM(val->rtype)) {
                    CError_Error(ERR_ILLEGAL_INITIALIZATION);
                } else if (ENODE_IS(val, EINTCONST)) {
                    CMach_InitIntMem(
                        TYPE_ENUM(target)->enumtype, val->data.intval, db->buffer + db->size
                    );
                } else {
                    CInit_InitNonConst(db, target, val);
                }
                break;
            case TYPEPOINTER:
                CInit_InitTypePointer(db, expr, TYPE_POINTER(target), qual);
                break;
            case TYPEMEMBERPOINTER:
                val = CExpr_AssignmentPromotion(expr, target, qual & (Q_CONST | Q_VOLATILE), TRUE);
                if (ENODE_IS(val, EINTCONST)) {
                    CMach_InitIntMem(TYPE(&stsignedint), val->data.intval, db->buffer + db->size);
                } else {
                    CInit_InitNonConst(db, target, val);
                }
                break;
            default:
                CError_Error(ERR_ILLEGAL_INITIALIZATION);
                return last;
        }

        if (unsized) {
            tarray->size = end;
            db->size = base + end - db->offset;
        }
    }

    return first;
}

// 0x50A200: a char or wchar_t array from a string literal
static void CInit_InitString(CInit_DataBuffer* db, ENode* expr, TypePointer* tarray, UInt32 qual) {
    SInt32 size;

    expr = pointer_generation(expr);
    if (!ENODE_IS(expr, ESTRINGCONST) ||
        !IS_TYPE_INT(tarray->target) ||
        (tarray->target->size != 1 && tarray->target->size != stwchar.size)) {
        CError_Error(ERR_ILLEGAL_INITIALIZATION);
        return;
    }

    if (IS_TYPE_POINTER_ONLY(expr->rtype) &&
        tarray->target->size != TPTR_TARGET(expr->rtype)->size) {
        CError_Warning(ERR_ILLEGAL_INITIALIZATION);
    }

    size = expr->data.string.size;
    if (tarray->size) {
        if (size > tarray->size) {
            if (copts.cplusplus || (size - 1) > tarray->size) {
                CError_Error(ERR_TOO_MANY_INITIALIZERS);
            }
            expr->data.string.size = tarray->size;
            size = tarray->size;
        }
        memcpy(db->buffer + db->size, expr->data.string.data, size);
    } else {
        tarray->size = size;
        if (db->bufferSize < size) {
            db = CInit_GrowBuffer(db, size);
        }
        memcpy(db->buffer, expr->data.string.data, size);
        db->size = size;
    }
}

extern Object* func_005043C0(TypeClass* tclass); // CClass_DefaultConstructor

// 0x509340: default-construct the class array ii->obj (no initializer)
static void CInit_InitClassArray(CInitInfo* ii, TypeClass* tclass) {
    Object* ctor;
    Object* dtor;
    Object* funcobj;
    Object* regmem;
    TypeFunc* tfunc;
    ENode* expr;
    ENode* dtor_expr;
    SInt32 count;
    SInt32 i;

    dtor = CClass_Destructor(tclass);
    count = ii->obj->type->size / tclass->size;
    if (CClass_Constructor(tclass)) {
        if (!(ctor = func_005043C0(tclass)) && !(ctor = CClass_DummyDefaultConstructor(tclass))) {
            CError_Error(ERR_CLASS_NO_DEFAULT_CONSTRUCTOR);
            return;
        }
    } else {
        ctor = NULL;
    }
    if (ctor) {
        func_00500ED0(TYPE(tclass), TYPE(tclass), TYPE(tclass), ctor->access, ctor, NULL);
    }
    if (dtor) {
        func_00500ED0(TYPE(tclass), TYPE(tclass), TYPE(tclass), dtor->access, dtor, NULL);
    }

    if (!ii->stmt && (count <= 1 || (ii->kind == CII_AUTO && count <= 8))) {
        for (i = 0; i < count; i++) {
            CInit_InitClassObject(ii, i * tclass->size, tclass, NULL, TRUE);
        }
        if (dtor) {
            CInit_EmitExpr(ii, nullnode());
        }
        return;
    }

    if (!ctor) {
        expr = nullnode();
    } else if (!dtor && ii->kind != CII_STATIC) {
        CInit_EmitExpr(ii, nullnode());
        ii->stmt = CInit_ConstructClassArray(
            ii->stmt, tclass, ctor, dtor, create_objectrefnode(ii->obj), count
        );
        expr = nullnode();
    } else {
        if (dtor) {
            dtor_expr = create_objectrefnode(CABI_GetDestructorObject(dtor, CABIDestroyComplete));
        } else {
            dtor_expr = nullnode();
        }
        expr = CExpr_FuncCallSix(
            carr_func,
            create_objectrefnode(ii->obj),
            create_objectrefnode(ctor),
            dtor_expr,
            intconstnode(TYPE(&stunsignedlong), tclass->size),
            intconstnode(TYPE(&stunsignedlong), count),
            NULL
        );
    }

    if (ii->kind == CII_AUTO) {
        CInit_EmitExpr(ii, expr);
        if (dtor) {
            CExcept_RegisterLocalArray(NULL, ii->obj, dtor, count, tclass->size);
            CInit_EmitExpr(ii, nullnode());
        }
        return;
    }

    if (dtor && !copts.no_static_dtors) {
        tfunc = galloc(sizeof(TypeFunc));
        memclrw(tfunc, sizeof(TypeFunc));
        tfunc->type = TYPEFUNC;
        tfunc->functype = &stvoid;
        CDecl_SetFuncFlags(tfunc, 1);

        funcobj = CParser_NewCompilerDefFunctionObject();
        funcobj->name = CParser_AppendUniqueName("__arraydtor");
        funcobj->type = TYPE(tfunc);
        funcobj->sclass = TK_STATIC;
        funcobj->qual = Q_INLINE;

        CParser_RegisterSingleExprFunction(
            funcobj,
            funccallexpr(
                darr_func,
                create_objectrefnode(ii->obj),
                create_objectrefnode(CABI_GetDestructorObject(dtor, CABIDestroyComplete)),
                intconstnode(TYPE(&stsignedlong), tclass->size),
                intconstnode(TYPE(&stsignedlong), count)
            )
        );

        expr = makediadicnode(expr, nullnode(), ECOMMA);
        expr->rtype = TYPE(&void_ptr);

        regmem = CInit_CreateStaticDataObject(CInit_GetRegMemType(), 0, NULL);
        declaredata(regmem, NULL, NULL, regmem->type->size, FALSE);
        expr = funccallexpr(
            Xgreg_func, expr, create_objectrefnode(funcobj), create_objectrefnode(regmem), NULL
        );
    }
    CInit_EmitExpr(ii, expr);
}

// 0x5099F0: construct the class object at ii->obj + offset (from `args`)
// and register its destruction
static void CInit_InitClassObject(
    CInitInfo* ii, SInt32 offset, TypeClass* tclass, ENodeList* args, Boolean flag
) {
    NameSpaceObjectList* ctor;
    Object* dtor;
    ENode* objexpr;
    ENode* expr;

    ctor = CClass_Constructor(tclass);
    dtor = CClass_Destructor(tclass);
    if (!ctor && !dtor && !args) {
        return;
    }

    if (dtor) {
        func_00500ED0(TYPE(tclass), TYPE(tclass), TYPE(tclass), dtor->access, dtor, NULL);
        if (!ctor) {
            if (args) {
                CError_Error(ERR_ILLEGAL_INITIALIZATION);
            }
            expr = CInit_RegisterDtor(ii, offset, dtor);
            if (ii->kind != CII_AUTO) {
                CInit_EmitExpr(ii, expr);
            }
            return;
        }
    }

    objexpr = create_objectrefnode(ii->obj);
    if (offset) {
        objexpr = makediadicnode(objexpr, intconstnode(TYPE(&stunsignedint), offset), EADD);
    }
    objexpr->rtype = CDecl_NewPointerType(TYPE(tclass));

    if (args && !args->next) {
        expr = func_00518C10(objexpr, args->node, tclass, NULL, TRUE, flag);
    } else {
        expr = CExpr_ConstructObject(tclass, objexpr, args, FALSE, TRUE, FALSE, TRUE, flag);
        if (!IS_TYPE_POINTER_ONLY(expr->rtype)) {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
        }
    }
    CInit_EmitExpr(ii, expr);

    if (dtor) {
        expr = CInit_RegisterDtor(ii, offset, dtor);
        if (ii->kind != CII_AUTO) {
            CInit_EmitExpr(ii, expr);
        } else if (!offset && ii->obj->type == TYPE(tclass)) {
            CInit_EmitExpr(ii, nullnode());
        }
    }
}

// 0x509B70: destroy ii->obj + offset at the end of its lifetime: a local's
// destructor action, or a static's registration with the runtime
static ENode* CInit_RegisterDtor(CInitInfo* ii, SInt32 offset, Object* dtor) {
    ENode* objexpr;
    ENode* expr;
    Object* regmem;

    if (ii->kind == CII_AUTO) {
        return CExcept_RegisterDestructorObject(ii->obj, offset, dtor, FALSE);
    }

    objexpr = create_objectrefnode(ii->obj);
    if (offset) {
        objexpr = makediadicnode(objexpr, intconstnode(TYPE(&stunsignedint), offset), EADD);
    }
    if (copts.no_static_dtors) {
        return objexpr;
    }

    expr = CExpr_NewENode(EFUNCCALL);
    expr->cost = 4;
    expr->rtype = TYPE(&void_ptr);
    expr->data.funccall.funcref = create_objectrefnode(Xgreg_func);
    expr->data.funccall.functype = TYPE_FUNC(Xgreg_func->type);
    expr->data.funccall.args = lalloc(sizeof(ENodeList));
    expr->data.funccall.args->node = objexpr;
    expr->data.funccall.args->next = lalloc(sizeof(ENodeList));
    expr->data.funccall.args->next->node =
        create_objectrefnode(CABI_GetDestructorObject(dtor, CABIDestroyComplete));
    expr->data.funccall.args->next->next = lalloc(sizeof(ENodeList));
    regmem = CInit_CreateStaticDataObject(CInit_GetRegMemType(), 0, NULL);
    declaredata(regmem, NULL, NULL, regmem->type->size, FALSE);
    expr->data.funccall.args->next->next->node = create_objectrefnode(regmem);
    expr->data.funccall.args->next->next->next = NULL;
    return expr;
}

// 0x509D00
static void CInit_EmitExpr(CInitInfo* ii, ENode* expr) {
    Statement* stmt;

    if (!expr) {
        return;
    }

    switch (ii->kind) {
        case CII_LOCALSTATIC:
            if (!ii->guard) {
                ii->guard = CABI_CreateGuard(ii->obj);
                ii->label = newlabel();
                if (ii->stmt) {
                    ii->stmt = CFunc_InsertStatement(ST_IFGOTO, ii->stmt);
                    ii->stmt->dobjstack = cexcept_dobjstack;
                    stmt = ii->stmt;
                } else {
                    stmt = CFunc_AppendStatement(ST_IFGOTO);
                }
                stmt->expr = CABI_GuardTest(ii->guard, TRUE);
                stmt->label = ii->label;
            }
        case CII_AUTO:
            if (ii->stmt) {
                ii->stmt = CFunc_InsertStatement(ST_EXPRESSION, ii->stmt);
                ii->stmt->dobjstack = cexcept_dobjstack;
                stmt = ii->stmt;
            } else {
                stmt = CFunc_AppendStatement(ST_EXPRESSION);
            }
            stmt->expr = expr;
            break;
        case CII_STATIC:
            InitExpr_Register(expr, ii->obj);
            break;
        default:
            CError_FATAL(1951);
    }
}

// not inlined in 3.0.1 (our compiler would)
#pragma auto_inline off
// 0x509DF0: the current expression
static ENode* CInit_GetExpr(CInitCursor* cursor) {
    if (cursor->islist) {
        CError_ASSERT(1802, cursor->cur);
        if (CURSOR_ITEM(cursor)->designator != NULL) {
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            return nullnode();
        }
        CError_ASSERT(1808, CURSOR_ITEM(cursor)->expr);
        return CURSOR_ITEM(cursor)->expr;
    }

    if (cursor->cur == NULL) {
        CError_ASSERT(1815, tk != '{');
        cursor->cur = pointer_generation(CInit_ParseInitializer(NULL));
    }
    return cursor->cur;
}
#pragma auto_inline reset

// 0x509E90: the expression that initializes a local (or, with obj NULL, a
// temporary) from a finished buffer: a copy from a static image, or a
// memset call when the image is all zero, then the non-constant parts.
// `commaObj`: an array's value follows the copy.
static ENode* CInit_AutoDataExpr(
    CInit_DataBuffer* db, Object* obj, Type* type, UInt32 qual, Boolean commaObj
) {
    NameSpaceName* nsname;
    CInit_NonConst* entry;
    ENode* expr;
    ENode* obj_expr;
    ENode* indirect_expr;
    Type* newtype;
    Object* newobj;
    Object* alias;

    if (!obj && !cscope_currentfunc) {
        obj = CParser_NewCompilerDefDataObject();
        obj->name = CParser_GetUniqueName();
        obj->type = type;
        obj->qual = qual;
        obj->sclass = TK_STATIC;
        CScope_AddGlobalObject(obj);
    }

    if (db->flexsize) {
        type = CDecl_NewStructType(type->size + db->flexsize, CMach_GetTypeAlign(type));
        if (obj) {
            if (IS_TYPE_STRUCT(obj->type) || IS_TYPE_CLASS(obj->type)) {
                // keep the declared type under a new name for later references
                CError_ASSERT(1487, !cscope_current->is_hash);
                nsname = CScope_FindNameSpaceName(cscope_current, obj->name);
                CError_ASSERT(1488, nsname);
                CError_ASSERT(1489, nsname->first.object == OBJ_BASE(obj));
                CError_ASSERT(1490, !nsname->first.next);
                nsname->name = CParser_AppendUniqueName(obj->name->name);
                alias = CParser_NewAliasObject(obj, 0);
                alias->type = obj->type;
            }
            obj->type = type;
        }
    }

    if (obj) {
        obj_expr = create_objectrefnode(obj);
    } else {
        obj_expr = CExpr_NewETEMPNode(type, TRUE);
    }

    if (IS_TYPE_ARRAY((newtype = type))) {
        newtype = CDecl_NewStructType(type->size, CMach_GetTypeAlign(type));
    }
    newobj = CInit_CreateStaticDataObject(newtype, 0, NULL);
    if (db->list || !CInit_IsAllZero(db->buffer, db->size)) {
        declaredata(newobj, db->buffer, db->list, db->size, FALSE);
        indirect_expr = makemonadicnode(obj_expr, EINDIRECT);
        indirect_expr->rtype = newtype;
        expr = makediadicnode(indirect_expr, create_objectnode(newobj), EASS);
    } else {
        expr = funccallexpr(
            clear_func, obj_expr, intconstnode(CABI_GetSizeTType(), newtype->size), NULL, NULL
        );
    }

    for (entry = db->nonconst; entry; entry = entry->next) {
        expr = CInit_InitConcat(expr, obj_expr, entry->offset, entry->type, entry->expr);
    }

    if (obj) {
        if (commaObj && IS_TYPE_ARRAY(type)) {
            expr = makecommaexpression(expr, create_objectnode(obj));
        }
    } else {
        indirect_expr = makemonadicnode(CExpr_CopyENode(obj_expr), EINDIRECT);
        indirect_expr->rtype = type;
        indirect_expr->flags = qual & ENODE_FLAG_QUALS;
        expr = makecommaexpression(expr, indirect_expr);
    }

    return expr;
}

// 0x50B010: merge a chain of buffers into the first one
static void CInit_FinishBuffer(CInit_DataBuffer* db, Type* type, UInt32 qual) {
    SInt32 size;
    char* buffer;
    CInit_DataBuffer* scan;

    size = type->size + db->flexsize;
    if (size) {
        if (db->next) {
            buffer = lalloc(size);
            for (scan = db; scan; scan = scan->next) {
                CError_ASSERT(331, scan->offset + scan->size <= size);
                memcpy(buffer + scan->offset, scan->buffer, scan->size);
            }
            db->buffer = buffer;
        }
    } else {
        CError_Error(ERR_ILLEGAL_INITIALIZATION);
    }
    db->size = size;
    db->next = NULL;
}

// 0x506790: the expression of a scalar's initializer
static ENode* CInit_RecordExpr(CInitRecord* rec, Boolean unused) {
    CInitItem* item;
    ENode* expr;

    if (rec) {
        switch (rec->kind) {
            case CIR_EXPR:
                return rec->data;
            case CIR_ARGS:
                CError_ASSERT(3173, rec->data);
                if (((ENodeList*)rec->data)->next != NULL) {
                    CError_Error(ERR_TOO_MANY_INITIALIZERS);
                }
                return ((ENodeList*)rec->data)->node;
            case CIR_LIST:
                CError_ASSERT(3178, rec->data);
                item = rec->data;
                if ((expr = item->expr) && !item->next && !item->sublist) {
                    return expr;
                }
                break;
            default:
                CError_FATAL(3188);
        }
    }

    CError_Error(ERR_ILLEGAL_INITIALIZATION);
    return NULL;
}

extern void CFunc_NameLocalStaticDataObject(Object* obj);

// 0x50A6F0: a const object with a constant initializer becomes inline data
static Boolean CInit_InlineConst(Object* obj, ENode* expr) {
    Type* type;

    if (!is_const_object(obj) || is_volatile_object(obj)) {
        return FALSE;
    }

    expr = pointer_generation(expr);
    type = obj->type;
    switch (type->type) {
        case TYPEENUM:
            type = TYPE_ENUM(type)->enumtype;
        case TYPEINT:
            if (!ENODE_IS(expr, EINTCONST)) {
                return FALSE;
            }
            obj->u.data.u.intconst = CExpr_IntConstConvert(type, expr->rtype, expr->data.intval);
            break;
        case TYPEFLOAT:
            if (!ENODE_IS(expr, EFLOATCONST)) {
                return FALSE;
            }
            obj->u.data.u.floatconst = galloc(sizeof(Float));
            *obj->u.data.u.floatconst = CMach_CalcFloatConvert(obj->type, expr->data.floatval);
            break;
        case TYPEPOINTER:
            while (ENODE_IS(expr, ETYPCON)) {
                expr = expr->data.monadic;
            }
            if (!ENODE_IS(expr, EINTCONST)) {
                return FALSE;
            }
            obj->u.data.u.intconst = expr->data.intval;
            break;
        default:
            return FALSE;
    }

    obj->qual |= Q_INLINE_DATA;
    if (obj->datatype == DLOCAL) {
        obj->sclass = TK_STATIC;
        obj->datatype = DDATA;
        obj->nspace = cscope_root;
        CFunc_NameLocalStaticDataObject(obj);
    } else if (obj->sclass != TK_STATIC || (obj->flags & OBJECT_REFERENCED)) {
        CInit_ExportConst(obj);
    }
    return TRUE;
}

extern Boolean func_00503C90(Type* type);

// 0x5054F0: initialize a static object (`isLocal`: a static local, whose
// dynamic initialization is guarded); the record comes from
// CInit_ParseInitRecord. `stmt` is where a static local's statements go.
void CInit_StaticObject(Object* obj, CInitRecord* rec, Statement* stmt, Boolean isLocal) {
    CInit_DataBuffer db;
    CInitInfo ii;
    CInitCursor cursor;
    TStreamElement* saved;
    Type* type;
    ENode* expr;
    ENodeList* args;
    Object* dtor;
    Boolean flag;
    Boolean ok;

    CInit_CheckUninitialized(obj, rec != NULL);
    memclrw(&ii, sizeof(CInitInfo));
    ii.obj = obj;
    ii.stmt = stmt;
    ii.kind = isLocal ? CII_LOCALSTATIC : CII_STATIC;

    type = obj->type;
    if (IS_TYPE_POINTER_ONLY(type) && (TYPE_POINTER(type)->qual & Q_REFERENCE)) {
        if ((expr = CInit_RecordExpr(rec, FALSE))) {
            CInit_EmitExpr(&ii, CInit_ReferenceExpr(&ii, expr));
            declaredata(obj, NULL, NULL, obj->type->size, FALSE);
        }
        goto finish;
    }

    if (IS_TYPE_CLASS(type) &&
        (CClass_Constructor(TYPE_CLASS(type)) || CClass_Destructor(TYPE_CLASS(type)))) {
        args = NULL;
        flag = TRUE;
        if (rec) {
            switch (rec->kind) {
                case CIR_EXPR:
                    args = lalloc(sizeof(ENodeList));
                    args->next = NULL;
                    args->node = rec->data;
                    flag = FALSE;
                    break;
                case CIR_ARGS:
                    args = rec->data;
                    break;
                case CIR_LIST:
                case CIR_BRACE:
                    if (!func_00503C90(obj->type)) {
                        CError_Error(ERR_ILLEGAL_INITIALIZATION);
                        return;
                    }
                    goto aggregate;
                default:
                    CError_FATAL(3608);
            }
        }
        CInit_InitClassObject(&ii, 0, TYPE_CLASS(obj->type), args, flag);
        declaredata(obj, NULL, NULL, obj->type->size, FALSE);
        goto finish;
    }

    type = obj->type;
    if (IS_TYPE_ARRAY(type)) {
        do {
            type = TPTR_TARGET(type);
        } while (IS_TYPE_ARRAY(type));
        if (IS_TYPE_CLASS(type) &&
            (CClass_Constructor(TYPE_CLASS(type)) || CClass_Destructor(TYPE_CLASS(type)))) {
            if (!rec) {
                CInit_InitClassArray(&ii, TYPE_CLASS(type));
                declaredata(obj, NULL, NULL, obj->type->size, FALSE);
                goto finish;
            }
            if (rec->kind == CIR_LIST || rec->kind == CIR_BRACE) {
                if (rec->kind == CIR_LIST) {
                    cursor.cur = rec->data;
                    cursor.islist = TRUE;
                } else {
                    cursor.cur = NULL;
                    cursor.islist = FALSE;
                }
                CInit_InitClassArrayList(
                    &ii, 0, &cursor, TYPE_POINTER(obj->type), obj->qual, FALSE
                );
                CInit_CursorLeave(&cursor);
                declaredata(obj, NULL, NULL, obj->type->size, FALSE);
                goto finish;
            }
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
        }
    }

    if (!rec) {
        if (copts.cplusplus && is_const_object(obj)) {
            CError_Error(ERR_CONST_AMPERSAND_VARIABLE_NEEDS_INITIALIZER);
        }
        declaredata(obj, NULL, NULL, obj->type->size, FALSE);
        return;
    }

    switch (rec->kind) {
        case CIR_ARGS:
            args = rec->data;
            if (!args || args->next) {
                CError_Error(ERR_ILLEGAL_INITIALIZATION);
                return;
            }
            expr = args->node;
            break;
        case CIR_EXPR:
            expr = rec->data;
            break;
        default:
            if (rec->kind != CIR_LIST && rec->kind != CIR_BRACE) {
                CError_FATAL(3742);
            }
aggregate:
            if (rec->kind == CIR_LIST) {
                cursor.cur = rec->data;
                cursor.islist = TRUE;
            } else {
                cursor.cur = NULL;
                cursor.islist = FALSE;
            }
            CInit_InitBuffer(&db, &cursor, obj->type, obj->qual, copts.cplusplus);
            CInit_CursorLeave(&cursor);
            if (IS_TYPE_CLASS(obj->type) && (dtor = CClass_Destructor(TYPE_CLASS(obj->type)))) {
                func_00500ED0(obj->type, obj->type, obj->type, dtor->access, dtor, NULL);
                CInit_EmitExpr(&ii, CInit_RegisterDtor(&ii, 0, dtor));
            }
            goto declare;
    }

    if (CInit_InlineConst(obj, expr)) {
        return;
    }

    type = obj->type;
    memclrw(&db, sizeof(CInit_DataBuffer));
    db.root = &db;
    if (type->size) {
        db.bufferSize = type->size;
    } else if (IS_TYPE_ARRAY(type)) {
        db.bufferSize = TPTR_TARGET(type)->size << 4;
    } else {
        CError_Error(ERR_DATA_TYPE_INCOMPLETE);
    }
    if ((UInt32)db.bufferSize >= 0x40000000) {
        CError_Error(ERR_ILLEGAL_INITIALIZATION);
        db.bufferSize = 100;
    }
    db.buffer = lalloc(db.bufferSize);
    memset(db.buffer, 0, db.bufferSize);
    db.allow_nonconst = copts.cplusplus;

    type = obj->type;
    switch (type->type) {
        case TYPEVOID:
        case TYPEFUNC:
        case TYPEBITFIELD:
        case TYPELABEL:
        case TYPETEMPLATE:
        case TYPEOBJCID:
        case TYPETEMPLDEPEXPR:
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            break;
        case TYPESTRUCT:
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            break;
        case TYPEINT:
            CInit_InitTypeInt(&db, expr, TYPE_INTEGRAL(type), obj->qual);
            break;
        case TYPEFLOAT:
            expr = CExpr_AssignmentPromotion(expr, type, obj->qual & (Q_CONST | Q_VOLATILE), TRUE);
            if (!IS_TYPE_FLOAT(expr->rtype)) {
                CError_Error(ERR_ILLEGAL_INITIALIZATION);
            } else if (ENODE_IS(expr, EFLOATCONST)) {
                CMach_InitFloatMem(type, expr->data.floatval, db.buffer + db.size);
            } else {
                CInit_InitNonConst(&db, type, expr);
            }
            break;
        case TYPEENUM:
            expr = CExpr_AssignmentPromotion(expr, type, obj->qual & (Q_CONST | Q_VOLATILE), TRUE);
            if (!IS_TYPE_ENUM(expr->rtype)) {
                CError_Error(ERR_ILLEGAL_INITIALIZATION);
            } else if (ENODE_IS(expr, EINTCONST)) {
                CMach_InitIntMem(TYPE_ENUM(type)->enumtype, expr->data.intval, db.buffer + db.size);
            } else {
                CInit_InitNonConst(&db, type, expr);
            }
            break;
        case TYPEPOINTER:
            CInit_InitTypePointer(&db, expr, TYPE_POINTER(type), obj->qual);
            break;
        case TYPEMEMBERPOINTER:
            expr = CExpr_AssignmentPromotion(expr, type, obj->qual & (Q_CONST | Q_VOLATILE), TRUE);
            if (ENODE_IS(expr, EINTCONST)) {
                CMach_InitIntMem(TYPE(&stsignedint), expr->data.intval, db.buffer + db.size);
            } else {
                CInit_InitNonConst(&db, type, expr);
            }
            break;
        case TYPEARRAY:
            CInit_InitString(&db, expr, TYPE_POINTER(type), obj->qual);
            break;
        case TYPECLASS:
            if (expr->rtype == type || CExpr_CanImplicitlyConvert(expr, type, 0)) {
                expr = CExpr_AssignmentPromotion(expr, type, obj->qual, TRUE);
                if (IS_TYPE_CLASS(expr->rtype)) {
                    CInit_InitNonConst(&db, type, expr);
                    ok = TRUE;
                } else {
                    ok = FALSE;
                }
            } else {
                ok = FALSE;
            }
            if (!ok) {
                CError_Error(ERR_ILLEGAL_INITIALIZATION);
            }
            break;
        default:
            CError_FATAL(3735);
    }
    CInit_FinishBuffer(&db, obj->type, obj->qual);

declare:
    if ((expr = CInit_DeclareBuffer(obj, &db))) {
        CInit_EmitExpr(&ii, expr);
    }

finish:
    if (ii.guard) {
        if (ii.stmt) {
            ii.stmt = CFunc_InsertStatement(ST_EXPRESSION, ii.stmt);
            ii.stmt->dobjstack = cexcept_dobjstack;
            stmt = ii.stmt;
        } else {
            stmt = CFunc_AppendStatement(ST_EXPRESSION);
        }
        stmt->expr = CABI_GuardSet(ii.guard, TRUE);
        if (ii.stmt) {
            ii.stmt = CFunc_InsertStatement(ST_LABEL, ii.stmt);
            ii.stmt->dobjstack = cexcept_dobjstack;
            stmt = ii.stmt;
        } else {
            stmt = CFunc_AppendStatement(ST_LABEL);
        }
        stmt->label = ii.label;
        ii.label->stmt = stmt;
    }
}

// 0x505D90: initialize a local (automatic) object
void CInit_LocalObject(Object* obj, CInitRecord* rec, Statement* stmt, Boolean flag) {
    CInit_DataBuffer db;
    CInitInfo ii;
    CInitCursor cursor;
    Type* type;
    ENode* expr;
    ENode* result;
    ENodeList* args;
    Object* dtor;
    ExceptionAction* ea;
    Boolean argflag;

    result = NULL;
    CInit_CheckUninitialized(obj, rec != NULL);
    memclrw(&ii, sizeof(CInitInfo));
    ii.obj = obj;
    ii.stmt = stmt;
    ii.kind = CII_AUTO;
    ii.keepRefTemp = flag;

    type = obj->type;
    if (IS_TYPE_POINTER_ONLY(type) && (TYPE_POINTER(type)->qual & Q_REFERENCE)) {
        if ((expr = CInit_RecordExpr(rec, FALSE))) {
            CInit_EmitExpr(&ii, CInit_ReferenceExpr(&ii, expr));
            if (ii.reftemp && stmt) {
                // the reference was bound to a temporary: its destruction
                // actions now name the temporary
                do {
                    for (ea = stmt->dobjstack; ea; ea = ea->prev) {
                        if (ea->type == 0x13 && ea->data.destroy_local.local == obj) {
                            ea->data.destroy_local.local = ii.reftemp;
                        }
                    }
                } while ((stmt = stmt->next));
            }
        }
        return;
    }

    switch (type->type) {
        case TYPEVOID:
        case TYPEFUNC:
        case TYPEBITFIELD:
        case TYPELABEL:
        case TYPETEMPLATE:
        case TYPETEMPLDEPEXPR:
            CError_Error(ERR_ILLEGAL_INITIALIZATION);
            break;

        case TYPEINT:
        case TYPEFLOAT:
        case TYPEENUM:
        case TYPEMEMBERPOINTER:
        case TYPEPOINTER:
        case TYPEOBJCID:
            if (rec && (expr = CInit_RecordExpr(rec, TRUE))) {
                if (CInit_InlineConst(obj, expr)) {
                    result = NULL;
                } else {
                    result = makediadicnode(
                        create_objectnode2(obj),
                        CExpr_AssignmentPromotion(
                            expr, obj->type, obj->qual & (Q_CONST | Q_VOLATILE), TRUE
                        ),
                        EASS
                    );
                }
            } else {
                result = NULL;
            }
            break;

        case TYPEARRAY:
            do {
                type = TPTR_TARGET(type);
            } while (IS_TYPE_ARRAY(type));
            if (IS_TYPE_CLASS(type) &&
                (CClass_Constructor(TYPE_CLASS(type)) || CClass_Destructor(TYPE_CLASS(type)))) {
                if (!rec) {
                    CInit_InitClassArray(&ii, TYPE_CLASS(type));
                } else if (rec->kind == CIR_LIST || rec->kind == CIR_BRACE) {
                    if (rec->kind == CIR_LIST) {
                        cursor.cur = rec->data;
                        cursor.islist = TRUE;
                    } else {
                        cursor.cur = NULL;
                        cursor.islist = FALSE;
                    }
                    CInit_InitClassArrayList(
                        &ii, 0, &cursor, TYPE_POINTER(obj->type), obj->qual, FALSE
                    );
                    CInit_CursorLeave(&cursor);
                } else {
                    CError_Error(ERR_ILLEGAL_INITIALIZATION);
                }
                break;
            }
            if (!rec) {
                break;
            }
            if (rec->kind == CIR_LIST || rec->kind == CIR_BRACE) {
                if (rec->kind == CIR_LIST) {
                    cursor.cur = rec->data;
                    cursor.islist = TRUE;
                } else {
                    cursor.cur = NULL;
                    cursor.islist = FALSE;
                }
                CInit_InitBuffer(
                    &db,
                    &cursor,
                    obj->type,
                    obj->qual,
                    copts.cplusplus || copts.gcc_extensions || copts.c9x || obj == NULL
                );
                result = CInit_AutoDataExpr(&db, obj, obj->type, obj->qual, FALSE);
                CInit_CursorLeave(&cursor);
                break;
            }
            if (rec->kind != CIR_EXPR) {
                CError_Error(ERR_ILLEGAL_INITIALIZATION);
                break;
            }
            // a string literal
            type = obj->type;
            memclrw(&db, sizeof(CInit_DataBuffer));
            db.root = &db;
            if (type->size) {
                db.bufferSize = type->size;
            } else if (IS_TYPE_ARRAY(type)) {
                db.bufferSize = TPTR_TARGET(type)->size << 4;
            } else {
                CError_Error(ERR_DATA_TYPE_INCOMPLETE);
            }
            if ((UInt32)db.bufferSize >= 0x40000000) {
                CError_Error(ERR_ILLEGAL_INITIALIZATION);
                db.bufferSize = 100;
            }
            db.buffer = lalloc(db.bufferSize);
            memset(db.buffer, 0, db.bufferSize);
            CInit_InitString(&db, rec->data, TYPE_POINTER(obj->type), obj->qual);
            CInit_FinishBuffer(&db, obj->type, obj->qual);
            result = CInit_AutoDataExpr(&db, obj, obj->type, obj->qual, FALSE);
            break;

        case TYPECLASS:
            if (CClass_Constructor(TYPE_CLASS(type)) || CClass_Destructor(TYPE_CLASS(type))) {
                args = NULL;
                argflag = TRUE;
                if (rec) {
                    switch (rec->kind) {
                        case CIR_EXPR:
                            args = lalloc(sizeof(ENodeList));
                            args->next = NULL;
                            args->node = rec->data;
                            argflag = FALSE;
                            break;
                        case CIR_ARGS:
                            args = rec->data;
                            break;
                        case CIR_LIST:
                        case CIR_BRACE:
                            if (!func_00503C90(obj->type)) {
                                CError_Error(ERR_ILLEGAL_INITIALIZATION);
                                return;
                            }
                            goto aggregate;
                        default:
                            CError_FATAL(3473);
                    }
                }
                CInit_InitClassObject(&ii, 0, TYPE_CLASS(obj->type), args, argflag);
                return;
            }
        case TYPESTRUCT:
            if (rec) {
                if (rec->kind == CIR_LIST || rec->kind == CIR_BRACE) {
aggregate:
                    if (rec->kind == CIR_LIST) {
                        cursor.cur = rec->data;
                        cursor.islist = TRUE;
                    } else {
                        cursor.cur = NULL;
                        cursor.islist = FALSE;
                    }
                    CInit_InitBuffer(
                        &db,
                        &cursor,
                        obj->type,
                        obj->qual,
                        copts.cplusplus || copts.gcc_extensions || copts.c9x || obj == NULL
                    );
                    result = CInit_AutoDataExpr(&db, obj, obj->type, obj->qual, FALSE);
                    CInit_CursorLeave(&cursor);
                } else if (rec && (expr = CInit_RecordExpr(rec, TRUE))) {
                    if (CInit_InlineConst(obj, expr)) {
                        result = NULL;
                    } else {
                        result = makediadicnode(
                            create_objectnode2(obj),
                            CExpr_AssignmentPromotion(
                                expr, obj->type, obj->qual & (Q_CONST | Q_VOLATILE), TRUE
                            ),
                            EASS
                        );
                    }
                } else {
                    result = NULL;
                }
            }
            if (IS_TYPE_CLASS(obj->type) && (dtor = CClass_Destructor(TYPE_CLASS(obj->type)))) {
                func_00500ED0(obj->type, obj->type, obj->type, dtor->access, dtor, NULL);
                CInit_RegisterDtor(&ii, 0, dtor);
            }
            break;

        default:
            CError_FATAL(3509);
    }

    CInit_EmitExpr(&ii, result);
}

#endif
