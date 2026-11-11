// virtual and multiple inheritance, pure virtuals, covariant returns,
// base/member initializer order, virtual destructors
struct Base {
    int b;
    Base(int x) : b(x) {}
    virtual ~Base() {}
    virtual int get() const { return b; }
    virtual Base* clone() const { return new Base(b); }
};

struct Left : virtual Base {
    int l;
    Left(int x) : Base(x), l(x + 1) {}
    int get() const { return l + b; }
};

struct Right : virtual Base {
    int r;
    Right(int x) : Base(x * 2), r(x - 1) {}
    virtual int rv() const { return r; }
};

struct Diamond : Left, Right {
    int d;
    Diamond(int x) : Base(x * 3), Left(x), Right(x), d(x) {}
    int get() const { return Left::get() + Right::rv() + d; }
    Diamond* clone() const { return new Diamond(d); }
};

struct Shape {
    virtual int area() const = 0;
    virtual const char* name() const { return "shape"; }
    virtual ~Shape() {}
};

struct Sq : Shape {
    int s;
    Sq(int x) : s(x) {}
    int area() const { return s * s; }
};

struct Rect : Sq {
    int w;
    Rect(int a, int b) : Sq(a), w(b) {}
    int area() const { return s * w; }
    const char* name() const { return "rect"; }
};

struct M1 { int a; M1() : a(1) {} virtual int f() { return a; } };
struct M2 { int b; M2() : b(2) {} virtual int g() { return b; } };
struct MM : M1, M2 {
    int c;
    MM() : c(3) {}
    int f() { return a + c; }
    int g() { return b * c; }
};

int use_diamond(int x) {
    Diamond d(x);
    Base* p = &d;
    Left* l = &d;
    Right* r = &d;
    Base* c = p->clone();
    int v = p->get() + l->get() + r->rv() + c->get();
    delete c;
    return v;
}

int use_shape(int a, int b) {
    Shape* s[2];
    s[0] = new Sq(a);
    s[1] = new Rect(a, b);
    int t = s[0]->area() + s[1]->area() + s[1]->name()[0];
    delete s[0];
    delete s[1];
    return t;
}

int use_mm() {
    MM m;
    M2* p2 = &m;
    M1* p1 = &m;
    MM* back = static_cast<MM*>(p2);
    return p1->f() + p2->g() + back->c;
}
