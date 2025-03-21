#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_mac.h"
#include <string.h>
#include "esp_log.h"
#include "sw_serial.h"
#include "ProjectSettings.h"
#include "slave.h"

#define BUF_SIZE_SERIAL 250
#define SERIAL_BAUD 9600

#define TAG "SW_SERIAL"

SwSerial *sw_serial = NULL;

bool sw_serial_initialized = false;

char *mac_to_str(char *buffer, uint8_t *mac)
{
    sprintf(buffer, MACSTR, MAC2STR(mac));
    return buffer;
}

void SW_Serial_write_wifiChannel(uint8_t channel)
{
    if (sw_serial_initialized == false)
    {
        sw_serial_initialized = true;
        sw_serial = sw_new(SLAVE_TX_PIN, SLAVE_RX_PIN, false, BUF_SIZE_SERIAL);
    }

    uint8_t dummydata[5] = {85, 3, WIFI_CHANNEL_ID, channel, 124};

    dummydata[4] = 0;
    for (uint8_t k = 0; k < 4; k++)
    {
        dummydata[4] = dummydata[4] + dummydata[k];
    }
    if (sw_serial != NULL)
    {
        sw_open(sw_serial, SERIAL_BAUD); // 115200 > 9600
        // puts("sw_write test");
        for (uint8_t i = 0; i < 5; i++)
        {
            sw_write(sw_serial, dummydata[i]);
        }
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}
void sw_serial_init(void)
{
    uint8_t my_mac[6];
    esp_efuse_mac_get_default(my_mac);
    char my_mac_str[13];
    ESP_LOGI(TAG, "My mac %s", mac_to_str(my_mac_str, my_mac));
    if (sw_serial_initialized == false)
    {
        sw_serial_initialized = true;
        sw_serial = sw_new(SLAVE_TX_PIN, SLAVE_RX_PIN, false, BUF_SIZE_SERIAL);
    }
    uint8_t dummydata[10] = {85, 8, MAC_ID, 1, 2, 3, 4, 5, 6, 124};
    memcpy(&dummydata[3], my_mac, 6);
    dummydata[9] = 0;
    for (uint8_t k = 0; k < 9; k++)
    {
        dummydata[9] = dummydata[9] + dummydata[k];
    }

    if (sw_serial != NULL)
    {
        sw_open(sw_serial, SERIAL_BAUD); // 115200 > 9600
        puts("sw_write test");
        for (uint8_t i = 0; i < 10; i++)
        {
            sw_write(sw_serial, dummydata[i]);
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    // sw_del(tmp);
}
