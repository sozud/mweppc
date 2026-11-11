#ifndef COMPILER_IROFLOWGRAPH_H
#define COMPILER_IROFLOWGRAPH_H

#include "compiler/common.h"
#include "compiler/CError.h"
#include "compiler/CompilerTools.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !MWCCPSP
#pragma pack(push, 2)
#endif
#if MWCCPSP
// 3.0.1: 0x44 bytes, natural alignment (StartNode 0x577BC0 and
// IRO_FreeFlowGraphNode 0x576A70); the loop depth moved up, 2.4.7's unused
// word is gone, and a node holding an asm optimization barrier is marked.
struct IRONode {
    UInt16 index; // +0
    UInt16 numsucc; // +2
    UInt16* succ; // +4
    UInt16 numpred; // +8
    UInt16* pred; // +0xC
    UInt16 loopdepth; // +0x10
    IROLinear* first; // +0x14
    IROLinear* last; // +0x18
    BitVector* in; // +0x1C
    BitVector* out; // +0x20
    BitVector* gen; // +0x24
    BitVector* kill; // +0x28
    BitVector* copyOut; // +0x2C
    BitVector* dom; // +0x30
    IRONode* nextnode; // +0x34
    struct ObjectSet* addressed; // +0x38
    Boolean reachable; // +0x3C
    Boolean visited; // +0x3D
    Boolean mustreach; // +0x3E
    Boolean unk3F; // new in 3.0.1
    Boolean referenced; // +0x40 (IRO_ComputeSuccPred)
    Boolean marked; // +0x41
    Boolean mustreach1; // +0x42
    Boolean hasAsmBarrier; // +0x43 (IRO_FindAsmBarriers 0x576550)
};
#else
struct IRONode {
    UInt16 index;
    UInt16 numsucc;
    UInt16* succ;
    UInt16 numpred;
    UInt16* pred;
    IROLinear* first;
    IROLinear* last;
    BitVector* in;
    BitVector* out;
    BitVector* gen;
    BitVector* kill;
    UInt32 unused;
    BitVector* copyOut;
    BitVector* dom;
    IRONode* nextnode;
    Boolean reachable;
    Boolean visited;
    Boolean mustreach;
    Boolean referenced;
    UInt16 loopdepth;
    Boolean marked;
    struct ObjectSet* addressed;
    Boolean mustreach1;
};
#endif

typedef struct IRONodes {
    UInt16* indices;
    UInt16 num;
    UInt16 base;
} IRONodes;
#if !MWCCPSP
#pragma pack(pop)
#endif

extern UInt16 IRO_NumNodes;
extern IRONode* IRO_FirstNode;
extern IRONode* IRO_LastNode;
extern IRONode* IRO_EndNode;
extern IRONode** IRO_NodeTable;
extern BitVector* IRO_VarKills;
extern BitVector* IRO_Avail;
extern BitVector* IRO_FuncKills;
extern BitVector* IRO_ExprKills;

extern void IRO_BuildFlowgraph(IROLinear* linear);
#if MWCCPSP
extern void IRO_RebuildNodeTable(void);
#endif
extern void IRO_ComputeDom(void);
extern void IRO_ComputeSuccPred(void);
extern IRONode* IRO_NewFlowGraphNode(void);
extern IRONode* IRO_MergeFlowGraphNodes(IRONode* a, IRONode* b);

HEADER_INLINE void IRO_FNodeWorkStack_Init(IRONodes* nodes) {
    nodes->indices = oalloc(sizeof(UInt16) * IRO_NumNodes);
    nodes->num = 0;
    nodes->base = 0;
}

HEADER_INLINE void IRO_FNodeWorkStack_Free(IRONodes* nodes) {
    (void)nodes;
}

HEADER_INLINE UInt16 IRO_FNodeWorkStack_Count(IRONodes* nodes) {
    return nodes->num;
}

HEADER_INLINE UInt16 IRO_FNodeWorkStack_Remove(IRONodes* nodes) {
    UInt16 result = -1;
    if (nodes->num) {
        result = nodes->indices[nodes->base];
        nodes->base = (nodes->base + 1) % IRO_NumNodes;
        nodes->num--;
    }
    return result;
}

HEADER_INLINE void IRO_FNodeWorkStack_Add(IRONodes* nodes, UInt16 index) {
    if (nodes->num < IRO_NumNodes) {
        nodes->indices[(nodes->base + nodes->num) % IRO_NumNodes] = index;
        nodes->num++;
    } else {
        CError_FATAL(93);
    }
}

#ifdef __cplusplus
}
#endif

#endif
