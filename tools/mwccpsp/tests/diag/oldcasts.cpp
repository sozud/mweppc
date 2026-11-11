// explicit conversion diagnostics under old_argmatch
#pragma old_argmatch on
struct S { int a; };
struct T { int b; };
void g(int i, float f, S s, double* dp) {
    int* p = (int*)f;
    float x = (float)dp;
    T t = (T)s;
    int a[2] = (int[2])i;
    S* sp = (S*)f;
}
