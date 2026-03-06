/**
 * @file      config.h
 * @brief     Production Configuration for GPS Tracker
 * @date      2024-01-01
 * 
 * @attention PRODUCTION SETTINGS - Review carefully before deployment!
 * 
 * This configuration is optimized for long-term remote deployment with:
 * - Power saving modes (PSM/eDRX) for battery life up to 10 years
 * - MQTT reliability features (LWT, persistent sessions, QoS)
 * - SD card wear leveling for extended lifetime
 * - Automatic recovery and watchdog protection
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// FIRMWARE VERSION
// =============================================================================
#define FIRMWARE_VERSION            "2.0.0"
#define FIRMWARE_BUILD_DATE         __DATE__ " " __TIME__

// =============================================================================
// DEBUG & DIAGNOSTICS
// =============================================================================

// Enable debug output (disable for production to save power)
#define DEBUG_ENABLED               true

// Enable verbose AT command debugging (disable for production)
// #define DUMP_AT_COMMANDS

// Enable SD card logging (writes debug logs to SD)
#define SD_LOGGING_ENABLED          true

// Enable health monitoring and self-diagnostics
#define HEALTH_MONITORING_ENABLED   true

// Health report interval (seconds)
#define HEALTH_REPORT_INTERVAL_S    3600  // 1 hour

// =============================================================================
// HARDWARE WATCHDOG
// =============================================================================

// Enable hardware watchdog timer (STRONGLY recommended for remote deployment)
#define WATCHDOG_ENABLED            true

// Watchdog timeout (seconds) - must be longer than longest operation
// GPS fix timeout + network connect + MQTT publish + margin
#define WATCHDOG_TIMEOUT_S          300   // 5 minutes

// =============================================================================
// GPS CONFIGURATION
// =============================================================================

// GPS update interval (milliseconds)
// Longer intervals = better battery life
#define GPS_UPDATE_INTERVAL_MS      60000     // 1 minute (adjust as needed)
// #define GPS_UPDATE_INTERVAL_MS   300000   // 5 minutes (better battery)
// #define GPS_UPDATE_INTERVAL_MS   900000   // 15 minutes (best battery)

// GPS fix timeout (milliseconds)
#define GPS_FIX_TIMEOUT_MS          120000    // 2 minutes

// Minimum accuracy for valid fix (meters)
#define GPS_MIN_ACCURACY            50.0

// Minimum satellites for valid fix
#define GPS_MIN_SATELLITES          4

// GPS hot start timeout (faster when GPS was recently used)
#define GPS_HOT_START_TIMEOUT_MS    30000     // 30 seconds

// =============================================================================
// CELLULAR NETWORK CONFIGURATION
// =============================================================================

// SIM Card PIN (leave empty if no PIN)
#define SIM_CARD_PIN                ""

// APN settings - CRITICAL for connectivity!
#define GPRS_APN                    "internet"
#define GPRS_USER                   ""
#define GPRS_PASS                   ""

// Network mode: 1=CAT-M only, 2=NB-IoT only, 3=Both (auto)
#define NETWORK_MODE                2

// Preferred mode
#define PREFERRED_MODE              MODEM_NB_IOT

// Network registration timeout (milliseconds)
#define NETWORK_REGISTRATION_TIMEOUT_MS 120000  // 2 minutes

// =============================================================================
// POWER SAVING MODE (PSM) - EXTENDS BATTERY LIFE TO 10 YEARS!
// =============================================================================

// Enable SIM7080G Power Saving Mode (PSM) - HUGE battery savings!
#define PSM_ENABLED                 true

// PSM Active Time (T3324) - how long modem stays active after TX
// Format: encoded per 3GPP spec - "00000100" = 4 seconds
// "00000000" = disabled, "01000001" = 10 seconds, etc.
#define PSM_ACTIVE_TIME             "00000100"  // 4 seconds active

// PSM Periodic TAU (T3412) - how often modem wakes to check network
// "00100101" = 5 minutes, "01000011" = 1 hour, "01100111" = 10 hours
#define PSM_PERIODIC_TAU            "01100111"  // 10 hours (maximize sleep)

// Enable eDRX (Extended Discontinuous Reception)
#define EDRX_ENABLED                true

// eDRX cycle length (seconds)
#define EDRX_CYCLE_S                20

// =============================================================================
// MQTT CONFIGURATION
// =============================================================================

// MQTT Broker settings
#define MQTT_SERVER                 "broker.hivemq.com"
#define MQTT_PORT                   1883

// MQTT Authentication
#define MQTT_USERNAME               ""
#define MQTT_PASSWORD               ""

// MQTT Client ID (auto-generated if empty)
#define MQTT_CLIENT_ID              ""

// =============================================================================
// MQTT RELIABILITY FEATURES
// =============================================================================

// Keep Alive interval (seconds) - how often to send heartbeat
// Must be 30-250 seconds per MQTT best practices
#define MQTT_KEEPALIVE_S            60

// QoS Level: 0=At most once, 1=At least once, 2=Exactly once
// QoS 1 recommended for GPS tracking (ensures delivery)
#define MQTT_QOS                    1

// Enable persistent session (Clean Session = false)
// CRITICAL: Allows broker to queue messages when device is offline
#define MQTT_PERSISTENT_SESSION     true

// Session expiry interval (seconds) - how long broker keeps session
#define MQTT_SESSION_EXPIRY_S       3600  // 1 hour

// Enable Last Will and Testament (LWT)
// Broker publishes this if device disconnects unexpectedly
#define MQTT_LWT_ENABLED            true

// LWT Topic and Message
#define MQTT_LWT_TOPIC              "gps/tracker/status"
#define MQTT_LWT_MESSAGE            "offline"
#define MQTT_LWT_QOS                1
#define MQTT_LWT_RETAIN             true

// MQTT Topics
#define MQTT_TOPIC_GPS              "gps/tracker/location"
#define MQTT_TOPIC_STATUS           "gps/tracker/status"
#define MQTT_TOPIC_BATTERY          "gps/tracker/battery"
#define MQTT_TOPIC_HEALTH           "gps/tracker/health"
#define MQTT_TOPIC_BUFFERED         "gps/tracker/buffered"

// =============================================================================
// MESSAGE BATCHING - REDUCES CELLULAR DATA USAGE
// =============================================================================

// Enable message batching (group multiple GPS points in one MQTT message)
#define BATCHING_ENABLED            true

// Maximum batch size (number of GPS records per message)
#define BATCH_SIZE                  10

// Maximum time to wait before sending batch (milliseconds)
#define BATCH_TIMEOUT_MS            300000  // 5 minutes

// =============================================================================
// RECONNECTION STRATEGY
// =============================================================================

// Maximum reconnection attempts before giving up
#define MAX_RECONNECT_ATTEMPTS      10

// Initial retry delay (milliseconds)
#define RECONNECT_INITIAL_DELAY_MS  5000    // 5 seconds

// Maximum retry delay (milliseconds)
#define RECONNECT_MAX_DELAY_MS      300000  // 5 minutes

// Exponential backoff multiplier
#define RECONNECT_BACKOFF_MULTIPLIER 2

// Add random jitter to prevent thundering herd (±25%)
#define RECONNECT_JITTER_PERCENT    25

// =============================================================================
// SD CARD / OFFLINE BUFFER CONFIGURATION
// =============================================================================

// SD Card pins (defined in utilities.h, but listed here for reference)
// SDMMC_CMD = 39, SDMMC_CLK = 38, SDMMC_DATA = 40

// Buffer file path
#define BUFFER_FILE_PATH            "/gps_buffer.csv"

// Log file path
#define LOG_FILE_PATH               "/gps_tracker.log"

// Health status file
#define HEALTH_FILE_PATH            "/health.json"

// Maximum buffer file size (bytes) - prevents SD card fill-up
#define MAX_BUFFER_FILE_SIZE        (50 * 1024 * 1024)  // 50 MB

// Buffer sync interval (records before flushing to SD)
#define BUFFER_SYNC_INTERVAL        5

// =============================================================================
// SD CARD WEAR LEVELING - EXTENDS SD LIFETIME
// =============================================================================

// Enable circular buffer (ring buffer) for SD writes
// Distributes writes across multiple files to reduce wear
#define SD_WEAR_LEVELING_ENABLED    true

// Number of rotating buffer files (higher = better wear leveling)
#define SD_ROTATING_FILES           4

// Rotate file every N records
#define SD_ROTATE_RECORD_COUNT      1000

// =============================================================================
// DATA VALIDATION
// =============================================================================

// Enable CRC32 checksums for buffered data
#define DATA_CRC_ENABLED            true

// Validate GPS data before accepting
#define GPS_DATA_VALIDATION         true

// Maximum acceptable HDOP (Horizontal Dilution of Precision)
#define GPS_MAX_HDOP                5.0

// =============================================================================
// BATTERY MANAGEMENT
// =============================================================================

// Battery voltage thresholds (volts)
#define BATTERY_FULL_VOLTAGE        4.20
#define BATTERY_GOOD_VOLTAGE        3.80
#define BATTERY_LOW_VOLTAGE         3.50
#define BATTERY_CRITICAL_VOLTAGE    3.30

// Enter emergency mode below this voltage
#define BATTERY_EMERGENCY_VOLTAGE   3.20

// Send low battery alert when crossing threshold
#define BATTERY_ALERT_ENABLED       true

// =============================================================================
// DEEP SLEEP CONFIGURATION
// =============================================================================

// Enable deep sleep between updates (major power savings)
#define ENABLE_DEEP_SLEEP           true

// Deep sleep duration (seconds)
#define DEEP_SLEEP_SECONDS          60

// Minimum sleep time (don't sleep if cycle took longer than this)
#define MIN_SLEEP_TIME_S            10

// =============================================================================
// OTA UPDATE SUPPORT
// =============================================================================

// Enable Over-The-Air firmware updates
#define OTA_ENABLED                 false

// OTA check interval (seconds)
#define OTA_CHECK_INTERVAL_S        86400   // 24 hours

// OTA server URL
#define OTA_SERVER_URL              ""

// =============================================================================
// EMERGENCY MODE
// =============================================================================

// Enable emergency mode on critical failures
#define EMERGENCY_MODE_ENABLED      true

// Emergency mode: reduce update interval to preserve battery
#define EMERGENCY_UPDATE_INTERVAL_S 3600    // 1 hour

// Maximum consecutive failures before emergency mode
#define MAX_CONSECUTIVE_FAILURES    10

#endif // CONFIG_H
