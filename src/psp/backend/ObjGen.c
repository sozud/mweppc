#include "compiler/common.h"
#include "compiler/CParser.h"
#include "psp/ELFgen.h"
#include "psp/ObjGen.h"
#include "psp/BE.h"
#include "compiler/CError.h"
#include "compiler/CMangler.h"
#include "compiler/CompilerTools.h"
#include "compiler/CInit.h"
#include "compiler/CPrep.h"
#include "compiler/objects.h"
#include "compiler/types.h"
#include "compiler/CMachine.h"
#include "compiler/IrOptimizer.h"
#include "compiler/CDecl.h"
#include "compiler/CPrepTokenizer.h"
#include "compiler/tokens.h"
#include <string.h>
#include <stdio.h>

// mwccpsp 3.0.1 ObjGen.c (0x462050..0x464B20): the MIPS/PSP object builder
// on top of ELFgen. Unidentified globals and callees are named by address
// (data_XXXXXXXX / func_XXXXXXXX).

typedef struct ObjSymCache {
    struct ObjSymCache* next;
    HashNameNode* name;
    ELFSymbol* sym;
} ObjSymCache;

typedef struct PooledFloat {
    struct PooledFloat* next;
    Object* object;
} PooledFloat;

extern SInt32 data_00625388;
extern SInt32 data_006241D4;
extern HashNameNode* data_006245BC; // the source file being compiled (DWARF)
extern void* data_00624724;
extern PooledFloat* data_00625970; // float constants to emit at the end
extern SInt32 data_00625094;
extern SInt32 data_006247A8;
extern UInt8 data_00625E9B;
extern GList data_005EAF88; // the finished object file
extern ObjSymCache* data_005EAF98[0x800]; // ELF symbol per link name, by hashval

extern void func_004A16E0(void);
extern void func_00439E20(void);
extern void func_00461EE0(void);
extern void func_00463B40(void);
extern void func_00531A60(void);
extern HashNameNode* func_0049F6E0(Object* obj); // link name of an object
extern void func_00530360(void);
extern void func_004A16D0(void);
extern void func_004B0000(void);
extern void func_005317D0(void);
extern void func_005303A0(GList* gl);
extern CWMemHandle COS_DetachHandle(Handle* handle);
extern void func_005302D0(SInt32* codesize, SInt32* idatasize, SInt32* udatasize);
extern void func_00462860(Object* obj, void* data, SInt32 unk, SInt32 size);

typedef struct ObjSectionPart {
    char* name;
    SInt32 type;
    ELFSection* section; // created when first used
} ObjSectionPart;

typedef struct ObjSectionInfo {
    char* name; // "text", "sdata", ...
    UInt8 mode; // addressing: 0 standard, 1 near_absolute, 2 far_absolute,
    // 3 near_code, 4 far_code, 5 near_data, 6 far_data
    UInt32 flags; // sh_flags: 2 R, 3 RW, 6 RX, 7 RWX; 0x10000000 data,
    // 0x40000000 code, 0x80000000 text16
    ObjSectionPart sect; // +0x0C: initialised data / code
    ObjSectionPart bss; // +0x18: uninitialised counterpart, if any
    ObjSectionPart rel; // +0x24: ".rel" + sect.name
    UInt8 gpreg; // +0x30, set for lit4/lit8
} ObjSectionInfo; // 0x34; index 0 unused
extern ObjSectionInfo data_005ECF98[0x40];

extern SInt16 data_00625E3E; // frame register
extern SInt32 data_00624DD4; // zdata
extern SInt32 data_00624BE8; // pinit (.ctor)
extern SInt32 data_00624B4C; // init
extern SInt32 data_00624FDC; // exception
extern SInt32 data_0062549C; // exceptix
extern SInt32 data_00624BC4; // picrelo
extern SInt32 data_00624D8C; // pidrelo
extern SInt32 data_00625880; // scratchpad
#if MWCCPS2
extern SInt32 ps2_vutext_636838; // b210: VU microcode sections
extern SInt32 ps2_vudata_636D54;
#endif
extern SInt32 data_00625310; // sections defined + 1

// The instruction list of the function being emitted. Its records are not
// rebuilt yet; these are the fields ObjGen reads.
// the function's blocks: data_00624FD0 (BE.h)

extern SInt32 data_00624DF4; // minimum function alignment
extern void func_00531D90(Object* obj, Boolean global, ELFSymbol* sym, SInt32 size); // debug info
extern ELFSection* func_005302C0(UInt16 index); // section by index
extern SInt16 func_00464E20(Object* obj); // alignment of an object
extern void func_00531C10(Object* obj, Boolean global, ELFSymbol* sym, SInt32 size); // debug info
#define CODENODE_INSTR(n) (*(char**)&((MPCodeBlock*)(n))->last) // +0x18
#define INSTR_FLAGS(i) (*(UInt32*)&((MPCode*)(i))->flags) // +0xC
#define INSTR_OFFSET(i) (*(SInt32*)&((MPCode*)(i))->offset) // +0x1C
#define INSTR_OP28(i) (*(SInt16*)&((MPCode*)(i))->op) // +0x28
#define INSTR_OP44(i) (*(SInt16*)&((MPCode*)(i))->args[0].reg) // +0x44
SInt32 ObjGen_DefineSection(
    char* name, char* elfname, char* bssname, char* addressing, char* access
);
extern SInt32 data_0062477C; // default text section
extern SInt32 data_006241DC; // vtables
extern SInt32 data_006250B4; // read-only data
extern SInt32 data_00624D74;
extern SInt32 data_00624D60;
extern SInt32 data_00624764; // default data section
extern SInt32 data_0062524C; // small data
extern SInt32 func_00438FC0(void); // small-data size threshold
extern Boolean is_const_object(Object* obj);

// Object's layout past the first fields is not rebuilt for 3.0.1 yet; the
// constant's value pointer of a pooled float sits at +0x28.
#define OBJECT_VALUE_PTR(obj) (*(Float**)&((Object*)(obj))->u.data.u.floatconst) // +0x28

void ObjGen_Setup(void) {
    UInt32 flags;
    SInt32 i;

    data_00625388 = 0;
    if (copts.mips_option0A == 1) {
        data_00625388 |= 0x20000000;
    }
    func_004A16E0();
    func_00439E20();
    data_006241D4 = 0;
    data_006245BC = 0;
    data_00624724 = 0;
    data_00625970 = (PooledFloat*)data_00624724;
    data_00625094 = 0;
    data_006247A8 = 0;
    data_00625E9B = 0;
    func_00461EE0();
    data_005EAF88.data = NULL;
    func_00463B40();
    for (i = 0; i < 0x800; i++) {
        data_005EAF98[i] = 0;
    }

    flags = 1; // EF_MIPS_NOREORDER
    if (copts.mips_isa >= 4) {
        flags |= 0x30000000; // EF_MIPS_ARCH_4
    } else if (copts.mips_isa >= 3) {
        flags |= 0x20000000; // EF_MIPS_ARCH_3
    } else if (copts.mips_isa >= 2) {
        flags |= 0x10000000; // EF_MIPS_ARCH_2
    }
#if MWCCPS2
    ELF_Init(8, flags | 0x924000, copts.littleendian ? 1 : 2); // EM_MIPS, R5900 machine flags
#else
    ELF_Init(8, flags | 0x970000, copts.littleendian ? 1 : 2); // EM_MIPS
#endif
    ELF_AddString(ELF_NewSection(".comment", SHT_PROGBITS, 0), "MW MIPS C Compiler (3.0.0)");
    if (copts.mips_option132 == 1) {
        func_00531A60();
    }
}

ELFSymbol* ObjGen_GetELFSymbol(Object* obj) {
    HashNameNode* name;
    ObjSymCache* entry;

    name = func_0049F6E0(obj);
    for (entry = data_005EAF98[name->hashval]; entry; entry = entry->next) {
        if (entry->name == name) {
            return entry->sym;
        }
    }

    entry = galloc(sizeof(ObjSymCache));
    memclrw(entry, sizeof(ObjSymCache));
    entry->next = data_005EAF98[name->hashval];
    data_005EAF98[name->hashval] = entry;
    entry->name = name;
    if (obj->datatype == DDATA) {
        entry->sym = ELF_NewUndefinedSymbol(
            name->name, 0, 1, 1, obj->type->size, 0
        ); // STB_GLOBAL, STT_OBJECT
    } else {
        entry->sym = ELF_NewUndefinedSymbol(name->name, 0, 1, 0, 0, 0); // STB_GLOBAL, STT_NOTYPE
    }
    return entry->sym;
}

#if MWCCPS2
extern SInt32 ps2_objgen_636288; // mwccps2-only state, cleared with the rest
#endif

void ObjGen_Cleanup(void) {
    FreeGList(&data_005EAF88);
    func_00530360();
    IRO_Cleanup();
    func_004A16D0();
    func_004B0000();
#if MWCCPS2
    ps2_objgen_636288 = 0;
#endif
}

void ObjGen_Finish(void) {
    PooledFloat* pf;
    Object* obj;
    Type* type;
    Float* value;
    union {
        double d;
        float f;
        SInt32 w[2];
    } buf;
    float f;

    CInit_DeclarePooledStrings();
    for (pf = data_00625970; pf; pf = pf->next) {
        obj = pf->object;
        type = obj->type;
        value = OBJECT_VALUE_PTR(obj);
        if (TYPE_INTEGRAL(type)->integral == IT_FLOAT ||
            (type->type == TYPEFLOAT && type->size == 4)) {
            f = value->data.value;
            buf.w[1] = *(SInt32*)&f;
            func_00462860(obj, &buf.w[1], 0, 4);
        } else {
            buf.d = value->data.value;
            func_00462860(obj, &buf, 0, 8);
        }
    }

    if (copts.mips_option132 == 1) {
        func_005317D0();
    }
    if (InitGList(&data_005EAF88, 0x8000)) {
        CError_NoMem();
    }
    func_005303A0(&data_005EAF88);
    cparamblkptr->object.objectdata = COS_DetachHandle((Handle*)&data_005EAF88);
    data_005EAF88.data = NULL;
    func_005302D0(
        &cparamblkptr->object.codesize,
        &cparamblkptr->object.idatasize,
        &cparamblkptr->object.udatasize
    );
    func_004A16D0();
}

// Which section an object goes in.
SInt32 ObjGen_GetObjectSection(Object* obj) {
    SInt32 sec;
    HashNameNode* name;

    sec = obj->section;
    if (obj->datatype == DFUNC || obj->datatype == DVFUNC) {
        if (!sec || !(data_005ECF98[sec].flags & 4)) {
            return data_0062477C;
        }
    }

    name = obj->name;
    if (name->name[1] == '_' && !strncmp(name->name, "__vt__", 6)) {
        return data_006241DC;
    }
    if (!strncmp(name->name, "@stringBase0", 12)) {
        return data_006250B4;
    }
    if (!strncmp(name->name, "@wstringBase0", 13)) {
        return data_006250B4;
    }
    if (sec == data_00624D74 || sec == data_00624D60) {
        return sec;
    }
    if (data_00625E9B) {
        data_00625E9B = 0;
        obj->section = data_006250B4;
        obj->qual |= Q_CONST;
        return data_006250B4;
    }
    if (sec) {
        return sec;
    }
    if (is_const_object(obj)) {
        return data_006250B4;
    }
    if (!sec) {
        if (obj->type->size <= 0 || obj->type->size > func_00438FC0()) {
            sec = data_00624764;
        } else {
            sec = data_0062524C;
        }
    }
    return sec;
}

void ObjGen_GetObjectAddress(Object* obj, ObjAddress* addr) {
    SInt32 sec;

    addr->gpreg = 0;
    if (!obj) {
        addr->basereg = -1;
        addr->near = 0;
        return;
    }
#if MWCCPS2
    // b210 (0x46EE20): a global bound to a register (its register-variable
    // record at +0x3C: class at +0x2E, register at +0x32) has no address
    if (obj->datatype == DDATA) {
#if MWCCPSP_NATIVE
        char* rv = (char*)obj->u.data.info;
#else
        char* rv = *(char**)((char*)obj + 0x3C);
#endif
        if (rv &&
            (*(SInt16*)PS2_VARINFO_AT(rv, 0x32) ||
             *PS2_VARINFO_AT(rv, 0x2E) == 5 ||
             (*PS2_VARINFO_AT(rv, 0x2F) && !*PS2_VARINFO_AT(rv, 0x2E)))) {
            addr->basereg = -1;
            addr->near = 0;
            return;
        }
    }
#endif
    if (obj->datatype == DLOCAL) {
        addr->basereg = data_00625E3E;
        addr->near = 0;
        return;
    }

    sec = ObjGen_GetObjectSection(obj);
    switch (data_005ECF98[sec].mode) {
        case 0:
        case 2:
            addr->basereg = 0;
            addr->near = 0;
            break;
        case 1:
            addr->basereg = 0;
            addr->near = 1;
            break;
        case 3:
            if (copts.mips_option08) {
                addr->basereg = 23;
                addr->near = 1;
            } else {
                CError_FATAL(677);
            }
            break;
        case 4:
            if (copts.mips_option08) {
                addr->basereg = 23;
                addr->near = 0;
            } else {
                CError_FATAL(686);
            }
            break;
        case 5:
            addr->basereg = 28;
            addr->near = 1;
            addr->gpreg = data_005ECF98[sec].gpreg;
            break;
        case 6:
            if (copts.mips_option07) {
                addr->basereg = 28;
                addr->near = 0;
            } else {
                CError_FATAL(701);
            }
            break;
    }
}

void ObjGen_InitSections(void) {
    data_00625310 = 1;
    data_0062524C = ObjGen_DefineSection("sdata", ".sdata", ".sbss", "near_data", "RW");
    data_00624DD4 = ObjGen_DefineSection("zdata", ".zdata", ".zbss", "near_absolute", "RW");
    data_00624D74 = ObjGen_DefineSection("lit4", ".lit4", NULL, "near_data", "RW");
    data_005ECF98[data_00624D74].gpreg = 1;
    data_00624D60 = ObjGen_DefineSection("lit8", ".lit8", NULL, "near_data", "RW");
    data_005ECF98[data_00624D60].gpreg = 1;
    data_00624BE8 = ObjGen_DefineSection("pinit", ".ctor", NULL, "far_absolute", "R");
    data_00624B4C = ObjGen_DefineSection("init", ".init", NULL, "far_absolute", "RX");
    if (copts.mips_option08) {
        data_0062477C = ObjGen_DefineSection("text", ".text", NULL, "far_code", "RX");
        data_006250B4 = ObjGen_DefineSection("rodata", ".rodata", NULL, "far_code", "R");
        data_00624FDC = ObjGen_DefineSection("exception", ".exception", NULL, "far_code", "R");
        data_0062549C = ObjGen_DefineSection("exceptix", ".exceptix", NULL, "far_code", "R");
        data_00624BC4 = ObjGen_DefineSection("picrelo", ".picdynrel", NULL, "far_data", "R");
    } else {
        data_0062477C = ObjGen_DefineSection("text", ".text", NULL, "far_absolute", "RX");
        data_006250B4 = ObjGen_DefineSection("rodata", ".rodata", NULL, "far_absolute", "R");
        data_00624FDC = ObjGen_DefineSection("exception", ".exception", NULL, "far_absolute", "R");
        data_0062549C = ObjGen_DefineSection("exceptix", ".exceptix", NULL, "far_absolute", "R");
    }
    if (copts.mips_option07) {
        data_00624764 = ObjGen_DefineSection("data", ".data", ".bss", "far_data", "RW");
        data_006241DC = ObjGen_DefineSection("vtables", ".vtables", NULL, "far_data", "R");
        data_00624D8C = ObjGen_DefineSection("pidrelo", ".piddynrel", NULL, "far_data", "R");
    } else {
        data_00624764 = ObjGen_DefineSection("data", ".data", ".bss", "far_absolute", "RW");
        data_006241DC = ObjGen_DefineSection("vtables", ".vtables", NULL, "far_absolute", "R");
    }
    data_00625880 =
        ObjGen_DefineSection("scratchpad", ".spad_data", ".spad_bss", "far_absolute", "RW");
#if MWCCPS2
    ps2_vutext_636838 = ObjGen_DefineSection("vutext", ".vutext", NULL, "far_absolute", "RX");
    ps2_vudata_636D54 = ObjGen_DefineSection("vudata", ".vudata", ".vubss", "far_absolute", "RW");
#endif
}

SInt32 ObjGen_DefineSection(
    char* name, char* elfname, char* bssname, char* addressing, char* access
) {
    UInt8 mode;
    UInt32 flags;
#if MWCCPS2
    ObjSectionInfo* info;
    SInt32 i;
    ObjSectionInfo* scan;
#else
    SInt32 i;
    ObjSectionInfo* info;
#endif

    if (!addressing) {
        addressing = "standard";
    }
    if (!access) {
        access = "RWX";
    }

    if (!strcmp(addressing, "standard")) {
        mode = 0;
    } else if (!strcmp(addressing, "near_absolute")) {
        mode = 1;
    } else if (!strcmp(addressing, "far_absolute")) {
        mode = 2;
    } else if (!strcmp(addressing, "near_code")) {
        mode = 3;
    } else if (!strcmp(addressing, "far_code")) {
        mode = 4;
    } else if (!strcmp(addressing, "near_data")) {
        mode = 5;
    } else if (!strcmp(addressing, "far_data")) {
        mode = 6;
    } else {
        return 0;
    }

    if (!strcmp(access, "R")) {
        flags = 2;
    } else if (!strcmp(access, "RW")) {
        flags = 3;
    } else if (!strcmp(access, "RX")) {
        flags = 6;
    } else if (!strcmp(access, "RWX")) {
        flags = 7;
    } else {
        return 0;
    }

    if (!strcmp(name, "text16")) {
        flags += 0x80000000;
    }

#if MWCCPS2
    i = 1;
    if (data_00625310 > 1) {
        scan = &data_005ECF98[1];
        do {
            if (!strcmp(name, scan->name)) {
                break;
            }
            i++;
            scan++;
        } while (i < data_00625310);
    }
#else
    i = 1;
    if (data_00625310 > 1) {
        for (; i < data_00625310; i++) {
            if (!strcmp(name, data_005ECF98[i].name)) {
                break;
            }
        }
    }
#endif
    if (i == data_00625310) {
        if (data_00625310 == 0x40) {
            return 0;
        }
        data_00625310++;
    }

    info = &data_005ECF98[i];
    info->name = name;
    info->mode = mode;
    info->flags = flags;
    if (mode == 5 || mode == 6) {
        info->flags |= 0x10000000;
    } else if (mode == 3 || mode == 4) {
        info->flags |= 0x40000000;
    }
    info->sect.name = elfname;
    info->sect.type = SHT_PROGBITS;
    info->sect.section = NULL;
    info->bss.name = bssname;
    info->bss.type = SHT_NOBITS;
    info->bss.section = NULL;
    info->rel.name = galloc(strlen(elfname) + 5);
    strcpy(info->rel.name, ".rel");
    strcat(info->rel.name, elfname);
    info->rel.type = SHT_REL;
    info->rel.section = NULL;
    info->gpreg = 0;
    return i;
}

// .mwcats: per-function table of code offsets of flagged instructions.
void ObjGen_GenerateMWCats(Object* obj, SInt32 codesize) {
    char* linkname;
    ELFSymbol* funcsym;
    ELFSection* section;
    MPCodeBlock* node;
    char* instr;
    SInt32 count;
    SInt32 size;
    SInt32 small;
    UInt8* buf;
    UInt8* p;
    SInt32 addroffset;
    char* name;

    linkname = func_0049F6E0(obj)->name;
    funcsym = ObjGen_GetELFSymbol(obj);
    section = ELF_NewSection(".mwcats", 0xCA2A82C2, 0);
    section->shdr.sh_link = funcsym->sym.st_shndx;
    ELF_SetRelSection(section, ELF_NewSection(".rel.mwcats", SHT_REL, 0));
    section->shdr.sh_addralign = 4;

    count = 0;
    for (node = data_00624FD0; node; node = node->next) {
        instr = CODENODE_INSTR(node);
        if (instr &&
            (INSTR_FLAGS(instr) & 0x80000) &&
            !(INSTR_OP28(instr) == 0x3B && INSTR_OP44(instr) == 0x1F)) {
            count++;
        }
    }

    if (codesize > 0x7FFF) {
        size = count * 4 + 12;
        small = 0;
    } else {
        size = count * 2 + 8;
        small = 1;
    }
    buf = lalloc(size);
    p = buf;
    if (small) {
        p[0] = 2;
        p[1] = count;
        *(SInt16*)(p + 2) = codesize;
        *(SInt32*)(p + 4) = 0;
        p += 8;
        addroffset = 4;
    } else {
        p[0] = 3;
        p[1] = 0;
        *(SInt16*)(p + 2) = count;
        *(SInt32*)(p + 4) = codesize;
        *(SInt32*)(p + 8) = 0;
        p += 12;
        addroffset = 8;
    }

    if (count) {
        for (node = data_00624FD0; node; node = node->next) {
            instr = CODENODE_INSTR(node);
            if (instr &&
                (INSTR_FLAGS(instr) & 0x80000) &&
                !(INSTR_OP28(instr) == 0x3B && INSTR_OP44(instr) == 0x1F)) {
                if (small) {
                    *(SInt16*)p = INSTR_OFFSET(instr);
                    p += 2;
                } else {
                    *(SInt32*)p = INSTR_OFFSET(instr);
                    p += 4;
                }
            }
        }
    }

    name = galloc(strlen(linkname) + 10);
    sprintf(name, ".mwcats_%s", linkname);
    ELF_AddReloc(
        2, ELF_NewSymbol(name, section->index, 0, 3, size, buf, 4), addroffset, funcsym, 0
    ); // R_MIPS_32
}

// Emit an initialised (data != NULL) or uninitialised object.
void ObjGen_EmitData(Object* obj, void* data, ObjDataReloc* relocs, SInt32 size) {
    SInt32 sec;
    ELFSymbol* elfsym;
    ELFSection* elfsection;
    ObjSectionInfo* info;
    ObjSectionPart* part;
    ObjDataReloc* r;
    SInt32 bind;
    SInt32 kind;
    SInt32 align;
    UInt8* copy;
    char* linkname;
    char* name;
    ObjAddress addr;
    ObjAddress objaddr;
    SInt32 piccount;
    SInt32 pidcount;
    SInt32* pictable;
    SInt32* pidtable;
    ELFSection* picsection;
    ELFSection* pidsection;
    ELFSymbol* picsym;
    ELFSymbol* pidsym;
    SInt32 pi;
    SInt32 di;
    SInt32 picoff;
    SInt32 pidoff;

    sec = ObjGen_GetObjectSection(obj);
    elfsym = ObjGen_GetELFSymbol(obj);
    piccount = 0;
    pidcount = 0;

    if (elfsym->sym.st_shndx == 0) {
        info = &data_005ECF98[sec];
        part = &info->sect;
        if (!data && info->bss.name) {
            part = &info->bss;
        }
        part->section = ELF_NewSection(part->name, part->type, info->flags);
        elfsection = part->section;
    } else {
        elfsection = func_005302C0(elfsym->sym.st_shndx);
    }

    if (CParser_HasInternalLinkage(obj)) {
        bind = 0; // STB_LOCAL
    } else if (obj->qual & 0x20000) {
        bind = 13;
    } else if (obj->qual & 0x40000) {
        bind = 2; // STB_WEAK
    } else if (obj->flags & 0x40) {
        bind = 15;
    } else {
        bind = 1; // STB_GLOBAL
    }
    if (copts.force_active || (bind == 1 && (obj->flags & 0x40)) || bind == 15) {
        elfsection->shdr.sh_flags |= 0x20000000;
    }

    align = func_00464E20(obj);
    if (relocs) {
        copy = lalloc(size);
        memcpy(copy, data, size);
        for (r = relocs; r; r = r->next) {
            *(SInt32*)(copy + r->offset) += r->addend;
            r->addend = 0;
        }
        data = copy;
    }
    if (elfsym->sym.st_shndx == 0) {
        ELF_DefineSymbol(elfsym, elfsection->index, bind, 1, size, data, align); // STT_OBJECT
    }

    if (relocs) {
        info = &data_005ECF98[sec];
        info->rel.section = ELF_NewSection(info->rel.name, info->rel.type, 0);
        ELF_SetRelSection(info->sect.section, info->rel.section);
        for (r = relocs; r; r = r->next) {
            if (copts.mips_option07 || copts.mips_option08) {
                ObjGen_GetObjectAddress(r->target, &addr);
                if (copts.mips_option07 && addr.basereg == 28) {
                    pidcount++;
                }
                if (copts.mips_option08 && addr.basereg == 23) {
                    piccount++;
                }
            }
            ELF_AddReloc(2, elfsym, r->offset, ObjGen_GetELFSymbol(r->target), 0); // R_MIPS_32
        }
    }

    if (obj->datatype == DLOCAL || !strchr(func_0049F6E0(obj)->name, '$')) {
        if (copts.mips_option132 == 1) {
            func_00531C10(obj, bind != 0, elfsym, size);
        }
    }

    linkname = func_0049F6E0(obj)->name;
    ObjGen_GetObjectAddress(obj, &objaddr);
    if (objaddr.basereg == 23) {
        kind = 2;
    } else if (objaddr.basereg == 28) {
        kind = 0;
    } else {
        kind = 1;
    }

    if (piccount) {
        pictable = lalloc(piccount * 4);
        memset(pictable, 0, piccount * 4);
        info = &data_005ECF98[data_00624BC4];
        info->sect.section = ELF_NewSection(info->sect.name, info->sect.type, info->flags);
        picsection = info->sect.section;
        info = &data_005ECF98[data_00624BC4];
        info->rel.section = ELF_NewSection(info->rel.name, info->rel.type, 0);
        ELF_SetRelSection(info->sect.section, info->rel.section);
        name = galloc(strlen(linkname) + 7);
        sprintf(name, ".pic_%s", linkname);
        picsym = ELF_NewUndefinedSymbol(name, 0, 0, 3, 0, 0);
    }
    if (pidcount) {
        pidtable = lalloc(pidcount * 4);
        memset(pidtable, 0, pidcount * 4);
        info = &data_005ECF98[data_00624D8C];
        part = &info->sect;
        if (!data && info->bss.name) {
            part = &info->bss;
        }
        part->section = ELF_NewSection(part->name, part->type, info->flags);
        pidsection = part->section;
        info = &data_005ECF98[data_00624D8C];
        info->rel.section = ELF_NewSection(info->rel.name, info->rel.type, 0);
        ELF_SetRelSection(info->sect.section, info->rel.section);
        name = galloc(strlen(linkname) + 7);
        sprintf(name, ".pid_%s", linkname);
        pidsym = ELF_NewUndefinedSymbol(name, 0, 0, 3, 0, 0);
    }

    if (pidcount + piccount) {
        di = 0;
        pi = 0;
        for (r = relocs; r; r = r->next) {
            ObjGen_GetObjectAddress(r->target, &addr);
            if (copts.mips_option08 && addr.basereg == 23) {
                pictable[pi++] = r->offset + kind;
            }
            if (copts.mips_option07 && addr.basereg == 28) {
                pidtable[di++] = r->offset + kind;
            }
        }
        if (pi) {
            ELF_DefineSymbol(picsym, picsection->index, 0, 3, pi * 4, pictable, 4);
        }
        if (di) {
            ELF_DefineSymbol(pidsym, pidsection->index, 0, 3, di * 4, pidtable, 4);
        }

        di = 0;
        pi = 0;
        picoff = 0;
        pidoff = 0;
        for (r = relocs; r; r = r->next) {
            ObjGen_GetObjectAddress(r->target, &addr);
            if (copts.mips_option08 && addr.basereg == 23) {
                pictable[pi] = r->offset;
                ELF_AddReloc(2, picsym, picoff, elfsym, 0);
                picoff += 4;
                pi++;
            }
            if (copts.mips_option07 && addr.basereg == 28) {
                pidtable[di] = r->offset;
                ELF_AddReloc(2, pidsym, pidoff, elfsym, 0);
                pidoff += 4;
                di++;
            }
        }
    }
}

// Emit a function's code.
void ObjGen_EmitCode(Object* obj, void* code, SInt32 size, ObjCodeReloc* relocs) {
    ObjSectionInfo* info;
    ObjSectionInfo* pinfo;
    ObjSectionPart* part;
    ELFSection* elfsection;
    ELFSymbol* elfsym;
    ObjCodeReloc* r;
    SInt32 bind;
    SInt32 kind;
    SInt32 align;
    SInt32 savedsize;
    char* linkname;
    char* name;
    ObjAddress addr;
    ObjAddress objaddr;
    SInt32 piccount;
    SInt32 pidcount;
    SInt32* pictable;
    SInt32* pidtable;
    ELFSection* picsection;
    ELFSection* pidsection;
    ELFSymbol* picsym;
    ELFSymbol* pidsym;
    SInt32 pi;
    SInt32 di;
    SInt32 picoff;
    SInt32 pidoff;

    info = &data_005ECF98[ObjGen_GetObjectSection(obj)];
    info->sect.section = ELF_NewSection(info->sect.name, info->sect.type, info->flags);
    elfsection = info->sect.section;
    elfsym = ObjGen_GetELFSymbol(obj);
    piccount = 0;
    pidcount = 0;

    if (CParser_HasInternalLinkage(obj)) {
        bind = 0; // STB_LOCAL
    } else if (obj->qual & 0x20000) {
        bind = 13;
    } else if (obj->qual & 0x40000) {
        bind = 2; // STB_WEAK
    } else if (obj->flags & 0x40) {
        bind = 15;
    } else {
        bind = 1; // STB_GLOBAL
    }
    if (copts.force_active || copts.interrupt || (bind == 1 && (obj->flags & 0x40)) || bind == 15) {
        elfsection->shdr.sh_flags |= 0x20000000;
    }

#if MWCCPS2 // b210 (0x46E460) tests copts+0x41, as in CMachine.c and StructMoves.c
    if ((copts.mips_option41 ? 16 : 4) > data_00624DF4) {
        align = copts.mips_option41 ? 16 : 4;
    } else {
#else
    if ((copts.function_align16 ? 16 : 4) > data_00624DF4) {
        align = copts.function_align16 ? 16 : 4;
    } else {
#endif
        align = data_00624DF4;
    }
    savedsize = size;
    ELF_DefineSymbol(elfsym, elfsection->index, bind, 2, size, code, align); // STT_FUNC

    if (relocs) {
        part = &info->rel;
        part->section = ELF_NewSection(part->name, part->type, 0);
        ELF_SetRelSection(info->sect.section, part->section);
        for (r = relocs; r; r = r->next) {
            if (r->type == 2 && (copts.mips_option07 || copts.mips_option08)) {
                ObjGen_GetObjectAddress(r->target, &addr);
                if (copts.mips_option07 && addr.basereg == 28) {
                    pidcount++;
                }
                if (copts.mips_option08 && addr.basereg == 23) {
                    piccount++;
                }
            }
            if (r->target) {
                ELF_AddReloc(r->type, elfsym, r->offset, ObjGen_GetELFSymbol(r->target), 0);
            }
        }
    }

    if (copts.mips_option132 == 1) {
        func_00531D90(obj, bind != 0, elfsym, savedsize);
    }

    linkname = func_0049F6E0(obj)->name;
    ObjGen_GetObjectAddress(obj, &objaddr);
    if (objaddr.basereg == 23) {
        kind = 2;
    } else if (objaddr.basereg == 28) {
        kind = 0;
    } else {
        kind = 1;
    }

    if (piccount) {
        pictable = lalloc(piccount * 4);
        memset(pictable, 0, piccount * 4);
        pinfo = &data_005ECF98[data_00624BC4];
        pinfo->sect.section = ELF_NewSection(pinfo->sect.name, pinfo->sect.type, pinfo->flags);
        picsection = pinfo->sect.section;
        pinfo = &data_005ECF98[data_00624BC4];
        pinfo->rel.section = ELF_NewSection(pinfo->rel.name, pinfo->rel.type, 0);
        ELF_SetRelSection(pinfo->sect.section, pinfo->rel.section);
        name = galloc(strlen(linkname) + 7);
        sprintf(name, ".pic_%s", linkname);
        picsym = ELF_NewUndefinedSymbol(name, 0, 0, 3, 0, 0);
    }
    if (pidcount) {
        pidtable = lalloc(pidcount * 4);
        memset(pidtable, 0, pidcount * 4);
        pinfo = &data_005ECF98[data_00624D8C];
        part = &pinfo->sect;
        if (!code && pinfo->bss.name) {
            part = &pinfo->bss;
        }
        part->section = ELF_NewSection(part->name, part->type, pinfo->flags);
        pidsection = part->section;
        pinfo = &data_005ECF98[data_00624D8C];
        pinfo->rel.section = ELF_NewSection(pinfo->rel.name, pinfo->rel.type, 0);
        ELF_SetRelSection(pinfo->sect.section, pinfo->rel.section);
        name = galloc(strlen(linkname) + 7);
        sprintf(name, ".pid_%s", linkname);
        pidsym = ELF_NewUndefinedSymbol(name, 0, 0, 3, 0, 0);
    }

    if (pidcount + piccount) {
        di = 0;
        pi = 0;
        for (r = relocs; r; r = r->next) {
            if (r->type == 2) {
                ObjGen_GetObjectAddress(r->target, &addr);
                if (copts.mips_option08 && addr.basereg == 23) {
                    pictable[pi++] = r->offset + kind;
                }
                if (copts.mips_option07 && addr.basereg == 28) {
                    pidtable[di++] = r->offset + kind;
                }
            }
        }
        if (pi) {
            ELF_DefineSymbol(picsym, picsection->index, 0, 3, pi * 4, pictable, 4);
        }
        if (di) {
            ELF_DefineSymbol(pidsym, pidsection->index, 0, 3, di * 4, pidtable, 4);
        }

        di = 0;
        pi = 0;
        picoff = 0;
        pidoff = 0;
        for (r = relocs; r; r = r->next) {
            ObjGen_GetObjectAddress(r->target, &addr);
            if (copts.mips_option08 && addr.basereg == 23) {
                pictable[pi] = r->offset;
                ELF_AddReloc(2, picsym, picoff, elfsym, 0);
                picoff += 4;
                pi++;
            }
            if (copts.mips_option07 && addr.basereg == 28) {
                pidtable[di] = r->offset;
                ELF_AddReloc(2, pidsym, pidoff, elfsym, 0);
                pidoff += 4;
                di++;
            }
        }
    }
}

// the section whose ELF name ("." first) or section name this is, or 0
SInt32 func_00464210(char* name) {
    SInt32 i;

    if (name[0] == '.') {
        for (i = 1; i < data_00625310; i++) {
            if ((data_005ECF98[i].sect.name && !strcmp(name, data_005ECF98[i].sect.name)) ||
                (data_005ECF98[i].bss.name && !strcmp(name, data_005ECF98[i].bss.name))) {
                return i;
            }
        }
    } else {
        for (i = 1; i < data_00625310; i++) {
            if (!strcmp(name, data_005ECF98[i].name)) {
                return i;
            }
        }
    }
    return 0;
}

// __attribute__((...)) the target handles (CParser_ParseAttribute asks first):
// section, mode, weak, gp_safe and the import/export kinds. Nonzero if the
// attribute was this target's.
int func_004642D0(HashNameNode* attr, Type* type, DeclInfo* declinfo) {
    Type* stype;
    Type* utype;
    char* name;

    if (!strcmp(attr->name, "section") || !strcmp(attr->name, "__section__")) {
        if ((tk = lex()) != '(') {
            CError_Error(ERR_DECLARATION_SYNTAX_ERROR);
            return 1;
        }
        if ((tk = lex()) != TK_STRING) {
            CError_Error(ERR_DECLARATION_SYNTAX_ERROR);
            return 1;
        }

#if !MWCCPS2 // b210 (0x46F880) has neither
        // PSP library stubs and the module info get a section of their own
        if ((!strncmp(tkstring, ".psp_lib_mark_", 14) ||
             !strncmp(tkstring, ".psp_lib_markimport_", 20)) &&
            !func_00464210(tkstring) &&
            !ObjGen_DefineSection(tkstring + 1, tkstring, NULL, "far_absolute", "R")) {
            CError_Error(0x103);
            return 1;
        }
        if (!strcmp(tkstring, ".rodata.sceModuleInfo") &&
            !func_00464210(tkstring) &&
            !ObjGen_DefineSection(tkstring + 1, tkstring, NULL, "far_absolute", "R")) {
            CError_Error(0x103);
            return 1;
        }
#endif

#if MWCCPS2
        // b210: an unknown section leaves declinfo->section as it was
        {
            SInt32 sect;
            if (!(sect = func_00464210(tkstring))) {
                CError_Error(0x4D);
                return 1;
            }
            declinfo->section = sect;
        }
#else
        if (!(declinfo->section = func_00464210(tkstring))) {
            CError_Error(0x4D);
            return 1;
        }
#endif
        if ((tk = lex()) != ')') {
            CError_Error(ERR_DECLARATION_SYNTAX_ERROR);
        }
        return 1;
    }

    if (!strcmp(attr->name, "mode") || !strcmp(attr->name, "__mode__")) {
        if ((tk = lex()) != '(') {
            CError_Error(ERR_DECLARATION_SYNTAX_ERROR);
            return 1;
        }
        if ((tk = lex()) != TK_IDENTIFIER) {
            CError_Error(8);
            return 1;
        }

        name = tkidentifier->name;
        if (!strcmp(name, "QI") || !strcmp(name, "__QI__")) {
            stype = TYPE(&stsignedchar);
            utype = TYPE(&stunsignedchar);
        } else if (!strcmp(name, "HI") || !strcmp(name, "__HI__")) {
            stype = TYPE(&stsignedshort);
            utype = TYPE(&stunsignedshort);
        } else if (!strcmp(name, "SI") || !strcmp(name, "__SI__")) {
            stype = TYPE(&stsignedint);
            utype = TYPE(&stunsignedint);
        } else if (!strcmp(name, "DI") ||
                   !strcmp(name, "__DI__") ||
                   !strcmp(name, "V2SI") ||
                   !strcmp(name, "__V2SI__")) {
            stype = TYPE(&stsignedlonglong);
            utype = TYPE(&stunsignedlonglong);
        } else if (!strcmp(name, "TI") ||
                   !strcmp(name, "__TI__") ||
                   !strcmp(name, "V4SF") ||
                   !strcmp(name, "__V4SF__") ||
                   !strcmp(name, "V2DI") ||
                   !strcmp(name, "__V2DI__") ||
                   !strcmp(name, "V4SI") ||
                   !strcmp(name, "__V4SI__") ||
                   !strcmp(name, "V8HI") ||
                   !strcmp(name, "__V8HI__") ||
                   !strcmp(name, "V16QI") ||
                   !strcmp(name, "__V16QI__")) {
            stype = TYPE(&stint128);
            utype = TYPE(&stint128);
        } else if (!strcmp(name, "SF") || !strcmp(name, "__SF__")) {
            utype = NULL;
            stype = TYPE(&stfloat);
        } else if (!strcmp(name, "DF") || !strcmp(name, "__DF__")) {
            utype = NULL;
            stype = TYPE(&stdouble);
        } else {
            CError_Error(0x4D);
            return 1;
        }

        if ((tk = lex()) != ')') {
            CError_Error(ERR_DECLARATION_SYNTAX_ERROR);
            return 1;
        }
        if (is_unsigned(declinfo->thetype)) {
            if (!utype) {
                CError_Error(0x4D);
                return 1;
            }
            declinfo->thetype = utype;
        } else {
            declinfo->thetype = stype;
        }
        return 1;
    }

    if (!strcmp(attr->name, "weak") || !strcmp(attr->name, "__weak__")) {
        declinfo->qual |= Q_WEAK;
        return 1;
    }
#if !MWCCPS2
    if (!strcmp(attr->name, "gp_safe") || !strcmp(attr->name, "__gp_safe__")) {
        return 1;
    }
#endif
    if (!strcmp(attr->name, "internal") || !strcmp(attr->name, "__internal__")) {
        declinfo->exportflags |= 0x10;
        return 1;
    }
    if (!strcmp(attr->name, "import") ||
        !strcmp(attr->name, "__import__") ||
        !strcmp(attr->name, "dllimport") ||
        !strcmp(attr->name, "__dllimport__")) {
        declinfo->exportflags |= 0x20;
        return 1;
    }
    if (!strcmp(attr->name, "export") ||
        !strcmp(attr->name, "__export__") ||
        !strcmp(attr->name, "dllexport") ||
        !strcmp(attr->name, "__dllexport__")) {
        declinfo->exportflags |= 0x40;
        return 1;
    }
    if (!strcmp(attr->name, "lib_export") || !strcmp(attr->name, "__lib_export__")) {
        declinfo->exportflags |= 0x60;
        return 1;
    }
    return 0;
}

extern SInt32 data_00625010; // word size - 1

// an object's size rounded up to whole words
SInt32 func_00464B20(Object* obj) {
    return (obj->type->size + data_00625010) & ~data_00625010;
}

// the kind of an exception table's addressing: 1 absolute, 2 code-relative
static inline SInt32 ObjGen_ExceptionTableKind(SInt32 sec) {
    switch (data_005ECF98[sec].mode) {
        case 1:
        case 2:
            return 1;
        case 3:
        case 4:
            return 2;
        case 5:
        case 6:
            return 0;
        default:
            CError_FATAL(1126);
            return 0;
    }
}

// 0x4624F0: ObjGen_DeclareExceptionTables: the function's index entry
// ("@EX@name" in exceptix: its address, size and table) and, unless the table
// fits in the entry, the table ("@ET@name" in exception)
void func_004624F0(Object* func, SInt32 codesize, void* data, SInt32 datasize, OLinkList* refs) {
    char* name;
    char* etname;
    char* exname;
    ObjSectionInfo* info;
    ELFSection* ixsec;
    ELFSection* exsec;
    ELFSymbol* exsym;
    ELFSymbol* ixsym;
    OLinkList* r;
    ObjAddress addr;
    SInt32 adjust;
    SInt32 rec[3];

    name = CMangler_GetLinkName(func)->name;
    exname = galloc(strlen(name) + 5);
    strcpy(exname, "@ET@");
    strcat(exname, name);
    etname = galloc(strlen(name) + 5);
    strcpy(etname, "@EX@");
    strcat(etname, name);

    info = &data_005ECF98[data_0062549C];
    info->sect.section = ELF_NewSection(info->sect.name, info->sect.type, info->flags);
    ixsec = info->sect.section;
    info = &data_005ECF98[data_0062549C];
    info->rel.section = ELF_NewSection(info->rel.name, info->rel.type, 0);
    ELF_SetRelSection(info->sect.section, info->rel.section);

    if ((UInt32)datasize <= 4) {
        rec[0] = 0;
        rec[1] = codesize | 1;
        rec[2] = 0;
        memcpy(&rec[2], data, datasize);
        ixsym = ELF_NewSymbol(etname, ixsec->index, 0, 1, 12, rec, 4);
        ELF_AddReloc(2, ixsym, 0, ObjGen_GetELFSymbol(func), 0);
        return;
    }

    info = &data_005ECF98[data_00624FDC];
    info->sect.section = ELF_NewSection(info->sect.name, info->sect.type, info->flags);
    exsec = info->sect.section;
    for (r = refs; r; r = r->next) {
        if (copts.mips_option08 || copts.mips_option07) {
            ObjGen_GetObjectAddress(r->obj, &addr);
            if (addr.basereg == 23) {
                adjust = 2;
            } else if (addr.basereg == 28) {
                adjust = 0;
            } else {
                adjust = 1;
            }
        } else {
            adjust = 0;
        }
        *(SInt32*)((char*)data + r->offset) += adjust;
    }
    exsym = ELF_NewSymbol(exname, exsec->index, 0, 1, datasize, data, 4);
    if (refs) {
        info = &data_005ECF98[data_00624FDC];
        info->rel.section = ELF_NewSection(info->rel.name, info->rel.type, 0);
        ELF_SetRelSection(info->sect.section, info->rel.section);
        for (r = refs; r; r = r->next) {
            ELF_AddReloc(2, exsym, r->offset, ObjGen_GetELFSymbol(r->obj), r->addend);
        }
    }
    rec[0] = 0;
    rec[1] = codesize;
    rec[2] =
        (copts.mips_option08 || copts.mips_option07) ? ObjGen_ExceptionTableKind(data_00624FDC) : 0;
    ixsym = ELF_NewSymbol(etname, ixsec->index, 0, 1, 12, rec, 4);
    ELF_AddReloc(2, ixsym, 0, ObjGen_GetELFSymbol(func), 0);
    ELF_AddReloc(2, ixsym, 8, exsym, 0);
}

// 0x462300
// 0x4622E0: #pragma segment has no effect
void ObjGen_SegmentName(HashNameNode* name) {}

// 0x4622F0: the source file the following code comes from
void ObjGen_SrcBreakName(HashNameNode* name, unsigned long long fileModDate, Boolean unused) {
    data_006245BC = name;
}

void ObjGen_SetupSym(void) {}

// 0x462F00: constant data: as other data, but the object file is marked as
// having some (data_00625E9B)
void ObjGen_DeclareReadOnlyData(
    Object* object, const void* data, OLinkList* list, unsigned int size
) {
    data_00625E9B = 1;
    ObjGen_DeclareData(object, data, list, size);
}
