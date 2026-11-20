#ifndef IPTV_NET_PLAYLIST_H
#define IPTV_NET_PLAYLIST_H

#include <stddef.h>

/* Downloads an HTTP(S) M3U playlist to a temporary file and atomically
 * replaces destination only after a complete 2xx response. */
int iptv_download_playlist(const char *url, const char *destination,
                           size_t maximum_bytes, size_t *downloaded_bytes);

#endif
