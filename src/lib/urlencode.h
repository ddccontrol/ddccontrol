#ifndef URLENCODE_H
#define URLENCODE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * URL encodes a string before use in query parameters.
 *
 * - Returns a malloc allocated string (must be free()'d by caller)
 * - Returns NULL on error
 */
char *url_encode(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* URLENCODE_H */
