/* Type names in C diagnostics: struct, union, enum, arrays, pointers to
   functions with qualified return types, bitfields */
struct S { int x; int bf : 3; };
union U { int x; };
enum E { e1 };
typedef const volatile int (*pf)(int, char*);
void test(void) {
    struct S s;
    union U u;
    enum E en;
    int arr[3][4];
    pf p = 0;
    int* ip;
    ip = s;
    ip = u;
    ip = 1.5;
    s = arr;
    s = p;
    s = s.bf;
    u = en;
}
