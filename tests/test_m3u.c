#include "m3u.h"

#include <assert.h>
#include <stdio.h>
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

    FILE *playlist = tmpfile();
    assert(playlist != NULL);
    fputs("#EXTM3U\n", playlist);
    fputs("#EXTINF:-1 tvg-id=\"one\" group-title=\"News\",Canal Um HD\n", playlist);
    fputs("https://example.org/one.m3u8\n", playlist);
    fputs("#EXTVLCOPT:http-user-agent=ignored\n", playlist);
    fputs("#EXTINF:-1,Canal Dois\n", playlist);
    fputs("https://example.org/two.m3u8\n", playlist);
    rewind(playlist);

    IptvChannel channels[4];
    const int count = iptv_playlist_read(playlist, channels, 4);
    assert(count == 2);
    assert(strcmp(channels[0].name, "Canal Um HD") == 0);
    assert(strcmp(channels[0].url, "https://example.org/one.m3u8") == 0);
    assert(strcmp(channels[0].group, "News") == 0);
    assert(strcmp(channels[1].name, "Canal Dois") == 0);
    assert(strcmp(channels[1].url, "https://example.org/two.m3u8") == 0);
    fclose(playlist);

    return 0;
}
