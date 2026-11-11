// mangled names of templates with non-type arguments: integers, object and
// function addresses, member pointers (CMangler template argument lists)
int gvar;
int garr[4];
int gfunc(int x) { return x + 1; }

struct M {
    int a;
    int b;
    int f(int x) { return x + a; }
    static int s;
};
int M::s;

template <int N> struct IntArg {
    static int get() { return N; }
};
template <int* P> struct PtrArg {
    static int get() { return *P; }
};
template <int (*F)(int)> struct FuncArg {
    static int call(int x) { return F(x); }
};
template <int M::*PM> struct MemArg {
    static int get(M& m) { return m.*PM; }
};
template <int (M::*PMF)(int)> struct MemFuncArg {
    static int call(M& m, int x) { return (m.*PMF)(x); }
};
template <int* P, int N> struct Mixed {
    static int get() { return P[N]; }
};

template <int* P> int ptr_fn() { return *P + 1; }
template <int N, class T> T scale(T v) { return v * N; }

int use(M& m) {
    return IntArg<5>::get() + IntArg<-3>::get() + PtrArg<&gvar>::get() + PtrArg<&M::s>::get() +
        FuncArg<&gfunc>::call(2) + FuncArg<gfunc>::call(3) + MemArg<&M::a>::get(m) +
        MemArg<&M::b>::get(m) + MemFuncArg<&M::f>::call(m, 4) + Mixed<garr, 2>::get() +
        ptr_fn<&gvar>() + scale<3>(m.a) + (int)scale<2>(1.5f);
}
