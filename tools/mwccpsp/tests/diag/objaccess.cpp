// name access diagnostics
struct P { int x; private: int y; static int s; int f(); };
struct Q { int z; };
struct A1 { int m; }; struct A2 { int m; };
struct AM : A1, A2 { };
int g(P& p, Q& q, AM& am) {
    int a = p.y;
    int b = P::x;
    int c = p.s;
    int d = q.nosuch;
    int e = am.m;
    int f = undeclared_thing;
    return a + b + c + d + e + f + p.f;
}
int h() { return Q::z; }
struct AS { struct { int x, y; }; int z; };
int k(AS& a) { return a.x + a.z; }
