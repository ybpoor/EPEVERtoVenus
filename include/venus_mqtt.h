#ifndef VENUS_MQTT_H
#define VENUS_MQTT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "app_config.h"
#include "epever_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the ESP-IDF MQTT client for Venus OS broker.
 *
 * @param cfg Pointer to application configuration struct
 * @return esp_err_t ESP_OK on success
 */
esp_err_t venus_mqtt_init(const app_config_t *cfg);

/**
 * @brief Start the MQTT publisher task.
 */
void venus_mqtt_start_task(void);

/**
 * @brief Check if MQTT client is currently connected to the Venus OS broker.
 */
bool venus_mqtt_is_connected(void);

/**
 * @brief Publish formatted Venus OS JSON and diagnostic telemetry for an EPEVER unit.
 *
 * @param unit_index Index of unit (0 .. num_units-1), used for subtopics
 * @param data Pointer to unit telemetry data
 * @param base_topic Base MQTT topic (e.g. "solar_charger")
 * @return esp_err_t ESP_OK if published successfully
 */
esp_err_t venus_mqtt_publish_unit(uint8_t unit_index, const epever_unit_data_t *data, const char *base_topic);

#ifdef __cplusplus
}
#endif

#endif // VENUS_MQTT_H
