#define CLIENTID    "SysAlertMon"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <json-c/json.h>
#include "MQTTClient.h"

// Header for all shared system data structures and topic IDs
#include "../include/water.h"

// Headers for our new alarm system data types
#include "../alarm_engine/alarm_config.h"
#include "../alarm_engine/alarm_data_types.h"

// --- Blynk Connection Defines ---
#define BLYNK_ADDRESS     "blynk.cloud:1883"
#define BLYNK_CLIENTID    "SysAlertMon_Commands"
#define BLYNK_DEVICE_NAME "device"
#define BLYNK_AUTH_TOKEN  "vrCmamlLChWJ2dgueIoQXLWqnur2puAO"
#define RECONNECT_DELAY_SECONDS 5

// Forward declaration for the alarm checking function that will be included below
void check_alarms(alert_data_t* alert_data);

// Global flag to signal a clear command has been received
volatile int clear_command_received = 0;

// --- Globals for MQTT ---
int verbose = false;
MQTTClient_deliveryToken deliveredtoken;

/**
 * @brief Callback for when an MQTT message delivery is confirmed.
 */
void delivered(void *context, MQTTClient_deliveryToken dt) {
    deliveredtoken = dt;
}

/**
 * @brief Callback for incoming command messages from Blynk.
 */
int command_arrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) {

    printf("Blynk Command Message arrived: topic %s, payload %.*s\n", topicName, message->payloadlen, (char*)message->payload);
    
    if (strcmp(topicName, "downlink/ds/Clear") == 0) {
        if (strncmp((char*)message->payload, "1", message->payloadlen) == 0) {
            printf("--> Command Received: Clear All Alarms\n");
            clear_command_received = 1;
        }
    }
    // Note: We are not handling Mute here as it's a display-only concern for blynkAlert

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

/**
 * @brief Common handler for incoming MQTT messages.
 * This is included from mylib and populates the shared data structures from water.h
 */
#include "../mylib/msgarrvd.c"

/**
 * @brief Callback for when the MQTT connection is lost.
 */
void connlost(void *context, char *cause) {
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
    exit(EXIT_FAILURE); 
}

/**
 * @brief Manages the state of individual alarms, handling trigger delays.
 * This function is called by the auto-generated `check_alarms` function.
 */
void update_alarm_state(alert_data_t* alert_data, int alarm_id, bool condition_is_true) {
    static time_t pending_timestamps[ALARM_COUNT] = {0};

    if (alarm_id < 0 || alarm_id >= ALARM_COUNT) {
        return;
    }

    time_t current_time = time(NULL);
    const AlarmInfo* info = &alarmInfo[alarm_id];
    alarm_status_t* status = &alert_data->alarms[alarm_id];

    if (condition_is_true) {
        if (pending_timestamps[alarm_id] == 0) {
            pending_timestamps[alarm_id] = current_time;
            if (status->internalState != ALARM_STATE_PENDING) {
                if (verbose) printf("Alarm '%s' is now PENDING.\n", info->label);
                status->internalState = ALARM_STATE_PENDING;
            }
        }

        if ((current_time - pending_timestamps[alarm_id]) >= info->trigger_delay_seconds) {
            if (status->alarmState != ALARM_STATE_ACTIVE) {
                if (verbose) printf("Alarm '%s' has become ACTIVE after %ld seconds. Occurrences: %d\n", info->label, info->trigger_delay_seconds, status->occurences + 1);
                status->alarmState = ALARM_STATE_ACTIVE;
                status->eventSend = 1;
                status->occurences++;
                status->internalState = ALARM_STATE_ACTIVE;
            }
        }
    } else {
        if (status->internalState != ALARM_STATE_INACTIVE) {
            if (verbose) printf("Alarm '%s' condition cleared. Returning to INACTIVE.\n", info->label);
        }
        pending_timestamps[alarm_id] = 0;
        status->internalState = ALARM_STATE_INACTIVE;
        status->alarmState = ALARM_STATE_INACTIVE;
        status->eventSend = 0;
    }
}

/**
 * @brief Publishes the current alarm data structure to the MQTT broker.
 */
void publish_alarms(MQTTClient client, alert_data_t* alert_data) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;
    int i;

    pubmsg.payload = alert_data->alarms;
    pubmsg.payloadlen = sizeof(alarm_status_t) * ALARM_COUNT;
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    if ((rc = MQTTClient_publishMessage(client, ALERT_TOPICID, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
        printf("publish_alarms: Failed to publish alert data, rc %d\n", rc);
    } else if (verbose) {
        printf("Published %d bytes of alarm data to %s\n", pubmsg.payloadlen, ALERT_TOPICID);
    }
    json_object *root = json_object_new_object();

    for (i = 0; i < ALARM_COUNT; i++) {
        json_object *alarm_obj = json_object_new_object();
        json_object_object_add(alarm_obj, "alarm_state", json_object_new_int(alert_data->alarms[i].alarmState));
        json_object_object_add(alarm_obj, "internal_state", json_object_new_int(alert_data->alarms[i].internalState));
        json_object_object_add(alarm_obj, "event_send", json_object_new_int(alert_data->alarms[i].eventSend));
        json_object_object_add(alarm_obj, "occurences", json_object_new_int(alert_data->alarms[i].occurences));
        json_object_object_add(root, alarmInfo[i].label, alarm_obj);
    }
    

    const char *json_string = json_object_to_json_string(root);
    pubmsg.payload = (void *)json_string;
    pubmsg.payloadlen = strlen(json_string);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    if ((rc = MQTTClient_publishMessage(client, ALERT_JSONID, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
        printf("publish_alarms: Failed to publish JSON alert data, rc %d\n", rc);
    } else if (verbose) {
        //printf("Published JSON to %s: %s\n", ALERT_JSONID, json_string);
    }
    MQTTClient_waitForCompletion(client, token, TIMEOUT);

    json_object_put(root); // Free the memory allocated to the JSON object
}
void log_alarms(alert_data_t* current_alert_data, const alert_data_t* previous_alert_data) {
    FILE* log_file = fopen("/home/pi/MWPLogData/alarm-log.txt", "a");
    if (log_file == NULL) {
        printf("Error: Unable to open alarm log file for writing.\n");
        return;
    }

    time_t now = time(NULL);
    char* timestamp = ctime(&now);
    // Remove newline from ctime output
    if (timestamp[strlen(timestamp) - 1] == '\n') {
        timestamp[strlen(timestamp) - 1] = '\0';
    }

    for (int i = 0; i < ALARM_COUNT; i++) {
        // Check if this alarm has newly become active (transition from not active to active)
        if (current_alert_data->alarms[i].alarmState == ALARM_STATE_ACTIVE &&
            previous_alert_data->alarms[i].alarmState != ALARM_STATE_ACTIVE) {
            // Log the alarm: timestamp, type, message
            fprintf(log_file, "[%s] %s: %s\n", timestamp, 
                    (alarmInfo[i].type == ALARM_TYPE_CRITICAL) ? "CRITICAL" :
                    (alarmInfo[i].type == ALARM_TYPE_WARN) ? "WARN" : "INFO",
                    alarmInfo[i].eventMessage);
        }
    }

    fclose(log_file);
}
/**
 * @brief This is the auto-generated alarm evaluation logic.
 * It is included here to be part of the same compilation unit.
 */
#include "../alarm_engine/alarm_logic.c.inc"

/**
 * @brief Initializes and connects the MQTT client for receiving commands from Blynk.
 */
int initialize_command_client(MQTTClient* client_handle_ptr) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    if ((rc = MQTTClient_create(client_handle_ptr, BLYNK_ADDRESS, BLYNK_CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("InitializeCommandClient: Failed to create client, rc %d\n", rc);
        return rc;
    }

    if ((rc = MQTTClient_setCallbacks(*client_handle_ptr, NULL, NULL, command_arrvd, delivered)) != MQTTCLIENT_SUCCESS) {
        printf("InitializeCommandClient: Failed to set callbacks, rc %d\n", rc);
        MQTTClient_destroy(client_handle_ptr);
        return rc;
    }

    conn_opts.username = BLYNK_DEVICE_NAME;
    conn_opts.password = BLYNK_AUTH_TOKEN;
    conn_opts.keepAliveInterval = 45;
    conn_opts.cleansession = 1;

    if ((rc = MQTTClient_connect(*client_handle_ptr, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("InitializeCommandClient: Failed to connect, rc %d\n", rc);
        MQTTClient_destroy(client_handle_ptr);
        return rc;
    }

    printf("InitializeCommandClient: Subscribing to downlink/ds/#\n");
    if ((rc = MQTTClient_subscribe(*client_handle_ptr, "downlink/ds/#", QOS)) != MQTTCLIENT_SUCCESS) {
        printf("InitializeCommandClient: Failed to subscribe, rc %d\n", rc);
        MQTTClient_disconnect(*client_handle_ptr, 1000);
        MQTTClient_destroy(client_handle_ptr);
        return rc;
    }

    printf("Command client connected and subscribed successfully.\n");
    return MQTTCLIENT_SUCCESS;
}

int main(int argc, char *argv[]) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int i;
    int rc;
    int opt;
    const char *mqtt_ip = NULL;
    int mqtt_port = 0;

    alert_data_t sys_alerts;
    memset(&sys_alerts, 0, sizeof(sys_alerts));
    // Static copy to track the previous state for change detection
    static alert_data_t previous_sys_alerts;
    memset(&previous_sys_alerts, 0, sizeof(previous_sys_alerts));

    while ((opt = getopt(argc, argv, "vPD")) != -1) {
        switch (opt) {
            case 'v':
                verbose = true;
                break;
            case 'P':
                mqtt_ip = PROD_MQTT_IP;
                mqtt_port = PROD_MQTT_PORT;
                break;
            case 'D':
                mqtt_ip = DEV_MQTT_IP;
                mqtt_port = DEV_MQTT_PORT;
                break;
            default:
                fprintf(stderr, "Usage: %s [-v] [-P | -D]\n", argv[0]);
                return 1;
        }
    }

    if (mqtt_ip == NULL) {
        fprintf(stderr, "Please specify either Production (-P) or Development (-D) server\n");
        return 1;
    }

    char mqtt_address[256];
    snprintf(mqtt_address, sizeof(mqtt_address), "tcp://%s:%d", mqtt_ip, mqtt_port);

    if (verbose) {
        printf("Verbose mode enabled\n");
        printf("Connecting to MQTT broker at %s\n", mqtt_address);
    }

    MQTTClient_create(&client, mqtt_address, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);
    
    conn_opts.keepAliveInterval = 120;
    conn_opts.cleansession = 1;

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }

    // --- Command Client (for Blynk commands) Setup ---
    MQTTClient command_client = NULL;
    bool command_client_connected = false;

    printf("Main: Attempting initial Blynk command client connection...\n");
    if (initialize_command_client(&command_client) == MQTTCLIENT_SUCCESS) {
        command_client_connected = true;
    } else {
        printf("Main: Initial Blynk command client connection failed. Will retry in main loop.\n");
    }

   printf("Subscribing to all monitor topics: %s\nfor client: %s using QoS: %d\n\n", "mwp/data/monitor/#", MONITOR_CLIENTID, QOS);
   log_message("Monitor: Subscribing to topic: %s for client: %s\n", "mwp/data/monitor/#", MONITOR_CLIENTID);
   MQTTClient_subscribe(client, "mwp/data/monitor/#", QOS);
    
    printf("SysAlertMon started. Entering main loop.\n");

    while (1) {
        // --- Handle Command Client State ---
        if (!command_client_connected) {
            printf("Main: Command client not connected. Attempting to reconnect in %d seconds...\n", RECONNECT_DELAY_SECONDS);
            sleep(RECONNECT_DELAY_SECONDS);
            if (initialize_command_client(&command_client) == MQTTCLIENT_SUCCESS) {
                command_client_connected = true;
                printf("Main: Command client reconnected successfully.\n");
            } else {
                printf("Main: Command client reconnection failed. Will retry later.\n");
            }
        }
        
        check_alarms(&sys_alerts);

        if (clear_command_received) {
            printf("Clear command acknowledged. Resetting alarm occurrences.\n");
            for (i = 0; i < ALARM_COUNT; i++) {
                sys_alerts.alarms[i].occurences = 0;
                sys_alerts.alarms[i].alarmState = ALARM_STATE_INACTIVE;
                sys_alerts.alarms[i].internalState = ALARM_STATE_INACTIVE;
                // Also reset the eventSend flag so it can be re-sent if the alarm becomes active again
                sys_alerts.alarms[i].eventSend = 0;
            }
            clear_command_received = 0; // Reset the flag
        }
       // Publish only if sys_alerts has changed since last publish
       if (memcmp(&sys_alerts, &previous_sys_alerts, sizeof(alert_data_t)) != 0) {
            publish_alarms(client, &sys_alerts);
            if (verbose) {
                printf("Alarm states changed. Published updated alarm data.\n");
            }
            log_alarms(&sys_alerts, &previous_sys_alerts);
           // Update the previous state after publishing
           memcpy(&previous_sys_alerts, &sys_alerts, sizeof(alert_data_t));
       }


        sleep(1);
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    if (command_client != NULL) {
        MQTTClient_disconnect(command_client, 10000);
        MQTTClient_destroy(&command_client);
    }
    
    return 0;
} 