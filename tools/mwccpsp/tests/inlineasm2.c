/* inline assembly operand forms: local labels, %hi/%lo, VFPU immediates and
   constants, condition codes, struct and array memory operands */
struct S {
    int a;
    int b[3];
};
struct S g_s;
int g_arr[4];

int asm_hilo(void) {
    int r;
    __asm__("lui %0, %hi(0x12345678)\n"
            "ori %0, %0, %lo(0x12345678)\n"
            : "=r"(r));
    return r;
}

void asm_vfpu_imm(void) {
    __asm__(".set push\n"
            ".set noreorder\n"
            "vfim.s S000, 1.5\n"
            "vfim.s S001, -0.25\n"
            "vcst.s S002, VFPU_PI\n"
            "vcst.s S003, vfpu_sqrt2\n"
            "vcmp.s EQ, S000, S001\n"
            "vcmp.s lt, S002, S003\n"
            "vmov.s S010, S000\n"
            ".set pop\n");
}

void asm_structmem(void) {
    asm {
        lw $8, g_s.b
        lw $9, g_arr+4
        sw $9, g_s
        sw $8, g_arr-0
    }
}

void asm_float_imm(void) {
    asm {
        li.s $f0, 1.5
        li.s $f1, -2.0
    }
}
void asm_labels(int n) {
    asm {
    1:  addiu n, n, -1
        bnez n, 1 b
        nop
        b 2 f
        nop
    2:
    }
}

void asm_vfpu_prefix(void) {
    __asm__(".set push\n"
            ".set noreorder\n"
            "vpfxs -x, |y|, 1/2, 3\n"
            "vpfxt w, z, -|x|, 1/6\n"
            "vpfxd [0:1], y, m, [-1:1]\n"
            "vadd.q C000, C010, C020\n"
            "vpfxs 0x12345\n"
            "vpfxd 0x123\n"
            "vpfxs 0, 1, 2, 1/3\n"
            "vpfxt 1/4, -0, x, y\n"
            ".set pop\n");
}

void asm_vfpu_vrot(void) {
    __asm__(".set push\n"
            ".set noreorder\n"
            "vrot.p C000, S010, [c, s]\n"
            "vrot.p C000, S010, [s, c]\n"
            "vrot.p C000, S010, [-s, c]\n"
            "vrot.p C000, S010, [c, 0]\n"
            "vrot.t C000, S010, [c, s, 0]\n"
            "vrot.t C000, S010, [s, s, c]\n"
            "vrot.t C000, S010, [0, -s, c]\n"
            "vrot.q C000, S010, [c, s, s, s]\n"
            "vrot.q C000, S010, [s, c, 0, 0]\n"
            "vrot.q C000, S010, [-s, -s, c, -s]\n"
            "vrot.q C000, S010, 5\n"
            ".set pop\n");
}

void asm_vfpu_matrix(void) {
    __asm__(".set push\n"
            ".set noreorder\n"
            "vmidt.q M000\n"
            "vmidt.t M100\n"
            "vmidt.p M200\n"
            "vmmov.q M300, E000\n"
            "vmmov.t M011, E111\n"
            "vmmul.q M000, M100, E200\n"
            "vmmul.t M100, E200, M300\n"
            "vmmul.p M022, M122, E222\n"
            "vtfm4.q C000, M100, C200\n"
            "vtfm3.t C000, E100, C200\n"
            "vtfm2.p C000, M100, C200\n"
            "vhtfm4.q C000, M100, C200\n"
            "vdot.q S000, C010, C020\n"
            "vdot.p S000, C012, R020\n"
            "vcrs.t C000, C010, C020\n"
            "vscl.q C000, C010, S020\n"
            "vscl.t R000, R010, S021\n"
            "vadd.p C002, C012, C022\n"
            "vadd.t R101, R111, R100\n"
            "vmscl.q M000, M100, S200\n"
            "vmzero.q E300\n"
            "vmone.t M200\n"
            "vfad.q S000, C010\n"
            "vsgn.p C030, C020\n"
            "lv.q C000, 0($4)\n"
            "sv.q R100, 16($4)\n"
            "mtv $2, S000\n"
            "mfv $3, S733\n"
            ".set pop\n");
}
