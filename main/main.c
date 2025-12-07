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

typedef enum {
    MODE_COMFORT,
    MODE_ECO,
    MODE_H_GEL,
    MODE_STOP,
} modes_t;

static const char* modes_str[] = { "Confort", "Eco", "Hors Gel", "Stop" };

static TaskHandle_t main_task_handle = NULL;
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

static io_mode_config_t heater_io_config[] =
{
    {
        MODE_COMFORT,
        {
            { IO1_0, LOW },
            { IO1_1, LOW },
            { IO2_0, LOW },
            { IO2_1, LOW },
            { IO3_0, LOW },
            { IO3_1, LOW },
            { IO4_0, LOW },
            { IO4_1, LOW },
        }
    },
    {
        MODE_ECO,
        {
            { IO1_0, HIGH },
            { IO1_1, HIGH },
            { IO2_0, HIGH },
            { IO2_1, HIGH },
            { IO3_0, HIGH },
            { IO3_1, HIGH },
            { IO4_0, HIGH },
            { IO4_1, HIGH },
        }
    },
    {
        MODE_H_GEL,
        {
            { IO1_0, LOW  },
            { IO1_1, HIGH },
            { IO2_0, LOW  },
            { IO2_1, HIGH },
            { IO3_0, LOW  },
            { IO3_1, HIGH },
            { IO4_0, LOW  },
            { IO4_1, HIGH },
        }
    },
    {
        MODE_STOP,
        {
            { IO1_0, HIGH },
            { IO1_1, LOW  },
            { IO2_0, HIGH },
            { IO2_1, LOW  },
            { IO3_0, HIGH },
            { IO3_1, LOW  },
            { IO4_0, HIGH },
            { IO4_1, LOW  },
        }
    },
};

static void set_mode_io_levels(const io_config_t* config, uint16 config_len)
{
    for (uint16 i = 0; i < config_len; i++)
    {
        gpio_set_level(config[i].pin, config[i].level);
    }
}

static void set_mode(modes_t mode)
{
    for (uint16 i = 0; i < ARRAY_DIM(heater_io_config); i++)
    {
        io_mode_config_t mode_config = heater_io_config[i];
        if (mode_config.mode == mode)
        {
            set_mode_io_levels(mode_config.config,
                               ARRAY_DIM(mode_config.config));
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

    set_mode(MODE_COMFORT);
}

void main_task(void *arg)
{
    while (1)
    {
        uint32_t mode = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "Mode: %s", modes_str[mode]);
        switch(mode)
        {
            case MODE_COMFORT:
            {
                set_mode(MODE_COMFORT);
                break;
            }
            case MODE_ECO:
            {
                set_mode(MODE_ECO);
                break;
            }
            case MODE_H_GEL:
            {
                set_mode(MODE_H_GEL);
                break;
            }
            case MODE_STOP:
            {
                set_mode(MODE_STOP);
                break;
            }
            default:
                break;
        }
    }
}

void on_mode_comfort(void)
{
    xTaskNotify(main_task_handle, MODE_COMFORT, eSetValueWithOverwrite);
}

void on_mode_eco(void)
{
    xTaskNotify(main_task_handle, MODE_ECO, eSetValueWithOverwrite);
}

void on_mode_h_gel(void)
{
    xTaskNotify(main_task_handle, MODE_H_GEL, eSetValueWithOverwrite);
}

void on_mode_stop(void)
{
    xTaskNotify(main_task_handle, MODE_STOP, eSetValueWithOverwrite);
}

void app_main(void)
{
    gpio_init();

    s_wifi_event_group = xEventGroupCreate();
    wifi_init(s_wifi_event_group);

    mqtt_init(MQTT_TOPIC, s_wifi_event_group);

    mqtt_subscribe_event(MQTT_MSG_MODE_COMFORT, on_mode_comfort);
    mqtt_subscribe_event(MQTT_MSG_MODE_ECO, on_mode_eco);
    mqtt_subscribe_event(MQTT_MSG_MODE_H_GEL, on_mode_h_gel);
    mqtt_subscribe_event(MQTT_MSG_MODE_STOP, on_mode_stop);

    xTaskCreate(main_task, "main_task", 4096, NULL, 5, &main_task_handle);
}
