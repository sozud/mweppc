/* __attribute__ forms: aligned, packed, section, mode, weak, export kinds, unknown */
struct Vec { float x, y, z; } __attribute__((aligned(16)));
struct Packed { char c; int i; } __attribute__((packed));
typedef int i8 __attribute__((mode(QI)));
typedef unsigned int u16 __attribute__((__mode__(__HI__)));
typedef int i64 __attribute__((mode(DI)));
typedef float f32 __attribute__((mode(SF)));

int g_aligned __attribute__((aligned(64))) = 1;
int* g_alignedptr __attribute__((aligned(8)));
int g_section __attribute__((section(".sdata"))) = 3;
int g_text_data __attribute__((section("data"))) = 4;
int g_weak __attribute__((weak)) = 5;
int g_unused __attribute__((unused, noreturn));
int g_unknown __attribute__((some_unknown_thing(1, (2), 3))) = 6;
int g_internal __attribute__((internal)) = 7;
void f_nothrow(void) __attribute__((nothrow));
int g_const __attribute__((const));

struct Vec g_vec;
struct Packed g_packed;
i8 g_i8 = -1;
u16 g_u16 = 2;
i64 g_i64 = 3;
f32 g_f32 = 4.0f;

int weak_func(int a) __attribute__((weak));
int weak_func(int a) { return a + g_section + g_weak; }

int use(void) {
    return sizeof(struct Vec) + sizeof(struct Packed) + sizeof(i8) + sizeof(u16) + sizeof(i64) + g_i8 + g_u16 + (int)g_i64 +
           (int)g_f32 + g_aligned + g_text_data + g_unknown + g_internal + weak_func(g_packed.i) + (int)g_vec.x;
}
