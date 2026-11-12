// mwccpsp 3.0.1 Intrinsics.c (MIPS): the builtin functions. 2.4.7's file is
// PowerPC and stays out of the build. The registration list was generated
// from the original (0x45B910) by the session's genintr.py; the argument
// types are what the exe registers (some are not what the names suggest).
#include "compiler/common.h"
#include "compiler/CError.h"
#include "compiler/CParser.h"
#include "compiler/CDecl.h"
#include "compiler/CScope.h"
#include "compiler/CMachine.h"
#include "compiler/CompilerTools.h"
#include "compiler/enode.h"
#include "compiler/objects.h"
#include "compiler/types.h"
#include "compiler/CFunc.h"
#include "compiler/CPrep.h"
#include "compiler/CMangler.h"
#include "psp/BE.h"
#include "compiler/CInt64.h"

extern Type* CABI_GetSizeTType(void);

typedef struct OpcodeDef {
    char* name;
    char* alias;
    char* format;
    UInt8 nargs;
    UInt8 size;
    UInt8 pad0E[2];
    UInt32 unk10;
    UInt8 pad14[4];
    UInt32 flags;
    UInt32 flags2;
    char* intrinsic;
    SInt32 intrinsicid;
    SInt32 unk28;
    UInt16 unk2C;
} OpcodeDef;

extern OpcodeDef data_006115F8[]; // OpcodeTable.c
extern TypePointer char_ptr; // 0x5CCB34: char *

Object* intrinsics_objects[0x4AC]; // 0x5E9AD0 (b210 0x601CD8), by intrinsic id

// 0x45B8C0: point a call at the intrinsic's object
void func_0045B8C0(ENode* funccall, short id) {
    funccall->data.funccall.funcref->data.objref = intrinsics_objects[id];
}

// 0x45B8E0: is this a call to an intrinsic? (callers test the whole
// register, so the result is an int)
int func_0045B8E0(ENode* funccall) {
    ENode* funcref = funccall->data.funccall.funcref;
    int result = 0;

    if (ENODE_IS(funcref, EOBJREF) &&
        funcref->data.objref->datatype == DFUNC &&
        (TYPE_FUNC(funcref->data.objref->type)->flags & FUNC_INTRINSIC)) {
        result++;
    }
    return result;
}

// 0x45C520: a builtin for every opcode that names one, with an argument per
// register or immediate operand in its format (unsigned for registers and
// unsigned fields, signed for signed fields)
#if MWCCPS2
// b210 (0x467D60): the opcode table indexed directly
static void Intrinsics_SetupOpcodeBuiltins(void) {
    UInt16 op;
    char* p;
    int nargs;
    SInt32 id;
    Type* args[6];

    for (op = 0; op <= 0x3E5; op++) {
        if (!data_006115F8[op].intrinsic) {
            continue;
        }
        nargs = 0;
        for (p = data_006115F8[op].format; *p && *p != ';'; p++) {
            if (nargs > 4) {
                CError_FATAL(92);
            }
            switch (*p) {
                case '(':
                    break;
                case '<':
                case '>':
                case '@':
                case 'B':
                case 'C':
                case 'X':
                case 'c':
                case 'k':
                    args[nargs++] = TYPE(&stunsignedlong);
                    break;
                case 'D':
                case 'E':
                case 'G':
                case 'N':
                case 'S':
                case 'T':
                case 'V':
                case 'W':
                case 'd':
                case 's':
                case 't':
                    args[nargs++] = TYPE(&stsignedlong);
                    break;
                case 'i':
                case 'u':
                    args[nargs++] = TYPE(&stunsignedlong);
                    break;
                case 'j':
                case 'o':
                    args[nargs++] = TYPE(&stsignedlong);
                    break;
                case 'I':
                    args[nargs++] = TYPE(&stunsignedlong);
                    break;
            }
        }
        if (nargs == 0) {
            intrinsics_objects[data_006115F8[op].intrinsicid] = CParser_NewRTFunc(
                TYPE(&stunsignedlong), GetHashNameNodeExport(data_006115F8[op].intrinsic), 0, 0
            );
        } else if (nargs == 1) {
            intrinsics_objects[data_006115F8[op].intrinsicid] = CParser_NewRTFunc(
                TYPE(&stunsignedlong),
                GetHashNameNodeExport(data_006115F8[op].intrinsic),
                0,
                1,
                args[0]
            );
        } else if (nargs == 2) {
            intrinsics_objects[data_006115F8[op].intrinsicid] = CParser_NewRTFunc(
                TYPE(&stunsignedlong),
                GetHashNameNodeExport(data_006115F8[op].intrinsic),
                0,
                2,
                args[0],
                args[1]
            );
        } else if (nargs == 3) {
            intrinsics_objects[data_006115F8[op].intrinsicid] = CParser_NewRTFunc(
                TYPE(&stunsignedlong),
                GetHashNameNodeExport(data_006115F8[op].intrinsic),
                0,
                3,
                args[0],
                args[1],
                args[2]
            );
        } else if (nargs == 4) {
            intrinsics_objects[data_006115F8[op].intrinsicid] = CParser_NewRTFunc(
                TYPE(&stunsignedlong),
                GetHashNameNodeExport(data_006115F8[op].intrinsic),
                0,
                4,
                args[0],
                args[1],
                args[2],
                args[3]
            );
        }
        id = data_006115F8[op].intrinsicid;
        intrinsics_objects[id]->u.func.u.intrinsicid = id;
        TYPE_FUNC(intrinsics_objects[id]->type)->flags |= FUNC_INTRINSIC;
        CScope_AddGlobalObject(intrinsics_objects[id]);
    }
}
#else
static void Intrinsics_SetupOpcodeBuiltins(void) {
    UInt16 op;
    OpcodeDef* def;
    char* p;
    int nargs;
    SInt32 id;
    Type* args[6];

    for (op = 0; op <= 0x3E5; op++) {
        def = &data_006115F8[op];
        if (!def->intrinsic) {
            continue;
        }
        nargs = 0;
        for (p = def->format; *p && *p != ';'; p++) {
            if (nargs > 4) {
                CError_FATAL(92);
            }
            switch (*p) {
#if MWCCPS2
                case '(':
                    break;
                case '<':
                case '>':
                case '@':
                case 'B':
                case 'C':
                case 'X':
                case 'c':
                case 'k':
                    args[nargs++] = TYPE(&stunsignedlong);
                    break;
                case 'D':
                case 'E':
                case 'G':
                case 'N':
                case 'S':
                case 'T':
                case 'V':
                case 'W':
                case 'd':
                case 's':
                case 't':
                    args[nargs++] = TYPE(&stsignedlong);
                    break;
                case 'i':
                case 'u':
                    args[nargs++] = TYPE(&stunsignedlong);
                    break;
                case 'j':
                case 'o':
                    args[nargs++] = TYPE(&stsignedlong);
                    break;
                case 'I':
                    args[nargs++] = TYPE(&stunsignedlong);
                    break;
#else
                case '<':
                case '>':
                case '@':
                case 'B':
                case 'C':
                case 'X':
                case 'c':
                case 'k':
                case 'I':
                case 'i':
                case 'u':
                    args[nargs++] = TYPE(&stunsignedlong);
                    break;
                case 'D':
                case 'E':
                case 'G':
                case 'N':
                case 'S':
                case 'T':
                case 'V':
                case 'W':
                case 'd':
                case 's':
                case 't':
                case 'j':
                case 'o':
                    args[nargs++] = TYPE(&stsignedlong);
                    break;
#endif
            }
        }
        if (nargs == 0) {
            intrinsics_objects[def->intrinsicid] = CParser_NewRTFunc(
                TYPE(&stunsignedlong), GetHashNameNodeExport(def->intrinsic), 0, 0
            );
        } else if (nargs == 1) {
            intrinsics_objects[def->intrinsicid] = CParser_NewRTFunc(
                TYPE(&stunsignedlong), GetHashNameNodeExport(def->intrinsic), 0, 1, args[0]
            );
        } else if (nargs == 2) {
            intrinsics_objects[def->intrinsicid] = CParser_NewRTFunc(
                TYPE(&stunsignedlong), GetHashNameNodeExport(def->intrinsic), 0, 2, args[0], args[1]
            );
        } else if (nargs == 3) {
            intrinsics_objects[def->intrinsicid] = CParser_NewRTFunc(
                TYPE(&stunsignedlong),
                GetHashNameNodeExport(def->intrinsic),
                0,
                3,
                args[0],
                args[1],
                args[2]
            );
        } else if (nargs == 4) {
            intrinsics_objects[def->intrinsicid] = CParser_NewRTFunc(
                TYPE(&stunsignedlong),
                GetHashNameNodeExport(def->intrinsic),
                0,
                4,
                args[0],
                args[1],
                args[2],
                args[3]
            );
        }
        id = def->intrinsicid;
        intrinsics_objects[id]->u.func.u.intrinsicid = id;
        TYPE_FUNC(intrinsics_objects[id]->type)->flags |= FUNC_INTRINSIC;
        CScope_AddGlobalObject(intrinsics_objects[id]);
    }
}
#endif

#if MWCCPS2
// mwccps2's builtins (the R5900 MMI and VU0 intrinsics through helpers);
// generated from the original (0x466400)
static void Intrinsics_DefInt128_6(
    int id, char* name, Type* a1, Type* a2, Type* a3, Type* a4, Type* a5, Type* a6
);
static void Intrinsics_DefInt128_5(
    int id, char* name, Type* a1, Type* a2, Type* a3, Type* a4, Type* a5
);
static void Intrinsics_DefFloat_5(
    int id, char* name, Type* a1, Type* a2, Type* a3, Type* a4, Type* a5
);
static void Intrinsics_DefInt128_4(int id, char* name, Type* a1, Type* a2, Type* a3, Type* a4);
static void Intrinsics_DefFloat_3(int id, char* name, Type* a1, Type* a2, Type* a3);
static void Intrinsics_DefInt128_3(int id, char* name, Type* a1, Type* a2, Type* a3);
static void Intrinsics_DefVoid2(int id, char* name, Type* type);
static void Intrinsics_DefBinary(int id, char* name, Type* type);
static void Intrinsics_DefUnary(int id, char* name, Type* type);

void func_0045B910(void) {
    Boolean savecpp;
    int i;

    savecpp = copts.cplusplus;
    copts.cplusplus = FALSE;
    for (i = 0; i < 0x4AC; i++) {
        intrinsics_objects[i] = NULL;
    }
    Intrinsics_SetupOpcodeBuiltins();
    intrinsics_objects[1] =
        CParser_NewRTFunc(TYPE(&stvoid), GetHashNameNodeExport("__asm_start"), 0, 0);
    intrinsics_objects[1]->u.func.u.intrinsicid = 1;
    TYPE_FUNC(intrinsics_objects[1]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[1]);
    intrinsics_objects[2] =
        CParser_NewRTFunc(TYPE(&stvoid), GetHashNameNodeExport("__asm_start_reord"), 0, 0);
    intrinsics_objects[2]->u.func.u.intrinsicid = 2;
    TYPE_FUNC(intrinsics_objects[2]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[2]);
    intrinsics_objects[3] =
        CParser_NewRTFunc(TYPE(&stvoid), GetHashNameNodeExport("__asm_end"), 0, 0);
    intrinsics_objects[3]->u.func.u.intrinsicid = 3;
    TYPE_FUNC(intrinsics_objects[3]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[3]);
    intrinsics_objects[4] = CParser_NewRTFunc(
        TYPE(&stvoid),
        GetHashNameNodeExport("__evaluate"),
        0,
        2,
        TYPE(&stsignedlong),
        TYPE(&stsignedlong)
    );
    intrinsics_objects[4]->u.func.u.intrinsicid = 4;
    TYPE_FUNC(intrinsics_objects[4]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[4]);
    if (copts.mips_long64) {
        intrinsics_objects[5] = CParser_NewRTFunc(
            TYPE(&stvoid), GetHashNameNodeExport("__builtin_initialize"), 0, 1, TYPE(&void_ptr)
        );
        intrinsics_objects[5]->u.func.u.intrinsicid = 5;
        TYPE_FUNC(intrinsics_objects[5]->type)->flags |= FUNC_INTRINSIC;
        CScope_AddGlobalObject(intrinsics_objects[5]);
        Intrinsics_DefUnary(6, "__builtin_args_info", TYPE(&stsignedint));
    }
    Intrinsics_DefUnary(38, "_pabsh", TYPE(&stint128));
    Intrinsics_DefUnary(39, "_pabsw", TYPE(&stint128));
    Intrinsics_DefBinary(40, "_paddb", TYPE(&stint128));
    Intrinsics_DefBinary(41, "_paddh", TYPE(&stint128));
    Intrinsics_DefBinary(42, "_paddsb", TYPE(&stint128));
    Intrinsics_DefBinary(43, "_paddsh", TYPE(&stint128));
    Intrinsics_DefBinary(44, "_paddsw", TYPE(&stint128));
    Intrinsics_DefBinary(45, "_paddub", TYPE(&stint128));
    Intrinsics_DefBinary(46, "_padduh", TYPE(&stint128));
    Intrinsics_DefBinary(47, "_padduw", TYPE(&stint128));
    Intrinsics_DefBinary(48, "_paddw", TYPE(&stint128));
    Intrinsics_DefBinary(49, "_padsbh", TYPE(&stint128));
    Intrinsics_DefBinary(50, "_pand", TYPE(&stint128));
    Intrinsics_DefBinary(51, "_pceqb", TYPE(&stint128));
    Intrinsics_DefBinary(52, "_pceqh", TYPE(&stint128));
    Intrinsics_DefBinary(53, "_pceqw", TYPE(&stint128));
    Intrinsics_DefBinary(54, "_pcgtb", TYPE(&stint128));
    Intrinsics_DefBinary(55, "_pcgth", TYPE(&stint128));
    Intrinsics_DefBinary(56, "_pcgtw", TYPE(&stint128));
    Intrinsics_DefUnary(57, "_pcpyh", TYPE(&stint128));
    Intrinsics_DefBinary(58, "_pcpyld", TYPE(&stint128));
    Intrinsics_DefBinary(59, "_pcpyud", TYPE(&stint128));
    Intrinsics_DefVoid2(60, "_pdivbw", TYPE(&stint128));
    Intrinsics_DefVoid2(61, "_pdivuw", TYPE(&stint128));
    Intrinsics_DefVoid2(62, "_pdivw", TYPE(&stint128));
    Intrinsics_DefBinary(63, "_phmadh", TYPE(&stint128));
    Intrinsics_DefBinary(64, "_phmsbh", TYPE(&stint128));
    Intrinsics_DefBinary(65, "_pmaddh", TYPE(&stint128));
    Intrinsics_DefBinary(66, "_pmadduw", TYPE(&stint128));
    Intrinsics_DefBinary(67, "_pmaddw", TYPE(&stint128));
    Intrinsics_DefBinary(68, "_pmsubh", TYPE(&stint128));
    Intrinsics_DefBinary(69, "_pmsubw", TYPE(&stint128));
    Intrinsics_DefBinary(70, "_pmulth", TYPE(&stint128));
    Intrinsics_DefBinary(71, "_pmultuw", TYPE(&stint128));
    Intrinsics_DefBinary(72, "_pmultw", TYPE(&stint128));
    Intrinsics_DefUnary(73, "_pexch", TYPE(&stint128));
    Intrinsics_DefUnary(74, "_pexcw", TYPE(&stint128));
    Intrinsics_DefUnary(75, "_pexeh", TYPE(&stint128));
    Intrinsics_DefUnary(76, "_pexew", TYPE(&stint128));
    Intrinsics_DefUnary(77, "_pext5", TYPE(&stint128));
    Intrinsics_DefBinary(78, "_pextlb", TYPE(&stint128));
    Intrinsics_DefBinary(79, "_pextlh", TYPE(&stint128));
    Intrinsics_DefBinary(80, "_pextlw", TYPE(&stint128));
    Intrinsics_DefBinary(81, "_pextub", TYPE(&stint128));
    Intrinsics_DefBinary(82, "_pextuh", TYPE(&stint128));
    Intrinsics_DefBinary(83, "_pextuw", TYPE(&stint128));
    Intrinsics_DefBinary(84, "_pinth", TYPE(&stint128));
    Intrinsics_DefBinary(85, "_pinteh", TYPE(&stint128));
    Intrinsics_DefUnary(86, "_plzcw", TYPE(&stint128));
    Intrinsics_DefBinary(87, "_pmaxh", TYPE(&stint128));
    Intrinsics_DefBinary(88, "_pmaxw", TYPE(&stint128));
    Intrinsics_DefBinary(89, "_pminh", TYPE(&stint128));
    Intrinsics_DefBinary(90, "_pminw", TYPE(&stint128));
    Intrinsics_DefBinary(91, "_pnor", TYPE(&stint128));
    Intrinsics_DefBinary(92, "_por", TYPE(&stint128));
    Intrinsics_DefUnary(93, "_ppac5", TYPE(&stint128));
    Intrinsics_DefBinary(94, "_ppacb", TYPE(&stint128));
    Intrinsics_DefBinary(95, "_ppach", TYPE(&stint128));
    Intrinsics_DefBinary(96, "_ppacw", TYPE(&stint128));
    Intrinsics_DefUnary(97, "_prevh", TYPE(&stint128));
    Intrinsics_DefUnary(98, "_prot3w", TYPE(&stint128));
    Intrinsics_DefBinary(99, "_psllh", TYPE(&stint128));
    Intrinsics_DefBinary(100, "_psllvw", TYPE(&stint128));
    Intrinsics_DefBinary(101, "_psllw", TYPE(&stint128));
    Intrinsics_DefBinary(102, "_psrah", TYPE(&stint128));
    Intrinsics_DefBinary(103, "_psravw", TYPE(&stint128));
    Intrinsics_DefBinary(104, "_psraw", TYPE(&stint128));
    Intrinsics_DefBinary(105, "_psrlh", TYPE(&stint128));
    Intrinsics_DefBinary(106, "_psrlvw", TYPE(&stint128));
    Intrinsics_DefBinary(107, "_psrlw", TYPE(&stint128));
    Intrinsics_DefBinary(108, "_psubb", TYPE(&stint128));
    Intrinsics_DefBinary(109, "_psubh", TYPE(&stint128));
    Intrinsics_DefBinary(110, "_psubsb", TYPE(&stint128));
    Intrinsics_DefBinary(111, "_psubsh", TYPE(&stint128));
    Intrinsics_DefBinary(112, "_psubsw", TYPE(&stint128));
    Intrinsics_DefBinary(113, "_psubub", TYPE(&stint128));
    Intrinsics_DefBinary(114, "_psubuh", TYPE(&stint128));
    Intrinsics_DefBinary(115, "_psubuw", TYPE(&stint128));
    Intrinsics_DefBinary(116, "_psubw", TYPE(&stint128));
    Intrinsics_DefBinary(117, "_pxor", TYPE(&stint128));
    Intrinsics_DefVoid2(118, "_mtsab", TYPE(&stint128));
    Intrinsics_DefVoid2(119, "_mtsah", TYPE(&stint128));
    intrinsics_objects[120] =
        CParser_NewRTFunc(TYPE(&stvoid), GetHashNameNodeExport("_mtsa"), 0, 1, TYPE(&stint128));
    intrinsics_objects[120]->u.func.u.intrinsicid = 120;
    TYPE_FUNC(intrinsics_objects[120]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[120]);
    intrinsics_objects[121] =
        CParser_NewRTFunc(TYPE(&stint128), GetHashNameNodeExport("_mfsa"), 0, 0);
    intrinsics_objects[121]->u.func.u.intrinsicid = 121;
    TYPE_FUNC(intrinsics_objects[121]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[121]);
    Intrinsics_DefBinary(122, "_qfsrv", TYPE(&stint128));
    intrinsics_objects[123] =
        CParser_NewRTFunc(TYPE(&stint128), GetHashNameNodeExport("_pmfhi"), 0, 0);
    intrinsics_objects[123]->u.func.u.intrinsicid = 123;
    TYPE_FUNC(intrinsics_objects[123]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[123]);
    intrinsics_objects[124] =
        CParser_NewRTFunc(TYPE(&stint128), GetHashNameNodeExport("_pmflo"), 0, 0);
    intrinsics_objects[124]->u.func.u.intrinsicid = 124;
    TYPE_FUNC(intrinsics_objects[124]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[124]);
    intrinsics_objects[125] =
        CParser_NewRTFunc(TYPE(&stint128), GetHashNameNodeExport("_pmfhl_lw"), 0, 0);
    intrinsics_objects[125]->u.func.u.intrinsicid = 125;
    TYPE_FUNC(intrinsics_objects[125]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[125]);
    intrinsics_objects[126] =
        CParser_NewRTFunc(TYPE(&stint128), GetHashNameNodeExport("_pmfhl_uw"), 0, 0);
    intrinsics_objects[126]->u.func.u.intrinsicid = 126;
    TYPE_FUNC(intrinsics_objects[126]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[126]);
    intrinsics_objects[127] =
        CParser_NewRTFunc(TYPE(&stint128), GetHashNameNodeExport("_pmfhl_slw"), 0, 0);
    intrinsics_objects[127]->u.func.u.intrinsicid = 127;
    TYPE_FUNC(intrinsics_objects[127]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[127]);
    intrinsics_objects[128] =
        CParser_NewRTFunc(TYPE(&stint128), GetHashNameNodeExport("_pmfhl_lh"), 0, 0);
    intrinsics_objects[128]->u.func.u.intrinsicid = 128;
    TYPE_FUNC(intrinsics_objects[128]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[128]);
    intrinsics_objects[129] =
        CParser_NewRTFunc(TYPE(&stint128), GetHashNameNodeExport("_pmfhl_sh"), 0, 0);
    intrinsics_objects[129]->u.func.u.intrinsicid = 129;
    TYPE_FUNC(intrinsics_objects[129]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[129]);
    intrinsics_objects[130] =
        CParser_NewRTFunc(TYPE(&stvoid), GetHashNameNodeExport("_pmthi"), 0, 1, TYPE(&stint128));
    intrinsics_objects[130]->u.func.u.intrinsicid = 130;
    TYPE_FUNC(intrinsics_objects[130]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[130]);
    intrinsics_objects[131] =
        CParser_NewRTFunc(TYPE(&stvoid), GetHashNameNodeExport("_pmtlo"), 0, 1, TYPE(&stint128));
    intrinsics_objects[131]->u.func.u.intrinsicid = 131;
    TYPE_FUNC(intrinsics_objects[131]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[131]);
    intrinsics_objects[132] =
        CParser_NewRTFunc(TYPE(&stvoid), GetHashNameNodeExport("_pmthl_lw"), 0, 1, TYPE(&stint128));
    intrinsics_objects[132]->u.func.u.intrinsicid = 132;
    TYPE_FUNC(intrinsics_objects[132]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[132]);
    Intrinsics_DefInt128_3(133, "_vabs", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128));
    Intrinsics_DefInt128_3(134, "_vmove", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128));
    Intrinsics_DefInt128_3(135, "_vmr32", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128));
    Intrinsics_DefInt128_4(
        136, "_vadd", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128), TYPE(&stint128)
    );
    Intrinsics_DefInt128_4(
        137, "_vaddf", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128), TYPE(&stfloat)
    );
    Intrinsics_DefInt128_5(
        138,
        "_vaddbc",
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128)
    );
    Intrinsics_DefInt128_5(
        139,
        "_vmadd",
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128)
    );
    Intrinsics_DefInt128_5(
        140,
        "_vmaddf",
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stfloat)
    );
    Intrinsics_DefInt128_6(
        141,
        "_vmaddbc",
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128)
    );
    Intrinsics_DefInt128_4(
        142, "_vmax", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128), TYPE(&stint128)
    );
    Intrinsics_DefInt128_4(
        143, "_vmaxf", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128), TYPE(&stfloat)
    );
    Intrinsics_DefInt128_5(
        144,
        "_vmaxbc",
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128)
    );
    Intrinsics_DefInt128_4(
        145, "_vmini", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128), TYPE(&stint128)
    );
    Intrinsics_DefInt128_4(
        146, "_vminif", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128), TYPE(&stfloat)
    );
    Intrinsics_DefInt128_5(
        147,
        "_vminibc",
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128)
    );
    Intrinsics_DefInt128_5(
        148,
        "_vmsub",
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128)
    );
    Intrinsics_DefInt128_5(
        149,
        "_vmsubf",
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stfloat)
    );
    Intrinsics_DefInt128_6(
        150,
        "_vmsubbc",
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128)
    );
    Intrinsics_DefInt128_4(
        151, "_vmul", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128), TYPE(&stint128)
    );
    Intrinsics_DefInt128_4(
        152, "_vmulf", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128), TYPE(&stfloat)
    );
    Intrinsics_DefInt128_5(
        153,
        "_vmulbc",
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128)
    );
    Intrinsics_DefInt128_4(
        154, "_vsub", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128), TYPE(&stint128)
    );
    Intrinsics_DefInt128_4(
        155, "_vsubf", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128), TYPE(&stfloat)
    );
    Intrinsics_DefInt128_5(
        156,
        "_vsubbc",
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128)
    );
    Intrinsics_DefInt128_3(157, "_vopmula", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128));
    Intrinsics_DefInt128_4(
        158, "_vopmsub", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128), TYPE(&stint128)
    );
    Intrinsics_DefFloat_5(
        159,
        "_vdiv",
        TYPE(&stfloat),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128)
    );
    Intrinsics_DefFloat_3(160, "_vsqrt", TYPE(&stfloat), TYPE(&stint128), TYPE(&stint128));
    Intrinsics_DefFloat_5(
        161,
        "_vrsqrt",
        TYPE(&stfloat),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128),
        TYPE(&stint128)
    );
    intrinsics_objects[162] =
        CParser_NewRTFunc(TYPE(&stfloat), GetHashNameNodeExport("_vwaitq"), 0, 1, TYPE(&stfloat));
    intrinsics_objects[162]->u.func.u.intrinsicid = 162;
    TYPE_FUNC(intrinsics_objects[162]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[162]);
    intrinsics_objects[163] = CParser_NewRTFunc(
        TYPE(&stint128),
        GetHashNameNodeExport("_vrget"),
        0,
        3,
        TYPE(&stint128),
        TYPE(&stsignedint),
        TYPE(&stfloat)
    );
    intrinsics_objects[163]->u.func.u.intrinsicid = 163;
    TYPE_FUNC(intrinsics_objects[163]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[163]);
    intrinsics_objects[164] = CParser_NewRTFunc(
        TYPE(&stfloat),
        GetHashNameNodeExport("_vrinit"),
        0,
        3,
        TYPE(&stfloat),
        TYPE(&stint128),
        TYPE(&stsignedint)
    );
    intrinsics_objects[164]->u.func.u.intrinsicid = 164;
    TYPE_FUNC(intrinsics_objects[164]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[164]);
    intrinsics_objects[165] = CParser_NewRTFunc(
        TYPE(&stint128),
        GetHashNameNodeExport("_vrnext"),
        0,
        3,
        TYPE(&stint128),
        TYPE(&stsignedint),
        TYPE(&stfloat)
    );
    intrinsics_objects[165]->u.func.u.intrinsicid = 165;
    TYPE_FUNC(intrinsics_objects[165]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[165]);
    intrinsics_objects[166] = CParser_NewRTFunc(
        TYPE(&stfloat),
        GetHashNameNodeExport("_vrxor"),
        0,
        3,
        TYPE(&stfloat),
        TYPE(&stint128),
        TYPE(&stsignedint)
    );
    intrinsics_objects[166]->u.func.u.intrinsicid = 166;
    TYPE_FUNC(intrinsics_objects[166]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[166]);
    intrinsics_objects[167] = CParser_NewRTFunc(
        TYPE(&stint128), GetHashNameNodeExport("_vclip"), 0, 2, TYPE(&stint128), TYPE(&stint128)
    );
    intrinsics_objects[167]->u.func.u.intrinsicid = 167;
    TYPE_FUNC(intrinsics_objects[167]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[167]);
    Intrinsics_DefInt128_3(168, "_vftoi0", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128));
    Intrinsics_DefInt128_3(169, "_vftoi4", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128));
    Intrinsics_DefInt128_3(170, "_vftoi12", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128));
    Intrinsics_DefInt128_3(171, "_vftoi15", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128));
    Intrinsics_DefInt128_3(172, "_vitof0", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128));
    Intrinsics_DefInt128_3(173, "_vitof4", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128));
    Intrinsics_DefInt128_3(174, "_vitof12", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128));
    Intrinsics_DefInt128_3(175, "_vitof15", TYPE(&stint128), TYPE(&stint128), TYPE(&stint128));
    intrinsics_objects[176] =
        CParser_NewRTFunc(TYPE(&stint128), GetHashNameNodeExport("_vnop"), 0, 0);
    intrinsics_objects[176]->u.func.u.intrinsicid = 176;
    TYPE_FUNC(intrinsics_objects[176]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[176]);
    intrinsics_objects[177] = CParser_NewRTFunc(
        TYPE(&stfloat), GetHashNameNodeExport("_vgetf"), 0, 2, TYPE(&stint128), TYPE(&stint128)
    );
    intrinsics_objects[177]->u.func.u.intrinsicid = 177;
    TYPE_FUNC(intrinsics_objects[177]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[177]);
    intrinsics_objects[0] = CParser_NewRTFunc(
        TYPE(&void_ptr), GetHashNameNodeExport("__alloca"), 0, 1, TYPE(&stunsignedlong)
    );
    intrinsics_objects[0]->u.func.u.intrinsicid = 0;
    TYPE_FUNC(intrinsics_objects[0]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[0]);
    Intrinsics_DefUnary(10, "__abs", TYPE(&stsignedint));
    Intrinsics_DefUnary(11, "__labs", TYPE(&stsignedlong));
    Intrinsics_DefUnary(12, "__s_abs", TYPE(&stfloat));
    Intrinsics_DefUnary(13, "__d_abs", TYPE(&stdouble));
    Intrinsics_DefBinary(14, "__mulhi", TYPE(&stsignedlong));
    Intrinsics_DefBinary(15, "__mulhiu", TYPE(&stsignedlong));
    Intrinsics_DefBinary(8, "__strcpy", TYPE(&stchar));
    Intrinsics_DefBinary(16, "__sqrt", TYPE(&stfloat));
    Intrinsics_DefBinary(17, "__rsqrt", TYPE(&stfloat));
    intrinsics_objects[9] = CParser_NewRTFunc(
        CDecl_NewPointerType(TYPE(&stvoid)),
        GetHashNameNodeExport("__memcpy"),
        0,
        3,
        CDecl_NewPointerType(TYPE(&stvoid)),
        CDecl_NewPointerType(TYPE(&stvoid)),
        CABI_GetSizeTType()
    );
    intrinsics_objects[9]->u.func.u.intrinsicid = 9;
    TYPE_FUNC(intrinsics_objects[9]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[9]);
    intrinsics_objects[18] = CParser_NewRTFunc(
        TYPE(&void_ptr),
        GetHashNameNodeExport("__clear"),
        0,
        2,
        TYPE(&char_ptr),
        CABI_GetSizeTType()
    );
    intrinsics_objects[18]->u.func.u.intrinsicid = 18;
    TYPE_FUNC(intrinsics_objects[18]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[18]);

    copts.cplusplus = savecpp;
}

// 0x4679D0: an intrinsic returning stint128 with 6 arguments
static void Intrinsics_DefInt128_6(
    int id, char* name, Type* a1, Type* a2, Type* a3, Type* a4, Type* a5, Type* a6
) {
    intrinsics_objects[id] = CParser_NewRTFunc(
        TYPE(&stint128), GetHashNameNodeExport(name), 0, 6, a1, a2, a3, a4, a5, a6
    );
    intrinsics_objects[id]->u.func.u.intrinsicid = id;
    TYPE_FUNC(intrinsics_objects[id]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[id]);
}

// 0x467A40: an intrinsic returning stint128 with 5 arguments
static void Intrinsics_DefInt128_5(
    int id, char* name, Type* a1, Type* a2, Type* a3, Type* a4, Type* a5
) {
    intrinsics_objects[id] =
        CParser_NewRTFunc(TYPE(&stint128), GetHashNameNodeExport(name), 0, 5, a1, a2, a3, a4, a5);
    intrinsics_objects[id]->u.func.u.intrinsicid = id;
    TYPE_FUNC(intrinsics_objects[id]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[id]);
}

// 0x467AB0: an intrinsic returning stfloat with 5 arguments
static void Intrinsics_DefFloat_5(
    int id, char* name, Type* a1, Type* a2, Type* a3, Type* a4, Type* a5
) {
    intrinsics_objects[id] =
        CParser_NewRTFunc(TYPE(&stfloat), GetHashNameNodeExport(name), 0, 5, a1, a2, a3, a4, a5);
    intrinsics_objects[id]->u.func.u.intrinsicid = id;
    TYPE_FUNC(intrinsics_objects[id]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[id]);
}

// 0x467B20: an intrinsic returning stint128 with 4 arguments
static void Intrinsics_DefInt128_4(int id, char* name, Type* a1, Type* a2, Type* a3, Type* a4) {
    intrinsics_objects[id] =
        CParser_NewRTFunc(TYPE(&stint128), GetHashNameNodeExport(name), 0, 4, a1, a2, a3, a4);
    intrinsics_objects[id]->u.func.u.intrinsicid = id;
    TYPE_FUNC(intrinsics_objects[id]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[id]);
}

// 0x467B80: an intrinsic returning stfloat with 3 arguments
static void Intrinsics_DefFloat_3(int id, char* name, Type* a1, Type* a2, Type* a3) {
    intrinsics_objects[id] =
        CParser_NewRTFunc(TYPE(&stfloat), GetHashNameNodeExport(name), 0, 3, a1, a2, a3);
    intrinsics_objects[id]->u.func.u.intrinsicid = id;
    TYPE_FUNC(intrinsics_objects[id]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[id]);
}

// 0x467BE0: an intrinsic returning stint128 with 3 arguments
static void Intrinsics_DefInt128_3(int id, char* name, Type* a1, Type* a2, Type* a3) {
    intrinsics_objects[id] =
        CParser_NewRTFunc(TYPE(&stint128), GetHashNameNodeExport(name), 0, 3, a1, a2, a3);
    intrinsics_objects[id]->u.func.u.intrinsicid = id;
    TYPE_FUNC(intrinsics_objects[id]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[id]);
}

// 0x467C40: void f(type, type)
static void Intrinsics_DefVoid2(int id, char* name, Type* type) {
    intrinsics_objects[id] =
        CParser_NewRTFunc(TYPE(&stvoid), GetHashNameNodeExport(name), 0, 2, type, type);
    intrinsics_objects[id]->u.func.u.intrinsicid = id;
    TYPE_FUNC(intrinsics_objects[id]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[id]);
}

// 0x467CA0: type f(type, type)
static void Intrinsics_DefBinary(int id, char* name, Type* type) {
    intrinsics_objects[id] = CParser_NewRTFunc(type, GetHashNameNodeExport(name), 0, 2, type, type);
    intrinsics_objects[id]->u.func.u.intrinsicid = id;
    TYPE_FUNC(intrinsics_objects[id]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[id]);
}

// 0x467D00: type f(type)
static void Intrinsics_DefUnary(int id, char* name, Type* type) {
    intrinsics_objects[id] = CParser_NewRTFunc(type, GetHashNameNodeExport(name), 0, 1, type);
    intrinsics_objects[id]->u.func.u.intrinsicid = id;
    TYPE_FUNC(intrinsics_objects[id]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[id]);
}
#endif

// 0x458530: whether obj is an intrinsic's runtime function
Boolean func_00458530(Object* obj) {
    int i;

    for (i = 0; i < 0x4AC; i++) {
        if (obj == intrinsics_objects[i]) {
            return TRUE;
        }
    }
    return FALSE;
}

#if !MWCCPS2
// 0x45B910 (CodeGen_SetupRuntimeObjects calls it)
void func_0045B910(void) {
    Boolean savecpp;
    int i;

    savecpp = copts.cplusplus;
    copts.cplusplus = FALSE;
    for (i = 0; i < 0x4A8; i++) {
        intrinsics_objects[i] = NULL;
    }
    Intrinsics_SetupOpcodeBuiltins();

    intrinsics_objects[1] =
        CParser_NewRTFunc(TYPE(&stvoid), GetHashNameNodeExport("__asm_start"), 0, 0);
    intrinsics_objects[1]->u.func.u.intrinsicid = 1;
    TYPE_FUNC(intrinsics_objects[1]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[1]);
    intrinsics_objects[2] =
        CParser_NewRTFunc(TYPE(&stvoid), GetHashNameNodeExport("__asm_start_reord"), 0, 0);
    intrinsics_objects[2]->u.func.u.intrinsicid = 2;
    TYPE_FUNC(intrinsics_objects[2]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[2]);
    intrinsics_objects[3] =
        CParser_NewRTFunc(TYPE(&stvoid), GetHashNameNodeExport("__asm_end"), 0, 0);
    intrinsics_objects[3]->u.func.u.intrinsicid = 3;
    TYPE_FUNC(intrinsics_objects[3]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[3]);
    intrinsics_objects[4] = CParser_NewRTFunc(
        TYPE(&stvoid),
        GetHashNameNodeExport("__evaluate"),
        0,
        2,
        TYPE(&stsignedlong),
        TYPE(&stsignedlong)
    );
    intrinsics_objects[4]->u.func.u.intrinsicid = 4;
    TYPE_FUNC(intrinsics_objects[4]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[4]);
    intrinsics_objects[5] = CParser_NewRTFunc(
        TYPE(&stvoid), GetHashNameNodeExport("__builtin_initialize"), 0, 1, TYPE(&void_ptr)
    );
    intrinsics_objects[5]->u.func.u.intrinsicid = 5;
    TYPE_FUNC(intrinsics_objects[5]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[5]);
    intrinsics_objects[6] = CParser_NewRTFunc(
        TYPE(&stsignedint), GetHashNameNodeExport("__builtin_args_info"), 0, 1, TYPE(&stsignedint)
    );
    intrinsics_objects[6]->u.func.u.intrinsicid = 6;
    TYPE_FUNC(intrinsics_objects[6]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[6]);
    intrinsics_objects[188] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__builtin_allegrex_bitrev"),
        0,
        1,
        TYPE(&stsignedlong)
    );
    intrinsics_objects[188]->u.func.u.intrinsicid = 188;
    TYPE_FUNC(intrinsics_objects[188]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[188]);
    intrinsics_objects[179] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__builtin_allegrex_clo"),
        0,
        1,
        TYPE(&stsignedlong)
    );
    intrinsics_objects[179]->u.func.u.intrinsicid = 179;
    TYPE_FUNC(intrinsics_objects[179]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[179]);
    intrinsics_objects[178] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__builtin_allegrex_clz"),
        0,
        1,
        TYPE(&stsignedlong)
    );
    intrinsics_objects[178]->u.func.u.intrinsicid = 178;
    TYPE_FUNC(intrinsics_objects[178]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[178]);
    intrinsics_objects[181] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__builtin_allegrex_cto"),
        0,
        1,
        TYPE(&stsignedlong)
    );
    intrinsics_objects[181]->u.func.u.intrinsicid = 181;
    TYPE_FUNC(intrinsics_objects[181]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[181]);
    intrinsics_objects[180] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__builtin_allegrex_ctz"),
        0,
        1,
        TYPE(&stsignedlong)
    );
    intrinsics_objects[180]->u.func.u.intrinsicid = 180;
    TYPE_FUNC(intrinsics_objects[180]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[180]);
    intrinsics_objects[191] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__builtin_allegrex_ext"),
        0,
        3,
        TYPE(&stsignedlong),
        TYPE(&stsignedlong),
        TYPE(&stsignedlong)
    );
    intrinsics_objects[191]->u.func.u.intrinsicid = 191;
    TYPE_FUNC(intrinsics_objects[191]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[191]);
    intrinsics_objects[192] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__builtin_allegrex_ins"),
        0,
        4,
        TYPE(&stsignedlong),
        TYPE(&stsignedlong),
        TYPE(&stsignedlong),
        TYPE(&stsignedlong)
    );
    intrinsics_objects[192]->u.func.u.intrinsicid = 192;
    TYPE_FUNC(intrinsics_objects[192]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[192]);
    intrinsics_objects[182] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__builtin_allegrex_max"),
        0,
        2,
        TYPE(&stsignedlong),
        TYPE(&stsignedlong)
    );
    intrinsics_objects[182]->u.func.u.intrinsicid = 182;
    TYPE_FUNC(intrinsics_objects[182]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[182]);
    intrinsics_objects[183] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__builtin_allegrex_min"),
        0,
        2,
        TYPE(&stsignedlong),
        TYPE(&stsignedlong)
    );
    intrinsics_objects[183]->u.func.u.intrinsicid = 183;
    TYPE_FUNC(intrinsics_objects[183]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[183]);
    intrinsics_objects[189] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__builtin_allegrex_rotr"),
        0,
        2,
        TYPE(&stsignedlong),
        TYPE(&stsignedlong)
    );
    intrinsics_objects[189]->u.func.u.intrinsicid = 189;
    TYPE_FUNC(intrinsics_objects[189]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[189]);
    intrinsics_objects[190] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__builtin_allegrex_rotl"),
        0,
        2,
        TYPE(&stsignedlong),
        TYPE(&stsignedlong)
    );
    intrinsics_objects[190]->u.func.u.intrinsicid = 190;
    TYPE_FUNC(intrinsics_objects[190]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[190]);
    intrinsics_objects[184] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__builtin_allegrex_seb"),
        0,
        1,
        TYPE(&stsignedlong)
    );
    intrinsics_objects[184]->u.func.u.intrinsicid = 184;
    TYPE_FUNC(intrinsics_objects[184]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[184]);
    intrinsics_objects[185] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__builtin_allegrex_seh"),
        0,
        1,
        TYPE(&stsignedlong)
    );
    intrinsics_objects[185]->u.func.u.intrinsicid = 185;
    TYPE_FUNC(intrinsics_objects[185]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[185]);
    intrinsics_objects[186] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__builtin_allegrex_wsbh"),
        0,
        1,
        TYPE(&stsignedlong)
    );
    intrinsics_objects[186]->u.func.u.intrinsicid = 186;
    TYPE_FUNC(intrinsics_objects[186]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[186]);
    intrinsics_objects[187] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__builtin_allegrex_wsbw"),
        0,
        1,
        TYPE(&stsignedlong)
    );
    intrinsics_objects[187]->u.func.u.intrinsicid = 187;
    TYPE_FUNC(intrinsics_objects[187]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[187]);
    intrinsics_objects[193] = CParser_NewRTFunc(
        TYPE(&stfloat), GetHashNameNodeExport("__builtin_allegrex_ceil_w_s"), 0, 1, TYPE(&stfloat)
    );
    intrinsics_objects[193]->u.func.u.intrinsicid = 193;
    TYPE_FUNC(intrinsics_objects[193]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[193]);
    intrinsics_objects[194] = CParser_NewRTFunc(
        TYPE(&stfloat), GetHashNameNodeExport("__builtin_allegrex_floor_w_s"), 0, 1, TYPE(&stfloat)
    );
    intrinsics_objects[194]->u.func.u.intrinsicid = 194;
    TYPE_FUNC(intrinsics_objects[194]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[194]);
    intrinsics_objects[195] = CParser_NewRTFunc(
        TYPE(&stfloat), GetHashNameNodeExport("__builtin_allegrex_round_w_s"), 0, 1, TYPE(&stfloat)
    );
    intrinsics_objects[195]->u.func.u.intrinsicid = 195;
    TYPE_FUNC(intrinsics_objects[195]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[195]);
    intrinsics_objects[196] = CParser_NewRTFunc(
        TYPE(&stfloat), GetHashNameNodeExport("__builtin_allegrex_trunc_w_s"), 0, 1, TYPE(&stfloat)
    );
    intrinsics_objects[196]->u.func.u.intrinsicid = 196;
    TYPE_FUNC(intrinsics_objects[196]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[196]);
    intrinsics_objects[197] = CParser_NewRTFunc(
        TYPE(&stfloat), GetHashNameNodeExport("__builtin_allegrex_sqrt_s"), 0, 1, TYPE(&stfloat)
    );
    intrinsics_objects[197]->u.func.u.intrinsicid = 197;
    TYPE_FUNC(intrinsics_objects[197]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[197]);
    intrinsics_objects[0] = CParser_NewRTFunc(
        TYPE(&void_ptr), GetHashNameNodeExport("__alloca"), 0, 1, TYPE(&stunsignedlong)
    );
    intrinsics_objects[0]->u.func.u.intrinsicid = 0;
    TYPE_FUNC(intrinsics_objects[0]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[0]);
    intrinsics_objects[10] = CParser_NewRTFunc(
        TYPE(&stsignedint), GetHashNameNodeExport("__abs"), 0, 1, TYPE(&stsignedint)
    );
    intrinsics_objects[10]->u.func.u.intrinsicid = 10;
    TYPE_FUNC(intrinsics_objects[10]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[10]);
    intrinsics_objects[11] = CParser_NewRTFunc(
        TYPE(&stsignedlong), GetHashNameNodeExport("__labs"), 0, 1, TYPE(&stsignedlong)
    );
    intrinsics_objects[11]->u.func.u.intrinsicid = 11;
    TYPE_FUNC(intrinsics_objects[11]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[11]);
    intrinsics_objects[12] =
        CParser_NewRTFunc(TYPE(&stfloat), GetHashNameNodeExport("__s_abs"), 0, 1, TYPE(&stfloat));
    intrinsics_objects[12]->u.func.u.intrinsicid = 12;
    TYPE_FUNC(intrinsics_objects[12]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[12]);
    intrinsics_objects[13] =
        CParser_NewRTFunc(TYPE(&stdouble), GetHashNameNodeExport("__d_abs"), 0, 1, TYPE(&stdouble));
    intrinsics_objects[13]->u.func.u.intrinsicid = 13;
    TYPE_FUNC(intrinsics_objects[13]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[13]);
    intrinsics_objects[14] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__mulhi"),
        0,
        2,
        TYPE(&stsignedlong),
        TYPE(&stsignedlong)
    );
    intrinsics_objects[14]->u.func.u.intrinsicid = 14;
    TYPE_FUNC(intrinsics_objects[14]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[14]);
    intrinsics_objects[15] = CParser_NewRTFunc(
        TYPE(&stsignedlong),
        GetHashNameNodeExport("__mulhiu"),
        0,
        2,
        TYPE(&stsignedlong),
        TYPE(&stsignedlong)
    );
    intrinsics_objects[15]->u.func.u.intrinsicid = 15;
    TYPE_FUNC(intrinsics_objects[15]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[15]);
    intrinsics_objects[8] = CParser_NewRTFunc(
        TYPE(&stchar), GetHashNameNodeExport("__strcpy"), 0, 2, TYPE(&stchar), TYPE(&stchar)
    );
    intrinsics_objects[8]->u.func.u.intrinsicid = 8;
    TYPE_FUNC(intrinsics_objects[8]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[8]);
    intrinsics_objects[16] = CParser_NewRTFunc(
        TYPE(&stfloat), GetHashNameNodeExport("__sqrt"), 0, 2, TYPE(&stfloat), TYPE(&stfloat)
    );
    intrinsics_objects[16]->u.func.u.intrinsicid = 16;
    TYPE_FUNC(intrinsics_objects[16]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[16]);
    intrinsics_objects[17] = CParser_NewRTFunc(
        TYPE(&stfloat), GetHashNameNodeExport("__rsqrt"), 0, 2, TYPE(&stfloat), TYPE(&stfloat)
    );
    intrinsics_objects[17]->u.func.u.intrinsicid = 17;
    TYPE_FUNC(intrinsics_objects[17]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[17]);
    intrinsics_objects[9] = CParser_NewRTFunc(
        CDecl_NewPointerType(TYPE(&stvoid)),
        GetHashNameNodeExport("__memcpy"),
        0,
        3,
        CDecl_NewPointerType(TYPE(&stvoid)),
        CDecl_NewPointerType(TYPE(&stvoid)),
        CABI_GetSizeTType()
    );
    intrinsics_objects[9]->u.func.u.intrinsicid = 9;
    TYPE_FUNC(intrinsics_objects[9]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[9]);
    intrinsics_objects[18] = CParser_NewRTFunc(
        TYPE(&void_ptr),
        GetHashNameNodeExport("__clear"),
        0,
        2,
        TYPE(&char_ptr),
        CABI_GetSizeTType()
    );
    intrinsics_objects[18]->u.func.u.intrinsicid = 18;
    TYPE_FUNC(intrinsics_objects[18]->type)->flags |= FUNC_INTRINSIC;
    CScope_AddGlobalObject(intrinsics_objects[18]);

    copts.cplusplus = savecpp;
}
#endif

// 0x458560 (Intrinsics_ReInitRuntimeObjects): the names again, after a
// precompiled header is loaded (stops one short of the last opcode, as the
// original does)
Boolean func_00458560(Boolean is_precompiler) {
    UInt16 op;
#if MWCCPS2
    char* name;

    if (is_precompiler) {
        for (op = 0; op < 0x3E5; op++) {
            name = data_006115F8[op].intrinsic;
            if (name != NULL) {
                intrinsics_objects[data_006115F8[op].intrinsicid]->name =
                    GetHashNameNodeExport(name);
            }
        }
    }
    return TRUE;
#else

    if (is_precompiler) {
        for (op = 0; op < 0x3E5; op++) {
            if (data_006115F8[op].intrinsic) {
                intrinsics_objects[data_006115F8[op].intrinsicid]->name =
                    GetHashNameNodeExport(data_006115F8[op].intrinsic);
            }
        }
    }
    return TRUE;
#endif
}

// --- the intrinsic call generator (0x4585B0) ----------------------------------

extern void (*data_005D0140[])(ENode* expr, void* target, VOperand* out); // expression generators
extern SInt32 data_00625A98[7]; // next virtual register, by class
extern UInt16 data_00625E48; // the move opcode
extern UInt16 data_00610C90[]; // the opcode of an opcode intrinsic
extern MPCodeBlock* data_006246B4;
extern SInt32 data_005E9AC8; // __evaluate bindings made
extern UInt8 data_005E9ACC; // inside __asm_start
extern VOperand data_0060D1E4[5]; // __evaluate bindings, 1..4 (the register for a negative number)
extern Object* rt_memcpy;
extern Object* rt_strcpy;
extern VOperand func_004C3BA0(SInt32 reg, UInt8 kind);
extern MOperand func_004C3D50(CInt64 value);
extern MPCode* func_00480420(int op, SInt16 nargs);
extern void appendpcode(MPCodeBlock* block, MPCode* pc);
extern void emitpcode(VarArgShort op, ...);
extern CLabel* newlabel(void);
extern void func_004A64C0(VOperand* op, Type* type, int flags); // the operand into a register
extern void func_004A47E0(VOperand* op, int unk, int argno); // an argument register
extern void func_004A4A20(Object* rtfunc); // call a runtime routine
extern int func_004A4580(VOperand* size); // alloca
extern void func_004A72B0(VOperand* op, ENode* expr);
extern void func_00479430(UInt16 op, VOperand* a, VOperand* b, CLabel* label);
extern void func_00479710(UInt16 op, VOperand* reg, VOperand* mem);
extern void func_00479540(UInt16 op, VOperand* src, CLabel* label);
extern VOperand func_0047F740(UInt16 op, VOperand* a, VOperand* b, char rclass);
extern void func_0046FA50(void);
extern void func_0046FA90(MPCodeLabel* label);
extern void func_0052DE30(VOperand* dst, VOperand* src, SInt32 size, SInt32 align);
extern SInt16 func_00464E20(Object* obj);
extern void func_004C5940(int code, ...); // MIPSError_Error
extern void func_00438750(VOperand* v); // __builtin_initialize
extern VOperand func_00438680(VOperand* v); // __builtin_args_info
extern VOperand func_004385E0(VOperand* v);
extern VOperand func_0049D710(int op, ENode* arg);
extern VOperand func_0049D7A0(int op1, int op2, ENode* a, ENode* b);

#define INTR_EVAL(e, v) data_005D0140[(e)->type]((e), NULL, &(v))
// $zero is the constant 0
#define INTR_ZERO_IS_CONST(v) \
    do { \
        if ((v).kind == 0 && (v).reg == 0) { \
            (v).kind = 3; \
            (v).imm = cint64_zero; \
        } \
    } while (0)
#define INTR_REG_OF(n) ((n) < 0 ? data_0060D1E4[-(n)].reg : (n))

// an opcode intrinsic: the operands follow the opcode's format string
static void Intrinsics_GenOpcode(SInt32 id, ENodeList* args, VOperand* out) {
    MOperand ops[10];
    OpcodeDef* def;
    char* fmt;
    ENode* arg;
    VOperand v;
    MPCode* pc;
    int nops;
    int mode;
    int i;
    UInt32 extraflags;
    UInt16 opcode;
    Boolean pair;
    SInt32 reg;

    nops = 0;
    extraflags = 0;
    opcode = data_00610C90[id];
    def = &data_006115F8[opcode];
    for (fmt = def->format; *fmt; fmt++) {
        arg = args ? args->node : NULL;
        if (*fmt == ';') {
            break;
        }
        mode = 1;
        if (*fmt == '=') {
            fmt++;
            mode = 2;
        } else if (*fmt == '+') {
            mode = 3;
            fmt++;
        }
        pair = FALSE;
        if (*fmt == '2') {
            fmt++;
            pair = TRUE;
            if (*fmt == '+') {
                fmt++;
            }
        }

        switch (*fmt) {
            case '<':
            case '>':
            case '@':
            case 'B':
            case 'C':
            case 'c':
                INTR_EVAL(arg, v);
                INTR_ZERO_IS_CONST(v);
                if (v.kind != 3) {
                    CError_Error(0xA7, def->intrinsic);
                }
                ops[nops].kind = 2;
                ops[nops].value = v.imm.lo;
                break;
            case 'X':
            case 'k':
            case 'i':
            case 'u':
            case 'j':
            case 'I':
                INTR_EVAL(arg, v);
                INTR_ZERO_IS_CONST(v);
                if (v.kind != 3) {
                    CError_Error(0xA7, def->intrinsic);
                }
                ops[nops] = func_004C3D50(v.imm);
                break;
            case 't':
            case 'd':
                INTR_EVAL(arg, v);
                INTR_ZERO_IS_CONST(v);
                if (v.kind != 3) {
                    CError_Error(0xA7, def->intrinsic);
                }
                ops[nops].kind = 0;
                ops[nops].rclass = 0;
                reg = v.imm.lo;
                ops[nops].reg = INTR_REG_OF(reg);
                if (*fmt == 't' && ops[nops].reg >= 32) {
                    CError_Error(0x2D);
                    ops[nops].reg = 0;
                }
                ops[nops].flags = mode;
                if (mode & 2) {
                    out->kind = 0;
                    out->reg = ops[nops].reg;
                }
                break;
            case 'D':
            case 'E':
            case 'N':
            case 'T':
            case 's':
            case 'S':
            case 'V':
            case 'W':
                INTR_EVAL(arg, v);
                INTR_ZERO_IS_CONST(v);
                if (v.kind != 3) {
                    CError_Error(0xA7, def->intrinsic);
                }
                ops[nops].kind = 0;
                if (*fmt == 's') {
                    ops[nops].rclass = 0;
                } else if (*fmt == 'S' || *fmt == 'V' || *fmt == 'W') {
                    ops[nops].rclass = 1;
                } else {
                    ops[nops].rclass = pair ? 5 : 1;
                }
                reg = v.imm.lo;
                ops[nops].reg = INTR_REG_OF(reg);
                ops[nops].flags = mode;
                break;
            case 'G':
                INTR_EVAL(arg, v);
                INTR_ZERO_IS_CONST(v);
                if (v.kind != 3) {
                    CError_Error(0xA7, def->intrinsic);
                }
                ops[nops].reg = v.imm.lo;
                ops[nops].kind = 0;
                ops[nops].rclass = 2;
                if (ops[nops].reg < 0 || ops[nops].reg > 31) {
                    CError_Error(0x37);
                }
                ops[nops].flags = mode;
                break;
            case 'o':
                INTR_EVAL(arg, v);
                INTR_ZERO_IS_CONST(v);
                if (v.kind != 3) {
                    CError_Error(0xA7, def->intrinsic);
                }
                extraflags |= 0x20;
                ops[nops].kind = 4;
                ops[nops].value = v.imm.lo;
                ops[nops].value2 = 0;
                break;
            default:
                continue; // no operand
        }
        args = args->next;
        nops++;
    }

    pc = func_00480420(opcode, nops);
    pc->flags |= extraflags + 0x100;
    for (i = 0; i < nops; i++) {
        pc->args[i] = ops[i];
    }
    appendpcode(data_006246B4, pc);
}

// a two-operand integer instruction r = op a, b
static void Intrinsics_GenBinary(int op, ENodeList* args, VOperand* out) {
    VOperand a;
    VOperand b;
    SInt32 r;

    INTR_EVAL(args->node, a);
    INTR_EVAL(args->next->node, b);
    if (a.kind != 0) {
        func_004A64C0(&a, TYPE(&stsignedlong), 1);
    }
    if (b.kind != 0) {
        func_004A64C0(&b, TYPE(&stsignedlong), 1);
    }
    r = data_00625A98[0]++;
    *out = func_004C3BA0(r, 0);
    emitpcode(op, r, a.reg, b.reg);
}

// a one-operand instruction r = op a, in register class rclass
static void Intrinsics_GenUnary(int op, ENodeList* args, VOperand* out, Type* type, int rclass) {
    VOperand a;
    SInt32 r;

    INTR_EVAL(args->node, a);
    if (a.kind != 0) {
        func_004A64C0(&a, type, 1);
    }
    r = data_00625A98[rclass]++;
    *out = func_004C3BA0(r, rclass);
    emitpcode(op, r, a.reg);
}

// 3.0.1: generate a call to an intrinsic. Opcode intrinsics (0xC6..) follow
// their opcode's operand format; the builtins below 0xC6 are generated here.
#if MWCCPS2
// b210's intrinsic code generator (0x459710, 42 KB): work in progress, does
// not match yet. Ids below 0x26 come from mwccpsp's version (0x13/0x14 and
// the inlined opcode-builtin path are still to do); the MMI and VU0 cases
// are generated by tools/mwccps2/tools/mwccpsp/gen_intrinsics_cg_ps2.py
// (tools/mwccps2/notes/intrinsics_codegen.md).
extern TypeIntegral ps2_int64_5E5EE8; // 0x5E5EE8: an 8-byte integral type (0x11), __sel's operand
extern void ps2_vu0op_463B20(
    ENode* call, SInt32 op, VOperand* out
); // 0x463B20, a VU0 op (not decompiled)

#undef CError_B145_SHIFT
#define CError_B145_SHIFT 4 // b145 asserts
void func_004585B0(ENode* call, SInt16 target, VOperand* out) {
    ENodeList* args;
    SInt32 id;
    VOperand v;
    VOperand mv2; // the MMI cases' operands
    VOperand mv;
    MOperand ops[10]; // the opcode intrinsic's operands
    VOperand v2;
    int nops;
    int mode;
    char* fmt;
    VOperand v3;
    VOperand zero;
    CLabel* label;
    CLabel* label2;
    SInt32 r;
    SInt32 t;
    SInt32 u;
    SInt32 n;
    SInt32 k;
    SInt32 align;
    CInt64 limit;
    int op;
    ENode* arg;
    MPCode* pc;
    int i;
    UInt32 extraflags;
    int opcode;
    Boolean pair;
    SInt32 reg;
    SInt32 r2_;
    SInt16 sop;

    args = call->data.funccall.args;
    id = call->data.funccall.funcref->data.objref->u.func.u.intrinsicid;
    nops = 0;
    extraflags = 0;
    *out = func_004C3BA0(0, 0);

    if (id >= 0xC6 && id <= 0x4AB) {
        // an opcode intrinsic (inline on b210, sharing v)
        opcode = data_00610C90[id];
        for (fmt = data_006115F8[opcode].format; *fmt; fmt++) {
            arg = args ? args->node : NULL;
            if (*fmt == ';') {
                break;
            }
            pair = FALSE;
            mode = 1;
            if (*fmt == '=') {
                fmt++;
                mode = 2;
            } else if (*fmt == '+') {
                mode = 3;
                fmt++;
            }
            if (*fmt == '2') {
                fmt++;
                pair = TRUE;
                if (*fmt == '+') {
                    fmt++;
                }
            }

            switch (*fmt) {
                case '<':
                case '>':
                case '@':
                    INTR_EVAL(arg, v);
                    INTR_ZERO_IS_CONST(v);
                    if (v.kind != 3) {
                        CError_Error(0xA7, data_006115F8[opcode].intrinsic);
                    }
                    ops[nops].kind = 2;
                    ops[nops].value = v.imm.lo;
                    break;
                case 'X':
                case 'k':
                    INTR_EVAL(arg, v);
                    INTR_ZERO_IS_CONST(v);
                    if (v.kind != 3) {
                        CError_Error(0xA7, data_006115F8[opcode].intrinsic);
                    }
                    ops[nops] = func_004C3D50(v.imm);
                    break;
                case 'B':
                case 'C':
                case 'c':
                    INTR_EVAL(arg, v);
                    INTR_ZERO_IS_CONST(v);
                    if (v.kind != 3) {
                        CError_Error(0xA7, data_006115F8[opcode].intrinsic);
                    }
                    ops[nops].kind = 2;
                    ops[nops].value = v.imm.lo;
                    break;
                case 't':
                    INTR_EVAL(arg, v);
                    INTR_ZERO_IS_CONST(v);
                    if (v.kind != 3) {
                        CError_Error(0xA7, data_006115F8[opcode].intrinsic);
                    }
                    ops[nops].kind = 0;
                    ops[nops].rclass = 0;
                    reg = v.imm.lo;
                    ops[nops].reg = INTR_REG_OF(reg);
                    if (ops[nops].reg >= 32) {
                        CError_Error(0x2D);
                        ops[nops].reg = 0;
                    }
                    ops[nops].flags = mode;
                    if (mode & 2) {
                        out->kind = 0;
                        out->reg = ops[nops].reg;
                    }
                    break;
                case 'd':
                    INTR_EVAL(arg, v);
                    INTR_ZERO_IS_CONST(v);
                    if (v.kind != 3) {
                        CError_Error(0xA7, data_006115F8[opcode].intrinsic);
                    }
                    ops[nops].kind = 0;
                    ops[nops].rclass = 0;
                    reg = v.imm.lo;
                    ops[nops].reg = INTR_REG_OF(reg);
                    ops[nops].flags = mode;
                    if (mode & 2) {
                        out->kind = 0;
                        out->reg = ops[nops].reg;
                    }
                    break;
                case 'D':
                case 'E':
                case 'N':
                case 'T':
                    INTR_EVAL(arg, v);
                    INTR_ZERO_IS_CONST(v);
                    if (v.kind != 3) {
                        CError_Error(0xA7, data_006115F8[opcode].intrinsic);
                    }
                    ops[nops].kind = 0;
                    ops[nops].rclass = pair ? 5 : 1;
                    reg = v.imm.lo;
                    ops[nops].reg = INTR_REG_OF(reg);
                    ops[nops].flags = mode;
                    break;
                case 's':
                    INTR_EVAL(arg, v);
                    INTR_ZERO_IS_CONST(v);
                    if (v.kind != 3) {
                        CError_Error(0xA7, data_006115F8[opcode].intrinsic);
                    }
                    ops[nops].kind = 0;
                    ops[nops].rclass = 0;
                    reg = v.imm.lo;
                    ops[nops].reg = INTR_REG_OF(reg);
                    ops[nops].flags = mode;
                    break;
                case 'S':
                    INTR_EVAL(arg, v);
                    INTR_ZERO_IS_CONST(v);
                    if (v.kind != 3) {
                        CError_Error(0xA7, data_006115F8[opcode].intrinsic);
                    }
                    ops[nops].kind = 0;
                    ops[nops].rclass = 1;
                    reg = v.imm.lo;
                    ops[nops].reg = INTR_REG_OF(reg);
                    ops[nops].flags = mode;
                    break;
                case 'V':
                case 'W':
                    INTR_EVAL(arg, v);
                    INTR_ZERO_IS_CONST(v);
                    if (v.kind != 3) {
                        CError_Error(0xA7, data_006115F8[opcode].intrinsic);
                    }
                    ops[nops].kind = 0;
                    ops[nops].rclass = 1;
                    reg = v.imm.lo;
                    ops[nops].reg = INTR_REG_OF(reg);
                    ops[nops].flags = mode;
                    break;
                case 'G':
                    INTR_EVAL(arg, v);
                    INTR_ZERO_IS_CONST(v);
                    if (v.kind != 3) {
                        CError_Error(0xA7, data_006115F8[opcode].intrinsic);
                    }
                    ops[nops].reg = v.imm.lo;
                    ops[nops].kind = 0;
                    ops[nops].rclass = 2;
                    if (ops[nops].reg < 0 || ops[nops].reg > 31) {
                        CError_Error(0x37);
                    }
                    ops[nops].flags = mode;
                    break;
                case 'i':
                case 'u':
                    INTR_EVAL(arg, v);
                    INTR_ZERO_IS_CONST(v);
                    if (v.kind != 3) {
                        CError_Error(0xA7, data_006115F8[opcode].intrinsic);
                    }
                    ops[nops] = func_004C3D50(v.imm);
                    break;
                case 'j':
                    INTR_EVAL(arg, v);
                    INTR_ZERO_IS_CONST(v);
                    if (v.kind != 3) {
                        CError_Error(0xA7, data_006115F8[opcode].intrinsic);
                    }
                    ops[nops] = func_004C3D50(v.imm);
                    break;
                case 'o':
                    INTR_EVAL(arg, v);
                    INTR_ZERO_IS_CONST(v);
                    if (v.kind != 3) {
                        CError_Error(0xA7, data_006115F8[opcode].intrinsic);
                    }
                    extraflags |= 0x20;
                    ops[nops].kind = 4;
                    ops[nops].value = v.imm.lo;
                    ops[nops].value2 = 0;
                    break;
                case 'I':
                    INTR_EVAL(arg, v);
                    INTR_ZERO_IS_CONST(v);
                    if (v.kind != 3) {
                        CError_Error(0xA7, data_006115F8[opcode].intrinsic);
                    }
                    ops[nops] = func_004C3D50(v.imm);
                    break;
                default:
                    continue; // no operand
            }
            args = args->next;
            nops++;
        }

        pc = func_00480420(opcode, nops);
        pc->flags |= extraflags + 0x100;
        for (i = 0; i < nops; i++) {
            pc->args[i] = ops[i];
        }
        appendpcode(data_006246B4, pc);
        return;
    }

    // b210: the MMI (0x26..0x75) and VU0 (0x85..0xB1) intrinsics need
    // #pragma (copts+0x47) on
    if (!copts.mips_option43) {
        if (call->data.funccall.funcref->data.objref->u.func.u.intrinsicid >= 0x26 &&
            call->data.funccall.funcref->data.objref->u.func.u.intrinsicid <= 0x75) {
            func_004C5940(0x1C);
        }
        if (call->data.funccall.funcref->data.objref->u.func.u.intrinsicid >= 0x85 &&
            call->data.funccall.funcref->data.objref->u.func.u.intrinsicid <= 0xB1) {
            func_004C5940(0x1C);
        }
    }

    switch (call->data.funccall.funcref->data.objref->u.func.u.intrinsicid) {
        case 1: // __asm_start: the __evaluate bindings into $a0..
            for (k = 1, r = 4; k <= data_005E9AC8; k++, r++) {
                emitpcode(data_00625E48, r, data_0060D1E4[k].reg);
                data_0060D1E4[k].reg = r;
            }
            data_005E9AC8 = 0;
            func_0046FA50();
            data_006246B4->flags2E |= 0x4008;
            data_005E9ACC = 1;
            break;
        case 2: // __asm_start_reord
            data_005E9ACC = 0;
            data_005E9AC8 = 0;
            break;
        case 3: // __asm_end
            data_005E9AC8 = 0;
            if (data_005E9ACC) {
                func_0046FA50();
            }
            break;
        case 5: // __builtin_initialize
            INTR_EVAL(args->node, v);
            func_00438750(&v);
            break;
        case 6: // __builtin_args_info
            INTR_EVAL(args->node, v);
            *out = func_00438680(&v);
            break;
        case 7:
            INTR_EVAL(args->node, v);
            *out = func_004385E0(&v);
            break;
        case 4: // __evaluate(-n, value): bind a value to $n for asm
            INTR_EVAL(args->node, v);
            INTR_EVAL(args->next->node, v2);
            if (v.kind != 3) {
                CError_Error(0xA7, "__evaluate");
            }
            n = -v.imm.lo;
            if (n <= 0 || n > 4) {
                CError_Error(0xA7, "__evaluate");
            }
            if (++data_005E9AC8 > 4) {
                CError_Error(0xA7, "__evaluate");
            }
            if (v2.kind != 0) {
                func_004A64C0(&v2, args->node->rtype, 0);
            }
            data_0060D1E4[n] = v2;
            break;
        case 0xA: // __abs
        case 0xB: // __labs
            INTR_EVAL(args->node, v);
            if (v.kind == 3) {
                if (v.imm.hi & 0x80000000) {
                    v.imm = CInt64_Neg(v.imm);
                }
                *out = v;
                break;
            }
            if (v.kind != 0) {
                func_004A64C0(
                    &v,
                    call->data.funccall.funcref->data.objref->u.func.u.intrinsicid == 0xA
                        ? TYPE(&stsignedint)
                        : TYPE(&stsignedlong),
                    1
                );
            }
            out->kind = 0;
            if (copts.conditional_move) {
                t = data_00625A98[0]++;
                emitpcode(0xD4, t, v.reg, 0);
                out->reg = data_00625A98[0]++;
                emitpcode(0xB8, out->reg, v.reg);
                emitpcode(0x111, out->reg, v.reg, t);
            } else {
                label = newlabel();
                label->pclabel = (struct PCodeLabel*)makepclabel();
                out->reg = data_00625A98[0]++;
                emitpcode(data_00625E48, out->reg, v.reg);
                func_00479540(0x1C, &v, label);
                emitpcode(0xB8, out->reg, v.reg);
                func_0046FA90((MPCodeLabel*)label->pclabel);
            }
            break;
        case 0xC: // __s_abs
            INTR_EVAL(args->node, v);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&stfloat), 1);
            }
            if (copts.fp_library) {
                out->reg = data_00625A98[1]++;
                emitpcode(0xEB, out->reg, v.reg);
                out->kind = 1;
            } else {
                sop = data_00625A98[0]++;
                emitpcode(0xD2, sop, v.reg, 1);
                out->reg = data_00625A98[0]++;
                emitpcode(0xDA, out->reg, sop, 1);
                out->kind = 0;
            }
            break;
        case 0xD: // __d_abs
            INTR_EVAL(args->node, v);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&stdouble), 1);
            }
            if (copts.fp_library & 2) {
                out->reg = data_00625A98[1]++;
                emitpcode(0xEA, out->reg, v.reg);
                out->kind = 1;
            } else {
                // b210: the double is one 64-bit GPR; clear its sign bit
                sop = data_00625A98[0]++;
                emitpcode(0x84, sop, v.reg, 1);
                out->reg = data_00625A98[0]++;
                emitpcode(0x8A, out->reg, sop, 1);
                out->kind = 0;
            }
            break;
        case 0x13: // __sel(x, n): element n (0..7) of a 64-bit vector
            INTR_EVAL(args->node, v);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&ps2_int64_5E5EE8), 0);
            }
            INTR_EVAL(args->next->node, v2);
            if (v2.kind != 3) {
                CError_Error(0xA7, "__sel");
            }
            n = v2.imm.lo;
            if (n < 0 || n > 7) {
                CError_Error(0xA7, "__sel");
            }
            v.sel = n + 1;
            *out = v;
            break;
        case 0x14: // __fixed__8x8_vector(x)
            INTR_EVAL(args->node, v);
            if (v.kind != 3) {
                CError_Error(0xA7, "__fixed__8x8_vector");
            }
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&ps2_int64_5E5EE8), 0);
            }
            v.sel = 0;
            *out = v;
            break;
        case 0x19:
            *out = func_0049D710(0x1B9, args->node);
            break;
        case 0x1A:
            *out = func_0049D710(0x1BA, args->node);
            break;
        case 0x1B:
            *out = func_0049D710(0x1BB, args->node);
            break;
        case 0x15:
            *out = func_0049D7A0(0x1A7, 0x1A8, args->node, args->next->node);
            break;
        case 0x16:
            *out = func_0049D7A0(0x1A9, 0x1AA, args->node, args->next->node);
            break;
        case 0x17:
            *out = func_0049D7A0(0x1B5, 0x1B6, args->node, args->next->node);
            break;
        case 0x18:
            *out = func_0049D7A0(0x1B7, 0x1B8, args->node, args->next->node);
            break;
        case 0x23:
        case 0x24:
        case 0x25: // b210: an 8x8 vector op on __sel'ed elements
            INTR_EVAL(args->node, v);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&ps2_int64_5E5EE8), 0);
            }
            INTR_EVAL(args->next->node, v2);
            if (v2.kind != 0) {
                func_004A64C0(&v2, TYPE(&ps2_int64_5E5EE8), 0);
            }
            n = v2.sel ? v2.sel - 1 : 11;
            switch (call->data.funccall.funcref->data.objref->u.func.u.intrinsicid) {
                case 0x23:
                    sop = 0x1A1;
                    break;
                case 0x24:
                    sop = 0x1A3;
                    break;
                case 0x25:
                    sop = 0x1A5;
                    break;
                default:
                    CError_FATAL(1162);
                    sop = 0;
            }
            emitpcode(sop, v.reg, v2.reg, n);
            break;
        case 0xE: // __mulhi
        case 0xF: // __mulhiu
            INTR_EVAL(args->node, v);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&stsignedlong), 1);
            }
            INTR_EVAL(args->next->node, v2);
            if (v2.kind != 0) {
                func_004A64C0(&v2, TYPE(&stsignedlong), 1);
            }
            op =
                call->data.funccall.funcref->data.objref->u.func.u.intrinsicid == 0xE ? 0xB6 : 0xB7;
            emitpcode(op, v.reg, v2.reg);
            out->reg = data_00625A98[0]++;
            emitpcode(0xA8, out->reg);
            out->kind = 0;
            break;
        case 0: // __alloca
            INTR_EVAL(args->node, v);
            out->kind = 0;
            out->offset = 0;
            out->object = 0;
            out->reg = func_004A4580(&v);
            break;
        case 9: // __memcpy
            INTR_EVAL(args->node, v);
            INTR_EVAL(args->next->node, v2);
            INTR_EVAL(args->next->next->node, v3);
            if (v3.kind == 3) {
                align = func_00464E20((Object*)v.object);
                if (func_00464E20((Object*)v2.object) < align) {
                    align = func_00464E20((Object*)v2.object);
                }
                func_0052DE30(&v, &v2, v3.imm.lo, align);
            } else {
                if (v.kind != 0) {
                    func_004A64C0(&v, TYPE(&void_ptr), 0);
                }
                if (v2.kind != 0) {
                    func_004A64C0(&v2, TYPE(&void_ptr), 0);
                }
                if (v3.kind != 0) {
                    func_004A64C0(&v3, TYPE(&stunsignedint), 0);
                }
                func_004A47E0(&v, 0, 1);
                func_004A47E0(&v2, 0, 2);
                func_004A47E0(&v3, 0, 2);
                func_004A4A20(rt_memcpy);
            }
            *out = v;
            break;
        case 8: // __strcpy (with a third argument: a copy of known size)
            INTR_EVAL(args->node, v);
            INTR_EVAL(args->next->node, v2);
            if (args->next->next) {
                INTR_EVAL(args->next->next->node, v3);
                align = func_00464E20((Object*)v.object);
                if (func_00464E20((Object*)v2.object) < align) {
                    align = func_00464E20((Object*)v2.object);
                }
                func_0052DE30(&v, &v2, v3.imm.lo, align);
            } else {
                if (v.kind != 0) {
                    func_004A64C0(&v, TYPE(&void_ptr), 0);
                }
                if (v2.kind != 0) {
                    func_004A64C0(&v2, TYPE(&void_ptr), 0);
                }
                func_004A47E0(&v, 0, 1);
                func_004A47E0(&v2, 0, 2);
                func_004A4A20(rt_strcpy);
            }
            *out = v;
            break;
        case 0x12: // __clear(ptr, size): store zero bytes
            INTR_EVAL(args->node, v);
            INTR_EVAL(args->next->node, v2);
            if (CInt64_IsZero(&v2.imm)) {
                break;
            }
            zero = func_004C3BA0(0, 0);
            if (CInt64_IsOne(&v2.imm)) {
                func_00479710(0xC3, &zero, &v);
                *out = v;
                break;
            }
            label = newlabel();
            label2 = newlabel();
            label->pclabel = (struct PCodeLabel*)makepclabel();
            label2->pclabel = (struct PCodeLabel*)makepclabel();
            if (v.kind != 0) {
                func_004A64C0(&v, args->node->rtype, 0);
            }
            if (v2.kind != 0) {
                func_004A64C0(&v2, args->node->rtype, 0); // (sic) the pointer's type
            }
            func_00479430(0x18, &v, &zero, label2);
            *out = func_0047F740(4, &v, &zero, 0);
            func_004A72B0(&v, args->node);
            func_0046FA90((MPCodeLabel*)label->pclabel);
            func_00479710(0xC3, &zero, &v);
            emitpcode(3, v.reg, v.reg, 1);
            emitpcode(3, v2.reg, v2.reg, -1);
            func_00479430(0x28, &v2, &zero, label);
            func_0046FA90((MPCodeLabel*)label2->pclabel);
            break;
        case 0x10: // __sqrt
            INTR_EVAL(args->node, v);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&stfloat), 1);
            }
            if (!copts.fp_library) {
                CError_FATAL(2681);
                break;
            }
            out->reg = data_00625A98[1]++;
            emitpcode(0x101, out->reg, v.reg);
            out->kind = 1;
            break;
        case 0x11: // __rsqrt
            INTR_EVAL(args->node, v);
            INTR_EVAL(args->next->node, v2);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&stfloat), 1);
            }
            if (v2.kind != 0) {
                func_004A64C0(&v2, TYPE(&stfloat), 1);
            }
            if (!copts.fp_library) {
                CError_FATAL(2695);
                break;
            }
            out->reg = data_00625A98[1]++;
            emitpcode(0x125, out->reg, v.reg, v2.reg);
            out->kind = 1;
            break;
        case 0x26:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x206, r, mv.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x27:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x207, r, mv.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x28:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x208, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x29:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x209, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x2A:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x20b, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x2B:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x20c, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x2C:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x20d, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x2D:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x20e, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x2E:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x20f, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x2F:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x210, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x30:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x20a, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x31:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x211, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x32:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x212, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x33:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x213, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x34:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x214, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x35:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x215, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x36:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x216, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x37:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x217, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x38:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x218, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x39:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x219, r, mv.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x3A:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x21a, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x3B:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x21b, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x3C:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            emitpcode(0x21c, mv.reg, mv2.reg);
            break;
        case 0x3D:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            emitpcode(0x21d, mv.reg, mv2.reg);
            break;
        case 0x3E:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            emitpcode(0x21e, mv.reg, mv2.reg);
            break;
        case 0x3F:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x22c, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x40:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x22d, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x41:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x231, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x42:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x232, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x43:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x233, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x44:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x23f, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x45:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x240, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x46:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x244, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x47:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x245, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x48:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x246, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x49:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x21f, r, mv.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x4A:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x220, r, mv.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x4B:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x221, r, mv.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x4C:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x223, r, mv.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x4D:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x225, r, mv.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x4E:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x226, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x4F:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x227, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x50:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x228, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x51:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x229, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x52:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x22a, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x53:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x22b, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x54:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x22e, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x55:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x22f, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x56:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x230, r, mv.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x57:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x234, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x58:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x235, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x59:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x23d, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x5A:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x23e, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x5B:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x247, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x5C:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x248, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x5D:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x249, r, mv.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x5E:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x24a, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x5F:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x24b, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x60:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x24c, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x61:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x24d, r, mv.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x62:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x24e, r, mv.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x63:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x24f, r, mv.reg, args->next->node->data.intval.lo);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x64:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x250, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x65:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x251, r, mv.reg, args->next->node->data.intval.lo);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x66:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x252, r, mv.reg, args->next->node->data.intval.lo);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x67:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x253, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x68:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x254, r, mv.reg, args->next->node->data.intval.lo);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x69:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x255, r, mv.reg, args->next->node->data.intval.lo);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x6A:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x256, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x6B:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x257, r, mv.reg, args->next->node->data.intval.lo);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x6C:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x258, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x6D:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x259, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x6E:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x25a, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x6F:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x25b, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x70:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x25c, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x71:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x25d, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x72:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x25e, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x73:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x25f, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x74:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x260, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x75:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x261, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x76:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            emitpcode(0x200, mv.reg, args->next->node->data.intval.lo);
            break;
        case 0x77:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            emitpcode(0x201, mv.reg, args->next->node->data.intval.lo);
            break;
        case 0x78:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            emitpcode(0x1ff, mv.reg);
            break;
        case 0x79:
            r = data_00625A98[0]++;
            emitpcode(0x1fc, r);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x7A:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            INTR_EVAL(args->next->node, mv2);
            if (mv2.kind != 0) {
                func_004A64C0(&mv2, TYPE(&stint128), 0);
            }
            r = data_00625A98[0]++;
            emitpcode(0x262, r, mv.reg, mv2.reg);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x7B:
            r = data_00625A98[0]++;
            emitpcode(0x236, r);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x7C:
            r = data_00625A98[0]++;
            emitpcode(0x237, r);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x7D:
            r = data_00625A98[0]++;
            emitpcode(0x238, r);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x7E:
            r = data_00625A98[0]++;
            emitpcode(0x239, r);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x7F:
            r = data_00625A98[0]++;
            emitpcode(0x23a, r);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x80:
            r = data_00625A98[0]++;
            emitpcode(0x23b, r);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x81:
            r = data_00625A98[0]++;
            emitpcode(0x23c, r);
            *out = func_004C3BA0(r, 0);
            break;
        case 0x82:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            emitpcode(0x241, mv.reg);
            break;
        case 0x83:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            emitpcode(0x242, mv.reg);
            break;
        case 0x84:
            INTR_EVAL(args->node, mv);
            if (mv.kind != 0) {
                func_004A64C0(&mv, TYPE(&stint128), 0);
            }
            emitpcode(0x243, mv.reg);
            break;
        case 0x85:
            ps2_vu0op_463B20(call, 0x269, out);
            break;
        case 0x86:
            ps2_vu0op_463B20(call, 0x2af, out);
            break;
        case 0x87:
            ps2_vu0op_463B20(call, 0x2b0, out);
            break;
        case 0x88:
            ps2_vu0op_463B20(call, 0x26a, out);
            break;
        case 0x89:
            ps2_vu0op_463B20(call, 0x26c, out);
            break;
        case 0x8A:
            ps2_vu0op_463B20(call, 0x26e, out);
            break;
        case 0x8B:
            ps2_vu0op_463B20(call, 0x294, out);
            break;
        case 0x8C:
            ps2_vu0op_463B20(call, 0x296, out);
            break;
        case 0x8D:
            ps2_vu0op_463B20(call, 0x298, out);
            break;
        case 0x8E:
            ps2_vu0op_463B20(call, 0x2a2, out);
            break;
        case 0x8F:
            ps2_vu0op_463B20(call, 0x2a3, out);
            break;
        case 0x90:
            ps2_vu0op_463B20(call, 0x2a5, out);
            break;
        case 0x91:
            ps2_vu0op_463B20(call, 0x2a9, out);
            break;
        case 0x92:
            ps2_vu0op_463B20(call, 0x2aa, out);
            break;
        case 0x93:
            ps2_vu0op_463B20(call, 0x2ac, out);
            break;
        case 0x94:
            ps2_vu0op_463B20(call, 0x2b1, out);
            break;
        case 0x95:
            ps2_vu0op_463B20(call, 0x2b3, out);
            break;
        case 0x96:
            ps2_vu0op_463B20(call, 0x2b5, out);
            break;
        case 0x97:
            ps2_vu0op_463B20(call, 0x2c0, out);
            break;
        case 0x98:
            ps2_vu0op_463B20(call, 0x2c2, out);
            break;
        case 0x99:
            ps2_vu0op_463B20(call, 0x2c4, out);
            break;
        case 0x9A:
            ps2_vu0op_463B20(call, 0x2d9, out);
            break;
        case 0x9B:
            ps2_vu0op_463B20(call, 0x2db, out);
            break;
        case 0x9C:
            ps2_vu0op_463B20(call, 0x2dd, out);
            break;
        case 0x9D:
            ps2_vu0op_463B20(call, 0x2cf, out);
            break;
        case 0x9E:
            ps2_vu0op_463B20(call, 0x2d0, out);
            break;
        case 0x9F:
            ps2_vu0op_463B20(call, 0x27c, out);
            break;
        case 0xA0:
            ps2_vu0op_463B20(call, 0x2d8, out);
            break;
        case 0xA1:
            ps2_vu0op_463B20(call, 0x2d4, out);
            break;
        case 0xA2:
            ps2_vu0op_463B20(call, 0x2e7, out);
            break;
        case 0xA3:
            ps2_vu0op_463B20(call, 0x2d1, out);
            break;
        case 0xA4:
            ps2_vu0op_463B20(call, 0x2d2, out);
            break;
        case 0xA5:
            ps2_vu0op_463B20(call, 0x2d3, out);
            break;
        case 0xA6:
            ps2_vu0op_463B20(call, 0x2d5, out);
            break;
        case 0xA7:
            ps2_vu0op_463B20(call, 0x27b, out);
            break;
        case 0xA8:
            ps2_vu0op_463B20(call, 0x27d, out);
            break;
        case 0xA9:
            ps2_vu0op_463B20(call, 0x27e, out);
            break;
        case 0xAA:
            ps2_vu0op_463B20(call, 0x27f, out);
            break;
        case 0xAB:
            ps2_vu0op_463B20(call, 0x280, out);
            break;
        case 0xAC:
            ps2_vu0op_463B20(call, 0x28e, out);
            break;
        case 0xAD:
            ps2_vu0op_463B20(call, 0x28f, out);
            break;
        case 0xAE:
            ps2_vu0op_463B20(call, 0x290, out);
            break;
        case 0xAF:
            ps2_vu0op_463B20(call, 0x291, out);
            break;
        case 0xB0:
            emitpcode(0x2ce);
            break;
        case 0xB1:
            // a VU0 register's field count n (1..15) read into an FPR
            INTR_EVAL(args->node, v3);
            if (v3.kind != 0) {
                func_004A64C0(&v3, TYPE(&stint128), 0);
            }
            if (v3.kind != 5 && v3.kind != 6) {
                t = data_00625A98[5]++;
                emitpcode(0x266, v3.reg, t);
            } else {
                t = v3.reg;
            }
            n = (UInt8)args->next->node->data.intval.lo;
            u = data_00625A98[5]++;
            emitpcode(0x2AF, n, u, t);
            k = data_00625A98[0]++;
            emitpcode(0x264, k, u);
            switch (n) {
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                    r = k;
                    break;
                case 4:
                case 5:
                case 6:
                case 7:
                    r2_ = data_00625A98[0]++;
                    emitpcode(0x220, r2_, k);
                    r = data_00625A98[0]++;
                    emitpcode(0x223, r, r2_);
                    break;
                case 2:
                case 3:
                    r = data_00625A98[0]++;
                    emitpcode(0x223, r, k);
                    break;
                case 1:
                    r = data_00625A98[0]++;
                    emitpcode(0x21B, r, k, k);
                    r2_ = data_00625A98[0]++;
                    emitpcode(0x220, r2_, r);
                    r = data_00625A98[0]++;
                    emitpcode(0x223, r, r2_);
                    break;
                default:
                    CError_FATAL(3900);
            }
            t = data_00625A98[1]++;
            emitpcode(0xAD, r, t);
            *out = func_004C3BA0(t, 1);
            break;
        default:
            CError_FATAL(4124);
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
#else
void func_004585B0(ENode* call, SInt16 target, VOperand* out) {
    ENodeList* args;
    SInt32 id;
    VOperand v;
    VOperand v2;
    VOperand v3;
    VOperand zero;
    CLabel* label;
    CLabel* label2;
    SInt32 r;
    SInt32 t;
    SInt32 u;
    SInt32 n;
    SInt32 k;
    SInt32 align;
    CInt64 limit;
    int op;

    args = call->data.funccall.args;
    id = call->data.funccall.funcref->data.objref->u.func.u.intrinsicid;
    *out = func_004C3BA0(0, 0);

    if (id >= 0xC6 && id <= 0x4AB) {
        Intrinsics_GenOpcode(id, args, out);
        return;
    }

    switch (call->data.funccall.funcref->data.objref->u.func.u.intrinsicid) {
        case 1: // __asm_start: the __evaluate bindings into $a0..
            for (k = 1, r = 4; k <= data_005E9AC8; k++, r++) {
                emitpcode(data_00625E48, r, data_0060D1E4[k].reg);
                data_0060D1E4[k].reg = r;
            }
            data_005E9AC8 = 0;
            func_0046FA50();
            data_006246B4->flags2E |= 0x4008;
            data_005E9ACC = 1;
            break;
        case 2: // __asm_start_reord
            data_005E9ACC = 0;
            data_005E9AC8 = 0;
            break;
        case 3: // __asm_end
            data_005E9AC8 = 0;
            if (data_005E9ACC) {
                func_0046FA50();
            }
            break;
        case 5: // __builtin_initialize
            INTR_EVAL(args->node, v);
            func_00438750(&v);
            break;
        case 6: // __builtin_args_info
            INTR_EVAL(args->node, v);
            *out = func_00438680(&v);
            break;
        case 7:
            INTR_EVAL(args->node, v);
            *out = func_004385E0(&v);
            break;
        case 4: // __evaluate(-n, value): bind a value to $n for asm
            INTR_EVAL(args->node, v);
            INTR_EVAL(args->next->node, v2);
            if (v.kind != 3) {
                CError_Error(0xA7, "__evaluate");
            }
            n = -v.imm.lo;
            if (n <= 0 || n > 4) {
                CError_Error(0xA7, "__evaluate");
            }
            if (++data_005E9AC8 > 4) {
                CError_Error(0xA7, "__evaluate");
            }
            if (v2.kind != 0) {
                func_004A64C0(&v2, args->node->rtype, 0);
            }
            data_0060D1E4[n] = v2;
            break;
        case 0xA: // __abs
        case 0xB: // __labs
            INTR_EVAL(args->node, v);
            if (v.kind == 3) {
                if (v.imm.hi & 0x80000000) {
                    v.imm = CInt64_Neg(v.imm);
                }
                *out = v;
                break;
            }
            if (v.kind != 0) {
                func_004A64C0(
                    &v,
                    call->data.funccall.funcref->data.objref->u.func.u.intrinsicid == 0xA
                        ? TYPE(&stsignedint)
                        : TYPE(&stsignedlong),
                    1
                );
            }
            out->kind = 0;
            if (copts.conditional_move) {
                t = data_00625A98[0]++;
                emitpcode(0xD4, t, v.reg, 0);
                out->reg = data_00625A98[0]++;
                emitpcode(0xB8, out->reg, v.reg);
                emitpcode(0x111, out->reg, v.reg, t);
            } else {
                label = newlabel();
                label->pclabel = (struct PCodeLabel*)makepclabel();
                out->reg = data_00625A98[0]++;
                emitpcode(data_00625E48, out->reg, v.reg);
                func_00479540(0x1C, &v, label);
                emitpcode(0xB8, out->reg, v.reg);
                func_0046FA90((MPCodeLabel*)label->pclabel);
            }
            break;
        case 0xC: // __s_abs
            INTR_EVAL(args->node, v);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&stfloat), 1);
            }
            if (copts.fp_library) {
                out->reg = data_00625A98[1]++;
                emitpcode(0xEB, out->reg, v.reg);
                out->kind = 1;
            } else {
                t = data_00625A98[0]++;
                emitpcode(0xD2, t, v.reg, 1);
                out->reg = data_00625A98[0]++;
                emitpcode(0xDA, out->reg, t, 1);
                out->kind = 0;
            }
            break;
        case 0xD: // __d_abs
            INTR_EVAL(args->node, v);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&stdouble), 1);
            }
            if (copts.fp_library & 2) {
                out->reg = data_00625A98[1]++;
                emitpcode(0xEA, out->reg, v.reg);
                out->kind = 1;
            } else {
                out->reg = v.reg;
                t = data_00625A98[0]++;
                emitpcode(0xD2, t, v.regHi, 1);
                out->regHi = data_00625A98[0]++;
                emitpcode(0xDA, out->regHi, t, 1);
                out->kind = 0;
            }
            break;
        case 0x19:
            *out = func_0049D710(0x1B9, args->node);
            break;
        case 0x1A:
            *out = func_0049D710(0x1BA, args->node);
            break;
        case 0x1B:
            *out = func_0049D710(0x1BB, args->node);
            break;
        case 0x15:
            *out = func_0049D7A0(0x1A7, 0x1A8, args->node, args->next->node);
            break;
        case 0x16:
            *out = func_0049D7A0(0x1A9, 0x1AA, args->node, args->next->node);
            break;
        case 0x17:
            *out = func_0049D7A0(0x1B5, 0x1B6, args->node, args->next->node);
            break;
        case 0x18:
            *out = func_0049D7A0(0x1B7, 0x1B8, args->node, args->next->node);
            break;
        case 0x23:
        case 0x24:
        case 0x25:
            CError_FATAL(2558);
        case 0xE: // __mulhi
        case 0xF: // __mulhiu
            INTR_EVAL(args->node, v);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&stsignedlong), 1);
            }
            INTR_EVAL(args->next->node, v2);
            if (v2.kind != 0) {
                func_004A64C0(&v2, TYPE(&stsignedlong), 1);
            }
            op =
                call->data.funccall.funcref->data.objref->u.func.u.intrinsicid == 0xE ? 0xB6 : 0xB7;
            emitpcode(op, v.reg, v2.reg);
            out->reg = data_00625A98[0]++;
            emitpcode(0xA8, out->reg);
            out->kind = 0;
            break;
        case 0: // __alloca
            INTR_EVAL(args->node, v);
            out->kind = 0;
            out->offset = 0;
            out->object = 0;
            out->reg = func_004A4580(&v);
            break;
        case 9: // __memcpy
            INTR_EVAL(args->node, v);
            INTR_EVAL(args->next->node, v2);
            INTR_EVAL(args->next->next->node, v3);
            if (v3.kind == 3) {
                align = func_00464E20((Object*)v.object);
                if (func_00464E20((Object*)v2.object) < align) {
                    align = func_00464E20((Object*)v2.object);
                }
                func_0052DE30(&v, &v2, v3.imm.lo, align);
            } else {
                if (v.kind != 0) {
                    func_004A64C0(&v, TYPE(&void_ptr), 0);
                }
                if (v2.kind != 0) {
                    func_004A64C0(&v2, TYPE(&void_ptr), 0);
                }
                if (v3.kind != 0) {
                    func_004A64C0(&v3, TYPE(&stunsignedint), 0);
                }
                func_004A47E0(&v, 0, 1);
                func_004A47E0(&v2, 0, 2);
                func_004A47E0(&v3, 0, 2);
                func_004A4A20(rt_memcpy);
            }
            *out = v;
            break;
        case 8: // __strcpy (with a third argument: a copy of known size)
            INTR_EVAL(args->node, v);
            INTR_EVAL(args->next->node, v2);
            if (args->next->next) {
                INTR_EVAL(args->next->next->node, v3);
                align = func_00464E20((Object*)v.object);
                if (func_00464E20((Object*)v2.object) < align) {
                    align = func_00464E20((Object*)v2.object);
                }
                func_0052DE30(&v, &v2, v3.imm.lo, align);
            } else {
                if (v.kind != 0) {
                    func_004A64C0(&v, TYPE(&void_ptr), 0);
                }
                if (v2.kind != 0) {
                    func_004A64C0(&v2, TYPE(&void_ptr), 0);
                }
                func_004A47E0(&v, 0, 1);
                func_004A47E0(&v2, 0, 2);
                func_004A4A20(rt_strcpy);
            }
            *out = v;
            break;
        case 0x12: // __clear(ptr, size): store zero bytes
            INTR_EVAL(args->node, v);
            INTR_EVAL(args->next->node, v2);
            if (CInt64_IsZero(&v2.imm)) {
                break;
            }
            zero = func_004C3BA0(0, 0);
            if (CInt64_IsOne(&v2.imm)) {
                func_00479710(0xC3, &zero, &v);
                *out = v;
                break;
            }
            label = newlabel();
            label2 = newlabel();
            label->pclabel = (struct PCodeLabel*)makepclabel();
            label2->pclabel = (struct PCodeLabel*)makepclabel();
            if (v.kind != 0) {
                func_004A64C0(&v, args->node->rtype, 0);
            }
            if (v2.kind != 0) {
                func_004A64C0(&v2, args->node->rtype, 0); // (sic) the pointer's type
            }
            func_00479430(0x18, &v, &zero, label2);
            *out = func_0047F740(4, &v, &zero, 0);
            func_004A72B0(&v, args->node);
            func_0046FA90((MPCodeLabel*)label->pclabel);
            func_00479710(0xC3, &zero, &v);
            emitpcode(3, v.reg, v.reg, 1);
            emitpcode(3, v2.reg, v2.reg, -1);
            func_00479430(0x28, &v2, &zero, label);
            func_0046FA90((MPCodeLabel*)label2->pclabel);
            break;
        case 0x10: // __sqrt
            INTR_EVAL(args->node, v);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&stfloat), 1);
            }
            if (!copts.fp_library) {
                CError_FATAL(2681);
                break;
            }
            out->reg = data_00625A98[1]++;
            emitpcode(0x101, out->reg, v.reg);
            out->kind = 1;
            break;
        case 0x11: // __rsqrt
            INTR_EVAL(args->node, v);
            INTR_EVAL(args->next->node, v2);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&stfloat), 1);
            }
            if (v2.kind != 0) {
                func_004A64C0(&v2, TYPE(&stfloat), 1);
            }
            if (!copts.fp_library) {
                CError_FATAL(2695);
                break;
            }
            out->reg = data_00625A98[1]++;
            emitpcode(0x125, out->reg, v.reg, v2.reg);
            out->kind = 1;
            break;
        case 0xB2: // __builtin_allegrex_clz
            Intrinsics_GenUnary(0x3FD, args, out, TYPE(&stsignedlong), 0);
            break;
        case 0xB3: // clo
            Intrinsics_GenUnary(0x3FC, args, out, TYPE(&stsignedlong), 0);
            break;
        case 0xB4: // ctz: clz of the reversed bits
        case 0xB5: // cto
            op = call->data.funccall.funcref->data.objref->u.func.u.intrinsicid == 0xB4 ? 0x3FD
                                                                                        : 0x3FC;
            INTR_EVAL(args->node, v);
            INTR_EVAL(args->node, v2);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&stsignedlong), 1);
            }
            if (v2.kind != 0) {
                func_004A64C0(&v2, TYPE(&stsignedlong), 1);
            }
            t = data_00625A98[0]++;
            emitpcode(0x3FB, t, v.reg);
            r = data_00625A98[0]++;
            *out = func_004C3BA0(r, 0);
            emitpcode(op, r, t, v2.reg);
            break;
        case 0xB6: // max
            Intrinsics_GenBinary(0x402, args, out);
            break;
        case 0xB7: // min
            Intrinsics_GenBinary(0x404, args, out);
            break;
        case 0xB8: // seb
            Intrinsics_GenUnary(0x3F2, args, out, TYPE(&stsignedlong), 0);
            break;
        case 0xB9: // seh
            Intrinsics_GenUnary(0x3F3, args, out, TYPE(&stsignedlong), 0);
            break;
        case 0xBA: // wsbh
            Intrinsics_GenUnary(0x3F4, args, out, TYPE(&stsignedlong), 0);
            break;
        case 0xBB: // wsbw
            Intrinsics_GenUnary(0x409, args, out, TYPE(&stsignedlong), 0);
            break;
        case 0xBC: // bitrev
            Intrinsics_GenUnary(0x3FB, args, out, TYPE(&stsignedlong), 0);
            break;
        case 0xC1: // ceil.w.s
            Intrinsics_GenUnary(0xF1, args, out, TYPE(&stfloat), 1);
            break;
        case 0xC2: // floor.w.s
            Intrinsics_GenUnary(0xF7, args, out, TYPE(&stfloat), 1);
            break;
        case 0xC3: // round.w.s
            Intrinsics_GenUnary(0xFF, args, out, TYPE(&stfloat), 1);
            break;
        case 0xC4: // trunc.w.s
            Intrinsics_GenUnary(0x107, args, out, TYPE(&stfloat), 1);
            break;
        case 0xC5: // sqrt.s
            Intrinsics_GenUnary(0x101, args, out, TYPE(&stfloat), 1);
            break;
        case 0xBF: // ext(x, pos, size)
            INTR_EVAL(args->node, v);
            INTR_EVAL(args->next->node, v2);
            INTR_EVAL(args->next->next->node, v3);
            INTR_ZERO_IS_CONST(v2);
            INTR_ZERO_IS_CONST(v3);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&stsignedlong), 1);
            }
            CInt64_SetLong(&limit, 32);
            if (v2.kind != 3 || CInt64_GreaterEqualU(v2.imm, limit)) {
                func_004C5940(0x22, 2);
                break;
            }
            CInt64_SetLong(&limit, 32);
            if (v3.kind != 3 || CInt64_GreaterEqualU(v3.imm, limit)) {
                func_004C5940(0x22, 3);
                break;
            }
            r = data_00625A98[0]++;
            *out = func_004C3BA0(r, 0);
            emitpcode(0x3EE, r, v.reg, v2.imm.lo, v3.imm.lo);
            break;
        case 0xC0: { // ins(x, y, pos, size)
            VOperand v4;

            INTR_EVAL(args->node, v);
            INTR_EVAL(args->next->node, v2);
            INTR_EVAL(args->next->next->node, v3);
            INTR_EVAL(args->next->next->next->node, v4);
            INTR_ZERO_IS_CONST(v3);
            INTR_ZERO_IS_CONST(v4);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&stsignedlong), 1);
            }
            if (v2.kind != 0) {
                func_004A64C0(&v2, TYPE(&stsignedlong), 1);
            }
            CInt64_SetLong(&limit, 32);
            if (v3.kind != 3 || CInt64_GreaterEqualU(v3.imm, limit)) {
                func_004C5940(0x22, 3);
                break;
            }
            CInt64_SetLong(&limit, 32);
            if (v4.kind != 3 || CInt64_GreaterEqualU(v4.imm, limit)) {
                func_004C5940(0x22, 4);
                break;
            }
            r = data_00625A98[0]++;
            *out = func_004C3BA0(r, 0);
            emitpcode(0xAA, r, v.reg);
            emitpcode(0x3EF, r, v2.reg, v3.imm.lo, v4.imm.lo);
            break;
        }
        case 0xBD: // rotr
        case 0xBE: // rotl
            INTR_EVAL(args->node, v);
            INTR_EVAL(args->next->node, v2);
            if (v.kind != 0) {
                func_004A64C0(&v, TYPE(&stsignedlong), 1);
            }
            r = data_00625A98[0]++;
            *out = func_004C3BA0(r, 0);
            if (v2.kind == 3) {
                CInt64_SetLong(&limit, 32);
                if (CInt64_LessU(v2.imm, limit)) {
                    if (call->data.funccall.funcref->data.objref->u.func.u.intrinsicid == 0xBD) {
                        emitpcode(0x3F0, r, v.reg, v2.imm.lo);
                    } else {
                        emitpcode(0x3F0, r, v.reg, 32 - v2.imm.lo);
                    }
                    break;
                }
            }
            if (v2.kind != 0) {
                func_004A64C0(&v2, TYPE(&stsignedlong), 1);
            }
            if (call->data.funccall.funcref->data.objref->u.func.u.intrinsicid == 0xBD) {
                emitpcode(0x3F1, r, v.reg, v2.reg);
            } else {
                t = data_00625A98[0]++;
                emitpcode(3, t, v2.reg, -32);
                u = data_00625A98[0]++;
                emitpcode(0xDC, u, 0, t);
                emitpcode(0x3F1, r, v.reg, u);
            }
            break;
        default:
            CError_FATAL(4124);
    }
}
#endif

// The MIPS16 runtime routines (0x5CBED8, 16 records; unit not known, the
// code follows the intrinsics). Only the objects are used.
typedef struct RuntimeRoutine {
    char* name;
    UInt32 flags;
    Type* type;
    Object* object;
} RuntimeRoutine;

extern RuntimeRoutine data_005CBED8[16];
extern Object* data_0062506C; // the first one (__MIPS16___ptr_glue)

// 0x45C740 (CodeGen_ReInitRuntimeObjects)
void func_0045C740(void) {
    int i;

    for (i = 0; i < 16; i++) {
        data_005CBED8[i].object->name = GetHashNameNodeExport(data_005CBED8[i].name);
    }
    data_0062506C = data_005CBED8[0].object;
}

// 0x45C780 (CodeGen_SetupRuntimeObjects)
void func_0045C780(void) {
    int i;

    data_005CBED8[0].object = CParser_NewRTFunc(TYPE(&stvoid), NULL, 0, 0);
    data_005CBED8[1].object = CParser_NewRTFunc(TYPE(&stvoid), NULL, 0, 0);
    data_005CBED8[2].object = CParser_NewRTFunc(TYPE(&stvoid), NULL, 0, 0);
    data_005CBED8[3].object = CParser_NewRTFunc(TYPE(&stsignedint), NULL, 0, 0);
    data_005CBED8[4].object = CParser_NewRTFunc(TYPE(&stunsignedint), NULL, 0, 0);
    data_005CBED8[5].object = CParser_NewRTFunc(TYPE(&stsignedint), NULL, 0, 0);
    data_005CBED8[6].object = CParser_NewRTFunc(TYPE(&stunsignedint), NULL, 0, 0);
    data_005CBED8[7].object = CParser_NewRTFunc(TYPE(&stsignedint), NULL, 0, 0);
    data_005CBED8[8].object = CParser_NewRTFunc(TYPE(&stunsignedint), NULL, 0, 0);
    for (i = 9; i < 16; i++) {
        data_005CBED8[i].object = CParser_NewRTFunc(TYPE(&stvoid), NULL, 0, 0);
    }
    data_0062506C = data_005CBED8[0].object;
}
