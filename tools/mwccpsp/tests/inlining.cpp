// inlining paths: dynamic initializers (CInline_Finish), deep and recursive
// inline chains, bottom-up inlining, member pointers and references inside
// inline functions, statement-expression kinds the expander walks
struct Point {
    int x, y;
    Point(int a, int b) : x(a), y(b) {}
    int sum() const { return x + y; }
};

inline int sq(int v) { return v * v; }
inline int quad(int v) { return sq(sq(v)); }
inline int oct(int v) { return quad(quad(v)); }
inline int fact(int n) { return n <= 1 ? 1 : n * fact(n - 1); }

int seed = 3;
int g1 = quad(seed);                 // dynamic: needs the init function
Point gp(sq(seed), oct(2));
static int g2 = Point(1, 2).sum() + fact(5);
int* gptr = &g1;

struct Holder {
    int a;
    int b;
    int pick(int Holder::*m) { return this->*m; }
};

inline int viaMember(Holder& h, bool first) {
    int Holder::*m = first ? &Holder::a : &Holder::b;
    int Holder::*none = 0;
    return h.pick(m) + (none == 0 ? 1 : 0);
}

inline int& ref(int& r) { return r; }
inline int aliasUse(int v) {
    int& r = ref(v);
    r += 2;
    return r;
}

int useAll(int v, Holder& h) {
    int t = 0;
    for (int i = 0; i < v; i++) {
        if (i & 1) {
            t += quad(i);
        } else {
            t -= oct(i);
        }
        switch (i % 3) {
            case 0: t += viaMember(h, true); break;
            case 1: t += viaMember(h, false); break;
            default: t += aliasUse(i); break;
        }
    }
    return t + fact(v) + g2;
}

#pragma inline_bottom_up on
inline int b1(int v) { return v + 1; }
inline int b2(int v) { return b1(b1(v)) * 2; }
inline int b3(int v) { return b2(b2(v)) - b1(v); }
int bottomUp(int v) { return b3(v) + b3(v + 1); }
#pragma inline_bottom_up off

#pragma inline_depth(2)
int shallow(int v) { return oct(v); }
#pragma inline_depth(smart)

#pragma always_inline on
inline int ai(int v) { return quad(v) + 1; }
int alwaysInl(int v) { return ai(v) + ai(v + 1); }
#pragma always_inline off
