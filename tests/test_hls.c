#include "hls.h"
#include <assert.h>
#include <string.h>
int main(void) {
 IptvHlsPlaylist p;
 const char *master="#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1800000,RESOLUTION=1280x720\n720/index.m3u8\n";
 assert(iptv_hls_parse(master,"https://cdn.example/live/master.m3u8",&p));
 assert(p.master && p.count==1 && p.items[0].bandwidth==1800000);
 assert(strcmp(p.items[0].uri,"https://cdn.example/live/720/index.m3u8")==0);
 const char *media="#EXTM3U\n#EXT-X-TARGETDURATION:6\n#EXT-X-MEDIA-SEQUENCE:12\n#EXTINF:5.5,First\nseg1.ts\n#EXTINF:6,Second\nhttps://cdn.example/seg2.ts\n#EXT-X-ENDLIST\n";
 assert(iptv_hls_parse(media,"http://example.org/path/list.m3u8",&p));
 assert(!p.master && p.end_list && p.count==2 && p.media_sequence==12);
 assert(p.items[0].duration>5.4 && strcmp(p.items[0].title,"First")==0);
 assert(strcmp(p.items[0].uri,"http://example.org/path/seg1.ts")==0);
 assert(strcmp(p.items[1].uri,"https://cdn.example/seg2.ts")==0);
 const char *enc="#EXTM3U\n#EXT-X-KEY:METHOD=AES-128,URI=\"key\"\n#EXTINF:4,Encrypted\na.ts\n";
 assert(iptv_hls_parse(enc,"https://x.example/a.m3u8",&p) && p.encrypted);
 return 0;
}