#include "compiler/common.h"
#include "psp/BE.h"
#include "compiler/CError.h"
#include "compiler/objects.h"

#line 1 "RegisterInfo.c"
// mwccpsp 3.0.1 RegisterInfo.c (named by its assertions).

// only locals have register info in 3.0.1
VarInfo* Registers_GetVarInfo(Object* obj) {
    switch (obj->datatype) {
        case DLOCAL:
            if (!obj->u.var.info) {
                CError_FATAL(834);
            }
            return obj->u.var.info;
        default:
            CError_FATAL(839);
            return NULL;
    }
}

Boolean func_004B8980(void) {
    return 0;
}

extern SInt32 data_006241E0[7]; // registers per class

SInt32 func_004B8990(void) {
    return data_006241E0[5]; // 0x6241F4: class 5's register count
}
