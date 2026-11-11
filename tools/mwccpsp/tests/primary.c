/* primary expressions in C: GCC statement expressions, constants, parens */
#pragma gcc_extensions on
int g(int x) {
    int y = ({ int t = x * 2; t + 1; });
    int z = (x = 3, x + 1);
    float f = 1.5f + 2.0;
    const char* s = "abc" "def";
    return y + z + (int)f + s[4] + __builtin_align(double) + (x = 4);
}
#pragma gcc_extensions off
/* block-scope extern declarations name aliases of the globals */
int use_alias(int i) {
    extern int tbl[];
    extern short shorts[];
    extern int count;
    int* p = &tbl[i];
    return *p + shorts[i] + count;
}
