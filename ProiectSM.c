#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "hardware/i2c.h"
#include "hardware/adc.h"

#include "lwip/err.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

// =====================================================
// ================= WIFI CONFIG =======================
// =====================================================

#define WIFI_SSID "aln"
#define WIFI_PASS "cocacola"

// =====================================================
// ================= OLED CONFIG =======================
// =====================================================

#define I2C_PORT i2c0
#define I2C_SDA 16
#define I2C_SCL 17
#define OLED_ADDR 0x3C

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_BUF_SIZE (OLED_WIDTH * OLED_HEIGHT / 8)

// =====================================================
// ================= HARDWARE PINS =====================
// =====================================================

#define BUTTON_MENU 14
#define BUTTON_WATER 15
#define PUMP_PIN 18

#define SOIL_ADC_PIN 26
#define SOIL_ADC_INPUT 0

// =====================================================
// ================= GLOBAL VARIABLES ==================
// =====================================================

uint8_t buffer[OLED_BUF_SIZE];

bool pump_on = false;
bool web_watering = false;

absolute_time_t web_watering_stop;

char html[2048];
char last_water_time[64] = "NU A FOST UDATA";

// =====================================================
// ================= OLED LOW LEVEL ====================
// =====================================================

void oled_cmd(uint8_t cmd)
{
    uint8_t data[2] = {0x00, cmd};
    i2c_write_blocking(I2C_PORT, OLED_ADDR, data, 2, false);
}

void oled_data(uint8_t *data, size_t len)
{
    uint8_t temp[OLED_BUF_SIZE + 1];
    temp[0] = 0x40;
    memcpy(&temp[1], data, len);
    i2c_write_blocking(I2C_PORT, OLED_ADDR, temp, len + 1, false);
}

void oled_init()
{
    sleep_ms(100);

    uint8_t cmds[] = {
        0xAE,
        0x20, 0x00,
        0xB0,
        0xC8,
        0x00,
        0x10,
        0x40,
        0x81, 0x7F,
        0xA1,
        0xA6,
        0xA8, 0x3F,
        0xA4,
        0xD3, 0x00,
        0xD5, 0x80,
        0xD9, 0xF1,
        0xDA, 0x12,
        0xDB, 0x40,
        0x8D, 0x14,
        0xAF};

    for (int i = 0; i < sizeof(cmds); i++)
    {
        oled_cmd(cmds[i]);
    }
}

void oled_clear()
{
    memset(buffer, 0, OLED_BUF_SIZE);
}

void oled_show()
{
    oled_cmd(0x21);
    oled_cmd(0);
    oled_cmd(127);

    oled_cmd(0x22);
    oled_cmd(0);
    oled_cmd(7);

    oled_data(buffer, OLED_BUF_SIZE);
}

// =====================================================
// ================= OLED DRAW =========================
// =====================================================

void draw_pixel(int x, int y)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT)
        return;

    buffer[x + (y / 8) * OLED_WIDTH] |= (1 << (y % 8));
}

void draw_rect(int x, int y, int w, int h)
{
    for (int i = x; i < x + w; i++)
    {
        draw_pixel(i, y);
        draw_pixel(i, y + h - 1);
    }

    for (int j = y; j < y + h; j++)
    {
        draw_pixel(x, j);
        draw_pixel(x + w - 1, j);
    }
}

// =====================================================
// ================= FONT ==============================
// =====================================================

const uint8_t font5x7[][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x03, 0x04, 0x78, 0x04, 0x03}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z

    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9

    {0x00, 0x00, 0x00, 0x00, 0x00}, // SPACE
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x08, 0x08, 0x08, 0x08, 0x08}  // -
};

int get_char_index(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= '0' && c <= '9')
        return 26 + (c - '0');
    if (c == '.')
        return 37;
    if (c == '-')
        return 38;
    return 36;
}

void draw_char(int x, int y, char c)
{
    int index = get_char_index(c);

    for (int col = 0; col < 5; col++)
    {
        uint8_t line = font5x7[index][col];

        for (int row = 0; row < 7; row++)
        {
            if (line & (1 << row))
            {
                draw_pixel(x + col, y + row);
            }
        }
    }
}

void draw_text(int x, int y, const char *text)
{
    while (*text)
    {
        draw_char(x, y, *text);
        x += 6;
        text++;
    }
}

void draw_text_center(int y, const char *text)
{
    int len = strlen(text);
    int x = (OLED_WIDTH - len * 6) / 2;

    if (x < 0)
        x = 0;

    draw_text(x, y, text);
}

// =====================================================
// ================= PUMP ==============================
// =====================================================

void pump_set(bool state)
{
    pump_on = state;

    gpio_put(PUMP_PIN, state ? 1 : 0);

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, state ? 1 : 0);
}

// =====================================================
// ================= HTML PAGE =========================
// =====================================================

void build_html()
{
    sprintf(html,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Connection: close\r\n"
            "\r\n"

            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "<meta charset='UTF-8'>"
            "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
            "<meta http-equiv='refresh' content='3'>"

            "<style>"
            "body{background:#111;color:white;font-family:Arial;text-align:center;padding:30px;}"
            ".card{background:#222;margin:auto;padding:25px;border-radius:20px;max-width:390px;}"
            "h1{font-size:28px;margin-bottom:25px;}"
            "h2{font-size:22px;}"
            "button{width:230px;height:75px;font-size:25px;border:none;border-radius:18px;background:#00aa88;color:white;}"
            "p{font-size:18px;color:#ccc;line-height:1.4;}"
            ".status{margin-top:25px;color:#aaa;}"
            "</style>"

            "<script>"
            "function udaAcum(){"
            "let d=new Date();"
            "let t=d.toLocaleString('ro-RO');"
            "window.location.href='/water?time='+encodeURIComponent(t);"
            "}"
            "</script>"

            "</head>"
            "<body>"
            "<div class='card'>"

            "<h1>SMART IRRIGATION</h1>"

            "<h2>Ultima udare:</h2>"
            "<h2>%s</h2>"

            "<br>"

            "<button onclick='udaAcum()'>UDA ACUM</button>"

            "<p>Daca apesi butonul, pompa uda timp de 5 secunde.</p>"

            "<h2 class='status'>Pompa: %s</h2>"

            "</div>"
            "</body>"
            "</html>",

            last_water_time,
            pump_on ? "PORNITA" : "OPRITA");
}

// =====================================================
// ================= URL DECODER SIMPLE ================
// =====================================================

void decode_url_time(char *dest, const char *src, int max_len)
{
    int i = 0;
    int j = 0;

    while (src[i] != '\0' && src[i] != ' ' && src[i] != '&' && j < max_len - 1)
    {

        if (src[i] == '+')
        {
            dest[j++] = ' ';
            i++;
        }
        else if (src[i] == '%' && src[i + 1] && src[i + 2])
        {
            char hex[3];
            hex[0] = src[i + 1];
            hex[1] = src[i + 2];
            hex[2] = '\0';

            int value = 0;
            sscanf(hex, "%x", &value);

            dest[j++] = (char)value;
            i += 3;
        }
        else
        {
            dest[j++] = src[i++];
        }
    }

    dest[j] = '\0';
}

// =====================================================
// ================= SERVER ============================
// =====================================================

static err_t server_callback(void *arg,
                             struct tcp_pcb *tpcb,
                             struct pbuf *p,
                             err_t err)
{
    if (p == NULL)
    {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char request[512];

    int len = p->tot_len;
    if (len > 511)
        len = 511;

    pbuf_copy_partial(p, request, len, 0);
    request[len] = '\0';

    tcp_recved(tpcb, p->tot_len);

    printf("REQUEST:\n%s\n", request);

    if (strstr(request, "GET /water"))
    {

        printf("WEB UDA ACUM\n");

        char *time_start = strstr(request, "time=");

        if (time_start)
        {
            time_start += 5;
            decode_url_time(last_water_time, time_start, sizeof(last_water_time));
        }

        web_watering = true;
        web_watering_stop = make_timeout_time_ms(5000);

        pump_set(true);

        const char *redirect =
            "HTTP/1.1 303 See Other\r\n"
            "Location: /\r\n"
            "Connection: close\r\n"
            "\r\n";

        tcp_write(tpcb, redirect, strlen(redirect), TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);

        pbuf_free(p);
        tcp_close(tpcb);

        return ERR_OK;
    }

    build_html();

    err_t write_err = tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);

    if (write_err != ERR_OK)
    {
        printf("TCP WRITE ERROR: %d\n", write_err);
    }

    tcp_output(tpcb);

    pbuf_free(p);

    tcp_close(tpcb);

    return ERR_OK;
}

static err_t server_accept(void *arg,
                           struct tcp_pcb *newpcb,
                           err_t err)
{
    if (err != ERR_OK || newpcb == NULL)
    {
        return ERR_VAL;
    }

    tcp_recv(newpcb, server_callback);

    return ERR_OK;
}

void start_server()
{
    struct tcp_pcb *pcb = tcp_new();

    if (!pcb)
    {
        printf("TCP NEW FAILED\n");
        return;
    }

    err_t bind_err = tcp_bind(pcb, IP_ADDR_ANY, 80);

    if (bind_err != ERR_OK)
    {
        printf("TCP BIND FAILED: %d\n", bind_err);
        return;
    }

    pcb = tcp_listen_with_backlog(pcb, 4);

    if (!pcb)
    {
        printf("TCP LISTEN FAILED\n");
        return;
    }

    tcp_accept(pcb, server_accept);

    printf("HTTP SERVER STARTED\n");
}

// =====================================================
// ================= OLED STATUS =======================
// =====================================================

void show_oled_status(const char *ip_text)
{
    oled_clear();

    draw_rect(0, 0, 128, 64);

    draw_text_center(6, "SMART IRIG");

    draw_text(8, 22, "IP:");
    draw_text(26, 22, ip_text);

    draw_text(8, 40, pump_on ? "PUMP ON" : "PUMP OFF");

    oled_show();
}

// =====================================================
// ================= MAIN ==============================
// =====================================================

int main()
{
    stdio_init_all();

    sleep_ms(8000);

    printf("BOOT OK\n");

    // ================= OLED =================

    i2c_init(I2C_PORT, 100000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);

    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    oled_init();

    oled_clear();
    draw_rect(0, 0, 128, 64);
    draw_text_center(20, "BOOT OK");
    draw_text_center(40, "STARTING");
    oled_show();

    // ================= BUTTONS =================

    gpio_init(BUTTON_MENU);
    gpio_set_dir(BUTTON_MENU, GPIO_IN);
    gpio_pull_up(BUTTON_MENU);

    gpio_init(BUTTON_WATER);
    gpio_set_dir(BUTTON_WATER, GPIO_IN);
    gpio_pull_up(BUTTON_WATER);

    // ================= PUMP =================

    gpio_init(PUMP_PIN);
    gpio_set_dir(PUMP_PIN, GPIO_OUT);

    // ================= ADC =================

    adc_init();
    adc_gpio_init(SOIL_ADC_PIN);
    adc_select_input(SOIL_ADC_INPUT);

    // ================= WIFI INIT =================

    if (cyw43_arch_init())
    {

        oled_clear();
        draw_rect(0, 0, 128, 64);
        draw_text_center(20, "CYW43 ERROR");
        oled_show();

        printf("CYW43 INIT FAILED\n");

        while (true)
        {
            sleep_ms(1000);
        }
    }

    pump_set(false);

    cyw43_arch_enable_sta_mode();

    oled_clear();
    draw_rect(0, 0, 128, 64);
    draw_text_center(20, "CONNECTING");
    draw_text_center(40, "TO WIFI");
    oled_show();

    printf("CONNECTING TO WIFI: %s\n", WIFI_SSID);

    int result = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID,
        WIFI_PASS,
        CYW43_AUTH_WPA2_AES_PSK,
        30000);

    if (result != 0)
    {

        oled_clear();
        draw_rect(0, 0, 128, 64);
        draw_text_center(16, "WIFI FAILED");
        draw_text_center(36, "CHECK PASS");
        oled_show();

        printf("WIFI CONNECT FAILED: %d\n", result);

        while (true)
        {
            cyw43_arch_poll();
            sleep_ms(100);
        }
    }

    // ================= GET IP =================

    struct netif *netif = netif_default;
    const ip4_addr_t *ip = netif_ip4_addr(netif);

    char ip_text[32];
    snprintf(ip_text, sizeof(ip_text), "%s", ip4addr_ntoa(ip));

    printf("IP: %s\n", ip_text);

    show_oled_status(ip_text);

    // ================= SERVER =================

    start_server();

    // ================= MAIN LOOP =================

    absolute_time_t last_oled_update = get_absolute_time();

    while (true)
    {
        cyw43_arch_poll();

        bool manual_button = !gpio_get(BUTTON_WATER);

        if (web_watering)
        {

            pump_set(true);

            if (time_reached(web_watering_stop))
            {
                web_watering = false;
                pump_set(false);
            }
        }
        else if (manual_button)
        {

            pump_set(true);
        }
        else
        {

            pump_set(false);
        }

        if (absolute_time_diff_us(last_oled_update, get_absolute_time()) > 500000)
        {
            show_oled_status(ip_text);
            last_oled_update = get_absolute_time();
        }

        sleep_ms(10);
    }
}