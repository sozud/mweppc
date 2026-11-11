// Type names in diagnostics (CError_BufferAppendType): member function
// cv-qualifiers, qualified return types, class template instances, struct,
// union, enum, arrays and pointers to functions.
struct A {
    void f() const;
    int g() volatile;
};
template <class T> struct B {
    T t;
};
typedef const int (*pf)(int);
struct S {
    int x;
};
union U {
    int x;
};
enum E { e1 };

void test() {
    int i;
    void (A::*mp)() const = &A::f;
    int (A::*mg)() volatile = &A::g;
    B<int> b;
    pf p = 0;
    S s;
    U u;
    int arr[3][4];
    E en = e1;
    i = mp;
    i = mg;
    i = b;
    i = p;
    i = s;
    i = u;
    i = arr;
    char* cp = en;
    const int (*rf)(int) = 0;
    i = rf;
}
