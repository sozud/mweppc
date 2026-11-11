// C++ statements: declarations in conditions and for-init, mixed
// declarations, template-dependent statements, try
extern int g(int);
struct S {
    int x;
    S(int v) : x(v) {}
    operator bool() const { return x != 0; }
};
int f(int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) sum += i;
    if (int t = g(n)) sum += t;
    while (int u = g(sum)) { sum -= u; if (sum < 0) break; }
    switch (int w = n & 7) { case 1: sum += w; break; default: break; }
    if (S s = S(n)) sum += s.x;
    int late = sum * 2;
    for (int a = 0, b = 10; a < b; a++, b--) late += a * b;
    return late;
}
template <class T> T tsum(T* p, int n) {
    T r = 0;
    for (int i = 0; i < n; i++) r += p[i];
    if (r) r = r + 1;
    while (n > 100) n--;
    switch (n) { case 1: r = r * 2; break; }
    return r;
}
int use(int* p) { return tsum(p, 3) + (int)tsum((double*)0, 0); }
int tr(int n) {
    try { if (n) throw n; } catch (int e) { return e; }
    return 0;
}
