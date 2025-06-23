#ifndef ALARM_DATA_TYPES_H
#define ALARM_DATA_TYPES_H

#include "alarm_config.h" // For ALARM_COUNT and other definitions

// Enum for the internal state machine of an alarm
typedef enum {
    ALARM_STATE_INACTIVE = 0,
    ALARM_STATE_PENDING = 1, // Condition is met, but delay timer is running
    ALARM_STATE_ACTIVE = 2   // Alarm is fully active after delay
} alarm_internal_state_t;

// This struct holds the dynamic state of a single alarm.
// Using bitfields to keep it compact (4 bytes per alarm).
typedef struct {
    unsigned int alarmState   : 8;  // Current state (e.g., active, inactive)
    unsigned int eventSend    : 8;  // Flag to indicate a new event should be notified
    unsigned int occurences   : 8;  // Counter for how many times the alarm has triggered
    unsigned int internalState: 8;  // Internal state for the management logic (e.g., pending)
} alarm_status_t;

// This struct holds the entire state for all alarms in the system.
typedef struct {
    alarm_status_t alarms[ALARM_COUNT];
} alert_data_t;

#endif // ALARM_DATA_TYPES_H