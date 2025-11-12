#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "driver/gpio.h"
#include "names.h"
#include "Display_SSD1306.h"

constexpr char *TAG = "Main";

#define BUTTON_GPIO GPIO_NUM_9
#define ESPNOW_CHANNEL 1
#define WIFI_IFACE WIFI_IF_STA

Display_SSD1306 display;


// --- Broadcast address ---
static constexpr uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// --- Enum for event types ---
enum espnow_message_event_t : uint8_t
{
    ESPNOW_MESSAGE_EVENT_BUTTON_PRESS = 0,
    ESPNOW_MESSAGE_EVENT_STARTUP = 1,
    ESPNOW_MESSAGE_EVENT_SCORE_UPDATE = 2,
};

// --- Compact binary message struct ---
typedef struct __attribute__((packed))
{
    char name[8];
    espnow_message_event_t event;
    int32_t value;
    uint8_t destinationMac[6];
} espnow_message_t;

static const char *EventToString(espnow_message_event_t e)
{
    switch (e)
    {
    case ESPNOW_MESSAGE_EVENT_BUTTON_PRESS:
        return "button";
    case ESPNOW_MESSAGE_EVENT_STARTUP:
        return "startup";
    case ESPNOW_MESSAGE_EVENT_SCORE_UPDATE:
        return "score";
    default:
        return "UNKNOWN";
    }
}

// --- Log helper ---
static const char *event_to_string(espnow_message_event_t e)
{
    switch (e)
    {
    case ESPNOW_MESSAGE_EVENT_BUTTON_PRESS:
        return "BUTTON_PRESS";
    case ESPNOW_MESSAGE_EVENT_STARTUP:
        return "STARTUP";
    case ESPNOW_MESSAGE_EVENT_SCORE_UPDATE:
        return "SCORE_UPDATE";
    default:
        return "UNKNOWN";
    }
}

// --- Globals ---
static uint8_t my_mac[6] = {0};

// --- Send raw message ---
static void send_message(const espnow_message_t &msg)
{
    esp_err_t result = esp_now_send(msg.destinationMac,
                                    reinterpret_cast<const uint8_t *>(&msg),
                                    sizeof(msg));
    if (result == ESP_OK)
    {
        ESP_LOGI(TAG, "Sent event=%s value=%ld name=%s",
                 event_to_string(msg.event), (long)msg.value, msg.name);
    }
    else
    {
        ESP_LOGE(TAG, "Error sending ESP-NOW message: %s", esp_err_to_name(result));
    }
}

// --- Helper to send with parameters ---
static void sendEvent(espnow_message_event_t event, int32_t value, const uint8_t *dest = BROADCAST_MAC)
{
    espnow_message_t msg = {};
    strncpy(msg.name, getDinoName(), sizeof(msg.name) - 1);
    msg.event = event;
    msg.value = value;
    memcpy(msg.destinationMac, dest, 6);
    send_message(msg);
}

// --- ESP-NOW receive callback ---
static void on_receive(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (!recv_info || !data || len != sizeof(espnow_message_t))
        return;

    // Filter: skip messages from self
    if (memcmp(recv_info->src_addr, my_mac, 6) == 0)
        return;

    const espnow_message_t *msg = reinterpret_cast<const espnow_message_t *>(data);

    // Filter: only accept messages addressed to me or broadcast
    if (memcmp(msg->destinationMac, my_mac, 6) != 0 &&
        memcmp(msg->destinationMac, BROADCAST_MAC, 6) != 0)
        return;

    // Handle specific event types
    if (msg->event == ESPNOW_MESSAGE_EVENT_SCORE_UPDATE)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld", (long)msg->value);

        display.fill(0);
        display.drawText(0, 0, getDinoName(), TextStyle::Default(2));
        display.drawText(0, 24, buf, TextStyle::Default(2));
        display.show();
    }
}

// --- Button ISR ---
static void IRAM_ATTR button_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(reinterpret_cast<TaskHandle_t>(arg), &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// --- Button Task ---
static void button_task(void *arg)
{
    TaskHandle_t taskHandle = xTaskGetCurrentTaskHandle();

    // Configure button GPIO
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << BUTTON_GPIO;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_NEGEDGE; // falling edge = press
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, (void *)taskHandle);

    while (true)
    {
        // Wait for press
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        sendEvent(ESPNOW_MESSAGE_EVENT_BUTTON_PRESS, 1);
    }
}

// --- ESP-NOW init ---
static void init_espnow()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    // Get our own MAC for filtering
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, my_mac));
    ESP_LOGI(TAG, "My MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             my_mac[0], my_mac[1], my_mac[2],
             my_mac[3], my_mac[4], my_mac[5]);

    ESP_ERROR_CHECK(esp_now_init());

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.ifidx = WIFI_IFACE;
    peerInfo.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));

    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_receive));

    ESP_LOGI(TAG, "ESP-NOW initialized");
}

// --- Main ---
extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    init_espnow();

    // Send startup event
    sendEvent(ESPNOW_MESSAGE_EVENT_STARTUP, 0);

    // Start button handler
    xTaskCreate(button_task, "button_task", 2048, nullptr, 5, nullptr);

    display.init();
    display.fill(0);
    display.drawText(0, 0, getDinoName(), TextStyle::Default(2));
    display.show();
}
