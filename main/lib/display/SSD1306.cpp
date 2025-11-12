#include "ssd1306.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <algorithm>
#include "esp_log.h"


// ==== SSD1306 Command Constants ====
constexpr uint8_t SET_CONTRAST        = 0x81;
constexpr uint8_t SET_ENTIRE_ON       = 0xA4;
constexpr uint8_t SET_NORM_INV        = 0xA6;
constexpr uint8_t SET_DISP            = 0xAE;
constexpr uint8_t SET_MEM_ADDR        = 0x20;
constexpr uint8_t SET_COL_ADDR        = 0x21;
constexpr uint8_t SET_PAGE_ADDR       = 0x22;
constexpr uint8_t SET_DISP_START_LINE = 0x40;
constexpr uint8_t SET_SEG_REMAP       = 0xA0;
constexpr uint8_t SET_MUX_RATIO       = 0xA8;
constexpr uint8_t SET_COM_OUT_DIR     = 0xC0;
constexpr uint8_t SET_DISP_OFFSET     = 0xD3;
constexpr uint8_t SET_COM_PIN_CFG     = 0xDA;
constexpr uint8_t SET_DISP_CLK_DIV    = 0xD5;
constexpr uint8_t SET_PRECHARGE       = 0xD9;
constexpr uint8_t SET_VCOM_DESEL      = 0xDB;
constexpr uint8_t SET_CHARGE_PUMP     = 0x8D;

SSD1306::SSD1306(uint8_t w, uint8_t h, bool ext_vcc)
    : width(w), height(h), external_vcc(ext_vcc)
{
    pages = height / 8;
    buffer = new uint8_t[pages * width];
    memset(buffer, 0, pages * width);
}

SSD1306::~SSD1306() {
    delete[] buffer;
}

void SSD1306::initDisplay() {
    const uint8_t cmds[] = {
        SET_DISP | 0x00,
        SET_MEM_ADDR, 0x00,
        SET_DISP_START_LINE | 0x00,
        SET_SEG_REMAP | 0x01,
        SET_MUX_RATIO, (uint8_t)(height - 1),
        SET_COM_OUT_DIR | 0x08,
        SET_DISP_OFFSET, 0x00,
        SET_COM_PIN_CFG, (uint8_t)((width > 2 * height) ? 0x02 : 0x12),
        SET_DISP_CLK_DIV, 0x80,
        SET_PRECHARGE, (uint8_t)(external_vcc ? 0x22 : 0xF1),
        SET_VCOM_DESEL, 0x30,
        SET_CONTRAST, 0xFF,
        SET_ENTIRE_ON,
        SET_NORM_INV,
        SET_CHARGE_PUMP, (uint8_t)(external_vcc ? 0x10 : 0x14),
        SET_DISP | 0x01
    };
    for (uint8_t c : cmds) writeCmd(c);
    fill(0);
    show();
}

void SSD1306::powerOff() { writeCmd(SET_DISP | 0x00); }
void SSD1306::powerOn()  { writeCmd(SET_DISP | 0x01); }

void SSD1306::contrast(uint8_t value) {
    writeCmd(SET_CONTRAST);
    writeCmd(value);
}

void SSD1306::invert(bool inv) {
    writeCmd(SET_NORM_INV | (inv ? 1 : 0));
}

void SSD1306::fill(uint8_t color) {
    memset(buffer, color ? 0xFF : 0x00, pages * width);
}

void SSD1306::drawPixel(int x, int y, bool color) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    int page = y / 8;
    int bit = y % 8;
    if (color)
        buffer[x + page * width] |= (1 << bit);
    else
        buffer[x + page * width] &= ~(1 << bit);
}

void SSD1306::show() {
    // These offsets align the 128×64 buffer to the visible 72×40 window
    const uint8_t xOffset = 26;  // shift right (try 26–32)
    const uint8_t yOffset = 3;   // shift down (3 pages × 8 px = 24 px)

    for (uint8_t page = 0; page < pages; page++) {
        writeCmd(0xB0 + page + yOffset);        // set page address (vertical offset)
        writeCmd((xOffset + 2) & 0x0F);         // lower column start
        writeCmd(0x10 | ((xOffset + 2) >> 4));  // higher column start
        writeData(&buffer[page * width], width);
    }
}

void SSD1306::drawChar(int x, int y, char c, const TextStyle& style)
{
    if (!style.font) return;

    const uint8_t* glyph = style.font->GetGlyph(c);
    if (!glyph) return;

    for (int col = 0; col < style.font->width; ++col)
    {
        uint8_t bits = glyph[col];
        for (int row = 0; row < style.font->height; ++row)
        {
            if (bits & (1 << row))
            {
                for (int dx = 0; dx < style.size; ++dx)
                {
                    for (int dy = 0; dy < style.size; ++dy)
                    {
                        drawPixel(
                            x + col * style.size + dx,
                            y + row * style.size + dy,
                            style.color
                        );
                    }
                }
            }
        }
    }
}

void SSD1306::drawText(int x, int y, const char* str, const TextStyle& style)
{
    if (!style.font || !str) return;

    int cursorX = x;
    while (*str)
    {
        drawChar(cursorX, y, *str++, style);
        cursorX += (style.font->width + 1) * style.size;
    }
}






// ===================== I2C =====================
SSD1306_I2C::SSD1306_I2C(uint8_t w, uint8_t h, bool ext_vcc)
    : SSD1306(w, h, ext_vcc)
{
}

// Initializes the SSD1306 device on an already initialized I2C bus
esp_err_t SSD1306_I2C::Init(i2c_master_bus_handle_t busHandle, uint8_t addr)
{
    address = addr;
    bus = busHandle;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 400000,
    };

    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SSD1306 device: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "SSD1306 device attached at address 0x%02X", address);

    // Initialize display
    initDisplay();
    contrast(0xFF);
    fill(0);
    show();

    return ESP_OK;
}

void SSD1306_I2C::writeCmd(uint8_t cmd)
{
    uint8_t buffer[2] = {0x80, cmd}; // control byte (Co=1, D/C#=0), then command
    esp_err_t err = i2c_master_transmit(dev, buffer, sizeof(buffer), 50);
    if (err != ESP_OK)
        ESP_LOGW(TAG, "writeCmd 0x%02X err=%s", cmd, esp_err_to_name(err));
}

void SSD1306_I2C::writeData(const uint8_t *data, size_t len)
{
    constexpr size_t CHUNK = 32;
    uint8_t buf[CHUNK + 1];
    buf[0] = 0x40; // control byte (Co=0, D/C#=1)

    while (len > 0)
    {
        size_t n = len > CHUNK ? CHUNK : len;
        memcpy(&buf[1], data, n);

        esp_err_t err = i2c_master_transmit(dev, buf, n + 1, 100);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "writeData chunk err=%s", esp_err_to_name(err));
            break;
        }

        data += n;
        len -= n;
        vTaskDelay(0); // yield for watchdog safety
    }
}


// ===================== SPI =====================

SSD1306_SPI::SSD1306_SPI(uint8_t w, uint8_t h, spi_device_handle_t dev,
                         gpio_num_t dc, gpio_num_t rst, bool ext_vcc)
    : SSD1306(w, h, ext_vcc), spi(dev), dc_pin(dc), rst_pin(rst)
{
    gpio_set_direction(dc_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(rst_pin, GPIO_MODE_OUTPUT);

    gpio_set_level(rst_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(rst_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(rst_pin, 1);

    initDisplay();
}

void SSD1306_SPI::writeCmd(uint8_t cmd) {
    gpio_set_level(dc_pin, 0);
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_transmit(spi, &t);
}

void SSD1306_SPI::writeData(const uint8_t* data, size_t len) {
    gpio_set_level(dc_pin, 1);
    spi_transaction_t t = {};
    t.length = len * 8;
    t.tx_buffer = data;
    spi_device_transmit(spi, &t);
}
