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

enum { BROWSER_MAX_ENTRIES = 96, BROWSER_NAME_BYTES = 256, BROWSER_PATH_BYTES = 512 };
typedef struct { char name[BROWSER_NAME_BYTES]; bool directory; } BrowserEntry;
static BrowserEntry browser_entries[BROWSER_MAX_ENTRIES];
static uint32_t browser_count;
static uint32_t browser_selected;
static bool browser_truncated;
static char browser_path[BROWSER_PATH_BYTES] = "usb:\\";
static char selected_playlist_path[BROWSER_PATH_BYTES] = "";
static char network_link_status[48] = "NAO VERIFICADO";
static char network_ip_status[48] = "NAO VERIFICADO";

static const char *const menu_items[MENU_ITEMS] = {
    "TV AO VIVO",
    "MINHAS LISTAS M3U",
    "FAVORITOS",
    "CONFIGURACOES"
};

static const char *const menu_descriptions[MENU_ITEMS] = {
    "Acesse o catalogo publico de canais.",
    "Adicione sua propria lista M3U ou M3U8.",
    "Seus canais favoritos ficarao aqui.",
    "Configure rede, listas e preferencias."
};

 
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

static void draw_home(uint32_t width, uint32_t height, uint32_t selected)
{
    (void)height;
    xsdk_text_draw(64, 42, 3, 0xFFFFFFFF, "XBOX IPTV");
    xsdk_text_draw(66, 112, 1, 0xFF8B95AA, "SEU ENTRETENIMENTO EM UM LUGAR");
    xsdk_text_draw(66, 174, 2, 0xFFFFFFFF, "INICIO");

    for (uint32_t i = 0; i < MENU_ITEMS; i++) {
        const uint32_t y = 236 + i * 76;
        const bool active = i == selected;
        const uint32_t color = active ? 0xFFFFFFFF : 0xFF8B95AA;
        if (active)
            xsdk_text_draw(66, (int32_t)y, 2, 0xFF48C7FA, ">");
        xsdk_text_draw(104, (int32_t)y, 2, color, menu_items[i]);
    }

    xsdk_text_draw((int32_t)(width / 2), 240, 2, 0xFFFFFFFF, menu_items[selected]);
    xsdk_text_draw((int32_t)(width / 2), 292, 1, 0xFFB5BDCC, menu_descriptions[selected]);
    xsdk_text_draw(66, 632, 1, 0xFF68748B, "D-PAD: NAVEGAR    A: ABRIR    BACK: SAIR");
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
        xsdk_text_draw(66, 220, 2, 0xFFFFFFFF, "NENHUM FAVORITO");
        xsdk_text_draw(66, 270, 1, 0xFF8B95AA, "Seus canais favoritos aparecerao nesta tela.");
    } else {
        xsdk_text_draw(66, 220, 2, 0xFFFFFFFF, "STATUS DA REDE");
        xsdk_text_draw(66, 274, 1, 0xFFB5BDCC, "CONEXAO ETHERNET:");
        xsdk_text_draw(350, 274, 1, 0xFF48C7FA, network_link_status);
        xsdk_text_draw(66, 318, 1, 0xFFB5BDCC, "ENDERECO IPv4:");
        xsdk_text_draw(350, 318, 1, 0xFF48C7FA, network_ip_status);
        xsdk_text_draw(66, 390, 1, 0xFF8B95AA, "Este teste verifica o link e o IP atribuido pelo roteador.");
    }

    const char *footer =
        screen == SCREEN_PLAYLISTS ? "D-PAD: NAVEGAR    A: ABRIR/SELECIONAR    B: VOLTAR" :
        screen == SCREEN_CHANNELS ? "D-PAD: NAVEGAR    X: RECARREGAR LISTA    B: PASTAS" :
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
                        (void)snprintf(browser_path, sizeof(browser_path), "usb:\\\\");
                        scan_browser_directory();
                    } else if (screen == SCREEN_SETTINGS) {
                        refresh_network_status();
                    }
                }
            } else if ((pressed & INPUT_GAMEPAD_B) != 0) {
                screen = screen == SCREEN_CHANNELS ? SCREEN_PLAYLISTS : SCREEN_HOME;
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
                if ((pressed & INPUT_GAMEPAD_X) != 0)
                    scan_browser_directory();
            } else if (screen == SCREEN_CHANNELS) {
                if ((pressed & INPUT_GAMEPAD_DPAD_UP) != 0 && playlist_selected > 0)
                    playlist_selected--;
                if ((pressed & INPUT_GAMEPAD_DPAD_DOWN) != 0 && playlist_count > 0 &&
                    playlist_selected + 1 < (uint32_t)playlist_count)
                    playlist_selected++;
                if ((pressed & INPUT_GAMEPAD_X) != 0)
                    load_playlist_file(selected_playlist_path);
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
