#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "../mylib/monitor_common.h"
#include "../include/water.h"
#include "../include/sensor_config.h"

// Global variables
static RingBuffer *pressure_rb = NULL;
float avgflowRateGPM = 0.0f;
int pumpState = OFF;
int lastpumpState = OFF;
static struct timespec previous_time = {0, 0};

// Process callback function for the tank monitor
bool process_tank_data(void *daily_gallons_ptr, void *context) {
    float *dailyGallons = (float*)daily_gallons_ptr;
    float intervalFlow = 0.0f;
    
    // Get pressure data and calculate water height
    float raw_pressure = wellSens_.well.adc_x5;
    
    // Initialize the ring buffer if not already done
    if (!pressure_rb) {
        pressure_rb = ring_buffer_create(TANK_PRESSURE_WINDOW_SIZE);
        if (!pressure_rb) {
            fprintf(stderr, "Failed to create pressure ring buffer\n");
            return false;
        }
    }
    
    // Calculate filtered pressure and water height
    float filtered_pressure = ring_buffer_add(pressure_rb, raw_pressure);
    float waterHeight = calculate_pressure(filtered_pressure, &TANK_PRESSURE_CAL);
    
    // Calculate tank volume
    float tank_area = PI * TANK_RADIUS_SQD;
    float tankGallons = ((tank_area * waterHeight) * VOLUME_TO_GALLON);
    float tankPerFull = tankGallons / MAX_TANK_GAL * 100.0f;
    
    // Process flow data
    flowmon(tankSens_.tank.new_data_flag, tankSens_.tank.milliseconds, 
           tankSens_.tank.pulse_count, &avgflowRateGPM, &intervalFlow, TANK_FLOW_CAL_FACTOR);
    
    *dailyGallons += intervalFlow;
    
    // Update tank monitor data
    tankMon_.tank.water_height = waterHeight;
    tankMon_.tank.tank_gallons = tankGallons;
    tankMon_.tank.tank_per_full = tankPerFull;
    tankMon_.tank.intervalFlow = intervalFlow;
    tankMon_.tank.gallonsMinute = avgflowRateGPM;
    tankMon_.tank.gallonsDay = *dailyGallons;
    tankMon_.tank.controller = 3;
    tankMon_.tank.zone = 1;
    tankMon_.tank.temperatureF = wellSens_.well.temp3;
    
    // Handle float sensor states
    int float100State = tankSens_.tank.gpio_sensor & 0x01;
    int float25State = (tankSens_.tank.gpio_sensor & 0x02) >> 1;
    tankMon_.tank.float1 = float100State;
    tankMon_.tank.float2 = float25State;
    tankMon_.tank.cycleCount = wellSens_.well.cycle_count;
    tankMon_.tank.fwVersion = 0;
    
    // Calculate time difference
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    tankMon_.tank.secondsOn = calculate_time_diff(&previous_time, &current_time, true);
    
    // Populate log structure
    log_.log.Controller = 5;
    log_.log.Zone = 0;
    log_.log.pressurePSI = tankMon_.tank.water_height;
    log_.log.temperatureF = tankMon_.tank.temperatureF;
    log_.log.intervalFlow = tankMon_.tank.intervalFlow;
    log_.log.amperage = wellSens_.well.adc_x5;
    log_.log.secondsOn = tankMon_.tank.secondsOn;
    log_.log.gallonsTank = tankMon_.tank.tank_gallons;
    
    // Publish log message
    publishLogMessage(client, &log_, "tank");
    
    // Check pump state for training mode
    if (wellMon_.well.well_pump_3_on == 1) {
        pumpState = ON;
    } else {
        pumpState = OFF;
    }
    
    // Handle pump state changes (could be moved to a separate function)
    if (pumpState == ON && lastpumpState == OFF) {
        // Pump just turned on
        lastpumpState = ON;
    } else if (pumpState == OFF && lastpumpState == ON) {
        // Pump just turned off
        lastpumpState = OFF;
    }
    
    return true; // Continue running
}

int main(int argc, char* argv[]) {
    int opt;
    const char *mqtt_ip = NULL;
    int mqtt_port = 0;
    bool verbose = FALSE;
    int training_mode = FALSE;
    char training_filename[256] = {0};
    
    // Parse command line arguments
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
                training_mode = TRUE;
                if (optarg && strlen(optarg) > 0) {
                    snprintf(training_filename, 256, "%s%s", trainingdata, optarg);
                } else {
                    fprintf(stderr, "Error: No filename provided for the -T option.\n");
                    fprintf(stderr, "Usage: %s [-v] [-P | -D] [-T filename]\n", argv[0]);
                    return 1;
                }
                break;
            default:
                fprintf(stderr, "Usage: %s [-v] [-P | -D] [-T filename]\n", argv[0]);
                return 1;
        }
    }
    
    if (!mqtt_ip) {
        fprintf(stderr, "Please specify either Production (-P) or Development (-D) server\n");
        return 1;
    }
    
    // Setup MQTT subscriptions
    MQTTSubscription tank_subscriptions[] = {
        { WELLSENS_TOPICID, QOS, "Well Sensor Data" },
        { WELLMON_TOPICID,  QOS, "Well Monitor Data" }
    };
    
    // Prepare MQTT address
    char mqtt_address[256];
    snprintf(mqtt_address, sizeof(mqtt_address), "tcp://%s:%d", mqtt_ip, mqtt_port);
    
    // Configure the monitor
    MonitorConfig tank_config = {
        .name = "TankMonitor",
        .mqtt_address = mqtt_address,
        .mqtt_config = {
            .address = mqtt_address,
            .client_id = TANKMON_CLIENTID,
            .topic_id = TANKMON_TOPICID,
            .json_id = TANKMON_JSONID,
            .qos = QOS,
            .keep_alive = 20,
            .username = NULL,
            .password = NULL
        },
        .subscriptions = tank_subscriptions,
        .subscription_count = sizeof(tank_subscriptions) / sizeof(tank_subscriptions[0]),
        .verbose = verbose,
        .training_file = training_mode ? training_filename : NULL,
        .data_payload = tankMon_.data_payload,
        .data_size = TANKMON_LEN,
        .var_names = (const char **)tankmon_ClientData_var_name
    };
    
    // MQTT connection variables
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_deliveryToken token;
    
    // Initialize the monitor
    int rc = monitor_init(&tank_config, &client, &conn_opts, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to initialize monitor: %d\n", rc);
        return 1;
    }
    
    // Run the monitor with our callback
    rc = monitor_run(client, &tank_config, &token, process_tank_data, NULL);
    
    // Cleanup (this is called by monitor_run, but included here for completeness)
    if (pressure_rb) {
        ring_buffer_destroy(pressure_rb);
    }
    
    return rc;
} 