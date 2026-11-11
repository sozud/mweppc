// 0x4AF320: inside a template function or a template class (its own unit so
// that it is called, not inlined, as in the original)
#include "compiler/common.h"
#include "compiler/CScope.h"
#include "compiler/objects.h"
#include "compiler/types.h"

Boolean func_004AF320(void) {
    Boolean result = TRUE;

    if ((!cscope_currentfunc || !(TYPE_FUNC(cscope_currentfunc->type)->flags & FUNC_IS_TEMPL)) &&
        (!cscope_currentclass || !(cscope_currentclass->flags & CLASS_IS_TEMPL))) {
        result = FALSE;
    }
    return result;
}
