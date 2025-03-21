#include <esp_log.h>
#include <inttypes.h>
#include <string.h>
#include "rc522.h"
#include "ocpp.h"
#include "EnergyMeter.h"
#include "Rfid.h"
#include "GpioExpander.h"

uint64_t rfidTagNumber = 0;
extern Config_t config;
extern char rfidTagNumberbuf[20];
extern char RemoteTagNumberbuf[20];
extern char AlprTagNumberbuf[20];
extern bool AlprAuthorize;
bool newRfidRead = false;
static const char *TAG = "RFID";
static rc522_handle_t scanner;

static void rc522_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    rc522_event_data_t *data = (rc522_event_data_t *)event_data;

    switch (event_id)
    {
    case RC522_EVENT_TAG_SCANNED:
    {
        rc522_tag_t *tag = (rc522_tag_t *)data->ptr;
        if (AlprAuthorize)
        {
            ESP_LOGI(TAG, "ALPR Authorization in progress, Can't Process RFID");
        }
        else
        {
            newRfidRead = true;
            rfidTagNumber = tag->serial_number;
            setNULL(rfidTagNumberbuf);
            setNULL(RemoteTagNumberbuf);
            memset(RemoteTagNumberbuf, 0xFF, 10);
            setNULL(AlprTagNumberbuf);
            memset(AlprTagNumberbuf, 0xFF, 10);
            snprintf(rfidTagNumberbuf, sizeof(rfidTagNumberbuf), "%02x%02x%02x%02x",
                     (uint8_t)rfidTagNumber,
                     (uint8_t)(rfidTagNumber / 256),
                     (uint8_t)(rfidTagNumber / 65536),
                     (uint8_t)(rfidTagNumber / (256 * 65536)));
            ESP_LOGI(TAG, "Tag scanned (sn: %s)", rfidTagNumberbuf);
        }
        BuzzerEnable();
        vTaskDelay(pdMS_TO_TICKS(100));
        BuzzerDisable();
    }
    break;
    }
}

void RFID_Init(void)
{
    int rfidChipSelectPin = RFID_CS_PIN;
    if (config.AC1)
    {
        gpio_reset_pin(RFID_CS_AC1_PIN);
        gpio_set_direction(RFID_CS_AC1_PIN, GPIO_MODE_OUTPUT);
        gpio_pulldown_dis(RFID_CS_AC1_PIN);
        gpio_pullup_dis(RFID_CS_AC1_PIN);
        gpio_set_level(RFID_CS_AC1_PIN, 1);
        rfidChipSelectPin = -1;
    }

    rc522_config_t config = {
        .scan_interval_ms = 500,
        .task_stack_size = 4096,
        .task_priority = 10,
        .transport = RC522_TRANSPORT_SPI,
        .spi.host = SPI3_HOST,
        .spi.miso_gpio = PIN_NUM_MISO,
        .spi.mosi_gpio = PIN_NUM_MOSI,
        .spi.sck_gpio = PIN_NUM_CLK,
        .spi.sda_gpio = rfidChipSelectPin,
        .spi.bus_is_initialized = true};

    rc522_create(&config, &scanner);
    rc522_register_events(scanner, RC522_EVENT_ANY, rc522_handler, NULL);
    rc522_start(scanner);
}