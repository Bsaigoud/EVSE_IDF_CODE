#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/queue.h>
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_websocket_client.h"
#include "websocket_connect.h"
#include "ocpp.h"
#include "slave.h"
#include "ProjectSettings.h"
#include "custom_nvs.h"

static char *TAG = "WEBSOCKET";
extern Config_t config;

extern QueueHandle_t OcppMsgQueue;
extern bool isWebsocketConnected;
extern bool isWebsocketBooted;
extern CPGetConfigurationResponse_t CPGetConfigurationResponse;
extern bool websocketConnectStarted;

struct esp_websocket_client *client;
char receivedMessage[OCPP_MSG_SIZE];
bool taskInitialized = false;
bool isWebsocketAppStarted = false;
TaskHandle_t Reconnect_taskHandle = NULL;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

void WebSocketTask(void *params)
{
    uint32_t loopCount = 0;
    while (true)
    {
        if (websocketConnectStarted == false)
        {
            if (esp_websocket_client_is_connected(client) && loopCount > 10)
            {
                setNULL(receivedMessage);
                if (xQueueReceive(OcppMsgQueue, &receivedMessage, portMAX_DELAY))
                {
                    // Process the received string
                    // printf("Received: %s\n", receivedMessage);
                    int len = strlen(receivedMessage);
                    ESP_LOGI(TAG, "Sending %s", receivedMessage);
                    ESP_LOGD(TAG, "length %d", len);
                    esp_websocket_client_send_text(client, receivedMessage, len, portMAX_DELAY);
                }
                loopCount = 0;
            }
            loopCount++;
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        else
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    char receivedString[OCPP_MSG_SIZE];
    setNULL(receivedString);
    switch (event_id)
    {
    case WEBSOCKET_EVENT_CLOSED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CLOSED");
        isWebsocketConnected = false;
        isWebsocketAppStarted = false;
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);
        if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", data->error_handle.esp_transport_sock_errno);
        }
        isWebsocketConnected = false;
        isWebsocketAppStarted = false;
        break;
    case WEBSOCKET_EVENT_CONNECTED:
        if (isWebsocketBooted == false)
            custom_nvs_read_connectors_offline_data_count();
        isWebsocketConnected = true;
        isWebsocketAppStarted = false;

        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        break;
    case WEBSOCKET_EVENT_DATA:
        isWebsocketConnected = true;
        if (data->op_code == 0x08 && data->data_len == 2)
        {
            ESP_LOGW(TAG, "Received closed message with code=%d", 256 * data->data_ptr[0] + data->data_ptr[1]);
        }
        else if (data->op_code == 0x0A)
        {
            ESP_LOGI(TAG, "PONG Received");
        }
        else
        {
            memcpy(receivedString, (char *)data->data_ptr, data->data_len);
            ESP_LOGI(TAG, "Received= %s\n", receivedString);
            ocpp_response((const char *)receivedString);
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WEBSOCKET_EVENT_ERROR");
        log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);
        if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", data->error_handle.esp_transport_sock_errno);
        }
        break;
    case WEBSOCKET_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEFORE_CONNECT");

        break;
    default:
        ESP_LOGI(TAG, "OTHER EVENT");
        break;
    }
}

void websocket_app_start(uint8_t network)
{
    if (isWebsocketAppStarted == false)
    {
        esp_err_t ret;
        if (taskInitialized)
        {
            websocket_app_stop();
        }
        // Define the websocket connection
        esp_websocket_client_config_t websocket_cfg = {};

        websocket_cfg.uri = config.webSocketURL;
        websocket_cfg.task_prio = 6;
        websocket_cfg.task_stack = 6144;
        websocket_cfg.disable_auto_reconnect = true;
        websocket_cfg.transport = WEBSOCKET_TRANSPORT_OVER_TCP;
        if (network == 2)
            websocket_cfg.keep_alive_enable = false;
        else
            websocket_cfg.keep_alive_enable = true;
        websocket_cfg.keep_alive_idle = 10;
        websocket_cfg.keep_alive_interval = 10;
        websocket_cfg.keep_alive_count = 3;
        websocket_cfg.subprotocol = "ocpp1.6";
        websocket_cfg.ping_interval_sec = (size_t)CPGetConfigurationResponse.WebSocketPingInterval;
        websocket_cfg.reconnect_timeout_ms = 10;
        websocket_cfg.network_timeout_ms = 10000;
        websocket_cfg.pingpong_timeout_sec = 10;
        if (network == 2)
            websocket_cfg.disable_pingpong_discon = true;
        else
            websocket_cfg.disable_pingpong_discon = false;
        ESP_LOGI(TAG, "Connecting to %s ...", websocket_cfg.uri);

        // Connect to Websocket Server
        client = esp_websocket_client_init(&websocket_cfg);
        esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);

        isWebsocketAppStarted = true;
        ret = esp_websocket_client_start(client);
        if (ret != ESP_OK)
            isWebsocketAppStarted = false;
        if (taskInitialized == false)
            xTaskCreate(&WebSocketTask, "WebSocketTask", 4096, "task websocket", 10, NULL);
        taskInitialized = true;
    }
    while (isWebsocketAppStarted)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void setWebsocketPingInterval(void)
{
    esp_websocket_client_set_ping_interval_sec(client, (size_t)CPGetConfigurationResponse.WebSocketPingInterval);
}

void websocket_disconnect(void)
{
    esp_websocket_client_stop(client);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void reconnect_websocket(void *params)
{

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    if ((WebsocketConnectedisConnected() == false) && (taskInitialized == true))
    {
        websocket_app_stop();
        esp_websocket_client_config_t websocket_cfg = {};
        // websocket_cfg.uri = "ws://ocpp.ad.hiev.network/ocpp/TEST_EVRE";
        // websocket_cfg.uri = "ws://central.pulseenergy.io/ws/OCPP16J/CP0MV84D7N";
        // websocket_cfg.uri = "ws://ocpp.ad.hiev.network/ocpp/ADN_TEST";
        // websocket_cfg.uri = "ws://3.108.43.129:3002/ev/websocket/CentralSystemService/evre_test";
        websocket_cfg.uri = config.webSocketURL;
        // websocket_cfg.task_prio = 6;
        websocket_cfg.task_stack = 6144;
        websocket_cfg.disable_auto_reconnect = true;
        // websocket_cfg.transport = WEBSOCKET_TRANSPORT_OVER_TCP;
        websocket_cfg.keep_alive_enable = false;
        websocket_cfg.subprotocol = "ocpp1.6";
        websocket_cfg.ping_interval_sec = (size_t)CPGetConfigurationResponse.WebSocketPingInterval;
        // websocket_cfg.reconnect_timeout_ms = 5000;
        // websocket_cfg.network_timeout_ms = 5000;
        websocket_cfg.pingpong_timeout_sec = 5;
        ESP_LOGI(TAG, "Connecting to %s ...", websocket_cfg.uri);

        // Connect to Websocket Server
        client = esp_websocket_client_init(&websocket_cfg);
        esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);
        esp_websocket_client_start(client);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    websocketConnectStarted = false;

    // while (1)
    // {
    //     if (isWebsocketAppStarted == false)
    //         websocket_app_start();

    //     if ((WebsocketConnectedisConnected() == false) && (isWebsocketAppStarted == true))
    //     {
    //     }
    // }

    vTaskDelete(NULL);
}

void reconnect_websocket_task_delete(void)
{
    ESP_LOGE(TAG, "Deleting reconnect_websocket task");
    vTaskDelete(Reconnect_taskHandle);
}

void websocket_connect(void)
{
    websocketConnectStarted = true;
    xTaskCreate(&reconnect_websocket, "reconnect_websocket", 4096, "reconnect_websocket", 6, &Reconnect_taskHandle);
}

void websocket_app_stop(void)
{
    // Stop websocket connection
    esp_websocket_client_stop(client);
    ESP_LOGI(TAG, "Websocket Stopped");
    esp_websocket_client_destroy(client);
}

void websocket_destroy(void)
{
    ESP_LOGE(TAG, "Destroying Websocket client");
    esp_websocket_client_destroy(client);
}
bool WebsocketConnectedisConnected(void)
{
    if (config.offline)
        return false;
    return esp_websocket_client_is_connected(client);
}