// mwccpsp 3.0.1: IRO_RewriteBitFieldTemps (0x5813B0; IroScalarization.c by
// its assert). Replaces IroVars.c's 2.4.7 body, which is guarded out under
// MWCCPSP. As 2.4.7, but the integer constant flags are refreshed afterwards.
#include "compiler/common.h"
#include "compiler/CError.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroUtil.h"
#include "compiler/IroVars.h"

#if MWCCPSP && !MWCCPS2 // b210: IroVars.c (MWCCPS2) has it
void IRO_RewriteBitFieldTemps(void) {
    IROLinear* nd;
    IROLinear* replacement;
    Object* obj;
    VarRecord* var;
    IROList list;

    for (nd = IRO_FirstLinear; nd; nd = nd->next) {
        if ((obj = IRO_IsVariable(nd)) && (nd->flags & IROLF_BitfieldIndirect)) {
            var = IRO_FindVar(obj, FALSE, TRUE);
            if (!var) {
                CError_Internal("IroScalarization.c", CError_B145_LINE(197));
            }

            replacement = var->bitFieldReplacement;
            if (!replacement) {
                CError_Internal("IroScalarization.c", CError_B145_LINE(203));
            }

            IRO_InitList(&list);
            IRO_DuplicateExpr(replacement, &list);
            IRO_NopOut(nd->u.monadic);
            nd->u.monadic = list.tail;
            IRO_Paste(list.head, list.tail, nd);
        }
    }

    IRO_UpdateFlagsOnInts(IRO_FirstLinear);
}
#endif
