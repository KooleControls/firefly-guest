#pragma once
#include "Display.h"
#include "SSD1306.h"



class Display_SSD1306 : public Display
{
    inline static constexpr const char *TAG = "Display_SSD1306";
    constexpr static const uint8_t OLED_WIDTH = 128;
    constexpr static const uint8_t OLED_HEIGHT = 64;
    constexpr static const uint8_t OLED_ADDR = 0x3C;
    constexpr static const bool EXTERNAL_VCC = false;

    constexpr static const i2c_port_t I2C_PORT = I2C_NUM_0;
    constexpr static const gpio_num_t SDA_PIN = GPIO_NUM_5;
    constexpr static const gpio_num_t SCL_PIN = GPIO_NUM_6;
    constexpr static const uint32_t I2C_FREQ_HZ = 400000;

public:
    esp_err_t init()
    {
        ESP_LOGI(TAG, "Initializing I2C bus...");

        i2c_master_bus_config_t bus_cfg = {};
        bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_cfg.i2c_port = I2C_PORT;
        bus_cfg.sda_io_num = SDA_PIN;
        bus_cfg.scl_io_num = SCL_PIN;
        bus_cfg.glitch_ignore_cnt = 7;
        bus_cfg.flags.enable_internal_pullup = true;
            
        

        esp_err_t err = i2c_new_master_bus(&bus_cfg, &busHandle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(err));
            return err;
        }

        ESP_LOGI(TAG, "I2C bus initialized successfully.");


        err = display.Init(busHandle, OLED_ADDR);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize SSD1306: %s", esp_err_to_name(err));
            return err;
        }

        display.fill(0);
        display.show();
        ESP_LOGI(TAG, "Display initialized successfully.");
        return ESP_OK;
    }

    void fill(uint8_t color) override { display.fill(color); }
    void drawPixel(int x, int y, bool color) override { display.drawPixel(x, y, color); }
    void show() override { display.show(); }
    void drawChar(int x, int y, char c, const TextStyle &style) override { display.drawChar(x, y, c, style); }
    void drawText(int x, int y, const char *str, const TextStyle &style) override { display.drawText(x, y, str, style); }

private:
    i2c_master_bus_handle_t busHandle = nullptr;
    SSD1306_I2C display{OLED_WIDTH, OLED_HEIGHT, EXTERNAL_VCC};
};
