#!/bin/bash

# Set library paths
export LD_LIBRARY_PATH=/usr/local/lib:/home/pi/lib:/home/pi/CodeDev/paho.mqtt.c-master/build/output:/home/pi/json-c/json-c-build

# Create a log directory if it doesn't exist
LOG_DIR="/home/pi/MWPCore/logs"
mkdir -p $LOG_DIR

# Create a timestamped log file
TIMESTAMP=$(date '+%Y%m%d_%H%M%S')
LOG_FILE="${LOG_DIR}/startup_${TIMESTAMP}.log"

# Function to log messages
log_message() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1" >> "$LOG_FILE"
}

# Start logging
log_message "### Starting Monitoring Processes ###"

# Define base directory
BASE_DIR="/home/pi/MWPCore"
BIN_DIR="${BASE_DIR}/bin"

# Start processes with logging
log_message "Starting Tank Monitor App"
nohup ${BIN_DIR}/tankmonitor -P >> "${LOG_DIR}/tankmonitor.log" 2>&1 &
sleep 3

log_message "Starting Well Monitor App"
nohup ${BIN_DIR}/wellmonitor -P >> "${LOG_DIR}/wellmonitor.log" 2>&1 &
sleep 3

nohup ${BIN_DIR}/well3monitor -P >> "${LOG_DIR}/well3monitor.log" 2>&1 &
sleep 3

nohup ${BIN_DIR}/rainbirdmon -P >> "${LOG_DIR}/rainbirdmonitor.log" 2>&1 &
sleep 3

log_message "Starting Irrigation Monitor App"
nohup ${BIN_DIR}/irrigationmonitor -P >> "${LOG_DIR}/irrigationmonitor.log" 2>&1 &
sleep 3

log_message "Starting House Monitor App"
nohup ${BIN_DIR}/housemonitor -P >> "${LOG_DIR}/housemonitor.log" 2>&1 &
sleep 3

log_message "Starting Data Monitor App"
nohup ${BIN_DIR}/monitor -P >> "${LOG_DIR}/monitor.log" 2>&1 &
sleep 3

log_message "Starting Blynk Interface App"
nohup ${BIN_DIR}/blynkWater -P >> "${LOG_DIR}/blynkW.log" 2>&1 &
sleep 3

log_message "Starting Alert Interface App"
nohup ${BIN_DIR}/alert -P >> "${LOG_DIR}/blynkA.log" 2>&1 &
sleep 3

log_message "Starting Blynk Alert Interface App"
nohup ${BIN_DIR}/blynkAlert -P >> "${LOG_DIR}/blynk.log" 2>&1 &
sleep 3

log_message "Starting RainbirdSync for Controller 1"
source /home/pi/mwp_venv/bin/activate && nohup python ${BASE_DIR}/pyrainbird/RainbirdSync.py -P -C 1 >> "${LOG_DIR}/rainbird1.log" 2>&1 &
sleep 3

log_message "Starting RainbirdSync for Controller 2"
source /home/pi/mwp_venv/bin/activate && nohup python ${BASE_DIR}/pyrainbird/RainbirdSync.py -P -C 2 >> "${LOG_DIR}/rainbird2.log" 2>&1 &

log_message "All processes started. Waiting 30 minutes..."
sleep 15

log_message "### Startup Complete ###"
