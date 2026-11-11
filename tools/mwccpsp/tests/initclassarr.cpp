// arrays of classes: default construction (short arrays unrolled, long ones
// through __construct_array or a loop), braced lists, nested and unsized
// arrays, static arrays with destructors (__arraydtor), static locals
struct C { int v; C(); C(int); ~C(); };
struct N { int v; N(); N(int); };          // no destructor
struct T { int v; ~T(); };                 // destructor only
extern int ext();
static C g1[1];
static C g2[5];
static N g3[12];
static C g4[] = { 1, 2, 3 };
static C g5[2][2] = { { 1, 2 }, { 3 } };
static N g6[3] = { 7 };
static T g7[4];
static C g8[2][3];
int f(int x) {
    C a1[2];
    C a2[9];
    N a3[20];
    N a4[3];
    C a5[] = { x, x + 1 };
    C a6[2][2] = { { x }, { 1, 2 } };
    N a7[4] = { x, 2 };
    T a8[10];
    C a9[3][4];
    static C s1[2];
    static C s2[10];
    static N s3[30];
    static C s4[] = { ext(), 2 };
    static T s5[3];
    return a1[1].v + a2[8].v + a3[19].v + a4[2].v + a5[1].v + a6[1][1].v + a7[3].v + a8[9].v + a9[2][3].v +
           s1[1].v + s2[9].v + s3[29].v + s4[1].v + s5[2].v + g1[0].v + g2[4].v + g3[11].v + g4[2].v + g5[1][1].v +
           g6[2].v + g7[3].v + g8[1][2].v + sizeof(g4) + sizeof(a5) + sizeof(s4);
}
#pragma no_static_dtors on
static C h1[6];
int g() { static C hs[7]; return h1[5].v + hs[6].v; }
