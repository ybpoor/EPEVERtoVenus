#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_config.h"
#include "epever_registers.h"
#include "epever_data.h"
#include "modbus_rtu.h"
#include "modbus_epever.h"

static const char *TAG = "modbus_epever";
extern app_config_t g_config;

static inline float decode_u32_100(uint16_t low, uint16_t high)
{
    uint32_t raw = ((uint32_t)high << 16) | (uint32_t)low;
    return (float)raw / 100.0f;
}

static inline float decode_u16_100(uint16_t val)
{
    return (float)val / 100.0f;
}

static inline float decode_s16_100(uint16_t val)
{
    return (float)((int16_t)val) / 100.0f;
}

esp_err_t modbus_epever_init(void)
{
    epever_data_init();
    return modbus_rtu_init(EPEVER_UART_NUM, EPEVER_PIN_TX, EPEVER_PIN_RX, EPEVER_PIN_RTS, EPEVER_BAUD_RATE);
}

esp_err_t modbus_epever_poll_unit(uint8_t slave_id, bool poll_infrequent)
{
    epever_unit_data_t *unit = epever_data_get_unit(slave_id);
    if (!unit) return ESP_ERR_NO_MEM;

    uint16_t regs[20];
    esp_err_t err;

    /* ---------------------------------------------------------------------
     * 1. Read Real-Time PV Telemetry (0x3100..0x3103: 4 registers)
     * --------------------------------------------------------------------- */
    err = modbus_read_input_registers(slave_id, REG_PV_VOLTAGE, 4, regs);
    if (err != ESP_OK) {
        unit->error_count++;
        unit->is_online = false;
        return err;
    }

    epever_data_lock();
    unit->realtime.pv_voltage = decode_u16_100(regs[0]);
    unit->realtime.pv_current = decode_u16_100(regs[1]);
    unit->realtime.pv_power   = decode_u32_100(regs[2], regs[3]);
    epever_data_unlock();

    /* ---------------------------------------------------------------------
     * 2. Read Load & Temperatures (0x310C..0x3111: 6 registers)
     * --------------------------------------------------------------------- */
    err = modbus_read_input_registers(slave_id, REG_LOAD_VOLTAGE, 6, regs);
    if (err == ESP_OK) {
        epever_data_lock();
        unit->realtime.load_voltage        = decode_u16_100(regs[0]);
        unit->realtime.load_current        = decode_u16_100(regs[1]);
        unit->realtime.load_power          = decode_u32_100(regs[2], regs[3]);
        unit->realtime.battery_temperature = decode_s16_100(regs[4]);
        unit->realtime.device_temperature  = decode_s16_100(regs[5]);
        epever_data_unlock();
    }

    /* ---------------------------------------------------------------------
     * 3. Read SOC & Real Rated Voltage (0x311A..0x311D: 4 registers)
     * --------------------------------------------------------------------- */
    err = modbus_read_input_registers(slave_id, REG_BAT_SOC, 4, regs);
    if (err == ESP_OK) {
        epever_data_lock();
        unit->realtime.battery_soc           = regs[0];
        unit->realtime.battery_rated_voltage = decode_u16_100(regs[3]);
        epever_data_unlock();
    }

    /* ---------------------------------------------------------------------
     * 4. Read Equipment Status Bitfields (0x3200..0x3202: 3 registers)
     * --------------------------------------------------------------------- */
    err = modbus_read_input_registers(slave_id, REG_STATUS_BATTERY, 3, regs);
    if (err == ESP_OK) {
        epever_data_lock();
        unit->realtime.battery_status_raw   = regs[0];
        unit->realtime.charge_status_raw    = regs[1];
        unit->realtime.discharge_status_raw = regs[2];

        // Charging stage: D3-D2 (0=None, 1=Float, 2=Boost, 3=Equalize)
        unit->realtime.charging_stage = (regs[1] >> 2) & 0x03;

        // Load active: D0 of discharge status
        unit->realtime.load_state = (regs[2] & DISCHG_STATUS_RUNNING) ? true : false;

        // Fault flag
        unit->realtime.fault_active = (regs[1] & CHG_STATUS_FAULT) || (regs[2] & DISCHG_STATUS_FAULT);
        epever_data_unlock();
    }

    /* ---------------------------------------------------------------------
     * 5. Read Battery Real-Time V & I (0x331A..0x331C: 3 registers)
     * --------------------------------------------------------------------- */
    err = modbus_read_input_registers(slave_id, REG_BAT_VOLTAGE, 3, regs);
    if (err == ESP_OK) {
        epever_data_lock();
        unit->realtime.battery_voltage = decode_u16_100(regs[0]);
        unit->realtime.battery_current = decode_u32_100(regs[1], regs[2]);
        unit->realtime.battery_power   = unit->realtime.battery_voltage * unit->realtime.battery_current;
        epever_data_unlock();
    }

    /* ---------------------------------------------------------------------
     * 6. Read Energy Statistics (0x3302..0x3313: 18 registers)
     * --------------------------------------------------------------------- */
    err = modbus_read_input_registers(slave_id, REG_STAT_BAT_VOLT_MAX_TODAY, 18, regs);
    if (err == ESP_OK) {
        epever_data_lock();
        unit->stats.max_battery_voltage_today = decode_u16_100(regs[0]);
        unit->stats.min_battery_voltage_today = decode_u16_100(regs[1]);
        unit->stats.consumed_energy_today     = decode_u32_100(regs[2], regs[3]);
        unit->stats.consumed_energy_month     = decode_u32_100(regs[4], regs[5]);
        unit->stats.consumed_energy_year      = decode_u32_100(regs[6], regs[7]);
        unit->stats.consumed_energy_total     = decode_u32_100(regs[8], regs[9]);
        unit->stats.generated_energy_today    = decode_u32_100(regs[10], regs[11]);
        unit->stats.generated_energy_month    = decode_u32_100(regs[12], regs[13]);
        unit->stats.generated_energy_year     = decode_u32_100(regs[14], regs[15]);
        unit->stats.generated_energy_total    = decode_u32_100(regs[16], regs[17]);
        epever_data_unlock();
    }

    /* ---------------------------------------------------------------------
     * 7. Read Discrete Inputs (0x2000 & 0x200C)
     * --------------------------------------------------------------------- */
    uint8_t discrete_byte = 0;
    if (modbus_read_discrete_inputs(slave_id, REG_DI_OVER_TEMP, 1, &discrete_byte) == ESP_OK) {
        unit->realtime.is_over_temp = (discrete_byte & 0x01) ? true : false;
    }
    if (modbus_read_discrete_inputs(slave_id, REG_DI_DAY_NIGHT, 1, &discrete_byte) == ESP_OK) {
        unit->realtime.is_night = (discrete_byte & 0x01) ? true : false;
    }

    /* ---------------------------------------------------------------------
     * 8. Read Infrequent Configuration / Ratings (if requested)
     * --------------------------------------------------------------------- */
    if (poll_infrequent) {
        // Rated Parameters (0x3000..0x3007: 8 registers)
        if (modbus_read_input_registers(slave_id, REG_RATED_PV_VOLTAGE, 8, regs) == ESP_OK) {
            epever_data_lock();
            unit->rated.pv_rated_voltage      = decode_u16_100(regs[0]);
            unit->rated.pv_rated_current      = decode_u16_100(regs[1]);
            unit->rated.pv_rated_power        = decode_u32_100(regs[2], regs[3]);
            unit->rated.battery_rated_voltage = decode_u16_100(regs[4]);
            unit->rated.battery_rated_current = decode_u16_100(regs[5]);
            unit->rated.battery_rated_power   = decode_u32_100(regs[6], regs[7]);
            epever_data_unlock();
        }

        // Battery Settings (0x9000..0x900E: 15 registers)
        if (modbus_read_holding_registers(slave_id, REG_SET_BATTERY_TYPE, 15, regs) == ESP_OK) {
            epever_data_lock();
            unit->settings.battery_type          = regs[0];
            unit->settings.battery_capacity      = regs[1];
            unit->settings.temp_comp_coeff       = decode_u16_100(regs[2]);
            unit->settings.high_volt_disconnect  = decode_u16_100(regs[3]);
            unit->settings.charging_limit_volt   = decode_u16_100(regs[4]);
            unit->settings.over_volt_reconnect   = decode_u16_100(regs[5]);
            unit->settings.equalize_volt         = decode_u16_100(regs[6]);
            unit->settings.boost_volt            = decode_u16_100(regs[7]);
            unit->settings.float_volt            = decode_u16_100(regs[8]);
            unit->settings.boost_reconnect_volt  = decode_u16_100(regs[9]);
            unit->settings.low_volt_reconnect    = decode_u16_100(regs[10]);
            unit->settings.under_volt_warning    = decode_u16_100(regs[12]);
            unit->settings.low_volt_disconnect   = decode_u16_100(regs[13]);
            unit->settings.discharging_limit_volt= decode_u16_100(regs[14]);
            epever_data_unlock();
        }
    }

    unit->poll_count++;
    unit->is_online = true;
    unit->last_seen_ms = (uint32_t)(esp_timer_get_time() / 1000);

    return ESP_OK;
}

static void modbus_epever_task(void *pvParameters)
{
    ESP_LOGI(TAG, "EPEVER polling task started");
    uint32_t loop_counter = 0;

    while (1) {
        uint8_t count = g_config.num_units;
        if (count == 0) count = 1;

        bool poll_infrequent = (loop_counter % 30 == 0); // Every ~60s if 2s poll interval

        for (uint8_t i = 0; i < count; i++) {
            uint8_t slave_id = g_config.slave_ids[i];
            if (slave_id == 0) continue;

            esp_err_t err = modbus_epever_poll_unit(slave_id, poll_infrequent);
            if (err == ESP_OK) {
                epever_unit_data_t *u = epever_data_get_unit(slave_id);
                if (u && loop_counter % 5 == 0) {
                    ESP_LOGI(TAG, "[Unit %d] PV: %.1fV/%.1fW | Bat: %.2fV/%.1fA (SOC %d%%, Stage %d) | Yield: %.2fkWh",
                             slave_id, u->realtime.pv_voltage, u->realtime.pv_power,
                             u->realtime.battery_voltage, u->realtime.battery_current,
                             u->realtime.battery_soc, u->realtime.charging_stage,
                             u->stats.generated_energy_today);
                }
            } else {
                ESP_LOGW(TAG, "[Unit %d] Poll failed (%s)", slave_id, esp_err_to_name(err));
            }

            // Brief pause between multiple units on the RS485 bus
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        loop_counter++;
        uint32_t interval = g_config.poll_interval_ms;
        if (interval < 500) interval = DEFAULT_POLL_INTERVAL;
        vTaskDelay(pdMS_TO_TICKS(interval));
    }
}

void modbus_epever_start_task(void)
{
    xTaskCreate(modbus_epever_task, "modbus_task", 4096, NULL, 5, NULL);
}
