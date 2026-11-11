#ifndef MWCC_HOST_PROBE_STDDEF_H
#define MWCC_HOST_PROBE_STDDEF_H

#ifdef __cplusplus
extern "C" {
#endif

#if MWCCPSP
#ifdef __LP64__
typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#else
typedef unsigned int size_t;
typedef int ptrdiff_t;
#endif
#else
typedef unsigned int size_t;
typedef int ptrdiff_t;
#endif
#ifndef NULL
#define NULL 0L
#endif
#ifdef __clang__
#define offsetof(type, member) __builtin_offsetof(type, member)
#else
#define offsetof(type, member) ((size_t)&(((type*)0)->member))
#endif

#ifdef __cplusplus
}
#endif

#endif
