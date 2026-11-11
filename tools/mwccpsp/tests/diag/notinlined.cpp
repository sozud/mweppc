// "inline function call not inlined" warnings from the post-expansion pass
#pragma warn_notinlined on
inline int rec(int n) { return n <= 0 ? 0 : n + rec(n - 1); }
struct V {
    virtual int f() { return 1; }
};
inline int big(int v) {
    int t = 0;
    for (int i = 0; i < v; i++) {
        t += i * v;
        t ^= (t << 3) | (t >> 2);
        t -= i;
    }
    return t;
}
#pragma inline_depth(1)
int use(int v, V* p) { return rec(v) + p->f() + big(v) + big(v + 1); }
#pragma dont_inline on
int use2(int v) { return rec(v); }
