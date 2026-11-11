#ifndef PSP_ELFGEN_H
#define PSP_ELFGEN_H

#include "compiler/common.h"
#include "compiler/CompilerTools.h"

// ELF object writer of mwccpsp 3.0.1 (ELFgen.c in the original; none of the
// function names survived, so the names here are descriptive).

typedef struct Elf32_Ehdr {
    UInt8 e_ident[16];
    UInt16 e_type;
    UInt16 e_machine;
    UInt32 e_version;
    UInt32 e_entry;
    UInt32 e_phoff;
    UInt32 e_shoff;
    UInt32 e_flags;
    UInt16 e_ehsize;
    UInt16 e_phentsize;
    UInt16 e_phnum;
    UInt16 e_shentsize;
    UInt16 e_shnum;
    UInt16 e_shstrndx;
} Elf32_Ehdr;

typedef struct Elf32_Shdr {
    UInt32 sh_name;
    UInt32 sh_type;
    UInt32 sh_flags;
    UInt32 sh_addr;
    UInt32 sh_offset;
    UInt32 sh_size;
    UInt32 sh_link;
    UInt32 sh_info;
    UInt32 sh_addralign;
    UInt32 sh_entsize;
} Elf32_Shdr;

typedef struct Elf32_Sym {
    UInt32 st_name;
    UInt32 st_value;
    UInt32 st_size;
    UInt8 st_info;
    UInt8 st_other;
    UInt16 st_shndx;
} Elf32_Sym;

typedef struct Elf32_Rela {
    UInt32 r_offset;
    UInt32 r_info; // holds just the type until the symbol indices are known
    SInt32 r_addend;
} Elf32_Rela;

typedef struct ELFSymbol {
    struct ELFSymbol* next;
    char* name;
    SInt32 index; // index in .symtab
    Elf32_Sym sym;
} ELFSymbol; // 0x1C

typedef struct ELFReloc {
    struct ELFReloc* next;
    ELFSymbol* symbol;
    Elf32_Rela rela;
} ELFReloc; // 0x14

typedef struct ELFSection {
    char* name;
    SInt32 index;
    GList data;
    struct ELFSection* relsection;
    ELFReloc* relocs;
    ELFReloc** reltail;
    Elf32_Shdr shdr;
} ELFSection; // 0x4C

enum {
    SHT_NULL = 0,
    SHT_PROGBITS = 1,
    SHT_SYMTAB = 2,
    SHT_STRTAB = 3,
    SHT_RELA = 4,
    SHT_NOBITS = 8,
    SHT_REL = 9
};

extern Elf32_Ehdr elf_ehdr;
extern ELFSection** elf_sections;
extern ELFSection* elf_symtab;
extern ELFSection* elf_strtab;
extern ELFSection* elf_shstrtab;
extern ELFSymbol* elf_symbols;

extern void ELF_Init(UInt16 machine, UInt32 flags, SInt32 encoding);
extern ELFSection* ELF_NewSection(char* name, SInt32 type, SInt32 flags);
extern void ELF_SetRelSection(ELFSection* section, ELFSection* relsection);
extern SInt32 ELF_AddString(ELFSection* section, char* str);
extern ELFSymbol* ELF_NewUndefinedSymbol(
    char* name, UInt16 shndx, UInt8 bind, UInt8 type, UInt32 size, UInt32 value
);
extern ELFSymbol* ELF_NewSymbol(
    char* name, UInt16 shndx, UInt8 bind, UInt8 type, UInt32 size, void* data, UInt32 align
);
extern void ELF_DefineSymbol(
    ELFSymbol* sym, UInt16 shndx, UInt8 bind, UInt8 type, UInt32 size, void* data, UInt32 align
);
extern void ELF_AddReloc(UInt8 type, ELFSymbol* at, UInt32 offset, ELFSymbol* to, SInt32 addend);
extern void ELF_WriteRelocs(ELFSection* relsection);
extern void ELF_BuildSymtab(void);
extern void ELF_WriteShdr(GList* gl, Elf32_Shdr* shdr);

#endif
