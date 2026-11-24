#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_idf_version.h"
#include "cJSON.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_config.h"
#include "epever_registers.h"
#include "epever_data.h"
#include "venus_mqtt.h"

static const char *TAG = "venus_mqtt";
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_is_connected = false;
extern app_config_t g_config;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to Venus OS MQTT Broker");
        s_is_connected = true;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from Venus OS MQTT Broker");
        s_is_connected = false;
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT Error occurred");
        break;
    default:
        break;
    }
}

esp_err_t venus_mqtt_init(const app_config_t *cfg)
{
    if (!cfg || strlen(cfg->mqtt_host) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    char uri_buf[128];
    snprintf(uri_buf, sizeof(uri_buf), "mqtt://%s:%u", cfg->mqtt_host, cfg->mqtt_port ? cfg->mqtt_port : 1883);
    ESP_LOGI(TAG, "Initializing MQTT client for URI: %s", uri_buf);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri_buf,
        .credentials.client_id = "epever2venus_esp32c3",
    };
    if (strlen(cfg->mqtt_user) > 0) {
        mqtt_cfg.credentials.username = cfg->mqtt_user;
        mqtt_cfg.credentials.authentication.password = cfg->mqtt_pass;
    }
#else
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = uri_buf,
        .client_id = "epever2venus_esp32c3",
    };
    if (strlen(cfg->mqtt_user) > 0) {
        mqtt_cfg.username = cfg->mqtt_user;
        mqtt_cfg.password = cfg->mqtt_pass;
    }
#endif

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_mqtt_client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return esp_mqtt_client_start(s_mqtt_client);
}

bool venus_mqtt_is_connected(void)
{
    return s_is_connected;
}

static uint8_t map_victron_state(const epever_unit_data_t *data)
{
    if (!data->is_online) return 0; // Off
    if (data->realtime.fault_active) return 2; // Fault
    if (data->realtime.is_night || data->realtime.pv_power < 0.5f) return 0; // Off

    switch (data->realtime.charging_stage) {
    case 1:  return 5; // Float
    case 2:  return 4; // Absorption (Boost)
    case 3:  return 7; // Equalize
    default: return 3; // Bulk (active charging)
    }
}

static uint8_t map_victron_error_code(const epever_unit_data_t *data)
{
    if (data->realtime.is_over_temp) return 17; // Charger temp high
    if ((data->realtime.charge_status_raw & CHG_STATUS_INPUT_VOLT_MASK) == 0x8000) return 33; // Input volt high
    if (data->realtime.charge_status_raw & CHG_STATUS_INPUT_OVERCURRENT) return 34; // Input current high
    if ((data->realtime.battery_status_raw & BAT_STATUS_TEMP_MASK) == 0x0010) return 1; // Battery temp high
    if ((data->realtime.battery_status_raw & BAT_STATUS_VOLT_MASK) == 0x0001) return 2; // Battery volt high
    return 0; // No error
}

static uint8_t map_mpp_operation_mode(const epever_unit_data_t *data)
{
    if (!data->is_online || data->realtime.is_night || data->realtime.pv_power < 0.5f) {
        return 0; // Off
    }
    if (data->realtime.charging_stage == 1) {
        return 1; // Voltage/Current limited (Float)
    }
    return 2; // MPPT Active
}

esp_err_t venus_mqtt_publish_unit(uint8_t unit_index, const epever_unit_data_t *data, const char *base_topic)
{
    if (!s_mqtt_client || !s_is_connected || !data) {
        return ESP_ERR_INVALID_STATE;
    }

    /* =====================================================================
     * 1. Primary Venus OS JSON (dbus-mqtt-solar-charger compatible)
     * ===================================================================== */
    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    // "Pv": { "V": 65.4 }
    cJSON *pv = cJSON_CreateObject();
    cJSON_AddNumberToObject(pv, "V", (double)data->realtime.pv_voltage);
    cJSON_AddItemToObject(root, "Pv", pv);

    // "Yield": { "Power": 450.0, "User": 3.42, "System": 852.1 }
    cJSON *yld = cJSON_CreateObject();
    cJSON_AddNumberToObject(yld, "Power", (double)data->realtime.pv_power);
    cJSON_AddNumberToObject(yld, "User", (double)data->stats.generated_energy_today);
    cJSON_AddNumberToObject(yld, "System", (double)data->stats.generated_energy_total);
    cJSON_AddItemToObject(root, "Yield", yld);

    // "Dc": { "0": { "Voltage": 26.8, "Current": 16.5 } }
    cJSON *dc = cJSON_CreateObject();
    cJSON *dc0 = cJSON_CreateObject();
    cJSON_AddNumberToObject(dc0, "Voltage", (double)data->realtime.battery_voltage);
    cJSON_AddNumberToObject(dc0, "Current", (double)data->realtime.battery_current);
    cJSON_AddItemToObject(dc, "0", dc0);
    cJSON_AddItemToObject(root, "Dc", dc);

    // "Load": { "State": 1, "I": 2.1 }
    cJSON *load = cJSON_CreateObject();
    cJSON_AddNumberToObject(load, "State", data->realtime.load_state ? 1 : 0);
    cJSON_AddNumberToObject(load, "I", (double)data->realtime.load_current);
    cJSON_AddItemToObject(root, "Load", load);

    // Victron States
    cJSON_AddNumberToObject(root, "State", map_victron_state(data));
    cJSON_AddNumberToObject(root, "ErrorCode", map_victron_error_code(data));
    cJSON_AddNumberToObject(root, "MppOperationMode", map_mpp_operation_mode(data));

    // "History": { "Daily": { "0": { ... } } }
    cJSON *history = cJSON_CreateObject();
    cJSON *daily = cJSON_CreateObject();
    cJSON *day0 = cJSON_CreateObject();
    cJSON_AddNumberToObject(day0, "Yield", (double)data->stats.generated_energy_today);
    cJSON_AddNumberToObject(day0, "MaxPvVoltage", (double)data->realtime.pv_voltage);
    cJSON_AddNumberToObject(day0, "MinBatteryVoltage", (double)data->stats.min_battery_voltage_today);
    cJSON_AddNumberToObject(day0, "MaxBatteryVoltage", (double)data->stats.max_battery_voltage_today);
    cJSON_AddItemToObject(daily, "0", day0);
    cJSON_AddItemToObject(history, "Daily", daily);
    cJSON_AddItemToObject(root, "History", history);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) return ESP_ERR_NO_MEM;

    char topic_buf[64];
    snprintf(topic_buf, sizeof(topic_buf), "%s/%u", base_topic ? base_topic : "solar_charger", unit_index);
    esp_mqtt_client_publish(s_mqtt_client, topic_buf, json_str, 0, 1, 0);
    free(json_str);

    /* =====================================================================
     * 2. Comprehensive Diagnostics Telemetry Topic
     * ===================================================================== */
    cJSON *diag = cJSON_CreateObject();
    cJSON_AddNumberToObject(diag, "slave_id", data->slave_id);
    cJSON_AddBoolToObject(diag, "online", data->is_online);
    cJSON_AddNumberToObject(diag, "pv_v", data->realtime.pv_voltage);
    cJSON_AddNumberToObject(diag, "pv_a", data->realtime.pv_current);
    cJSON_AddNumberToObject(diag, "pv_w", data->realtime.pv_power);
    cJSON_AddNumberToObject(diag, "bat_v", data->realtime.battery_voltage);
    cJSON_AddNumberToObject(diag, "bat_a", data->realtime.battery_current);
    cJSON_AddNumberToObject(diag, "bat_soc", data->realtime.battery_soc);
    cJSON_AddNumberToObject(diag, "bat_temp", data->realtime.battery_temperature);
    cJSON_AddNumberToObject(diag, "dev_temp", data->realtime.device_temperature);
    cJSON_AddBoolToObject(diag, "night", data->realtime.is_night);
    cJSON_AddNumberToObject(diag, "yield_today_kwh", data->stats.generated_energy_today);
    cJSON_AddNumberToObject(diag, "yield_total_kwh", data->stats.generated_energy_total);
    cJSON_AddNumberToObject(diag, "raw_status_chg", data->realtime.charge_status_raw);
    cJSON_AddNumberToObject(diag, "raw_status_bat", data->realtime.battery_status_raw);

    char *diag_str = cJSON_PrintUnformatted(diag);
    cJSON_Delete(diag);

    if (diag_str) {
        snprintf(topic_buf, sizeof(topic_buf), "epever/unit_%u/telemetry", data->slave_id);
        esp_mqtt_client_publish(s_mqtt_client, topic_buf, diag_str, 0, 0, 0);
        free(diag_str);
    }

    return ESP_OK;
}

static void venus_mqtt_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Venus MQTT publisher task started");

    while (1) {
        if (s_is_connected) {
            uint8_t count = g_config.num_units;
            if (count == 0) count = 1;

            epever_data_lock();
            for (uint8_t i = 0; i < count; i++) {
                uint8_t slave_id = g_config.slave_ids[i];
                if (slave_id == 0) continue;

                epever_unit_data_t *u = epever_data_get_unit(slave_id);
                if (u && u->is_online) {
                    venus_mqtt_publish_unit(i, u, g_config.mqtt_base_topic);
                }
            }
            epever_data_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(g_config.poll_interval_ms));
    }
}

void venus_mqtt_start_task(void)
{
    xTaskCreate(venus_mqtt_task, "mqtt_pub_task", 4096, NULL, 4, NULL);
}
