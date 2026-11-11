// mwccpsp 3.0.1: the back end's per-function optimizer driver
// (0x4AF700..0x4AF9FF). The original unit name is not known.
#include "compiler/common.h"
#include "compiler/CParser.h"
#include "compiler/CMangler.h"
#include "compiler/CompilerTools.h"
#include "compiler/objects.h"
#include "psp/BE.h"

extern void gather_alias_info(void); // 0x481370
extern void func_004AFFF0(char* name, char* title); // pcode listing
extern void func_004AFBE0(Object* func); // peephole
void func_004AFA00(Object* func, Boolean all); // below
extern void func_004B11D0(int unk); // removecommonsubexpressions
extern void func_00545080(Object* func, int unk); // propagatecopyinstructions
extern void func_0053C140(int unk);
extern int func_00545DA0(void); // memory store/load optimization
extern void func_0053BF00(int unk);
extern void func_004B35D0(Object* func);
extern void func_004B3650(void);
extern SInt32 data_00624BA0;

#define LISTING(name, title) \
    do { \
        if (copts.debuglisting) { \
            func_004AFFF0((name), (title)); \
        } \
    } while (0)

// 0x4AF7A0 (level 4)
static void GlobalOptimize4(Object* func) {
    char* name;

    name = CMangler_GetLinkName(func)->name;
    if (!copts.commonsubs) {
        gather_alias_info();
    } else {
        func_004B11D0(0);
        LISTING(name, "After removecommonsubexpressions");
    }
    func_00545080(func, 0);
    LISTING(name, "After propagatecopyinstructions");
    if (copts.peephole) {
        func_004AFA00(func, 0);
        func_004AFBE0(func);
        LISTING(name, "After peephole");
    }
    func_0053C140(1);
    if (func_00545DA0()) {
        LISTING(name, "AFTER MEMORY STORE-LOAD OPTIMIZATION");
        freeoheap();
        func_0053BF00(1);
        func_00545080(func, 0);
        LISTING(name, "After propagatecopyinstructions #2");
        func_0053C140(1);
    }
}

// 0x4AF8B0 (level 3)
static void GlobalOptimize3(Object* func) {
    char* name;

    name = CMangler_GetLinkName(func)->name;
    if (!copts.commonsubs) {
        gather_alias_info();
    } else {
        func_004B11D0(0);
        LISTING(name, "After removecommonsubexpressions");
    }
    func_00545080(func, 0);
    LISTING(name, "After propagatecopyinstructions");
    if (copts.peephole) {
        func_004AFA00(func, 0);
        func_004AFBE0(func);
        LISTING(name, "After peephole");
    }
    func_004B35D0(func);
    func_004B3650();
}

// 0x4AF960 (level 2, and higher levels in some cases)
static void GlobalOptimize2(Object* func) {
    char* name;

    name = CMangler_GetLinkName(func)->name;
    if (!copts.commonsubs) {
        gather_alias_info();
    } else {
        func_004B11D0(0);
        LISTING(name, "After removecommonsubexpressions");
    }
    func_00545080(func, 0);
    LISTING(name, "After propagatecopyinstructions");
    if (copts.peephole) {
        func_004AFBE0(func);
        LISTING(name, "After peephole");
    }
}

// 0x4AF700
void func_004AF700(Object* func) {
    HashNameNode* name;

    if (copts.optimizationlevel == 1) {
        name = CMangler_GetLinkName(func);
        if (copts.peephole) {
            gather_alias_info();
            func_004AFBE0(func);
            LISTING(name->name, "After peephole");
        }
    } else if (copts.optimizationlevel == 2 || (data_00624BA0 && copts.optimizationlevel > 2)) {
        GlobalOptimize2(func);
    } else if (copts.optimizationlevel == 3) {
        GlobalOptimize3(func);
    } else if (copts.optimizationlevel == 4) {
        GlobalOptimize4(func);
    }
}

extern MPCodeBlock* data_00624C18; // the function's first labelled block (stays)

// 0x4AFA00: a block that control can only reach by falling through from
// the one before it (and that leaves it only that way) is appended to it,
// as long as the result stays within 100 instructions. Unless `all`, loop
// headers and blocks marked 1 are left alone.
void func_004AFA00(Object* func, Boolean all) {
    MPCodeBlock* block;
    MPCodeBlock* next;
    MPCode* pc;
    MPCode* pnext;
    MPBlockLink* link;
    Boolean nextIsLoop;

    for (block = data_00624FD0; block; block = block->next) {
        nextIsLoop = FALSE;
        next = block->next;
        if (!next) {
            return;
        }
        if (!all) {
            nextIsLoop = next != NULL;
            if (nextIsLoop) {
                nextIsLoop = (next->flags2E & 2) != 0;
            }
            if (block->flags2E & 1) {
                continue;
            }
        }
        if (block->pcodecount <= 0) {
            continue;
        }
        for (pc = block->first; pc && !(pc->flags & 0x108); pc = pc->next) {}
        if (pc) {
            continue;
        }
        pc = block->last;
        if (pc && pc->op == 0x39) {
            if (pc->args[0].kind != 6 || ((MPCodeLabel*)pc->args[0].ptr)->block != next) {
                continue;
            }
            deletepcode(pc);
        }
        pc = block->last;
        if (pc && (pc->flags & 1) && pc->op != 0x39) {
            continue;
        }

merge:
        link = block->successors;
        if (link->block != next ||
            !link ||
            link->next ||
            !next ||
            !next->predecessors ||
            next->predecessors->from != block ||
            !next->predecessors ||
            next->predecessors->nextpred ||
            nextIsLoop) {
            continue;
        }
        if (next->pcodecount + block->pcodecount > 100) {
            continue;
        }
        if (next->pcodecount > 0) {
            for (pc = next->first; pc && !(pc->flags & 0x108); pc = pnext) {
                pnext = pc->next;
                deletepcode(pc);
                if (pc->op == 0x39) {
                    if (pc->args[0].kind == 6) {
                        if (((MPCodeLabel*)pc->args[0].ptr)->block != next->next) {
                            appendpcode(block, pc);
                        }
                    } else {
                        appendpcode(block, pc);
                    }
                } else {
                    appendpcode(block, pc);
                }
            }
        }
        if (next->pcodecount != 0 || next == data_00624C18) {
            continue;
        }
        for (link = next->successors; link; link = link->next) {
            linkblocks(block, link->block);
            unlinkblocks(next, link->block);
        }
        unlinkblocks(block, next);
        deleteblock(next);
        next = block->next;
        if (!all) {
            nextIsLoop = next != NULL;
            if (nextIsLoop) {
                nextIsLoop = (next->flags2E & 2) != 0;
            }
        }
        goto merge;
    }
}
