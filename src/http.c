#include "http.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static bool copy_part(char *out, size_t cap, const char *begin, size_t len)
{
    if (cap == 0 || len >= cap) return false;
    memcpy(out, begin, len);
    out[len] = '\0';
    return true;
}

bool iptv_http_parse_url(const char *url, IptvHttpUrl *parsed)
{
    if (!url || !parsed) return false;
    memset(parsed, 0, sizeof(*parsed));
    const char *authority;
    if (strncmp(url, "http://", 7) == 0) {
        parsed->tls = false;
        parsed->port = 80;
        authority = url + 7;
    } else if (strncmp(url, "https://", 8) == 0) {
        parsed->tls = true;
        parsed->port = 443;
        authority = url + 8;
    } else return false;

    const char *end = authority + strcspn(authority, "/?#");
    if (end == authority || memchr(authority, '@', (size_t)(end - authority))) return false;
    const char *host_end = end;
    const char *colon = memchr(authority, ':', (size_t)(end - authority));
    if (colon) {
        host_end = colon;
        if (colon + 1 == end) return false;
        unsigned long port = 0;
        for (const char *p = colon + 1; p < end; p++) {
            if (!isdigit((unsigned char)*p)) return false;
            port = port * 10 + (unsigned)(*p - '0');
            if (port > 65535) return false;
        }
        if (port == 0) return false;
        parsed->port = (unsigned short)port;
    }
    if (!copy_part(parsed->host, sizeof(parsed->host), authority, (size_t)(host_end - authority)))
        return false;
    for (char *p = parsed->host; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (*end == '\0') {
        strcpy(parsed->path, "/");
    } else if (*end == '?') {
        if (snprintf(parsed->path, sizeof(parsed->path), "/%s", end) >= (int)sizeof(parsed->path))
            return false;
    } else if (*end == '#') {
        strcpy(parsed->path, "/");
    } else {
        const char *fragment = strchr(end, '#');
        size_t n = fragment ? (size_t)(fragment - end) : strlen(end);
        if (!copy_part(parsed->path, sizeof(parsed->path), end, n)) return false;
    }
    return true;
}

static int send_all(int fd, const char *data, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        ssize_t n = send(fd, data + sent, length - sent, 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static char *find_header_end(char *data, size_t length)
{
    if (length < 4) return NULL;
    for (size_t i = 0; i + 3 < length; i++)
        if (data[i] == '\r' && data[i+1] == '\n' && data[i+2] == '\r' && data[i+3] == '\n')
            return data + i;
    return NULL;
}

int iptv_http_get(const char *url, unsigned char *body, size_t capacity,
                  size_t *body_length, int *http_status)
{
    if (!body || capacity == 0 || !body_length || !http_status) return -1;
    *body_length = 0;
    *http_status = 0;
    IptvHttpUrl parsed;
    if (!iptv_http_parse_url(url, &parsed) || parsed.tls) return -2;

    char service[8];
    (void)snprintf(service, sizeof(service), "%u", (unsigned)parsed.port);
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *addresses = NULL;
    if (getaddrinfo(parsed.host, service, &hints, &addresses) != 0 || !addresses)
        return -3;

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) { freeaddrinfo(addresses); return -4; }
    struct timeval timeout = { 10, 0 };
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (connect(fd, addresses->ai_addr, addresses->ai_addrlen) != 0) {
        close(fd); freeaddrinfo(addresses); return -5;
    }
    freeaddrinfo(addresses);

    char request[1600];
    int request_len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: XboxIPTV/0.1\r\nAccept: */*\r\nConnection: close\r\n\r\n",
        parsed.path, parsed.host);
    if (request_len < 0 || (size_t)request_len >= sizeof(request) ||
        send_all(fd, request, (size_t)request_len) != 0) {
        close(fd); return -6;
    }

    size_t total = 0;
    char *buffer = (char *)body;
    char *header_end = NULL;
    while (total < capacity) {
        ssize_t n = recv(fd, buffer + total, capacity - total, 0);
        if (n == 0) break;
        if (n < 0) { close(fd); return -7; }
        total += (size_t)n;
        header_end = find_header_end(buffer, total);
        if (header_end) break;
    }
    if (!header_end) { close(fd); return -8; }

    char *line_end = strstr(buffer, "\r\n");
    if (!line_end) { close(fd); return -9; }
    if (sscanf(buffer, "HTTP/%*u.%*u %d", http_status) != 1) {
        close(fd); return -9;
    }
    size_t header_bytes = (size_t)(header_end + 4 - buffer);
    size_t body_bytes = total - header_bytes;
    memmove(body, buffer + header_bytes, body_bytes);

    while (body_bytes < capacity) {
        ssize_t n = recv(fd, (char *)body + body_bytes, capacity - body_bytes, 0);
        if (n == 0) break;
        if (n < 0) { close(fd); return -10; }
        body_bytes += (size_t)n;
    }
    close(fd);
    if (body_bytes == capacity) return -11;
    *body_length = body_bytes;
    return 0;
}
