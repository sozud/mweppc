// calls: argument-dependent lookup, explicit destructor calls, calls
// through function pointers (promotion, varargs), default arguments,
// operator(), template calls with dependent arguments
namespace NS { struct T { int v; }; int adl(T t) { return t.v; } int adl2(T* t, int k = 3) { return t->v + k; } }
struct D { int v; ~D(); };
struct F { int operator()(int x) const { return x * 2; } };
int va(int n, ...);
int dflt(int a, int b = 2, long c = 7) { return a + b + (int)c; }
typedef int (*FP)(int, short, double);
typedef int (*VP)(int, ...);
template <class T> int tcall(T t) { return dflt(t) + NS::adl2(0, t); }
template <class T> int tcall2(T* p) { return adl(*p); }
int g(NS::T t, D* d, FP fp, VP vp, F f) {
    d->~D();
    d->D::~D();
    return adl(t) + adl2(&t) + adl2(&t, 4) + fp(1, 2, 3.0) + fp('c', 1.5, 2) + vp(1, 2.0f, 'c', (short)3) +
           va(2, 1.0f, 2) + dflt(1) + dflt(1, 2) + dflt(1, 2, 3) + f(4) + tcall(5) + tcall2(&t);
}
