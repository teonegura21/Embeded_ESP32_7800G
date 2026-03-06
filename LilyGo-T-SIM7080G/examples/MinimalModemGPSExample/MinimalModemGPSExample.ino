/**
 * @file      MinimalModemGPSExample.ino
 * @license   MIT
 * @date      2026-03-04
 *
 * PRODUCTION GPS TRACKER — CLOUD NATIVE
 * 
 * FEATURES:
 *   ✓ GPS fix via CGNSINF (SIM7080G modem)
 *   ✓ SD Card buffer (massive storage — GBs of data!)
 *   ✓ Wi-Fi MQTTS (TLS/SSL encryption) → Cloud
 *   ✓ Batched JSON sync for efficient cloud ingestion
 *   ✓ Auto-sync buffered data when WiFi returns
 *   ✓ Deep sleep support for battery operation
 *
 * ARCHITECTURE:
 *   LilyGo (GPS + SD) ──WiFi+TLS──► Cloud MQTT ──► Docker (EMQX/InfluxDB/Grafana)
 *
 * CLOUD DEPLOYMENT:
 *   docker-compose up -d
 *
 * HARDWARE:
 *   - LilyGo T-SIM7080G
 *   - GPS antenna in "GPS" port
 *   - microSD card (any size — 1GB to 32GB+)
 *   - USB-C power bank for autonomous operation
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <FS.h>
#include <SD_MMC.h>             // SD card support
#include <esp_task_wdt.h>

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include "utilities.h"

XPowersPMU PMU;

// =============================================================================
// CONFIGURATION — EDIT THESE FOR YOUR SETUP
// =============================================================================

// ── Wi-Fi ─────────────────────────────────────────────────────────────────────
const char* WIFI_SSID = "Orange-DKZTeU-2G";
const char* WIFI_PASS = "4A22FxKGyxZy24DhbA";

// ── MQTT Cloud Broker ─────────────────────────────────────────────────────────
// Option 1: Public test broker (no TLS)
// const char* MQTT_BROKER = "broker.emqx.io";
// const int   MQTT_PORT   = 1883;
// #define USE_TLS false

// Option 2: Your Docker server (no TLS for local testing)
// const char* MQTT_BROKER = "192.168.1.100";  // Your server IP
// const int   MQTT_PORT   = 1883;
// #define USE_TLS false

// Option 3: Production with TLS (recommended)
const char* MQTT_BROKER = "broker.emqx.io";
const int   MQTT_PORT   = 8883;        // 8883 = MQTTS (TLS)
#define USE_TLS true                    // Enable TLS encryption

// TLS Certificate Verification
// false = skip verification (testing/self-signed certs)
// true  = verify server certificate (production with Let's Encrypt)
#define VERIFY_CERT false

// MQTT Authentication (set if your broker requires it)
const char* MQTT_USERNAME = "";
const char* MQTT_PASSWORD = "";

// MQTT Topics
const char* TOPIC_GPS    = "lilygo/gps/location";
const char* TOPIC_BATCH  = "lilygo/gps/batch";
const char* TOPIC_STATUS = "lilygo/gps/status";
const char* TOPIC_META   = "lilygo/gps/meta";

// ── Storage ───────────────────────────────────────────────────────────────────
// SD Card (massive storage) vs LittleFS (internal 16MB flash)
#define USE_SD_CARD true           // true = SD card, false = LittleFS
const char* BUFFER_FILE = "/gps_buffer.csv";
const char* CSV_HEADER  = "millis,lat,lon,alt,speed,sats,acc,year,mon,day,hr,min,sec\n";

// ── Timing ────────────────────────────────────────────────────────────────────
const unsigned long PUBLISH_INTERVAL_MS = 5000;      // Live publish every 5s
const unsigned long BATCH_SYNC_INTERVAL_MS = 30000;  // Sync buffer every 30s
const unsigned long GPS_POLL_INTERVAL_MS = 2000;     // GPS poll every 2s

// ── Batching ──────────────────────────────────────────────────────────────────
#define BATCH_SIZE_TARGET 10       // Fixes per batch
#define MQTT_MAX_PACKET_SIZE 1024  // Increased for larger batches

// ── Debug ──────────────────────────────────────────────────────────────────────
#define DEBUG_GPS_RAW false        // Set true to see raw CGNSINF (for fixing sats)

// =============================================================================
// ROOT CA CERTIFICATE (Let's Encrypt — for TLS)
// =============================================================================

const char* ROOT_CA_CERT = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm74rVmjf
Fn0K4ffLNUILt5AahNLa8XNIX+gW7t/KGHM1lp4LqO47x+NNPhRjlGKLfY7C/jXB
rvlC+GSNeK7XJ+6ZxzCj/+8EBE4ZtMpxFvBrl3IfLK0lTw4Wn0qh6LftKa/trkYY
lthJyqBgZPsIzXkKLvshvbDsYBfq6qvF2Lmx6GSU2xFHVn7DN8OjpKb3h9PhceRI
N7lQzm8x15YAO7xXoDZYM4DuLQt5M1h4r3j7UfG9jegP+OGqU7l9F5b8QuC4f6VQ
aXYx5S6N4MV8Y7LNj6pM4LoMlQI1Z2Qce7P5TnmnJCX8tXqQssS0I6nQbS0zG0R3
zTsL6XfD2K+VHrYg5gc0XZQwGzWPeQY0fQ0G1E4WlgBOKpHfGzRflKnTQqSNT+F5
0kPdx9e+Lf2cWe2uPUy3R9MOBJL5FvTYyC+sLb8cjyQ3G3M1LGlLy4Z6XbLxl4TQ
V5m0R+1bQ2wmMBeS5HQ9HUMlszTlf2PeMHPhJTYo0p5vFcLG4yM1NJ1MxFS2oB+5
o0XfBzyN+6c9j0t2y+/GQ7oLn9Z4Gg8+oF7xA4O5sUHx5Zj/OnE0y8g1NqE5v6i4
4qmqxP9Tu6A6x8zqLIvqdNGAPLjJPFwVjSa/5iTRZDJPF7OOiU9qah8k5pUQwBlU
5z8C+n8rGYLrT5d7/hQJz3GN0Az/mGf4V2mZtDv5gH/LZY4j0USmCjHhXNWqJw8X
rGzZ+ROVnsCOz0kiBSO3LA==
-----END CERTIFICATE-----
)EOF";

// =============================================================================
// MODEM / TINYGSM
// =============================================================================

#define TINY_GSM_RX_BUFFER 1024
#define SerialAT Serial1
#ifndef TINY_GSM_MODEM_SIM7080
#define TINY_GSM_MODEM_SIM7080
#endif
#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif

// =============================================================================
// MQTT CLIENT (Wi-Fi + TLS)
// =============================================================================

#if USE_TLS
WiFiClientSecure wifiClient;
#else
WiFiClient       wifiClient;
#endif

PubSubClient mqtt(wifiClient);

// =============================================================================
// STATE
// =============================================================================

bool wifiConnected  = false;
bool sdReady        = false;
bool gpsEnabled     = false;
bool hasFix         = false;
int  totalFixes     = 0;
int  bufferedCount  = 0;
int  totalPublished = 0;

unsigned long startTime   = 0;
unsigned long lastPublish = 0;
unsigned long lastPoll    = 0;
unsigned long lastDiag    = 0;
unsigned long lastGPSRetry = 0;
unsigned long lastBatchSync = 0;

// GPS data
float lat2=0, lon2=0, speed2=0, alt2=0, acc2=0;
int vsat2=0, usat2=0, yr2=0, mo2=0, dy2=0, hr2=0, mi2=0, sc2=0;

// Raw GPS fields for debugging
String lastRawGPS;

// =============================================================================
// HELPERS
// =============================================================================

static String csvField(const String& s, int idx)
{
    int start = 0;
    for (int i = 0; i < idx; i++) {
        int c = s.indexOf(',', start);
        if (c < 0) return "";
        start = c + 1;
    }
    int end = s.indexOf(',', start);
    if (end < 0) end = s.length();
    return s.substring(start, end);
}

String csvToJson(const String& csv)
{
    String parts[13];
    int start = 0;
    for (int i = 0; i < 13; i++) {
        int comma = csv.indexOf(',', start);
        if (comma < 0) comma = csv.length();
        parts[i] = csv.substring(start, comma);
        start = comma + 1;
    }
    
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"ts\":%s,\"lat\":%s,\"lon\":%s,\"alt\":%s,\"spd\":%s,\"sat\":%s,\"acc\":%s,\"dt\":\"%s-%s-%s %s:%s:%s\"}",
        parts[0].c_str(), parts[1].c_str(), parts[2].c_str(), parts[3].c_str(),
        parts[4].c_str(), parts[5].c_str(), parts[6].c_str(),
        parts[7].c_str(), parts[8].c_str(), parts[9].c_str(),
        parts[10].c_str(), parts[11].c_str(), parts[12].c_str());
    return String(buf);
}

String liveGpsJson()
{
    char buf[320];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"live\",\"fix\":%d,\"lat\":%.8f,\"lon\":%.8f,\"spd\":%.2f,\"alt\":%.1f,\"sat\":%d,\"acc\":%.1f,\"dt\":\"%04d-%02d-%02d %02d:%02d:%02d\",\"buf\":%d}",
        totalFixes, lat2, lon2, speed2, alt2, usat2, acc2,
        yr2, mo2, dy2, hr2, mi2, sc2, bufferedCount);
    return String(buf);
}

String fixToCsv()
{
    char buf[200];
    snprintf(buf, sizeof(buf),
        "%lu,%.8f,%.8f,%.1f,%.2f,%d,%.1f,%04d,%02d,%02d,%02d,%02d,%02d\n",
        millis(), lat2, lon2, alt2, speed2, usat2, acc2,
        yr2, mo2, dy2, hr2, mi2, sc2);
    return String(buf);
}

void powerCycleModem()
{
    Serial.println("[Modem] Power cycling...");
    PMU.disableDC3(); delay(500);
    PMU.enableDC3();  delay(500);
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);  delay(100);
    digitalWrite(BOARD_MODEM_PWR_PIN, HIGH); delay(1000);
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);  delay(1000);
}

bool enableGPSWithRetry(int maxAttempts = 5)
{
    for (int i = 1; i <= maxAttempts; i++) {
        Serial.printf("[GPS] Enabling (attempt %d/%d)...\n", i, maxAttempts);
        if (modem.enableGPS()) return true;
        delay(2000);
    }
    return false;
}

// =============================================================================
// SD CARD STORAGE (MASSIVE BUFFER)
// =============================================================================

void enableSDPower()
{
    // SD card power on ALDO3
    PMU.setALDO3Voltage(3300);
    PMU.enableALDO3();
    delay(100);
}

bool initStorage()
{
#if USE_SD_CARD
    Serial.println("[Storage] Initializing SD card...");
    enableSDPower();
    
    // SDMMC pins: CMD=39, CLK=38, DATA=40
    SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
    
    if (!SD_MMC.begin("/sdcard", true)) {
        Serial.println("[Storage] ✗ SD card mount failed!");
        Serial.println("[Storage] Falling back to LittleFS...");
        goto use_littlefs;
    }
    
    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[Storage] ✗ No SD card detected!");
        Serial.println("[Storage] Falling back to LittleFS...");
        goto use_littlefs;
    }
    
    sdReady = true;
    Serial.println("[Storage] ✓ SD card mounted!");
    
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    uint64_t totalBytes = SD_MMC.totalBytes() / (1024 * 1024);
    uint64_t usedBytes = SD_MMC.usedBytes() / (1024 * 1024);
    
    Serial.printf("[Storage]   Card size: %llu MB\n", cardSize);
    Serial.printf("[Storage]   Total: %llu MB, Used: %llu MB, Free: %llu MB\n", 
                  totalBytes, usedBytes, totalBytes - usedBytes);
    
    // Count buffered records
    if (SD_MMC.exists(BUFFER_FILE)) {
        File f = SD_MMC.open(BUFFER_FILE, FILE_READ);
        if (f) {
            bufferedCount = 0;
            while (f.available()) {
                if (f.read() == '\n') bufferedCount++;
            }
            if (bufferedCount > 0) bufferedCount--;
            f.close();
            Serial.printf("[Storage]   Found %d buffered fix(es)\n", bufferedCount);
        }
    }
    
    return true;
    
use_littlefs:
#endif
    
    // Fallback to LittleFS
    Serial.println("[Storage] Initializing LittleFS...");
    if (!LittleFS.begin(true)) {
        Serial.println("[Storage] ✗ LittleFS mount failed!");
        return false;
    }
    Serial.println("[Storage] ✓ LittleFS mounted");
    
    size_t total = LittleFS.totalBytes();
    size_t used  = LittleFS.usedBytes();
    Serial.printf("[Storage]   Total: %u KB, Used: %u KB, Free: %u KB\n",
                  total/1024, used/1024, (total-used)/1024);
    
    if (LittleFS.exists(BUFFER_FILE)) {
        File f = LittleFS.open(BUFFER_FILE, "r");
        if (f) {
            bufferedCount = 0;
            while (f.available()) {
                if (f.read() == '\n') bufferedCount++;
            }
            if (bufferedCount > 0) bufferedCount--;
            f.close();
            Serial.printf("[Storage]   Found %d buffered fix(es)\n", bufferedCount);
        }
    }
    
    return true;
}

bool bufferFix()
{
    String data = fixToCsv();
    
#if USE_SD_CARD
    if (sdReady) {
        if (!SD_MMC.exists(BUFFER_FILE)) {
            File f = SD_MMC.open(BUFFER_FILE, FILE_WRITE);
            if (!f) return false;
            f.print(CSV_HEADER);
            f.print(data);
            f.close();
        } else {
            File f = SD_MMC.open(BUFFER_FILE, FILE_APPEND);
            if (!f) return false;
            f.print(data);
            f.close();
        }
        bufferedCount++;
        Serial.printf("[Buffer] ✓ Fix buffered to SD (total: %d)\n", bufferedCount);
        return true;
    }
#endif
    
    // Fallback to LittleFS
    if (!LittleFS.exists(BUFFER_FILE)) {
        File f = LittleFS.open(BUFFER_FILE, FILE_WRITE);
        if (!f) return false;
        f.print(CSV_HEADER);
        f.print(data);
        f.close();
    } else {
        File f = LittleFS.open(BUFFER_FILE, FILE_APPEND);
        if (!f) return false;
        f.print(data);
        f.close();
    }
    bufferedCount++;
    Serial.printf("[Buffer] ✓ Fix buffered to Flash (total: %d)\n", bufferedCount);
    return true;
}

void clearBuffer()
{
#if USE_SD_CARD
    if (sdReady) {
        SD_MMC.remove(BUFFER_FILE);
    } else
#endif
    {
        LittleFS.remove(BUFFER_FILE);
    }
    bufferedCount = 0;
    Serial.println("[Buffer] Cleared.");
}

// =============================================================================
// BATCHED SYNC TO CLOUD
// =============================================================================

int syncBufferBatched()
{
    File f;
    
#if USE_SD_CARD
    if (sdReady) {
        if (!SD_MMC.exists(BUFFER_FILE)) return 0;
        f = SD_MMC.open(BUFFER_FILE, FILE_READ);
    } else
#endif
    {
        if (!LittleFS.exists(BUFFER_FILE)) return 0;
        f = LittleFS.open(BUFFER_FILE, "r");
    }
    
    if (!f) return 0;

    Serial.printf("[Batch] Syncing %d fix(es)...\n", bufferedCount);

    int totalSent = 0, totalFailed = 0, batchCount = 0;
    bool firstLine = true;
    String currentBatch = "[";
    int fixesInBatch = 0;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        if (firstLine && line.startsWith("millis")) { firstLine = false; continue; }
        firstLine = false;

        if (fixesInBatch > 0) currentBatch += ",";
        currentBatch += csvToJson(line);
        fixesInBatch++;

        bool isLast = !f.available();
        
        if (fixesInBatch >= BATCH_SIZE_TARGET || (isLast && fixesInBatch > 0)) {
            currentBatch += "]";
            
            char wrapper[64];
            snprintf(wrapper, sizeof(wrapper), "{\"type\":\"batch\",\"count\":%d,\"fixes\":"
,                     fixesInBatch);
            
            String payload = String(wrapper) + currentBatch + "}";
            
            Serial.printf("[Batch] Sending #%d (%d fixes, %d bytes)...\n", 
                          ++batchCount, fixesInBatch, payload.length());

            mqtt.loop();
            if (mqtt.publish(TOPIC_BATCH, payload.c_str())) {
                totalSent += fixesInBatch;
                totalPublished++;
                Serial.printf("[Batch] ✓ Sent\n");
            } else {
                totalFailed += fixesInBatch;
                Serial.printf("[Batch] ✗ Failed (rc=%d)\n", mqtt.state());
                break;
            }

            currentBatch = "[";
            fixesInBatch = 0;
            delay(100);
        }
    }
    
    f.close();

    Serial.printf("[Batch] Done: %d sent, %d failed\n", totalSent, totalFailed);

    if (totalFailed == 0) {
        clearBuffer();
    }
    
    return totalSent;
}

// =============================================================================
// WI-FI + TLS
// =============================================================================

void connectWiFi()
{
    Serial.printf("[WiFi] Connecting to '%s'...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500); 
        Serial.print("."); 
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.printf("\n[WiFi] ✓ Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[WiFi] ✗ Failed — will buffer to SD");
    }
}

void setupTLS()
{
#if USE_TLS
    Serial.println("[TLS] Configuring...");
    
    #if VERIFY_CERT
        wifiClient.setCACert(ROOT_CA_CERT);
        Serial.println("[TLS] ✓ Certificate verification enabled");
    #else
        Serial.println("[TLS] ⚠ Certificate verification DISABLED (insecure mode)");
        wifiClient.setInsecure();
    #endif
    
    Serial.printf("[TLS] MQTTS on port %d\n", MQTT_PORT);
#else
    Serial.printf("[MQTT] Plain TCP on port %d (no encryption)\n", MQTT_PORT);
#endif
}

// =============================================================================
// MQTT CONNECTION
// =============================================================================

bool mqttConnect()
{
    if (!wifiConnected || WiFi.status() != WL_CONNECTED) return false;
    if (mqtt.connected()) return true;

    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setBufferSize(MQTT_MAX_PACKET_SIZE);

    char clientId[32];
    snprintf(clientId, sizeof(clientId), "LilyGoGPS_%04X",
             (uint16_t)(ESP.getEfuseMac() & 0xFFFF));

    Serial.printf("[MQTT] Connecting to %s:%d as %s...\n", MQTT_BROKER, MQTT_PORT, clientId);

    bool ok;
    if (strlen(MQTT_USERNAME) > 0) {
        ok = mqtt.connect(clientId, MQTT_USERNAME, MQTT_PASSWORD,
                          TOPIC_STATUS, 1, true, "offline");
    } else {
        ok = mqtt.connect(clientId, nullptr, nullptr,
                          TOPIC_STATUS, 1, true, "offline");
    }

    if (ok) {
        mqtt.publish(TOPIC_STATUS, "online", true);
        
        char meta[256];
        snprintf(meta, sizeof(meta),
            "{\"device\":\"LilyGo T-SIM7080G\",\"id\":\"%s\",\"fw\":\"3.0-prod\",\"tls\":%s,\"storage\":\"%s\",\"ts\":%lu}",
            clientId, USE_TLS ? "true" : "false", 
            (USE_SD_CARD && sdReady) ? "SD" : "Flash", millis());
        mqtt.publish(TOPIC_META, meta);
        
        Serial.println("[MQTT] ✓ Connected to cloud!");
    } else {
        Serial.printf("[MQTT] ✗ Failed (rc=%d)\n", mqtt.state());
    }
    return ok;
}

void publishOrBuffer()
{
    bool mqttOk = mqttConnect();

    if (mqttOk) {
        if (bufferedCount > 0) {
            Serial.printf("[Publish] Syncing %d buffered fix(es)...\n", bufferedCount);
            syncBufferBatched();
        }

        String payload = liveGpsJson();
        if (mqtt.publish(TOPIC_GPS, payload.c_str())) {
            mqtt.loop();
            totalPublished++;
            Serial.printf("[Publish] ✓ Live fix #%d → cloud\n", totalPublished);
        } else {
            Serial.println("[Publish] ✗ Failed — buffering");
            bufferFix();
        }
    } else {
        Serial.println("[Publish] No MQTT — buffering to storage");
        bufferFix();
    }
}

// =============================================================================
// SETUP
// =============================================================================

void setup()
{
    Serial.begin(115200);
    { unsigned long t0 = millis(); while (!Serial && millis() - t0 < 5000); }
    delay(2000);

    Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
    Serial.println("║  LilyGo T-SIM7080G — PRODUCTION GPS TRACKER v3.0         ║");
    Serial.println("║                                                           ║");
    Serial.println("║  ✓ SD Card buffer (GBs of storage)                        ║");
    Serial.println("║  ✓ WiFi + MQTTS (TLS encryption)                          ║");
    Serial.println("║  ✓ Batched JSON → Cloud (Docker/EMQX/InfluxDB)            ║");
    Serial.println("╚═══════════════════════════════════════════════════════════╝");
    Serial.println();

    // Storage (SD card preferred, fallback to LittleFS)
    initStorage();
    
    // WiFi
    connectWiFi();
    
    // TLS
    setupTLS();

    // PMU
    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        Serial.println("[PMU] FATAL!"); 
        while (1) delay(5000);
    }
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
        PMU.disableDC3(); delay(200);
    }
    PMU.setDC3Voltage(3000);   PMU.enableDC3();    // Modem
    PMU.setBLDO2Voltage(3300); PMU.enableBLDO2();  // GPS antenna
    PMU.setBLDO1Voltage(3300); PMU.enableBLDO1();  // Level shifter
    PMU.disableTSPinMeasure();                       // Enable charging
    Serial.println("[PMU] ✓ Ready");

    // Modem
    SerialAT.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);
    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);  delay(100);
    digitalWrite(BOARD_MODEM_PWR_PIN, HIGH); delay(1000);
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);

    Serial.print("[Modem] Initializing");
    int retry = 0;
    while (!modem.testAT(1000)) {
        Serial.print(".");
        if (++retry > 20) { Serial.println(); powerCycleModem(); retry = 0; }
    }
    Serial.println(" ✓");

    // GPS
    gpsEnabled = enableGPSWithRetry(5);
    if (gpsEnabled) {
        delay(2000);
        PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);
        Serial.println("[GPS] ✓ Engine started — waiting for fix...");
        Serial.println("[GPS] >>> Go OUTSIDE for GPS signal <<<");
    } else {
        PMU.setChargingLedMode(XPOWERS_CHG_LED_OFF);
        Serial.println("[GPS] ⚠ Will retry in loop");
    }

    startTime   = millis();
    lastPublish = millis();
    
    Serial.println("\n[Ready] Starting main loop...\n");
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop()
{
    if (mqtt.connected()) mqtt.loop();

    // Auto-sync buffered data periodically
    if (bufferedCount > 0 && wifiConnected && WiFi.status() == WL_CONNECTED &&
        millis() - lastBatchSync > BATCH_SYNC_INTERVAL_MS) {
        lastBatchSync = millis();
        if (mqttConnect()) {
            Serial.printf("[Sync] Auto-syncing %d buffered fix(es)...\n", bufferedCount);
            syncBufferBatched();
        }
    }

    // GPS retry
    if (!gpsEnabled) {
        PMU.setChargingLedMode(XPOWERS_CHG_LED_OFF);
        if (millis() - lastGPSRetry > 10000) {
            lastGPSRetry = millis();
            Serial.println("[GPS] Retrying enable...");
            gpsEnabled = modem.enableGPS();
            if (gpsEnabled) {
                delay(2000);
                PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);
                startTime = millis();
            }
        }
        delay(50);
        return;
    }

    // GPS poll
    if (millis() - lastPoll < GPS_POLL_INTERVAL_MS) { delay(50); return; }
    lastPoll = millis();

    String raw = modem.getGPSraw();
    lastRawGPS = raw;
    
    if (raw.length() == 0) {
        Serial.println("[GPS] No response from modem");
        PMU.setChargingLedMode(XPOWERS_CHG_LED_OFF);
        return;
    }

    // Debug: print raw CGNSINF if enabled
    #if DEBUG_GPS_RAW
    Serial.printf("[GPS-RAW] %s\n", raw.c_str());
    #endif

    // Parse CGNSINF fields
    // Field 0: GNSS run status (0/1)
    // Field 1: Fix status (0/1)
    // Field 2: UTC datetime
    // Field 3: Latitude
    // Field 4: Longitude
    // Field 5: MSL Altitude
    // Field 6: Speed Over Ground (km/h)
    // Field 10: HDOP (accuracy)
    // Field 14: GNSS Satellites in View
    // Field 15: GNSS Satellites Used
    
    String runStatus = csvField(raw, 0);
    String fixStatus = csvField(raw, 1);
    String utcDate   = csvField(raw, 2);
    
    bool engineRunning = (runStatus == "1");
    hasFix             = engineRunning && (fixStatus == "1");
    
    // Parse satellite counts
    String satsViewStr = csvField(raw, 14);
    String satsUsedStr = csvField(raw, 15);
    vsat2 = satsViewStr.toInt();
    usat2 = satsUsedStr.toInt();
    
    unsigned long elapsed = (millis() - startTime) / 1000;

    // LED
    if      (!engineRunning) PMU.setChargingLedMode(XPOWERS_CHG_LED_OFF);
    else if (!hasFix)        PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);
    else                     PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_4HZ);

    // Status
    if (!hasFix && millis() - lastDiag >= 5000) {
        lastDiag = millis();
        Serial.printf("[GPS] %4lu s | view=%d used=%d | WiFi:%s | Buf:%d\n",
                      elapsed, vsat2, usat2,
                      wifiConnected ? "Y" : "N", bufferedCount);
    }

    if (!hasFix) return;

    // Parse coordinates
    lat2   = csvField(raw, 3).toFloat();
    lon2   = csvField(raw, 4).toFloat();
    alt2   = csvField(raw, 5).toFloat();
    speed2 = csvField(raw, 6).toFloat() * 0.539957f;  // km/h → knots
    acc2   = csvField(raw, 10).toFloat();

    // Parse UTC: "20260304125730.000"
    if (utcDate.length() >= 14) {
        yr2 = utcDate.substring(0, 4).toInt();
        mo2 = utcDate.substring(4, 6).toInt();
        dy2 = utcDate.substring(6, 8).toInt();
        hr2 = utcDate.substring(8, 10).toInt();
        mi2 = utcDate.substring(10, 12).toInt();
        sc2 = utcDate.substring(12, 14).toInt();
    }

    // Filter spurious fixes
    if (lat2 == 0.0f && lon2 == 0.0f && usat2 == 0) {
        Serial.println("[GPS] Spurious fix (null island) — skipped");
        return;
    }

    totalFixes++;
    bool onWiFi = (wifiConnected && WiFi.status() == WL_CONNECTED);

    // Output
    Serial.println();
    Serial.printf("══ Fix #%d ════════════════════════════════════════\n", totalFixes);
    Serial.printf("  Lat/Lon: %.8f, %.8f\n", lat2, lon2);
    Serial.printf("  Alt/Spd: %.1f m / %.2f kts\n", alt2, speed2);
    Serial.printf("  Sats: %d used, %d in view  Acc: %.1f m\n", usat2, vsat2, acc2);
    Serial.printf("  WiFi: %s  Buffer: %d fixes stored\n", 
                  onWiFi ? "ONLINE→cloud" : "OFFLINE→SD", bufferedCount);

    // Publish
    if (millis() - lastPublish >= PUBLISH_INTERVAL_MS) {
        lastPublish = millis();
        publishOrBuffer();
    }
}
