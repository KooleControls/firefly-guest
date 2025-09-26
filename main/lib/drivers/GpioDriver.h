#pragma once
#include "driver/gpio.h"
#include "esp_log.h"
#include "InitGuard.h"

class GpioDriver
{
    static constexpr const char *TAG = "GpioDriver";

public:
    GpioDriver() = default;

    void Init()
    {
        if (initGuard.IsReady())
            return;

        // Install ISR service with IRAM flag so ISRs can live in IRAM
        ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));

        initGuard.SetReady();
        ESP_LOGI(TAG, "GPIO driver initialized");
    }

    esp_err_t SetPinMode(
        gpio_num_t pin,
        gpio_mode_t mode,
        gpio_pullup_t pullUp = GPIO_PULLUP_DISABLE,
        gpio_pulldown_t pullDown = GPIO_PULLDOWN_DISABLE,
        gpio_int_type_t intrType = GPIO_INTR_DISABLE)
    {
        gpio_config_t io_conf = {};
        io_conf.intr_type = intrType;
        io_conf.mode = mode;
        io_conf.pin_bit_mask = (1ULL << pin);
        io_conf.pull_up_en = pullUp;
        io_conf.pull_down_en = pullDown;
        return gpio_config(&io_conf);
    }

    void SetPin(gpio_num_t pin, bool value)
    {
        gpio_set_level(pin, value);
    }

    int ReadPin(gpio_num_t pin)
    {
        return gpio_get_level(pin);
    }

    /// Directly register ISR callback for a given pin
    esp_err_t RegisterIsr(gpio_num_t pin, gpio_isr_t cb, void* args = nullptr)
    {
        if (pin < 0 || pin >= GPIO_NUM_MAX)
            return ESP_ERR_INVALID_ARG;

        return gpio_isr_handler_add(pin, cb, args);
    }

private:
    InitGuard initGuard;
};
