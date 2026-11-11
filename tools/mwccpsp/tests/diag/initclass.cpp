// aggregate class initializer diagnostics
struct Priv { int a; private: int b; };
struct Base { int b; };
struct Der : Base { int d; };
struct Virt { int v; virtual void m(); };
struct P { int x, y; };
struct F { int n; int v[]; };
struct G { F f; };
static Priv p = { 1, 2 };
static Der d = { 1, 2 };
static Virt v = { 1 };
static P q = { 1, 2, 3 };
static P r = { { 1, 2 } };
static F f = { 1, { 2 }, 3 };
static G g = { 1, 2 };
#pragma gcc_extensions on
static P s = { .z = 1 };
static P t = { [0] = 1 };
struct N { P p; int z; };
static N gnd = { .z = 9, .p.y = 3 };
#pragma gcc_extensions off
