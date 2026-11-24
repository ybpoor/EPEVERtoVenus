# ESP32-C3 EPEVER to Venus OS Gateway

A high-performance, modular **C application** for the **ESP32-C3 SuperMini** board that polls **all real-time telemetry, equipment status, daily/monthly/annual energy statistics, rated parameters, and battery settings** from **one or more EPEVER Tracer solar charge controllers** (such as the Tracer 5415AN) over an RS485 multi-drop bus and forwards the data to a **Venus OS MQTT server**.

Designed for seamless integration with Venus OS using [dbus-mqtt-solar-charger](https://github.com/mr-manuel/venus-os_dbus-mqtt-solar-charger), registering native Solar Charger devices on the Victron D-Bus with full Remote Console, VRM Portal, and Touch GUI display.

---

## Key Features

- **Multi-Unit RS485 Cascading:** Polls multiple EPEVER controllers (e.g. Unit 1, Unit 2, Unit 3...) connected in a daisy chain via their dual RJ45 ports. Each unit publishes to an independent Venus OS topic (`solar_charger/0`, `solar_charger/1`, etc.).
- **Built-in Captive Portal Web Setup:** If not configured or if the SuperMini onboard **Boot button (GPIO 9)** is held for 3 seconds, the gateway starts a SoftAP (`EPEVER-Gateway-XXXX`) with an embedded mobile-friendly captive portal for zero-cable configuration of Wi-Fi credentials, MQTT host/port, and active Modbus slave IDs.
- **NVS Persistent Storage:** Saves all configurations to ESP32 Non-Volatile Storage (flash memory).
- **3.3V RS485 Transceiver Support:** Directly compatible with 3.3V transceivers (such as SP3485, MAX3485, or auto-flow modules) without needing level shifters.
- **Exhaustive Data Collection:** Extracts PV voltage/current/power, battery voltage/current/SOC/temperatures, load metrics, 3-stage charge state (Bulk, Absorption, Float, Equalize), Victron error codes, daily/monthly/yearly energy yields, and controller diagnostics.
- **Dual MQTT Publishing:**
  - Standard Venus OS D-Bus JSON (`solar_charger/<unit_index>`)
  - Full diagnostic telemetry topic (`epever/unit_<slave_id>/telemetry`)

---

## Hardware Pinout & Wiring

### ESP32-C3 SuperMini Pinout
```
         +-------------+
    5V --|             |-- GPIO 5   ---> RS485 Transceiver DI (TX)
   GND --|             |-- GPIO 6   ---> RS485 Transceiver DE & /RE
  3.3V --|   ESP32-C3  |-- GPIO 7
GPIO 0 --|  SuperMini  |-- GPIO 8   ---> Onboard Blue LED (Heartbeat & Status)
GPIO 1 --|             |-- GPIO 9   (Boot Button: hold 3s for Web Setup)
GPIO 2 --|             |-- GPIO 10
GPIO 3 --|             |-- GPIO 20  (USB-CDC / UART0 RX)
GPIO 4 --|             |-- GPIO 21  (USB-CDC / UART0 TX)
         +-------------+
            USB-C Port
```

### Transceiver & EPEVER RJ45 Connections

| SuperMini Pin | 3.3V Transceiver Pin | EPEVER RJ45 Pin | Description |
|---|---|---|---|
| **3.3V** | VCC | - | 3.3V power to SP3485 |
| **GND** | GND | Pin 7 & 8 (GND) | Common ground |
| **GPIO 4** | RO (Receiver Output) | - | UART1 RX |
| **GPIO 5** | DI (Driver Input) | - | UART1 TX |
| **GPIO 6** | DE & /RE | - | Hardware RTS Direction Control |
| - | A+ | Pin 5 & 6 (RS485-A) | Differential Data + |
| - | B- | Pin 3 & 4 (RS485-B) | Differential Data - |
| **5V** | - | Pin 1 & 2 (+5VDC) | Power from Tracer (add 470µF-1000µF buffer cap) |

> [!WARNING]
> **Power Supply Note:** The EPEVER Tracer RJ45 5V pin is rated for 200mA maximum. During Wi-Fi transmission bursts, the ESP32-C3 can briefly draw up to 300mA. To prevent brownouts, solder a **470µF to 1000µF low-ESR electrolytic capacitor** across the 5V and GND pins of the SuperMini, or power the ESP32-C3 via a small 12V/24V-to-5V DC-DC buck converter connected to your battery bank.

### Cascading Multiple EPEVER Units

The EPEVER Tracer 5415AN has two RJ45 ports connected internally in parallel.
1. Connect the ESP32-C3 transceiver to Port 1 of the first controller.
2. Run a standard straight-through RJ45 patch cable from Port 2 of the first controller to Port 1 of the second controller.
3. Configure each controller with a unique Modbus Slave ID (e.g. Unit 1 = ID 1, Unit 2 = ID 2) using an MT50 meter or EPEVER PC software before connecting them together.

---

## First-Time Setup (Captive Portal)

1. Connect the hardware and power up the ESP32-C3 SuperMini.
2. The onboard blue LED will illuminate solid, indicating **Setup Mode**.
3. On your phone or laptop, scan for Wi-Fi networks and connect to:
   - **SSID:** `EPEVER-Gateway-XXXX` (Open network, no password required)
4. A captive portal page will appear automatically (or navigate to `http://192.168.4.1` in your browser).
5. In the web interface:
   - Click **Scan** to find and select your 2.4GHz Wi-Fi network, and enter the password.
   - Enter your **Venus OS MQTT Broker Host / IP** (e.g. `venus.local` or `192.168.1.100`).
   - Enter the active **Modbus Slave IDs** (e.g. `1` for a single unit, or `1, 2` for multiple units).
   - Click **Save Settings & Connect**.
6. The gateway will save the settings to NVS flash and reboot.
7. Once connected to Wi-Fi and Venus OS MQTT, the blue LED will pulse gently at 1Hz.

> [!TIP]
> **Factory Reset / Reconfiguration:**
> Press and hold the onboard **Boot button (GPIO 9)** for **3 seconds**. The blue LED will flash rapidly, erase the saved settings, and restart the Captive Portal AP.

---

## Building and Flashing

### Using PlatformIO (Recommended)
Compile the project:
```bash
pio run
```

Upload firmware to the ESP32-C3 SuperMini via USB-C:
```bash
pio run --target upload
```

Monitor serial logs:
```bash
pio device monitor --baud 115200
```

### Running Unit Tests on Linux Host
To test the Modbus CRC-16 computation and telemetry decoding against official EPEVER test vectors:
```bash
gcc -Wall -Wextra -Iinclude test/test_protocol.c -o /tmp/test_protocol && /tmp/test_protocol
```

---

## Venus OS MQTT Integration

This gateway formats data to match [mr-manuel/venus-os_dbus-mqtt-solar-charger](https://github.com/mr-manuel/venus-os_dbus-mqtt-solar-charger).

### Sample Payload Published to `solar_charger/0`:
```json
{
  "Pv": {
    "V": 65.4
  },
  "Yield": {
    "Power": 450.0,
    "User": 3.42,
    "System": 852.1
  },
  "Dc": {
    "0": {
      "Voltage": 26.8,
      "Current": 16.5
    }
  },
  "Load": {
    "State": 1,
    "I": 2.1
  },
  "State": 3,
  "ErrorCode": 0,
  "MppOperationMode": 2,
  "History": {
    "Daily": {
      "0": {
        "Yield": 3.42,
        "MaxPower": 520,
        "MaxPvVoltage": 85.2,
        "MinBatteryVoltage": 24.2,
        "MaxBatteryVoltage": 28.4
      }
    }
  }
}
```

### Extended Telemetry Published to `epever/unit_1/telemetry`:
```json
{
  "slave_id": 1,
  "online": true,
  "pv_v": 65.4,
  "pv_a": 6.88,
  "pv_w": 450.0,
  "bat_v": 26.8,
  "bat_a": 16.5,
  "bat_soc": 98,
  "bat_temp": 22.5,
  "dev_temp": 34.0,
  "night": false,
  "yield_today_kwh": 3.42,
  "yield_total_kwh": 852.1,
  "raw_status_chg": 9,
  "raw_status_bat": 0
}
```
