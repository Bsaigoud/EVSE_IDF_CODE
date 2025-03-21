#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "ProjectSettings.h"
#include "custom_nvs.h"
#include "esp_spiffs.h"
#include "logsToSpiffs.h"
#include "cJSON.h"
#include "ocpp.h"
#include "ocpp_task.h"

#define TAG "LOGS_TO_SPIFFS"

#define MAX_LOG_FILES 8
#define MAX_LOG_FILE_SIZE (350 * 1024)
#define RING_BUFFER_SIZE (10 * 1024)
// char data[MAX_LOG_FILE_SIZE + 2000];
extern Config_t config;
extern CPGetDiagnosticsResponse_t CPGetDiagnosticsResponse;
extern CPDiagnosticsStatusNotificationRequest_t CPDiagnosticsStatusNotificationRequest;
static RingbufHandle_t ring_buffer = NULL;

SemaphoreHandle_t mutexLogs;

bool uploadingFirstLogFile = false;
uint32_t logFileNo = 0;
uint32_t currentFileSize = 0;
extern bool uploadingLogs;

static void http_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

void uploadLogs(void *params)
{
    bool uploadFailed = false;
    char logText[256];
    memset(logText, 0, sizeof(logText));
    sprintf(logText, " uploadLogs Started\n");
    printf(logText);

    uint32_t FileNo = logFileNo;
    char sampleURL[100];
    memset(sampleURL, '\0', sizeof(sampleURL));
    memcpy(sampleURL, "http://34.100.138.28/idf_release/logData.php", strlen("http://34.100.138.28/idf_release/logData.php"));

    esp_http_client_config_t clientLogUploadConfig = {
        .url = sampleURL,
        .cert_pem = NULL,
        .timeout_ms = 5000,
        .keep_alive_enable = true,
        .skip_cert_common_name_check = true};
    esp_http_client_handle_t clientLogUpload = esp_http_client_init(&clientLogUploadConfig);
    if (clientLogUpload == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialise HTTP connection");
        setNULL(CPDiagnosticsStatusNotificationRequest.status);
        memcpy(CPDiagnosticsStatusNotificationRequest.status, UploadFailed, strlen(UploadFailed));
        sendDiagnosticsStatusNotificationRequest();
        vTaskDelete(NULL);
    }

    esp_http_client_set_method(clientLogUpload, HTTP_METHOD_POST);
    esp_err_t err = esp_http_client_set_header(clientLogUpload, "Content-Type", "application/json");
    if (err != ESP_OK)
    {
        printf("Error setting header: (%s)", esp_err_to_name(err));
        // Handle error as needed
    }
    uploadingLogs = true;
    vTaskDelay(pdMS_TO_TICKS(100));

    char fileName[50];
    char data[3100];
    uint32_t fileSize = 0;
    setNULL(CPDiagnosticsStatusNotificationRequest.status);
    memcpy(CPDiagnosticsStatusNotificationRequest.status, Uploading, strlen(Uploading));
    sendDiagnosticsStatusNotificationRequest();
    uploadingFirstLogFile = true;
    for (uint32_t i = (FileNo + 1); i < MAX_LOG_FILES; i++)
    {
        if (uploadFailed == false)
        {
            memset(data, '\0', sizeof(data));
            memset(fileName, '\0', sizeof(fileName));
            sprintf(fileName, "/logs/logs%ld.txt", i);

            FILE *file = fopen(fileName, "r");
            if (file == NULL)
            {
                char line[1024];
                memset(line, 0, sizeof(line));
                sprintf(line, "File: %s not found\n", fileName);
                printf(line);
            }
            else
            {
                fseek(file, 0, SEEK_END);
                fileSize = ftell(file);
                fseek(file, 0, SEEK_SET);
                char line[1024];
                memset(line, 0, sizeof(line));
                sprintf(line, "File: %s found, Size = %ld\n", fileName, fileSize);
                printf(line);
                memset(data, '\0', sizeof(data));
                // while ((fread(data, 1, 1000, file) > 0) && (uploadFailed == false))
                while ((fscanf(file, "%3000c", data) > 0) && (uploadFailed == false))
                {
                    for (uint32_t len = 0; len < strlen(data); len++)
                    {
                        if (data[len] == '\"')
                            data[len] = '\'';
                    }
                    cJSON *object = cJSON_CreateObject();

                    cJSON_AddStringToObject(object, "fileName", CPGetDiagnosticsResponse.fileName);
                    cJSON_AddStringToObject(object, "data", data);

                    char *json_string = cJSON_Print(object);
                    cJSON_Delete(object);
                    char json_output[strlen(json_string) + 1];
                    memset(json_output, '\0', strlen(json_string) + 1);
                    memcpy(json_output, json_string, strlen(json_string));
                    if (json_string != NULL)
                    {
                        free(json_string); // Free the allocated memory for the JSON string
                    }

                    esp_http_client_set_post_field(clientLogUpload, json_output, strlen(json_output));
                    err = esp_http_client_perform(clientLogUpload);
                    if (err != ESP_OK)
                    {
                        uint8_t retryCount = 0;
                        while ((err != ESP_OK) && (retryCount < 10))
                        {
                            vTaskDelay(100 / portTICK_PERIOD_MS);
                            err = esp_http_client_perform(clientLogUpload);
                            if (err != ESP_OK)
                                ESP_LOGE(TAG, "Error: SSL data write error (%s), retryCount = %hhu", esp_err_to_name(err), retryCount);
                            retryCount++;
                        }
                        if (err != ESP_OK)
                        {
                            uploadFailed = true;
                            ESP_LOGE(TAG, "Error: SSL data write error (%s), Upload Failed", esp_err_to_name(err));
                        }
                    }
                    memset(data, '\0', sizeof(data));
                    // vTaskDelay(100 / portTICK_PERIOD_MS);
                }
                memset(logText, 0, sizeof(logText));
                sprintf(logText, "\n%s uploadLogs completed\n", fileName);
                printf(logText);
                memset(data, '\0', sizeof(data));
                fclose(file);
            }
        }
        uploadingFirstLogFile = false;

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    for (uint32_t i = 0; i <= FileNo; i++)
    {
        if (uploadFailed == false)
        {
            memset(data, '\0', sizeof(data));
            memset(fileName, '\0', sizeof(fileName));
            sprintf(fileName, "/logs/logs%ld.txt", i);

            FILE *file = fopen(fileName, "r");
            if (file == NULL)
            {
                char line[1024];
                memset(line, 0, sizeof(line));
                sprintf(line, "File: %s not found\n", fileName);
                printf(line);
            }
            else
            {
                fseek(file, 0, SEEK_END);
                fileSize = ftell(file);
                fseek(file, 0, SEEK_SET);
                char line[1024];
                memset(line, 0, sizeof(line));
                sprintf(line, "File: %s found, Size = %ld\n", fileName, fileSize);
                printf(line);
                memset(data, '\0', sizeof(data));
                // while ((fread(data, 1, 1000, file) > 0) && (uploadFailed == false))
                while ((fscanf(file, "%3000c", data) > 0) && (uploadFailed == false))
                {
                    for (uint32_t len = 0; len < strlen(data); len++)
                    {
                        if (data[len] == '\"')
                            data[len] = '\'';
                    }
                    cJSON *object = cJSON_CreateObject();

                    cJSON_AddStringToObject(object, "fileName", CPGetDiagnosticsResponse.fileName);
                    cJSON_AddStringToObject(object, "data", data);

                    char *json_string = cJSON_Print(object);
                    cJSON_Delete(object);
                    char json_output[strlen(json_string) + 1];
                    memset(json_output, '\0', strlen(json_string) + 1);
                    memcpy(json_output, json_string, strlen(json_string));
                    if (json_string != NULL)
                    {
                        free(json_string); // Free the allocated memory for the JSON string
                    }

                    esp_http_client_set_post_field(clientLogUpload, json_output, strlen(json_output));
                    err = esp_http_client_perform(clientLogUpload);
                    if (err != ESP_OK)
                    {
                        uint8_t retryCount = 0;
                        while ((err != ESP_OK) && (retryCount < 10))
                        {
                            vTaskDelay(100 / portTICK_PERIOD_MS);
                            err = esp_http_client_perform(clientLogUpload);
                            if (err != ESP_OK)
                                ESP_LOGE(TAG, "Error: SSL data write error (%s), retryCount = %hhu", esp_err_to_name(err), retryCount);
                            retryCount++;
                        }
                        if (err != ESP_OK)
                        {
                            uploadFailed = true;
                            ESP_LOGE(TAG, "Error: SSL data write error (%s), Upload Failed", esp_err_to_name(err));
                        }
                    }
                    memset(data, '\0', sizeof(data));
                    // vTaskDelay(100 / portTICK_PERIOD_MS);
                }
                memset(logText, 0, sizeof(logText));
                sprintf(logText, "\n%s uploadLogs completed\n", fileName);
                printf(logText);
                memset(data, '\0', sizeof(data));
                fclose(file);
            }
        }
        uploadingFirstLogFile = false;

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    http_cleanup(clientLogUpload);

    if (uploadFailed)
    {
        setNULL(CPDiagnosticsStatusNotificationRequest.status);
        memcpy(CPDiagnosticsStatusNotificationRequest.status, UploadFailed, strlen(UploadFailed));
        sendDiagnosticsStatusNotificationRequest();
        printf("Upload Logs Failed\n");
    }
    else
    {
        setNULL(CPDiagnosticsStatusNotificationRequest.status);
        memcpy(CPDiagnosticsStatusNotificationRequest.status, Uploaded, strlen(Uploaded));
        sendDiagnosticsStatusNotificationRequest();
        printf("Upload Logs Completed\n");
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    setNULL(CPDiagnosticsStatusNotificationRequest.status);
    memcpy(CPDiagnosticsStatusNotificationRequest.status, Idle, strlen(Idle));
    sendDiagnosticsStatusNotificationRequest();
    uploadingLogs = false;
    vTaskDelete(NULL);
}

static int ringbuffer_vprintf(const char *str, va_list args)
{
    static char buffer[2000];
    int len = vsnprintf(buffer, sizeof(buffer), str, args);
    if (len > 0)
    {
        // Write the log message to the ring buffer
        if (xRingbufferSend(ring_buffer, buffer, len, pdMS_TO_TICKS(100)) != pdTRUE)
        {
            // Handle ring buffer overflow here (optional)
        }
    }
    return len;
}

void log_printer_task(void *args)
{
    char fileName[50];
    memset(fileName, '\0', sizeof(fileName));
    sprintf(fileName, "/logs/logs%ld.txt", logFileNo);

    FILE *file = fopen(fileName, "a");
    if (file != NULL)
    {
        char line[256];
        memset(line, 0, sizeof(line));
        sprintf(line, "File: %s Open Successful\n", fileName);
        printf(line);
    }
    else
    {
        char line[256];
        memset(line, 0, sizeof(line));
        sprintf(line, "File: %s Open Failed\n", fileName);
        printf(line);
        if (remove(fileName) == 0)
        {
            memset(line, 0, sizeof(line));
            sprintf(line, "File: %s deleted successfully\n", fileName);
            printf(line);
        }
        else
        {
            memset(line, 0, sizeof(line));
            sprintf(line, "File: %s deletion failed\n", fileName);
            printf(line);
        }
        file = fopen(fileName, "at");
        if (file != NULL)
        {
            memset(line, 0, sizeof(line));
            sprintf(line, "File: %s Open Successful in 2nd attempt\n", fileName);
            printf(line);
        }
        else
        {
            memset(line, 0, sizeof(line));
            sprintf(line, "File: %s Open Failed after 2nd attempt\n", fileName);
            printf(line);
        }
    }
    while (1)
    {
        size_t item_size;

        char *item = (char *)xRingbufferReceive(ring_buffer, &item_size, pdMS_TO_TICKS(1000));
        if (item != NULL)
        {

            if (file != NULL)
            {
                fwrite(item, 1, item_size, file);
                // fprintf(file, "%.*s", item_size, item);
                fflush(file);
                fseek(file, 0, SEEK_END);
                currentFileSize = ftell(file);
            }

            // Print the log message
            printf("%.*s", item_size, item);
            // Return the item to the ring buffer
            vRingbufferReturnItem(ring_buffer, (void *)item);

            if ((currentFileSize >= (MAX_LOG_FILE_SIZE - 200)) && (uploadingFirstLogFile == false))
            {
                if (file != NULL)
                {
                    fclose(file);
                    file = NULL;
                }
                logFileNo++;
                if (logFileNo >= MAX_LOG_FILES)
                {
                    logFileNo = 0;
                }
                currentFileSize = 0;

                memset(fileName, '\0', sizeof(fileName));
                sprintf(fileName, "/logs/logs%ld.txt", logFileNo);

                if (remove(fileName) == 0)
                {
                    char line[256];
                    memset(line, 0, sizeof(line));
                    sprintf(line, "%s File deleted successfully\n", fileName);
                    printf(line);
                }
                else
                {
                    char line[256];
                    memset(line, 0, sizeof(line));
                    sprintf(line, "%s File deletion failed\n", fileName);
                    printf(line);
                }
                if (file == NULL)
                    file = fopen(fileName, "a");

                custom_nvs_write_logFileNo();
            }
        }
    }

    vTaskDelete(NULL);
}

void sendLogsToCloud(void)
{
#if TEST
    if (config.redirectLogs == false)
    {
        mutexLogs = xSemaphoreCreateMutex();
        custom_nvs_read_logFileNo();
        esp_vfs_spiffs_conf_t config_spiffs = {
            .base_path = "/logs",
            .partition_label = NULL,
            .max_files = MAX_LOG_FILES,
            .format_if_mount_failed = true,
        };
        esp_vfs_spiffs_register(&config_spiffs);
        setNULL(CPGetDiagnosticsResponse.fileName);
        memcpy(CPGetDiagnosticsResponse.fileName, "logs.txt", strlen("logs.txt"));
    }
#endif
    xTaskCreate(&uploadLogs, "uploadLogs", 12 * 1024, NULL, 5, NULL);
}

void log_spiffs_init(void)
{
    ring_buffer = xRingbufferCreate(RING_BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (ring_buffer == NULL)
    {
        ESP_LOGE("Main", "Failed to create ring buffer");
        return;
    }
    mutexLogs = xSemaphoreCreateMutex();
    custom_nvs_read_logFileNo();
    esp_vfs_spiffs_conf_t config_spiffs = {
        .base_path = "/logs",
        .partition_label = NULL,
        .max_files = MAX_LOG_FILES,
        .format_if_mount_failed = true,
    };
    esp_err_t ret = esp_vfs_spiffs_register(&config_spiffs);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize spiffs (%s)", esp_err_to_name(ret));
        ret = esp_vfs_spiffs_register(&config_spiffs);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize spiffs second time(%s)", esp_err_to_name(ret));
        }
    }
    else
    {
        ESP_LOGI(TAG, "Spiffs initialized");
    }

    esp_log_set_vprintf(ringbuffer_vprintf);
    xTaskCreate(log_printer_task, "LogPrinterTask", 3072, NULL, 1, NULL);
}