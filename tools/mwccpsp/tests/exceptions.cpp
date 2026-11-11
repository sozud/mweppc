// C++ exceptions: destructors to run during unwinding (locals, members,
// bases, arrays, deletes), catch blocks and specifications: the
// exception-table code (Exceptions.c).
struct A { int x; A(int v) : x(v) {} ~A(); virtual int f(); };
struct B : A { A m; B(int v) : A(v), m(v + 1) {} ~B(); };
extern void g(int);
extern int c;

int h(int v) {
    A a(v);
    try { g(v); A b(v + 1); g(b.x); }
    catch (int e) { return e + a.x; }
    catch (A& x) { return x.x; }
    catch (...) { throw; }
    return a.f();
}
int arr(int n) { A as[3] = { A(1), A(2), A(n) }; g(as[2].x); return as[0].f(); }
int cond(int n) { if (n) { A a(n); g(a.x); return a.f(); } B b(n); g(b.m.x); return b.f(); }
int del(int n) { A* p = new A(n); g(p->x); int r = p->f(); delete p; return r; }
void spec(int n) throw(int) { A a(n); if (n) throw n; g(a.x); }
int nested(int n) { try { try { g(n); } catch (int) { g(1); throw; } } catch (...) { return 2; } return 0; }
