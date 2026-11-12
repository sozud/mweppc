// mwccpsp 3.0.1: the MIPS back end's memory store-load optimization
// (0x545DA0..0x5469C0; "AFTER MEMORY STORE-LOAD OPTIMIZATION"). Not in the
// 2.4.7 sources. For each object with direct uses or definitions it replaces
// loads whose reaching stores all store a register of the load's class by a
// copy from a new virtual register (a register-allocated local named
// "name'reg") that every store also writes; stores left without loads are then
// deleted. A load in a triangle X -> A -> B, X -> B whose value is stored in A
// and loaded in X is replaced the same way through a register written in both.
#include "compiler/common.h"
#include "compiler/Alias.h"
#include "compiler/CompilerTools.h"
#include "compiler/objects.h"
#include "psp/BE.h"
#include "psp/UseDef301.h"

extern ObjectUseDef301* data_00624780; // objectusedeflist
extern SInt32 data_00624728[7]; // last_usedef_register
extern SInt32 data_00624744[7]; // n_real: the first virtual register of each class
extern SInt32 data_00625A98[7]; // the next virtual register of each class
extern int func_0048FB20(MPCode* pc); // a load/store's move kind (0: none)
extern MPCode* func_0048FDC0(int kind, MOperand* src, MOperand* dst);
extern MPCode* func_00490280(MOperand* src, MOperand* dst);
#if MWCCPS2
extern void func_00435AA0(Object* obj, char rclass, int reg);
#else
extern void func_00435AA0(Object* obj, char rclass, short reg);
#endif
extern Boolean uniquely_aliases(PCode* a, PCode* b);
extern Boolean uniquely_aliases_alias(PCode* a, Alias* b);
extern Boolean may_alias_worst_case(PCode* pcode);

#define Defs data_00624B58
#define Uses data_00624768
#define RESULT(pc) ((pc)->args[((pc)->flags & 0x80000000) ? 1 : 0])
#define GETBIT(vec, n) (((vec)[(n) >> 5] >> ((n) & 31)) & 1)

static int follows(MPCode* first, MPCode* pc) {
    MPCode* scan;

    for (scan = first->next; scan; scan = scan->next) {
        if (scan == pc) {
            return 1;
        }
    }
    return 0;
}

// 0x546600: the X of a triangle X -> a -> b, X -> b (NULL otherwise).
MPCodeBlock* func_00546600(MPCodeBlock* a, MPCodeBlock* b) {
    MPCodeBlock* other = NULL;
    MPBlockLink* link;

    for (link = b->predecessors; link; link = link->nextpred) {
        if (link->from == a->prev) {
            other = link->from;
        } else if (link->from != a) {
            return NULL;
        }
    }
    for (link = a->successors; link; link = link->next) {
        if (link->block != b) {
            return NULL;
        }
    }
    for (link = a->predecessors; link; link = link->nextpred) {
        if (link->from != other) {
            return NULL;
        }
    }
    for (link = other->successors; link; link = link->next) {
        if (link->block != a && link->block != b) {
            return NULL;
        }
    }
    return other;
}

// 0x5466A0: renames the register that store stores to reg, when its only
// definition is earlier in the store's block with no memory access between
// that may alias the store.
#if MWCCPS2
// b210 keeps func_005466A0 and the reaches() wrappers out of line
#define SL_NOINLINE __declspec(noinline)
#else
#define SL_NOINLINE
#endif

SL_NOINLINE int func_005466A0(MPCode* store, short reg) {
    MOperand* op;
    RegUseOrDef301* defs;
    RegUseOrDef301* uses;
    MPCode* defpc;
    MPCode* pc;
    int rclass;
    int r;

    op = &store->args[0];
    rclass = op->rclass;
    r = op->reg;
    if (r < data_00624744[rclass]) {
        return 0;
    }
    if (op->kind == 0 && r >= data_00624728[rclass]) {
        return 0;
    }
    defs = data_00624BFC[rclass][r];
    uses = data_006247F4[rclass][r];
    if (!defs) {
        return 0;
    }
    defpc = Defs[defs->id].pcode;
    if (defs->next ||
        !defpc ||
        !defpc->block ||
        !uses ||
        uses->next ||
        Uses[uses->id].pcode != store) {
        return 0;
    }
    if (defpc->block != store->block) {
        return 0;
    }
    if (!follows(defpc, store)) {
        return 0;
    }
    for (pc = defpc->next; pc && pc != store; pc = pc->next) {
        if ((pc->flags & 0x60006) && may_alias((PCode*)pc, (PCode*)store)) {
            return 0;
        }
    }
    RESULT(defpc).reg = reg;
    op->reg = reg;
    return 1;
}

// 0x546800 (may_alias) and 0x5468E0 (uniquely_aliases): def reaches use and
// no store between them aliases def.
#if MWCCPS2
// b210 expands it into both wrappers (direct calls to kills)
inline int reaches(MPCode* def, int defid, MPCode* use, Boolean (*kills)(PCode*, PCode*)) {
#else
static int reaches(MPCode* def, int defid, MPCode* use, Boolean (*kills)(PCode*, PCode*)) {
#endif
    MPCodeBlock* block;
    MPCode* pc;

    block = use->block;
    if (!block) {
        return 0;
    }
    if (def->block == block && follows(def, use)) {
        for (pc = def->next; pc && pc != use; pc = pc->next) {
            if ((pc->flags & 4) && kills((PCode*)pc, (PCode*)def)) {
                return 0;
            }
        }
        return 1;
    }
    if (!GETBIT(data_00625988[block->number].defin, defid)) {
        return 0;
    }
    for (pc = block->first; pc && pc != use; pc = pc->next) {
        if ((pc->flags & 4) && kills((PCode*)pc, (PCode*)def)) {
            return 0;
        }
    }
    return 1;
}

SL_NOINLINE int func_00546800(MPCode* def, int defid, MPCode* use) {
    return reaches(def, defid, use, may_alias);
}

SL_NOINLINE int func_005468E0(MPCode* def, int defid, MPCode* use) {
    return reaches(def, defid, use, uniquely_aliases);
}

int func_00545DA0(void) {
    ObjectUseDef301* oud;
    RegUseOrDef301* node;
    RegUseOrDef301* dnode;
    MPCode* use;
    MPCode* def;
    MPCode* lastdef;
    MPCode* prevdef;
    MPCode* pc;
    MPCode* npc;
    MPCodeBlock* block;
    MPCodeBlock* other;
    MPBlockLink* pred;
    Alias* alias;
    MOperand op0;
    MOperand op10;
    int prevdefid;
    int changed;
    int havereg;
    int tmpreg;
    int newreg;
    int nloads;
    int ndeleted;
    int kind;
    int ok;
    int triangle;
    int del;
    char rclass;
    char lastrclass;

#if MWCCPS2
    // b210 leaves the scratch operands and tmpreg/lastdef/prevdefid unset
    changed = 0;
    lastrclass = (char)0xFE;
#else
    changed = 0;
    lastrclass = (char)0xFE;
    tmpreg = 0;
    lastdef = NULL;
    prevdefid = 0;
    memclrw(&op0, sizeof(op0));
    memclrw(&op10, sizeof(op10));
#endif
    for (oud = data_00624780; oud; oud = oud->next) {
        havereg = 0;
        nloads = 0;
        ndeleted = 0;
        if (oud->directuses == 0 && oud->directdefs == 0) {
            continue;
        }

        for (node = oud->uses; node; node = node->next) {
            ok = 1;
            use = Uses[node->id].pcode;
            block = use->block;
            rclass = RESULT(use).rclass;
            if (!block || !(use->flags & 2)) {
                continue;
            }
            nloads++;
            prevdef = NULL;
            kind = func_0048FB20(use);
            triangle = 0;
            alias = use->alias;
            if (!kind || alias->type > 1 || alias->object != oud->object || (use->flags & 0x180)) {
                ok = 0;
            } else {
                pred = block->predecessors;
                if (block->prev && pred && pred->nextpred && !pred->nextpred->nextpred) {
                    triangle = 1;
                }
                for (dnode = oud->defs; dnode; dnode = dnode->next) {
                    def = lastdef = Defs[dnode->id].pcode;
                    if (func_005468E0(def, dnode->id, use)) {
                        if ((def->flags & 4) != 4) {
                            ok = 0;
                        } else {
                            if (def->block == block->prev && (!prevdef || follows(prevdef, def))) {
                                prevdefid = dnode->id;
                                prevdef = def;
                            }
                            if (!uniquely_aliases((PCode*)use, (PCode*)def)) {
                                ok = 0;
                            } else if (def->args[0].rclass != rclass) {
                                ok = 0;
                                triangle = 0;
                            }
                        }
                    }
                    if (!ok && !triangle) {
                        break;
                    }
                }
            }

            if (ok && GETBIT(data_00625988[data_00624FD0->number].usein, node->id)) {
                ok = 0;
            }

            if (!ok &&
                prevdef &&
                triangle &&
                uniquely_aliases((PCode*)use, (PCode*)prevdef) &&
                func_00546800(prevdef, prevdefid, use) &&
                block->prev == prevdef->block &&
                (other = func_00546600(prevdef->block, block))) {
                for (pc = other->last; pc; pc = pc->prev) {
                    if (!pc->alias || !may_alias((PCode*)use, (PCode*)pc)) {
                        continue;
                    }
                    if (pc->flags & 2) {
                        if (!uniquely_aliases((PCode*)use, (PCode*)pc)) {
                            continue;
                        }
                        newreg = data_00625A98[RESULT(use).rclass]++;
                        op10 = RESULT(use);
                        op10.reg = newreg;
                        insertpcodeafter(pc, func_0048FDC0(kind, &RESULT(pc), &op10));
                        if (!(kind == 1 && func_005466A0(prevdef, newreg))) {
                            insertpcodebefore(
                                prevdef, func_0048FDC0(kind, &prevdef->args[0], &op10)
                            );
                        }
                        insertpcodebefore(use, func_0048FDC0(kind, &op10, &RESULT(use)));
                        if (Defs[use->defID].pcode == use) {
                            Defs[use->defID].pcode = lastdef;
                        }
                        deletepcode(use);
                        ndeleted++;
                        changed = 1;
                    } else if (pc->flags & 0xC) {
                        break;
                    }
                }
            }

            if (!ok) {
                continue;
            }
            if (!havereg || rclass != lastrclass) {
                havereg = 1;
                lastrclass = RESULT(use).rclass;
                tmpreg = data_00625A98[lastrclass]++;
                func_00435AA0(oud->object, lastrclass, tmpreg);
                for (dnode = oud->defs; dnode; dnode = dnode->next) {
                    def = Defs[dnode->id].pcode;
                    if (uniquely_aliases((PCode*)use, (PCode*)def) &&
                        (def->flags & 4) &&
                        def->args[0].rclass == lastrclass &&
                        !func_005466A0(def, tmpreg)) {
                        op0 = RESULT(use);
                        op0.reg = tmpreg;
                        insertpcodebefore(def, func_00490280(&def->args[0], &op0));
                    }
                }
            }
            op0.kind = 0;
            op0.rclass = lastrclass;
            op0.flags = 1;
            op0.reg = tmpreg;
            changed = 1;
            npc = func_0048FDC0(kind, &op0, &RESULT(use));
            insertpcodebefore(use, npc);
            if (Defs[use->defID].pcode == use) {
                Defs[use->defID].pcode = npc;
            }
            deletepcode(use);
            ndeleted++;
        }

        for (node = oud->defs; node; node = node->next) {
            def = Defs[node->id].pcode;
            if (!def->block || !(def->flags & 4)) {
                continue;
            }
            alias = def->alias;
            del = 1;
            if (alias->type > 1 ||
                alias->object != oud->object ||
                (def->flags & 0x180) ||
                !uniquely_aliases_alias((PCode*)def, alias)) {
                del = 0;
            } else if (nloads == ndeleted &&
                       !may_alias_worst_case((PCode*)def) &&
                       oud->object->datatype == DLOCAL) {
                // a local whose loads are all gone
            } else {
                if (GETBIT(data_00625988[data_006246B4->number].defin, node->id)) {
                    del = 0;
                    for (pc = data_006246B4->first; pc; pc = pc->next) {
                        if ((pc->flags & 4) && uniquely_aliases((PCode*)pc, (PCode*)def)) {
                            del = 1;
                            break;
                        }
                    }
                }
                if (del) {
                    for (dnode = oud->uses; dnode; dnode = dnode->next) {
                        use = Uses[dnode->id].pcode;
                        if (use->block && func_005468E0(def, node->id, use)) {
                            del = 0;
                            break;
                        }
                    }
                }
            }
            if (del) {
                changed = 1;
                deletepcode(def);
            }
        }
    }
    return changed;
}
