#ifndef COMPILER_OBJECTS_H
#define COMPILER_OBJECTS_H

#include "compiler/common.h"
#include "compiler/tokens.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OBJ_BASE(obj) ((ObjBase*)(obj))
#define OBJ_ENUM_CONST(obj) ((ObjEnumConst*)(obj))
#define OBJ_TYPE(obj) ((ObjType*)(obj))
#define OBJ_MEMBER_VAR(obj) ((ObjMemberVar*)(obj))
#define OBJ_TYPE_TAG(obj) ((ObjTypeTag*)(obj))
#define OBJ_NAMESPACE(obj) ((ObjNameSpace*)(obj))
#define OBJ_MEMBER_VAR_PATH(obj) ((ObjMemberVarPath*)(obj))
#define OBJECT(obj) ((Object*)(obj))
enum {
    OT_ENUMCONST,
    OT_TYPE,
    OT_TYPETAG,
    OT_NAMESPACE,
    OT_MEMBERVAR,
    OT_OBJECT,
    OT_ILLEGAL
};
enum {
    DDATA,
    DLOCAL,
    DABSOLUTE,
    DFUNC,
    DVFUNC,
    DINLINEFUNC,
    DALIAS,
    DEXPR,
    DLABEL
};
typedef UInt16 Section;
#define SECT_DEFAULT 0
#define SECT_TEXT 1
#define SECT_DATA 2
#define SECT_OBJC_CAT_CLS_METH 7
#define SECT_OBJC_CAT_INST_METH 8
#define SECT_OBJC_STRING_OBJECT 9
#define SECT_OBJC_CSTRING_OBJECT 10
#define SECT_OBJC_MESSAGE_REFS 11
#define SECT_OBJC_SEL_FIXUP 12
#define SECT_OBJC_CLS_REFS 13
#define SECT_OBJC_CLASS 14
#define SECT_OBJC_META_CLASS 15
#define SECT_OBJC_CLS_METH 16
#define SECT_OBJC_INST_METH 17
#define SECT_OBJC_PROTOCOL 18
#define SECT_OBJC_CLASS_NAMES 19
#define SECT_OBJC_METH_VAR_TYPES 20
#define SECT_OBJC_METH_VAR_NAMES 21
#define SECT_OBJC_CATEGORY 22
#define SECT_OBJC_CLASS_VARS 23
#define SECT_OBJC_INSTANCE_VARS 24
#define SECT_OBJC_MODULE_INFO 25
#define SECT_OBJC_MSYMBOLS 26
#define SECT_CONST_TYPE 0x20
#define SECT_SDATA_TYPE 0x21
#define SECT_SCONST_TYPE 0x22
enum {
    OBJECT_USED = 1,
    OBJECT_REFERENCED = 2,
    OBJECT_DEFINED = 4,
    OBJECT_LAZY = 8,
    OBJECT_INTERNAL = 0x10,
    OBJECT_IMPORT = 0x20,
    OBJECT_EXPORT = 0x40,
    OBJECT_LIB_EXPORT = 0x60
};
#if MWCCPSP
// 3.0.1: natural alignment, and every object's access is a single byte
typedef UInt8 ObjAccess;
#else
#pragma pack(push, 2)
#endif
struct ObjectList {
    ObjectList* next;
    Object* object;
};
struct ObjBase {
    UInt8 otype;
#if MWCCPSP
    ObjAccess access;
#else
    AccessType access;
#endif
};
struct ObjEnumConst {
    UInt8 otype;
#if MWCCPSP
    ObjAccess access;
#else
    AccessType access;
#endif
    ObjEnumConst* next;
    HashNameNode* name;
    Type* type;
    CInt64 val;
};
struct ObjNameSpace {
    UInt8 otype;
#if MWCCPSP
    ObjAccess access;
#else
    AccessType access;
#endif
    NameSpace* nspace;
};
struct ObjType {
    UInt8 otype;
#if MWCCPSP
    ObjAccess access;
#else
    AccessType access;
#endif
    Type* type;
    UInt32 qual;
};
struct ObjMemberVar {
    UInt8 otype;
#if MWCCPSP
    ObjAccess access;
#else
    AccessType access;
#endif
    Boolean anonunion;
    Boolean has_path;
    ObjMemberVar* next;
    HashNameNode* name;
    Type* type;
    UInt32 qual;
    UInt32 offset;
};
struct ObjTypeTag {
    UInt8 otype;
#if MWCCPSP
    ObjAccess access;
#else
    AccessType access;
#endif
    Type* type;
};
struct ObjMemberVarPath {
    UInt8 otype;
#if MWCCPSP
    ObjAccess access;
#else
    AccessType access;
#endif
    Boolean anonunion;
    Boolean has_path;
    ObjMemberVar* next;
    HashNameNode* name;
    Type* type;
    UInt32 qual;
    UInt32 offset;
    BClassList* path;
};
struct InlineXRef {
    struct InlineXRef* next;
    Object* object;
    UInt16 xrefoffset;
    UInt16 numxrefs;
    struct {
        UInt32 offset;
        SInt32 varoffset;
    } xref[1];
};
#if MWCCPSP
#pragma pack(push, 2) // Object and VarInfo below are laid out under it
#endif
struct Object {
    UInt8 otype;
#if MWCCPSP
    // 3.0.1: access and section are single bytes (CParser_NewObject: access
    // at +1, section at +3; sizeof(Object) is 0x40)
    UInt8 access;
    UInt8 datatype;
    UInt8 section;
#else
    AccessType access;
    UInt8 datatype;
    UInt16 section;
#endif
    NameSpace* nspace;
    HashNameNode* name;
    Type* type;
    UInt32 qual;
    SInt16 sclass;
    UInt8 flags;
    struct ExtendedParam* extParam;
    union {
        struct Object* toc;
        struct DwarfNode* pendingEntry;
    };
    union {
        struct DwarfSymbol* symbol;
        struct DwarfNode* entry;
    } debugInfo;
    struct VarRecord* varptr;
    union {
        struct {
            union {
                CInt64 intconst;
                Float* floatconst;
                char* string;
                struct {
                    char* data;
                    SInt32 size;
                } switchtable;
            } u;
#if MWCCPSP
            // 3.0.1: the link name follows the value (CMangler_GetLinkName
            // reads it at +0x30, a switch table's size is at +0x2C); there
            // is no register info (CParser_UpdateObject does not copy it).
            HashNameNode* linkname;
            Boolean islocalstatic; // +0x34 (add_aliased_locals_to_worst_case)
            union {
                VarInfo* info;
                struct InterruptGenerationRecord* interruptInfo;
            };
#else
            union {
                VarInfo* info;
                struct InterruptGenerationRecord* interruptInfo;
            };
#if VERSION < VERSION_GC_2_5
            ObjRegInfo reginfo;
#endif
            HashNameNode* linkname;
#if VERSION >= VERSION_GC_2_5
            ObjRegInfo reginfo;
#endif
            Boolean islocalstatic;
#endif
        } data;
        struct {
            union {
                TemplateFunction* templ;
                struct CI_FuncData* ifuncdata;
                SInt32 intrinsicid;
            } u;
            DefArgCtorInfo* defargdata;
            HashNameNode* linkname;
            TemplFuncInstance* inst;
            struct PTFList* ptfList;
            ObjectList* argList;
#if !MWCCPSP // 3.0.1: gone, sizeof(Object) is 0x40
            UInt8 interruptflags;
#endif
        } func;
        struct {
            VarInfo* info;
            SInt32 uid;
#if MWCCPSP
            union {
                struct Object* realObj;
                SInt32 offset; // 3.0.1: a local's frame offset (0x4A4530)
            };
#else
            struct Object* realObj;
#endif
        } var;
        struct {
            UInt8* data;
            SInt32 size;
            InlineXRef* xrefs;
        } ifunc;
        struct {
            Object* object;
            BClassList* member;
            SInt32 offset;
        } alias;
        SInt16 intrinsic;
        ENode* expr;
        UInt32 address;
#if MWCCPSP
        struct {
            struct Object* function; // a label object's function (IRO_CollectAddressedLabels)
        } label;
#endif
    } u;
};
struct ObjectTemplated {
    Object object;
    Object* parent;
#if MWCCPSP
    // 3.0.1: the instance stack when a templated function was first referenced
    // (CInline_ObjectAddrRef; sizeof is 0x48 in CTemplClass_CopyObject)
    TemplStack* inststack;
#endif
};
struct VarInfo {
    struct Object* func;
    SInt32 usage;
    TStreamElement deftoken;
    SInt16 varnumber;
    Boolean noregister;
    Boolean used;
#if MWCCPSP
    UInt8 unk24
        [2]; // 3.0.1: flags at +0x26, rclass +0x27, reg +0x28, regHi +0x2A (Coloring.c, RegClasses.c)
#endif
    UInt8 flags;
    UInt8 rclass;
    SInt16 reg;
    SInt16 regHi;
#if MWCCPSP
    UInt8 unk2C[4]; // 3.0.1: CodeGen_GetNewVarInfo allocates 0x30
#endif
};
#pragma pack(pop)
enum {
    VarInfoFlag_ParamArea = 1,
    VarInfoFlag_InReg = 2,
    VarInfoFlag_RegPair = 4,
    VarInfoFlag_TempInUse = 0x20,
    VarInfoFlag_AsmNamed = 0x40,
    VarInfoFlag_HasMemory = 0x80
};

#define OBJECT_TEMPL(obj) ((ObjectTemplated*)(obj))

HEADER_INLINE Boolean Inline_IsObjectData(Object* object) {
    return object->datatype == DDATA || object->datatype == DABSOLUTE;
}

#ifdef __cplusplus
}
#endif

#endif
