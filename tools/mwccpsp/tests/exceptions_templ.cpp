// exception type names for classes nested in template instances
// (CExcept_MangleClassName 3.0.1 path) and plain nested classes
template <class T> struct Outer {
    struct Err {
        T code;
    };
    class Inner {
    public:
        struct Deep {
            int v;
        };
    };
};

template <class T, int N> struct Pair {
    struct Bad {
        T a[N];
    };
};

struct Plain {
    struct Err {
        int x;
    };
};

namespace ns {
template <class T> struct Box {
    struct Fail {};
};
} // namespace ns

void throw_int_err(int c) {
    Outer<int>::Err e;
    e.code = c;
    throw e;
}

void throw_char_err() { throw Outer<char>::Err(); }

void throw_deep() { throw Outer<short>::Inner::Deep(); }

void throw_pair() { throw Pair<long, 3>::Bad(); }

void throw_plain() { throw Plain::Err(); }

void throw_ns() { throw ns::Box<unsigned>::Fail(); }

void throw_ptr(Outer<float>::Err* p) { throw p; }

int catch_all(int sel) {
    try {
        switch (sel) {
            case 0: throw_int_err(sel); break;
            case 1: throw_char_err(); break;
            case 2: throw_deep(); break;
            case 3: throw_pair(); break;
            case 4: throw_plain(); break;
            case 5: throw_ns(); break;
            case 6: throw_ptr(0); break;
        }
    } catch (Outer<int>::Err& e) {
        return e.code;
    } catch (const Outer<char>::Err&) {
        return 10;
    } catch (Outer<short>::Inner::Deep d) {
        return d.v;
    } catch (Pair<long, 3>::Bad&) {
        return 30;
    } catch (Plain::Err&) {
        return 40;
    } catch (ns::Box<unsigned>::Fail) {
        return 50;
    } catch (Outer<float>::Err*) {
        return 60;
    }
    return -1;
}

void local_class() {
    struct Loc {
        int q;
    };
    try {
        throw Loc();
    } catch (Loc&) {
    }
}
