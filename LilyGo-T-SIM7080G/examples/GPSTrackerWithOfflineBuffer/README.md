# GPS Tracker with Offline Buffer

A complete GPS tracking solution for the LilyGo T-SIM7080G board featuring offline data buffering, MQTT communication, and smart power management.

## Features

- 📍 **GPS Positioning**: Configurable accuracy and satellite requirements
- 💾 **Offline Buffering**: SD card storage when network is unavailable
- 📡 **MQTT Communication**: Automatic data transmission when connected
- 🔄 **Auto-Sync**: Buffered data automatically sent when connection restored
- 🔋 **Power Management**: Battery monitoring and deep sleep support
- 📊 **Smart Switching**: Handles SIM7080G GPS/Cellular limitation

## Hardware Requirements

- LilyGo T-SIM7080G board
- SIM card with NB-IoT or CAT-M support (2G/3G/4G will NOT work!)
- GPS antenna (connected to the board)
- Micro SD card (for offline buffering)
- Battery (optional but recommended)
- USB-C cable for programming

## Wiring/Connections

### GPS Antenna
Connect the GPS antenna to the `GPS` connector on the board.

### SIM Card
1. Remove the SIM card holder (slide out)
2. Insert nano-SIM card with gold contacts facing down
3. Slide holder back into place
4. **IMPORTANT**: Insert SIM BEFORE powering on the board!

### SD Card
Insert a FAT32 formatted micro SD card into the slot.

## Configuration

Before building and uploading, edit `config.h` with your settings:

### 1. MQTT Broker Settings

```cpp
#define MQTT_SERVER     "broker.hivemq.com"  // Your MQTT broker
#define MQTT_PORT       1883
#define MQTT_USERNAME   ""                    // Leave empty if not required
#define MQTT_PASSWORD   ""                    // Leave empty if not required
```

**Public Test Brokers:**
- `broker.hivemq.com` (public, no auth)
- `test.mosquitto.org` (public, no auth)
- `mqtt.eclipseprojects.io` (public, no auth)

**Cloud MQTT Services:**
- HiveMQ Cloud
- AWS IoT Core
- Azure IoT Hub
- Adafruit IO
- Cayenne (myDevices)

### 2. APN Settings (Important!)

```cpp
#define GPRS_APN    "internet"   // Your carrier's APN
#define GPRS_USER   ""           // Usually empty
#define GPRS_PASS   ""           // Usually empty
```

**Common APNs:**
| Carrier | APN |
|---------|-----|
| Generic IoT | `internet` |
| Vodafone IoT | `vfd1.konyne` |
| T-Mobile IoT | `fast.t-mobile.com` |
| AT&T IoT | `m2m.com.attz` |
| Hologram | `hologram` |
| 1NCE | `iot.1nce.net` |

**To find your APN:** Check your SIM card provider's documentation or contact their support.

### 3. SIM Card PIN (Optional)

If your SIM card has a PIN code, enter it here:

```cpp
#define SIM_CARD_PIN    ""       // Leave empty for no PIN
// OR
#define SIM_CARD_PIN    "1234"   // Your 4-digit PIN
```

The code will automatically unlock the SIM at startup. **After 3 failed attempts, the SIM will be PUK-blocked!**

### 4. GPS Settings

```cpp
#define GPS_UPDATE_INTERVAL_MS      30000   // 30 seconds between updates
#define GPS_FIX_TIMEOUT_MS          120000  // 2 minutes max for fix
#define GPS_MIN_ACCURACY            50.0    // Maximum acceptable accuracy (meters)
#define GPS_MIN_SATELLITES          4       // Minimum satellites for valid fix
```

### 5. Buffer Settings

```cpp
#define MAX_BUFFERED_RECORDS        1000    // Max records to store on SD
#define BUFFER_FILE_PATH            "/gps_buffer.csv"
```

## Building and Uploading

### Using PlatformIO (Recommended)

1. **Select the example:**
   Edit `platformio.ini` and uncomment:
   ```ini
   src_dir = examples/GPSTrackerWithOfflineBuffer
   ```

2. **Update configuration:**
   Edit `examples/GPSTrackerWithOfflineBuffer/config.h` with your settings.

3. **Build:**
   ```bash
   cd LilyGo-T-SIM7080G
   pio run
   ```

4. **Upload:**
   ```bash
   pio run --target upload
   ```

5. **Monitor:**
   ```bash
   pio device monitor --baud 115200
   ```

### Using Arduino IDE

1. Copy the `GPSTrackerWithOfflineBuffer` folder to your Arduino sketches folder
2. Copy all libraries from `LilyGo-T-SIM7080G/lib/` to your Arduino libraries folder
3. Open `GPSTrackerWithOfflineBuffer.ino` in Arduino IDE
4. Select board: `ESP32S3 Dev Module`
5. Select partition scheme: `16M Flash (3MB APP/9.9MB FATFS)`
6. Select USB CDC On Boot: `Enabled`
7. Click Upload

### If Upload Fails (Port Not Found)

1. Hold the **BOOT** button on the board
2. While holding BOOT, press and release **RST** button
3. Release BOOT button
4. Try uploading again
5. Press RST again after upload completes

## First Time Setup Checklist

- [ ] SIM card inserted (before power on!)
- [ ] GPS antenna connected
- [ ] SD card inserted
- [ ] `config.h` updated with your APN
- [ ] `config.h` updated with MQTT broker
- [ ] Board has power (USB or battery)

## Understanding the Output

### Serial Monitor Output

```
----------------------------------------
Cycle #1

[1/5] Acquiring GPS fix...
GPS fix acquired in 45 attempts
✓ GPS Fix acquired:
  Lat: 40.712776
  Lon: -74.005974
  Sats: 8
  Accuracy: 3.5m

[2/5] Connecting to cellular network...
Network: Registered, home network
✓ Network connected

[3/5] Connecting to MQTT broker...
✓ MQTT connected

[4/5] Publishing current GPS data...
✓ GPS data published

--- Status Summary ---
  Battery: 4.15V
  SD Card: Available
  Buffered Records: 0
  Total Sent: 1
  Last GPS: 0s ago
----------------------
```

### Data Format (MQTT)

GPS data is sent as JSON:

```json
{
  "lat": 40.712776,
  "lon": -74.005974,
  "alt": 10.5,
  "speed": 0.0,
  "accuracy": 3.5,
  "sats": 8,
  "battery": 4.15,
  "ts": 12345678,
  "date": "2024-01-15",
  "time": "14:30:25"
}
```

## Troubleshooting

### "SIM Card is not inserted!!!" or "SIM PIN unlock failed"

**If SIM has no PIN:**
- Power off the board completely
- Remove and reinsert the SIM card
- Ensure it's properly seated in the holder
- Set in `config.h`: `#define SIM_CARD_PIN ""`
- Power on and try again

**If SIM has a PIN:**
- Edit `config.h` and set your PIN: `#define SIM_CARD_PIN "1234"`
- Re-upload the code
- The board will auto-unlock the SIM

**⚠️ After 3 failed attempts, SIM will be PUK-blocked!**
If this happens, you need the PUK code from your carrier to unlock.

### "GPS fix timeout"
- Make sure GPS antenna is connected
- Try moving near a window or outside
- First fix can take 2-5 minutes (cold start)
- Subsequent fixes are faster (warm/hot start)

### "Network registration timeout"
- Verify your APN is correct in `config.h`
- Check that your SIM supports NB-IoT or CAT-M
- Verify signal strength (may need antenna adjustment)
- Check SIM is activated and has data plan

### "MQTT connection failed"
- Verify MQTT broker address is correct
- Check firewall isn't blocking port 1883
- Verify username/password if required
- Try a public test broker first

### No data being buffered to SD card
- Ensure SD card is FAT32 formatted
- Try a different SD card
- Check SD card is properly inserted

### Board keeps resetting
- Check battery voltage (if using battery)
- Ensure USB cable provides enough power
- Disable deep sleep for debugging

## Important Hardware Limitations

### GPS and Cellular Cannot Work Simultaneously

The SIM7080G modem **cannot** use GPS and cellular at the same time. This code handles this automatically:

1. Powers on and enables GPS
2. Acquires GPS fix
3. Disables GPS
4. Enables cellular
5. Sends data via MQTT
6. Disables cellular
7. Repeats

This switching happens automatically and is normal behavior.

### Power Domains

The code manages these power channels:
- **DC3**: Modem main power (always 3000mV)
- **BLDO1**: Level converter (NEVER disable!)
- **BLDO2**: GPS antenna power
- **ALDO3**: SD card power

## MQTT Topic Structure

Default topics (configurable in `config.h`):
- `gps/tracker/location` - GPS coordinates
- `gps/tracker/status` - Device status
- `gps/tracker/battery` - Battery voltage
- `gps/tracker/buffered` - Synced buffered records

## SD Card File Structure

```
/gps_buffer.csv    - Buffered GPS records (CSV format)
/gps_tracker.log   - Debug log file
```

CSV format:
```
latitude,longitude,altitude,speed,accuracy,satellites,battery,timestamp,date,time
40.712776,-74.005974,10.5,0.0,3.5,8,4.15,12345678,2024-01-15,14:30:25
```

## Power Consumption Tips

1. **Increase GPS update interval** to save power:
   ```cpp
   #define GPS_UPDATE_INTERVAL_MS 300000  // 5 minutes
   ```

2. **Enable deep sleep** in `config.h`:
   ```cpp
   #define ENABLE_DEEP_SLEEP true
   #define DEEP_SLEEP_SECONDS 300  // 5 minutes
   ```

3. **Use a battery** - The board supports 18650 Li-ion batteries

4. **Add solar panel** - Supports 5V-6V solar charging

## MQTT Testing with mosquitto

Install mosquitto clients:
```bash
# Ubuntu/Debian
sudo apt-get install mosquitto-clients

# macOS
brew install mosquitto
```

Subscribe to GPS data:
```bash
mosquitto_sub -h broker.hivemq.com -t "gps/tracker/location" -v
```

## License

MIT License - Based on LilyGo examples

## Support

- LilyGo Product Page: https://www.lilygo.cc/products/t-sim7080-s3
- TinyGSM Library: https://github.com/vshymanskyy/TinyGSM
- XPowersLib: https://github.com/lewisxhe/XPowersLib
