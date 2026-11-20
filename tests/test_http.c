#include "http.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    IptvHttpUrl u;
    assert(iptv_http_parse_url("http://Example.COM:8080/live/list.m3u?x=1", &u));
    assert(strcmp(u.host, "example.com") == 0);
    assert(strcmp(u.path, "/live/list.m3u?x=1") == 0);
    assert(u.port == 8080 && !u.tls);
    assert(iptv_http_parse_url("https://example.org/stream.m3u8", &u));
    assert(strcmp(u.host, "example.org") == 0 && u.port == 443 && u.tls);
    assert(iptv_http_parse_url("http://example.org", &u));
    assert(strcmp(u.path, "/") == 0);
    assert(!iptv_http_parse_url("ftp://example.org/a", &u));
    assert(!iptv_http_parse_url("http://user@example.org/a", &u));
    assert(!iptv_http_parse_url("http://example.org:99999/a", &u));
    return 0;
}
