#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "app_config.h"
#include "wifi_manager.h"
#include "modbus_epever.h"
#include "venus_mqtt.h"
#include "web_portal.h"

static const char *TAG = "main";
app_config_t g_config;

static void configure_gpio(void)
{
    // Status Blue LED (GPIO 8)
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << EPEVER_PIN_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);
    gpio_set_level(EPEVER_PIN_LED, 1); // LED off (active low on SuperMini)

    // Boot Button (GPIO 9, active low)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << EPEVER_PIN_BTN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_conf);
}

static void supervisor_task(void *pvParameters)
{
    uint32_t btn_hold_ticks = 0;
    bool led_state = false;

    while (1) {
        // 1. Check Boot Button (GPIO 9) - Hold for 3s to reset into setup mode
        if (gpio_get_level(EPEVER_PIN_BTN) == 0) {
            btn_hold_ticks++;
            if (btn_hold_ticks >= 30) { // 30 * 100ms = 3.0s
                ESP_LOGW(TAG, "Factory reset button triggered! Clearing config and entering Web Setup...");
                // Rapid LED blink indicator
                for (int i = 0; i < 10; i++) {
                    gpio_set_level(EPEVER_PIN_LED, i % 2);
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
                app_config_reset();
                esp_restart();
            }
        } else {
            btn_hold_ticks = 0;
        }

        // 2. LED Status Indication
        if (web_portal_is_running()) {
            // Setup Mode: Solid ON (active low = 0)
            gpio_set_level(EPEVER_PIN_LED, 0);
        } else if (venus_mqtt_is_connected()) {
            // Normal operation with MQTT connected: Slow 1Hz gentle heartbeat
            led_state = !led_state;
            gpio_set_level(EPEVER_PIN_LED, led_state ? 0 : 1);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        } else if (wifi_manager_is_connected()) {
            // Wi-Fi connected but connecting to MQTT: 2Hz blink
            led_state = !led_state;
            gpio_set_level(EPEVER_PIN_LED, led_state ? 0 : 1);
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        } else {
            // Connecting to Wi-Fi: Rapid blink
            led_state = !led_state;
            gpio_set_level(EPEVER_PIN_LED, led_state ? 0 : 1);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main_c(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, " ESP32-C3 SuperMini EPEVER to Venus OS Gateway");
    ESP_LOGI(TAG, "==================================================");

    configure_gpio();

    // Initialize NVS
    ESP_ERROR_CHECK(app_config_init());

    // Load configuration
    app_config_load(&g_config);

    // Initialize Wi-Fi subsystem
    ESP_ERROR_CHECK(wifi_manager_init());

    // Start background button and LED supervisor task
    xTaskCreate(supervisor_task, "supervisor", 2048, NULL, 3, NULL);

    // Determine startup mode: Setup (Captive Portal) or Normal (Station + MQTT)
    if (!g_config.is_configured || strlen(g_config.wifi_ssid) == 0) {
        ESP_LOGW(TAG, "No valid Wi-Fi configuration found -> Entering Web Setup Mode");

        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
        char ap_ssid[32];
        snprintf(ap_ssid, sizeof(ap_ssid), "EPEVER-Gateway-%02X%02X", mac[4], mac[5]);

        ESP_ERROR_CHECK(wifi_manager_start_ap(ap_ssid, NULL));
        ESP_ERROR_CHECK(web_portal_start());

        ESP_LOGI(TAG, "--------------------------------------------------");
        ESP_LOGI(TAG, "Connect your phone or PC to Wi-Fi AP: '%s'", ap_ssid);
        ESP_LOGI(TAG, "Open browser to: http://192.168.4.1");
        ESP_LOGI(TAG, "--------------------------------------------------");
    } else {
        ESP_LOGI(TAG, "Configuration found -> Starting Normal Operating Mode");
        ESP_LOGI(TAG, "Target Wi-Fi SSID: '%s'", g_config.wifi_ssid);
        ESP_LOGI(TAG, "Venus OS MQTT: '%s:%u'", g_config.mqtt_host, g_config.mqtt_port);
        ESP_LOGI(TAG, "Configured Units: %d", g_config.num_units);

        // Start Wi-Fi Station
        ESP_ERROR_CHECK(wifi_manager_start_sta(g_config.wifi_ssid, g_config.wifi_pass));

        // Initialize Modbus RTU RS485 communication
        ESP_ERROR_CHECK(modbus_epever_init());
        modbus_epever_start_task();

        // Initialize Venus OS MQTT client and publisher
        ESP_ERROR_CHECK(venus_mqtt_init(&g_config));
        venus_mqtt_start_task();
    }
}

#ifndef ARDUINO
void app_main(void)
{
    app_main_c();
}
#endif
