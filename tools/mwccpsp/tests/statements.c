/* every statement kind through 3.0.1's statement/CFunc_LoopStatement/
   CFunc_ParseCondition/CFunc_ParseLocalDeclaration */
extern int g(int);
int f(int n, int* p) {
    int i, j = 2, k[3];
    int sum = 0;
    for (i = 0; i < n; i++) {
        if (i == 3) continue;
        if (i > 10) break;
        sum += g(i);
    }
    for (;;) {
        if (sum > 100) break;
        sum++;
    }
    for (; j < 5;) j++;
    while (n-- > 0) {
        sum ^= n;
    }
    do {
        sum--;
    } while (sum > 50);
    switch (sum & 3) {
        case 0:
            sum += 1;
        case 1:
            break;
        default:
            sum = 7;
    }
    switch (n) {
    }
    if (p)
        *p = sum;
    else
        sum = 0;
    goto done;
again:
    sum++;
done:
    if (sum < 0) goto again;
    k[0] = j;
    {
        int inner = k[0];
        sum += inner;
    }
    ;
    return sum;
}
void v(int* p) {
    if (!p) return;
    *p = 0;
    return;
}
