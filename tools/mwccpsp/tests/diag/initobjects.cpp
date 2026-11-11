// object initializer diagnostics
struct C { int v; C(int); ~C(); };
struct P { int a; };
void f(int x) {
    C c = { 1, 2 };
    C arr[2] = 3;
    int i(1, 2);
    int& r;
    const int k;
    P p(1, 2);
    static int s(1, 2);
    static C sc = { 1 };
    static C sarr[1] = 4;
}
static int g(1, 2);
static const int gk;
