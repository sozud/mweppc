/* builtin functions and opcode intrinsics (Intrinsics.c's call generator) */
int g_i;
long g_l;
float g_f;
double g_d;
char g_buf[64];
char g_src[64];

int allegrex(int a, int b, long c) {
    int r = 0;
    r += __builtin_allegrex_clz(a);
    r += __builtin_allegrex_clo(b);
    r += __builtin_allegrex_ctz(a);
    r += __builtin_allegrex_cto(c);
    r += __builtin_allegrex_max(a, b);
    r += __builtin_allegrex_min(a, 7);
    r += __builtin_allegrex_seb(a);
    r += __builtin_allegrex_seh(b);
    r += __builtin_allegrex_wsbh(a);
    r += __builtin_allegrex_wsbw(b);
    r += __builtin_allegrex_bitrev(c);
    r += __builtin_allegrex_ext(a, 3, 5);
    r += __builtin_allegrex_ins(a, b, 4, 8);
    r += __builtin_allegrex_rotr(a, 5);
    r += __builtin_allegrex_rotr(a, b);
    r += __builtin_allegrex_rotl(b, 9);
    r += __builtin_allegrex_rotl(b, a);
    return r;
}

float fp(float x, double y) {
    float r = 0;
    r += __builtin_allegrex_ceil_w_s(x);
    r += __builtin_allegrex_floor_w_s(x);
    r += __builtin_allegrex_round_w_s(x);
    r += __builtin_allegrex_trunc_w_s(x);
    r += __builtin_allegrex_sqrt_s(x);
    r += __s_abs(x);
    r += (float)__d_abs(y);
    return r;
}

int misc(int a, long b, int n) {
    int r = __abs(a) + (int)__labs(b) + __abs(-5) + (int)__labs(7);
    r += __mulhi(a, (long)n) + __mulhiu(b, 3);
    __memcpy(g_buf, g_src, 16);
    __memcpy(g_buf, g_src, n);
    __clear(g_buf, 32);
    __clear(g_buf, 1);
    __clear(g_buf, n);
    return r;
}

void opcodes(void) {
    __I_nop();
    __I_addu(2, 4, 5);
    __I_ori(3, 0, 0x1234);
    __I_lui(8, 0x8000);
    __I_sll(9, 8, 4);
}
