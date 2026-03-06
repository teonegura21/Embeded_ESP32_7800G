# LilyGo T-SIM7080G Project Guide for AI Agents

## Project Overview

This is an **embedded systems project** for the **LilyGo T-SIM7080G** development board - an ESP32-S3 based IoT board with integrated SIM7080G cellular modem, GPS, camera interface, SD card slot, and solar charging support.

**Key Hardware Features:**
- **MCU**: ESP32-S3 (Dual-core Xtensa LX7, 240MHz, WiFi 4 + BLE 5.0)
- **Cellular Modem**: SIM7080G (NB-IoT + Cat-M, global bands, **NO 2G/3G/4G support**)
- **GPS**: Integrated with SIM7080G (cannot use GPS and cellular simultaneously - hardware limitation)
- **PMU**: AXP2101 power management unit with battery charging
- **Camera**: OV2640/OV5640 support (optional)
- **Storage**: 1-bit SD_MMC interface + 16MB Flash + 8MB PSRAM (Octal SPI)
- **Power**: USB-C, battery connector (3.5-4.2V), solar input (4.4-6V)

**Important Hardware Limitations:**
- GPS and cellular cannot be used simultaneously - SIM7080G hardware limitation
- SIM card must be inserted before power-on (hot-swap not supported)
- GPIO35~GPIO37 are unavailable (reserved for Octal SPI PSRAM)
- BLDO1 power channel must NEVER be disabled (powers ESP32↔SIM7080G level shifter)

---

## Technology Stack

| Component | Technology |
|-----------|------------|
| Build System | PlatformIO (preferred) or Arduino IDE |
| Platform | espressif32 @ 6.3.0 |
| Framework | Arduino |
| Board Definition | ESP32S3 Dev Module / esp32s3box |
| Primary Language | C/C++ (Arduino) |
| Partition Scheme | huge_app.csv (16MB Flash) |

---

## Project Structure

```
LilyGo-T-SIM7080G/
├── platformio.ini          # PlatformIO configuration - edit src_dir to switch examples
├── examples/               # Example sketches (33 examples)
│   ├── AllFunction/        # Full feature demo (camera, modem, SD, web server)
│   ├── ATDebug/            # AT command debugging via serial passthrough
│   ├── FleetGPSTracker/    # WiFi-based GPS tracker with SD buffer + HiveMQ TLS
│   ├── Minimal*/           # Single-feature minimal examples (PMU, GPS, modem, SD, camera)
│   ├── Modem*Example/      # Cellular/MQTT/HTTP examples
│   ├── BLE5_*/             # Bluetooth 5.0 examples
│   ├── GPSTracker*/        # GPS tracking applications (production-grade)
│   └── SIM7080G-ATT-NB-IOT-*/  # AWS/HTTP/MQTT examples by @bootcampiot
├── lib/                    # Bundled libraries (local copies - do not install via registry)
│   ├── TinyGSM/            # Cellular modem library (SIM7080 support)
│   ├── XPowersLib/         # AXP2101 PMU driver
│   ├── ArduinoJson-5/      # JSON parsing
│   ├── pubsubclient/       # MQTT client for WiFi
│   ├── Cayenne-MQTT-Arduino/  # Cayenne IoT platform
│   ├── StreamDebugger/     # AT command debugging utility
│   └── ArduinoBIGIOT/      # BIGIOT platform support
├── docs/                   # Documentation
│   └── sim7080_update_firmware.md  # Modem firmware update guide
├── schematic/              # Hardware schematic (PDF)
├── datasheet/              # SIM7080G datasheets and app notes
├── dwg/                    # Board DXF mechanical drawing (T-SIM7080G.DXF)
├── firmware/               # Pre-compiled firmware binaries
├── shell/                  # 3D printable case files (STL/STEP)
└── image/                  # Documentation images
```

---

## Build System

### PlatformIO (Recommended)

**Configuration File**: `LilyGo-T-SIM7080G/platformio.ini`

**Selecting an Example:**
Edit `platformio.ini` and set `src_dir` to the desired example. Only ONE example can be active:

```ini
[platformio]
src_dir = examples/FleetGPSTracker
; src_dir = examples/ATDebug
; src_dir = examples/MinimalModemGPSExample
; ... etc
```

**Available Build Environments:**
- `[env:lilygo-t-sim7080x-s3]` - Default environment (AT command debugging enabled)
- `[env:fleet-gps-tracker]` - Production environment (no AT debugging)

**Build Commands:**
```bash
cd LilyGo-T-SIM7080G

# Build
pio run

# Flash (requires board connected via USB-C)
pio run --target upload

# Flash specific environment
pio run -e fleet-gps-tracker --target upload

# Serial monitor
pio device monitor --baud 115200

# Build and flash specific example
pio run -e lilygo-t-sim7080x-s3 --target upload
```

**Upload Troubleshooting:**
If upload fails, enter download mode manually:
1. Press and hold BOOT button
2. While holding BOOT, press RST
3. Release RST
4. Release BOOT
5. Upload sketch

**Key Build Flags:**
```ini
build_flags =
    -DBOARD_HAS_PSRAM              # Enable PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=1    # USB serial on boot (disable for battery-only)
    -DTINY_GSM_MODEM_SIM7080       # Select SIM7080 modem driver
    -DTINY_GSM_RX_BUFFER=1024      # Modem RX buffer size
    -DDUMP_AT_COMMANDS             # Enable AT command logging (remove for production)
    -DCONFIG_BT_BLE_50_FEATURES_SUPPORTED
    -DMQTT_MAX_PACKET_SIZE=512     # PubSubClient buffer size
```

**⚠️ Security Warning:** Remove `-DDUMP_AT_COMMANDS` for production builds to avoid credential leakage in serial output.

### Arduino IDE

**Board Settings:**
| Setting | Value |
|---------|-------|
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enable (Disable for battery-only operation) |
| CPU Frequency | 240MHz |
| Flash Mode | QIO 80MHz |
| Flash Size | 16MB (128Mb) |
| PSRAM | OPI PSRAM |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
| Upload Mode | UART0/Hardware CDC |
| Upload Speed | 921600 |

**Library Installation:**
Copy all folders from `LilyGo-T-SIM7080G/lib/` to `<Documents>/Arduino/libraries/`

---

## Hardware Pin Definitions

**PMU (AXP2101) - I2C:**
| Signal | GPIO |
|--------|------|
| SDA | 15 |
| SCL | 7 |
| IRQ | 6 |

**SIM7080G Modem:**
| Signal | GPIO |
|--------|------|
| PWR | 41 |
| RXD | 4 |
| TXD | 5 |
| RI | 3 |
| DTR | 42 |

**SD Card (1-bit SD_MMC):**
| Signal | GPIO |
|--------|------|
| CMD | 39 |
| CLK | 38 |
| DATA0 | 40 |

**Camera (OV2640/OV5640):**
| Signal | GPIO |
|--------|------|
| Reset | 18 |
| XCLK | 8 |
| SDA (SIOD) | 2 |
| SCL (SIOC) | 1 |
| VSYNC | 16 |
| HREF | 17 |
| PCLK | 12 |
| Y2-Y9 | 14, 47, 48, 21, 13, 11, 10, 9 |

**⚠️ Unavailable GPIOs (PSRAM uses Octal SPI):**
- GPIO35, GPIO36, GPIO37 - **DO NOT USE**

**Other Pins:**
| Function | GPIO |
|----------|------|
| User Button | 0 |
| PMU Input/IRQ | 6 |

---

## Power Domain Mapping (AXP2101 PMU)

| Peripheral | PMU Channel | Voltage | Notes |
|------------|-------------|---------|-------|
| ESP32-S3 Core | DC1 | 3.3V | **FIXED - DO NOT CHANGE** |
| SIM7080G Modem | DC3 | 3000mV | Range: 2700-3400mV |
| Camera DVDD | ALDO1 | 1800mV | Camera power |
| Camera DVDD | ALDO2 | 2800mV | Camera power |
| Camera AVDD | ALDO4 | 3000mV | Camera power |
| SD Card | ALDO3 | 3300mV | SD card power |
| GPS Antenna | BLDO2 | 3300mV | Enable for GPS |
| Level Shifter | BLDO1 | 3300mV | **⚠️ NEVER DISABLE!** |
| External (row pins) | DC5 | 3300mV | Adjustable: 1400-3700mV, max 1A |

---

## Critical Hardware Rules

1. **⚠️ CRITICAL: Never disable BLDO1** - Powers the ESP32-S3↔SIM7080G level shifter; communication breaks immediately if disabled
2. **⚠️ CRITICAL: Never change DC1 voltage** - Fixed 3.3V for ESP32-S3 core
3. **DC3 must be 3000mV for SIM7080G**: `PMU.setDC3Voltage(3000); PMU.enableDC3();`
4. **Insert SIM card before power-on** - Hot-swap not supported
5. **GPS and cellular are mutually exclusive** - SIM7080G hardware limitation
6. **For battery charging, disable TS Pin**: `PMU.disableTSPinMeasure();` - Without this, charging is blocked
7. **Two USB ports**:
   - USB-C: ESP32-S3 programming and Serial
   - Micro-USB: SIM7080G firmware updates only

---

## Code Organization and Patterns

### Required Includes (Modem Sketches)
```cpp
// PMU
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
XPowersPMU PMU;

// Modem
#define TINY_GSM_RX_BUFFER 1024
#define SerialAT Serial1
#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>

// AT debugging (optional)
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif
```

### PMU Initialization Pattern
```cpp
bool pmuInit() {
    Wire.begin(I2C_SDA, I2C_SCL);
    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        return false;
    }
    
    // CRITICAL: BLDO1 powers level converter - NEVER disable
    PMU.setBLDO1Voltage(3300);
    PMU.enableBLDO1();
    
    // GPS antenna power
    PMU.setBLDO2Voltage(3300);
    PMU.enableBLDO2();
    
    // Modem power (3000mV required)
    PMU.setDC3Voltage(3000);
    PMU.enableDC3();
    
    // Disable TS pin for charging
    PMU.disableTSPinMeasure();
    
    return true;
}
```

### Modem Power-On Sequence
```cpp
void modemPowerOn() {
    Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);
    
    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
    delay(1000);
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
    
    // Wait for AT response
    while (!modem.testAT(1000)) {
        Serial.print(".");
    }
}
```

### GPS Read Pattern (AT+CGNSINF)
```cpp
// Fields in CGNSINF response:
// 0: GNSS run status, 1: Fix status, 2: UTC datetime
// 3: Lat, 4: Lon, 5: Altitude, 6: Speed(km/h), 7: Course/heading
// 8: Fix mode, 9: Reserved, 10: HDOP, 11: PDOP, 12: VDOP
// 13: Reserved, 14: Sats in view, 15: GPS sats used

bool gpsRead(float& lat, float& lon, float& spd, float& heading,
             float& alt, float& hdop, int& sats, bool& fix) {
    modem.sendAT("+CGNSINF");
    if (modem.waitResponse(5000L, "+CGNSINF: ") != 1) {
        modem.waitResponse();
        return false;
    }
    
    String raw = modem.stream.readStringUntil('\n');
    raw.trim();
    modem.waitResponse();  // consume OK
    
    // Parse comma-separated fields...
}
```

### Network Registration Pattern
```cpp
// Check SIM
if (modem.getSimStatus() != SIM_READY) {
    Serial.println("No SIM!");
    return;
}

// Wait for network
SIM70xxRegStatus s;
do {
    s = modem.getRegistrationStatus();
    delay(1000);
} while (s != REG_OK_HOME && s != REG_OK_ROAMING);
```

### NB-IoT Configuration
```cpp
// Set network mode (2 = automatic)
modem.setNetworkMode(2);
// Set preferred mode to NB-IoT
modem.setPreferredMode(2);  // 2 = NB-IOT, 1 = CAT-M, 3 = CAT-M & NB-IOT

// Set APN manually if required
modem.sendAT("+CGDCONT=1,\"IP\",\"", apn, "\"");
modem.waitResponse();

// Activate bearer
modem.sendAT("+CNACT=0,1");
modem.waitResponse();
```

---

## Examples Reference

| Example | Purpose | Key Features |
|---------|---------|--------------|
| `ATDebug` | Modem debugging | AT command passthrough to SIM7080G |
| `MinimalPowersExample` | PMU basics | Power channels, charging, IRQ handling |
| `MinimalModemGPSExample` | GPS usage | Location fix, coordinate parsing |
| `MinimalModemNBIOTExample` | NB-IoT connection | Network registration, HTTP/HTTPS |
| `MinimalDeepSleepExample` | Power saving | Deep sleep, wake sources, timer wakeup |
| `ModemMqttPublishExample` | MQTT publish | GSM MQTT client, cellular MQTT |
| `ModemMqttsExample` | Secure MQTT | SSL/TLS MQTT connection |
| `FleetGPSTracker` | Fleet tracking | WiFi + HiveMQ TLS, SD card buffer, GPS |
| `GPSTrackerProduction` | Production tracker | Watchdog, PSM sleep, persistent sessions, ring buffer |
| `GPSTrackerWithOfflineBuffer` | Offline tracking | SD card ring buffer, batch uploads |
| `AllFunction` | Full demo | Camera, SD, web server, modem integration |

---

## Testing and Debugging

### Enable AT Command Logging
`-DDUMP_AT_COMMANDS` is enabled by default in `platformio.ini`. All AT traffic is echoed to Serial. Remove this flag for production builds to avoid credential leakage.

In code:
```cpp
#define DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
```

### Serial Monitor
Default baud rate: **115200**

### Power Lockout Recovery
If ESP32-S3 power channel was incorrectly disabled:
1. Insert USB
2. Press and hold BOOT
3. Press and hold PWRKEY
4. Board enters download mode
5. Upload corrected sketch

### LED Indicators

| LED | Color | Location | Function |
|-----|-------|----------|----------|
| MODEM STATUS | Red | Near modem | Power status (cannot be turned off) |
| MODEM NETWORK | Red | Near modem | Network state (see below) |
| CHARGE LED | Blue | Near battery | Charging indicator (PMU controlled) |

**Network Light Patterns:**
| Pattern | Status |
|---------|--------|
| 64ms on / 800ms off | Not registered on network |
| 64ms on / 3000ms off | Registered (PS domain success) |
| 64ms on / 300ms off | Data transmission active |
| Off | Power off or PSM sleep mode |

---

## Security Considerations

1. **Remove `-DDUMP_AT_COMMANDS`** for production builds - prevents credential leakage
2. **Use TLS/SSL** for MQTT connections when possible (`WiFiClientSecure`)
3. **Store credentials in config.h** - never hardcode in main .ino files
4. **SD card data** - encrypted storage recommended for sensitive GPS data

---

## External Resources

- **Schematic**: `schematic/T-SIM7080G_Schematic.pdf`
- **Modem AT Manual**: `datasheet/SIM7070_SIM7080_SIM7090 Series_AT Command Manual_V*.pdf`
- **Modem Firmware Update**: `docs/sim7080_update_firmware.md`
- **Main README**: `README.MD` (English), `README_CN.MD` (Chinese)

---

## License

All example code is MIT licensed. See individual file headers for attribution.

---

**Last Updated**: 2024-03-07  
**Project**: LilyGo T-SIM7080G ESP32-S3 Cellular Development Board
