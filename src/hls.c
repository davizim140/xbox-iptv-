#include "hls.h"
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool resolve_uri(const char *base, const char *uri, char *out, size_t cap)
{
    if (strncmp(uri, "http://", 7) == 0 || strncmp(uri, "https://", 8) == 0)
        return snprintf(out, cap, "%s", uri) < (int)cap;
    IptvHttpUrl parsed;
    if (!iptv_http_parse_url(base, &parsed)) return false;
    char origin[320];
    bool default_port = (parsed.tls && parsed.port == 443) || (!parsed.tls && parsed.port == 80);
    int n = default_port
        ? snprintf(origin, sizeof(origin), "%s://%s", parsed.tls ? "https" : "http", parsed.host)
        : snprintf(origin, sizeof(origin), "%s://%s:%u", parsed.tls ? "https" : "http", parsed.host, (unsigned)parsed.port);
    if (n < 0 || (size_t)n >= sizeof(origin)) return false;
    if (uri[0] == '/') return snprintf(out, cap, "%s%s", origin, uri) < (int)cap;
    char *slash = strrchr(parsed.path, '/');
    if (!slash) return false;
    *slash = '\0';
    return snprintf(out, cap, "%s%s/%s", origin, parsed.path, uri) < (int)cap;
}
static bool append_text(char *out, size_t cap, const char *a, const char *b)
{
 size_t n=strlen(out), m=(size_t)(b-a);
 if(n+m>=cap) return false;
 memcpy(out+n,a,m); out[n+m]=0; return true;
}
static void trim(char *s)
{
    size_t n = strlen(s);
    while (n && (s[n-1] == '\r' || s[n-1] == '\n' || s[n-1] == ' ' || s[n-1] == '\t')) s[--n] = 0;
}
bool iptv_hls_parse(const char *text, const char *manifest_url, IptvHlsPlaylist *out)
{
    if (!text || !manifest_url || !out || strncmp(text, "#EXTM3U", 7) != 0) return false;
    memset(out, 0, sizeof(*out));
    char *copy = malloc(strlen(text) + 1);
    if (!copy) return false;
    strcpy(copy, text);
    bool next_variant = false, next_segment = false;
    unsigned pending_bandwidth = 0, pending_width = 0, pending_height = 0;
    double pending_duration = 0.0;
    char pending_title[128] = "";
    for (char *line = strtok(copy, "\n"); line; line = strtok(NULL, "\n")) {
        trim(line);
        if (!*line) continue;
        if (strncmp(line, "#EXT-X-STREAM-INF:", 18) == 0) {
            out->master = true; next_variant = true;
            const char *p = strstr(line, "BANDWIDTH=");
            if (p) pending_bandwidth = (unsigned)strtoul(p + 10, NULL, 10);
            p = strstr(line, "RESOLUTION=");
            if (p) (void)sscanf(p + 11, "%ux%u", &pending_width, &pending_height);
        } else if (strncmp(line, "#EXTINF:", 8) == 0) {
            next_segment = true;
            pending_duration = strtod(line + 8, NULL);
            const char *comma = strchr(line, ',');
            if (comma) { size_t n=strlen(comma+1); if(n>=sizeof(pending_title)) n=sizeof(pending_title)-1; memcpy(pending_title,comma+1,n); pending_title[n]=0; }
            else pending_title[0] = 0;
        } else if (strncmp(line, "#EXT-X-TARGETDURATION:", 22) == 0) {
            out->target_duration = (unsigned)strtoul(line + 22, NULL, 10);
        } else if (strncmp(line, "#EXT-X-MEDIA-SEQUENCE:", 22) == 0) {
            out->media_sequence = (unsigned)strtoul(line + 22, NULL, 10);
        } else if (strncmp(line, "#EXT-X-ENDLIST", 14) == 0) {
            out->end_list = true;
        } else if (strncmp(line, "#EXT-X-KEY:", 11) == 0 && strstr(line, "METHOD=NONE") == NULL) {
            out->encrypted = true;
        } else if (line[0] != '#') {
            if ((!next_variant && !next_segment) || out->count >= IPTV_HLS_MAX_ITEMS) continue;
            IptvHlsItem *item = &out->items[out->count];
            if (!resolve_uri(manifest_url, line, item->uri, sizeof(item->uri))) { free(copy); return false; }
            if (next_variant) {
                item->bandwidth = pending_bandwidth; item->width = pending_width; item->height = pending_height;
            } else {
                item->duration = pending_duration;
                memcpy(item->title, pending_title, sizeof(item->title)); item->title[sizeof(item->title)-1]=0;
            }
            out->count++;
            next_variant = next_segment = false;
            pending_title[0] = 0;
        }
    }
    free(copy);
    return out->count > 0;
}
