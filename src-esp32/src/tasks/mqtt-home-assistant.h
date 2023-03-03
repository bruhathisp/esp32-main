#ifndef TASK_HOME_ASSISTANT
#define TASK_HOME_ASSISTANT

#if HA_ENABLED == true

    #include <Arduino.h>
    #include <WiFiClient.h>
    #include <MQTTClient.h>
    #include "../config/config.h"

    WiFiClient HA_net;
    MQTTClient HA_mqtt(1024);
    int statesCount = 5;    // No. of state values part of mqtt message.
extern double measurements[];
extern double measurements_ap[];
extern double measurements_v[];
extern double measurements_a[];
extern double measurements_pf[];

// real power (W), apparent power (VA), rms voltage (V), rms current (A) and power factor
    const char* PROGMEM HA_discovery_topics[] = {
            "homeassistant/sensor/" DEVICE_NAME "rp/config",
            "homeassistant/sensor/" DEVICE_NAME "ap/config",
            "homeassistant/sensor/" DEVICE_NAME "v/config",
            "homeassistant/sensor/" DEVICE_NAME "a/config",
            "homeassistant/sensor/" DEVICE_NAME "pf/config"
        };
    const char* PROGMEM HA_discovery_msg[] = { "{"
            "\"name\":\"" DEVICE_NAME "\","
            "\"device_class\":\"power\","
            "\"unit_of_measurement\":\"W\","
            "\"icon\":\"mdi:transmission-tower\","
            "\"state_topic\":\"homeassistant/sensor/" DEVICE_NAME "/state\","
            "\"value_template\":\"{{ value_json.power}}\","
            "\"device\": {"
                "\"name\":\"" DEVICE_NAME "\","
                "\"sw_version\":\"2.0\","
                "\"model\":\"HW V2\","
                "\"manufacturer\":\"Hari Krishna Vemula\","
                "\"identifiers\":[\"" DEVICE_NAME "\"]"
            "}"
        "}",
        "{"
            "\"name\":\"" DEVICE_NAME "\","
            "\"device_class\":\"power\","
            "\"unit_of_measurement\":\"VA\","
            "\"icon\":\"mdi:transmission-tower\","
            "\"state_topic\":\"homeassistant/sensor/" DEVICE_NAME "/state\","
            "\"value_template\":\"{{ value_json.apparentpower}}\","
            "\"device\": {"
                "\"name\":\"" DEVICE_NAME "\","
                "\"sw_version\":\"2.0\","
                "\"model\":\"HW V2\","
                "\"manufacturer\":\"Hari Krishna Vemula\","
                "\"identifiers\":[\"" DEVICE_NAME "\"]"
            "}"
        "}",
        "{"
            "\"name\":\"" DEVICE_NAME "\","
            "\"device_class\":\"power\","
            "\"unit_of_measurement\":\"V\","
            "\"icon\":\"mdi:transmission-tower\","
            "\"state_topic\":\"homeassistant/sensor/" DEVICE_NAME "/state\","
            "\"value_template\":\"{{ value_json.voltage}}\","
            "\"device\": {"
                "\"name\":\"" DEVICE_NAME "\","
                "\"sw_version\":\"2.0\","
                "\"model\":\"HW V2\","
                "\"manufacturer\":\"Hari Krishna Vemula\","
                "\"identifiers\":[\"" DEVICE_NAME "\"]"
            "}"
        "}",
        "{"
            "\"name\":\"" DEVICE_NAME "\","
            "\"device_class\":\"power\","
            "\"unit_of_measurement\":\"A\","
            "\"icon\":\"mdi:transmission-tower\","
            "\"state_topic\":\"homeassistant/sensor/" DEVICE_NAME "/state\","
            "\"value_template\":\"{{ value_json.current}}\","
            "\"device\": {"
                "\"name\":\"" DEVICE_NAME "\","
                "\"sw_version\":\"2.0\","
                "\"model\":\"HW V2\","
                "\"manufacturer\":\"Hari Krishna Vemula\","
                "\"identifiers\":[\"" DEVICE_NAME "\"]"
            "}"
        "}",
        "{"
            "\"name\":\"" DEVICE_NAME "\","
            "\"device_class\":\"power\","
            "\"unit_of_measurement\":\"\","
            "\"icon\":\"mdi:transmission-tower\","
            "\"state_topic\":\"homeassistant/sensor/" DEVICE_NAME "/state\","
            "\"value_template\":\"{{ value_json.powerfactor}}\","
            "\"device\": {"
                "\"name\":\"" DEVICE_NAME "\","
                "\"sw_version\":\"2.0\","
                "\"model\":\"HW V2\","
                "\"manufacturer\":\"Hari Krishna Vemula\","
                "\"identifiers\":[\"" DEVICE_NAME "\"]"
            "}"
        "}"
    };

    /**
     * Established a connection to Home Assistant MQTT broker.
     * 
     * This task should run continously. It will check if an
     * MQTT connection is active and if so, will sleep for 1
     * minute. If not, a new connection will be established.
     */
    void keepHAConnectionAlive(void * parameter){

        for(;;){
            // When we are connected, loop the MQTT client and sleep for 0,5s
            if(HA_mqtt.connected()){
                HA_mqtt.loop();
                vTaskDelay(250 / portTICK_PERIOD_MS);
                continue;
            }

            if(!WiFi.isConnected()){
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                continue;
            }

            serial_println(F("[MQTT] Connecting to HA..."));
            HA_mqtt.begin(HA_ADDRESS, HA_PORT, HA_net);

            long startAttemptTime = millis();
        
            while (!HA_mqtt.connect(DEVICE_NAME, HA_USER, HA_PASSWORD) &&
                    millis() - startAttemptTime < MQTT_CONNECT_TIMEOUT)
            {
                vTaskDelay(MQTT_CONNECT_DELAY / portTICK_PERIOD_MS);
            }

            if(!HA_mqtt.connected()){
                serial_println(F("[MQTT] HA connection failed. Waiting 30s.."));
                vTaskDelay(30000 / portTICK_PERIOD_MS);
            }

            serial_println(F("[MQTT] HA Connected!"));
        }
    }

    /**
     * TASK: Every 15 minutes we send Home Assistant a discovery message
     *       so that the energy monitor shows up in the device registry.
     */
    void HADiscovery(void * parameter){
        for(;;){
            if(!HA_mqtt.connected()){
                serial_println("[MQTT] HA: no MQTT connection.");
                vTaskDelay(30 * 1000 / portTICK_PERIOD_MS);
                continue;
            }

            serial_println("[MQTT] HA sending auto discovery");
            
            for (int iCnt=0;iCnt<statesCount;iCnt++) {

                serial_print("[TOPIC]");
                serial_println(HA_discovery_topics[iCnt]);
                HA_mqtt.publish(HA_discovery_topics[iCnt], HA_discovery_msg[iCnt]);
                serial_print("Published Discovery Message:");
                serial_println(HA_discovery_msg[iCnt]);
                delay(100);
            }
            
            vTaskDelay(15 * 60 * 1000 / portTICK_PERIOD_MS);
        }
    }

    void sendEnergyToHA(void * parameter){
        serial_println("[MQTT] HA sendEnergyToHA");
        if(!HA_mqtt.connected()){
            serial_println("[MQTT] Can't send to HA without MQTT. Abort.");
            vTaskDelete(NULL);
        }
        serial_print("[MQTT] HA Construct message ");
        char msg[150];
        strcpy(msg, "{\"power\":");
            strcat(msg, String(measurements[LOCAL_MEASUREMENTS-1]).c_str());
            strcat(msg, ",\"apparentpower\":");
            strcat(msg, String(measurements_ap[LOCAL_MEASUREMENTS-1]).c_str());
            strcat(msg, ",\"voltage\":");
            strcat(msg, String(measurements_v[LOCAL_MEASUREMENTS-1]).c_str());
            strcat(msg, ",\"current\":");
            strcat(msg, String(measurements_a[LOCAL_MEASUREMENTS-1]).c_str());
            strcat(msg, ",\"powerfactor\":");
            strcat(msg, String(measurements_pf[LOCAL_MEASUREMENTS-1]).c_str());
        strcat(msg, "}");

        serial_print("[MQTT] HA publish: ");
        serial_println(msg);

        HA_mqtt.publish("homeassistant/sensor/" DEVICE_NAME "/state", msg);

        // Task is done!
        vTaskDelete(NULL);
    }
#endif
#endif
