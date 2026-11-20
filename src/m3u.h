#ifndef IPTV_M3U_H
#define IPTV_M3U_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

enum {
    IPTV_CHANNEL_NAME_BYTES = 128,
    IPTV_CHANNEL_GROUP_BYTES = 64,
    IPTV_CHANNEL_LOGO_BYTES = 256,
    IPTV_CHANNEL_ID_BYTES = 64,
    IPTV_CHANNEL_URL_BYTES = 512,
    IPTV_MAX_CHANNELS = 64
};

typedef struct {
    char name[IPTV_CHANNEL_NAME_BYTES];
    char group[IPTV_CHANNEL_GROUP_BYTES];
    char logo[IPTV_CHANNEL_LOGO_BYTES];
    char id[IPTV_CHANNEL_ID_BYTES];
    char url[IPTV_CHANNEL_URL_BYTES];
} IptvChannel;

/* Parses one #EXTINF line. Returns false for malformed lines or an empty channel name. */
bool iptv_parse_extinf(const char *line, IptvChannel *channel);

/* Reads #EXTINF/URL pairs from an already-open M3U stream.
 * Returns the number of complete channels, or -1 on an I/O/argument error. */
int iptv_playlist_read(FILE *stream, IptvChannel *channels, size_t capacity);

#endif
