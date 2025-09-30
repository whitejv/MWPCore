#define ADDRESS     "blynk.cloud:1883"   // Blynk MQTT Server
#define CLIENTID    "Water Blynk"               // Unique client ID for your device

#define BLYNK_TEMPLATE_ID "TMPL2ETcb4Wlm"
#define BLYNK_TEMPLATE_NAME "VillaMilano Water"
#define BLYNK_AUTH_TOKEN "vA8SxCCl9M_JVl8nIz6m5GizzcLXsQtM"

#define BLYNK_TOPIC       "batch_ds" // Topic for Virtual Pin V1
#define BLYNK_DEVICE_NAME "device"


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

int verbose = FALSE;
int disc_finished = 0;
int subscribed = 0;
int finished = 0;

// Global flag for connection loss detection, to be set by connlost callback
volatile int connection_lost_flag = 0;
// Reconnect delay in seconds
#define RECONNECT_DELAY_SECONDS 5

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

void connlost(void *context, char *cause)
{
   printf("\nConnection lost\n");
   printf("     cause: %s\n", cause);
   connection_lost_flag = 1; // Signal main loop about connection loss
}

int loop(MQTTClient blynkClient)
{
   MQTTClient_message pubmsg = MQTTClient_message_initializer;
   MQTTClient_deliveryToken token;
   int rc;
   char payload_str[55]; // For LED color strings, ensure size is adequate
   int i;

   const char *ledcolor[] = {"Green",
                             "Blue",
                             "Orange",
                             "Red",
                             "Yellow",
                             "Purple",
                             "Fuscia",
                             "Black"};

   const char *ledcolorPalette[] = {"0x00ff00",  // green
                                    "0x0000ff",  // blue
                                    "0xff8000",  // orange
                                    "0xff0000",  // red
                                    "0xffff00",  // yellow
                                    "0xbf00ff",  // purple
                                    "0xfe2e9a",  // fuscia
                                    "0x000000"}; // black

   // --- LED Color Publishes (retaining original behavior: no critical error check leading to client destruction here) ---
   // A more robust implementation would check return codes for these too.
   strcpy(payload_str, ledcolorPalette[(int)monitor_.monitor.Well_1_LED_Color]);
   pubmsg.payload = (void*)payload_str;
   pubmsg.payloadlen = strlen(payload_str);
   pubmsg.qos = QOS;
   pubmsg.retained = 0;
   MQTTClient_publishMessage(blynkClient, "ds/Well_1_LED_Bright/prop/color", &pubmsg, &token);
   
   strcpy(payload_str, ledcolorPalette[(int)monitor_.monitor.Well_2_LED_Color]);
   MQTTClient_publishMessage(blynkClient, "ds/Well_2_LED_Bright/prop/color", &pubmsg, &token);
   
   strcpy(payload_str, ledcolorPalette[(int)monitor_.monitor.Well_3_LED_Color]);
   MQTTClient_publishMessage(blynkClient, "ds/Well_3_LED_Bright/prop/color", &pubmsg, &token);
   
   strcpy(payload_str, ledcolorPalette[(int)monitor_.monitor.Irrig_4_LED_Color]);
   MQTTClient_publishMessage(blynkClient, "ds/Irrig_4_LED_Bright/prop/color", &pubmsg, &token);


   //***  SEND INFO TO BLYNK (Main JSON Data Publish) ***

   json_object *root = json_object_new_object();
   if (!root) {
      printf("CRITICAL: Failed to create json_object in loop(). Skipping this publish cycle.\n");
      return 0; // Indicate success for this cycle to main, but operation was skipped. Client not destroyed.
   }

   for (i=0; i<=MONITOR_LEN-23; i++) {
      json_object_object_add(root, monitor_ClientData_var_name [i], json_object_new_double(monitor_.data_payload[i]));
   }
   for (i=18; i<=MONITOR_LEN-17; i++) {
      json_object_object_add(root, monitor_ClientData_var_name [i], json_object_new_int(monitor_.data_payload[i]));
   }
   for (i=28; i<=MONITOR_LEN-9; i++) {
      json_object_object_add(root, monitor_ClientData_var_name [i], json_object_new_int(monitor_.data_payload[i]));
   }
   
   const char *json_string = json_object_to_json_string(root);
   pubmsg.payload = (void *)json_string;
   pubmsg.payloadlen = strlen(json_string);
   pubmsg.qos = QOS;
   pubmsg.retained = 0;
   printf("Publishing JSON to topic %s: %s\n", BLYNK_TOPIC, json_string);
   rc = MQTTClient_publishMessage(blynkClient, BLYNK_TOPIC, &pubmsg, &token);
   if (rc != MQTTCLIENT_SUCCESS) {
      printf("MQTT Publish (JSON) failed to topic %s, rc %d. Destroying client in loop().\n", BLYNK_TOPIC, rc);
      json_object_put(root); 
      MQTTClient_disconnect(blynkClient, 1000); // Disconnect client (operates on the resource via handle copy)
      MQTTClient_destroy(&blynkClient);         // Destroy client resource (sets local blynkClient copy to NULL)
      return -1; // Signal critical MQTT failure to main
   }

   rc = MQTTClient_waitForCompletion(blynkClient, token, TIMEOUT);
   json_object_put(root); // Free JSON object after MQTT lib is done with payload (publish and wait)
   // root = NULL; // Good practice if root were to be used further, to prevent double free.

   if (rc != MQTTCLIENT_SUCCESS) {
      printf("MQTT WaitForCompletion (JSON) failed for token %d, topic %s, rc %d. Destroying client in loop().\n", token, BLYNK_TOPIC, rc);
      MQTTClient_disconnect(blynkClient, 1000); 
      MQTTClient_destroy(&blynkClient);        
      return -1; // Signal critical MQTT failure to main
   }
   return 0; // Success for this loop iteration
}

// New function to initialize and connect the Blynk MQTT client
int initialize_blynk_client(MQTTClient* blynk_client_handle_ptr) {
    MQTTClient_connectOptions blynk_conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    // Create Blynk MQTT client
    // ADDRESS, CLIENTID, BLYNK_DEVICE_NAME, BLYNK_AUTH_TOKEN are #defines from the top of the file
    if ((rc = MQTTClient_create(blynk_client_handle_ptr, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("Initialize: Failed to create Blynk MQTT client, rc %d\n", rc);
        return rc;
    }

    // Set Blynk MQTT connection options
    blynk_conn_opts.username = BLYNK_DEVICE_NAME;
    blynk_conn_opts.password = BLYNK_AUTH_TOKEN;
    blynk_conn_opts.keepAliveInterval = 45; // As per original blynkClient settings
    blynk_conn_opts.cleansession = 1;
    // Consider: blynk_conn_opts.automaticReconnect = 1; // If Paho lib supports & desired over manual loop

    printf("Initialize: Connecting to Blynk MQTT server at %s...\n", ADDRESS);
    if ((rc = MQTTClient_connect(*blynk_client_handle_ptr, &blynk_conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Initialize: Failed to connect to Blynk MQTT server, rc %d\n", rc);
        MQTTClient_destroy(blynk_client_handle_ptr); // Clean up client if connection failed
        return rc;
    }

    printf("Initialize: Successfully connected to Blynk MQTT server: %s\n", ADDRESS);
    return MQTTCLIENT_SUCCESS;
}

int main(int argc, char *argv[])
{
   MQTTClient client;
   MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
   int rc;
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
      printf("Failed to create client, return code %d\n", rc);
      //log_message("Blynk: Error == Failed to Create Client. Return Code: %d\n", rc);
      rc = EXIT_FAILURE;
      exit(EXIT_FAILURE);
   }
   
   if ((rc = MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered)) != MQTTCLIENT_SUCCESS)
   {
      printf("Failed to set callbacks, return code %d\n", rc);
      //log_message("Blynk: Error == Failed to Set Callbacks. Return Code: %d\n", rc);
      rc = EXIT_FAILURE;
      exit(EXIT_FAILURE);
   }
   
   conn_opts.keepAliveInterval = 120;
   conn_opts.cleansession = 1;
   //conn_opts.username = mqttUser;       //only if req'd by MQTT Server
   //conn_opts.password = mqttPassword;   //only if req'd by MQTT Server
   if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
   {
      printf("Failed to connect main client, return code %d\n", rc);
      //log_message("Blynk: Error == Failed to Connect. Return Code: %d\n", rc);
      // rc = EXIT_FAILURE; // Original code did not set rc here for exit
      MQTTClient_destroy(&client); // Ensure client is destroyed if connect fails
      exit(EXIT_FAILURE);
   }

   // --- Blynk Client (blynkClient) Setup and Management --- 
   MQTTClient blynkClient = NULL; // Initialize Blynk client handle to NULL
   int blynkClient_initialized_and_connected = 0;
   int rc_loop_status; // To store return status from new loop()

   // Initial attempt to connect the Blynk client
   printf("Main: Attempting initial Blynk client connection...\n");
   if (initialize_blynk_client(&blynkClient) == MQTTCLIENT_SUCCESS) {
      blynkClient_initialized_and_connected = 1;
      // log_message("BlynkW: Initial Blynk client connection successful."); // Optional logging
   } else {
      printf("Main: Initial Blynk client connection failed. Will retry in main loop.\n");
      // initialize_blynk_client handles cleanup of blynkClient on its own failure, so blynkClient should be NULL or safe
      // log_message("BlynkW: Initial Blynk client connection failed."); // Optional logging
   }
   
   // Subscribe the main client (for mwp/data/monitor/#)
   printf("Main: Subscribing main client to all monitor topics: %s\nfor client: %s using QoS: %d\n\n", "mwp/data/monitor/#", MONITOR_CLIENTID, QOS);
   // log_message("BlynkW: Subscribing to topic: %s for client: %s\n", "mwp/data/monitor/#", MONITOR_CLIENTID);
   MQTTClient_subscribe(client, "mwp/data/monitor/#", QOS); // Assuming QOS and MONITOR_CLIENTID are defined

   // --- Main Application Loop ---
   while (TRUE) // Replace TRUE with a proper shutdown condition if needed
   {
      // --- Handle Blynk Client State ---
      if (!blynkClient_initialized_and_connected) {
         printf("Main: Blynk client not connected. Attempting to reconnect in %d seconds...\n", RECONNECT_DELAY_SECONDS);
         // log_message("BlynkW: Attempting to reconnect Blynk client."); // Optional
         sleep(RECONNECT_DELAY_SECONDS); 
         if (initialize_blynk_client(&blynkClient) == MQTTCLIENT_SUCCESS) {
            blynkClient_initialized_and_connected = 1;
            printf("Main: Blynk client reconnected successfully.\n");
            // log_message("BlynkW: Reconnected Blynk client successfully."); // Optional
         } else {
            printf("Main: Blynk client reconnection failed. Will retry later.\n");
            // log_message("BlynkW: Blynk client reconnection failed."); // Optional
            // blynkClient should be NULL or properly managed by initialize_blynk_client upon its failure.
         }
      }

      // If Blynk client is ready, call its processing loop
      if (blynkClient_initialized_and_connected) {
         if (blynkClient == NULL) { // Safety check, should not happen if logic is correct
             printf("CRITICAL ERROR in Main: blynkClient_initialized_and_connected is true, but blynkClient handle is NULL! Correcting state.\n");
             blynkClient_initialized_and_connected = 0; 
         } else {
            rc_loop_status = loop(blynkClient); // Call the modified loop function
            if (rc_loop_status == -1) {
               printf("Main: loop() reported critical MQTT error for Blynk. Client resource was destroyed by loop.\n");
               // log_message("BlynkW: Critical error in Blynk client loop. Client destroyed by loop."); //Optional
               blynkClient = NULL; // The handle 'blynkClient' in main is now stale. Mark it NULL.
                                  // The resource was destroyed by loop(), do not destroy again here.
               blynkClient_initialized_and_connected = 0; 
            } 
            // else if (rc_loop_status == 0) { /* Loop was successful or skipped harmlessly */ }
         }
      } else {
         printf("Main: Skipping Blynk operations as client is not connected.\n");
      }

      // TODO: Add logic for the main 'client' (mwp/data/monitor/#) if it needs yielding or periodic checks.
      // For example, MQTTClient_yield() if 'client' is asynchronous or uses persistence that needs it.
      // The original code structure implies 'client' messages are handled via callbacks (msgarrvd).

      sleep(1); // Main application cycle delay
   }

   //log_message("Blynk: Exited Main Loop\n"); // This seems to refer to the overall application
   
   // --- Cleanup before exit ---
   printf("Main: Cleaning up resources before exit...\n");

   // Cleanup for the first 'client' (mwp/data/monitor/#)
   if (client != NULL) { // Check if client was successfully created
      printf("Main: Unsubscribing and disconnecting main client.\n");
      MQTTClient_unsubscribe(client, "mwp/data/monitor/#");
      MQTTClient_disconnect(client, 10000);
      MQTTClient_destroy(&client);
   }

   // Cleanup for blynkClient if it exists and is connected/initialized
   if (blynkClient_initialized_and_connected && blynkClient != NULL) {
      printf("Main: Disconnecting and destroying Blynk client before exit.\n");
      MQTTClient_disconnect(blynkClient, 10000);
      MQTTClient_destroy(&blynkClient);
   } else if (blynkClient != NULL) { // If not connected but handle isn't NULL (e.g. creation failed mid-way outside init func)
      printf("Main: Destroying non-connected Blynk client before exit.\n");
      MQTTClient_destroy(&blynkClient);
   }

   printf("Main: Application exiting.\n");
   //log_message("Blynk: Exited Main Loop\n"); // Duplicate? Or different context?
   return EXIT_SUCCESS;
}
