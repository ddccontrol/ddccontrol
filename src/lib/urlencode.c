#include "urlencode.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *url_encode(const char *s) {
    if (!s) return NULL;

    size_t len = 0;
    const unsigned char *p;

    for (p = (const unsigned char *)s; *p; p++) {
        if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
            len += 1;
        } else {
            len += 3;
        }
    }

    char *out = malloc(len + 1);
    if (!out) {
        return NULL;
    }

    char *q = out;
    for (p = (const unsigned char *)s; *p; p++) {
        if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
            *q++ = (char)*p;
        } else {
            sprintf(q, "%%%02X", *p);
            q += 3;
        }
    }

    *q = '\0';
    return out;
}
