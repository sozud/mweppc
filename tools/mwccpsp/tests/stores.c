// Stores (func_004A5A90) and loads (func_004A64C0): locals living in
// registers of every width, long long swaps (the register-pair overlap
// cases), structs with a single member, volatile objects, float library
// doubles.
typedef long long s64;

int narrow_locals(int x) {
    char c = x; unsigned char uc = x + 1; short s = x * 3; unsigned short us = x ^ 5;
    signed char sc;
    c += 3; uc *= 2; s -= x; us |= 9; sc = c + uc;
    return c + uc + s + us + sc;
}
s64 swap64(s64 a, s64 b, int n) {
    s64 t;
    while (n--) { t = a; a = b; b = t; a += 1; }
    return a - b;
}
s64 rot64(s64 a, s64 b, s64 c, int n) {
    while (n--) { s64 t = a; a = b; b = c; c = t; }
    return a + b * 2 + c * 3;
}
struct OneF { float f; };
struct OneI { int i; };
struct OneD { double d; };
void store1(struct OneF* p, struct OneI* q, struct OneD* r, float f, int i, double d) {
    struct OneF a; struct OneI b; struct OneD c;
    a.f = f; b.i = i; c.d = d;
    *p = a; *q = b; *r = c;
}
volatile int vi;
volatile s64 vl;
volatile float vf;
void vol(int x, s64 y, float z) { vi = x; vl = y; vf = z; vi = vi + 1; }
double dloc(double a, double b, int n) { double t = a; while (n--) { t = t * b + a; a = t; } return t; }
float floc(float a, float b, int n) { float t = a; while (n--) { t = t * b + a; a = t; } return t; }
int fbits(float f) { union { float f; int i; } u; u.f = f; return u.i; }
s64 dbits(double d) { union { double d; s64 i; } u; u.d = d; return u.i; }
float ffrombits(int i) { union { float f; int i; } u; u.i = i; return u.f; }
// a local's register pair overlapping the stored value's (word shuffles)
s64 hi_to_lo(s64 a, int n) { while (n--) a = a >> 32; return a; }
s64 lo_to_hi(s64 a, int n) { while (n--) a = a << 32; return a; }
unsigned long long wswap(unsigned long long a, int n) { while (n--) a = (a >> 32) | (a << 32); return a; }
s64 hilo(s64 a, s64 b, int n) { while (n--) { a = (a << 32) | (unsigned)(b >> 32); b = a >> 32; } return a ^ b; }
