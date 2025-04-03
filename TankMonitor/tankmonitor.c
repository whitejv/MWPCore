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
float dailyGallons = 0;
float TotalDailyGallons = 0;
float TotalGPM = 0;
float avgflowRateGPM = 0;

int pumpState = 0;
int lastpumpState = 0;
int startGallons = 0;
int stopGallons = 0;
int tankstartGallons = 0;
int tankstopGallons = 0;


/* Function Declarations */

float GallonsInTankPress(void) ;
void GallonsPumped(void) ;
void MyMQTTSetup(char *mqtt_address) ;
void MyMQTTPublish(void) ;
float moving_average(float new_sample, float samples[], uint8_t *sample_index, uint8_t window_size) ;

MQTTClient client;
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
MQTTClient_message pubmsg = MQTTClient_message_initializer;
MQTTClient_deliveryToken token;MQTTClient_deliveryToken deliveredtoken;

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
   int i = 0;
   int rc;
   FILE *fptr;
   time_t t;
   time_t start_t, end_t;
   double diff_t;
   struct tm timenow;
   time(&t);

   int SecondsFromMidnight = 0 ;
   int PriorSecondsFromMidnight =0;
   float intervalFlow = 0;
   float waterHeightPress = 0;
   float temperatureF;
   float waterHeight = 0;
   float TankGallons = 0;
   float TankPerFull = 0;
   float Tank_Area = 0;
   int Float100State = 0;
   int Float25State = 0;
   // Add a static variable to track the previous timestamp
   static struct timespec previous_time = {0, 0}; // Initialized to 0 at the start
   struct timespec current_time;

   int opt;
   const char *mqtt_ip;
   int mqtt_port;
   int training_mode = FALSE;
   char training_filename[256];
   while ((opt = getopt(argc, argv, "vPDT:")) != -1) {
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
         case 'T':
            {
                training_mode = TRUE;
                
                if (optarg != NULL && strlen(optarg) > 0) {
                    snprintf(training_filename, 256, "%s%s", trainingdata, optarg);
                    // Use training_mode and training_filename as needed
                } else {
                    fprintf(stderr, "Error: No filename provided for the -T option.\n");
                    fprintf(stderr, "Usage: %s [-v] [-P | -D] [-T filename]\n", argv[0]);
                    return 1;
                }
            }
            break;
         default:
               fprintf(stderr, "Usage: %s [-v] [-P | -D] [-T filename\n", argv[0]);
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

   log_message("TankMonitor: Started\n");

   /*
   * MQTT Setup
   */

   MyMQTTSetup(mqtt_address);
   
   /*
    * Main Loop
    */

   log_message("TankMonitor: Entering Main Loop\n") ;
   
   while(1)
   {
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
         
         fprintf(fptr, "Daily Gallons Used: %f %s", dailyGallons, ctime(&t));
         dailyGallons = 0; 
         fclose(fptr);
         
      }
      //printf("seconds since midnight: %d\n", SecondsFromMidnight);
      PriorSecondsFromMidnight = SecondsFromMidnight ;
      
      waterHeight = GallonsInTankPress() ;
      /*
       * Use the Equation (PI*R^2*WaterHeight)*VoltoGal to compute Water Gallons in tank
      */
      Tank_Area = PI * Tank_Radius_sqd; // area of base of tank
      TankGallons = ((Tank_Area)*waterHeight) * VoltoGal;
      //printf("Gallons in Tank = %f\n", TankGallons);
      TankPerFull = TankGallons / MaxTankGal * 100;
      // printf("Percent Gallons in Tank = %f\n", TankPerFull);
     
     
      tankMon_.tank.water_height =  waterHeight;
      tankMon_.tank.tank_gallons =  TankGallons;
      tankMon_.tank.tank_per_full =  TankPerFull;

      flowmon(tankSens_.tank.new_data_flag, tankSens_.tank.milliseconds, tankSens_.tank.pulse_count, &avgflowRateGPM, &intervalFlow, .98) ;
      dailyGallons = dailyGallons + intervalFlow;
      tankMon_.tank.intervalFlow = intervalFlow;
      tankMon_.tank.gallonsMinute =  avgflowRateGPM;
      tankMon_.tank.gallonsDay =    dailyGallons;
      tankMon_.tank.controller = 3;
      tankMon_.tank.zone = 1;
      
      temperatureF = wellSens_.well.temp3;
      tankMon_.tank.temperatureF =    temperatureF;

      Float100State = tankSens_.tank.gpio_sensor & 0x01;
      Float25State = (tankSens_.tank.gpio_sensor & 0x02) >> 1;
      
      tankMon_.tank.float1 =    Float100State;
      tankMon_.tank.float2 =    Float25State;
      tankMon_.tank.cycleCount =  wellSens_.well.cycle_count ;
      tankMon_.tank.fwVersion = 0;
      
       
      MyMQTTPublish() ;

      clock_gettime(CLOCK_MONOTONIC, &current_time);

      if (previous_time.tv_sec != 0 || previous_time.tv_nsec != 0) {
         // Calculate the time slice in seconds since the last execution
         tankMon_.tank.secondsOn = (float)(current_time.tv_sec - previous_time.tv_sec) +
                                       (float)(current_time.tv_nsec - previous_time.tv_nsec) / 1.0e9f;
      } else {
         // This is the first execution; initialize secondsOn to 0
         tankMon_.tank.secondsOn = 0.0f;
      }

      // Update the previous timestamp to the current time
      previous_time = current_time;
      // Populate the log structure
      log_.log.Controller = 5;
      log_.log.Zone = 0;
      log_.log.pressurePSI = tankMon_.tank.water_height;
      log_.log.temperatureF = tankMon_.tank.temperatureF;
      log_.log.intervalFlow = tankMon_.tank.intervalFlow;
      log_.log.amperage = wellSens_.well.adc_x5;
      log_.log.secondsOn = tankMon_.tank.secondsOn ;
      log_.log.gallonsTank = tankMon_.tank.tank_gallons;
      
      publishLogMessage(client, &log_, "tank");
      
      if (training_mode && pumpState == ON) {
         FILE *file = fopen(training_filename, "a");
         if (file != NULL) {
            time_t current_time = time(NULL);
            fprintf(file, "%ld, %f, %f, %f\n", current_time, wellMon_.well.amp_pump_3, avgflowRateGPM, 1.0);
            fclose(file);
         } else {
            fprintf(stderr, "Error opening file: %s\n", training_filename);
         }
      }
   /*
    * Run at this interval
    */
   
      sleep(1) ;
   }
   
   log_message("TankMonitor: Exited Main Loop\n");
   MQTTClient_unsubscribe(client, TANKMON_TOPICID);
   MQTTClient_disconnect(client, 10000);
   MQTTClient_destroy(&client);
   return rc;
}

#define SAMPLES_COUNT 50
float samples[SAMPLES_COUNT] = {0};
uint8_t sample_index = 0;
uint8_t window_size = 40; // Change this value to the desired window size (60-100)

float GallonsInTankPress(void) {
   float waterHeight = 0;
   float PresSensorValue = 0;
   float PresSensorRawValue = 0;

   float ConstantX = 3.6557; // Used Excel Polynomial Fitting to come up with equation
   float Constant = 1.2205;
   

   /*
   * Convert Raw hydrostatic Pressure Sensor
   * A/D to Water Height, Gallons & Percent Full
   */

   PresSensorRawValue = wellSens_.well.adc_x5;
           
   PresSensorValue = moving_average(PresSensorRawValue, samples, &sample_index, window_size);
   printf("Sample: %d, Raw Pressure Sensor: %f Moving average Sensor: %f\n", sample_index, PresSensorRawValue, PresSensorValue);
   /*
      *** Use the Equation y=Constandx(x) + Constant solve for x to compute Water Height in tank
      */
/*
   if (PresSensorValue <= Constant)
   {
      PresSensorValue = Constant;
   }
*/
   //WaterHeight = ((PresSensorValue - ConstantX) / Constant) + .1; // The .1 accounts for the sensor not sitting on the bottom
   //printf("PresSensorValue: %f, ConstantX: %f, Constant: %f\n", PresSensorValue, ConstantX, Constant );
   waterHeight = ((PresSensorValue*ConstantX) - Constant) ; 
   //printf("Water Height = %f\n", waterHeight);
   
   return waterHeight;

}

void MyMQTTSetup(char* mqtt_address){

   int rc;

   if ((rc = MQTTClient_create(&client, mqtt_address, TANKMON_CLIENTID,
                               MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
   {
      printf("Failed to create client, return code %d\n", rc);
      log_message("TankMonitor: Error == Failed to Create Client. Return Code: %d\n", rc);
      rc = EXIT_FAILURE;
      exit(EXIT_FAILURE);
   }
   
   if ((rc = MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered)) != MQTTCLIENT_SUCCESS)
   {
      printf("Failed to set callbacks, return code %d\n", rc);
      log_message("TankMonitor: Error == Failed to Set Callbacks. Return Code: %d\n", rc);
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
      log_message("TankMonitor: Error == Failed to Connect. Return Code: %d\n", rc);
      rc = EXIT_FAILURE;
        exit(EXIT_FAILURE);
    }
    
    // Subscribe to required topics
    printf("Subscribing to topic: %s\nfor client: %s using QoS: %d\n\n", 
           WELLSENS_TOPICID, WELLSENS_CLIENTID, QOS);
    log_message("TankMonitor: Subscribing to topic: %s for client: %s\n", 
                WELLSENS_TOPICID, WELLSENS_CLIENTID);
    MQTTClient_subscribe(client, WELLSENS_TOPICID, QOS);
    
    printf("Subscribing to topic: %s\nfor client: %s using QoS: %d\n\n", 
           WELLMON_TOPICID, WELLMON_CLIENTID, QOS);
    log_message("TankMonitor: Subscribing to topic: %s for client: %s\n", 
                WELLMON_TOPICID, WELLMON_CLIENTID);
    MQTTClient_subscribe(client, WELLMON_TOPICID, QOS);
}
void MyMQTTPublish() {
    int rc;
   int i;
    time_t t;
    time(&t);

    if (verbose) {
      for (i=0; i<=TANKMON_LEN-1; i++) {
         printf("%f ", tankMon_.data_payload[i]);
        }
        printf("%s", ctime(&t));
    }
   pubmsg.payload = tankMon_.data_payload;
   pubmsg.payloadlen = TANKMON_LEN * 4;
   pubmsg.qos = QOS;
   pubmsg.retained = 0;
   deliveredtoken = 0;
   if ((rc = MQTTClient_publishMessage(client, TANKMON_TOPICID, &pubmsg, &token)) != MQTTCLIENT_SUCCESS)
   {
      printf("Failed to publish message, return code %d\n", rc);
      log_message("TankMonitor: Error == Failed to Publish Message. Return Code: %d\n", rc);
      rc = EXIT_FAILURE;
    }
    

json_object *root = json_object_new_object();
for (i=0; i<=TANKMON_LEN-1; i++) {
   json_object_object_add(root, tankmon_ClientData_var_name [i], json_object_new_double(tankMon_.data_payload[i]));
}

const char *json_string = json_object_to_json_string(root);

pubmsg.payload = (void *)json_string; // Make sure to cast the const pointer to void pointer
pubmsg.payloadlen = strlen(json_string);
pubmsg.qos = QOS;
pubmsg.retained = 0;
MQTTClient_publishMessage(client, TANKMON_JSONID, &pubmsg, &token);
//printf("Waiting for publication of %s\non topic %s for client with ClientID: %s\n", json_string, TANK_TOPIC, TANK_MONID);
MQTTClient_waitForCompletion(client, token, TIMEOUT);
//printf("Message with delivery token %d delivered\n", token);

json_object_put(root); // Free the memory allocated to the JSON object

}

void PumpStats() {

FILE *fptr;
time_t t;
time_t start_t, end_t;
double diff_t;

time(&t);

if (wellMon_.well.well_pump_3_on  == 1) {
      pumpState = ON;
   }
   else {
      pumpState = OFF;
   }
   
   if ((pumpState == ON) && (lastpumpState == OFF)){
      startGallons = dailyGallons;
      //tankstartGallons = tankmon_data_payload[2];
      time(&start_t);
      lastpumpState = ON;
   }
   else if ((pumpState == OFF) && (lastpumpState == ON)){
      fptr = fopen(flowdata, "a");
      stopGallons = dailyGallons - startGallons ;
      //tankstopGallons = tankmon_data_payload[2];
      time(&end_t);
      diff_t = difftime(end_t, start_t);
      fprintf(fptr, "Last Pump Cycle Gallons Used: %d   ", stopGallons);
      fprintf(fptr, "Run Time: %f  Min. ", (diff_t/60));
      fprintf(fptr, "Well Gallons Pumped: %d  ", (stopGallons-startGallons));
      fprintf(fptr, "%s", ctime(&t));
      fclose(fptr);
      //TotalDailyGallons = TotalDailyGallons + (stopGallons-startGallons);
      lastpumpState = OFF ;
   }
}

float moving_average(float new_sample, float samples[], uint8_t *sample_index, uint8_t window_size) {
    static float sum = 0;
    static uint8_t n = 0;

    // Remove the oldest sample from the sum
    sum -= samples[*sample_index];

    // Add the new sample to the sum
    sum += new_sample;

    // Replace the oldest sample with the new sample
    samples[*sample_index] = new_sample;

    // Update the sample index
    *sample_index = (*sample_index + 1) % window_size;

    // Update the number of samples, up to the window size
    if (n < window_size) {
        n++;
    }

    // Calculate and return the moving average
    return sum / n;
}
