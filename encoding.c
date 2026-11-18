#include "config.h"
#include "encoding.h"

#include <iconv.h>
#include <string.h>

char *convert_utf8(char *text, const char *encoding, char *buffer, size_t capacity)
{
    iconv_t converter = iconv_open(encoding, "UTF-8");
    if (converter == (iconv_t)-1)
        return text;

#ifdef ICONV_INPUT_CONST
    const char *input = text;
#else
    char *input = text;
#endif
    char *output = buffer;
    size_t remaining = strlen(text);
    size_t available = capacity;
    int converted = iconv(converter, &input, &remaining, &output, &available) == 0 &&
                    iconv(converter, NULL, NULL, &output, &available) == 0 &&
                    available > 0;
    iconv_close(converter);
    if (!converted)
        return text;
    *output = '\0';
    return buffer;
}
