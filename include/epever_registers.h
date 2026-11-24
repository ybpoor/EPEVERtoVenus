#ifndef EPEVER_REGISTERS_H
#define EPEVER_REGISTERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * 1. Discrete Inputs (Read Discrete Inputs - Function Code 0x02)
 * ========================================================================= */
#define REG_DI_OVER_TEMP            0x2000  // 1: Over-temperature inside device, 0: Normal
#define REG_DI_DAY_NIGHT            0x200C  // 1: Night, 0: Day

/* =========================================================================
 * 2. Rated Parameters (Read Input Registers - Function Code 0x04)
 * ========================================================================= */
#define REG_RATED_PV_VOLTAGE        0x3000  // PV array rated voltage (V * 100)
#define REG_RATED_PV_CURRENT        0x3001  // PV array rated current (A * 100)
#define REG_RATED_PV_POWER_L        0x3002  // PV array rated power Low (W * 100)
#define REG_RATED_PV_POWER_H        0x3003  // PV array rated power High (W * 100)
#define REG_RATED_BAT_VOLTAGE       0x3004  // Battery rated voltage (V * 100)
#define REG_RATED_BAT_CURRENT       0x3005  // Battery rated current (A * 100)
#define REG_RATED_BAT_POWER_L       0x3006  // Battery rated power Low (W * 100)
#define REG_RATED_BAT_POWER_H       0x3007  // Battery rated power High (W * 100)
#define REG_RATED_LOAD_VOLTAGE      0x300D  // Rated load voltage (V * 100)
#define REG_RATED_LOAD_CURRENT      0x300E  // Rated load current (A * 100)
#define REG_RATED_LOAD_POWER_L      0x300F  // Rated load power Low (W * 100)
#define REG_RATED_LOAD_POWER_H      0x3010  // Rated load power High (W * 100)

/* =========================================================================
 * 3. Real-Time Telemetry (Read Input Registers - Function Code 0x04)
 * ========================================================================= */
#define REG_PV_VOLTAGE              0x3100  // PV input voltage (V * 100)
#define REG_PV_CURRENT              0x3101  // PV input current (A * 100)
#define REG_PV_POWER_L              0x3102  // PV input power Low (W * 100)
#define REG_PV_POWER_H              0x3103  // PV input power High (W * 100)

#define REG_LOAD_VOLTAGE            0x310C  // Load port voltage (V * 100)
#define REG_LOAD_CURRENT            0x310D  // Load port current (A * 100)
#define REG_LOAD_POWER_L            0x310E  // Load port power Low (W * 100)
#define REG_LOAD_POWER_H            0x310F  // Load port power High (W * 100)

#define REG_BAT_TEMPERATURE        0x3110  // Battery temp (signed int16, deg C * 100)
#define REG_DEV_TEMPERATURE        0x3111  // Heatsink/Device temp (signed int16, deg C * 100)

#define REG_BAT_SOC                 0x311A  // Battery SOC (%) (1)
#define REG_BAT_REAL_RATED_VOLT     0x311D  // System rated voltage (1200=12V, 2400=24V, 4800=48V)

/* =========================================================================
 * 4. Equipment Status Bitfields (Read Input Registers - Function Code 0x04)
 * ========================================================================= */
#define REG_STATUS_BATTERY          0x3200  // Battery status bitfield
#define REG_STATUS_CHARGING         0x3201  // Charging equipment status bitfield
#define REG_STATUS_DISCHARGING      0x3202  // Discharging / Load status bitfield

/* 0x3200 Battery Status bit masks */
#define BAT_STATUS_VOLT_MASK        0x000F  // D3-D0: 0=Normal, 1=OverVolt, 2=UnderVolt, 3=OverDischarge, 4=Fault
#define BAT_STATUS_TEMP_MASK        0x0070  // D7-D4: 0=Normal, 1=OverTemp, 2=LowTemp
#define BAT_STATUS_RESISTANCE_ERR   0x0100  // D8: Inner resistance abnormal
#define BAT_STATUS_WRONG_RATED_V    0x8000  // D15: Wrong rated voltage identification

/* 0x3201 Charging Equipment Status bit masks */
#define CHG_STATUS_RUNNING          0x0001  // D0: 1=Running, 0=Standby
#define CHG_STATUS_FAULT            0x0002  // D1: 1=Fault, 0=Normal
#define CHG_STATUS_STAGE_MASK       0x000C  // D3-D2: 00=No charging, 01=Float, 02=Boost, 03=Equalization
#define CHG_STATUS_STAGE_NONE       0x0000
#define CHG_STATUS_STAGE_FLOAT      0x0004
#define CHG_STATUS_STAGE_BOOST      0x0008
#define CHG_STATUS_STAGE_EQUALIZE   0x000C
#define CHG_STATUS_PV_SHORT         0x0010  // D4: PV input short circuit
#define CHG_STATUS_LOAD_SHORT       0x0100  // D8: Load short circuit
#define CHG_STATUS_LOAD_OVERCURRENT 0x0200  // D9: Load over current
#define CHG_STATUS_INPUT_OVERCURRENT 0x0400 // D10: Input over current
#define CHG_STATUS_ANTIREV_SHORT    0x0800  // D11: Anti-reverse MOSFET short
#define CHG_STATUS_MOSFET_OPEN      0x1000  // D12: Charging/Anti-reverse MOSFET open
#define CHG_STATUS_CHG_MOSFET_SHORT 0x2000  // D13: Charging MOSFET short circuit
#define CHG_STATUS_INPUT_VOLT_MASK  0xC000  // D15-D14: 00=Normal, 01=No input, 02=Higher input, 03=Input error

/* 0x3202 Discharging / Load Equipment Status bit masks */
#define DISCHG_STATUS_RUNNING       0x0001  // D0: 1=Running, 0=Standby
#define DISCHG_STATUS_FAULT         0x0002  // D1: 1=Fault, 0=Normal
#define DISCHG_STATUS_OVER_VOLT     0x0010  // D4: Output over voltage
#define DISCHG_STATUS_BOOST_OVER_V  0x0020  // D5: Boost over voltage
#define DISCHG_STATUS_HIGH_V_SHORT  0x0040  // D6: High voltage side short circuit
#define DISCHG_STATUS_INPUT_OVER_V  0x0080  // D7: Input over voltage
#define DISCHG_STATUS_OUTPUT_V_ABN  0x0100  // D8: Output voltage abnormal
#define DISCHG_STATUS_UNABLE_STOP   0x0200  // D9: Unable to stop discharging
#define DISCHG_STATUS_UNABLE_DISCHG 0x0400  // D10: Unable to discharge
#define DISCHG_STATUS_SHORT_CIRCUIT 0x0800  // D11: Short circuit
#define DISCHG_STATUS_PWR_LEVEL     0x3000  // D13-D12: 00=Light, 01=Moderate, 02=Rated, 03=Overload

/* =========================================================================
 * 5. Energy Statistics (Read Input Registers - Function Code 0x04)
 * ========================================================================= */
#define REG_STAT_BAT_VOLT_MAX_TODAY 0x3302  // Max battery voltage today (V * 100)
#define REG_STAT_BAT_VOLT_MIN_TODAY 0x3303  // Min battery voltage today (V * 100)
#define REG_STAT_CONSUMED_TODAY_L   0x3304  // Consumed energy today L (kWh * 100)
#define REG_STAT_CONSUMED_TODAY_H   0x3305  // Consumed energy today H (kWh * 100)
#define REG_STAT_CONSUMED_MONTH_L   0x3306  // Consumed energy this month L (kWh * 100)
#define REG_STAT_CONSUMED_MONTH_H   0x3307  // Consumed energy this month H (kWh * 100)
#define REG_STAT_CONSUMED_YEAR_L    0x3308  // Consumed energy this year L (kWh * 100)
#define REG_STAT_CONSUMED_YEAR_H    0x3309  // Consumed energy this year H (kWh * 100)
#define REG_STAT_CONSUMED_TOTAL_L   0x330A  // Total consumed energy L (kWh * 100)
#define REG_STAT_CONSUMED_TOTAL_H   0x330B  // Total consumed energy H (kWh * 100)
#define REG_STAT_GENERATED_TODAY_L  0x330C  // Generated energy today L (kWh * 100) -> Victron /Yield/User
#define REG_STAT_GENERATED_TODAY_H  0x330D  // Generated energy today H (kWh * 100)
#define REG_STAT_GENERATED_MONTH_L  0x330E  // Generated energy this month L (kWh * 100)
#define REG_STAT_GENERATED_MONTH_H  0x330F  // Generated energy this month H (kWh * 100)
#define REG_STAT_GENERATED_YEAR_L   0x3310  // Generated energy this year L (kWh * 100)
#define REG_STAT_GENERATED_YEAR_H   0x3311  // Generated energy this year H (kWh * 100)
#define REG_STAT_GENERATED_TOTAL_L  0x3312  // Total generated energy L (kWh * 100) -> Victron /Yield/System
#define REG_STAT_GENERATED_TOTAL_H  0x3313  // Total generated energy H (kWh * 100)

#define REG_BAT_VOLTAGE             0x331A  // Real-time battery voltage (V * 100)
#define REG_BAT_CURRENT_L           0x331B  // Battery current Low (A * 100)
#define REG_BAT_CURRENT_H           0x331C  // Battery current High (A * 100)

/* =========================================================================
 * 6. Battery Profile & Settings (Read Holding Registers - Function Code 0x03)
 * ========================================================================= */
#define REG_SET_BATTERY_TYPE        0x9000  // 0=User, 1=Sealed, 2=GEL, 3=Flooded
#define REG_SET_BATTERY_CAPACITY    0x9001  // Rated capacity (Ah)
#define REG_SET_TEMP_COMP_COEFF     0x9002  // Temp compensation coeff (0.01 mV/deg C/2V)
#define REG_SET_HIGH_VOLT_DISCONN   0x9003  // Over voltage disconnect (V * 100)
#define REG_SET_CHARGING_LIMIT_V    0x9004  // Charging limit voltage (V * 100)
#define REG_SET_OVER_VOLT_RECONN    0x9005  // Over voltage reconnect (V * 100)
#define REG_SET_EQUALIZE_VOLT       0x9006  // Equalize charging voltage (V * 100)
#define REG_SET_BOOST_VOLT          0x9007  // Boost charging voltage (V * 100)
#define REG_SET_FLOAT_VOLT          0x9008  // Float charging voltage (V * 100)
#define REG_SET_BOOST_RECONN_VOLT   0x9009  // Boost reconnect charging voltage (V * 100)
#define REG_SET_LOW_VOLT_RECONN     0x900A  // Low voltage reconnect (V * 100)
#define REG_SET_UNDER_VOLT_RECOVER  0x900B  // Under voltage recover (V * 100)
#define REG_SET_UNDER_VOLT_WARN     0x900C  // Under voltage warning (V * 100)
#define REG_SET_LOW_VOLT_DISCONN    0x900D  // Low voltage disconnect (V * 100)
#define REG_SET_DISCHARGING_LIMIT_V 0x900E  // Discharging limit voltage (V * 100)
#define REG_SET_RTC_SEC_MIN         0x9013  // D7-0 Sec, D15-8 Min
#define REG_SET_RTC_HOUR_DAY        0x9014  // D7-0 Hour, D15-8 Day
#define REG_SET_RTC_MONTH_YEAR      0x9015  // D7-0 Month, D15-8 Year
#define REG_SET_BOOST_DURATION      0x906C  // Boost duration (minutes)
#define REG_SET_EQUALIZE_DURATION   0x906D  // Equalize duration (minutes)
#define REG_SET_DISCHARGE_LIMIT_PCT 0x906E  // Discharge percentage limit (%)
#define REG_SET_CHARGE_LIMIT_PCT    0x906F  // Charge percentage limit (%)

#ifdef __cplusplus
}
#endif

#endif // EPEVER_REGISTERS_H
