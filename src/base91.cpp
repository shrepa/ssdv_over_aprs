#include "base91.h"

static const char enctab[91] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
  "!#$%&()*+,./:;<=>?@[]^_`{|}~\"";

size_t base91_encode(const uint8_t *data, size_t len, char *out) {
    uint32_t b = 0;
    uint32_t n = 0;
    size_t outlen = 0;

    for (size_t i = 0; i < len; ++i) {
        b |= ((uint32_t)data[i]) << n;
        n += 8;
        if (n > 13) {
            uint32_t v = b & 8191;
            if (v > 88) {
                b >>= 13;
                n -= 13;
            } else {
                v = b & 16383;
                b >>= 14;
                n -= 14;
            }
            out[outlen++] = enctab[v % 91];
            out[outlen++] = enctab[v / 91];
        }
    }

    if (n) {
        out[outlen++] = enctab[b % 91];
        if (n > 7 || b > 90)
            out[outlen++] = enctab[b / 91];
    }

    out[outlen] = '\0'; // Null terminate
    return outlen;
}
