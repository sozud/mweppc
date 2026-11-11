// primary expressions: strings in inline and template functions (numbered
// for pooling), true/false, this, comma operators, builtins
#pragma wchar_type on
inline const char* inl() { return "inline-string"; }
inline const wchar_t* winl() { return L"wide-inline"; }
template <class T> const char* tf(T) { return "template-string"; }
static inline const char* sinl() { return "static-inline"; }
struct S { int v; int get() { return this->v + (v, 1); } const char* name() { return "member"; } };
const char* plain() { return "plain"; }
int f(int x) {
    S s;
    bool b = true, c = false;
    int y = (x, x + 1, x + 2);
    return inl()[0] + winl()[1] + tf(1)[2] + tf('c')[3] + sinl()[1] + s.get() + s.name()[0] + plain()[2] + b + c + y +
           (true ? 1 : 2) + __builtin_align(int) + __builtin_classify_type(x);
}
int use_alias(int i) { extern int ext_tbl[]; extern int ext_count; return ext_tbl[i] + ext_count; }
