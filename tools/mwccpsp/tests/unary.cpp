// unary operators: overloaded operators, templates (dependent operands),
// bool ++, constant folding, addresses of members and functions
struct N { int v; N operator-() const; N operator~() const; bool operator!() const; N& operator++(); N& operator--(); int operator*() const; N* operator&(); };
struct M { int m; int f(); static int s; };
template <class T> T neg(T x) { return -x; }
template <class T> T inv(T x) { return ~x; }
template <class T> bool lnot(T x) { return !x; }
template <class T> T plus(T x) { return +x; }
template <class T> T deref(T* p) { return *p; }
template <class T> T* addr(T& x) { return &x; }
template <class T> T pre(T x) { ++x; --x; return x; }
int g(N n, int i, float f, bool b, int* p, M& mm) {
    N a = -n, c = ~n;
    bool d = !n;
    ++n; --n;
    int e = *n;
    N* q = &n;
    ++b;
    int (M::*pf)() = &M::f;
    int M::*pm = &M::m;
    int* ps = &M::s;
    return a.v + c.v + d + e + q->v + b + -i + ~i + !i + +i + (int)-f + !f + *p + !p + -5 + ~7 + !0 +
           neg(i) + inv(i) + lnot(i) + plus(i) + deref(p) + *addr(i) + pre(i) + (int)neg(f) + (mm.*pf)() + mm.*pm + *ps;
}
