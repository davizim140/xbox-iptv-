#include "net_playlist.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *file;
    size_t bytes;
    size_t maximum;
    int overflow;
} DownloadSink;

static size_t write_file(char *data, size_t size, size_t count, void *opaque)
{
    DownloadSink *sink = (DownloadSink *)opaque;
    if (size != 0 && count > (size_t)-1 / size)
        return 0;
    const size_t bytes = size * count;
    if (bytes > sink->maximum - sink->bytes) {
        sink->overflow = 1;
        return 0;
    }
    if (fwrite(data, 1, bytes, sink->file) != bytes)
        return 0;
    sink->bytes += bytes;
    return bytes;
}

int iptv_download_playlist(const char *url, const char *destination,
                           size_t maximum_bytes, size_t *downloaded_bytes)
{
    if (downloaded_bytes)
        *downloaded_bytes = 0;
    if (!url || !destination || !*destination || maximum_bytes == 0)
        return -1;

    char temporary[768];
    const int length = snprintf(temporary, sizeof(temporary), "%s.part", destination);
    if (length < 0 || (size_t)length >= sizeof(temporary))
        return -2;

    FILE *file = fopen(temporary, "wb");
    if (!file)
        return -3;

    DownloadSink sink = { file, 0, maximum_bytes, 0 };
    CURL *curl = NULL;
    CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (result != CURLE_OK) {
        fclose(file);
        remove(temporary);
        return -4;
    }
    curl = curl_easy_init();
    if (!curl) {
        curl_global_cleanup();
        fclose(file);
        remove(temporary);
        return -4;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 15000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 45000L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 15L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "game:\\cacert.pem");
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    result = curl_easy_perform(curl);
    long status = 0;
    (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    const int close_error = fclose(file);
    if (result != CURLE_OK || status < 200 || status >= 300 ||
        sink.overflow || close_error != 0 || sink.bytes == 0) {
        remove(temporary);
        if (sink.overflow)
            return -5;
        return -6;
    }
    if (rename(temporary, destination) != 0) {
        remove(temporary);
        return -7;
    }
    if (downloaded_bytes)
        *downloaded_bytes = sink.bytes;
    return 0;
}
