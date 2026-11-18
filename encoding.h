#ifndef LOKI_ENCODING_H
#define LOKI_ENCODING_H

#include <stddef.h>

/* Return the converted buffer or the original string on conversion failure. */
char *convert_utf8(char *text, const char *encoding, char *buffer, size_t capacity);

#endif
