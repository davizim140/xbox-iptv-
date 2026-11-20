#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <strings.h>

#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/net.h"
#include "xsdk/present.h"
#include "xsdk/splash.h"
#include "xsdk/text.h"
#include "m3u.h"

enum {
    DISPLAY_BUFFERS = 2,
    SWAP_INTERVAL = 1,
    BEGIN_TIMEOUT_MS = 5000,
    SPLASH_MS = 1200,
    CLEAR_ARGB = 0xFF10131D,
    MENU_ITEMS = 4
};

typedef enum {
    SCREEN_HOME,
    SCREEN_LIVE,
    SCREEN_PLAYLISTS,
    SCREEN_FAVOURITES,
    SCREEN_SETTINGS,
    SCREEN_CHANNELS
} Screen;

static IptvChannel playlist_channels[IPTV_MAX_CHANNELS];
static int playlist_count = -1;
static uint32_t playlist_selected;
static IptvChannel favourite_channels[IPTV_MAX_CHANNELS];
static uint32_t favourite_count;
static uint32_t favourite_selected;

static void save_favourites(void);

static int find_favourite(const IptvChannel *channel)
{
    for (uint32_t i = 0; i < favourite_count; i++) {
        if (strcmp(favourite_channels[i].url, channel->url) == 0)
            return (int)i;
    }
    return -1;
}

static void toggle_favourite(const IptvChannel *channel)
{
    const int existing = find_favourite(channel);
    if (existing >= 0) {
        for (uint32_t i = (uint32_t)existing; i + 1 < favourite_count; i++)
            favourite_channels[i] = favourite_channels[i + 1];
        favourite_count--;
        if (favourite_count == 0)
            favourite_selected = 0;
        else if (favourite_selected >= favourite_count)
            favourite_selected = favourite_count - 1;
        save_favourites();
        xsdk_log_write("xbox-iptv: favorito removido: %s", channel->name);
        return;
    }

    if (favourite_count >= IPTV_MAX_CHANNELS) {
        xsdk_log_write("xbox-iptv: limite de favoritos atingido");
        return;
    }
    favourite_channels[favourite_count++] = *channel;
    save_favourites();
    xsdk_log_write("xbox-iptv: favorito adicionado: %s", channel->name);
}

enum { BROWSER_MAX_ENTRIES = 96, BROWSER_NAME_BYTES = 256, BROWSER_PATH_BYTES = 512 };
typedef struct { char name[BROWSER_NAME_BYTES]; char path[BROWSER_PATH_BYTES]; bool directory; } BrowserEntry;
static BrowserEntry browser_entries[BROWSER_MAX_ENTRIES];
static uint32_t browser_count;
static uint32_t browser_selected;
static bool browser_truncated;
static char browser_path[BROWSER_PATH_BYTES] = "usb:\\";
static bool browser_selecting_drive = true;
static char selected_playlist_path[BROWSER_PATH_BYTES] = "";
static char favourites_storage_root[BROWSER_PATH_BYTES] = "";

static void hex_encode(const char *source, char *destination, size_t capacity)
{
    static const char digits[] = "0123456789ABCDEF";
    size_t out = 0;
    if (capacity == 0)
        return;
    for (size_t i = 0; source[i] != '\0' && out + 2 < capacity; i++) {
        const unsigned char value = (unsigned char)source[i];
        destination[out++] = digits[value >> 4];
        destination[out++] = digits[value & 15];
    }
    destination[out] = '\0';
}

static bool hex_decode(const char *source, char *destination, size_t capacity)
{
    size_t out = 0;
    if (capacity == 0)
        return false;
    while (source[0] != '\0' && source[1] != '\0') {
        unsigned int value = 0;
        if (sscanf(source, "%2x", &value) != 1 || out + 1 >= capacity)
            return false;
        destination[out++] = (char)value;
        source += 2;
    }
    if (*source != '\0')
        return false;
    destination[out] = '\0';
    return true;
}

static void save_favourites(void)
{
    if (favourites_storage_root[0] == '\0')
        return;
    char path[BROWSER_PATH_BYTES];
    const int length = snprintf(path, sizeof(path), "%sxbox-iptv-fav.dat", favourites_storage_root);
    if (length < 0 || (size_t)length >= sizeof(path))
        return;

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        xsdk_log_write("xbox-iptv: nao foi possivel salvar favoritos em %s", path);
        return;
    }
    char url[IPTV_CHANNEL_URL_BYTES * 2 + 1];
    char name[IPTV_CHANNEL_NAME_BYTES * 2 + 1];
    char group[IPTV_CHANNEL_GROUP_BYTES * 2 + 1];
    char logo[IPTV_CHANNEL_LOGO_BYTES * 2 + 1];
    char id[IPTV_CHANNEL_ID_BYTES * 2 + 1];
    bool ok = true;
    for (uint32_t i = 0; i < favourite_count; i++) {
        const IptvChannel *channel = &favourite_channels[i];
        hex_encode(channel->url, url, sizeof(url));
        hex_encode(channel->name, name, sizeof(name));
        hex_encode(channel->group, group, sizeof(group));
        hex_encode(channel->logo, logo, sizeof(logo));
        hex_encode(channel->id, id, sizeof(id));
        if (fprintf(file, "%s|%s|%s|%s|%s\n", url, name, group, logo, id) < 0) {
            ok = false;
            break;
        }
    }
    if (fclose(file) != 0)
        ok = false;
    if (!ok)
        xsdk_log_write("xbox-iptv: erro ao gravar favoritos");
}

static void load_favourites(void)
{
    if (favourites_storage_root[0] == '\0')
        return;
    char path[BROWSER_PATH_BYTES];
    const int length = snprintf(path, sizeof(path), "%sxbox-iptv-fav.dat", favourites_storage_root);
    if (length < 0 || (size_t)length >= sizeof(path))
        return;
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return;

    favourite_count = 0;
    favourite_selected = 0;
    char line[2112];
    while (favourite_count < IPTV_MAX_CHANNELS && fgets(line, sizeof(line), file) != NULL) {
        char *fields[5];
        fields[0] = line;
        for (size_t i = 1; i < 5; i++) {
            char *separator = strchr(fields[i - 1], '|');
            if (separator == NULL) {
                fields[0] = NULL;
                break;
            }
            *separator = '\0';
            fields[i] = separator + 1;
        }
        if (fields[0] == NULL)
            continue;
        char *newline = strpbrk(fields[4], "\r\n");
        if (newline != NULL)
            *newline = '\0';
        IptvChannel channel;
        memset(&channel, 0, sizeof(channel));
        if (hex_decode(fields[0], channel.url, sizeof(channel.url)) &&
            hex_decode(fields[1], channel.name, sizeof(channel.name)) &&
            hex_decode(fields[2], channel.group, sizeof(channel.group)) &&
            hex_decode(fields[3], channel.logo, sizeof(channel.logo)) &&
            hex_decode(fields[4], channel.id, sizeof(channel.id)) &&
            channel.url[0] != '\0' && channel.name[0] != '\0' &&
            find_favourite(&channel) < 0)
            favourite_channels[favourite_count++] = channel;
    }
    (void)fclose(file);
    xsdk_log_write("xbox-iptv: %u favoritos carregados do USB",
        (unsigned)favourite_count);
}

typedef struct { const char *label; const char *path; } StorageRoot;
static const StorageRoot storage_roots[] = {
    { "USB (PADRAO)", "usb:\\" },
    { "USB 0", "usb0:\\" },
    { "USB 1", "usb1:\\" },
    { "USB 2", "usb2:\\" },
};
static char network_link_status[48] = "NAO VERIFICADO";
static char network_ip_status[48] = "NAO VERIFICADO";


static void refresh_network_status(void)
{
    uint32_t link = 0;
    const int link_error = xsdk_net_link(&link);
    if (link_error != 0) {
        (void)snprintf(network_link_status, sizeof(network_link_status), "ERRO DE REDE (%d)", link_error);
    } else if ((link & XSDK_NET_LINK_ACTIVE) != 0) {
        (void)snprintf(network_link_status, sizeof(network_link_status), "CABO CONECTADO");
    } else {
        (void)snprintf(network_link_status, sizeof(network_link_status), "SEM LINK ETHERNET");
    }

    struct in_addr address = { 0 };
    const int address_error = xsdk_net_address(&address);
    if (address_error == 0) {
        const unsigned char *octets = (const unsigned char *)&address.s_addr;
        (void)snprintf(network_ip_status, sizeof(network_ip_status), "%u.%u.%u.%u",
            (unsigned)octets[0], (unsigned)octets[1],
            (unsigned)octets[2], (unsigned)octets[3]);
    } else {
        (void)snprintf(network_ip_status, sizeof(network_ip_status), "IP INDISPONIVEL (%d)", address_error);
    }
    xsdk_log_write("xbox-iptv: rede: %s; IP: %s", network_link_status, network_ip_status);
}

static bool is_drive_root(const char *path)
{
    const char *colon = strchr(path, ':');
    return colon != NULL && colon[1] == '\\' && colon[2] == '\0';
}

static bool is_m3u_file(const char *name)
{
    const char *extension = strrchr(name, '.');
    return extension != NULL &&
        (strcasecmp(extension, ".m3u") == 0 || strcasecmp(extension, ".m3u8") == 0);
}

static void scan_browser_drives(void)
{
    browser_count = 0;
    browser_selected = 0;
    browser_truncated = false;

    for (size_t i = 0; i < sizeof(storage_roots) / sizeof(storage_roots[0]); i++) {
        DIR *directory = opendir(storage_roots[i].path);
        if (directory == NULL)
            continue;
        (void)closedir(directory);
        if (browser_count >= BROWSER_MAX_ENTRIES)
            break;
        (void)snprintf(browser_entries[browser_count].name,
            sizeof(browser_entries[browser_count].name), "%s", storage_roots[i].label);
        (void)snprintf(browser_entries[browser_count].path,
            sizeof(browser_entries[browser_count].path), "%s", storage_roots[i].path);
        browser_entries[browser_count].directory = true;
        browser_count++;
    }
    xsdk_log_write("xbox-iptv: unidades de armazenamento encontradas: %u",
        (unsigned)browser_count);
}

static void scan_browser_directory(void)
{
    browser_count = 0;
    browser_selected = 0;
    browser_truncated = false;

    if (!is_drive_root(browser_path) && browser_count < BROWSER_MAX_ENTRIES) {
        (void)snprintf(browser_entries[browser_count].name,
            sizeof(browser_entries[browser_count].name), "..");
        browser_entries[browser_count++].directory = true;
    }

    DIR *directory = opendir(browser_path);
    if (directory == NULL) {
        xsdk_log_write("xbox-iptv: nao foi possivel abrir pasta: %s", browser_path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        const bool is_directory = entry->d_type == DT_DIR;
        if (!is_directory && !is_m3u_file(entry->d_name))
            continue;

        if (browser_count >= BROWSER_MAX_ENTRIES) {
            browser_truncated = true;
            break;
        }

        (void)snprintf(browser_entries[browser_count].name,
            sizeof(browser_entries[browser_count].name), "%s", entry->d_name);
        browser_entries[browser_count].path[0] = '\0';
        browser_entries[browser_count].directory = is_directory;
        browser_count++;
    }

    (void)closedir(directory);
    xsdk_log_write("xbox-iptv: pasta %s: %u itens M3U/pastas",
        browser_path, (unsigned)browser_count);
}

static bool enter_browser_directory(const char *name)
{
    if (strcmp(name, "..") == 0) {
        if (is_drive_root(browser_path))
            return false;
        size_t length = strlen(browser_path);
        if (length > 0 && browser_path[length - 1] == '\\')
            length--;
        while (length > 0 && browser_path[length - 1] != '\\')
            length--;
        if (length == 0)
            return false;
        browser_path[length] = '\0';
    } else {
        const size_t length = strlen(browser_path);
        const int written = snprintf(browser_path + length,
            sizeof(browser_path) - length, "%s\\", name);
        if (written < 0 || (size_t)written >= sizeof(browser_path) - length)
            return false;
    }
    scan_browser_directory();
    return true;
}

static void load_playlist_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        playlist_count = -1;
        playlist_selected = 0;
        selected_playlist_path[0] = '\0';
        xsdk_log_write("xbox-iptv: falha ao abrir lista: %s", path);
        return;
    }

    playlist_count = iptv_playlist_read(file, playlist_channels, IPTV_MAX_CHANNELS);
    (void)fclose(file);
    if (playlist_count < 0)
        playlist_count = 0;
    playlist_selected = 0;
    (void)snprintf(selected_playlist_path, sizeof(selected_playlist_path), "%s", path);
    xsdk_log_write("xbox-iptv: %d canais lidos de %s", playlist_count, path);
}

static void select_browser_entry(void)
{
    if (browser_selected >= browser_count)
        return;

    const BrowserEntry *entry = &browser_entries[browser_selected];
    if (browser_selecting_drive) {
        (void)snprintf(browser_path, sizeof(browser_path), "%s", entry->path);
        browser_selecting_drive = false;
        (void)snprintf(favourites_storage_root, sizeof(favourites_storage_root), "%s", entry->path);
        load_favourites();
        scan_browser_directory();
        return;
    }
    if (entry->directory) {
        (void)enter_browser_directory(entry->name);
        return;
    }

    char path[BROWSER_PATH_BYTES];
    const int written = snprintf(path, sizeof(path), "%s%s", browser_path, entry->name);
    if (written < 0 || (size_t)written >= sizeof(path))
        return;

    load_playlist_file(path);
    xsdk_log_write("xbox-iptv: lista selecionada: %s", path);
}

static void draw_home_card(int32_t x, int32_t y, const char *title, const char *subtitle, bool active)
{
    const uint32_t color = active ? 0xFFE50914 : 0xFF465064;
    const uint32_t text_color = active ? 0xFFFFFFFF : 0xFFB5BDCC;
    xsdk_text_draw(x, y, 1, color, "+------------------------+");
    xsdk_text_draw(x, y + 24, 1, color, "|                        |");
    xsdk_text_draw(x + 10, y + 24, 1, text_color, title);
    xsdk_text_draw(x, y + 48, 1, color, "|                        |");
    xsdk_text_draw(x + 10, y + 48, 1, 0xFF8B95AA, subtitle);
    xsdk_text_draw(x, y + 72, 1, color, "+------------------------+");
}

static void draw_home(uint32_t width, uint32_t height, uint32_t selected)
{
    (void)height;
    xsdk_text_draw(66, 34, 3, 0xFFE50914, "XBOX IPTV");
    xsdk_text_draw(760, 48, 1, 0xFFB5BDCC, "INICIO   MINHAS LISTAS   FAVORITOS   AJUSTES");
    xsdk_text_draw(66, 132, 1, 0xFF8B95AA, "SUA CENTRAL DE ENTRETENIMENTO");
    xsdk_text_draw(66, 184, 4, 0xFFFFFFFF, "TV DO SEU JEITO.");
    xsdk_text_draw(66, 274, 2, 0xFFB5BDCC, "Suas listas M3U e canais organizados em um so lugar.");
    xsdk_text_draw(66, 330, 1, 0xFFE50914, "DESTAQUES");
    xsdk_text_draw(66, 374, 1, 0xFF8B95AA, "ESCOLHA UMA SECAO PARA CONTINUAR");

    draw_home_card(66, 430, "TV AO VIVO", "CATALOGO", selected == 0);
    draw_home_card(356, 430, "MINHAS LISTAS", "M3U / M3U8", selected == 1);
    draw_home_card(646, 430, "FAVORITOS", "SEUS CANAIS", selected == 2);
    draw_home_card(936, 430, "CONFIGURACOES", "REDE E SISTEMA", selected == 3);

    xsdk_text_draw(66, 632, 1, 0xFF68748B, "D-PAD: NAVEGAR    A: ABRIR    BACK: SAIR");
    (void)width;
}

static void draw_section(Screen screen, uint32_t width)
{
    const char *heading =
        screen == SCREEN_LIVE ? "TV AO VIVO" :
        screen == SCREEN_PLAYLISTS ? "ESCOLHER ARQUIVO M3U" :
        screen == SCREEN_CHANNELS ? "CANAIS DA LISTA" :
        screen == SCREEN_FAVOURITES ? "FAVORITOS" : "CONFIGURACOES";
    xsdk_text_draw(64, 42, 3, 0xFFFFFFFF, "XBOX IPTV");
    xsdk_text_draw(66, 132, 2, 0xFF48C7FA, heading);

    if (screen == SCREEN_LIVE) {
        xsdk_text_draw(66, 220, 2, 0xFFFFFFFF, "CATALOGO PUBLICO");
        xsdk_text_draw(66, 270, 1, 0xFFB5BDCC, "Fonte inicial: iptv-org (lista publica M3U)");
        xsdk_text_draw(66, 306, 1, 0xFF8B95AA, "A conexao e a reproducao serao ativadas nas proximas etapas.");
    } else if (screen == SCREEN_PLAYLISTS) {
        if (browser_selecting_drive) {
            xsdk_text_draw(66, 178, 1, 0xFFB5BDCC, "ESCOLHA O DISPOSITIVO:");
            if (browser_count == 0) {
                xsdk_text_draw(66, 250, 2, 0xFFFFC66D, "NENHUMA UNIDADE DISPONIVEL");
                xsdk_text_draw(66, 294, 1, 0xFFB5BDCC, "Conecte um dispositivo e pressione X para atualizar.");
            } else {
                for (uint32_t i = 0; i < browser_count && i < 8; i++) {
                    char row[BROWSER_NAME_BYTES + 24];
                    (void)snprintf(row, sizeof(row), "%c %.240s",
                        i == browser_selected ? '>' : ' ', browser_entries[i].name);
                    xsdk_text_draw(78, (int32_t)(224 + i * 44), 1,
                        i == browser_selected ? 0xFFFFFFFF : 0xFFB5BDCC, row);
                }
            }
        } else {
        xsdk_text_draw(66, 178, 1, 0xFFB5BDCC, "PASTA:");
        xsdk_text_draw(180, 178, 1, 0xFF48C7FA, browser_path);
        if (browser_count == 0) {
            xsdk_text_draw(66, 250, 2, 0xFFFFC66D, "NENHUMA PASTA OU LISTA M3U");
            xsdk_text_draw(66, 294, 1, 0xFFB5BDCC, "Use A para abrir pastas ou volte para outra pasta.");
        } else {
            const uint32_t first = browser_selected >= 8 ? browser_selected - 7 : 0;
            for (uint32_t i = first; i < browser_count && i < first + 8; i++) {
                char row[BROWSER_NAME_BYTES + 16];
                const char *kind = browser_entries[i].directory ? "[PASTA]" : "[M3U]";
                (void)snprintf(row, sizeof(row), "%c %s %s",
                    i == browser_selected ? '>' : ' ', kind, browser_entries[i].name);
                xsdk_text_draw(78, (int32_t)(224 + (i - first) * 44),
                    1, i == browser_selected ? 0xFFFFFFFF : 0xFFB5BDCC, row);
            }
        }
        if (browser_truncated)
            xsdk_text_draw(66, 584, 1, 0xFFFFC66D, "LISTA LIMITADA A 96 ITENS NESTA PASTA");
        }
    } else if (screen == SCREEN_CHANNELS) {
        xsdk_text_draw(66, 178, 1, 0xFF48C7FA, selected_playlist_path);
        if (playlist_count <= 0) {
            xsdk_text_draw(66, 250, 2, 0xFFFFC66D, "NENHUM CANAL VALIDO");
            xsdk_text_draw(66, 294, 1, 0xFFB5BDCC, "Verifique o formato M3U/M3U8 e as URLs.");
        } else {
            char count_text[64];
            (void)snprintf(count_text, sizeof(count_text), "%d CANAIS CARREGADOS", playlist_count);
            xsdk_text_draw(66, 208, 1, 0xFF48C7FA, count_text);
            const uint32_t first = playlist_selected >= 7 ? playlist_selected - 6 : 0;
            for (uint32_t i = first; i < (uint32_t)playlist_count && i < first + 7; i++) {
                char row[IPTV_CHANNEL_NAME_BYTES + 8];
                (void)snprintf(row, sizeof(row), "%c %s",
                    i == playlist_selected ? '>' : ' ', playlist_channels[i].name);
                xsdk_text_draw(78, (int32_t)(250 + (i - first) * 46),
                    1, i == playlist_selected ? 0xFFFFFFFF : 0xFFB5BDCC, row);
            }
        }
    } else if (screen == SCREEN_FAVOURITES) {
        if (favourite_count == 0) {
            xsdk_text_draw(66, 220, 2, 0xFFFFFFFF, "NENHUM FAVORITO");
            xsdk_text_draw(66, 270, 1, 0xFF8B95AA, "Na lista de canais, pressione Y para favoritar.");
        } else {
            char count_text[64];
            (void)snprintf(count_text, sizeof(count_text), "%u CANAIS FAVORITOS", (unsigned)favourite_count);
            xsdk_text_draw(66, 178, 1, 0xFF48C7FA, count_text);
            const uint32_t first = favourite_selected >= 7 ? favourite_selected - 6 : 0;
            for (uint32_t i = first; i < favourite_count && i < first + 7; i++) {
                char row[IPTV_CHANNEL_NAME_BYTES + 8];
                (void)snprintf(row, sizeof(row), "%c %s",
                    i == favourite_selected ? '>' : ' ', favourite_channels[i].name);
                xsdk_text_draw(78, (int32_t)(230 + (i - first) * 46),
                    1, i == favourite_selected ? 0xFFFFFFFF : 0xFFB5BDCC, row);
            }
            xsdk_text_draw(66, 584, 1, 0xFF8B95AA, "Y: REMOVER O FAVORITO SELECIONADO");
        }
    } else {
        xsdk_text_draw(66, 220, 2, 0xFFFFFFFF, "STATUS DA REDE");
        xsdk_text_draw(66, 274, 1, 0xFFB5BDCC, "CONEXAO ETHERNET:");
        xsdk_text_draw(350, 274, 1, 0xFF48C7FA, network_link_status);
        xsdk_text_draw(66, 318, 1, 0xFFB5BDCC, "ENDERECO IPv4:");
        xsdk_text_draw(350, 318, 1, 0xFF48C7FA, network_ip_status);
        xsdk_text_draw(66, 390, 1, 0xFF8B95AA, "Este teste verifica o link e o IP atribuido pelo roteador.");
    }

    const char *footer =
        screen == SCREEN_PLAYLISTS ? (browser_selecting_drive ? "D-PAD: ESCOLHER UNIDADE    A: ABRIR    X: ATUALIZAR    B: VOLTAR" : "D-PAD: NAVEGAR    A: ABRIR/SELECIONAR    B: UNIDADES") :
        screen == SCREEN_CHANNELS ? "D-PAD: NAVEGAR    Y: FAVORITAR    X: RECARREGAR    B: PASTAS" :
        screen == SCREEN_FAVOURITES ? "D-PAD: NAVEGAR    Y: REMOVER    B: VOLTAR" :
        screen == SCREEN_SETTINGS ? "X: ATUALIZAR REDE    B: VOLTAR" : "B: VOLTAR";
    xsdk_text_draw(66, 632, 1, 0xFF68748B, footer);
    (void)width;
}

static void draw_frame(uint32_t width, uint32_t height, uint32_t selected, Screen screen)
{
    xsdk_gpu_set_target(width, height);
    if (screen == SCREEN_HOME)
        draw_home(width, height, selected);
    else
        draw_section(screen, width);
}

static void run(uint32_t width, uint32_t height)
{
    Screen screen = SCREEN_HOME;
    uint32_t selected = 0;
    uint16_t previous_buttons = 0;

    for (;;) {
        INPUT_STATE input = { 0 };
        if (XamInputGetState(0, 0, &input) == ERROR_SUCCESS) {
            const uint16_t buttons = input.gamepad.buttons;
            const uint16_t pressed = (uint16_t)(buttons & (uint16_t)~previous_buttons);
            previous_buttons = buttons;

            if ((pressed & INPUT_GAMEPAD_BACK) != 0)
                return;
            if (screen == SCREEN_HOME) {
                if ((pressed & INPUT_GAMEPAD_DPAD_UP) != 0)
                    selected = selected == 0 ? MENU_ITEMS - 1 : selected - 1;
                if ((pressed & INPUT_GAMEPAD_DPAD_DOWN) != 0)
                    selected = (selected + 1) % MENU_ITEMS;
                if ((pressed & INPUT_GAMEPAD_A) != 0) {
                    screen = (Screen)(selected + 1);
                    if (screen == SCREEN_PLAYLISTS) {
                        browser_selecting_drive = true;
                        scan_browser_drives();
                    } else if (screen == SCREEN_SETTINGS) {
                        refresh_network_status();
                    }
                }
            } else if ((pressed & INPUT_GAMEPAD_B) != 0) {
                if (screen == SCREEN_CHANNELS) {
                    screen = SCREEN_PLAYLISTS;
                } else if (screen == SCREEN_PLAYLISTS && !browser_selecting_drive && !is_drive_root(browser_path)) {
                    (void)enter_browser_directory("..");
                } else if (screen == SCREEN_PLAYLISTS && !browser_selecting_drive) {
                    browser_selecting_drive = true;
                    scan_browser_drives();
                } else {
                    screen = SCREEN_HOME;
                }
            } else if (screen == SCREEN_SETTINGS) {
                if ((pressed & INPUT_GAMEPAD_X) != 0)
                    refresh_network_status();
            } else if (screen == SCREEN_PLAYLISTS) {
                if ((pressed & INPUT_GAMEPAD_DPAD_UP) != 0 && browser_selected > 0)
                    browser_selected--;
                if ((pressed & INPUT_GAMEPAD_DPAD_DOWN) != 0 &&
                    browser_selected + 1 < browser_count)
                    browser_selected++;
                if ((pressed & INPUT_GAMEPAD_A) != 0) {
                    if (browser_selected < browser_count && !browser_entries[browser_selected].directory) {
                        select_browser_entry();
                        screen = SCREEN_CHANNELS;
                    } else {
                        select_browser_entry();
                    }
                }
                if ((pressed & INPUT_GAMEPAD_X) != 0) {
                    if (browser_selecting_drive)
                        scan_browser_drives();
                    else
                        scan_browser_directory();
                }
            } else if (screen == SCREEN_FAVOURITES) {
                if ((pressed & INPUT_GAMEPAD_DPAD_UP) != 0 && favourite_selected > 0)
                    favourite_selected--;
                if ((pressed & INPUT_GAMEPAD_DPAD_DOWN) != 0 &&
                    favourite_selected + 1 < favourite_count)
                    favourite_selected++;
                if ((pressed & INPUT_GAMEPAD_Y) != 0 && favourite_count > 0) {
                    const IptvChannel selected_favourite = favourite_channels[favourite_selected];
                    toggle_favourite(&selected_favourite);
                }
            } else if (screen == SCREEN_CHANNELS) {
                if ((pressed & INPUT_GAMEPAD_DPAD_UP) != 0 && playlist_selected > 0)
                    playlist_selected--;
                if ((pressed & INPUT_GAMEPAD_DPAD_DOWN) != 0 && playlist_count > 0 &&
                    playlist_selected + 1 < (uint32_t)playlist_count)
                    playlist_selected++;
                if ((pressed & INPUT_GAMEPAD_X) != 0)
                    load_playlist_file(selected_playlist_path);
                if ((pressed & INPUT_GAMEPAD_Y) != 0 && playlist_count > 0)
                    toggle_favourite(&playlist_channels[playlist_selected]);
            }
        }

        uint32_t address = 0;
        const NTSTATUS status = xsdk_present_begin(&address, BEGIN_TIMEOUT_MS);
        if (!NT_SUCCESS(status)) {
            xsdk_log_write("xbox-iptv: xsdk_present_begin failed: 0x%08X", (unsigned)status);
            return;
        }
        draw_frame(width, height, selected, screen);
        xsdk_gpu_resolve(address, width, height, CLEAR_ARGB);
        xsdk_present_end();
    }
}

int main(void)
{
    xsdk_log_write("xbox-iptv: starting interface shell");
    const NTSTATUS splash = xsdk_splash_show(SPLASH_MS);
    if (!NT_SUCCESS(splash))
        xsdk_log_write("xbox-iptv: splash unavailable: 0x%08X", (unsigned)splash);

    NTSTATUS status = xsdk_gpu_open();
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("xbox-iptv: xsdk_gpu_open failed: 0x%08X", (unsigned)status);
        return 1;
    }
    status = xsdk_text_open();
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("xbox-iptv: xsdk_text_open failed: 0x%08X", (unsigned)status);
        (void)xsdk_gpu_close();
        return 1;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    status = xsdk_present_open(XSDK_DISPLAY_720P, DISPLAY_BUFFERS, SWAP_INTERVAL, &width, &height);
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("xbox-iptv: xsdk_present_open failed: 0x%08X", (unsigned)status);
        xsdk_text_close();
        (void)xsdk_gpu_close();
        return 1;
    }

    xsdk_log_write("xbox-iptv: display %ux%u", (unsigned)width, (unsigned)height);
    run(width, height);
    xsdk_present_close();
    xsdk_text_close();
    status = xsdk_gpu_close();
    if (!NT_SUCCESS(status))
        xsdk_log_write("xbox-iptv: xsdk_gpu_close failed: 0x%08X", (unsigned)status);
    xsdk_log_write("xbox-iptv: clean exit");
    return 0;
}
