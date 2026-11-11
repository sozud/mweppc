// Bitfield stores: read-modify-write through memory and into register words
// (func_00496D50's bitfield path, func_004A5380, func_004A5730).
struct S { unsigned a : 3; unsigned b : 5; unsigned c : 20; unsigned d : 4; };
struct T { signed char a : 2; signed char b : 3; short c : 9; };

void f1(struct S* p, int x) { struct S s = *p; s.b = x; *p = s; }
void f2(struct S* p, int x, int y) { struct S s; s = *p; s.c = x; s.d = y; s.a = x + y; *p = s; }
unsigned f3(int x) { struct S s; *(unsigned*)&s = 0; s.b = x; s.c = x * 3; return *(unsigned*)&s; }
int f4(int x, int y) { struct S s; s.a = x; s.b = y; s.c = x ^ y; s.d = 1; return s.a + s.b + s.c + s.d; }
int f5(int x) { struct T t; t.a = x; t.b = x >> 1; t.c = x * 7; return t.a + t.b + t.c; }

// the value of a bitfield assignment (func_004A5730), signed and unsigned,
// at offset zero and not
struct V { int s0 : 4; unsigned u4 : 6; int s10 : 12; unsigned u22 : 10; };
int g1(struct V* p, int x) { return p->s0 = x; }
int g2(struct V* p, int x) { return p->u4 = x; }
int g3(struct V* p, int x) { return p->s10 = x; }
int g4(struct V* p, int x) { return (p->u22 = x) + (p->s0 = x >> 2); }
int g5(int x) { struct V v; *(int*)&v = 0; return (v.s10 = x) * (v.u4 = x + 1); }
