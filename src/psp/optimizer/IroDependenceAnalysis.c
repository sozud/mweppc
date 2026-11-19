// mwccps2 b210/b198's IroDependenceAnalysis.c: the loop dependence analysis loop
// reversal asks. Only IRO_BuildInOutLists is placed so far; the assignment records it
// collects (b198 0x5B6B50) and the dependence tests are not yet decompiled.
#include "compiler/mwcc_core.h"
#include "compiler/common.h"
#include "compiler/BitVector.h"
#include "compiler/IroFlowgraph.h"
#include "compiler/IroLinearForm.h"
#include "compiler/IroLoop301.h"
#include "compiler/IroUtil.h"

#if MWCCPS2 && !MWCC_CORE
typedef struct DependenceRecord {
    UInt8 pad[0x18];
    void* result; // +0x18
} DependenceRecord;

UInt32 IRO_DependenceFlags; // b198 0x620638
UInt32 IRO_DependenceNumAssignments; // 0x62063C
IRONaturalLoop* IRO_DependenceLoop; // 0x620640
IRONode* IRO_DependenceCurrentNode; // 0x620644
UInt32 IRO_DependenceMode; // 0x620648
void* IRO_LoopAssignmentListHead; // 0x635B08
DependenceRecord* IRO_DependenceHeadRecord; // 0x6361BC
void* IRO_DependenceResult; // 0x636280

extern void ps2_dep_assignment_5B6B50(IROLinear* nd); // records an assignment

// b198 0x5B6A70: the loop's assignments that are statements (not the induction update)
void IRO_BuildInOutLists(IRONaturalLoop* loop, UInt32 mode, int* count) {
    IRONode* node;
    IROLinear* nd;

    IRO_DependenceFlags = 0;
    IRO_DependenceLoop = loop;
    IRO_DependenceMode = mode;
    node = IRO_FirstNode;
    IRO_LoopAssignmentListHead = NULL;
    IRO_DependenceNumAssignments = 0;
    for (; node; node = node->nextnode) {
        if (Bv_IsBitSetFn(node->index, loop->nodes)) {
            IRO_DependenceCurrentNode = node;
            for (nd = node->first; nd; nd = nd->next) {
                if (!(nd->flags & IROLF_Reffed) &&
                    nd != ((IROLoopInd301*)loop->induction)->nd &&
                    IRO_IsAssignment(nd)) {
                    ps2_dep_assignment_5B6B50(nd);
                }
                if (nd == node->last) {
                    break;
                }
            }
        }
    }
    if (IRO_LoopAssignmentListHead) {
        IRO_DependenceResult = IRO_DependenceHeadRecord->result;
    } else {
        IRO_DependenceResult = NULL;
    }
    *count = IRO_DependenceNumAssignments;
}
#endif
