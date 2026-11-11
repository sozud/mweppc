// 3.0.1 IroVars.c consistency checks (0x55A3C0, 0x55A3E0), compiled out of
// this build: they follow IRO_FindVar in the original and are not inlined
// into it, so they live in their own unit here.
#include "compiler/IroVars.h"
#include "compiler/objects.h"

// 0x558B40, called (not inlined) by IRO_FindVar and IRO_FindAllVars
Boolean IRO_IsLocalObject(Object* object) {
    return object->datatype == DLOCAL;
}

void IRO_CheckVars(void* list) {
    VarRecord* var;

    for (var = IRO_FirstVar; var; var = var->next) {}
}

void IRO_CheckVar(void* list, VarRecord* var) {}
