#ifndef BASE91_H
#define BASE91_H

#include <stdint.h>
#include <stddef.h>

size_t base91_encode(const uint8_t *data, size_t len, char *out);

#endif
