/* comparisons with constants as values: int and long long, edge constants */
int le_i(int a) { return a <= 5; }
int le_max(int a) { return a <= 0x7FFFFFFF; }
int le_big(int a) { return a <= 0x12345; }
int gt_i(int a) { return a > 7; }
int gt_neg(int a) { return a > -0x9000; }
int ne_i(int a) { return a != 3; }
int ne_zero(int a) { return a != 0; }
int ne_big(int a) { return a != 0x123456; }
int le_ll(long long a) { return a <= 0x123456789LL; }
int le_llmax(long long a) { return a <= 0x7FFFFFFFFFFFFFFFLL; }
int le_llsmall(long long a) { return a <= 9; }
int gt_ll(long long a) { return a > 0x1000000000LL; }
int gt_llsmall(long long a) { return a > -2; }
int ne_ll(long long a) { return a != 0x100000001LL; }
int ne_llsmall(long long a) { return a != 4; }
int ne_llzero(long long a) { return a != 0; }
unsigned ugt(unsigned a) { return a > 9u; }
int mixed(int a, long long b) { return (a <= 1) + (a > 2) + (a != 3) + (b <= 0x200000000LL) + (b > 0x300000000LL) + (b != 0x400000000LL); }
