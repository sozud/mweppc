#include "compiler/common.h"
#include "psp/BE.h"
#include "compiler/CompilerTools.h"
#include "compiler/CError.h"
#include "compiler/CException.h"
#include "compiler/CInit.h"
#include "compiler/Exceptions.h"
#include "compiler/objects.h"
#include "compiler/CFunc.h"
#include "compiler/CParser.h"

// mwccpsp 3.0.1 exception tables for the MIPS backend: registers that the
// unwinder needs, and the actions in effect at each call.

typedef struct PCAction {
    struct PCAction* next;
    struct PCAction* prev;
    MPCode* firstInstr;
    MPCode* lastInstr;
    ExceptionAction* actions;
    struct EANode* node;
} PCAction;

extern PCAction* data_005F0C2C; // pc_actions
extern PCAction* data_005F0C28; // last_pc_action
extern void func_0046FA50(void); // makepcblock

// 0x49E310: initializeexceptiontables
void func_0049E310(void) {
    data_005F0C2C = data_005F0C28 = NULL;
}

// a local's home register, or 0
#define OBJECT_REG(obj) ((obj)->datatype == DLOCAL ? (obj)->u.var.info->reg : 0)

int func_0049E1C0(ExceptionAction* actions) {
    int count = 0;

    while (actions) {
        switch (actions->type) {
            case EAT_DESTROYLOCALCOND:
                if (OBJECT_REG(actions->data.destroy_local_cond.cond)) {
                    count++;
                }
                break;
            case EAT_DESTROYLOCALPOINTER:
                if (OBJECT_REG(actions->data.destroy_local_pointer.pointer)) {
                    count++;
                }
                break;
            case EAT_DESTROYMEMBER:
                if (OBJECT_REG(actions->data.destroy_member.objectptr)) {
                    count++;
                }
                break;
            case EAT_DESTROYBASE:
                if (OBJECT_REG(actions->data.destroy_base.objectptr)) {
                    count++;
                }
                break;
            case EAT_DESTROYMEMBERCOND:
                if (OBJECT_REG(actions->data.destroy_member_cond.cond)) {
                    count++;
                }
                if (OBJECT_REG(actions->data.destroy_member_cond.objectptr)) {
                    count++;
                }
                break;
            case EAT_DESTROYMEMBERARRAY:
                if (OBJECT_REG(actions->data.destroy_member_array.objectptr)) {
                    count++;
                }
                break;
            case EAT_DELETEPOINTER:
            case EAT_DELETELOCALPOINTER:
                if (OBJECT_REG(actions->data.delete_pointer.pointerobject)) {
                    count++;
                }
                break;
            case EAT_DELETEPOINTERCOND:
                if (OBJECT_REG(actions->data.delete_pointer_cond.cond)) {
                    count++;
                }
                if (OBJECT_REG(actions->data.delete_pointer_cond.pointerobject)) {
                    count++;
                }
                break;
            default:
                break;
        }
        actions = actions->prev;
    }

    return count;
}

#define NOTE(o) \
    reg = OBJECT_REG(obj = (o)); \
    if (reg) { \
        ops->kind = 0; \
        ops->rclass = obj->u.var.info->rclass; \
        ops->reg = reg; \
        ops->flags = 9; \
        ops++; \
    }

void func_0049DE40(ExceptionAction* actions, MOperand* ops) {
    Object* obj;
    int reg;

    while (actions) {
        switch (actions->type) {
            case EAT_DESTROYLOCALCOND:
                NOTE(actions->data.destroy_local_cond.cond)
                break;
            case EAT_DESTROYLOCALPOINTER:
                NOTE(actions->data.destroy_local_pointer.pointer)
                break;
            case EAT_DESTROYMEMBER:
                NOTE(actions->data.destroy_member.objectptr)
                break;
            case EAT_DESTROYBASE:
                NOTE(actions->data.destroy_base.objectptr)
                break;
            case EAT_DESTROYMEMBERCOND:
                NOTE(actions->data.destroy_member_cond.cond)
                NOTE(actions->data.destroy_member_cond.objectptr)
                break;
            case EAT_DESTROYMEMBERARRAY:
                NOTE(actions->data.destroy_member_array.objectptr)
                break;
            case EAT_DELETEPOINTER:
            case EAT_DELETELOCALPOINTER:
                NOTE(actions->data.delete_pointer.pointerobject)
                break;
            case EAT_DELETEPOINTERCOND:
                NOTE(actions->data.delete_pointer_cond.cond)
                NOTE(actions->data.delete_pointer_cond.pointerobject)
                break;
            default:
                break;
        }
        actions = actions->prev;
    }
}

void func_0049DD90(MPCode* instr, ExceptionAction* actions) {
    PCAction* pca;

    if (!actions && (!data_005F0C28 || !data_005F0C28->actions)) {
        return;
    }

    pca = lalloc(sizeof(PCAction));
    pca->next = NULL;
    pca->firstInstr = pca->lastInstr = instr;
    pca->actions = actions;
    pca->prev = data_005F0C28;
    if (data_005F0C28) {
        data_005F0C28->next = pca;
    } else {
        data_005F0C2C = pca;
    }
    data_005F0C28 = pca;

    func_0046FA50();
    while (actions) {
        if (actions->type == EAT_CATCHBLOCK) {
            pcbranch(instr->block, (MPCodeLabel*)actions->data.catch_block.catch_label->pclabel, 2);
        } else if (actions->type == EAT_SPECIFICATION) {
            pcbranch(instr->block, (MPCodeLabel*)actions->data.catch_block.catch_label->pclabel, 2);
        }
        actions = actions->prev;
    }
}

extern ObjectList* data_00625008; // locals the exception actions refer to

// Note a local variable that the exception tables refer to (once).
void func_0049E150(Object* obj) {
    ObjectList* list;

    if (!obj) {
        return;
    }
    if (obj->otype != OT_OBJECT) {
        return;
    }
    if (obj->datatype != DLOCAL) {
        return;
    }
    for (list = data_00625008; list; list = list->next) {
        if (list->object == obj) {
            return;
        }
    }
    list = lalloc(sizeof(ObjectList));
    memclrw(list, sizeof(ObjectList));
    list->object = obj;
    list->next = data_00625008;
    data_00625008 = list;
}

// Note every local the actions refer to.
void func_0049E0B0(ExceptionAction* actions) {
    while (actions) {
        switch (actions->type) {
            case EAT_DESTROYLOCAL:
                func_0049E150(actions->data.destroy_local.local);
                break;
            case EAT_DESTROYLOCALCOND:
                func_0049E150(actions->data.destroy_local_cond.local);
                func_0049E150(actions->data.destroy_local_cond.cond);
                break;
            case EAT_DESTROYLOCALOFFSET:
                func_0049E150(actions->data.destroy_local_offset.local);
                break;
            case EAT_DESTROYLOCALPOINTER:
                func_0049E150(actions->data.destroy_local_pointer.pointer);
                break;
            case EAT_DESTROYLOCALARRAY:
                func_0049E150(actions->data.destroy_local_array.localarray);
                break;
            case EAT_DESTROYBASE:
                func_0049E150(actions->data.destroy_base.objectptr);
                break;
            case EAT_DESTROYPARTIALARRAY:
                func_0049E150(actions->data.destroy_partial_array.arraypointer);
                func_0049E150(actions->data.destroy_partial_array.arraycounter);
                func_0049E150(actions->data.destroy_partial_array.element_size);
                break;
            case EAT_DESTROYMEMBER:
                func_0049E150(actions->data.destroy_member.objectptr);
                break;
            case EAT_DESTROYMEMBERCOND:
                func_0049E150(actions->data.destroy_member_cond.objectptr);
                func_0049E150(actions->data.destroy_member_cond.cond);
                break;
            case EAT_DESTROYMEMBERARRAY:
                func_0049E150(actions->data.destroy_member_array.objectptr);
                break;
            case EAT_DELETEPOINTER:
            case EAT_DELETELOCALPOINTER:
                func_0049E150(actions->data.delete_pointer.pointerobject);
                break;
            case EAT_DELETEPOINTERCOND:
                func_0049E150(actions->data.delete_pointer_cond.pointerobject);
                func_0049E150(actions->data.delete_pointer_cond.cond);
                break;
            case EAT_CATCHBLOCK:
                func_0049E150(actions->data.catch_block.catch_object);
                func_0049E150(actions->data.catch_block.catch_info_object);
                break;
            case EAT_ACTIVECATCHBLOCK:
                func_0049E150(actions->data.active_catch_block.catch_info_object);
                break;
            case EAT_SPECIFICATION:
            case EAT_TERMINATE:
                break;
        }
        actions = actions->prev;
    }
}

extern GList data_005F0C18; // exceptmodule: the table being built

// 0x49F1F0: value in one to four bytes, big-endian, its length in the low
// bits of the first
int func_0049F1F0(UInt32 value, UInt8* p) {
    if (value < 0x80) {
        p[0] = value << 1;
        return 1;
    }
    if (value < 0x4000) {
        p[0] = ((value >> 8) << 2) | 1;
        p[1] = value;
        return 2;
    }
    if (value < 0x200000) {
        p[0] = ((value >> 16) << 3) | 3;
        p[1] = value >> 8;
        p[2] = value;
        return 3;
    }
    if (value < 0x20000000) {
        p[0] = ((value >> 24) << 3) | 7;
        p[1] = value >> 16;
        p[2] = value >> 8;
        p[3] = value;
        return 4;
    }
    CError_FATAL(166);
    return 0;
}

// 0x49F100: append value to the table
void func_0049F100(UInt32 value) {
    UInt8 buf[4];

    AppendGListData(&data_005F0C18, buf, func_0049F1F0(value, buf));
}

typedef struct EANode {
    struct EANode* dagListNext;
    struct EANode* prev;
    ExceptionAction* action;
    UInt16 count;
    UInt16 actionOffset;
} EANode;

extern EANode* data_00608DF8[20]; // DAG: the nodes made, by action type

// 0x49F130: makeEAnode: the node for an action chain, shared between chains
// with the same tail
EANode* func_0049F130(ExceptionAction* ea) {
    EANode* prev;
    EANode* node;

    for (node = data_00608DF8[ea->type]; node; node = node->dagListNext) {
        if (node->action == ea) {
            return node;
        }
    }
    if (ea->prev) {
        prev = func_0049F130(ea->prev);
    } else {
        prev = NULL;
    }
    for (node = data_00608DF8[ea->type]; node; node = node->dagListNext) {
        if (node->prev == prev && CExcept_ActionCompare(node->action, ea)) {
            return node;
        }
    }
    node = lalloc(sizeof(EANode));
    node->prev = prev;
    node->action = ea;
    node->count = 0;
    node->dagListNext = data_00608DF8[ea->type];
    data_00608DF8[ea->type] = node;
    if (prev) {
        prev->count++;
    }
    return node;
}

static void deleteexceptionaction(PCAction* pca) {
    if (pca->prev) {
        pca->prev->next = pca->next;
    } else {
        data_005F0C2C = pca->next;
    }
    if (pca->next) {
        pca->next->prev = pca->prev;
    }
}

// 0x49DC10: mergeexceptionactions: drop the ranges of deleted blocks, merge
// neighbours with the same actions, and count the ranges left
int func_0049DC10(void) {
    int count;
    int i;
    PCAction* pca;
    PCAction* prev;

    if (!data_005F0C2C) {
        return 0;
    }
    for (pca = data_005F0C2C; pca; pca = pca->next) {
        if (pca->firstInstr->block->flags2E & 0x20) {
            deleteexceptionaction(pca);
        }
    }
    if (!data_005F0C2C) {
        return 0;
    }
    for (i = 0; i < 20; i++) {
        data_00608DF8[i] = NULL;
    }
    for (pca = data_005F0C2C; pca; pca = pca->next) {
        pca->node = pca->actions ? func_0049F130(pca->actions) : NULL;
    }
    prev = data_005F0C2C;
    for (pca = data_005F0C2C->next; pca; pca = pca->next) {
        if (pca->node == prev->node) {
            prev->lastInstr = pca->lastInstr;
            deleteexceptionaction(pca);
        } else {
            prev = pca;
        }
    }
    count = 0;
    for (pca = data_005F0C2C; pca; pca = pca->next) {
        if (!pca->actions) {
            deleteexceptionaction(pca);
        } else {
            count++;
        }
    }
    return count;
}

extern Boolean data_00625EAE; // the function saves registers in its own way (interrupt)
extern SInt32 data_00625478[7]; // registers used, per class (RegClasses.c)
extern SInt32 data_0062421C; // frame alignment
extern SInt32 data_00624FCC;
extern SInt32 data_0062597C; // frame size
extern SInt32 data_006242B4; // outgoing arguments' size

// 0x540710 (a unit of its own after MultByConst.c): the frame description at
// the head of an exception table
#ifdef __MWERKS__
#pragma dont_inline on
#endif
void func_00540710(GList* list) {
    char flags;

    flags = data_00625478[0];
    if (data_00625EAE) {
        flags |= 0x20;
    }
    if ((copts.fp_library & 1) && data_00625478[1]) {
        flags |= 0x40;
    }
    AppendGListByte(list, flags);
    func_0049F100(((data_00624FCC + (data_0062421C - 1)) & ~(data_0062421C - 1)) + data_0062597C);
    func_0049F100(data_006242B4);
    if ((copts.fp_library & 1) && data_00625478[1]) {
        func_0049F100(data_00625478[1]);
    }
}
#ifdef __MWERKS__
#pragma dont_inline reset
#endif

extern OLinkList* data_005F0C14; // except_refs
extern OLinkList* data_005F0C10; // last_except_ref
extern SInt32 func_004A4530(Object* obj); // a local's frame offset

#define LOCAL_REG(obj) ((obj)->datatype == DLOCAL ? (obj)->u.var.info->reg : 0)

// 0x49F2A0: a signed value in one to four bytes, as func_0049F1F0
int func_0049F2A0(SInt32 value, UInt8* p) {
    if (value >= -0x40 && value < 0x40) {
        p[0] = value << 1;
        return 1;
    }
    if (value >= -0x2000 && value < 0x2000) {
        p[0] = ((value >> 8) << 2) | 1;
        p[1] = value;
        return 2;
    }
    if (value >= -0x100000 && value < 0x100000) {
        p[0] = ((value >> 16) << 3) | 3;
        p[1] = value >> 8;
        p[2] = value;
        return 3;
    }
    if (value >= -0x10000000 && value < 0x10000000) {
        p[0] = ((value >> 24) << 3) | 7;
        p[1] = value >> 16;
        p[2] = value >> 8;
        p[3] = value;
        return 4;
    }
    CError_FATAL(109);
    return 0;
}

static inline void PutSigned(SInt32 value) {
    UInt8 buf[4];

    AppendGListData(&data_005F0C18, buf, func_0049F2A0(value, buf));
}

static inline void PutUnsigned(UInt32 value) {
    UInt8 buf[4];

    AppendGListData(&data_005F0C18, buf, func_0049F1F0(value, buf));
}

// a word for obj's address, relocated
static inline void PutReloc(Object* obj) {
    OLinkList* ref;
    SInt32 offset;

    offset = data_005F0C18.size;
    AppendGListLong(&data_005F0C18, 0);
    if (obj) {
        ref = lalloc(sizeof(OLinkList));
        ref->next = NULL;
        ref->obj = obj;
        ref->offset = offset;
        ref->addend = 0;
        if (data_005F0C14) {
            data_005F0C10->next = ref;
        } else {
            data_005F0C14 = ref;
        }
        data_005F0C10 = ref;
    }
}

static inline UInt32 CodeOffset(CLabel* label) {
    return ((MPCodeLabel*)label->pclabel)->block->codeoffset;
}

// 0x49E330: allocateactioninfo: the records for node's chain of actions, up
// to a chain written before
void func_0049E330(EANode* node) {
    ExceptionAction* ea;
    char flags;
    int reg;
    int reg2;
    int i;

    while (node && (node->actionOffset == 0 || node->prev == NULL)) {
        if (node->actionOffset == 0) {
            node->actionOffset = data_005F0C18.size;
        }
        flags = node->prev ? 0 : 0x80;
        ea = node->action;
        switch (ea->type) {
            case EAT_DESTROYLOCAL:
                AppendGListByte(&data_005F0C18, flags | 2);
                PutSigned(func_004A4530(ea->data.destroy_local.local));
                PutReloc(ea->data.destroy_local.dtor);
                break;
            case EAT_DESTROYLOCALCOND:
                reg = LOCAL_REG(ea->data.destroy_local_cond.cond);
                if (reg) {
                    flags |= 0x40;
                }
                AppendGListByte(&data_005F0C18, flags | 3);
                PutSigned(reg ? reg : func_004A4530(ea->data.destroy_local_cond.cond));
                PutSigned(func_004A4530(ea->data.destroy_local_cond.local));
                PutReloc(ea->data.destroy_local_cond.dtor);
                break;
            case EAT_DESTROYLOCALOFFSET:
                AppendGListByte(&data_005F0C18, flags | 2);
                PutSigned(
                    func_004A4530(ea->data.destroy_local_offset.local) +
                    ea->data.destroy_local_offset.offset
                );
                PutReloc(ea->data.destroy_local_offset.dtor);
                break;
            case EAT_DESTROYLOCALPOINTER:
                reg = LOCAL_REG(ea->data.destroy_local_pointer.pointer);
                if (reg) {
                    flags |= 0x20;
                }
                AppendGListByte(&data_005F0C18, flags | 4);
                PutSigned(reg ? reg : func_004A4530(ea->data.destroy_local_pointer.pointer));
                PutReloc(ea->data.destroy_local_pointer.dtor);
                break;
            case EAT_DESTROYLOCALARRAY:
                AppendGListByte(&data_005F0C18, flags | 5);
                PutSigned(func_004A4530(ea->data.destroy_local_array.localarray));
                PutUnsigned(ea->data.destroy_local_array.elements);
                PutUnsigned(ea->data.destroy_local_array.element_size);
                PutReloc(ea->data.destroy_local_array.dtor);
                break;
            case EAT_DESTROYBASE:
                reg = LOCAL_REG(ea->data.destroy_base.objectptr);
                if (reg) {
                    flags |= 0x20;
                }
                AppendGListByte(&data_005F0C18, flags | 6);
                PutSigned(reg ? reg : func_004A4530(ea->data.destroy_base.objectptr));
                PutSigned(ea->data.destroy_base.offset);
                PutReloc(ea->data.destroy_base.dtor);
                break;
            case EAT_DESTROYMEMBER:
                reg = LOCAL_REG(ea->data.destroy_member.objectptr);
                if (reg) {
                    flags |= 0x20;
                }
                AppendGListByte(&data_005F0C18, flags | 7);
                PutSigned(reg ? reg : func_004A4530(ea->data.destroy_member.objectptr));
                PutSigned(ea->data.destroy_member.offset);
                PutReloc(ea->data.destroy_member.dtor);
                break;
            case EAT_DESTROYMEMBERCOND:
                reg2 = LOCAL_REG(ea->data.destroy_member_cond.cond);
                if (reg2) {
                    flags |= 0x40;
                }
                reg = LOCAL_REG(ea->data.destroy_member_cond.objectptr);
                if (reg) {
                    flags |= 0x20;
                }
                AppendGListByte(&data_005F0C18, flags | 8);
                PutSigned(reg2 ? reg2 : func_004A4530(ea->data.destroy_member_cond.cond));
                PutSigned(reg ? reg : func_004A4530(ea->data.destroy_member_cond.objectptr));
                PutSigned(ea->data.destroy_member_cond.offset);
                PutReloc(ea->data.destroy_member_cond.dtor);
                break;
            case EAT_DESTROYMEMBERARRAY:
                reg = LOCAL_REG(ea->data.destroy_member_array.objectptr);
                if (reg) {
                    flags |= 0x20;
                }
                AppendGListByte(&data_005F0C18, flags | 9);
                PutSigned(reg ? reg : func_004A4530(ea->data.destroy_member_array.objectptr));
                PutSigned(ea->data.destroy_member_array.offset);
                PutUnsigned(ea->data.destroy_member_array.elements);
                PutUnsigned(ea->data.destroy_member_array.element_size);
                PutReloc(ea->data.destroy_member_array.dtor);
                break;
            case EAT_DELETEPOINTER:
            case EAT_DELETELOCALPOINTER:
                reg = LOCAL_REG(ea->data.delete_pointer.pointerobject);
                if (reg) {
                    flags |= 0x20;
                }
                AppendGListByte(&data_005F0C18, flags | 0xA);
                PutSigned(reg ? reg : func_004A4530(ea->data.delete_pointer.pointerobject));
                PutReloc(ea->data.delete_pointer.deletefunc);
                break;
            case EAT_DELETEPOINTERCOND:
                reg2 = LOCAL_REG(ea->data.delete_pointer_cond.cond);
                if (reg2) {
                    flags |= 0x40;
                }
                reg = LOCAL_REG(ea->data.delete_pointer_cond.pointerobject);
                if (reg) {
                    flags |= 0x20;
                }
                AppendGListByte(&data_005F0C18, flags | 0xB);
                PutSigned(reg2 ? reg2 : func_004A4530(ea->data.delete_pointer_cond.cond));
                PutSigned(reg ? reg : func_004A4530(ea->data.delete_pointer_cond.pointerobject));
                PutReloc(ea->data.delete_pointer_cond.deletefunc);
                break;
            case EAT_CATCHBLOCK:
                AppendGListByte(&data_005F0C18, flags | 0xC);
                PutReloc(ea->data.catch_block.catch_typeid);
                PutUnsigned(CodeOffset(ea->data.catch_block.catch_label));
                PutSigned(func_004A4530(ea->data.catch_block.catch_info_object));
                break;
            case EAT_ACTIVECATCHBLOCK:
                AppendGListByte(&data_005F0C18, flags | 0xD);
                PutSigned(func_004A4530(ea->data.active_catch_block.catch_info_object));
                break;
            case EAT_TERMINATE:
                AppendGListByte(&data_005F0C18, flags | 0xE);
                break;
            case EAT_SPECIFICATION:
                AppendGListByte(&data_005F0C18, flags | 0xF);
                PutUnsigned(ea->data.specification.unexp_ids);
                PutUnsigned(CodeOffset(ea->data.specification.unexp_label));
                PutSigned(func_004A4530(ea->data.specification.unexp_info_object));
                for (i = 0; i < ea->data.specification.unexp_ids; i++) {
                    PutReloc(ea->data.specification.unexp_id[i]);
                }
                break;
            default:
                CError_FATAL(571);
                break;
        }
        node = node->prev;
    }
    if (node) {
        reg = node->actionOffset - data_005F0C18.size;
        AppendGListByte(&data_005F0C18, 1);
        PutSigned(reg);
    }
}

extern void func_004624F0(
    Object* func, SInt32 size, void* data, SInt32 datasize, OLinkList* refs
); // ObjGen_DeclareExceptionTables

// the pc of an instruction for the table: branches (0x37E..0x3E5) count
// from their delay slot
static inline SInt32 TablePC(MPCode* pc) {
    if (pc->op >= 0x37E && pc->op <= 0x3E5) {
        return pc->offset + 6;
    }
    return pc->offset + 8;
}

// 0x49D9B0: dumpexceptiontables: the frame description, the ranges (sized
// by trial until the encoding fits) and the action records
void func_0049D9B0(Object* func, SInt32 size) {
    PCAction* pca;
    EANode* node;
    UInt8* p;
    SInt32 rangesize;
    SInt32 start;
    SInt32 last;
    SInt32 first;
    SInt32 length;
    int count;

    count = func_0049DC10();
    InitGList(&data_005F0C18, 256);
    data_005F0C10 = NULL;
    data_005F0C14 = data_005F0C10;
    rangesize = count * 12;
    for (;;) {
        func_00540710(&data_005F0C18);
        start = data_005F0C18.size;
        AppendGListNoData(&data_005F0C18, rangesize);
        AppendGListByte(&data_005F0C18, 0);
        for (pca = data_005F0C2C; pca; pca = pca->next) {
            for (node = pca->node; node; node = node->prev) {
                node->actionOffset = 0;
            }
        }
        for (pca = data_005F0C2C; pca; pca = pca->next) {
            node = pca->node;
            if (node->count == 0 && node->actionOffset == 0) {
                func_0049E330(node);
            }
        }
        p = (UInt8*)*data_005F0C18.data + start;
        last = 0;
        for (pca = data_005F0C2C; pca; pca = pca->next) {
            first = TablePC(pca->firstInstr);
            length = TablePC(pca->lastInstr) - first;
            CError_ASSERT(1086, first >= last);
            p += func_0049F1F0(first - last, p);
            p += func_0049F1F0(length, p);
            p += func_0049F1F0(pca->node->actionOffset, p);
            last = first + length;
        }
        length = p - ((UInt8*)*data_005F0C18.data + start);
        if (length == rangesize) {
            break;
        }
        data_005F0C18.size = 0;
        data_005F0C10 = NULL;
        rangesize = length;
        data_005F0C14 = data_005F0C10;
    }
    LockGList(&data_005F0C18);
    func_004624F0(func, size, *data_005F0C18.data, data_005F0C18.size, data_005F0C14);
    FreeGList(&data_005F0C18);
}
