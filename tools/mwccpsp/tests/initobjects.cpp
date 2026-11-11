// local and static objects: references (bound to temporaries), classes with
// constructors and destructors, arrays of them, guarded static locals,
// aggregate classes, scalars from parenthesized initializers
struct C { int v; C(); C(int); C(int, int); ~C(); };
struct D { int v; D(int x) : v(x) {} };
struct Agg { int a; float b; };
struct E2 { int k; ~E2(); };
extern int ext();
C mk(int);
static C gc1;
static C gc2(3);
static C gc3 = 4;
static C gc4(1, 2);
static C garr[3];
static C garr2[2] = { 5, 6 };
static D gd = 7;
static Agg ga = { 1, 2.0f };
static int gi(5);
static const int& gr = 9;
static E2 ge = { 3 };
int f(int x) {
    int i(x);
    int j = { x };
    const int& r = x + 1;
    const C& rc = mk(x);
    C c1;
    C c2(x);
    C c3 = x;
    C c4(x, x);
    C arr[2];
    C arr2[2] = { x, 1 };
    D d = x;
    Agg a = { x, 1.0f };
    E2 e = { x };
    static C sc(ext());
    static C sarr[2];
    static int si = ext();
    static const int& sr = ext();
    static Agg sa = { ext(), 2.0f };
    static D sd(ext());
    float fl(x);
    int* p(&i);
    char str[] = "abc";
    return i + j + r + rc.v + c1.v + c2.v + c3.v + c4.v + arr[1].v + arr2[0].v + d.v + a.a + e.k + sc.v + sarr[1].v + si + sr + sa.a + sd.v + (int)fl + *p + str[1] + gc3.v + garr2[1].v + gd.v + ga.a + gi + gr + ge.k;
}
