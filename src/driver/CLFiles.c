#include "driver/CLFiles.h"
#include "driver/CLLoadAndCache.h"
#if MWCCPSP
#include "driver/Generic.h"
#endif
#include "driver/cmdline.h"
#include "driver/Memory.h"
#include "driver/MemUtils.h"
#include "driver/MsDos.h"
#include "driver/oslib.h"
#include <stddef.h>
#include <string.h>

File* File_New(void) {
    File* file;
    file = xmalloc(NULL, sizeof(File));
    if (!file) {
        return NULL;
    } else {
        memset(file, 0, sizeof(File));
        return file;
    }
}
#if MWCCPSP

#endif
#if MWCCPSP
#define DisposeHandle OS_FreeHandle // 3.0.1: File and VFile data are OS handles
#endif

void File_Free(File* file) {
    if (file) {
        if (file->textdata) {
            DisposeHandle(file->textdata);
            file->textdata = NULL;
        }
        if (file->objectdata) {
            DisposeHandle(file->objectdata);
            file->objectdata = NULL;
        }
        if (file->browsedata) {
            DisposeHandle(file->browsedata);
            file->browsedata = NULL;
        }
        xfree(file);
    }
}

Boolean Files_Initialize(Files* this) {
#if MWCCPSP
    OS_ASSERT(52, this != NULL);
#else
    OS_ASSERT(47, this != NULL);
#endif
    return TRUE;
}

Boolean Files_Terminate(Files* this) {
    File* file;
    File* next;

#if MWCCPSP
    OS_ASSERT(61, this != NULL);
#else
    OS_ASSERT(56, this != NULL);
#endif

    for (file = this->fileList; file; file = next) {
        next = file->next;
        File_Free(file);
    }

    return TRUE;
}

Boolean Files_AddFile(Files* this, File* file) {
    return Files_InsertFile(this, file, this->fileCount);
}

Boolean Files_InsertFile(Files* this, File* file, SInt32 position) {
    File** scan;

#if MWCCPSP
    OS_ASSERT(85, this != NULL);
    OS_ASSERT(86, file != NULL);
#else
    OS_ASSERT(80, this != NULL);
    OS_ASSERT(81, file != NULL);
#endif

    if (position < 0) {
        position = 0;
    } else if (position > this->fileCount) {
        position = this->fileCount;
    }

    scan = &this->fileList;
    while (position-- > 0) {
        scan = &(*scan)->next;
    }

    file->filenum = this->fileCount++;
    file->next = *scan;
    *scan = file;
    return TRUE;
}

File* Files_GetFile(Files* this, SInt32 filenum) {
    File* file;

#if MWCCPSP
    OS_ASSERT(109, this != NULL);
    OS_ASSERT(110, filenum >= 0);
#else
    OS_ASSERT(104, this != NULL);
    OS_ASSERT(105, filenum >= 0);
#endif

    file = this->fileList;
    while (file && file->filenum != filenum) {
        file = file->next;
    }
    return file;
}

File* Files_FindFile(Files* this, OSSpec* spec) {
    File* file;

    file = this->fileList;
    while (file && !OS_EqualSpec(&file->srcfss, spec)) {
        file = file->next;
    }
    return file;
}

int Files_Count(Files* this) {
#if MWCCPSP
    OS_ASSERT(132, this != NULL);
#else
    OS_ASSERT(127, this != NULL);
#endif

    return this->fileCount;
}

Boolean VFiles_Initialize(VFile** list) {
    *list = NULL;
    return TRUE;
}

void VFiles_Terminate(VFile** list) {
    VFile* next;

    while (*list) {
        next = (*list)->next;
#if MWCCPSP
        DisposeHandle(&(*list)->data);
#else
        DisposeHandle((*list)->data);
#endif
        xfree(*list);
        list = &next;
    }

    *list = NULL;
}

#if MWCCPSP
VFile* VFile_New(const char* name, OSHandle* data) {
    VFile* vf;
    void* ptr;
    UInt32 size;

    vf = xmalloc(NULL, sizeof(VFile));
    if (!vf) {
        return NULL;
    }

    strncpy(vf->displayName, name, sizeof(vf->displayName));
    vf->displayName[sizeof(vf->displayName) - 1] = 0;

    if (OS_NewHandle(0, &vf->data)) {
        xfree(vf);
        return NULL;
    }

    ptr = OS_LockHandle(data);
    OS_GetHandleSize(data, &size);
    if (OS_AppendHandle(&vf->data, ptr, size)) {
        OS_UnlockHandle(data);
        xfree(vf);
        return NULL;
    }
    OS_UnlockHandle(data);

    FixTextHandle(&vf->data);
    vf->next = NULL;
    return vf;
}
#else
VFile* VFile_New(const char* name, Handle data) {
    VFile* vf;

    vf = xmalloc(NULL, sizeof(VFile));
    if (!vf) {
        return NULL;
    }

    strncpy(vf->displayName, name, sizeof(vf->displayName));
    vf->displayName[sizeof(vf->displayName) - 1] = 0;

    vf->data = NewHandle(0);
    if (!vf->data || HandAndHand(data, vf->data)) {
        xfree(vf);
        return NULL;
    }

    FixTextHandle(vf->data);
    vf->next = NULL;
    return vf;
}
#endif

Boolean VFiles_Add(VFile** list, VFile* entry) {
    VFile** scan = list;
    while (*scan) {
        scan = &(*scan)->next;
    }
    *scan = entry;
    return TRUE;
}

VFile* VFiles_Find(VFile* list, const char* name) {
    VFile* scan = list;
    while (scan) {
        if (!strcmp(scan->displayName, name)) {
            break;
        }
        scan = scan->next;
    }
    return scan;
}
