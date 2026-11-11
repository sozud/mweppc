// class array initializer diagnostics
struct C { int v; C(int); ~C(); };      // no default constructor
struct D { int v; D(); D(int); };
static C a[3];
static D b[2] = { 1, 2, 3 };
static D c[2] = { { 1 }, 2 };
static D d[] = { };
void f() {
    C la[2];
    D lb[1] = { 1, 2 };
    D lc[2][2] = { 1, { 2 }, 3 };
}
