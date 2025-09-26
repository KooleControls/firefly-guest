#pragma once
#include "esp_log.h"
#include "InitGuard.h"
#include "WifiDriver.h"
#include "SystemInit.h"
#include "GpioDriver.h"

class HardwareManager {
    constexpr static const char* TAG = "HardwareManager";

public:
    HardwareManager() = default;
    ~HardwareManager() = default;
    HardwareManager(const HardwareManager &) = delete;
    HardwareManager &operator=(const HardwareManager &) = delete;

    void init()
    {
        if(initGuard.IsReady())
            return;
            
        SystemInit::InitNvs();
        SystemInit::InitNetworkStack();
        wifiDriver.Init();
        gpioDriver.Init();

        initGuard.SetReady();
    }

    WifiDriver& GetWifiDriver() { 
        REQUIRE_READY(initGuard);
        return wifiDriver; 
    }

    GpioDriver& GetGpioDriver() { 
        REQUIRE_READY(initGuard);
        return gpioDriver; 
    }



private:
    InitGuard initGuard;
    WifiDriver wifiDriver;
    GpioDriver gpioDriver;

};
