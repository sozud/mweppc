/* Diagnostics that are warnings: the object is still written. */
#pragma warn_unusedvar on
#pragma warn_implicitconv on
#pragma warning_errors off

int h(long long v) {
    int unused;
    char c = v;
    return c;
}
