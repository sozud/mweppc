/* calls in C: prototypes, old-style functions, varargs, function pointers */
int proto(int a, short b, float c);
int old();
int va(const char* fmt, ...);
int (*fp)(int, double);
int g(char c, float f, int (*lp)(long)) {
    return proto(c, f, c) + old(c, f) + va("x", c, f, (short)1) + fp(c, c) + lp(f) + (*fp)(1, 2) + (*lp)(3);
}
