#ifndef COMPILER_CPARSER_H
#define COMPILER_CPARSER_H

#include "compiler/common.h"
#include "compiler/enode.h"
#include "compiler/tokens.h"
#include "driver/CWPlugins.h"
#include "driver/oslib.h"
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 2)
enum {
    AlignMode0_Mac68k,
    AlignMode1_Mac68k4byte,
    AlignMode2_PPC,
    AlignMode3_1Byte,
    AlignMode4_2Byte,
    AlignMode5_4Byte,
    AlignMode6_8Byte,
    AlignMode7_16Byte,
    AlignMode8_Packed
};
enum {
    CPU_PPC401,
    CPU_PPC403,
    CPU_PPC505,
    CPU_PPC509,
    CPU_PPC555,
    CPU_PPC601,
    CPU_PPC602,
    CPU_PPC603,
    CPU_PPC603e,
    CPU_PPC604,
    CPU_PPC604e,
    CPU_PPC740,
    CPU_PPC750,
    CPU_PPC801,
    CPU_PPC821,
    CPU_PPC823,
    CPU_PPC850,
    CPU_PPC860,
    CPU_PPC8240,
    CPU_PPC8260,
    CPU_Generic,
    CPU_PPC7400,
    CPU_PPCGEKKO,
    CPU_PPCELF,
    CPU_PPC7450,
    CPU_PPC556,
    CPU_PPC565
};

#if MWCCPSP
struct CompilerLinkerOptions {
    // mwccpsp 3.0.1 layout, 0x144 bytes, from the compileroptions table in
    // mwccpsp.exe (#pragma name -> offset) and initialize_compiler_options
    // (0x423500).
    UInt8 littleendian; // 0x000 #pragma little_endian
    UInt8 mips_option01; // 0x001 cleared from the PSP MIPS CodeGen panel
    SInt16
        mips_flags02; // 0x002 BE nop pass: 0x2000 puts a nop between some HI/LO operations; 0x1E from the PSP MIPS CodeGen panel
    SInt16 mips_option04; // 0x004 set from the PSP Project panel; 0 turns on .mwcats
    char fp_library; // 0x006 #pragma fp_library
    UInt8 mips_option07; // 0x007 ObjGen_GetObjectAddress: required for far $gp-relative sections
    UInt8 mips_option08; // 0x008 ObjGen_GetObjectAddress: required for $s7-relative sections
    UInt8 unknown09[1]; // 0x009 unknown (1 bytes)
    SInt16 mips_option0A; // 0x00a ObjGen_Setup: 1 sets a flag (0x20000000) in data_00625388
    SInt16 mips_isa; // 0x00c ObjGen_Setup: ISA level, picks the EF_MIPS_ARCH bits
    UInt8 unknown0E[1]; // 0x00e unknown (1 bytes)
    UInt8 mips_option0F; // 0x00f from the PSP MIPS CodeGen panel
    UInt8 mips_option10; // 0x010 from the PSP MIPS CodeGen panel
    UInt8 mips_option11; // 0x011 from the PSP MIPS CodeGen panel
    UInt8 inline_intrinsics; // 0x012 #pragma inline_intrinsics
    unsigned char debuglisting; // 0x013 #pragma debug_listing
    UInt8 interrupt; // 0x014 #pragma interrupt
    UInt8 mips_long64; // 0x015 CMach_Configure: long is 8 bytes
    UInt8 mips_option16; // 0x016 processor setup (0x435300)
    UInt8 mips_option17; // 0x017 set when the PSP Project panel's option is nonzero
    char cats; // 0x018 #pragma cats
    UInt8 mips_option19; // 0x019 from the PSP MIPS CodeGen panel
    UInt8 mips_option1A; // 0x01a Coloring
    UInt8 warn_inlineasm; // 0x01b #pragma warn_inlineasm
    UInt8 conditional_move; // 0x01c #pragma conditional_move
    UInt8 late_conditional_move; // 0x01d #pragma late_conditional_move
    UInt8 gencvtcheck; // 0x01e #pragma gencvtcheck
    UInt8 far_call; // 0x01f #pragma far_call
    UInt8 prefetch; // 0x020 #pragma prefetch
    UInt8 trunc; // 0x021 #pragma trunc
    UInt8 sqrt; // 0x022 #pragma sqrt
    UInt8 mips_option23; // 0x023 processor setup (0x435300)
    UInt8 floatsp32; // 0x024 #pragma floatsp32
    UInt8 mips_option25; // 0x025 processor setup (0x435300)
    UInt8 mips_option26; // 0x026 processor setup (0x435300)
    UInt8 no_branch_likely; // 0x027 #pragma no_branch_likely
    UInt8 debug_1; // 0x028 #pragma debug_1
    UInt8 debug_2; // 0x029 #pragma debug_2
    UInt8 debug_3; // 0x02a #pragma debug_3
    UInt8 debug_4; // 0x02b #pragma debug_4
    UInt8 debug_5; // 0x02c #pragma debug_5
    UInt8 int_multiply_add; // 0x02d #pragma int_multiply_add
    UInt8 no_load_delay_nops; // 0x02e BE nop pass: when set, loads get no delay nop
    UInt8 mips_option2F; // 0x02f processor setup (0x435300)
    UInt8 mips_option30; // 0x030 processor setup (0x435300)
    UInt8 mips_option31; // 0x031 processor setup (0x435300)
    UInt8 nofpregforblkmv; // 0x032 #pragma nofpregforblkmv
    UInt8 mips_option33; // 0x033 from the PSP MIPS CodeGen panel (4)
    UInt8 schedule; // 0x034 #pragma schedule
    UInt8 tailcall; // 0x035 #pragma tailcall
    UInt8 unknown36[1]; // 0x036 unknown (1 bytes)
    UInt8 divbyzerocheck; // 0x037 #pragma divbyzerocheck
    UInt8 nomacro; // 0x038 #pragma nomacro
    UInt8 mips_option39; // 0x039 from the PSP MIPS CodeGen panel (2)
    UInt8 mips_option3A; // 0x03a set without optimize_for_size
    UInt8 mips_option3B; // 0x03b from the PSP MIPS CodeGen panel
    UInt8 mips_option3C; // 0x03c processor setup (0x435300)
    UInt8 mips_option3D; // 0x03d from the PSP MIPS CodeGen panel
    UInt8 function_align16; // 0x03e ObjGen_EmitCode: functions aligned to 16 instead of 4
    UInt8 unknown3F[2]; // 0x03f unknown (2 bytes)
    UInt8 mips_option41; // 0x041 from the PSP MIPS CodeGen panel
    UInt8 fastmath; // 0x042 #pragma fastmath
    UInt8 mips_option43; // 0x043 from the PSP MIPS CodeGen panel (0x20)
    UInt8 volatileasm; // 0x044 #pragma volatileasm
    UInt8 memoryrewrite; // 0x045 #pragma memoryrewrite
    UInt8 unknown46[1]; // 0x046 unknown (1 bytes)
    UInt8 mips_option47; // 0x047 processor setup (0x435300)
    UInt8 mwabi_bitfields; // 0x048 #pragma mwabi_bitfields
    Boolean asmsemicolcomment; // 0x049 #pragma asmsemicolcomment
    Boolean asmpoundcomment; // 0x04a #pragma asmpoundcomment
    UInt8 unknown4B[1]; // 0x04b unknown (1 bytes)
    StringPtr oldprefixname; // 0x04c initialize_compiler_options
    SInt16 inlinelevel; // 0x050 initialize_compiler_options
    SInt16 unknown52; // 0x052 set to 15 at startup
    SInt32 inlinemaxsize; // 0x054 default 256
    SInt32 inlinemaxtotalsize; // 0x058 default 10000
    UInt8 inline_bottom_up; // 0x05c #pragma inline_bottom_up
    UInt8 inline_bottom_up_once; // 0x05d #pragma inline_bottom_up_once
    UInt8 cplusplus; // 0x05e #pragma cplusplus
    UInt8 ecplusplus; // 0x05f #pragma ecplusplus
    char ARMscoping; // 0x060 #pragma ARM_scoping
    Boolean checkprotos; // 0x061 #pragma require_prototypes
    char trigraphs; // 0x062 #pragma trigraphs
    UInt8 onlystdkeywords; // 0x063 #pragma only_std_keywords
    UInt8 enumsalwaysint; // 0x064 #pragma enumsalwaysint
    UInt8 ANSIstrict; // 0x065 #pragma ANSI_strict
    UInt8 mpwc_relax; // 0x066 #pragma mpwc_relax
    UInt8 mpwc_newline; // 0x067 #pragma mpwc_newline
    Boolean ignore_oldstyle; // 0x068 #pragma ignore_oldstyle
    Boolean cpp_extensions; // 0x069 #pragma cpp_extensions
    UInt8 pointercast_lvalue; // 0x06a #pragma pointercast_lvalue
    char RTTI; // 0x06b #pragma RTTI
    UInt8 delete_exception; // 0x06c #pragma delete_exception
    Boolean test_dummy; // 0x06d gap filled in 2.4.7 order
    Boolean oldalignment; // 0x06e #pragma oldalignment
    UInt8 unsigned_char; // 0x06f #pragma unsigned_char
    char multibyteaware; // 0x070 #pragma multibyteaware
    UInt8 auto_inline; // 0x071 #pragma auto_inline
    UInt8 defer_codegen; // 0x072 #pragma defer_codegen
    UInt8 direct_to_som; // 0x073 #pragma direct_to_som
    UInt8 som_env_check; // 0x074 #pragma SOMCheckEnvironment
    UInt8 som_call_optimize; // 0x075 #pragma SOMCallOptimization
    UInt8 booltruefalse; // 0x076 #pragma _Bool / bool
    UInt8 longlong; // 0x077 #pragma longlong
    UInt8 longlong_enums; // 0x078 #pragma longlong_enums
    Boolean no_tfuncinline; // 0x079 #pragma no_tfuncinline
    UInt8 always_vdispatch; // 0x07a gap filled in 2.4.7 order
    char flat_include; // 0x07b #pragma flat_include
    char syspath_once; // 0x07c #pragma syspath_once
    char always_import; // 0x07d #pragma always_import
    UInt8 simple_class_byval; // 0x07e #pragma simple_class_byval
    char wchar_type; // 0x07f #pragma wchar_type
    UInt8 vbase_ctor_offset; // 0x080 #pragma vbase_ctor_offset
    char vbase_abi_v2; // 0x081 #pragma vbase_abi_v2
    char def_inherited; // 0x082 #pragma def_inherited
    UInt8 template_patch; // 0x083 #pragma template_patch
    UInt8 template_friends; // 0x084 #pragma template_friends
    Boolean faster_pch_gen; // 0x085 #pragma faster_pch_gen
    UInt8 array_new_delete; // 0x086 #pragma array_new_delete
    UInt8 dollar_identifiers; // 0x087 #pragma dollar_identifiers
    Boolean def_inline_tfuncs; // 0x088 #pragma def_inline_tfuncs
    UInt8 arg_dep_lookup; // 0x089 #pragma arg_dep_lookup
    char simple_prepdump; // 0x08a #pragma simple_prepdump
    char line_prepdump; // 0x08b #pragma line_prepdump
    UInt8 fullpath_prepdump; // 0x08c #pragma fullpath_prepdump
    UInt8 unknown8D[1]; // 0x08d unknown (1 bytes)
    Boolean old_mtemplparser; // 0x08e #pragma old_mtemplparser
    UInt8 suppress_init_code; // 0x08f #pragma suppress_init_code
    Boolean reverse_bitfields; // 0x090 #pragma reverse_bitfields
    UInt8 c9x; // 0x091 #pragma c99 / c9x
    Boolean float_constants; // 0x092 #pragma float_constants
    Boolean no_static_dtors; // 0x093 #pragma no_static_dtors
    Boolean longlong_prepeval; // 0x094 #pragma longlong_prepeval
    UInt8 const_strings; // 0x095 #pragma const_strings
    Boolean dumpir; // 0x096 #pragma dumpir
    UInt8 experimental; // 0x097 #pragma experimental
    UInt8 late_fixes; // 0x098 #pragma late_fixes
    UInt8 gcc_extensions; // 0x099 #pragma gcc_extensions
    Boolean stdc_fp_contract; // 0x09a #pragma stdc_fp_contract
    Boolean stdc_fenv_access; // 0x09b #pragma stdc_fenv_access
    Boolean stdc_cx_limitedr; // 0x09c #pragma stdc_cx_limitedr
    UInt8 old_argmatch; // 0x09d #pragma old_argmatch
    char optEH; // 0x09e #pragma optEH
    Boolean optEH2; // 0x09f #pragma optEH2
    Boolean new_mangler; // 0x0a0 #pragma new_mangler
    UInt8 parse_func_templ; // 0x0a1 #pragma parse_func_templ
    UInt8 parse_mfunc_templ; // 0x0a2 #pragma parse_mfunc_templ
    UInt8 old_depfunc_lookup; // 0x0a3 #pragma old_depfunc_lookup
    UInt8 show_error_filestack; // 0x0a4 #pragma show_error_filestack
    UInt8 defer_defarg_parsing; // 0x0a5 #pragma defer_defarg_parsing
    UInt8 reg_class_allocs; // 0x0a6 #pragma reg_class_allocs
    UInt8 old_pragma_once; // 0x0a7 #pragma old_pragma_once
    UInt8 new_calldeduction; // 0x0a8 #pragma new_calldeduction
    Boolean fullpath_file; // 0x0a9 #pragma fullpath_file
    UInt8 opt_classresults; // 0x0aa #pragma opt_classresults
    UInt8 ushort_wchar_t; // 0x0ab #pragma ushort_wchar_t
    UInt8 thread_safe_init; // 0x0ac #pragma thread_safe_init
    Boolean microsoft_EH; // 0x0ad #pragma microsoft_RTTI / microsoft_exceptions
    Boolean no_ansi_lib_builtin; // 0x0ae gap filled in 2.4.7 order
    char warningerrors; // 0x0af #pragma warning_errors
    UInt8 access_errors; // 0x0b0 #pragma access_errors
    char pedantic; // 0x0b1 #pragma extended_errorcheck
    UInt8 checkprecompflags; // 0x0b2 #pragma check_header_flags
    Boolean supress_warnings; // 0x0b3 #pragma suppress_warnings
    Boolean warn_illpragma; // 0x0b4 #pragma warn_illpragma
    UInt8 warn_emptydecl; // 0x0b5 #pragma warn_emptydecl
    Boolean warn_possunwant; // 0x0b6 #pragma warn_possunwant
    char warn_unusedvar; // 0x0b7 #pragma warn_unusedvar
    char warn_unusedarg; // 0x0b8 #pragma warn_unusedarg
    UInt8 warn_extracomma; // 0x0b9 #pragma warn_extracomma
    Boolean warn_hidevirtual; // 0x0ba #pragma warn_hidevirtual
    char warn_largeargs; // 0x0bb #pragma warn_largeargs
    Boolean warn_implicitconv; // 0x0bc #pragma warn_implicitconv
    UInt8 warn_impl_i2f_conv; // 0x0bd #pragma warn_impl_i2f_conv
    UInt8 warn_impl_f2i_conv; // 0x0be #pragma warn_impl_f2i_conv
    UInt8 warn_impl_s2u_conv; // 0x0bf #pragma warn_impl_s2u_conv
    UInt8 warn_notinlined; // 0x0c0 #pragma warn_notinlined
    SInt8 warn_structclass; // 0x0c1 #pragma warn_structclass
    Boolean warn_padding; // 0x0c2 #pragma warn_padding
    Boolean warn_no_side_effect; // 0x0c3 #pragma warn_no_side_effect
    Boolean warn_resultnotused; // 0x0c4 #pragma warn_resultnotused
    UInt8 warn_ptr_int_conv; // 0x0c5 #pragma warn_ptr_int_conv
    UInt8 warn_no_typename; // 0x0c6 #pragma warn_no_typename
    UInt8 warn_filenamecaps; // 0x0c7 #pragma warn_filenamecaps
    UInt8 warn_filenamecaps_system; // 0x0c8 #pragma warn_filenamecaps_system
    UInt8 warn_no_explicit_virtual; // 0x0c9 #pragma warn_no_explicit_virtual
    UInt8 warn_pch_portability; // 0x0ca #pragma warn_pch_portability
    UInt8 warn_uninitializedvar; // 0x0cb #pragma warn_uninitializedvar
    UInt8 warn_illunionmembers; // 0x0cc #pragma warn_illunionmembers
    UInt8 warn_undefmacro; // 0x0cd #pragma warn_undefmacro
    signed char structalignment; // 0x0ce re/field_map.py: 2.4.7 0xd3 -> 0xce at 3 sites
    UInt8 alignarraymembers; // 0x0cf #pragma align_array_members
    SInt8 dont_reuse_strings; // 0x0d0 #pragma dont_reuse_strings
    UInt8 poolstrings; // 0x0d1 #pragma pool_strings
    UInt8 explicit_zero_data; // 0x0d2 #pragma explicit_zero_data
    UInt8 readonly_strings; // 0x0d3 #pragma readonly_strings
    UInt8 exceptions; // 0x0d4 #pragma exceptions
    Boolean dddummy; // 0x0d5 gap filled in 2.4.7 order
    UInt8 dontinline; // 0x0d6 #pragma dont_inline
    UInt8 alwaysinline; // 0x0d7 #pragma always_inline
    char peephole; // 0x0d8 #pragma peephole
    Boolean globaloptimizer; // 0x0d9 #pragma global_optimizer
    UInt8 sideeffects; // 0x0da #pragma side_effects
    UInt8 cfm_internal; // 0x0db #pragma internal
    UInt8 cfm_import; // 0x0dc #pragma import
    UInt8 cfm_export; // 0x0dd #pragma export
    UInt8 cfm_lib_export; // 0x0de #pragma lib_export
    char nosyminline; // 0x0df #pragma nosyminline
    char force_active; // 0x0e0 #pragma force_active
    unsigned char block_assign; // 0x0e1 #pragma block_assign
    Boolean has8bytebitfields; // 0x0e2 #pragma has8bytebitfields
    SInt8 optimizationlevel; // 0x0e3 initialize_compiler_options
    UInt8 optimizesize; // 0x0e4 #pragma optimize_for_size
    UInt8 optimizewithasm; // 0x0e5 gap filled in 2.4.7 order
    UInt8 crippled; // 0x0e6 initialize_compiler_options
    UInt8 commonsubs; // 0x0e7 #pragma opt_common_subs
    UInt8 loopinvariants; // 0x0e8 #pragma opt_loop_invariants
    UInt8 propagation; // 0x0e9 #pragma opt_propagation
    UInt8 deadstore; // 0x0ea #pragma opt_dead_assignments
    UInt8 strengthreduction; // 0x0eb #pragma opt_strength_reduction
    UInt8 strengthreductionstrict; // 0x0ec #pragma opt_strength_reduction_strict
    UInt8 deadcode; // 0x0ed #pragma opt_dead_code
    UInt8 unknownEE[1]; // 0x0ee unknown (1 bytes)
    UInt8 lifetimes; // 0x0ef #pragma opt_lifetimes
    UInt8 unrolling; // 0x0f0 #pragma opt_unroll_loops
    Boolean vectorizeloops; // 0x0f1 #pragma opt_vectorize_loops
    UInt8 multiplepasses; // 0x0f2 gap filled in 2.4.7 order
    UInt8 opt_pointer_analysis; // 0x0f3 #pragma opt_pointer_analysis
    UInt8 opt_pointer_analysis_mode; // 0x0f4 gap filled in 2.4.7 order
    signed char unrollfactor; // 0x0f5 default 8
    UInt8 unknownF6; // 0x0f6 new, default 8
    SInt8 unrollinstrfactor; // 0x0f7 default 100
    UInt8 opt_scalarize; // 0x0f8 #pragma opt_scalarize
    UInt8 opt_prelinearize; // 0x0f9 #pragma opt_prelinearize
    UInt8 opt_treetransformation; // 0x0fa #pragma opt_treetransformation
    UInt8 opt_arithtransformation; // 0x0fb #pragma opt_arithtransformation
    UInt8 opt_rebuildconditionals; // 0x0fc #pragma opt_rebuildconditionals
    UInt8 opt_rebuildlogicals; // 0x0fd #pragma opt_rebuildlogicals
    UInt8 opt_generateconditionalassignments; // 0x0fe #pragma opt_generateconditionalassignments
    UInt8 opt_removeemptyloops; // 0x0ff #pragma opt_removeemptyloops
    UInt8 opt_alwaysdumpopts; // 0x100 #pragma opt_alwaysdumpopts
    UInt8 opt_rotateloops; // 0x101 #pragma opt_rotateloops
    UInt8 opt_optimizenonregaccess; // 0x102 #pragma opt_optimizenonregaccess
    UInt8 opt_movepostops; // 0x103 #pragma opt_movepostops
    UInt8 opt_findoptimalunrollfactor; // 0x104 #pragma opt_findoptimalunrollfactor
    UInt8 opt_findoptimalvectorunrollfactor; // 0x105 #pragma opt_findoptimalvectorunrollfactor
    UInt8 opt_unrollpostloop; // 0x106 #pragma opt_unrollpostloop
    UInt8 opt_unrollvectorpostloop; // 0x107 #pragma opt_unrollvectorpostloop
    UInt8 opt_createvectortemps; // 0x108 #pragma opt_createvectortemps
    UInt8 opt_unpromotetypes; // 0x109 #pragma opt_unpromotetypes
    UInt8 opt_dumptypes; // 0x10a #pragma opt_dumptypes
    UInt8 opt_scalarizebitfields; // 0x10b #pragma opt_scalarizebitfields
    UInt8
        opt_propagatemultiplydivideassignments; // 0x10c #pragma opt_propagatemultiplydivideassignments
    UInt8 opt_moveinvariantsinaddressexpr; // 0x10d #pragma opt_moveinvariantsinaddressexpr
    UInt8 opt_convertfpconsts; // 0x10e #pragma opt_convertfpconsts
    UInt8 opt_markcounterloops; // 0x10f #pragma opt_markcounterloops
    UInt8 opt_markvariablecounterloops; // 0x110 #pragma opt_markvariablecounterloops
    UInt8 opt_pulloutconstants; // 0x111 #pragma opt_pulloutconstants
    UInt8 opt_defineargorder; // 0x112 #pragma opt_defineargorder
    UInt8
        opt_useexpressioncostswhenlinearizingassignments; // 0x113 #pragma opt_useexpressioncostswhenlinearizingassignments
    UInt8 opt_intelcompareoplinearizationhack; // 0x114 #pragma opt_intelcompareoplinearizationhack
    UInt8 opt_rearrangemultiplesubscripts; // 0x115 #pragma opt_rearrangemultiplesubscripts
    UInt8 opt_returnloopdepthinstatementvalue; // 0x116 #pragma opt_returnloopdepthinstatementvalue
    UInt8 opt_marknonregtemps; // 0x117 #pragma opt_marknonregtemps
    UInt8 opt_addgotounrolledlooptest; // 0x118 #pragma opt_addgotounrolledlooptest
    UInt8
        opt_addtogetherconstantpartsoftreeexpressions; // 0x119 #pragma opt_addtogetherconstantpartsoftreeexpressions
    UInt8 opt_propagateequivalentscalartypes; // 0x11a #pragma opt_propagateequivalentscalartypes
    UInt8 opt_unpromoteenumtointconversions; // 0x11b #pragma opt_unpromoteenumtointconversions
    UInt8 opt_unpromotemultipliestochartype; // 0x11c #pragma opt_unpromotemultipliestochartype
    UInt8 opt_dospecialmultiplyunpromotion; // 0x11d #pragma opt_dospecialmultiplyunpromotion
    UInt8
        opt_docrossconversionofinductionreference; // 0x11e #pragma opt_docrossconversionofinductionreference
    UInt8
        opt_intrinsicfunctioncallscanhavesideeffects; // 0x11f #pragma opt_intrinsicfunctioncallscanhavesideeffects
    UInt8
        opt_collapselogicalselectionintobittest; // 0x120 #pragma opt_collapselogicalselectionintobittest
    UInt8
        opt_collapselogicalselectionintobittest_uselonglong; // 0x121 #pragma opt_collapselogicalselectionintobittest_uselonglong
    UInt8 opt_reverseloops; // 0x122 #pragma opt_reverseloops
    UInt8 opt_foldconstants; // 0x123 #pragma opt_foldconstants
    UInt8 opt_optimizecontrolflow; // 0x124 #pragma opt_optimizecontrolflow
    UInt8 opt_serializeassignments; // 0x125 #pragma opt_serializeassignments
    UInt8 opt_branchlessexpensivethandiv; // 0x126 #pragma opt_branchlessexpensivethandiv
    UInt8 opt_repositioncode; // 0x127 #pragma opt_repositioncode
    UInt8 opt_decomposeaggregates; // 0x128 #pragma opt_decomposeaggregates
    UInt8 opt_recomposeaggregates; // 0x129 #pragma opt_recomposeaggregates
    UInt8 opt_scalarizeliveranges; // 0x12a #pragma opt_scalarizeliveranges
    UInt8 opt_constnonchanging; // 0x12b #pragma opt_constnonchanging
    UInt8 filesyminfo; // 0x12c #pragma sym
    UInt8 symstringtypes; // 0x12d #pragma symstringtypes
    UInt8 unknown12E[2]; // 0x12e unknown (2 bytes)
    UInt8 unknown130[1]; // 0x130 unknown (1 bytes)
    Boolean absolutepath; // 0x131 byte 5 of the PSP Linker Panel (symfullpath in 2.4.7)
    UInt8 mips_option132; // 0x132 from the PSP MIPS CodeGen panel
    UInt8 unknown133[1]; // 0x133 unknown (1 bytes)
    CWDataType appltype; // 0x134 'CWIE'
    CWDataType headtype; // 0x138 'MMCH'
    CWDataType texttype; // 0x13c 'TEXT'
    struct HashNameNode* cursegmentname; // 0x140 last field, placement unverified
};
#else
struct CompilerLinkerOptions {
    UInt8 littleendian;
    SInt16 processor;
    SInt8 scheduling;
    UInt8 schedule_factor;
    unsigned char debuglisting;
    char profile;
    Boolean traceback;
    char disable_registers;
    UInt8 fp_contract;
    UInt8 no_register_save_helpers;
    Boolean unroll_speculative;
    SInt16 unroll_instr_limit;
    SInt16 unroll_factor_limit;
    UInt8 altivec_model;
    UInt8 altivec_vrsave;
    UInt8 function_align;
    char cats;
    char force_cats;
    SInt32 sdata_threshold;
    int sdata2_threshold;
    UInt8 pool_data;
    UInt8 common;
    char use_fp;
    Boolean soft_float;
    UInt8 codegen_pic;
    UInt8 full_dwarf;
    char no_comment_records;
    SInt8 rel109_offset;
    struct ObjGenSection* textSection;
    struct ObjGenSection* dataSection;
    struct ObjGenSection* constSection;
    struct ObjGenSection* sdataSection;
    struct ObjGenSection* sconstSection;
    SInt32 sectionHeaderTableSize;
    struct SectionSymbolAttributes** sectionHeaderTable;
    struct InterruptList* interruptList;
    struct InterruptGenerationRecord* interruptOptions;
    UInt8 incompatible_return_small_structs;
    Boolean create_file_object;
    UInt8 incompatible_sfpe_double_params;
    UInt8 ibm_stackframe;
    UInt8 gen_isel;
    UInt8 gen_fsel;
    UInt8 gecko_float_typecons;
    UInt8 disable_vector_frame;
    UInt8 opt_bcc_lr_ctr;
    UInt8 use_lmw_stmw;
    char report_heap_info;
    UInt8 misaligned_mem_access;
    UInt8 switch_tables;
    UInt8 prepare_compress;
    char min_struct_alignment;
    UInt8 unsafe_global_reg_vars;
    UInt8 ignore_global_reg_vars;
    UInt32 bc_range;
    UInt8 gprfloatcopy;
#if VERSION >= VERSION_GC_2_5
    UInt8 old_style_bitfields;
#endif
    Boolean asmsemicolcomment;
    Boolean asmpoundcomment;
#if VERSION >= VERSION_GC_2_5
    UInt8 unknown66[2];
#endif
    StringPtr oldprefixname;
    SInt16 inlinelevel;
    SInt32 inlinemaxsize;
    SInt32 inlinemaxtotalsize;
    UInt8 inline_bottom_up;
    UInt8 cplusplus;
    UInt8 ecplusplus;
    UInt8 objective_c;
    SInt8 objc_strict;
    char ARMconform;
    char ARMscoping;
    Boolean checkprotos;
    char trigraphs;
    UInt8 onlystdkeywords;
    UInt8 enumsalwaysint;
    UInt8 ANSIstrict;
    UInt8 mpwc_relax;
    UInt8 mpwc_newline;
    Boolean ignore_oldstyle;
    Boolean cpp_extensions;
    UInt8 pointercast_lvalue;
    char RTTI;
    UInt8 delete_exception;
    Boolean test_dummy;
    Boolean oldalignment;
    UInt8 unsigned_char;
    char multibyteaware;
    UInt8 auto_inline;
    UInt8 defer_codegen;
    UInt8 direct_to_som;
    UInt8 som_env_check;
    UInt8 som_call_optimize;
    UInt8 booltruefalse;
    Boolean old_enum_mangler;
    UInt8 longlong;
    UInt8 longlong_enums;
    Boolean no_tfuncinline;
    UInt8 always_vdispatch;
    char flat_include;
    char syspath_once;
    char always_import;
    UInt8 simple_class_byval;
    char wchar_type;
    UInt8 vbase_ctor_offset;
    char vbase_abi_v2;
    char def_inherited;
    UInt8 template_patch;
    UInt8 template_friends;
    Boolean faster_pch_gen;
    UInt8 array_new_delete;
    UInt8 dollar_identifiers;
    Boolean def_inline_tfuncs;
    UInt8 arg_dep_lookup;
    char simple_prepdump;
    char line_prepdump;
    UInt8 fullpath_prepdump;
    Boolean old_mtemplparser;
    UInt8 suppress_init_code;
    Boolean reverse_bitfields;
    UInt8 c9x;
    Boolean float_constants;
    Boolean no_static_dtors;
    Boolean longlong_prepeval;
    UInt8 const_strings;
    Boolean dumpir;
    UInt8 experimental;
    UInt8 gcc_extensions;
    Boolean stdc_fp_contract;
    Boolean stdc_fenv_access;
    Boolean stdc_cx_limitedr;
    UInt8 old_argmatch;
    char optEH;
    Boolean optEH2;
    Boolean new_mangler;
    Boolean fullpath_file;
    Boolean microsoft_EH;
    Boolean no_ansi_lib_builtin;
    char warningerrors;
    char pedantic;
    UInt8 checkprecompflags;
    Boolean supress_warnings;
    Boolean warn_illpragma;
    UInt8 warn_emptydecl;
    Boolean warn_possunwant;
    char warn_unusedvar;
    char warn_unusedarg;
    UInt8 warn_extracomma;
    Boolean warn_hidevirtual;
    char warn_largeargs;
    Boolean warn_implicitconv;
    UInt8 warn_notinlined;
    SInt8 warn_structclass;
    Boolean warn_padding;
    Boolean warn_no_side_effect;
    Boolean warn_resultnotused;
    UInt8 warn_ptr_int_conv;
    UInt8 warn_uninitializedvar;
    signed char structalignment;
    UInt8 alignarraymembers;
    SInt8 dont_reuse_strings;
    UInt8 poolstrings;
    UInt8 explicit_zero_data;
    UInt8 readonly_strings;
    UInt8 exceptions;
    Boolean dddummy;
    UInt8 dontinline;
    UInt8 alwaysinline;
    char peephole;
    Boolean globaloptimizer;
    UInt8 sideeffects;
    UInt8 cfm_internal;
    UInt8 cfm_import;
    UInt8 cfm_export;
    UInt8 cfm_lib_export;
    char nosyminline;
    char force_active;
    unsigned char block_assign;
    Boolean has8bytebitfields;
    SInt8 optimizationlevel;
    UInt8 optimizesize;
    UInt8 optimizewithasm;
    UInt8 crippled;
    UInt8 commonsubs;
    UInt8 loopinvariants;
    UInt8 propagation;
    UInt8 deadstore;
    UInt8 strengthreduction;
    UInt8 strengthreductionstrict;
    UInt8 deadcode;
    UInt8 lifetimes;
    UInt8 logopts;
    UInt8 unrolling;
    Boolean vectorizeloops;
    UInt8 multiplepasses;
    UInt8 opt_pointer_analysis;
    UInt8 opt_pointer_analysis_mode;
    signed char unrollfactor;
    SInt8 unrollinstrfactor;
    UInt8 filesyminfo;
    UInt8 unknownfd;
    char dwarf_siblings;
    UInt8 dwarf_relocations;
    Boolean absolutepath;
    CWDataType appltype;
    CWDataType headtype;
    CWDataType texttype;
    struct HashNameNode* cursegmentname;
};
#endif
#pragma pack(pop)

struct ParserTryBlock {
    struct ParserTryBlock* next;
    jmp_buf jmpbuf;
    NameSpace* cscope_current;
    struct TypeClass* cscope_currentclass;
    Object* cscope_currentfunc;
    TemplStack* ctempl_curinstance;
    struct TStreamElement* cerror_locktoken;
    Boolean cscope_is_member_func;
};

typedef struct CallbackAction {
    struct CallbackAction* next;
    Object* obj;
    struct TypeClass* tclass;
} CallbackAction;

extern FileOffsetInfo cparser_fileoffset;
extern struct TStreamElement symdecltoken;
extern ParserTryBlock* trychain;
extern TypeStruct ptmstruct;
extern TypeStruct catchinfostruct;
extern Boolean in_assembler;
extern Boolean illegalimplicitconversion;
extern Boolean in_func_arglist;
extern NameSpaceName* newp_fobj;
extern NameSpaceName* newa_fobj;
extern NameSpaceName* delp_fobj;
extern NameSpaceName* dela_fobj;
extern Object* newh_func;
extern Object* delh_func;
extern Object* copy_func;
extern Object* clear_func;
extern Object* Rgtid_func;
extern Object* Rdync_func;
extern Object* rt_ptmf_cast;
extern Object* rt_ptmf_cmpr;
extern Object* rt_ptmf_test;
extern Object* rt_ptmf_call;
extern Object* rt_ptmf_scall;
extern Object* rt_ptmf_call4;
extern Object* rt_ptmf_scall4;
extern Object* rt_ptmf_null;
extern Object* rt_som_new;
extern Object* rt_som_newcheck;
extern Object* rt_som_check;
extern Object* rt_som_glue1;
extern Object* rt_som_glue2;
extern Object* rt_som_glue3;
extern Object* carr_func;
extern Object* cnar_func;
extern Object* darr_func;
extern Object* dnar_func;
extern Object* dnar3_func;
extern Object* Xgreg_func;
extern Object* Xthrw_func;
extern Object* Xicth_func;
extern Object* Xecth_func;
extern Object* Xunex_func;
extern CompilerLinkerOptions copts;
#if MWCCPSP
// 2.4.7 options that mwccpsp 3.0.1 does not have, or whose place in the
// 0x144-byte struct is not known yet. Shared code reads them from here (all
// zero: "option off"); a function that does will not match until the option
// is located.
typedef struct CompilerLinkerOptionsGone {
    UInt8 ARMconform;
    UInt8 altivec_model;
    UInt8 incompatible_sfpe_double_params;
    UInt8 objective_c;
    UInt8 soft_float;
    UInt8 objc_strict;
    UInt8 create_file_object;
    UInt8 disable_registers;
    UInt8 disable_vector_frame;
    UInt8 force_cats;
    UInt8 fp_contract;
    UInt8 gecko_float_typecons;
    UInt8 gprfloatcopy;
    UInt8 ibm_stackframe;
    UInt8 ignore_global_reg_vars;
    UInt8 incompatible_return_small_structs;
    UInt8 misaligned_mem_access;
    UInt8 no_register_save_helpers;
    UInt8 old_style_bitfields;
    UInt8 opt_bcc_lr_ctr;
    UInt8 pool_data;
    UInt8 prepare_compress;
    UInt8 profile;
    UInt8 schedule_factor;
    UInt8 switch_tables;
    UInt8 unsafe_global_reg_vars;
    UInt8 use_fp;
    UInt8 use_lmw_stmw;
    SInt16 processor;
} CompilerLinkerOptionsGone;
extern CompilerLinkerOptionsGone copts_gone;
// nothing sets these (3.0.1 has no such options), so they are constant
// zero: the code that tested them compiles away as in the original
#define COPTS_GONE(f) (0)

#endif
extern GList name_mangle_list;
extern HashNameNode* no_name_node;
extern HashNameNode* temp_argument_name;
extern HashNameNode* this_name_node;
extern HashNameNode* self_name_node;
extern HashNameNode* vptr_name_node;
extern CallbackAction* callbackactions;
extern Boolean fatalerrors;
extern Boolean anyerrors;
extern jmp_buf errorreturn;
extern char string[256];

extern Type sttemplexpr;
extern Type stillegal;
extern Type stvoid;
extern TypePointer void_ptr;
extern TypeFunc rt_func;

#ifdef __MWERKS__
#define ORIGINAL_ADDRESS(ptr) ((UInt32)(ptr))
#define TYPE_INTEGRAL_UNCHECKED(ty) (TYPE_INTEGRAL(ty)->integral)
#else
#define ORIGINAL_ADDRESS(ptr) CParser_OriginalAddress(ptr)
#define TYPE_INTEGRAL_UNCHECKED(ty) CParser_UncheckedIntegral(TYPE(ty))
extern UInt32 CParser_OriginalAddress(const void* ptr);
extern UInt8 CParser_UncheckedIntegral(Type* type);
#endif

enum {
    ATOM_VOID = 1,
    ATOM_CHAR,
    ATOM_UCHAR,
    ATOM_WCHAR,
    ATOM_SHORT,
    ATOM_USHORT,
    ATOM_INT,
    ATOM_UINT,
    ATOM_LONG,
    ATOM_ULONG,
    ATOM_LONGLONG,
    ATOM_ULONGLONG,
#if MWCCPSP
    ATOM_INT128, // 3.0.1: a 16-byte integer (stint128)
#endif
    ATOM_FLOAT,
    ATOM_SHORTDOUBLE,
    ATOM_DOUBLE,
    ATOM_LONGDOUBLE
};

extern Object* CParser_NewRTFunc(Type* rettype, HashNameNode* name, UInt8 kind, int argcount, ...);
extern Boolean CParser_IsPublicRuntimeObject(Object* obj);
extern Boolean CParser_ReInitRuntimeObjects(Boolean is_precompiler);
extern void CParser_Setup(void);
extern void CParser_Cleanup(void);
#if MWCCPSP
extern void CParser_FreeLocalHeap(void);
#endif
extern short GetPrec(short token);
#if MWCCPSP
// 3.0.1 can also hand back a conversion operator's type and qualifiers
extern Boolean CParser_ParseOperatorName(
    short* token,
    Boolean allowConversion,
    Boolean parseConversionType,
    Type** conversionType,
    UInt32* conversionQual
);
#define CPARSER_PARSEOPNAME(token, a, b) CParser_ParseOperatorName(token, a, b, NULL, NULL)
#else
extern Boolean CParser_ParseOperatorName(
    short* token, Boolean allowConversion, Boolean parseConversionType
);
#endif
extern SInt32 CParser_GetUniqueID(void);
extern void CParser_PrintUniqueID(char* buf);
extern void CParser_SetUniqueID(SInt32 id);
extern HashNameNode* CParser_GetUniqueName(void);
extern HashNameNode* CParser_NameConcat(const char* a, const char* b);
extern HashNameNode* CParser_AppendUniqueName(char* prefix);
extern HashNameNode* CParser_AppendUniqueNameFile(char* prefix);
extern Boolean IsTempName(HashNameNode* name);
extern void CParser_UpdateObject(Object* object, DeclInfo* declinfo);
extern Object* CParser_NewObject(DeclInfo* declinfo);
extern Object* CParser_NewLocalDataObject(DeclInfo* declinfo, Boolean add_to_locals);
extern Object* CParser_NewGlobalDataObject(DeclInfo* declinfo);
extern Object* CParser_NewCompilerDefDataObject(void);
extern Object* CParser_NewFunctionObject(DeclInfo* declinfo);
extern Object* CParser_NewCompilerDefFunctionObject(void);
extern Object* CParser_NewAliasObject(Object* object, SInt32 offset);
extern FuncArg* CParser_NewFuncArg(void);
extern Type* atomtype(void);
extern Object* CParser_FindDeallocationObject(
    Type* type, FuncArg* args, Boolean isArray, Boolean isGlobal, Boolean* includeSize
);
extern short is_memberpointerequal(Type* a, Type* b);
extern short is_typeequal(Type* a, Type* b);
extern short iscpp_typeequal(Type* a, Type* b);
extern short CParser_CompareArgLists(FuncArg* a, FuncArg* b);
extern Boolean is_arglistsame(FuncArg* a, FuncArg* b);
extern short is_typesame(Type* a, Type* b);
extern Type* CParser_GetBoolType(void);
extern Type* CParser_GetWCharType(void);
extern short CParser_GetOperator(ENodeType t);
extern Boolean CParser_IsMoreCVQualified(UInt32 a, UInt32 b);
extern Boolean CParser_IsSameOrMoreCVQualified(UInt32 a, UInt32 b);
extern Boolean is_unsigned(Type* type);
extern StructMember* ismember(TypeStruct* tstruct, HashNameNode* name);
extern void appendmember(TypeStruct* tstruct, StructMember* member);
extern Boolean isdeclaration(
    UInt8 scanDeclarator, UInt8 requireName, UInt8 isNewExpression, short token
);
extern Boolean islookaheaddeclaration(void);
extern Type* CParser_ParseTypeID(UInt32* qual, Boolean* isTemplateName);
extern Boolean CParser_TryParamList(Boolean stopAtComma);
extern Type* CParser_RemoveTopMostQualifiers(Type* type, UInt32* qual);
extern UInt32 CParser_GetTypeQualifiers(Type* type, UInt32 qual);
extern UInt32 CParser_GetCVTypeQualifiers(Type* type, UInt32 qual);
#if VERSION >= VERSION_GC_2_5
extern Boolean CParser_IsReadOnly(Type* type, UInt32 qual);
#endif
extern Boolean CParser_IsConst(Type* type, UInt32 qual);
extern Boolean CParser_IsVolatile(Type* type, UInt32 qual);
extern Boolean is_const_object(Object* obj);
#if MWCCPSP
extern Boolean CParser_IsDataObject(Object* obj);
#endif
extern Boolean is_volatile_object(Object* obj);
extern Boolean CParserIsConstExpr(ENode* expr);
extern Boolean CParserIsVolatileExpr(ENode* expr);
extern Boolean CParser_HasInternalLinkage(const Object* obj);
extern Boolean CParser_HasInternalLinkage2(const Object* obj);
extern Boolean CParser_IsVirtualFunction(Object* obj, TypeClass** tclass, SInt32* index);
extern Boolean is_pascal_object(Object* obj);
extern void CParser_ParseAttribute(Type* type, DeclInfo* declinfo);
extern void CParser_ParseDeclSpec(DeclInfo* declinfo, Boolean unused);
#if MWCCPSP
extern Boolean CParser_CheckTemplateClassUsage(TypeClass* tclass, Boolean reportError);
#define CPARSER_CHECKTEMPLCLASSUSAGE(t, r) CParser_CheckTemplateClassUsage(TYPE_CLASS(t), r)
#else
extern Boolean CParser_CheckTemplateClassUsage(TemplClass* tmclass, Boolean reportError);
#endif
extern void CParser_GetDeclSpecs(DeclInfo* di, Boolean resolveMembers);
extern void CParser_RegisterNonGlobalClass(TypeClass* tclass);
extern void CParser_RegisterSingleExprFunction(Object* func, ENode* expr);
extern void CParser_RegisterDummyCtorFunction(Object* func, Object* obj);
extern Boolean CParser_IsAnonymousUnion(DeclInfo* di, Boolean allowStruct);
extern void CParser_CheckAnonymousUnion(DeclInfo* di, Boolean isLocal);
extern void CParser_NewCallBackAction(Object* obj, TypeClass* tclass);
extern void CParser_NewClassAction(TypeClass* tclass);
extern void CParser_CallBackAction(Object* obj);
extern Object* CParser_ParseObject(void);
extern void CParser_ParseGlobalDeclaration(void);
extern void cparser(void);

#ifdef __cplusplus
}
#endif

#endif
