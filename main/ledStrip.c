/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"
#include "ledStrip.h"
#include "led_strip.h"
#include "ocpp_task.h"
#include "custom_nvs.h"

extern Config_t config;
extern ConnectorPrivateData_t ConnectorData[NUM_OF_CONNECTORS];
extern ledColors_t ledStateColor[NUM_OF_CONNECTORS];
ledColors_t ledStateColor_old[NUM_OF_CONNECTORS];
extern ledColors_t ledColors;
uint8_t led_loopCount[NUM_OF_CONNECTORS];

static const char *TAG = "LED_STRIP";

// GPIO assignment
#define LED_STRIP_GPIO_PIN RMT_LED_STRIP_GPIO_V5_PIN
// Numbers of the LED in the strip
#define LED_STRIP_LED_COUNT EXAMPLE_LED_NUMBERS
// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ (8 * 1000 * 1000)

led_strip_handle_t configure_led(void)
{
    int PwmLedPin = 0;
    if (config.vcharge_v5)
    {
        PwmLedPin = RMT_LED_STRIP_GPIO_V5_PIN;
    }
    else if (config.AC1)
    {
        PwmLedPin = RMT_LED_STRIP_GPIO_AC1_PIN;
    }
    else
    {
        PwmLedPin = RMT_LED_STRIP_GPIO_V5_PIN;
    }
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = PwmLedPin,                                 // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_COUNT,                             // The number of LEDs in the strip,
        .led_model = LED_MODEL_WS2812,                               // LED strip model
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color order of the strip: GRB
        .flags = {
            .invert_out = false, // don't invert the output signal
        }};

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .mem_block_symbols = 64,               // the memory size of each RMT channel, in words (4 bytes)
        .flags = {
            .with_dma = false, // DMA feature is available on chips like ESP32-S3/P4
        }};

    // LED Strip object handle
    led_strip_handle_t led_strip;
    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}

// static uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3];
// rmt_channel_handle_t led_chan = NULL;
// rmt_tx_channel_config_t tx_chan_config = {
//     .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
//     .gpio_num = RMT_LED_STRIP_GPIO_V5_PIN,
//     .mem_block_symbols = 64, // increase the block size can make the LED less flickering
//     .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
//     .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
// };
// rmt_encoder_handle_t led_encoder = NULL;
// led_strip_encoder_config_t encoder_config = {
//     .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
// };
// rmt_transmit_config_t tx_config = {
//     .loop_count = 0, // no transfer loop
// };

// void setLeds(led_strip_handle_t led_strip, uint8_t count)
// {
//     for (uint8_t j = 0; j < EXAMPLE_LED_NUMBERS; j++)
//     {
//         if (count % 2 == 0)
//         {
//             if (j % 2 == 0)
//                 led_strip_set_pixel(led_strip, j, 0, 255, 0);
//             else
//                 led_strip_set_pixel(led_strip, j, 0, 0, 0);
//         }
//         else
//         {
//             if (j % 2 == 0)
//                 led_strip_set_pixel(led_strip, j, 0, 0, 0);
//             else
//                 led_strip_set_pixel(led_strip, j, 0, 255, 0);
//         }
//         if ((j == (EXAMPLE_LED_NUMBERS - 1)) || (j == (EXAMPLE_LED_NUMBERS - 2)))
//             led_strip_set_pixel(led_strip, j, 255, 255, 255);
//     }
//     led_strip_refresh(led_strip);
// }

void setLEDColor(led_strip_handle_t led_strip, uint8_t red, uint8_t green, uint8_t blue, uint8_t ledsToGlow)
{
    for (uint8_t j = 0; j < EXAMPLE_LED_NUMBERS; j++)
    {
        portMUX_TYPE ledMux = portMUX_INITIALIZER_UNLOCKED;
        portENTER_CRITICAL(&ledMux);
        if (j < ledsToGlow)
        {
            led_strip_set_pixel(led_strip, j, red, green, blue);
        }
        else
        {
            led_strip_set_pixel(led_strip, j, 0, 0, 0);
        }
        portEXIT_CRITICAL(&ledMux);
    }
    led_strip_refresh(led_strip);
}

void LedTask1(void *params)
{
    led_strip_handle_t led_strip = configure_led();

    uint8_t ledId = 1;
    custom_nvs_read_connector_data(1);
    ledStateColor[ledId] = POWERON_LED;

    uint8_t loadingLedsNumber = 0;
    while (true)
    {
        if (ledStateColor[ledId] != ledStateColor_old[ledId])
        {
            led_loopCount[ledId] = 0;
        }
        switch (ledStateColor[ledId])
        {
        case SOLID_BLACK:
            led_strip_clear(led_strip);
            break;
        case SOLID_WHITE:
            led_strip_clear(led_strip);
            setLEDColor(led_strip, 255, 255, 255, EXAMPLE_LED_NUMBERS);
            break;
        case SOLID_ORANGE:
            loadingLedsNumber = led_loopCount[ledId] % EXAMPLE_LED_NUMBERS + 1;
            led_strip_clear(led_strip);
            setLEDColor(led_strip, 255, 255, 0, loadingLedsNumber);
            break;
        case SOLID_GREEN:
            led_strip_clear(led_strip);
            setLEDColor(led_strip, 0, 255, 0, EXAMPLE_LED_NUMBERS);
            break;
        case SOLID_BLUE:
            led_strip_clear(led_strip);
            setLEDColor(led_strip, 0, 0, 255, EXAMPLE_LED_NUMBERS);
            break;
        case SOLID_RED:
            led_strip_clear(led_strip);
            setLEDColor(led_strip, 255, 0, 0, EXAMPLE_LED_NUMBERS);
            break;
        case BLINK_BLUE:
            if (led_loopCount[ledId] % 2 == 0)
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 0, 0, 255, EXAMPLE_LED_NUMBERS);
            }
            else
            {
                led_strip_clear(led_strip);
            }
            break;
        case BLINK_GREEN:
            if (led_loopCount[ledId] % 2 == 0)
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 0, 255, 0, EXAMPLE_LED_NUMBERS);
            }
            else if (led_loopCount[ledId] % 2 == 1)
            {
                led_strip_clear(led_strip);
            }
            break;
        case BLINK_RED:
            if (led_loopCount[ledId] % 2 == 0)
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 255, 0, 0, EXAMPLE_LED_NUMBERS);
            }
            else
            {
                led_strip_clear(led_strip);
            }
            break;
        case BLINK_WHITE:
            if (led_loopCount[ledId] % 2 == 0)
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 255, 255, 255, EXAMPLE_LED_NUMBERS);
            }
            else
            {
                led_strip_clear(led_strip);
            }
            break;
        case SOLID_GREEN5_SOLID_WHITE1:
            if ((led_loopCount[ledId] % 12) < 10)
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 0, 255, 0, EXAMPLE_LED_NUMBERS);
            }
            else
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 255, 255, 255, EXAMPLE_LED_NUMBERS);
            }
            break;
        case BLINK_BLUE5_SOLID_WHITE1:
            if (((led_loopCount[ledId] % 12) < 10) && (led_loopCount[ledId] % 2 == 0))
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 0, 0, 255, EXAMPLE_LED_NUMBERS);
            }
            else if (((led_loopCount[ledId] % 12) < 10) && (led_loopCount[ledId] % 2 == 1))
            {
                led_strip_clear(led_strip);
            }
            else
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 255, 255, 255, EXAMPLE_LED_NUMBERS);
            }
            break;
        case SOLID_BLUE5_SOLID_WHITE1:
            if ((led_loopCount[ledId] % 12) < 10)
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 0, 0, 255, EXAMPLE_LED_NUMBERS);
            }
            else
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 255, 255, 255, EXAMPLE_LED_NUMBERS);
            }
            break;

        case BLINK_GREEN5_SOLID_WHITE1:
            if (((led_loopCount[ledId] % 12) < 10) && (led_loopCount[ledId] % 2 == 0))
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 0, 255, 0, EXAMPLE_LED_NUMBERS);
            }
            else if (((led_loopCount[ledId] % 12) < 10) && (led_loopCount[ledId] % 2 == 1))
            {
                led_strip_clear(led_strip);
            }
            else
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 255, 255, 255, EXAMPLE_LED_NUMBERS);
            }
            break;
        case SOLID_GREEN3_SOLID_BLUE1:
            if ((led_loopCount[ledId] % 8) < 6)
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 0, 255, 0, EXAMPLE_LED_NUMBERS);
            }
            else
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 0, 0, 255, EXAMPLE_LED_NUMBERS);
            }
            break;

        case SOLID_GREEN3_SOLID_BLUE1_SOLID_WHITE1:
            if ((led_loopCount[ledId] % 10) < 6)
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 0, 255, 0, EXAMPLE_LED_NUMBERS);
            }
            else if ((led_loopCount[ledId] % 10) < 8)
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 0, 0, 255, EXAMPLE_LED_NUMBERS);
            }
            else
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 255, 255, 255, EXAMPLE_LED_NUMBERS);
            }
            break;
        case BLINK_RED5_SOLID_WHITE1:
            if (((led_loopCount[ledId] % 12) < 10) && (led_loopCount[ledId] % 2 == 0))
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 255, 0, 0, EXAMPLE_LED_NUMBERS);
            }
            else if (((led_loopCount[ledId] % 12) < 10) && (led_loopCount[ledId] % 2 == 1))
            {
                led_strip_clear(led_strip);
            }
            else
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 255, 255, 255, EXAMPLE_LED_NUMBERS);
            }
            break;
        case SOLID_RED5_SOLID_WHITE1:
            if ((led_loopCount[ledId] % 12) < 10)
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 255, 0, 0, EXAMPLE_LED_NUMBERS);
            }
            else
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 255, 255, 255, EXAMPLE_LED_NUMBERS);
            }
            break;
        case SOLID_BLUE5_BLINK_WHITE1:
            if ((led_loopCount[ledId] % 12) < 10)
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 0, 0, 255, EXAMPLE_LED_NUMBERS);
            }
            else if (led_loopCount[ledId] % 2 == 0)
            {
                led_strip_clear(led_strip);
                setLEDColor(led_strip, 255, 255, 255, EXAMPLE_LED_NUMBERS);
            }
            else
            {
                led_strip_clear(led_strip);
            }
            break;
        default:
            led_strip_clear(led_strip);
            break;
        }
        ledStateColor_old[ledId] = ledStateColor[ledId];
        led_loopCount[ledId]++;
        vTaskDelay(BLINK_TIME / portTICK_PERIOD_MS);
    }
}

void LED_Strip(void)
{
    xTaskCreate(&LedTask1, "LED_TASK1", 3072, "task led1", 20, NULL);
}
