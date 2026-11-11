// long long arithmetic: shifts, multiply, divide, modulo, compares, ++/--,
// unary minus, conversions.
typedef long long s64;
typedef unsigned long long u64;

s64 shl(s64 a, int n) { return a << n; }
s64 shr(s64 a, int n) { return a >> n; }
u64 shru(u64 a, int n) { return a >> n; }
s64 shlc(s64 a) { return (a << 5) + (a << 33); }
s64 shrc(s64 a) { return (a >> 7) ^ (a >> 40); }
u64 shruc(u64 a) { return (a >> 1) | (a >> 63); }
s64 mul(s64 a, s64 b) { return a * b; }
s64 mulc(s64 a) { return a * 10 + a * -3; }
s64 div_(s64 a, s64 b) { return a / b; }
u64 divu(u64 a, u64 b) { return a / b; }
s64 mod_(s64 a, s64 b) { return a % b; }
u64 modu(u64 a, u64 b) { return a % b; }
s64 divc(s64 a) { return a / 8 + a / 7 + a % 16; }
int cmp(s64 a, s64 b) { return (a < b) + (a <= b) * 2 + (a == b) * 4 + (a != b) * 8 + (a > b) * 16 + (a >= b) * 32; }
int cmpu(u64 a, u64 b) { return (a < b) + (a > b) * 2; }
s64 neg(s64 a) { return -a; }
s64 inc(s64* p) { (*p)++; ++*p; return (*p)-- + --*p; }
s64 bits(s64 a, s64 b) { return (a & b) | (a ^ ~b); }
s64 asg(s64* p, s64 b) { *p += b; *p -= 3; *p *= b; *p <<= 2; *p >>= 1; *p &= b; *p |= 1; *p ^= b; return *p; }
s64 conv(int i, unsigned u, short s, unsigned char c) { return (s64)i + (u64)u + s + c; }
int trunc(s64 a) { return (int)a + (short)a + (unsigned char)a; }
s64 cond(int c, s64 a, s64 b) { return c ? a : b; }
int test(s64 a) { if (a) return 1; if (!a) return 2; return a ? 3 : 4; }
