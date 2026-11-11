#ifndef COMPILER_IROMALLOC_H
#define COMPILER_IROMALLOC_H

#include <stddef.h>
#if MWCCPSP
#include "compiler/common.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern void* IRO_malloc(size_t size);
#if MWCCPSP
extern void* IRO_Alloc(size_t size); // 3.0.1 0x4F8B10: oalloc, used for linear nodes
#endif
extern void IRO_free(void* buf);
#if MWCCPSP
extern void IRO_free2(void* buf); // 3.0.1: a second copy of IRO_free
#endif
#if MWCCPSP
extern void IRO_InitializeAllocator(void* (*heap)(SInt32 size));
extern void IRO_TerminateAllocator(void* (*heap)(SInt32 size));
#else
extern void IRO_InitializeAllocator(void);
extern void IRO_TerminateAllocator(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
