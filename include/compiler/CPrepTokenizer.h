#ifndef COMPILER_CPREPTOKENIZER_H
#define COMPILER_CPREPTOKENIZER_H

#include "compiler/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MWCCPSP
#pragma enumsalwaysint on // int constants, as in 3.0.1 (built with minimal enums)
#endif
enum {
    CR = 13,
    LF = 10
};
#if MWCCPSP
#pragma enumsalwaysint reset
#endif

extern short prepskipnextchar(void);
extern short prepnextchar(void);
#if MWCCPSP
extern short prepcurchar(void);
#endif
extern short prepnextstringchar(unsigned char* str, Boolean countNewlines);
extern void CPrep_MatchChar(char ch, Boolean countNewlines);
extern char* CPrep_MatchChar2(char* start, char ch, Boolean countNewlines);
extern short prepcurchar(void);
extern char* ReadIdentifier(const char* p);
extern Boolean skipendoflinematch(unsigned char* str, short token);
extern void skipendofline(void);
extern short lookahead(void);
extern short lookahead_noeol(void);
extern short lex(void);
extern short plex(void);
extern short lexidentifier(void);

#if MWCCPSP
extern void skipcurchar(short c);
#endif
#if MWCCPSP

#endif
#ifdef __cplusplus
}
#endif

#endif
