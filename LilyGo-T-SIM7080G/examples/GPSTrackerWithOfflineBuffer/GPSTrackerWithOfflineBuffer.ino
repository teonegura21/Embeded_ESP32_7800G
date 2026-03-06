/**
 * @file      GPSTrackerWithOfflineBuffer.ino
 * @brief     Complete GPS Tracker with Offline Buffer and MQTT Communication
 * @author    Based on LilyGo examples
 * @date      2024-01-01
 *
 * @description
 * This is a complete GPS tracking solution for the LilyGo T-SIM7080G board.
 * Features:
 *   - GPS positioning with configurable accuracy
 *   - Offline buffering using SD card (stores data when no network)
 *   - MQTT communication to send location data
 *   - Automatic sync of buffered data when connection restored
 *   - Battery monitoring and power management
 *   - Smart switching between GPS and cellular (SIM7080G limitation)
 *
 * Hardware Requirements:
 *   - LilyGo T-SIM7080G board
 *   - SIM card with NB-IoT or CAT-M support
 *   - GPS antenna
 *   - SD card (for offline buffering)
 *   - Battery (optional but recommended)
 *
 * Setup Instructions:
 *   1. Update config.h with your MQTT broker and APN settings
 *   2. Insert SIM card (before powering on!)
 *   3. Insert SD card
 *   4. Connect GPS antenna
 *   5. Upload this sketch
 *   6. Open Serial Monitor at 115200 baud
 */

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include "config.h"

// Power Management
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
XPowersPMU PMU;

// Modem
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

// Utilities
#include "utilities.h"

// =============================================================================
// DATA STRUCTURES
// =============================================================================

struct GPSData {
    float latitude;
    float longitude;
    float altitude;
    float speed;
    float accuracy;
    int satellites;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    uint32_t timestamp;
    float batteryVoltage;
    bool valid;
};

enum NetworkState {
    NET_DISCONNECTED = 0,
    NET_CONNECTING,
    NET_CONNECTED,
    NET_ERROR
};

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

GPSData lastGPSData;
NetworkState netState = NET_DISCONNECTED;
bool gpsEnabled = false;
bool sdCardAvailable = false;
uint32_t lastGPSTimestamp = 0;
uint32_t bufferedRecordCount = 0;
uint32_t totalRecordsSent = 0;
bool mqttConnected = false;
char mqttClientID[32];

// Network registration info strings
const char* register_info[] = {
    "Not registered, not searching",
    "Registered, home network",
    "Not registered, searching",
    "Registration denied",
    "Unknown",
    "Registered, roaming"
};

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

// Power Management
bool initializePMU();
void enableModemPower();
void disableModemPower();
void enableGPSPower();
void disableGPSPower();
void enableSDCardPower();
float getBatteryVoltage();

// SD Card
bool initializeSDCard();
void logToSD(const char* message);
bool bufferGPSRecord(const GPSData& data);
bool syncBufferedRecords();
uint32_t countBufferedRecords();
void clearBufferFile();

// GPS
bool initializeGPS();
bool getGPSFix(GPSData& data);
void disableGPS();

// Modem/Network
bool initializeModem();
bool unlockSIM();
bool connectNetwork();
void disconnectNetwork();
bool isNetworkConnected();

// MQTT
bool connectMQTT();
void disconnectMQTT();
bool publishGPSData(const GPSData& data);
bool isMQTTConnected();

// Utilities
void debugPrint(const char* msg);
void debugPrintln(const char* msg);
void logMessage(const char* level, const char* msg);
String gpsDataToJSON(const GPSData& data);
String gpsDataToCSV(const GPSData& data);
uint32_t getTimestamp();
void enterDeepSleep();
void printStatus();

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000); // Wait for serial with timeout
    
    debugPrintln("\n========================================");
    debugPrintln("  GPS Tracker with Offline Buffer");
    debugPrintln("========================================\n");
    
    // Initialize PMU (Power Management)
    if (!initializePMU()) {
        debugPrintln("FATAL: PMU initialization failed!");
        while (1) { delay(1000); }
    }
    debugPrintln("✓ PMU initialized");
    
    // Enable SD Card power
    enableSDCardPower();
    
    // Initialize SD Card for offline buffering
    sdCardAvailable = initializeSDCard();
    if (sdCardAvailable) {
        debugPrintln("✓ SD Card initialized");
        bufferedRecordCount = countBufferedRecords();
        debugPrint("  Buffered records: ");
        debugPrintln(String(bufferedRecordCount).c_str());
    } else {
        debugPrintln("✗ SD Card not available - will run without offline buffer");
    }
    
    // Check buffered records
    if (sdCardAvailable && bufferedRecordCount > 0) {
        logMessage("INFO", "Found buffered records to sync");
    }
    
    // Generate MQTT client ID if not set
    if (strlen(MQTT_CLIENT_ID) == 0) {
        snprintf(mqttClientID, sizeof(mqttClientID), "GPSTracker_%04X", 
                 (uint16_t)(ESP.getEfuseMac() & 0xFFFF));
    } else {
        strncpy(mqttClientID, MQTT_CLIENT_ID, sizeof(mqttClientID) - 1);
    }
    debugPrint("MQTT Client ID: ");
    debugPrintln(mqttClientID);
    
    logMessage("INFO", "Setup complete, starting main loop");
    
    printStatus();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
    static unsigned long lastCycleTime = 0;
    static unsigned long cycleCount = 0;
    
    unsigned long cycleStart = millis();
    cycleCount++;
    
    debugPrintln("\n----------------------------------------");
    debugPrint("Cycle #"); debugPrintln(String(cycleCount).c_str());
    
    // Step 1: Get GPS Fix
    // Important: GPS and Cellular cannot work simultaneously on SIM7080G!
    debugPrintln("\n[1/5] Acquiring GPS fix...");
    
    enableGPSPower();
    delay(500); // Allow power to stabilize
    
    if (!initializeGPS()) {
        logMessage("ERROR", "GPS initialization failed");
        delay(5000);
        return;
    }
    
    GPSData gpsData;
    bool gpsSuccess = getGPSFix(gpsData);
    
    // Disable GPS to save power and prepare for cellular
    disableGPS();
    disableGPSPower();
    
    if (!gpsSuccess || !gpsData.valid) {
        logMessage("WARN", "No valid GPS fix this cycle");
        // Continue anyway - we might have buffered data to send
    } else {
        lastGPSData = gpsData;
        lastGPSTimestamp = millis();
        
        debugPrintln("✓ GPS Fix acquired:");
        debugPrint("  Lat: "); debugPrintln(String(gpsData.latitude, 6).c_str());
        debugPrint("  Lon: "); debugPrintln(String(gpsData.longitude, 6).c_str());
        debugPrint("  Sats: "); debugPrintln(String(gpsData.satellites).c_str());
        debugPrint("  Accuracy: "); debugPrint(String(gpsData.accuracy).c_str()); debugPrintln("m");
        
        // Add battery info
        gpsData.batteryVoltage = getBatteryVoltage();
    }
    
    // Step 2: Check if we have valid GPS data to process
    if (!gpsData.valid) {
        debugPrintln("No valid GPS data, skipping network operations");
        delay(GPS_UPDATE_INTERVAL_MS);
        return;
    }
    
    // Step 3: Try to connect to network and send data
    debugPrintln("\n[2/5] Connecting to cellular network...");
    
    enableModemPower();
    delay(500);
    
    bool networkConnected = false;
    bool dataSent = false;
    
    if (initializeModem() && connectNetwork()) {
        netState = NET_CONNECTED;
        networkConnected = true;
        debugPrintln("✓ Network connected");
        
        // Step 4: Connect to MQTT and send data
        debugPrintln("\n[3/5] Connecting to MQTT broker...");
        
        if (connectMQTT()) {
            mqttConnected = true;
            debugPrintln("✓ MQTT connected");
            
            // Step 5: Send current GPS data
            debugPrintln("\n[4/5] Publishing current GPS data...");
            if (publishGPSData(gpsData)) {
                dataSent = true;
                totalRecordsSent++;
                debugPrintln("✓ GPS data published");
            } else {
                logMessage("ERROR", "Failed to publish GPS data");
            }
            
            // Step 6: Sync any buffered records
            if (sdCardAvailable && bufferedRecordCount > 0) {
                debugPrintln("\n[5/5] Syncing buffered records...");
                if (syncBufferedRecords()) {
                    debugPrintln("✓ Buffered records synced");
                    bufferedRecordCount = 0;
                } else {
                    logMessage("WARN", "Some buffered records failed to sync");
                }
            }
            
            disconnectMQTT();
        } else {
            logMessage("ERROR", "MQTT connection failed");
        }
        
        disconnectNetwork();
    } else {
        netState = NET_ERROR;
        logMessage("ERROR", "Network connection failed");
    }
    
    // Disable modem to save power
    disableModemPower();
    
    // Step 7: If data wasn't sent, buffer it for later
    if (!dataSent && sdCardAvailable) {
        debugPrintln("\n[*] Buffering GPS record for later sync...");
        if (bufferGPSRecord(gpsData)) {
            bufferedRecordCount++;
            debugPrintln("✓ Record buffered to SD card");
        } else {
            logMessage("ERROR", "Failed to buffer GPS record");
        }
    }
    
    // Print status summary
    printStatus();
    
    // Calculate sleep time
    unsigned long cycleDuration = millis() - cycleStart;
    long sleepTime = GPS_UPDATE_INTERVAL_MS - cycleDuration;
    
    if (sleepTime > 0) {
        debugPrint("\nSleeping for "); 
        debugPrint(String(sleepTime / 1000).c_str()); 
        debugPrintln(" seconds...");
        
        #if ENABLE_DEEP_SLEEP
        // Configure deep sleep
        enterDeepSleep();
        #else
        delay(sleepTime);
        #endif
    }
}


// =============================================================================
// POWER MANAGEMENT IMPLEMENTATION
// =============================================================================

bool initializePMU() {
    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        return false;
    }
    
    // If cold boot, power cycle the modem
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
        PMU.disableDC3();
        delay(200);
    }
    
    // CRITICAL: NEVER disable BLDO1 - it powers the level converter!
    PMU.setBLDO1Voltage(3300);
    PMU.enableBLDO1();
    
    // Disable TS Pin detection for charging
    PMU.disableTSPinMeasure();
    
    return true;
}

void enableModemPower() {
    // Modem main power on DC3
    PMU.setDC3Voltage(3000);  // SIM7080 requires 3000mV - DO NOT CHANGE!
    PMU.enableDC3();
    delay(100);
}

void disableModemPower() {
    PMU.disableDC3();
}

void enableGPSPower() {
    // GPS antenna power on BLDO2
    PMU.setBLDO2Voltage(3300);
    PMU.enableBLDO2();
    delay(100);
}

void disableGPSPower() {
    PMU.disableBLDO2();
}

void enableSDCardPower() {
    // SD Card power on ALDO3
    PMU.setALDO3Voltage(3300);
    PMU.enableALDO3();
    delay(100);
}

float getBatteryVoltage() {
    return PMU.getBattVoltage() / 1000.0f;
}

// =============================================================================
// SD CARD IMPLEMENTATION
// =============================================================================

bool initializeSDCard() {
    SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
    
    if (!SD_MMC.begin("/sdcard", true)) {
        return false;
    }
    
    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        return false;
    }
    
    return true;
}

void logToSD(const char* message) {
    if (!sdCardAvailable) return;
    
    File file = SD_MMC.open(LOG_FILE_PATH, FILE_APPEND);
    if (file) {
        char timestamp[32];
        snprintf(timestamp, sizeof(timestamp), "[%lu] ", millis());
        file.print(timestamp);
        file.println(message);
        file.close();
    }
}

bool bufferGPSRecord(const GPSData& data) {
    if (!sdCardAvailable) return false;
    
    File file = SD_MMC.open(BUFFER_FILE_PATH, FILE_APPEND);
    if (!file) {
        return false;
    }
    
    String csv = gpsDataToCSV(data);
    bool success = file.println(csv);
    file.close();
    
    return success;
}

bool syncBufferedRecords() {
    if (!sdCardAvailable) return false;
    
    File file = SD_MMC.open(BUFFER_FILE_PATH, FILE_READ);
    if (!file) {
        return true; // No file means nothing to sync
    }
    
    uint32_t synced = 0;
    uint32_t failed = 0;
    
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        
        if (line.length() == 0) continue;
        
        // Parse CSV and create GPSData (simplified)
        GPSData data;
        // Note: In a full implementation, parse the CSV back to GPSData
        // For now, we'll just send the raw CSV as a message
        
        // Publish buffered record
        char topic[128];
        snprintf(topic, sizeof(topic), "%s/buffered", MQTT_TOPIC_GPS);
        
        char payload[256];
        snprintf(payload, sizeof(payload), "+SMPUB=\"%s\",%d,%d,%d", 
                 topic, line.length(), MQTT_QOS, MQTT_RETAIN);
        
        modem.sendAT(payload);
        if (modem.waitResponse(">") == 1) {
            modem.stream.write(line.c_str(), line.length());
            if (modem.waitResponse(5000)) {
                synced++;
            } else {
                failed++;
            }
        } else {
            failed++;
        }
        
        delay(100); // Small delay between publishes
    }
    
    file.close();
    
    debugPrint("Synced: "); debugPrint(String(synced).c_str());
    debugPrint(", Failed: "); debugPrintln(String(failed).c_str());
    
    // Clear buffer file if all synced successfully
    if (failed == 0) {
        clearBufferFile();
    }
    
    return (failed == 0);
}

uint32_t countBufferedRecords() {
    if (!sdCardAvailable) return 0;
    
    File file = SD_MMC.open(BUFFER_FILE_PATH, FILE_READ);
    if (!file) return 0;
    
    uint32_t count = 0;
    while (file.available()) {
        if (file.read() == '\n') count++;
    }
    file.close();
    
    return count;
}

void clearBufferFile() {
    if (!sdCardAvailable) return;
    SD_MMC.remove(BUFFER_FILE_PATH);
}

// =============================================================================
// GPS IMPLEMENTATION
// =============================================================================

bool initializeGPS() {
    // Power on modem (needed for GPS functionality)
    Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);
    
    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
    
    // Power on sequence
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
    delay(1000);
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
    
    // Wait for modem to respond
    int retry = 0;
    while (!modem.testAT(1000)) {
        if (retry++ > 15) {
            // Retry power on
            digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
            delay(100);
            digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
            delay(1000);
            digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
            retry = 0;
        }
    }
    
    // Enable GPS
    if (!modem.enableGPS()) {
        return false;
    }
    
    gpsEnabled = true;
    return true;
}

bool getGPSFix(GPSData& data) {
    if (!gpsEnabled) return false;
    
    // Initialize data as invalid
    data.valid = false;
    
    // Variables for GPS data
    float lat = 0, lon = 0, speed = 0, alt = 0, accuracy = 0;
    int vsat = 0, usat = 0;
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, sec = 0;
    
    // Try to get GPS fix with timeout
    unsigned long startTime = millis();
    int attempts = 0;
    
    while (millis() - startTime < GPS_FIX_TIMEOUT_MS) {
        attempts++;
        
        if (modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy,
                         &year, &month, &day, &hour, &minute, &sec)) {
            
            // Check if fix meets quality criteria
            if (usat >= GPS_MIN_SATELLITES && accuracy <= GPS_MIN_ACCURACY && accuracy > 0) {
                data.latitude = lat;
                data.longitude = lon;
                data.altitude = alt;
                data.speed = speed;
                data.accuracy = accuracy;
                data.satellites = usat;
                data.year = year;
                data.month = month;
                data.day = day;
                data.hour = hour;
                data.minute = minute;
                data.second = sec;
                data.timestamp = millis();
                data.valid = true;
                
                debugPrint("GPS fix acquired in "); 
                debugPrint(String(attempts).c_str()); 
                debugPrintln(" attempts");
                return true;
            }
        }
        
        // Progress indicator every 10 seconds
        if (attempts % 10 == 0) {
            debugPrint("GPS attempt #"); 
            debugPrint(String(attempts).c_str());
            debugPrint(", Sats: "); 
            debugPrint(String(usat).c_str());
            debugPrint(", Acc: "); 
            debugPrint(String(accuracy).c_str()); 
            debugPrintln("m");
        }
        
        delay(1000);
    }
    
    debugPrintln("GPS fix timeout");
    return false;
}

void disableGPS() {
    if (gpsEnabled) {
        modem.disableGPS();
        gpsEnabled = false;
    }
}

// =============================================================================
// MODEM & NETWORK IMPLEMENTATION
// =============================================================================

bool initializeModem() {
    // Modem should already be powered on from GPS init
    // Just test AT communication
    int retry = 0;
    while (!modem.testAT(1000)) {
        if (retry++ > 5) {
            return false;
        }
        delay(1000);
    }
    
    // Restart modem for clean state
    modem.restart();
    
    // Try to unlock SIM if PIN is configured
    if (strlen(SIM_CARD_PIN) > 0) {
        if (!unlockSIM()) {
            logMessage("ERROR", "SIM PIN unlock failed");
            return false;
        }
    }
    
    // Check SIM
    if (modem.getSimStatus() != SIM_READY) {
        logMessage("ERROR", "SIM card not ready");
        return false;
    }
    
    return true;
}

bool unlockSIM() {
    debugPrint("Attempting to unlock SIM with PIN... ");
    
    // Check SIM status first
    SIM70xxSimStatus simStatus = modem.getSimStatus();
    
    if (simStatus == SIM_READY) {
        debugPrintln("SIM already ready (no PIN needed)");
        return true;
    }
    
    if (simStatus == SIM_LOCKED) {
        debugPrintln("SIM is PIN-locked, unlocking...");
        
        // Send PIN unlock command: AT+CPIN="PIN"
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "+CPIN=\"%s\"", SIM_CARD_PIN);
        
        modem.sendAT(cmd);
        int8_t rsp = modem.waitResponse(10000);
        
        if (rsp != 1) {
            debugPrintln("Failed!");
            logMessage("ERROR", "SIM PIN incorrect or blocked");
            return false;
        }
        
        // Wait a moment for SIM to initialize
        delay(2000);
        
        // Verify SIM is now ready
        simStatus = modem.getSimStatus();
        if (simStatus == SIM_READY) {
            debugPrintln("Success!");
            logMessage("INFO", "SIM unlocked successfully");
            return true;
        } else {
            debugPrintln("Failed - SIM still not ready");
            return false;
        }
    }
    
    if (simStatus == SIM_ERROR) {
        logMessage("ERROR", "SIM card error (may be damaged or not inserted)");
        return false;
    }
    
    // Unknown status
    debugPrint("Unknown SIM status: ");
    debugPrintln(String(simStatus).c_str());
    return false;
}

bool connectNetwork() {
    // Set network mode
    modem.setNetworkMode(2);  // Automatic
    modem.setPreferredMode(PREFERRED_MODE);
    
    debugPrint("Network mode: ");
    debugPrintln(String(modem.getNetworkMode()).c_str());
    
    // Wait for network registration
    debugPrintln("Waiting for network registration...");
    SIM70xxRegStatus s;
    unsigned long start = millis();
    
    do {
        s = modem.getRegistrationStatus();
        if (s != REG_OK_HOME && s != REG_OK_ROAMING) {
            delay(1000);
        }
        
        // Timeout after 60 seconds
        if (millis() - start > 60000) {
            logMessage("ERROR", "Network registration timeout");
            return false;
        }
    } while (s != REG_OK_HOME && s != REG_OK_ROAMING);
    
    debugPrint("Network: ");
    debugPrintln(register_info[s]);
    
    // Activate network bearer
    if (!modem.isGprsConnected()) {
        modem.sendAT("+CNACT=0,1");
        if (modem.waitResponse(30000) != 1) {
            logMessage("ERROR", "Failed to activate network bearer");
            return false;
        }
    }
    
    return true;
}

void disconnectNetwork() {
    modem.sendAT("+CNACT=0,0");
    modem.waitResponse();
}

bool isNetworkConnected() {
    return modem.isGprsConnected();
}


// =============================================================================
// MQTT IMPLEMENTATION
// =============================================================================

bool connectMQTT() {
    // Disconnect any existing connection
    modem.sendAT("+SMDISC");
    modem.waitResponse();
    
    char buffer[256];
    
    // Configure MQTT broker URL
    snprintf(buffer, sizeof(buffer), "+SMCONF=\"URL\",\"%s\",%d", MQTT_SERVER, MQTT_PORT);
    modem.sendAT(buffer);
    if (modem.waitResponse() != 1) {
        logMessage("ERROR", "MQTT URL config failed");
        return false;
    }
    
    // Configure username if provided
    if (strlen(MQTT_USERNAME) > 0) {
        snprintf(buffer, sizeof(buffer), "+SMCONF=\"USERNAME\",\"%s\"", MQTT_USERNAME);
        modem.sendAT(buffer);
        if (modem.waitResponse() != 1) {
            logMessage("ERROR", "MQTT username config failed");
            return false;
        }
    }
    
    // Configure password if provided
    if (strlen(MQTT_PASSWORD) > 0) {
        snprintf(buffer, sizeof(buffer), "+SMCONF=\"PASSWORD\",\"%s\"", MQTT_PASSWORD);
        modem.sendAT(buffer);
        if (modem.waitResponse() != 1) {
            logMessage("ERROR", "MQTT password config failed");
            return false;
        }
    }
    
    // Configure client ID
    snprintf(buffer, sizeof(buffer), "+SMCONF=\"CLIENTID\",\"%s\"", mqttClientID);
    modem.sendAT(buffer);
    if (modem.waitResponse() != 1) {
        logMessage("ERROR", "MQTT client ID config failed");
        return false;
    }
    
    // Connect to MQTT broker
    int8_t ret;
    int attempts = 0;
    do {
        modem.sendAT("+SMCONN");
        ret = modem.waitResponse(30000);
        if (ret != 1) {
            debugPrintln("MQTT connect failed, retrying...");
            delay(2000);
        }
        attempts++;
    } while (ret != 1 && attempts < 3);
    
    if (ret != 1) {
        logMessage("ERROR", "MQTT connection failed after retries");
        return false;
    }
    
    mqttConnected = true;
    return true;
}

void disconnectMQTT() {
    modem.sendAT("+SMDISC");
    modem.waitResponse();
    mqttConnected = false;
}

bool isMQTTConnected() {
    modem.sendAT("+SMSTATE?");
    if (modem.waitResponse("+SMSTATE: ") == 1) {
        String res = modem.stream.readStringUntil('\r');
        return res.toInt() == 1;
    }
    return false;
}

bool publishGPSData(const GPSData& data) {
    if (!isMQTTConnected()) {
        return false;
    }
    
    // Create JSON payload
    String json = gpsDataToJSON(data);
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "+SMPUB=\"%s\",%d,%d,%d", 
             MQTT_TOPIC_GPS, json.length(), MQTT_QOS, MQTT_RETAIN);
    
    modem.sendAT(cmd);
    if (modem.waitResponse(">") != 1) {
        return false;
    }
    
    // Send the payload
    modem.stream.write(json.c_str(), json.length());
    
    // Wait for response
    if (modem.waitResponse(10000)) {
        return true;
    }
    
    return false;
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

void debugPrint(const char* msg) {
    #if DEBUG_ENABLED
    Serial.print(msg);
    #endif
}

void debugPrintln(const char* msg) {
    #if DEBUG_ENABLED
    Serial.println(msg);
    #endif
}

void logMessage(const char* level, const char* msg) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "[%s] %s", level, msg);
    
    debugPrintln(buffer);
    
    #if SD_LOGGING_ENABLED
    logToSD(buffer);
    #endif
}

String gpsDataToJSON(const GPSData& data) {
    String json = "{";
    json += "\"lat\":" + String(data.latitude, 6) + ",";
    json += "\"lon\":" + String(data.longitude, 6) + ",";
    json += "\"alt\":" + String(data.altitude, 1) + ",";
    json += "\"speed\":" + String(data.speed, 1) + ",";
    json += "\"accuracy\":" + String(data.accuracy, 1) + ",";
    json += "\"sats\":" + String(data.satellites) + ",";
    json += "\"battery\":" + String(data.batteryVoltage, 2) + ",";
    json += "\"ts\":" + String(data.timestamp) + ",";
    json += "\"date\":\"" + String(data.year) + "-" + 
            String(data.month) + "-" + String(data.day) + "\",";
    json += "\"time\":\"" + String(data.hour) + ":" + 
            String(data.minute) + ":" + String(data.second) + "\"";
    json += "}";
    return json;
}

String gpsDataToCSV(const GPSData& data) {
    String csv = "";
    csv += String(data.latitude, 6) + ",";
    csv += String(data.longitude, 6) + ",";
    csv += String(data.altitude, 1) + ",";
    csv += String(data.speed, 1) + ",";
    csv += String(data.accuracy, 1) + ",";
    csv += String(data.satellites) + ",";
    csv += String(data.batteryVoltage, 2) + ",";
    csv += String(data.timestamp) + ",";
    csv += String(data.year) + "-" + String(data.month) + "-" + String(data.day) + ",";
    csv += String(data.hour) + ":" + String(data.minute) + ":" + String(data.second);
    return csv;
}

void printStatus() {
    debugPrintln("\n--- Status Summary ---");
    debugPrint("  Battery: "); 
    debugPrint(String(getBatteryVoltage()).c_str()); 
    debugPrintln("V");
    debugPrint("  SD Card: "); 
    debugPrintln(sdCardAvailable ? "Available" : "Not Available");
    debugPrint("  Buffered Records: "); 
    debugPrintln(String(bufferedRecordCount).c_str());
    debugPrint("  Total Sent: "); 
    debugPrintln(String(totalRecordsSent).c_str());
    debugPrint("  Last GPS: ");
    if (lastGPSTimestamp > 0) {
        debugPrint(String((millis() - lastGPSTimestamp) / 1000).c_str());
        debugPrintln("s ago");
    } else {
        debugPrintln("Never");
    }
    debugPrintln("----------------------");
}

void enterDeepSleep() {
    #if ENABLE_DEEP_SLEEP
    debugPrintln("Entering deep sleep...");
    
    // Make sure everything is powered off
    disableGPS();
    disableGPSPower();
    disableModemPower();
    
    // Configure wake up timer
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_SECONDS * 1000000ULL);
    
    // Enter deep sleep
    esp_deep_sleep_start();
    #endif
}
