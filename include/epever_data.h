#ifndef EPEVER_DATA_H
#define EPEVER_DATA_H

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // PV Array
    float pv_voltage;                   // V
    float pv_current;                   // A
    float pv_power;                     // W

    // Battery
    float battery_voltage;               // V
    float battery_current;               // A (positive = charging)
    float battery_power;                 // W
    uint16_t battery_soc;                // %
    float battery_rated_voltage;         // V (e.g. 12, 24, 36, 48)
    float battery_temperature;           // °C

    // Load Output
    float load_voltage;                  // V
    float load_current;                  // A
    float load_power;                    // W
    bool load_state;                     // true = running/on

    // Device & Environment
    float device_temperature;            // °C heatsink
    bool is_night;                       // true = night
    bool is_over_temp;                   // true = controller over temp

    // Raw & Decoded Status
    uint16_t battery_status_raw;         // 0x3200
    uint16_t charge_status_raw;          // 0x3201
    uint16_t discharge_status_raw;       // 0x3202

    uint8_t charging_stage;              // 0=None, 1=Float, 2=Boost, 3=Equalize
    bool fault_active;                   // true if fault flag is set
} epever_realtime_t;

typedef struct {
    float max_battery_voltage_today;     // V
    float min_battery_voltage_today;     // V
    float consumed_energy_today;         // kWh
    float consumed_energy_month;         // kWh
    float consumed_energy_year;          // kWh
    float consumed_energy_total;         // kWh
    float generated_energy_today;        // kWh (Victron Yield User)
    float generated_energy_month;        // kWh
    float generated_energy_year;         // kWh
    float generated_energy_total;        // kWh (Victron Yield System)
} epever_statistics_t;

typedef struct {
    float pv_rated_voltage;              // V
    float pv_rated_current;              // A
    float pv_rated_power;                // W
    float battery_rated_voltage;         // V
    float battery_rated_current;         // A (e.g. 50A for 5415AN)
    float battery_rated_power;           // W
    float load_rated_current;            // A
} epever_rated_t;

typedef struct {
    uint16_t battery_type;               // 0=User, 1=Sealed, 2=GEL, 3=Flooded
    uint16_t battery_capacity;           // Ah
    float temp_comp_coeff;               // mV/°C/2V
    float high_volt_disconnect;          // V
    float charging_limit_volt;           // V
    float over_volt_reconnect;           // V
    float equalize_volt;                 // V
    float boost_volt;                    // V
    float float_volt;                    // V
    float boost_reconnect_volt;          // V
    float low_volt_reconnect;            // V
    float under_volt_warning;            // V
    float low_volt_disconnect;           // V
    float discharging_limit_volt;        // V
    uint16_t boost_duration_min;         // minutes
    uint16_t equalize_duration_min;      // minutes
} epever_settings_t;

typedef struct {
    uint8_t slave_id;
    bool is_online;
    uint32_t last_seen_ms;
    uint32_t poll_count;
    uint32_t error_count;

    epever_realtime_t realtime;
    epever_statistics_t stats;
    epever_rated_t rated;
    epever_settings_t settings;
} epever_unit_data_t;

/**
 * @brief Initialize the global data store.
 */
void epever_data_init(void);

/**
 * @brief Thread-safe lock of unit data store.
 */
void epever_data_lock(void);

/**
 * @brief Thread-safe unlock of unit data store.
 */
void epever_data_unlock(void);

/**
 * @brief Get unit data struct pointer by Modbus slave ID.
 */
epever_unit_data_t* epever_data_get_unit(uint8_t slave_id);

/**
 * @brief Get unit data struct pointer by array index (0 .. MAX_EPEVER_UNITS-1).
 */
epever_unit_data_t* epever_data_get_by_index(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif // EPEVER_DATA_H
