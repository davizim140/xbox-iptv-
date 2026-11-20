#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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
    SCREEN_SETTINGS
} Screen;

static IptvChannel playlist_channels[IPTV_MAX_CHANNELS];
static int playlist_count = -1;
static uint32_t playlist_selected;
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

static void load_usb_playlist(void)
{
    FILE *file = fopen("usb:\\playlist.m3u", "rb");
    if (file == NULL) {
        playlist_count = -1;
        playlist_selected = 0;
        xsdk_log_write("xbox-iptv: nao foi possivel abrir usb:\\playlist.m3u");
        return;
    }

    playlist_count = iptv_playlist_read(file, playlist_channels, IPTV_MAX_CHANNELS);
    fclose(file);
    if (playlist_count < 0)
        playlist_count = 0;
    playlist_selected = 0;
    xsdk_log_write("xbox-iptv: %d canais lidos da lista USB", playlist_count);
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
    xsdk_text_draw(64, 42, 3, 0xFFFFFFFF, "XBOX IPTV");
    xsdk_text_draw(66, 132, 2, 0xFF48C7FA,
        screen == SCREEN_LIVE ? "TV AO VIVO" :
        screen == SCREEN_PLAYLISTS ? "MINHAS LISTAS" :
        screen == SCREEN_FAVOURITES ? "FAVORITOS" : "CONFIGURACOES");

    if (screen == SCREEN_LIVE) {
        xsdk_text_draw(66, 220, 2, 0xFFFFFFFF, "CATALOGO PUBLICO");
        xsdk_text_draw(66, 270, 1, 0xFFB5BDCC, "Fonte inicial: iptv-org (lista publica M3U)");
        xsdk_text_draw(66, 306, 1, 0xFF8B95AA, "A conexao e a reproducao serao ativadas nas proximas etapas.");
    } else if (screen == SCREEN_PLAYLISTS) {
        xsdk_text_draw(66, 178, 1, 0xFFB5BDCC, "Arquivo esperado: usb:\\playlist.m3u");
        if (playlist_count < 0) {
            xsdk_text_draw(66, 250, 2, 0xFFFFC66D, "LISTA NAO ENCONTRADA");
            xsdk_text_draw(66, 294, 1, 0xFFB5BDCC, "Copie playlist.m3u para a raiz do pendrive.");
        } else if (playlist_count == 0) {
            xsdk_text_draw(66, 250, 2, 0xFFFFC66D, "NENHUM CANAL VALIDO");
            xsdk_text_draw(66, 294, 1, 0xFFB5BDCC, "Confira se cada #EXTINF tem uma URL na linha seguinte.");
        } else {
            char count_text[64];
            (void)snprintf(count_text, sizeof(count_text), "%d CANAIS CARREGADOS", playlist_count);
            xsdk_text_draw(66, 208, 1, 0xFF48C7FA, count_text);
            const uint32_t first = playlist_selected >= 7 ? playlist_selected - 6 : 0;
            for (uint32_t i = first; i < (uint32_t)playlist_count && i < first + 7; i++) {
                char row[IPTV_CHANNEL_NAME_BYTES + 8];
                (void)snprintf(row, sizeof(row), "%c %s", i == playlist_selected ? '>' : ' ', playlist_channels[i].name);
                xsdk_text_draw(78, (int32_t)(250 + (i - first) * 46),
                    1, i == playlist_selected ? 0xFFFFFFFF : 0xFFB5BDCC, row);
            }
        }
    } else if (screen == SCREEN_FAVOURITES) {
        xsdk_text_draw(66, 220, 2, 0xFFFFFFFF, "NENHUM FAVORITO");
        xsdk_text_draw(66, 270, 1, 0xFF8B95AA, "Seus canais salvos aparecerao nesta tela.");
    } else {
        xsdk_text_draw(66, 220, 2, 0xFFFFFFFF, "STATUS DA REDE");
        xsdk_text_draw(66, 274, 1, 0xFFB5BDCC, "CONEXAO ETHERNET:");
        xsdk_text_draw(350, 274, 1, 0xFF48C7FA, network_link_status);
        xsdk_text_draw(66, 318, 1, 0xFFB5BDCC, "ENDERECO IPv4:");
        xsdk_text_draw(350, 318, 1, 0xFF48C7FA, network_ip_status);
        xsdk_text_draw(66, 390, 1, 0xFF8B95AA, "Este teste verifica o link e o IP atribuido pelo roteador.");
    }

    xsdk_text_draw(66, 632, 1, 0xFF68748B,
        screen == SCREEN_PLAYLISTS ? "D-PAD: NAVEGAR    X: RECARREGAR USB    B: VOLTAR" :
        screen == SCREEN_SETTINGS ? "X: ATUALIZAR REDE    B: VOLTAR" : "B: VOLTAR");
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
                    if (screen == SCREEN_PLAYLISTS)
                        load_usb_playlist();
                    else if (screen == SCREEN_SETTINGS)
                        refresh_network_status();
                }
            } else if ((pressed & INPUT_GAMEPAD_B) != 0) {
                screen = SCREEN_HOME;
            } else if (screen == SCREEN_SETTINGS) {
                if ((pressed & INPUT_GAMEPAD_X) != 0)
                    refresh_network_status();
            } else if (screen == SCREEN_PLAYLISTS) {
                if ((pressed & INPUT_GAMEPAD_DPAD_UP) != 0 && playlist_selected > 0)
                    playlist_selected--;
                if ((pressed & INPUT_GAMEPAD_DPAD_DOWN) != 0 && playlist_count > 0 &&
                    playlist_selected + 1 < (uint32_t)playlist_count)
                    playlist_selected++;
                if ((pressed & INPUT_GAMEPAD_X) != 0)
                    load_usb_playlist();
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
