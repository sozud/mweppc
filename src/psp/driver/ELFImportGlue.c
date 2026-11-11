#include "driver/CWPlugins.h"
#include "driver/CWPluginsPrivate.h"
#include "driver/DropInCompilerLinker.h"
#include "driver/COSToolsCLT.h"
#include <string.h>
#include <stdio.h>

// 3.0.1's static "ELF Importer" plugin (imports .o ELF objects): its
// information entries return records in the original's data (pinned).

#if MWCCPSP_NATIVE
// the native build has no exe data: the same records, from 0x5C6A42..
static const DropInFlags ELFImport_sFlags = {2, 'Comp', 9, 0x80400000, '????', 13};
static const char* const ELFImport_sDropInName = "ELF Importer";
static const char* const ELFImport_sDisplayName = "ELF Importer";
static CWDataType ELFImport_sCPU = '****';
static CWDataType ELFImport_sOS = '****';
static const CWTargetList ELFImport_sTargetList = {1, 1, &ELFImport_sCPU, 1, &ELFImport_sOS};
// clang-format off
static CWExtensionMapping ELFImport_sMappings[6] = {
    {'ELF ', ".o", 0},
    {'ELF ', "", 0},
    {0, ".elf", 0},
    {'MPLF', ".lib", 0},
    {'MPLF', ".a", 0},
    {'MPLF', "", 0}
};
// clang-format on
static const CWExtMapList ELFImport_sMappingList = {1, 6, ELFImport_sMappings};
static const CWPanelList ELFImport_sPanelList = {1, 0, NULL};
#else
extern const DropInFlags ELFImport_sFlags; // 0x5C6A42
extern const char* const ELFImport_sDropInName; // 0x5C6A64
extern const char* const ELFImport_sDisplayName; // 0x5C6A68
extern const CWTargetList ELFImport_sTargetList; // 0x5C6A74
extern const CWExtMapList ELFImport_sMappingList; // 0x5C6B72
extern const CWPanelList ELFImport_sPanelList; // 0x5C6B7A
#endif

extern void COS_SetPluginContext(CWPluginContext context);
extern CW_CALLBACK CWSetPluginAPI(CWPluginContext context, SInt32 api);

// The shell's (newer, packed) CWObjectData: this unit's CWObjectData is the
// compiler's older 0x28-byte record.
#pragma pack(push, 2)
typedef struct ELFObjectData {
    CWMemHandle objectdata;
    CWMemHandle browsedata;
    SInt32 reserved1;
    SInt32 codesize;
    SInt32 udatasize;
    SInt32 idatasize;
    SInt32 compiledlines;
    Boolean interfaceChanged;
    SInt32 reserved2;
    void* compilecontext;
    void* dependencies;
    short dependencyCount;
    CWFileSpec* objectfile;
} ELFObjectData;
#pragma pack(pop)

typedef struct ELFHeader {
    UInt8 e_ident[16];
    UInt16 e_type;
    UInt16 e_machine;
    UInt32 e_version;
    UInt32 e_entry;
    UInt32 e_phoff;
    UInt32 e_shoff;
    UInt32 e_flags;
    UInt16 e_ehsize;
    UInt16 e_phentsize;
    UInt16 e_phnum;
    UInt16 e_shentsize;
    UInt16 e_shnum;
    UInt16 e_shstrndx;
} ELFHeader;

typedef struct ELFSection {
    UInt32 sh_name;
    UInt32 sh_type;
    UInt32 sh_flags;
    UInt32 sh_addr;
    UInt32 sh_offset;
    UInt32 sh_size;
    UInt32 sh_link;
    UInt32 sh_info;
    UInt32 sh_addralign;
    UInt32 sh_entsize;
} ELFSection;

static ELFObjectData ELFImport_sNoObject; // 0x5E81C8
static int ELFImport_sDataEncoding; // 0x5E81F8 (EI_DATA: 1 little-endian)
static CWPluginContext ELFImport_sContext; // 0x5E81FC

#pragma auto_inline off

// 0x423E90
static void ELFImport_Error(int index) {
    char buf[256];
    COS_GetString(buf, 911, index);
    CWReportMessage(ELFImport_sContext, NULL, buf, NULL, messagetypeError, 0);
}

// 0x423ED0
static int ELFImport_LoadFile(const CWFileSpec* spec, CWMemHandle* handle, SInt32* size) {
    CWMemHandle h;
    void* ptr;
    SInt32 len;
    COSFileRef ref;
    int err;

    if (COS_FileOpen(spec, &ref)) {
        return 2;
    }
    COS_FileGetSize(ref, &len);
    if (CWAllocMemHandle(ELFImport_sContext, len, TRUE, &h) < 0) {
        COS_FileClose(ref);
        return 1;
    }
    CWLockMemHandle(ELFImport_sContext, h, FALSE, &ptr);
    err = COS_FileRead(ref, ptr, len);
    CWUnlockMemHandle(ELFImport_sContext, h);
    COS_FileClose(ref);
    if (err) {
        CWFreeMemHandle(ELFImport_sContext, h);
        return 3;
    }
    *handle = h;
    *size = len;
    return 0;
}

// 0x423FC0
static int ELFImport_IsELF(const UInt8* data) {
    if (data[0] != 0x7F) {
        return 0;
    }
    if (data[1] != 'E') {
        return 0;
    }
    if (data[2] != 'L') {
        return 0;
    }
    if (data[3] != 'F') {
        return 0;
    }
    ELFImport_sDataEncoding = data[5];
    return 1;
}

// 0x424000
static UInt16 ELFImport_Get16(const UInt8* p) {
    if (ELFImport_sDataEncoding == 1) {
        return (p[1] << 8) + p[0];
    } else {
        return (p[0] << 8) + p[1];
    }
}

// 0x424030
static UInt32 ELFImport_Get32(const UInt8* p) {
    if (ELFImport_sDataEncoding == 1) {
        return (p[3] << 24) + (p[2] << 16) + (p[1] << 8) + p[0];
    } else {
        return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
    }
}

// 0x424080
static void ELFImport_ReadHeader(const UInt8* data, ELFHeader* hdr) {
    int i;
    for (i = 0; i < 16; i++) {
        hdr->e_ident[i] = data[i];
    }
    hdr->e_type = ELFImport_Get16(data + 0x10);
    hdr->e_machine = ELFImport_Get16(data + 0x12);
    hdr->e_version = ELFImport_Get32(data + 0x14);
    hdr->e_entry = ELFImport_Get32(data + 0x18);
    hdr->e_phoff = ELFImport_Get32(data + 0x1C);
    hdr->e_shoff = ELFImport_Get32(data + 0x20);
    hdr->e_flags = ELFImport_Get32(data + 0x24);
    hdr->e_ehsize = ELFImport_Get16(data + 0x28);
    hdr->e_phentsize = ELFImport_Get16(data + 0x2A);
    hdr->e_phnum = ELFImport_Get16(data + 0x2C);
    hdr->e_shentsize = ELFImport_Get16(data + 0x2E);
    hdr->e_shnum = ELFImport_Get16(data + 0x30);
    hdr->e_shstrndx = ELFImport_Get16(data + 0x32);
}

// 0x4241A0
static void ELFImport_ReadSection(const UInt8* data, ELFSection* sec) {
    sec->sh_name = ELFImport_Get32(data);
    sec->sh_type = ELFImport_Get32(data + 4);
    sec->sh_flags = ELFImport_Get32(data + 8);
    sec->sh_addr = ELFImport_Get32(data + 0xC);
    sec->sh_offset = ELFImport_Get32(data + 0x10);
    sec->sh_size = ELFImport_Get32(data + 0x14);
    sec->sh_link = ELFImport_Get32(data + 0x18);
    sec->sh_info = ELFImport_Get32(data + 0x1C);
    sec->sh_addralign = ELFImport_Get32(data + 0x20);
    sec->sh_entsize = ELFImport_Get32(data + 0x24);
}

// 0x424230: sums the allocated sections' sizes
static int ELFImport_ScanObject(
    const UInt8* data, SInt32* codesize, SInt32* udatasize, SInt32* idatasize
) {
    ELFHeader hdr;
    ELFSection sec;
    int i;

    *codesize = 0;
    *udatasize = 0;
    *idatasize = 0;
    ELFImport_ReadHeader(data, &hdr);
    for (i = 1; i < hdr.e_shnum; i++) {
        ELFImport_ReadSection(data + hdr.e_shoff + hdr.e_shentsize * i, &sec);
        if (sec.sh_flags & 2) { // SHF_ALLOC
            if (sec.sh_type == 1) { // SHT_PROGBITS
                if (sec.sh_flags & 4) { // SHF_EXECINSTR
                    *codesize += sec.sh_size;
                } else {
                    *idatasize += sec.sh_size;
                }
            } else if (sec.sh_type == 8) { // SHT_NOBITS
                *udatasize += sec.sh_size;
            }
        }
    }
    return 1;
}

// 0x4242F0
static int ELFImport_IsArchive(const char* data) {
    return strncmp(data, "!<arch>\n", 8) == 0;
}

typedef struct ARHeader {
    char ar_name[16];
    char ar_date[12];
    char ar_uid[6];
    char ar_gid[6];
    char ar_mode[8];
    char ar_size[10];
    char ar_fmag[2];
} ARHeader;

// 0x424310: sums the sizes of an archive's ELF members
static int ELFImport_ScanArchive(
    const char* data, SInt32 size, SInt32* codesize, SInt32* udatasize, SInt32* idatasize
) {
    SInt32 offset;
    SInt32 code;
    SInt32 udata;
    const char* archive;
    SInt32 idata;
    long membersize;
    const ARHeader* header;
    const char* member;

    archive = data;
    *codesize = 0;
    *udatasize = 0;
    *idatasize = 0;
    for (offset = 8; offset < size; offset += (membersize + 0x3D) & ~1) {
        member = archive + offset;
        header = (const ARHeader*)member;
        if (strncmp(header->ar_fmag, "`\n", 2)) {
            return 0;
        }
        sscanf(header->ar_size, "%ld", &membersize);
        member += sizeof(ARHeader);
        if (ELFImport_IsELF((const UInt8*)member)) {
            if (!ELFImport_ScanObject((const UInt8*)member, &code, &udata, &idata)) {
                return 0;
            }
            *codesize += code;
            *udatasize += udata;
            *idatasize += idata;
        }
    }
    return 1;
}

// 0x424400
static void ELFImport_ImportFile(const CWFileSpec* spec, ELFObjectData* object) {
    CWMemHandle h;
    SInt32 size;
    void* ptr;
    int ok;
    int err;

    ok = 0;
    *object = ELFImport_sNoObject;
    if ((err = ELFImport_LoadFile(spec, &h, &size)) != 0) {
        ELFImport_Error(err);
        return;
    }
    if (size == 0) {
        ok = 0;
    } else {
        CWLockMemHandle(ELFImport_sContext, h, FALSE, &ptr);
        if (ELFImport_IsELF(ptr)) {
            ok = ELFImport_ScanObject(
                ptr, &object->codesize, &object->udatasize, &object->idatasize
            );
        } else if (ELFImport_IsArchive(ptr)) {
            ok = ELFImport_ScanArchive(
                ptr, size, &object->codesize, &object->udatasize, &object->idatasize
            );
            CWResizeMemHandle(ELFImport_sContext, h, 8);
        }
        CWUnlockMemHandle(ELFImport_sContext, h);
    }
    if (!ok) {
        ELFImport_Error(4);
        CWFreeMemHandle(ELFImport_sContext, h);
        h = NULL;
    }
    object->objectdata = h;
}

// 0x424540
CWPLUGIN_ENTRY(ELFImport_GetDropInFlags)(const DropInFlags** flags, SInt32* flagsSize) {
    *flags = &ELFImport_sFlags;
    *flagsSize = sizeof(DropInFlags);
    return 0;
}

// 0x424560
CWPLUGIN_ENTRY(ELFImport_GetDropInName)(const char** dropinName) {
    *dropinName = ELFImport_sDropInName;
    return 0;
}

// 0x424580
CWPLUGIN_ENTRY(ELFImport_GetDisplayName)(const char** displayName) {
    *displayName = ELFImport_sDisplayName;
    return 0;
}

// 0x4245A0
CWPLUGIN_ENTRY(ELFImport_GetTargetList)(const CWTargetList** targetList) {
    *targetList = &ELFImport_sTargetList;
    return 0;
}

// 0x4245B0
CWPLUGIN_ENTRY(ELFImport_GetDefaultMappingList)(const CWExtMapList** defaultMappingList) {
    *defaultMappingList = &ELFImport_sMappingList;
    return 0;
}

// 0x4245C0
CWPLUGIN_ENTRY(ELFImport_GetPanelList)(const CWPanelList** panelList) {
    *panelList = &ELFImport_sPanelList;
    return 0;
}

// 0x4245D0
CWPLUGIN_ENTRY(ELFImport_main)(CWPluginContext context) {
    CWFileSpec spec;
    ELFObjectData object;
    SInt32 request;
    SInt32 fileNumber;
    CWResult result;

    result = cwNoErr;
    ELFImport_sContext = context;
    CWGetPluginRequest(context, &request);
    switch (request) {
        case reqInitialize:
            CWSetPluginAPI(context, 2);
            break;
        case reqTerminate:
            break;
        case reqCompile:
            COS_SetPluginContext(context);
            CWGetMainFileSpec(context, &spec);
            CWGetMainFileNumber(context, &fileNumber);
            ELFImport_ImportFile(&spec, &object);
            result = CWDonePluginRequest(
                context,
                object.objectdata ? CWStoreObjectData(context, fileNumber, (CWObjectData*)&object)
                                  : cwErrRequestFailed
            );
            break;
    }
    return result;
}
