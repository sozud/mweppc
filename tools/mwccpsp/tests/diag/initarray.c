/* array initializer diagnostics */
static int a[2] = { 1, 2, 3 };
static char s[2] = "abcdef";
static int b[2][2] = { { 1, 2, 3 } };
static char c[] = { "x", "y" };
static int n[] = { };
#pragma c99 on
static int d[3] = { [5] = 1 };
static int e[3] = { [-1] = 1 };
static int g[3] = { .x = 1 };
#pragma c99 off
#pragma gcc_extensions on
static int h[4] = { [3 ... 1] = 1 };
static int k[4] = { [1 ... 9] = 1 };
static char r3[4][2] = { [1 ... 2] = { 1, 2 } };
static int big[] = { [0 ... 40] = 3, 4 };
#pragma gcc_extensions off
static int m[2][3][2] = { 1, 2, 3, 4, 5, 6, { 7, 8 }, { 9 } };
void f(int x) { int a[] = { x, x }; }
static int m2[2][3][2] = { 1, 2, 3, 4, 5, 6, { 7 }, { 9 }, 10 };
