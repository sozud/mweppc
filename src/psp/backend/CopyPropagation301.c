// mwccpsp 3.0.1: the propagation framework of the MIPS back end
// (propagateinstructions, 0x5452C0..0x545D90). 2.4.7's CopyPropagation.c is
// the template; 3.0.1 runs it on MPCode with UseDefChains301.c's chains and
// keeps its state at fixed addresses.
#include "compiler/common.h"
#include "compiler/BitVectors.h"
#include "compiler/CMangler.h"
#include "compiler/CParser.h"
#include "compiler/CompilerTools.h"
#include "compiler/objects.h"
#include "psp/BE.h"
#include "psp/UseDef301.h"
#include <stdio.h>

typedef struct Candidate301 {
    MPCode* pcode;
    RegUseOrDef301* list;
} Candidate301;

typedef struct PropInfo301 {
    UInt32* gen;
    UInt32* kill;
    UInt32* in;
    UInt32* out;
} PropInfo301;

typedef int (*IsCandidateFunc301)(MPCode* pcode);
typedef int (*PropagatesToUseFunc301)(int candidateID, int useID);
typedef void (*PropagateAndFinishFunc301)(int candidateID);

typedef struct Propagation301 {
    IsCandidateFunc301 iscandidate; // +0
    PropagatesToUseFunc301 touse; // +4
    PropagateAndFinishFunc301 finish; // +8
    char* name; // +0xC
    char* dumpname; // +0x10
    char* nodumpname; // +0x14
    UInt32 candmask; // +0x18
    Boolean computesUseDefChains; // +0x1C
} Propagation301;

IsCandidateFunc301 data_00607078; // is_candidate
PropagatesToUseFunc301 data_0060707C; // propagatestouse
PropagateAndFinishFunc301 data_00607080; // propagateandfinish
UInt32 data_00607084; // cp_mask
int data_00624584; // propagated_instructions
int data_00625020; // recursive_propagation
int data_00624D68; // number_of_candidates
Candidate301* data_00625348; // Candidates
PropInfo301* data_00624B9C; // propinfo
int* data_006258EC; // ncandidatesinblock
int* data_00624DE4; // firstcandidateinblock

#define is_candidate data_00607078
#define propagatestouse data_0060707C
#define propagateandfinish data_00607080
#define cp_mask data_00607084
#define propagated_instructions data_00624584
#define recursive_propagation data_00625020
#define number_of_candidates data_00624D68
#define Candidates data_00625348
#define propinfo data_00624B9C
#define ncandidatesinblock data_006258EC
#define firstcandidateinblock data_00624DE4

extern SInt32 data_00624D40; // number of blocks
extern MPCodeBlock** data_00625418; // depthfirstordering
extern void func_0053C140(int includeMemory); // computeusedefchains
extern void computedepthfirstordering(void);
extern void BE_DumpPCode(char* name, char* title);

#define SETBIT(bit, vec) ((vec)[(bit) >> 5] |= 1u << ((bit) & 31))
#define CLEARBIT(bit, vec) ((vec)[(bit) >> 5] &= ~(1u << ((bit) & 31)))
#define GETBIT(bit, vec) (((vec)[(bit) >> 5] >> ((bit) & 31)) & 1)

// 0x545CF0
void func_00545CF0(void) {
    MPCodeBlock* block;
    MPCode* pcode;
    int count;

    ncandidatesinblock = oalloc(sizeof(int) * data_00624D40);
    firstcandidateinblock = oalloc(sizeof(int) * data_00624D40);
    number_of_candidates = 0;
    for (block = data_00624FD0; block; block = block->next) {
        firstcandidateinblock[block->number] = number_of_candidates;
        count = 0;
        for (pcode = block->first; pcode; pcode = pcode->next) {
            if (is_candidate(pcode)) {
                number_of_candidates++;
                count++;
            }
        }
        ncandidatesinblock[block->number] = count;
    }
}

// 0x545AB0
void func_00545AB0(void) {
    RegUseOrDef301* list;
    RegUseOrDef301* newlist;
    MPCodeBlock* block;
    MPCode* pcode;
    Candidate301* candidate;
    UseOrDef301* def;
    UseOrDef301* use;
    UInt32* vec;
    int reg;
    int defID;
    int useID;

    Candidates = oalloc(sizeof(Candidate301) * number_of_candidates);
    vec = oalloc(4 * ((data_00625AB8 + 31) >> 5));
    for (block = data_00624FD0; block; block = block->next) {
        if (!ncandidatesinblock[block->number]) {
            continue;
        }
        bitvectorcopy(vec, data_00625988[block->number].useout, data_00625AB8);
        candidate =
            &Candidates
                [firstcandidateinblock[block->number] + ncandidatesinblock[block->number] - 1];
        for (pcode = block->last; pcode; pcode = pcode->prev) {
            if (!pcode->argcount) {
                continue;
            }
            if (is_candidate(pcode)) {
                reg = pcode->args[0].reg;
                candidate->pcode = pcode;
                candidate->list = NULL;
                for (list = data_006247F4[pcode->args[0].rclass][reg]; list; list = list->next) {
                    if (GETBIT(list->id, vec)) {
                        newlist = oalloc(sizeof(RegUseOrDef301));
                        newlist->id = list->id;
                        newlist->next = candidate->list;
                        candidate->list = newlist;
                    }
                }
                candidate--;
            }
            for (def = &data_00624B58[defID = pcode->defID];
                 defID < data_00625374 && def->pcode == pcode;
                 def++, defID++) {
                if (def->kind == 0) {
                    for (list = data_006247F4[def->rclass][def->reg]; list; list = list->next) {
                        CLEARBIT(list->id, vec);
                    }
                }
            }
            for (use = &data_00624768[useID = pcode->useID];
                 useID < data_00625AB8 && use->pcode == pcode;
                 use++, useID++) {
                if (use->kind == 0) {
                    SETBIT(useID, vec);
                }
            }
        }
    }
}

// a write of op kills the candidates that use or define its register
static inline void killcandidates(
    MPCodeBlock* block, MOperand* op, UInt32* gen, UInt32* kill, Boolean local
) {
    Candidate301* candidate;
    MOperand* candOp;
    int cndi;
    int j;

    for (cndi = 0, candidate = Candidates; number_of_candidates > cndi; cndi++, candidate++) {
        for (j = 0; j < candidate->pcode->argcount; j++) {
            candOp = candidate->pcode->args + j;
            if (candOp->kind == 0 && (local ? candOp->rclass == 0 : candOp->rclass == op->rclass)) {
                if (candOp->reg == op->reg) {
                    if (candidate->pcode->block == block) {
                        CLEARBIT(cndi, gen);
                    } else {
                        SETBIT(cndi, kill);
                    }
                    break;
                }
            } else if (!local && j == 0) {
                break;
            }
        }
    }
}

// 0x545720
void func_00545720(Boolean localflag) {
    PropInfo301* info;
    MPCode* pcode;
    MPCodeBlock* block;
    MOperand* op;
    UInt32* gen;
    UInt32* kill;
    int index;
    int i;

    for (block = data_00624FD0; block; block = block->next) {
        info = &propinfo[block->number];
        gen = info->gen;
        kill = info->kill;
        bitvectorinitialize(gen, number_of_candidates, 0);
        bitvectorinitialize(kill, number_of_candidates, 0);
        index = firstcandidateinblock[block->number];
        for (pcode = block->first; pcode; pcode = pcode->next) {
            if (!pcode->argcount) {
                continue;
            }
            op = pcode->args;
            i = pcode->argcount;
            while (i--) {
                if (localflag) {
                    if (op->kind == 0 && op->rclass == 0 && (op->flags & 2)) {
                        killcandidates(block, op, gen, kill, TRUE);
                    }
                } else if (op->kind == 0 && (op->flags & 2)) {
                    killcandidates(block, op, gen, kill, FALSE);
                }
                op++;
            }
            if (is_candidate(pcode)) {
                SETBIT(index, gen);
                index++;
            }
        }
    }
}

// 0x545510
void func_00545510(void) {
    MPBlockLink* preds;
    PropInfo301* info;
    MPCodeBlock* block;
    UInt32* gen;
    UInt32* kill;
    UInt32* in;
    UInt32* out;
    UInt32 val;
    int bitvecsize;
    int blockIndex;
    int changed;
    int i;

    bitvecsize = (number_of_candidates + 31) >> 5;
    changed = TRUE;
    info = &propinfo[data_00624FD0->number];
    bitvectorinitialize(info->in, number_of_candidates, 0);
    bitvectorcopy(info->out, info->gen, number_of_candidates);
    for (block = data_00624FD0->next; block; block = block->next) {
        info = &propinfo[block->number];
        out = info->out;
        kill = info->kill;
        for (i = 0; i < bitvecsize; out++, kill++, i++) {
            *out = ~*kill;
        }
    }
    while (changed) {
        changed = FALSE;
        for (blockIndex = 0; blockIndex < data_00624D40; blockIndex++) {
            if ((block = data_00625418[blockIndex])) {
                info = &propinfo[block->number];
                if ((preds = block->predecessors)) {
                    in = info->in;
                    bitvectorcopy(in, propinfo[preds->from->number].out, number_of_candidates);
                    // 3.0.1 starts the intersection again from the first edge
                    for (preds = block->predecessors; preds; preds = preds->nextpred) {
                        bitvectorintersect(
                            in, propinfo[preds->from->number].out, number_of_candidates
                        );
                    }
                }
                out = info->out;
                in = info->in;
                gen = info->gen;
                kill = info->kill;
                for (i = 0; i < bitvecsize; i++) {
                    val = *gen | (*in & ~*kill);
                    if (val != *out) {
                        *out = val;
                        changed = TRUE;
                    }
                    in++;
                    out++;
                    kill++;
                    gen++;
                }
            }
        }
    }
}

// 0x5452C0: propagateinstructions
void func_005452C0(Object* proc, Propagation301* config, int passCount, Boolean localflag) {
    PropInfo301* info;
    RegUseOrDef301* list;
    MPCode* pcode;
    char buf[52];
    UInt32 flags;
    int ok;
    int i;

    is_candidate = config->iscandidate;
    propagatestouse = config->touse;
    propagateandfinish = config->finish;
    cp_mask = config->candmask;
    propagated_instructions = FALSE;
    while (1) {
        recursive_propagation = FALSE;
        func_00545CF0();
        if (number_of_candidates <= 0) {
            freeoheap();
            if (config->computesUseDefChains) {
                func_0053C140(0);
            }
            return;
        }
        func_0053C140(0);
        func_00545AB0();
        propinfo = oalloc(sizeof(PropInfo301) * data_00624D40);
        for (i = 0, info = propinfo; i < data_00624D40; i++, info++) {
            info->gen = oalloc(4 * ((number_of_candidates + 31) >> 5));
            info->kill = oalloc(4 * ((number_of_candidates + 31) >> 5));
            info->in = oalloc(4 * ((number_of_candidates + 31) >> 5));
            info->out = oalloc(4 * ((number_of_candidates + 31) >> 5));
        }
        func_00545720(localflag);
        computedepthfirstordering();
        func_00545510();
        for (i = 0; i < number_of_candidates; i++) {
            pcode = Candidates[i].pcode;
            ok = TRUE;
            if (cp_mask) {
                flags = (pcode->flags & 9) ? 0 : pcode->flags;
                if (cp_mask & flags) {
                    ok = FALSE;
                }
            }
            if (ok) {
                for (list = Candidates[i].list; list; list = list->next) {
                    if (!propagatestouse(i, list->id)) {
                        ok = FALSE;
                        break;
                    }
                }
            }
            if (ok) {
                propagateandfinish(i);
            }
        }
        passCount--;
        if (propagated_instructions && copts.debuglisting) {
            sprintf(buf, "AFTER %s PROPAGATION", config->name);
            BE_DumpPCode(CMangler_GetLinkName(proc)->name, buf);
        }
        if (!passCount || !recursive_propagation) {
            if (!config->computesUseDefChains) {
                freeoheap();
            }
            return;
        }
        freeoheap();
    }
}

// --- copy propagation ------------------------------------------------------

int data_00607088; // moreaggressiveoptimization
int data_00624D4C; // propagatedcopies

extern int func_0053BDF0(
    MPCode* candidate, int defID, int reg, MPCode* use, UInt32* defvec, char rclass
);
extern SInt32 data_00624744[7]; // the first virtual register of each class

// 0x545280: is_copy
int func_00545280(MPCode* pcode) {
    return (pcode->flags & 0x10) &&
        pcode->args[0].reg >= data_00624744[pcode->args[0].rclass] &&
        pcode->args[0].kind == 0 &&
        pcode->args[0].rclass == 0;
}

// 0x545170: copypropagatestouse
int func_00545170(int candidateID, int useID) {
    MPCode* pcode = Candidates[candidateID].pcode;
    MPCode* use = data_00624768[useID].pcode;
    char rclass = pcode->args[0].rclass;
    int destReg = pcode->args[0].reg;
    int srcReg = pcode->args[1].reg;
    MOperand* op;
    int i;

    if (use->flags & 0x10) {
        return FALSE;
    }
    for (op = use->args, i = use->argcount; i--; op++) {
        if (op->kind == 0 &&
            op->rclass == rclass &&
            op->reg == destReg &&
            ((op->flags & 3) == 3 || (op->flags & 8))) {
            return FALSE;
        }
    }
    // 3.0.1 asks canidate_reaches_use, with the candidate's own index into
    // the propagation sets
    return func_0053BDF0(
               pcode, candidateID, srcReg, use, propinfo[use->block->number].in, rclass
           ) != 0;
}

// 0x5450B0: propagateandremovecopy
void func_005450B0(int id) {
    Candidate301* candidate = &Candidates[id];
    RegUseOrDef301* list;
    MOperand* op;
    MPCode* use;
    char rclass = candidate->pcode->args[0].rclass;
    int destReg = candidate->pcode->args[0].reg;
    int srcReg = candidate->pcode->args[1].reg;
    int i;

    if (!data_00607088 && srcReg < data_00624744[rclass]) {
        return;
    }
    for (list = candidate->list; list; list = list->next) {
        use = data_00624768[list->id].pcode;
        for (op = use->args, i = use->argcount; i--; op++) {
            if (op->kind == 0 && op->rclass == rclass && op->reg == destReg && (op->flags & 1)) {
                op->reg = srcReg;
            }
        }
    }
    deletepcode(candidate->pcode);
    propagated_instructions = TRUE;
}

static Propagation301 copy_prop301 = {
    func_00545280, func_00545170, func_005450B0, "COPY", "COPIES", "c%ld", 0x180, FALSE
};

// 0x545080: propagatecopyinstructions
void func_00545080(Object* proc, int moreAggressive) {
    data_00607088 = moreAggressive;
    func_005452C0(proc, &copy_prop301, 1, TRUE);
    data_00624D4C = propagated_instructions;
}
