// mwccps2 b210/b198's IroLoopReversal.c: the parts of loop reversal placed so far. The
// pass itself (IRO_LoopReversal, b198 0x5ABEB0) is not yet decompiled.
#include "compiler/mwcc_core.h"
#include "compiler/common.h"
#include "compiler/enode.h"
#include "compiler/IroDump.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroLoop301.h"
#include "compiler/IroUtil.h"

#if MWCCPS2 && !MWCC_CORE
extern void IRO_BuildInOutLists(
    IRONaturalLoop* loop, UInt32 mode, int* count
); // IroDependenceAnalysis.c
extern Boolean IRO_LoopHasStrongStoreLoadDependencies(int a, int b, int c);
extern Boolean IRO_LoopHasStrongStoreStoreDependencies(int a, int b, int c);
extern void IRO_FreeLoopAssignmentList(void);
extern IROLinear* func_00588A10(IRONaturalLoop* loop, void* induction); // the induction's start

// b198 0x5AC0A0: a loop whose stores do not depend on each other or on its loads
Boolean IRO_LoopIsReversible(IRONaturalLoop* loop) {
    int count;
    Boolean passes = TRUE;

    IRO_BuildInOutLists(loop, FALSE, &count);
    if (count <= 0) {
        IRO_Dump("LoadsAndStoresPass:No because loop body is empty\n");
        passes = FALSE;
    } else if (IRO_LoopHasStrongStoreLoadDependencies(0, 0, 1)) {
        IRO_Dump("LoadsAndStoresPass:No because loop has strong store load dependencies\n");
        passes = FALSE;
    } else if (IRO_LoopHasStrongStoreStoreDependencies(0, 0, 1)) {
        IRO_Dump("LoadsAndStoresPass:No because loop has strong store store dependencies\n");
        passes = FALSE;
    }
    IRO_FreeLoopAssignmentList();
    return passes;
}

// b198 0x5AC460: the reversed loop tests the induction against its old start
void IRO_ReverseLoopTest(IRONaturalLoop* loop) {
    IROLinear* cond;
    IROLinear* upper;
    IROLinear* lower;
    IROList list;

    cond = loop->cond;
    upper = func_00588900(loop);
    lower = func_00588A10(loop, loop->induction);
    if (cond->nodetype != ENOTEQU) {
        cond->nodetype = EGREATEREQU;
    }
    IRO_InitList(&list);
    IRO_DuplicateExpr(lower, &list);
    IRO_Paste(list.head, list.tail, cond);
    IRO_LocateFather_Cut_And_Paste(upper, list.tail);
}
#endif
