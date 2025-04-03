
void publishLogMessage(MQTTClient client, union LOG_ *log_data, const char *message_id) {
   json_object *root = json_object_new_object();
   char topic[250];

   MQTTClient_message pubmsg = MQTTClient_message_initializer;
   MQTTClient_deliveryToken token;
   // Create JSON object with proper types
   // First two fields are integers
   json_object_object_add(root, log_ClientData_var_name[0], 
      json_object_new_int(log_data->log.Controller));
   json_object_object_add(root, log_ClientData_var_name[1], 
      json_object_new_int(log_data->log.Zone));  
   json_object_object_add(root, log_ClientData_var_name[2], 
      json_object_new_double(log_data->log.pressurePSI));
   json_object_object_add(root, log_ClientData_var_name[3], 
      json_object_new_double(log_data->log.temperatureF));
   json_object_object_add(root, log_ClientData_var_name[4], 
      json_object_new_double(log_data->log.intervalFlow));
   json_object_object_add(root, log_ClientData_var_name[5], 
      json_object_new_double(log_data->log.amperage));
   json_object_object_add(root, log_ClientData_var_name[6], 
      json_object_new_double(log_data->log.secondsOn));
   json_object_object_add(root, log_ClientData_var_name[7], 
      json_object_new_double(log_data->log.gallonsTank));


   const char *json_string = json_object_to_json_string(root);
   
   // Create topic string with message_id
   snprintf(topic, sizeof(topic), "%s%s/", LOG_JSONID, message_id);
   
   // Prepare and publish MQTT message
   pubmsg.payload = (void *)json_string;
   pubmsg.payloadlen = strlen(json_string);
   pubmsg.qos = QOS;
   pubmsg.retained = 0;
   
   MQTTClient_publishMessage(client, topic, &pubmsg, &token);
   MQTTClient_waitForCompletion(client, token, TIMEOUT);
   
   json_object_put(root);
}