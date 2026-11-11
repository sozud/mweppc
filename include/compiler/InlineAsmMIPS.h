#ifndef COMPILER_INLINEASMMIPS_H
#define COMPILER_INLINEASMMIPS_H

// mwccpsp 3.0.1: the MIPS inline assembler's statement records (an ST_ASM
// statement's expr)

#include "compiler/common.h"
#include "compiler/CInt64.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MOperandType {
    MOpnd_Imm = 0, // u.imm
    MOpnd_Reg = 1, // u.reg: a register, or the local variable in one
    MOpnd_GlobalObj = 2, // u.obj
    MOpnd_LocalObj = 3, // u.obj
    MOpnd_Lab = 4, // u.lab
    MOpnd_GCCReg = 5, // u.gcc: %N, a register operand of a GCC asm statement
    MOpnd_GCCImm = 6 // u.gcc: %N, its immediate operand
} MOperandType;

// an operand (0x10 bytes)
typedef struct MAsmOperand {
    UInt8 type;
    UInt8 pad1[3];
    union {
        CInt64 imm;
        struct {
            SInt32 num;
            struct Object* object;
        } reg;
        struct {
            SInt32 offset;
            struct Object* obj;
        } obj;
        struct {
            struct CLabel* label;
        } lab;
        struct {
            SInt32 num; // the GCC operand's index
            SInt16 effect;
            SInt16 rclass;
        } gcc;
    } u;
    SInt16 effect; // a register's: 1 read, 2 written
    SInt16 rclass;
} MAsmOperand;

// an instruction: 8 bytes and its operands
typedef struct MAsmInstruction {
    SInt32 opcode;
    SInt16 flags; // 4 volatile, 8 ...
    SInt16 nops;
    MAsmOperand ops[1];
} MAsmInstruction;

typedef struct OpcodeDef {
    char* name;
    char* alias;
    char* format;
    UInt8 nargs;
    UInt8 size;
    UInt8 pad0E[2];
    UInt32 unk10;
    UInt8 pad14[4];
    UInt32 flags;
    UInt32 flags2;
    char* intrinsic;
    SInt32 intrinsicid;
    SInt32 unk28;
    UInt16 unk2C;
} OpcodeDef; // 0x30
extern OpcodeDef data_006115F8[]; // the opcodes (0x5C5)
extern UInt16 data_00625CE4[]; // an opcode's form with a register offset, from 0x518

// a name's meaning (InlineAsm_LookupSymbolMIPS)
typedef struct MLookupResult {
    struct HashNameNode* name;
    struct CLabel* label;
    struct Object* object;
    struct Type* type;
    CInt64 value;
    Boolean has_value;
} MLookupResult;

extern Boolean InlineAsm_LookupSymbolMIPS(struct HashNameNode* name, MLookupResult* result);
extern CInt64 InlineAsm_Int64Expression(void);

#ifdef __cplusplus
}
#endif

#endif
