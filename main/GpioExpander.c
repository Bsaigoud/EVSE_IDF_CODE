#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include "driver/gpio.h"
#include <esp_log.h>
#include "ProjectSettings.h"
#include "mcp23008.h"

extern Config_t config;
extern uint8_t gfciButton;
extern uint8_t earthDisconnectButton;
extern uint8_t emergencyButton;
extern uint8_t gfciTestAlarmButton;
extern uint8_t enclosureOpenButton;

static const char *TAG = "Expander";
static EventGroupHandle_t ExpanderEventGroup = NULL;
static i2c_dev_t mcp23008_dev = {0};
#define BIT_BUTTON_CHANGED BIT(0)

bool GetBuzzerStatus(void)
{
    bool status;
    if (config.AC1)
        mcp23008_get_level(&mcp23008_dev, 4, &status);
    return status;
}

bool GetBatteryStatus(void)
{
    bool status;
    if (config.AC1)
        mcp23008_get_level(&mcp23008_dev, 5, &status);
    return status;
}

bool GetRelayDropStatus(void)
{
    bool status;
    if (config.AC1)
        mcp23008_get_level(&mcp23008_dev, 6, &status);
    return status;
}

bool GetRfidResetStatus(void)
{
    bool status;
    if (config.AC1)
        mcp23008_get_level(&mcp23008_dev, 7, &status);
    return status;
}

void BuzzerEnable(void)
{
    if (config.AC1)
        mcp23008_set_level(&mcp23008_dev, 4, true);
}

void BuzzerDisable(void)
{
    if (config.AC1)
        mcp23008_set_level(&mcp23008_dev, 4, false);
}

void BatteryEnable(void)
{
    if (config.AC1)
        mcp23008_set_level(&mcp23008_dev, 5, true);
}

void BatteryDisable(void)
{
    if (config.AC1)
        mcp23008_set_level(&mcp23008_dev, 5, false);
}

void RelayDropEnable(void)
{
    if (config.AC1)
        mcp23008_set_level(&mcp23008_dev, 6, true);
}

void RelayDropDisable(void)
{
    if (config.AC1)
        mcp23008_set_level(&mcp23008_dev, 6, false);
}

void RfidResetEnable(void)
{
    if (config.AC1)
        mcp23008_set_level(&mcp23008_dev, 7, true);
}

void RfidResetDisable(void)
{
    if (config.AC1)
        mcp23008_set_level(&mcp23008_dev, 7, false);
}

static void IRAM_ATTR Expander_intr_handler(void *arg)
{
    // On interrupt set bit in event group
    BaseType_t hp_task;
    if (xEventGroupSetBitsFromISR(ExpanderEventGroup, BIT_BUTTON_CHANGED, &hp_task) != pdFAIL)
        portYIELD_FROM_ISR(hp_task);
}

void GpioExpanderTask(void *pvParameters)
{
    mcp23008_init_desc(&mcp23008_dev, EXP_ADDR, EXP_I2C_PORT, I2C1_SDA, I2C1_SCL);
    mcp23008_set_mode(&mcp23008_dev, 0, MCP23008_GPIO_INPUT);  // GFCI TEST ALARM INPUT
    mcp23008_set_mode(&mcp23008_dev, 1, MCP23008_GPIO_INPUT);  // ENCLOSURE OPEN INPUT
    mcp23008_set_mode(&mcp23008_dev, 2, MCP23008_GPIO_INPUT);  // EMERGENCY INPUT
    mcp23008_set_mode(&mcp23008_dev, 3, MCP23008_GPIO_INPUT);  // EARTH DISCONNECT INPUT
    mcp23008_set_mode(&mcp23008_dev, 4, MCP23008_GPIO_OUTPUT); // BUZZER OUTPUT
    mcp23008_set_mode(&mcp23008_dev, 5, MCP23008_GPIO_OUTPUT); // BATTERY ENABLE OUTPUT
    mcp23008_set_mode(&mcp23008_dev, 6, MCP23008_GPIO_OUTPUT); // RELAY DROP OUTPUT
    mcp23008_set_mode(&mcp23008_dev, 7, MCP23008_GPIO_OUTPUT); // RFID RESET OUTPUT
    mcp23008_set_interrupt(&mcp23008_dev, 0, MCP23008_INT_ANY_EDGE);
    mcp23008_set_interrupt(&mcp23008_dev, 1, MCP23008_INT_ANY_EDGE);
    mcp23008_set_interrupt(&mcp23008_dev, 2, MCP23008_INT_ANY_EDGE);
    mcp23008_set_interrupt(&mcp23008_dev, 3, MCP23008_INT_ANY_EDGE);

    // Setup GPIO interrupt
    gpio_set_direction(MCP_23008_INT_PIN, GPIO_MODE_INPUT);
    gpio_set_intr_type(MCP_23008_INT_PIN, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(MCP_23008_INT_PIN, Expander_intr_handler, NULL);

    mcp23008_get_level(&mcp23008_dev, 0, &gfciTestAlarmButton);
    mcp23008_get_level(&mcp23008_dev, 1, &enclosureOpenButton);
    mcp23008_get_level(&mcp23008_dev, 2, &emergencyButton);
    mcp23008_get_level(&mcp23008_dev, 3, &earthDisconnectButton);

    while (1)
    {
        // wait for BIT_BUTTON_CHANGED, clear it on exit
        if (xEventGroupWaitBits(ExpanderEventGroup, BIT_BUTTON_CHANGED, pdTRUE, pdTRUE, portMAX_DELAY) != BIT_BUTTON_CHANGED)
            continue;
        // OK, we got this bit set

        mcp23008_get_level(&mcp23008_dev, 0, &gfciTestAlarmButton);
        mcp23008_get_level(&mcp23008_dev, 1, &enclosureOpenButton);
        mcp23008_get_level(&mcp23008_dev, 2, &emergencyButton);
        mcp23008_get_level(&mcp23008_dev, 3, &earthDisconnectButton);
    }
}

void GpioExpanderInit(void)
{
    if (config.AC1)
    {
        ExpanderEventGroup = xEventGroupCreate();
        xTaskCreate(GpioExpanderTask, "GpioExpanderTask", 4096, NULL, 5, NULL);
    }
}