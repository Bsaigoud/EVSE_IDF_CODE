#include <driver/i2c.h>
#include <string.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"
#include "ProjectSettings.h"
#include "rtc_clock.h"
#include "pcf8563.h"

i2c_dev_t rtc_dev;

void RTC_Clock_init(void)
{
    i2cdev_init();
    memset(&rtc_dev, 0, sizeof(i2c_dev_t));
    pcf8563_init_desc(&rtc_dev, 1, I2C1_SDA, I2C1_SCL);
}

void RTC_Clock_setTime(struct tm time)
{
    pcf8563_set_time(&rtc_dev, &time);
}

void RTC_Clock_setTimeWithBuffer(const char *timestamp)
{
    struct tm tm_time = {0};
    sscanf(timestamp, "%d-%d-%dT%d:%d:%d.%*3sZ",
           &tm_time.tm_year, &tm_time.tm_mon, &tm_time.tm_mday,
           &tm_time.tm_hour, &tm_time.tm_min, &tm_time.tm_sec);

    tm_time.tm_year -= 1900; // Adjust year
    tm_time.tm_mon -= 1;     // Adjust month

    pcf8563_set_time(&rtc_dev, &tm_time);
}

struct tm RTC_Clock_getTime(void)
{
    struct tm time;
    bool valid = false;
    esp_err_t r = pcf8563_get_time(&rtc_dev, &time, &valid);
    if (r == ESP_OK)
        printf("%04d-%02d-%02d %02d:%02d:%02d, %s\n", time.tm_year + 1900, time.tm_mon + 1,
               time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec, "VALID");
    else
        printf("Error %d: %s\n", r, esp_err_to_name(r));

    return time;
}