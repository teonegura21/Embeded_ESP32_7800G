/**
 * @file      config_test.h
 * @brief     TEST CONFIGURATION - GPS Only, No Cellular
 * @date      2024-01-01
 * 
 * @attention This is for LOCAL TESTING ONLY!
 * - No SIM card required
 * - No SD card required  
 * - GPS works and shows on Serial Monitor
 * - No MQTT/cellular functions
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// TEST MODE FLAGS
// =============================================================================

// Set this to test without cellular network
#define TEST_MODE_NO_CELLULAR       true

// Will log GPS to Serial and SD card only (no MQTT)
#define SKIP_MQTT                   true

// =============================================================================
// DEBUG & DIAGNOSTICS (ALL ENABLED FOR TESTING)
// =============================================================================

#define DEBUG_ENABLED               true
#define DUMP_AT_COMMANDS            // Verbose modem output
#define SD_LOGGING_ENABLED          true
#define HEALTH_MONITORING_ENABLED   true

// =============================================================================
// HARDWARE WATCHDOG
// =============================================================================

#define WATCHDOG_ENABLED            true
#define WATCHDOG_TIMEOUT_S          300

// =============================================================================
// GPS CONFIGURATION (FAST UPDATES FOR TESTING)
// =============================================================================

// Update every 30 seconds (fast for testing)
#define GPS_UPDATE_INTERVAL_MS      30000     // 30 seconds

// GPS fix timeout (2 minutes)
#define GPS_FIX_TIMEOUT_MS          120000

// Quality thresholds
#define GPS_MIN_ACCURACY            50.0
#define GPS_MIN_SATELLITES          4

// =============================================================================
// CELLULAR NETWORK (WON'T BE USED, BUT NEEDS VALUES)
// =============================================================================

#define SIM_CARD_PIN                ""
#define GPRS_APN                    "internet"  // Doesn't matter in test mode
#define NETWORK_MODE                2
#define PREFERRED_MODE              MODEM_NB_IOT

// =============================================================================
// POWER SAVING (DISABLED FOR USB TESTING)
// =============================================================================

#define PSM_ENABLED                 false     // Disable for USB testing
#define EDRX_ENABLED                false

// =============================================================================
// MQTT (DISABLED IN TEST MODE)
// =============================================================================

#define MQTT_SERVER                 "broker.hivemq.com"
#define MQTT_PORT                   1883
#define MQTT_USERNAME               ""
#define MQTT_PASSWORD               ""
#define MQTT_KEEPALIVE_S            60
#define MQTT_QOS                    1
#define MQTT_PERSISTENT_SESSION     false
#define MQTT_LWT_ENABLED            false

// Topics
#define MQTT_TOPIC_GPS              "gps/tracker/location"
#define MQTT_TOPIC_STATUS           "gps/tracker/status"
#define MQTT_TOPIC_BATTERY          "gps/tracker/battery"
#define MQTT_TOPIC_HEALTH           "gps/tracker/health"

// =============================================================================
// MESSAGE BATCHING
// =============================================================================

#define BATCHING_ENABLED            false     // Disable for simple testing
#define BATCH_SIZE                  1

// =============================================================================
// RECONNECTION (NOT USED IN TEST MODE)
// =============================================================================

#define MAX_RECONNECT_ATTEMPTS      3
#define RECONNECT_INITIAL_DELAY_MS  5000

// =============================================================================
// SD CARD
// =============================================================================

#define BUFFER_FILE_PATH            "/gps_buffer.csv"
#define LOG_FILE_PATH               "/gps_test.log"
#define MAX_BUFFER_FILE_SIZE        (10 * 1024 * 1024)
#define SD_WEAR_LEVELING_ENABLED    true
#define SD_ROTATING_FILES           4

// =============================================================================
// BATTERY
// =============================================================================

#define BATTERY_LOW_VOLTAGE         3.3f
#define BATTERY_CRITICAL_VOLTAGE    3.0f

// =============================================================================
// DEEP SLEEP (DISABLED FOR TESTING - STAYS AWAKE)
// =============================================================================

#define ENABLE_DEEP_SLEEP           false     // Keep awake for testing

// =============================================================================
// FIRMWARE VERSION
// =============================================================================

#define FIRMWARE_VERSION            "2.0.0-TEST"

#endif // CONFIG_H
