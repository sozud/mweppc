/* 3.0.1: an empty array must be the last struct member. */
struct Ok { int n; int a[]; };
struct Bad { int n; int a[]; int b; };
