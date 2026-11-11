// mwccpsp 3.0.1: the static target (0x4048D0..0x404960), mweppc's
// cc-eabi-ppc-mw.c for the PSP
#include "driver/cc-eabi-ppc-mw.h"
#include "driver/cc-eabi-ppc.h"
#include "driver/CWPlugins.h"
#include "driver/libimp-eabi-ppc.h"
#include "driver/oslib.h"

void GetStaticTarget(OSType* cpu, OSType* os) {
    *cpu = 'mips';
    *os = 'psx2';
}

void GetStaticPluginType(OSType* language, OSType* plugintype) {
    *language = Lang_Any;
    *plugintype = CWDROPINCOMPILERTYPE;
}

void GetStaticParserPluginType(OSType* style) {
    *style = 'Seep';
}

int RegisterStaticTargetPlugins(void) {
    return RegisterStaticCompilerPlugin() && RegisterStaticLibImporterPlugin();
}

int RegisterStaticTargetResources(void) {
    return RegisterCompilerResources() && RegisterLibImporterResources();
}
