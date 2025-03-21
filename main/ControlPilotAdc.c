#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/portmacro.h>
#include "driver/adc.h"
#include "rom/ets_sys.h"
#include "driver/gptimer.h"
#include "ControlPilotAdc.h"
#include "ProjectSettings.h"

extern Config_t config;
uint16_t cp_adc_value[NUM_OF_CONNECTORS];

void ControlPilotAdcInit(void)
{
    if (config.vcharge_v5 || config.AC1)
    {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);
    }
}

void readAdcValue(void)
{
    portMUX_TYPE AdcMux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&AdcMux);

    uint32_t i = 0;
    uint32_t val1 = 0;
    for (i = 0; i < 300; i++)
    {
        val1 = val1 + adc1_get_raw(ADC1_CHANNEL_7);
        ets_delay_us(10);
    }
    cp_adc_value[1] = (uint16_t)(val1 / 300);

    portEXIT_CRITICAL(&AdcMux);
}
