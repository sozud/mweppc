// mwccpsp 3.0.1 optimizer type conversions (0x55A3F0..0x55A620): they sit
// between IroVars.c's checks and IroEval.c in the original.
#include "compiler/IroLinearForm.h"
#include "compiler/IroUtil.h"
#include "compiler/CParser.h"
#include "compiler/enode.h"
#include "compiler/types.h"

// a conversion between integer types that keeps every value: to a type at
// least as wide with the same signedness, or from unsigned to a wider signed
Boolean func_0055A3F0(IROLinear* conv) {
    Type* from = conv->u.monadic->rtype;
    Type* to = conv->rtype;
    SInt32 fromsize;
    SInt32 tosize;
    Boolean fromunsigned;
    Boolean tounsigned;

    if (IS_TYPE_INT(from) &&
        TYPE_INTEGRAL(from)->integral < 0x11 &&
        IS_TYPE_INT(to) &&
        TYPE_INTEGRAL(to)->integral < 0x11) {
        fromsize = from->size;
        tosize = to->size;
        fromunsigned = is_unsigned(from);
        tounsigned = is_unsigned(to);
        if (fromunsigned == tounsigned && tosize >= fromsize) {
            return TRUE;
        }
        if (fromunsigned == TRUE && !tounsigned && tosize > fromsize) {
            return TRUE;
        }
    }
    return FALSE;
}

// linear converted to type: !!linear for bool, else a typecast (folded into an
// inner widening cast). Added to list, or pasted after linear.
IROLinear* IRO_MakeTypeConversion(IROLinear* linear, Type* type, IROList* list) {
    IROLinear* not1;
    IROLinear* not2;
    IROLinear* conv;

    if (IS_TYPE_INT(type) && TYPE_INTEGRAL(type)->integral == IT_BOOL) {
        not1 = IRO_NewLinear(IROLinearOp1Arg);
        *not1 = *linear;
        not1->type = IROLinearOp1Arg;
        not1->nodetype = ELOGNOT;
        not1->index = IRO_NumLinear++;
        not1->rtype = type;
        not1->u.monadic = linear;
        not2 = IRO_NewLinear(IROLinearOp1Arg);
        not1->next = not2;
        *not2 = *linear;
        not2->type = IROLinearOp1Arg;
        not2->nodetype = ELOGNOT;
        not2->index = IRO_NumLinear++;
        not2->rtype = type;
        not2->u.monadic = not1;
        not2->next = NULL;
        if (list) {
            IRO_AddToList(not1, list);
        } else {
            IRO_PasteAfter(not1, not2, linear);
        }
        return not2;
    }

    conv = IRO_NewLinear(IROLinearOp1Arg);
    *conv = *linear;
    conv->type = IROLinearOp1Arg;
    conv->nodetype = ETYPCON;
    conv->index = IRO_NumLinear++;
    conv->rtype = type;
    conv->u.monadic = linear;
    conv->next = NULL;
#if !MWCCPS2 || MWCCPS2_BUILD >= 210 // b145 does not fold into an inner cast
    if (func_0055A3F0(conv) && linear->nodetype == ETYPCON && func_0055A3F0(linear)) {
        linear->rtype = type;
        return linear;
    }
#endif
    if (list) {
        IRO_AddToList(conv, list);
    } else {
        IRO_PasteAfter(conv, conv, linear);
    }
    return conv;
}
