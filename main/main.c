#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_http_client.h"
#include "esp_bit_defs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/projdefs.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"
#include <string.h>

#include "config.h"
#include "mqtt.h"
#include "utils.h"
#include "utils/types.h"
#include "wifi.h"

static EventGroupHandle_t s_wifi_event_group;

static const char *TAG = "heater-control";

#define GPIO_OUTPUT_PIN_MASK  BIT(IO1_0) | \
                              BIT(IO1_1) | \
                              BIT(IO2_0) | \
                              BIT(IO2_1) | \
                              BIT(IO3_0) | \
                              BIT(IO3_1) | \
                              BIT(IO4_0) | \
                              BIT(IO4_1)

#define NB_GPIO 8

typedef struct { uint32 pin; uint32 level; } io_config_t;
typedef struct { uint32 mode; const io_config_t config[NB_GPIO]; } io_mode_config_t;

typedef enum
{
    HEATER_1,
    HEATER_2,
    HEATER_3,
    HEATER_4,
} heater_t;

typedef enum {
    MODE_CONFORT,
    MODE_ECO,
    MODE_H_GEL,
    MODE_STOP,
} heater_mode_t;

typedef struct
{
    heater_t heater;
    const char* topic;
} heater_topic_t;

static const heater_topic_t heaters_topics[] =
{
    { HEATER_1, MQTT_TOPIC_HEATER_1 },
    { HEATER_2, MQTT_TOPIC_HEATER_2 },
    { HEATER_3, MQTT_TOPIC_HEATER_3 },
    { HEATER_4, MQTT_TOPIC_HEATER_4 },
};

const struct { const char* topic; heater_t heater; } topic_to_heater[] = {
    { CONFIG_HEATER_CONTROL_MQTT_TOPIC_HEATER_1, HEATER_1 },
    { CONFIG_HEATER_CONTROL_MQTT_TOPIC_HEATER_2, HEATER_2 },
    { CONFIG_HEATER_CONTROL_MQTT_TOPIC_HEATER_3, HEATER_3 },
    { CONFIG_HEATER_CONTROL_MQTT_TOPIC_HEATER_4, HEATER_4 },
};

const struct { const char* msg; heater_mode_t mode; } msg_to_mode[] = {
    { CONFIG_HEATER_CONTROL_MQTT_MSG_MODE_CONFORT, MODE_CONFORT },
    { CONFIG_HEATER_CONTROL_MQTT_MSG_MODE_ECO, MODE_ECO },
    { CONFIG_HEATER_CONTROL_MQTT_MSG_MODE_H_GEL, MODE_H_GEL },
    { CONFIG_HEATER_CONTROL_MQTT_MSG_MODE_STOP, MODE_STOP },
};

typedef struct { heater_mode_t mode; uint32 io_0; uint32 io_1; } a;

static a heater_mode_config[] =
{
    { MODE_CONFORT, LOW,  LOW  },
    { MODE_H_GEL,   LOW,  HIGH },
    { MODE_STOP,    HIGH, LOW  },
    { MODE_ECO,     HIGH, HIGH },
};

typedef struct { heater_t heater; uint32 io_0; uint32 io_1; } b;

static b ios[] =
{
    { HEATER_1, IO1_0, IO1_1 },
    { HEATER_2, IO2_0, IO2_1 },
    { HEATER_3, IO3_0, IO3_1 },
    { HEATER_4, IO4_0, IO4_1 },
};

static void set_mode_io_levels(uint32 io_0_pin,
                               uint32 io_0_level,
                               uint32 io_1_pin,
                               uint32 io_1_level)
{
    gpio_set_level(io_0_pin, io_0_level);
    gpio_set_level(io_1_pin, io_1_level);
}

static void set_mode(heater_t heater, heater_mode_t mode)
{
    for (uint16 i = 0; i < ARRAY_DIM(heater_mode_config); i++)
    {
        a mode_config = heater_mode_config[i];
        if (mode_config.mode == mode)
        {
            for (uint16 j = 0; j < ARRAY_DIM(ios); j++)
            {
                b io_config = ios[j];
                if (io_config.heater == heater)
                {
                    set_mode_io_levels(io_config.io_0, mode_config.io_0,
                                       io_config.io_1, mode_config.io_1);
                    return;
                }
            }
            return;
        }
    }
}

static void gpio_init(void)
{
    gpio_config_t io_conf = {};

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_MASK;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    set_mode(HEATER_1, MODE_STOP);
    set_mode(HEATER_2, MODE_STOP);
    set_mode(HEATER_3, MODE_STOP);
    set_mode(HEATER_4, MODE_STOP);
}

typedef struct
{
    heater_t heater;
    heater_mode_t mode;
} heater_event_t;

QueueHandle_t xStructQueue = NULL;

heater_t getHeaterFromTopic(const char* topic, int topic_len)
{
    for (uint16 i = 0; i < ARRAY_DIM(topic_to_heater); i++)
    {
        if (strncmp(topic, topic_to_heater[i].topic, topic_len) == 0)
        {
            return topic_to_heater[i].heater;
        }
    }
    return HEATER_1;
}

heater_mode_t getModeFromMsg(const char* msg, int msg_len)
{
    for (uint16 i = 0; i < ARRAY_DIM(msg_to_mode); i++)
    {
        if (strncmp(msg, msg_to_mode[i].msg, msg_len) == 0)
        {
            return msg_to_mode[i].mode;
        }
    }
    return MODE_STOP;
}

void main_task(void *arg)
{
    while (1)
    {
        heater_event_t event = {0};

        if (xStructQueue != NULL)
        {
            if (xQueueReceive(xStructQueue,
                             &event,
                             portMAX_DELAY) == pdPASS)
            {
                ESP_LOGI(TAG, "heater=%u, mode=%u", event.heater, event.mode);
                set_mode(event.heater, event.mode);
            }
        }
    }
}

void on_msg(const char* topic,
            int topic_len,
            const char* msg,
            int msg_len)
{
    heater_event_t event = {0};

    event.heater = getHeaterFromTopic(topic, topic_len);
    event.mode = getModeFromMsg(msg, msg_len);

    xQueueSend(xStructQueue, (void *)&event, (TickType_t) 0);
}

void app_main(void)
{
    gpio_init();

    s_wifi_event_group = xEventGroupCreate();
    wifi_init(s_wifi_event_group);

    mqtt_init();

    for (uint16 i = 0; i < ARRAY_DIM(heaters_topics); i++)
    {
        mqtt_subscribe_topic(heaters_topics[i].topic);
    }

    mqtt_subscribe_listener(on_msg);

    mqtt_start(s_wifi_event_group);

    xStructQueue = xQueueCreate(10, sizeof(heater_event_t));

    xTaskCreate(main_task, "main_task", 4096, NULL, 5, NULL);
}
