// names to expressions: members through objects and pointers, static
// members and enumerators through objects (with side effects), anonymous
// unions, overloaded and template functions, pointers to members,
// members of class templates, base class members
struct B { int b; static int sb; int bf() { return b; } };
struct D : B {
    enum { K = 3, L };
    int d;
    union { int u; float fl; };
    static int sd;
    int f(int);
    int f(double);
    static int sf(int);
    int g() { return d + b + u + K + sd + bf() + f(1) + sf(2) + this->d + D::b; }
};
int B::sb = 1;
int D::sd = 2;
int D::f(int x) { return x; }
int D::f(double x) { return (int)x; }
int D::sf(int x) { return x; }
template <class T> struct TC {
    T v;
    static T s;
    enum { E = sizeof(T) };
    T get() { return v + s + E; }
};
template <class T> T TC<T>::s = 0;
template <class T> T tmax(T a, T b) { return a > b ? a : b; }
D* mk();
int ov(int);
int ov(char);
int h(D& d, D* p) {
    int D::*pm = &D::d;
    int (D::*pf)(int) = &D::f;
    int (*sp)(int) = &D::sf;
    TC<int> ti;
    TC<short> ts;
    return d.d + p->b + d.u + d.K + p->L + mk()->sd + mk()->K + d.sb + p->sf(3) + d.f(2.0) +
           d.*pm + (p->*pf)(4) + sp(5) + ti.get() + ts.get() + ti.E + TC<char>::E + tmax(1, 2) +
           tmax<long>(3, 4) + ov('c') + ov(1) + d.B::b + p->D::d;
}
int use_ext(int i) { extern int ext_arr[]; extern int ext_n; return ext_arr[i] + ext_n; }
