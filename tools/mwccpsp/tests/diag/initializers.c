/* initializer diagnostics: too many initializers, illegal initializations */
struct P { int a, b; };
static int x[2] = { 1, 2, 3 };
static struct P p = { 1, 2, 3 };
static int y = { 1, 2 };
static int z = { };
void f(void) {
    int q = { { 1 } };
    struct P r = { 1, { 2 } };
}
