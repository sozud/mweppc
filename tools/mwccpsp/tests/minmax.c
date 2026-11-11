/* GCC minimum/maximum operators (3.0.1, #pragma gcc_extensions) */
#pragma gcc_extensions on
int imin(int a, int b) { return a <? b; }
int imax(int a, int b) { return a >? b; }
int cmin = 4 <? 9;
int cmax = 4 >? 9;
float fmin(float a, float b) { return a <? b; }
