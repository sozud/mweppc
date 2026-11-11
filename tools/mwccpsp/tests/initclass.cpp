// C++ aggregate classes: anonymous unions, bitfields, unions, nested
// aggregates, designators, flexible array members, class-valued members
struct P { int x, y; };
struct A { int a; union { int u; float f; }; int b; };
struct B { unsigned a : 3, b : 5; unsigned c : 4; int d; };
union U { int i; char c[4]; };
struct N { P p; U u; P arr[2]; };
struct F { int n; int v[]; };
struct E { };
struct Q { char c; double d; int i; };
static A ga = { 1, 2, 3 };
static B gb = { 1, 2, 3, 4 };
static U gu = { 5 };
static N gn = { { 1, 2 }, { 3 }, { { 4, 5 }, 6 } };
static N gn2 = { 1, 2, 3, 4, 5, 6, 7 };
static F gf = { 2, { 7, 8 } };
static F gf2 = { 2, 9, 10 };
static E ge = { };
static Q gq = { 'a', 1.5, 3 };
static P gps[] = { { 1, 2 }, 3, 4 };
#pragma gcc_extensions on
static P gd = { .y = 1, .x = 2 };
static N gnd = { .u = { 9 }, .p = { .y = 3 } };
#pragma gcc_extensions off
P mk();
int f(int x) {
    P p = { x, x + 1 };
    P q = p;
    A a = { x, x, x };
    N n = { p, { x }, { q, p } };
    N n2 = { mk() };
    U u = { x };
    static P sp = { 1, 2 };
    return p.y + q.x + a.u + a.b + n.arr[1].y + n2.p.x + u.i + sp.y + ga.b + gb.c + gu.i + gn.arr[1].x + gn2.u.i +
           gf.v[1] + gf2.v[1] + gq.i + gps[1].y + gd.y + gnd.p.y + sizeof(gf) + sizeof(gps);
}
