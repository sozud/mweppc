// OverlayOptions.c: the PS2 command line's overlay options (mwccps2 b210/b198 0x412168,
// 0x4121AC; the parser tables call them). From the PS2 b198 decompilation's
// OverlayOptions.c, taken as a hint.
#include "driver/ParserFace.h"
#include "driver/CWPlugins.h"
#include "driver/Projects.h"
#include <string.h>

#if MWCCPS2 && !MWCC_CORE
extern char ps2_overlay_name_634C58[256]; // the option's name argument (the parser fills it)
extern CWAddr64 ps2_overlay_addr_625CC8; // the group's address argument
extern void AddOverlayGroup(
    const char* name, CWAddr64* addr, SInt32* groupnum, SInt32* overlaynum
); // Projects.c
extern void AddOverlay(SInt32 groupnum, const char* name, SInt32* overlaynum); // Projects.c

// -overlaygroup [name]: a new overlay group (unnamed if no name was given)
int ps2_overlaygroup_412168(void) {
    if (!ps2_overlay_name_634C58[0]) {
        memcpy(ps2_overlay_name_634C58, "(unnamed overlay group)", 24);
    }
    AddOverlayGroup(
        ps2_overlay_name_634C58,
        &ps2_overlay_addr_625CC8,
        &parseopts.currentOverlayGroup,
        &parseopts.currentOverlay
    );
    ps2_overlay_name_634C58[0] = 0;
    return 1;
}

// -overlay [name]: a new overlay in the current group
int ps2_overlay_4121AC(void) {
    if (!ps2_overlay_name_634C58[0]) {
        memcpy(ps2_overlay_name_634C58, "(unnamed overlay)", 18);
    }
    AddOverlay(parseopts.currentOverlayGroup, ps2_overlay_name_634C58, &parseopts.currentOverlay);
    ps2_overlay_name_634C58[0] = 0;
    return 1;
}
#endif
