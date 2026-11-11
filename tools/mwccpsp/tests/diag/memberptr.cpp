// member pointer diagnostics
struct V { int a; int f(int); int f(char); ~V(); static int s(int); };
int g(V* p) {
    int (V::*pf)(double) = &V::f;
    int (*sf)(int) = &V::f;
    int V::*pa = &V::f;
    void (V::*pd)() = &V::~V;
    int (V::*ps)(int) = &V::s;
    return 0;
}
