#ifndef PSP_LOOP301_H
#define PSP_LOOP301_H

// mwccpsp 3.0.1: the MIPS back end's loop tree (0x54A9E0..0x54B9B0). Every
// node is lalloc'd and 0x70 bytes in the original.

#include "psp/BE.h"

typedef struct BlockList301 {
    struct BlockList301* next;
    MPCodeBlock* block;
} BlockList301;

enum {
    LOOP301_LOOP = 0, // a natural loop
    LOOP301_BLOCK = 1, // a block directly in a loop (or the function)
    LOOP301_ROOT = 2 // the whole function
};

typedef struct Loop301 {
    UInt8 kind; // +0x00
    UInt8 flags; // +0x01: 1 no exits, 2 innermost (no children)
    SInt16 id; // +0x02
    struct Loop301* parent; // +0x04
    struct Loop301* children; // +0x08
    struct Loop301* next; // +0x0C: next sibling
    MPCodeBlock* header; // +0x10: the loop header, or the block
    SInt32 nblocks; // +0x14
    BlockList301* blocks; // +0x18
    UInt32* membervec; // +0x1C: by block number
    SInt32 nexits; // +0x20
    BlockList301* exits; // +0x24: member blocks with a successor outside
    UInt32* exitvec; // +0x28
    UInt8 unk2C[0x24];
    MPCodeBlock* preheader; // +0x50
    SInt32 pressure[7]; // +0x54: most registers live at once, per class
} Loop301;

typedef struct RegPressure301 {
    SInt32* perblock; // most live at once in each block, by block number
    SInt32 count; // live now (walking a block backward)
    SInt32 avail; // registers available in the class
} RegPressure301;

extern Loop301* data_006258BC; // the loop forest
extern RegPressure301 data_00624E04[7];

extern Loop301* func_0054AB90(
    Boolean blocks
); // finds loops (with blocks: plus LOOP301_BLOCK leaves and a root)
extern void func_0054AAB0(void); // register pressure of every loop
extern void func_0054AD10(void (*func)(Loop301*)); // every node, inner first
extern Boolean func_0054A9E0(MPCode* first, MPCode* last, MOperand* op);

#endif
