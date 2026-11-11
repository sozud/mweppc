#ifndef DRIVER_LICENSEIMPORTS_H
#define DRIVER_LICENSEIMPORTS_H

#include "compiler/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VENDORCODE {
    short type;
    UInt32 data[2];
    UInt32 keys[4];
    short flexlm_version;
    short flexlm_revision;
    char flexlm_patch[2];
    char behavior_ver[5];
#if MWCCPSP
    char unk27[0x27C - 0x27]; // 3.0.1 links FlexLM 8: VENDORCODE is 0x27C bytes
#endif
} VENDORCODE;

typedef struct LPCODE_HANDLE {
    VENDORCODE* code;
    const char* vendor;
} LPCODE_HANDLE;

typedef struct LP_HANDLE LP_HANDLE;

extern int lp_checkout(
    LPCODE_HANDLE* code,
    int policy,
    const char* feature,
    const char* version,
    int nlic,
    const char* licensePath,
    LP_HANDLE** handle
);
extern void lp_checkin(LP_HANDLE* handle);
extern char* lp_errstring(LP_HANDLE* handle);

#ifdef __cplusplus
}
#endif

#endif
