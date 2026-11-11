// template instantiation from function bodies, nested instantiation,
// recursive class templates and #pragma template_depth
#pragma template_depth(80)

template <int N> struct Fact {
    enum { value = N * Fact<N - 1>::value };
};
template <> struct Fact<0> {
    enum { value = 1 };
};

template <class T> inline T twice(T x) { return x + x; }

template <class T> inline T quad(T x) { return twice(twice(x)); }

template <class T> struct Holder {
    T v;
    Holder(T x) : v(x) {}
    T get() const { return quad(v); }
    template <class U> U as() const { return (U)twice(v); }
};

template <class T, int N> struct Chain {
    static T sum(T x) { return x + Chain<T, N - 1>::sum(x); }
};
template <class T> struct Chain<T, 0> {
    static T sum(T x) { return x; }
};

int use_fact() { return Fact<10>::value; }

int use_holder(int a) {
    Holder<int> h(a);
    Holder<short> s((short)a);
    return h.get() + s.get() + h.as<char>();
}

long use_chain(long x) { return Chain<long, 40>::sum(x); }

float use_twice(float f) { return twice(f) + quad(f); }

template <class T> T (*pick(int sel))(T) {
    T (*f)(T);
    if (sel) {
        f = &twice<T>;
    } else {
        f = &quad<T>;
    }
    return f;
}

int use_pick(int sel, int v) { return pick<int>(sel)(v); }
