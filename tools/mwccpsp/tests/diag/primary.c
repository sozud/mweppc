/* statement expressions under ANSI_strict without gcc_extensions */
#pragma ANSI_strict on
int g(int x) { return ({ x; }); }
int h(int x) { return (x + ; }
int k(int x) { return ); }
