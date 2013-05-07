/^\#ifdef/p
/^\#endif/p
/^\#define/!d
s/^\#define //
s/^\([^[:space:]]*\).*$/{ "\1", \1 }, /
