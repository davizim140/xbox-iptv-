#ifndef IPTV_EPG_H
#define IPTV_EPG_H
#include <stddef.h>
#define IPTV_EPG_MAX_PROGRAMMES 64
typedef struct {
 char channel_id[64];
 char start[32];
 char stop[32];
 char title[128];
 char description[256];
} IptvProgramme;
/* Bounded XMLTV subset parser: extracts programme/channel/time/title/desc text. */
int iptv_epg_parse(const char *xml, IptvProgramme *items, size_t capacity);
#endif
