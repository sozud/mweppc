// float and double: arithmetic, compares, conversions both ways (signed and
// unsigned), unary minus, ++/--, ?:.
float fadd(float a, float b) { return a + b - a * b / (b + 1.0f); }
double dadd(double a, double b) { return a + b - a * b / (b + 1.0); }
int fcmp(float a, float b) { return (a < b) + (a <= b) * 2 + (a == b) * 4 + (a != b) * 8 + (a > b) * 16 + (a >= b) * 32; }
int dcmp(double a, double b) { return (a < b) + (a <= b) * 2 + (a == b) * 4 + (a != b) * 8 + (a > b) * 16 + (a >= b) * 32; }
float fneg(float a) { return -a; }
double dneg(double a) { return -a; }
int f2i(float a) { return (int)a; }
unsigned f2u(float a) { return (unsigned)a; }
int d2i(double a) { return (int)a; }
unsigned d2u(double a) { return (unsigned)a; }
long long f2ll(float a) { return (long long)a; }
unsigned long long d2ull(double a) { return (unsigned long long)a; }
float i2f(int i) { return (float)i; }
float u2f(unsigned u) { return (float)u; }
double i2d(int i) { return i; }
double u2d(unsigned u) { return u; }
float ll2f(long long a) { return a; }
double ull2d(unsigned long long a) { return a; }
double f2d(float a) { return a; }
float d2f(double a) { return a; }
float finc(float* p) { (*p)++; ++*p; return (*p)-- + --*p; }
double dinc(double* p) { (*p)++; return --*p; }
float fcond(int c, float a, float b) { return c ? a : b; }
int ftest(float a) { if (a) return 1; return !a ? 2 : 3; }
float fconst(void) { return 1.5f + 0.0f; }
double dconst(void) { return 3.25; }
float fasg(float* p, float b) { *p += b; *p -= 2.0f; *p *= b; *p /= 3.0f; return *p; }
// comparisons used as values (func_00491E40)
int fcmpv(float a, float b) { int r = a < b; int s = a == b; return r * 2 + s + (a >= b); }
int dcmpv(double a, double b) { int r = a < b; return r + (a != b) * 4; }
// expandIR rewrites (func_004A2810): sqrt intrinsics, x / sqrt(y) under
// fastmath, integral float constants
extern double sqrt(double);
extern float sqrtf(float);
float rsq(float a, float b) { return (float)(a / sqrt(b)); }
float sq(float x) { return sqrtf(x) + (float)sqrt(x); }
double dconsts(double x) { return x * 3.0 + 0.5 - 1e10 + 65536.0; }
