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

// Forward declaration for the alarm checking function that will be included below
void check_alarms(alert_data_t* alert_data);


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
            status->internalState = ALARM_STATE_PENDING;
        }

        if ((current_time - pending_timestamps[alarm_id]) >= info->trigger_delay_seconds) {
            if (status->alarmState != ALARM_STATE_ACTIVE) {
                status->alarmState = ALARM_STATE_ACTIVE;
                status->eventSend = 1;
                status->occurences++;
            }
            status->internalState = ALARM_STATE_ACTIVE;
        }
    } else {
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
    MQTTClient_publishMessage(client, ALERT_JSONID, &pubmsg, &token);
    MQTTClient_waitForCompletion(client, token, TIMEOUT);

    json_object_put(root); // Free the memory allocated to the JSON object
}

/**
 * @brief This is the auto-generated alarm evaluation logic.
 * It is included here to be part of the same compilation unit.
 */
#include "../alarm_engine/alarm_logic.c.inc"


int main(int argc, char *argv[]) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    int opt;
    const char *mqtt_ip = NULL;
    int mqtt_port = 0;

    alert_data_t sys_alerts;
    memset(&sys_alerts, 0, sizeof(sys_alerts));

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

   printf("Subscribing to all monitor topics: %s\nfor client: %s using QoS: %d\n\n", "mwp/data/monitor/#", MONITOR_CLIENTID, QOS);
   log_message("Monitor: Subscribing to topic: %s for client: %s\n", "mwp/data/monitor/#", MONITOR_CLIENTID);
   MQTTClient_subscribe(client, "mwp/data/monitor/#", QOS);
    
    printf("SysAlertMon started. Entering main loop.\n");

    while (1) {
        check_alarms(&sys_alerts);
        publish_alarms(client, &sys_alerts);
        sleep(1);
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return 0;
} 