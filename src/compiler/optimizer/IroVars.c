#include "compiler/IroVars.h"
#include "compiler/IroDump.h"
#if MWCCPSP
#include "compiler/IrOptimizer.h"
#endif
#include "compiler/IroFlowgraph.h"
#include "compiler/IroJump.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroMalloc.h"
#include "compiler/IroPointerAnalysis.h"
#include "compiler/IROUseDef.h"
#include "compiler/IroUtil.h"
#include "compiler/BitVector.h"
#include "compiler/CClass.h"
#include "compiler/CError.h"
#include "compiler/CExpr.h"
#include "compiler/CFunc.h"
#include "compiler/CompilerTools.h"
#include "compiler/CParser.h"
#include "compiler/enode.h"
#include "compiler/Exceptions.h"
#include "compiler/InlineAsm.h"
#include "compiler/InlineAsmPPC.h"
#include "compiler/objects.h"
#include "compiler/types.h"
#if MWCCPSP
#include "compiler/CMachine.h"
#include "compiler/CScope.h"
#include <string.h>
#endif

#pragma pack(push, 2)
typedef struct IndirectRecordMatch {
    IROLinear* nd;
    struct IndirectRecordMatch* otherMatches;
} IndirectRecordMatch;

typedef struct IndirectRecord {
    IROLinear* linear;
    unsigned char flags;
    VarRecord* var;
    SInt32 addend;
    SInt32 size;
    UInt32 startbit;
    UInt32 endbit;
    Type* type;
    struct IndirectRecord* next;
    IndirectRecordMatch* matches;
    IROLinear* replacement;
} IndirectRecord;
#pragma pack(pop)

enum IndirectRecordFlags {
    IRF_Disabled = 1,
    IRF_Bitfield = 2
};

#if !MWCCPSP // 3.0.1: IroScalarization301.c
static IndirectRecord* IRO_FirstIndirectRecord;
static IndirectRecord* IRO_LastIndirectRecord;
#endif
static IROLinear* TheBaseTerm;
VarRecord* IRO_FirstVar;
VarRecord* IRO_LastVar;
SInt32 IRO_NumVars;
Boolean IRO_IsBitField;
SInt32 IRO_BaseTerms;
SInt32 IRO_VarTerms;
#if MWCCPSP
Boolean IRO_IsModifyOp[IRO_MAXEXPR];
Boolean IRO_IsAssignOp[IRO_MAXEXPR];
#else
Boolean IRO_IsModifyOp[MAXEXPR];
Boolean IRO_IsAssignOp[MAXEXPR];
#endif

#if !MWCCPSP
static int CheckAddress(IROLinear* nd, Boolean* resultFlag);
static int CheckAddressConsistency(IROLinear* nd, VarRecord* var, SInt32 addend, Type* type);
static void DisableEntries(IndirectRecord* irec);
static UInt32 ObjectIsArg(Object* obj);
#endif

#if MWCCPSP // 3.0.1 entries, generated from the original's stores
void IRO_InitializeIRO_IsModifyOpArray(void) {
    int i;

    for (i = 0; i < IRO_MAXEXPR; i++) {
        IRO_IsModifyOp[i] = FALSE;
    }

    IRO_IsModifyOp[EPOSTINC] = TRUE;
    IRO_IsModifyOp[EPOSTDEC] = TRUE;
    IRO_IsModifyOp[EPREINC] = TRUE;
    IRO_IsModifyOp[EPREDEC] = TRUE;
    IRO_IsModifyOp[EINDIRECT] = FALSE;
    IRO_IsModifyOp[EMONMIN] = FALSE;
    IRO_IsModifyOp[EBINNOT] = FALSE;
    IRO_IsModifyOp[ELOGNOT] = FALSE;
    IRO_IsModifyOp[EFORCELOAD] = FALSE;
    IRO_IsModifyOp[EMUL] = FALSE;
    IRO_IsModifyOp[EMULV] = FALSE;
    IRO_IsModifyOp[EDIV] = FALSE;
    IRO_IsModifyOp[EMODULO] = FALSE;
    IRO_IsModifyOp[EADDV] = FALSE;
    IRO_IsModifyOp[ESUBV] = FALSE;
    IRO_IsModifyOp[EADD] = FALSE;
    IRO_IsModifyOp[ESUB] = FALSE;
    IRO_IsModifyOp[ESHL] = FALSE;
    IRO_IsModifyOp[ESHR] = FALSE;
    IRO_IsModifyOp[ELESS] = FALSE;
    IRO_IsModifyOp[EGREATER] = FALSE;
    IRO_IsModifyOp[ELESSEQU] = FALSE;
    IRO_IsModifyOp[EGREATEREQU] = FALSE;
    IRO_IsModifyOp[EEQU] = FALSE;
    IRO_IsModifyOp[ENOTEQU] = FALSE;
    IRO_IsModifyOp[EAND] = FALSE;
    IRO_IsModifyOp[EXOR] = FALSE;
    IRO_IsModifyOp[EOR] = FALSE;
    IRO_IsModifyOp[ELAND] = FALSE;
    IRO_IsModifyOp[ELOR] = FALSE;
    IRO_IsModifyOp[EASS] = FALSE;
    IRO_IsModifyOp[EMULASS] = TRUE;
    IRO_IsModifyOp[EDIVASS] = TRUE;
    IRO_IsModifyOp[EMODASS] = TRUE;
    IRO_IsModifyOp[EADDASS] = TRUE;
    IRO_IsModifyOp[ESUBASS] = TRUE;
    IRO_IsModifyOp[ESHLASS] = TRUE;
    IRO_IsModifyOp[ESHRASS] = TRUE;
    IRO_IsModifyOp[EANDASS] = TRUE;
    IRO_IsModifyOp[EXORASS] = TRUE;
    IRO_IsModifyOp[EORASS] = TRUE;
    IRO_IsModifyOp[ECOMMA] = FALSE;
    IRO_IsModifyOp[EMIN] = FALSE;
    IRO_IsModifyOp[EMAX] = FALSE;
    IRO_IsModifyOp[EPMODULO] = TRUE;
    IRO_IsModifyOp[EROTL] = TRUE;
    IRO_IsModifyOp[EROTR] = TRUE;
    IRO_IsModifyOp[EBCLR] = TRUE;
    IRO_IsModifyOp[EBTST] = TRUE;
    IRO_IsModifyOp[EBSET] = TRUE;
    IRO_IsModifyOp[ETYPCON] = FALSE;
    IRO_IsModifyOp[EBITFIELD] = FALSE;
    IRO_IsModifyOp[EINTCONST] = FALSE;
    IRO_IsModifyOp[EFLOATCONST] = FALSE;
    IRO_IsModifyOp[ESTRINGCONST] = FALSE;
    IRO_IsModifyOp[ECOND] = FALSE;
    IRO_IsModifyOp[EFUNCCALL] = FALSE;
    IRO_IsModifyOp[EFUNCCALLP] = FALSE;
    IRO_IsModifyOp[EOBJREF] = FALSE;
    IRO_IsModifyOp[ENULLCHECK] = FALSE;
    IRO_IsModifyOp[EPRECOMP] = FALSE;
    IRO_IsModifyOp[ELABEL] = FALSE;
    IRO_IsModifyOp[ESCOPEBEGIN] = FALSE;
    IRO_IsModifyOp[ESCOPEEND] = FALSE;
    IRO_IsModifyOp[EMFPOINTER] = FALSE;
    IRO_IsModifyOp[ETEMP] = FALSE;
    IRO_IsModifyOp[ELOCOBJ] = FALSE;
    IRO_IsModifyOp[EARGOBJ] = FALSE;
    IRO_IsModifyOp[ESETCONST] = FALSE;
    IRO_IsModifyOp[ENEWEXCEPTION] = FALSE;
    IRO_IsModifyOp[ENEWEXCEPTIONARRAY] = FALSE;
    IRO_IsModifyOp[EINITTRYCATCH] = FALSE;
    IRO_IsModifyOp[EOBJACCESS] = FALSE;
    IRO_IsModifyOp[ETEMPLDEP] = FALSE;
    IRO_IsModifyOp[EPOINTERSTAR] = FALSE;
    IRO_IsModifyOp[EDOTSTAR] = FALSE;
    IRO_IsModifyOp[ECTORINIT] = FALSE;
    IRO_IsModifyOp[ESTMT] = FALSE;
    IRO_IsModifyOp[EINSTRUCTION] = FALSE;
    IRO_IsModifyOp[EDEFINE] = FALSE;
    IRO_IsModifyOp[EREUSE] = FALSE;
    IRO_IsModifyOp[EASSBLK] = FALSE;
    IRO_IsModifyOp[ECONDASS] = TRUE;
}
#else
void IRO_InitializeIRO_IsModifyOpArray(void) {
    int i;

    for (i = 0; i < MAXEXPR; i++) {
        IRO_IsModifyOp[i] = FALSE;
    }

    IRO_IsModifyOp[EPOSTINC] = TRUE;
    IRO_IsModifyOp[EPOSTDEC] = TRUE;
    IRO_IsModifyOp[EPREINC] = TRUE;
    IRO_IsModifyOp[EPREDEC] = TRUE;
    IRO_IsModifyOp[EINDIRECT] = FALSE;
    IRO_IsModifyOp[EMONMIN] = FALSE;
    IRO_IsModifyOp[EBINNOT] = FALSE;
    IRO_IsModifyOp[ELOGNOT] = FALSE;
    IRO_IsModifyOp[EFORCELOAD] = FALSE;
    IRO_IsModifyOp[EMUL] = FALSE;
    IRO_IsModifyOp[EMULV] = FALSE;
    IRO_IsModifyOp[EDIV] = FALSE;
    IRO_IsModifyOp[EMODULO] = FALSE;
    IRO_IsModifyOp[EADDV] = FALSE;
    IRO_IsModifyOp[ESUBV] = FALSE;
    IRO_IsModifyOp[EADD] = FALSE;
    IRO_IsModifyOp[ESUB] = FALSE;
    IRO_IsModifyOp[ESHL] = FALSE;
    IRO_IsModifyOp[ESHR] = FALSE;
    IRO_IsModifyOp[ELESS] = FALSE;
    IRO_IsModifyOp[EGREATER] = FALSE;
    IRO_IsModifyOp[ELESSEQU] = FALSE;
    IRO_IsModifyOp[EGREATEREQU] = FALSE;
    IRO_IsModifyOp[EEQU] = FALSE;
    IRO_IsModifyOp[ENOTEQU] = FALSE;
    IRO_IsModifyOp[EAND] = FALSE;
    IRO_IsModifyOp[EXOR] = FALSE;
    IRO_IsModifyOp[EOR] = FALSE;
    IRO_IsModifyOp[ELAND] = FALSE;
    IRO_IsModifyOp[ELOR] = FALSE;
    IRO_IsModifyOp[EASS] = FALSE;
    IRO_IsModifyOp[EMULASS] = TRUE;
    IRO_IsModifyOp[EDIVASS] = TRUE;
    IRO_IsModifyOp[EMODASS] = TRUE;
    IRO_IsModifyOp[EADDASS] = TRUE;
    IRO_IsModifyOp[ESUBASS] = TRUE;
    IRO_IsModifyOp[ESHLASS] = TRUE;
    IRO_IsModifyOp[ESHRASS] = TRUE;
    IRO_IsModifyOp[EANDASS] = TRUE;
    IRO_IsModifyOp[EXORASS] = TRUE;
    IRO_IsModifyOp[EORASS] = TRUE;
    IRO_IsModifyOp[ECOMMA] = FALSE;
    IRO_IsModifyOp[EPMODULO] = TRUE;
    IRO_IsModifyOp[EROTL] = TRUE;
    IRO_IsModifyOp[EROTR] = TRUE;
    IRO_IsModifyOp[EBCLR] = TRUE;
    IRO_IsModifyOp[EBTST] = TRUE;
    IRO_IsModifyOp[EBSET] = TRUE;
    IRO_IsModifyOp[ETYPCON] = FALSE;
    IRO_IsModifyOp[EBITFIELD] = FALSE;
    IRO_IsModifyOp[EINTCONST] = FALSE;
    IRO_IsModifyOp[EFLOATCONST] = FALSE;
    IRO_IsModifyOp[ESTRINGCONST] = FALSE;
    IRO_IsModifyOp[ECOND] = FALSE;
    IRO_IsModifyOp[EFUNCCALL] = FALSE;
    IRO_IsModifyOp[EFUNCCALLP] = FALSE;
    IRO_IsModifyOp[EOBJREF] = FALSE;
    IRO_IsModifyOp[EMFPOINTER] = FALSE;
    IRO_IsModifyOp[ENULLCHECK] = FALSE;
    IRO_IsModifyOp[EPRECOMP] = FALSE;
    IRO_IsModifyOp[ETEMP] = FALSE;
    IRO_IsModifyOp[EARGOBJ] = FALSE;
    IRO_IsModifyOp[ELOCOBJ] = FALSE;
    IRO_IsModifyOp[ELABEL] = FALSE;
    IRO_IsModifyOp[ESETCONST] = FALSE;
    IRO_IsModifyOp[ENEWEXCEPTION] = FALSE;
    IRO_IsModifyOp[ENEWEXCEPTIONARRAY] = FALSE;
    IRO_IsModifyOp[EOBJLIST] = FALSE;
    IRO_IsModifyOp[EMEMBER] = FALSE;
    IRO_IsModifyOp[ETEMPLDEP] = FALSE;
    IRO_IsModifyOp[EINSTRUCTION] = FALSE;
    IRO_IsModifyOp[EDEFINE] = FALSE;
    IRO_IsModifyOp[EREUSE] = FALSE;
    IRO_IsModifyOp[EASSBLK] = FALSE;
    IRO_IsModifyOp[EVECTOR128CONST] = FALSE;
    IRO_IsModifyOp[ECONDASS] = TRUE;
}
#endif

#if MWCCPSP // 3.0.1 entries, generated from the original's stores
void IRO_InitializeIRO_IsAssignOpArray(void) {
    int i;

    for (i = 0; i < IRO_MAXEXPR; i++) {
        IRO_IsAssignOp[i] = FALSE;
    }

    IRO_IsAssignOp[EPOSTINC] = TRUE;
    IRO_IsAssignOp[EPOSTDEC] = TRUE;
    IRO_IsAssignOp[EPREINC] = TRUE;
    IRO_IsAssignOp[EPREDEC] = TRUE;
    IRO_IsAssignOp[EINDIRECT] = FALSE;
    IRO_IsAssignOp[EMONMIN] = FALSE;
    IRO_IsAssignOp[EBINNOT] = FALSE;
    IRO_IsAssignOp[ELOGNOT] = FALSE;
    IRO_IsAssignOp[EFORCELOAD] = FALSE;
    IRO_IsAssignOp[EMUL] = FALSE;
    IRO_IsAssignOp[EMULV] = FALSE;
    IRO_IsAssignOp[EDIV] = FALSE;
    IRO_IsAssignOp[EMODULO] = FALSE;
    IRO_IsAssignOp[EADDV] = FALSE;
    IRO_IsAssignOp[ESUBV] = FALSE;
    IRO_IsAssignOp[EADD] = FALSE;
    IRO_IsAssignOp[ESUB] = FALSE;
    IRO_IsAssignOp[ESHL] = FALSE;
    IRO_IsAssignOp[ESHR] = FALSE;
    IRO_IsAssignOp[ELESS] = FALSE;
    IRO_IsAssignOp[EGREATER] = FALSE;
    IRO_IsAssignOp[ELESSEQU] = FALSE;
    IRO_IsAssignOp[EGREATEREQU] = FALSE;
    IRO_IsAssignOp[EEQU] = FALSE;
    IRO_IsAssignOp[ENOTEQU] = FALSE;
    IRO_IsAssignOp[EAND] = FALSE;
    IRO_IsAssignOp[EXOR] = FALSE;
    IRO_IsAssignOp[EOR] = FALSE;
    IRO_IsAssignOp[ELAND] = FALSE;
    IRO_IsAssignOp[ELOR] = FALSE;
    IRO_IsAssignOp[EASS] = TRUE;
    IRO_IsAssignOp[EMULASS] = TRUE;
    IRO_IsAssignOp[EDIVASS] = TRUE;
    IRO_IsAssignOp[EMODASS] = TRUE;
    IRO_IsAssignOp[EADDASS] = TRUE;
    IRO_IsAssignOp[ESUBASS] = TRUE;
    IRO_IsAssignOp[ESHLASS] = TRUE;
    IRO_IsAssignOp[ESHRASS] = TRUE;
    IRO_IsAssignOp[EANDASS] = TRUE;
    IRO_IsAssignOp[EXORASS] = TRUE;
    IRO_IsAssignOp[EORASS] = TRUE;
    IRO_IsAssignOp[ECOMMA] = FALSE;
    IRO_IsAssignOp[EMIN] = FALSE;
    IRO_IsAssignOp[EMAX] = FALSE;
    IRO_IsAssignOp[EPMODULO] = TRUE;
    IRO_IsAssignOp[EROTL] = FALSE;
    IRO_IsAssignOp[EROTR] = FALSE;
    IRO_IsAssignOp[EBCLR] = TRUE;
    IRO_IsAssignOp[EBTST] = TRUE;
    IRO_IsAssignOp[EBSET] = TRUE;
    IRO_IsAssignOp[ETYPCON] = FALSE;
    IRO_IsAssignOp[EBITFIELD] = FALSE;
    IRO_IsAssignOp[EINTCONST] = FALSE;
    IRO_IsAssignOp[EFLOATCONST] = FALSE;
    IRO_IsAssignOp[ESTRINGCONST] = FALSE;
    IRO_IsAssignOp[ECOND] = FALSE;
    IRO_IsAssignOp[EFUNCCALL] = FALSE;
    IRO_IsAssignOp[EFUNCCALLP] = FALSE;
    IRO_IsAssignOp[EOBJREF] = FALSE;
    IRO_IsAssignOp[ENULLCHECK] = FALSE;
    IRO_IsAssignOp[EPRECOMP] = FALSE;
    IRO_IsAssignOp[ELABEL] = FALSE;
    IRO_IsAssignOp[ESCOPEBEGIN] = FALSE;
    IRO_IsAssignOp[ESCOPEEND] = FALSE;
    IRO_IsAssignOp[EMFPOINTER] = FALSE;
    IRO_IsAssignOp[ETEMP] = FALSE;
    IRO_IsAssignOp[ELOCOBJ] = FALSE;
    IRO_IsAssignOp[EARGOBJ] = FALSE;
    IRO_IsAssignOp[ESETCONST] = FALSE;
    IRO_IsAssignOp[ENEWEXCEPTION] = FALSE;
    IRO_IsAssignOp[ENEWEXCEPTIONARRAY] = FALSE;
    IRO_IsAssignOp[EINITTRYCATCH] = FALSE;
    IRO_IsAssignOp[EOBJACCESS] = FALSE;
    IRO_IsAssignOp[ETEMPLDEP] = FALSE;
    IRO_IsAssignOp[EPOINTERSTAR] = FALSE;
    IRO_IsAssignOp[EDOTSTAR] = FALSE;
    IRO_IsAssignOp[ECTORINIT] = FALSE;
    IRO_IsAssignOp[ESTMT] = FALSE;
    IRO_IsAssignOp[EINSTRUCTION] = FALSE;
    IRO_IsAssignOp[EDEFINE] = FALSE;
    IRO_IsAssignOp[EREUSE] = FALSE;
    IRO_IsAssignOp[EASSBLK] = FALSE;
    IRO_IsAssignOp[ECONDASS] = TRUE;
}
#else
void IRO_InitializeIRO_IsAssignOpArray(void) {
    int i;

    for (i = 0; i < MAXEXPR; i++) {
        IRO_IsAssignOp[i] = FALSE;
    }

    IRO_IsAssignOp[EPOSTINC] = TRUE;
    IRO_IsAssignOp[EPOSTDEC] = TRUE;
    IRO_IsAssignOp[EPREINC] = TRUE;
    IRO_IsAssignOp[EPREDEC] = TRUE;
    IRO_IsAssignOp[EINDIRECT] = FALSE;
    IRO_IsAssignOp[EMONMIN] = FALSE;
    IRO_IsAssignOp[EBINNOT] = FALSE;
    IRO_IsAssignOp[ELOGNOT] = FALSE;
    IRO_IsAssignOp[EFORCELOAD] = FALSE;
    IRO_IsAssignOp[EMUL] = FALSE;
    IRO_IsAssignOp[EMULV] = FALSE;
    IRO_IsAssignOp[EDIV] = FALSE;
    IRO_IsAssignOp[EMODULO] = FALSE;
    IRO_IsAssignOp[EADDV] = FALSE;
    IRO_IsAssignOp[ESUBV] = FALSE;
    IRO_IsAssignOp[EADD] = FALSE;
    IRO_IsAssignOp[ESUB] = FALSE;
    IRO_IsAssignOp[ESHL] = FALSE;
    IRO_IsAssignOp[ESHR] = FALSE;
    IRO_IsAssignOp[ELESS] = FALSE;
    IRO_IsAssignOp[EGREATER] = FALSE;
    IRO_IsAssignOp[ELESSEQU] = FALSE;
    IRO_IsAssignOp[EGREATEREQU] = FALSE;
    IRO_IsAssignOp[EEQU] = FALSE;
    IRO_IsAssignOp[ENOTEQU] = FALSE;
    IRO_IsAssignOp[EAND] = FALSE;
    IRO_IsAssignOp[EXOR] = FALSE;
    IRO_IsAssignOp[EOR] = FALSE;
    IRO_IsAssignOp[ELAND] = FALSE;
    IRO_IsAssignOp[ELOR] = FALSE;
    IRO_IsAssignOp[EASS] = TRUE;
    IRO_IsAssignOp[EMULASS] = TRUE;
    IRO_IsAssignOp[EDIVASS] = TRUE;
    IRO_IsAssignOp[EMODASS] = TRUE;
    IRO_IsAssignOp[EADDASS] = TRUE;
    IRO_IsAssignOp[ESUBASS] = TRUE;
    IRO_IsAssignOp[ESHLASS] = TRUE;
    IRO_IsAssignOp[ESHRASS] = TRUE;
    IRO_IsAssignOp[EANDASS] = TRUE;
    IRO_IsAssignOp[EXORASS] = TRUE;
    IRO_IsAssignOp[EORASS] = TRUE;
    IRO_IsAssignOp[ECOMMA] = FALSE;
    IRO_IsAssignOp[EPMODULO] = TRUE;
    IRO_IsAssignOp[EROTL] = TRUE;
    IRO_IsAssignOp[EROTR] = TRUE;
    IRO_IsAssignOp[EBCLR] = TRUE;
    IRO_IsAssignOp[EBTST] = TRUE;
    IRO_IsAssignOp[EBSET] = TRUE;
    IRO_IsAssignOp[ETYPCON] = FALSE;
    IRO_IsAssignOp[EBITFIELD] = FALSE;
    IRO_IsAssignOp[EINTCONST] = FALSE;
    IRO_IsAssignOp[EFLOATCONST] = FALSE;
    IRO_IsAssignOp[ESTRINGCONST] = FALSE;
    IRO_IsAssignOp[ECOND] = FALSE;
    IRO_IsAssignOp[EFUNCCALL] = FALSE;
    IRO_IsAssignOp[EFUNCCALLP] = FALSE;
    IRO_IsAssignOp[EOBJREF] = FALSE;
    IRO_IsAssignOp[EMFPOINTER] = FALSE;
    IRO_IsAssignOp[ENULLCHECK] = FALSE;
    IRO_IsAssignOp[EPRECOMP] = FALSE;
    IRO_IsAssignOp[ETEMP] = FALSE;
    IRO_IsAssignOp[EARGOBJ] = FALSE;
    IRO_IsAssignOp[ELOCOBJ] = FALSE;
    IRO_IsAssignOp[ELABEL] = FALSE;
    IRO_IsAssignOp[ESETCONST] = FALSE;
    IRO_IsAssignOp[ENEWEXCEPTION] = FALSE;
    IRO_IsAssignOp[ENEWEXCEPTIONARRAY] = FALSE;
    IRO_IsAssignOp[EOBJLIST] = FALSE;
    IRO_IsAssignOp[EMEMBER] = FALSE;
    IRO_IsAssignOp[ETEMPLDEP] = FALSE;
    IRO_IsAssignOp[EINSTRUCTION] = FALSE;
    IRO_IsAssignOp[EDEFINE] = FALSE;
    IRO_IsAssignOp[EREUSE] = FALSE;
    IRO_IsAssignOp[EASSBLK] = FALSE;
    IRO_IsAssignOp[EVECTOR128CONST] = FALSE;
    IRO_IsAssignOp[ECONDASS] = TRUE;
}
#endif

#if MWCCPSP
// 3.0.1: new records start with inductionState TRUE; every lookup is shown
// to IRO_CheckVar (0x55A3E0, empty in this build).
extern void IRO_CheckVar(void* list, VarRecord* var);
extern Boolean IRO_IsLocalObject(Object* object);
extern char IRO_VarCheckList[];

VarRecord* IRO_FindVar(Object* object, Boolean create, Boolean allowRegister) {
    VarRecord* var;
    VarInfo* vi;

    var = object->varptr;
    if (!var && create) {
        vi = NULL;
        if (IRO_IsLocalObject(object)) {
            vi = object->u.var.info;
        }
        if (vi) {
            vi->usage = 0;
        }
        var = IRO_Alloc(sizeof(VarRecord));
        memset(var, 0, sizeof(VarRecord));
        var->object = object;
        var->index = ++IRO_NumVars;
        var->unused = 0;
        var->inductionState = TRUE;
        var->next = NULL;
        var->defs = NULL;
        var->uses = NULL;
        var->bitfieldType = NULL;
        var->bitFieldReplacement = NULL;
        var->noregister = FALSE;
        if (IRO_FirstVar) {
            IRO_LastVar->next = var;
        } else {
            IRO_FirstVar = var;
        }
        IRO_LastVar = var;
        object->varptr = var;
    }

    if (var && (IRO_CheckVar(IRO_VarCheckList, var), !allowRegister)) {
        var->noregister = TRUE;
        if (IRO_IsLocalObject(object) &&
            object->u.var.info &&
            object->u.var.info->noregister == 0) {
            object->u.var.info->noregister = 2;
        }
    }

    return var;
}
#else
VarRecord* IRO_FindVar(Object* object, Boolean create, Boolean allowRegister) {
    VarRecord* var;
    VarInfo* vi;

    var = object->varptr;
    if (!var && create) {
        vi = NULL;
        if (object->datatype == DLOCAL) {
            vi = object->u.var.info;
        }
        if (vi) {
            vi->usage = 0;
        }
        var = oalloc(sizeof(VarRecord));
        var->object = object;
        var->index = ++IRO_NumVars;
        var->unused = 0;
        var->inductionState = FALSE;
        var->next = NULL;
        var->defs = NULL;
        var->uses = NULL;
        var->bitfieldType = NULL;
        var->bitFieldReplacement = NULL;
        var->noregister = FALSE;
        if (IRO_FirstVar) {
            IRO_LastVar->next = var;
        } else {
            IRO_FirstVar = var;
        }
        IRO_LastVar = var;
        object->varptr = var;
    }

    if (var && !allowRegister) {
        var->noregister = TRUE;
        if (object->datatype == DLOCAL &&
            object->u.var.info &&
            object->u.var.info->noregister == 0) {
            object->u.var.info->noregister = 2;
        }
    }

    return var;
}
#endif

void ResetVars(void) {
    VarRecord* var;

    for (var = IRO_FirstVar; var; var = var->next) {
        var->noregister = FALSE;
        if (var->object && var->object->datatype == DLOCAL && var->object->u.var.info) {
            if (var->object->u.var.info->noregister == 2) {
                var->object->u.var.info->noregister = FALSE;
            }
        }
    }
}

static void IRO_HandleExceptionActions(IROLinear* linear) {
    ExceptionAction* action;

    for (action = linear->stmt->dobjstack; action; action = action->prev) {
        switch (action->type) {
            case EAT_DESTROYLOCAL:
                IRO_FindVar(action->data.destroy_local.local, TRUE, FALSE);
                break;
            case EAT_DESTROYLOCALCOND:
                IRO_FindVar(action->data.destroy_local_cond.local, TRUE, FALSE);
                break;
            case EAT_DESTROYLOCALOFFSET:
                IRO_FindVar(action->data.destroy_local_offset.local, TRUE, FALSE);
                break;
            case EAT_DESTROYLOCALARRAY:
                IRO_FindVar(action->data.destroy_local_array.localarray, TRUE, FALSE);
                break;
            case EAT_DESTROYMEMBER:
            case EAT_DESTROYBASE:
                IRO_FindVar(action->data.destroy_member.objectptr, TRUE, FALSE);
                break;
            case EAT_DESTROYMEMBERCOND:
                IRO_FindVar(action->data.destroy_member_cond.objectptr, TRUE, FALSE);
                break;
            case EAT_DESTROYMEMBERARRAY:
                IRO_FindVar(action->data.destroy_member_array.objectptr, TRUE, FALSE);
                break;
            case EAT_CATCHBLOCK:
                if (action->data.catch_block.catch_object) {
                    IRO_FindVar(action->data.catch_block.catch_object, TRUE, FALSE);
                }
                IRO_FindVar(action->data.catch_block.catch_info_object, TRUE, FALSE);
                break;
            case EAT_ACTIVECATCHBLOCK:
                IRO_FindVar(action->data.active_catch_block.catch_info_object, TRUE, FALSE);
                break;
            default:
                break;
        }
    }
}
#if MWCCPSP

#endif
#if MWCCPSP
// 3.0.1 (0x558F10): with lookupOnly the records must already exist (and are
// shown to IRO_CheckVar); only the objects a cleanup reads through memory
// (destroyed locals, catch info, the specification's info) lose their
// register.
static inline void FindVarEH(Object* object, Boolean lookupOnly, Boolean allowRegister) {
    VarRecord* var;
    VarInfo* vi;

    var = object->varptr;
    if (!var && !lookupOnly) {
        vi = NULL;
        if (IRO_IsLocalObject(object)) {
            vi = object->u.var.info;
        }
        if (vi) {
            vi->usage = 0;
        }
        var = IRO_Alloc(sizeof(VarRecord));
        memset(var, 0, sizeof(VarRecord));
        var->object = object;
        var->index = ++IRO_NumVars;
        var->unused = 0;
        var->inductionState = TRUE;
        var->next = NULL;
        var->defs = NULL;
        var->uses = NULL;
        var->bitfieldType = NULL;
        var->bitFieldReplacement = NULL;
        var->noregister = FALSE;
        if (IRO_FirstVar) {
            IRO_LastVar->next = var;
        } else {
            IRO_FirstVar = var;
        }
        IRO_LastVar = var;
        object->varptr = var;
    }
    if (var && lookupOnly) {
        IRO_CheckVar(IRO_VarCheckList, var);
    }
    if (var && !allowRegister) {
        var->noregister = TRUE;
        if (IRO_IsLocalObject(object) &&
            object->u.var.info &&
            object->u.var.info->noregister == 0) {
            object->u.var.info->noregister = 2;
        }
    }
}

void IRO_HandleExceptionActions301(IROLinear* linear, Boolean lookupOnly) {
    ExceptionAction* action;

    for (action = linear->stmt->dobjstack; action; action = action->prev) {
        switch (action->type) {
            case EAT_DESTROYLOCAL:
                FindVarEH(action->data.destroy_local.local, lookupOnly, FALSE);
                break;
            case EAT_DESTROYLOCALCOND:
                FindVarEH(action->data.destroy_local_cond.local, lookupOnly, FALSE);
                FindVarEH(action->data.destroy_local_cond.cond, lookupOnly, TRUE);
                break;
            case EAT_DESTROYLOCALOFFSET:
                FindVarEH(action->data.destroy_local_offset.local, lookupOnly, FALSE);
                break;
            case EAT_DESTROYLOCALPOINTER:
                FindVarEH(action->data.destroy_local_pointer.pointer, lookupOnly, TRUE);
                break;
            case EAT_DESTROYLOCALARRAY:
                FindVarEH(action->data.destroy_local_array.localarray, lookupOnly, FALSE);
                break;
            case EAT_DESTROYPARTIALARRAY:
                FindVarEH(action->data.destroy_partial_array.arraypointer, lookupOnly, TRUE);
                FindVarEH(action->data.destroy_partial_array.arraycounter, lookupOnly, TRUE);
                FindVarEH(action->data.destroy_partial_array.element_size, lookupOnly, TRUE);
                break;
            case EAT_DESTROYMEMBER:
            case EAT_DESTROYBASE:
                FindVarEH(action->data.destroy_member.objectptr, lookupOnly, TRUE);
                break;
            case EAT_DESTROYMEMBERCOND:
                FindVarEH(action->data.destroy_member_cond.objectptr, lookupOnly, TRUE);
                FindVarEH(action->data.destroy_member_cond.cond, lookupOnly, TRUE);
                break;
            case EAT_DESTROYMEMBERARRAY:
                FindVarEH(action->data.destroy_member_array.objectptr, lookupOnly, TRUE);
                break;
            case EAT_DELETEPOINTER:
            case EAT_DELETELOCALPOINTER:
                FindVarEH(action->data.delete_pointer.pointerobject, lookupOnly, TRUE);
                break;
            case EAT_DELETEPOINTERCOND:
                FindVarEH(action->data.delete_pointer_cond.pointerobject, lookupOnly, TRUE);
                FindVarEH(action->data.delete_pointer_cond.cond, lookupOnly, TRUE);
                break;
            case EAT_CATCHBLOCK:
                if (action->data.catch_block.catch_object) {
                    FindVarEH(action->data.catch_block.catch_object, lookupOnly, TRUE);
                }
                FindVarEH(action->data.catch_block.catch_info_object, lookupOnly, FALSE);
                break;
            case EAT_ACTIVECATCHBLOCK:
                FindVarEH(action->data.active_catch_block.catch_info_object, lookupOnly, FALSE);
                break;
            case EAT_SPECIFICATION:
                FindVarEH(action->data.specification.unexp_info_object, lookupOnly, FALSE);
                break;
            case EAT_UNK18:
                FindVarEH(action->data.destroy_local.local, lookupOnly, TRUE);
                break;
        }
    }
}
#endif

#if MWCCPSP
// 3.0.1: the variable records come from an inlined copy of IRO_FindVar;
// asm statements and calls that might throw hand their exception actions to
// IRO_HandleExceptionActions; the kill sets are built by
// IRO_ComputeKillSets (0x569D90) instead of here.
extern Boolean IRO_IsThrowingCall(IROLinear* linear);
extern void IRO_ComputeKillSets(void);
extern void IRO_CheckVars(void* list);

static inline VarRecord* FindVar(Object* object, Boolean allowRegister) {
    VarRecord* var;
    VarInfo* vi;

    var = object->varptr;
    if (!var) {
        vi = NULL;
        if (IRO_IsLocalObject(object)) {
            vi = object->u.var.info;
        }
        if (vi) {
            vi->usage = 0;
        }
        var = IRO_Alloc(sizeof(VarRecord));
        memset(var, 0, sizeof(VarRecord));
        var->object = object;
        var->index = ++IRO_NumVars;
        var->unused = 0;
        var->inductionState = TRUE;
        var->next = NULL;
        var->defs = NULL;
        var->uses = NULL;
        var->bitfieldType = NULL;
        var->bitFieldReplacement = NULL;
        var->noregister = FALSE;
        if (IRO_FirstVar) {
            IRO_LastVar->next = var;
        } else {
            IRO_FirstVar = var;
        }
        IRO_LastVar = var;
        object->varptr = var;
    }

    if (var && !allowRegister) {
        var->noregister = TRUE;
        if (IRO_IsLocalObject(object) &&
            object->u.var.info &&
            object->u.var.info->noregister == 0) {
            object->u.var.info->noregister = 2;
        }
    }
    return var;
}

void IRO_FindAllVars(void) {
    IROLinear* linear;
    Object* obj;

    IRO_NumVars = 0;
    IRO_FirstVar = IRO_LastVar = NULL;

    for (linear = IRO_FirstLinear; linear; linear = linear->next) {
        if (IS_LINEAR_ENODE(linear, EOBJREF)) {
            obj = linear->u.node->data.objref;
            if (obj->datatype == DLOCAL || obj->datatype == DDATA || obj->datatype == DABSOLUTE) {
                FindVar(obj, (linear->flags & IROLF_Ind) != 0);
            } else {
                obj->varptr = NULL;
            }
        } else if (linear->type == IROLinearAsm && IRO_GetAsmEffectsHook && linear->u.asm_stmt) {
            IROAsmEffects effects = IRO_EmptyAsmEffects;
            int i;

            IRO_GetAsmEffectsHook(linear->u.asm_stmt, &effects);
            for (i = 0; i < effects.numoperands; i++) {
                FindVar(effects.operands[i].object, effects.operands[i].type != 3);
            }
            if (effects.mayThrow && linear->stmt) {
                IRO_HandleExceptionActions301(linear, FALSE);
            }
        } else if (linear->stmt && IRO_IsThrowingCall(linear)) {
            IRO_HandleExceptionActions301(linear, FALSE);
        }
    }

    IRO_ComputeKillSets();
    IRO_CheckVars(IRO_VarCheckList);
    IRO_CheckForUserBreak();
}
#else
void IRO_FindAllVars(void) {
    IROLinear* linear;
    VarRecord* var;
    Object* obj;

    linear = IRO_FirstLinear;
    IRO_NumVars = 0;
    IRO_FirstVar = IRO_LastVar = NULL;

    while (linear) {
        if (IS_LINEAR_ENODE(linear, EOBJREF)) {
            obj = linear->u.node->data.objref;
            if (Inline_IsObjectData(obj) || obj->datatype == DLOCAL) {
                IRO_FindVar(obj, TRUE, (linear->flags & IROLF_Ind) != 0);
            } else {
                obj->varptr = NULL;
            }
        } else if (linear->type == IROLinearFunccall) {
            if (linear->stmt && IRO_FunctionCallMightThrowException(linear)) {
                IRO_HandleExceptionActions(linear);
            }
        } else if (linear->type == IROLinearAsm) {
            IAEffects effects;
            int i;
            CodeGen_GetAsmEffects(linear->u.asm_stmt, &effects);
            for (i = 0; i < effects.numoperands; i++) {
                IRO_FindVar(
                    effects.operands[i].object, TRUE, effects.operands[i].type != IAEffect_AddressOf
                );
            }
        }
        linear = linear->next;
    }

    var = IRO_FirstVar;
    Bv_AllocVector(&IRO_FuncKills, IRO_NumVars + 1);
    Bv_SetBit(0, IRO_FuncKills);
    while (var) {
        if (Inline_IsObjectData(var->object) || var->noregister) {
            Bv_SetBit(var->index, IRO_FuncKills);
        }
        var = var->next;
    }

    IRO_CheckForUserBreak();
}
#endif

void IRO_ZapVarPtrs(void) {
    VarRecord* var;

    for (var = IRO_FirstVar; var; var = var->next) {
        var->object->varptr = NULL;
    }
}

#if MWCCPSP
// 3.0.1 (0x5588D0): no lookups; the variables' register flags are redone
// from the address-taken references (through Object.varptr), then the kill
// sets and the variable checks
void IRO_UpdateVars(void) {
    IROLinear* linear;
    VarRecord* var;
    Object* obj;
    IROAsmEffects effects;
    int i;
    Boolean notaddr;

    for (var = IRO_FirstVar; var; var = var->next) {
        var->noregister = FALSE;
        obj = var->object;
        if (obj && obj->datatype == DLOCAL && obj->u.var.info && obj->u.var.info->noregister == 2) {
            obj->u.var.info->noregister = FALSE;
        }
    }
    for (linear = IRO_FirstLinear; linear; linear = linear->next) {
        if (linear->type == IROLinearOperand && linear->u.node->type == EOBJREF) {
            obj = linear->u.node->data.objref;
            if (obj->datatype == DLOCAL || obj->datatype == DDATA || obj->datatype == DABSOLUTE) {
                notaddr = (linear->flags & IROLF_Ind) || !(linear->flags & IROLF_Reffed);
                var = obj->varptr;
                if (var) {
                    IRO_CheckVar(IRO_VarCheckList, var);
                }
                if (var && !notaddr) {
                    var->noregister = TRUE;
                    if (IRO_IsLocalObject(obj) &&
                        obj->u.var.info &&
                        obj->u.var.info->noregister == 0) {
                        obj->u.var.info->noregister = 2;
                    }
                }
            }
            continue;
        }
        if (linear->type == IROLinearAsm && IRO_GetAsmEffectsHook && linear->u.asm_stmt) {
            effects = IRO_EmptyAsmEffects;
            IRO_GetAsmEffectsHook(linear->u.asm_stmt, &effects);
            for (i = 0; i < effects.numoperands; i++) {
                notaddr = effects.operands[i].type != 3;
                obj = effects.operands[i].object;
                var = obj->varptr;
                if (var) {
                    IRO_CheckVar(IRO_VarCheckList, var);
                }
                if (var && !notaddr) {
                    var->noregister = TRUE;
                    if (IRO_IsLocalObject(obj) &&
                        obj->u.var.info &&
                        obj->u.var.info->noregister == 0) {
                        obj->u.var.info->noregister = 2;
                    }
                }
            }
            if (effects.mayThrow && linear->stmt) {
                IRO_HandleExceptionActions301(linear, TRUE);
            }
        } else if (linear->stmt && IRO_IsThrowingCall(linear)) {
            IRO_HandleExceptionActions301(linear, TRUE);
        }
    }
    IRO_ComputeKillSets();
    IRO_CheckVars(IRO_VarCheckList);
}
#else
void IRO_UpdateVars(void) {
    IROLinear* linear;
    VarRecord* var;
    Object* obj;

    ResetVars();

    for (linear = IRO_FirstLinear; linear; linear = linear->next) {
        if (IS_LINEAR_ENODE(linear, EOBJREF)) {
            obj = linear->u.node->data.objref;
            if (Inline_IsObjectData(obj) || obj->datatype == DLOCAL) {
                IRO_FindVar(
                    obj, FALSE, (linear->flags & IROLF_Ind) || !(linear->flags & IROLF_Reffed)
                );
            }
        } else if (linear->type == IROLinearFunccall) {
            if (linear->stmt && IRO_FunctionCallMightThrowException(linear)) {
                IRO_HandleExceptionActions(linear);
            }
        } else if (linear->type == IROLinearAsm) {
            IAEffects effects;
            int i;
            CodeGen_GetAsmEffects(linear->u.asm_stmt, &effects);
            for (i = 0; i < effects.numoperands; i++) {
                IRO_FindVar(
                    effects.operands[i].object,
                    FALSE,
                    effects.operands[i].type != IAEffect_AddressOf
                );
            }
        }
    }

    var = IRO_FirstVar;
    Bv_AllocVector(&IRO_FuncKills, IRO_NumVars + 1);
    Bv_SetBit(0, IRO_FuncKills);
    while (var) {
        if (Inline_IsObjectData(var->object) || var->noregister) {
            Bv_SetBit(var->index, IRO_FuncKills);
        }
        var = var->next;
    }
}
#endif

void IRO_AddElmToList(IROLinear* linear, IROElmList** list) {
    IROElmList* l = oalloc(sizeof(IROElmList));
    l->element = linear;
    l->next = NULL;
    if (!*list) {
        *list = l;
    } else {
        l->next = *list;
        *list = l;
    }
}

void IRO_DecomposeAddressExpression(IROLinear* linear, IROAddrRecord* rec) {
    if (IS_LINEAR_DIADIC(linear->u.diadic.left, EADD)) {
        IRO_DecomposeAddressExpression(linear->u.diadic.left, rec);
    } else if (IS_LINEAR_ENODE(linear->u.diadic.left, EINTCONST)) {
        rec->numInts++;
        IRO_AddElmToList(linear->u.diadic.left, &rec->ints);
    } else if (IS_LINEAR_ENODE(linear->u.diadic.left, EOBJREF)) {
        rec->numObjRefs++;
        IRO_AddElmToList(linear->u.diadic.left, &rec->objRefs);
    } else {
        rec->numMisc++;
        IRO_AddElmToList(linear->u.diadic.left, &rec->misc);
    }

    if (IS_LINEAR_DIADIC(linear->u.diadic.right, EADD)) {
        IRO_DecomposeAddressExpression(linear->u.diadic.right, rec);
    } else if (IS_LINEAR_ENODE(linear->u.diadic.right, EINTCONST)) {
        rec->numInts++;
        IRO_AddElmToList(linear->u.diadic.right, &rec->ints);
    } else if (IS_LINEAR_ENODE(linear->u.diadic.right, EOBJREF)) {
        rec->numObjRefs++;
        IRO_AddElmToList(linear->u.diadic.right, &rec->objRefs);
    } else {
        rec->numMisc++;
        IRO_AddElmToList(linear->u.diadic.right, &rec->misc);
    }
}

void IRO_DecomposeAddressExpression_Cheap(IROLinear* linear) {
    if (IS_LINEAR_DIADIC(linear->u.diadic.left, EADD)) {
        IRO_DecomposeAddressExpression_Cheap(linear->u.diadic.left);
    } else if (!IS_LINEAR_ENODE(linear->u.diadic.left, EINTCONST)) {
        if (IS_LINEAR_ENODE(linear->u.diadic.left, EOBJREF)) {
            IRO_BaseTerms++;
            TheBaseTerm = linear->u.diadic.left;
        } else {
            IRO_VarTerms++;
        }
    }

    if (IS_LINEAR_DIADIC(linear->u.diadic.right, EADD)) {
        IRO_DecomposeAddressExpression_Cheap(linear->u.diadic.right);
    } else if (!IS_LINEAR_ENODE(linear->u.diadic.right, EINTCONST)) {
        if (IS_LINEAR_ENODE(linear->u.diadic.right, EOBJREF)) {
            IRO_BaseTerms++;
            TheBaseTerm = linear->u.diadic.right;
        } else {
            IRO_VarTerms++;
        }
    }
}

VarRecord* IRO_FindAssigned(IROLinear* linear) {
    VarRecord* rec;

    IRO_IsBitField = FALSE;
    if (linear->type == IROLinearOp2Arg) {
        linear = linear->u.diadic.left;
    } else if (linear->type == IROLinearOp1Arg) {
        linear = linear->u.monadic;
    } else {
        CError_FATAL(817);
    }

    if (IS_LINEAR_MONADIC(linear, EINDIRECT)) {
        linear = linear->u.monadic;
    }

    if (IS_LINEAR_MONADIC(linear, EBITFIELD)) {
        IRO_IsBitField = TRUE;
        linear = linear->u.monadic;
    }

    if (IS_LINEAR_DIADIC(linear, EADD)) {
        if (IS_LINEAR_ENODE(linear->u.diadic.left, EOBJREF) &&
            IS_LINEAR_ENODE(linear->u.diadic.right, EINTCONST)) {
            linear = linear->u.diadic.left;
        } else {
            IRO_BaseTerms = 0;
            IRO_DecomposeAddressExpression_Cheap(linear);
            if (IRO_BaseTerms == 1) {
                linear = TheBaseTerm;
            }
        }
    }

    if (!IS_LINEAR_ENODE(linear, EOBJREF)) {
        return NULL;
    }

    if ((rec = IRO_FindVar(linear->u.node->data.objref, FALSE, TRUE))) {
        return rec;
    } else {
        return NULL;
    }
}

#if !MWCCPSP
static void GetKillsByIndirectAssignment(IROLinear* linear) {
    IROListNode* resultList;
    IROListNode* scan;
    IROLinear* nd;
    IROLinear* inner;
    Boolean result;
    Boolean foundLinear;

    result = FALSE;

    if (linear->type == IROLinearOp2Arg) {
        linear = linear->u.diadic.left;
    } else {
        linear = linear->u.monadic;
    }

    if (linear &&
        linear->type == IROLinearOp1Arg &&
        linear->nodetype == EINDIRECT &&
        (inner = linear->u.monadic) &&
        copts.opt_pointer_analysis &&
        inner->pointsToFunction &&
        FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupLinearNodePointerExpr(FunctionName, inner, &resultList);
        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail) {
                    result = TRUE;
                    break;
                }

                foundLinear = FALSE;
                for (nd = scan->list.head; nd != scan->list.tail->next; nd = nd->next) {
                    if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
                        foundLinear = TRUE;
                        break;
                    }
                }
                if (!foundLinear) {
                    result = TRUE;
                    break;
                }
            }

            if (!result) {
                for (scan = resultList; scan; scan = scan->nextList) {
                    for (nd = scan->list.head; nd != scan->list.tail->next; nd = nd->next) {
                        if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
                            Object* obj;
                            VarRecord* var;
                            int index;

                            obj = nd->u.node->data.objref;
                            CError_ASSERT(951, obj != NULL);

                            var = IRO_FindVar(obj, TRUE, TRUE);
                            CError_ASSERT(953, var != NULL);

                            index = var->index;
                            CError_ASSERT(955, index != 0);

                            Bv_SetBit(index, IRO_VarKills);
                        }
                    }
                }
            }

            while (resultList) {
                IROListNode* next = resultList->nextList;
                IRO_free(resultList);
                resultList = next;
            }
        } else {
            result = TRUE;
        }
    } else {
        result = TRUE;
    }

    if (result) {
        Bv_Or(IRO_FuncKills, IRO_VarKills);
    }
}

static void GetKillsByFunctionCall(IROLinear* linear) {
    IROListNode* resultList;
    IROListNode* list;
    IROListNode* scan;
    IROLinear* nd;
    IROLinear* inner;
    Boolean result;
    Boolean foundLinear;
    Object* obj;
    ObjectList* killList;
    ObjectList* olist;

    result = FALSE;

    if ((inner = linear->u.funccall.callee) &&
        copts.opt_pointer_analysis &&
        inner->pointsToFunction &&
        FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupLinearNodePointerExpr(FunctionName, inner, &resultList);
        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail) {
                    result = TRUE;
                    break;
                }

                foundLinear = FALSE;
                for (nd = scan->list.head; nd != scan->list.tail->next; nd = nd->next) {
                    if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
                        foundLinear = TRUE;
                        obj = nd->u.node->data.objref;
                        CError_ASSERT(1027, obj != NULL);

                        killList = NULL;
                        PointerAnalysis_GetFunctionKills(obj, linear, &killList);

                        for (olist = killList; olist; olist = olist->next) {
                            if (!olist->object) {
                                result = TRUE;
                                break;
                            }
                        }

                        while (killList) {
                            ObjectList* next = killList->next;
                            IRO_free(killList);
                            killList = next;
                        }

                        if (result) {
                            break;
                        }
                    }
                }

                if (!foundLinear) {
                    result = TRUE;
                }
                if (result) {
                    break;
                }
            }

            if (!result) {
                for (scan = resultList; scan; scan = scan->nextList) {
                    for (nd = scan->list.head; nd != scan->list.tail->next; nd = nd->next) {
                        if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
                            obj = nd->u.node->data.objref;
                            killList = NULL;
                            PointerAnalysis_GetFunctionKills(obj, linear, &killList);

                            for (olist = killList; olist; olist = olist->next) {
                                VarRecord* var;
                                int index;

                                var = IRO_FindVar(olist->object, TRUE, TRUE);
                                CError_ASSERT(1078, var != NULL);

                                index = var->index;
                                CError_ASSERT(1080, index != 0);

                                Bv_SetBit(index, IRO_VarKills);
                            }

                            while (killList) {
                                ObjectList* next = killList->next;
                                IRO_free(killList);
                                killList = next;
                            }
                        }
                    }
                }
            }

            while (resultList) {
                list = resultList->nextList;
                IRO_free(resultList);
                resultList = list;
            }
        } else {
            result = TRUE;
        }
    } else {
        result = TRUE;
    }

    if (result) {
        Bv_Or(IRO_FuncKills, IRO_VarKills);
    }
}

void IRO_GetKills(IROLinear* linear) {
    VarRecord* var;

    switch (linear->type) {
        case IROLinearOp1Arg:
        case IROLinearOp2Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                int index;

                var = IRO_FindAssigned(linear);
                index = 0;
                if (var) {
                    index = var->index;
                }
                Bv_SetBit(index, IRO_VarKills);

                if (!index) {
                    GetKillsByIndirectAssignment(linear);
                }
            }
            break;
        case IROLinearFunccall:
            GetKillsByFunctionCall(linear);
            break;
        case IROLinearAsm: {
            IAEffects effects;
            int i;

            CodeGen_GetAsmEffects(linear->u.asm_stmt, &effects);
            for (i = 0; i < effects.numoperands; i++) {
                switch (effects.operands[i].type) {
                    case IAEffect_Write:
                    case IAEffect_Clobber:
                    case IAEffect_ReadWrite:
                        if ((var = IRO_FindVar(effects.operands[i].object, FALSE, TRUE))) {
                            Bv_SetBit(var->index, IRO_VarKills);
                        }
                        break;
                    default:
                        break;
                }
            }

            if (effects.writesMemory || effects.branchWithLink) {
                Bv_Or(IRO_FuncKills, IRO_VarKills);
            }
            break;
        }
        default:
            break;
    }
}
#else
// 3.0.1 (0x569B40..0x56AB70): what a store through a pointer or a call may
// change comes from the kill sets by store size (IRO_ComputeKillSets) into
// data_00624DA8, not from 2.4.7's IRO_FuncKills; a call may also reach the
// objects of its exception actions. Pointer analysis lists are freed with
// IRO_free2.
extern BitVector* data_00624DA8;
extern SInt32 IRO_MaxKillSize;
extern BitVector* IRO_SizeKills[0x401];
extern BitVector* IRO_KillWritable;
extern VarRecord* IRO_FindAssigned301(IROLinear* linear, Boolean* isBitfield);
extern Boolean IRO_FunctionCallMightThrowException(IROLinear* linear);
extern void PointerAnalysis_LookupGotoTargets(
    Object* func, IROLinear* linear, IROListNode** list
); // 0x4CEAB0
extern void func_0054FC80(ExceptionAction* action, void (*func)(Object*, void*), void* arg);
extern VarRecord* func_0057B6C0(void);
extern Boolean func_0057B770(void);
extern Boolean func_0057B7A0(Object* obj);
extern void* CABI_GetSizeTType(void);

// 0x569B40: what nd may change: the buffer argument of __clear(void*,
// size_t), only the errno variable for a library function that sets nothing
// else, else the variables a store of its size (or any write) may reach
void func_00569B40(BitVector* bv, IROLinear* nd) {
    IROLinear* callee;
    Object* obj;
    TypeFunc* tfunc;
    IROLinear* arg;
    VarRecord* var;
    SInt32 size;
    BitVector** table;

    if (nd->type == IROLinearFunccall &&
        (callee = nd->u.funccall.callee)->type == IROLinearOperand &&
        callee->u.node->type == EOBJREF &&
        (obj = callee->u.node->data.objref) &&
        obj->name &&
        !strcmp(obj->name->name, "__clear") &&
        obj->otype == OT_OBJECT &&
        obj->access == 0 &&
        obj->datatype == DFUNC &&
        (!copts.cplusplus || obj->nspace == cscope_root) &&
        obj->type &&
        obj->type->type == TYPEFUNC &&
        (tfunc = TYPE_FUNC(obj->type))->functype == TYPE(&void_ptr) &&
        tfunc->args &&
        tfunc->args->type == TYPE(&void_ptr) &&
        tfunc->args->next &&
        tfunc->args->next->type == (Type*)CABI_GetSizeTType() &&
        !tfunc->args->next->next &&
        (arg = nd->u.funccall.args[0])->type == IROLinearOperand &&
        arg->u.node->type == EOBJREF &&
        (var = IRO_FindVar(arg->u.node->data.objref, FALSE, TRUE))) {
        Bv_SetBit(var->index, bv);
        return;
    }
    if (nd->type == IROLinearFunccall &&
        (callee = nd->u.funccall.callee)->type == IROLinearOperand &&
        callee->u.node->type == EOBJREF &&
        !func_0057B7A0(callee->u.node->data.objref) &&
        func_0057B770()) {
        Bv_Clear(bv);
        if ((var = func_0057B6C0())) {
            Bv_SetBit(var->index, bv);
        }
        return;
    }
    if (nd->type == IROLinearOp1Arg && nd->nodetype == EINDIRECT && (nd->flags & IROLF_Assigned)) {
        size = nd->rtype->size;
        if (size > 0x400) {
            size = 0x400;
        }
        table = IRO_SizeKills;
    } else {
        size = 0;
        table = &IRO_KillWritable;
    }
    if (size > IRO_MaxKillSize) {
        Bv_Clear(bv);
        Bv_SetBit(0, bv);
    } else {
        Bv_Copy(table[size], bv);
    }
}

// 0x56A880: an object of an exception action
static void KillExceptionObject(Object* obj, void* arg) {
    VarRecord* var;
    int index;

    var = IRO_FindVar(obj, TRUE, TRUE);
    CError_ASSERT(772, var != NULL);
    index = var->index;
    CError_ASSERT(774, index != 0);
    Bv_SetBit(index, IRO_VarKills);
}

static inline Boolean ListHasObject(IROListNode* scan) {
    IROLinear* nd;

    for (nd = scan->list.head; nd != scan->list.tail->next; nd = nd->next) {
        if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
            return TRUE;
        }
    }
    return FALSE;
}

// 0x56A900
static void GetKillsByIndirectAssignment(IROLinear* linear) {
    IROListNode* resultList;
    IROListNode* scan;
    IROListNode* next;
    IROLinear* nd;
    IROLinear* inner;
    IROLinear* ptr;
    Boolean result;
    Object* obj;
    VarRecord* var;
    int index;

    inner = linear->u.monadic;
    result = FALSE;
    if (inner &&
        inner->type == IROLinearOp1Arg &&
        inner->nodetype == EINDIRECT &&
        (ptr = inner->u.monadic) &&
        copts.opt_pointer_analysis &&
        ptr->pointsToFunction &&
        FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupGotoTargets(FunctionName, ptr, &resultList);
        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail || !ListHasObject(scan)) {
                    result = TRUE;
                    break;
                }
            }
            if (!result) {
                for (scan = resultList; scan; scan = scan->nextList) {
                    for (nd = scan->list.head; nd != scan->list.tail->next; nd = nd->next) {
                        if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
                            obj = nd->u.node->data.objref;
                            CError_ASSERT(730, obj != NULL);
                            var = IRO_FindVar(obj, TRUE, TRUE);
                            CError_ASSERT(732, var != NULL);
                            index = var->index;
                            CError_ASSERT(734, index != 0);
                            Bv_SetBit(index, IRO_VarKills);
                        }
                    }
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
    if (result) {
        func_00569B40(data_00624DA8, inner);
        Bv_Or(data_00624DA8, IRO_VarKills);
    }
}

// 0x56A520
static void GetKillsByFunctionCall(IROLinear* linear) {
    IROListNode* resultList;
    IROListNode* scan;
    IROListNode* next;
    IROLinear* nd;
    IROLinear* inner;
    Boolean result;
    Boolean foundLinear;
    Object* obj;
    ObjectList* killList;
    ObjectList* olist;
    ObjectList* onext;
    VarRecord* var;
    int index;

    inner = linear->u.funccall.callee;
    result = FALSE;
    if (inner && copts.opt_pointer_analysis && inner->pointsToFunction && FunctionName) {
        resultList = NULL;
        PointerAnalysis_LookupGotoTargets(FunctionName, inner, &resultList);
        if (resultList) {
            for (scan = resultList; scan; scan = scan->nextList) {
                if (!scan->list.head || !scan->list.tail) {
                    result = TRUE;
                    break;
                }
                foundLinear = FALSE;
                for (nd = scan->list.head; nd != scan->list.tail->next; nd = nd->next) {
                    if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
                        foundLinear = TRUE;
                        obj = nd->u.node->data.objref;
                        CError_ASSERT(824, obj != NULL);
                        killList = NULL;
                        PointerAnalysis_GetFunctionKills(obj, linear, &killList);
                        for (olist = killList; olist; olist = olist->next) {
                            if (!olist->object) {
                                result = TRUE;
                                break;
                            }
                        }
                        while (killList) {
                            onext = killList->next;
                            IRO_free2(killList);
                            killList = onext;
                        }
                        if (result) {
                            break;
                        }
                    }
                }
                if (!foundLinear) {
                    result = TRUE;
                }
                if (result) {
                    break;
                }
            }
            if (!result) {
                for (scan = resultList; scan; scan = scan->nextList) {
                    for (nd = scan->list.head; nd != scan->list.tail->next; nd = nd->next) {
                        if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
                            obj = nd->u.node->data.objref;
                            killList = NULL;
                            PointerAnalysis_GetFunctionKills(obj, linear, &killList);
                            for (olist = killList; olist; olist = olist->next) {
                                var = IRO_FindVar(olist->object, TRUE, TRUE);
                                CError_ASSERT(875, var != NULL);
                                index = var->index;
                                CError_ASSERT(877, index != 0);
                                Bv_SetBit(index, IRO_VarKills);
                            }
                            while (killList) {
                                onext = killList->next;
                                IRO_free2(killList);
                                killList = onext;
                            }
                        }
                    }
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
    if (result) {
        func_00569B40(data_00624DA8, linear);
        Bv_Or(data_00624DA8, IRO_VarKills);
    }
    if (linear->stmt && IRO_FunctionCallMightThrowException(linear)) {
        func_0054FC80(linear->stmt->dobjstack, KillExceptionObject, NULL);
    }
}

// 0x56A300
void IRO_GetKills(IROLinear* linear) {
    VarRecord* var;
    int index;
    int i;

    switch (linear->type) {
        case IROLinearOp1Arg:
        case IROLinearOp2Arg:
            if (IRO_IsAssignOp[linear->nodetype]) {
                var = IRO_FindAssigned301(linear, NULL);
                index = 0;
                if (var) {
                    index = var->index;
                }
                Bv_SetBit(index, IRO_VarKills);
                if (!index) {
                    GetKillsByIndirectAssignment(linear);
                }
            }
            break;
        case IROLinearFunccall:
            if (!(linear->u.funccall.functype->flags & FUNC_INTRINSIC) ||
                copts.opt_intrinsicfunctioncallscanhavesideeffects) {
                GetKillsByFunctionCall(linear);
            }
            break;
        case IROLinearAsm:
            if (IRO_GetAsmEffectsHook && linear->u.asm_stmt) {
                IROAsmEffects effects = IRO_EmptyAsmEffects;
                IRO_GetAsmEffectsHook(linear->u.asm_stmt, &effects);
                for (i = 0; i < effects.numoperands; i++) {
                    switch (effects.operands[i].type) {
                        case 1: // written
                        case 2:
                        case 4:
                            if ((var = IRO_FindVar(effects.operands[i].object, FALSE, TRUE))) {
                                Bv_SetBit(var->index, IRO_VarKills);
                            }
                            break;
                    }
                }
                if (effects.mayThrow) {
                    func_00569B40(data_00624DA8, linear);
                    Bv_Or(data_00624DA8, IRO_VarKills);
                    if (linear->stmt) {
                        func_0054FC80(linear->stmt->dobjstack, KillExceptionObject, NULL);
                    }
                }
                if (effects.flag1) {
                    Bv_Set(IRO_VarKills);
                }
            }
            break;
        default:
            break;
    }
}

// 0x56A2E0: IRO_WalkTree callback
void func_0056A2E0(IROLinear* linear, Boolean isEntry) {
    if (isEntry) {
        IRO_GetKills(linear);
    }
}
#endif

#if MWCCPSP
// 3.0.1: reachability comes from IRO_FindAsmBarriers, asm operands from the
// back end's hook (kinds 1, 2 and 4 write), and with copts.microsoft_EH the
// objects of catch blocks are never reported.
extern void IRO_FindAsmBarriers(void);
extern VarRecord* IRO_FindAssigned301(IROLinear* linear, Boolean* isBitfield);

void IRO_CheckInit(void) {
    IRONode* fnode;
    IROLinear* nd;
    VarRecord* var;
    ObjectList* olist;
    ExceptionAction* act;
    int changed;
    int i;
    int j;
    BitVector* bv;

    IRO_FindAsmBarriers();
    fnode = IRO_FirstNode;
    Bv_AllocVector(&IRO_VarKills, IRO_NumVars + 1);

    while (fnode) {
        Bv_AllocVector(&fnode->in, IRO_NumVars + 1);
        Bv_AllocVector(&fnode->gen, IRO_NumVars + 1);

        for (nd = fnode->first; nd; nd = nd->next) {
            if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
                if (!(nd->flags & IROLF_Ind)) {
                    if ((var = IRO_FindVar(nd->u.node->data.objref, FALSE, FALSE))) {
                        Bv_SetBit(var->index, fnode->gen);
                    }
                } else if (!(nd->flags & IROLF_Assigned) || (nd->flags & IROLF_Used)) {
                    if ((var = IRO_FindVar(nd->u.node->data.objref, FALSE, TRUE))) {
                        if (!Bv_IsBitSet(var->index, fnode->gen)) {
                            Bv_SetBit(var->index, fnode->in);
                        }
                    }
                }
            } else if (nd->type == IROLinearOp2Arg && nd->nodetype == EASS) {
                if ((var = IRO_FindAssigned301(nd, NULL))) {
                    Bv_SetBit(var->index, fnode->gen);
                }
            } else if (nd->type == IROLinearAsm && IRO_GetAsmEffectsHook && nd->u.asm_stmt) {
                IROAsmEffects effects = IRO_EmptyAsmEffects;

                IRO_GetAsmEffectsHook(nd->u.asm_stmt, &effects);
                for (i = 0; i < effects.numoperands; i++) {
                    var = IRO_FindVar(effects.operands[i].object, FALSE, TRUE);
                    switch (effects.operands[i].type) {
                        case 1:
                        case 2:
                        case 4:
                            Bv_SetBit(var->index, fnode->gen);
                            break;
                    }
                }
            }

            if (nd == fnode->last) {
                break;
            }
        }

        fnode = fnode->nextnode;
    }

    Bv_AllocVector(&bv, IRO_NumVars + 1);

    do {
        changed = FALSE;
        for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
            Bv_Copy(fnode->gen, bv);
            for (j = 0; j < fnode->numpred; j++) {
                Bv_Or(IRO_NodeTable[fnode->pred[j]]->gen, bv);
            }
            if (!Bv_Compare(bv, fnode->gen)) {
                Bv_Copy(bv, fnode->gen);
                changed = TRUE;
            }
        }
    } while (changed);

    Bv_Clear(IRO_VarKills);

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (fnode->reachable) {
            Bv_Copy(fnode->in, bv);
            for (i = 0; i < fnode->numpred; i++) {
                Bv_Minus(IRO_NodeTable[fnode->pred[i]]->gen, bv);
            }
            Bv_Or(bv, IRO_VarKills);
        }
    }

    if (copts.microsoft_EH) {
        for (nd = IRO_FirstLinear; nd; nd = nd->next) {
            if (nd->type == IROLinearBeginCatch && nd->stmt) {
                for (act = nd->stmt->dobjstack; act; act = act->prev) {
                    if (act->type == EAT_CATCHBLOCK &&
                        act->data.catch_block.catch_object &&
                        (var = IRO_FindVar(act->data.catch_block.catch_object, FALSE, TRUE))) {
                        Bv_ClearBit(var->index, IRO_VarKills);
                    }
                }
            }
        }
    }

    for (olist = locals; olist; olist = olist->next) {
        if ((var = IRO_FindVar(olist->object, FALSE, TRUE))) {
            if (Bv_IsBitSet(var->index, IRO_VarKills)) {
                VarInfo* vi = olist->object->u.var.info;
                if (!IsTempName(olist->object->name) && !is_volatile_object(olist->object)) {
                    if (!IS_TYPE_CLASS(olist->object->type) ||
                        !CClass_IsEmpty(TYPE_CLASS(olist->object->type))) {
                        CError_SetErrorToken(&vi->deftoken);
                        CError_Warning(
                            ERR_VARIABLE_NOT_INITIALIZED_BEFORE_BEING_USED,
                            olist->object->name->name
                        );
                    }
                }
            }
        }
    }

    IRO_CheckForUserBreak();
}
#else
void IRO_CheckInit(void) {
    IRONode* fnode;
    IROLinear* nd;
    VarRecord* var;
    ObjectList* olist;
    int changed;
    int i;
    int j;
    BitVector* bv;

    IRO_MakeReachable(IRO_FirstNode);
    fnode = IRO_FirstNode;
    Bv_AllocVector(&IRO_VarKills, IRO_NumVars + 1);

    while (fnode) {
        Bv_AllocVector(&fnode->in, IRO_NumVars + 1);
        Bv_AllocVector(&fnode->gen, IRO_NumVars + 1);

        for (nd = fnode->first; nd; nd = nd->next) {
            if (nd->type == IROLinearOperand && nd->u.node->type == EOBJREF) {
                if (nd->flags & IROLF_Ind) {
                    if (!(nd->flags & IROLF_Assigned) || (nd->flags & IROLF_Used)) {
                        if ((var = IRO_FindVar(nd->u.node->data.objref, FALSE, TRUE))) {
                            if (!Bv_IsBitSet(var->index, fnode->gen)) {
                                Bv_SetBit(var->index, fnode->in);
                            }
                        }
                    }
                } else {
                    if ((var = IRO_FindVar(nd->u.node->data.objref, FALSE, FALSE))) {
                        Bv_SetBit(var->index, fnode->gen);
                    }
                }
            } else if (nd->type == IROLinearOp2Arg && nd->nodetype == EASS) {
                if ((var = IRO_FindAssigned(nd))) {
                    Bv_SetBit(var->index, fnode->gen);
                }
            } else if (nd->type == IROLinearAsm) {
                IAEffects effects;

                CodeGen_GetAsmEffects(nd->u.asm_stmt, &effects);
                for (i = 0; i < effects.numoperands; i++) {
                    var = IRO_FindVar(effects.operands[i].object, FALSE, TRUE);
                    switch (effects.operands[i].type) {
                        case IAEffect_Write:
                        case IAEffect_Clobber:
                        case IAEffect_ReadWrite:
                            Bv_SetBit(var->index, fnode->gen);
                            break;
                        default:
                            break;
                    }
                }
            }

            if (nd == fnode->last) {
                break;
            }
        }

        fnode = fnode->nextnode;
    }

    Bv_AllocVector(&bv, IRO_NumVars + 1);

    do {
        changed = FALSE;
        for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
            Bv_Copy(fnode->gen, bv);
            for (j = 0; j < fnode->numpred; j++) {
                Bv_Or(IRO_NodeTable[fnode->pred[j]]->gen, bv);
            }
            if (!Bv_Compare(bv, fnode->gen)) {
                Bv_Copy(bv, fnode->gen);
                changed = TRUE;
            }
        }
    } while (changed);

    Bv_Clear(IRO_VarKills);

    for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
        if (fnode->reachable) {
            Bv_Copy(fnode->in, bv);
            for (i = 0; i < fnode->numpred; i++) {
                Bv_Minus(IRO_NodeTable[fnode->pred[i]]->gen, bv);
            }
            Bv_Or(bv, IRO_VarKills);
        }
    }

    for (olist = locals; olist; olist = olist->next) {
        if ((var = IRO_FindVar(olist->object, FALSE, TRUE))) {
            if (Bv_IsBitSet(var->index, IRO_VarKills)) {
                VarInfo* vi = olist->object->u.var.info;
                if (!IsTempName(olist->object->name) && !is_volatile_object(olist->object)) {
                    if (!IS_TYPE_CLASS(olist->object->type) ||
                        !CClass_IsEmpty(TYPE_CLASS(olist->object->type))) {
                        CError_SetErrorToken(&vi->deftoken);
                        CError_Warning(
                            ERR_VARIABLE_NOT_INITIALIZED_BEFORE_BEING_USED,
                            olist->object->name->name
                        );
                    }
                }
            }
        }
    }

    IRO_CheckForUserBreak();
}
#endif

#if MWCCPSP
void RewriteIndDec(void) { // 0x581470; IroScalarization301.c calls it
#else
static void RewriteIndDec(void) {
#endif
    IROLinear* nd;
    IROLinear* add;
    IROLinear* Int;
    IROLinear* indirect;
    CInt64 value;
    IROList list;

    for (nd = IRO_FirstLinear; nd; nd = nd->next) {
        if (nd->type == IROLinearOp1Arg &&
            (nd->nodetype == EPOSTINC || nd->nodetype == EPOSTDEC) &&
            !(nd->flags & IROLF_Reffed)) {
            indirect = nd->u.monadic;
            if (indirect->type == IROLinearOp1Arg &&
                indirect->nodetype == EINDIRECT &&
                indirect->u.monadic->type == IROLinearOp1Arg &&
                indirect->u.monadic->nodetype == EBITFIELD) {
                value = IRO_GetSelfAssignmentVal(nd);
                IRO_InitList(&list);

                nd->type = IROLinearOp2Arg;
                nd->nodetype = EASS;
                nd->u.diadic.left = indirect;
                IRO_DuplicateExpr(indirect, &list);

                Int = IRO_NewIntConst(value, indirect->rtype);
                Int->flags |= IROLF_Used | IROLF_Reffed;

                add = IRO_NewLinear(IROLinearOp2Arg);
                add->nodetype = EADD;
                add->u.diadic.left = list.tail;
                add->u.diadic.left->flags = 0;
                add->u.diadic.left->flags |= IROLF_Used | IROLF_Reffed;
                add->u.diadic.right = Int;
                add->rtype = indirect->rtype;
                add->flags = 0;
                add->flags |= IROLF_Used | IROLF_Reffed;

                nd->u.diadic.right = add;

                IRO_AddToList(Int, &list);
                IRO_AddToList(add, &list);
                IRO_PasteAfter(list.head, list.tail, indirect);
            }
        }
    }
}

#if !MWCCPSP // 3.0.1's is in src/psp/optimizer/IroBitFieldTemps301.c
void IRO_RewriteBitFieldTemps(void) {
    IROLinear* nd;
    IROLinear* replacement;
    Object* obj;
    VarRecord* var;
    IROList list;

    for (nd = IRO_FirstLinear; nd; nd = nd->next) {
        if ((obj = IRO_IsVariable(nd)) && (nd->flags & IROLF_BitfieldIndirect)) {
            var = IRO_FindVar(obj, FALSE, TRUE);
            CError_ASSERT(1525, var != NULL);

            replacement = var->bitFieldReplacement;
            CError_ASSERT(1531, replacement != NULL);

            IRO_InitList(&list);
            IRO_DuplicateExpr(replacement, &list);
            IRO_NopOut(nd->u.monadic);
            nd->u.monadic = list.tail;
            IRO_Paste(list.head, list.tail, nd);
        }
    }
}
#endif

#if !MWCCPSP // 3.0.1: IRO_ScalarizeClassDataMembers and its helpers are in IroScalarization301.c
static Boolean FunctionCallMightUseOrKillAnyAddressedVar(IROLinear* funccall) {
    IROListNode* nextList;
    IROLinear* funcnd;
    IROLinear* nd;
    IROListNode* scan;
    IROListNode* resultList;
    ObjectList* olist;
    ObjectList* next;
    ObjectList* killList;
    VarRecord* var;
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
                        CError_ASSERT(1591, obj != NULL);

                        killList = NULL;
                        PointerAnalysis_GetFunctionKills(obj, funccall, &killList);
                        for (olist = killList; olist; olist = olist->next) {
                            if (!olist->object) {
                                result = TRUE;
                            } else {
                                var = IRO_FindVar(olist->object, TRUE, TRUE);
                                CError_ASSERT(1603, var != NULL);
                                if (var->noregister) {
                                    result = TRUE;
                                }
                            }

                            if (result) {
                                break;
                            }
                        }

                        while (killList) {
                            next = killList->next;
                            IRO_free(killList);
                            killList = next;
                        }

                        if (result) {
                            break;
                        }

                        killList = NULL;
                        PointerAnalysis_GetFunctionDependencies(obj, funccall, &killList);
                        for (olist = killList; olist; olist = olist->next) {
                            if (!olist->object) {
                                result = TRUE;
                            } else {
                                var = IRO_FindVar(olist->object, TRUE, TRUE);
                                CError_ASSERT(1631, var != NULL);
                                if (var->noregister) {
                                    result = TRUE;
                                }
                            }

                            if (result) {
                                break;
                            }
                        }

                        while (killList) {
                            next = killList->next;
                            IRO_free(killList);
                            killList = next;
                        }

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
                IRO_free(resultList);
                resultList = nextList;
            }
        } else {
            result = TRUE;
        }
    } else {
        result = TRUE;
    }

    return result;
}

static Boolean IndirectMightUseOrKillAnyAddressedVar(IROLinear* indirect) {
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
                        CError_ASSERT(1722, obj != NULL);
                        var = IRO_FindVar(obj, TRUE, TRUE);
                        CError_ASSERT(1724, var != NULL);

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
                IRO_free(resultList);
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

void IRO_ScalarizeClassDataMembers(void) {
    IROLinear* nd;
    Object* obj;
    IndirectRecord* irec;
    IndirectRecordMatch* match;
    IROLinear* father;
    IROList list1;
    VarRecord* var;
    IROList list2;
    Boolean unsafe;
    Object* argobj;

    nd = IRO_FirstLinear;
    IRO_FirstIndirectRecord = NULL;
    IRO_LastIndirectRecord = NULL;
    RewriteIndDec();
    IRO_DumpAfterPhase("RewriteIndDec", FALSE);
    unsafe = FALSE;

    while (nd) {
        if (nd->type == IROLinearOp1Arg && nd->nodetype == EINDIRECT) {
            CheckAddress(nd, &unsafe);
        }
        if (nd->type == IROLinearAsm) {
            IAEffects effects;
            SInt32 i;
            CodeGen_GetAsmEffects(nd->u.asm_stmt, &effects);
            for (i = 0; i < effects.numoperands; i++) {
                if ((var = IRO_FindVar(effects.operands[i].object, FALSE, TRUE))) {
                    CheckAddressConsistency(nd, var, -1, &stvoid);
                } else {
                    CError_FATAL(1822);
                }
            }
        }
        if (nd->type == IROLinearFunccall && !unsafe) {
            unsafe = FunctionCallMightUseOrKillAnyAddressedVar(nd);
        }

        nd = nd->next;
    }

    if (unsafe) {
        for (irec = IRO_FirstIndirectRecord; irec; irec = irec->next) {
            if (irec->var->noregister) {
                irec->flags |= IRF_Disabled;
            }
        }
    }

    for (irec = IRO_FirstIndirectRecord; irec; irec = irec->next) {
        if (!(irec->flags & IRF_Disabled)) {
            obj = create_temp_object(irec->type);
            var = IRO_FindVar(obj, TRUE, TRUE);
            for (match = irec->matches; match; match = match->otherMatches) {
                IRO_InitList(&list1);
                IRO_InitList(&list2);
                IRO_DuplicateExpr(match->nd, &list1);
                IRO_DuplicateExpr(match->nd, &list2);
                if (match->nd->type == IROLinearOperand && match->nd->u.node->type == EOBJREF) {
                    father = IRO_LocateFather(match->nd);
                    if (irec->flags & IRF_Bitfield) {
                        father->flags |= IROLF_BitfieldIndirect;
                        var->bitfieldType = TYPE_BITFIELD(match->nd->rtype);
                        var->bitFieldReplacement = list2.tail;
                    }
                    match->nd->u.node = create_objectrefnode(obj);
                    irec->replacement = list2.tail;
                } else {
                    father = IRO_LocateFather(match->nd);
                    if (irec->flags & IRF_Bitfield) {
                        father->flags |= IROLF_BitfieldIndirect;
                        var->bitfieldType = TYPE_BITFIELD(match->nd->rtype);
                        var->bitFieldReplacement = list2.tail;
                    }
                    irec->replacement = list2.tail;

                    IRO_NopOut(father->u.monadic);
                    father->u.monadic = IRO_NewLinear(IROLinearOperand);
                    father->u.monadic->u.node = create_objectrefnode(obj);
                    father->u.monadic->rtype = match->nd->rtype;
                    father->u.monadic->index = ++IRO_NumLinear;
                    father->u.monadic->flags |= IROLF_Ind | IROLF_Reffed;
                    IRO_NopOut(match->nd);
                    IRO_PasteAfter(father->u.monadic, father->u.monadic, match->nd);
                }
            }

            argobj = irec->var->object;
            if (ObjectIsArg(argobj)) {
                IROLinear* tempRef;
                IROLinear* addr;
                IROLinear* tempInd;
                IROLinear* valueInd;
                IROLinear* assign;

                tempRef = IRO_NewLinear(IROLinearOperand);
                tempRef->u.node = create_objectrefnode(obj);
                tempRef->rtype = tempRef->u.node->data.objref->type;
                tempRef->index = ++IRO_NumLinear;
                tempRef->flags |= IROLF_Ind | IROLF_Assigned;

                addr = list1.tail;

                tempInd = IRO_NewLinear(IROLinearOp1Arg);
                tempInd->nodetype = EINDIRECT;
                tempInd->rtype = irec->type;
                tempInd->u.monadic = tempRef;
                tempInd->index = ++IRO_NumLinear;
                tempInd->flags |= IROLF_Assigned;

                valueInd = IRO_NewLinear(IROLinearOp1Arg);
                valueInd->nodetype = EINDIRECT;
                valueInd->rtype = irec->type;
                valueInd->u.monadic = addr;
                valueInd->index = ++IRO_NumLinear;
                valueInd->flags |= IROLF_Reffed;

                assign = IRO_NewLinear(IROLinearOp2Arg);
                assign->nodetype = EASS;
                assign->u.diadic.left = tempInd;
                assign->u.diadic.right = valueInd;
                assign->rtype = irec->type;
                assign->index = ++IRO_NumLinear;

                addr->next = valueInd;
                valueInd->next = tempRef;
                tempRef->next = tempInd;
                tempInd->next = assign;

                IRO_PasteAfter(list1.head, assign, IRO_FirstLinear);
            }
        }
    }

    IRO_CheckForUserBreak();
}

static int CheckAddress(IROLinear* nd, Boolean* resultFlag) {
    IROLinear* inner;
    Object* obj;
    VarRecord* var;
    Boolean result;
    SInt32 hi;

    inner = nd->u.monadic;
    if (inner->type == IROLinearOp1Arg && inner->nodetype == EBITFIELD) {
        inner = inner->u.monadic;
    }

    result = FALSE;

    if (inner->type == IROLinearOperand && inner->u.node->type == EOBJREF) {
        obj = inner->u.node->data.objref;
        if (IS_TYPE_CLASS(obj->type) || IS_TYPE_STRUCT(obj->type) || IS_TYPE_ARRAY(obj->type)) {
            if (obj->datatype == DLOCAL) {
                var = IRO_FindVar(obj, FALSE, TRUE);
                CError_ASSERT(2239, var != NULL);
                CheckAddressConsistency(nd, var, 0, nd->rtype);
                result = TRUE;
            }
        }
    } else if (inner->type == IROLinearOp2Arg &&
               inner->nodetype == EADD &&
               inner->u.diadic.left->type == IROLinearOperand &&
               inner->u.diadic.left->u.node->type == EOBJREF) {
        if (inner->u.diadic.right->type == IROLinearOperand &&
            inner->u.diadic.right->u.node->type == EINTCONST &&
            (hi = inner->u.diadic.right->u.node->data.intval.hi) == 0) {
            obj = inner->u.diadic.left->u.node->data.objref;
            if (IS_TYPE_CLASS(obj->type) || IS_TYPE_STRUCT(obj->type) || IS_TYPE_ARRAY(obj->type)) {
                if (obj->datatype == DLOCAL) {
                    var = IRO_FindVar(obj, FALSE, TRUE);
                    CError_ASSERT(2266, var != NULL);
                    CheckAddressConsistency(
                        nd, var, inner->u.diadic.right->u.node->data.intval.lo, nd->rtype
                    );
                    result = TRUE;
                }
            }
        } else {
            obj = inner->u.diadic.left->u.node->data.objref;
            if (IS_TYPE_CLASS(obj->type) || IS_TYPE_STRUCT(obj->type) || IS_TYPE_ARRAY(obj->type)) {
                if (obj->datatype == DLOCAL) {
                    var = IRO_FindVar(obj, FALSE, TRUE);
                    CheckAddressConsistency(nd, var, -1, nd->rtype);
                    result = TRUE;
                }
            }
        }
    }

    if (!result && !*resultFlag) {
        *resultFlag = IndirectMightUseOrKillAnyAddressedVar(nd);
    }

    return FALSE;
}

static int CheckAddressConsistency(IROLinear* nd, VarRecord* var, SInt32 addend, Type* type) {
    IndirectRecord* rp;
    IndirectRecordMatch* match;
    UInt32 conflict;
    IROLinear* inner;
    UInt32 start_bit;
    UInt32 end_bit;
    TypeBitfield* tbitfield;

    conflict = FALSE;
    inner = nd->u.monadic;
    if (inner->type == IROLinearOp1Arg && inner->nodetype == EBITFIELD) {
        tbitfield = TYPE_BITFIELD(inner->rtype);
        start_bit = tbitfield->offset + 8 * addend;
        end_bit = start_bit + tbitfield->bitlength - 1;
    } else {
        start_bit = 8 * addend;
        end_bit = start_bit + 8 * type->size - 1;
    }

    if (var->noregister && !copts.opt_pointer_analysis) {
        return FALSE;
    }

    if (nd->rtype && CParser_IsVolatile(nd->rtype, nd->nodeflags & ENODE_FLAG_QUALS)) {
        addend = -1;
    }
    if (is_volatile_object(var->object)) {
        addend = -1;
    }

    for (rp = IRO_FirstIndirectRecord; rp; rp = rp->next) {
        if (rp->var->index == var->index) {
            if (rp->flags & IRF_Disabled) {
                return FALSE;
            }

            if (addend == -1) {
                conflict = TRUE;
                break;
            }

            if (IRO_TypesEqual(rp->type, type) &&
                rp->startbit == start_bit &&
                rp->endbit == end_bit) {
                match = oalloc(sizeof(IndirectRecordMatch));
                match->nd = inner;
                match->otherMatches = NULL;
                if (!rp->matches) {
                    rp->matches = match;
                } else {
                    match->otherMatches = rp->matches;
                    rp->matches = match;
                }

                IRO_Dump(
                    "Exact Match of Type and bits at %d and %d\n", rp->linear->index, nd->index
                );
                IRO_Dump(
                    "Rp->startbit=%d,Rp->endbit=%d,start_bit=%d,end_bit=%d\n",
                    rp->startbit,
                    rp->endbit,
                    start_bit,
                    end_bit
                );
                return TRUE;
            }

            if (rp->startbit == start_bit &&
                rp->endbit == end_bit &&
                !IRO_TypesEqual(rp->type, type)) {
                IRO_Dump(
                    "match on bits but mismatch on type %d and %d\n", rp->linear->index, nd->index
                );
                IRO_Dump(
                    "Rp->startbit=%d,Rp->endbit=%d,start_bit=%d,end_bit=%d\n",
                    rp->startbit,
                    rp->endbit,
                    start_bit,
                    end_bit
                );
                conflict = TRUE;
                break;
            }

            if (rp->startbit >= start_bit && end_bit >= rp->startbit) {
                IRO_Dump("Overlap detected --1 %d and %d\n", rp->linear->index, nd->index);
                IRO_Dump(
                    "Rp->startbit=%d,Rp->endbit=%d,start_bit=%d,end_bit=%d\n",
                    rp->startbit,
                    rp->endbit,
                    start_bit,
                    end_bit
                );
                conflict = TRUE;
                break;
            }

            if (rp->startbit < start_bit && rp->endbit >= start_bit) {
                IRO_Dump("Overlap detected --2 %d and %d\n", rp->linear->index, nd->index);
                IRO_Dump(
                    "Rp->startbit=%d,Rp->endbit=%d,start_bit=%d,end_bit=%d\n",
                    rp->startbit,
                    rp->endbit,
                    start_bit,
                    end_bit
                );
                conflict = TRUE;
                break;
            }
        }
    }

    if (conflict) {
        DisableEntries(rp);
        if (addend >= 0) {
            return FALSE;
        }
    }

    IRO_Dump("Create Entry at %d\n", nd->index);
    IRO_Dump("start_bit=%d,end_bit=%d\n", start_bit, end_bit);

    rp = oalloc(sizeof(IndirectRecord));
    rp->linear = nd;
    rp->flags = 0;
    rp->var = var;
    rp->addend = addend;
    rp->size = type->size;
    rp->type = type;
    rp->next = NULL;
    rp->matches = oalloc(sizeof(IndirectRecordMatch));
    rp->matches->nd = inner;
    rp->matches->otherMatches = NULL;
    rp->startbit = start_bit;
    rp->endbit = end_bit;

    if (nd->type == IROLinearOp1Arg &&
        nd->nodetype == EINDIRECT &&
        nd->u.monadic->type == IROLinearOp1Arg &&
        nd->u.monadic->nodetype == EBITFIELD) {
        rp->flags |= IRF_Bitfield;
    }

    if (!(IS_TYPE_FLOAT(type) || IS_TYPE_INT(type) || IS_TYPE_POINTER_ONLY(type)) ||
        addend == -1 ||
        ((inner->flags & IROLF_InCallArg) && ObjectIsArg(var->object))) {
        rp->flags |= IRF_Disabled;
    }

    if (IRO_FirstIndirectRecord) {
        IRO_LastIndirectRecord->next = rp;
    } else {
        IRO_FirstIndirectRecord = rp;
    }
    IRO_LastIndirectRecord = rp;
    return TRUE;
}

static void DisableEntries(IndirectRecord* irec) {
    IndirectRecord* scan;

    for (scan = IRO_FirstIndirectRecord; scan; scan = scan->next) {
        if (irec->var->index == scan->var->index) {
            scan->flags |= IRF_Disabled;
        }
    }
}

static UInt32 ObjectIsArg(Object* obj) {
    ObjectList* scan;

    for (scan = arguments; scan; scan = scan->next) {
        if (scan->object == obj) {
            return TRUE;
        }
    }

    return FALSE;
}
#endif
#if MWCCPSP

// 2.4.7's CountUsage (IrOptimizer.c) moved here in 3.0.1 (0x557620): the
// loop-depth weight is 1 << 2*depth, the address-multiply test is the back
// end's hook, and asm operands come from IRO_GetAsmEffectsHook.
static void CountRefToObject(Object* object, int depth) {
    if (depth > 3) {
        depth = 3;
    }
    object->u.var.info->usage += (1 << depth) << depth;
    object->u.var.info->used = TRUE;
}

void IRO_CountARef(IROLinear* node, int depth) {
    Object* object;

    object = node->u.node->data.objref;
    CError_ASSERT(1625, object->datatype != DALIAS);
    if (object->datatype == DLOCAL && object->u.var.info) {
        CountRefToObject(object, depth);
        if ((node->flags & IROLF_Used) && (node->flags & IROLF_Assigned)) {
            CountRefToObject(object, depth);
        }
        if (!(node->flags & IROLF_Immind) && !object->u.var.info->noregister) {
            object->u.var.info->noregister = 2;
        }
    }
}

void IRO_CountDoubleInd(IROLinear* node, int depth) {
    if (IRO_IsVariable(node)) {
        IRO_CountARef(node->u.monadic, depth);
    } else if (node->type == IROLinearOp2Arg) {
        if (node->nodetype == EADD) {
            IRO_CountDoubleInd(node->u.diadic.left, depth);
            IRO_CountDoubleInd(node->u.diadic.right, depth);
        } else if (IRO_IsAddressMultiplyHook && IRO_IsAddressMultiplyHook(node)) {
            if (IRO_IsVariable(node->u.diadic.left)) {
                IRO_CountARef(node->u.diadic.left->u.monadic, depth);
            }
        }
    }
}

static void CountAsm(IROLinear* linear, int depth) {
    IROAsmEffects effects = IRO_EmptyAsmEffects;
    int i;
    Object* object;

    IRO_GetAsmEffectsHook(linear->u.asm_stmt, &effects);
    for (i = 0; i < effects.numoperands; i++) {
        object = effects.operands[i].object;
        if (object->datatype == DLOCAL && object->u.var.info) {
            CountRefToObject(object, depth);
            if (effects.operands[i].type == 3 && !object->u.var.info->noregister) {
                object->u.var.info->noregister = 2;
            }
        }
    }
}

void IRO_CountUsage(void) {
    IRONode* fnode;
    IROLinear* node;

    if (IRO_FirstNode) {
        for (fnode = IRO_FirstNode; fnode; fnode = fnode->nextnode) {
            for (node = fnode->first; node; node = node->next) {
                if (IS_LINEAR_ENODE(node, EOBJREF)) {
                    IRO_CountARef(node, fnode->loopdepth);
                } else if (IS_LINEAR_MONADIC(node, EINDIRECT)) {
                    IRO_CountDoubleInd(node->u.monadic, fnode->loopdepth);
                } else if (node->type == IROLinearAsm &&
                           IRO_GetAsmEffectsHook &&
                           node->u.asm_stmt) {
                    CountAsm(node, fnode->loopdepth);
                }
                if (node == fnode->last) {
                    break;
                }
            }
        }
    } else {
        for (node = IRO_FirstLinear; node; node = node->next) {
            if (IS_LINEAR_ENODE(node, EOBJREF)) {
                IRO_CountARef(node, 0);
            } else if (IS_LINEAR_MONADIC(node, EINDIRECT)) {
                IRO_CountDoubleInd(node->u.monadic, 0);
            } else if (node->type == IROLinearAsm && IRO_GetAsmEffectsHook && node->u.asm_stmt) {
                CountAsm(node, 0);
            }
        }
    }

    IRO_CheckForUserBreak();
}

// 3.0.1 (0x569D90): the sets of variables a store through a pointer may
// kill. IRO_KillAll has every addressed or global variable, IRO_KillWritable
// only those not read-only (or classes with a constructor or destructor),
// and IRO_SizeKills[n] those an n-byte store may reach (sizes capped at
// 0x400; a smaller size includes every larger one). Names not known.
extern BitVector* data_00624DA8;
extern SInt32 IRO_MaxKillSize;
extern BitVector* IRO_SizeKills[0x401];
extern BitVector* IRO_KillAll;
extern BitVector* IRO_KillWritable;

static Boolean MayBeWritten(Object* obj) {
    return !CParser_IsReadOnly(obj->type, obj->qual) ||
        (IS_TYPE_CLASS(obj->type) &&
         (CClass_Constructor(TYPE_CLASS(obj->type)) || CClass_Destructor(TYPE_CLASS(obj->type))));
}

static void NeedVector(BitVector** bv) {
    if (!*bv) {
        Bv_AllocVector(bv, IRO_NumVars + 1);
        Bv_SetBit(0, *bv);
    }
}

void IRO_ComputeKillSets(void) {
    VarRecord* var;
    Object* obj;
    SInt32 size;
    int i;

    Bv_AllocVector(&data_00624DA8, IRO_NumVars + 1);
    IRO_MaxKillSize = 0;
    for (var = IRO_FirstVar; var; var = var->next) {
        obj = var->object;
        if (var->noregister || obj->datatype == DDATA || obj->datatype == DABSOLUTE) {
            if (MayBeWritten(obj)) {
                size = obj->type->size;
                if (size > 0x400) {
                    size = 0x400;
                }
                if (IRO_MaxKillSize < size) {
                    IRO_MaxKillSize = size;
                }
            }
        }
    }

    IRO_KillAll = NULL;
    IRO_KillWritable = NULL;
    for (i = 0; i < 0x401; i++) {
        IRO_SizeKills[i] = NULL;
    }

    for (var = IRO_FirstVar; var; var = var->next) {
        obj = var->object;
        if (var->noregister || obj->datatype == DDATA || obj->datatype == DABSOLUTE) {
            NeedVector(&IRO_KillAll);
            Bv_SetBit(var->index, IRO_KillAll);
            if (MayBeWritten(obj)) {
                NeedVector(&IRO_KillWritable);
                Bv_SetBit(var->index, IRO_KillWritable);
                if (!var->noregister) {
                    Boolean isGlobal = (obj->datatype == DDATA || obj->datatype == DABSOLUTE) &&
                        obj->sclass != TK_STATIC;
                    if (!isGlobal &&
                        (!(obj->datatype == DDATA || obj->datatype == DABSOLUTE) ||
                         obj->sclass != TK_STATIC ||
                         obj->u.data.islocalstatic)) {
                        continue;
                    }
                }
                size = obj->type->size;
                if (size > 0x400) {
                    size = 0x400;
                }
                NeedVector(&IRO_SizeKills[size]);
                Bv_SetBit(var->index, IRO_SizeKills[size]);
            }
        }
    }

    NeedVector(&IRO_KillAll);
    NeedVector(&IRO_KillWritable);
    NeedVector(&IRO_SizeKills[IRO_MaxKillSize]);
    for (i = IRO_MaxKillSize - 1; i >= 0; i--) {
        if (!IRO_SizeKills[i]) {
            IRO_SizeKills[i] = IRO_SizeKills[i + 1];
        } else {
            Bv_Or(IRO_SizeKills[i + 1], IRO_SizeKills[i]);
        }
    }
}

#endif
#if MWCCPSP
// 3.0.1 (0x558850): the object reference a store goes to (through a bitfield
// or an address with one object), else NULL
IROLinear* IRO_FindAssignedObjRef(IROLinear* linear, Boolean* isBitfield) {
    IROLinear* objref;
    int numObjRefs;
    int numOther;

    if (isBitfield) {
        *isBitfield = FALSE;
    }
    if (IS_LINEAR_MONADIC(linear, EBITFIELD)) {
        if (isBitfield) {
            *isBitfield = TRUE;
        }
        linear = linear->u.monadic;
    }
    if (IS_LINEAR_DIADIC(linear, EADD)) {
        IRO_FindAddressObjRefs(linear, &objref, &numObjRefs, &numOther);
        if (numObjRefs == 1) {
            linear = objref;
        }
    }
    if (IS_LINEAR_ENODE(linear, EOBJREF)) {
        return linear;
    }
    return NULL;
}

// 0x5587B0: the variable an assignment stores to
VarRecord* IRO_FindAssigned301(IROLinear* linear, Boolean* isBitfield) {
    IROLinear* dst;
    IROLinear* ref;

    if (isBitfield) {
        *isBitfield = FALSE;
    }
    if (!linear) {
        ref = NULL;
    } else {
        dst = linear->u.monadic;
        if (IS_LINEAR_MONADIC(dst, EINDIRECT)) {
            dst = dst->u.monadic;
        }
        ref = IRO_FindAssignedObjRef(dst, isBitfield);
    }
    if (ref) {
        return IRO_FindVar(ref->u.node->data.objref, FALSE, TRUE);
    }
    return NULL;
}

// 0x558B50
void IRO_ZapVarPtrs301(void) {
    VarRecord* var;

    for (var = IRO_FirstVar; var; var = var->next) {
        var->object->varptr = NULL;
    }
}
#endif
