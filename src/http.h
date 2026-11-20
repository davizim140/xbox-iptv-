#ifndef IPTV_HTTP_H
#define IPTV_HTTP_H

#include <stddef.h>
#include <stdbool.h>

enum { IPTV_HTTP_MAX_RESPONSE = 524288 };

typedef struct {
    char host[256];
    char path[1024];
    unsigned short port;
    bool tls;
} IptvHttpUrl;

/* Accepts http:// and https:// URLs. */
bool iptv_http_parse_url(const char *url, IptvHttpUrl *parsed);

/* Fetches an HTTP resource into caller storage. HTTPS is currently rejected
 * until the mbedTLS transport is linked and certificate validation is wired. */
int iptv_http_get(const char *url, unsigned char *body, size_t capacity,
                  size_t *body_length, int *http_status);

#endif
