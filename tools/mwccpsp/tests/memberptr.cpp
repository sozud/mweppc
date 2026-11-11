// pointers to members: data members, overloaded, virtual and template
// member functions, conversions between member pointer types
struct V { int a; virtual int vf(int); virtual int vf(char); int nf(int); int nf(long); template <class T> int tf(T); };
struct W : V { int b; int vf(int); };
int V::vf(int x) { return x; }
int V::vf(char x) { return x + 1; }
int V::nf(int x) { return x + 2; }
int V::nf(long x) { return (int)x + 3; }
template <class T> int V::tf(T x) { return (int)x; }
int W::vf(int x) { return x * 2; }
typedef int (V::*VIntFn)(int);
typedef int (V::*VCharFn)(char);
int call(V* p, VIntFn f, int x) { return (p->*f)(x); }
int g(V* p, W* w) {
    int V::*pa = &V::a;
    int W::*pb = &W::b;
    int W::*pa2 = &V::a;
    VIntFn f1 = &V::vf;
    VCharFn f2 = &V::vf;
    VIntFn f3 = &V::nf;
    int (V::*f4)(long) = &V::nf;
    int (V::*f5)(short) = &V::tf<short>;
    int (V::*f6)(double) = &V::tf;
    int (W::*f7)(int) = &W::vf;
    return p->*pa + w->*pb + w->*pa2 + (p->*f1)(1) + (p->*f2)('c') + (p->*f3)(2) + (p->*f4)(3L) + (p->*f5)(4) +
           (p->*f6)(5.0) + (w->*f7)(6) + call(p, &V::vf, 7) + call(w, &V::nf, 8);
}
