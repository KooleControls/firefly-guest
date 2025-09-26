#pragma once
#include "EspNow.h"
#include "rtos.h"
#include "InitGuard.h"


class EspNowManager
{
    static constexpr uint8_t BROADCAST[ESP_NOW_ETH_ALEN] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

public:
    EspNowManager() = default;
    ~EspNowManager() = default;

    void init()
    {
        if (initGuard.IsReady())
            return;

        LOCK(mutex);

        espnow.init();
        
        task.Init("EspNow", 7, 1024 * 4);
        task.SetHandler([this]() { work(); });
        task.Run();

        initGuard.SetReady();
    }

    void reportButtonPresses(int32_t count)
    {
        LOCK(mutex);
        if (memcmp(hostMac, "\x00\x00\x00\x00\x00\x00", ESP_NOW_ETH_ALEN) == 0) {
            ESP_LOGW("EspNowManager", "Host not discovered yet, cannot report button presses");
            return;
        }

        auto pkg = EspNow::Package::Make(hostMac, "CBUT", count);
        espnow.Send(pkg);
    }

private:
    InitGuard initGuard;
    Mutex mutex;
    EspNow espnow;
    Task task;
    uint8_t hostMac[ESP_NOW_ETH_ALEN]{0};

    void work()
    {
        EspNow::Package rxPackage;
        while (1)
        {
            // Periodically send the discovery packet
            SendDiscovery();

            // Here we abuse the timeout for the discovery interval
            if (espnow.Receive(rxPackage, pdMS_TO_TICKS(10000)))
            {
                processReceivedPackage(rxPackage);
            }
        }
    }

    void SendDiscovery()
    {
        if(memcmp(hostMac, "\x00\x00\x00\x00\x00\x00", ESP_NOW_ETH_ALEN) != 0) {
            // Already discovered
            return;
        }
        auto pkg = EspNow::Package::Make(BROADCAST, "CDSC");
        espnow.Send(pkg);
        ESP_LOGI("EspNowManager", "Sent discovery packet");
    }

    // --- Command routing types ---
    enum class MatchFlags : uint8_t
    {
        None      = 0,
        Broadcast = 1 << 0,
        ForMe     = 1 << 1,
        OnlyForMe = 1 << 2,
        Any       = Broadcast | ForMe | OnlyForMe
    };

    friend constexpr MatchFlags operator|(MatchFlags a, MatchFlags b)
    {
        return static_cast<MatchFlags>(
            static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    friend constexpr bool operator&(MatchFlags a, MatchFlags b)
    {
        return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
    }

    struct CommandRoute
    {
        char commandId[EspNow::CommandSize + 1];
        MatchFlags flags;
        void (EspNowManager::*handler)(const EspNow::Package &package);
    };

    // --- Handlers ---
    void handleGuestDiscovery(const EspNow::Package &package)
    {
        memcpy(hostMac, package.source, ESP_NOW_ETH_ALEN);
        ESP_LOGI("EspNowManager", "Discovered host with MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 hostMac[0], hostMac[1], hostMac[2],
                 hostMac[3], hostMac[4], hostMac[5]);
    }

    constexpr static CommandRoute commandRoutes[] = {
        {"RDSC", MatchFlags::OnlyForMe, &EspNowManager::handleGuestDiscovery},
    };

    // --- Dispatch ---
    void processReceivedPackage(const EspNow::Package &package)
    {
        LOCK(mutex);
        for (auto &entry : commandRoutes)
        {
            if (strncmp(package.commandId, entry.commandId, EspNow::CommandSize) == 0)
            {
                if (matchesFlags(package, entry.flags))
                {
                    (this->*entry.handler)(package);
                }
                return;
            }
        }
        ESP_LOGW("EspNowManager", "Unknown command: %.4s", package.commandId);
    }

    static bool matchesFlags(const EspNow::Package &pkg, MatchFlags flags)
    {
        if ((flags & MatchFlags::Broadcast) && pkg.isBroadcast)
            return true;
        if ((flags & MatchFlags::ForMe) && pkg.isForMe)
            return true;
        if ((flags & MatchFlags::OnlyForMe) && pkg.isOnlyForMe())
            return true;
        return false;
    }
};
