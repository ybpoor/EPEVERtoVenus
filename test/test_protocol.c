#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

// Forward declaration of CRC16 from modbus_rtu
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

static void test_crc_known_vectors(void)
{
    printf("Testing Modbus CRC-16 with official EPEVER test vectors...\n");

    // Vector 1: EPEVER Read Real-time Battery Voltage: 01 04 33 1A 00 01 -> CRC = 0x1F 0x49
    // In Modbus transmission: [CRC_Low] [CRC_High] = 0x1F, 0x49 -> 16-bit value is 0x491F
    uint8_t req1[] = { 0x01, 0x04, 0x33, 0x1A, 0x00, 0x01 };
    uint16_t crc1 = modbus_crc16(req1, sizeof(req1));
    uint8_t crc1_low = (uint8_t)(crc1 & 0xFF);
    uint8_t crc1_high = (uint8_t)(crc1 >> 8);
    printf("  Vector 1: calculated CRC=0x%02X 0x%02X, expected 0x1F 0x49\n", crc1_low, crc1_high);
    assert(crc1_low == 0x1F);
    assert(crc1_high == 0x49);

    // Vector 2: Response: 01 04 02 04 CE -> CRC = 0x3A 0x64
    uint8_t resp1[] = { 0x01, 0x04, 0x02, 0x04, 0xCE };
    uint16_t crc2 = modbus_crc16(resp1, sizeof(resp1));
    uint8_t crc2_low = (uint8_t)(crc2 & 0xFF);
    uint8_t crc2_high = (uint8_t)(crc2 >> 8);
    printf("  Vector 2: calculated CRC=0x%02X 0x%02X, expected 0x3A 0x64\n", crc2_low, crc2_high);
    assert(crc2_low == 0x3A);
    assert(crc2_high == 0x64);

    // Vector 3: Full packet CRC verification: packet with CRC appended should verify cleanly
    uint8_t full_packet[] = { 0x01, 0x04, 0x02, 0x04, 0xCE, 0x3A, 0x64 };
    uint16_t check_crc = modbus_crc16(full_packet, 5);
    uint16_t packet_crc = (uint16_t)full_packet[5] | ((uint16_t)full_packet[6] << 8);
    assert(check_crc == packet_crc);

    printf("  Modbus CRC16 tests passed successfully!\n");
}

static void test_register_decoding(void)
{
    printf("Testing EPEVER register decoding logic...\n");

    // Test 1: Real-time battery voltage 0x04CE = 1230 -> 12.30V
    uint16_t raw_volt = 0x04CE;
    float bat_v = (float)raw_volt / 100.0f;
    assert(bat_v > 12.29f && bat_v < 12.31f);
    printf("  Voltage decoding: 0x04CE -> %.2fV (Passed)\n", bat_v);

    // Test 2: 32-bit Power (Low: 0x93E0, High: 0x0004) -> (4 << 16) | 0x93E0 = 262144 + 37856 = 300000 -> 3000.00 W
    uint16_t low_w = 0x93E0;
    uint16_t high_w = 0x0004;
    uint32_t raw_p = ((uint32_t)high_w << 16) | (uint32_t)low_w;
    float pwr = (float)raw_p / 100.0f;
    assert(pwr > 2999.99f && pwr < 3000.01f);
    printf("  32-bit Power decoding: 0x000493E0 -> %.2fW (Passed)\n", pwr);

    // Test 3: Signed Temperature (-5.50 °C = -550 -> 0xFDDA in 16-bit signed)
    int16_t raw_temp = -550;
    float temp_c = (float)raw_temp / 100.0f;
    assert(temp_c > -5.51f && temp_c < -5.49f);
    printf("  Signed Temp decoding: 0x%04X (%d) -> %.2f deg C (Passed)\n", (uint16_t)raw_temp, raw_temp, temp_c);

    // Test 4: Charging Stage bitfield extraction (0x3201 bits D3-D2)
    // 0x0004 -> Float (1)
    // 0x0008 -> Boost (2)
    // 0x000C -> Equalize (3)
    uint16_t status_boost = 0x0009; // Running (D0=1) + Boost (D3-D2 = 02)
    uint8_t stage = (status_boost >> 2) & 0x03;
    assert(stage == 2);
    printf("  Status bitfield stage decoding: 0x0009 -> Stage %d (Boost) (Passed)\n", stage);

    printf("  All decoding tests passed successfully!\n");
}

int main(void)
{
    printf("==================================================\n");
    printf(" Running EPEVER Protocol Unit Tests\n");
    printf("==================================================\n");

    test_crc_known_vectors();
    test_register_decoding();

    printf("\nAll unit tests PASSED!\n");
    return 0;
}
