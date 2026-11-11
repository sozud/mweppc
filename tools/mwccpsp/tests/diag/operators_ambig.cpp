// operator overloading, conversion operators and constructors
struct Fixed {
    int raw;
    Fixed() : raw(0) {}
    Fixed(int i) : raw(i << 8) {}
    explicit Fixed(float f) : raw((int)(f * 256.0f)) {}
    operator int() const { return raw >> 8; }
    Fixed operator+(const Fixed& o) const { Fixed r; r.raw = raw + o.raw; return r; }
    Fixed operator-() const { Fixed r; r.raw = -raw; return r; }
    Fixed& operator+=(const Fixed& o) { raw += o.raw; return *this; }
    Fixed operator*(const Fixed& o) const { Fixed r; r.raw = (raw * o.raw) >> 8; return r; }
    bool operator<(const Fixed& o) const { return raw < o.raw; }
    bool operator!() const { return raw == 0; }
};

Fixed operator/(const Fixed& a, int d) { Fixed r; r.raw = a.raw / d; return r; }

struct Vec {
    int x, y;
    Vec(int a = 0, int b = 0) : x(a), y(b) {}
    int& operator[](int i) { return i ? y : x; }
    Vec operator,(const Vec& o) const { return Vec(x + o.x, y + o.y); }
    int operator()(int a, int b) const { return x * a + y * b; }
    Vec* operator->() { return this; }
};

struct Str {
    const char* p;
    Str(const char* s) : p(s) {}
    operator const char*() const { return p; }
    char operator*() const { return *p; }
    Str& operator++() { ++p; return *this; }
};

struct Bits {
    unsigned v;
    Bits(unsigned x) : v(x) {}
    Bits operator|(Bits o) const { return Bits(v | o.v); }
    Bits operator&(Bits o) const { return Bits(v & o.v); }
    Bits operator^(Bits o) const { return Bits(v ^ o.v); }
    Bits operator~() const { return Bits(~v); }
    Bits operator<<(int n) const { return Bits(v << n); }
    Bits& operator>>=(int n) { v >>= n; return *this; }
};

int use_fixed(int a, int b) {
    Fixed x(a), y(b), z(1.5f);
    x += y;
    Fixed w = -(x + z) * Fixed(2) / 3;
    return (int)w + (x < y) + !Fixed(0) + (x + 1);
}

int use_vec(int a) {
    Vec v(a, 2), u(3, a);
    v[1] = 7;
    Vec s = (v, u);
    return s(2, 3) + v->x + u[0];
}

int use_str(const char* s) {
    Str t(s);
    ++t;
    const char* q = t;
    return *t + (q - s);
}

unsigned use_bits(unsigned a) {
    Bits b(a);
    b >>= 2;
    return ((b | Bits(1)) & ~Bits(2) ^ (b << 3)).v;
}
