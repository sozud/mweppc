/* illegal operand types: 3.0.1 names both types and the operator */
struct S { int a; } s;
const struct S cs = { 1 };
int f1(void) { return s % 3; }
int f2(void) { return 4 & s; }
int f3(void) { return cs ^ 1; }
int f4(void) { return s | cs; }
int f5(void) { return s * 2; }
int f6(void) { return 2 - s; }
int f7(void) { return s && 1; }
int f8(void) { return 0 || cs; }
