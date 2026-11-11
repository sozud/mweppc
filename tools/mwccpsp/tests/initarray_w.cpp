// wide strings into wchar_t arrays; non-constant local aggregates
#pragma wchar_type on
static wchar_t w1[] = L"wide";
static wchar_t w2[8] = L"ab";
static const wchar_t* wp[] = { L"p", 0 };
int g() { wchar_t l[] = L"loc"; return w1[2] + w2[7] + wp[0][0] + l[1] + sizeof(w1); }
struct P { int a, b; };
int h(int x) {
    int a[] = { x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x };
    int c[3][2] = { { x }, { x, x } };
    P p = { x, x + 1 };
    P ps[] = { { x }, p, { 1, x } };
    return a[18] + c[1][1] + p.b + ps[2].b + sizeof(ps);
}
