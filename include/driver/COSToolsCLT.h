#ifndef DRIVER_COSTOOLSCLT_H
#define DRIVER_COSTOOLSCLT_H

#include "compiler/common.h"
#include "driver/Files.h"
#include "driver/oslib.h"
#if MWCCPSP
#include "driver/TextUtils.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif
#if MWCCPSP

// 3.0.1's file calls take the plugin API's file spec (a path).
typedef CWFileSpec COSFileSpec;
#endif

extern Handle COS_NewHandle(SInt32 byteCount);
extern Handle COS_NewOSHandle(SInt32 logicalSize);
extern void COS_FreeHandle(Handle handle);
extern Boolean COS_ResizeHandle(Handle handle, SInt32 newSize);
extern void COS_LockHandle(Handle handle);
extern void COS_LockHandleHi(Handle handle);
extern void COS_UnlockHandle(Handle handle);
extern SInt32 COS_GetTicks(void);
extern void COS_GetString(char* buffer, SInt16 strListID, SInt16 index);
extern Boolean COS_IsMultiByte(const void* str, const void* pos);
#if MWCCPSP
// 3.0.1 (0x439560..0x439710): Win32 file handles opened by path
typedef void* COSFileRef;
extern int COS_FileNew(const COSFileSpec* spec, COSFileRef* refNum);
extern int COS_FileOpen(const COSFileSpec* spec, COSFileRef* refNum);
extern OSErr COS_FileGetType(const COSFileSpec* spec, OSType* fileType);
extern int COS_FileGetSize(COSFileRef refNum, SInt32* logEOF);
extern int COS_FileRead(COSFileRef refNum, void* buffPtr, SInt32 count);
extern int COS_FileWrite(COSFileRef refNum, const void* buffPtr, SInt32 count);
extern int COS_FileGetPos(COSFileRef refNum, SInt32* filePos);
extern int COS_FileSetPos(COSFileRef refNum, SInt32 filePos);
extern void COS_FileClose(COSFileRef refNum);
#else
extern OSErr COS_FileNew(const FSSpec* spec, SInt16* refNum, OSType creator, OSType fileType);
extern OSErr COS_FileOpen(const FSSpec* spec, SInt16* refNum);
extern OSErr COS_FileGetType(const FSSpec* spec, OSType* fileType);
extern OSErr COS_FileGetSize(SInt16 refNum, SInt32* logEOF);
extern OSErr COS_FileRead(SInt16 refNum, void* buffPtr, SInt32 count);
extern OSErr COS_FileWrite(SInt16 refNum, const void* buffPtr, SInt32 count);
extern OSErr COS_FileGetPos(SInt16 refNum, SInt32* filePos);
extern OSErr COS_FileSetPos(SInt16 refNum, SInt32 filePos);
extern OSErr COS_FileClose(SInt16 refNum);
#endif
#if MWCCPSP
// 3.0.1 (0x439440, 0x439C30): specs are paths; the directory and the name
// come back as C strings in sized buffers, the time as a Win32 FILETIME
typedef struct COSFileTime {
    UInt32 lo;
    UInt32 hi;
} COSFileTime;
extern void COS_FileGetFSSpecInfo(
    const COSFileSpec* spec, char* dir, SInt32 dirSize, char* fileName, SInt32 nameSize
);
extern void COS_FileGetPathName(char* buffer, const COSFileSpec* spec, COSFileTime* mdDat);
// for 2.4.7 code that still expects the name as a Pascal string (Str255)
extern void COS_c2pstrcpy(unsigned char* dst, const char* src);
#define COS_FileGetPascalName(spec, pname) \
    (COS_FileGetFSSpecInfo((spec), NULL, 0, (char*)(pname), 256), \
     COS_c2pstrcpy((unsigned char*)(pname), (char*)(pname)))
#else
extern void COS_FileGetFSSpecInfo(
    const FSSpec* spec, SInt16* vRefNum, SInt32* dirID, StringPtr fileName
);
extern void COS_FileGetPathName(char* buffer, const FSSpec* spec, SInt32* mdDat);
#endif

#ifdef __cplusplus
}
#endif

#endif
