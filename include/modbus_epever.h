#ifndef MODBUS_EPEVER_H
#define MODBUS_EPEVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Modbus RTU driver and EPEVER polling engine.
 */
esp_err_t modbus_epever_init(void);

/**
 * @brief Launch the FreeRTOS background task that polls all configured EPEVER units sequentially.
 */
void modbus_epever_start_task(void);

/**
 * @brief Manually poll an individual EPEVER unit.
 *
 * @param slave_id Modbus slave address of controller
 * @param poll_infrequent If true, also polls static rated specs and battery settings
 * @return esp_err_t ESP_OK if communication succeeded
 */
esp_err_t modbus_epever_poll_unit(uint8_t slave_id, bool poll_infrequent);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_EPEVER_H
