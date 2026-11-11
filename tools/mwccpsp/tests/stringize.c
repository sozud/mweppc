/* # and ## in macro bodies (3.0.1 XpandMacro: stringizing tracks literals;
   quoted text in bodies and arguments is copied by CPrep_CopyQuoted) */
#define S(x) #x
#define XS(x) S(x)
#define CAT(a, b) a ## b
#define Q "body \"quoted\" 'c' \\ end"
#define VA(fmt, ...) fmt, #__VA_ARGS__
const char* s1 = S("a\"b");
const char* s2 = S('\'');
const char* s3 = S('"');
const char* s4 = S("x\\y");
const char* s5 = S(  spaced    out  );
const char* s6 = S("lit" 'c' "lit2");
const char* s7 = XS(CAT(1, 2));
const char* s8 = Q;
const char* s9 = S(Q);
const char* s10 = XS(Q);
const char* s11[] = { VA("f", 1, "two", '3') };
int CAT(var, 1) = 1;
