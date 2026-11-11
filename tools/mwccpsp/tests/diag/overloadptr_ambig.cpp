// overloaded and template function names converted to function pointers
// during overload resolution (CExpr_StandardConversionMatch EOBJACCESS)
int f(int x) { return x + 1; }
int f(char c) { return c + 2; }
long f(long l) { return l + 3; }

template <class T> T g(T x) { return x * 2; }

struct S {
    static int h(int x) { return x - 1; }
    static int h(short x) { return x - 2; }
};

int apply_i(int (*fn)(int), int v) { return fn(v); }
int apply_c(int (*fn)(char), char v) { return fn(v); }
long apply_l(long (*fn)(long), long v) { return fn(v); }

int pick(int (*fn)(int)) { return fn(1); }
int pick(long (*fn)(long)) { return (int)fn(2); }

int use1(int v) { return apply_i(f, v) + apply_c(f, (char)v) + (int)apply_l(f, v); }
int use2(int v) { return apply_i(g, v) + apply_i(&g<int>, v) + (int)apply_l(g<long>, v); }
int use3(int v) { return apply_i(S::h, v) + apply_i(&S::h, v); }
int use4() { return pick(&f) + pick(g<int>); }

int (*tbl[3])(int) = {f, g, S::h};
long (*lp)(long) = &f;

int use5(int i, int v) { return tbl[i](v) + (int)lp(v); }
