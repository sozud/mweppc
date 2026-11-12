#ifndef VERSION_H
#define VERSION_H

#define VERSION_GC_2_0 0
#define VERSION_GC_2_0P2 1
#define VERSION_GC_2_5 2
#define VERSION_GC_2_6 3
#define VERSION_GC_2_7 4

#ifndef VERSION
#define VERSION VERSION_GC_2_6
#endif

#if MWCCPSP
/* PS2 compiler builds (mwccps2 3.0.1 bNNN). MWCCPS2 marks any PS2 build;
   code that differs between PS2 builds tests MWCCPS2_BUILD, e.g.
   `#if MWCCPS2_BUILD >= 145`. b210 (Persona 4) is the default; build_ps2.py
   also passes it on the command line (MWCCPS2_BUILD in the environment). */
#if MWCCPS2 && !defined(MWCCPS2_BUILD)
#define MWCCPS2_BUILD 210
#endif

#endif
#endif
