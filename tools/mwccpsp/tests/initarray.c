/* array initialization: unsized arrays growing past 16 elements, strings,
   wide strings, designators and GCC ranges, multi-dimensional arrays */
typedef unsigned short wchar;
static int big[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                     21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35 };
static short sbig[][3] = { { 1 }, { 2 }, { 3 }, { 4 }, { 5 }, { 6 }, { 7 }, { 8 }, { 9 },
                           { 10 }, { 11 }, { 12 }, { 13 }, { 14 }, { 15 }, { 16 }, { 17 }, 18, 19 };
static char s1[] = "abc";
static char s2[10] = "abc";
static char s3[3] = "abc";
static char s4[] = { "xyz" };
static char s5[2][4] = { "ab", "cde" };
static unsigned char s6[] = "u";
static const char* sp[] = { "p", "q", 0 };
static int m[2][3][2] = { 1, 2, 3, 4, 5, 6, { { 7 }, 8, { 9 } } };
static struct { int k; char n[4]; } recs[] = { { 1, "a" }, 2, "bc", { 3 } };
#pragma c99 on
static int d1[] = { [3] = 1, 2, [1] = 7 };
static int d2[6] = { 1, [4] = 4, 5 };
static int d3[] = { [20] = 1 };
#pragma c99 off
#pragma gcc_extensions on
static int r1[8] = { [1 ... 3] = 5, [6] = 1 };
static int r2[] = { [0 ... 2] = 9, [5 ... 7] = 2 };
static char r3[4][2] = { [1] = { 1, 2 }, [2][1] = 3 };
#pragma gcc_extensions off
int f(int x) {
    int a[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 };
    char b[] = "local";
    int c[3][2] = { { 1 }, { 2, 3 } };
    return a[18] + b[2] + c[1][1] + big[34] + sbig[17][0] + s1[1] + s2[9] + s3[2] + s4[0] + s5[1][2] + s6[0] +
           sp[1][0] + m[1][1][0] + recs[1].n[1] + d1[4] + d2[5] + d3[20] + r1[3] + r2[7] + r3[2][1] + sizeof(big) + sizeof(sbig) + sizeof(d1) + sizeof(d3) + sizeof(r2) + sizeof(recs);
}
