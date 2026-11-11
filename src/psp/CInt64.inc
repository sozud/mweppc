#include "compiler/CInt64.h"

// mwccpsp 3.0.1 CInt64 operations that work word by word (included by
// src/compiler/frontend/CInt64.c), written so they
// scale to wider integers (word 0 is the most significant). The rest of
// CInt64.c is 2.4.7's (src/compiler/frontend/CInt64.c).

#define CINT64_WORDS 2
#define W(x, i) (((UInt32*)&(x))[i])

void CInt64_SetLong(CInt64* pN, SInt32 n) {
    pN->hi = (n < 0) ? 0xFFFFFFFF : 0;
    pN->lo = n;
}

void CInt64_SetULong(CInt64* pN, UInt32 n) {
    pN->hi = 0;
    pN->lo = n;
}

Boolean CInt64_IsZero(CInt64* n) {
    int i;

    for (i = 0; i < CINT64_WORDS; i++) {
        if (W(*n, i)) {
            return 0;
        }
    }
    return 1;
}

Boolean CInt64_IsOne(CInt64* n) {
    int i;

    if (W(*n, CINT64_WORDS - 1) != 1) {
        return 0;
    }
    for (i = 0; i < CINT64_WORDS - 1; i++) {
        if (W(*n, i)) {
            return 0;
        }
    }
    return 1;
}

extern CInt64 cint64_negone;
extern CInt64 cint64_zero;

CInt64 CInt64_Or(CInt64 lhs, CInt64 rhs) {
    int i;

    for (i = 0; i < CINT64_WORDS; i++) {
        W(lhs, i) |= W(rhs, i);
    }
    return lhs;
}

CInt64 CInt64_Xor(CInt64 lhs, CInt64 rhs) {
    int i;

    for (i = 0; i < CINT64_WORDS; i++) {
        W(lhs, i) ^= W(rhs, i);
    }
    return lhs;
}

CInt64 CInt64_And(CInt64 lhs, CInt64 rhs) {
    int i;

    for (i = 0; i < CINT64_WORDS; i++) {
        W(lhs, i) &= W(rhs, i);
    }
    return lhs;
}

Boolean CInt64_NotEqual(CInt64 lhs, CInt64 rhs) {
    int i;

    for (i = 0; i < CINT64_WORDS; i++) {
        if (W(lhs, i) != W(rhs, i)) {
            return 1;
        }
    }
    return 0;
}

Boolean CInt64_Equal(CInt64 lhs, CInt64 rhs) {
    int i;

    for (i = 0; i < CINT64_WORDS; i++) {
        if (W(lhs, i) != W(rhs, i)) {
            return 0;
        }
    }
    return 1;
}

static int ucompare(CInt64 lhs, CInt64 rhs) {
    int i;

    for (i = 0; i < CINT64_WORDS; i++) {
        if (W(lhs, i) != W(rhs, i)) {
            return (W(lhs, i) < W(rhs, i)) ? -1 : 1;
        }
    }
    return 0;
}

// Signed order is unsigned order with the sign bits flipped.
static int compare(CInt64 lhs, CInt64 rhs) {
    W(lhs, 0) ^= 0x80000000;
    W(rhs, 0) ^= 0x80000000;
    return ucompare(lhs, rhs);
}

Boolean CInt64_GreaterEqualU(CInt64 lhs, CInt64 rhs) {
    return ucompare(lhs, rhs) >= 0;
}

Boolean CInt64_GreaterEqual(CInt64 lhs, CInt64 rhs) {
    return compare(lhs, rhs) >= 0;
}

Boolean CInt64_LessEqualU(CInt64 lhs, CInt64 rhs) {
    return ucompare(lhs, rhs) <= 0;
}

Boolean CInt64_LessEqual(CInt64 lhs, CInt64 rhs) {
    return compare(lhs, rhs) <= 0;
}

Boolean CInt64_GreaterU(CInt64 lhs, CInt64 rhs) {
    return ucompare(lhs, rhs) > 0;
}

Boolean CInt64_Greater(CInt64 lhs, CInt64 rhs) {
    return compare(lhs, rhs) > 0;
}

Boolean CInt64_LessU(CInt64 lhs, CInt64 rhs) {
    return ucompare(lhs, rhs) < 0;
}

Boolean CInt64_Less(CInt64 lhs, CInt64 rhs) {
    return compare(lhs, rhs) < 0;
}

CInt64 CInt64_ShrU(CInt64 lhs, CInt64 rhs) {
    int i;
    UInt32 n;

    for (i = 0; i < CINT64_WORDS - 1; i++) {
        if (W(rhs, i)) {
            return cint64_zero;
        }
    }
    n = W(rhs, CINT64_WORDS - 1);
    if (n >= CINT64_WORDS * 32) {
        return cint64_zero;
    }
    for (; (int)n > 0; n--) {
        W(lhs, 1) >>= 1;
        if (W(lhs, 0) & 1) {
            W(lhs, 1) |= 0x80000000;
        }
        W(lhs, 0) >>= 1;
    }
    return lhs;
}

CInt64 CInt64_Shr(CInt64 lhs, CInt64 rhs) {
    int i;
    UInt32 n;

    for (i = 0; i < CINT64_WORDS - 1; i++) {
        if (W(rhs, i)) {
            goto toobig;
        }
    }
    n = W(rhs, CINT64_WORDS - 1);
    if (n >= CINT64_WORDS * 32) {
        goto toobig;
    }
    for (; (int)n > 0; n--) {
        W(lhs, 1) >>= 1;
        if (W(lhs, 0) & 1) {
            W(lhs, 1) |= 0x80000000;
        }
        W(lhs, 0) = (SInt32)W(lhs, 0) >> 1;
    }
    return lhs;
toobig:
    return (W(lhs, 0) & 0x80000000) ? cint64_negone : cint64_zero;
}

CInt64 CInt64_Shl(CInt64 lhs, CInt64 rhs) {
    int i;
    UInt32 n;

    for (i = 0; i < CINT64_WORDS - 1; i++) {
        if (W(rhs, i)) {
            return cint64_zero;
        }
    }
    n = W(rhs, CINT64_WORDS - 1);
    if (n >= CINT64_WORDS * 32) {
        return cint64_zero;
    }
    for (; (int)n > 0; n--) {
        W(lhs, 0) <<= 1;
        if (W(lhs, 1) & 0x80000000) {
            W(lhs, 0) |= 1;
        }
        W(lhs, 1) <<= 1;
    }
    return lhs;
}

// lhs + rhs + carry, word by word from the least significant.
#define ADD_WORDS(lhs, rhs, carry) \
    do { \
        int _i; \
        UInt32 _a; \
        UInt32 _b; \
        for (_i = CINT64_WORDS - 1; _i >= 0; _i--) { \
            _a = W(lhs, _i); \
            _b = W(rhs, _i); \
            if (_a & 0x80000000) { \
                if (_b & 0x80000000) { \
                    _a += carry + _b; \
                    carry = 1; \
                } else { \
                    goto _mixed; \
                } \
            } else if (_b & 0x80000000) { \
            _mixed: \
                _a += _b; \
                if (carry) { \
                    carry = 0; \
                    if (!(_a & 0x80000000)) { \
                        carry++; \
                    } \
                    _a++; \
                    if (!(_a & 0x80000000)) { \
                        carry = 1; \
                    } \
                } else if (!(_a & 0x80000000)) { \
                    carry = 1; \
                } else { \
                    carry = 0; \
                } \
            } else { \
                _a += carry + _b; \
                carry = 0; \
            } \
            W(lhs, _i) = _a; \
        } \
    } while (0)

extern CInt64 cint64_one;

CInt64 CInt64_Not(CInt64 input) {
    CInt64 output;
    int i;
    Boolean zero;

    for (i = 0; i < CINT64_WORDS; i++) {
        if (W(input, i)) {
            zero = 0;
            goto done;
        }
    }
    zero = 1;
done:
    W(output, 1) = zero;
    W(output, 0) = ((SInt32)zero < 0) ? -1 : 0;
    return output;
}

CInt64 CInt64_Inv(CInt64 input) {
    W(input, 0) = ~W(input, 0);
    W(input, 1) = ~W(input, 1);
    return input;
}

static inline CInt64 CInt64_NegInline(CInt64 input) {
    int carry = 0;

    W(input, 0) = ~W(input, 0);
    W(input, 1) = ~W(input, 1);
    ADD_WORDS(input, cint64_one, carry);
    return input;
}

CInt64 CInt64_Neg(CInt64 input) {
    int carry = 0;

    W(input, 0) = ~W(input, 0);
    W(input, 1) = ~W(input, 1);
    ADD_WORDS(input, cint64_one, carry);
    return input;
}

CInt64 CInt64_Sub(CInt64 lhs, CInt64 rhs) {
    int carry;

    W(rhs, 0) = ~W(rhs, 0);
    W(rhs, 1) = ~W(rhs, 1);
    carry = 1;
    ADD_WORDS(lhs, rhs, carry);
    return lhs;
}

// CInt64_Add as 3.0.1 inlines it into the scanners
static inline CInt64 CInt64_AddInline(CInt64 lhs, CInt64 rhs) {
    int carry = 0;

    ADD_WORDS(lhs, rhs, carry);
    return lhs;
}

// shift *pN left by n bits, one at a time (inlined in the scanners)
#define SHL_IN_PLACE(pN, n) \
    do { \
        int _k; \
        for (_k = 0; _k < (n); _k++) { \
            W(*(pN), 0) <<= 1; \
            if (W(*(pN), 1) & 0x80000000) { \
                W(*(pN), 0) |= 1; \
            } \
            W(*(pN), 1) <<= 1; \
        } \
    } while (0)

CInt64 CInt64_Add(CInt64 lhs, CInt64 rhs) {
    int carry = 0;

    ADD_WORDS(lhs, rhs, carry);
    return lhs;
}

CInt64 CInt64_MulU(CInt64 lhs, CInt64 rhs) {
    CInt64 result;
    CInt64 work;
    int i;
    int j;
    UInt32 bits;
    SInt32 product;
    int carry;

    for (i = 0; i < CINT64_WORDS - 1; i++) {
        if (W(lhs, i) || W(rhs, i)) {
            break;
        }
    }
    if (i >= CINT64_WORDS - 1 && !((W(lhs, 1) | W(rhs, 1)) & 0xFFFF0000)) {
        // both fit in 16 bits
        product = W(lhs, 1) * W(rhs, 1);
        W(result, 1) = product;
        W(result, 0) = (product < 0) ? -1 : 0;
        return result;
    }

    W(result, 0) = 0;
    W(result, 1) = 0;
    for (i = 0; i < CINT64_WORDS; i++) {
        bits = W(rhs, CINT64_WORDS - 1 - i);
        if (!bits) {
            continue;
        }
        for (j = 0; j < CINT64_WORDS; j++) {
            W(work, j) = (j + i < CINT64_WORDS) ? W(lhs, j + i) : 0;
        }
        for (;;) {
            if (bits & 1) {
                carry = 0;
                ADD_WORDS(result, work, carry);
            }
            bits >>= 1;
            if (!bits) {
                break;
            }
            W(work, 0) <<= 1;
            if (W(work, 1) & 0x80000000) {
                W(work, 0) |= 1;
            }
            W(work, 1) <<= 1;
        }
    }
    return result;
}

CInt64 CInt64_Mul(CInt64 lhs, CInt64 rhs) {
    if (W(rhs, 0) & 0x80000000) {
        if (W(lhs, 0) & 0x80000000) {
            return CInt64_MulU(CInt64_Neg(lhs), CInt64_Neg(rhs));
        }
        return CInt64_Neg(CInt64_MulU(lhs, CInt64_Neg(rhs)));
    }
    if (W(lhs, 0) & 0x80000000) {
        return CInt64_Neg(CInt64_MulU(CInt64_Neg(lhs), rhs));
    }
    return CInt64_MulU(lhs, rhs);
}

// Does the value fit in len bytes (as an unsigned / a signed integer)?
Boolean CInt64_IsInURange(CInt64 value, short len) {
    int n;
    int i;

    if (len >= CINT64_WORDS * 4) {
        return 1;
    }
    n = CINT64_WORDS - 1 - ((len - 1) >> 2);
    for (i = 0; i < n; i++) {
        if (W(value, i)) {
            return 0;
        }
    }
    switch (len & 3) {
        case 0:
            return 1;
        case 1:
            return !(W(value, i) & 0xFFFFFF00);
        case 2:
            return !(W(value, i) & 0xFFFF0000);
        case 3:
            return !(W(value, i) & 0xFF000000);
    }
}

Boolean CInt64_IsInRange(CInt64 value, short len) {
    int n;
    int i;

    if (len >= CINT64_WORDS * 4) {
        return 1;
    }
    if (W(value, 0) & 0x80000000) {
        W(value, 0) = ~W(value, 0);
        W(value, 1) = ~W(value, 1);
    }
    n = CINT64_WORDS - 1 - ((len - 1) >> 2);
    for (i = 0; i < n; i++) {
        if (W(value, i)) {
            return 0;
        }
    }
    switch (len & 3) {
        case 0:
            return !(W(value, i) & 0x80000000);
        case 1:
            return !(W(value, i) & 0xFFFFFF80);
        case 2:
            return !(W(value, i) & 0xFFFF8000);
        case 3:
            return !(W(value, i) & 0xFF800000);
    }
}

extern const double data_005CBC10;  // 1.0

double CInt64_ConvertUToLongDouble(const CInt64* value) {
    double result = 0.0;
    UInt32 work;
    int i;
    int j;

    if (!CInt64_IsZero((CInt64*)value)) {
        for (i = 0; i < CINT64_WORDS; i++) {
            work = W(*value, i);
            for (j = 0; j < 32; j++) {
                result += result;
                if (work & 0x80000000) {
                    result += data_005CBC10;
                }
                work <<= 1;
            }
        }
    }
    return result;
}

static inline CInt64 negate(CInt64 input) {
    int carry = 0;

    W(input, 0) = ~W(input, 0);
    W(input, 1) = ~W(input, 1);
    ADD_WORDS(input, cint64_one, carry);
    return input;
}

double CInt64_ConvertToLongDouble(const CInt64* value) {
    CInt64 magnitude;

    if (W(*value, 0) & 0x80000000) {
        magnitude = negate(*value);
        return -CInt64_ConvertUToLongDouble(&magnitude);
    } else {
        return CInt64_ConvertUToLongDouble(value);
    }
}
