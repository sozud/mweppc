#include "driver/CLLoadAndCache.h"
#include "driver/CLIncludeFileCache.h"
#include "driver/MacFileTypes.h"
#include "driver/Memory.h"
#include "driver/MemUtils.h"
#include "driver/MsDos.h"
#include "driver/oslib.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if MWCCPSP
short FixTextHandle(OSHandle* txt) {
    char* tptr;
    UInt32 tlen;
    char* scan;

    tptr = OS_LockHandle(txt);
    OS_GetHandleSize(txt, &tlen);

    scan = tptr;
    while (scan < (tptr + tlen)) {
        if (scan[0] == '\r') {
            if (scan[1] == '\n') {
                scan += 2;
            } else {
                scan += 1;
            }
        } else if (scan[0] == '\n') {
            scan[0] = '\r';
            scan++;
        } else {
            scan++;
        }
    }

    OS_UnlockHandle(txt);
    return 0;
}

int LoadAndCacheFile(OSSpec* spec, OSHandle** texthandle, Boolean* precomp) {
    OSHandle* h;
    char* ptr;
    int err;
    OSType mactype;
    int refnum;
    UInt32 ltxtlen;

    if ((*texthandle = CachedIncludeFile(spec, precomp))) {
        return 0;
    }

    *precomp = OS_GetMacFileType(spec, &mactype) == 0 && mactype != 'TEXT';
    *texthandle = NULL;

    err = OS_Open(spec, OSReadOnly, &refnum);
    if (err) {
        return err;
    }
    err = OS_GetSize(refnum, &ltxtlen);
    if (err) {
        return err;
    }

    h = xmalloc(NULL, sizeof(OSHandle));
    if (OS_NewHandle(ltxtlen + 1, h)) {
        fprintf(stderr, "\n*** Out of memory\n");
        exit(-23);
    }

    ptr = OS_LockHandle(h);
    err = OS_Read(refnum, ptr, &ltxtlen);
    if (err) {
        OS_FreeHandle(h);
        xfree(h);
        OS_Close(refnum);
        return err;
    }

    OS_Close(refnum);
    ptr[ltxtlen] = 0;
    OS_UnlockHandle(h);

    if (!*precomp) {
        FixTextHandle(h);
    }

    CacheIncludeFile(spec, h, *precomp);
    *texthandle = h;
    return 0;
}

void CopyFileText(OSHandle* src, char** text, SInt32* textsize) {
    void* data;

    OS_GetHandleSize(src, (UInt32*)textsize);
    *text = xmalloc(NULL, *textsize);
    data = OS_LockHandle(src);
    memcpy(*text, data, *textsize);
    *textsize -= 1;
    OS_UnlockHandle(src);
}
#else
short FixTextHandle(Handle txt) {
    char* tptr;
    UInt32 tlen;
    char* scan;

    HLock(txt);
    tptr = *txt;
    tlen = GetHandleSize(txt);

    scan = tptr;
    while (scan < (tptr + tlen)) {
        if (scan[0] == '\r') {
            if (scan[1] == '\n') {
                scan += 2;
            } else {
                scan += 1;
            }
        } else if (scan[0] == '\n') {
            scan[0] = '\r';
            scan++;
        } else {
            scan++;
        }
    }

    HUnlock(txt);
    return 0;
}

int LoadAndCacheFile(OSSpec* spec, Handle* texthandle, Boolean* precomp) {
    Handle h;
    int err;
    OSType mactype;
    int refnum;
    UInt32 ltxtlen;

    if ((*texthandle = CachedIncludeFile(spec, precomp))) {
        return 0;
    }

    *precomp = OS_GetMacFileType(spec, &mactype) == 0 && mactype != 'TEXT';
    *texthandle = NULL;

    err = OS_Open(spec, OSReadOnly, &refnum);
    if (err) {
        return err;
    }
    err = OS_GetSize(refnum, &ltxtlen);
    if (err) {
        return err;
    }

    h = NewHandle(ltxtlen + 1);
    if (!h) {
        fprintf(stderr, "\n*** Out of memory\n");
        exit(-23);
    }

    HLock(h);
    err = OS_Read(refnum, *h, &ltxtlen);
    if (err) {
        DisposeHandle(h);
        OS_Close(refnum);
        return err;
    }

    OS_Close(refnum);
    (*h)[ltxtlen] = 0;
    HUnlock(h);

    if (!*precomp) {
        FixTextHandle(h);
    }

    CacheIncludeFile(spec, h, *precomp);
    *texthandle = h;
    return 0;
}

void CopyFileText(Handle src, char** text, SInt32* textsize) {
    *textsize = GetHandleSize(src);
    *text = xmalloc(NULL, *textsize);

    HLock(src);
    memcpy(*text, *src, *textsize);
    *textsize -= 1;
    HUnlock(src);
}
#endif
