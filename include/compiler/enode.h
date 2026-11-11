#ifndef COMPILER_ENODE_H
#define COMPILER_ENODE_H

#include "compiler/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENODE_IS(_enode, _etype) ((_enode)->type == (_etype))
#define ENODE_IS2(_enode, _etype1, _etype2) \
    (ENODE_IS(_enode, (_etype1)) || ENODE_IS(_enode, (_etype2)))
#define ENODE_IS3(_enode, _etype1, _etype2, _etype3) \
    (ENODE_IS2(_enode, _etype1, _etype2) || ENODE_IS(_enode, (_etype3)))
#define ENODE_IS4(_enode, _etype1, _etype2, _etype3, _etype4) \
    (ENODE_IS3(_enode, _etype1, _etype2, _etype3) || ENODE_IS(_enode, (_etype4)))
#define ENODE_IS_RANGE(_enode, _lo, _hi) (((_enode)->type >= (_lo)) && ((_enode)->type <= (_hi)))
#define ENODE_QUALS(_enode) ((UInt32)((_enode)->flags & ENODE_FLAG_QUALS))
#define ENODE_IS_INDIRECT_TO(_enode, _etype) \
    (ENODE_IS((_enode), EINDIRECT) && ENODE_IS((_enode)->data.monadic, (_etype)))

#define ENODE_CASE_MONADIC \
    case EPOSTINC: \
    case EPOSTDEC: \
    case EPREINC: \
    case EPREDEC: \
    case EINDIRECT: \
    case EMONMIN: \
    case EBINNOT: \
    case ELOGNOT: \
    case EFORCELOAD: \
    case ETYPCON: \
    case EBITFIELD

#define ENODE_CASE_DIADIC_ALL \
    case EMUL: \
    case EMULV: \
    case EDIV: \
    case EMODULO: \
    case EADDV: \
    case ESUBV: \
    case EADD: \
    case ESUB: \
    case ESHL: \
    case ESHR: \
    case ELESS: \
    case EGREATER: \
    case ELESSEQU: \
    case EGREATEREQU: \
    case EEQU: \
    case ENOTEQU: \
    case EAND: \
    case EXOR: \
    case EOR: \
    case ELAND: \
    case ELOR: \
    case EASS: \
    case EMULASS: \
    case EDIVASS: \
    case EMODASS: \
    case EADDASS: \
    case ESUBASS: \
    case ESHLASS: \
    case ESHRASS: \
    case EANDASS: \
    case EXORASS: \
    case EORASS: \
    case ECOMMA: \
    case EPMODULO: \
    case EROTL: \
    case EROTR: \
    case EBCLR: \
    case EBTST: \
    case EBSET

#if MWCCPSP
// 3.0.1 order, from the expression dumper's name table in the exe (its
// cases follow the enum: EINTCONST is 0x34 there, and intconstnode stores
// 0x34). New: EMIN/EMAX (GCC <? and >?), scope markers, EOBJACCESS,
// EPOINTERSTAR/EDOTSTAR, ECTORINIT, ESTMT. 2.4.7's EOBJLIST, EMEMBER and
// EVECTOR128CONST are not in the table; they get values past MAXEXPR so the
// 2.4.7 code that names them still compiles.
typedef enum ENodeType {
    EPOSTINC,
    EPOSTDEC,
    EPREINC,
    EPREDEC,
    EINDIRECT,
    EMONMIN,
    EBINNOT,
    ELOGNOT,
    EFORCELOAD,
    EMUL,
    EMULV,
    EDIV,
    EMODULO,
    EADDV,
    ESUBV,
    EADD,
    ESUB,
    ESHL,
    ESHR,
    ELESS,
    EGREATER,
    ELESSEQU,
    EGREATEREQU,
    EEQU,
    ENOTEQU,
    EAND,
    EXOR,
    EOR,
    ELAND,
    ELOR,
    EASS,
    EMULASS,
    EDIVASS,
    EMODASS,
    EADDASS,
    ESUBASS,
    ESHLASS,
    ESHRASS,
    EANDASS,
    EXORASS,
    EORASS,
    ECOMMA,
    EMIN,
    EMAX,
    EPMODULO,
    EROTL,
    EROTR,
    EBCLR,
    EBTST,
    EBSET,
    ETYPCON,
    EBITFIELD,
    EINTCONST,
    EFLOATCONST,
    ESTRINGCONST,
    ECOND,
    EFUNCCALL,
    EFUNCCALLP,
    EOBJREF,
    ENULLCHECK,
    EPRECOMP,
    ELABEL,
    ESCOPEBEGIN,
    ESCOPEEND,
    EMFPOINTER,
    ETEMP,
    ELOCOBJ,
    EARGOBJ,
    ESETCONST,
    ENEWEXCEPTION,
    ENEWEXCEPTIONARRAY,
    EINITTRYCATCH,
    EOBJACCESS,
    ETEMPLDEP,
    EPOINTERSTAR,
    EDOTSTAR,
    ECTORINIT,
    ESTMT,
    EINSTRUCTION,
    EDEFINE,
    EREUSE,
    EASSBLK,
    ECONDASS,
    MAXEXPR,
    EOBJLIST,
    EVECTOR128CONST = EOBJLIST + 2,
    // 3.0.1's EOBJACCESS took 2.4.7's EMEMBER place: tests of the node kind
    // are the same, the payload is an EObjAccess (not an EMemberInfo)
    EMEMBER = EOBJACCESS
} ENodeType;
#else
typedef enum ENodeType {
    EPOSTINC,
    EPOSTDEC,
    EPREINC,
    EPREDEC,
    EINDIRECT,
    EMONMIN,
    EBINNOT,
    ELOGNOT,
    EFORCELOAD,
    EMUL,
    EMULV,
    EDIV,
    EMODULO,
    EADDV,
    ESUBV,
    EADD,
    ESUB,
    ESHL,
    ESHR,
    ELESS,
    EGREATER,
    ELESSEQU,
    EGREATEREQU,
    EEQU,
    ENOTEQU,
    EAND,
    EXOR,
    EOR,
    ELAND,
    ELOR,
    EASS,
    EMULASS,
    EDIVASS,
    EMODASS,
    EADDASS,
    ESUBASS,
    ESHLASS,
    ESHRASS,
    EANDASS,
    EXORASS,
    EORASS,
    ECOMMA,
    EPMODULO,
    EROTL,
    EROTR,
    EBCLR,
    EBTST,
    EBSET,
    ETYPCON,
    EBITFIELD,
    EINTCONST,
    EFLOATCONST,
    ESTRINGCONST,
    ECOND,
    EFUNCCALL,
    EFUNCCALLP,
    EOBJREF,
    EMFPOINTER,
    ENULLCHECK,
    EPRECOMP,
    ETEMP,
    EARGOBJ,
    ELOCOBJ,
    ELABEL,
    ESETCONST,
    ENEWEXCEPTION,
    ENEWEXCEPTIONARRAY,
    EINITTRYCATCH,
    EOBJLIST,
    EMEMBER,
    ETEMPLDEP,
    EINSTRUCTION,
    EDEFINE,
    EREUSE,
    EASSBLK,
    EVECTOR128CONST,
    ECONDASS,
    MAXEXPR
} ENodeType;
#endif
#if MWCCPSP
// 3.0.1's optimizer tables cover 124 node kinds (every initializer clears
// 124 entries and uses 124 as "no such operator"); only 83 are named.
#define IRO_MAXEXPR 124
#else
#endif
enum {
    ENODE_FLAG_CONST = Q_CONST,
    ENODE_FLAG_VOLATILE = Q_VOLATILE,
    ENODE_FLAG_QUALS = Q_CONST | Q_VOLATILE,
    ENODE_FLAG_SOM_ENV_CHECK = 0x10,
    ENODE_FLAG_NONVIRTUAL = 0x80,
    ENODE_FLAG_PARENS = 0x80,
    ENODE_FLAG_SAME_TYPE_CAST = 0x80,
    ENODE_FLAG_RESULT_VIA_TEMP = 0x80
};
#pragma pack(push, 2)
struct EMemberInfo {
    BClassList* path;
    ENode* expr;
    NameSpaceObjectList* list;
    TemplArg* templargs;
    Boolean is_qualified;
    Boolean addressTaken;
    Boolean isambig;
};
struct ENodeList {
    ENodeList* next;
    ENode* node;
};
#if MWCCPSP
// 3.0.1's numbering (the values CExpr_NewTemplDepENode is called with):
// sizeof and alignof share a kind (a selector byte follows the operand),
// qualified template names and object accesses are new
typedef enum TemplDepSubType {
    TDE_PARAM, // 0
    TDE_SIZEOF, // 1
    TDE_CAST, // 2
    TDE_QUALNAME, // 3
    TDE_QUALTEMPL, // 4
    TDE_OBJACCESS, // 5
    TDE_SOURCEREF, // 6
    TDE_FUNCCALL, // 7: f(args) (CExpr_MakeFunctionCall builds the node itself)
    TDE_LOCAL, // 8
    TDE_ADDRESS_OF, // 9: &x
    TDE_UNARYPLUS, // 10: +x
    TDE_DEREF, // 11: *x
    // 2.4.7 kinds 3.0.1 does not have (alignof is TDE_SIZEOF with a
    // selector; a template class's object is a TDE_OBJACCESS); values of
    // their own so the 2.4.7 switches still compile
    TDE_ALIGNOF = 0x7E,
    TDE_OBJ = 0x7F
} TemplDepSubType;
#else
typedef enum TemplDepSubType {
    TDE_PARAM,
    TDE_SIZEOF,
    TDE_ALIGNOF,
    TDE_CAST,
    TDE_QUALNAME,
    TDE_OBJ,
    TDE_SOURCEREF,
    TDE_ADDRESS_OF
} TemplDepSubType;
#endif
#if MWCCPSP
// 3.0.1: a name's lookup result on its way to an expression; carried whole
// in EOBJACCESS nodes and template-dependent object accesses
typedef struct EObjAccess {
    struct NameSpaceObjectList* list;
    struct TypeClass* tclass; // the class the name was found in
    struct BClassList* path;
    TemplArg* templargs; // name<args>
    struct HashNameNode* name; // unqualified call: argument-dependent lookup
    ENode* expr; // the object expression (x.name, x->name)
    Boolean is_qualified;
    Boolean nonstatic; // a non-static member (function) is among them
    UInt8 isambig : 1;
    UInt8 addressTaken : 1; // &C::m
    UInt8 externalOnly : 1; // argument-dependent lookup keeps only external functions
} EObjAccess;
#endif
union ENodeUnion {
#if MWCCPSP
    UInt8 size20[0x20]; // 3.0.1: the operands take 0x20 bytes
    EObjAccess objaccess;
#endif
    CInt64 intval;
    Float floatval;
    MWVector128 vector128val;
    SInt32 longval;
    ENode* monadic;
    EMemberInfo* emember;
    Object* objref;
    struct CLabel* label;
    struct {
        struct NameSpaceObjectList* list;
        TemplArg* templargs;
        struct HashNameNode* name;
    } objlist;
    struct {
        ENode* left;
        ENode* right;
    } diadic;
    struct {
        ENode* accessnode;
        ENode* mfpointer;
    } mfpointer;
    struct {
        ENode* nullcheckexpr;
        ENode* condexpr;
        SInt32 precompid;
    } nullcheck;
    SInt32 precompid;
    struct {
        ENode* cond;
        ENode* expr1;
        ENode* expr2;
    } cond;
    struct {
        ENode* funcref;
        ENodeList* args;
        TypeFunc* functype;
    } funccall;
    struct {
        Type* type;
        SInt32 uniqueid;
        Boolean needs_dtor;
    } temp;
    struct {
        SInt32 size;
        char* data;
#if MWCCPSP
        SInt32
            inlineid; // 3.0.1: numbers strings in inline functions with external linkage (primary expression, 0x516990)
#endif
        unsigned char ispascal;
    } string;
    struct {
        Object* objref;
        SInt32 offset;
    } addr;
    struct {
        ENode* initexpr;
        ENode* tryexpr;
        Object* pointertemp;
        Object* deletefunc;
    } newexception;
    struct {
        ENode* initexpr;
        ENode* tryexpr;
        ENode* catchexpr;
        ENode* result;
    } itc;
    struct {
        union {
            TemplParamID pid;
            struct {
                Type* type;
            } typeexpr;
            struct {
                ENodeList* args;
                Type* type;
                UInt32 qual;
            } cast;
            struct {
                struct TypeTemplDep* type;
                HashNameNode* name;
            } qual;
            struct {
                ENode* expr;
                struct TStreamElement* token;
            } sourceref;
            ENode* monadic;
            Object* obj;
        } u;
#if MWCCPSP
        UInt8 pad[0x10];
        UInt8 subtype; // 3.0.1: a byte in the node's last word (+0x3C)
#else
        TemplDepSubType subtype;
#endif
    } templdep;
};
struct ENode {
#if MWCCPSP
    // 3.0.1: 0x40 bytes (nodes are lalloc(0x40)+memclrw, copied whole);
    // rtype at +0x0C and the operands at +0x20 (makediadicnode,
    // intconstnode). The other fields' places are provisional.
    UInt8 type;
    UInt8 cost;
    UInt16 flags;
    Boolean ignored;
    Boolean hascall;
    UInt8 unk06[6];
    Type* rtype;
    struct PointsToFunction* pointsTo;
    UInt8 unk14[0x0C];
    ENodeUnion data;
#else
    ENodeType type;
    UInt8 cost;
    UInt16 flags;
    Boolean ignored;
    Boolean hascall;
    Type* rtype;
    struct PointsToFunction* pointsTo;
    ENodeUnion data;
#endif
};
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif
