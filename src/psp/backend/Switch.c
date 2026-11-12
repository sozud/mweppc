#include "compiler/common.h"
#include "psp/BE.h"
#include "compiler/CError.h"
#include "compiler/CInt64.h"
#include "compiler/CDecl.h"
#include "compiler/CMachine.h"
#include "compiler/CParser.h"
#include "compiler/CompilerTools.h"
#include "compiler/CFunc.h"
#include "compiler/enode.h"
#include "compiler/objects.h"
#include "compiler/Switch.h"
#include "compiler/tokens.h"
#include "compiler/types.h"

// mwccpsp 3.0.1 switchstatement (0x4A4A40): a jump table when the cases are
// dense enough, else a compare and branch per case. Case labels may be
// ranges (GCC `case a ... b`).

typedef struct MSwitchTable {
    struct MSwitchTable* next;
    Object* object;
} MSwitchTable;

#define IS_LONGLONG(t) (((t)->type == TYPEINT || (t)->type == TYPEENUM) && (t)->size == 8)
#if MWCCPS2
// b210 (0x4B0380): a long long selector is one 64-bit register; no
// split compare of the halves
#undef IS_LONGLONG
#define IS_LONGLONG(t) 0
#endif

extern SwitchCase* data_00625268; // the case being looked at
extern SInt32 data_005F0EC4; // number of cases
extern VOperand data_006247C4; // the selector
extern CInt64 data_00610C38; // the smallest value (start of the maximum)
extern CInt64 data_00610C40; // the largest value (start of the minimum)
extern MSwitchTable* data_00624724; // switch tables to emit
extern SInt32 data_006250B4; // read-only data section
extern SInt32 data_00625A98[7]; // next virtual register, per class
extern void (*data_005D0140[])(ENode* expr, void* target, VOperand* out);

extern MOperand func_004C3C00(CLabel* label); // a label operand
extern void func_004A64C0(VOperand* op, Type* type, short copy); // make the operand a value
extern VOperand func_004A7390(VOperand* src);
extern VOperand func_00479E40(UInt16 op, VOperand* src, CInt64 value);
extern void func_00479310(UInt16 op, VOperand* a, SInt32 imm, CLabel* label);
extern void func_0047A850(VOperand* a, CInt64* imm, Type* type, CLabel* label); // branch if equal
extern void func_0047A170(VOperand* a, CInt64* imm, Type* type, CLabel* label); // branch if less
extern void func_0047A0C0(
    VOperand* a, CInt64* imm, Type* type, CLabel* label
); // branch if not greater
extern void func_0047B480(VOperand* a, SInt32 imm, CLabel* label);
extern void func_0047B6A0(VOperand* a, SInt32 imm, CLabel* label);
extern void func_00480710(MPCodeLabel* label); // branch_always
extern void func_0046FA50(void); // makepcblock
extern void func_0046FA90(MPCodeLabel* label); // place a label

void func_004A4A40(ENode* expr, SwitchInfo* info) {
    CInt64 lo;
    CInt64 hi;
    Type* type;
    CInt64 three;
    CInt64 span;
    CInt64 idx;
    CInt64 v;
    double density;
    MOperand** table;
    MOperand* deflt;
    MOperand* op;
    SInt32 n;
    SInt32 i;
    Object* obj;
    MSwitchTable* list;
    VOperand t;
    VOperand hiword;
    CLabel* label;
    int r1;
    int r2;
    int r3;

    lo = data_00610C40;
    hi = data_00610C38;
    type = expr->rtype;
    data_005F0EC4 = 0;
    for (data_00625268 = info->cases; data_00625268; data_00625268 = data_00625268->next) {
        if (!data_00625268->label->pclabel) {
            data_00625268->label->pclabel = (PCodeLabel*)makepclabel();
        }
        lo = CInt64_Less(lo, data_00625268->min) ? lo : data_00625268->min;
        hi = CInt64_Greater(hi, data_00625268->min) ? hi : data_00625268->min;
        hi = CInt64_Greater(hi, data_00625268->max) ? hi : data_00625268->max;
        data_005F0EC4++;
    }

    CInt64_SetLong(&three, 3);
    if (CInt64_Greater(lo, cint64_zero) && CInt64_Less(lo, three)) {
        CInt64_SetLong(&lo, 0);
    }
    span = CInt64_Sub(hi, lo);
    if (CInt64_IsZero(&span)) {
        density = 0.0;
    } else {
        density = data_005F0EC4 / CInt64_ConvertUToLongDouble(&span);
    }

    if (!info->defaultlabel->pclabel) {
        info->defaultlabel->pclabel = (PCodeLabel*)makepclabel();
    }
    data_005D0140[expr->type](expr, NULL, &data_006247C4);
    if (data_006247C4.kind) {
        func_004A64C0(&data_006247C4, type, 0);
    }

    if (density > 0.7 && data_005F0EC4 >= 6) {
        n = span.lo + 1;
        table = lalloc(n * sizeof(MOperand*));
        deflt = lalloc(sizeof(MOperand));
        *deflt = func_004C3C00(info->defaultlabel);
        for (i = 0; i < n; i++) {
            table[i] = deflt;
        }

        data_00625268 = info->cases;
        for (i = 0; i < data_005F0EC4; i++) {
            idx = CInt64_Sub(data_00625268->min, lo);
            op = lalloc(sizeof(MOperand));
            *op = func_004C3C00(data_00625268->label);
            table[idx.lo] = op;
            if (CInt64_Greater(data_00625268->max, data_00625268->min)) {
                // as written: the next value is always min + 1, so a range
                // of three or more values never ends
                for (v = CInt64_Add(data_00625268->min, cint64_one);
                     CInt64_LessEqual(v, data_00625268->max);
                     v = CInt64_Add(data_00625268->min, cint64_one)) {
                    idx = CInt64_Sub(v, lo);
                    op = lalloc(sizeof(MOperand));
                    *op = func_004C3C00(data_00625268->label);
                    table[idx.lo] = op;
                }
            }
            data_00625268 = data_00625268->next;
        }

        obj = galloc(sizeof(Object));
        memclrw(obj, sizeof(Object));
        list = galloc(sizeof(MSwitchTable));
        memclrw(list, sizeof(MSwitchTable));
        list->object = obj;
        list->next = data_00624724;
        data_00624724 = list;
        obj->name = CParser_GetUniqueName();
        obj->type = CDecl_NewArrayType(TYPE(&void_ptr), n * 4);
        obj->sclass = TK_STATIC;
        obj->datatype = DDATA;
        obj->qual = Q_CONST;
        obj->section = data_006250B4;
        obj->u.data.u.switchtable.size = n * 4;
        obj->u.data.u.switchtable.data = (char*)table;

        t.kind = 2;
        t.reg = 28;
        t.object = (PtrInt)obj;
        t.offset = 0;
        if (!CInt64_IsZero(&lo)) {
            data_006247C4 = func_00479E40(MACRO_OP(0x96), &data_006247C4, lo);
        }
        func_00479310(MACRO_OP(0x0B), &data_006247C4, n, info->defaultlabel);
        if (t.kind) {
            func_004A64C0(&t, NULL, 0);
        }
        r1 = data_00625A98[0]++;
        emitpcode(0xD2, r1, data_006247C4.reg, 2);
        r2 = data_00625A98[0]++;
        emitpcode(4, r2, r1, t.reg);
        r3 = data_00625A98[0]++;
        emitpcode(0x9D, r3, 0, 0, r2);
        emitpcode(0x3B, r3);
        for (data_00625268 = info->cases; data_00625268; data_00625268 = data_00625268->next) {
            pcbranch(data_006246B4, (MPCodeLabel*)data_00625268->label->pclabel, 0);
        }
        pcbranch(data_006246B4, (MPCodeLabel*)info->defaultlabel->pclabel, 0);
        func_0046FA50();
    } else {
        if (IS_LONGLONG(type)) {
            hiword = func_004A7390(&data_006247C4);
        }
        data_00625268 = info->cases;
        for (i = 0; i < data_005F0EC4; i++) {
            if (CInt64_Greater(data_00625268->max, data_00625268->min)) {
                label = newlabel();
                label->pclabel = (PCodeLabel*)makepclabel();
                func_0047A170(&data_006247C4, &data_00625268->min, type, label);
                func_0047A0C0(&data_006247C4, &data_00625268->max, type, data_00625268->label);
                func_0046FA90((MPCodeLabel*)label->pclabel);
            } else if (IS_LONGLONG(type)) {
                label = newlabel();
                label->pclabel = (PCodeLabel*)makepclabel();
                func_0047B480(&data_006247C4, data_00625268->min.lo, label);
                func_0047B6A0(&hiword, data_00625268->min.hi, data_00625268->label);
                func_0046FA90((MPCodeLabel*)label->pclabel);
            } else {
                func_0047A850(&data_006247C4, &data_00625268->min, type, data_00625268->label);
            }
            data_00625268 = data_00625268->next;
        }
        func_00480710((MPCodeLabel*)info->defaultlabel->pclabel);
    }
}
