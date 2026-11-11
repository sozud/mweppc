#include "driver/CLIncludeFileCache.h"
#include "driver/Memory.h"
#include "driver/MemUtils.h"
#include "driver/MsDos.h"
#include "driver/oslib.h"

#if MWCCPSP
#define DisposeHandle OS_FreeHandle // 3.0.1: the cache holds OS handles
#endif
#if MWCCPSP

#endif
typedef struct CacheEntry {
    struct CacheEntry* next;
    struct CacheEntry* prev;
    int locked;
    int precompiled;
#if MWCCPSP
    DriverHandle text;
#else
    Handle text;
#endif
    OSSpec spec;
    UInt32 size;
    UInt32 when;
} CacheEntry;

enum {
    CACHE_SIZE = 0x800000
};

static CacheEntry* cachelist;
static CacheEntry* freelist;
static UInt32 availablecache;

static CacheEntry* makecacheentry(void) {
    CacheEntry* c;

    if (freelist) {
        c = freelist;
        freelist = c->next;
    } else {
        c = xmalloc("include file cache", sizeof(CacheEntry));
    }
    return c;
}

static void insertcacheentry(CacheEntry* c) {
    if (cachelist) {
        cachelist->prev = c;
    }
    c->next = cachelist;
    c->prev = NULL;
    cachelist = c;
}

static void deletecacheentry(CacheEntry* c) {
    if (c->next) {
        c->next->prev = c->prev;
    }
    if (c->prev) {
        c->prev->next = c->next;
    } else {
        cachelist = c->next;
    }
}

void InitializeIncludeCache(void) {
    cachelist = freelist = NULL;
    availablecache = CACHE_SIZE;
}

void CleanupIncludeCache(void) {
    CacheEntry* c;
    CacheEntry* c1;

    for (c = cachelist; c; c = c1) {
        c1 = c->next;
        if (c->text) {
            DisposeHandle(c->text);
        }
        xfree(c);
    }

    for (c = freelist; c; c = c1) {
        c1 = c->next;
        xfree(c);
    }

    InitializeIncludeCache();
}

#if MWCCPSP
void CacheIncludeFile(OSSpec* spec, DriverHandle text, Boolean precompiled) {
#else
void CacheIncludeFile(OSSpec* spec, Handle text, Boolean precompiled) {
#endif
#if MWCCPSP
    UInt32 size;
#else
    SInt32 size;
#endif
    CacheEntry* c;
    CacheEntry* lru;

#if MWCCPSP
    OS_GetHandleSize(text, &size);
#else
    size = GetHandleSize(text);
#endif
    if (precompiled) {
        lru = NULL;
        for (c = cachelist; c; c = c->next) {
            if (c->precompiled) {
                lru = c;
                break;
            }
        }
        if (lru) {
            DisposeHandle(lru->text);
            lru->text = NULL;
            deletecacheentry(lru);
            lru->next = freelist;
            freelist = lru;
        }
    } else {
#if MWCCPSP
        if (size > CACHE_SIZE) {
#else
        if ((SInt32)size > (SInt32)CACHE_SIZE) {
#endif
            return;
        } else {
            while ((UInt32)size > availablecache) {
                lru = NULL;
                for (c = cachelist; c; c = c->next) {
                    if (!c->locked && !c->precompiled && (lru == NULL || c->when < lru->when)) {
                        lru = c;
                    }
                }

                if (lru) {
                    DisposeHandle(lru->text);
                    lru->text = NULL;
                    availablecache += lru->size;
                    deletecacheentry(lru);
                    lru->next = freelist;
                    freelist = lru;
                } else {
                    return;
                }
            }
        }
    }

    c = makecacheentry();
    c->locked = 1;
    c->precompiled = precompiled;
    c->spec = *spec;
    c->text = text;
    c->size = size;
    c->when = OS_GetMilliseconds();
    insertcacheentry(c);

    if (!precompiled) {
        availablecache -= size;
    }
}

#if MWCCPSP
DriverHandle CachedIncludeFile(OSSpec* spec, Boolean* precompiled) {
#else
Handle CachedIncludeFile(OSSpec* spec, Boolean* precompiled) {
#endif
    CacheEntry* c;

    for (c = cachelist; c; c = c->next) {
        if (OS_EqualSpec(&c->spec, spec)) {
            c->when = OS_GetMilliseconds();
            *precompiled = c->precompiled;
            c->locked++;
            return c->text;
        }
    }

    return NULL;
}

#if MWCCPSP
void FreeIncludeFile(DriverHandle text) {
#else
void FreeIncludeFile(Handle text) {
#endif
    CacheEntry* c;

    for (c = cachelist; c; c = c->next) {
        if (c->text == text) {
            c->locked--;
            return;
        }
    }

    DisposeHandle(text);
}
