#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "freertos/FreeRTOS.h"
#include "ProjectSettings.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "slave.h"
#include "OTA.h"
#include "display.h"
#include "esp_ota_ops.h"
#include "ocpp.h"
#include "ocpp_task.h"
#include "esp_spiffs.h"

#define TAG "OTA"
#define TAG_SLAVE "OTA_SLAVE"
#define TAG_CONNECT "CONNECT"
extern Config_t config;
OtaStatus_t MasterOTA = {false, false, false, false, false, false};
OtaStatus_t SlaveOTA = {false, false, false, false, false, false};
bool slaveClientError = false;
extern ledStates_t ledState[NUM_OF_CONNECTORS];
static bool slaveFimwareUpdated = false;
static bool slaveDownloadCompleted = false;
extern bool FirmwareUpdatingForOfflineCharger;
extern CMSFirmwareStatusNotificationResponse_t CMSFirmwareStatusNotificationResponse;
extern CPFirmwareStatusNotificationRequest_t CPFirmwareStatusNotificationRequest;
extern CMSUpdateFirmwareRequest_t CMSUpdateFirmwareRequest;
extern bool FirmwareUpdateStarted;
extern bool FirmwareUpdateFailed;
extern UART_t slave_uart;
double masterOtaDownloadPercentage = 0;
double slaveOtaDownloadPercentage = 0;
double firmwareUpdateProgress = 0;
bool slaveFirmwareStatusReceived = false;
bool updatingMasterAndSlave = false;
// extern const uint8_t server_cert_pem_start[] asm("_binary_google_cer_start");

// esp_err_t client_event_handler(esp_http_client_event_t *evt)
// {
//     return ESP_OK;
// }
esp_err_t client_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer; // Buffer to store response of http request from event handler
    static int output_len;      // Stores number of bytes read
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        slaveClientError = true;
        ESP_LOGE(TAG_SLAVE, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG_SLAVE, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG_SLAVE, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG_SLAVE, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG_SLAVE, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG_SLAVE, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG_SLAVE, "HTTP_EVENT_ON_FINISH");

        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG_SLAVE, "HTTP_EVENT_REDIRECT");
        esp_http_client_set_header(evt->client, "From", "user@example.com");
        esp_http_client_set_header(evt->client, "Accept", "text/html");
        esp_http_client_set_redirection(evt->client);
        break;
    }
    return ESP_OK;
}
esp_err_t validate_image_header(esp_app_desc_t *incoming_ota_desc)
{
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    esp_app_desc_t running_partition_description;
    esp_ota_get_partition_description(running_partition, &running_partition_description);

    ESP_LOGI(TAG, "current version is %s\n", running_partition_description.version);
    ESP_LOGI(TAG, "new version is %s\n", incoming_ota_desc->version);

    if (strcmp(running_partition_description.version, incoming_ota_desc->version) == 0)
    {
        ESP_LOGW(TAG, "NEW VERSION IS THE SAME AS CURRENT VERSION. ABORTING");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void http_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

bool getOtaVersion(void)
{
    char sampleURL[100];
    memset(sampleURL, '\0', sizeof(sampleURL));
    strcat(sampleURL, config.otaURLConfig);
    strcat(sampleURL, "master/");
    strcat(sampleURL, config.serialNumber);

    esp_http_client_config_t clientDownloadConfig = {
        .url = sampleURL,
        .cert_pem = NULL,
        .timeout_ms = 5000,
        .keep_alive_enable = true,
        .skip_cert_common_name_check = true};
    esp_http_client_handle_t clientDownload = esp_http_client_init(&clientDownloadConfig);
    if (clientDownload == NULL)
    {
        ESP_LOGE(TAG_CONNECT, "Failed to initialise HTTP connection");
        return false;
    }
    esp_err_t err = esp_http_client_open(clientDownload, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_CONNECT, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(clientDownload);
        return false;
    }

    esp_http_client_fetch_headers(clientDownload);
    uint8_t buffer[1001];
    uint32_t DownloadBuffSize = 1000;
    uint32_t data_read = 0;
    memset(buffer, 0, sizeof(buffer));
    data_read = esp_http_client_read(clientDownload, (char *)buffer, DownloadBuffSize);
    if (data_read > 0)
    {
        esp_app_desc_t new_app_info;
        if (data_read > (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)))
        {
            // check current version with downloading
            memcpy(&new_app_info, &buffer[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
            ESP_LOGE(TAG_CONNECT, "firmware version read success: %s", new_app_info.version);
        }
        esp_http_client_cleanup(clientDownload);
        return true;
    }
    else
    {
        esp_http_client_cleanup(clientDownload);
        return false;
    }
}

void uploadTask(void)
{
    uint32_t DownloadBuffSize = 200;
    esp_err_t err;
    char sampleURL[100];
    memset(sampleURL, '\0', sizeof(sampleURL));
    memcpy(sampleURL, "http://34.100.138.28/idf_release/sampleTest.php", strlen("http://34.100.138.28/idf_release/sampleTest.php"));
    char downloadBuffer[1024];
    memset(downloadBuffer, '\0', sizeof(downloadBuffer));
    esp_http_client_config_t clientDownloadConfig = {
        .url = sampleURL,
        .cert_pem = NULL,
        .timeout_ms = 5000,
        .keep_alive_enable = true,
        .skip_cert_common_name_check = true};
    esp_http_client_handle_t clientDownload = esp_http_client_init(&clientDownloadConfig);
    if (clientDownload == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialise HTTP connection");
    }
    memcpy(downloadBuffer, "Data written from ESP32", sizeof("Data written from ESP32"));
    esp_http_client_set_method(clientDownload, HTTP_METHOD_POST);
    esp_http_client_set_header(clientDownload, "Content-Type", "text/plain");
    esp_http_client_set_post_field(clientDownload, downloadBuffer, strlen(downloadBuffer));
    err = esp_http_client_perform(clientDownload);
    if (err != ESP_OK)
    {
        esp_http_client_cleanup(clientDownload);
        ESP_LOGE(TAG, "Error: SSL data write error");
    }

    http_cleanup(clientDownload);
}

bool getSlaveFirmwareVersion(void)
{
    if (SlaveSendID(FIRMWARE_VERSION_ID))
    {
        ESP_LOGI(TAG_SLAVE, "Firmware Version Received is: %s", config.slavefirmwareVersion);
        return true;
    }
    ESP_LOGI(TAG_SLAVE, "Firmware Version Received failed");
    return false;
}

esp_err_t PerformSlaveOta(void)
{
    esp_err_t err = ESP_OK;

    uint32_t DownloadBuffSize = 3000;
    char sampleURL[100];
    memset(sampleURL, '\0', sizeof(sampleURL));
    strcat(sampleURL, config.otaURLConfig);
    if (config.vcharge_lite_1_4)
        strcat(sampleURL, "v14/");
    else if (config.vcharge_v5)
        strcat(sampleURL, "v5/");
    else if (config.AC1)
        strcat(sampleURL, "ac1/");
    else if (config.AC2)
        strcat(sampleURL, "ac2/");
    else
        strcat(sampleURL, "ac1/");
    strcat(sampleURL, "slave/");
    strcat(sampleURL, config.serialNumber);
    // memset(sampleURL, '\0', sizeof(sampleURL));
    // memcpy(sampleURL, "http://34.100.138.28/idf_release/slave_v14.bin", strlen("http://34.100.138.28/idf_release/slave_v14.bin"));

    ESP_LOGI(TAG, "Slave OTA URL: %s", sampleURL);

    esp_http_client_config_t clientDownloadConfig = {
        .url = sampleURL,
        .cert_pem = NULL,
        .event_handler = client_event_handler,
        .timeout_ms = 10000,
        .buffer_size = 4096,
        .keep_alive_enable = true,
        .skip_cert_common_name_check = true};
    esp_http_client_handle_t clientDownload = esp_http_client_init(&clientDownloadConfig);
    if (clientDownload == NULL)
    {
        ESP_LOGE(TAG_SLAVE, "Failed to initialise HTTP connection");
        return err;
    }
    err = esp_http_client_open(clientDownload, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_SLAVE, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(clientDownload);
        return err;
    }

    esp_http_client_fetch_headers(clientDownload);
    int32_t file_length = esp_http_client_get_content_length(clientDownload);
    ESP_LOGE(TAG, "File length: %ld", file_length);

    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(TAG_SLAVE, "Starting SLAVE OTA");

    update_partition = esp_ota_get_next_update_partition(NULL);
    assert(update_partition != NULL);
    ESP_LOGI(TAG_SLAVE, "Writing to partition subtype %d at offset 0x%" PRIx32,
             update_partition->subtype, update_partition->address);
    uint32_t total_data_read = 0;
    bool otaStarted = false;
    uint32_t binary_file_length = 0;
    uint32_t data_read = 0;
    uint32_t read_bytes = DownloadBuffSize;
    uint8_t buffer[read_bytes + 1];
    uint32_t bytesSentToSlave = 0;
    uint32_t bytesTosend = 0;
    bool printData = true;
    slaveClientError = false;
    uint8_t waitCount = 0;
    memset(buffer, 0, sizeof(buffer));
    /*deal with all receive packet*/
    uint32_t count = 0;
    bool image_header_was_checked = false;

    int32_t TotalImageSize = 0;
    int32_t DownloadedImageSize = 0;
    slaveOtaDownloadPercentage = 0.0;

    TotalImageSize = file_length;

    if (SlaveSendID(FIRMWARE_UPDATE_START_ID))
    {
        otaStarted = true;
        FirmwareUpdateStarted = true;
        SetChargerFirmwareUpdateBit();
        ledState[1] = OTA_LED_STATE;
        ledState[2] = OTA_LED_STATE;
        ledState[3] = OTA_LED_STATE;
    }
    else
    {
        otaStarted = false;
        return ESP_FAIL;
    }

    do
    {
        memset(buffer, '0', sizeof(buffer));
        data_read = 0;
        data_read = esp_http_client_read(clientDownload, (char *)buffer, DownloadBuffSize);
        if (slaveClientError == true)
        {
            data_read = 0;
            SlaveSendID(FIRMWARE_UPDATE_END_ID);
            break;
        }
        if (data_read > 0)
        {
            SlaveOTA.UpdateStarted = true;
            bytesSentToSlave = 0;
            // ESP_LOGI(TAG, "Data Read : %ld", data_read);
            while (bytesSentToSlave < data_read)
            {
                bytesTosend = (uint32_t)(((data_read - bytesSentToSlave) > 200) ? 200 : (data_read - bytesSentToSlave));
                while (sendFirmwareData(&buffer[bytesSentToSlave], bytesTosend, count) == false)
                {
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                }
                // vTaskDelay(200 / portTICK_PERIOD_MS);
                if (count <= 2)
                {
                    vTaskDelay(2000 / portTICK_PERIOD_MS);
                }
                count++;
                if (printData)
                {
                    ESP_LOGI(TAG, "Buffer Start\t: %hhu", buffer[bytesSentToSlave]);
                    ESP_LOGI(TAG, "Buffer End\t: %hhu", buffer[bytesSentToSlave + bytesTosend - 1]);
                    ESP_LOGI(TAG, "Buffer length\t: %ld", bytesTosend);
                }
                waitCount = 0;

                bytesSentToSlave = bytesSentToSlave + bytesTosend;
            }
            binary_file_length += data_read;
            DownloadedImageSize = binary_file_length;
            slaveOtaDownloadPercentage = (DownloadedImageSize * 100.0) / TotalImageSize;
            if (updatingMasterAndSlave)
            {
                firmwareUpdateProgress = slaveOtaDownloadPercentage / 2.0;
            }
            else
            {
                firmwareUpdateProgress = slaveOtaDownloadPercentage;
            }
            SetChargerFirmwareUpdateBit();
            if (count % 100 == 0)
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
            if (count % 100 == 0)
            {
                ESP_LOGI(TAG_SLAVE, "Downloading Data \t: %ld", count);
                ESP_LOGI(TAG_SLAVE, "binary data length\t: %ld", binary_file_length);
            }
            printData = false;
            ESP_LOGD(TAG_SLAVE, "Written image length %ld", binary_file_length);
        }
        if (binary_file_length >= file_length)
        {
            data_read = 0;
            SlaveSendID(FIRMWARE_UPDATE_END_ID);
            break;
        }

        if (slaveClientError == true)
        {
            // esp_http_client_close(clientDownload);
            // esp_http_client_open(clientDownload, 0);
        }

    } while (data_read > 0);
    ESP_LOGI(TAG_SLAVE, "Total Write binary data length: %ld", binary_file_length);

    if (SlaveSendID(FIRMWARE_UPDATE_COMPLETED_ID) == false)
    {
        SlaveOTA.InstallationFail = true;
    }

    SlaveOTA.UpdateSuccess = true;
    http_cleanup(clientDownload);

    ESP_LOGI(TAG_SLAVE, "SLAVE OTA Update Completed!");
    return err;
}

esp_err_t PerformMasterOta(void)
{
    esp_err_t err = ESP_OK;
    ESP_LOGI(TAG, "Starting MASTER OTA");

    char sampleURL[100];
    memset(sampleURL, '\0', sizeof(sampleURL));
    strcat(sampleURL, config.otaURLConfig);
    if (config.vcharge_lite_1_4)
        strcat(sampleURL, "v14/");
    else if (config.vcharge_v5)
        strcat(sampleURL, "v5/");
    else if (config.AC1)
        strcat(sampleURL, "ac1/");
    else if (config.AC2)
        strcat(sampleURL, "ac2/");
    else
        strcat(sampleURL, "ac1/");
    strcat(sampleURL, "master/");
    strcat(sampleURL, config.serialNumber);
    ESP_LOGI(TAG, "Master OTA URL: %s", sampleURL);

    esp_http_client_config_t clientConfig = {
        .url = sampleURL,
        .event_handler = client_event_handler,
        .cert_pem = NULL,
        .timeout_ms = 10000,
        .skip_cert_common_name_check = true};

    esp_https_ota_config_t ota_config = {
        .http_config = &clientConfig};

    esp_https_ota_handle_t ota_handle = NULL;
    int32_t TotalImageSize = 0;
    int32_t DownloadedImageSize = 0;
    masterOtaDownloadPercentage = 0.0;
    if (esp_https_ota_begin(&ota_config, &ota_handle) != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_https_ota_begin failed");
        setNULL(CPFirmwareStatusNotificationRequest.status);
        memcpy(CPFirmwareStatusNotificationRequest.status, DownloadFailed, strlen(DownloadFailed));
        MasterOTA.DownloadFail = true;
        if (FirmwareUpdatingForOfflineCharger == false)
            sendFirmwareStatusNotificationRequest();
        return ESP_FAIL;
    }
    else
    {
        FirmwareUpdateStarted = true;
        SetChargerFirmwareUpdateBit();
        ledState[1] = OTA_LED_STATE;
        ledState[2] = OTA_LED_STATE;
        ledState[3] = OTA_LED_STATE;
        setNULL(CPFirmwareStatusNotificationRequest.status);
        memcpy(CPFirmwareStatusNotificationRequest.status, Downloading, strlen(Downloading));
        MasterOTA.UpdateStarted = true;
        if (FirmwareUpdatingForOfflineCharger == false)
            sendFirmwareStatusNotificationRequest();
        TotalImageSize = esp_https_ota_get_image_size(ota_handle);
        while (true)
        {
            esp_err_t ota_result = esp_https_ota_perform(ota_handle);
            if (ota_result != ESP_ERR_HTTPS_OTA_IN_PROGRESS)
                break;
            DownloadedImageSize = esp_https_ota_get_image_len_read(ota_handle);
            masterOtaDownloadPercentage = (DownloadedImageSize * 100.0) / TotalImageSize;
            if (updatingMasterAndSlave)
            {
                firmwareUpdateProgress = 50.0 + masterOtaDownloadPercentage / 2.0;
            }
            else
            {
                firmwareUpdateProgress = masterOtaDownloadPercentage;
            }
            SetChargerFirmwareUpdateBit();
        }
        setNULL(CPFirmwareStatusNotificationRequest.status);
        memcpy(CPFirmwareStatusNotificationRequest.status, Downloaded, strlen(Downloaded));
        if (FirmwareUpdatingForOfflineCharger == false)
            sendFirmwareStatusNotificationRequest();
        if (esp_https_ota_finish(ota_handle) != ESP_OK)
        {
            setNULL(CPFirmwareStatusNotificationRequest.status);
            memcpy(CPFirmwareStatusNotificationRequest.status, InstallationFailed, strlen(InstallationFailed));
            MasterOTA.InstallationFail = true;
            if (FirmwareUpdatingForOfflineCharger == false)
                sendFirmwareStatusNotificationRequest();
            ESP_LOGE(TAG, "esp_https_ota_finish failed");
        }
        else
        {
            setNULL(CPFirmwareStatusNotificationRequest.status);
            memcpy(CPFirmwareStatusNotificationRequest.status, Installing, strlen(Installing));
            MasterOTA.UpdateSuccess = true;
            if (FirmwareUpdatingForOfflineCharger == false)
                sendFirmwareStatusNotificationRequest();
            vTaskDelay(pdMS_TO_TICKS(1000));

            setNULL(CPFirmwareStatusNotificationRequest.status);
            memcpy(CPFirmwareStatusNotificationRequest.status, Installed, strlen(Installed));
            if (FirmwareUpdatingForOfflineCharger == false)
                sendFirmwareStatusNotificationRequest();
            vTaskDelay(pdMS_TO_TICKS(1000));

            setNULL(CPFirmwareStatusNotificationRequest.status);
            memcpy(CPFirmwareStatusNotificationRequest.status, Idle, strlen(Idle));
            if (FirmwareUpdatingForOfflineCharger == false)
                sendFirmwareStatusNotificationRequest();
        }
    }

    return err;
}

OtaStatus_t CheckOtaRequired(uint8_t device)
{
    // http://34.100.138.28:8080/ota/ac1/master/serialNumber
    // http://34.100.138.28:8080/ota/ac2/master/serialNumber
    // http://34.100.138.28:8080/ota/v14/master/serialNumber
    // http://34.100.138.28:8080/ota/v5/master/serialNumber

    OtaStatus_t Status = {false, false, false, false, false, false};
    ;
    char sampleURL[100];
    memset(sampleURL, '\0', sizeof(sampleURL));
    strcat(sampleURL, config.otaURLConfig);
    if (config.vcharge_lite_1_4)
        strcat(sampleURL, "v14/");
    else if (config.vcharge_v5)
        strcat(sampleURL, "v5/");
    else if (config.AC1)
        strcat(sampleURL, "ac1/");
    else if (config.AC2)
        strcat(sampleURL, "ac2/");
    else
        strcat(sampleURL, "ac1/");

    if (device == MASTER)
        strcat(sampleURL, "master/");
    else
        strcat(sampleURL, "slave/");

    strcat(sampleURL, config.serialNumber);
    if (device == MASTER)
        ESP_LOGI(TAG, "Master OTA URL: %s", sampleURL);
    else
        ESP_LOGI(TAG, "Slave OTA URL: %s", sampleURL);

    esp_http_client_config_t clientDownloadConfig = {
        .url = sampleURL,
        .cert_pem = NULL,
        .timeout_ms = 10000,
        .keep_alive_enable = true,
        .skip_cert_common_name_check = true};
    esp_http_client_handle_t clientDownload = esp_http_client_init(&clientDownloadConfig);
    if (clientDownload == NULL)
    {
        ESP_LOGE(TAG_SLAVE, "Failed to initialise HTTP connection");
        return Status;
    }
    esp_err_t err = esp_http_client_open(clientDownload, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_SLAVE, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(clientDownload);
        return Status;
    }

    esp_http_client_fetch_headers(clientDownload);

    uint32_t DownloadBuffSize = 1000;
    uint8_t buffer[DownloadBuffSize + 1];
    memset(buffer, 0, sizeof(buffer));
    if (esp_http_client_read(clientDownload, (char *)buffer, DownloadBuffSize) <
        (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)))
    {
        setNULL(CPFirmwareStatusNotificationRequest.status);
        memcpy(CPFirmwareStatusNotificationRequest.status, DownloadFailed, strlen(DownloadFailed));
        if (FirmwareUpdatingForOfflineCharger == false)
            sendFirmwareStatusNotificationRequest();
        ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed");
        http_cleanup(clientDownload);
        Status.GetImageDesc = false;
    }
    else
    {
        Status.GetImageDesc = true;
        esp_app_desc_t new_app_info;
        memcpy(&new_app_info, &buffer[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));

        const esp_partition_t *running_partition = esp_ota_get_running_partition();
        esp_app_desc_t running_partition_description;
        if (device == SLAVE)
        {
            uint32_t SlaveVersionRequestCount = 0;
            Status.GetImageDesc = false;
            setNULL(running_partition_description.version);
            while ((Status.GetImageDesc == false) && (SlaveVersionRequestCount < 5))
            {
                if (SlaveSendID(FIRMWARE_VERSION_ID))
                {
                    Status.GetImageDesc = true;
                    setNULL(running_partition_description.version);
                    memcpy(running_partition_description.version, config.slavefirmwareVersion, strlen(config.slavefirmwareVersion));
                }
                else
                {
                    Status.GetImageDesc = false;
                    // if (SlaveVersionRequestCount == 0)
                    // {
                    //     SlaveSendID(RESTART_ID);
                    //     vTaskDelay(pdMS_TO_TICKS(2000));
                    // }
                    ESP_LOGI(TAG_SLAVE, "Firmware Version Received failed from slave");
                }
                SlaveVersionRequestCount++;
            }
        }
        else
        {
            esp_ota_get_partition_description(running_partition, &running_partition_description);
        }

        ESP_LOGI(TAG, "current version is %s\n", running_partition_description.version);
        ESP_LOGI(TAG, "new version is %s\n", new_app_info.version);
        size_t length = (device == MASTER) ? strlen(new_app_info.version) : 5;
        if (device == SLAVE)
        {
            ESP_LOGI(TAG, "String length of running_partition_description.version is %d", strlen(running_partition_description.version));
            ESP_LOGI(TAG, "String length of new_app_info.version is %d", strlen(new_app_info.version));
        }
        if (strncmp(running_partition_description.version, new_app_info.version, strlen(running_partition_description.version)) == 0)
        {
            Status.UpdateRequired = false;
        }
        else
        {
            Status.UpdateRequired = true;
        }
    }
    http_cleanup(clientDownload);
    return Status;
}

void OtaTask(void *params)
{
    MasterOTA.GetImageDesc = true;
    MasterOTA.UpdateRequired = false;
    MasterOTA.UpdateStarted = false;
    MasterOTA.DownloadFail = false;
    MasterOTA.InstallationFail = false;
    MasterOTA.UpdateSuccess = false;

    SlaveOTA.GetImageDesc = true;
    SlaveOTA.UpdateRequired = false;
    SlaveOTA.UpdateStarted = false;
    SlaveOTA.DownloadFail = false;
    SlaveOTA.InstallationFail = false;
    SlaveOTA.UpdateSuccess = false;
    firmwareUpdateProgress = 0.0;
    masterOtaDownloadPercentage = 0.0;
    slaveOtaDownloadPercentage = 0.0;
    int retries = 2;
    int SlaveRetries = 2;
    int retryInterval = 60;
    int SlaveRetryInterval = 60;
    if ((CMSUpdateFirmwareRequest.retries < 0) ||
        (CMSUpdateFirmwareRequest.retries > 5))
    {
        CMSUpdateFirmwareRequest.retries = 5;
    }
    if (CMSUpdateFirmwareRequest.retries > 0)
    {
        retries = CMSUpdateFirmwareRequest.retries;
        SlaveRetries = CMSUpdateFirmwareRequest.retries;
    }
    if ((CMSUpdateFirmwareRequest.retryInterval < 0) ||
        (CMSUpdateFirmwareRequest.retryInterval > 5))
    {
        CMSUpdateFirmwareRequest.retryInterval = 5;
    }
    if (CMSUpdateFirmwareRequest.retryInterval > 0)
    {
        retryInterval = CMSUpdateFirmwareRequest.retryInterval;
        SlaveRetryInterval = CMSUpdateFirmwareRequest.retryInterval;
    }
    bool Slave = false;
    if (config.vcharge_lite_1_4)
        Slave = true;
    do
    {
        if (MasterOTA.GetImageDesc == false)
        {
            vTaskDelay(pdMS_TO_TICKS(retryInterval * 1000));
            retries = retries - 1;
        }
        MasterOTA = CheckOtaRequired(MASTER);
    } while ((MasterOTA.GetImageDesc == false) && (retries > 0));

    if (Slave)
    {
        do
        {
            if (SlaveOTA.GetImageDesc == false)
            {
                vTaskDelay(pdMS_TO_TICKS(SlaveRetryInterval * 1000));
                SlaveRetries = SlaveRetries - 1;
            }
            SlaveOTA = CheckOtaRequired(SLAVE);
        } while ((SlaveOTA.GetImageDesc == false) && (SlaveRetries > 0));
    }

    if (CMSUpdateFirmwareRequest.retries > 0)
    {
        retries = CMSUpdateFirmwareRequest.retries;
        SlaveRetries = CMSUpdateFirmwareRequest.retries;
    }
    if (CMSUpdateFirmwareRequest.retryInterval > 0)
    {
        retryInterval = CMSUpdateFirmwareRequest.retryInterval;
        SlaveRetryInterval = CMSUpdateFirmwareRequest.retryInterval;
    }

    if ((MasterOTA.GetImageDesc && MasterOTA.UpdateRequired) ||
        ((Slave == false) || (Slave && SlaveOTA.GetImageDesc && SlaveOTA.UpdateRequired)))
    {
        FirmwareUpdateStarted = true;
        if ((MasterOTA.GetImageDesc && MasterOTA.UpdateRequired) &&
            (Slave && SlaveOTA.GetImageDesc && SlaveOTA.UpdateRequired))
            updatingMasterAndSlave = true;
        if (Slave && SlaveOTA.GetImageDesc && SlaveOTA.UpdateRequired)
        {
            do
            {
                if (CMSUpdateFirmwareRequest.retries > SlaveRetries)
                {
                    ESP_LOGE(TAG, "OTA Failed");
                    ESP_LOGI(TAG, "Restrying OTA After %d seconds", CMSUpdateFirmwareRequest.retryInterval);
                    vTaskDelay(pdMS_TO_TICKS(SlaveRetryInterval * 1000));
                    ESP_LOGI(TAG, "Restrying OTA for %d time", CMSUpdateFirmwareRequest.retries - SlaveRetries);
                }
                PerformSlaveOta();
                slaveFirmwareStatusReceived = false;
                while (slaveFirmwareStatusReceived == false)
                {
                    SlaveSendID(FIRMWARE_STATUS_ID);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                if (slave_uart.FIRMWARE_UPDATE_SUCCEESS_ID_RECEIVED)
                {
                    SlaveOTA.UpdateSuccess = true;
                }
                else if (slave_uart.FIRMWARE_UPDATE_FAIL_ID_RECEIVED)
                {
                    SlaveOTA.UpdateSuccess = false;
                }
                else
                {
                    SlaveOTA.UpdateSuccess = false;
                }

                SlaveRetries = SlaveRetries - 1;
            } while ((SlaveOTA.UpdateSuccess == false) && (SlaveRetries > 0));
        }
        if ((Slave && SlaveOTA.GetImageDesc && SlaveOTA.UpdateRequired && SlaveOTA.UpdateSuccess) ||
            (Slave && SlaveOTA.GetImageDesc && (SlaveOTA.UpdateRequired == false)) ||
            (Slave == false))
        {
            if (MasterOTA.GetImageDesc && MasterOTA.UpdateRequired)
            {
                do
                {
                    if (CMSUpdateFirmwareRequest.retries > retries)
                    {
                        ESP_LOGE(TAG, "OTA Failed");
                        ESP_LOGI(TAG, "Restrying OTA After %d seconds", CMSUpdateFirmwareRequest.retryInterval);
                        vTaskDelay(pdMS_TO_TICKS(retryInterval * 1000));
                        ESP_LOGI(TAG, "Restrying OTA for %d time", CMSUpdateFirmwareRequest.retries - retries);
                    }
                    PerformMasterOta();
                    retries = retries - 1;
                } while ((MasterOTA.UpdateSuccess == false) && (retries > 0));
            }
        }
        updatingMasterAndSlave = false;
        if ((Slave && SlaveOTA.UpdateSuccess) || (MasterOTA.UpdateSuccess))
        {
            ESP_LOGI(TAG, "Restarting in 5 seconds");
            vTaskDelay(pdMS_TO_TICKS(5000));
            esp_restart();
        }
        else
        {
            FirmwareUpdateFailed = true;
            ESP_LOGE(TAG, "Firmware Update Failed");
        }
    }
    else if ((MasterOTA.GetImageDesc && (MasterOTA.UpdateRequired == false)) &&
             ((Slave == false) || (Slave && SlaveOTA.GetImageDesc && (SlaveOTA.UpdateRequired == false))))
    {
        ESP_LOGI(TAG, "Firmware Already Updated with Latest Version");
        setNULL(CPFirmwareStatusNotificationRequest.status);
        memcpy(CPFirmwareStatusNotificationRequest.status, Installed, strlen(Installed));
        if (FirmwareUpdatingForOfflineCharger == false)
            sendFirmwareStatusNotificationRequest();
        vTaskDelay(pdMS_TO_TICKS(1000));

        setNULL(CPFirmwareStatusNotificationRequest.status);
        memcpy(CPFirmwareStatusNotificationRequest.status, Idle, strlen(Idle));
        if (FirmwareUpdatingForOfflineCharger == false)
            sendFirmwareStatusNotificationRequest();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    else
    {
        setNULL(CPFirmwareStatusNotificationRequest.status);
        memcpy(CPFirmwareStatusNotificationRequest.status, DownloadFailed, strlen(DownloadFailed));
        if (FirmwareUpdatingForOfflineCharger == false)
            sendFirmwareStatusNotificationRequest();
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGE(TAG, "OTA FAILED");
        FirmwareUpdateStarted = false;
    }

    vTaskDelete(NULL);
}

void UpdateFirmware(void)
{
    xTaskCreate(&OtaTask, "OtaTask", 2048 * 5, NULL, 5, NULL);
}