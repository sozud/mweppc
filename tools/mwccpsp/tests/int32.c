// 32-bit and narrow integers: division and modulo by constants (powers of
// two and not, signed and unsigned), shifts, ++/-- on pointers and chars,
// struct copies, switch.
int sdivc(int a) { return a / 4 + a / 3 + a / -8 + a / 1000 + a % 8 + a % 7; }
unsigned udivc(unsigned a) { return a / 4 + a / 3 + a / 1000 + a % 8 + a % 7; }
int sdiv(int a, int b) { return a / b + a % b; }
unsigned udiv(unsigned a, unsigned b) { return a / b + a % b; }
int shifts(int a, unsigned b, int n) { return (a << n) + (a >> n) + (b >> n) + (a >> 3) + (b >> 31); }
char cinc(char* p) { (*p)++; return --*p; }
short sinc(short* p) { return (*p)++ + ++*p; }
int* pinc(int** p) { (*p)++; return ++*p; }
struct Big { int a[9]; char c; };
void scopy(struct Big* d, struct Big* s) { *d = *s; }
struct Big sret(struct Big* s) { return *s; }
int sw(int x) {
    switch (x) {
    case 0: return 5;
    case 1: return 9;
    case 2: case 3: return x * 2;
    case 10: return 7;
    case 100: return 1;
    default: return -1;
    }
}
int neg(int a, short b, signed char c) { return -a + -b + -c; }
int narrow(int a) { short s = a; signed char c = a; unsigned char u = a; unsigned short us = a; return s + c + u + us; }
int cond(int c, int a, int b) { return c ? a : b; }
int logic(int a, int b) { return (a && b) + (a || b) * 2 + !a * 4; }
// EBINNOT (func_0049AF50): ~x and ~(a | b), 32 and 64 bits
int bnot(int a, int b) { return ~a + ~(a | b) * 3; }
long long bnot64(long long a, long long b) { return ~a ^ ~(a | b); }
// GCC <? and >? (EMIN/EMAX), setjmp
int minmax(int a, int b) { return (a <? b) * 2 + (a >? b); }
extern int __setjmp(int*);
int sj(int* buf, int v) { if (__setjmp(buf)) return v; return 0; }
