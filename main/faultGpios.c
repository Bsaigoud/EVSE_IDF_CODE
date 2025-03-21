#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include <esp_log.h>
#include "ocpp_task.h"
#include "ProjectSettings.h"
#include "faultGpios.h"
#include "slave.h"

#define TAG_RELAY "RELAY"

extern Config_t config;

bool earthDisconnectCircuit = false;

uint8_t relay_state = 0;
uint32_t relayOnTime = 0;
uint32_t relayOffTime = 0;
int gfciButton_pin = 0;
int earthDisconnectButton_pin = 0;
int emergencyButton_pin = 0;
int relay_pin = 0;
int watchdog_pin = 0;

uint8_t relayWeldDetectionButton = false;
uint8_t gfciTestAlarmButton = false;
uint8_t enclosureOpenButton = false;
uint8_t gfciButton = false;
uint8_t earthDisconnectButton = false;
uint8_t emergencyButton = false;
// uint8_t relayState = false;
void ReadFaultsTask(void *params)
{
    int gfciButton_temp = false;
    int earthDisconnectButton_temp = false;
    int emergencyButton_temp = false;

    while (true)
    {
        // togglewatchdog();
        if (config.AC1)
        {
            if (relay_state == 1)
            {
                if (relayOnTime < 230)
                    relayOnTime++;
                if (relayOnTime > 225)
                {
                    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 256);
                    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
                    ESP_LOGD(TAG_RELAY, "Relay PWM Set to 25");
                }
                else if (relayOnTime > 150)
                {
                    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 512);
                    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
                    ESP_LOGD(TAG_RELAY, "Relay PWM Set to 50");
                }
                else if (relayOnTime > 75)
                {
                    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 768);
                    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
                    ESP_LOGD(TAG_RELAY, "Relay PWM Set to 75");
                }
                else
                {
                    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 1024);
                    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
                    ESP_LOGD(TAG_RELAY, "Relay PWM Set to 100");
                }
            }
            else
            {
                if (relayOffTime < 230)
                    relayOffTime++;
            }
        }

        if (config.gfci)
        {
            gfciButton_temp = gpio_get_level(gfciButton_pin);
        }
        if (earthDisconnectCircuit && (config.AC1 == false))
        {
            earthDisconnectButton_temp = gpio_get_level(earthDisconnectButton_pin);
        }
        if (config.vcharge_v5)
        {
            emergencyButton_temp = gpio_get_level(emergencyButton_pin);
        }
        vTaskDelay(20 / portTICK_PERIOD_MS);
        if (config.gfci)
        {
            if (gfciButton_temp == gpio_get_level(gfciButton_pin))
            {
                gfciButton = (gfciButton_temp == 1) ? true : false;
            }
        }
        else
        {
            gfciButton = false;
        }
        if (earthDisconnectCircuit && (config.AC1 == false))
        {
            if (earthDisconnectButton_temp == gpio_get_level(earthDisconnectButton_pin))
            {
                earthDisconnectButton = (earthDisconnectButton_temp == 1) ? true : false;
            }
        }
        if ((emergencyButton_temp == gpio_get_level(emergencyButton_pin)) && config.vcharge_v5)
        {
            emergencyButton = (emergencyButton_temp == 1) ? true : false;
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void fault_gpios_init(void)
{
    if (config.vcharge_lite_1_4)
    {
        gfciButton_pin = GFCI_V14_PIN;
        earthDisconnectButton_pin = EARTH_DISCONNECT_V14_PIN;
    }
    else if (config.vcharge_v5)
    {
        gfciButton_pin = GFCI_V5_PIN;
        earthDisconnectButton_pin = EARTH_DISCONNECT_V5_PIN;
        emergencyButton_pin = EMERGENCY_V5_PIN;
        relay_pin = RELAY_V5_PIN;
        watchdog_pin = WATCH_DOG_V5_PIN;
    }
    else if (config.AC1)
    {
        gfciButton_pin = GFCI_AC1_PIN;
        relay_pin = RELAY_AC1_PIN;
        watchdog_pin = WATCH_DOG_AC1_PIN;
    }

    if (config.gfci)
    {
        gpio_set_direction(gfciButton_pin, GPIO_MODE_INPUT);
        gpio_pulldown_dis(gfciButton_pin);
        gpio_pullup_dis(gfciButton_pin);
    }

    if (config.Ac11s ||
        config.Ac22s ||
        config.Ac44s ||
        config.Ac14D ||
        config.Ac10DP ||
        config.Ac10DA ||
        config.Ac6D ||
        config.Ac10t ||
        config.Ac14t ||
        config.Ac18t)
    {
        earthDisconnectCircuit = true;
    }
    else
    {
        earthDisconnectCircuit = false;
    }
    if (config.vcharge_v5)
    {
        earthDisconnectCircuit = true;
    }

    if (earthDisconnectCircuit && (config.AC1 == false))
    {
        gpio_set_direction(earthDisconnectButton_pin, GPIO_MODE_INPUT);
        gpio_pulldown_dis(earthDisconnectButton_pin);
        gpio_pullup_dis(earthDisconnectButton_pin);
    }

    if (config.vcharge_v5)
    {
        gpio_set_direction(emergencyButton_pin, GPIO_MODE_INPUT);
        gpio_pulldown_dis(emergencyButton_pin);
        gpio_pullup_dis(emergencyButton_pin);

        gpio_reset_pin(relay_pin);
        gpio_set_direction(relay_pin, GPIO_MODE_OUTPUT);
        gpio_pulldown_dis(relay_pin);
        gpio_pullup_dis(relay_pin);
        gpio_set_level(relay_pin, 0);
    }
    if (config.AC1)
    {
        ledc_timer_config_t timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_10_BIT,
            .timer_num = LEDC_TIMER_0,
            .freq_hz = 1000,
            .clk_cfg = LEDC_AUTO_CLK};

        ledc_timer_config(&timer);

        ledc_channel_config_t channel = {
            .gpio_num = relay_pin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_1,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0};
        ledc_channel_config(&channel);

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
        relay_state = 0;
        relayOnTime = 0;
        relayOffTime = 0;
    }

    if (config.vcharge_v5 || config.AC1)
    {
        gpio_reset_pin(watchdog_pin);
        gpio_set_direction(watchdog_pin, GPIO_MODE_OUTPUT);
        gpio_pulldown_dis(watchdog_pin);
        gpio_pullup_dis(watchdog_pin);
        gpio_set_level(watchdog_pin, 0);
    }

    xTaskCreate(&ReadFaultsTask, "ReadFaultsTask", 2048, NULL, 2, NULL);
}

void updateRelayState(uint8_t relay, uint8_t state)
{
    if (config.AC1)
    {
        if ((relay == RELAY1) && (state == 0) && (relay_state == 1))
        {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
            relay_state = 0;
            relayOnTime = 0;
            relayOffTime = 0;
        }
        else if ((relay == RELAY1) && (state == 1) && (relay_state == 0))
        {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 1024);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
            relay_state = 1;
            relayOnTime = 0;
            relayOffTime = 0;
        }
    }
    else
    {
        if ((relay == RELAY1) && (state == 0))
        {
            gpio_set_level(relay_pin, 0);
        }
        else if ((relay == RELAY1) && (state == 1))
        {
            gpio_set_level(relay_pin, 1);
        }
    }
}

void togglewatchdog(void)
{
    // static uint8_t state = 0;
    // if ((config.vcharge_v5) || (config.AC1))
    //     gpio_set_level(watchdog_pin, state);

    // if (state == 0)
    // {
    //     state = 1;
    // }

    // else if (state == 1)
    // {
    //     state = 0;
    // }

    gpio_set_level(watchdog_pin, 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(watchdog_pin, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(watchdog_pin, 0);
}