/* #pragma warn_undefmacro: an undefined identifier in an evaluated #if
   operand warns; in the unevaluated side of && / || it does not. */
#pragma warn_undefmacro on
#if UNDEFINED_A
int a;
#endif
#if 0 && UNDEFINED_B
int b;
#endif
#if 1 || UNDEFINED_C
int c;
#endif
#if defined(UNDEFINED_D) || -UNDEFINED_E
int d;
#endif
#pragma warn_undefmacro off
#if UNDEFINED_F
int f;
#endif
int g;
