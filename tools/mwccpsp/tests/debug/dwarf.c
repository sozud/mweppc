/* debug info: globals, statics, locals, parameters, types */
typedef struct Point {
    short x, y;
    unsigned char flags : 3;
    unsigned char kind : 5;
} Point;

enum Color { RED, GREEN = 4, BLUE };

struct Node {
    struct Node* next;
    Point pos;
    enum Color color;
    int values[4];
    float scale;
    double weight;
    const char* name;
};

int g_count;
static struct Node g_nodes[8];
Point g_origin = { 1, 2 };
const char* g_names[] = { "a", "b", "c" };
static int (*g_callback)(int, struct Node*);
volatile unsigned long long g_ticks;

static int helper(int a, struct Node* n) {
    int i;
    int sum = 0;
    for (i = 0; i < 4; i++) {
        sum += n->values[i] * a;
    }
    return sum + n->pos.x;
}

float scale_all(float factor, int count) {
    static int calls;
    struct Node* n;
    float total = 0.0f;
    int i;

    calls++;
    for (i = 0; i < count && i < 8; i++) {
        n = &g_nodes[i];
        n->scale *= factor;
        total += n->scale;
        {
            double w = n->weight * factor;
            n->weight = w;
        }
    }
    g_callback = helper;
    return total;
}

enum Color pick(Point* p, char c) {
    switch (c) {
    case 'r':
        return RED;
    case 'g':
        p->flags = 1;
        return GREEN;
    default:
        p->kind = c;
        return BLUE;
    }
}

int main(void) {
    Point local = g_origin;
    g_count = g_callback ? g_callback(3, &g_nodes[0]) : 0;
    g_ticks++;
    return pick(&local, 'g') + (int)scale_all(2.0f, g_count);
}
