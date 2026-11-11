/* #define: quoted text in a body (copied by CPrep_CopyQuoted in 3.0.1),
   a lone # in an object-like macro under ANSI_strict (only an error in
   function-like macros in 3.0.1), __VA_ARGS__, every redefinition branch
   (gcc_extensions, cpp_extensions with and without extended_errorcheck),
   argument list errors */
#define STR "a \"quoted\" string with 'x' and \\ backslash"
#define CHR '\''
#define HASH a # b
#pragma ANSI_strict on
#define HASH2 x # y
#define FHASH(a) # a
#define FBAD(a) # b
#pragma ANSI_strict off
#define VA(fmt, ...) f(fmt, __VA_ARGS__)
#define VA2(...) g(__VA_ARGS__)
#define RED 1
#define RED 2
#pragma gcc_extensions on
#define RED 3
#pragma gcc_extensions off
#pragma cpp_extensions on
#define RED 4
#pragma extended_errorcheck on
#define RED 5
#pragma extended_errorcheck off
#pragma cpp_extensions off
#define ARGS(a, b) a + b
#define ARGS(a, c) a + c
#define ARGS(a) a
#define BADARG(__VA_ARGS__) 1
#define DUP(a, a) 1
#define PASTE(a, b) a ## b
#define PASTE2(a) # ## a
#define EMPTY
#define EMPTY 1
int f(const char*, ...);
int g(int, ...);
const char* s = STR;
char c = CHR;
int v = VA2(1, 2);
int p = PASTE(1, 2);
