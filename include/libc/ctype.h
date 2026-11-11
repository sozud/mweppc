#ifndef MWCC_HOST_PROBE_CTYPE_H
#define MWCC_HOST_PROBE_CTYPE_H

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned char __ctype_map[];
extern unsigned char __lower_map[];
int isalpha(int c);
int isprint(int c);
int toupper(int c);

#ifdef __MWERKS__
#define MSL_CTYPE_INLINE inline
#else
#define MSL_CTYPE_INLINE static inline
#endif
MSL_CTYPE_INLINE int isdigit(int c) {
    return __ctype_map[(int)(unsigned char)(c)] & 0x10;
}
MSL_CTYPE_INLINE int isspace(int c) {
    return __ctype_map[(int)(unsigned char)(c)] & 0x06;
}

#ifndef _MSL_USE_INLINE
#define _MSL_USE_INLINE 1
#endif
#if MWCCPSP
#if MWCCPSP && defined(__MWERKS__) && !MWCCPSP_DRIVER // the driver links the older MSL
// mwccpsp 3.0.1's MSL: the locale's lower-case map, from thread-local data
// (locale at +0x1BC, map at +0x10), for 0..255 only
struct _ThreadLocalData;
extern struct _ThreadLocalData* _GetThreadLocalData(int create);
MSL_CTYPE_INLINE int tolower(int c) {
    return (c >= 0 && c < 256)
        ? (*(unsigned char**)(*(char**)((char*)_GetThreadLocalData(1) + 0x1BC) + 0x10))[c]
        : c;
}
#elif _MSL_USE_INLINE
MSL_CTYPE_INLINE int tolower(int c) {
    return ((c == -1) ? -1 : ((int)__lower_map[(int)(unsigned char)(c)]));
}
#else
int tolower(int c);
#endif
#else
#if _MSL_USE_INLINE
MSL_CTYPE_INLINE int tolower(int c) {
    return ((c == -1) ? -1 : ((int)__lower_map[(int)(unsigned char)(c)]));
}
#else
int tolower(int c);
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
