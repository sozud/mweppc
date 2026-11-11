// reference binding: lvalues, temporaries, base classes, explicit casts
struct A {
    int a;
};
struct B : A {
    int b;
};
struct C {
    int c;
    operator int() const { return c; }
    operator A&() { return *(A*)this; }
};

int take(const int& r) { return r; }
int takeA(const A& r) { return r.a; }
int takeAm(A& r) { return r.a; }
double taked(const double& d) { return d; }

int g1(int x) { return take(x) + take(x + 1) + take(3); }
int g2(B& b) { return takeA(b) + takeAm(b); }
int g3(const B& b) { return takeA(b); }
int g4(C& c) { return take(c) + takeAm(c); }
double g5(float f) { return taked(f) + taked(2); }
int g6(B* p) { return ((A&)*p).a + ((const A&)*p).a; }
int g7(A& a) { return ((B&)a).b; }
int g8(long l) { const int& r = l; return r; }
int g9() { const A& r = B(); return r.a; }
int g10(C c) { const int& r = c; return r; }
