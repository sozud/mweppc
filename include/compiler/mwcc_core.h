#ifndef COMPILER_MWCC_CORE_H
#define COMPILER_MWCC_CORE_H

// Where a build's shared compiler code (front end, optimizer, back end) sits
// among the PS2 builds, established from the code itself, not from the
// product version strings (b69 reports 3.0.3, b52 "3.0 build 52"; the build
// numbers don't give the order either). Oldest first:
//
//   b69   mwccps2 build 69 (Silent Hill 3): the GameCube 2.x front end and
//         optimizer (its optimizer phases are exactly GC 1.3 .. 2.7's) with
//         the MIPS back end
//   b52   mwccps2 3.0 build 52 (Monster Hunter): b69's code plus 3.0.1's
//         MPCode and IRO optimizer (GC 3.0a3's phases minus eight), C99
//         designated initializers and GCC statement expressions
//   upstream: mwccps2 b210 and mwccpsp b219 (3.0.1)
//
// The b69/b52 differences from the upstream code are guarded by position:
//     #if MWCC_CORE && MWCC_CORE <= MWCC_CORE_B52   b69 and b52
//     #if MWCC_CORE && MWCC_CORE < MWCC_CORE_B52    b69 only
//     #if MWCC_CORE == MWCC_CORE_B52                b52 only
// MWCC_CORE is 0 for every other build (b210, b219, the GameCube compilers),
// so none of these apply there. A build selects its position with
// -DMWCCPS2_B69=1 or -DMWCCPS2_B52=1 (tools/mwccps2/b69, tools/mwccps2/b52).

#define MWCC_CORE_B69 69
#define MWCC_CORE_B52 152 // after b69 by its code, despite the lower build number

#if defined(MWCCPS2_B52) && MWCCPS2_B52
#define MWCC_CORE MWCC_CORE_B52
#elif defined(MWCCPS2_B69) && MWCCPS2_B69
#define MWCC_CORE MWCC_CORE_B69
#else
#define MWCC_CORE 0
#endif

// b210/b198 code (MWCCPS2) that calls b210 functions no source has yet: the
// CodeWarrior build (tools/exact) compiles it, as the originals' code, while a
// native build (MWCCPSP_NATIVE), which could not run the missing callees, compiles 3.0.1 b219's
// code there instead (the guard's #else). Each use goes once its callees exist.
#if defined(MWCCPS2) && MWCCPS2 && !(defined(MWCCPSP_NATIVE) && MWCCPSP_NATIVE)
#define MWCCPS2_B210_PARTIAL 1
#else
#define MWCCPS2_B210_PARTIAL 0
#endif

#endif
