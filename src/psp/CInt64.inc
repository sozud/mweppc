#include "compiler/CInt64.h"

// mwccpsp 3.0.1 CInt64 operations that work word by word (included by
// src/compiler/frontend/CInt64.c), written so they
// scale to wider integers (word 0 is the most significant). The rest of
// CInt64.c is 2.4.7's (src/compiler/frontend/CInt64.c).

#if MWCCPS2
#define CINT64_WORDS 4
#else
#define CINT64_WORDS 2
#endif
#define W(x, i) (((UInt32*)&(x))[i])

#if MWCCPS2
void CInt64_SetLong(CInt64* pN, SInt32 n) {
    UInt32 sign = (n < 0) ? 0xFFFFFFFF : 0;
    pN->hi = sign;
    pN->mid[0] = sign;
    pN->mid[1] = sign;
    pN->lo = n;
}

void CInt64_SetULong(CInt64* pN, UInt32 n) {
    pN->hi = 0;
    pN->mid[0] = 0;
    pN->mid[1] = 0;
    pN->lo = n;
}
#else
void CInt64_SetLong(CInt64* pN, SInt32 n) {
    pN->hi = (n < 0) ? 0xFFFFFFFF : 0;
    pN->lo = n;
}

void CInt64_SetULong(CInt64* pN, UInt32 n) {
    pN->hi = 0;
    pN->lo = n;
}
#endif

#if MWCCPS2
Boolean CInt64_IsZero(CInt64* n) {
    int i;
    UInt32* w = (UInt32*)n;

    for (i = 0; i < CINT64_WORDS; i++) {
        if (w[i]) {
            return 0;
        }
    }
    return 1;
}
#else
Boolean CInt64_IsZero(CInt64* n) {
    int i;

    for (i = 0; i < CINT64_WORDS; i++) {
        if (W(*n, i)) {
            return 0;
        }
    }
    return 1;
}
#endif

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

#if MWCCPS2
CInt64 CInt64_Or(CInt64 lhs, CInt64 rhs) {
    UInt32 a = rhs.lo;
    UInt32 b = rhs.mid[1];
    UInt32 c = rhs.mid[0];
    UInt32 d = rhs.hi;

    lhs.hi |= d;
    lhs.mid[0] |= c;
    lhs.mid[1] |= b;
    lhs.lo |= a;
    return lhs;
}

CInt64 CInt64_Xor(CInt64 lhs, CInt64 rhs) {
    UInt32 a = rhs.lo;
    UInt32 b = rhs.mid[1];
    UInt32 c = rhs.mid[0];
    UInt32 d = rhs.hi;

    lhs.hi ^= d;
    lhs.mid[0] ^= c;
    lhs.mid[1] ^= b;
    lhs.lo ^= a;
    return lhs;
}

CInt64 CInt64_And(CInt64 lhs, CInt64 rhs) {
    UInt32 a = rhs.lo;
    UInt32 b = rhs.mid[1];
    UInt32 c = rhs.mid[0];
    UInt32 d = rhs.hi;

    lhs.hi &= d;
    lhs.mid[0] &= c;
    lhs.mid[1] &= b;
    lhs.lo &= a;
    return lhs;
}

#else
CInt64 CInt64_Or(CInt64 lhs, CInt64 rhs) {
    UInt32 lo = rhs.lo;
    SInt32 hi = rhs.hi;

    lhs.hi |= hi;
    lhs.lo |= lo;
    return lhs;
}

CInt64 CInt64_Xor(CInt64 lhs, CInt64 rhs) {
    UInt32 lo = rhs.lo;
    SInt32 hi = rhs.hi;

    lhs.hi ^= hi;
    lhs.lo ^= lo;
    return lhs;
}

CInt64 CInt64_And(CInt64 lhs, CInt64 rhs) {
    UInt32 lo = rhs.lo;
    SInt32 hi = rhs.hi;

    lhs.hi &= hi;
    lhs.lo &= lo;
    return lhs;
}
#endif

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

#if MWCCPS2
// mwccps2 b210 compares through pointers: the inlined loops read the
// arguments in place and reload each word for the second test
static int ucompare(CInt64* lhs, CInt64* rhs) {
    int i;

    for (i = 0; i < CINT64_WORDS; i++) {
        if (((SInt32*)lhs)[i] != ((SInt32*)rhs)[i]) {
            return (W(*lhs, i) < W(*rhs, i)) ? -1 : 1;
        }
    }
    return 0;
}

static int compare(CInt64* lhs, CInt64* rhs) {
    CInt64 l = *lhs;
    CInt64 r;

    W(l, 0) ^= 0x80000000;
    r = *rhs;
    W(r, 0) ^= 0x80000000;
    return ucompare(&l, &r);
}

Boolean CInt64_GreaterEqualU(CInt64 lhs, CInt64 rhs) {
    return ucompare(&lhs, &rhs) >= 0;
}

Boolean CInt64_GreaterEqual(CInt64 lhs, CInt64 rhs) {
    return compare(&lhs, &rhs) >= 0;
}

Boolean CInt64_LessEqualU(CInt64 lhs, CInt64 rhs) {
    return ucompare(&lhs, &rhs) <= 0;
}

Boolean CInt64_LessEqual(CInt64 lhs, CInt64 rhs) {
    return compare(&lhs, &rhs) <= 0;
}

Boolean CInt64_GreaterU(CInt64 lhs, CInt64 rhs) {
    return ucompare(&lhs, &rhs) > 0;
}

Boolean CInt64_Greater(CInt64 lhs, CInt64 rhs) {
    return compare(&lhs, &rhs) > 0;
}

Boolean CInt64_LessU(CInt64 lhs, CInt64 rhs) {
    return ucompare(&lhs, &rhs) < 0;
}

Boolean CInt64_Less(CInt64 lhs, CInt64 rhs) {
    return compare(&lhs, &rhs) < 0;
}

#else
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

#endif

CInt64 CInt64_ShrU(CInt64 lhs, CInt64 rhs) {
    int i;
    UInt32 n;
#if MWCCPS2
    int count;
#endif

    for (i = 0; i < CINT64_WORDS - 1; i++) {
        if (W(rhs, i)) {
            return cint64_zero;
        }
    }
#if MWCCPS2
    n = W(rhs, i);
#else
    n = W(rhs, CINT64_WORDS - 1);
#endif
    if (n >= CINT64_WORDS * 32) {
        return cint64_zero;
    }
#if MWCCPS2
    for (count = n; count > 0; count--) {
#else
    for (; (int)n > 0; n--) {
#endif
#if MWCCPS2
        for (i = CINT64_WORDS - 1; i > 0; i--) {
            W(lhs, i) >>= 1;
            if (W(lhs, i - 1) & 1) {
                W(lhs, i) |= 0x80000000;
            }
        }
#else
        W(lhs, 1) >>= 1;
        if (W(lhs, 0) & 1) {
            W(lhs, 1) |= 0x80000000;
        }
#endif
        W(lhs, 0) >>= 1;
    }
    return lhs;
}

#if MWCCPS2
// b210 sends a too-large count back to the word test (the count word is
// nonzero, so it takes the too-big return)
CInt64 CInt64_Shr(CInt64 lhs, CInt64 rhs) {
    int i;
    UInt32 n;
    int count;

    for (i = 0; i < CINT64_WORDS - 1; i++) {
    check:
        if (W(rhs, i)) {
            return (W(lhs, 0) & 0x80000000) ? cint64_negone : cint64_zero;
        }
    }
    n = W(rhs, i);
    if (n >= CINT64_WORDS * 32) {
        goto check;
    }
    for (count = n; count > 0; count--) {
        for (i = CINT64_WORDS - 1; i > 0; i--) {
            W(lhs, i) >>= 1;
            if (W(lhs, i - 1) & 1) {
                W(lhs, i) |= 0x80000000;
            }
        }
        W(lhs, 0) = (SInt32)W(lhs, 0) >> 1;
    }
    return lhs;
}
#else
CInt64 CInt64_Shr(CInt64 lhs, CInt64 rhs) {
    int i;
    UInt32 n;
#if MWCCPS2
    int count;
#endif

    for (i = 0; i < CINT64_WORDS - 1; i++) {
        if (W(rhs, i)) {
            goto toobig;
        }
    }
#if MWCCPS2
    n = W(rhs, i);
#else
    n = W(rhs, CINT64_WORDS - 1);
#endif
    if (n >= CINT64_WORDS * 32) {
        goto toobig;
    }
#if MWCCPS2
    for (count = n; count > 0; count--) {
#else
    for (; (int)n > 0; n--) {
#endif
#if MWCCPS2
        for (i = CINT64_WORDS - 1; i > 0; i--) {
            W(lhs, i) >>= 1;
            if (W(lhs, i - 1) & 1) {
                W(lhs, i) |= 0x80000000;
            }
        }
#else
        W(lhs, 1) >>= 1;
        if (W(lhs, 0) & 1) {
            W(lhs, 1) |= 0x80000000;
        }
#endif
        W(lhs, 0) = (SInt32)W(lhs, 0) >> 1;
    }
    return lhs;
toobig:
    return (W(lhs, 0) & 0x80000000) ? cint64_negone : cint64_zero;
}
#endif

CInt64 CInt64_Shl(CInt64 lhs, CInt64 rhs) {
    int i;
    UInt32 n;
#if MWCCPS2
    int count;
    int j;
#endif

    for (i = 0; i < CINT64_WORDS - 1; i++) {
        if (W(rhs, i)) {
            return cint64_zero;
        }
    }
#if MWCCPS2
    n = W(rhs, i);
#else
    n = W(rhs, CINT64_WORDS - 1);
#endif
    if (n >= CINT64_WORDS * 32) {
        return cint64_zero;
    }
#if MWCCPS2
    for (count = n; count > 0; count--) {
#else
    for (; (int)n > 0; n--) {
#endif
#if MWCCPS2
        for (j = 0; j < CINT64_WORDS - 1; j++) {
            W(lhs, j) <<= 1;
            if (W(lhs, j + 1) & 0x80000000) {
                W(lhs, j) |= 1;
            }
        }
        W(lhs, CINT64_WORDS - 1) <<= 1;
#else
        W(lhs, 0) <<= 1;
        if (W(lhs, 1) & 0x80000000) {
            W(lhs, 0) |= 1;
        }
        W(lhs, 1) <<= 1;
#endif
    }
    return lhs;
}

#if MWCCPS2
// lhs + rhs + carry, word by word from the least significant.
#define ADD_WORDS(lhs, rhs, carry) \
    do { \
        UInt32 _a; \
        int _i; \
        UInt32 _b; \
        for (_i = CINT64_WORDS - 1; _i >= 0; _i--) { \
            _a = W(lhs, _i); \
            _b = W(rhs, _i); \
            if (_a & 0x80000000) { \
                if (_b & 0x80000000) { \
                    _a += _b + carry; \
                    carry = 1; \
                } else { \
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
                    } else { \
                        carry = !(_a & 0x80000000) ? 1 : 0; \
                    } \
                } \
            } else if (_b & 0x80000000) { \
                goto _mixed; \
            } else { \
                _a += _b + carry; \
                carry = 0; \
            } \
            W(lhs, _i) = _a; \
        } \
    } while (0)
#else
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
#endif

extern CInt64 cint64_one;

#if MWCCPS2
static inline Boolean iszero(CInt64* n) {
    int i;

    for (i = 0; i < CINT64_WORDS; i++) {
        if (W(*n, i)) {
            return 0;
        }
    }
    return 1;
}

CInt64 CInt64_Not(CInt64 input) {
    CInt64 output;

    CInt64_SetLong(&output, iszero(&input));
    return output;
}
#else
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
#endif

#if MWCCPS2
#define INV_WORDS(x) do { W(x, 0) = ~W(x, 0); W(x, 1) = ~W(x, 1); W(x, 2) = ~W(x, 2); W(x, 3) = ~W(x, 3); } while (0)
#else
#define INV_WORDS(x) do { W(x, 0) = ~W(x, 0); W(x, 1) = ~W(x, 1); } while (0)
#endif

CInt64 CInt64_Inv(CInt64 input) {
    INV_WORDS(input);
    return input;
}

static inline CInt64 CInt64_NegInline(CInt64 input) {
#if MWCCPS2
    int carry;

    INV_WORDS(input);
    carry = 0;
#else
    int carry = 0;

    INV_WORDS(input);
#endif
    ADD_WORDS(input, cint64_one, carry);
    return input;
}

CInt64 CInt64_Neg(CInt64 input) {
#if MWCCPS2
    int carry;

    INV_WORDS(input);
    carry = 0;
#else
    int carry = 0;

    INV_WORDS(input);
#endif
    ADD_WORDS(input, cint64_one, carry);
    return input;
}

CInt64 CInt64_Sub(CInt64 lhs, CInt64 rhs) {
    int carry;

    INV_WORDS(rhs);
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
#if MWCCPS2
#define SHL_IN_PLACE(pN, n) \
    do { \
        int _k; \
        int _j; \
        for (_k = 0; _k < (n); _k++) { \
            for (_j = 0; _j < CINT64_WORDS - 1; _j++) { \
                W(*(pN), _j) <<= 1; \
                if (W(*(pN), _j + 1) & 0x80000000) { \
                    W(*(pN), _j) |= 1; \
                } \
            } \
            W(*(pN), CINT64_WORDS - 1) <<= 1; \
        } \
    } while (0)
#define ZERO_RESULT(pN) \
    do { \
        (pN)->hi = 0; \
        (pN)->mid[0] = 0; \
        (pN)->mid[1] = 0; \
        (pN)->lo = 0; \
    } while (0)
#else
#define ZERO_RESULT(pN) \
    do { \
        (pN)->hi = 0; \
        (pN)->lo = 0; \
    } while (0)
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
#endif

CInt64 CInt64_Add(CInt64 lhs, CInt64 rhs) {
    int carry = 0;

    ADD_WORDS(lhs, rhs, carry);
    return lhs;
}

#if MWCCPS2
CInt64 CInt64_MulU(CInt64 lhs, CInt64 rhs) {
    CInt64 result;
    CInt64 work;
    int i;
    int j;
    UInt32 bits;
    int carry;

    for (i = 0; i < CINT64_WORDS - 1; i++) {
        if (W(lhs, i) || W(rhs, i)) {
            break;
        }
    }
    if (i >= CINT64_WORDS - 1 && !((W(lhs, CINT64_WORDS - 1) | W(rhs, CINT64_WORDS - 1)) & 0xFFFF0000)) {
        // both fit in 16 bits
        CInt64_SetLong(&result, W(lhs, CINT64_WORDS - 1) * W(rhs, CINT64_WORDS - 1));
        return result;
    }

    result.hi = 0;
    result.mid[0] = 0;
    result.mid[1] = 0;
    result.lo = 0;
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
            for (j = 0; j < CINT64_WORDS - 1; j++) {
                W(work, j) <<= 1;
                if (W(work, j + 1) & 0x80000000) {
                    W(work, j) |= 1;
                }
            }
            W(work, CINT64_WORDS - 1) <<= 1;
        }
    }
    return result;
}

CInt64 CInt64_Mul(CInt64 lhs, CInt64 rhs) {
    if (W(rhs, 0) & 0x80000000) {
        if (W(lhs, 0) & 0x80000000) {
            return CInt64_MulU(CInt64_NegInline(lhs), CInt64_NegInline(rhs));
        }
    } else if (W(lhs, 0) & 0x80000000) {
        return CInt64_NegInline(CInt64_MulU(CInt64_NegInline(lhs), rhs));
    } else {
        return CInt64_MulU(lhs, rhs);
    }
    return CInt64_NegInline(CInt64_MulU(lhs, CInt64_NegInline(rhs)));
}

#else
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
#endif

// Does the value fit in len bytes (as an unsigned / a signed integer)?
#if MWCCPS2
Boolean CInt64_IsInURange(CInt64 value, short len) {
    int i;
    int n;

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
        default:
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
#else
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
#endif

#if MWCCPS2
Boolean CInt64_IsInRange(CInt64 value, short len) {
    int i;
    int n;

    if (len >= CINT64_WORDS * 4) {
        return 1;
    }
    if (W(value, 0) & 0x80000000) {
        value = CInt64_Inv(value);
    }
    n = CINT64_WORDS - 1 - ((len - 1) >> 2);
    for (i = 0; i < n; i++) {
        if (W(value, i)) {
            return 0;
        }
    }
    switch (len & 3) {
        default:
        case 1:
            return !(W(value, i) & 0xFFFFFF80);
        case 2:
            return !(W(value, i) & 0xFFFF8000);
        case 3:
            return !(W(value, i) & 0xFF800000);
        case 0:
            return !(W(value, i) & 0x80000000);
    }
}
#else
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
#endif

extern const double data_005CBC10;  // 1.0

double CInt64_ConvertUToLongDouble(const CInt64* value) {
    double result = 0.0;
    UInt32 work;
    int i;
    int j;

#if MWCCPS2
    if (!iszero((CInt64*)value)) {
#else
    if (!CInt64_IsZero((CInt64*)value)) {
#endif
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
#if MWCCPS2
    int carry;

    INV_WORDS(input);
    carry = 0;
#else
    int carry = 0;


    W(input, 0) = ~W(input, 0);
    W(input, 1) = ~W(input, 1);
#endif
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
