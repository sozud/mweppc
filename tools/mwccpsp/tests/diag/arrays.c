/* 3.0.1 array declarator checks: total size limit, unsized inner array */
int big[0x40000000][4];
int inner[3][];
int ok[2][3];
