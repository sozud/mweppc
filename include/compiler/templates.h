#include "compiler/mwcc_core.h"
#ifndef COMPILER_TEMPLATES_H
#define COMPILER_TEMPLATES_H

#include "compiler/common.h"
#include "compiler/types.h"

#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 2)
struct PackedDeclInfo {
    Type* thetype;
#else
#ifdef __cplusplus
extern "C" {
#endif

#if !MWCCPSP // 3.0.1: natural alignment
#pragma pack(push, 2)
#endif
struct PackedDeclInfo {
    Type* thetype;
#endif
    UInt32 qual;
    NameSpace* nspace;
    HashNameNode* name;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    TemplArg* expltargs;
    SInt16 storageclass;
    UInt8 section;
#else
    TemplArg* expltargs;
    SInt16 storageclass;
#if MWCCPSP
    UInt8 section; // a byte in 3.0.1, as in DeclInfo
#else
    SInt16 section;
#endif
#endif
    UInt8 exportflags;
    Boolean has_expltargs;
};

struct TemplateFriend {
    PackedDeclInfo decl;
    FileOffsetInfo fileoffset;
    TokenStream stream;
};

struct TemplateMember {
    TemplateMember* next;
    TemplParam* params;
    Object* object;
    FileOffsetInfo fileoffset;
    TokenStream stream;
    CPrepFileInfo* srcfile;
    SInt32 startoffset;
    SInt32 endoffset;
#if !MWCC_CORE || MWCC_CORE > MWCC_CORE_B52
#if MWCCPS2
    void* unk30; // b210 allocates 0x34 bytes (CTemplClass_DefineMember)
#endif
#endif
};

struct TemplParam {
    TemplParam* next;
    HashNameNode* name;
    TemplParamID pid;
    union {
        struct {
            Type* type;
            UInt32 qual;
        } typeparam;
        struct {
            Type* type;
            UInt32 qual;
            ENode* defaultarg;
        } paramdecl;
        struct {
            TemplParam* plist;
            Type* defaultarg;
        } templparam;
    } data;
};

struct TemplArg {
    TemplArg* next;
    TemplParamID pid;
    union {
        struct {
            Type* type;
            UInt32 qual;
        } typeparam;
        struct {
            ENode* expr;
            Boolean is_ref;
        } paramdecl;
        Type* ttargtype;
    } data;
    Boolean is_deduced;
};

struct DeduceInfo {
    TemplArg* args;
    TemplArg argBuffer[16];
    int maxCount;
    int count;
    UInt8 nindex;
};

struct DefAction {
    DefAction* next;
    TemplateAction* action;
    ObjBase* refobj;
    int xC;
    TypeEnum* enumtype;
};
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52

struct TypeDeduce {
    TemplClass* tmclass;
    TemplClassInst* inst;
    TemplParam* params;
    TemplArg* args;
    DefAction* defActions;
    Boolean processingClassTypes;
    Boolean processingArgument;
    Boolean suppressErrors;
    Boolean hasNewVBases;
    UInt8 nindex;
#else

struct TypeDeduce {
    TemplClass* tmclass;
    TemplClassInst* inst;
    TemplParam* params;
    TemplArg* args;
    DefAction* defActions;
#if MWCCPS2
    // b210: 0x20 bytes (CTempl_InstantiateTemplateClass clears 0x20)
    void* unk14;
    Boolean processingArgument; // +0x18 (CTemplClass_CopyMemberTemplate)
    Boolean unk19;
    Boolean hasNewVBases; // +0x1A (CTemplClass_CopyObject)
    Boolean suppressErrors; // +0x1B (unverified)
    Boolean unk1C;
    Boolean processingClassTypes; // +0x1D (unverified: 0x4B5090 sets it on a fresh state)
    UInt8 unk1E;
    UInt8 nindex; // +0x1F (CTemplClass_CopyMemberTemplate)
#else
    Boolean processingClassTypes;
    Boolean processingArgument;
    Boolean suppressErrors;
    Boolean hasNewVBases;
    UInt8 nindex;
#endif
#endif
};

struct TemplPartialSpec {
    TemplPartialSpec* next;
    TemplClass* templ;
    TemplArg* args;
};

struct TemplStack {
    TemplStack* next;
    union {
        Object* func;
        TypeClass* theclass;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    } u;
    Boolean is_func;
    Boolean is_poi;
};

struct TemplClass {
    TypeClass theclass;
    TemplClass* next;
    TemplClass* templ_parent;
    TemplClassInst* inst_parent;
    TemplParam* templ__params;
    TemplateMember* members;
    TemplClassInst* instances;
    TemplClass* pspec_owner;
    TemplPartialSpec* pspecs;
    TemplateAction* actions;
    UInt16 lex_order_count;
    SInt8 align;
    UInt8 flags;
};

enum {
    TEMPLCLASS_HAS_VTABLE = 1,
    TEMPLCLASS_IS_SPECIALIZATION = 2
};

struct TemplClassInst {
    TypeClass theclass;
    TemplClassInst* next;
    TemplClassInst* parent;
    TemplClass* templ;
    TemplArg* inst_args;
    TemplArg* oargs;
    Boolean is_instantiated;
    Boolean is_specialized;
    Boolean is_extern;
    Boolean static_instantiated;
};

#define TEMPL_CLASS(ty) ((TemplClass*)(ty))
#define TEMPL_CLASS_INST(ty) ((TemplClassInst*)(ty))
#define IS_TEMPL_CLASS(ty) (IS_TYPE_CLASS(ty) && (TYPE_CLASS(ty)->flags & CLASS_IS_TEMPL))
#define IS_TEMPL_CLASS_INST(ty) (IS_TYPE_CLASS(ty) && (TYPE_CLASS(ty)->flags & CLASS_IS_TEMPL_INST))

struct TemplateFunction {
    TemplateFunction* next;
    TemplateFunction* original;
    HashNameNode* name;
    TemplParam* params;
    TokenStream stream;
    TStreamElement deftoken;
    Object* tfunc;
    TemplFuncInstance* instances;
    CPrepFileInfo* srcfile;
    SInt32 startoffset;
    SInt32 endoffset;
};

struct TemplFuncInstance {
    TemplFuncInstance* next;
    Object* object;
    TemplArg* args;
    Boolean is_instantiated;
    Boolean is_specialized;
    Boolean is_extern;
};
#else
    } u;
    Boolean is_func;
    Boolean is_poi;
};

// b210 and b198 reach a class template instance's template, an instance's
// enclosing instance and a nested member template's original through 16-byte
// "template headers" (the block at TemplClass.templ_parent / TemplClassInst.parent;
// CTemplTool_TemplClassFromHeader). The CodeWarrior build keeps that representation
// (it is the originals' code); a native build (MWCCPSP_NATIVE) keeps 3.0.1 b219's plain pointers,
// which give the same results (an internal representation only).
#if MWCCPS2 && !MWCCPSP_NATIVE
#define MWCCPS2_TEMPL_HEADERS 1
#else
#define MWCCPS2_TEMPL_HEADERS 0
#endif

struct TemplClass {
    TypeClass theclass;
    TemplClass* next;
    TemplClass* templ_parent;
    TemplClassInst* inst_parent;
    TemplParam* templ__params;
#if MWCCPS2
    // b210: members at +0x44 (CTemplClass_DefineMember 0x553700), pspec_owner
    // at +0x48 (CTempl_ClassGetType 0x4F5470)
    // +0x30..+0x3F is a template header that instances and nested member
    // templates point at (CTemplTool_TemplClassFromHeader); +0x3C marks a
    // class template
    Boolean is_templ_header;
    TemplClassInst* instances;
    TemplateMember* members;
    TemplClass* pspec_owner;
#else
    TemplateMember* members;
    TemplClassInst* instances;
    TemplClass* pspec_owner;
#endif
#if MWCCPSP && !MWCCPS2
    void* unk48; // 3.0.1: pspecs is at +0x4C (CTempl_Instantiate), actions at +0x50
#endif
    TemplPartialSpec* pspecs;
    TemplateAction* actions;
    UInt16 lex_order_count;
    SInt8 align;
    UInt8 flags;
};

enum {
    TEMPLCLASS_HAS_VTABLE = 1,
    TEMPLCLASS_IS_SPECIALIZATION = 2
};

struct TemplClassInst {
    TypeClass theclass;
    TemplClassInst* next;
    TemplClassInst* parent;
    TemplClass* templ;
    TemplArg* inst_args;
#if MWCCPSP
    // 3.0.1: the flags come before oargs (is_specialized at +0x3D in
    // CTempl_Instantiate, oargs at +0x40 in CError_BufferAppendType)
    Boolean is_instantiated;
    Boolean is_specialized;
    Boolean is_extern;
    Boolean static_instantiated;
    TemplArg* oargs;
#else
    TemplArg* oargs;
    Boolean is_instantiated;
    Boolean is_specialized;
    Boolean is_extern;
    Boolean static_instantiated;
#endif
};

#define TEMPL_CLASS(ty) ((TemplClass*)(ty))
#define TEMPL_CLASS_INST(ty) ((TemplClassInst*)(ty))
#define IS_TEMPL_CLASS(ty) (IS_TYPE_CLASS(ty) && (TYPE_CLASS(ty)->flags & CLASS_IS_TEMPL))
#define IS_TEMPL_CLASS_INST(ty) (IS_TYPE_CLASS(ty) && (TYPE_CLASS(ty)->flags & CLASS_IS_TEMPL_INST))

#if MWCCPSP
// 3.0.1 order, 0x50 bytes (CTempl_DeclareTemplateFunction 0x4E6D50,
// CTemplClass_CopyMemberTemplate 0x541680, CTempl_ParseTemplateFunction 0x4E6BD0)
struct TemplateFunction {
    TemplateFunction* original; // +0x00 (a member template: its class template's +0x30)
    void* unk04; // +0x04 the copying class instance
    TemplParam* params; // +0x08
    void* unk0C;
    TemplateFunction* next; // +0x10
    HashNameNode* name; // +0x14
    TokenStream stream; // +0x18
    TStreamElement deftoken; // +0x20
    void* unk38;
    Object* tfunc; // +0x3C
    TemplFuncInstance* instances; // +0x40
    CPrepFileInfo* srcfile; // +0x44
    SInt32 startoffset; // +0x48
    SInt32 endoffset; // +0x4C
};
#else
struct TemplateFunction {
    TemplateFunction* next;
    TemplateFunction* original;
    HashNameNode* name;
    TemplParam* params;
    TokenStream stream;
    TStreamElement deftoken;
    Object* tfunc;
    TemplFuncInstance* instances;
    CPrepFileInfo* srcfile;
    SInt32 startoffset;
    SInt32 endoffset;
};
#endif

#if MWCCPSP
// 3.0.1, 0x1C bytes (CTempl_GetCreateFuncInstance 0x54EE20, CTempl_Instantiate)
struct TemplFuncInstance {
    TemplateFunction* templ; // +0x00
    void* unk04; // +0x04 a member template's class instance (+0x30)
    TemplArg* args; // +0x08
    Boolean is_instantiated; // +0x0C
    Boolean is_specialized; // +0x0D
    Boolean is_extern; // +0x0E
    TemplFuncInstance* next; // +0x10
    struct TemplStack* instStack; // +0x14 CTemplTool_GetInstanceStack()
    Object* object; // +0x18
};
#else
struct TemplFuncInstance {
    TemplFuncInstance* next;
    Object* object;
    TemplArg* args;
    Boolean is_instantiated;
    Boolean is_specialized;
    Boolean is_extern;
};
#endif
#endif

#define IS_TEMPL_FUNC(ty) (IS_TYPE_FUNC(ty) && (TYPE_FUNC(ty)->flags & FUNC_IS_TEMPL))

typedef enum TemplateActionType {
    TAT_NESTEDCLASS,
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    TAT_ENUMTYPE,
    TAT_FRIEND,
    TAT_ENUMERATOR,
#else
    TAT_ENUMTYPE,
    TAT_FRIEND,
#if !MWCCPSP
    TAT_ENUMERATOR,
#endif
#endif
    TAT_BASE,
    TAT_OBJECTINIT,
    TAT_USINGDECL,
    TAT_OBJECTDEF,
#if !MWCC_CORE || MWCC_CORE > MWCC_CORE_B52
#if MWCCPSP
    // 3.0.1 has no enumerator action (its template enums are handled in
    // another way, not ported yet); ours is kept past the 3.0.1 values
    TAT_ENUMERATOR,
#endif
#endif
    TAT_ILLEGAL
} TemplateActionType;

struct TemplateAction {
    TemplateAction* next;
    TStreamElement source_ref;
    union {
        TemplClass* tclasstype;
        TypeEnum* enumtype;
        TemplateFriend* tfriend;
        struct {
            ObjEnumConst* objenumconst;
            ENode* initexpr;
        } enumerator;
        struct {
            Type* type;
            ClassList* insert_after;
            AccessType access;
            Boolean is_virtual;
        } base;
        struct {
            Object* object;
            ENode* initexpr;
        } objectinit;
        struct {
            TypeTemplDep* type;
            AccessType access;
        } usingdecl;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
        ObjBase* refobj;
    } u;
    TemplateActionType type;
#else
        ObjBase* refobj;
    } u;
#if MWCCPSP
    UInt8 type; // TemplateActionType
#else
    TemplateActionType type;
#endif
#endif
};

struct TemplateScopeState {
    NameSpace* scope;
#if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52
    NameSpace* linkedNamespace;
};
#pragma pack(pop)
#else
    NameSpace* linkedNamespace;
};
#if !MWCCPSP
#pragma pack(pop)
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
