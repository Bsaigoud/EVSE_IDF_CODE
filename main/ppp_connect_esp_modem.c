/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_modem_api.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "projectSettings.h"
#include "ppp_connect.h"
#include "custom_nvs.h"
#include "websocket_connect.h"

extern Config_t config;
static const char *TAG = "ppp_esp_modem";
bool modemDiscoonectedFromPPPServer = false;
const esp_netif_driver_ifconfig_t *ppp_driver_cfg = NULL;
extern bool isWebsocketConnected;

void ppp_task(void *args)
{
    ppp_info_t *ppp_info = args;
    uint32_t NotConnectedWithSyncCount = 0;
    uint32_t websocketOffCount = 0;
    int backoff_time = 15000;
    const int max_backoff = 60000;
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG((char *)config.gsmAPN);
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.dte_buffer_size = 4096;
    dte_config.task_priority = 24;
    dte_config.uart_config.tx_buffer_size = 1024;
    dte_config.uart_config.tx_io_num = MODEM_TX_PIN;
    dte_config.uart_config.rx_io_num = MODEM_RX_PIN;

    esp_modem_dce_t *dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, ppp_info->parent.netif);
    ppp_info->context = dce;

    // ESP_LOGE(TAG, "Resetting Modem:");
    // esp_modem_reset(dce);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    // ESP_LOGE(TAG, "Reset Modem Completed");

    int rssi, ber;
    esp_err_t ret = esp_modem_get_signal_quality(dce, &rssi, &ber);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with %d %s", ret, esp_err_to_name(ret));
        goto failed;
    }
    ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);
    char imei[20];
    ret = esp_modem_get_imei(dce, imei);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_get_imei failed with %d %s", ret, esp_err_to_name(ret));
        goto failed;
    }
    else
    {
        setNULL(config.simIMEINumber);
        memcpy(config.simIMEINumber, imei, strlen(imei));
        custom_nvs_write_config();
    }
    ESP_LOGI(TAG, "IMEI: %s", imei);
    char imsi[20];
    ret = esp_modem_get_imsi(dce, imsi);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_get_imsi failed with %d %s", ret, esp_err_to_name(ret));
        goto failed;
    }
    else
    {
        setNULL(config.simIMSINumber);
        memcpy(config.simIMSINumber, imsi, strlen(imsi));
        custom_nvs_write_config();
    }
    ESP_LOGI(TAG, "IMSI: %s", imsi);
    ret = esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_DATA) failed with %d", ret);
    }

failed:

#define CONTINUE_LATER()            \
    backoff_time *= 2;              \
    if (backoff_time > max_backoff) \
    {                               \
        backoff_time = max_backoff; \
    }                               \
    continue;

    // now let's keep retrying
    while (!ppp_info->stop_task)
    {
        if (isWebsocketConnected == false)
        {
            websocketOffCount++;
            if (websocketOffCount > 90)
            {
                websocketOffCount = 0;
                modemDiscoonectedFromPPPServer = true;
            }
        }
        else
        {
            websocketOffCount = 0;
        }
        if (modemDiscoonectedFromPPPServer)
        {
            modemDiscoonectedFromPPPServer = false;
            ret = esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_COMMAND) failed with %d", ret);
            }
            ESP_LOGE(TAG, "Resetting Modem:");
            esp_modem_reset(dce);
            ESP_LOGE(TAG, "Reset Modem Completed");
            ret = esp_modem_get_signal_quality(dce, &rssi, &ber);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with %d %s", ret, esp_err_to_name(ret));
            }
            ret = esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_DATA) failed with %d", ret);
            }
        }
        ESP_LOGI(TAG, "sleeping for %d ms", backoff_time);
        vTaskDelay(pdMS_TO_TICKS(backoff_time));
        if (ppp_info->parent.connected)
        {
            backoff_time = 5000;
            continue;
        }
        else
        {
            // try if the modem got stuck in data mode
            ESP_LOGI(TAG, "Trying to Sync with modem");
            ret = esp_modem_sync(dce);
            if (ret != ESP_OK)
            {
                ESP_LOGI(TAG, "Switching to command mode");
                esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
                ESP_LOGI(TAG, "Retry sync 3 times");
                for (int i = 0; i < 3; ++i)
                {
                    ret = esp_modem_sync(dce);
                    if (ret == ESP_OK)
                    {
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                if (ret != ESP_OK)
                {
                    ret = esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
                    if (ret != ESP_OK)
                    {
                        ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_COMMAND) failed with %d", ret);
                    }
                    ESP_LOGE(TAG, "Resetting Modem:");
                    esp_modem_reset(dce);
                    ESP_LOGE(TAG, "Reset Modem Completed");
                    ret = esp_modem_get_signal_quality(dce, &rssi, &ber);
                    if (ret != ESP_OK)
                    {
                        ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with %d %s", ret, esp_err_to_name(ret));
                    }
                    ret = esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
                    if (ret != ESP_OK)
                    {
                        ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_DATA) failed with %d", ret);
                    }
                    CONTINUE_LATER();
                }
            }
            else
            {
                NotConnectedWithSyncCount++;
                if (NotConnectedWithSyncCount > 20)
                {
                    modemDiscoonectedFromPPPServer = true;
                }
            }
            ESP_LOGI(TAG, "Manual hang-up before reconnecting");
            ret = esp_modem_at(dce, "ATH", NULL, 2000);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_modem_at failed with %d %s", ret, esp_err_to_name(ret));
            }
            ret = esp_modem_get_signal_quality(dce, &rssi, &ber);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with %d %s", ret, esp_err_to_name(ret));
            }
            else
            {
                ESP_LOGW(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);
                char imei[20];
                ret = esp_modem_get_imei(dce, imei);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "esp_modem_get_imei failed with %d %s", ret, esp_err_to_name(ret));
                }
                else
                {
                    setNULL(config.simIMEINumber);
                    memcpy(config.simIMEINumber, imei, strlen(imei));
                    custom_nvs_write_config();
                    ESP_LOGI(TAG, "IMEI: %s", imei);
                }
                char imsi[20];
                ret = esp_modem_get_imsi(dce, imsi);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "esp_modem_get_imsi failed with %d %s", ret, esp_err_to_name(ret));
                }
                else
                {
                    setNULL(config.simIMSINumber);
                    memcpy(config.simIMSINumber, imsi, strlen(imsi));
                    custom_nvs_write_config();
                    ESP_LOGI(TAG, "IMSI: %s", imsi);
                }
                ret = esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
                if (ret != ESP_OK)
                {
                    CONTINUE_LATER();
                }
            }
        }
    }
    vTaskDelete(NULL);
}

void ppp_destroy_context(ppp_info_t *ppp_info)
{
    esp_modem_dce_t *dce = ppp_info->context;
    esp_err_t err = esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_COMMAND) failed with %d", err);
        return;
    }
    esp_modem_destroy(dce);
}
