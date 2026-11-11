/* inline assembly: GCC-style statements with operands, VFPU and integer
   instructions, directives, an asm block and an asm function */
float g_x, g_y;
int g_buf[8];

void vfpu_load(void) {
    float x = g_x;
    float y = g_y;
    __asm__(".set push\n"
            ".set noreorder\n"
            "lv.s s410, 0(%0)\n"
            "lv.s s411, 0(%1)\n"
            "vadd.s s412, s410, s411\n"
            "vone.s s402\n"
            "vzero.s S400\n"
            "sv.s s412, 0(%0)\n"
            ".set pop\n"
            :
            : "m"(x), "m"(y));
    g_x = x;
}

int gcc_regs(int a, int b) {
    int r;
    __asm__ volatile("addu %0, %1, %2\n"
                     "sll %0, %0, 2\n"
                     : "=r"(r)
                     : "r"(a), "r"(b));
    return r;
}

void gcc_mem(int* p) {
    __asm__("lw $8, 0(%0)\n"
            "addiu $8, $8, 1\n"
            "sw $8, 4(%0)\n"
            :
            : "r"(p)
            : "$8", "memory");
}

int cw_block(int a) {
    int r;
    asm {
        addu r, a, a
        nop
    }
    return r;
}

asm void cw_function(void) {
    addiu $2, $0, 5
    jr $31
    nop
}
