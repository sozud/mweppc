/* struct/union initialization: flexible array members, unions, unnamed
   bitfields, nested designators, GCC struct-valued initializers */
struct Flex { int n; short v[]; };
union U { int i; float f; char c[4]; };
struct BF { int a : 3; int : 5; int b : 4; char c; };
struct Pt { int x, y; };
struct Line { struct Pt a, b; union U u; };
static struct Flex fl = { 3, 1, 2, 3 };
static union U u1 = { 5 };
static union U u2 = { { 1 } };
static struct BF bf = { 1, 2, 'z' };
static struct Line ln = { { 1, 2 }, 3, 4, { 7 } };
#pragma c99 on
static union U u3 = { .f = 1.5f };
static struct Line ln2 = { .b = { 5, 6 }, .a = { .y = 2 }, .u = { .c = { 'a', 'b' } } };
static struct Pt pts[3] = { [1] = { .y = 1, .x = 2 }, [0] = { 9 } };
#pragma c99 off
#pragma gcc_extensions on
struct Pt mk(void);
int g(struct Pt p) {
    struct Pt q = p;
    struct Line l = { p, q };
    struct Pt r = { .y = p.x };
    struct Pt s[2] = { p, { 1, 2 } };
    return q.x + l.b.y + r.y + s[0].x + fl.v[2] + u1.i + bf.b + ln.b.x + u3.c[1] + ln2.a.y + pts[1].x + u2.c[0];
}
#pragma gcc_extensions off
