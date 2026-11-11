/* Diagnostics: an undeclared identifier, a type mismatch, a missing
   semicolon and a redefinition. The messages and their numbers must match. */
int f(int a) {
    return a + undeclared;
}

struct S { int x; };

int g(struct S s) {
    int* p = s;
    return *p
}

int f(int a) {
    return a;
}
