/* struct member diagnostics: an empty struct (an error unless
   #pragma gcc_extensions), a member with no type specifier, a redefined
   member, an anonymous struct member, and @defs-free nesting. */
struct Empty {};

#pragma gcc_extensions on
struct AlsoEmpty {};
#pragma gcc_extensions off

struct S {
    const x;
    int a;
    int a;
    struct { int b; };
    void v;
};

typedef struct T T;
struct T { T* next; int (*fn)(void); };
