/* member access in C: struct returns, nested members, unions, arrays of structs */
struct In { int a, b; };
struct Out { struct In in[2]; union { int i; float f; } u; int last; };
struct Out mk(int);
struct Out g;
int f(struct Out* p, int i) {
    struct Out local = mk(i);
    return mk(1).in[1].b + mk(2).last + p->in[i].a + p->u.i + g.in[0].b + local.u.i + (&g)->last + (*p).in[1].a;
}
