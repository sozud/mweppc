// The native PS2 build's function trace. Every compiler unit is built with
// -finstrument-functions-after-inlining; when MWCCPS2_NATIVE_TRACE names a
// file, the first call of each function appends its address (minus the
// anchor's, so it can be matched against nm) to that file. The corpus script
// (tools/mwccps2/harness/native_corpus.py) maps them to names and reports
// which unverified functions each case ran.
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#define NO_TRACE __attribute__((no_instrument_function))

static int trace_state; // 0 not checked, 1 on, -1 off
static int trace_fd = -1;
static uintptr_t seen[1 << 16];

NO_TRACE static void put_hex(char* buf, int* n, uintptr_t v) {
    char tmp[20];
    int k = 0;
    do {
        tmp[k++] = "0123456789abcdef"[v & 15];
        v >>= 4;
    } while (v);
    while (k) {
        buf[(*n)++] = tmp[--k];
    }
}

NO_TRACE void __cyg_profile_func_enter(void* fn, void* site) {
    uintptr_t a = (uintptr_t)fn;
    unsigned h;
    char buf[64];
    int n = 0;

    (void)site;
    if (trace_state < 0) {
        return;
    }
    if (trace_state == 0) {
        const char* path = getenv("MWCCPS2_NATIVE_TRACE");
        trace_state = -1;
        if (!path || (trace_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644)) < 0) {
            return;
        }
        trace_state = 1;
        // the anchor: this function's own address (its nm name is known)
        buf[n++] = 'a';
        buf[n++] = ' ';
        put_hex(buf, &n, (uintptr_t)&__cyg_profile_func_enter);
        buf[n++] = '\n';
        write(trace_fd, buf, n);
        n = 0;
    }
    h = (unsigned)((a >> 4) ^ (a >> 20)) & 0xFFFF;
    while (seen[h]) {
        if (seen[h] == a) {
            return;
        }
        h = (h + 1) & 0xFFFF;
    }
    seen[h] = a;
    put_hex(buf, &n, a);
    buf[n++] = '\n';
    write(trace_fd, buf, n);
}

NO_TRACE void __cyg_profile_func_exit(void* fn, void* site) {
    (void)fn;
    (void)site;
}
