// mwccpsp 3.0.1 MIPSError.c (0x4C5940): the MIPS back end's diagnostics, as
// 2.4.7's PPCError.c
#include "compiler/common.h"
#include "compiler/CError.h"
#include "compiler/CParser.h"
#include "compiler/InlineAsm.h"
#include "driver/COSToolsCLT.h"
#include <setjmp.h>
#include <stdarg.h>

static void MIPSError_GetErrorString(char* str, short code) {
    CError_ASSERT(45, code >= 1 && code < 35);
    COS_GetString(str, 10001, code + 500);
}

static void MIPSError_VAErrorMessage(
    int code, va_list list, Boolean isTerminal, Boolean isWarning
) {
    char format[256];

    MIPSError_GetErrorString(format, code);
    CError_ErrorMessageVA(code + 36000, format, list, isTerminal, isWarning);
}

// 0x4C5940: MIPSError_Error
void func_004C5940(int code, ...) {
    va_list list;

    if (trychain) {
        longjmp(trychain->jmpbuf, 1);
    }

    va_start(list, code);
    MIPSError_VAErrorMessage(code, list, FALSE, FALSE);
    va_end(list);

    if (in_assembler) {
        AssemblerError();
    }
}
