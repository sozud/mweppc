// C++ diagnostics: an ambiguous overload and access to a private member.
void k(int);
void k(long);

class C {
    int priv;
};

int use(C& c) {
    k(1u);
    return c.priv;
}
