#include "compiler/mwcc_core.h"
#ifndef COMPILER_IROMALLOC_H
#define COMPILER_IROMALLOC_H

#include <stddef.h>
#if MWCC_CORE == MWCC_CORE_B52
#include "compiler/common.h"
#pragma pack(push, 2)
#endif
#if !MWCC_CORE || MWCC_CORE > MWCC_CORE_B52
#if MWCCPSP
#include "compiler/common.h"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
extern void* IRO_malloc(size_t size);
#if MWCC_CORE == MWCC_CORE_B52
extern void* IRO_Alloc(size_t size); // 3.0.1 0x4F8B10: oalloc, used for linear nodes
#endif
extern void IRO_free(void* buf);
#if MWCC_CORE == MWCC_CORE_B52
extern void IRO_free2(void* buf); // 3.0.1: a second copy of IRO_free
extern void IRO_InitializeAllocator(void* (*heap)(SInt32 size));
extern void IRO_TerminateAllocator(void* (*heap)(SInt32 size));
#else
extern void IRO_InitializeAllocator(void);
extern void IRO_TerminateAllocator(void);
#endif
#else
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
#endif

#ifdef __cplusplus
}
#endif

#if MWCC_CORE == MWCC_CORE_B52
#pragma pack(pop)
#endif
#if MWCCPS2 && !MWCC_CORE
extern void* IRO_gmalloc(size_t size); // mwccps2: variable pools on the global heap
#endif

#endif
