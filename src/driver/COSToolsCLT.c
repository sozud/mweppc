#include "driver/COSToolsCLT.h"
#if MWCCPSP
#include "driver/MacFileTypes.h"
#endif
#if MWCCPSP
#include "compiler/win32.h"
#endif
#include "driver/CCompiler.h"
#if MWCCPSP
#include "driver/CWPluginsPrivate.h"
#endif
#include "driver/Files.h"
#include "driver/LowMem.h"
#include "driver/MacSpecs.h"
#include "driver/Memory.h"
#include "driver/MsDos.h"
#include "driver/oslib.h"
#include "driver/TextUtils.h"
#if MWCCPSP
#include <stdlib.h>
#include <string.h>
#endif

static void COS_pstrcpy(StringPtr dst, ConstStringPtr src) {
    short cnt;
    for (cnt = src[0]; cnt >= 0; cnt--) {
        *(dst++) = *(src++);
    }
}
#if MWCCPSP

#endif
#if MWCCPSP
// 3.0.1 keeps its memory in the plugin shell's handles. A Handle is a record
// whose first field is the locked block, so *h still reaches the data.
typedef struct COSHandleRec {
    void* ptr;
    SInt32 reserved;
    CWMemHandle mh;
} COSHandleRec;

static Boolean cos_memerror; // 0x5E8DAC
static HINSTANCE cos_instance; // 0x5E8DAE
static CWPluginContext cos_context; // 0x5E8DB2

// 0x439050
static void COS_ClearMemError(void) {
    cos_memerror = FALSE;
}

// 0x439060
static COSHandleRec* COS_NewHandleRec(void) {
    COSHandleRec* rec = (COSHandleRec*)calloc(sizeof(COSHandleRec), 1);
    if (!rec) {
        cos_memerror = TRUE;
        return NULL;
    }
    return rec;
}

// 0x439080
static int COS_FreeHandleRec(COSHandleRec* rec) {
    if (rec) {
        free(rec);
        return 0;
    }
    cos_memerror = TRUE;
    return 0;
}

// 0x4390A0
static Boolean COS_IsHandleRec(COSHandleRec* rec) {
    if (rec) {
        return TRUE;
    }
    return FALSE;
}

// 0x4390C0
static void COS_FreeAllHandles(void) {}

// 0x439E00: set before each plugin request
void COS_SetPluginContext(CWPluginContext context) {
    cos_context = context;
    COS_ClearMemError();
}

// 0x439E10
void COS_ClearPluginContext(void) {
    COS_FreeAllHandles();
    cos_context = NULL;
}

// 0x4390D0
Handle TempNewHandle301(SInt32 size, Boolean sys) {
    COSHandleRec* rec;
    CWResult err;

    if (size < 0) {
        cos_memerror = TRUE;
        return NULL;
    }
    rec = COS_NewHandleRec();
    if (!rec) {
        return NULL;
    }
    err = CWAllocMemHandle(cos_context, size, sys, &rec->mh);
    if (!err) {
        err = CWLockMemHandle(cos_context, rec->mh, FALSE, &rec->ptr);
    }
    if (err) {
        if (rec->mh) {
            CWFreeMemHandle(cos_context, rec->mh);
        }
        cos_memerror = TRUE;
        COS_FreeHandleRec(rec);
        rec = NULL;
    }
    return (Handle)rec;
}
#endif

Handle COS_NewHandle(SInt32 byteCount) {
#if MWCCPSP
    return TempNewHandle301(byteCount, FALSE);
#else
    return NewHandle(byteCount);
#endif
}

#if MWCCPSP
Handle COS_NewOSHandle(SInt32 logicalSize) {
    return TempNewHandle301(logicalSize, TRUE);
}
#else
Handle COS_NewOSHandle(SInt32 logicalSize) {
    OSErr err;
    Handle h;

    if (systemHandles) {
        h = TempNewHandle(logicalSize, &err);
        if (!err) {
            return h;
        }
    }

    return NULL;
}
#endif

#if MWCCPSP
void COS_FreeHandle(Handle handle) {
    COSHandleRec* rec = (COSHandleRec*)handle;

    if (!rec) {
        return;
    }
    if (!COS_IsHandleRec(rec)) {
        cos_memerror = TRUE;
    } else if (rec->mh) {
        if (CWUnlockMemHandle(cos_context, rec->mh) || CWFreeMemHandle(cos_context, rec->mh)) {
            cos_memerror = TRUE;
        }
        COS_FreeHandleRec(rec);
    }
}

Boolean COS_ResizeHandle(Handle handle, SInt32 newSize) {
    COSHandleRec* rec = (COSHandleRec*)handle;

    if (!rec || !COS_IsHandleRec(rec)) {
        cos_memerror = TRUE;
        return FALSE;
    }
    if (CWUnlockMemHandle(cos_context, rec->mh) ||
        CWResizeMemHandle(cos_context, rec->mh, newSize) ||
        CWLockMemHandle(cos_context, rec->mh, FALSE, &rec->ptr)) {
        cos_memerror = TRUE;
        return FALSE;
    }
    return TRUE;
}
#else
void COS_FreeHandle(Handle handle) {
    DisposeHandle(handle);
}

Boolean COS_ResizeHandle(Handle handle, SInt32 newSize) {
    SetHandleSize(handle, newSize);
    return LMGetMemErr() == noErr;
}
#endif

void COS_LockHandle(Handle handle) {
#if !MWCCPSP
    HLock(handle);
#endif
}

void COS_LockHandleHi(Handle handle) {
#if !MWCCPSP
    HLockHi(handle);
#endif
}

void COS_UnlockHandle(Handle handle) {
#if !MWCCPSP
    HUnlock(handle);
#endif
}
#if MWCCPSP

#endif
#if MWCCPSP
// 0x439290: unused
void* COS_AllocateMemory(SInt32 size) {
    void* ptr;
    CWResult err;

    err = CWAllocateMemory(cos_context, size, FALSE, &ptr);
    cos_memerror |= (err != 0);
    return err ? NULL : ptr;
}

// 0x4392D0: hands the shell handle over and frees the record
CWMemHandle COS_DetachHandle(Handle* handle) {
    COSHandleRec* rec = (COSHandleRec*)*handle;
    CWMemHandle mh;

    if (rec && !COS_IsHandleRec(rec)) {
        cos_memerror = TRUE;
        return NULL;
    }
    mh = rec->mh;
    COS_FreeHandleRec(rec);
    *handle = NULL;
    return mh;
}

// 0x439310
const char* COS_GetFileName(const char* path) {
    const char* p;

    p = strrchr(path, '\\');
    if (!p) {
        p = strrchr(path, '/');
        if (!p) {
            p = strrchr(path, ':');
        }
    }
    return p ? p + 1 : path;
}

// 0x439360
const char* COS_GetFileExtension(const char* path) {
    const char* name = COS_GetFileName(path);
    const char* p = strrchr(name, '.');
    if (!p) {
        p = name + strlen(name);
    }
    return p;
}
#endif

SInt32 COS_GetTicks(void) {
#if MWCCPSP
    return (GetTickCount() * 60) / 1000;
#else
    return LMGetTicks();
#endif
}

void COS_GetString(char* buffer, SInt16 strListID, SInt16 index) {
#if MWCCPSP
    if (!LoadStringA(cos_instance, index, buffer, 256)) {
        strcpy(buffer, "<string not found>");
    }
#else
    GetIndString((StringPtr)buffer, strListID, index);
    p2cstr((StringPtr)buffer);
#endif
}

Boolean COS_IsMultiByte(const void* str, const void* pos) {
    return OS_IsMultiByte(str, pos);
}

#if MWCCPSP
void COS_FileClose(COSFileRef refNum) {
    CloseHandle(refNum);
}

int COS_FileGetSize(COSFileRef refNum, SInt32* logEOF) {
    int err = 0;

    *logEOF = GetFileSize(refNum, NULL);
    if (*logEOF == -1) {
        err = GetLastError();
    }
    return err;
}

static int COS_FileOpenAccess(
    const COSFileSpec* spec, COSFileRef* refNum, DWORD access, DWORD creation
) {
    int err = 0;
    HANDLE h;

    h = CreateFileA(
        (const char*)spec,
        access,
        access == GENERIC_READ ? FILE_SHARE_READ : 0,
        NULL,
        creation,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    *refNum = h;
    if (h == INVALID_HANDLE_VALUE) {
        *refNum = NULL;
        err = GetLastError();
    }
    return err;
}

int COS_FileOpen(const COSFileSpec* spec, COSFileRef* refNum) {
    return COS_FileOpenAccess(spec, refNum, GENERIC_READ, OPEN_EXISTING);
}

int COS_FileNew(const COSFileSpec* spec, COSFileRef* refNum) {
    return COS_FileOpenAccess(spec, refNum, GENERIC_READ | GENERIC_WRITE, CREATE_ALWAYS);
}

int COS_FileRead(COSFileRef refNum, void* buffPtr, SInt32 count) {
    DWORD done;
    int err = 0;

    if (!ReadFile(refNum, buffPtr, count, &done, NULL)) {
        err = GetLastError();
    }
    return err;
}

int COS_FileWrite(COSFileRef refNum, const void* buffPtr, SInt32 count) {
    DWORD done;
    int err = 0;

    if (!WriteFile(refNum, buffPtr, count, &done, NULL)) {
        err = GetLastError();
    }
    return err;
}

int COS_FileSetPos(COSFileRef refNum, SInt32 filePos) {
    SetFilePointer(refNum, filePos, NULL, FILE_BEGIN);
    return 0;
}

int COS_FileGetPos(COSFileRef refNum, SInt32* filePos) {
    int err = 0;

    *filePos = SetFilePointer(refNum, 0, NULL, FILE_CURRENT);
    if (*filePos == -1) {
        err = GetLastError();
    }
    return err;
}

#if CW_PLUGIN_PATHSPEC
// a path spec: the type from the file itself
OSErr COS_FileGetType(const COSFileSpec* spec, OSType* fileType) {
    OSSpec oss;
    int err;

    if ((err = OS_MakeSpec((const char*)spec, &oss, NULL))) {
        return err;
    }
    return OS_GetMacFileType(&oss, fileType);
}
#else
OSErr COS_FileGetType(const FSSpec* spec, OSType* fileType) {
    OSErr err;
    FInfo fi;

    err = HGetFInfo(spec->vRefNum, spec->parID, spec->name, &fi);
    *fileType = fi.fdType;
    return err;
}
#endif

#else
OSErr COS_FileNew(const FSSpec* spec, SInt16* refNum, OSType creator, OSType fileType) {
    OSErr err;

    HDelete(spec->vRefNum, spec->parID, spec->name);
    err = HCreate(spec->vRefNum, spec->parID, spec->name, creator, fileType);
    if (!err) {
        err = HOpenDF(spec->vRefNum, spec->parID, spec->name, fsRdWrPerm, refNum);
    }
    return err;
}

OSErr COS_FileOpen(const FSSpec* spec, SInt16* refNum) {
    return HOpenDF(spec->vRefNum, spec->parID, spec->name, fsRdPerm, refNum);
}

OSErr COS_FileGetType(const FSSpec* spec, OSType* fileType) {
    OSErr err;
    FInfo fi;

    err = HGetFInfo(spec->vRefNum, spec->parID, spec->name, &fi);
    *fileType = fi.fdType;
    return err;
}

OSErr COS_FileGetSize(SInt16 refNum, SInt32* logEOF) {
    return GetEOF(refNum, logEOF);
}

OSErr COS_FileRead(SInt16 refNum, void* buffPtr, SInt32 count) {
    return FSRead(refNum, &count, buffPtr);
}

OSErr COS_FileWrite(SInt16 refNum, const void* buffPtr, SInt32 count) {
    return FSWrite(refNum, &count, buffPtr);
}

OSErr COS_FileGetPos(SInt16 refNum, SInt32* filePos) {
    return GetFPos(refNum, filePos);
}

OSErr COS_FileSetPos(SInt16 refNum, SInt32 filePos) {
    return SetFPos(refNum, fsFromStart, filePos);
}

OSErr COS_FileClose(SInt16 refNum) {
    return FSClose(refNum);
}

#endif
#if MWCCPSP

#endif
#if MWCCPSP
// 0x439A70: a directory's path gets its trailing backslash; the Win32 error
// if the path does not exist
int func_00439A70(char* path) {
    char* end;
    DWORD attr;

    end = path + strlen(path);
    if (end == path || end[-1] == '\\') {
        return 0;
    }
    attr = GetFileAttributesA(path);
    if (attr == (DWORD)-1) {
        return GetLastError();
    }
    if ((attr & FILE_ATTRIBUTE_DIRECTORY) && end[-1] != '\\') {
        strcpy(end, "\\");
    }
    return 0;
}

// 0x439CF0: the current time as a FILETIME
unsigned long long func_00439CF0(void) {
    SYSTEMTIME st;
    FILETIME ft;

    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);
    return *(unsigned long long*)&ft;
}

void COS_FileGetFSSpecInfo(
    const COSFileSpec* spec, char* dir, SInt32 dirSize, char* fileName, SInt32 nameSize
) {
    char path[260];
    char* name;

    COS_FileGetPathName(path, spec, NULL);
    name = (char*)COS_GetFileName(path);
    if (dir) {
        int len = name - path;
        if (len >= dirSize - 1) {
            len = dirSize - 1;
        }
        strncpy(dir, path, len);
        dir[len] = 0;
    }
    if (fileName) {
        strncpy(fileName, name, nameSize - 1);
        fileName[nameSize - 1] = 0;
    }
}

// 0x4393A0: the spec's file name, in a static buffer
char* func_004393A0(const COSFileSpec* spec) {
    static char name[260]; // 0x5E8CA8
    COS_FileGetFSSpecInfo(spec, NULL, 0, name, sizeof(name));
    return name;
}

// 0x4393C0: reads count bytes at pos, keeping the file position; unused
int COS_FileReadAt(COSFileRef refNum, SInt32 pos, void* buffer, SInt32 count) {
    SInt32 size;
    SInt32 oldpos;
    int err;

    if (!(err = COS_FileGetPos(refNum, &oldpos)) &&
        !(err = COS_FileSetPos(refNum, pos)) &&
        !(err = COS_FileGetSize(refNum, &size)) &&
        size >= count &&
        !(err = COS_FileRead(refNum, buffer, count)) &&
        !(err = COS_FileSetPos(refNum, oldpos))) {
        return 0;
    }
    return err;
}

// 0x4394F0
void COS_p2cstrcpy(char* dst, ConstStringPtr src) {
    int len = src[0];
    memmove(dst, src + 1, len);
    dst[len] = 0;
}

// 0x439520
void COS_c2pstrcpy(StringPtr dst, const char* src) {
    int len = strlen(src);
    if (len > 255) {
        len = 255;
    }
    memmove(dst + 1, src, len);
    dst[0] = len;
}
#else
void COS_FileGetFSSpecInfo(const FSSpec* spec, SInt16* vRefNum, SInt32* dirID, StringPtr fileName) {
    if (vRefNum) {
        *vRefNum = spec->vRefNum;
    }
    if (dirID) {
        *dirID = spec->parID;
    }
    if (fileName) {
        COS_pstrcpy(fileName, spec->name);
    }
}
#endif

#ifdef __MWERKS__
static void COS_MakePath(SInt16 vRefNum, SInt32 dirID, StringPtr path) {
    FSSpec fss;
    OSSpec oss;
    fss.vRefNum = vRefNum;
    fss.parID = dirID;
    COS_pstrcpy(fss.name, path);
    if (!OS_FSSpec_To_OSSpec(&fss, &oss)) {
        OS_SpecToString(&oss, (char*)path, 260);
        c2pstr((char*)path);
    }
}
#else
static void COS_MakePath(SInt16 vRefNum, SInt32 dirID, StringPtr path) {
    struct {
        FSSpec fss;
        UInt8 gap[2];
        OSSpec oss;
    } frame;
    frame.fss.vRefNum = vRefNum;
    frame.fss.parID = dirID;
    COS_pstrcpy(frame.fss.name, path);
    if (!OS_FSSpec_To_OSSpec(&frame.fss, &frame.oss)) {
        OS_SpecToString(&frame.oss, (char*)path, 260);
        c2pstr((char*)path);
    }
}
#endif

#if MWCCPSP
void COS_FileGetPathName(char* buffer, const COSFileSpec* spec, COSFileTime* mdDat) {
    WIN32_FIND_DATAA fd;
    HANDLE h;
    size_t len;

    len = strlen((const char*)spec);
    if (len >= 260) {
        len = 259;
    }
    memcpy(buffer, spec, len);
    buffer[len] = 0;
    func_00439A70(buffer);

    if (mdDat) {
        h = FindFirstFileA((const char*)spec, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            *(FILETIME*)mdDat = fd.ftLastWriteTime;
            FindClose(h);
        } else {
            *(unsigned long long*)mdDat = func_00439CF0();
        }
    }
}
#else
void COS_FileGetPathName(char* buffer, const FSSpec* spec, SInt32* mdDat) {
    HParamBlockRec rec;

    if (mdDat) {
        rec.fileParam.ioNamePtr = (StringPtr)spec->name;
        rec.fileParam.ioVRefNum = spec->vRefNum;
        rec.fileParam.ioDirID = spec->parID;
        rec.fileParam.ioFDirIndex = 0;
        if (!PBHGetFInfoSync(&rec)) {
            *mdDat = rec.fileParam.ioFlMdDat;
        } else {
            *mdDat = 0;
        }
    }

    COS_pstrcpy((StringPtr)buffer, spec->name);
    COS_MakePath(spec->vRefNum, spec->parID, (StringPtr)buffer);
    p2cstr((StringPtr)buffer);
}
#endif
