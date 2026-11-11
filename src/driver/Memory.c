#include "driver/Memory.h"
#include "driver/Generic.h"
#include "driver/MsDos.h"
#include "driver/oslib.h"
#include <stdio.h>
#include <stdlib.h>

static OSErr memError;

#if MWCCPSP
// 3.0.1: every Mac handle carries a magic word; the handle calls reject
// anything else with memAZErr
typedef struct HandleStructure {
    char* addr;
    UInt32 magic;
    OSHandle hand;
} HandleStructure;

#define MAC_HANDLE_MAGIC 0xFEE1600D
#define memAZErr -113
#define HS(h) ((HandleStructure*)(h))
#define CHECK_MAC_HANDLE(h, ret) \
    if ((h) && HS(h)->magic != MAC_HANDLE_MAGIC) { \
        memError = memAZErr; \
        return ret; \
    }

OSHandle* OS_PeekMacHandle(Handle h) {
    return &HS(h)->hand;
}

// the Mac handle that owns an OSHandle, or NULL (name not known; it steps
// back by the offset of the magic word, not of the OSHandle)
Handle OS_MacHandleFromOSHandle(OSHandle* oh) {
    HandleStructure* hs = (HandleStructure*)((char*)oh - 4);
    if (hs->magic == MAC_HANDLE_MAGIC) {
        return (Handle)hs;
    }
    return NULL;
}

static void FixHandlePtr(Handle h, void* ptr) {
    HS(h)->addr = ptr;
}

Handle OS_CreateMacHandle(OSHandle* h) {
    HandleStructure* hs = malloc(sizeof(HandleStructure));
    if (!hs) {
        memError = memFullErr;
        return 0;
    }

    hs->hand = *h;
    OS_InvalidateHandle(h);
    hs->addr = OS_LockHandle(&hs->hand);
    OS_UnlockHandle(&hs->hand);
    hs->magic = MAC_HANDLE_MAGIC;
    return (Handle)hs;
}

// a Mac handle holding a copy of an OSHandle (name not known)
Handle OS_CreateMacHandleDup(OSHandle* h) {
    HandleStructure* hs = malloc(sizeof(HandleStructure));
    if (!hs) {
        memError = memFullErr;
        return 0;
    }

    OS_CopyHandle(h, &hs->hand);
    hs->addr = OS_LockHandle(&hs->hand);
    OS_UnlockHandle(&hs->hand);
    hs->magic = MAC_HANDLE_MAGIC;
    return (Handle)hs;
}
#else
typedef struct HandleStructure {
    char* addr;
    OSHandle hand;
} HandleStructure;

#define HS(h) ((HandleStructure*)(h))

OSHandle* OS_PeekMacHandle(Handle h) {
    return &HS(h)->hand;
}

static void FixHandlePtr(Handle h, void* ptr) {
    HS(h)->addr = ptr;
}

Handle OS_CreateMacHandle(OSHandle* h) {
    HandleStructure* hs = malloc(sizeof(HandleStructure));
    if (!hs) {
        memError = memFullErr;
        return 0;
    }

    hs->hand = *h;
    OS_InvalidateHandle(h);
    hs->addr = OS_LockHandle(&hs->hand);
    OS_UnlockHandle(&hs->hand);
    return (Handle)hs;
}
#endif

void OS_DestroyMacHandle(Handle h, OSHandle* ret) {
    *ret = *OS_PeekMacHandle(h);
    free(h);
}

OSErr MemError(void) {
    return memError;
}

#if MWCCPSP
Handle __stdcall NewHandle(SInt32 dataSize) {
#else
Handle NewHandle(SInt32 dataSize) {
#endif
    int err;
    OSHandle oh;

    err = OS_NewHandle(dataSize, &oh);
    if (err) {
        memError = OS_OSErrorToMacError(err);
        return 0;
    }

    return OS_CreateMacHandle(&oh);
}

Handle TempNewHandle(UInt32 dataSize, OSErr* resultCode) {
    Handle hdl;

    *resultCode = 0;
    hdl = NewHandle(dataSize);
    if (!hdl) {
        *resultCode = memError;
    }
    return hdl;
}

void __stdcall DisposeHandle(Handle h) {
#if MWCCPSP
    CHECK_MAC_HANDLE(h, )
#endif
    if (h) {
        memError = OS_OSErrorToMacError(OS_FreeHandle(&HS(h)->hand));
        FixHandlePtr(h, 0);
    } else {
        memError = nilHandleErr;
    }
}

void __stdcall HLock(Handle h) {
    void* buffer;

#if MWCCPSP
    CHECK_MAC_HANDLE(h, )
#endif
    if (h) {
        buffer = OS_LockHandle(&HS(h)->hand);
        FixHandlePtr(h, buffer);
        memError = 0;
    } else {
        memError = nilHandleErr;
    }
}

void __stdcall HLockHi(Handle h) {
    HLock(h);
}

void __stdcall HUnlock(Handle h) {
#if MWCCPSP
    CHECK_MAC_HANDLE(h, )
#endif
    if (h) {
        memError = 0;
        OS_UnlockHandle(&HS(h)->hand);
    } else {
        memError = nilHandleErr;
    }
}

SInt32 __stdcall GetHandleSize(Handle h) {
    UInt32 sz;

#if MWCCPSP
    CHECK_MAC_HANDLE(h, 0)
#endif
    if (h) {
        memError = OS_OSErrorToMacError(OS_GetHandleSize(&HS(h)->hand, &sz));
    } else {
        sz = 0;
        memError = nilHandleErr;
    }

    return sz;
}

void __stdcall SetHandleSize(Handle h, SInt32 newSize) {
    OSHandle* oh;

#if MWCCPSP
    CHECK_MAC_HANDLE(h, )
#endif
    memError = OS_OSErrorToMacError(OS_ResizeHandle(oh = &HS(h)->hand, newSize));
    FixHandlePtr(h, OS_LockHandle(oh));
    OS_UnlockHandle(oh);
}

OSErr __stdcall PtrAndHand(const void* srcPtr, Handle destHandle, SInt32 size) {
    OSErr err;
    OSHandle* oh;

#if MWCCPSP
    CHECK_MAC_HANDLE(destHandle, memAZErr)
#endif
    err = OS_OSErrorToMacError(OS_AppendHandle(oh = &HS(destHandle)->hand, srcPtr, size));
    FixHandlePtr(destHandle, OS_LockHandle(oh));
    OS_UnlockHandle(oh);

    return err;
}

OSErr __stdcall HandAndHand(Handle src, Handle dst) {
    OSErr err;
    SInt32 size;

    HLock(src);
    size = GetHandleSize(src);
    err = PtrAndHand(*src, dst, size);
    HUnlock(src);
    return err;
}

void DebugStr(ConstStringPtr str) {
    fprintf(stderr, "%*.*s", str[0], str[0], &str[1]);
}
