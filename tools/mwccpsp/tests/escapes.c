/* character and string escapes: hex, octal, universal character names */

char c1 = '\x41';
char c2 = '\101';
char c3 = '\7';
char c4 = '\e';
int c5 = '\xff';
char s1[] = "\x41\x4a\101\12\t\v\f\a\b\r\n\\\"\'\?";
char s2[] = "\x7fz\0101";
short w1 = L'é';
short w2 = L'\x1234';
int w3 = L'\U0001F600';
const void* ws = L"é中\x41\101";
const int wi[] = {L'A', L'\U00000042', L'\x43'};

int f(int i) {
    switch (i) {
        case '\x01':
            return 'A';
        case '\177':
            return L'\U00000100';
        default:
            return "é"[0];
    }
}
