/* intrinsic misuse: non-constant or out-of-range operands */
int bad(int a, int b) {
    int r = __builtin_allegrex_ext(a, b, 3);
    r += __builtin_allegrex_ext(a, 40, 3);
    r += __builtin_allegrex_ins(a, b, 2, 33);
    __I_addu(2, a, 5);
    __I_sll(9, 8, b);
    return r;
}
