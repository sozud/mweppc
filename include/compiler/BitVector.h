#include "compiler/mwcc_core.h"
#ifndef COMPILER_BITVECTOR_H
#define COMPILER_BITVECTOR_H

#include "compiler/common.h"
#include "compiler/CError.h"
#if MWCC_CORE == MWCC_CORE_B52
#pragma pack(push, 2)
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct BitVector {
    UInt32 size;
    UInt32 data[0];
};

extern void Bv_AllocVector(BitVector** bv, UInt32 size);
extern void Bv_ClearBit(UInt32 bit, BitVector* bv);
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
extern void Bv_And(const BitVector* a, BitVector* b);
extern void Bv_Or(const BitVector* a, BitVector* b);
extern Boolean Bv_BitsInCommon(const BitVector* a, const BitVector* b);
#else
extern void Bv_And(const BitVector* a, BitVector* b);
extern void Bv_Or(const BitVector* a, BitVector* b);
#if MWCCPS2
// mwccps2 b210 has no Bv_BitsInCommon: every caller inlines it (IroCSE
// 0x575520 +0xE8, the loop-invariant and propagation passes)
HEADER_INLINE Boolean Bv_BitsInCommon_ps2(const BitVector* a, const BitVector* b) {
    UInt32 len;
    UInt32 i;

    len = a->size;
    if (b->size < len) {
        len = b->size;
    }

    for (i = 0; i < len; i++) {
        if (a->data[i] & b->data[i]) {
            return TRUE;
        }
    }

    return FALSE;
}
#define Bv_BitsInCommon Bv_BitsInCommon_ps2
#else
extern Boolean Bv_BitsInCommon(const BitVector* a, const BitVector* b);
#endif
#endif
extern Boolean Bv_Compare(const BitVector* a, const BitVector* b);
extern void Bv_Minus(const BitVector* a, BitVector* b);
extern void Bv_Copy(const BitVector* src, BitVector* dst);
extern void Bv_Clear(BitVector* bv);
extern void Bv_Set(BitVector* bv);
extern Boolean Bv_IsSubset(const BitVector* a, const BitVector* b);
extern Boolean Bv_IsEmpty(const BitVector* bv);

HEADER_INLINE void Bv_SetBit(UInt32 bit, BitVector* bv) {
    if ((bit / 32) < bv->size) {
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        bv->data[bit / 32] |= 1 << (bit & 31);
    } else {
#if MWCC_CORE == MWCC_CORE_B52
        CError_FATAL(80); // 3.0.1: "BitVector.h", 0x50
#else
        CError_FATAL(56);
#endif
#else
        bv->data[bit / 32] |= 1 << (bit & 31);
    } else {
#if MWCCPSP
        CError_FATAL(80); // 3.0.1: "BitVector.h", 0x50
#else
        CError_FATAL(56);
#endif
#endif
    }
}

#define Bv_IsBitSet(_bit, _bv) \
    (((UInt32)((_bit) >> 5) < (_bv)->size) && ((_bv)->data[(_bit) >> 5] & (1 << ((_bit) & 31))))

#if MWCCPS2
// the PS2 3.0.1 header's Bv_IsBitSet: a function (an unsigned bit number, the result as a
// Boolean), which some of b210's and b198's passes use in place of the macro
static inline Boolean Bv_IsBitSetFn(UInt32 bit, BitVector* bv) {
    return ((UInt32)(bit >> 5) < bv->size) && (bv->data[bit >> 5] & (1 << (bit & 31)));
}
#endif

#ifdef __cplusplus
}
#endif

#if MWCC_CORE == MWCC_CORE_B52
#pragma pack(pop)
#endif
#endif
