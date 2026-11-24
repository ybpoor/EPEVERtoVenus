#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the Captive Portal DNS server and embedded HTTP configuration server.
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t web_portal_start(void);

/**
 * @brief Stop the Captive Portal and HTTP server.
 */
void web_portal_stop(void);

/**
 * @brief Check if the Captive Portal web server is active.
 */
bool web_portal_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // WEB_PORTAL_H
