#ifndef COMPILER_SWITCH_H
#define COMPILER_SWITCH_H

#include "compiler/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MWCCPSP
// 3.0.1 (scancase 0x46D1E0, the case statement 0x46CF30): cases are ranges
// (`case a ... b`), and case labels whose values depend on template
// arguments are kept on a separate list until instantiation.
struct SwitchCase {
    struct SwitchCase* next;
    struct CLabel* label;
    CInt64 min;
    CInt64 max;
};

struct SwitchInfo {
    struct SwitchCase* cases;
    void* depcases;
    struct CLabel* defaultlabel;
    Type* exprtype;
};
#else
struct SwitchCase {
    struct SwitchCase* next;
    struct CLabel* label;
    CInt64 min;
};

struct SwitchInfo {
    struct SwitchCase* cases;
    struct CLabel* defaultlabel;
    Type* exprtype;
};
#endif

extern ObjectList* switchtables;

extern void switchstatement(ENode* expr, struct SwitchInfo* info);
extern void dumpswitchtables(Object* funcobj);

#ifdef __cplusplus
}
#endif

#endif
