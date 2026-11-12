#include "psp/ELFgen.h"
#include <string.h>
#include "compiler/CError.h"
#include "compiler/CompilerTools.h"

// mwccpsp 3.0.1 ELFgen.c (0x5309D0..0x531730). Functions are in the order
// the original links them, except the section data helpers DWARF output
// uses (0x531420..0x531580), at the end.

// Store a word in the object file's byte order.
#define PUT32(p, v) \
    do { \
        UInt32 _v = (v); \
        if (elf_encoding == 1) { \
            (p)[0] = _v; \
            (p)[1] = _v >> 8; \
            (p)[2] = _v >> 16; \
            (p)[3] = _v >> 24; \
        } else { \
            (p)[0] = _v >> 24; \
            (p)[1] = _v >> 16; \
            (p)[2] = _v >> 8; \
            (p)[3] = _v; \
        } \
    } while (0)

#define PUT16(p, v) \
    do { \
        UInt16 _v = (v); \
        if (elf_encoding == 1) { \
            (p)[0] = _v; \
            (p)[1] = _v >> 8; \
        } else { \
            (p)[0] = _v >> 8; \
            (p)[1] = _v; \
        } \
    } while (0)

static Elf32_Sym elf_nullsym;
static UInt8 elf_zeros[0x40];
static SInt32 elf_encoding; // 1: little endian
static ELFSymbol** elf_symtail;
ELFSymbol* elf_symbols;
ELFSection* elf_shstrtab;
ELFSection* elf_strtab;
ELFSection* elf_symtab;
ELFSection** elf_sections;
static SInt32 elf_sectioncapacity;
Elf32_Ehdr elf_ehdr;

static void ELF_WriteRelocEntry(ELFSection* section, Elf32_Rela* rela);
static void ELF_WriteSymEntry(ELFSection* section, Elf32_Sym* sym);

void ELF_WriteShdr(GList* gl, Elf32_Shdr* shdr) {
    SInt32 offset;
    UInt8* p;

    offset = gl->size;
    AppendGListNoData(gl, sizeof(Elf32_Shdr));
    p = (UInt8*)&(*gl->data)[offset];
    PUT32(p, shdr->sh_name);
    p = (UInt8*)&(*gl->data)[offset + 4];
    PUT32(p, shdr->sh_type);
    p = (UInt8*)&(*gl->data)[offset + 8];
    PUT32(p, shdr->sh_flags);
    p = (UInt8*)&(*gl->data)[offset + 12];
    PUT32(p, shdr->sh_addr);
    p = (UInt8*)&(*gl->data)[offset + 16];
    PUT32(p, shdr->sh_offset);
    p = (UInt8*)&(*gl->data)[offset + 20];
    PUT32(p, shdr->sh_size);
    p = (UInt8*)&(*gl->data)[offset + 24];
    PUT32(p, shdr->sh_link);
    p = (UInt8*)&(*gl->data)[offset + 28];
    PUT32(p, shdr->sh_info);
    p = (UInt8*)&(*gl->data)[offset + 32];
    PUT32(p, shdr->sh_addralign);
    p = (UInt8*)&(*gl->data)[offset + 36];
    PUT32(p, shdr->sh_entsize);
}

void ELF_Init(UInt16 machine, UInt32 flags, SInt32 encoding) {
    elf_encoding = encoding;
    elf_ehdr.e_ident[0] = 0x7F;
    elf_ehdr.e_ident[1] = 'E';
    elf_ehdr.e_ident[2] = 'L';
    elf_ehdr.e_ident[3] = 'F';
    elf_ehdr.e_ident[4] = 1; // ELFCLASS32
    elf_ehdr.e_ident[5] = encoding;
    elf_ehdr.e_ident[6] = 1; // EV_CURRENT
    elf_ehdr.e_type = 1; // ET_REL
    elf_ehdr.e_machine = machine;
    elf_ehdr.e_version = 1;
    elf_ehdr.e_flags = flags;
    elf_ehdr.e_ehsize = sizeof(Elf32_Ehdr);
    elf_ehdr.e_shentsize = sizeof(Elf32_Shdr);
    elf_ehdr.e_shnum = 0;
    elf_sectioncapacity = 0;
    elf_sections = NULL;
    elf_symbols = NULL;
    elf_symtail = &elf_symbols;
    ELF_NewSection("", SHT_NULL, 0);
    elf_symtab = ELF_NewSection(".symtab", SHT_SYMTAB, 0);
    elf_strtab = ELF_NewSection(".strtab", SHT_STRTAB, 0);
    elf_shstrtab = ELF_NewSection(".shstrtab", SHT_STRTAB, 0);
}

void ELF_WriteRelocs(ELFSection* relsection) {
    ELFReloc* reloc;

    for (reloc = relsection->relocs; reloc; reloc = reloc->next) {
        reloc->rela.r_info = (reloc->symbol->index << 8) + (UInt8)reloc->rela.r_info;
        ELF_WriteRelocEntry(relsection, &reloc->rela);
    }
}

static void ELF_WriteRelocEntry(ELFSection* section, Elf32_Rela* rela) {
    GList* gl;
    SInt32 type;
    SInt32 offset;
    SInt32 size;
    UInt8* p;

    gl = &section->data;
    type = section->shdr.sh_type;
    offset = gl->size;
    size = (type == SHT_RELA) ? sizeof(Elf32_Rela) : 8;
    section->shdr.sh_size += size;
    AppendGListNoData(gl, size);
    p = (UInt8*)&(*gl->data)[offset];
    PUT32(p, rela->r_offset);
    p = (UInt8*)&(*gl->data)[offset + 4];
    PUT32(p, rela->r_info);
    if (type == SHT_RELA) {
        p = (UInt8*)&(*gl->data)[offset + 8];
        PUT32(p, rela->r_addend);
    }
}

void ELF_AddReloc(UInt8 type, ELFSymbol* at, UInt32 offset, ELFSymbol* to, SInt32 addend) {
    ELFSection* relsection;
    ELFReloc* reloc;

    relsection = elf_sections[at->sym.st_shndx]->relsection;
    reloc = galloc(sizeof(ELFReloc));
    reloc->next = NULL;
    reloc->symbol = to;
    reloc->rela.r_offset = at->sym.st_value + offset;
    reloc->rela.r_info = type;
    reloc->rela.r_addend = addend;
    *relsection->reltail = reloc;
    relsection->reltail = &reloc->next;
}

void ELF_BuildSymtab(void) {
    ELFSymbol* sym;
    SInt32 index;

    elf_symtab->shdr.sh_link = elf_strtab->index;
    index = 1;
    elf_strtab->shdr.sh_size += 1;
    AppendGListData(&elf_strtab->data, "", 1);
    for (sym = elf_symbols; sym; sym = sym->next) {
        sym->sym.st_name = ELF_AddString(elf_strtab, sym->name);
    }

    ELF_WriteSymEntry(elf_symtab, &elf_nullsym);
    for (sym = elf_symbols; sym; sym = sym->next) {
        if ((sym->sym.st_info >> 4) == 0) { // STB_LOCAL first, as ELF requires
            sym->index = index++;
            ELF_WriteSymEntry(elf_symtab, &sym->sym);
        }
    }
    elf_symtab->shdr.sh_info = index;
    for (sym = elf_symbols; sym; sym = sym->next) {
        if ((sym->sym.st_info >> 4) != 0) {
            sym->index = index++;
            ELF_WriteSymEntry(elf_symtab, &sym->sym);
        }
    }
}

static void ELF_WriteSymEntry(ELFSection* section, Elf32_Sym* sym) {
    GList* gl;
    SInt32 offset;
    UInt8* p;

    gl = &section->data;
    offset = gl->size;
    section->shdr.sh_size += sizeof(Elf32_Sym);
    AppendGListNoData(gl, sizeof(Elf32_Sym));
    p = (UInt8*)&(*gl->data)[offset];
    PUT32(p, sym->st_name);
    p = (UInt8*)&(*gl->data)[offset + 4];
    PUT32(p, sym->st_value);
    p = (UInt8*)&(*gl->data)[offset + 8];
    PUT32(p, sym->st_size);
    (*gl->data)[offset + 12] = sym->st_info;
    (*gl->data)[offset + 13] = sym->st_other;
    p = (UInt8*)&(*gl->data)[offset + 14];
    PUT16(p, sym->st_shndx);
}

ELFSymbol* ELF_NewSymbol(
    char* name, UInt16 shndx, UInt8 bind, UInt8 type, UInt32 size, void* data, UInt32 align
) {
    ELFSymbol* sym;

    sym = galloc(sizeof(ELFSymbol));
    memclrw(sym, sizeof(ELFSymbol));
    sym->name = name;
    sym->sym.st_shndx = 0;
    sym->sym.st_info = 0x10; // STB_GLOBAL, STT_NOTYPE
    sym->sym.st_size = 0;
    sym->sym.st_value = 0;
    *elf_symtail = sym;
    elf_symtail = &sym->next;
    ELF_DefineSymbol(sym, shndx, bind, type, size, data, align);
    return sym;
}

void ELF_DefineSymbol(
    ELFSymbol* sym, UInt16 shndx, UInt8 bind, UInt8 type, UInt32 size, void* data, UInt32 align
) {
    ELFSection* section;
    UInt32 misalign;
    UInt32 offset;

    CError_ASSERT(470, sym->sym.st_shndx == 0);
    CError_ASSERT(474, shndx < 0xFF00);
    sym->sym.st_shndx = shndx;
    sym->sym.st_info = (bind << 4) + (type & 0xF);
    sym->sym.st_size = size;
    section = elf_sections[shndx];

#if MWCCPS2 // b210 (0x540B30) tests the alignment signed
    if ((SInt32)align > 1 && (misalign = section->shdr.sh_size & (align - 1))) {
#else
    if (align > 1 && (misalign = section->shdr.sh_size & (align - 1))) {
#endif
        if (section->shdr.sh_type != SHT_NOBITS) {
            AppendGListData(&section->data, elf_zeros, align - misalign);
        }
        section->shdr.sh_size += align - misalign;
    }
    if (align > section->shdr.sh_addralign) {
        section->shdr.sh_addralign = align;
    }

    if (section->shdr.sh_type == SHT_NOBITS) {
        sym->sym.st_value = section->shdr.sh_size;
        section->shdr.sh_size += size;
    } else if (data) {
        offset = section->shdr.sh_size;
        section->shdr.sh_size += size;
        AppendGListData(&section->data, data, size);
        sym->sym.st_value = offset;
    } else {
        offset = section->shdr.sh_size;
        section->shdr.sh_size += size;
        while (size > sizeof(elf_zeros)) {
            AppendGListData(&section->data, elf_zeros, sizeof(elf_zeros));
            size -= sizeof(elf_zeros);
        }
        AppendGListData(&section->data, elf_zeros, size);
        sym->sym.st_value = offset;
    }
}

ELFSymbol* ELF_NewUndefinedSymbol(
    char* name, UInt16 shndx, UInt8 bind, UInt8 type, UInt32 size, UInt32 value
) {
    ELFSymbol* sym;

    sym = galloc(sizeof(ELFSymbol));
    memclrw(sym, sizeof(ELFSymbol));
    sym->name = name;
    sym->sym.st_shndx = shndx;
    sym->sym.st_info = (bind << 4) + (type & 0xF);
    sym->sym.st_size = size;
    sym->sym.st_value = value;
    *elf_symtail = sym;
    elf_symtail = &sym->next;
    return sym;
}

SInt32 ELF_AddString(ELFSection* section, char* str) {
    SInt32 len;
    SInt32 offset;

    len = strlen(str) + 1;
    offset = section->shdr.sh_size;
    section->shdr.sh_size += len;
    AppendGListData(&section->data, str, len);
    return offset;
}

void ELF_SetRelSection(ELFSection* section, ELFSection* relsection) {
    section->relsection = relsection;
    relsection->shdr.sh_info = section->index;
    relsection->shdr.sh_link = elf_symtab->index;
}

ELFSection* ELF_NewSection(char* name, SInt32 type, SInt32 flags) {
    ELFSection* section;
    ELFSection** old;
    SInt32 i;

    if (elf_ehdr.e_shnum == elf_sectioncapacity) {
        old = elf_sections;
        elf_sections = galloc((elf_sectioncapacity + 0x400) * sizeof(ELFSection*));
        for (i = 0; i < elf_sectioncapacity; i++) {
            elf_sections[i] = old[i];
        }
        elf_sectioncapacity += 0x400;
    }

    section = galloc(sizeof(ELFSection));
    memclrw(section, sizeof(ELFSection));
    section->name = name;
    section->index = elf_ehdr.e_shnum;
    section->reltail = &section->relocs;
    section->shdr.sh_type = type;
    section->shdr.sh_flags = flags;
    if (InitGList(&section->data, 0x200)) {
        CError_NoMem();
    }
    if (type == SHT_RELA) {
        section->shdr.sh_entsize = sizeof(Elf32_Rela);
    } else if (type == SHT_REL) {
        section->shdr.sh_entsize = 8;
    } else if (type == SHT_SYMTAB) {
        section->shdr.sh_entsize = sizeof(Elf32_Sym);
    }
    elf_sections[section->index] = section;
    elf_ehdr.e_shnum++;
    return section;
}

// ---------------------------------------------------------------------------
// Writing the object file (0x5302D0..0x5309D0, before the functions above in
// the link).

// Code, initialized and uninitialized data sizes, for the IDE's statistics.
void func_005302D0(SInt32* codesize, SInt32* idatasize, SInt32* udatasize) {
    int i;
    ELFSection* section;

    *codesize = 0;
    *idatasize = 0;
    *udatasize = 0;
    for (i = 1; i < elf_ehdr.e_shnum; i++) {
        section = elf_sections[i];
        if (section->shdr.sh_flags & 4) {
            *codesize += section->shdr.sh_size;
        } else if (section->shdr.sh_type == SHT_NOBITS && (section->shdr.sh_flags & 2)) {
            *udatasize += section->shdr.sh_size;
        } else if (section->shdr.sh_type == SHT_PROGBITS && (section->shdr.sh_flags & 2)) {
            *idatasize += section->shdr.sh_size;
        }
    }
}

void func_00530360(void) {
    int i;

    for (i = 1; i < elf_ehdr.e_shnum; i++) {
        FreeGList(&elf_sections[i]->data);
    }
}

// The ELF header, over the space reserved at the start of the file.
void func_00530600(GList* gl, Elf32_Ehdr* ehdr) {
    int i;
    UInt8* p;

    for (i = 0; i < 16; i++) {
        (*gl->data)[i] = ehdr->e_ident[i];
    }
    p = (UInt8*)*gl->data + 0x10;
    PUT16(p, ehdr->e_type);
    p = (UInt8*)*gl->data + 0x12;
    PUT16(p, ehdr->e_machine);
    p = (UInt8*)*gl->data + 0x14;
    PUT32(p, ehdr->e_version);
    p = (UInt8*)*gl->data + 0x18;
    PUT32(p, ehdr->e_entry);
    p = (UInt8*)*gl->data + 0x1C;
    PUT32(p, ehdr->e_phoff);
    p = (UInt8*)*gl->data + 0x20;
    PUT32(p, ehdr->e_shoff);
    p = (UInt8*)*gl->data + 0x24;
    PUT32(p, ehdr->e_flags);
    p = (UInt8*)*gl->data + 0x28;
    PUT16(p, ehdr->e_ehsize);
    p = (UInt8*)*gl->data + 0x2A;
    PUT16(p, ehdr->e_phentsize);
    p = (UInt8*)*gl->data + 0x2C;
    PUT16(p, ehdr->e_phnum);
    p = (UInt8*)*gl->data + 0x2E;
    PUT16(p, ehdr->e_shentsize);
    p = (UInt8*)*gl->data + 0x30;
    PUT16(p, ehdr->e_shnum);
    p = (UInt8*)*gl->data + 0x32;
    PUT16(p, ehdr->e_shstrndx);
}

// The file: section contents (16-aligned), section headers, ELF header.
void func_005303A0(GList* gl) {
    int i;
    ELFSection* section;
    SInt32 len;
    SInt32 offset;

    ELF_BuildSymtab();
    for (i = 1; i < elf_ehdr.e_shnum; i++) {
        section = elf_sections[i];
        if (section->shdr.sh_type == SHT_REL || section->shdr.sh_type == SHT_RELA) {
            ELF_WriteRelocs(section);
        }
    }
    elf_ehdr.e_shstrndx = elf_shstrtab->index;
    elf_shstrtab->shdr.sh_size += 1;
    AppendGListData(&elf_shstrtab->data, "", 1);
    for (i = 1; i < elf_ehdr.e_shnum; i++) {
        section = elf_sections[i];
        len = strlen(section->name) + 1;
        offset = elf_shstrtab->shdr.sh_size;
        elf_shstrtab->shdr.sh_size += len;
        AppendGListData(&elf_shstrtab->data, section->name, len);
        elf_sections[i]->shdr.sh_name = offset;
    }

    AppendGListNoData(gl, sizeof(Elf32_Ehdr));
    for (i = 1; i < elf_ehdr.e_shnum; i++) {
        section = elf_sections[i];
        if (gl->size & 0xF) {
            AppendGListData(gl, elf_zeros, 16 - (gl->size & 0xF));
        }
        offset = gl->size;
        if (section->shdr.sh_type != SHT_NOBITS) {
            LockGList(&section->data);
            AppendGListData(gl, *section->data.data, section->data.size);
            UnlockGList(&section->data);
            FreeGList(&section->data);
        }
        elf_sections[i]->shdr.sh_offset = offset;
    }
    if (gl->size & 0xF) {
        AppendGListData(gl, elf_zeros, 16 - (gl->size & 0xF));
    }
    elf_ehdr.e_shoff = gl->size;
    for (i = 0; i < elf_ehdr.e_shnum; i++) {
        ELF_WriteShdr(gl, &elf_sections[i]->shdr);
    }
    func_00530600(gl, &elf_ehdr);
    ShrinkGList(gl);
}

// 0x531420: store a halfword at offset in a section's data
void func_00531420(ELFSection* section, SInt32 offset, UInt16 value) {
    UInt8* p = (UInt8*)*section->data.data + offset;
    PUT16(p, value);
}

// 0x531460: store a word at offset in a section's data
void func_00531460(ELFSection* section, SInt32 offset, UInt32 value) {
    UInt8* p = (UInt8*)*section->data.data + offset;
    PUT32(p, value);
}

// 0x5314F0: append a byte; its offset
SInt32 func_005314F0(ELFSection* section, UInt8 value) {
    SInt32 offset = section->shdr.sh_size++;
    AppendGListNoData(&section->data, 1);
    (*section->data.data)[offset] = value;
    return offset;
}

// 0x531520: append a halfword; its offset
SInt32 func_00531520(ELFSection* section, UInt16 value) {
    SInt32 offset = section->shdr.sh_size;
    UInt8* p;

    section->shdr.sh_size += 2;
    AppendGListNoData(&section->data, 2);
    p = (UInt8*)*section->data.data + offset;
    PUT16(p, value);
    return offset;
}

// 0x531580: append a word; its offset
SInt32 func_00531580(ELFSection* section, UInt32 value) {
    SInt32 offset = section->shdr.sh_size;
    UInt8* p;

    section->shdr.sh_size += 4;
    AppendGListNoData(&section->data, 4);
    p = (UInt8*)*section->data.data + offset;
    PUT32(p, value);
    return offset;
}

// 0x5302C0: the section with this index
ELFSection* func_005302C0(UInt16 index) {
    return elf_sections[index];
}
