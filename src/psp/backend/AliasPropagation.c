// mwccpsp 3.0.1 AliasPropagation.c (0x53B390..)
#include "compiler/common.h"
#include "compiler/CError.h"
#include "compiler/objects.h"
#include "psp/BE.h"

extern struct Alias* make_alias(Object* obj, SInt32 offset, SInt32 size); // 0x4817A0
extern SInt32 func_0048FBE0(UInt16 op); // bytes an opcode moves
extern SInt16 data_00625E3E; // frame register

// 0x53BD10: the alias of a memory operand. A local addressed through
// another register than the frame pointer could be anywhere in it.
struct Alias* func_0053BD10(VOperand* mem, MPCode* pc) {
    Object* obj;
    struct Alias* alias;

    alias = NULL;
    obj = (Object*)mem->object;
    if (!obj) {
        CError_Internal("AliasPropagation.c", CError_B145_LINE(37));
    }
    if (obj->datatype == DLOCAL && mem->reg != data_00625E3E) {
        alias = make_alias(obj, 0, 0);
    } else if (obj->datatype == DLOCAL) {
        alias = make_alias(
            obj,
            pc->args[1].value - ((Object*)MOP_OBJ(pc->args[1]))->u.var.offset,
            func_0048FBE0(pc->op)
        );
    } else if (obj->datatype == DDATA) {
        alias = make_alias(obj, pc->args[1].value, func_0048FBE0(pc->op));
#if MWCCPS2 && MWCCPS2_BUILD < 210 // b145 has no function case
    } else {
        CError_Internal("AliasPropagation.c", 53);
    }
#else
    } else if (obj->datatype == DFUNC) {
        alias = make_alias(obj, pc->args[1].value, func_0048FBE0(pc->op));
    } else {
        CError_Internal("AliasPropagation.c", CError_B145_LINE(57));
    }
#endif
    return alias;
}

// --- alias gathering (moved here from 2.4.7's Alias.c) --------------------

#include "compiler/Alias.h"
#include "psp/UseDef301.h"
#include "compiler/CParser.h"
#include <stddef.h>

extern Alias* worst_case_ptr;
extern Alias* Alias_CreateUnion301(Alias* a, Alias* b); // Alias.c: create_alias_union
#define create_alias_union Alias_CreateUnion301
extern void add_alias_member(Alias* parent, Alias* child);
extern void func_004AFF90(MPCode* pc); // listing
extern void func_004AFFA0(MPCode* def, MPCode* pc); // listing

// 0x53BDF0: def (defID, writing reg) reaches use
int func_0053BDF0(MPCode* candidate, int defID, int reg, MPCode* use, UInt32* defvec, char rclass) {
    MPCodeBlock* block = use->block;
    MPCode* pcode;
    MOperand* op;
    int i;
    Boolean precedes = FALSE;

    if (candidate->block == block) {
        for (pcode = candidate->next; pcode; pcode = pcode->next) {
            if (pcode == use) {
                precedes = TRUE;
                break;
            }
        }
    }
    if (precedes) {
        for (pcode = candidate->next; pcode && pcode != use; pcode = pcode->next) {
            for (op = pcode->args, i = pcode->argcount; i--; op++) {
                if (op->kind == 0 && op->rclass == rclass && (op->flags & 2) && op->reg == reg) {
                    return FALSE;
                }
            }
        }
    } else {
        if (!((defvec[defID >> 5] >> (defID & 31)) & 1)) {
            return FALSE;
        }
        for (pcode = block->first; pcode && pcode != use; pcode = pcode->next) {
            for (op = pcode->args, i = pcode->argcount; i--; op++) {
                if (op->kind == 0 && op->rclass == rclass && (op->flags & 2) && op->reg == reg) {
                    return FALSE;
                }
            }
        }
    }
    return TRUE;
}

// 0x53BC90: the alias a definition's address computation makes
#if MWCCPS2
__declspec(noinline) // b210 calls it from func_0053B500/func_0053B7D0
#endif
Alias* func_0053BC90(MPCode* pc) {
#if MWCCPS2
    SInt32 offset;
    Object* obj;
    Alias* alias;

    if (pc->argcount > 2 &&
        pc->args[1].kind == 4 &&
        (obj = (Object*)MOP_OBJ(pc->args[1])) != NULL) {
        alias = pc->alias;
        offset = pc->args[1].value;
        alias = alias ? alias : (pc->alias = make_alias(obj, offset, 1));
    } else {
        alias = pc->alias;
    }
    return alias;
#else
    if (pc->argcount > 2 && pc->args[1].kind == 4 && MOP_OBJ(pc->args[1])) {
        if (!pc->alias) {
            pc->alias = make_alias((Object*)MOP_OBJ(pc->args[1]), pc->args[1].value, 1);
        }
    }
    return pc->alias;
#endif
}

// 0x53B500: the alias of operand index of pc (2.4.7's operand_alias); *pdef
// is the one definition reaching it, when there is one
Alias* func_0053B500(MPCode* pcode, int index, UInt32* myvec, MPCode** pdef) {
    RegUseOrDef301* list;
    MPCode* defpcode = NULL;
    Alias* alias;
    Alias* result = NULL;
    MOperand* op = &pcode->args[index];
    int count = 0;

    for (list = data_00624BFC[op->rclass][op->reg]; list; list = list->next) {
        defpcode = data_00624B58[list->id].pcode;
        if (func_0053BDF0(defpcode, list->id, op->reg, pcode, myvec, 0)) {
            alias = func_0053BC90(defpcode);
            count++;
            if (alias) {
                if (count == 1) {
                    result = alias;
                } else if (!result) {
                    result = create_alias_union(worst_case_ptr, alias);
                } else {
                    result = create_alias_union(result, alias);
                }
            } else if (result && result != worst_case_ptr) {
                result = create_alias_union(worst_case_ptr, result);
            }
        }
    }
    *pdef = (count == 1) ? defpcode : NULL;
    return result;
}

// 0x53B620: a memory access through the address def computes takes def's
// object and base register directly
void func_0053B620(MPCode* pc, MPCode* def) {
    Alias* alias;
    Object* obj;
    Object* obj2;
    SInt16 breg;
    SInt32 off;

    if (!pc || !def) {
        return;
    }
    if (def->op != MACRO_OP(0x44) || !(pc->flags & 6) || !(pc->flags & 0x20)) {
        return;
    }
    if (pc->args[1].kind != 4) {
        return;
    }
    alias = def->alias;
    if (alias->type != 1) {
        return;
    }
    obj = (Object*)MOP_OBJ(def->args[1]);
    if (!obj || !obj->name) {
        return;
    }
    if (def->args[0].reg != pc->args[2].reg) {
        return;
    }
    breg = def->args[2].reg;
    if (!(breg == data_00625E3E || breg == 0x1C || breg == 0 || breg == 0x17)) {
        return;
    }
    if (MOP_OBJ(pc->args[1]) && MOP_OBJ(pc->args[1]) != (void*)obj) {
        return;
    }
    off = pc->args[1].value + alias->offset;
    if (pc->op <= BE_NUM_OPCODES) {
        return;
    }
    if (copts.debuglisting) {
        func_004AFFA0(def, pc);
    }
    obj2 = def->alias->object;
    if (obj2) {
        pc->args[1].value = off + obj2->u.var.offset;
    }
#if !MWCCPS2 || MWCCPS2_BUILD >= 210 // b145 stops at the local offset
    obj2 = def->alias->object;
    if (obj2->datatype == DLOCAL) {
        pc->args[1].value = off + obj2->u.var.offset;
    } else if (obj2->datatype == DDATA) {
        pc->args[1].value = off;
    } else {
        CError_Internal("AliasPropagation.c", CError_B145_LINE(586));
    }
#endif
    MOP_SETOBJ(pc->args[1], MOP_OBJ(def->args[1]));
    pc->args[2].reg = breg;
    pc->flags &= ~0x20;
    if (copts.debuglisting) {
        func_004AFF90(pc);
    }
}

// 0x53B390: gather_instruction_alias
void func_0053B390(MPCode* pcode, UInt32* myvec) {
    Alias* alias_choice;
    Alias* alias;
    MOperand* op;
    MOperand* skip;
    MPCode* defpc = NULL;
    int i;

    if (!pcode->alias) {
        pcode->alias = worst_case_ptr;
    } else if (pcode->alias != worst_case_ptr) {
        if (!(pcode->argcount >= 3 && pcode->args[1].kind == 4 && MOP_OBJ(pcode->args[1]))) {
            alias_choice = NULL;
            skip = (pcode->flags & 4) ? &pcode->args[0]
                                      : &pcode->args[(pcode->flags & 0x80000000) ? 1 : 0];
            for (i = 0, op = pcode->args; i < pcode->argcount; i++, op++) {
                if (op != skip && op->kind == 0 && op->rclass == 0 && (op->flags & 1)) {
                    alias = func_0053B500(pcode, i, myvec, &defpc);
                    if (alias) {
                        alias_choice =
                            alias_choice ? create_alias_union(alias_choice, alias) : alias;
                    }
                }
            }
            if (alias_choice == worst_case_ptr) {
                pcode->flags |= 0x20;
                pcode->alias = worst_case_ptr;
                add_alias_member(worst_case_ptr, pcode->alias);
            } else if (defpc) {
                func_0053B620(pcode, defpc);
            }
        }
    }
    if (pcode->alias->type <= 1 && pcode->alias->size == func_0048FBE0(pcode->op)) {
        pcode->flags &= ~0x20;
    } else {
        pcode->flags |= 0x20;
    }
}

extern int coloring;
extern Object* data_00625078; // gFunction
extern Alias* worst_case_func;

extern void Alias_GatherMembervecs301(void);
#if MWCCPS2
// b210 inlines Alias.c's member-vector tail into gather_alias_info; these are
// Alias.c's statics (aliases, n_aliases, n_gathered_aliases)
extern Alias* ps2_alias_aliases;
extern int ps2_alias_n_aliases;
extern int ps2_alias_n_gathered;
extern void* lalloc(SInt32 size);
#define PS2_GATHER_MEMBERVECS() \
    do { \
        Alias* alias_; \
        AliasMember* member_; \
        AliasMember* member2_; \
        if (ps2_alias_n_gathered != ps2_alias_n_aliases) { \
            for (alias_ = ps2_alias_aliases; alias_; alias_ = alias_->next) { \
                if (alias_->type == AliasType_Set) { \
                    alias_->membervec = lalloc(4 * ((ps2_alias_n_aliases + 31) >> 5)); \
                    bitvectorinitialize(alias_->membervec, ps2_alias_n_aliases, 0); \
                    for (member_ = alias_->parents; member_; member_ = member_->nextParent) { \
                        bitvectorsetbit(member_->child->index, alias_->membervec); \
                        for (member2_ = member_->child->parents; member2_; \
                             member2_ = member2_->nextParent) { \
                            bitvectorsetbit(member2_->child->index, alias_->membervec); \
                        } \
                    } \
                } \
            } \
            ps2_alias_n_gathered = ps2_alias_n_aliases; \
        } \
    } while (0)
#endif
void func_0053B790(Object* proc); // propagatealiasinfo (below)
extern void bitvectorcopy(UInt32* dst, UInt32* src, SInt32 len);
extern void* oalloc(SInt32 size);
extern void freeoheap(void);

// 0x481370: the aliases of memory instructions
void gather_alias_info(void) {
    MPCodeBlock* block;
    MPCode* pcode;
    UseOrDef301* def;
    RegUseOrDef301* list;
    UInt32* myvec;
    int defID;

    if (coloring) {
        func_0053B790(data_00625078);
        add_alias_member(worst_case_func, worst_case_ptr);
        myvec = oalloc(4 * ((data_00625374 + 31) >> 5));
        for (block = data_00624FD0; block; block = block->next) {
            bitvectorcopy(myvec, data_00625988[block->number].defin, data_00625374);
            for (pcode = block->first; pcode; pcode = pcode->next) {
                if (pcode->flags & 6) {
                    func_0053B390(pcode, myvec);
                } else if ((pcode->flags & 8) && !pcode->alias) {
                    pcode->alias = worst_case_func;
                }
                for (def = &data_00624B58[defID = pcode->defID];
                     defID < data_00625374 && def->pcode == pcode;
                     def++, defID++) {
                    if (def->kind == 0 && def->rclass == 0) {
                        for (list = data_00624BFC[def->rclass][def->reg]; list; list = list->next) {
                            myvec[list->id >> 5] &= ~(1 << (list->id & 31));
                        }
                    }
                    myvec[defID >> 5] |= 1 << (defID & 31);
                }
            }
        }
        freeoheap();
    } else {
        for (block = data_00624FD0; block; block = block->next) {
            for (pcode = block->first; pcode; pcode = pcode->next) {
                if (!pcode->alias) {
                    if (pcode->flags & 6) {
                        pcode->alias = worst_case_ptr;
                    } else if (pcode->flags & 8) {
                        pcode->alias = worst_case_func;
                    }
                }
            }
        }
    }
#if MWCCPS2
    PS2_GATHER_MEMBERVECS();
#else
    Alias_GatherMembervecs301();
#endif
}

// --- alias propagation (propagatealiasinfo) --------------------------------

typedef struct Propagation301 {
    int (*iscandidate)(MPCode* pcode);
    int (*touse)(int candidateID, int useID);
    void (*finish)(int candidateID);
    char* name;
    char* dumpname;
    char* nodumpname;
    UInt32 candmask;
    Boolean computesUseDefChains;
} Propagation301;

typedef struct Candidate301 {
    MPCode* pcode;
    void* list;
} Candidate301;

extern Candidate301* data_00625348; // Candidates
extern int data_00624584; // propagated_instructions
extern int data_00625020; // recursive_propagation
extern SInt32 data_00624744[7];
extern SInt16 data_00625E48; // the register move opcode
extern Alias* Alias_MakeDisplAlias301(
    Alias* alias, SInt32 offset, SInt32 size
); // Alias.c: make_displ_alias
extern void add_aliased_locals_to_worst_case(Alias* alias);
extern void func_005452C0(Object* proc, Propagation301* config, int passCount, Boolean localflag);

// 0x53B8B0: propagate_alias_to_use
#if MWCCPS2 && MWCCPS2_BUILD < 210 // b145 asserts
#define B145_ALP_M4 -4
#define B145_ALP_M7 -7
#else
#define B145_ALP_M4
#define B145_ALP_M7
#endif
void func_0053B8B0(MPCode* use, int reg, Alias* alias) {
    SInt32 size = 1;
    SInt32 offset = 0;
    MOperand* op;
    Object* obj;
    int i;

    if (use->flags & 6) {
        size = func_0048FBE0(use->op);
    }
#if MWCCPS2
    // b210: qmove/vmove (0x1CC/0x2AF) are moves too
    if (use->op != data_00625E48 && use->op != 0x1CC && use->op != 0x2AF) {
#else
    if (use->op != data_00625E48) {
#endif
        if (use->argcount < 3) {
            return;
        }
        if (use->op == MACRO_OP(0x44)) {
            if (use->args[1].kind == 4 && MOP_OBJ(use->args[1])) {
                if (MOP_OBJ(use->args[1]) == (void*)alias->object) {
                    return;
                }
                CError_Internal("AliasPropagation.c", CError_B145_LINE(175) B145_ALP_M4);
            } else {
                offset = use->args[1].value;
            }
        } else if (use->op == 4) {
            if (use->args[1].reg != 0 || (use->args[1].flags & 1)) {
                offset = 0;
                size = 0;
            } else {
                offset = 0;
            }
        } else if (use->flags & 6) {
            if (use->args[2].reg != reg) {
                return;
            }
            switch (use->args[1].kind) {
                case 2:
                    offset = use->args[1].value;
                    break;
                case 4:
                    if (MOP_OBJ(use->args[1])) {
                        return;
                    }
#if MWCCPS2 && MWCCPS2_BUILD < 210 // b145 has no frame-relative local case
                    offset = use->args[1].value;
#else
                    obj = alias->object;
                    if (obj && obj->datatype == DLOCAL && reg != data_00625E3E) {
                        size = 0;
                        offset = 0;
                    } else {
                        offset = use->args[1].value;
                    }
#endif
                    break;
                case 6:
                    return;
                default:
                    CError_Internal("AliasPropagation.c", CError_B145_LINE(241) B145_ALP_M7);
                    break;
            }
        } else if (use->args[2].kind == 2) {
            if (!(use->args[1].kind == 0 && use->args[1].rclass == 0 && use->args[1].reg == reg)) {
                return;
            }
            if (use->op == 3) {
                offset = use->args[2].value;
            } else if (use->op == 0x90) {
                offset = use->args[2].value << 16;
            } else {
                add_alias_member(worst_case_ptr, alias);
                return;
            }
        } else if (use->args[2].kind == 3) {
            if (!(use->args[1].kind == 0 && use->args[1].rclass == 0 && use->args[1].reg == reg)) {
                return;
            }
            if (use->op == 3) {
#if MWCCPS2
                offset = use->args[2].value_mid[0]; // the 128-bit constant's low word
#else
                offset = use->args[2].value2;
#endif
            } else {
                add_alias_member(worst_case_ptr, alias);
                return;
            }
        } else {
            for (i = 0, op = use->args; i < use->argcount; i++, op++) {
                if (op->kind == 0 && op->rclass == 0 && (op->flags & 2) && op->reg == reg) {
                    add_alias_member(worst_case_ptr, alias);
                    use->alias = worst_case_ptr;
                    return;
                }
            }
            return;
        }
    }

    if (alias->type <= 1) {
        alias = Alias_MakeDisplAlias301(alias, offset, size);
    }
    if (!use->alias) {
#if MWCCPS2
        if (use->op == 3 ||
            use->op == 4 ||
            use->op == data_00625E48 ||
            use->op == 0x1CC ||
            use->op == 0x2AF) {
#else
        if (use->op == 3 || use->op == 4 || use->op == data_00625E48) {
#endif
            data_00625020 = TRUE;
        }
        use->alias = alias;
    } else if (use->alias == worst_case_ptr) {
        add_alias_member(worst_case_ptr, alias);
    } else if (use->alias == alias) {
        return;
    } else if (use->alias->type <= 1 && alias->type <= 1) {
        if (alias->object == use->alias->object) {
            if (alias->offset != use->alias->offset || alias->size != use->alias->size) {
                use->alias = make_alias(alias->object, 0, 0);
            }
        } else {
            use->alias = create_alias_union(alias, use->alias);
        }
    } else {
        use->alias = create_alias_union(alias, use->alias);
    }
    data_00624584 = TRUE;
}

// 0x53B7D0: addresspropagatestouse
int func_0053B7D0(int candidateID, int useID) {
    MPCode* cand = data_00625348[candidateID].pcode;
    MPCode* use = data_00624768[useID].pcode;
    Alias* alias;
    int reg;

    if (cand->op == 0x3AA &&
        cand->args[2].reg == 0x1D &&
        cand->args[1].kind == 2 &&
        cand->args[1].value == 0) {
        return FALSE;
    }
    reg = cand->args[0].reg;
    if (!func_0053BDF0(cand, cand->defID, reg, use, data_00625988[use->block->number].defin, 0)) {
        return TRUE;
    }
    alias = func_0053BC90(cand);
    if (alias) {
        if (alias != worst_case_ptr) {
            if ((use->flags & 4) &&
                use->args[0].kind == 0 &&
                use->args[0].rclass == 0 &&
                use->args[0].reg == reg) {
                add_aliased_locals_to_worst_case(alias);
            } else if (!(use->flags & 6) && use->args[0].reg < data_00624744[0]) {
                add_aliased_locals_to_worst_case(alias);
            }
        }
        func_0053B8B0(use, reg, alias);
    }
    return TRUE;
}

// 0x53B7C0
void func_0053B7C0(int candidateID) {
    data_00624584 = TRUE;
}

// 0x53BCD0: an address computation
int func_0053BCD0(MPCode* pcode) {
    switch (pcode->op) {
        case 3:
        case 4:
        case 0x78:
        case 0x79:
        case 0xAA:
        case 0xAB:
        case MACRO_OP(0x44):
            return TRUE;
    }
    return FALSE;
}

static Propagation301 alias_prop301 = {
    func_0053BCD0, func_0053B7D0, func_0053B7C0, "ALIAS", "ALIAS", "ALIAS", 0, TRUE
};

// 0x53B790: propagatealiasinfo
void func_0053B790(Object* proc) {
    func_005452C0(proc, &alias_prop301, (copts.optimizationlevel >= 4) ? 4 : 1, TRUE);
}
