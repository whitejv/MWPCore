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

MQTTClient client;
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
MQTTClient_message pubmsg = MQTTClient_message_initializer;
MQTTClient_deliveryToken token;
int verbose = FALSE;

float TotalDailyGallons = 0;
float TotalGPM = 0;


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
#include "../mylib/publogmsg.c"

void connlost(void *context, char *cause)
{
   printf("\nConnection lost\n");
   printf("     cause: %s\n", cause);
}

int main(int argc, char* argv[])
{
   int i=0;
   FILE *fptr;
   time_t t;
   time_t start_t, end_t;
   double diff_t;
   struct tm timenow;
   time(&t);
   int SecondsFromMidnight = 0 ;
   int PriorSecondsFromMidnight =0;
// Add a static variable to track the previous timestamp
   static struct timespec previous_time = {0, 0}; // Initialized to 0 at the start
   struct timespec current_time;

   float irrigationPressure = 0;
   float temperatureF;

   float intervalFlow = 0;
   float dailyGallons = 0;
   float avgflowRateGPM = 0;
    
   int pumpState = 0;
   int lastpumpState = 0;
   int startGallons = 0;
   int stopGallons = 0;
   int tankstartGallons = 0;
   int tankstopGallons = 0;
   // Define the bit-packed structure
   struct FlowData {
     uint32_t pulses : 12;       // 12 bits for pulse count
     uint32_t milliseconds : 19; // 19 bits for milliseconds
     uint32_t newData : 1;       // 1 bit for new data flag
   } ;

   struct FlowData flowData;

   int rc;
   int opt;
   const char *mqtt_ip;
   int mqtt_port;
   int training_mode = FALSE;
   int limit_mode = FALSE;
   float limit_gallons = 0;
   float start_limit_gallons = 0;
   char training_filename[256];
   while ((opt = getopt(argc, argv, "vPDL:T:")) != -1) {
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
         case 'L':
               limit_mode = TRUE;
               if (optarg != NULL && strlen(optarg) > 0) {
                   limit_gallons = atof(optarg);
                   // Use limit_mode and limit_gallons as needed
               } else {
                   fprintf(stderr, "Warning: Number of gallons not provided. Using default 200 for the -L option.\n");
                   fprintf(stderr, "Usage: %s [-v] [-P | -D] [-L #ofGallons] [-T filename]\n", argv[0]);
                   limit_gallons = 200 ;
               }
               break;
         case 'T':
            {
                training_mode = TRUE;
                
                if (optarg != NULL && strlen(optarg) > 0) {
                    snprintf(training_filename, 256, "%s%s", trainingdata, optarg);
                    // Use training_mode and training_filename as needed
                } else {
                    fprintf(stderr, "Error: No filename provided for the -T option.\n");
                    fprintf(stderr, "Usage: %s [-v] [-P | -D] [-L #ofGallons] [-T filename]\n", argv[0]);
                    return 1;
                }
            }
            break;
         default:
               fprintf(stderr, "Usage: %s [-v] [-P | -D] [-L #ofGallons] [-T filename]\n", argv[0]);
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

   log_message("IrrigationMonitor Started\n");

   if ((rc = MQTTClient_create(&client, mqtt_address, IRRIGATIONMON_CLIENTID,
                               MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
   {
      printf("Failed to create client, return code %d\n", rc);
      log_message("IrrigationMonitor Error == Failed to Create Client. Return Code: %d\n", rc);
      rc = EXIT_FAILURE;
      exit(EXIT_FAILURE);
   }
   
   if ((rc = MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered)) != MQTTCLIENT_SUCCESS)
   {
      printf("Failed to set callbacks, return code %d\n", rc);
      log_message("IrrigationMonitor Error == Failed to Set Callbacks. Return Code: %d\n", rc);
      rc = EXIT_FAILURE;
      exit(EXIT_FAILURE);
   }
   
   conn_opts.keepAliveInterval = 20;
   conn_opts.cleansession = 1;
   //conn_opts.username = mqttUser;       //only if req'd by MQTT Server
   //conn_opts.password = mqttPassword;   //only if req'd by MQTT Server
   if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
   {
      printf("Failed to connect, return code %d\n", rc);
      log_message("IrrigationMonitor Error == Failed to Connect. Return Code: %d\n", rc);
      rc = EXIT_FAILURE;
      exit(EXIT_FAILURE);
   }
   printf("Subscribing to topic: %s\nfor client: %s using QoS: %d\n\n", WELLSENS_CLIENTID, WELLSENS_TOPICID, QOS);
   log_message("IrrigationMonitor Subscribing to topic: %s for client: %s\n", WELLSENS_CLIENTID, WELLSENS_TOPICID);
   MQTTClient_subscribe(client, WELLSENS_TOPICID, QOS);
   
   printf("Subscribing to topic: %s\nfor client: %s using QoS: %d\n\n", WELLMON_TOPICID, WELLMON_CLIENTID, QOS);
   log_message("IrrigationMonitor Subscribing to topic: %s for client: %s\n", WELLMON_TOPICID, WELLMON_CLIENTID);
   MQTTClient_subscribe(client, WELLMON_TOPICID, QOS);

   printf("Subscribing to topic: %s for client: %s using QoS: 0\n", IRRIGATIONRESPONSE_TOPICID, IRRIGATIONRESPONSE_CLIENTID );
   log_message("IrrigationMonitor Subscribing to topic: %s for client: %s\n",IRRIGATIONRESPONSE_TOPICID, IRRIGATIONRESPONSE_CLIENTID);
   MQTTClient_subscribe(client, IRRIGATIONRESPONSE_TOPICID, 0);
   
   /*
    * Main Loop
    */

   log_message("IrrigationMonitor Entering Main Loop\n") ;
   
   while(1) {
      time(&t);
      localtime_r(&t, &timenow);
      
      /*
       * Check the time and see if we passed midnight
       * if we have then reset data like MyPumpStats to 0 for a new day
       */
      
      SecondsFromMidnight = (timenow.tm_hour * 60 * 60) + (timenow.tm_min * 60) + timenow.tm_sec ;
      if (SecondsFromMidnight < PriorSecondsFromMidnight) {
         
         fptr = fopen(flowfile, "a");
         
         /* reset 24 hr stuff */
         
         fprintf(fptr, "Daily Irrigation Gallons Used: %f %s", dailyGallons, ctime(&t));
         dailyGallons = 0; 
         fclose(fptr);
         
      }
      //printf("seconds since midnight: %d\n", SecondsFromMidnight);
      PriorSecondsFromMidnight = SecondsFromMidnight ;
      
      /*
      * Call the flow monitor function
      */
      memcpy(&flowData, &wellSens_.well.flowData2, sizeof(struct FlowData));
      flowmon(flowData.newData, flowData.milliseconds, flowData.pulses, &avgflowRateGPM, &intervalFlow, 1.0) ;
      //flowmon(irrigationSens_.irrigation.new_data_flag, irrigationSens_.irrigation.milliseconds, irrigationSens_.irrigation.pulse_count, &avgflowRateGPM, &intervalFlow, .935) ;
      dailyGallons += intervalFlow ;
      //irrigationPressure = ((irrigationSens_.irrigation.adc_sensor * 0.1329)-4.7351);
      irrigationPressure  = ((wellSens_.well.adc_x7 - .5) * 25);
      
      temperatureF = wellSens_.well.temp2 ;
        
      /*
       * Rainbird Monitor is source of Controller & Zone info
       */

       irrigationMon_.irrigation.controller = irrigationResponse_.irrigation.command_response_w1 ;
       irrigationMon_.irrigation.zone = irrigationResponse_.irrigation.command_response_w2;
      /*
       * Load Up the Data
       */
      irrigationMon_.irrigation.intervalFlow = intervalFlow;
      irrigationMon_.irrigation.amperage = wellMon_.well.amp_pump_4;
      irrigationMon_.irrigation.gallonsMinute = avgflowRateGPM;
      irrigationMon_.irrigation.gallonsDay =   dailyGallons;
      irrigationMon_.irrigation.pressurePSI  =  irrigationPressure;
      irrigationMon_.irrigation.temperatureF  =  temperatureF;
      irrigationMon_.irrigation.cycleCount =  wellSens_.well.cycle_count ;
      irrigationMon_.irrigation.fwVersion = 0;
      
      /* Prevent Reading Flow Data Twice */
      
      wellSens_.well.flowData2 = 0 ;
      
      if (verbose) {
         for (i=0; i<=IRRIGATIONMON_LEN-1; i++) {
            printf("%.3f ", irrigationMon_.data_payload[i]);
         }
         printf("%s", ctime(&t));
      }
      
      pubmsg.payload = irrigationMon_.data_payload;
      pubmsg.payloadlen = IRRIGATIONMON_LEN * 4;
      pubmsg.qos = QOS;
      pubmsg.retained = 0;
      deliveredtoken = 0;
      if ((rc = MQTTClient_publishMessage(client, IRRIGATIONMON_TOPICID, &pubmsg, &token)) != MQTTCLIENT_SUCCESS)
      {
         printf("Failed to publish message, return code %d\n", rc);
         log_message("IrrigationMonitor Error == Failed to Publish Message. Return Code: %d\n", rc);
         rc = EXIT_FAILURE;
      }
      json_object *root = json_object_new_object();
      for (i=0; i<=IRRIGATIONMON_LEN-1; i++) {
         json_object_object_add(root, irrigationmon_ClientData_var_name [i], json_object_new_double(irrigationMon_.data_payload[i]));
      }

      const char *json_string = json_object_to_json_string(root);

      pubmsg.payload = (void *)json_string; // Make sure to cast the const pointer to void pointer
      pubmsg.payloadlen = strlen(json_string);
      pubmsg.qos = QOS;
      pubmsg.retained = 0;
      MQTTClient_publishMessage(client, IRRIGATIONMON_JSONID, &pubmsg, &token);
      //printf("Waiting for publication of %s\non topic %s for client with ClientID: %s\n", json_string, TANK_TOPIC, TANK_MONID);
      MQTTClient_waitForCompletion(client, token, TIMEOUT);
      //printf("Message with delivery token %d delivered\n", token);

      json_object_put(root); // Free the memory allocated to the JSON object
      /*
       * Run at this interval
       */
      /*
       * Log the Data Based on Pump 4 on/off
       */
      if (wellMon_.well.irrigation_pump_on == 1) {
         pumpState = ON;
      }
      else {
         pumpState = OFF;
      }

      if ((pumpState == ON) && (lastpumpState == OFF)){
         startGallons = dailyGallons;
         start_limit_gallons = dailyGallons;
         time(&start_t);
         lastpumpState = ON;
         
      }
      else if ((pumpState == OFF) && (lastpumpState == ON)){
         fptr = fopen(flowdata, "a");
         stopGallons = dailyGallons - startGallons ;
         time(&end_t);
         diff_t = difftime(end_t, start_t);
         if (irrigationMon_.irrigation.controller == 1) {
            fprintf(fptr, "Controller: Front ");
            fprintf(fptr, "Zone: %d   ", (int)irrigationMon_.irrigation.zone);
         }
         else if (irrigationMon_.irrigation.controller == 2) {
            fprintf(fptr, "Controller: Back  ");
            fprintf(fptr, "Zone: %d   ", (int)irrigationMon_.irrigation.zone);
         }
         else {
            fprintf(fptr, "Controller: ??  ");
            fprintf(fptr, "Zone: ??   ");
         }
         fprintf(fptr, "Gallons Used: %d   ", stopGallons);
         fprintf(fptr, "Run Time: %f  Min. ", (diff_t/60));
         fprintf(fptr, "%s", ctime(&t));
         fclose(fptr);
         lastpumpState = OFF ;
         sleep(1);
         irrigationMon_.irrigation.controller  =    0;  
         irrigationMon_.irrigation.zone        =    0;  
      }
      else if (( pumpState == ON) && (limit_mode == TRUE)) {
         if ((dailyGallons - start_limit_gallons) > limit_gallons) {
            pubmsg.payload = rainbird_command4;
            pubmsg.payloadlen = strlen(rainbird_command4);
            pubmsg.qos = 0;
            pubmsg.retained = 0;
            deliveredtoken = 0;
            if ((rc = MQTTClient_publishMessage(client, RAINBIRDCOMMAND_TOPICID, &pubmsg, &token)) != MQTTCLIENT_SUCCESS)
            {
               printf("Failed to publish rainbird command message for controller STOP, return code %d\n", rc);
               log_message("IrrigationMonitor Error == Failed to Publish Rainbird Command Message for controller 1. Return Code: %d\n", rc);
               rc = EXIT_FAILURE;
            }
            sleep(3) ;
         }
      }
      if (pumpState == ON) {
             clock_gettime(CLOCK_MONOTONIC, &current_time);

         if (previous_time.tv_sec != 0 || previous_time.tv_nsec != 0) {
            // Calculate the time slice in seconds since the last execution
            irrigationMon_.irrigation.secondsOn = (float)(current_time.tv_sec - previous_time.tv_sec) +
                                          (float)(current_time.tv_nsec - previous_time.tv_nsec) / 1.0e9f;
         } else {
            // This is the first execution; initialize secondsOn to 0
            irrigationMon_.irrigation.secondsOn = 0.0f;
         }

         // Update the previous timestamp to the current time
         previous_time = current_time;
         // Populate the log structure
         log_.log.Controller = (int)irrigationMon_.irrigation.controller;
         log_.log.Zone = (int)irrigationMon_.irrigation.zone;
         log_.log.pressurePSI = irrigationMon_.irrigation.pressurePSI;
         log_.log.temperatureF = irrigationMon_.irrigation.temperatureF;
         log_.log.intervalFlow = irrigationMon_.irrigation.intervalFlow;
         log_.log.amperage = irrigationMon_.irrigation.amperage;
         log_.log.secondsOn = irrigationMon_.irrigation.secondsOn ;
         log_.log.gallonsTank = 0;
         
         publishLogMessage(client, &log_, "irrigation");
      } else {
      // If the pump is not running, set secondsOn to 0
         irrigationMon_.irrigation.secondsOn = 0.0f;

      // Optionally update previous_time to avoid long intervals when the pump starts again
         clock_gettime(CLOCK_MONOTONIC, &previous_time);
      }
      
      if (training_mode && pumpState == ON) {
         FILE *file = fopen(training_filename, "a");
         if (file != NULL) {
            time_t current_time = time(NULL);
            fprintf(file, "%ld, %f, %f, %f\n", current_time, wellMon_.well.amp_pump_4, avgflowRateGPM, irrigationPressure);
            fclose(file);
         } else {
            fprintf(stderr, "Error opening file: %s\n", training_filename);
         }
      }
      sleep(1) ;
   }
   
   log_message("IrrigationMonitor Exited Main Loop\n");
   MQTTClient_unsubscribe(client, IRRIGATIONMON_TOPICID);
   MQTTClient_disconnect(client, 10000);
   MQTTClient_destroy(&client);
   return rc;
}