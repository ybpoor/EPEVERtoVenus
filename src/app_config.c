#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "app_config.h"
#include "epever_data.h"

static const char *TAG = "app_config";
static const char *NVS_NAMESPACE = "epever_cfg";
static const char *NVS_KEY_BLOB = "config_blob";

static epever_unit_data_t s_units[MAX_EPEVER_UNITS];
static SemaphoreHandle_t s_data_mutex = NULL;

void app_config_set_defaults(app_config_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(app_config_t));
    strncpy(cfg->wifi_ssid, "", sizeof(cfg->wifi_ssid));
    strncpy(cfg->wifi_pass, "", sizeof(cfg->wifi_pass));
    strncpy(cfg->mqtt_host, "venus.local", sizeof(cfg->mqtt_host));
    cfg->mqtt_port = 1883;
    strncpy(cfg->mqtt_user, "", sizeof(cfg->mqtt_user));
    strncpy(cfg->mqtt_pass, "", sizeof(cfg->mqtt_pass));
    strncpy(cfg->mqtt_base_topic, "solar_charger", sizeof(cfg->mqtt_base_topic));
    cfg->slave_ids[0] = 1; // Default Unit 1
    cfg->num_units = 1;
    cfg->poll_interval_ms = DEFAULT_POLL_INTERVAL;
    cfg->is_configured = false;
}

esp_err_t app_config_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated or new version found, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t app_config_load(app_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No saved config found in NVS, applying defaults");
        app_config_set_defaults(cfg);
        return err;
    }

    size_t required_size = sizeof(app_config_t);
    err = nvs_get_blob(handle, NVS_KEY_BLOB, cfg, &required_size);
    nvs_close(handle);

    if (err != ESP_OK || required_size != sizeof(app_config_t)) {
        ESP_LOGW(TAG, "Failed to read config blob (%s), applying defaults", esp_err_to_name(err));
        app_config_set_defaults(cfg);
        return err;
    }

    ESP_LOGI(TAG, "Config loaded: SSID='%s', MQTT='%s:%u', Units=%u, Poll=%ums",
             cfg->wifi_ssid, cfg->mqtt_host, cfg->mqtt_port, cfg->num_units, cfg->poll_interval_ms);
    return ESP_OK;
}

esp_err_t app_config_save(const app_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing (%s)", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, NVS_KEY_BLOB, cfg, sizeof(app_config_t));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        ESP_LOGI(TAG, "Config successfully committed to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to write config blob (%s)", esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}

esp_err_t app_config_reset(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "NVS configuration erased");
    }
    return err;
}

/* =========================================================================
 * Thread-Safe EPEVER Data Store Implementation
 * ========================================================================= */
void epever_data_init(void)
{
    if (!s_data_mutex) {
        s_data_mutex = xSemaphoreCreateMutex();
    }
    memset(s_units, 0, sizeof(s_units));
}

void epever_data_lock(void)
{
    if (s_data_mutex) {
        xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    }
}

void epever_data_unlock(void)
{
    if (s_data_mutex) {
        xSemaphoreGive(s_data_mutex);
    }
}

epever_unit_data_t* epever_data_get_unit(uint8_t slave_id)
{
    for (int i = 0; i < MAX_EPEVER_UNITS; i++) {
        if (s_units[i].slave_id == slave_id) {
            return &s_units[i];
        }
    }
    // If not found, assign first empty slot
    for (int i = 0; i < MAX_EPEVER_UNITS; i++) {
        if (s_units[i].slave_id == 0) {
            s_units[i].slave_id = slave_id;
            return &s_units[i];
        }
    }
    return NULL;
}

epever_unit_data_t* epever_data_get_by_index(uint8_t index)
{
    if (index < MAX_EPEVER_UNITS) {
        return &s_units[index];
    }
    return NULL;
}
