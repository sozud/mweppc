/* incomplete and illegal types (3.0.1 names the type) */
void v;
struct S;
struct S s;
int f(void);
int g(void) { return sizeof(f); }
