#!/bin/bash

# Configuration
SERVICE_NAME="blueproximity"
USERNAME="michael"
WORK_DIR="/home/michael/blueproximity.cpp"
EXEC_SCRIPT="$WORK_DIR/run.sh"
SERVICE_FILE="/etc/systemd/system/$SERVICE_NAME.service"

echo "Configuring $SERVICE_NAME to run as user $USERNAME..."

# 1. Stop the running daemon if it exists
echo "Stopping any running BlueProximity processes..."
# Pkill returns 0 if at least one process matched and was signaled
pkill -f "./BlueProximity" || echo "No running process found."

# 2. Create the systemd service file
# We use sudo to write to /etc/systemd/system
echo "Creating service file at $SERVICE_FILE..."
sudo bash -c "cat > $SERVICE_FILE" <<EOF
[Unit]
Description=BlueProximity Bluetooth Lock Service
After=network.target bluetooth.target

[Service]
Type=simple
User=$USERNAME
WorkingDirectory=$WORK_DIR
ExecStart=$EXEC_SCRIPT
# Restart automatically if it crashes
Restart=always
RestartSec=5
# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=$SERVICE_NAME

[Install]
WantedBy=multi-user.target
EOF

# 3. Reload systemd, enable and start the service
echo "Reloading systemd..."
sudo systemctl daemon-reload

echo "Enabling $SERVICE_NAME to start on boot..."
sudo systemctl enable $SERVICE_NAME

echo "Starting $SERVICE_NAME..."
sudo systemctl start $SERVICE_NAME

# 4. Show status
echo "Service status:"
sudo systemctl status $SERVICE_NAME --no-pager
