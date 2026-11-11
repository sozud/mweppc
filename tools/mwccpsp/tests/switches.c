// switchstatement (func_004A4A40): dense switches become jump tables
// (density > 0.7 and at least 6 cases), the rest compare and branch; long
// long selectors compare word by word; GCC case ranges (only in sparse
// switches: in a jump table the original never finishes a range).
int dense(int x) {
    switch (x) {
    case 0: return 3; case 1: return 7; case 2: return 1; case 3: return 9;
    case 4: return 4; case 5: return 2; case 6: return 8; case 7: return 5;
    default: return -1;
    }
}
int dense_off(int x) {
    switch (x) {
    case 100: return 3; case 101: return 7; case 102: return 1; case 103: return 9;
    case 104: return 4; case 105: return 2; case 107: return 8;
    default: return 0;
    }
}
int dense_small(int x) {
    switch (x) {
    case 2: return 3; case 3: return 7; case 4: return 1; case 5: return 9;
    case 6: return 4; case 7: return 2;
    default: return 0;
    }
}
int dense_neg(int x) {
    switch (x) {
    case -3: return 1; case -2: return 2; case -1: return 3; case 0: return 4;
    case 1: return 5; case 2: return 6; case 3: return 7;
    default: return 0;
    }
}
int dense_uchar(unsigned char c) {
    switch (c) {
    case 'a': return 1; case 'b': return 2; case 'c': return 3; case 'd': return 4;
    case 'e': return 5; case 'f': return 6; case 'g': return 7;
    }
    return 0;
}
int sparse(int x) {
    switch (x) {
    case 1: return 10; case 50: return 20; case 1000: return 30; case -7: return 40;
    default: return 0;
    }
}
int ranges(int x) {
    switch (x) {
    case 1 ... 5: return 1;
    case 100 ... 101: return 2;
    case 1000: return 3;
    case -50 ... -40: return 4;
    }
    return 0;
}
int ll(long long x) {
    switch (x) {
    case 0: return 1; case 1: return 2; case 0x100000000LL: return 3; case -1: return 4;
    }
    return 0;
}
int uns(unsigned x) {
    switch (x) {
    case 0x80000000u: return 1; case 0xffffffffu: return 2; case 3: return 3;
    }
    return 0;
}
