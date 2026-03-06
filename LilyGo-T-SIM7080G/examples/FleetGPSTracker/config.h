/**
 * @file      config.h
 * @brief     Fleet GPS Tracker — Anti-Theft Transport System
 *            Edit WIFI_SSID, WIFI_PASSWORD, and BUS_ID before flashing.
 *            Everything else is pre-configured for HiveMQ Cloud.
 */

#pragma once
#include <Arduino.h>

// =============================================================================
// BUS IDENTITY — change this per device
// =============================================================================
#define BUS_ID                  "BUS_01"

// =============================================================================
// WiFi — edit to match your phone hotspot / router
// =============================================================================
#define WIFI_SSID               "YOUR_HOTSPOT_NAME"
#define WIFI_PASSWORD           "YOUR_HOTSPOT_PASSWORD"

// WiFi connection timeout (ms)
#define WIFI_CONNECT_TIMEOUT_MS 20000

// =============================================================================
// MQTT — HiveMQ Cloud TLS (pre-configured for the fleet system)
// =============================================================================
#define MQTT_BROKER             "c0aee68307fa4d4185b4aa532da90ca8.s1.eu.hivemq.cloud"
#define MQTT_PORT               8883
#define MQTT_USER               "fleet_user"
#define MQTT_PASSWORD_STR       "dragnea1dragneA"
#define MQTT_TOPIC              "fleet/gps/" BUS_ID
#define MQTT_KEEPALIVE          60
#define MQTT_QOS                1

// LWT — broker publishes "offline" if device disconnects unexpectedly
#define MQTT_LWT_TOPIC          "fleet/status/" BUS_ID
#define MQTT_LWT_MSG_OFFLINE    "offline"
#define MQTT_LWT_MSG_ONLINE     "online"

// =============================================================================
// GPS CONFIGURATION
// =============================================================================

// Publish interval (ms) — 1 second for gate accuracy, increase to save battery
#define GPS_PUBLISH_INTERVAL_MS     1000

// Maximum time to wait for a GPS fix on startup (ms)
#define GPS_FIX_TIMEOUT_MS          120000  // 2 minutes

// Minimum satellites for a valid fix
#define GPS_MIN_SATELLITES          4

// Maximum HDOP for a valid fix
#define GPS_MAX_HDOP                5.0f

// Watchdog: reboot if no GPS response for this long (ms)
#define GPS_WATCHDOG_MS             300000  // 5 minutes

// =============================================================================
// OFFLINE BUFFER — SD card
// =============================================================================
#define BUFFER_FILE_PATH            "/fleet_buffer.csv"
#define MAX_BUFFER_RECORDS          1000    // ~1000 seconds at 1 Hz
#define BUFFER_REPLAY_BATCH         20      // Records sent per reconnect cycle

// =============================================================================
// DEBUG
// =============================================================================
#define DEBUG_ENABLED               true

// Uncomment to dump raw AT commands to Serial (very verbose)
// #define DUMP_AT_COMMANDS
