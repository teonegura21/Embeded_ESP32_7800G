/**
 * @file      config.h
 * @brief     Configuration file for GPS Tracker with Offline Buffer
 * @date      2024-01-01
 *
 * @attention Update these settings with your actual credentials before deploying!
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// GPS CONFIGURATION
// =============================================================================

// GPS update interval in milliseconds (default: 30 seconds)
#define GPS_UPDATE_INTERVAL_MS      30000

// GPS fix timeout in milliseconds (default: 120 seconds)
#define GPS_FIX_TIMEOUT_MS          120000

// Minimum accuracy required for valid GPS fix (in meters)
#define GPS_MIN_ACCURACY            50.0

// Number of satellites required for a valid fix
#define GPS_MIN_SATELLITES          4

// =============================================================================
// SD CARD / OFFLINE BUFFER CONFIGURATION
// =============================================================================

// Maximum number of GPS records to store in buffer file
#define MAX_BUFFERED_RECORDS        1000

// Buffer file path on SD card
#define BUFFER_FILE_PATH            "/gps_buffer.csv"

// Log file path for debug messages
#define LOG_FILE_PATH               "/gps_tracker.log"

// Sync buffer to SD card every N records
#define BUFFER_SYNC_INTERVAL        5

// =============================================================================
// MQTT CONFIGURATION - UPDATE THESE!
// =============================================================================

// MQTT Broker settings
// Example: "mqtt.mydevices.com", "broker.hivemq.com", or your own broker IP
#define MQTT_SERVER                 "broker.hivemq.com"
#define MQTT_PORT                   1883

// MQTT Authentication (leave empty if no auth required)
#define MQTT_USERNAME               ""
#define MQTT_PASSWORD               ""

// MQTT Client ID (must be unique for each device)
// Use a unique ID like "GPSTracker_001" or leave empty for auto-generated
#define MQTT_CLIENT_ID              ""

// MQTT Topics
#define MQTT_TOPIC_GPS              "gps/tracker/location"
#define MQTT_TOPIC_STATUS           "gps/tracker/status"
#define MQTT_TOPIC_BATTERY          "gps/tracker/battery"

// MQTT Quality of Service (0, 1, or 2)
#define MQTT_QOS                    1

// MQTT Retain flag for messages
#define MQTT_RETAIN                 0

// =============================================================================
// CELLULAR NETWORK CONFIGURATION - UPDATE THESE!
// =============================================================================

// SIM Card PIN - Leave empty string "" if no PIN required
// If your SIM has a PIN, enter it here (4 digits)
#define SIM_CARD_PIN                ""

// APN settings - Update based on your SIM card provider
// Common APNs:
//   - Generic/Cheap IoT SIMs: "internet" or "apn"
//   - Vodafone IoT: "vfd1.konyne"
//   - T-Mobile IoT: "fast.t-mobile.com"
//   - AT&T IoT: "m2m.com.attz"
#define GPRS_APN                    "internet"
#define GPRS_USER                   ""
#define GPRS_PASS                   ""

// Network mode: 1=CAT-M, 2=NB-IoT, 3=Both
#define NETWORK_MODE                2

// Preferred mode: MODEM_CATM=1, MODEM_NB_IOT=2, MODEM_CATM_NBIOT=3
#define PREFERRED_MODE              MODEM_NB_IOT

// =============================================================================
// POWER MANAGEMENT CONFIGURATION
// =============================================================================

// Enable deep sleep between updates (saves battery)
#define ENABLE_DEEP_SLEEP           false

// Deep sleep duration in seconds (if enabled)
#define DEEP_SLEEP_SECONDS          60

// Battery voltage thresholds
#define BATTERY_LOW_VOLTAGE         3.3f    // Low battery warning
#define BATTERY_CRITICAL_VOLTAGE    3.0f    // Critical, enter emergency mode

// =============================================================================
// TRACKING MODES
// =============================================================================

// Tracking mode
enum TrackingMode {
    MODE_REALTIME = 0,      // Send every GPS fix immediately
    MODE_BATCHED = 1,       // Collect and send in batches
    MODE_ADAPTIVE = 2       // Adaptive based on movement
};

#define DEFAULT_TRACKING_MODE       MODE_BATCHED

// Batch size for batched mode
#define BATCH_SIZE                  10

// =============================================================================
// DEBUG CONFIGURATION
// =============================================================================

// Enable debug output to Serial
#define DEBUG_ENABLED               true

// Enable AT command debugging (verbose modem output)
#define DUMP_AT_COMMANDS

// Enable SD card logging
#define SD_LOGGING_ENABLED          true

#endif // CONFIG_H
