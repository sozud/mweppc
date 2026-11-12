// mwccpsp 3.0.1 inline assembler name tables (0x53E980..0x53ECC0, next to
// GCCInlineAsm.c's InlineAsm_SkipComment): mnemonics map to the opcodes that
// spell them, register names to a class and number.
#include "compiler/common.h"
#include "compiler/CompilerTools.h"
#include <string.h>
#if MWCCPSP_NATIVE
#include <stdio.h>
#include <stdlib.h>
#endif

// a mnemonic and the opcodes with that name (0x1C bytes)
typedef struct AsmMnemonic {
    struct AsmMnemonic* next;
    char* name; // +4: the lookup returns &name
    SInt32 capacity; // +8
    SInt32 count; // +C
    SInt32* opcodes; // +10
    SInt32 unk14;
    SInt32 unk18;
} AsmMnemonic;

// a register name (0x14 bytes)
typedef struct AsmRegister {
    struct AsmRegister* next;
    char* name; // +4: the lookup returns &name
    SInt32 rclass; // +8
    SInt32 num; // +C
    void* extra; // +10: a register variable's object (AsmRegRec.object)
} AsmRegister;

static AsmMnemonic* asm_mnemonics[0x100]; // 0x605C70
static AsmRegister* asm_registers[0x200]; // 0x606070

// the mnemonic's record (from its name field), or NULL
void* func_0053E980(char* name) {
#if MWCCPS2
    int h;
#endif
    AsmMnemonic* m;

#if MWCCPS2
    h = CHash(name);
    for (m = asm_mnemonics[(UInt8)h]; m; m = m->next) {
        if (!strcmp(m->name, name)) {
            return &m->name;
        }
    }
#else
    for (m = asm_mnemonics[CHash(name) & 0xFF]; m; m = m->next) {
        if (!strcmp(m->name, name)) {
            return &m->name;
        }
    }
#endif
    return NULL;
}

// add an opcode under its mnemonic
void* func_0053E9C0(char* name, SInt32 opcode) {
    AsmMnemonic** bucket;
    AsmMnemonic* m;
    SInt32* old;
    SInt32 n;
    SInt32 i;

    bucket = &asm_mnemonics[CHash(name) & 0xFF];
    for (m = *bucket; m; m = m->next) {
        if (!strcmp(m->name, name)) {
            if (m->count == m->capacity) {
                n = m->capacity;
                old = m->opcodes;
                m->opcodes = lalloc(n * 2 * sizeof(SInt32));
                for (i = 0; i < n; i++) {
                    m->opcodes[i] = old[i];
                }
                m->capacity <<= 1;
            }
            m->opcodes[m->count++] = opcode;
            return &m->name;
        }
    }

    m = lalloc(sizeof(AsmMnemonic));
    m->name = lalloc(strlen(name) + 1);
    strcpy(m->name, name);
    m->capacity = 1;
    m->opcodes = lalloc(m->capacity * sizeof(SInt32));
    m->count = 1;
    m->opcodes[0] = opcode;
    m->next = *bucket;
    *bucket = m;
    return &m->name;
}

void func_0053EB40(void) {
    int i;

    for (i = 0; i < 0x100; i++) {
        asm_mnemonics[i] = NULL;
    }
}

// the register's record (from its name field), or NULL
void* func_0053EBB0(char* name) {
    AsmRegister* r;
#if MWCCPS2
    char** pname;

    for (r = asm_registers[CHash(name) & 0x1FF]; r != NULL; r = r->next) {
        pname = &r->name;
        if (!strcmp(*pname, name)) {
            return pname;
        }
    }
    return NULL;
#else

    for (r = asm_registers[CHash(name) & 0x1FF]; r; r = r->next) {
        if (!strcmp(r->name, name)) {
            return &r->name;
        }
    }
    return NULL;
#endif
}

void func_0053EC00(char* name, SInt16 rclass, SInt16 num, void* extra) {
    AsmRegister** bucket;
    AsmRegister* r;

    bucket = &asm_registers[CHash(name) & 0x1FF];
    r = lalloc(sizeof(AsmRegister));
    r->name = name;
    r->rclass = rclass;
    r->num = num;
    r->extra = extra;
    r->next = *bucket;
    *bucket = r;
}

void func_0053EC50(void) {
    int i;

    for (i = 0; i < 0x200; i++) {
        asm_registers[i] = NULL;
    }
}
