#ifndef PSP_BE_H
#define PSP_BE_H

#include "compiler/common.h"
#include <stddef.h>
#include "psp/ObjGen.h"

// MIPS backend records of mwccpsp 3.0.1, modelled as far as the decompiled
// code reads them.

// The register classes. 3.0.1 loops over them with a variable of this
// minimal enum type, which is a signed byte because of the -1 member; that
// is why such loops keep their entry test.
typedef enum RegClass301 {
    RC301_NONE = -1,
    RC301_GPR,
    RC301_FPR,
    RC301_HILO,
    RC301_CLASS3,
    RC301_CLASS4,
    RC301_CLASS5,
    RC301_CLASS6,
    RC301_MAX
} RegClass301;

// 1 << n as x86 computes it (the count taken mod 32; register masks shift by
// VFPU register numbers up to 127)
#if MWCCPSP_NATIVE
#define BIT32(n) (1 << ((n) & 31))
#define SHR32(x, n) ((x) >> ((n) & 31))
#else
#define BIT32(n) (1 << (n))
#define SHR32(x, n) ((x) >> (n))
#endif

// An integer field that also holds pointers: 32 bits in 3.0.1, pointer-sized
// natively (the matching build's code is the same either way)
#if MWCCPSP_NATIVE
typedef long PtrInt;
#else
typedef SInt32 PtrInt;
#endif

// An instruction operand. A constant is a CInt64 {hi, lo} in value/value2.
// 3.0.1 also keeps pointers in them: a label in value (kind 6), the object of
// a memory operand or the peephole's defining instruction in value2. The
// native build gives the pointers their own fields (value/value2 keep a
// truncated copy, so tests and comparisons behave as before); use the MOP_
// macros, which are the old accesses in the matching build.
typedef struct MOperand {
    UInt8 kind; // 0 register, 2 immediate, 4, 6 label
    char rclass; // register operands: class (signed in 3.0.1)
    UInt16 flags; // 0x4000: second register of a pair; 0x100/0x200/0x400 select a variant
    union {
        SInt32 value; // immediate (a constant's high word)
        SInt16 reg; // register number
#if !MWCCPSP_NATIVE
        void* ptr; // kind 6: the label (MPCodeLabel*)
#endif
    };
    SInt32 value2; // a constant's low word; memory operands: the object
    SInt32 unk0C;
#if MWCCPSP_NATIVE
    void* ptr; // kind 6: the label
    void* obj; // the object (kind 4) or defining instruction
#endif
} MOperand; // 0x10

#if MWCCPSP_NATIVE
#define MOP_OBJ(op) ((op).obj)
#define MOP_SETOBJ(op, o) ((op).obj = (void*)(o), (op).value2 = (SInt32)(long)(o))
#define MOP_SETLABEL(op, l) ((op).ptr = (void*)(l), (op).value = (SInt32)(long)(l))
#else
#define MOP_OBJ(op) ((void*)(op).value2)
#define MOP_SETOBJ(op, o) ((op).value2 = (SInt32)(o))
#define MOP_SETLABEL(op, l) ((op).ptr = (void*)(l))
#endif

// A value for the code generator: a register, a memory location or a
// constant (VReg.c; 2.4.7's Operand).
typedef struct VOperand {
    UInt8 kind; // 0 integer register, 1 FP register, 2/4 memory, 3 constant, 5 coprocessor 2
    UInt8 pad01;
    UInt16 reg; // +0x02
    UInt16 regHi; // +0x04: second register of a pair
    UInt8 pad06[2];
    SInt32 offset; // +0x08
    CInt64 imm; // +0x0C
    PtrInt object; // +0x14: Object*
    PtrInt unk18; // +0x18: bitfield type (TypeBitfield*)
    UInt32 flags; // +0x1C: 0x80 must be loaded
    UInt8 pad20[4];
} VOperand; // 0x24

typedef struct MPCode {
    struct MPCode* next;
    struct MPCode* prev; // +0x04
    struct MPCodeBlock* block; // +0x08
    UInt32 flags; // +0x0C; 0x4000000: target is one instruction further;
    // 0x100000: alignment directive; 0x40000000: emit last in block
    SInt32 useID; // +0x10: the first of its uses (UseDefChains)
    SInt32 defID; // +0x14: the first of its definitions
    struct Alias* alias; // +0x18: memory operand's alias
    SInt32 offset; // +0x1C: byte offset in the function
    SInt32 unk20; // +0x20: copied from the neighbour on insertion
    SInt32 unk24; // +0x24: likewise
    UInt16 op; // +0x28
    SInt16 argcount; // +0x2A
    UInt8 unk2C[4];
    UInt32 word; // +0x30: the encoded instruction
    struct Object* relobj; // +0x34: object a relocation refers to
    SInt32 unk38; // +0x38: relocation kind (1, 5 hi, 6 lo, 7/8 $gp, ...)
    struct MPCode* pair; // +0x3C: the lui of a hi/lo pair
    MOperand args[1]; // +0x40
} MPCode;

// bytes for an instruction with n operands (0x40 + n * 0x10 in 3.0.1)
#define PCODE_SIZE(n) (offsetof(MPCode, args) + (n) * sizeof(MOperand))

// A control-flow edge; edges are also chained in a hash table on
// (from, to) (PCode.c).
typedef struct MPBlockLink {
    struct MPBlockLink* hashnext; // +0x00
    struct MPBlockLink* next; // +0x04: next successor edge of `from`
    struct MPBlockLink* nextpred; // +0x08: next predecessor edge of `block`
    struct MPCodeBlock* from; // +0x0C
    struct MPCodeBlock* block; // +0x10: the destination
    SInt32 index; // +0x14
    UInt16 hash; // +0x18
    UInt16 flags; // +0x1A
} MPBlockLink; // 0x1C

typedef struct MPCodeLabel {
    struct MPCodeLabel* next; // +0x00: next label of the block
    struct MPCodeBlock* block; // +0x04: or, unresolved, the pending branches
    UInt16 resolved; // +0x08
    UInt16 index; // +0x0A
} MPCodeLabel; // 0x0C

// A branch to a label that has no block yet.
typedef struct MPPendingBranch {
    struct MPPendingBranch* next;
    struct MPCodeBlock* from;
    UInt16 flags;
} MPPendingBranch; // 0x0C

typedef struct MPCodeBlock {
    struct MPCodeBlock* next;
    struct MPCodeBlock* prev; // +0x04
    MPCodeLabel* labels; // +0x08
    MPBlockLink* predecessors; // +0x0C
    MPBlockLink* successors; // +0x10
    MPCode* first; // +0x14
    MPCode* last; // +0x18
    SInt32 number; // +0x1C
    SInt32 unk20; // +0x20: -1 when made
    SInt32 codeoffset; // +0x24
    SInt32 weight; // +0x28: loop-nesting weight
    SInt16 pcodecount; // +0x2C
    UInt16 flags2E; // +0x2E: 8 cleared whenever the code changes
} MPCodeBlock;

extern MPCodeBlock* data_00624FD0; // pcbasicblocks: blocks of the function being emitted
extern MPCodeBlock* data_006246B4; // pclastblock
extern SInt32 data_00624D40; // pcblockcount
extern SInt32 data_0062478C; // pcloopweight
extern MPCodeBlock** data_00625418; // depthfirstordering

// PCode.c
extern MPCode* makepcode(VarArgShort op, ...);
extern void emitpcode(VarArgShort op, ...);
extern MPCode* copypcode(MPCode* pc);
extern MPCodeLabel* makepclabel(void);
extern MPCodeBlock* makepcblock(void);
extern void pclabel(MPCodeBlock* block, MPCodeLabel* label);
extern void pcbranch(MPCodeBlock* block, MPCodeLabel* label, UInt16 flags);
extern void deleteblock(MPCodeBlock* block);
extern void deleteunreachableblocks(void);
extern void appendpcode(MPCodeBlock* block, MPCode* pc);
extern void deletepcode(MPCode* pc);
extern void insertpcodebefore(MPCode* anchor, MPCode* pc);
extern void insertpcodeafter(MPCode* anchor, MPCode* pc);
extern void setpcodeflags(UInt32 flags);
extern void computedepthfirstordering(void);
extern MPBlockLink* linkblocks(MPCodeBlock* from, MPCodeBlock* to);
extern void unlinkblocks(MPCodeBlock* from, MPCodeBlock* to);

typedef struct BlockLiveness {
    UInt32* use; // read before written in the block
    UInt32* def; // written in the block
    UInt32* livein;
    UInt32* liveout; // +0x0C
} BlockLiveness; // 0x10

typedef struct ClassLiveness {
    SInt32 n; // registers in the class
    BlockLiveness* blocks; // by MPCodeBlock.number
    UInt32* current; // scratch: live set while walking a block
} ClassLiveness; // 0x0C

extern ClassLiveness data_0062541C[7];

#endif
