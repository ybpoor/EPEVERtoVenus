#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize UART for RS485 half-duplex Modbus communication.
 *
 * @param uart_num Hardware UART port (e.g. UART_NUM_1)
 * @param tx_pin GPIO for TX (DI)
 * @param rx_pin GPIO for RX (RO)
 * @param rts_pin GPIO for RTS (DE / /RE direction pin)
 * @param baud_rate Communication baud rate (e.g. 115200)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t modbus_rtu_init(uart_port_t uart_num, int tx_pin, int rx_pin, int rts_pin, int baud_rate);

/**
 * @brief Modbus Function 0x04 - Read Input Registers
 *
 * @param slave_id Target slave address (1-247)
 * @param start_addr Starting register address (e.g. 0x3100)
 * @param reg_count Number of 16-bit registers to read (1-125)
 * @param dest_regs Buffer to receive 16-bit register values in host endianness
 * @return esp_err_t ESP_OK on success, ESP_ERR_TIMEOUT or error code on failure
 */
esp_err_t modbus_read_input_registers(uint8_t slave_id, uint16_t start_addr, uint16_t reg_count, uint16_t *dest_regs);

/**
 * @brief Modbus Function 0x03 - Read Holding Registers
 *
 * @param slave_id Target slave address (1-247)
 * @param start_addr Starting register address (e.g. 0x9000)
 * @param reg_count Number of 16-bit registers to read (1-125)
 * @param dest_regs Buffer to receive 16-bit register values in host endianness
 * @return esp_err_t ESP_OK on success, ESP_ERR_TIMEOUT or error code on failure
 */
esp_err_t modbus_read_holding_registers(uint8_t slave_id, uint16_t start_addr, uint16_t reg_count, uint16_t *dest_regs);

/**
 * @brief Modbus Function 0x02 - Read Discrete Inputs
 *
 * @param slave_id Target slave address (1-247)
 * @param start_addr Starting discrete input address (e.g. 0x2000)
 * @param bit_count Number of discrete inputs to read
 * @param dest_bits Buffer receiving packed bytes of discrete inputs
 * @return esp_err_t ESP_OK on success, ESP_ERR_TIMEOUT or error code on failure
 */
esp_err_t modbus_read_discrete_inputs(uint8_t slave_id, uint16_t start_addr, uint16_t bit_count, uint8_t *dest_bits);

/**
 * @brief Standard Modbus CRC-16 computation.
 *
 * @param buffer Data buffer
 * @param length Length of buffer in bytes
 * @return uint16_t CRC-16 value (Low byte in bits 0-7, High byte in bits 8-15)
 */
uint16_t modbus_crc16(const uint8_t *buffer, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_RTU_H
