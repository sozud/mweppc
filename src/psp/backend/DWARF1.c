// mwccpsp 3.0.1 DWARF.c (0x531730..0x535110): DWARF 1 debug information
// (.debug and .line sections, one pair per function) for the MIPS ELF
// writer. Only the entry points that ObjGen and InstrSelection call are
// written here; the entry, type and line emitters they use are still the
// original's (func_005xxxxx).
//
// Every DWARF entry is a node of 0x1858 bytes in a tree: the root holds the
// file-level data entries, each function adds a unit (func_00535220) with the
// function entry, its parameters, a lexical block with its locals, and null
// entries ending each sibling chain. Field use depends on the kind of node, so
// the part past the common header is accessed by offset.
#include "compiler/common.h"
#include "compiler/CError.h"
#include "compiler/CMangler.h"
#include "compiler/CParser.h"
#include "compiler/CFunc.h"
#include "compiler/CPrep.h"
#include "compiler/CompilerTools.h"
#include "compiler/objects.h"
#include "compiler/types.h"
#include "psp/BE.h"
#include "driver/COSToolsCLT.h"
#include "psp/ELFgen.h"
#include "psp/ObjGen.h"
#include <string.h>

typedef struct DwNode {
    struct DwNode* next; // +0x00 next sibling
    struct DwNode* first; // +0x04 first child (previous sibling once appended)
    struct DwNode* last; // +0x08 last child; in a chain, the entry after this one
    struct DwNode* parent; // +0x0C (the root: the last data entry)
    SInt32 offset; // +0x10 (a unit: its kind)
    void* f14; // +0x14 (a unit: the .debug ELFSection)
    SInt16 tag; // +0x18 DW_TAG_*; -1 is a null entry
    UInt8 pad1A[2];
    void* f1C; // +0x1C (a unit: the .line ELFSection)
    void* f20; // +0x20 name, object or ELF symbol
    void* f24; // +0x24 link name (a unit: the .debug section symbol)
    void* f28; // +0x28 type (a unit: the .line section symbol)
    UInt8 body[0x1858 - 0x2C];
} DwNode;

#define DW_S16(n, off) (*(SInt16*)((char*)(n) + (off)))
#define DW_S32(n, off) (*(SInt32*)((char*)(n) + (off)))
#define DW_PTR(n, off) (*(void**)((char*)(n) + (off)))

// a type waiting to be emitted (func_00534450), hashed into dw_types
typedef struct DwType {
    struct DwType* next;
    void* unk04;
    void* unk08;
    Boolean emitted; // +0x0C
} DwType;

typedef struct DwFuncRef {
    Object* object;
    struct DwFuncRef* next;
} DwFuncRef;

typedef struct DwFrame {
    UInt8 data[0x10];
} DwFrame;

typedef struct DwSavedRegs {
    UInt8 data[0x800];
} DwSavedRegs;

static SInt32 dw_active; // 0x604840 set up for this file
static char dw_filename[0x400]; // 0x604844
static SInt32 dw_unk604C44; // 0x604C44
static DwType* dw_types[0x400]; // 0x604C48
static DwNode* dw_root; // 0x605C4C
static DwNode* dw_current; // 0x605C50 entries are added under it
static DwNode* dw_unit; // 0x605C54 the unit func_00535220 made
static DwNode* dw_units; // 0x605C58
static void* dw_unk605C5C; // 0x605C5C
static void* dw_unk605C60; // 0x605C60
static void* dw_unk605C64; // 0x605C64
static DwFuncRef* dw_funcrefs; // 0x605C68 functions referenced (declared at the next function)
static Boolean dw_changed; // 0x605C6C

extern DwFrame data_0060EF38; // the frame of the function being finished (BE_macro.c)
extern DwSavedRegs data_00610438; // integer registers saved
extern DwSavedRegs data_0060A020; // FP registers saved as singles
extern DwSavedRegs data_00609820; // FP registers saved as doubles
extern SInt32 data_0062597C; // frame size
extern SInt16 data_00625E3E; // frame register
extern ObjectList* data_006242D4; // the function's static locals

void func_00535220(int kind); // begin a unit (dw_unit)
void* func_00535110(Type* type); // the type's reference
SInt32* func_00532800(SInt32 base, SInt32 size);
extern SInt32 func_005328B0(DwNode* unit);
extern void func_00532990(DwNode* node);
extern void func_005331C0(DwNode* node);
extern void func_00534450(DwType* type);
extern SInt32 func_004A4530(Object* obj); // a local's frame offset
extern ELFSymbol* ObjGen_GetELFSymbol(Object* obj);
extern void NameHashExportReset(void);

void func_00531C10(Object* obj, Boolean global, ELFSymbol* sym, SInt32 size);

inline DwNode* DW_NewNode(UInt16 tag) {
    DwNode* node;

    CError_ASSERT(111, tag > 0);
    node = galloc(sizeof(DwNode));
    memclrw(node, sizeof(DwNode));
    node->tag = tag;
    if (dw_current->last) {
        dw_current->last->next = node;
        node->first = dw_current->last;
    } else {
        dw_current->first = node;
    }
    node->last = NULL;
    node->parent = dw_current;
    dw_current->last = node;
    return dw_current->last;
}

// a function the code refers to, declared when the next function is written
void func_00531730(Object* obj) {
    DwFuncRef* prev;
    DwFuncRef* ref;

    if (copts.mips_option132 != 1) {
        return;
    }
    if (!copts.filesyminfo) {
        return;
    }
    if (!dw_active) {
        return;
    }
    if (obj->name->name[0] == '@') {
        return;
    }
    if (obj->type->type == 10) {
        return;
    }

    for (prev = ref = dw_funcrefs; ref; prev = ref, ref = ref->next) {
        if (ref->object == obj) {
            if (prev != ref) {
                prev->next = ref->next;
                ref->next = dw_funcrefs;
                dw_funcrefs = ref;
            }
            return;
        }
    }

    ref = galloc(sizeof(DwFuncRef));
    ref->object = obj;
    ref->next = dw_funcrefs;
    dw_funcrefs = ref;
}

// end of file: emit the pending types, then patch each unit's references
void func_005317D0(void) {
    DwNode* unit;
    DwNode* entry;
    DwNode* func;
    DwNode* node;
    DwType* type;
    int i;
    SInt32 lineoffset;
    char* data;

    if (!copts.filesyminfo || !dw_active) {
        return;
    }

    dw_current = dw_root;
    do {
        dw_changed = FALSE;
        for (i = 0; i < 0x400; i++) {
            for (type = dw_types[i]; type; type = type->next) {
                if (!dw_changed && !type->emitted) {
                    func_00534450(type);
                }
            }
        }
    } while (dw_changed);

    if (dw_current->parent) {
        DW_NewNode(0xFFFF);
        dw_current->parent->last = dw_current->last;
    }

    NameHashExportReset();
    for (unit = dw_units; unit; unit = unit->next) {
        dw_current = unit;
        ((ELFSymbol*)unit->f24)->sym.st_value = ((ELFSection*)unit->f14)->data.size;
        entry = unit->first;
        if (unit->next) {
            entry->last = unit->next->first;
        }
        if (unit->offset == 2) {
            dw_unk605C5C = entry->f20;
        }

        for (node = unit->first; node; node = node->next) {
            func_005331C0(node);
        }
        for (node = unit->first; node; node = node->next) {
            func_00532990(node);
        }

        if (unit->offset == 2) {
            lineoffset = func_005328B0(unit);
            func = entry->f20;
            if (func) {
                data = *((ELFSection*)entry->parent->f14)->data.data;
                *(SInt32*)(data + entry->offset + 0xE) = 0;
                ELF_AddReloc(2, entry->parent->f24, entry->offset + 0xE, DW_PTR(func, 0x30), 0);
                data = *((ELFSection*)entry->parent->f14)->data.data;
                *(SInt32*)(data + entry->offset + 0x14) = DW_S32(func, 0x34);
                ELF_AddReloc(2, entry->parent->f24, entry->offset + 0x14, DW_PTR(func, 0x30), 0);
                ELF_AddReloc(2, unit->f24, entry->offset + 0x1A, unit->f28, 0);
                ELF_AddReloc(2, unit->f28, lineoffset + 4, DW_PTR(func, 0x30), 0);
            }
        }

        data = *((ELFSection*)entry->parent->f14)->data.data;
        *(SInt32*)(data + entry->offset + 8) = ((ELFSection*)unit->f14)->data.size;
        ELF_AddReloc(2, entry->parent->f24, entry->offset + 8, unit->f24, 0);
    }
}

// start of file
void func_00531A60(void) {
    int i;

    if (!copts.filesyminfo) {
        return;
    }

    dw_units = NULL;
    dw_unit = NULL;
    dw_current = NULL;
    dw_unk604C44 = 0;
    dw_funcrefs = NULL;
    dw_unk605C64 = NULL;
    dw_unk605C60 = NULL;
    dw_unk605C5C = NULL;
    func_00535220(1);
    dw_current = dw_root = dw_unit;
    DW_NewNode(0x11);

    if (copts.absolutepath) {
        COS_FileGetPathName(dw_filename, &cparamblkptr->sourcefile, NULL);
    } else {
        COS_FileGetFSSpecInfo(&cparamblkptr->sourcefile, NULL, 0x104, dw_filename, 0x100);
    }

    for (i = 0; i < 0x400; i++) {
        dw_types[i] = NULL;
    }
    dw_active = 1;
}

// a data object (global, or a function's static)
void func_00531C10(Object* obj, Boolean global, ELFSymbol* sym, SInt32 size) {
    ObjAddress addr;
    DwNode* node;

    if (!copts.filesyminfo || !dw_active) {
        return;
    }
    if (obj->debugInfo.entry) {
        return;
    }
    if (obj->name->name[0] == '@') {
        return;
    }
    if (obj->type->type == 10) {
        return;
    }

    ObjGen_GetObjectAddress(obj, &addr);
    dw_current = dw_root;
    node = DW_NewNode(global ? 7 : 0xC);
    obj->debugInfo.entry = (struct DwarfNode*)node;
    node->f20 = obj->name;
    node->f24 = CMangler_GetLinkName(obj);
    node->f28 = func_00535110(obj->type);
    DW_S16(node, 0x2C) = (copts.mips_option07 && addr.basereg == 28) ? 6 : 3;
    DW_S32(node, 0x34) = 0;
    DW_PTR(node, 0x38) = sym;
    if (dw_current->parent) {
        dw_current->parent->last = node;
    }
    dw_current->parent = node;
}

// a parameter's or local's location
#define DW_SET_LOCATION(node, obj, vi, line) \
    do { \
        if ((vi)->regHi) { \
            DW_S16(node, 0x2C) = 2; \
            DW_S16(node, 0x30) = (vi)->regHi; \
            DW_S16(node, 0x2E) = (vi)->reg; \
        } else if ((vi)->reg) { \
            switch ((vi)->rclass) { \
                case 0: \
                    DW_S16(node, 0x2C) = 1; \
                    break; \
                case 1: \
                    DW_S16(node, 0x2C) = 5; \
                    break; \
                default: \
                    CError_FATAL(line); \
            } \
            DW_S16(node, 0x2E) = (vi)->reg; \
        } else { \
            DW_S16(node, 0x2C) = 4; \
            DW_S16(node, 0x2E) = data_00625E3E; \
            DW_S32(node, 0x34) = func_004A4530(obj); \
            DW_S32(node, 0x38) = 0; \
        } \
    } while (0)

// a function, after its code: the referenced functions, then a unit with the
// function entry, its parameters and a lexical block of its locals
void func_00531D90(Object* obj, Boolean global, ELFSymbol* sym, SInt32 size) {
    ObjAddress addr;
    DwNode* block;
    DwNode* cu;
    SInt32 numparams;
    SInt32 argoffset;
    DwNode* func;
    DwNode* prev;
    DwNode* node;
    DwNode* unit;
    DwFuncRef* ref;
    ObjectList* list;
    Object* o;
    Type* type;
    VarInfo* vi;
    SInt32 align;
    char* linename;
    char* debugname;
    char* name;

    if (!copts.filesyminfo || !dw_active) {
        return;
    }

    func_00535220(2);
    dw_current = unit = dw_unit;
    for (ref = dw_funcrefs; ref; ref = ref->next) {
        CMangler_GetLinkName(ref->object);
        func_00531C10(ref->object, TRUE, ObjGen_GetELFSymbol(ref->object), ref->object->type->size);
    }
    dw_current = unit;
    DW_PTR(unit, 0x2C) = dw_funcrefs;
    DW_PTR(dw_current, 0x30) = dw_unk605C64;
    dw_funcrefs = NULL;
    dw_unk605C64 = NULL;
    dw_unk605C60 = NULL;

    cu = DW_NewNode(0x11);
    func = DW_NewNode(global ? 6 : 0x14);
    func->f20 = obj->name;
    func->body[0x46 - 0x2C] = 0;
    func->f24 = CMangler_GetLinkName(obj);
    func->f28 = func_00535110(TYPE_FUNC(obj->type)->functype);
    DW_PTR(func, 0x30) = sym;
    DW_S32(func, 0x34) = size;
    DW_PTR(func, 0x40) = func_00532800(0, size);
    *(DwFrame*)((char*)func + 0x48) = data_0060EF38;
    *(DwSavedRegs*)((char*)func + 0x58) = data_00610438;
    *(DwSavedRegs*)((char*)func + 0x858) = data_0060A020;
    *(DwSavedRegs*)((char*)func + 0x1058) = data_00609820;

    name = ((HashNameNode*)func->f24)->name;
    linename = galloc(strlen(name) + 8);
    strcpy(linename, ".line_");
    strcpy(linename + 6, ((HashNameNode*)func->f24)->name);
    debugname = galloc(strlen(((HashNameNode*)func->f24)->name) + 9);
    strcpy(debugname, ".debug_");
    strcpy(debugname + 7, ((HashNameNode*)func->f24)->name);
    dw_current->f24 =
        ELF_NewUndefinedSymbol(debugname, ((ELFSection*)dw_current->f14)->index, 0, 3, 0, 0);
    dw_current->f28 =
        ELF_NewUndefinedSymbol(linename, ((ELFSection*)dw_current->f1C)->index, 0, 3, 0, 0);

    argoffset = 0;
    numparams = 0;
    prev = NULL;
    for (list = arguments; list; list = list->next) {
        o = list->object;
        type = o->type;
        if (type->type == TYPEFLOAT && type->size == 8) {
            align = 8;
        } else {
            align = 4;
        }
        argoffset += type->size;
        argoffset = (argoffset + (align - 1)) & ~(align - 1);
        vi = o->u.var.info;
        if (vi->used && o->name->name[0] != '@') {
            node = DW_NewNode(5);
            node->f20 = o->name;
            node->f28 = func_00535110(o->type);
            DW_SET_LOCATION(node, o, vi, 2388);
            numparams++;
            if (prev) {
                prev->last = node;
            }
            prev = node;
        }
    }

    block = DW_NewNode(0xB);
    block->f20 = sym;
    block->f24 = NULL;
    block->f28 = (void*)size;
    if (prev) {
        prev->last = block;
    }

    prev = NULL;
    for (list = locals; list; list = list->next) {
        o = list->object;
        vi = o->u.var.info;
        if (vi->used && o->name->name[0] != '@') {
            node = DW_NewNode(0xC);
            node->f20 = o->name;
            node->f28 = func_00535110(o->type);
            DW_SET_LOCATION(node, o, vi, 2440);
            if (prev) {
                prev->last = node;
            }
            prev = node;
        }
    }

    for (list = data_006242D4; list; list = list->next) {
        o = list->object;
        if (!o->debugInfo.entry) {
            node = DW_NewNode(0xC);
            o->debugInfo.entry = (struct DwarfNode*)node;
            if (copts.mips_option07) {
                ObjGen_GetObjectAddress(list->object, &addr);
            }
            node->f20 = o->name;
            node->f24 = CMangler_GetLinkName(o);
            node->f28 = func_00535110(o->type);
            DW_S16(node, 0x2C) = (copts.mips_option07 && addr.basereg == 28) ? 6 : 3;
            DW_S32(node, 0x34) = 0;
            DW_PTR(node, 0x38) = ObjGen_GetELFSymbol(o);
            if (prev) {
                prev->last = node;
            }
            prev = node;
        }
    }
    data_006242D4 = NULL;

    if (prev) {
        DW_NewNode(0xFFFF);
        prev->last = dw_current->last;
    }
    DW_NewNode(0xFFFF);
    block->last = dw_current->last;
    DW_NewNode(0xFFFF);
    func->last = dw_current->last;

    DW_S32(func, 0x38) = argoffset;
    DW_S32(func, 0x3C) = data_0062597C;
    DW_S16(func, 0x2C) = numparams;
    cu->f20 = func;
}

extern SInt32 data_006245BC;

// 0x535220: begin a unit: its .debug section (and for kind 2, the .line
// section), appended to dw_units
void func_00535220(int kind) {
    DwNode* unit;

    unit = galloc(0x3C);
    memclrw(unit, 0x3C);
    unit->offset = kind;
    unit->f14 = ELF_NewSection(".debug", 0x70000005, 0);
    DW_PTR(unit, 0x18) = ELF_NewSection(".rel.debug", SHT_REL, 0);
    ELF_SetRelSection(unit->f14, DW_PTR(unit, 0x18));
    ((ELFSection*)unit->f14)->shdr.sh_addralign = 0;
    if (kind == 2) {
        unit->f1C = ELF_NewSection(".line", 0x70000005, 0);
        unit->f20 = ELF_NewSection(".rel.line", SHT_REL, 0);
        ELF_SetRelSection(unit->f1C, unit->f20);
        ((ELFSection*)unit->f1C)->shdr.sh_addralign = 0;
    } else {
        unit->f24 =
            ELF_NewUndefinedSymbol(".debug", (UInt16)((ELFSection*)unit->f14)->index, 0, 3, 0, 0);
    }
    DW_S32(unit, 0x34) = data_006245BC;
    if (dw_unit) {
        dw_unit->next = unit;
    } else {
        dw_units = unit;
    }
    dw_unit = unit;
    *((UInt8*)unit + 0x38) = 0;
}

// 0x535110: the record of a type to describe, made (with the records of the
// types it refers to) the first time it is asked for
void* func_00535110(Type* type) {
    DwType* rec;
    StructMember* member;
    ClassList* base;
    ObjMemberVar* ivar;
    FuncArg* arg;
    int hash;

    hash = ((PtrInt)type >> 2) & 0x3FF;
    for (rec = dw_types[hash]; rec; rec = rec->next) {
        if (rec->unk04 == type) {
            return rec;
        }
    }
    rec = galloc(sizeof(DwType));
    rec->next = dw_types[hash];
    dw_types[hash] = rec;
    rec->unk04 = type;
    rec->emitted = FALSE;
    rec->unk08 = NULL;
    dw_changed = TRUE;
    switch (type->type) {
        case TYPESTRUCT:
            for (member = TYPE_STRUCT(type)->members; member; member = member->next) {
                func_00535110(member->type);
            }
            break;
        case TYPECLASS:
            for (base = TYPE_CLASS(type)->bases; base; base = base->next) {
                func_00535110(TYPE(base->base));
            }
            for (ivar = TYPE_CLASS(type)->ivars; ivar; ivar = ivar->next) {
                func_00535110(ivar->type);
            }
            break;
        case TYPEFUNC:
            func_00535110(TYPE_FUNC(type)->functype);
            for (arg = TYPE_FUNC(type)->args; arg && arg != &elipsis && arg != &oldstyle;
                 arg = arg->next) {
                func_00535110(arg->type);
            }
            break;
        case TYPEBITFIELD:
            func_00535110(TYPE_BITFIELD(type)->bitfieldtype);
            break;
        case TYPEPOINTER:
        case TYPEARRAY:
            func_00535110(TPTR_TARGET(type));
            break;
        default:
            break;
    }
    return rec;
}

extern MPCodeBlock* data_00624FD0; // the function's first block

// 0x532800: the function's line table: a count and the code size, then a
// (line, offset) pair at each change of source line
SInt32* func_00532800(SInt32 base, SInt32 size) {
    MPCodeBlock* block;
    MPCode* pc;
    SInt32* table;
    SInt32* entry;
    SInt32 count;
    SInt32 last;

    count = 0;
    last = -1;
    for (block = data_00624FD0; block; block = block->next) {
        for (pc = block->first; pc; pc = pc->next) {
            if (!(pc->flags & 0x40000000) && pc->unk20 > 0 && pc->unk20 != last) {
                last = pc->unk20;
                count++;
            }
        }
    }
    table = galloc((count + 1) * 8);
    table[0] = count;
    table[1] = size;
    entry = table;
    last = -1;
    for (block = data_00624FD0; block; block = block->next) {
        for (pc = block->first; pc; pc = pc->next) {
            if (!(pc->flags & 0x40000000) && pc->unk20 > 0 && pc->unk20 != last) {
                entry += 2;
                last = pc->unk20;
                entry[0] = last;
                entry[1] = pc->offset + base;
            }
        }
    }
    return table;
}
