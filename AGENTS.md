# Workspace Guide for AI Agents

This workspace contains firmware for the **LilyGo T-SIM7080G** — an ESP32-S3 IoT board with a SIM7080G cellular modem (NB-IoT / Cat-M only), AXP2101 PMU, GPS, camera, and SD card.

**Detailed reference**: [LilyGo-T-SIM7080G/AGENTS.md](LilyGo-T-SIM7080G/AGENTS.md)

---

## Build & Flash (PlatformIO)

```bash
cd LilyGo-T-SIM7080G
pio run                          # build
pio run --target upload          # flash (921600 baud)
pio device monitor --baud 115200 # serial monitor
```

**Switch example**: edit `LilyGo-T-SIM7080G/platformio.ini` — uncomment exactly ONE `src_dir` line.  
Currently active: `src_dir = examples/MinimalModemGPSExample`

Available examples: `ATDebug`, `MinimalPowersExample`, `MinimalModemGPSExample`, `MinimalModemNBIOTExample`, `MinimalDeepSleepExample`, `ModemMqttPublishExample`, `ModemMqttsExample`, `GPSTrackerWithOfflineBuffer`, `GPSTrackerProduction`, `AllFunction`, and more.

If upload fails: hold BOOT → press RST → release RST → release BOOT → upload → press RST.

---

## Critical Hardware Rules

1. **Never disable BLDO1** — powers the ESP32↔SIM7080G level shifter; communication breaks immediately.
2. **Never change DC1 voltage** — fixed 3.3V for ESP32-S3 core.
3. **DC3 must be 3000mV** for SIM7080G: `PMU.setDC3Voltage(3000); PMU.enableDC3();`
4. **Insert SIM card before power-on** — hot-swap unsupported.
5. **GPS and cellular are mutually exclusive** — SIM7080G hardware limitation.
6. **For battery charging** call `PMU.disableTSPinMeasure();` — without it, charging is blocked.
7. **GPIO 35–37 are unavailable** — reserved for Octal SPI PSRAM.

---

## Key Code Patterns

### Required includes (modem sketches)
```cpp
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
XPowersPMU PMU;

#define TINY_GSM_RX_BUFFER 1024
#define SerialAT Serial1
#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif
```

### PMU init (always first)
```cpp
PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
PMU.setDC3Voltage(3000);  PMU.enableDC3();   // modem
PMU.setBLDO2Voltage(3300); PMU.enableBLDO2(); // GPS antenna
PMU.setBLDO1Voltage(3300); PMU.enableBLDO1(); // level shifter (critical)
PMU.disableTSPinMeasure();                     // enable charging
```

### Modem power-on sequence
```cpp
Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);
pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
digitalWrite(BOARD_MODEM_PWR_PIN, LOW); delay(100);
digitalWrite(BOARD_MODEM_PWR_PIN, HIGH); delay(1000);
digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
while (!modem.testAT(1000)) {}
```

---

## Pin Reference

| Peripheral | Pins |
|------------|------|
| PMU I2C | SDA=15, SCL=7, IRQ=6 |
| Modem | PWR=41, RXD=4, TXD=5, RI=3, DTR=42 |
| SD Card | CMD=39, CLK=38, DATA0=40 |
| Camera | Reset=18, XCLK=8, SDA=2, SCL=1, VSYNC=16, HREF=17, PCLK=12, Y2-Y9=14,47,48,21,13,11,10,9 |

---

## AT Command Debugging

`-DDUMP_AT_COMMANDS` is enabled in `platformio.ini` by default — all AT traffic is echoed to Serial. Remove this flag in production builds to avoid credential leakage.

---

## Libraries (bundled in `lib/`)

All libraries are local — no internet access needed for compilation. Do **not** install them via PlatformIO registry; the bundled versions are pre-patched for this board.
