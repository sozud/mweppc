#include "compiler/BitVector.h"
#include "compiler/CError.h"
#include "compiler/CompilerTools.h"
#if MWCCPSP
#include "compiler/IroMalloc.h"
#endif
#include <string.h>
#if MWCCPSP
#line 6 "IrObitvect.c" // 3.0.1 asserts name this file
#endif

#if MWCCPSP
void Bv_AllocVector(BitVector** bv, UInt32 size) {
    size = (size / 32) + 1;
    *bv = IRO_malloc(sizeof(BitVector) + sizeof(UInt32) * size);
    (*bv)->size = size;
    Bv_Clear(*bv);
}
#else
void Bv_AllocVector(BitVector** bv, UInt32 size) {
    size = (size / 32) + 1;
    *bv = oalloc(sizeof(BitVector) + sizeof(UInt32) * size);
    (*bv)->size = size;
    Bv_Clear(*bv);
}
#endif

void Bv_AllocVectorLocal(BitVector** bv, UInt32 size) {
    UInt32 long_size = (size / 32) + 1;
    *bv = lalloc(sizeof(BitVector) + sizeof(UInt32) * long_size);
    (*bv)->size = long_size;
    Bv_Clear(*bv);
}

#if MWCCPSP
void Bv_ClearBit(UInt32 bit, BitVector* bv) {
    if ((bit / 32) < bv->size) {
        bv->data[bit / 32] &= ~(1 << (bit & 31));
    } else {
        CError_FATAL(56);
    }
}
#else
void Bv_ClearBit(UInt32 bit, BitVector* bv) {
    if ((bit / 32) < bv->size) {
        bv->data[bit / 32] &= ~(1 << (bit & 31));
    } else {
        CError_FATAL(73);
    }
}
#endif

#if MWCCPSP
// 3.0.1: the shorter vector decides how far the operations go
void Bv_And(const BitVector* a, BitVector* b) {
    UInt32 i, len;

    len = a->size;
    if (b->size < len) {
        len = b->size;
    }
    for (i = 0; i < len; i++) {
        b->data[i] &= a->data[i];
    }
}
#else
void Bv_And(const BitVector* a, BitVector* b) {
    UInt32 i;
    for (i = 0; i < b->size; i++) {
        b->data[i] &= a->data[i];
    }
}
#endif

void Bv_Or(const BitVector* a, BitVector* b) {
    UInt32 i, len;

    len = a->size;
    if (b->size < len) {
        len = b->size;
    }

    for (i = 0; i < len; i++) {
        b->data[i] |= a->data[i];
    }
}

#if !MWCCPS2 // b210 inlines it (BitVector.h)
Boolean Bv_BitsInCommon(const BitVector* a, const BitVector* b) {
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
#endif

#if MWCCPSP
// 3.0.1: the longer vector's extra words must be zero
Boolean Bv_Compare(const BitVector* a, const BitVector* b) {
    UInt32 i, len, alen, blen;

    blen = b->size;
    alen = a->size;
    len = blen;
    if (alen < blen) {
        len = alen;
    }
    for (i = 0; i < len; i++) {
        if (a->data[i] != b->data[i]) {
            return FALSE;
        }
    }
    if (alen < blen) {
        for (; alen < blen; alen++) {
            if (b->data[alen]) {
                return FALSE;
            }
        }
    } else if (blen < alen) {
        for (; blen < alen; blen++) {
            if (a->data[blen]) {
                return FALSE;
            }
        }
    }
    return TRUE;
}
#else
Boolean Bv_Compare(const BitVector* a, const BitVector* b) {
    UInt32 i;
    for (i = 0; i < a->size; i++) {
        if (a->data[i] != b->data[i]) {
            return FALSE;
        }
    }

    return TRUE;
}
#endif

#if MWCCPSP
void Bv_Minus(const BitVector* a, BitVector* b) {
    UInt32 len, i;

    len = a->size;
    if (b->size < len) {
        len = b->size;
    }
    for (i = 0; i < len; i++) {
        b->data[i] &= ~a->data[i];
    }
}
#else
void Bv_Minus(const BitVector* a, BitVector* b) {
    UInt32 i;
    for (i = 0; i < b->size; i++) {
        b->data[i] &= ~a->data[i];
    }
}
#endif

#if MWCCPSP
// 3.0.1: a shorter source leaves the rest of dst cleared
void Bv_Copy(const BitVector* src, BitVector* dst) {
    if (dst->size <= src->size) {
        memcpy(dst->data, src->data, sizeof(UInt32) * dst->size);
    } else {
        memcpy(dst->data, src->data, sizeof(UInt32) * src->size);
        memset(dst->data + src->size, 0, sizeof(UInt32) * (dst->size - src->size));
    }
}
#else
void Bv_Copy(const BitVector* src, BitVector* dst) {
    memcpy(dst->data, src->data, sizeof(UInt32) * dst->size);
}
#endif

void Bv_Clear(BitVector* bv) {
    memset(bv->data, 0, sizeof(UInt32) * bv->size);
}

void Bv_Set(BitVector* bv) {
    memset(bv->data, 0xFF, sizeof(UInt32) * bv->size);
}

Boolean Bv_IsSubset(const BitVector* a, const BitVector* b) {
    int i;

    for (i = 0; (UInt32)i < a->size; i++) {
        if (b->size < (UInt32)i) {
            if (a->data[i]) {
                return FALSE;
            }
        } else {
            if (a->data[i] & ~(b->data[i])) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

Boolean Bv_IsEmpty(const BitVector* bv) {
    UInt32 i;

    for (i = 0; i < bv->size; i++) {
        if (bv->data[i]) {
            return FALSE;
        }
    }

    return TRUE;
}
#if MWCCPSP

#endif
#if MWCCPSP
// 0x57D1F0: 3.0.1 has Bv_IsBitSet as a function too (the Iro passes call it)
int Bv_IsBitSet301(UInt32 bit, BitVector* bv) {
    int result = 0;

    if ((bit >> 5) < bv->size && (bv->data[bit >> 5] & (1 << (bit & 31)))) {
        result = 1;
    }
    return result;
}
#endif
