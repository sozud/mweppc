#ifndef COMPILER_CSCOPE_H
#define COMPILER_CSCOPE_H

#include "compiler/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 2)
struct CScopeSave {
    struct NameSpace* current;
    struct TypeClass* currentclass;
    struct Object* currentfunc;
    Boolean is_member_func;
};
struct NameResult {
    struct NameSpace* nspace;
    struct HashNameNode* name;
    struct Type* type;
    UInt32 qual;
    struct ObjBase* object;
    struct NameSpaceObjectList* objects;
    struct BClassList* basePath;
#if MWCCPSP
    // 3.0.1: 0x28 bytes, is_qualified at +0x22. One pointer after objects
    // and one Boolean before is_qualified are new; where exactly they sit
    // among their neighbours is not settled by the matched code.
    void* unk1C;
    UInt8 access; // +0x20: a found type's access (CScope_SetupParseNameResult)
#endif
    Boolean is_destructor;
    Boolean is_qualified;
    Boolean isambig;
    Boolean unused;
    Boolean is_type;
    Boolean is_conv_operator;
#if MWCCPSP_NATIVE
    // 3.0.1 keeps only the ends of the path to a class member: basePath holds
    // the most derived class and unk1C the class the member is in. Code here
    // still written the 2.4.7 way reads the whole path from this field (see
    // NAMERESULT_PATH).
    struct BClassList* path;
#endif
};
#pragma pack(pop)
#if MWCCPSP

// the path to a class member, for code written the 2.4.7 way
#if MWCCPSP_NATIVE
#define NAMERESULT_PATH(pr) ((pr)->path)
#else
#define NAMERESULT_PATH(pr) ((pr)->basePath)
#endif
#endif
struct CScopeObjectIterator {
    struct NameSpace* nspace;
    struct NameSpaceName* nextname;
    struct NameSpaceObjectList* currlist;
    int hashindex;
};

extern Boolean cscope_is_member_func;
extern Object* cscope_currentfunc;
extern TypeClass* cscope_currentclass;
extern NameSpace* cscope_current;
extern NameSpace* cscope_root;

extern void CScope_Setup(void);
extern void CScope_Cleanup(void);
extern void CScope_GetScope(CScopeSave* save);
extern void CScope_SetNameSpaceScope(NameSpace* nspace, CScopeSave* save);
extern void CScope_SetClassScope(TypeClass* cls, CScopeSave* save);
extern void CScope_SetClassDefScope(TypeClass* cls, CScopeSave* save);
extern void CScope_SetFunctionScope(Object* function, CScopeSave* save);
extern void CScope_SetMethodScope(
    Object* function, TypeClass* cls, Boolean is_static, CScopeSave* save
);
extern void CScope_RestoreScope(CScopeSave* saved);
extern Boolean CScope_IsEmptySymTable(void);
extern Boolean CScope_IsInLocalNameSpace(NameSpace* nspace);
extern NameSpaceObjectList* CScope_FindName(NameSpace* nspace, HashNameNode* name);
extern NameSpaceName* CScope_FindNameSpaceName(NameSpace* nspace, HashNameNode* name);
extern NameSpaceObjectList* CScope_InsertName(NameSpace* nspace, HashNameNode* name);
extern NameSpaceObjectList* CScope_ArgumentDependentNameLookup(
    NameSpaceObjectList* list, HashNameNode* name, ENodeList* argNodes, Boolean skipMethods
);
extern NameSpace* CScope_NewHashNameSpace(HashNameNode* name);
extern NameSpace* CScope_NewListNameSpace(HashNameNode* name, Boolean is_global);
extern NameSpace* CScope_FindNonClassNonFunctionNS(NameSpace* nspace);
extern NameSpace* CScope_FindGlobalNS(NameSpace* nspace);
extern Boolean CScope_IsEmptyNameSpace(NameSpace* nspace);
extern void CScope_MergeNameSpace(NameSpace* dst, NameSpace* src);
extern void CScope_AddObject(NameSpace* nspace, HashNameNode* name, ObjBase* obj);
extern void CScope_AddGlobalObject(Object* obj);
extern Boolean CScope_FindQualifiedClassMember(
    NameResult* result, TypeClass* tclass, HashNameNode* name
);
extern Type* CScope_GetType(NameSpace* nspace, HashNameNode* name, UInt32* qual);
extern Type* CScope_GetTagType(NameSpace* nspace, HashNameNode* name);
extern Boolean CScope_ParseExprName(NameResult* result);
#if MWCCPSP
// 3.0.1 takes the namespace to look in (NULL: the current scope)
extern Boolean CScope_ParseDeclName(NameResult* result, NameSpace* nspace);
#define CSCOPE_PARSEDECLNAME(result) CScope_ParseDeclName(result, NULL)
// 3.0.1 takes the namespace to start from (callers pass NULL)
extern Boolean CScope_ParseQualifiedNameSpace(
    NameResult* result, NameSpace* nspace, Boolean allowOperator, Boolean noTemplateArgs
);
#define CSCOPE_PARSEQUALNS(result, a, b) CScope_ParseQualifiedNameSpace(result, NULL, a, b)
#else
extern Boolean CScope_ParseDeclName(NameResult* result);
extern Boolean CScope_ParseQualifiedNameSpace(
    NameResult* result, Boolean allowOperator, Boolean noTemplateArgs
);
#endif
extern Boolean CScope_ParseElaborateName(NameResult* result);
#if MWCCPSP
extern Boolean CScope_ParseTypeName(NameResult* result);
#endif
extern Boolean CScope_FindNonClassObject(NameSpace* nspace, NameResult* result, HashNameNode* name);
extern NameSpaceObjectList* CScope_FindObjectList(NameResult* result, HashNameNode* name);
extern Boolean CScope_PossibleTypeName(HashNameNode* name);
extern Boolean CScope_FindClassMemberObject(
    TypeClass* tclass, NameResult* result, HashNameNode* name
);
extern void CScope_InitObjectIterator(CScopeObjectIterator* iterator, NameSpace* nspace);
extern ObjBase* CScope_NextObjectIteratorObject(CScopeObjectIterator* iterator);
extern NameSpaceObjectList* CScope_NextObjectIteratorObjectList(CScopeObjectIterator* iterator);
#if MWCCPSP
extern struct ObjTypeTag* CScope_DefineTypeTag(NameSpace* nspace, HashNameNode* name, Type* type);
#else
extern void CScope_DefineTypeTag(NameSpace* nspace, HashNameNode* name, Type* type);
#endif
extern Type* CScope_GetLocalTagType(NameSpace* nspace, HashNameNode* name);
extern Boolean CScope_FindTypeName(NameSpace* nspace, HashNameNode* name, NameResult* result);
extern NameSpaceObjectList* CScope_GetLocalObject(NameSpace* nspace, HashNameNode* name);
extern BClassList* CScope_GetClassAccessPath(BClassList* list, TypeClass* tclass);
extern Boolean CScope_ParseMemberName(TypeClass* tclass, NameResult* result, Boolean isTemplate);
extern void CScope_AddClassUsingDeclaration(
    TypeClass* tclass, TypeClass* tclass2, HashNameNode* name, AccessType access
);
extern void CScope_ParseUsingDeclaration(NameSpace* nspace, AccessType access, Boolean unused);
extern void CScope_ParseNameSpaceAlias(HashNameNode* name);
extern void CScope_ParseUsingDirective(NameSpace* nspace);

#ifdef __cplusplus
}
#endif

#endif
