#include "m3u.h"

#include <stddef.h>
#include <string.h>

static void copy_range(char *destination, size_t capacity, const char *begin, const char *end)
{
    if (capacity == 0)
        return;
    size_t length = (size_t)(end - begin);
    while (length > 0 && (*begin == ' ' || *begin == '\t')) {
        begin++;
        length--;
    }
    while (length > 0 && (begin[length - 1] == ' ' || begin[length - 1] == '\t' ||
                          begin[length - 1] == '\r' || begin[length - 1] == '\n'))
        length--;
    if (length >= capacity)
        length = capacity - 1;
    memcpy(destination, begin, length);
    destination[length] = '\0';
}

static void read_attribute(const char *line, const char *attribute, char *destination, size_t capacity)
{
    destination[0] = '\0';
    const char *value = strstr(line, attribute);
    if (value == NULL)
        return;
    value += strlen(attribute);
    const char *end = strchr(value, '\"');
    if (end != NULL)
        copy_range(destination, capacity, value, end);
}

bool iptv_parse_extinf(const char *line, IptvChannel *channel)
{
    if (line == NULL || channel == NULL || strncmp(line, "#EXTINF:", 8) != 0)
        return false;

    bool quoted = false;
    const char *separator = NULL;
    for (const char *cursor = line + 8; *cursor != '\0'; cursor++) {
        if (*cursor == '\"')
            quoted = !quoted;
        else if (*cursor == ',' && !quoted) {
            separator = cursor;
            break;
        }
    }
    if (separator == NULL)
        return false;

    memset(channel, 0, sizeof(*channel));
    copy_range(channel->name, sizeof(channel->name), separator + 1, line + strlen(line));
    if (channel->name[0] == '\0')
        return false;

    read_attribute(line, "tvg-id=\"", channel->id, sizeof(channel->id));
    read_attribute(line, "tvg-logo=\"", channel->logo, sizeof(channel->logo));
    read_attribute(line, "group-title=\"", channel->group, sizeof(channel->group));
    return true;
}
