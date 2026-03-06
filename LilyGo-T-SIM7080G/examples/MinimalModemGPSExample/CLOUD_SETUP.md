# Cloud GPS Tracker Setup Guide

Complete guide for running your LilyGo GPS tracker with a **cloud/Docker backend**.

## Architecture

```
┌─────────────┐      WiFi+TLS       ┌─────────────────────────────────────┐
│ LilyGo      │  ═════════════════► │  Cloud Server (Docker)              │
│ T-SIM7080G  │                     │  • EMQX (MQTT broker)               │
│             │                     │  • Node-RED (data processing)       │
│ GPS + Flash │                     │  • InfluxDB (time-series DB)        │
│ Buffer      │                     │  • Grafana (visualization)          │
└─────────────┘                     └─────────────────────────────────────┘
```

## Quick Start (5 minutes)

### 1. Start the Cloud Stack

On your server/VPS (or local machine for testing):

```bash
# Create project directory
mkdir ~/gps-cloud && cd ~/gps-cloud

# Copy docker-compose.yml from this folder
cp /path/to/LilyGo-T-SIM7080G/examples/MinimalModemGPSExample/docker-compose.yml .

# Start services
docker-compose up -d

# Check status
docker-compose ps
```

Access points:
- **EMQX Dashboard**: http://your-server-ip:18083 (admin/public)
- **Node-RED**: http://your-server-ip:1880
- **Grafana**: http://your-server-ip:3000 (admin/admin)

### 2. Configure LilyGo Board

Edit `MinimalModemGPSExample.ino`:

```cpp
// WiFi (your network)
const char* WIFI_SSID = "YourWiFi";
const char* WIFI_PASS = "YourPassword";

// MQTT Broker (your server)
const char* MQTT_BROKER = "your-server-ip";  // or domain
const int   MQTT_PORT   = 1883;              // 8883 for TLS

// Security (recommended)
#define USE_TLS     false  // Set true for port 8883
#define VERIFY_CERT false  // Set true with valid certs

// Cloud credentials (if auth enabled on broker)
const char* MQTT_USERNAME = "";
const char* MQTT_PASSWORD = "";
```

### 3. Flash & Test

```bash
cd LilyGo-T-SIM7080G
pio run --target upload
pio device monitor
```

You should see:
```
[MQTT] ✓ Connected to cloud broker!
[Publish] ✓ Live fix #1 → cloud
[Batch] ✓ Batch 1 sent (10 fixes)
```

## MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `lilygo/gps/location` | Device → Cloud | Live GPS fixes (JSON) |
| `lilygo/gps/batch` | Device → Cloud | Batched buffered fixes |
| `lilygo/gps/status` | Device → Cloud | online/offline (LWT) |
| `lilygo/gps/meta` | Device → Cloud | Device metadata |

### JSON Formats

**Live Fix:**
```json
{
  "type": "live",
  "fix": 42,
  "lat": 47.16823959,
  "lon": 27.59916878,
  "spd": 0.50,
  "alt": 73.4,
  "sat": 8,
  "acc": 2.5,
  "dt": "2026-03-04 14:32:10",
  "buf": 0
}
```

**Batched Fixes:**
```json
{
  "type": "batch",
  "count": 10,
  "fixes": [
    {"ts":199007,"lat":47.1682,"lon":27.5991,"spd":0.00,"sat":8,"acc":0.8,"dt":"2026-03-04 11:51:31"},
    {"ts":208647,"lat":47.1682,"lon":27.5991,"spd":0.00,"sat":8,"acc":0.8,"dt":"2026-03-04 11:51:41"}
  ]
}
```

## Production Setup with TLS

### Option A: Let's Encrypt (Recommended)

1. Get domain + cert:
```bash
certbot certonly --standalone -d gps.yourdomain.com
```

2. Update docker-compose.yml volumes:
```yaml
volumes:
  - /etc/letsencrypt/live/gps.yourdomain.com:/opt/emqx/etc/certs:ro
```

3. Update LilyGo firmware:
```cpp
const char* MQTT_BROKER = "gps.yourdomain.com";
const int   MQTT_PORT   = 8883;
#define USE_TLS         true
#define VERIFY_CERT     true  // Now safe with real cert
```

### Option B: Self-Signed Certificate (Testing)

```bash
# Generate CA + server certs
openssl req -x509 -newkey rsa:4096 -keyout ca-key.pem -out ca-cert.pem -days 365 -nodes
```

Copy `ca-cert.pem` to LilyGo source and update `ROOT_CA_CERT`.

## Data Processing with Node-RED

Import this flow to save GPS data to InfluxDB:

```json
[{
  "id": "mqtt-in",
  "type": "mqtt in",
  "topic": "lilygo/gps/#",
  "broker": "emqx",
  "x": 150,
  "y": 100,
  "wires": [["parse-json"]]
}, {
  "id": "parse-json",
  "type": "json",
  "x": 300,
  "y": 100,
  "wires": [["switch-type"]]
}, {
  "id": "switch-type",
  "type": "switch",
  "property": "payload.type",
  "rules": [
    {"t": "eq", "v": "live"},
    {"t": "eq", "v": "batch"}
  ],
  "x": 450,
  "y": 100,
  "wires": [["influx-live"], ["split-batch"]]
}, {
  "id": "influx-live",
  "type": "influxdb out",
  "measurement": "gps",
  "x": 650,
  "y": 80,
  "wires": []
}]
```

## Managed Cloud Brokers (No Server)

Don't want to run your own server? Use these managed MQTT services:

### HiveMQ Cloud (Free Tier)
- 100 devices, 10GB/month
- TLS included
```cpp
const char* MQTT_BROKER = "your-instance.hivemq.cloud";
const int   MQTT_PORT = 8883;
#define USE_TLS true
```

### EMQX Cloud
- Serverless pay-per-use
- Built-in data persistence
```cpp
const char* MQTT_BROKER = "your-instance.emqx.cloud";
```

### AWS IoT Core
- Enterprise grade
- Certificate-based auth
```cpp
// Requires AWS root CA + device cert
#define USE_TLS true
#define VERIFY_CERT true
```

## Troubleshooting

**"[MQTT] ✗ Failed (rc=-4)"**
- TLS certificate issue
- Try `#define VERIFY_CERT false` for testing

**"[MQTT] ✗ Failed (rc=2)"**
- Wrong username/password
- Check EMQX dashboard authentication settings

**"[Batch] ⚠ Batch too large"**
- Reduce `BATCH_SIZE_TARGET` in config
- Or increase `MQTT_MAX_PACKET_SIZE`

**No data in InfluxDB**
- Check Node-RED debug panel
- Verify InfluxDB token/bucket settings

## Security Checklist

Production deployment:
- [ ] Change default passwords (EMQX, Grafana, InfluxDB)
- [ ] Enable MQTT authentication (disable anonymous)
- [ ] Use TLS (port 8883)
- [ ] Verify server certificates (`VERIFY_CERT true`)
- [ ] Firewall: only expose 8883, block 1883
- [ ] Regular backups of InfluxDB data

## Battery Optimization

For remote deployment without WiFi:
1. GPS collects fixes → buffers to LittleFS
2. Deep sleep between fixes (5-15 min intervals)
3. When WiFi available → syncs all buffered data
4. Runtime: ~6 months on 18650 battery with 5-min intervals

```cpp
// In config:
#define PUBLISH_INTERVAL_MS 300000  // 5 minutes
```
