#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize underlying network interfaces and default event loop.
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Start Wi-Fi in Station mode and begin connecting to the configured AP.
 *
 * @param ssid Target Wi-Fi SSID
 * @param pass Target Wi-Fi password
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_manager_start_sta(const char *ssid, const char *pass);

/**
 * @brief Start Wi-Fi in Access Point mode (for Captive Portal setup).
 *
 * @param ap_ssid SSID for the broadcast AP (e.g. "EPEVER-Gateway-XXXX")
 * @param ap_pass Password for the AP (NULL or empty string for open network)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_manager_start_ap(const char *ap_ssid, const char *ap_pass);

/**
 * @brief Check if Wi-Fi station is currently connected and has an IP address.
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Scan available 2.4GHz Wi-Fi networks.
 *
 * @param ap_records Array of wifi_ap_record_t to store results
 * @param count In/out parameter: max size of array on entry, actual found on exit
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_manager_scan(wifi_ap_record_t *ap_records, uint16_t *count);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
