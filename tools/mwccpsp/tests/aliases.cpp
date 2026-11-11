// objects reached through using-declarations and namespace aliases: the
// referenced object must be marked used through the alias chain
namespace inner {
static int hidden = 7;
static int unused_hidden = 9;
static const char* name = "inner";
int counter;
inline int bump() { return ++counter; }
}
namespace outer {
using inner::hidden;
using inner::counter;
using inner::bump;
}
namespace io = inner;

using outer::hidden;
using outer::counter;

static int* ptr = &hidden;

int f(int v) {
    using io::name;
    counter += v;
    return hidden + *ptr + name[0] + outer::bump() + io::bump();
}

template <class T> T get(T* p) { return *p; }
int g() {
    using inner::hidden;
    return get(&hidden) + get(&outer::counter);
}
