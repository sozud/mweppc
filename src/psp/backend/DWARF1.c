#if MWCCPS2 // the original unit is DWARF.c (its assert messages name it)
#line 4 "DWARF.c"
#endif
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
#if !MWCCPS2 // (PS2: named at the top)
#line 27 "DWARF.c" // 3.0.1 asserts name this file
#endif

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
#if MWCCPS2
    UInt8 body[0x658 - 0x2C]; // mwccps2 b210: 0x658-byte nodes (DW_NewNode, 0x531A60's twin)
#elif MWCCPSP_NATIVE
    UInt8 body[2 * (0x1858 - 0x2C)]; // every 32-bit slot widened to 8 bytes
#else
    UInt8 body[0x1858 - 0x2C];
#endif
} DwNode;

// a field at its 3.0.1 offset: natively the header has wider pointers, so the
// tag is reached by name and each 4-byte slot below 0x48 (where pointers are
// kept) is given 8 bytes; from 0x48 on (the frame and saved register
// records, copied whole) offsets stay linear
#if MWCCPSP_NATIVE
#define DW_ADDR(n, off) \
    ((off) == 0x18      ? (char*)&((DwNode*)(n))->tag \
         : (off) < 0x48 ? (char*)((DwNode*)(n))->body + 2 * ((off) - 0x2C) \
                        : (char*)((DwNode*)(n))->body + 2 * (0x48 - 0x2C) + ((off) - 0x48))
#else
#define DW_ADDR(n, off) ((char*)(n) + (off))
#endif
// a unit uses fields up to 0x3C
#if MWCCPSP_NATIVE
#define DW_UNIT_SIZE (offsetof(DwNode, body) + 2 * (0x3C - 0x2C))
#else
#define DW_UNIT_SIZE 0x3C
#endif
#define DW_S16(n, off) (*(SInt16*)DW_ADDR(n, off))
#define DW_S32(n, off) (*(SInt32*)DW_ADDR(n, off))
#define DW_PTR(n, off) (*(void**)DW_ADDR(n, off))

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
#if MWCCPS2
    UInt8 data[0x200]; // b210: 32 registers per class
#else
    UInt8 data[0x800];
#endif
} DwSavedRegs;

static SInt32 dw_active; // 0x604840 set up for this file
#if MWCCPS2 && MWCCPS2_BUILD < 210
#define DW_ACTIVE 1 // b145 has no dw_active: every test of it is gone
#define DW_SET_ACTIVE() ((void)0)
#else
#define DW_ACTIVE dw_active
#define DW_SET_ACTIVE() (dw_active = 1)
#endif
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
SInt32 func_005328B0(DwNode* unit);
void func_00532990(DwNode* node);
SInt32 func_005331C0(DwNode* node);
void func_00534450(DwType* type);
DwNode* func_00534750(Type* type);
DwNode* func_00534900(Type* type, Boolean top, SInt32 offset);
DwNode* func_00534D40(Type* type);
extern SInt32 func_004A4530(Object* obj); // a local's frame offset
extern ELFSymbol* ObjGen_GetELFSymbol(Object* obj);
extern void NameHashExportReset(void);

void func_00531C10(Object* obj, Boolean global, ELFSymbol* sym, SInt32 size);

inline DwNode* DW_NewNode(UInt16 tag) {
    DwNode* node;

#undef CError_B145_SHIFT
#define CError_B145_SHIFT -2 // b145 asserts
    CError_ASSERT(111, tag > 0);
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
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
    if (!copts.filesyminfo || !DW_ACTIVE) {
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

    if (!copts.filesyminfo || !DW_ACTIVE) {
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
    DW_SET_ACTIVE();
}

// a data object (global, or a function's static)
void func_00531C10(Object* obj, Boolean global, ELFSymbol* sym, SInt32 size) {
    ObjAddress addr;
    DwNode* node;

    if (!copts.filesyminfo || !DW_ACTIVE) {
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
#if MWCCPS2
    // b210: a data object kept in a register (copts+0x47, func_004B8980's
    // test inline) gets a register location
    {
        ObjAddress a2;
        VarInfo* vi;
        Boolean inreg;

        a2 = addr;
        if (a2.basereg == -1 &&
            copts.mips_option43 &&
            obj->datatype == DDATA &&
            (vi = obj->u.data.info) != NULL &&
            (vi->reg != 0 || vi->unk24[2] == 5 || (vi->unk24[3] != 0 && vi->unk24[2] == 0))) {
            inreg = TRUE;
        } else {
            inreg = FALSE;
        }
        if (inreg) {
            switch (obj->u.data.info->unk24[2]) {
                case 0:
                    DW_S16(node, 0x2C) = 1;
                    break;
                case 1:
                    DW_S16(node, 0x2C) = 5;
                    break;
                case 5:
                    DW_S16(node, 0x2C) = 1;
                    break;
                case 6:
                    DW_S16(node, 0x2C) = 1;
                    break;
            }
        } else {
            DW_S16(node, 0x2C) = (copts.mips_option07 && addr.basereg == 28) ? 6 : 3;
        }
    }
#else
    DW_S16(node, 0x2C) = (copts.mips_option07 && addr.basereg == 28) ? 6 : 3;
#endif
    DW_S32(node, 0x34) = 0;
    DW_PTR(node, 0x38) = sym;
    if (dw_current->parent) {
        dw_current->parent->last = node;
    }
    dw_current->parent = node;
}

// a parameter's or local's location
#if MWCCPS2
#define CError_PS2_LINE_DW(psp, ps2) (ps2)
#else
#define CError_PS2_LINE_DW(psp, ps2) (psp)
#endif
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
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -2 // b145 asserts
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

    if (!copts.filesyminfo || !DW_ACTIVE) {
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
    *(UInt8*)DW_ADDR(func, 0x46) = 0;
    func->f24 = CMangler_GetLinkName(obj);
    func->f28 = func_00535110(TYPE_FUNC(obj->type)->functype);
    DW_PTR(func, 0x30) = sym;
    DW_S32(func, 0x34) = size;
    DW_PTR(func, 0x40) = func_00532800(0, size);
#if MWCCPS2 // b210's 32-entry tables
    *(DwFrame*)((char*)func + 0x48) = data_0060EF38;
    *(DwSavedRegs*)((char*)func + 0x58) = data_00610438;
    *(DwSavedRegs*)((char*)func + 0x58 + sizeof(DwSavedRegs)) = data_0060A020;
    *(DwSavedRegs*)((char*)func + 0x58 + 2 * sizeof(DwSavedRegs)) = data_00609820;
#else
    *(DwFrame*)DW_ADDR(func, 0x48) = data_0060EF38;
    *(DwSavedRegs*)DW_ADDR(func, 0x58) = data_00610438;
    *(DwSavedRegs*)DW_ADDR(func, 0x858) = data_0060A020;
    *(DwSavedRegs*)DW_ADDR(func, 0x1058) = data_00609820;
#endif

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
            DW_SET_LOCATION(node, o, vi, CError_PS2_LINE_DW(2388, 2382));
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
            DW_SET_LOCATION(node, o, vi, CError_PS2_LINE_DW(2440, 2434));
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
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

extern HashNameNode* data_006245BC; // the source file (ObjGen_SrcBreakName)

// 0x535220: begin a unit: its .debug section (and for kind 2, the .line
// section), appended to dw_units
void func_00535220(int kind) {
    DwNode* unit;

    unit = galloc(DW_UNIT_SIZE);
    memclrw(unit, DW_UNIT_SIZE);
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
    DW_PTR(unit, 0x34) = data_006245BC;
    if (dw_unit) {
        dw_unit->next = unit;
    } else {
        dw_units = unit;
    }
    dw_unit = unit;
    *(UInt8*)DW_ADDR(unit, 0x38) = 0;
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

// 0x534450: the entry of a type: an array, enumeration or class here, the
// others by their own functions; a pointer stands for its target
void func_00534450(DwType* rec) {
    char anon[20];
    char anon2[20];
    Type* type;
    Type* elem;
    DwNode* node;
    ObjEnumConst* oec;
    HashNameNode* name;

    type = rec->unk04;
loop:
    if (rec->emitted) {
        return;
    }
    rec->emitted = TRUE;
    switch (type->type) {
        case TYPEVOID:
        case TYPEINT:
        case TYPEFLOAT:
            break;
        case TYPEPOINTER:
            type = TPTR_TARGET(type);
            rec = func_00535110(type);
            goto loop;
        case TYPEARRAY:
            elem = TPTR_TARGET(type);
            node = DW_NewNode(1);
            node->f20 = func_00535110(elem);
            node->f24 = (void*)(PtrInt)type->size;
            if (elem->size) {
                node->f28 = (void*)(PtrInt)(type->size / elem->size - 1);
            } else {
                node->f28 = 0;
            }
            rec->unk08 = node;
            break;
        case TYPEENUM:
            node = DW_NewNode(4);
            name = TYPE_ENUM(type)->enumname;
            if (!name) {
                sprintf(anon, "@anon%d", dw_unk604C44++);
                name = GetHashNameNodeExport(anon);
            }
            node->f20 = name;
            elem = TYPE_ENUM(type)->enumtype;
            node->f24 = (void*)(PtrInt)elem->size;
            node->f28 = 0;
            DW_PTR(node, 0x2C) = TYPE_ENUM(type)->enumlist;
            for (oec = TYPE_ENUM(type)->enumlist; oec; oec = oec->next) {
                node->f28 = (void*)((PtrInt)node->f28 + strlen(oec->name->name) + elem->size + 1);
            }
            rec->unk08 = node;
            break;
        case TYPESTRUCT:
            rec->unk08 = func_00534D40(type);
            break;
        case TYPECLASS:
            node = DW_NewNode(2);
            name = TYPE_CLASS(type)->classname;
            if (!name) {
                sprintf(anon2, "@anon%d", dw_unk604C44++);
                name = GetHashNameNodeExport(anon2);
            }
            node->f20 = name;
            node->f24 = (void*)(PtrInt)type->size;
            func_00534900(type, 1, 0);
            rec->unk08 = node;
            break;
        case TYPEFUNC:
            rec->unk08 = func_00534750(type);
            break;
        default:
            break;
    }
    node = rec->unk08;
    if (node) {
        if (dw_current->parent) {
            dw_current->parent->last = node;
        }
        dw_current->parent = node;
    }
}

// 0x534D40: a structure or union and its members (siblings that follow it,
// chained through last, ended by a null entry)
DwNode* func_00534D40(Type* type) {
    char anon[20];
    DwNode* node;
    DwNode* m;
    DwNode* prev;
    StructMember* member;
    HashNameNode* name;

    node = DW_NewNode(TYPE_STRUCT(type)->stype == 1 ? 0x17 : 0x13);
    name = TYPE_STRUCT(type)->name;
    if (!name) {
        sprintf(anon, "@anon%d", dw_unk604C44++);
        name = GetHashNameNodeExport(anon);
    }
    node->f20 = name;
    node->f24 = (void*)(PtrInt)type->size;

    prev = NULL;
    for (member = TYPE_STRUCT(type)->members; member; member = member->next) {
        m = DW_NewNode(0xD);
        m->f20 = member->name;
        DW_S32(m, 0x2C) = member->offset;
        if (member->type->type == TYPEBITFIELD) {
            m->f24 = func_00535110(TYPE_BITFIELD(member->type)->bitfieldtype);
            DW_S16(m, 0x32) = TYPE_BITFIELD(member->type)->offset;
            DW_S16(m, 0x30) = TYPE_BITFIELD(member->type)->bitlength;
        } else {
            m->f24 = func_00535110(member->type);
        }
        if (prev) {
            prev->last = m;
        }
        prev = m;
    }
    if (prev) {
        prev->last = DW_NewNode(0xFFFF);
    }
    return node;
}

// 0x534750: a function type and its parameters (siblings chained through
// last, ended by a null entry)
DwNode* func_00534750(Type* type) {
    DwNode* node;
    DwNode* p;
    DwNode* prev;
    FuncArg* arg;

    node = DW_NewNode(0x15);
    node->f28 = func_00535110(TYPE_FUNC(type)->functype);
    DW_S16(node, 0x2C) = 0;
    prev = NULL;
    for (arg = TYPE_FUNC(type)->args; arg && arg != &elipsis && arg != &oldstyle; arg = arg->next) {
        DW_S16(node, 0x2C)++;
        p = DW_NewNode(5);
        p->f28 = func_00535110(arg->type);
        if (prev) {
            prev->last = p;
        }
        prev = p;
    }
    if (prev) {
        prev->last = DW_NewNode(0xFFFF);
    }
    return node;
}

// the access attribute of a member or base
#define DW_ACCESS(n, access) \
    switch (access) { \
        case 0: \
            DW_S16(n, 0x28) = 0x288; \
            break; \
        case 1: \
            DW_S16(n, 0x28) = 0x248; \
            break; \
        case 2: \
            DW_S16(n, 0x28) = 0x268; \
            break; \
    }

// 0x534B80: a class's bases (inheritance entries; a virtual base also gets
// the member that points to it); the last entry made
static DwNode* func_00534B80(TypeClass* tclass) {
    ClassList* base;
    VClassList* vbase;
    DwNode* b;
    DwNode* m;
    DwNode* last;

    last = NULL;
    for (base = tclass->bases; base; base = base->next) {
        b = DW_NewNode(0x1C);
        b->f24 = (void*)(PtrInt)base->offset;
        b->f20 = func_00535110(TYPE(base->base));
        DW_ACCESS(b, base->access);
        if (base->is_virtual) {
            *(UInt8*)DW_ADDR(b, 0x2A) = 8;
        }
        if (last) {
            last->last = b;
        }
        last = b;
        if (base->is_virtual) {
            for (vbase = tclass->vbases; vbase; vbase = vbase->next) {
                if (!strcmp(base->base->classname->name, vbase->base->classname->name)) {
                    break;
                }
            }
            m = DW_NewNode(0xD);
            m->f20 = base->base->classname;
            DW_S32(m, 0x2C) = vbase->offset;
            *(UInt8*)DW_ADDR(m, 0x34) = 1;
            DW_S16(m, 0x28) = 0x248;
            m->f24 = func_00535110(TYPE(base->base));
            if (b) {
                b->last = m;
            }
            last = m;
        }
    }
    return last;
}

// 0x534900: a class's members (with its bases at the top level), their
// offsets moved by offset; the first member entry
DwNode* func_00534900(Type* type, Boolean top, SInt32 offset) {
    ObjMemberVar* ivar;
    DwNode* m;
    DwNode* prev;
    DwNode* first;

    prev = NULL;
    first = NULL;
    if (top) {
        prev = func_00534B80(TYPE_CLASS(type));
    }
    for (ivar = TYPE_CLASS(type)->ivars; ivar; ivar = ivar->next) {
        if (ivar->name == vptr_name_node) {
            continue;
        }
        if (ivar->otype == OT_MEMBERVAR) {
            m = DW_NewNode(0xD);
            m->f20 = ivar->name;
            DW_S32(m, 0x2C) = ivar->offset + offset;
            *(UInt8*)DW_ADDR(m, 0x34) = 1;
            DW_ACCESS(m, ivar->access);
            if (ivar->type->type == TYPEBITFIELD) {
                m->f24 = func_00535110(TYPE_BITFIELD(ivar->type)->bitfieldtype);
                DW_S16(m, 0x32) = TYPE_BITFIELD(ivar->type)->offset;
                DW_S16(m, 0x30) = TYPE_BITFIELD(ivar->type)->bitlength;
            } else {
                m->f24 = func_00535110(ivar->type);
            }
        } else {
            m = DW_NewNode(6);
            m->f20 = OBJECT(ivar)->name;
            DW_S16(m, 0x44) = (TYPE_CLASS(type)->flags & 8) ? 0x298 : 0x308;
            m->f28 = func_00535110(TYPE_FUNC(OBJECT(ivar)->type)->functype);
            *(UInt8*)DW_ADDR(m, 0x46) = 1;
        }
        if (prev) {
            prev->last = m;
        } else {
            first = m;
        }
        prev = m;
    }
    if (prev && top) {
        prev->last = DW_NewNode(0xFFFF);
    }
    return first;
}

// a type attribute as func_00534F60 describes it: the attribute (fund_type,
// mod_fund_type, user_def_type, mod_u_d_type), the fundamental type and the
// modifiers (one byte each, in halfword slots)
typedef struct DwTypeDesc {
    UInt16 attr;
    UInt16 fundtype;
    UInt16 count;
    UInt16 mods[33];
    SInt32 size; // +0x48 bytes the attribute takes
    DwNode* ref; // +0x4C a user type's entry
} DwTypeDesc;

extern SInt32 func_00531520(ELFSection* section, UInt16 value); // append a halfword (ELFgen.c)
extern SInt32 func_00531580(ELFSection* section, UInt32 value); // append a word
extern SInt32 func_005314F0(ELFSection* section, UInt8 value); // append a byte
extern void func_00531420(ELFSection* section, SInt32 offset, UInt16 value); // store a halfword
extern void func_00531460(ELFSection* section, SInt32 offset, UInt32 value); // store a word

// 0x5342D0: write a type attribute to .debug (a user type's reference is a
// zero word to be filled in); its size
SInt32 func_005342D0(DwTypeDesc* desc) {
    SInt32 size;
    UInt32 i;

    func_00531520(dw_current->f14, desc->attr);
    size = 2;
    switch (desc->attr) {
        case 0x55: // fund_type
            func_00531520(dw_current->f14, desc->fundtype);
            return 4;
        case 0x72: // user_def_type
            func_00531580(dw_current->f14, 0);
            return 6;
        case 0x63: // mod_fund_type
            func_00531520(dw_current->f14, desc->count + 2);
            size += 2;
            for (i = 0; i < desc->count; i++) {
                func_005314F0(dw_current->f14, desc->mods[i]);
            }
            size += i;
            func_00531520(dw_current->f14, desc->fundtype);
            return size + 2;
        case 0x83: // mod_u_d_type
            func_00531520(dw_current->f14, desc->count + 4);
            size += 2;
            for (i = 0; i < desc->count; i++) {
                func_005314F0(dw_current->f14, desc->mods[i]);
            }
            size += i;
            func_00531580(dw_current->f14, 0);
            return size + 4;
        default:
            CError_FATAL(947);
            return 0;
    }
}

// fundamental type codes of the integral types (IT_*)
static const UInt16 dw_fundtypes[20] = {3,   1,   2,   3,   8,      5,      6,
                                        8,   9,   0xB, 0xC, 0x8108, 0x8208, 0xE,
                                        0xF, 0xF, 0xF, 0xE, 0xA510, 0xA408};

// 0x534F60: describe a type as an attribute: pointers and references become
// modifiers of their target, a function's pointer refers to the function
void func_00534F60(DwType* rec, DwTypeDesc* desc) {
    Type* type;
    DwNode* ref;
    UInt16 fund;

    ref = NULL;
    type = rec->unk04;
    desc->count = 0;
    for (;;) {
        switch (type->type) {
            case TYPEPOINTER:
                desc->mods[desc->count++] = (TPTR_QUAL(type) & Q_REFERENCE) ? 2 : 1;
                type = TPTR_TARGET(type);
                if (type->type == TYPEFUNC) {
                    ref = rec->unk08;
                }
                rec = func_00535110(type);
                continue;
            case TYPEVOID:
            case TYPEMEMBERPOINTER:
                desc->attr = 0x55;
                desc->fundtype = 0x14;
                desc->size = 4;
                break;
            case TYPEINT:
            case TYPEFLOAT:
                if (TYPE_INTEGRAL(type)->integral < 20) {
                    fund = dw_fundtypes[TYPE_INTEGRAL(type)->integral];
                } else {
                    CError_FATAL(337);
                }
                desc->attr = 0x55;
                desc->fundtype = fund;
                desc->size = 4;
                break;
            case TYPEENUM:
            case TYPESTRUCT:
            case TYPECLASS:
            case TYPEFUNC:
            case TYPEARRAY:
                desc->attr = 0x72;
                desc->ref = ref ? ref : rec->unk08;
                desc->size = 6;
                break;
            default:
                CError_FATAL(362);
                break;
        }
        break;
    }
    if (desc->count) {
        desc->size += desc->count + 2;
        desc->attr = desc->attr == 0x55 ? 0x63 : 0x83;
    }
}

// a location as the backend records it for the frame and each saved register
// (BE_macro.c SavedReg): how, the register, an offset and the base register
typedef struct DwLoc {
    UInt8 kind;
    UInt8 reg;
    UInt8 pad[2];
    SInt32 offset;
    UInt8 base;
    UInt8 pad2[7];
} DwLoc;

// 0x533F30: write a location attribute (attr) to .debug; deref is the size of
// the value at a frame address; the size written, 0 for no location
SInt32 func_00533F30(DwLoc* loc, UInt16 attr, int deref) {
    ELFSection* sec;
    SInt32 size;

    sec = dw_current->f14;
    if (!loc->kind) {
        return 0;
    }
    func_00531520(sec, attr);
    size = 2;
    switch (loc->kind) {
        case 4: // in the frame: BASEREG base, [CONST offset, ADD,] DEREF
            func_00531520(sec, loc->offset ? 12 : 6);
            func_005314F0(sec, 2);
            func_00531580(sec, loc->base);
            size += 7;
            if (loc->offset) {
                func_005314F0(sec, 4);
                func_00531580(sec, loc->offset);
                func_005314F0(sec, 7);
                size += 6;
            }
            func_005314F0(sec, deref == 4 ? 6 : 0x82);
            return size + 1;
        case 1: // in a register: REG reg[, CONST offset, ADD]
            func_00531520(sec, loc->offset ? 11 : 5);
            func_005314F0(sec, 1);
            func_00531580(sec, loc->reg);
            size += 7;
            if (loc->offset) {
                func_005314F0(sec, 4);
                func_00531580(sec, loc->offset);
                func_005314F0(sec, 7);
                size += 6;
            }
            return size;
        case 2:
            func_00531520(sec, 5);
            func_005314F0(sec, 0x80);
            func_00531580(sec, loc->reg);
            return 9;
        case 3:
            func_00531520(sec, 5);
            func_005314F0(sec, 0x81);
            func_00531580(sec, loc->reg);
            return 9;
        default:
            CError_FATAL(1153);
            return 0;
    }
}

// 0x5340F0: write a variable's location attribute to .debug from what
// DW_SET_LOCATION recorded; its size
SInt32 func_005340F0(DwNode* node) {
    ELFSection* sec;
    SInt32 size;

    sec = dw_current->f14;
    func_00531520(sec, 0x23);
    size = 2;
    switch (DW_S16(node, 0x2C)) {
        case 3: // a static: ADDR, filled in by a relocation
            func_00531520(sec, 5);
            func_005314F0(sec, 3);
            func_00531580(sec, 0);
            return 9;
        case 1: // a register
            func_00531520(sec, 5);
            func_005314F0(sec, 1);
            func_00531580(sec, (UInt16)DW_S16(node, 0x2E));
            return 9;
        case 5:
            func_00531520(sec, 5);
            func_005314F0(sec, 0x80);
            func_00531580(sec, (UInt16)DW_S16(node, 0x2E));
            return 9;
        case 2: // a register pair
            func_00531520(sec, 10);
            func_005314F0(sec, 1);
            func_00531580(sec, (UInt16)DW_S16(node, 0x30));
            func_005314F0(sec, 1);
            func_00531580(sec, (UInt16)DW_S16(node, 0x2E));
            return 14;
        case 4: // in the frame: BASEREG reg[, CONST offset, ADD]
            func_00531520(sec, (DW_S32(node, 0x38) || DW_S32(node, 0x34)) ? 11 : 5);
            func_005314F0(sec, 2);
            func_00531580(sec, (UInt16)DW_S16(node, 0x2E));
            size += 7;
            if (DW_S32(node, 0x38) || DW_S32(node, 0x34)) {
                func_005314F0(sec, 4);
                func_00531580(sec, DW_S32(node, 0x38) ? 0 : DW_S32(node, 0x34));
                func_005314F0(sec, 7);
                size += 6;
            }
            return size;
        case 6: // $gp relative
            func_00531520(sec, 11);
            func_005314F0(sec, 2);
            func_00531580(sec, 0x1C);
            func_005314F0(sec, 4);
            func_00531580(sec, 0);
            func_005314F0(sec, 7);
            return 15;
        default:
            CError_FATAL(1071);
            return 0;
    }
}

// 0x5328B0: the function's .line table: its length, the base address (a
// relocation), each (line, position, offset), and an end entry at the code
// size; the offset of the length
SInt32 func_005328B0(DwNode* unit) {
    DwNode* func;
    ELFSection* line;
    SInt32* table;
    SInt32 count;
    SInt32 offset;
    SInt32 i;

    func = unit->first->next;
    line = unit->f1C;
    CError_ASSERT(2173, func && (func->tag == 0x14 || func->tag == 6));
    table = DW_PTR(func, 0x40);
    count = table[0];
    offset = func_00531580(line, (count + 1) * 10 + 8);
    func_00531580(line, 0);
    for (i = 1; i <= count; i++) {
        func_00531580(line, table[i * 2]);
        func_00531520(line, 0xFFFF);
        func_00531580(line, table[i * 2 + 1]);
    }
    func_00531580(line, 0);
    func_00531520(line, 0xFFFF);
    func_00531580(line, table[1]);
    return offset;
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

// the name attribute: the name up to any '$' (a local's or member's suffix);
// the string's offset is kept in the name; the size written
#define DW_NAME_ATTR(nm, size) \
    do { \
        HashNameNode* n_ = (nm); \
        char* s_ = strchr(n_->name, '$'); \
        char* t_; \
        SInt32 len_; \
        if (s_) { \
            len_ = s_ - n_->name; \
            t_ = lalloc(len_ + 1); \
            strncpy(t_, n_->name, len_); \
            t_[len_] = 0; \
        } else { \
            t_ = n_->name; \
        } \
        func_00531520(dw_current->f14, 0x38); \
        n_->id = ELF_AddString(dw_current->f14, t_); \
        (size) += strlen(t_) + 3; \
    } while (0)

// the mangled name of a C++ function or variable (MW_mangled)
#define DW_MANGLED_ATTR(node, size) \
    do { \
        HashNameNode* l_ = (node)->f24; \
        if (copts.cplusplus && l_ && (node)->f20 != l_) { \
            func_00531520(dw_current->f14, 0x2008); \
            l_->id = ELF_AddString(dw_current->f14, l_->name); \
            (size) += strlen(l_->name) + 3; \
        } \
    } while (0)

// 0x5331C0: write an entry to .debug: its length, tag and sibling (filled in
// by func_00532990), then its attributes; the length
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -2 // b145 asserts
SInt32 func_005331C0(DwNode* node) {
    DwTypeDesc desc;
    ELFSection* sec;
    SInt32 size;
    SInt32 elemsize;
    SInt32 start;
    SInt32 count;
    SInt32 i;
    UInt16 attr;
    DwFuncRef* ref;
    ObjEnumConst* oec;
    HashNameNode* file;
    char* name;
    DwLoc* loc;

    sec = dw_current->f14;
    if (node->tag == -1) {
        node->offset = func_00531580(sec, 4);
        return 0;
    }

    node->f14 = (void*)(PtrInt)sec->shdr.sh_size;
    node->offset = func_00531580(sec, 0);
    func_00531520(sec, (UInt16)node->tag);
    func_00531520(sec, 0x12); // sibling
    func_00531580(sec, 0);
    size = 12;

    switch ((UInt16)node->tag) {
        case 2: // class_type
        case 0x13: // structure_type
        case 0x17: // union_type
            func_00531520(sec, 0xB6); // byte_size
            func_00531580(sec, (UInt32)(PtrInt)node->f24);
            size += 6;
            DW_NAME_ATTR(node->f20, size);
            break;
        case 0xD: // member
            func_00534F60(node->f24, &desc);
            size += func_005342D0(&desc);
            func_00531520(sec, 0x23); // location: CONST offset, ADD
            func_00531520(sec, 6);
            func_005314F0(sec, 4);
            func_00531580(sec, DW_S32(node, 0x2C));
            func_005314F0(sec, 7);
            size += 10;
            if (DW_S16(node, 0x30)) {
                func_00531520(sec, 0xD6); // bit_size
                func_00531580(sec, (UInt16)DW_S16(node, 0x30));
                func_00531520(sec, 0xC5); // bit_offset
                func_00531520(sec, DW_S16(node, 0x32));
                size += 10;
            }
            DW_NAME_ATTR(node->f20, size);
            break;
        case 0x1C: // inheritance
            func_00534F60(node->f20, &desc);
            size += func_005342D0(&desc);
            func_00531520(sec, 0x23);
            func_00531520(sec, 6);
            func_005314F0(sec, 4);
            func_00531580(sec, (UInt32)(PtrInt)node->f24);
            func_005314F0(sec, 7);
            func_00531520(sec, DW_S16(node, 0x28)); // access
            func_005314F0(sec, 0);
            size += 13;
            if (*(UInt8*)DW_ADDR(node, 0x2A)) {
                func_00531520(sec, 0x308); // virtuality
                func_005314F0(sec, 0);
                size += 3;
            }
            break;
        case 1: // array_type
            func_00531520(sec, 0x95); // ordering
            func_00531520(sec, 0);
            func_00531520(sec, 0xA3); // subscr_data
            start = sec->shdr.sh_size;
            func_00531520(sec, 0);
            func_005314F0(sec, 0); // FT_C_C: fundamental type, constant bounds
            func_00531520(sec, 0xA);
            func_00531580(sec, 0);
            func_00531580(sec, (UInt32)(PtrInt)node->f28);
            func_005314F0(sec, 8); // FMT_ET
            size += 20;
            func_00534F60(node->f20, &desc);
            size += func_005342D0(&desc);
            func_00531420(sec, start, size - 20);
            break;
        case 4: // enumeration_type
            func_00531520(sec, 0xB6);
            func_00531580(sec, (UInt32)(PtrInt)node->f24);
            func_00531520(sec, 0xF4); // element_list
            func_00531580(sec, (UInt32)(PtrInt)node->f28);
            size += 12;
            elemsize = (PtrInt)node->f24;
            if (elemsize != 1 && elemsize != 2) {
                elemsize = 4;
            }
            for (oec = DW_PTR(node, 0x2C); oec; oec = oec->next) {
                switch (elemsize) {
                    case 1:
                        func_005314F0(sec, oec->val.lo);
                        break;
                    case 2:
                        func_00531520(sec, oec->val.lo);
                        break;
                    default:
                        func_00531580(sec, oec->val.lo);
                        break;
                }
                ELF_AddString(sec, oec->name->name);
                size += strlen(oec->name->name) + elemsize + 1;
            }
            DW_NAME_ATTR(node->f20, size);
            break;
        case 0x15: // subroutine_type
            func_00534F60(node->f28, &desc);
            size += func_005342D0(&desc);
            break;
        case 7: // global_variable
        case 0xC: // local_variable
            func_00534F60(node->f28, &desc);
            size += func_005342D0(&desc);
            size += func_005340F0(node);
            DW_NAME_ATTR(node->f20, size);
            DW_MANGLED_ATTR(node, size);
            break;
        case 5: // formal_parameter
            func_00534F60(node->f28, &desc);
            size += func_005342D0(&desc);
            if (node->f20) {
                size += func_005340F0(node);
                DW_NAME_ATTR(node->f20, size);
            }
            break;
        case 6: // global_subroutine
            if (*(UInt8*)DW_ADDR(node, 0x46)) {
                // a member function's declaration
                func_00534F60(node->f28, &desc);
                size += func_005342D0(&desc);
                func_00531520(sec, DW_S16(node, 0x44));
                func_005314F0(sec, 0);
                size += 3;
                DW_NAME_ATTR(node->f20, size);
                break;
            }
        case 0x14: // subroutine
            func_00531520(sec, 0x111); // low_pc, high_pc: relocations
            func_00531580(sec, 0);
            func_00531520(sec, 0x121);
            func_00531580(sec, 0);
            size += 12;
            func_00534F60(node->f28, &desc);
            size += func_005342D0(&desc);
            count = 0;
            for (ref = DW_PTR(dw_current, 0x2C); ref; ref = ref->next) {
                count++;
            }
            if (count) {
                func_00531520(sec, 0x2303); // the functions referenced
                func_00531520(sec, count * 4);
                size += 4;
            }
            for (ref = DW_PTR(dw_current, 0x2C); ref; ref = ref->next) {
                func_00531580(sec, 0);
                size += 4;
            }
            // the saved integer registers: s0-s7, fp, ra
            loc = (DwLoc*)DW_ADDR(node, 0x58);
            for (i = 0x10; i < 0x20; i++) {
                if (loc[i].kind != 4) {
                    continue;
                }
                if (i < 0x18) {
                    attr = 0x2043 + (i - 0x10) * 0x10;
                } else if (i == 0x1E) {
                    attr = 0x20C3;
                } else if (i == 0x1F) {
                    if (loc[i].kind) {
                        size += func_00533F30(&loc[i], 0x2A3, 4);
                    }
                    continue;
                } else {
                    continue;
                }
                size += func_00533F30(&loc[i], attr, 4);
            }
            // the saved FP registers: f20-f30
            loc = (DwLoc*)DW_ADDR(node, 0x858);
            for (i = 0x14; i < 0x20; i++) {
                if (loc[i].kind == 4 && i < 0x1F) {
                    size += func_00533F30(&loc[i], 0x20D3 + (i - 0x14) * 0x10, 4);
                }
            }
            if (DW_S32(node, 0x4C)) {
                size += func_00533F30((DwLoc*)DW_ADDR(node, 0x48), 0x2013, 4);
            }
            DW_NAME_ATTR(node->f20, size);
            DW_MANGLED_ATTR(node, size);
            if (*(UInt8*)DW_ADDR(dw_current, 0x38)) {
                func_00531520(dw_current->f14, 0x2338);
                ELF_AddString(dw_current->f14, "");
                size += 3;
            }
            break;
        case 0x11: // compile_unit
            if (dw_current->offset == 2) {
                func_00531520(sec, 0x111);
                func_00531580(sec, 0);
                func_00531520(sec, 0x121);
                func_00531580(sec, 0);
                func_00531520(sec, 0x106); // stmt_list
                func_00531580(sec, 0);
                size += 18;
            }
            func_00531520(sec, 0x136); // language
            func_00531580(sec, copts.cplusplus ? 4 : 2);
            func_00531520(sec, 0x258); // producer
            size += 8;
            ELF_AddString(sec, "MW MIPS C Compiler");
            size += 19;
            file = DW_PTR(dw_current, 0x34);
            if (file) {
                name = file->name;
                if (!copts.absolutepath) {
                    for (i = strlen(name); i >= 0; i--) {
                        if (name[i] == '\\' || name[i] == '/') {
                            name += i + 1;
                            break;
                        }
                    }
                }
                func_00531520(dw_current->f14, 0x38);
                ELF_AddString(dw_current->f14, name);
                size += strlen(name) + 3;
            } else {
                func_00531520(dw_current->f14, 0x38);
                ELF_AddString(dw_current->f14, dw_filename);
                size += strlen(dw_filename) + 3;
            }
            break;
        case 0xB: // lexical_block
            func_00531520(sec, 0x111);
            func_00531580(sec, 0);
            func_00531520(sec, 0x121);
            func_00531580(sec, 0);
            size += 12;
            break;
        default:
            CError_FATAL(1801);
            break;
    }
    func_00531460(sec, (SInt32)(PtrInt)node->f14, size);
    return size;
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0

// where a type attribute keeps a user type's reference: 0 for none
#define DW_TYPE_REF_POS(desc, pos) \
    switch ((desc).attr) { \
        case 0x55: \
        case 0x63: \
            pos = 0; \
            break; \
        case 0x72: \
            pos = 2; \
            break; \
        case 0x83: \
            pos = (desc).count + 4; \
            break; \
        default: \
            CError_FATAL(985); \
            pos = 0; \
            break; \
    }

// the size of a type attribute and where it keeps a user type's reference
#define DW_TYPE_REF_SIZE(desc, pos, size) \
    switch ((desc).attr) { \
        case 0x55: \
            size = 4; \
            pos = 0; \
            break; \
        case 0x63: \
            size = (desc).count + 6; \
            pos = 0; \
            break; \
        case 0x72: \
            size = 6; \
            pos = 2; \
            break; \
        case 0x83: \
            size = (desc).count + 8; \
            pos = (desc).count + 4; \
            break; \
        default: \
            CError_FATAL(985); \
            pos = 0; \
            break; \
    }

// in a section's data, the word at the entry's offset plus at
#define DW_ENTRY_WORD(node, at) \
    (*(SInt32*)(*((ELFSection*)(node)->parent->f14)->data.data + (node)->offset + (at)))

// a word in the entry that refers to another entry: its offset, relocated
// by that entry's unit
#define DW_PATCH_REF(node, at, ref) \
    do { \
        DW_ENTRY_WORD(node, at) = (ref)->offset; \
        ELF_AddReloc(2, (node)->parent->f24, (node)->offset + (at), (ref)->parent->f24, 0); \
    } while (0)

typedef struct DwUnk605C64 {
    SInt32 offset;
    void* unk04;
    struct DwUnk605C64* next;
} DwUnk605C64;

// the place a variable's address is written in its location attribute: 0
// for none
static inline SInt32 DW_LocAddrPos(DwNode* node) {
    switch (DW_S16(node, 0x2C)) {
        case 3:
            return 5;
        case 4:
            return 10;
        default:
            return 0;
    }
}

// 0x532990: fill in an entry's references once every entry has its offset:
// its sibling, its type, and the relocations of addresses
#undef CError_B145_SHIFT
#define CError_B145_SHIFT -2 // b145 asserts
void func_00532990(DwNode* node) {
    DwTypeDesc desc;
    SInt32 pos;
    SInt32 size;
    SInt32 at;
    SInt32 i;
    DwFuncRef* ref;
    DwUnk605C64* u;
    DwNode* entry;

    if (node->tag == -1 || node->tag == 0x11) {
        return;
    }

    if (node->last) {
        DW_ENTRY_WORD(node, 8) = node->last->offset;
        ELF_AddReloc(2, node->parent->f24, node->offset + 8, node->parent->f24, 0);
    }

    size = 0;
    switch ((UInt16)node->tag) {
        case 0xD: // member
            func_00534F60(node->f24, &desc);
            DW_TYPE_REF_POS(desc, pos);
            if (pos) {
                DW_PATCH_REF(node, pos + 0xC, desc.ref);
            }
            break;
        case 0x1C: // inheritance
            func_00534F60(node->f20, &desc);
            DW_TYPE_REF_POS(desc, pos);
            if (pos) {
                DW_PATCH_REF(node, pos + 0xC, desc.ref);
            }
            break;
        case 1: // array_type
            func_00534F60(node->f20, &desc);
            DW_TYPE_REF_POS(desc, pos);
            if (pos) {
                DW_PATCH_REF(node, pos + 0x20, desc.ref);
            }
            break;
        case 0x15: // subroutine_type
            func_00534F60(node->f28, &desc);
            DW_TYPE_REF_POS(desc, pos);
            if (pos) {
                DW_PATCH_REF(node, pos + 0xC, desc.ref);
            }
            break;
        case 7: // global_variable
        case 0xC: // local_variable
            func_00534F60(node->f28, &desc);
            DW_TYPE_REF_SIZE(desc, pos, size);
            if (pos) {
                DW_PATCH_REF(node, pos + 0xC, desc.ref);
            }
            if (DW_PTR(node, 0x38) && (pos = DW_LocAddrPos(node))) {
                at = size + 0xC + pos;
                if (DW_S16(node, 0x2C) == 3) {
                    DW_ENTRY_WORD(node, at) = 0;
                }
                ELF_AddReloc(2, node->parent->f24, node->offset + at, DW_PTR(node, 0x38), 0);
            }
            break;
        case 5: // formal_parameter
            func_00534F60(node->f28, &desc);
            DW_TYPE_REF_SIZE(desc, pos, size);
            if (pos) {
                DW_PATCH_REF(node, pos + 0xC, desc.ref);
            }
            if (node->f20 && DW_PTR(node, 0x38) && (pos = DW_LocAddrPos(node))) {
                at = size + 0xC + pos;
                DW_ENTRY_WORD(node, at) = 0;
                ELF_AddReloc(2, node->parent->f24, node->offset + at, DW_PTR(node, 0x38), 0);
            }
            break;
        case 6: // global_subroutine
            if (*(UInt8*)DW_ADDR(node, 0x46)) {
                break;
            }
        case 0x14: // subroutine
            func_00534F60(node->f28, &desc);
            DW_TYPE_REF_SIZE(desc, pos, size);
            if (pos) {
                DW_PATCH_REF(node, pos + 0x18, desc.ref);
            }
            DW_ENTRY_WORD(node, 0xE) = 0;
            ELF_AddReloc(2, node->parent->f24, node->offset + 0xE, DW_PTR(node, 0x30), 0);
            DW_ENTRY_WORD(node, 0x14) = DW_S32(node, 0x34);
            ELF_AddReloc(2, node->parent->f24, node->offset + 0x14, DW_PTR(node, 0x30), 0);
            i = 0;
            for (ref = DW_PTR(dw_current, 0x2C); ref; ref = ref->next) {
                entry = (DwNode*)ref->object->debugInfo.entry;
                DW_PATCH_REF(node, size + 0x1C + i, entry);
                i += 4;
            }
            if (DW_PTR(dw_current, 0x2C)) {
                i += 4;
            }
            if (dw_unk605C5C) {
                for (u = DW_PTR(dw_current, 0x30); u; u = u->next) {
                    at = size + 0x1C + i;
                    DW_ENTRY_WORD(node, at) = u->offset;
                    ELF_AddReloc(
                        2, node->parent->f24, node->offset + at, DW_PTR(dw_unk605C5C, 0x30), 0
                    );
                    i += 6;
                }
            }
            dw_unk605C5C = NULL;
            break;
        case 0xB: // lexical_block
            DW_ENTRY_WORD(node, 0xE) = (SInt32)(PtrInt)node->f24;
            ELF_AddReloc(2, node->parent->f24, node->offset + 0xE, node->f20, 0);
            DW_ENTRY_WORD(node, 0x14) = (SInt32)(PtrInt)node->f28;
            ELF_AddReloc(2, node->parent->f24, node->offset + 0x14, node->f20, 0);
            break;
    }
}
#undef CError_B145_SHIFT
#define CError_B145_SHIFT 0
