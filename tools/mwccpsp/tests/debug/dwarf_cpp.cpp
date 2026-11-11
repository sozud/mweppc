// debug info for C++: classes, members, methods, references, namespaces
namespace geo {
class Shape {
public:
    Shape(int id) : m_id(id), m_area(0.0f) {}
    virtual ~Shape() {}
    virtual float area() const { return m_area; }
    int id() const { return m_id; }
    static int s_count;

protected:
    int m_id;
    float m_area;
};

int Shape::s_count = 0;

class Rect : public Shape {
public:
    Rect(int id, float w, float h) : Shape(id), m_w(w), m_h(h) { s_count++; }
    float area() const { return m_w * m_h; }

private:
    float m_w, m_h;
};

template <class T> T twice(const T& v) { return v + v; }
}

struct Pair {
    int first;
    long second;
};

static float total(const geo::Shape& s, int& counter) {
    counter += s.id();
    return s.area();
}

int run(int n) {
    geo::Rect r(n, 2.0f, 3.0f);
    Pair p = { n, 2 };
    int counter = 0;
    float t = total(r, counter);
    int (geo::Shape::*getter)() const = &geo::Shape::id;
    return (int)t + geo::twice(p.first) + (r.*getter)() + counter + (int)p.second;
}
