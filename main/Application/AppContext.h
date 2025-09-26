#pragma once
#include "InitGuard.h"
#include "SystemInit.h"
#include "HardwareManager.h"
#include "EspnowManager.h"
#include "freertos/semphr.h"

class AppContext
{
public:
    AppContext() = default;
    ~AppContext() = default;
    AppContext(const AppContext &) = delete;
    AppContext &operator=(const AppContext &) = delete;

    void Init()
    {
        if (initGuard.IsReady())
            return;

        hardwareManager.init();
        SystemInit::InitNtp();
        espnowManager.init();

        GpioDriver& gpio = hardwareManager.GetGpioDriver();

        // Configure GPIO0 as input with pull-up and falling edge interrupt
        gpio.SetPinMode(GPIO_NUM_0,
                        GPIO_MODE_INPUT,
                        GPIO_PULLUP_ENABLE,
                        GPIO_PULLDOWN_DISABLE,
                        GPIO_INTR_NEGEDGE);

        // Create counting semaphore
        buttonSem = xSemaphoreCreateCounting(100, 0);
        assert(buttonSem != nullptr);

        // Register ISR for button press
        gpio.RegisterIsr(GPIO_NUM_0, ButtonIsr, this);

        initGuard.SetReady();
    }

    void Tick()
    {
        if (xSemaphoreTake(buttonSem, portMAX_DELAY) == pdTRUE)
        {
            buttonPressCount++;
            espnowManager.reportButtonPresses(buttonPressCount);
        }
    }

private:
    InitGuard initGuard;
    HardwareManager hardwareManager;
    EspNowManager espnowManager;
    SemaphoreHandle_t buttonSem = nullptr;
    uint32_t buttonPressCount = 0;

    static void IRAM_ATTR ButtonIsr(void* arg)
    {
        auto* self = static_cast<AppContext*>(arg);
        if (!self || !self->buttonSem) return;

        BaseType_t hpw = pdFALSE;
        xSemaphoreGiveFromISR(self->buttonSem, &hpw);
        portYIELD_FROM_ISR(hpw);
    }
};
