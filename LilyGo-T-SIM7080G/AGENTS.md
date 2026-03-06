# LilyGo T-SIM7080G Project Guide for AI Agents

## Project Overview

This is an **embedded systems project** for the **LilyGo T-SIM7080G** development board - an ESP32-S3 based IoT board with integrated SIM7080G cellular modem, GPS, camera interface, SD card slot, and solar charging support.

**Key Hardware Features:**
- **MCU**: ESP32-S3 (Dual-core Xtensa LX7, 240MHz, WiFi 4 + BLE 5.0)
- **Cellular Modem**: SIM7080G (NB-IoT + Cat-M, global bands, **NO 2G/3G/4G support**)
- **GPS**: Integrated with SIM7080G (cannot use GPS and cellular simultaneously)
- **PMU**: AXP2101 power management unit with battery charging
- **Camera**: OV2640/OV5640 support (optional)
- **Storage**: 1-bit SD_MMC interface + 16MB Flash + 8MB PSRAM (Octal SPI)
- **Power**: USB-C, battery connector (3.5-4.2V), solar input (4.4-6V)

## Technology Stack

| Component | Technology |
|-----------|------------|
| Build System | PlatformIO (preferred) or Arduino IDE |
| Platform | espressif32 @ 6.3.0 |
| Framework | Arduino |
| Board Definition | ESP32S3 Dev Module / esp32s3box |
| Primary Language | C/C++ (Arduino) |

## Project Structure

```
LilyGo-T-SIM7080G/
├── platformio.ini          # PlatformIO configuration
├── examples/               # Example sketches (29 examples)
│   ├── AllFunction/        # Full feature demo (camera, modem, SD, web server)
│   ├── ATDebug/            # AT command debugging
│   ├── Minimal*/           # Single-feature minimal examples
│   ├── Modem*Example/      # Cellular/MQTT examples
│   ├── BLE5_*/             # Bluetooth 5.0 examples
│   ├── GPSTracker*/        # GPS tracking applications
│   └── SIM7080G-ATT-NB-IOT-*/  # AWS/HTTP/MQTT examples by @bootcampiot
├── lib/                    # Bundled libraries
│   ├── TinyGSM/            # Cellular modem library (SIM7080)
│   ├── XPowersLib/         # AXP2101 PMU driver
│   ├── ArduinoJson-5/      # JSON parsing
│   ├── pubsubclient/       # MQTT client
│   ├── Cayenne-MQTT-Arduino/  # Cayenne IoT platform
│   ├── StreamDebugger/     # AT command debugging
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

## Build Configuration

### PlatformIO (Recommended)

**File**: `platformio.ini`

Key settings:
```ini
[env:lilygo-t-sim7080x-s3]
platform = espressif32@6.3.0
framework = arduino
board = esp32s3box
upload_speed = 921600
monitor_speed = 115200

build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=1    ; Enable USB serial on boot
    -DTINY_GSM_MODEM_SIM7080
    -DTINY_GSM_RX_BUFFER=1024
    -DDUMP_AT_COMMANDS             ; Uncomment to debug AT commands
    -DCONFIG_BT_BLE_50_FEATURES_SUPPORTED

board_build.partitions = huge_app.csv
```

**Selecting an Example:**
Edit `platformio.ini` and uncomment ONE line in the `[platformio]` section:
```ini
[platformio]
src_dir = examples/ATDebug
; src_dir = examples/MinimalModemGPSExample
; src_dir = examples/ModemMqttPublishExample
; ... etc
```

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
Copy all folders from `lib/` to `<Documents>/Arduino/libraries/`

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

## Power Domain Mapping

| Peripheral | PMU Channel | Voltage |
|------------|-------------|---------|
| ESP32-S3 Core | DC1 | 3.3V (fixed) |
| SIM7080G Modem | DC3 | 3000mV (2700-3400mV range) |
| Camera DVDD | ALDO1 | 1800mV |
| Camera DVDD | ALDO2 | 2800mV |
| Camera AVDD | ALDO4 | 3000mV |
| SD Card | ALDO3 | 3300mV |
| GPS Antenna | BLDO2 | 3300mV |
| Level Shifter | BLDO1 | 3300mV (**NEVER DISABLE!**) |
| External (row pins) | DC5 | 3300mV (1400-3700mV adjustable) |

## Critical Hardware Notes

1. **⚠️ CRITICAL: Never disable BLDO1** - ESP32-S3 and SIM7080G will lose communication
2. **⚠️ TS Pin must be disabled** for charging: `PMU.disableTSPinMeasure()`
3. **⚠️ Cannot use GPS and cellular simultaneously** - hardware limitation
4. **SIM card must be inserted before power-on** - hot-swap not supported
5. **Supports NB-IoT and Cat-M only** - no 2G/3G/4G fallback
6. **Two USB ports:**
   - USB-C: ESP32-S3 programming and Serial
   - Micro-USB: SIM7080G firmware updates only

## Common Code Patterns

### PMU Initialization
```cpp
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
XPowersPMU PMU;

void setupPMU() {
    // Initialize I2C
    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        Serial.println("PMU init failed!");
        return;
    }
    
    // Modem power (DC3: 3000mV)
    PMU.setDC3Voltage(3000);
    PMU.enableDC3();
    
    // GPS antenna power (BLDO2)
    PMU.setBLDO2Voltage(3300);
    PMU.enableBLDO2();
    
    // Disable TS pin for charging
    PMU.disableTSPinMeasure();
}
```

### Modem Initialization
```cpp
#define TINY_GSM_MODEM_SIM7080
#define TINY_GSM_RX_BUFFER 1024
#include <TinyGsmClient.h>

TinyGsm modem(Serial1);

void setupModem() {
    Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);
    
    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
    
    // Power on sequence (PWRKEY low 1s)
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

### Network Registration
```cpp
// Check SIM card
if (modem.getSimStatus() != SIM_READY) {
    Serial.println("No SIM card!");
    return;
}

// Wait for network registration
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

// Set APN manually (required for some carriers)
modem.sendAT("+CGDCONT=1,\"IP\",\"", apn, "\"");
modem.waitResponse();

// Activate bearer
modem.sendAT("+CNACT=0,1");
modem.waitResponse();
```

## Debugging

### Enable AT Command Logging
`-DDUMP_AT_COMMANDS` is **enabled by default** in `platformio.ini`. Remove it for production builds to avoid credential leakage in Serial output. To use it in code:
```cpp
#define DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(Serial1, Serial);
TinyGsm modem(debugger);
```

### Upload Issues - Boot Mode
If upload fails, enter download mode manually:
1. Press and hold BOOT button
2. While holding BOOT, press RST
3. Release RST
4. Release BOOT
5. Upload sketch

### Power Lockout Recovery
If ESP32-S3 power channel was incorrectly disabled:
1. Insert USB
2. Press and hold BOOT
3. Press and hold PWRKEY
4. Board enters download mode
5. Upload corrected sketch

## Examples Reference

| Example | Purpose | Key Features |
|---------|---------|--------------|
| `ATDebug` | Modem debugging | AT command passthrough |
| `MinimalPowersExample` | PMU basics | Power channels, charging, IRQ |
| `MinimalModemGPSExample` | GPS usage | Location fix, coordinates |
| `MinimalModemNBIOTExample` | NB-IoT connection | Network registration, HTTP/HTTPS |
| `MinimalDeepSleepExample` | Power saving | Deep sleep, wake sources |
| `ModemMqttPublishExample` | MQTT publish | GSM MQTT client |
| `ModemMqttsExample` | Secure MQTT | SSL/TLS MQTT connection |
| `GPSTrackerProduction` | Production tracker | Watchdog, persistent sessions, PSM; has `config.h` + `config_test.h` for test broker |
| `AllFunction` | Full demo | Camera, SD, web server, modem |

## Documentation Resources

- **Schematic**: `schematic/T-SIM7080G_Schematic.pdf`
- **Modem AT Manual**: `datasheet/SIM7070_SIM7080_SIM7090 Series_AT Command Manual_V*.pdf`
- **Modem Firmware Update**: `docs/sim7080_update_firmware.md`
- **Main README**: `README.MD` (English), `README_CN.MD` (Chinese)

## External Dependencies

When using PlatformIO, these are auto-installed:
- `TinyGSM` - Cellular modem driver
- `XPowersLib` - AXP2101 driver

For Arduino IDE, libraries are bundled in `lib/` folder.

## License

All example code is MIT licensed. See individual file headers for attribution.

---

**Last Updated**: Generated from project analysis  
**Project**: LilyGo T-SIM7080G ESP32-S3 Cellular Development Board
