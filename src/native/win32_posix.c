// POSIX versions of the Win32 calls the 3.0.1 driver makes (native build
// only). Only what a command-line compile needs is real: files,
// directories, the current directory, times and memory. Processes,
// resources and version info report failure.
//
// File handles are small integers (fd + HANDLE_BASE) because the driver keeps
// them in an int (MsDos.c: *ref = (int)CreateFileA(...)).

#define __declspec(x)
#include "compiler/common.h"
#include "compiler/win32.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef __APPLE__
#define st_atimespec st_atim
#define st_mtimespec st_mtim
#define st_ctimespec st_ctim
#endif

#define HANDLE_BASE 0x100
#define FD_OF(h) ((int)(intptr_t)(h) - HANDLE_BASE)
#define HANDLE_OF(fd) ((HANDLE)(intptr_t)((fd) + HANDLE_BASE))

static DWORD last_error;

static DWORD win32_error(int e) {
    switch (e) {
        case 0:
            return 0;
        case ENOENT:
            return 2; // ERROR_FILE_NOT_FOUND
        case ENOTDIR:
            return 3; // ERROR_PATH_NOT_FOUND
        case EMFILE:
            return 4; // ERROR_TOO_MANY_OPEN_FILES
        case EACCES:
        case EPERM:
            return 5; // ERROR_ACCESS_DENIED
        case EBADF:
            return 6; // ERROR_INVALID_HANDLE
        case ENOMEM:
            return 8; // ERROR_NOT_ENOUGH_MEMORY
        case EEXIST:
            return 80; // ERROR_FILE_EXISTS
        case ENOSPC:
            return 112; // ERROR_DISK_FULL
        case ENAMETOOLONG:
            return 206;
        default:
            return 31; // ERROR_GEN_FAILURE
    }
}

static BOOL fail(void) {
    last_error = win32_error(errno);
    return FALSE;
}

// Windows paths (backslashes, drive letters) as POSIX paths
static const char* posix_path(const char* path, char* buf) {
    size_t i;

    if (((path[0] | 0x20) >= 'a' && (path[0] | 0x20) <= 'z') && path[1] == ':') {
        path += 2; // a drive letter from the Windows-style driver code
    }
    for (i = 0; path[i] && i < PATH_MAX - 1; i++) {
        buf[i] = path[i] == '\\' ? '/' : path[i];
    }
    buf[i] = 0;
    return buf;
}

// POSIX paths back to the driver's Windows style
// fopen for code that opens a path built with Windows separators (CLIO.c)
FILE* win32_fopen(const char* path, const char* mode) {
    char buf[PATH_MAX];
    return fopen(posix_path(path, buf), mode);
}

static void windows_path(char* p) {
    for (; *p; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }
}

// the MSL assertion handler (3.0.1's four-argument form)
void __assertion_failed(
    char const* condition, char const* filename, char const* funcname, int lineno
) {
    fprintf(stderr, "Assertion (%s) failed in \"%s\" on line %d\n", condition, filename, lineno);
    abort();
}

DWORD __stdcall GetLastError(void) {
    return last_error;
}

// the Win32 environment block: "NAME=value\0" strings ending in an empty one
extern char** environ;

LPSTR __stdcall GetEnvironmentStrings(void) {
    size_t size;
    char** e;
    char* block;
    char* p;

    size = 1;
    for (e = environ; *e; e++) {
        size += strlen(*e) + 1;
    }
    block = malloc(size);
    if (!block) {
        return NULL;
    }
    p = block;
    for (e = environ; *e; e++) {
        strcpy(p, *e);
        p += strlen(*e) + 1;
    }
    *p = 0;
    return block;
}

BOOL __stdcall FreeEnvironmentStringsA(LPSTR lpszEnvironmentBlock) {
    free(lpszEnvironmentBlock);
    return TRUE;
}

HANDLE __stdcall CreateFileA(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    char buf[PATH_MAX];
    int flags;
    int fd;

    if ((dwDesiredAccess & GENERIC_READ) && (dwDesiredAccess & GENERIC_WRITE)) {
        flags = O_RDWR;
    } else if (dwDesiredAccess & GENERIC_WRITE) {
        flags = O_WRONLY;
    } else {
        flags = O_RDONLY;
    }
    switch (dwCreationDisposition) {
        case 1:
            flags |= O_CREAT | O_EXCL;
            break; // CREATE_NEW
        case 2:
            flags |= O_CREAT | O_TRUNC;
            break; // CREATE_ALWAYS
        case 4:
            flags |= O_CREAT;
            break; // OPEN_ALWAYS
        case 5:
            flags |= O_TRUNC;
            break; // TRUNCATE_EXISTING
        default:
            break; // OPEN_EXISTING
    }
    fd = open(posix_path(lpFileName, buf), flags, 0666);
    if (fd < 0) {
        fail();
        return INVALID_HANDLE_VALUE;
    }
    last_error = 0;
    return HANDLE_OF(fd);
}

BOOL __stdcall CloseHandle(HANDLE hObject) {
    if (close(FD_OF(hObject)) < 0) {
        return fail();
    }
    return TRUE;
}

BOOL __stdcall ReadFile(
    HANDLE hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
) {
    ssize_t n = read(FD_OF(hFile), lpBuffer, nNumberOfBytesToRead);
    if (n < 0) {
        if (lpNumberOfBytesRead) {
            *lpNumberOfBytesRead = 0;
        }
        return fail();
    }
    if (lpNumberOfBytesRead) {
        *lpNumberOfBytesRead = (DWORD)n;
    }
    return TRUE;
}

BOOL __stdcall WriteFile(
    HANDLE hFile,
    LPCVOID lpBuffer,
    DWORD nNumberOfBytesToWrite,
    LPDWORD lpNumberOfBytesWritten,
    LPOVERLAPPED lpOverlapped
) {
    ssize_t n = write(FD_OF(hFile), lpBuffer, nNumberOfBytesToWrite);
    if (n < 0) {
        if (lpNumberOfBytesWritten) {
            *lpNumberOfBytesWritten = 0;
        }
        return fail();
    }
    if (lpNumberOfBytesWritten) {
        *lpNumberOfBytesWritten = (DWORD)n;
    }
    return TRUE;
}

DWORD __stdcall GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    struct stat st;

    if (fstat(FD_OF(hFile), &st) < 0) {
        fail();
        return 0xFFFFFFFF;
    }
    if (lpFileSizeHigh) {
        *lpFileSizeHigh = (DWORD)((uint64_t)st.st_size >> 32);
    }
    return (DWORD)st.st_size;
}

DWORD __stdcall SetFilePointer(
    HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod
) {
    off_t off = lDistanceToMove;
    off_t r;

    if (lpDistanceToMoveHigh) {
        off = ((off_t)*lpDistanceToMoveHigh << 32) | (UInt32)lDistanceToMove;
    }
    r = lseek(
        FD_OF(hFile),
        off,
        dwMoveMethod == 0       ? SEEK_SET
            : dwMoveMethod == 1 ? SEEK_CUR
                                : SEEK_END
    );
    if (r < 0) {
        fail();
        return 0xFFFFFFFF;
    }
    if (lpDistanceToMoveHigh) {
        *lpDistanceToMoveHigh = (LONG)(r >> 32);
    }
    return (DWORD)r;
}

BOOL __stdcall SetEndOfFile(HANDLE hFile) {
    off_t here = lseek(FD_OF(hFile), 0, SEEK_CUR);
    if (here < 0 || ftruncate(FD_OF(hFile), here) < 0) {
        return fail();
    }
    return TRUE;
}

// FILETIME: 100 ns units since 1601-01-01
#define EPOCH_DIFF 116444736000000000ULL

static void to_filetime(time_t t, long nsec, FILETIME* ft) {
    uint64_t v = (uint64_t)t * 10000000ULL + (uint64_t)nsec / 100 + EPOCH_DIFF;
    ft->dwLowDateTime = (DWORD)v;
    ft->dwHighDateTime = (DWORD)(v >> 32);
}

BOOL __stdcall GetFileTime(
    HANDLE hFile, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime
) {
    struct stat st;

    if (fstat(FD_OF(hFile), &st) < 0) {
        return fail();
    }
    if (lpCreationTime) {
        to_filetime(st.st_ctimespec.tv_sec, st.st_ctimespec.tv_nsec, lpCreationTime);
    }
    if (lpLastAccessTime) {
        to_filetime(st.st_atimespec.tv_sec, st.st_atimespec.tv_nsec, lpLastAccessTime);
    }
    if (lpLastWriteTime) {
        to_filetime(st.st_mtimespec.tv_sec, st.st_mtimespec.tv_nsec, lpLastWriteTime);
    }
    return TRUE;
}

BOOL __stdcall SetFileTime(
    HANDLE hFile,
    const FILETIME* lpCreationTime,
    const FILETIME* lpLastAccessTime,
    const FILETIME* lpLastWriteTime
) {
    return TRUE; // modification times are not part of the output
}

DWORD __stdcall GetFileAttributesA(LPCSTR lpFileName) {
    char buf[PATH_MAX];
    struct stat st;

    if (stat(posix_path(lpFileName, buf), &st) < 0) {
        fail();
        return 0xFFFFFFFF;
    }
    return S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}

BOOL __stdcall SetFileAttributesA(LPCSTR lpFileName, DWORD dwFileAttributes) {
    return TRUE;
}

BOOL __stdcall DeleteFileA(LPCSTR lpFileName) {
    char buf[PATH_MAX];
    return unlink(posix_path(lpFileName, buf)) < 0 ? fail() : TRUE;
}

BOOL __stdcall CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
    char buf[PATH_MAX];
    return mkdir(posix_path(lpPathName, buf), 0777) < 0 ? fail() : TRUE;
}

BOOL __stdcall RemoveDirectoryA(LPCSTR lpPathName) {
    char buf[PATH_MAX];
    return rmdir(posix_path(lpPathName, buf)) < 0 ? fail() : TRUE;
}

DWORD __stdcall GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer) {
    char buf[PATH_MAX];
    size_t n;

    if (!getcwd(buf, sizeof(buf))) {
        fail();
        return 0;
    }
    windows_path(buf);
    n = strlen(buf);
    if (n + 1 > nBufferLength) {
        return (DWORD)(n + 1);
    }
    memcpy(lpBuffer, buf, n + 1);
    return (DWORD)n;
}

// an absolute POSIX path without "." and ".." components (as Windows'
// GetFullPathName gives them)
static void collapse_dots(char* path) {
    char* parts[PATH_MAX / 2];
    int n = 0;
    char* p = path + 1;
    char* q;
    char out[PATH_MAX * 2];
    size_t len = 0;
    int i;

    while (*p) {
        q = strchr(p, '/');
        if (q) {
            *q = 0;
        }
        if (!strcmp(p, "..")) {
            if (n) {
                n--;
            }
        } else if (*p && strcmp(p, ".")) {
            parts[n++] = p;
        }
        if (!q) {
            break;
        }
        p = q + 1;
    }
    out[0] = 0;
    for (i = 0; i < n; i++) {
        len += snprintf(out + len, sizeof(out) - len, "/%s", parts[i]);
    }
    if (!n) {
        strcpy(out, "/");
    }
    strcpy(path, out);
}

DWORD __stdcall GetFullPathNameA(
    LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart
) {
    char in[PATH_MAX];
    char out[PATH_MAX * 2];
    char* slash;
    size_t n;

    posix_path(lpFileName, in);
    if (in[0] == '/') {
        snprintf(out, sizeof(out), "%s", in);
    } else {
        if (!getcwd(out, PATH_MAX)) {
            fail();
            return 0;
        }
        n = strlen(out);
        snprintf(out + n, sizeof(out) - n, "/%s", in);
    }
    collapse_dots(out);
    windows_path(out);
    n = strlen(out);
    if (n + 1 > nBufferLength) {
        return (DWORD)(n + 1);
    }
    memcpy(lpBuffer, out, n + 1);
    if (lpFilePart) {
        slash = strrchr(lpBuffer, '\\');
        *lpFilePart = slash ? slash + 1 : lpBuffer;
    }
    return (DWORD)n;
}

DWORD __stdcall GetShortPathNameA(LPCSTR lpszLongPath, LPSTR lpszShortPath, DWORD cchBuffer) {
    size_t n = strlen(lpszLongPath);
    if (n + 1 > cchBuffer) {
        return (DWORD)(n + 1);
    }
    memcpy(lpszShortPath, lpszLongPath, n + 1);
    return (DWORD)n;
}

// directory searches: "dir/pattern"
typedef struct FindState {
    DIR* dir;
    char dirpath[PATH_MAX];
    char pattern[PATH_MAX];
} FindState;

static BOOL find_next(FindState* fs, LPWIN32_FIND_DATAA data) {
    struct dirent* de;
    struct stat st;
    char full[PATH_MAX * 2];

    while ((de = readdir(fs->dir))) {
        if (fnmatch(fs->pattern, de->d_name, 0) != 0) {
            continue;
        }
        memset(data, 0, sizeof(*data));
        snprintf(full, sizeof(full), "%s/%s", fs->dirpath, de->d_name);
        if (stat(full, &st) == 0) {
            data->dwFileAttributes =
                S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
            data->nFileSizeLow = (DWORD)st.st_size;
            data->nFileSizeHigh = (DWORD)((uint64_t)st.st_size >> 32);
            to_filetime(st.st_mtimespec.tv_sec, st.st_mtimespec.tv_nsec, &data->ftLastWriteTime);
        }
        snprintf(data->cFileName, sizeof(data->cFileName), "%s", de->d_name);
        return TRUE;
    }
    last_error = 18; // ERROR_NO_MORE_FILES
    return FALSE;
}

HANDLE __stdcall FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData) {
    char buf[PATH_MAX];
    FindState* fs;
    char* slash;

    fs = calloc(1, sizeof(FindState));
    posix_path(lpFileName, buf);
    slash = strrchr(buf, '/');
    if (slash) {
        *slash = 0;
        snprintf(fs->dirpath, sizeof(fs->dirpath), "%s", buf[0] ? buf : "/");
        snprintf(fs->pattern, sizeof(fs->pattern), "%s", slash + 1);
    } else {
        snprintf(fs->dirpath, sizeof(fs->dirpath), ".");
        snprintf(fs->pattern, sizeof(fs->pattern), "%s", buf);
    }
    if (!strcmp(fs->pattern, "*.*")) {
        strcpy(fs->pattern, "*");
    }
    fs->dir = opendir(fs->dirpath);
    if (!fs->dir) {
        fail();
        free(fs);
        return INVALID_HANDLE_VALUE;
    }
    if (!find_next(fs, lpFindFileData)) {
        closedir(fs->dir);
        free(fs);
        return INVALID_HANDLE_VALUE;
    }
    return (HANDLE)fs;
}

BOOL __stdcall FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData) {
    return find_next((FindState*)hFindFile, lpFindFileData);
}

BOOL __stdcall FindClose(HANDLE hFindFile) {
    FindState* fs = (FindState*)hFindFile;
    closedir(fs->dir);
    free(fs);
    return TRUE;
}

// standard handles
static HANDLE std_handles[3] = {HANDLE_OF(0), HANDLE_OF(1), HANDLE_OF(2)};

HANDLE __stdcall GetStdHandle(DWORD nStdHandle) {
    switch ((int)nStdHandle) {
        case -10:
            return std_handles[0];
        case -11:
            return std_handles[1];
        case -12:
            return std_handles[2];
        default:
            return INVALID_HANDLE_VALUE;
    }
}

BOOL __stdcall SetStdHandle(DWORD nStdHandle, HANDLE hHandle) {
    int which = -10 - (int)nStdHandle;
    if (which < 0 || which > 2) {
        return FALSE;
    }
    // the driver redirects output to a file this way
    if (dup2(FD_OF(hHandle), which) < 0) {
        return fail();
    }
    return TRUE;
}

BOOL __stdcall GetConsoleScreenBufferInfo(
    HANDLE hConsoleOutput, PCONSOLE_SCREEN_BUFFER_INFO lpConsoleScreenBufferInfo
) {
    return FALSE; // not a console: no line wrapping
}

BOOL __stdcall SetConsoleCtrlHandler(PHANDLER_ROUTINE HandlerRoutine, BOOL Add) {
    return TRUE;
}

// times
DWORD __stdcall GetTickCount(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (DWORD)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

void __stdcall GetSystemTime(LPSYSTEMTIME lpSystemTime) {
    struct timeval tv;
    struct tm tm;

    gettimeofday(&tv, NULL);
    gmtime_r(&tv.tv_sec, &tm);
    lpSystemTime->wYear = tm.tm_year + 1900;
    lpSystemTime->wMonth = tm.tm_mon + 1;
    lpSystemTime->wDayOfWeek = tm.tm_wday;
    lpSystemTime->wDay = tm.tm_mday;
    lpSystemTime->wHour = tm.tm_hour;
    lpSystemTime->wMinute = tm.tm_min;
    lpSystemTime->wSecond = tm.tm_sec;
    lpSystemTime->wMilliseconds = tv.tv_usec / 1000;
}

BOOL __stdcall SystemTimeToFileTime(const SYSTEMTIME* lpSystemTime, LPFILETIME lpFileTime) {
    struct tm tm;
    time_t t;

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = lpSystemTime->wYear - 1900;
    tm.tm_mon = lpSystemTime->wMonth - 1;
    tm.tm_mday = lpSystemTime->wDay;
    tm.tm_hour = lpSystemTime->wHour;
    tm.tm_min = lpSystemTime->wMinute;
    tm.tm_sec = lpSystemTime->wSecond;
    t = timegm(&tm);
    to_filetime(t, lpSystemTime->wMilliseconds * 1000000L, lpFileTime);
    return TRUE;
}

BOOL __stdcall FileTimeToSystemTime(const FILETIME* lpFileTime, LPSYSTEMTIME lpSystemTime) {
    uint64_t v = ((uint64_t)lpFileTime->dwHighDateTime << 32) | lpFileTime->dwLowDateTime;
    time_t t = (time_t)((v - EPOCH_DIFF) / 10000000ULL);
    struct tm tm;

    gmtime_r(&t, &tm);
    lpSystemTime->wYear = tm.tm_year + 1900;
    lpSystemTime->wMonth = tm.tm_mon + 1;
    lpSystemTime->wDayOfWeek = tm.tm_wday;
    lpSystemTime->wDay = tm.tm_mday;
    lpSystemTime->wHour = tm.tm_hour;
    lpSystemTime->wMinute = tm.tm_min;
    lpSystemTime->wSecond = tm.tm_sec;
    lpSystemTime->wMilliseconds = (WORD)((v - EPOCH_DIFF) / 10000ULL % 1000);
    return TRUE;
}

// memory: GMEM_FIXED blocks are plain pointers (MsDos.c's OS handles)
HGLOBAL __stdcall GlobalAlloc(UINT uFlags, SIZE_T dwBytes) {
    void* p = malloc(dwBytes ? dwBytes : 1);
    if (!p) {
        last_error = 8;
    }
    return p;
}

HGLOBAL __stdcall GlobalReAlloc(HGLOBAL hMem, SIZE_T dwBytes, UINT uFlags) {
    void* p = realloc(hMem, dwBytes ? dwBytes : 1);
    if (!p) {
        last_error = 8;
    }
    return p;
}

UINT __stdcall GlobalFlags(HGLOBAL hMem) {
    return hMem ? 0 : GMEM_INVALID_HANDLE;
}

HGLOBAL __stdcall GlobalFree(HGLOBAL hMem) {
    free(hMem);
    return NULL;
}

// processes, resources, version info and messages: not available natively
BOOL __stdcall CreateProcessA(
    LPCSTR lpApplicationName,
    LPSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    LPVOID lpEnvironment,
    LPCSTR lpCurrentDirectory,
    LPSTARTUPINFOA lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation
) {
    last_error = 2;
    return FALSE;
}

DWORD __stdcall WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    return 0xFFFFFFFF;
}

BOOL __stdcall GetExitCodeProcess(HANDLE hProcess, LPDWORD lpExitCode) {
    return FALSE;
}

HMODULE __stdcall GetModuleHandleA(LPCSTR lpModuleName) {
    return NULL;
}

HRSRC __stdcall FindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType) {
    last_error = 1813; // ERROR_RESOURCE_TYPE_NOT_FOUND
    return NULL;
}

HGLOBAL __stdcall LoadResource(HMODULE hModule, HRSRC hResInfo) {
    return NULL;
}

LPVOID __stdcall LockResource(HGLOBAL hResData) {
    return NULL;
}

DWORD __stdcall SizeofResource(HMODULE hModule, HRSRC hResInfo) {
    return 0;
}

// the exe's string table resources (src/native/strings.c)
typedef struct NativeString {
    unsigned int id;
    const char* text;
} NativeString;
extern const unsigned int native_string_count;
extern const NativeString native_strings[];

int __stdcall LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax) {
    unsigned int lo = 0, hi = native_string_count;
    int n;

    if (cchBufferMax <= 0) {
        return 0;
    }
    lpBuffer[0] = 0;
    while (lo < hi) {
        unsigned int mid = (lo + hi) / 2;
        if (native_strings[mid].id < uID) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo == native_string_count || native_strings[lo].id != uID) {
        return 0;
    }
    n = (int)strlen(native_strings[lo].text);
    if (n > cchBufferMax - 1) {
        n = cchBufferMax - 1;
    }
    memcpy(lpBuffer, native_strings[lo].text, n);
    lpBuffer[n] = 0;
    return n;
}

DWORD __stdcall GetFileVersionInfoSizeA(LPCSTR lptstrFilename, LPDWORD lpdwHandle) {
    return 0;
}

BOOL __stdcall GetFileVersionInfoA(
    LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData
) {
    return FALSE;
}

BOOL __stdcall VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, UINT* puLen) {
    return FALSE;
}

DWORD __stdcall FormatMessageA(
    DWORD dwFlags,
    LPCVOID lpSource,
    DWORD dwMessageId,
    DWORD dwLanguageId,
    LPSTR lpBuffer,
    DWORD nSize,
    va_list* Arguments
) {
    const char* msg = strerror(EIO);
    size_t n;

    if (!lpBuffer || !nSize) {
        return 0;
    }
    snprintf(lpBuffer, nSize, "error %u (%s)", (unsigned)dwMessageId, msg);
    n = strlen(lpBuffer);
    return (DWORD)n;
}

UINT __stdcall GetSystemDirectoryA(LPSTR lpBuffer, UINT uSize) {
    return (UINT)snprintf(lpBuffer, uSize, "/");
}

UINT __stdcall GetWindowsDirectoryA(LPSTR lpBuffer, UINT uSize) {
    return (UINT)snprintf(lpBuffer, uSize, "/");
}

// Shift-JIS file name wrappers (src/host/ShiftJIS.c): paths are passed through as is
HANDLE __stdcall ShiftJIS_CreateFileA(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    return CreateFileA(
        lpFileName,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile
    );
}

BOOL __stdcall ShiftJIS_CloseHandle(HANDLE hObject) {
    return CloseHandle(hObject);
}

DWORD __stdcall ShiftJIS_GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    return GetFileSize(hFile, lpFileSizeHigh);
}

BOOL __stdcall ShiftJIS_ReadFile(
    HANDLE hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
) {
    return ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
}

DWORD __stdcall ShiftJIS_SetFilePointer(
    HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod
) {
    return SetFilePointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
}

BOOL __stdcall ShiftJIS_IsDBCSLeadByte(BYTE TestChar) {
    return FALSE;
}
