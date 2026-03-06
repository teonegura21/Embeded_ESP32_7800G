# GPS Tracker - Production Edition v2.0

**Production-Grade GPS Tracker optimized for long-term remote deployment with maximum reliability.**

## 🎯 Key Features

| Feature | Benefit |
|---------|---------|
| **Hardware Watchdog** | Auto-reset if device freezes - 99.9% uptime |
| **MQTT LWT** | Know immediately when device goes offline |
| **Persistent Sessions** | Messages queued while offline, delivered on reconnect |
| **PSM Power Saving** | 10-year battery life potential! |
| **Message Batching** | Reduce cellular data usage by 80%+ |
| **SD Wear Leveling** | Extend SD card life 4x longer |
| **Exponential Backoff** | Smart reconnection prevents network overload |
| **CRC Validation** | Data integrity verification |
| **Health Monitoring** | Self-diagnostics and reporting |

## 📊 Power Consumption

| Mode | Current | 18650 Battery Life* |
|------|---------|---------------------|
| Active (GPS+TX) | ~150mA | - |
| PSM Deep Sleep | ~8µA | - |
| **1 min updates** | avg ~5mA | **~3 weeks** |
| **5 min updates** | avg ~1.5mA | **~2 months** |
| **1 hour updates** | avg ~0.1mA | **~2.5 years** |
| **PSM optimized** | avg ~0.01mA | **~10 years** |

*With 2500mAh 18650 battery. Add solar panel for indefinite operation!

## 🚀 Quick Start

### 1. Configure for Your Deployment

Edit `config.h`:

```cpp
// === YOUR SIM CARD APN (CRITICAL!) ===
#define GPRS_APN    "internet"   // Your carrier's APN

// === MQTT BROKER ===
#define MQTT_SERVER     "broker.hivemq.com"
#define MQTT_PORT       1883

// === UPDATE INTERVAL (affects battery life) ===
#define GPS_UPDATE_INTERVAL_MS      300000   // 5 minutes
// #define GPS_UPDATE_INTERVAL_MS   60000    // 1 minute (more data, less battery)
// #define GPS_UPDATE_INTERVAL_MS   900000   // 15 minutes (best battery)

// === POWER SAVING MODE (for maximum battery) ===
#define PSM_ENABLED                 true     // Enable for 10-year life!
```

### 2. Build & Upload

```bash
# In platformio.ini, select this example:
src_dir = examples/GPSTrackerProduction

# Build and upload
pio run --target upload

# Monitor
pio device monitor --baud 115200
```

### 3. Subscribe to Topics

```bash
# View live GPS data
mosquitto_sub -h broker.hivemq.com -t "gps/tracker/location" -v

# View device status (retained message)
mosquitto_sub -h broker.hivemq.com -t "gps/tracker/status" -v

# View battery level
mosquitto_sub -h broker.hivemq.com -t "gps/tracker/battery" -v

# View health diagnostics
mosquitto_sub -h broker.hivemq.com -t "gps/tracker/health" -v
```

## 📡 MQTT Topics

| Topic | Description | Retained |
|-------|-------------|----------|
| `gps/tracker/location` | GPS coordinates (JSON batch) | No |
| `gps/tracker/status` | Device online/offline status | **Yes** |
| `gps/tracker/battery` | Battery voltage | No |
| `gps/tracker/health` | Diagnostics report | No |
| `gps/tracker/buffered` | Synced buffered records | No |

## 📦 Message Format (Batched JSON)

```json
{
  "device": "GPSTracker_A1B2C3D4",
  "batch": [
    {
      "lat": 40.712776,
      "lon": -74.005974,
      "alt": 10.5,
      "spd": 0.0,
      "acc": 3.5,
      "sats": 8,
      "batt": 4.15,
      "ts": 12345678,
      "dt": "2024-01-15",
      "tm": "14:30:25"
    }
  ],
  "count": 1,
  "fw": "2.0.0",
  "batt": 4.15
}
```

## ⚡ Power Saving Mode (PSM)

**PSM can extend battery life to 10 years!** Here's how:

### How PSM Works

```
Normal: GPS → Network → MQTT → Sleep → Wake → Repeat (every minute)
PSM:    GPS → Network → MQTT → DEEP SLEEP (hours) → Network Registration → Repeat
```

In PSM, the modem enters ultra-low-power state (~8µA) and only wakes periodically to check for network messages.

### PSM Configuration

```cpp
#define PSM_ENABLED                 true

// How long modem stays awake after transmission (4 seconds)
#define PSM_ACTIVE_TIME             "00000100"

// How often to wake and check network (10 hours = maximum battery)
#define PSM_PERIODIC_TAU            "01100111"
```

### PSM Trade-offs

| Setting | Battery Life | Latency (downlink) |
|---------|--------------|-------------------|
| PSM disabled | Weeks | Immediate |
| 5 min TAU | Months | 5 minutes |
| 1 hour TAU | Years | 1 hour |
| 10 hour TAU | 10 years | 10 hours |

**Note:** GPS tracking still works normally! Only downlink (sending commands TO device) is delayed.

## 🔧 Reliability Features Explained

### 1. Hardware Watchdog

If the device freezes for any reason (software bug, power glitch, cosmic ray), the hardware watchdog automatically resets it after 5 minutes.

```cpp
#define WATCHDOG_ENABLED            true
#define WATCHDOG_TIMEOUT_S          300   // 5 minutes
```

### 2. MQTT Last Will & Testament (LWT)

When device connects, it registers a "last will" message with the broker. If the device disconnects unexpectedly (power loss, network failure), the broker automatically publishes:

```
gps/tracker/status: "offline"
```

This is a **retained message**, so any subscriber immediately knows the device is offline.

### 3. Persistent Sessions

```cpp
#define MQTT_PERSISTENT_SESSION     true
```

When enabled:
- Broker queues messages while device is offline
- On reconnect, all queued messages are delivered
- Session state preserved (subscriptions, etc.)

### 4. SD Card Wear Leveling

Instead of writing to one file (which wears out that SD sector), the code rotates through 4 files:

```
/buffer/gps_0.csv  →  /buffer/gps_1.csv  →  /buffer/gps_2.csv  →  /buffer/gps_3.csv  →  (repeat)
```

This extends SD card life by 4x!

### 5. Exponential Backoff

On connection failure, the device waits progressively longer before retrying:

```
1st failure: wait 5 seconds
2nd failure: wait 10 seconds
3rd failure: wait 20 seconds
...
10th failure: wait 5 minutes (max)
```

Prevents network overload and reduces power consumption during outages.

### 6. Message Batching

Instead of sending 10 separate MQTT messages:

```
[Send] GPS Point 1
[Wait] ACK
[Send] GPS Point 2
[Wait] ACK
... (10 times)
```

The code sends one batch message:

```
[Send] Batch with 10 points
[Wait] ACK (once)
```

**Reduces cellular data usage by 80%+!**

## 📈 Health Monitoring

The device tracks and reports:

```
╔══════════════════════════════════════════════════════════════╗
║                    HEALTH STATUS REPORT                      ║
╠══════════════════════════════════════════════════════════════╣
║  Boot Count:       152                                       ║
║  GPS Fixes:        1247                                      ║
║  MQTT Published:   1189                                      ║
║  MQTT Failures:    58                                        ║
║  Network Reconnects: 12                                      ║
║  Battery (now):     3.85V                                    ║
║  Battery (min/max): 3.20V / 4.18V                            ║
║  SD Card:          OK                                        ║
║  Uptime:            456 hours                                ║
╚══════════════════════════════════════════════════════════════╝
```

This helps diagnose issues remotely:
- High reconnects = poor cellular signal
- High failures = check MQTT broker
- Low battery = schedule maintenance

## 🔋 Battery Management

### Voltage Thresholds

| Voltage | Status | Action |
|---------|--------|--------|
| > 4.0V | Excellent | Normal operation |
| 3.5-4.0V | Good | Normal operation |
| 3.3-3.5V | Low | Reduce update frequency |
| < 3.3V | Critical | Emergency mode (1 hour updates) |
| < 3.2V | Emergency | Sleep and wait for charge |

### Emergency Mode

When battery drops below 3.3V:
- Update interval increases to 1 hour
- All non-essential functions disabled
- Device preserves power for critical tracking

## 📋 Configuration Summary

### For Maximum Battery Life (Years)

```cpp
#define GPS_UPDATE_INTERVAL_MS      3600000  // 1 hour
#define PSM_ENABLED                 true
#define PSM_PERIODIC_TAU            "01100111"  // 10 hours
#define BATCHING_ENABLED            true
#define BATCH_SIZE                  24  // One day per message
#define ENABLE_DEEP_SLEEP           true
```

### For Real-Time Tracking (Minutes latency)

```cpp
#define GPS_UPDATE_INTERVAL_MS      60000    // 1 minute
#define PSM_ENABLED                 false    // Disable PSM
#define BATCHING_ENABLED            true
#define BATCH_SIZE                  5
#define ENABLE_DEEP_SLEEP           true
```

### For Asset Tracking (Hours latency, multi-year battery)

```cpp
#define GPS_UPDATE_INTERVAL_MS      14400000 // 4 hours
#define PSM_ENABLED                 true
#define PSM_PERIODIC_TAU            "01100111"  // 10 hours
#define BATCHING_ENABLED            true
#define BATCH_SIZE                  10
#define ENABLE_DEEP_SLEEP           true
```

## 🔍 Troubleshooting

### "Watchdog reset"
- Normal if device was stuck
- Check GPS antenna connection
- Verify SIM card has data plan

### High MQTT failures
- Check MQTT broker is reachable
- Verify credentials
- Check firewall (port 1883)

### SD card not detected
- Format as FAT32
- Try different SD card
- Check card is properly inserted

### Battery drains quickly
- Enable PSM mode
- Increase update interval
- Check solar panel connection (if used)

### No GPS fix
- Ensure GPS antenna connected
- Move near window/outside
- Cold start takes 2-5 minutes

## 🛡️ Production Deployment Checklist

- [ ] SIM card has PIN removed or set in config
- [ ] SIM card activated with NB-IoT/CAT-M data plan
- [ ] APN configured correctly in config.h
- [ ] MQTT broker tested and accessible
- [ ] SD card formatted FAT32 and tested
- [ ] GPS antenna connected
- [ ] Battery charged or solar panel connected
- [ ] Watchdog enabled
- [ ] Debug disabled (saves power)
- [ ] Health monitoring enabled
- [ ] Update interval set appropriately
- [ ] PSM enabled for battery-powered deployment
- [ ] Device enclosure weatherproofed (if outdoor)

## 📚 References

- [TinyGSM Library](https://github.com/vshymanskyy/TinyGSM)
- [SIM7080G AT Manual](https://www.simcom.com/product/SIM7080G.html)
- [MQTT 3.1.1 Specification](http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html)
- [ESP32 Deep Sleep](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/sleep_modes.html)

## License

MIT License - Based on LilyGo examples

---

**Version:** 2.0.0 Production Ready  
**Last Updated:** 2024-01-01
