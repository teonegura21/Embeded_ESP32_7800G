/**
 * @file      FleetGPSTracker.ino
 * @brief     Fleet Analytics Platform — GPS Tracker Firmware
 *            LilyGO T-SIM7080G (ESP32-S3) + AXP2101 PMU
 *
 * Transport : WiFi (phone hotspot) → HiveMQ Cloud TLS 8883
 * Topic     : fleet/gps/<BUS_ID>  (e.g. fleet/gps/BUS_01)
 * Buffer    : SD card JSONL, replayed on reconnect
 *
 * Prerequisites:
 *   1. Edit config.h: set WIFI_SSID, WIFI_PASSWORD, BUS_ID
 *   2. Flash: pio run -e fleet-gps-tracker --target upload
 *   3. Monitor: pio device monitor --baud 115200
 */

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "utilities.h"

// ─── PMU (AXP2101) ──────────────────────────────────────────────────────────
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
XPowersPMU PMU;

// ─── TinyGSM (modem used for GPS only — cellular is NOT used) ───────────────
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

// ─── WiFi + MQTT ────────────────────────────────────────────────────────────
WiFiClientSecure wifiSecure;
PubSubClient     mqtt(wifiSecure);
char             mqttClientId[32];

// ─── State ───────────────────────────────────────────────────────────────────
bool sdAvailable    = false;
bool gpsEnabled     = false;
uint32_t bufferCount = 0;

unsigned long lastPublishMs  = 0;
unsigned long lastGpsMs      = 0;   // updated whenever GPS response received

// ─── Macros ──────────────────────────────────────────────────────────────────
#if DEBUG_ENABLED
  #define LOG(...)  Serial.printf(__VA_ARGS__)
#else
  #define LOG(...)
#endif

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================
bool  pmuInit();
void  modemPowerOn();
bool  gpsInit();
bool  gpsRead(float& lat, float& lon, float& spd, float& heading,
              float& alt, float& hdop, int& sats, bool& fix);
bool  sdInit();
bool  bufferAppend(const char* json);
void  bufferReplay();
void  wifiConnect();
bool  mqttConnect();
void  publishGps(float lat, float lon, float spd, float heading,
                 float alt, float hdop, int sats, bool fix);

// =============================================================================
// setup()
// =============================================================================
void setup() {
#if DEBUG_ENABLED
    Serial.begin(115200);
    delay(500);
#endif
    LOG("\n==============================\n");
    LOG("  Fleet GPS Tracker v1.0\n");
    LOG("  Bus: %s\n", BUS_ID);
    LOG("==============================\n\n");

    // PMU — must be first; powers modem and GPS antenna
    if (!pmuInit()) {
        LOG("[PMU] FATAL: AXP2101 init failed\n");
        // Continue anyway — board might run without PMU in some revisions
    }

    // SD card
    sdAvailable = sdInit();
    LOG("[SD]  %s\n", sdAvailable ? "OK" : "Not available — buffer disabled");

    // Modem power-on then GPS init
    modemPowerOn();
    gpsEnabled = gpsInit();
    if (!gpsEnabled) LOG("[GPS] WARNING: GPS init failed — will retry in loop\n");

    // WiFi
    wifiConnect();

    // MQTT client ID from MAC
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(mqttClientId, sizeof(mqttClientId), "fleet_%02X%02X_%s",
             mac[4], mac[5], BUS_ID);

    // TLS: skip certificate verification (password-protected broker)
    wifiSecure.setInsecure();

    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setKeepAlive(MQTT_KEEPALIVE);
    mqtt.setBufferSize(512);

    mqttConnect();

    LOG("[SETUP] Done — entering loop\n\n");
}

// =============================================================================
// loop()
// =============================================================================
void loop() {
    // ── MQTT keepalive ──────────────────────────────────────────────────────
    if (mqtt.connected()) {
        mqtt.loop();
    }

    // ── WiFi watchdog ───────────────────────────────────────────────────────
    if (WiFi.status() != WL_CONNECTED) {
        LOG("[WiFi] Lost connection — reconnecting...\n");
        wifiConnect();
    }

    // ── MQTT watchdog ───────────────────────────────────────────────────────
    if (!mqtt.connected() && WiFi.status() == WL_CONNECTED) {
        LOG("[MQTT] Disconnected — reconnecting...\n");
        if (mqttConnect()) {
            // Replay any buffered packets on successful reconnect
            if (sdAvailable && bufferCount > 0) {
                LOG("[BUF]  Replaying %u buffered packets...\n", bufferCount);
                bufferReplay();
            }
        }
    }

    // ── GPS init retry ──────────────────────────────────────────────────────
    if (!gpsEnabled) {
        modemPowerOn();
        gpsEnabled = gpsInit();
    }

    // ── GPS watchdog ────────────────────────────────────────────────────────
    if (lastGpsMs > 0 && (millis() - lastGpsMs) > GPS_WATCHDOG_MS) {
        LOG("[GPS] WATCHDOG: no response for %lu s — rebooting\n",
            GPS_WATCHDOG_MS / 1000);
        ESP.restart();
    }

    // ── Publish cycle ───────────────────────────────────────────────────────
    if ((millis() - lastPublishMs) >= GPS_PUBLISH_INTERVAL_MS) {
        lastPublishMs = millis();

        float lat = 0, lon = 0, spd = 0, heading = 0, alt = 0, hdop = 9.9f;
        int   sats = 0;
        bool  fix  = false;

        if (gpsEnabled) {
            bool ok = gpsRead(lat, lon, spd, heading, alt, hdop, sats, fix);
            if (ok) lastGpsMs = millis();
        }

        publishGps(lat, lon, spd, heading, alt, hdop, sats, fix);
    }
}

// =============================================================================
// PMU
// =============================================================================
bool pmuInit() {
    Wire.begin(I2C_SDA, I2C_SCL);
    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        return false;
    }

    // CRITICAL: NEVER disable BLDO1 — it powers the level converter!
    PMU.setBLDO1Voltage(3300);
    PMU.enableBLDO1();

    // GPS antenna power (BLDO2)
    PMU.setBLDO2Voltage(3300);
    PMU.enableBLDO2();

    // Modem main power (DC3) — SIM7080G requires exactly 3000 mV
    PMU.setDC3Voltage(3000);
    PMU.enableDC3();

    PMU.disableTSPinMeasure();
    LOG("[PMU]  AXP2101 OK, battery %.2f V\n",
        PMU.getBattVoltage() / 1000.0f);
    return true;
}

// =============================================================================
// MODEM POWER ON
// =============================================================================
void modemPowerOn() {
    SerialAT.begin(115200, SERIAL_8N1,
                   BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);

    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
    delay(1000);
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);

    // Wait up to 15 s for AT response
    int retries = 0;
    while (!modem.testAT(1000)) {
        if (retries++ > 15) {
            LOG("[MODEM] AT timeout — power cycling\n");
            digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
            delay(1000);
            digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
            retries = 0;
        }
    }
    LOG("[MODEM] AT ready\n");
}

// =============================================================================
// GPS INIT
// =============================================================================
bool gpsInit() {
    if (!modem.enableGPS()) {
        LOG("[GPS]  enableGPS() failed\n");
        return false;
    }
    LOG("[GPS]  GNSS enabled (waiting for fix...)\n");
    return true;
}

// =============================================================================
// GPS READ  — parses AT+CGNSINF directly for heading + HDOP
// =============================================================================
// CGNSINF response fields (comma-separated, 0-indexed after "+CGNSINF: "):
//  0  GNSS run   1  Fix status   2  UTC datetime
//  3  Lat        4  Lon          5  Altitude
//  6  Speed(km/h) 7 Course(deg) 8  Fix mode
//  9  Reserved   10 HDOP       11  PDOP  12 VDOP
//  13 Reserved   14 Sats view  15  GPS sats used
// ─────────────────────────────────────────────────────────────────────────────
bool gpsRead(float& lat, float& lon, float& spd, float& heading,
             float& alt, float& hdop, int& sats, bool& fix) {
    modem.sendAT("+CGNSINF");

    // Read line that starts with "+CGNSINF: "
    if (modem.waitResponse(5000L, "+CGNSINF: ") != 1) {
        modem.waitResponse();   // consume OK/ERROR
        return false;
    }

    String raw = modem.stream.readStringUntil('\n');
    raw.trim();
    modem.waitResponse();   // consume trailing OK

    // Tokenize
    String toks[21];
    int    count = 0;
    int    start = 0;
    for (int i = 0; i <= raw.length() && count < 21; i++) {
        if (i == (int)raw.length() || raw[i] == ',') {
            toks[count++] = raw.substring(start, i);
            start = i + 1;
        }
    }

    if (count < 16) return false;

    bool gnssRun = toks[0].toInt() == 1;
    bool fixOk   = toks[1].toInt() == 1;
    fix = fixOk;

    if (!gnssRun) return true;   // GNSS running but no fix yet — not an error

    if (fixOk) {
        lat     = toks[3].toFloat();
        lon     = toks[4].toFloat();
        alt     = toks[5].toFloat();
        spd     = toks[6].toFloat();    // km/h
        heading = toks[7].toFloat();    // course over ground degrees
        hdop    = (count > 10 && toks[10].length() > 0) ? toks[10].toFloat() : 9.9f;
        sats    = (count > 15 && toks[15].length() > 0) ? toks[15].toInt()   : 0;
    }

    return true;
}

// =============================================================================
// SD CARD
// =============================================================================
bool sdInit() {
    SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
    if (!SD_MMC.begin("/sdcard", true)) return false;
    if (SD_MMC.cardType() == CARD_NONE) return false;

    // Count existing buffered records
    File f = SD_MMC.open(BUFFER_FILE_PATH, FILE_READ);
    if (f) {
        while (f.available()) { if (f.read() == '\n') bufferCount++; }
        f.close();
    }
    LOG("[SD]   Card OK, %u buffered records found\n", bufferCount);
    return true;
}

bool bufferAppend(const char* json) {
    if (!sdAvailable) return false;
    if (bufferCount >= MAX_BUFFER_RECORDS) {
        LOG("[BUF]  Buffer full (%u records) — dropping oldest\n", MAX_BUFFER_RECORDS);
        // Simple strategy: truncate and restart (for simplicity)
        SD_MMC.remove(BUFFER_FILE_PATH);
        bufferCount = 0;
    }
    File f = SD_MMC.open(BUFFER_FILE_PATH, FILE_APPEND);
    if (!f) return false;
    f.println(json);
    f.close();
    bufferCount++;
    return true;
}

void bufferReplay() {
    if (!sdAvailable || bufferCount == 0) return;

    File f = SD_MMC.open(BUFFER_FILE_PATH, FILE_READ);
    if (!f) { bufferCount = 0; return; }

    uint32_t replayed = 0, failed = 0;
    String   leftover = "";

    while (f.available() && replayed < bufferCount) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        // Ensure MQTT is still up
        if (!mqtt.connected()) { leftover += line + "\n"; continue; }

        if (mqtt.publish(MQTT_TOPIC, line.c_str(), false)) {
            replayed++;
        } else {
            failed++;
            leftover += line + "\n";
        }
        mqtt.loop();
        delay(50);

        if (replayed % BUFFER_REPLAY_BATCH == 0) {
            LOG("[BUF]  Replayed %u / %u\n", replayed, bufferCount);
        }
    }

    // Read remainder (if we stopped early)
    while (f.available()) { leftover += f.readStringUntil('\n') + "\n"; }
    f.close();

    // Rewrite remaining records
    SD_MMC.remove(BUFFER_FILE_PATH);
    if (leftover.length() > 0) {
        File fw = SD_MMC.open(BUFFER_FILE_PATH, FILE_WRITE);
        if (fw) { fw.print(leftover); fw.close(); }
        bufferCount = failed;
    } else {
        bufferCount = 0;
    }

    LOG("[BUF]  Replay done: sent=%u, remaining=%u\n", replayed, bufferCount);
}

// =============================================================================
// WiFi
// =============================================================================
void wifiConnect() {
    LOG("[WiFi] Connecting to %s...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED &&
           (millis() - t0) < WIFI_CONNECT_TIMEOUT_MS) {
        delay(500);
        LOG(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        LOG("\n[WiFi] Connected, IP %s\n", WiFi.localIP().toString().c_str());
    } else {
        LOG("\n[WiFi] FAILED — will retry next cycle\n");
    }
}

// =============================================================================
// MQTT
// =============================================================================
bool mqttConnect() {
    if (WiFi.status() != WL_CONNECTED) return false;

    LOG("[MQTT] Connecting to %s:%d as %s...\n",
        MQTT_BROKER, MQTT_PORT, mqttClientId);

    bool ok = mqtt.connect(
        mqttClientId,
        MQTT_USER,
        MQTT_PASSWORD_STR,
        MQTT_LWT_TOPIC,   // last will topic
        MQTT_QOS,         // last will qos
        true,             // last will retain
        MQTT_LWT_MSG_OFFLINE
    );

    if (ok) {
        LOG("[MQTT] Connected\n");
        // Announce online status
        mqtt.publish(MQTT_LWT_TOPIC, MQTT_LWT_MSG_ONLINE, true);
    } else {
        LOG("[MQTT] Failed, rc=%d\n", mqtt.state());
    }
    return ok;
}

// =============================================================================
// PUBLISH
// =============================================================================
void publishGps(float lat, float lon, float spd, float heading,
                float alt, float hdop, int sats, bool fix) {
    // Build JSON
    StaticJsonDocument<256> doc;
    doc["bus_id"]      = BUS_ID;
    doc["lat"]         = serialized(String(lat, 6));
    doc["lon"]         = serialized(String(lon, 6));
    doc["speed_kmh"]   = serialized(String(spd, 1));
    doc["heading"]     = serialized(String(heading, 1));
    doc["altitude"]    = serialized(String(alt, 1));
    doc["hdop"]        = serialized(String(hdop, 2));
    doc["satellites"]  = sats;
    doc["fix"]         = fix;
    doc["timestamp_ms"]= millis();   // relative; server uses arrival time for abs

    char buf[256];
    serializeJson(doc, buf, sizeof(buf));

    if (mqtt.connected()) {
        bool ok = mqtt.publish(MQTT_TOPIC, buf, false);
        if (ok) {
            LOG("[GPS]  → %s  spd=%.1f fix=%d sats=%d hdop=%.1f\n",
                fix ? "FIX" : "NOFIX", spd, fix, sats, hdop);
        } else {
            LOG("[GPS]  MQTT publish failed — buffering\n");
            bufferAppend(buf);
        }
    } else {
        LOG("[GPS]  OFFLINE — buffering (total=%u)\n", bufferCount + 1);
        bufferAppend(buf);
    }
}
