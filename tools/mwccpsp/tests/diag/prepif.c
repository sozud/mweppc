/* #if expressions: division by zero in the unevaluated side of && and ||
   is not an error; in an evaluated operand it is. Plus #elif/#else nesting. */
#if 0 && (1 / 0)
int a;
#elif 1 || (2 / 0)
int b;
#else
int c;
#endif

#ifdef __cplusplus
int d;
#elif defined(__STDC__) && (__STDC__ + 0)
int e;
#endif

#if (3 / 0)
int f;
#endif
