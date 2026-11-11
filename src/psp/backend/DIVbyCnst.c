// mwccpsp 3.0.1 DIVbyCnst.c (0x53ECC0..0x53EEF0): the multiply-high "magic
// numbers" that turn a division by a constant into a multiplication (the
// algorithms of Hacker's Delight, 10-1 and 10-2).
#include "compiler/common.h"
#include "compiler/CError.h"
#include <stdlib.h>

typedef struct SMagic {
    SInt32 magic;
    SInt32 shift;
} SMagic;

typedef struct UMagic {
    UInt32 magic;
    SInt32 shift;
    SInt32 add; // the magic number needs 33 bits: add the dividend back
} UMagic;

// unsigned division by d (d > 1)
UMagic func_0053ECC0(UInt32 d) {
    UMagic magu;
    SInt32 p;
    UInt32 nc;
    UInt32 delta;
    UInt32 q1;
    UInt32 r1;
    UInt32 q2;
    UInt32 r2;

    CError_ASSERT(54, d > 1);
    magu.add = 0;
    nc = -1 - (-d) % d;
    p = 31;
    q1 = 0x80000000 / nc;
    r1 = 0x80000000 - q1 * nc;
    q2 = 0x7FFFFFFF / d;
    r2 = 0x7FFFFFFF - q2 * d;
    do {
        p = p + 1;
        if (r1 >= nc - r1) {
            q1 = 2 * q1 + 1;
            r1 = 2 * r1 - nc;
        } else {
            q1 = 2 * q1;
            r1 = 2 * r1;
        }
        if (r2 + 1 >= d - r2) {
            if (q2 >= 0x7FFFFFFF) {
                magu.add = 1;
            }
            q2 = 2 * q2 + 1;
            r2 = 2 * r2 + 1 - d;
        } else {
            if (q2 >= 0x80000000) {
                magu.add = 1;
            }
            q2 = 2 * q2;
            r2 = 2 * r2 + 1;
        }
        delta = d - 1 - r2;
    } while (p < 64 && (q1 < delta || (q1 == delta && r1 == 0)));

    magu.magic = q2 + 1;
    magu.shift = p - 32;
    return magu;
}

// signed division by d (|d| > 2)
SMagic func_0053EE10(SInt32 d) {
    SMagic mag;
    SInt32 p;
    UInt32 ad;
    UInt32 anc;
    UInt32 delta;
    UInt32 q1;
    UInt32 r1;
    UInt32 q2;
    UInt32 r2;
    UInt32 t;

    ad = abs(d);
    CError_ASSERT(14, ad > 2);
    t = 0x80000000 + ((UInt32)d >> 31);
    anc = t - 1 - t % ad;
    p = 31;
    q1 = 0x80000000 / anc;
    r1 = 0x80000000 - q1 * anc;
    q2 = 0x80000000 / ad;
    r2 = 0x80000000 - q2 * ad;
    do {
        p = p + 1;
        q1 = 2 * q1;
        r1 = 2 * r1;
        if (r1 >= anc) {
            q1 = q1 + 1;
            r1 = r1 - anc;
        }
        q2 = 2 * q2;
        r2 = 2 * r2;
        if (r2 >= ad) {
            q2 = q2 + 1;
            r2 = r2 - ad;
        }
        delta = ad - r2;
    } while (q1 < delta || (q1 == delta && r1 == 0));

    mag.magic = q2 + 1;
    if (d < 0) {
        mag.magic = -mag.magic;
    }
    mag.shift = p - 32;
    return mag;
}
