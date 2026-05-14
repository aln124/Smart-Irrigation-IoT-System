#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/i2c.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

#define WIFI_SSID "aln"
#define WIFI_PASS "cocacola"

#define I2C_PORT i2c0
#define I2C_SDA 16
#define I2C_SCL 17
#define OLED_ADDR 0x3C

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_BUF_SIZE (OLED_WIDTH * OLED_HEIGHT / 8)

uint8_t buffer[OLED_BUF_SIZE];

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
        0xAE, 0x20, 0x00, 0xB0, 0xC8, 0x00, 0x10, 0x40,
        0x81, 0x7F, 0xA1, 0xA6, 0xA8, 0x3F, 0xA4,
        0xD3, 0x00, 0xD5, 0x80, 0xD9, 0xF1,
        0xDA, 0x12, 0xDB, 0x40, 0x8D, 0x14, 0xAF
    };

    for (int i = 0; i < sizeof(cmds); i++) {
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

void draw_pixel(int x, int y)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    buffer[x + (y / 8) * OLED_WIDTH] |= (1 << (y % 8));
}

void draw_rect(int x, int y, int w, int h)
{
    for (int i = x; i < x + w; i++) {
        draw_pixel(i, y);
        draw_pixel(i, y + h - 1);
    }

    for (int j = y; j < y + h; j++) {
        draw_pixel(x, j);
        draw_pixel(x + w - 1, j);
    }
}

const uint8_t font5x7[][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x7A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x04,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x7F,0x20,0x18,0x20,0x7F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x03,0x04,0x78,0x04,0x03}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z

    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9

    {0x00,0x00,0x00,0x00,0x00}, // SPACE
    {0x00,0x36,0x36,0x00,0x00}, // :
    {0x08,0x08,0x08,0x08,0x08}, // -
    {0x00,0x60,0x60,0x00,0x00}  // .
};

int get_char_index(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '0' && c <= '9') return 26 + (c - '0');
    if (c == ' ') return 36;
    if (c == ':') return 37;
    if (c == '-') return 38;
    if (c == '.') return 39;
    return 36;
}

void draw_char(int x, int y, char c)
{
    int index = get_char_index(c);

    for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[index][col];

        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                draw_pixel(x + col, y + row);
            }
        }
    }
}

void draw_text(int x, int y, const char *text)
{
    while (*text) {
        draw_char(x, y, *text);
        x += 6;
        text++;
    }
}

void draw_text_center(int y, const char *text)
{
    int len = strlen(text);
    int x = (OLED_WIDTH - len * 6) / 2;
    if (x < 0) x = 0;
    draw_text(x, y, text);
}

void show_message(const char *line1, const char *line2, const char *line3)
{
    oled_clear();
    draw_rect(0, 0, 128, 64);

    draw_text_center(8, line1);
    draw_text_center(28, line2);
    draw_text_center(46, line3);

    oled_show();
}

int main()
{
    stdio_init_all();
    sleep_ms(2000);

    i2c_init(I2C_PORT, 100000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);

    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    oled_init();

    show_message("SMART IRIG", "WIFI START", "PLEASE WAIT");

    printf("Starting WiFi...\n");

    if (cyw43_arch_init()) {
        printf("CYW43 init failed\n");
        show_message("WIFI ERROR", "CYW43 FAIL", "CHECK SDK");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    show_message("CONNECTING", "TO HOTSPOT", WIFI_SSID);

    printf("Connecting to WiFi: %s\n", WIFI_SSID);

    int result = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID,
        WIFI_PASS,
        CYW43_AUTH_WPA2_AES_PSK,
        30000
    );

    if (result != 0) {
        printf("WiFi connect failed: %d\n", result);
        show_message("WIFI FAILED", "CHECK PASS", "OR HOTSPOT");
        return 1;
    }

    printf("WiFi connected!\n");

    char ip_text[32];

    struct netif *netif = netif_default;
    const ip4_addr_t *ip = netif_ip4_addr(netif);

    snprintf(ip_text, sizeof(ip_text), "%s", ip4addr_ntoa(ip));

    printf("IP address: %s\n", ip_text);

    show_message("WIFI OK", "OPEN BROWSER", ip_text);

    while (true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(300);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(700);
    }
}