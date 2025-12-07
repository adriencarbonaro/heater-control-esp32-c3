#ifndef MQTT_H_
#define MQTT_H_

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"

typedef struct {
    const char* topic;
    bool is_connected;
} mqtt_task_data_t;

typedef void (*mqtt_event_listener_cb_t)(void);

void mqtt_subscribe_event(const char* msg, mqtt_event_listener_cb_t callback);

void mqtt_init(const char* topic, EventGroupHandle_t wifi_event_group);

#endif /* MQTT_H_ */
