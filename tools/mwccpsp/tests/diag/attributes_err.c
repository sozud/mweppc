/* attribute errors: unknown section, bad mode, bad alignment, attribute on nothing */
int a __attribute__((section(".no_such_section")));
typedef int b __attribute__((mode(XF)));
typedef unsigned int c __attribute__((mode(SF)));
int d __attribute__((aligned(3)));
int e __attribute__((nothrow));
int f __attribute__((mode QI));
int g __attribute__((section 7));
