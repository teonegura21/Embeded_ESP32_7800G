#!/bin/bash
# Cloud GPS Tracker — Deployment Script
# Run this on your server/VPS to deploy the full stack

set -e

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║  LilyGo GPS Tracker — Cloud Deployment                    ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo -e "${RED}✗ Docker not found!${NC}"
    echo "Install Docker first:"
    echo "  curl -fsSL https://get.docker.com | sh"
    exit 1
fi

if ! command -v docker-compose &> /dev/null; then
    echo -e "${RED}✗ Docker Compose not found!${NC}"
    echo "Install Docker Compose:"
    echo "  sudo apt install docker-compose-plugin"
    exit 1
fi

echo -e "${GREEN}✓ Docker found${NC}"

# Create directories
mkdir -p certs backups grafana/dashboards grafana/provisioning/datasources grafana/provisioning/dashboards node-red-flows

# Check for existing data
echo
if [ -d "emqx-data" ] || [ -d "influxdb-data" ]; then
    echo -e "${YELLOW}⚠ Existing data found!${NC}"
    read -p "Keep existing data? (y/n): " keep_data
    if [[ $keep_data != "y" ]]; then
        echo "Removing old data..."
        docker-compose down -v 2>/dev/null || true
        sudo rm -rf emqx-data emqx-log influxdb-data grafana-data node-red-data
        echo -e "${GREEN}✓ Old data removed${NC}"
    fi
fi

# Start services
echo
echo "Starting services..."
docker-compose pull
docker-compose up -d

# Wait for services
echo
echo "Waiting for services to start..."
sleep 5

# Check status
echo
echo "Service status:"
docker-compose ps

echo
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║  Deployment Complete!                                     ║"
echo "╠═══════════════════════════════════════════════════════════╣"
echo "║                                                           ║"
echo "║  Dashboards:                                              ║"
echo "║    • EMQX:     http://$(hostname -I | awk '{print $1}'):18083  ║"
echo "║    • Node-RED: http://$(hostname -I | awk '{print $1}'):1880   ║"
echo "║    • Grafana:  http://$(hostname -I | awk '{print $1}'):3000   ║"
echo "║                                                           ║"
echo "║  MQTT Broker:                                             ║"
echo "║    • Plain:    $(hostname -I | awk '{print $1}'):1883 (disable in prod) ║"
echo "║    • TLS:      $(hostname -I | awk '{print $1}'):8883 (USE THIS)        ║"
echo "║                                                           ║"
echo "║  Update LilyGo firmware with:                             ║"
echo "║    MQTT_BROKER = \"$(hostname -I | awk '{print $1}')\"                    ║"
echo "║    MQTT_PORT = 1883 (or 8883 for TLS)                     ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo

echo "Commands:"
echo "  View logs:    docker-compose logs -f"
echo "  Stop:         docker-compose down"
echo "  Restart:      docker-compose restart"
echo "  Update:       docker-compose pull && docker-compose up -d"
echo

# Generate InfluxDB token
echo -e "${YELLOW}⚠ Action Required:${NC}"
echo "1. Open InfluxDB at http://$(hostname -I | awk '{print $1}'):8086"
echo "2. Complete setup (user: admin, password: gps-admin-password-123)"
echo "3. Generate API token for Node-RED"
echo "4. Configure Node-RED InfluxDB node with the token"
echo
