// call diagnostics
struct D { int v; ~D(); };
int one(int);
int (*fp)(int, int);
void g(D* d, int i) {
    d->~D(1);
    one();
    one(1, 2);
    fp(1);
    i(2);
    undeclared_fn(3);
}
