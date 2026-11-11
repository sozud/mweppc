// partial specialization, member templates, default template arguments,
// non-type parameters, template friends and dependent names
template <class T, class U = int> struct Pair {
    T first;
    U second;
    Pair() : first(), second() {}
    Pair(const T& a, const U& b) : first(a), second(b) {}
    template <class V> Pair(const Pair<V, U>& o) : first((T)o.first), second(o.second) {}
    template <class V> V sum() const { return (V)first + (V)second; }
};

template <class T> struct Traits {
    enum { is_ptr = 0 };
    typedef T base;
};
template <class T> struct Traits<T*> {
    enum { is_ptr = 1 };
    typedef T base;
};
template <class T> struct Traits<const T*> {
    enum { is_ptr = 2 };
    typedef T base;
};

template <int N, class T> struct Array {
    T data[N];
    int size() const { return N; }
    T& operator[](int i) { return data[i]; }
    const T& operator[](int i) const { return data[i]; }
    T total() const {
        T t = T();
        for (int i = 0; i < N; i++) t += data[i];
        return t;
    }
};

template <class T> class Counter {
    T n;
public:
    Counter() : n(0) {}
    Counter& operator++() { ++n; return *this; }
    Counter operator++(int) { Counter c = *this; ++n; return c; }
    friend bool operator==(const Counter& a, const Counter& b) { return a.n == b.n; }
    T value() const { return n; }
};

template <class C> typename C::value_type first_of(const C& c) { return c.items[0]; }

struct IntBox {
    typedef int value_type;
    int items[4];
};

template <class T> T maxof(T a, T b) { return a < b ? b : a; }
template <class T> T maxof(T a, T b, T c) { return maxof(maxof(a, b), c); }
template <> int maxof<int>(int a, int b) { return a > b ? a : b; }

int use_pair() {
    Pair<short> p((short)3, 4);
    Pair<long> q(p);
    return p.sum<int>() + (int)q.sum<long>();
}

int use_traits() {
    return Traits<int>::is_ptr + Traits<char*>::is_ptr * 10 + Traits<const char*>::is_ptr * 100;
}

int use_array() {
    Array<5, int> a;
    for (int i = 0; i < a.size(); i++) a[i] = i * i;
    const Array<5, int>& c = a;
    return c.total() + c[2];
}

int use_counter() {
    Counter<unsigned char> c, d;
    c++;
    ++c;
    d = c++;
    return (c == d) + c.value() * 2 + d.value();
}

int use_dep() {
    IntBox b = {{7, 8, 9, 10}};
    return first_of(b) + maxof(1, 5) + maxof(2.0f, 1.0f, 3.0f) + maxof<char>('a', 'b');
}
