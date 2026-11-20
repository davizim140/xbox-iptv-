#ifndef IPTV_HLS_H
#define IPTV_HLS_H
#include <stdbool.h>
#include <stddef.h>
#define IPTV_HLS_MAX_ITEMS 64
#define IPTV_HLS_URI_BYTES 512
typedef struct {
    char uri[IPTV_HLS_URI_BYTES];
    char title[128];
    double duration;
    unsigned bandwidth;
    unsigned width;
    unsigned height;
} IptvHlsItem;
typedef struct {
    bool master;
    bool end_list;
    bool encrypted;
    unsigned target_duration;
    unsigned media_sequence;
    size_t count;
    IptvHlsItem items[IPTV_HLS_MAX_ITEMS];
} IptvHlsPlaylist;
/* Parses a UTF-8 HLS manifest. Resolves absolute and relative HTTP(S) URIs. */
bool iptv_hls_parse(const char *text, const char *manifest_url, IptvHlsPlaylist *out);
#endif
