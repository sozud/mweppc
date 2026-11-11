#include "compiler/GCCInlineAsm.h"
#include "compiler/CError.h"
#include "compiler/CExpr.h"
#include "compiler/CFunc.h"
#include "compiler/CInt64.h"
#include "compiler/CParser.h"
#include "compiler/CPrep.h"
#include "compiler/CPrepTokenizer.h"
#include "compiler/enode.h"
#include "compiler/InlineAsm.h"
#include "compiler/objects.h"
#include "compiler/tokens.h"
#if MWCCPSP

#endif
#if MWCCPSP
#include "compiler/CMachine.h"
#include "compiler/CompilerTools.h"
#include "compiler/InlineAsmMIPS.h"
#include "compiler/types.h"
#include <string.h>
#endif

#if MWCCPSP
// 3.0.1: GCC-style operands for the MIPS assembler
Statement* first_ST_ASM;
MLookupResult gcc_name_list[30];
int gcc_name_list_index;

// an operand's constraint ("=r", "+f", "m", "0", ...)
typedef struct GCCConstraint {
    UInt8 kind; // 0 'i', 1 'n', 2 'r', 3 'f', 4 'j' (VFPU), 5 'm'; 0xFF none
    UInt8 mode; // 1 output, 2 input, 3 both
    SInt32 flags; // 1 read, 2 written
    SInt32 match; // an input tied to output N, or -1
} GCCConstraint;

// what the operand table keeps of each constraint (8 bytes)
typedef struct GCCOperandInfo {
    UInt8 kind;
    UInt8 mode;
    SInt32 flags;
} GCCOperandInfo;

GCCOperandInfo gcc_operands[30];
SInt32 data_006242D0;

extern Boolean data_00625EC1;
extern char data_00625FD6;
extern void func_004C5890(SInt32 code, ...); // a warning
extern void func_004C5940(SInt32 code, ...); // a warning
extern char* func_00481FE0(MAsmInstruction* mi); // the instruction's mnemonic
extern void func_00481EE0(
    Statement* stmt, Object* gpr, Object* fpr, Boolean tofloat, Boolean after
); // an int/float move around the asm
static ENode* gcc_copy_expression(ENode* expr);
#else
Statement* first_ST_ASM;
IALookupResult gcc_name_list[20];
int gcc_name_list_index;
#endif

void InlineAsm_SkipComment(void) {
    while (1) {
        if (tk != '/') {
            break;
        }
        if (lookahead() != '*') {
            break;
        }

        tk = lex();
        while (!((tk = lex()) == '*' && (tk = lex()) == '/')) {}
        tk = lex();
    }
}

#if MWCCPSP
static GCCConstraint gcc_parse_attribute(Boolean isOutput) {
    GCCConstraint c;

    c.flags = 0;
    c.kind = 0xFF;
    c.mode = 0;
    c.match = -1;

    while (tk == TK_EOL) {
        tk = lex();
    }

    if (tk != '"') {
        CError_Error(ERR_ILLEGAL_TOKEN);
    }
    tk = lex();

    if (isOutput) {
        switch (tk) {
            case '+':
                c.flags |= 3;
                c.mode = 3;
                tk = lex();
                while (tk == '&' || tk == '%' || tk == '$') {
                    tk = lex();
                }
                break;
            case '=':
                c.flags |= 2;
                c.mode = 1;
                tk = lex();
                while (tk == '&' || tk == '%' || tk == '$') {
                    tk = lex();
                }
                break;
            default:
                CError_Error(ERR_ILLEGAL_TOKEN);
                break;
        }
    } else {
        while (tk == '$' || tk == '%' || tk == '&') {
            tk = lex();
        }
        c.flags |= 1;
        c.mode = 2;
    }

    if (tk == ')') {
        c.kind = 0xFF;
        c.mode = 0;
        c.flags = 0;
        c.match = -1;
        return c;
    }

    if (tk == TK_INTCONST) {
        c.match = tkintconst.lo;
        if (c.match < 0) {
            CError_Error(ERR_ILLEGAL_OPERAND);
        }
    } else if (tk == TK_IDENTIFIER) {
        switch (tkidentifier->name[0]) {
            case 'I':
            case 'i':
                c.kind = 0;
                break;
            case 'n':
                c.kind = 1;
                break;
            case 'g':
            case 'r':
                c.kind = 2;
                break;
            case 'f':
                c.kind = 3;
                break;
            case 'j':
                c.kind = 4;
                break;
            case 'm':
                c.kind = 5;
                break;
            case 'o':
                c.kind = 5;
                break;
            default:
                CError_Error(ERR_ILLEGAL_TOKEN);
                c.kind = 0xFF;
                break;
        }
        if (c.kind != 0xFF) {
            copts.cplusplus = data_00625FD6;
        }
        if (c.kind == 4) {
            copts.mips_option41 = 1;
        }
    } else {
        CError_Error(ERR_ILLEGAL_TOKEN);
    }

    tk = lex();
    if (tk == '&') {
        if (!isOutput) {
            CError_Error(ERR_ILLEGAL_TOKEN);
        }
        tk = lex();
    }
    if (tk != '"') {
        CError_Error(ERR_ILLEGAL_TOKEN);
    }
    tk = lex();
    return c;
}

static void gcc_parse_name(Boolean isOutput, GCCConstraint c) {
    ENode* expr;
    ENode* tempexpr;
    Object* tempobj;
    Statement* stmt;
    MLookupResult* entry;
    SInt32 i;
    short next;
    Boolean done = FALSE;
    Boolean notmem = FALSE;

    while (tk == TK_EOL) {
        tk = lex();
    }

    if (tk != '(') {
        CError_Error(ERR_LPAREN_EXPECTED);
    }
    next = lookahead();

    if (isOutput) {
        if (++gcc_name_list_index >= 30) {
            CError_FATAL(338);
            return;
        }
        gcc_operands[gcc_name_list_index].mode = c.mode;
        gcc_operands[gcc_name_list_index].flags = c.flags;
        gcc_operands[gcc_name_list_index].kind = c.kind;
        if (gcc_operands[gcc_name_list_index].kind != 5) {
            notmem = TRUE;
        }

        if (notmem) {
            // the asm writes a temporary; copy it to the output after
            expr = expression();
            done = TRUE;
            tempobj = create_temp_object(expr->rtype);
            tempexpr = create_objectnode(tempobj);
            if (tempexpr->rtype != expr->rtype) {
                tempexpr->rtype = expr->rtype;
            }
            gcc_name_list[gcc_name_list_index].object = tempobj;
            if (c.flags & 1) {
                stmt = CFunc_InsertBeforeStatement(ST_EXPRESSION, first_ST_ASM);
                stmt->expr = makediadicnode(tempexpr, gcc_copy_expression(expr), EASS);
            }
            expr = makediadicnode(expr, tempexpr, EASS);
            stmt = CFunc_InsertStatement(ST_EXPRESSION, curstmt);
            stmt->expr = expr;
            curstmt = stmt;
        } else if (gcc_operands[gcc_name_list_index].kind == 5) {
            // a memory operand: the address goes in a temporary
            expr = expression();
            done = TRUE;
            if (expr && ENODE_IS(expr, EINDIRECT)) {
                expr = expr->data.monadic;
                tempobj = create_temp_object(expr->rtype);
                tempexpr = create_objectnode2(tempobj);
                gcc_name_list[gcc_name_list_index].object = tempobj;
                expr = makediadicnode(tempexpr, expr, EASS);
                stmt = CFunc_InsertBeforeStatement(ST_EXPRESSION, first_ST_ASM);
                first_ST_ASM = stmt->next;
                if (!first_ST_ASM->next) {
                    curstmt = first_ST_ASM;
                }
                stmt->expr = expr;
            } else {
                if (next == '&') {
                    func_004C5890(13, "'m'");
                }
                gcc_name_list[gcc_name_list_index].object = NULL;
                goto finish;
            }
        }

        if ((tempobj = gcc_name_list[gcc_name_list_index].object)) {
            tempobj->flags |= 1;
            if (tempobj->u.var.info) {
                tempobj->u.var.info->used = TRUE;
            }
        }
        if (!done) {
            tk = lex();
        }
    } else {
        tk = lex();
        in_assembler = FALSE;
        data_00625EC1 = FALSE;
        copts.cplusplus = TRUE;

        if (c.match != -1 && gcc_name_list_index != -1) {
            // tied to an output
            if (c.match > gcc_name_list_index) {
                CError_Error(ERR_ILLEGAL_OPERAND);
            }
            i = c.match;
            entry = &gcc_name_list[i];
            gcc_operands[i].flags |= 1;
            gcc_operands[i].mode = 3;
        } else {
            entry = &gcc_name_list[++gcc_name_list_index];
            memset(entry, 0, sizeof(MLookupResult));
            gcc_operands[gcc_name_list_index].mode = c.mode;
            gcc_operands[gcc_name_list_index].flags = c.flags;
            gcc_operands[gcc_name_list_index].kind = c.kind;
            i = gcc_name_list_index;
        }

        expr = expression();
        if (gcc_operands[i].kind == 0) {
            if (!ENODE_IS(expr, EINTCONST)) {
                CError_Error(ERR_ILLEGAL_OPERAND);
            }
            entry->value = expr->data.intval;
            entry->has_value = TRUE;
        } else if (gcc_operands[i].kind == 1) {
            if (!ENODE_IS(expr, EINTCONST)) {
                CError_Error(ERR_ILLEGAL_OPERAND);
            }
            entry->value = expr->data.intval;
            entry->has_value = TRUE;
        } else {
            if (gcc_operands[i].kind == 5) {
                if (expr && ENODE_IS(expr, EINDIRECT)) {
                    expr = expr->data.monadic;
                } else {
                    if (next == '&') {
                        func_004C5890(13, "'m'");
                    }
                    gcc_name_list[i].object = NULL;
                    goto finish;
                }
            }

            if (c.match != -1 && gcc_name_list[i].object) {
                tempexpr = create_objectnode(gcc_name_list[i].object);
                tempobj = gcc_name_list[i].object;
            } else {
                tempobj = create_temp_object(expr->rtype);
                tempexpr = create_objectnode2(tempobj);
            }
            if (tempobj->u.var.info) {
                tempobj->u.var.info->used = TRUE;
            }
            expr = makediadicnode(tempexpr, expr, EASS);
            stmt = CFunc_InsertBeforeStatement(ST_EXPRESSION, first_ST_ASM);
            first_ST_ASM = stmt->next;
            if (!first_ST_ASM->next) {
                curstmt = first_ST_ASM;
            }
            stmt->expr = expr;

            entry->name = tempobj->name;
            entry->object = tempobj;
            entry->label = NULL;
            entry->type = NULL;
            entry->has_value = FALSE;
        }
    }

finish:
    data_00625EC1 = TRUE;
    in_assembler = TRUE;
    if (tk != ')' && !done) {
        CError_Error(ERR_RPAREN_EXPECTED);
    }
    if (!done) {
        tk = lex();
    }
}

// replace the GCC operands (%N) in an instruction
static void gcc_replace_arg_st_asm(Statement* stmt) {
    MAsmInstruction* mi;
    MAsmInstruction* nmi;
    Statement* nstmt;
    ENode* tempexpr;
    OpcodeDef* def;
    OpcodeDef* ndef;
    MAsmOperand* op;
    Object* obj;
    Object* temp;
    Type* type;
    int i;
    SInt32 n;
    UInt16 opcode;
    SInt16 effect;
    SInt16 rclass;
    UInt8 kind;

    if ((mi = (MAsmInstruction*)stmt->expr)) {
        for (i = 0, op = mi->ops; i < mi->nops; i++, op++) {
            switch (op->type) {
                case MOpnd_Imm:
                case MOpnd_Reg:
                case MOpnd_GlobalObj:
                case MOpnd_LocalObj:
                case MOpnd_Lab:
                    break;

                case MOpnd_GCCReg:
                    def = &data_006115F8[mi->opcode];
                    n = op->u.gcc.num;
                    if (gcc_operands[n].kind == 5 && (def->flags & 6) && mi->opcode > 0x518) {
                        // a memory operand: the instruction's form with an offset
                        opcode = data_00625CE4[mi->opcode - 0x518];
                        ndef = &data_006115F8[opcode];
                        nmi = lalloc(ndef->nargs * sizeof(MAsmOperand) + 8);
                        memset(nmi, 0, ndef->nargs * sizeof(MAsmOperand) + 8);
                        *nmi = *mi;
                        nmi->opcode = opcode;
                        nmi->nops = ndef->nargs;
                        nmi->ops[1].type = MOpnd_Imm;
                        nmi->ops[1].u.imm = cint64_zero;
                        nmi->ops[2] = mi->ops[1];
                        stmt->expr = (ENode*)nmi;
                        gcc_replace_arg_st_asm(stmt);
                        return;
                    }

                    if (!gcc_name_list[n].object || !gcc_name_list[n].object->type) {
                        if (gcc_name_list[n].has_value == TRUE) {
                            op->type = MOpnd_Imm;
                            op->u.imm = gcc_name_list[n].value;
                            break;
                        }
                        CError_Error(ERR_ILLEGAL_OPERAND);
                    }

                    effect = op->u.gcc.effect;
                    rclass = op->u.gcc.rclass;
                    op->type = MOpnd_Reg;
                    op->u.reg.num = 0;
                    if ((effect & 2) && (effect & 1) && !(gcc_operands[n].flags & 1)) {
                        effect &= ~1;
                    }
                    if (gcc_operands[n].mode == 3) {
                        if (!(effect & 1)) {
                            effect |= 1;
                        }
                    }
                    op->effect = effect;
                    op->rclass = rclass;
                    op->u.reg.object = NULL;

                    if (n > gcc_name_list_index) {
                        func_004C5940(4, "%", n);
                        break;
                    }

                    if (op->rclass != -1 && gcc_name_list[n].object) {
                        if (gcc_operands[n].kind == 2 && op->rclass != 0) {
                            func_004C5940(8, "%", n, func_00481FE0(mi));
                        }
                        if (gcc_operands[n].kind == 3 && op->rclass != 1) {
                            func_004C5940(8, "%", n, func_00481FE0(mi));
                        }
                        if (gcc_operands[n].kind == 4 && op->rclass != 5) {
                            func_004C5940(8, "%", n, func_00481FE0(mi));
                        }
                        if (gcc_operands[n].kind == 5 && op->rclass != 0) {
                            func_004C5940(8, "%", n, func_00481FE0(mi));
                        }
                    }

                    if (!gcc_name_list[n].object) {
                        op->u.reg.object = NULL;
                    }

                    if (gcc_operands[n].mode == 1 || gcc_operands[n].mode == 3) {
                        // written: a global or a value of the other class goes through a temporary
                        obj = gcc_name_list[n].object;
                        if (obj->datatype == DDATA) {
                            obj = create_temp_object(TYPE(&stsignedint));
                            if (obj->u.var.info) {
                                obj->u.var.info->used = TRUE;
                            }
                            tempexpr = create_objectnode(gcc_name_list[n].object);
                            tempexpr = makediadicnode(tempexpr, create_objectnode(obj), EASS);
                            nstmt = CFunc_InsertStatement(ST_EXPRESSION, stmt);
                            nstmt->expr = tempexpr;
                            if (curstmt == stmt) {
                                curstmt = nstmt;
                            }
                        } else {
                            kind = gcc_operands[n].kind;
                            if (kind == 3 && obj->type->type == TYPEINT) {
                                obj = create_temp_object(TYPE(&stfloat));
                                if (obj->u.var.info) {
                                    obj->u.var.info->used = TRUE;
                                }
                                if (op->effect & 2) {
                                    func_00481EE0(stmt, gcc_name_list[n].object, obj, FALSE, TRUE);
                                }
                                if (op->effect & 1) {
                                    func_00481EE0(stmt, gcc_name_list[n].object, obj, TRUE, FALSE);
                                    stmt = stmt->next;
                                }
                            } else if (kind == 2 &&
                                       obj->type->type == TYPEFLOAT &&
                                       obj->type->size == 4) {
                                obj = create_temp_object(TYPE(&stsignedint));
                                if (op->effect & 2) {
                                    func_00481EE0(stmt, obj, gcc_name_list[n].object, TRUE, TRUE);
                                }
                                if (op->effect & 1) {
                                    func_00481EE0(stmt, obj, gcc_name_list[n].object, FALSE, FALSE);
                                    stmt = stmt->next;
                                }
                            }
                        }
                        if (obj->u.var.info) {
                            obj->u.var.info->used = TRUE;
                        }
                        op->u.reg.object = obj;
                        if (gcc_operands[n].mode == 1) {
                            gcc_operands[n].mode = 3;
                            gcc_operands[n].flags |= 3;
                        }
                    } else if (gcc_operands[n].mode == 2) {
                        obj = gcc_name_list[n].object;
                        type = obj->type;
                        if (type->type == TYPEFLOAT &&
                            type->size == 4 &&
                            gcc_operands[n].kind == 2) {
                            temp = create_temp_object(TYPE(&stsignedint));
                            if (temp->u.var.info) {
                                temp->u.var.info->used = TRUE;
                            }
                            op->u.reg.object = temp;
                            func_00481EE0(stmt, temp, gcc_name_list[n].object, FALSE, FALSE);
                            stmt = stmt->next;
                        } else if ((type->type == TYPEINT || type->type == TYPEPOINTER) &&
                                   type->size == 4 &&
                                   gcc_operands[n].kind == 3) {
                            temp = create_temp_object(TYPE(&stfloat));
                            if (temp->u.var.info) {
                                temp->u.var.info->used = TRUE;
                            }
                            op->u.reg.object = temp;
                            func_00481EE0(stmt, gcc_name_list[n].object, temp, TRUE, FALSE);
                            stmt = stmt->next;
                        } else {
                            op->u.reg.object = obj;
                        }
                    }

                    kind = gcc_operands[gcc_name_list_index].kind;
                    if (kind == 2) {
                        op->rclass = 0;
                    } else if (kind == 3) {
                        op->rclass = 1;
                    } else if (kind == 4) {
                        op->rclass = 5;
                    } else if (kind == 5) {
                        op->rclass = 0;
                    } else if (kind == 6) {
                        op->rclass = 0;
                    }
                    break;

                case MOpnd_GCCImm:
                    op->type = MOpnd_Imm;
                    op->u.imm = gcc_name_list[op->u.gcc.num].value;
                    break;

                default:
                    break;
            }
        }
    }
}

static inline void gcc_parse_output(void) {
    GCCConstraint c;

    if (tk == ':') {
        if ((tk = lex()) == ':' || tk == ')' || tk == '}') {
            return;
        }
        while (1) {
            c = gcc_parse_attribute(TRUE);
            if (c.flags == 0 && c.kind == 0xFF) {
                break;
            }
            gcc_parse_name(TRUE, c);
            if (tk != ',') {
                break;
            }
            tk = lex();
        }
        copts.cplusplus = FALSE;
    }
}

static inline void gcc_parse_input(void) {
    GCCConstraint c;

    if (tk == ':') {
        if ((tk = lex()) == ':' || tk == ')' || tk == '}') {
            return;
        }
        while (1) {
            c = gcc_parse_attribute(FALSE);
            if (c.flags == 0 && c.kind == 0xFF) {
                break;
            }
            gcc_parse_name(FALSE, c);
            if (tk != ',') {
                break;
            }
            tk = lex();
        }
    }
}

static inline void gcc_parse_killed(void) {
    if (tk == ':') {
        while (1) {
            if ((tk = lex()) != '"') {
                return;
            }

            tk = lex();
            while (1) {
                if (tk == '"') {
                    if (lookahead() == ',') {
                        tk = lex();
                        break;
                    }
                    tk = lex();
                    return;
                }
                tk = lex();
            }
        }
    }
}

static inline void gcc_replace_arg(void) {
    Statement* stmt;

    for (stmt = first_ST_ASM; stmt; stmt = stmt->next) {
        if (stmt->type == ST_ASM) {
            gcc_replace_arg_st_asm(stmt);
        }
    }
}

void InlineAsm_gcc_parse(void) {
    int i;

    gcc_name_list_index = -1;
    for (i = 0; i < 30; i++) {
        gcc_operands[i].mode = 0;
        gcc_operands[i].kind = 0xFF;
        gcc_operands[i].flags = 0;
    }
    data_006242D0 = 0;
    cprep_eoltokens = FALSE;

    if (tk == TK_EOL) {
        tk = lex();
    }

    gcc_parse_output();
    gcc_parse_input();
    gcc_parse_killed();
    gcc_replace_arg();
}

static ENode* gcc_copy_expression(ENode* expr) {
    ENode* copy;

    copy = lalloc(sizeof(ENode));
    memset(copy, 0, sizeof(ENode));
    *copy = *expr;

    switch (copy->type) {
        case EPOSTINC:
        case EPOSTDEC:
        case EPREINC:
        case EPREDEC:
        case EMONMIN:
        case EBINNOT:
        case ELOGNOT:
        case EFORCELOAD:
        case ETYPCON:
        case EBITFIELD:
            copy->data.monadic = gcc_copy_expression(copy->data.monadic);
            break;
        case EMUL:
        case EMULV:
        case EDIV:
        case EMODULO:
        case EADDV:
        case ESUBV:
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
        case EASS:
        case EMULASS:
        case EDIVASS:
        case EMODASS:
        case EADDASS:
        case ESUBASS:
        case ESHLASS:
        case ESHRASS:
        case EANDASS:
        case EXORASS:
        case EORASS:
        case ECOMMA:
        case EMIN:
        case EMAX:
        case EPMODULO:
        case EROTL:
        case EROTR:
        case EBCLR:
        case EBTST:
        case EBSET:
        case EPOINTERSTAR:
        case EDOTSTAR:
            copy->data.diadic.left = gcc_copy_expression(copy->data.diadic.left);
            copy->data.diadic.right = gcc_copy_expression(copy->data.diadic.right);
            break;
        case ECOND:
            copy->data.cond.cond = gcc_copy_expression(copy->data.cond.cond);
            copy->data.cond.expr1 = gcc_copy_expression(copy->data.cond.expr1);
            copy->data.cond.expr2 = gcc_copy_expression(copy->data.cond.expr2);
            break;
        case EMFPOINTER:
            copy->data.mfpointer.accessnode = gcc_copy_expression(copy->data.mfpointer.accessnode);
            copy->data.mfpointer.mfpointer = gcc_copy_expression(copy->data.mfpointer.mfpointer);
            break;
        case EFUNCCALL:
        case EFUNCCALLP:
        case ENULLCHECK:
        case EPRECOMP:
            CError_FATAL(1135);
            break;
        case EINDIRECT:
            copy->data.monadic = gcc_copy_expression(copy->data.monadic);
            break;
        case ENEWEXCEPTION:
        case ENEWEXCEPTIONARRAY:
            copy->data.newexception.initexpr =
                gcc_copy_expression(copy->data.newexception.initexpr);
            copy->data.newexception.tryexpr = gcc_copy_expression(copy->data.newexception.tryexpr);
            break;
        case EINITTRYCATCH:
            copy->data.itc.initexpr = gcc_copy_expression(copy->data.itc.initexpr);
            copy->data.itc.tryexpr = gcc_copy_expression(copy->data.itc.tryexpr);
            copy->data.itc.catchexpr = gcc_copy_expression(copy->data.itc.catchexpr);
            copy->data.itc.result = gcc_copy_expression(copy->data.itc.result);
            break;
        case EINTCONST:
        case EFLOATCONST:
        case ESTRINGCONST:
        case EOBJREF:
        case ELABEL:
        case ETEMP:
        case ETEMPLDEP:
        case EINSTRUCTION:
            break;
        default:
            CError_FATAL(1159);
            break;
    }

    return copy;
}
#else
static char gcc_parse_attribute(void) {
    char ch;

    while (tk == TK_EOL) {
        tk = lex();
    }

    if (tk != '"') {
        CError_Error(ERR_ILLEGAL_TOKEN);
    }

    while ((tk = lex()) != TK_IDENTIFIER) {}

    ch = tkidentifier->name[0];

    if ((tk = lex()) != '"') {
        CError_Error(ERR_ILLEGAL_TOKEN);
    }
    tk = lex();

    return ch;
}

static void gcc_parse_name(Boolean isOutput, char attribute) {
    ENode* expr;
    IALookupResult* nameentry;
    Object* tempobj;
    ENode* tempexpr;
    Statement* stmt;

    while (tk == TK_EOL) {
        tk = lex();
    }

    if (tk != '(') {
        CError_Error(ERR_LPAREN_EXPECTED);
    }

    tk = lex();

    if (isOutput) {
        if (tk != TK_IDENTIFIER) {
            CError_Error(ERR_ILLEGAL_TOKEN);
        }

        InlineAsm_LookupSymbol(tkidentifier, &gcc_name_list[++gcc_name_list_index]);
        if (gcc_name_list[gcc_name_list_index].object &&
            gcc_name_list[gcc_name_list_index].object->u.var.info) {
            gcc_name_list[gcc_name_list_index].object->u.var.info->used = TRUE;
        }

        tk = lex();
    } else {
        in_assembler = FALSE;
        cprep_nostring = FALSE;
        nameentry = &gcc_name_list[++gcc_name_list_index];

        expr = expression();
        if (attribute == 'i' || attribute == 'I') {
            if (!ENODE_IS(expr, EINTCONST)) {
                CError_Error(ERR_ILLEGAL_OPERAND);
            }
            nameentry->value = CInt64_GetULong(&expr->data.intval);
            nameentry->has_value = TRUE;
        } else {
            tempobj = create_temp_object(expr->rtype);
            tempexpr = create_objectnode(tempobj);
            if (tempobj->u.var.info) {
                tempobj->u.var.info->used = TRUE;
            }
            expr = makediadicnode(tempexpr, expr, EASS);

            stmt = CFunc_InsertBeforeStatement(ST_EXPRESSION, first_ST_ASM);
            first_ST_ASM = stmt->next;
            if (!first_ST_ASM->next) {
                curstmt = first_ST_ASM;
            }
            stmt->expr = expr;

            nameentry->name = tempobj->name;
            nameentry->object = tempobj;
            nameentry->label = NULL;
            nameentry->type = NULL;
            nameentry->has_value = FALSE;
        }
    }

    cprep_nostring = TRUE;
    in_assembler = TRUE;

    if (tk != ')') {
        CError_Error(ERR_RPAREN_EXPECTED);
    }
    tk = lex();
}

static void gcc_parse_expression(Boolean isOutput) {
    while (1) {
        gcc_parse_name(isOutput, gcc_parse_attribute());
        if (tk != ',') {
            break;
        }
        tk = lex();
    }
}

static void gcc_parse_input(void) {
    if (tk == ':') {
        if ((tk = lex()) == ':' || tk == ')' || tk == '}') {
            return;
        }
        gcc_parse_expression(FALSE);
    }
}

static void gcc_parse_output(void) {
    if (tk == ':') {
        if ((tk = lex()) == ':' || tk == ')' || tk == '}') {
            return;
        }
        gcc_parse_expression(TRUE);
    }
}

static inline void gcc_parse_killed(void) {
    if (tk == ':') {
        while (1) {
            if ((tk = lex()) != '"') {
                return;
            }

            tk = lex();
            while (1) {
                if (tk == '"') {
                    if (lookahead() == ',') {
                        tk = lex();
                        break;
                    }
                    tk = lex();
                    return;
                }
                tk = lex();
            }
        }
    }
}

static void gcc_replace_arg_st_asm(Statement* stmt) {
    InlineAsm* ia;
    int i;
    IAOperand* op;
    short effect;
    short rclass;
    SInt32 num;

    if ((ia = (InlineAsm*)stmt->expr)) {
        for (i = 0, op = ia->args; i < ia->argcount; i++, op++) {
            switch (op->type) {
                case IAOpnd_Imm:
                case IAOpnd_Reg:
                case IAOpnd_GlobalObj:
                case IAOpnd_LocalObj:
                case IAOpnd_Lab:
                    break;

                case IAOpnd_GCCReg:
                    if (op->u.gccreg.kind == 2) {
                        effect = op->u.gccreg.effect;
                        rclass = op->u.gccreg.rclass;
                        num = op->u.gccreg.num;
                        op->type = IAOpnd_Reg;
                        op->u.reg.effect = effect;
                        op->u.reg.rclass = rclass;
                        op->u.reg.object = NULL;
                        if (num <= gcc_name_list_index) {
                            op->u.reg.object = gcc_name_list[num].object;
                        } else {
                            CError_Error(ERR_ILLEGAL_OPERAND);
                        }
                        op->u.reg.num = 0;
                    } else {
                        CError_FATAL(365);
                    }
                    break;

                case IAOpnd_GCCImm:
                    op->type = IAOpnd_Imm;
                    op->u.imm.value = gcc_name_list[op->u.gccimm.value].value;
                    break;
                default:
                    break;
            }
        }
    }
}

static void gcc_replace_arg(void) {
    Statement* stmt;

    for (stmt = first_ST_ASM; stmt; stmt = stmt->next) {
        if (stmt->type == ST_ASM) {
            gcc_replace_arg_st_asm(stmt);
        }
    }
}

void InlineAsm_gcc_parse(void) {
    gcc_name_list_index = -1;
    cprep_eoltokens = FALSE;

    if (tk == TK_EOL) {
        tk = lex();
    }

    gcc_parse_output();
    gcc_parse_input();
    gcc_parse_killed();
    gcc_replace_arg();
}
#endif
