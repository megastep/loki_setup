#include "encoding.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    char buffer[16];
    char latin1[] = "caf\303\251";
    char euro[] = "\342\202\254";

    assert(strcmp(convert_utf8(latin1, "ISO-8859-1", buffer, sizeof(buffer)), "caf\351") == 0);
    assert(convert_utf8(euro, "ISO-8859-1", buffer, sizeof(buffer)) == euro);
    assert(convert_utf8(latin1, "ISO-8859-1", buffer, 2) == latin1);
    return 0;
}
