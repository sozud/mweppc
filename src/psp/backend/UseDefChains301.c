// mwccpsp 3.0.1: use-def chains on the MIPS back end's pcode
// (computeusedefchains, 0x53C140..0x53D5D0). 2.4.7's UseDefChains.c (its
// GC 2.5 branches: compressed bit vectors, every register class) is the
// template; 3.0.1 runs it on MPCode and keeps its state at fixed addresses
// that the rest of the original back end reads.
#include "compiler/common.h"
#include "compiler/Alias.h"
#include "compiler/BitVectors.h"
#include "compiler/CompilerTools.h"
#include "compiler/objects.h"
#include "psp/BE.h"

#include "psp/UseDef301.h"

int data_00625374; // number_of_Defs
int data_00625AB8; // number_of_Uses
UseOrDef301* data_00624B58; // Defs
UseOrDef301* data_00624768; // Uses
RegUseOrDef301** data_00624BFC[7]; // reg_Defs
RegUseOrDef301** data_006247F4[7]; // reg_Uses
UseDefInfo301* data_00625988; // usedefinfo
ObjectUseDef301* data_00625014; // objectusedefs (a binary tree)
ObjectUseDef301* data_00624780; // objectusedeflist
SInt32 data_00624728[7]; // last_usedef_register

#define number_of_Defs data_00625374
#define number_of_Uses data_00625AB8
#define Defs data_00624B58
#define Uses data_00624768
#define reg_Defs data_00624BFC
#define reg_Uses data_006247F4
#define usedefinfo data_00625988
#define objectusedefs data_00625014
#define objectusedeflist data_00624780
#define last_usedef_register data_00624728

extern SInt32 data_00624744[7]; // the first virtual register of each class
extern SInt32 data_00625A98[7]; // the registers used in each class
extern SInt32 data_00624D40; // number of blocks
extern MPCodeBlock** data_00625418; // depthfirstordering
extern Alias* worst_case_ptr;
extern Alias* worst_case_func;
extern Object worst_case_obj;
extern void gather_alias_info(void);
extern void computedepthfirstordering(void);
extern SInt32 func_0048FBE0(UInt16 op); // bytes an opcode moves
extern Boolean may_alias_object(PCode* pcode, Object* object);
extern Boolean must_alias_object(PCode* pcode, Object* object);

static inline ObjectUseDef301* findobjectusedef(Object* object) {
    ObjectUseDef301* oud = objectusedefs;

    while (oud) {
        if (object < oud->object) {
            oud = oud->leftchild;
        } else if (object > oud->object) {
            oud = oud->rightchild;
        } else {
            return oud;
        }
    }
    return NULL;
}

static inline void addobjectusedef(Object* object) {
    ObjectUseDef301* list;
    ObjectUseDef301** ptr;

    ptr = &objectusedefs;
    while ((list = *ptr)) {
        if (object < list->object) {
            ptr = &list->leftchild;
        } else if (object > list->object) {
            ptr = &list->rightchild;
        } else {
            return;
        }
    }
    list = oalloc(sizeof(ObjectUseDef301));
    memclrw(list, sizeof(ObjectUseDef301));
    list->object = object;
    list->next = objectusedeflist;
    objectusedeflist = list;
    *ptr = list;
}

// 0x53D420
void func_0053D420(Alias* alias) {
    AliasMember* member;

    if (alias && alias != worst_case_ptr && alias != worst_case_func) {
        if (alias->type == AliasType_Set) {
            for (member = alias->parents; member; member = member->nextParent) {
                func_0053D420(member->child);
            }
        } else {
            addobjectusedef(alias->object);
        }
    }
}

// 0x53D330: the objects memory instructions use or define
void func_0053D330(void) {
    MPCodeBlock* block;
    MPCode* pcode;

    objectusedeflist = NULL;
    objectusedefs = objectusedeflist;
    for (block = data_00624FD0; block; block = block->next) {
        for (pcode = block->first; pcode; pcode = pcode->next) {
            if (pcode->flags & 6) {
                if (pcode->alias) {
                    if ((pcode->alias->type != AliasType_ObjectPart &&
                         pcode->alias->type != AliasType_Object) ||
                        pcode->alias->size != func_0048FBE0(pcode->op)) {
                        pcode->flags |= 0x20;
                    }
                    func_0053D420(pcode->alias);
                } else {
                    pcode->flags |= 0x20;
                }
            }
        }
    }
    addobjectusedef(&worst_case_obj);
}

// an indirect access through pcode's alias
static inline Boolean IsIndirect301(MPCode* pcode) {
    return pcode->alias->type != AliasType_ObjectPart && pcode->alias->type != AliasType_Object;
}

// 0x53D0B0
void func_0053D0B0(int includeMemory) {
    MPCodeBlock* block;
    MPCode* pcode;
    MOperand* op;
    ObjectUseDef301* oud;
    UseDefInfo301* info;
    int i;

    number_of_Defs = 0;
    number_of_Uses = number_of_Defs;
    usedefinfo = oalloc(data_00624D40 * sizeof(UseDefInfo301));
    memclrw(usedefinfo, data_00624D40 * sizeof(UseDefInfo301));
    for (block = data_00624FD0; block; block = block->next) {
        usedefinfo[block->number].usestart = number_of_Uses;
        usedefinfo[block->number].defstart = number_of_Defs;
        for (pcode = block->first; pcode; pcode = pcode->next) {
            if (pcode->argcount) {
                pcode->useID = number_of_Uses;
                pcode->defID = number_of_Defs;
                op = pcode->args;
                for (i = pcode->argcount; i--; op++) {
                    if (op->kind == 0 && op->reg >= data_00624744[op->rclass]) {
                        if (op->flags & 1) {
                            number_of_Uses++;
                        }
                        if (op->flags & 2) {
                            number_of_Defs++;
                        }
                    }
                }
                if (includeMemory) {
                    if (pcode->flags & 2) {
                        if (IsIndirect301(pcode)) {
                            for (oud = objectusedeflist; oud; oud = oud->next) {
                                if (may_alias_object((PCode*)pcode, oud->object)) {
                                    number_of_Uses++;
                                }
                            }
                        } else {
                            number_of_Uses++;
                        }
                    } else if (pcode->flags & 4) {
                        if (IsIndirect301(pcode)) {
                            for (oud = objectusedeflist; oud; oud = oud->next) {
                                if (may_alias_object((PCode*)pcode, oud->object)) {
                                    number_of_Defs++;
                                }
                            }
                        } else {
                            number_of_Defs++;
                        }
                    } else if (pcode->flags & 8) {
                        for (oud = objectusedeflist; oud; oud = oud->next) {
                            if (may_alias_object((PCode*)pcode, oud->object)) {
                                number_of_Uses++;
                                number_of_Defs++;
                            }
                        }
                    }
                }
            }
        }
        info = &usedefinfo[block->number];
        info->usecount = number_of_Uses - info->usestart;
        info = &usedefinfo[block->number];
        info->defcount = number_of_Defs - info->defstart;
    }
}

static inline void addreglist(RegUseOrDef301** head, int id) {
    RegUseOrDef301* list = oalloc(sizeof(RegUseOrDef301));
    list->id = id;
    list->next = *head;
    *head = list;
}

static inline void setmemory(UseOrDef301* ud, MPCode* pcode, char arg, Object* object) {
    ud->pcode = pcode;
    ud->kind = 4;
    ud->rclass = arg;
    ud->object = object;
}

// 0x53CB70
void func_0053CB70(int includeMemory) {
    ObjectUseDef301* oud;
    ObjectUseDef301* objUseDef;
    MPCodeBlock* block;
    MPCode* pcode;
    MOperand* op;
    UseOrDef301* use;
    UseOrDef301* def;
    Object* object;
    int useID;
    int defID;
    int rclass;
    int i;

    Uses = oalloc(sizeof(UseOrDef301) * number_of_Uses);
    Defs = oalloc(sizeof(UseOrDef301) * number_of_Defs);
    for (rclass = 0; rclass < 7; rclass++) {
        reg_Uses[rclass] = oalloc(sizeof(RegUseOrDef301*) * data_00625A98[rclass]);
        reg_Defs[rclass] = oalloc(sizeof(RegUseOrDef301*) * data_00625A98[rclass]);
        for (i = 0; i < data_00625A98[rclass]; i++) {
            reg_Uses[rclass][i] = NULL;
            reg_Defs[rclass][i] = NULL;
        }
    }
    for (block = data_00624FD0; block; block = block->next) {
        for (pcode = block->first; pcode; pcode = pcode->next) {
            if (!pcode->argcount) {
                continue;
            }
            use = &Uses[useID = pcode->useID];
            def = &Defs[defID = pcode->defID];
            op = pcode->args;
            i = pcode->argcount;
            while (i--) {
                if (op->kind == 0 && op->reg >= data_00624744[op->rclass]) {
                    if (op->flags & 1) {
                        use->pcode = pcode;
                        use->kind = op->kind;
                        use->rclass = op->rclass;
                        use->reg = op->reg;
                        addreglist(&reg_Uses[op->rclass][op->reg], useID);
                        use++;
                        useID++;
                    }
                    if (op->flags & 2) {
                        def->pcode = pcode;
                        def->kind = op->kind;
                        def->rclass = op->rclass;
                        def->reg = op->reg;
                        addreglist(&reg_Defs[op->rclass][op->reg], defID);
                        def++;
                        defID++;
                    }
                }
                op++;
            }
            if (!includeMemory) {
                continue;
            }
            if (pcode->flags & 2) {
                if (IsIndirect301(pcode)) {
                    for (oud = objectusedeflist; oud; oud = oud->next) {
                        if (may_alias_object((PCode*)pcode, object = oud->object)) {
                            setmemory(use, pcode, 1, object);
                            addreglist(&oud->uses, useID);
                            oud->indirectuses++;
                            use++;
                            useID++;
                        }
                    }
                } else {
                    object = pcode->alias->object;
                    objUseDef = findobjectusedef(object);
                    setmemory(use, pcode, 0, object);
                    if (must_alias_object((PCode*)pcode, object)) {
                        use->rclass = 0;
                        objUseDef->directuses++;
                    } else {
                        use->rclass = 1;
                        objUseDef->indirectuses++;
                    }
                    addreglist(&objUseDef->uses, useID);
                }
            } else if (pcode->flags & 4) {
                if (IsIndirect301(pcode)) {
                    for (oud = objectusedeflist; oud; oud = oud->next) {
                        if (may_alias_object((PCode*)pcode, object = oud->object)) {
                            setmemory(def, pcode, 1, object);
                            addreglist(&oud->defs, defID);
                            oud->indirectdefs++;
                            def++;
                            defID++;
                        }
                    }
                } else {
                    object = pcode->alias->object;
                    setmemory(def, pcode, 0, object);
                    objUseDef = findobjectusedef(object);
                    if (must_alias_object((PCode*)pcode, object)) {
                        def->rclass = 0;
                        objUseDef->directdefs++;
                    } else {
                        def->rclass = 1;
                        objUseDef->indirectdefs++;
                    }
                    addreglist(&objUseDef->defs, defID);
                }
            } else if (pcode->flags & 8) {
                for (oud = objectusedeflist; oud; oud = oud->next) {
                    if (may_alias_object((PCode*)pcode, object = oud->object)) {
                        setmemory(use, pcode, 1, object);
                        addreglist(&oud->uses, useID);
                        oud->indirectuses++;
                        use++;
                        useID++;
                        setmemory(def, pcode, 1, object);
                        addreglist(&oud->defs, defID);
                        oud->indirectdefs++;
                        def++;
                        defID++;
                    }
                }
            }
        }
    }
}

// 0x53C590
void func_0053C590(void) {
    MPCodeBlock* block;
    MPCode* pcode;
    BitVectorRange* usegen;
    BitVectorRange* defgen;
    UInt32* defkill;
    UInt32* usekill;
    UseOrDef301* use;
    UseOrDef301* def;
    ObjectUseDef301* oud;
    RegUseOrDef301* list;
    UseDefInfo301* info;
    int useID;
    int defID;

    defkill = oalloc(4 * ((number_of_Defs + 31) >> 5));
    usekill = oalloc(4 * ((number_of_Uses + 31) >> 5));
    for (block = data_00624FD0; block; block = block->next) {
        info = &usedefinfo[block->number];
        defgen = info->defgen;
        usegen = info->usegen;
        bitvectorinitialize_range(defgen, info->defstart, info->defcount, 0);
        bitvectorinitialize(defkill, number_of_Defs, 0);
        bitvectorinitialize_range(usegen, info->usestart, info->usecount, 0);
        bitvectorinitialize(usekill, number_of_Uses, 0);
        bitvectorinitialize(info->defin, number_of_Defs, 0);
        bitvectorinitialize(info->defout, number_of_Defs, 0);
        bitvectorinitialize(info->usein, number_of_Uses, 0);
        bitvectorinitialize(info->useout, number_of_Uses, 0);
        for (pcode = block->first; pcode; pcode = pcode->next) {
            if (!pcode->argcount) {
                continue;
            }
            for (use = &Uses[useID = pcode->useID]; useID < number_of_Uses && use->pcode == pcode;
                 use++, useID++) {
                bitvectorsetbit_range(useID, usegen);
                if (use->kind == 0) {
                    for (list = reg_Defs[use->rclass][use->reg]; list; list = list->next) {
                        if (bitvectorgetbit_range(list->id, defgen)) {
                            bitvectorclearbit_range(useID, usegen);
                        }
                    }
                } else {
                    for (list = findobjectusedef(use->object)->defs; list; list = list->next) {
                        if (must_alias_object((PCode*)pcode, Defs[list->id].object)) {
                            if (Defs[list->id].rclass == 0 &&
                                bitvectorgetbit_range(list->id, defgen)) {
                                bitvectorclearbit_range(useID, usegen);
                            }
                        }
                    }
                }
            }
            for (def = &Defs[defID = pcode->defID]; defID < number_of_Defs && def->pcode == pcode;
                 def++, defID++) {
                if (def->kind == 0) {
                    for (list = reg_Uses[def->rclass][def->reg]; list; list = list->next) {
                        if (Uses[list->id].pcode->block != block) {
                            bitvectorsetbit(list->id, usekill);
                        }
                    }
                    for (list = reg_Defs[def->rclass][def->reg]; list; list = list->next) {
                        if (Defs[list->id].pcode->block != block) {
                            bitvectorsetbit(list->id, defkill);
                        } else {
                            bitvectorclearbit_range(list->id, defgen);
                        }
                    }
                } else if (def->rclass == 0) {
                    oud = findobjectusedef(def->object);
                    for (list = oud->uses; list; list = list->next) {
                        if (may_alias_object((PCode*)pcode, Uses[list->id].object)) {
                            if (Uses[list->id].pcode->block != block) {
                                bitvectorsetbit(list->id, usekill);
                            }
                        }
                    }
                    for (list = oud->defs; list; list = list->next) {
                        if (must_alias_object((PCode*)pcode, Defs[list->id].object)) {
                            if (Defs[list->id].pcode->block != block) {
                                bitvectorsetbit(list->id, defkill);
                            } else {
                                bitvectorclearbit_range(list->id, defgen);
                            }
                        }
                    }
                }
                bitvectorsetbit_range(defID, defgen);
            }
        }
        info->defkill = bitvector_compress(defkill, number_of_Defs);
        info->usekill = bitvector_compress(usekill, number_of_Uses);
    }
}

// 0x53C460: out = (in - kill) + gen; whether out changed
int func_0053C460(
    UInt32* out, UInt32* in, CompressedBitVector* kill, BitVectorRange* gen, int bit_count
) {
    int words;
    int runs;
    int genStart;
    int genLength;
    int changed;
    CompressedBitRun* run;
    int runStart;
    int runLength;
    int i;
    UInt32 bits;
    UInt32 old;

    words = (bit_count + 31) >> 5;
    runs = kill->nruns;
    genStart = gen->firstword;
    genLength = gen->nwords;
    changed = FALSE;
    run = NULL;
    if (runs) {
        run = kill->runs;
        runStart = run->firstword;
        runLength = run->nwords;
    }
    for (i = 0; i < words; i++) {
        old = out[i];
        bits = in[i];
        if (run) {
            if (i >= runStart + runLength) {
                if (--runs != 0) {
                    run = (CompressedBitRun*)&run->words[runLength];
                    runStart = run->firstword;
                    runLength = run->nwords;
                } else {
                    run = NULL;
                }
            }
            if (run && i >= runStart) {
                bits &= ~run->words[i - runStart];
            }
        }
        if (i >= genStart && i < genStart + genLength) {
            bits |= gen->words[i - genStart];
        }
        if (bits != old) {
            changed = TRUE;
            out[i] = bits;
        }
    }
    return changed;
}

// 0x53C370
void func_0053C370(void) {
    MPBlockLink* preds;
    UseDefInfo301* udi;
    MPCodeBlock* block;
    UInt32* defin;
    int i;
    int changed;

    changed = TRUE;
    while (changed) {
        changed = FALSE;
        for (i = 0; i < data_00624D40; i++) {
            if ((block = data_00625418[i])) {
                udi = &usedefinfo[block->number];
                if ((preds = block->predecessors) && preds->from) {
                    defin = udi->defin;
                    bitvectorcopy(defin, usedefinfo[preds->from->number].defout, number_of_Defs);
                    for (preds = preds->nextpred; preds && preds->from; preds = preds->nextpred) {
                        bitvectorunion(
                            defin, usedefinfo[preds->from->number].defout, number_of_Defs
                        );
                    }
                }
                changed |= func_0053C460(
                    udi->defout, udi->defin, udi->defkill, udi->defgen, number_of_Defs
                );
            }
        }
    }
}

// 0x53C290
void func_0053C290(void) {
    MPBlockLink* succs;
    UseDefInfo301* udi;
    MPCodeBlock* block;
    UInt32* useout;
    int i;
    int changed;

    changed = TRUE;
    while (changed) {
        changed = FALSE;
        i = data_00624D40;
        while (i) {
            if ((block = data_00625418[--i])) {
                udi = &usedefinfo[block->number];
                if ((succs = block->successors) && succs->block) {
                    useout = udi->useout;
                    bitvectorcopy(useout, usedefinfo[succs->block->number].usein, number_of_Uses);
                    for (succs = succs->next; succs && succs->block; succs = succs->next) {
                        bitvectorunion(
                            useout, usedefinfo[succs->block->number].usein, number_of_Uses
                        );
                    }
                }
                changed |= func_0053C460(
                    udi->usein, udi->useout, udi->usekill, udi->usegen, number_of_Uses
                );
            }
        }
    }
}

// 0x53C140: computeusedefchains
void func_0053C140(int includeMemory) {
    UseDefInfo301* info;
    int i;
    int rclass;

    if (includeMemory) {
        gather_alias_info();
        func_0053D330();
    }
    func_0053D0B0(includeMemory);
    func_0053CB70(includeMemory);
    init_compressed_bitvectors(number_of_Defs > number_of_Uses ? number_of_Defs : number_of_Uses);
    for (i = 0, info = usedefinfo; i < data_00624D40; i++, info++) {
        info->defgen = bitvectorcreate_range(info->defstart, info->defcount);
        info->defkill = bitvector_empty_compressed();
        info->defin = oalloc(4 * ((number_of_Defs + 31) >> 5));
        info->defout = oalloc(4 * ((number_of_Defs + 31) >> 5));
        info->usegen = bitvectorcreate_range(info->usestart, info->usecount);
        info->usekill = bitvector_empty_compressed();
        info->usein = oalloc(4 * ((number_of_Uses + 31) >> 5));
        info->useout = oalloc(4 * ((number_of_Uses + 31) >> 5));
    }
    func_0053C590();
    computedepthfirstordering();
    func_0053C370();
    func_0053C290();
    for (rclass = 0; rclass < 7; rclass++) {
        last_usedef_register[rclass] = data_00625A98[rclass];
    }
}

// 0x53BF00: removes instructions whose registers are never used (with
// rebuild, after recomputing the register use-def chains).
void func_0053BF00(int rebuild) {
    MPCodeBlock* block;
    MPCode* pc;
    MOperand* op;
    int dead;
    int n;
    int rclass;
    int i;
    UseDefInfo301* info;

    if (rebuild) {
        func_0053D0B0(0);
        func_0053CB70(0);
        init_compressed_bitvectors(
            number_of_Defs > number_of_Uses ? number_of_Defs : number_of_Uses
        );
        for (i = 0, info = usedefinfo; i < data_00624D40; i++, info++) {
            info->defgen = bitvectorcreate_range(info->defstart, info->defcount);
            info->defkill = bitvector_empty_compressed();
            info->defin = oalloc(4 * ((number_of_Defs + 31) >> 5));
            info->defout = oalloc(4 * ((number_of_Defs + 31) >> 5));
            info->usegen = bitvectorcreate_range(info->usestart, info->usecount);
            info->usekill = bitvector_empty_compressed();
            info->usein = oalloc(4 * ((number_of_Uses + 31) >> 5));
            info->useout = oalloc(4 * ((number_of_Uses + 31) >> 5));
        }
        func_0053C590();
        computedepthfirstordering();
        func_0053C370();
        func_0053C290();
        for (rclass = 0; rclass < 7; rclass++) {
            last_usedef_register[rclass] = data_00625A98[rclass];
        }
    }

    for (block = data_00624FD0; block; block = block->next) {
        for (pc = block->last; pc; pc = pc->prev) {
            dead = 1;
            if ((pc->flags & 0x18D) || (pc->block->flags2E & 3)) {
                dead = 0;
            } else {
                for (n = pc->argcount, op = pc->args; n; n--, op++) {
                    if (op->kind == 0 && (op->flags & 2)) {
                        if (op->reg < data_00624744[op->rclass] ||
                            data_006247F4[op->rclass][op->reg]) {
                            dead = 0;
                            break;
                        }
                    }
                }
            }
            if (dead) {
                deletepcode(pc);
            }
        }
    }
    freeoheap();
}
