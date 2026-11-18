#include "m3u.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    IptvChannel channel;
    const char *valid = "#EXTINF:-1 tvg-id=\"news.br\" tvg-logo=\"https://example.org/logo.png\" group-title=\"Noticias\",Canal de Noticias HD";
    assert(iptv_parse_extinf(valid, &channel));
    assert(strcmp(channel.name, "Canal de Noticias HD") == 0);
    assert(strcmp(channel.id, "news.br") == 0);
    assert(strcmp(channel.logo, "https://example.org/logo.png") == 0);
    assert(strcmp(channel.group, "Noticias") == 0);

    assert(iptv_parse_extinf("#EXTINF:-1,Canal, versao HD", &channel));
    assert(strcmp(channel.name, "Canal, versao HD") == 0);
    assert(channel.group[0] == '\0');
    assert(!iptv_parse_extinf("#EXTVLCOPT:http-user-agent=demo", &channel));
    assert(!iptv_parse_extinf("#EXTINF:-1 tvg-id=\"sem-nome\"", &channel));
    return 0;
}
