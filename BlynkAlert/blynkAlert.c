#define ADDRESS     "blynk.cloud:1883"   // Blynk MQTT Server
#define CLIENTID    "Alert Blynk"               // Unique client ID for your device
#define BLYNK_TOPIC       "batch_ds"
#define BLYNK_DS "ds"
#define BLYNK_PROP_COLOR "prop/color"
#define BLYNK_PROP_LABEL "prop/label"
#define BLYNK_EVENT "event"


#define BLYNK_TEMPLATE_ID "TMPLuui_eIOW"
#define BLYNK_TEMPLATE_NAME "VillaMilano Alerts"
#define BLYNK_DEVICE_NAME "device"
#define BLYNK_AUTH_TOKEN "vrCmamlLChWJ2dgueIoQXLWqnur2puAO"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <json-c/json.h>
#include "unistd.h"
#include "MQTTClient.h"
#include "../include/water.h"
#include "../include/alert.h"
#include <stdbool.h> // Required for using 'bool' type

// Add RECONNECT_DELAY_SECONDS for consistency
#define RECONNECT_DELAY_SECONDS 5

int verbose = FALSE;
int disc_finished = 0;
int subscribed = 0;
int finished = 0;
int clearCommand = 0;
int muteCommand = 0;
MQTTClient_deliveryToken deliveredtoken;

void delivered(void *context, MQTTClient_deliveryToken dt)
{
   //printf("Message with token value %d delivery confirmed\n", dt);
   deliveredtoken = dt;
}

/* Using an include here to allow me to reuse a chunk of code that
   would not work as a library file. So treating it like an include to 
   copy and paste the same code into multiple programs. 
*/
#include "../mylib/msgarrvd.c"
int blynkarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
   int i;
   
   if (verbose) {
      printf("Message arrived:\n");
      printf("          topic: %s  ", topicName);
      printf("         length: %d  ", topicLen);
      printf("     PayloadLen: %d\n", message->payloadlen);
      printf("message: %s\n", message->payload);
   }

   if ( strcmp(topicName, "downlink/ds/Clear") == 0) {
      if (strcmp(message->payload, "1") == 0) {;
         clearCommand = 1;
      }
      else {
         clearCommand = 0;
      }
      printf("-->Command Recv'd: Clear Command: %s\n", message->payload);
   }
   if ( strcmp(topicName, "downlink/ds/Mute") == 0) {
      if (strcmp(message->payload, "1") == 0) {;
         muteCommand = 1;
      }
      else {
         muteCommand = 0;
      }
      printf("-->Command Recv'd: Mute Command: %s\n", message->payload);
   }

   MQTTClient_freeMessage(&message);
   MQTTClient_free(topicName);
   return 1;
}  
void connlost(void *context, char *cause)
{
   printf("\nConnection lost\n");
   printf("     cause: %s\n", cause);
   // This connlost is used by the 'client' instance, not directly for blynkClient state management in this new design.
}

// Modified loop function to ensure consistent error return and client destruction
int loop(MQTTClient blynkClient) // blynkClient is passed by value (handle copy)
{
   MQTTClient_message pubmsg = MQTTClient_message_initializer;
   MQTTClient_deliveryToken token;
   int rc;
   int i;
   int alarmValue = 0;
   char topic[256];   // Buffer for topic
   char payload[256]; // Buffer for payload
   static int initProperties = 0; // This static variable will reset if the application restarts, but not on reconnect.
                                 // Consider if this state needs to be managed differently if loop can be re-entered after client recreation.
                                 // For now, keeping original behavior.
   
   typedef struct {
    unsigned int alarmState   : 8;  // 8-bit field for alarm state
    unsigned int eventSend    : 8;  // 8-bit field for event send flag
    unsigned int occurences   : 8;  // 8-bit field for occurrences
    unsigned int internalState: 8;  // 8-bit field for internal state
   } alarmStatus;

   alarmStatus alarmStat[ALARM_COUNT]; // Array of alarmStatus structures
   

   const char *ledcolor[] = {"Green",
                             "Blue",
                             "Orange",
                             "Red",
                             "Yellow",
                             "Purple",
                             "Fuscia",
                             "Black"};

   const char *ledcolorPalette[] = {"ff0000",  // red
                                    "ffff00",  // yellow
                                    "00ff00",  // green
                                    "0000FF",  // blue
                                    "ff8000",  // orange
                                    "bf00ff",  // purple
                                    "fe2e9a",  // fuscia
                                    "000000"}; // black
  

   if (initProperties == 0) {
      for (i=0; i<=5; i++) {
         // Set label
         sprintf(topic, "%s/%s/%s", BLYNK_DS, alert_ClientData_var_name[i], BLYNK_PROP_LABEL);
         strcpy(payload, alarmInfo[i].label);
         pubmsg.payload = payload;
         pubmsg.payloadlen = strlen(payload);
         pubmsg.qos = 1;
         pubmsg.retained = 0;
         if ((rc = MQTTClient_publishMessage(blynkClient, topic, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
            printf("Loop: Failed to publish initProperties (label) for %s, rc %d\n", alert_ClientData_var_name[i], rc);
            // Not destroying client for this failure, but logging it.
         }

         // Set color
         sprintf(topic, "%s/%s/%s", BLYNK_DS, alert_ClientData_var_name[i], BLYNK_PROP_COLOR);
         strcpy(payload, ledcolorPalette[alarmInfo[i].type]);
         pubmsg.payload = payload;
         pubmsg.payloadlen = strlen(payload);
         if ((rc = MQTTClient_publishMessage(blynkClient, topic, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
            printf("Loop: Failed to publish initProperties (color) for %s, rc %d\n", alert_ClientData_var_name[i], rc);
            // Not destroying client for this failure, but logging it.
         }
      }
      initProperties = 60;
   }
   else {
      initProperties--;
   }     
   // Memcpy the payload to my bit packed structure
   
   memcpy(alarmStat, alert_.data_payload, sizeof(alarmStat));
   for (i=0; i<=5; i++) {
      //printf("%x\n", alert_.data_payload[i]);
      //printf("Alarm %d: State: %d, Event: %d, Occurrences: %d, Internal: %d\n", i, alarmStat[i].alarmState, alarmStat[i].eventSend, alarmStat[i].occurences, alarmStat[i].internalState);
   }
   /* 
    * Send the alarm data to Blynk 100=active alarm; 0=inactive alarm; 1-99=occurrences
    */

   json_object *root = json_object_new_object();
   if (!root) {
       printf("Loop CRITICAL: Failed to create json_object. Skipping this publish cycle.\n");
       return 0; // Not a client failure, but can't proceed with this publish.
   }
   
   for (i=0; i<=5; i++) {
      if (alarmStat[i].alarmState == active) {
         alarmValue = 100;  // full scale if alarm is active
      }
      else {
         alarmValue = alarmStat[i].occurences;    // Set to show occurrences since last reset
      }
      json_object_object_add(root, alert_ClientData_var_name [i], json_object_new_int((alarmValue)));
   }

   const char *json_string = json_object_to_json_string(root);
   pubmsg.payload = (void *)json_string;
   pubmsg.payloadlen = strlen(json_string);
   pubmsg.qos = QOS;
   pubmsg.retained = 0;

   if ((rc = MQTTClient_publishMessage(blynkClient, BLYNK_TOPIC, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
      printf("Loop: Failed to publish main JSON data to topic %s, rc %d. Destroying client.\n", BLYNK_TOPIC, rc);
      json_object_put(root); // Free JSON object
      MQTTClient_disconnect(blynkClient, 1000);
      MQTTClient_destroy(&blynkClient); // Destroy the client resource
      return EXIT_FAILURE; // Signal critical error to main
   }

   // Wait for the message to be delivered
   rc = MQTTClient_waitForCompletion(blynkClient, token, TIMEOUT);
   json_object_put(root); // Free JSON object after MQTT lib is done with payload

   if (rc != MQTTCLIENT_SUCCESS) {
       printf("Loop: Failed MQTTWaitForCompletion for main JSON data, token %d, rc %d. Destroying client.\n", token, rc);
       MQTTClient_disconnect(blynkClient, 1000);
       MQTTClient_destroy(&blynkClient);
       return EXIT_FAILURE; // Signal critical error to main
   }

   /* 
    * Send the alarm event to Blynk - Only once per alarm event
    */
   if (muteCommand == 0) {
      for (i=0; i<=5; i++) {
         if (alarmStat[i].eventSend == 1) {
            sprintf(topic, "%s/%s", BLYNK_EVENT, alert_ClientData_var_name[i]);
            strcpy(payload, alarmInfo[i].eventMessage);
            pubmsg.payload = payload;
            pubmsg.payloadlen = strlen(payload);
            pubmsg.qos = 1; // QOS for events
            pubmsg.retained = 0;
            if ((rc = MQTTClient_publishMessage(blynkClient, topic, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
                printf("Loop: Failed to publish event for %s, rc %d\n", alert_ClientData_var_name[i], rc);
                // Not destroying client for this failure, but logging it.
                // Consider if this should also be a critical failure.
            }
            // Add MQTTClient_waitForCompletion for event messages if QOS 1 requires it and delivery is critical.
         }
      }
   }
   return 0; // Success for this loop iteration
}

// New function to initialize and connect the Blynk Alert MQTT client
int initialize_blynk_alert_client(MQTTClient* client_handle_ptr) {
    MQTTClient_connectOptions blynk_conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    // ADDRESS, CLIENTID, BLYNK_DEVICE_NAME, BLYNK_AUTH_TOKEN are #defines
    if ((rc = MQTTClient_create(client_handle_ptr, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("InitializeAlert: Failed to create Blynk client, rc %d\n", rc);
        return rc;
    }

    // Set specific callbacks for blynkClient: blynkarrvd and delivered. connlost is not set for this client.
    // blynkarrvd is essential for receiving commands like Clear/Mute.
    if ((rc = MQTTClient_setCallbacks(*client_handle_ptr, NULL, NULL /* No connlost for blynkClient */, blynkarrvd, delivered)) != MQTTCLIENT_SUCCESS) {
        printf("InitializeAlert: Failed to set Blynk client callbacks (blynkarrvd, delivered), rc %d\n", rc);
        MQTTClient_destroy(client_handle_ptr); // Clean up
        return rc;
    }

    blynk_conn_opts.username = BLYNK_DEVICE_NAME;
    blynk_conn_opts.password = BLYNK_AUTH_TOKEN;
    blynk_conn_opts.keepAliveInterval = 45; // Original keepAlive
    blynk_conn_opts.cleansession = 1;

    printf("InitializeAlert: Connecting to Blynk MQTT server at %s...\n", ADDRESS);
    if ((rc = MQTTClient_connect(*client_handle_ptr, &blynk_conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("InitializeAlert: Failed to connect to Blynk MQTT server, rc %d\n", rc);
        MQTTClient_destroy(client_handle_ptr); // Clean up
        return rc;
    }
    printf("InitializeAlert: Successfully connected to Blynk MQTT server: %s\n", ADDRESS);

    // Subscribe to downlink topics for commands (Clear, Mute)
    // Assuming QOS is defined elsewhere
    printf("InitializeAlert: Subscribing blynkClient to downlink/ds/# with QoS %d\n", QOS);
    if ((rc = MQTTClient_subscribe(*client_handle_ptr, "downlink/ds/#", QOS)) != MQTTCLIENT_SUCCESS) {
        printf("InitializeAlert: Failed to subscribe blynkClient to downlink/ds/#, rc %d\n", rc);
        MQTTClient_disconnect(*client_handle_ptr, 1000);
        MQTTClient_destroy(client_handle_ptr); // Clean up
        return rc;
    }
    printf("InitializeAlert: blynkClient subscribed to downlink/ds/# successfully.\n");

    return MQTTCLIENT_SUCCESS;
}

int main(int argc, char *argv[])
{
   MQTTClient client; // This is the primary client for alerts from other services
   MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
   // MQTTClient_message pubmsg = MQTTClient_message_initializer; // Not used in main scope
   // MQTTClient_deliveryToken token; // Not used in main scope
   int rc; // General return code
   // int i; // Not used in main scope
   int opt;
   const char *mqtt_ip = NULL;
   int mqtt_port = 0;

   while ((opt = getopt(argc, argv, "vPD")) != -1) {
      switch (opt) {
         case 'v':
               verbose = TRUE;
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

   if (verbose) {
      printf("Verbose mode enabled\n");
   }

   if (mqtt_ip == NULL) {
      fprintf(stderr, "Please specify either Production (-P) or Development (-D) server\n");
      return 1;
   }

   char mqtt_address[256];
   snprintf(mqtt_address, sizeof(mqtt_address), "tcp://%s:%d", mqtt_ip, mqtt_port);

   printf("MQTT Address: %s\n", mqtt_address);
   
   //log_message("Blynk: Started\n");

   if ((rc = MQTTClient_create(&client, mqtt_address, CLIENTID,
                               MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
   {
      printf("Main: Failed to create primary client, return code %d\n", rc);
      MQTTClient_destroy(&client); // Clean up main client
      exit(EXIT_FAILURE);
   }
   
   if ((rc = MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered)) != MQTTCLIENT_SUCCESS)
   {
      printf("Main: Failed to set primary client callbacks, return code %d\n", rc);
      MQTTClient_destroy(&client); // Clean up main client
      exit(EXIT_FAILURE);
   }
   
   conn_opts.keepAliveInterval = 120;
   conn_opts.cleansession = 1;
   //conn_opts.username = mqttUser;       //only if req'd by MQTT Server
   //conn_opts.password = mqttPassword;   //only if req'd by MQTT Server
   if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
   {
      printf("Main: Failed to connect primary client, return code %d\n", rc);
      MQTTClient_destroy(&client); // Clean up main client
      exit(EXIT_FAILURE);
   }


   // --- Blynk Client (blynkClient for Blynk Cloud) Setup and Management ---
   MQTTClient blynkClient = NULL; // Initialize Blynk client handle
   int blynk_client_initialized_and_connected = 0;
   int rc_loop_status;

   printf("Main: Attempting initial Blynk Alert client connection...\n");
   if (initialize_blynk_alert_client(&blynkClient) == MQTTCLIENT_SUCCESS) {
       blynk_client_initialized_and_connected = 1;
   } else {
       printf("Main: Initial Blynk Alert client connection failed. Will retry in main loop.\n");
       // initialize_blynk_alert_client handles its own cleanup on failure
   }
   
   // Subscribe the primary client (client for local system alerts)
   // ALERT_TOPICID, ALERT_CLIENTID, QOS are assumed to be defined
   printf("Main: Subscribing primary client to topics: %s for client: %s using QoS: %d\n", ALERT_TOPICID, ALERT_CLIENTID, QOS);
   // log_message("BlynkAlert: Subscribing to topic: %s for client: %s\n", ALERT_TOPICID, ALERT_CLIENTID); // Original log
   MQTTClient_subscribe(client, ALERT_TOPICID, QOS);


   // --- Main Application Loop ---
   while (TRUE) // Consider a proper shutdown mechanism eventually
   {
      // --- Handle Blynk Alert Client State ---
      if (!blynk_client_initialized_and_connected) {
         printf("Main: Blynk Alert client not connected. Attempting to reconnect in %d seconds...\n", RECONNECT_DELAY_SECONDS);
         sleep(RECONNECT_DELAY_SECONDS);
         if (initialize_blynk_alert_client(&blynkClient) == MQTTCLIENT_SUCCESS) {
            blynk_client_initialized_and_connected = 1;
            printf("Main: Blynk Alert client reconnected successfully.\n");
         } else {
            printf("Main: Blynk Alert client reconnection failed. Will retry later.\n");
            // blynkClient should be NULL or safely managed by initialize_blynk_alert_client
         }
      }

      if (blynk_client_initialized_and_connected) {
         if (blynkClient == NULL) { // Safety check
             printf("CRITICAL ERROR in Main: blynk_client_initialized_and_connected is true, but blynkClient handle is NULL! Correcting state.\n");
             blynk_client_initialized_and_connected = 0;
         } else {
            rc_loop_status = loop(blynkClient); // Call the modified loop
            if (rc_loop_status == EXIT_FAILURE) { // Or -1 if loop uses that
               printf("Main: loop() reported critical MQTT error for Blynk Alert client. Client was destroyed by loop().\n");
               blynkClient = NULL; // Stale handle after destruction in loop
               blynk_client_initialized_and_connected = 0;
            }
         }
      } else {
         printf("Main: Skipping Blynk Alert operations as client is not connected.\n");
      }

      // TODO: Add yielding or other processing for the primary 'client' if necessary
      // MQTTClient_yield(client, 100); // Example if it's not fully callback-driven or needs periodic work

      sleep(1); // Main application cycle delay
   }

   // --- Cleanup before exit ---
   printf("Main: Cleaning up resources before exit...\n");

   // Cleanup for the primary 'client'
   if (client != NULL) {
      printf("Main: Unsubscribing and disconnecting primary client.\n");
      MQTTClient_unsubscribe(client, ALERT_TOPICID); // Use ALERT_TOPICID as used in subscribe
      MQTTClient_disconnect(client, 10000);
      MQTTClient_destroy(&client);
   }

   // Cleanup for blynkClient
   if (blynk_client_initialized_and_connected && blynkClient != NULL) {
      printf("Main: Disconnecting and destroying Blynk Alert client.\n");
      MQTTClient_disconnect(blynkClient, 10000);
      MQTTClient_destroy(&blynkClient);
   } else if (blynkClient != NULL) { // If not marked connected but handle exists
      printf("Main: Destroying non-formally-connected Blynk Alert client.\n");
      MQTTClient_destroy(&blynkClient);
   }

   printf("Main: BlynkAlert application exiting.\n");
   return EXIT_SUCCESS; // Use EXIT_SUCCESS from stdlib.h
}