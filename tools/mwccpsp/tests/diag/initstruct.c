/* struct initializer diagnostics */
struct Pt { int x, y; };
struct Flex { int n; int v[]; };
struct Outer { struct Flex f; int z; };
struct Inc;
static struct Pt a = { };
static struct Pt b = { 1, 2, 3 };
#pragma c99 on
static struct Pt c = { .z = 1 };
static struct Pt d = { [0] = 1 };
struct Line { struct Pt a, b; };
static struct Line ln = { .a.y = 2, .b.x = 1 };
#pragma c99 off
static struct Pt e = { { 1, 2 } };
void f(struct Pt p) {
    struct Pt q = { p };
    struct Pt r[1] = { 1, 2, 3 };
}
static struct Flex fl2 = { 3, { 1, 2, 3 } };
