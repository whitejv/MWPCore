#!/bin/bash

# Set log directory
LOG_DIR="/home/pi/MWPCore/logs"
mkdir -p $LOG_DIR

# Create a timestamped log file
TIMESTAMP=$(date '+%Y%m%d_%H%M%S')
KILL_LOG="${LOG_DIR}/kill_${TIMESTAMP}.log"

# Function to log messages
log_message() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1" >> "$KILL_LOG"
}

log_message "### Stopping Monitoring Processes ###"

# List of process names to kill
PROCESSES=(
    "tankmonitor"
    "wellmonitor"
    "well3monitor"
    "rainbirdmon"
    "irrigationmonitor"
    "housemonitor"
    "monitor"
    "blynkWater"
    "alert"
    "blynkAlert"
    "python"  # This will catch the RainbirdSync processes
)

# Kill each process
for proc in "${PROCESSES[@]}"; do
    log_message "Stopping $proc"
    pkill -f "$proc"
    sleep 1
done

log_message "All processes stopped. Waiting 5 seconds before proceeding..."
sleep 5

# Remove old log files
log_message "Cleaning up old log files..."
rm -rf "${LOG_DIR}"

# Verify nothing is left running
log_message "Checking for remaining processes..."
ps -ef | grep -E "$(printf "%s|" "${PROCESSES[@]}")" | grep -v grep >> "$KILL_LOG"

log_message "### Kill Process Complete ###"
