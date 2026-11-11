/* static and automatic initializers through 3.0.1's CInit engine:
   braces around scalars, nested aggregates, arrays of unknown size,
   bitfields, enums, floats, pointers, strings, C99 designators */
enum E { A, B = 5, C };
struct In { char c; short s; };
struct S { int a; float f; enum E e; struct In in[2]; unsigned bf : 3; int* p; };
static int gi = { 7 };
static float gf = 2.5f;
static enum E ge = C;
static int arr[] = { 1, 2, 3, 4 };
static int arr2[3][2] = { { 1, 2 }, { 3 }, 5, 6 };
static struct S gs = { 1, 2.0f, B, { { 'a', 3 }, { 'b' } }, 5, &gi };
static struct S gs2 = { 2, 1.5f, A, 'x', 4, 'y', 5, 6, arr };
static char str[] = "hello";
static char str2[8] = "hi";
static const int ci = 42;
static int* pp[2] = { &gi, arr + 1 };
#pragma c99 on
static int des[5] = { [2] = 7, [4] = 9, [0] = 1 };
static struct S gs3 = { .f = 3.0f, .a = 9, .in[1].s = 4 };
#pragma c99 off
int f(int x) {
    int a = x;
    int b[3] = { x, x + 1 };
    struct In in = { (char)x, 2 };
    struct S s = { x, 1.0f, C, { { 1, 2 } }, 1, &a };
    static int st = 5;
    static struct In sin = { 1, 2 };
    const int k = 3;
    float fl = { 1.25f };
    return a + b[2] + in.s + s.a + st + sin.s + k + (int)fl + gi + arr[3] + arr2[1][1] + gs.in[1].c + str[1] + ci + des[2] + gs3.in[1].s;
}
