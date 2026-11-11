// Field offsets of the MIPS backend's records for units that cannot include
// psp/BE.h (it clashes with the PowerPC PCode declarations).
#include "psp/BE.h"
#include <stddef.h>

const int mpcode_op_offset = offsetof(MPCode, op);
