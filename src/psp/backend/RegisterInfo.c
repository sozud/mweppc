#include "compiler/common.h"
#include "psp/BE.h"
#include "compiler/CError.h"
#include "compiler/objects.h"

#line 1 "RegisterInfo.c"
// mwccpsp 3.0.1 RegisterInfo.c (named by its assertions).

// only locals have register info in 3.0.1
VarInfo* Registers_GetVarInfo(Object* obj) {
    switch (obj->datatype) {
#if MWCCPS2
        case DDATA: // b210: data objects carry register info too
            if (!obj->u.data.info) {
                CError_FATAL(821);
            }
            return obj->u.data.info;
#endif
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

#if MWCCPS2
#include "compiler/CParser.h"
#include "compiler/CMachine.h"
#include "compiler/types.h"

extern char data_00625ECC; // register class being colored (Coloring.c)

// 0x4727F0: is obj a data object that must live in a quadword (128-bit)
// register? (b210's data objects carry a VarInfo at +0x3C)
// (mwccpsp's stub at 0x4B8980 always says no)
Boolean func_004B8980(Object* obj) {
    VarInfo* vi;

    if (copts.mips_option43 &&
        obj != NULL &&
        (vi = obj->u.data.info) != NULL &&
        obj->datatype == DDATA) {
        if (vi->reg != 0 || vi->unk24[2] == 5 || (vi->unk24[3] != 0 && vi->unk24[2] == 0)) {
            return TRUE;
        }
    }
    return FALSE;
}

// 0x472850 (mwccpsp's 0x4B89A0, Coloring.c): may a and b be coalesced in
// the current class? GPRs and, with VU0 registers enabled, class 5
Boolean func_004B89A0(SInt32 a, SInt32 b) {
    if (data_00625ECC == 0) {
        return (a == 0) || (b == 0);
    }
    if (copts.enable_vu0_registers && data_00625ECC == 5) {
        return (a == 0) || (b == 0);
    }
    return 0;
}

// 0x4728A0 (mwccpsp's 0x4B89D0, Coloring.c): the type a spill slot of the
// current class holds; GPRs and class 5 spill all 128 bits
Type* func_004B89D0(void* node) {
    if (data_00625ECC == 0) {
        return TYPE(&stint128);
    }
    if (copts.enable_vu0_registers) {
        if (data_00625ECC == 5) {
            return TYPE(&stint128);
        }
    }
    if (!copts.enable_vu0_registers && data_00625ECC == 5) {
        return TYPE(&stint128);
    }
    if (copts.floatsp32) {
        return TYPE(&stfloat);
    }
    return TYPE(&stlongdouble);
}
#else
Boolean func_004B8980(void) {
    return 0;
}
#endif

extern SInt32 data_006241E0[7]; // registers per class

SInt32 func_004B8990(void) {
    return data_006241E0[5]; // 0x6241F4: class 5's register count
}
