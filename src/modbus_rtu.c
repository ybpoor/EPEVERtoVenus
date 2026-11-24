#include <string.h>
#include "esp_log.h"
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "modbus_rtu.h"

static const char *TAG = "modbus_rtu";
static uart_port_t s_uart_num = UART_NUM_1;
static SemaphoreHandle_t s_bus_mutex = NULL;

#define MODBUS_TIMEOUT_MS   250
#define MODBUS_MAX_RETRIES  3

uint16_t modbus_crc16(const uint8_t *buffer, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t pos = 0; pos < length; pos++) {
        crc ^= (uint16_t)buffer[pos];
        for (int i = 8; i != 0; i--) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

esp_err_t modbus_rtu_init(uart_port_t uart_num, int tx_pin, int rx_pin, int rts_pin, int baud_rate)
{
    s_uart_num = uart_num;

    if (!s_bus_mutex) {
        s_bus_mutex = xSemaphoreCreateMutex();
    }

    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .source_clk = UART_SCLK_DEFAULT,
#endif
    };

    ESP_LOGI(TAG, "Initializing UART%d on TX=%d, RX=%d, RTS=%d at %d baud",
             uart_num, tx_pin, rx_pin, rts_pin, baud_rate);

    esp_err_t err = uart_param_config(uart_num, &uart_config);
    if (err != ESP_OK) return err;

    err = uart_set_pin(uart_num, tx_pin, rx_pin, rts_pin, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;

    // Install driver with RX and TX ring buffers
    err = uart_driver_install(uart_num, 512, 512, 0, NULL, 0);
    if (err != ESP_OK) return err;

    // Set RS485 half-duplex mode: hardware automatically controls RTS pin during TX
    err = uart_set_mode(uart_num, UART_MODE_RS485_HALF_DUPLEX);
    if (err != ESP_OK) return err;

    return ESP_OK;
}

static esp_err_t modbus_transceive(const uint8_t *req, size_t req_len, uint8_t *resp, size_t max_resp_len, size_t *actual_resp_len)
{
    if (xSemaphoreTake(s_bus_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // Clear any residual data in RX FIFO
    uart_flush_input(s_uart_num);

    // Send request frame
    int written = uart_write_bytes(s_uart_num, (const char *)req, req_len);
    if (written < 0 || (size_t)written != req_len) {
        xSemaphoreGive(s_bus_mutex);
        return ESP_FAIL;
    }

    // Wait until all bytes are sent out of FIFO and line goes back to receive mode
    uart_wait_tx_done(s_uart_num, pdMS_TO_TICKS(50));

    // Read response
    int len = uart_read_bytes(s_uart_num, resp, max_resp_len, pdMS_TO_TICKS(MODBUS_TIMEOUT_MS));

    // Inter-frame silent interval (minimum 3.5 character times, ~5ms buffer)
    vTaskDelay(pdMS_TO_TICKS(10));

    xSemaphoreGive(s_bus_mutex);

    if (len <= 0) {
        return ESP_ERR_TIMEOUT;
    }

    *actual_resp_len = (size_t)len;
    return ESP_OK;
}

static esp_err_t modbus_read_registers_cmd(uint8_t slave_id, uint8_t func_code, uint16_t start_addr, uint16_t reg_count, uint16_t *dest_regs)
{
    if (!dest_regs || reg_count == 0 || reg_count > 125) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t req[8];
    req[0] = slave_id;
    req[1] = func_code;
    req[2] = (uint8_t)(start_addr >> 8);
    req[3] = (uint8_t)(start_addr & 0xFF);
    req[4] = (uint8_t)(reg_count >> 8);
    req[5] = (uint8_t)(reg_count & 0xFF);

    uint16_t crc = modbus_crc16(req, 6);
    req[6] = (uint8_t)(crc & 0xFF);
    req[7] = (uint8_t)(crc >> 8);

    size_t expected_bytes = 5 + (reg_count * 2);
    uint8_t resp[260];
    size_t rx_len = 0;

    for (int retry = 0; retry < MODBUS_MAX_RETRIES; retry++) {
        esp_err_t err = modbus_transceive(req, 8, resp, sizeof(resp), &rx_len);
        if (err == ESP_OK && rx_len >= expected_bytes) {
            // Verify address and function code
            if (resp[0] != slave_id) {
                ESP_LOGD(TAG, "Slave ID mismatch (expected %d, got %d)", slave_id, resp[0]);
                continue;
            }
            if (resp[1] != func_code) {
                if (resp[1] == (func_code | 0x80)) {
                    ESP_LOGW(TAG, "Modbus exception response: code 0x%02X from slave %d", resp[2], slave_id);
                    return ESP_ERR_INVALID_RESPONSE;
                }
                continue;
            }
            if (resp[2] != (reg_count * 2)) {
                ESP_LOGD(TAG, "Byte count mismatch");
                continue;
            }

            // Check CRC
            uint16_t rx_crc = (uint16_t)resp[rx_len - 2] | ((uint16_t)resp[rx_len - 1] << 8);
            uint16_t calc_crc = modbus_crc16(resp, rx_len - 2);
            if (rx_crc != calc_crc) {
                ESP_LOGW(TAG, "CRC error on response from slave %d", slave_id);
                continue;
            }

            // Unpack 16-bit registers (Big-Endian to Host)
            for (uint16_t i = 0; i < reg_count; i++) {
                dest_regs[i] = ((uint16_t)resp[3 + (i * 2)] << 8) | (uint16_t)resp[4 + (i * 2)];
            }
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(15));
    }

    return ESP_ERR_TIMEOUT;
}

esp_err_t modbus_read_input_registers(uint8_t slave_id, uint16_t start_addr, uint16_t reg_count, uint16_t *dest_regs)
{
    return modbus_read_registers_cmd(slave_id, 0x04, start_addr, reg_count, dest_regs);
}

esp_err_t modbus_read_holding_registers(uint8_t slave_id, uint16_t start_addr, uint16_t reg_count, uint16_t *dest_regs)
{
    return modbus_read_registers_cmd(slave_id, 0x03, start_addr, reg_count, dest_regs);
}

esp_err_t modbus_read_discrete_inputs(uint8_t slave_id, uint16_t start_addr, uint16_t bit_count, uint8_t *dest_bits)
{
    if (!dest_bits || bit_count == 0 || bit_count > 2000) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t req[8];
    req[0] = slave_id;
    req[1] = 0x02; // Read Discrete Inputs
    req[2] = (uint8_t)(start_addr >> 8);
    req[3] = (uint8_t)(start_addr & 0xFF);
    req[4] = (uint8_t)(bit_count >> 8);
    req[5] = (uint8_t)(bit_count & 0xFF);

    uint16_t crc = modbus_crc16(req, 6);
    req[6] = (uint8_t)(crc & 0xFF);
    req[7] = (uint8_t)(crc >> 8);

    uint8_t expected_data_bytes = (uint8_t)((bit_count + 7) / 8);
    size_t expected_bytes = 5 + expected_data_bytes;
    uint8_t resp[64];
    size_t rx_len = 0;

    for (int retry = 0; retry < MODBUS_MAX_RETRIES; retry++) {
        esp_err_t err = modbus_transceive(req, 8, resp, sizeof(resp), &rx_len);
        if (err == ESP_OK && rx_len >= expected_bytes) {
            if (resp[0] != slave_id || resp[1] != 0x02) continue;
            uint16_t rx_crc = (uint16_t)resp[rx_len - 2] | ((uint16_t)resp[rx_len - 1] << 8);
            if (rx_crc != modbus_crc16(resp, rx_len - 2)) continue;

            memcpy(dest_bits, &resp[3], expected_data_bytes);
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(15));
    }

    return ESP_ERR_TIMEOUT;
}
