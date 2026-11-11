// member access: operator-> chains, class rvalues, pseudo destructor
// calls, members of class templates, struct members through pointers
struct P { int x, y; int sum() const { return x + y; } };
struct Q { P p; P* operator->() { return &p; } };
struct R { Q q; Q& operator->() { return q; } };
P mkp();
struct S { int a[3]; P p; };
S mks();
template <class T> struct Box { T v; T get() { return v; } T* operator->() { return &v; } };
template <class T> int tmem(T& t) { return t.x + t->y; }
typedef int I;
int f(Q& q, R& r, P* pp, S* sp, int* ip) {
    Box<P> b;
    b.v.x = 1;
    pp->~P();
    ip->~I();
    return q->x + r->y + mkp().x + mkp().sum() + mks().a[1] + mks().p.y + sp->p.x + sp->a[2] + b->y + b.get().x;
}
