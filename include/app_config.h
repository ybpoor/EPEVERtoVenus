#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

// Hardware Pin Definitions for ESP32-C3 SuperMini
#define EPEVER_UART_NUM         UART_NUM_1
#define EPEVER_PIN_RX           GPIO_NUM_4   // UART1 RX (connected to transceiver RO)
#define EPEVER_PIN_TX           GPIO_NUM_5   // UART1 TX (connected to transceiver DI)
#define EPEVER_PIN_RTS          GPIO_NUM_6   // UART1 RTS / DE & /RE direction pin
#define EPEVER_PIN_LED          GPIO_NUM_8   // Onboard Blue Status LED
#define EPEVER_PIN_BTN          GPIO_NUM_9   // Onboard Boot button (hold 3s for setup)

#define EPEVER_BAUD_RATE        115200

#define MAX_EPEVER_UNITS        8
#define DEFAULT_POLL_INTERVAL   2000         // ms

typedef struct {
    char wifi_ssid[33];
    char wifi_pass[64];
    char mqtt_host[64];
    uint16_t mqtt_port;
    char mqtt_user[33];
    char mqtt_pass[33];
    char mqtt_base_topic[33];                // e.g. "solar_charger"
    uint8_t slave_ids[MAX_EPEVER_UNITS];     // e.g. {1, 2, 0, 0...}
    uint8_t num_units;                       // Number of active units
    uint16_t poll_interval_ms;               // Modbus poll interval in ms
    bool is_configured;                      // Has valid Wi-Fi and MQTT setup
} app_config_t;

/**
 * @brief Initialize NVS flash storage for configuration.
 */
esp_err_t app_config_init(void);

/**
 * @brief Load configuration from NVS. If none found, populates with defaults.
 */
esp_err_t app_config_load(app_config_t *cfg);

/**
 * @brief Save configuration to NVS.
 */
esp_err_t app_config_save(const app_config_t *cfg);

/**
 * @brief Erase configuration from NVS (triggering setup mode on reboot).
 */
esp_err_t app_config_reset(void);

/**
 * @brief Populate configuration struct with sensible defaults.
 */
void app_config_set_defaults(app_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif // APP_CONFIG_H
