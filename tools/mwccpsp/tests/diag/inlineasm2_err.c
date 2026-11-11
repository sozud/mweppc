/* inline assembly operand errors */
void bad_half(void) {
    __asm__("vfim.s S000, 1.0e10\n");
}

void bad_label(int n) {
    asm {
        bnez n, 3 b
        nop
    }
}

void bad_const(void) {
    __asm__("vcst.s S000, VFPU_NOPE\n");
}

void bad_class(void) {
    asm {
        add.s $f0, $8, $f1
    }
}

void bad_prefix(void) {
    __asm__("vpfxd 0, 1, x, w\n"
            "vpfxs x, y, z\n"
            "vpfxt |x, y, z, w\n");
}

void bad_vrot(void) {
    __asm__("vrot.p C000, S010, [c, c]\n"
            "vrot.t C000, S010, [s, s, s, s]\n"
            "vrot.q C000, S010, [c, s, x, s]\n"
            "vrot.q C000, S010, [0, 0, 0, c]\n");
}

void bad_vfpu_regs(void) {
    __asm__("vavg.t S001, R020\n");
}

void bad_vfpu_regs2(void) {
    __asm__("vdot.t S000, R010, R020\n");
}

void bad_vfpu_regs3(void) {
    __asm__("vadd.q C001, C010, C020\n"
            "vmmul.q M011, M100, M200\n");
}
