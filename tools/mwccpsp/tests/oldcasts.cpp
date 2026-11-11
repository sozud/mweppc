// explicit conversions under #pragma old_argmatch (do_typecast's own path)
#pragma old_argmatch on
struct B { int b; };
struct D : B { int d; };
enum E { E0, E1 = 5 };
struct M { int m; int f(int); };
struct Conv { int v; Conv(int); operator int() const; };
int M::f(int x) { return x; }
int g(int i, float f, char* p, D* d, B* b, E e, Conv c) {
    long l = (long)i;
    short s = (short)f;
    unsigned char uc = (unsigned char)e;
    float ff = (float)i + (float)l;
    double dd = (double)ff;
    int ip = (int)p;
    char* cp = (char*)ip;
    B* bp = (B*)d;
    D* dp = (D*)b;
    void* vp = (void*)d;
    E ee = (E)i;
    bool bb = (bool)i;
    int (M::*pf)(int) = (int (M::*)(int))&M::f;
    int M::*pm = (int M::*)0;
    const int& r = (const int&)i;
    Conv c2 = (Conv)i;
    int ci = (int)c;
    (void)i;
    return l + s + uc + (int)dd + ip + (cp != 0) + (bp != 0) + (dp != 0) + (vp != 0) + ee + bb + (pf != 0) + (pm != 0) + r + c2.v + ci + (int)(short)(char)i + (int)(unsigned short)l;
}
#pragma old_argmatch off
