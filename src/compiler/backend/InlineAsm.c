#include "compiler/InlineAsm.h"
#include "compiler/CError.h"
#include "compiler/CExpr.h"
#include "compiler/CFunc.h"
#include "compiler/CInit.h"
#include "compiler/CInline.h"
#include "compiler/CInt64.h"
#include "compiler/CMachine.h"
#include "compiler/CompilerTools.h"
#include "compiler/COptimizer.h"
#include "compiler/CParser.h"
#include "compiler/CPrep.h"
#include "compiler/CPrepTokenizer.h"
#include "compiler/CScope.h"
#include "compiler/GCCInlineAsm.h"
#include "compiler/InlineAsmPPC.h"
#if MWCCPSP
#include "compiler/InlineAsmMIPS.h"
#endif
#include "compiler/objects.h"
#include "compiler/PCode.h"
#include "compiler/Registers.h"
#include "compiler/scopes.h"
#include "compiler/tokens.h"
#include "compiler/types.h"
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

int allow_array_expressions = TRUE;

int backtracking;
jmp_buf backtrack;
jmp_buf InlineAsm_assemblererror;
static char buffer[1024];
static int ASMstmtnb;

Boolean InlineAsm_setjmp_valid;

SInt32 InlineAsm_ConstantExpression(void);
#if MWCCPSP
CInt64 InlineAsm_Int64Expression(void);
Boolean InlineAsm_LookupSymbolMIPS(HashNameNode* name, MLookupResult* result);
#endif

#if MWCCPSP
// 3.0.1: no setjmp_valid flag; a syntax error during backtracking tries the
// mnemonic's next opcode (InlineAsmMIPS.c)
void AssemblerError(void) {
    longjmp(InlineAsm_assemblererror, 1);
}

void InlineAsm_SyntaxError(short code) {
    if (backtracking) {
        longjmp(backtrack, 1);
    }
    CError_Error(code);
}
#else
void AssemblerError(void) {
    if (InlineAsm_setjmp_valid) {
        longjmp(InlineAsm_assemblererror, 1);
    }
}

void InlineAsm_SyntaxError(short code) {
    if (backtracking) {
        longjmp(backtrack, 1);
    }

    if (tk == TK_EOL || tk == ';') {
        code = ERR_UNEXPECTED_END_LINE;
    }
    CError_Error(code);
}
#endif

CLabel* InlineAsm_LookupLabel(HashNameNode* name) {
    CLabel* label;

    for (label = Labels; label; label = label->next) {
        if (name == label->name) {
            break;
        }
    }

    return label;
}

CLabel* InlineAsm_DeclareLabel(HashNameNode* name) {
    CLabel* label = newlabel();
    label->name = name;
    label->next = Labels;
    Labels = label;
    return label;
}

static void InlineAsm_DefineLabel(HashNameNode* name) {
    CLabel* label;
    Statement* stmt;

    if (!(label = InlineAsm_LookupLabel(name))) {
        label = InlineAsm_DeclareLabel(name);
    } else {
        if (label->stmt) {
            CError_Error(ERR_LABEL_REDEFINED, name->name);
        }
    }

    stmt = CFunc_AppendStatement(ST_LABEL);
    stmt->label = label;
    label->stmt = stmt;
}

#if !MWCCPSP // 3.0.1 has InlineAsm_LookupSymbolMIPS (0x555190/0x5551B0 are other functions)
Boolean InlineAsm_LookupSymbolOrTag(HashNameNode* name, IALookupResult* result, Boolean allow_tag) {
    Object* object;
    NameSpace* nspace;
    NameSpaceObjectList* list;

    result->name = name;
    result->object = NULL;
    result->label = NULL;
    result->type = NULL;
    result->has_value = FALSE;

    if ((result->label = InlineAsm_LookupLabel(name))) {
        return TRUE;
    }

    for (nspace = cscope_current; nspace; nspace = nspace->parent) {
        if ((list = CScope_FindName(nspace, name))) {
            switch (list->object->otype) {
                case OT_ENUMCONST:
                    result->has_value = TRUE;
                    result->value = OBJ_ENUM_CONST(list->object)->val.lo;
                    return TRUE;
                case OT_OBJECT:
                    object = OBJECT(list->object);
                    if (object->datatype == DABSOLUTE) {
                        result->has_value = TRUE;
                        result->value = object->u.address;
                    } else {
                        if (object->datatype == DDATA && (object->qual & Q_INLINE_DATA)) {
                            CInit_ExportConst(object);
                        }
                        result->object = object;
                    }
                    return TRUE;
                case OT_TYPE:
                    result->type = OBJ_TYPE(list->object)->type;
                    return TRUE;
                case OT_TYPETAG:
                    if (allow_tag) {
                        result->type = OBJ_TYPE_TAG(list->object)->type;
                        return TRUE;
                    }
                case OT_NAMESPACE:
                case OT_MEMBERVAR:
                    return FALSE;
                default:
                    CError_FATAL(248);
            }
        }
    }

    return FALSE;
}

Boolean InlineAsm_LookupSymbol(HashNameNode* name, IALookupResult* result) {
    return InlineAsm_LookupSymbolOrTag(name, result, FALSE);
}
#endif
#if MWCCPSP

#endif
#if MWCCPSP
// 3.0.1's lookup for the MIPS assembler: values are 64-bit, no tags
Boolean InlineAsm_LookupSymbolMIPS(HashNameNode* name, MLookupResult* result) {
    CInt64 value;
    Object* object;
    NameSpace* nspace;
    NameSpaceObjectList* list;

    result->name = name;
    result->object = NULL;
    result->label = NULL;
    result->type = NULL;
    result->has_value = FALSE;

    if ((result->label = InlineAsm_LookupLabel(name))) {
        return TRUE;
    }

    for (nspace = cscope_current; nspace; nspace = nspace->parent) {
        if ((list = CScope_FindName(nspace, name))) {
            switch (list->object->otype) {
                case OT_ENUMCONST:
                    result->has_value = TRUE;
                    result->value = OBJ_ENUM_CONST(list->object)->val;
                    return TRUE;
                case OT_OBJECT:
                    object = OBJECT(list->object);
                    if (object->datatype == DABSOLUTE) {
                        result->has_value = TRUE;
                        CInt64_SetLong(&value, object->u.address);
                        result->value = value;
                    } else {
                        if (object->datatype == DDATA && (object->qual & Q_INLINE_DATA)) {
                            CInit_ExportConst(object);
                        }
                        if (object->datatype == DALIAS) {
                            result->object = object->u.alias.object;
                        } else {
                            result->object = object;
                        }
                    }
                    return TRUE;
                case OT_TYPE:
                    result->type = OBJ_TYPE(list->object)->type;
                    return TRUE;
                case OT_TYPETAG:
                case OT_NAMESPACE:
                case OT_MEMBERVAR:
                    return FALSE;
                default:
                    CError_FATAL(237);
            }
        }
    }

    return FALSE;
}
#endif

#if MWCCPSP
// 3.0.1: a member of the class or of a direct base
static ObjMemberVar* isclassmember(TypeClass* tclass, HashNameNode* name) {
    NameSpaceObjectList* list;
    ClassList* base;

    list = CScope_FindName(tclass->nspace, name);
    if (list && list->object->otype == OT_MEMBERVAR) {
        return OBJ_MEMBER_VAR(list->object);
    }
    for (base = tclass->bases; base; base = base->next) {
        list = CScope_FindName(base->base->nspace, name);
        if (list && list->object->otype == OT_MEMBERVAR) {
            return OBJ_MEMBER_VAR(list->object);
        }
    }
    return NULL;
}
#else
static ObjMemberVar* isbaseclassmember(ClassList* bases, HashNameNode* name) {
    ObjMemberVar* ivar = NULL;
    NameSpaceObjectList* list;
    TypeClass* base;

    for (; bases && (base = bases->base); bases = ivar ? NULL : bases->next) {
        list = CScope_FindName(base->nspace, name);
        if (list && list->object->otype == OT_MEMBERVAR) {
            ivar = galloc(sizeof(ObjMemberVar));
            memclrw(ivar, sizeof(ObjMemberVar));
            *ivar = *OBJ_MEMBER_VAR(list->object);
            ivar->offset += bases->offset;
        } else {
            ivar = isbaseclassmember(base->bases, name);
        }
    }

    return ivar;
}

static ObjMemberVar* isclassmember(TypeClass* tclass, HashNameNode* name) {
    NameSpaceObjectList* list;
    ObjMemberVar* ivar;

    list = CScope_FindName(tclass->nspace, name);
    if (list && list->object->otype == OT_MEMBERVAR) {
        return OBJ_MEMBER_VAR(list->object);
    }
    if ((ivar = isbaseclassmember(tclass->bases, name))) {
        return ivar;
    }
    return NULL;
}
#endif

SInt32 InlineAsm_StructMemberOffset(Type* type) {
    StructMember* member;
    ObjMemberVar* ivar;
    SInt32 offset = 0;

    do {
        if (IS_TYPE_STRUCT(type)) {
            tk = lex();
            if (tk != TK_IDENTIFIER) {
                InlineAsm_SyntaxError(ERR_IDENTIFIER_EXPECTED);
            }
            member = ismember(TYPE_STRUCT(type), tkidentifier);
            if (!member) {
                CError_Error(ERR_NOT_STRUCT_UNION_CLASS_MEMBER, tkidentifier->name);
            }
            offset += member->offset;
            type = member->type;
            tk = lex();
        } else if (IS_TYPE_CLASS(type)) {
            tk = lex();
            if (tk != TK_IDENTIFIER) {
                InlineAsm_SyntaxError(ERR_IDENTIFIER_EXPECTED);
            }
            ivar = isclassmember(TYPE_CLASS(type), tkidentifier);
            if (!ivar) {
                CError_Error(ERR_NOT_STRUCT_UNION_CLASS_MEMBER, tkidentifier->name);
            }
            offset += ivar->offset;
            type = ivar->type;
            tk = lex();
        } else {
            CError_Error(ERR_NOT_STRUCT_UNION_CLASS);
        }
    } while (tk == '.');

    return offset;
}

SInt32 InlineAsm_StructArrayMemberOffset(Type* type) {
    StructMember* member;
    ObjMemberVar* ivar;
    SInt32 offset = 0;

    do {
        if (tk == '.') {
            if (IS_TYPE_STRUCT(type)) {
                tk = lex();
                if (tk != TK_IDENTIFIER) {
                    InlineAsm_SyntaxError(ERR_IDENTIFIER_EXPECTED);
                }
                member = ismember(TYPE_STRUCT(type), tkidentifier);
                if (!member) {
                    CError_Error(ERR_NOT_STRUCT_UNION_CLASS_MEMBER, tkidentifier->name);
                }
                offset += member->offset;
                type = member->type;
                tk = lex();
            } else if (IS_TYPE_CLASS(type)) {
                tk = lex();
                if (tk != TK_IDENTIFIER) {
                    InlineAsm_SyntaxError(ERR_IDENTIFIER_EXPECTED);
                }
                ivar = isclassmember(TYPE_CLASS(type), tkidentifier);
                if (!ivar) {
                    CError_Error(ERR_NOT_STRUCT_UNION_CLASS_MEMBER, tkidentifier->name);
                }
                offset += ivar->offset;
                type = ivar->type;
                tk = lex();
            } else {
                CError_Error(ERR_NOT_STRUCT_UNION_CLASS);
            }
        } else {
            if (IS_TYPE_ARRAY(type)) {
                type = TPTR_TARGET(type);
                tk = lex();
                offset += type->size * InlineAsm_ConstantExpression();
                if (tk != ']') {
                    InlineAsm_SyntaxError(ERR_RBRACKET_EXPECTED);
                }
                tk = lex();
            } else {
                CError_Error(ERR_POINTER_ARRAY_REQUIRED);
            }
        }
    } while (tk == '.' || tk == '[');

    return offset;
}

#if MWCCPSP
// 3.0.1: 64-bit constant expressions
static CInt64 PrimaryExpression(void) {
    MLookupResult result;
    CInt64 value;

    CInt64_SetULong(&value, 0);
    switch (tk) {
        case TK_IDENTIFIER:
            if (InlineAsm_LookupSymbolMIPS(tkidentifier, &result)) {
                if (result.has_value) {
                    tk = lex();
                    return result.value;
                }

                if (result.type && (IS_TYPE_STRUCT(result.type) || IS_TYPE_CLASS(result.type))) {
                    tk = lex();
                    if (tk != '.') {
                        InlineAsm_SyntaxError(ERR_UNEXPECTED_TOKEN);
                    }
                    if (allow_array_expressions) {
                        CInt64_SetLong(&value, InlineAsm_StructArrayMemberOffset(result.type));
                    } else {
                        CInt64_SetLong(&value, InlineAsm_StructMemberOffset(result.type));
                    }
                    return value;
                } else {
                    InlineAsm_SyntaxError(ERR_ILLEGAL_CONSTANT_EXPRESSION);
                }
            } else {
                InlineAsm_SyntaxError(ERR_ILLEGAL_CONSTANT_EXPRESSION);
            }
            break;
        case TK_INTCONST:
            value = tkintconst;
            tk = lex();
            return value;
        case TK_SIZEOF:
            CInt64_SetLong(&value, scansizeof());
            return value;
        case '+':
            tk = lex();
            return PrimaryExpression();
        case '-':
            tk = lex();
            return CInt64_Neg(PrimaryExpression());
        case '!':
            tk = lex();
            return CInt64_Not(PrimaryExpression());
        case '~':
            tk = lex();
            return CInt64_Inv(PrimaryExpression());
        case '(':
            tk = lex();
            value = InlineAsm_Int64Expression();
            if (tk != ')') {
                InlineAsm_SyntaxError(ERR_RPAREN_EXPECTED);
            }
            tk = lex();
            return value;
        default:
            InlineAsm_SyntaxError(ERR_UNEXPECTED_TOKEN);
    }

    return value;
}

static CInt64 ConstantExpressionTail(CInt64 value) {
    CInt64 right;
    short left_token;
    short right_prec;

    while (1) {
        left_token = tk;
        tk = lex();
        right = PrimaryExpression();

        right_prec = GetPrec(tk);
        if (right_prec == 0) {
            return CMach_CalcIntDiadic(TYPE(&stsignedlonglong), value, left_token, right);
        }

        if (GetPrec(left_token) >= right_prec) {
            value = CMach_CalcIntDiadic(TYPE(&stsignedlonglong), value, left_token, right);
        } else {
            value = CMach_CalcIntDiadic(
                TYPE(&stsignedlonglong), value, left_token, ConstantExpressionTail(right)
            );
            if (GetPrec(tk) == 0) {
                return value;
            }
        }
    }
}

CInt64 InlineAsm_Int64Expression(void) {
    CInt64 left = PrimaryExpression();
    CInt64 value;

    if (GetPrec(tk) != 0) {
        value = ConstantExpressionTail(left);
    } else {
        value = left;
    }
    return value;
}

SInt32 InlineAsm_ConstantExpression(void) {
    CInt64 value = InlineAsm_Int64Expression();
    return CInt64_GetULong(&value);
}
#else
SInt32 InlineAsm_StructPointerMemberOffset(Type* type) {
    StructMember* member;
    ObjMemberVar* ivar;
    SInt32 offset;

    tk = lex();
    if (tk != TK_IDENTIFIER) {
        InlineAsm_SyntaxError(ERR_IDENTIFIER_EXPECTED);
    }

    if (IS_TYPE_STRUCT(type)) {
        member = ismember(TYPE_STRUCT(type), tkidentifier);
        if (!member) {
            CError_Error(ERR_NOT_STRUCT_UNION_CLASS_MEMBER, tkidentifier->name);
        }
        offset = member->offset;
        type = member->type;
    } else {
        ivar = isclassmember(TYPE_CLASS(type), tkidentifier);
        if (!ivar) {
            CError_Error(ERR_NOT_STRUCT_UNION_CLASS_MEMBER, tkidentifier->name);
        }
        offset = ivar->offset;
        type = ivar->type;
    }

    tk = lex();
    if (tk == '.' || tk == '[') {
        offset += InlineAsm_StructArrayMemberOffset(type);
    }

    return offset;
}

static SInt32 DiadicOperator(SInt32 left, short op, SInt32 right) {
    CInt64 left64;
    CInt64 right64;
    CInt64_SetLong(&left64, left);
    CInt64_SetLong(&right64, right);
    right64 = CMach_CalcIntDiadic(TYPE(&stsignedint), left64, op, right64);
    return CInt64_GetULong(&right64);
}

static SInt32 PrimaryExpression(void) {
    IALookupResult result;
    SInt32 value;

    switch (tk) {
        case TK_IDENTIFIER:
            if (InlineAsm_LookupSymbol(tkidentifier, &result)) {
                if (result.has_value) {
                    tk = lex();
                    return result.value;
                }

                if (result.type && (IS_TYPE_STRUCT(result.type) || IS_TYPE_CLASS(result.type))) {
                    tk = lex();
                    if (tk != '.') {
                        InlineAsm_SyntaxError(ERR_UNEXPECTED_TOKEN);
                    }
                    if (allow_array_expressions) {
                        return InlineAsm_StructArrayMemberOffset(result.type);
                    } else {
                        return InlineAsm_StructMemberOffset(result.type);
                    }
                } else {
                    InlineAsm_SyntaxError(ERR_ILLEGAL_CONSTANT_EXPRESSION);
                }
            } else {
                InlineAsm_SyntaxError(ERR_ILLEGAL_CONSTANT_EXPRESSION);
            }
            break;
        case TK_INTCONST:
            value = tkintconst.lo;
            tk = lex();
            return value;
        case TK_SIZEOF:
            return scansizeof();
        case '+':
            tk = lex();
            return PrimaryExpression();
        case '-':
            tk = lex();
            return -PrimaryExpression();
        case '!':
            tk = lex();
            return !PrimaryExpression();
        case '~':
            tk = lex();
            return ~PrimaryExpression();
        case '(':
            tk = lex();
            value = InlineAsm_ConstantExpression();
            if (tk != ')') {
                InlineAsm_SyntaxError(ERR_RPAREN_EXPECTED);
            }
            tk = lex();
            return value;
        default:
            InlineAsm_SyntaxError(ERR_UNEXPECTED_TOKEN);
    }

    return 0;
}

static SInt32 ConstantExpressionTail(SInt32 value) {
    SInt32 right;
    short left_token;
    short right_prec;

    while (1) {
        left_token = tk;
        tk = lex();
        right = PrimaryExpression();

        right_prec = GetPrec(tk);
        if (right_prec == 0) {
            return DiadicOperator(value, left_token, right);
        }

        if (GetPrec(left_token) >= right_prec) {
            value = DiadicOperator(value, left_token, right);
        } else {
            value = DiadicOperator(value, left_token, ConstantExpressionTail(right));
            if (GetPrec(tk) == 0) {
                return value;
            }
        }
    }
}

SInt32 InlineAsm_ConstantExpression(void) {
    SInt32 value = PrimaryExpression();

    if (GetPrec(tk) != 0) {
        value = ConstantExpressionTail(value);
    }
    return value;
}
#endif

HashNameNode* MakeLocalLabel(CInt64 num) {
    char buf[80];
    sprintf(buf, "@%i_%i", ASMstmtnb, (int)CInt64_GetULong(&num));
    return GetHashNameNodeExport(buf);
}

static void ScanOptionalLabel(void) {
    if (tk == TK_INTCONST) {
        if (lookahead() != ':') {
            return;
        }
        InlineAsm_DefineLabel(MakeLocalLabel(tkintconst));
        tk = lex();
        tk = lex();
        return;
    } else if (tkidentifier->name[0] == '@') {
        InlineAsm_DefineLabel(tkidentifier);
        tk = lex();
        if (tk == ':') {
            tk = lex();
        }
    } else {
        HashNameNode* name = tkidentifier;
        short t = lookahead();
        tkidentifier = name;
        if (t != ':') {
            return;
        }
        InlineAsm_DefineLabel(tkidentifier);
        tk = lex();
        tk = lex();
    }
}

#if MWCCPSP
extern void func_00483F30(void); // set up the MIPS assembler
extern void func_00483C40(void); // an assembler directive
extern void func_00483B20(void); // an instruction
extern void func_0053D790(void); // GCC-style operands after ':'

// 3.0.1 (0x52B660): asm statements up to endToken, for asm functions and
// asm blocks; GCC-style "..." strings switch to GCC mode
void ScanStatements(volatile short endToken) {
    func_00483F30();
    InlineAsm_gccmode = FALSE;
    if (setjmp(InlineAsm_assemblererror)) {
        while (tk != ';' && tk != TK_EOL && tk != endToken && tk != '}' && tk) {
            tk = lex();
        }
        if (tk == ';' || tk == TK_EOL) {
            tk = lex();
        }
    }

    while (tk && tk != endToken) {
        backtracking = FALSE;
        sourceoffset = CPrep_GetFileOffsetInfo(&cparser_fileoffset);
        if (tk == '"') {
            if (InlineAsm_gccmode) {
                tk = lex();
                while (tk == TK_EOL) {
                    CPrep_TokenStreamFlush();
                    tk = lex();
                    sourceoffset = CPrep_GetFileOffsetInfo(&cparser_fileoffset);
                }
                if (tk == '"') {
                    tk = lex();
                    while (tk == TK_EOL) {
                        CPrep_TokenStreamFlush();
                        tk = lex();
                    }
                }
                if (tk == ':') {
                    func_0053D790();
                    InlineAsm_gccmode = FALSE;
                }
                if (tk == ';' || tk == TK_EOL) {
                    CPrep_TokenStreamFlush();
                    tk = lex();
                }
                if (tk == endToken) {
                    break;
                }
            } else {
                InlineAsm_gccmode = TRUE;
                copts.cplusplus = FALSE;
                tk = lex();
            }
        }

        if (tk == '.') {
            func_00483C40();
        } else {
            if (tk == TK_IDENTIFIER) {
                ScanOptionalLabel();
            } else if (tk == TK_INTCONST) {
                ScanOptionalLabel();
            }
            if (tk != TK_EOL && tk != '"') {
                func_00483B20();
            }
        }

        if (tk == ';' || tk == TK_EOL) {
            CPrep_TokenStreamFlush();
            tk = lex();
        } else if (tk != TK_STRING && tk != '"' && tk != TK_IDENTIFIER && tk != endToken) {
            if (endToken == ')') {
                CError_Error(ERR_RPAREN_EXPECTED);
            } else {
                CError_Error(ERR_END_LINE_EXPECTED);
            }
        }
    }
}
#else
static void ScanStatements(volatile short endToken, AssemblerType mode) {
    if (setjmp(InlineAsm_assemblererror)) {
        InlineAsm_setjmp_valid = FALSE;
        while (tk != TK_EOL && tk != endToken && tk != '}' && tk) {
            tk = lex();
        }
        if (tk == ';' || tk == TK_EOL) {
            tk = lex();
        }
        return;
    }

    InlineAsm_setjmp_valid = TRUE;
    InlineAsm_Initialize(mode);
    InlineAsm_gccmode = FALSE;
    if (setjmp(InlineAsm_assemblererror)) {
        InlineAsm_setjmp_valid = FALSE;
        while (tk != ';' && tk != TK_EOL && tk != endToken && tk != '}' && tk) {
            tk = lex();
        }
        if (tk == ';' || tk == TK_EOL) {
            tk = lex();
        }
    }
    InlineAsm_setjmp_valid = TRUE;

    while (tk && tk != endToken) {
        backtracking = FALSE;
        sourceoffset = CPrep_GetFileOffsetInfo(&cparser_fileoffset);
        if (tk == '"') {
            if (InlineAsm_gccmode) {
                tk = lex();
                InlineAsm_gcc_parse();
            } else {
                InlineAsm_gccmode = TRUE;
                copts.cplusplus = FALSE;
                copts.asmpoundcomment = TRUE;
                tk = lex();
            }
        }

        if (tk == '.') {
            InlineAsm_ScanAssemblyDirective();
        } else if (tk == TK_IDENTIFIER) {
            ScanOptionalLabel();
            if (tk == TK_IDENTIFIER) {
                InlineAsm_ScanAssemblyInstruction();
            }
        } else if (tk == TK_INTCONST) {
            ScanOptionalLabel();
            if (tk == TK_IDENTIFIER) {
                InlineAsm_ScanAssemblyInstruction();
            }
        }

        if (InlineAsm_gccmode && tk == '"') {
            tk = lex();
            InlineAsm_gcc_parse();
        }

        if (tk == ';' || tk == TK_EOL) {
            CPrep_TokenStreamFlush();
            tk = lex();
        } else if (tk != endToken) {
            if (endToken == ')') {
                CError_Error(ERR_RPAREN_EXPECTED);
            } else {
                CError_Error(ERR_END_LINE_EXPECTED);
            }
        }
    }
}

#endif
#if MWCCPSP

#endif
#if MWCCPSP
void InlineAsm_ScanStatements(volatile short endToken) {
    ScanStatements(endToken);
}

void InlineAsm_ScanFunction(volatile short endToken) {
    ScanStatements(endToken);
}
#else
void InlineAsm_ScanStatements(volatile short endToken) {
    ScanStatements(endToken, AssemblerType_Statements);
}

void InlineAsm_ScanFunction(volatile short endToken) {
    ScanStatements(endToken, AssemblerType_Function);
}
#endif

#if !MWCCPSP
void InlineAsm_Assemble(void) {
    short token = (tk == '(') ? ')' : '}';
    char save_pc = copts.asmpoundcomment;
    char save_cpp = copts.cplusplus;

    cprep_nostring = TRUE;
    CFunc_AppendStatement(ST_NOP);
    first_ST_ASM = curstmt;
    ASMstmtnb++;

    cprep_eoltokens = TRUE;
    InlineAsm_setjmp_valid = FALSE;
    in_assembler = TRUE;
    tk = lex();
    InlineAsm_ScanStatements(token);
    in_assembler = FALSE;
    cprep_eoltokens = FALSE;
    cprep_nostring = FALSE;

    copts.asmpoundcomment = save_pc;
    copts.cplusplus = save_cpp;
    InlineAsm_setjmp_valid = FALSE;
}

void InlineAsm_PackAsmStatement(Statement* stmt, Statement* first, void** output, SInt32* outsize) {
    InlineAsm* src;
    InlineAsm* dest;
    IAOperand* op;
    SInt32 i;
    int size;

    src = (InlineAsm*)stmt->expr;
    size = sizeof(InlineAsm) + sizeof(IAOperand) * src->argcount;
    dest = galloc(size);
    memcpy(dest, src, size);

    for (i = 0, op = dest->args; i < dest->argcount; i++, op++) {
        switch (op->type) {
            case IAOpnd_Imm:
                break;
            case IAOpnd_Reg:
            case IAOpnd_LocalObj:
                op->u.reg.object = (Object*)CInline_GetLocalID(op->u.reg.object);
                break;
            case IAOpnd_Lab:
                op->u.lab.label =
                    (CLabel*)(SInt32)CInline_GetStatementNumber(first, op->u.lab.label->stmt);
                break;
            case IAOpnd_LabDiff:
                op->u.labdiff.label1 =
                    (CLabel*)(SInt32)CInline_GetStatementNumber(first, op->u.labdiff.label1->stmt);
                op->u.labdiff.label2 =
                    (CLabel*)(SInt32)CInline_GetStatementNumber(first, op->u.labdiff.label2->stmt);
                break;
            default:
                break;
        }
    }

    *output = dest;
    *outsize = size;
}

void InlineAsm_UnpackAsmStatement(
    Statement* stmt, CLabel** labelArray, Boolean inlineCopy, void* data, SInt32 size
) {
    InlineAsm* ia;
    IAOperand* op;
    SInt32 i;

    ia = galloc(size);
    memcpy(ia, data, size);

    for (i = 0, op = ia->args; i < ia->argcount; i++, op++) {
        switch (op->type) {
            case IAOpnd_Imm:
                break;
            case IAOpnd_Reg:
            case IAOpnd_LocalObj:
                op->u.reg.object = CInline_GetLocalObj((SInt32)op->u.reg.object, inlineCopy);
                break;
            case IAOpnd_Lab:
                op->u.lab.label = labelArray[(SInt16)(SInt32)op->u.lab.label];
                break;
            case IAOpnd_LabDiff:
                op->u.labdiff.label1 = labelArray[(SInt16)(SInt32)op->u.labdiff.label1];
                op->u.labdiff.label2 = labelArray[(SInt16)(SInt32)op->u.labdiff.label2];
                break;
            default:
                break;
        }
    }

    stmt->expr = (ENode*)ia;
}

void InlineAsm_CheckLocalUsage(Statement* stmt) {
    InlineAsm* ia = (InlineAsm*)stmt->expr;
    IAOperand* op;
    SInt32 i;

    for (i = 0, op = ia->args; i < ia->argcount; i++, op++) {
        switch (op->type) {
            case IAOpnd_Reg:
                if (op->u.reg.object) {
                    SetVarUsage(op->u.reg.object, FALSE);
                }
                break;
            case IAOpnd_LocalObj:
                SetVarUsage(op->u.obj.obj, TRUE);
                break;
            default:
                break;
        }
    }
}

CLabel* InlineAsm_GetReferencedLabel(Statement* stmt) {
    InlineAsm* ia = (InlineAsm*)stmt->expr;
    IAOperand* op;
    SInt32 i;

    for (i = 0, op = ia->args; i < ia->argcount; i++, op++) {
        if (op->type == IAOpnd_Lab) {
            return op->u.lab.label;
        }
        if (op->type == IAOpnd_LabDiff) {
            return op->u.labdiff.label1;
        }
    }

    return NULL;
}

CLabel* InlineAsm_GetReferencedLabel2(Statement* stmt) {
    InlineAsm* ia = (InlineAsm*)stmt->expr;
    IAOperand* op;
    SInt32 i;

    for (i = 0, op = ia->args; i < ia->argcount; i++, op++) {
        if (op->type == IAOpnd_LabDiff) {
            return op->u.labdiff.label2;
        }
    }

    return NULL;
}

Object* InlineAsm_GetObjectOffset(void* p, SInt32 index, SInt32* offset) {
    InlineAsm* ia = p;
    IAOperand* op;
    SInt32 i;
    SInt32 counter;

    for (i = 0, counter = 0, op = ia->args; i < ia->argcount; i++, op++) {
        if (op->type == IAOpnd_GlobalObj) {
            if (counter++ == index) {
                *offset = (char*)&op->u.obj.obj - (char*)ia;
                return op->u.obj.obj;
            }
        }
    }

    return NULL;
}

char* InlineAsm_DumpStatement(Statement* stmt) {
    InlineAsm* ia;
    IAOperand* arg;
    int i;
    char ch;
    SInt32 offset;

    ia = (InlineAsm*)stmt->expr;

    strcpy(buffer, "\"");
    strcat(buffer, InlineAsm_GetMnemonic(ia));
    strcat(buffer, "\"");

    for (i = 0, arg = ia->args; i < ia->argcount; i++, arg++) {
        char argbuf[1024];

        switch (arg->type) {
            case IAOpnd_Imm:
                sprintf(argbuf, " imm(%ld)", arg->u.imm.value);
                break;
            case IAOpnd_Reg:
                ch = ' ';
                if (arg->u.reg.effect & EffectWrite) {
                    if (arg->u.reg.effect & EffectRead) {
                        ch = '+';
                    } else {
                        ch = '=';
                    }
                } else {
                    if (!(arg->u.reg.effect & EffectRead)) {
                        ch = '0';
                    }
                }

                if (arg->u.reg.object) {
                    sprintf(argbuf, "%creg(%s)", ch, arg->u.reg.object->name->name);
                } else {
                    sprintf(
                        argbuf,
                        "%creg(%s%d)",
                        ch,
                        register_class_name[arg->u.reg.rclass],
                        (int)arg->u.reg.num
                    );
                }
                break;

            case IAOpnd_GlobalObj:
            case IAOpnd_LocalObj:
                if (arg->u.obj.offset > 0) {
                    sprintf(argbuf, " obj(%s+%ld)", arg->u.obj.obj->name->name, arg->u.obj.offset);
                } else if (arg->u.obj.offset < 0) {
                    sprintf(argbuf, " obj(%s-%ld)", arg->u.obj.obj->name->name, -arg->u.obj.offset);
                } else {
                    sprintf(argbuf, " obj(%s)", arg->u.obj.obj->name->name);
                }
                break;

            case IAOpnd_Lab:
                sprintf(argbuf, " lab(%s)", arg->u.lab.label->uniquename->name);
                break;

            case IAOpnd_LabDiff:
                offset = !arg->negated ? 0 : arg->u.labdiff.offset;
                sprintf(
                    argbuf,
                    " labdiff(%s-%s%c%d)",
                    arg->u.labdiff.label1->uniquename->name,
                    arg->u.labdiff.label2->uniquename->name,
                    (arg->negated == 1) ? '-' : '+',
                    (int)offset
                );
                break;
            default:
                break;
        }

        strcat(buffer, argbuf);
    }

    return buffer;
}
#endif
#if MWCCPSP

#endif
#if MWCCPSP
// 3.0.1: the MIPS assembler's statements (compiler/InlineAsmMIPS.h)
extern Boolean data_00625EC1; // the tokenizer passes strings through
extern char data_00625FD6;
extern char* func_00481FE0(MAsmInstruction* mi); // the instruction's mnemonic

void InlineAsm_Assemble(void) {
    short token = (tk == '(') ? ')' : '}';
    char save_cpp = copts.cplusplus;
    char save_pc = copts.asmpoundcomment;

    data_00625FD6 = save_cpp;
    data_00625EC1 = TRUE;
    CFunc_AppendStatement(ST_NOP);
    first_ST_ASM = curstmt;
    ASMstmtnb++;

    cprep_eoltokens = TRUE;
    in_assembler = TRUE;
    tk = lex();
    ScanStatements(token);
    InlineAsm_gccmode = FALSE;
    in_assembler = FALSE;
    cprep_eoltokens = FALSE;
    data_00625EC1 = FALSE;

    copts.asmpoundcomment = save_pc;
    copts.cplusplus = data_00625FD6;
}

void InlineAsm_PackAsmStatement(Statement* stmt, Statement* first, void** output, SInt32* outsize) {
    MAsmInstruction* src;
    MAsmInstruction* dest;
    MAsmOperand* op;
    int i;
    int size;

    src = (MAsmInstruction*)stmt->expr;
    size = src->nops * sizeof(MAsmOperand) + 8;
    dest = galloc(size);
    memcpy(dest, src, size);

    for (i = 0, op = dest->ops; i < dest->nops; i++, op++) {
        switch (op->type) {
            case MOpnd_Imm:
                break;
            case MOpnd_Reg:
            case MOpnd_LocalObj:
                if (op->u.obj.obj && op->u.obj.obj->datatype == DLOCAL) {
                    op->u.reg.object = (Object*)CInline_GetLocalID(op->u.reg.object);
                }
                break;
            case MOpnd_Lab:
                op->u.lab.label =
                    (CLabel*)(SInt32)CInline_GetStatementNumber(first, op->u.lab.label->stmt);
                break;
            default:
                break;
        }
    }

    *output = dest;
    *outsize = size;
}

void InlineAsm_UnpackAsmStatement(
    Statement* stmt, CLabel** labelArray, Boolean inlineCopy, void* data, SInt32 size
) {
    MAsmInstruction* mi;
    MAsmOperand* op;
    int i;

    mi = galloc(size);
    memcpy(mi, data, size);

    for (i = 0, op = mi->ops; i < mi->nops; i++, op++) {
        switch (op->type) {
            case MOpnd_Imm:
                break;
            case MOpnd_Reg:
            case MOpnd_LocalObj:
                op->u.reg.object = CInline_GetLocalObj((SInt32)op->u.reg.object, inlineCopy);
                break;
            case MOpnd_Lab:
                op->u.lab.label = labelArray[(SInt16)(SInt32)op->u.lab.label];
                break;
            default:
                break;
        }
    }

    stmt->expr = (ENode*)mi;
}

void InlineAsm_CheckLocalUsage(Statement* stmt) {
    MAsmInstruction* mi = (MAsmInstruction*)stmt->expr;
    MAsmOperand* op;
    int i;

    for (i = 0, op = mi->ops; i < mi->nops; i++, op++) {
        switch (op->type) {
            case MOpnd_Reg:
                if (op->u.reg.object) {
                    SetVarUsage(op->u.reg.object, FALSE);
                }
                break;
            case MOpnd_LocalObj:
                SetVarUsage(op->u.obj.obj, TRUE);
                break;
            default:
                break;
        }
    }
}

CLabel* InlineAsm_GetReferencedLabel(Statement* stmt) {
    MAsmInstruction* mi = (MAsmInstruction*)stmt->expr;
    MAsmOperand* op;
    SInt32 i;

    for (i = 0, op = mi->ops; i < mi->nops; i++, op++) {
        if (op->type == MOpnd_Lab) {
            return op->u.lab.label;
        }
    }

    return NULL;
}

Object* InlineAsm_GetObjectOffset(void* p, SInt32 index, SInt32* offset) {
    MAsmInstruction* mi = p;
    MAsmOperand* op;
    SInt32 i;
    SInt32 counter;

    for (i = 0, counter = 0, op = mi->ops; i < mi->nops; i++, op++) {
        if (op->type == MOpnd_GlobalObj) {
            if (counter++ == index) {
                *offset = (char*)&op->u.obj.obj - (char*)mi;
                return op->u.obj.obj;
            }
        }
    }

    return NULL;
}

char* InlineAsm_DumpStatement(Statement* stmt) {
    MAsmInstruction* mi;
    MAsmOperand* arg;
    int i;

    mi = (MAsmInstruction*)stmt->expr;

    strcpy(buffer, "\"");
    strcat(buffer, func_00481FE0(mi));
    strcat(buffer, "\"");

    for (i = 0, arg = mi->ops; i < mi->nops; i++, arg++) {
        char argbuf[1024];

        switch (arg->type) {
            case MOpnd_Imm:
                sprintf(argbuf, " imm(%ld)", arg->u.imm);
                break;
            case MOpnd_Reg:
                if (arg->u.reg.object) {
                    sprintf(argbuf, " reg(%s)", arg->u.reg.object->name->name);
                } else {
                    sprintf(argbuf, " reg(%d)", arg->u.reg.num);
                }
                break;
            case MOpnd_GlobalObj:
            case MOpnd_LocalObj:
                if (arg->u.obj.offset > 0) {
                    sprintf(argbuf, " obj(%s+%ld)", arg->u.obj.obj->name->name, arg->u.obj.offset);
                } else if (arg->u.obj.offset < 0) {
                    sprintf(argbuf, " obj(%s-%ld)", arg->u.obj.obj->name->name, -arg->u.obj.offset);
                } else {
                    sprintf(argbuf, " obj(%s)", arg->u.obj.obj->name->name);
                }
                break;
            case MOpnd_Lab:
                sprintf(argbuf, " lab(%s)", arg->u.lab.label->uniquename->name);
                break;
            default:
                break;
        }

        strcat(buffer, argbuf);
    }

    return buffer;
}
#endif
