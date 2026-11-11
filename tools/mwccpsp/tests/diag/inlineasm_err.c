/* inline asm errors: unknown mnemonic, bad register, bad operand */
void bad(int a) {
    __asm__("frobnicate $4, $5\n" : : "r"(a));
    __asm__("addu $4, $99, $5\n");
    __asm__("lw $4, nothere($5)\n");
    __asm__("addiu $4, $5\n");
}
