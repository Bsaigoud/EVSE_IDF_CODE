#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "ProjectSettings.h"
#include "freertos/semphr.h"
#include "esp_mac.h"
#include "wifi_connect.h"
#include "slave.h"
#include "networkInterfacesControl.h"
#include <netdb.h>

static char *TAG = "WIFI CONNECT";
static esp_netif_t *esp_netif = NULL;
esp_netif_ip_info_t if_info;
static EventGroupHandle_t wifi_events;
static int CONNECTED = BIT0;
static int DISCONNECTED = BIT1;
extern bool isWifiConnected;
extern bool wifiRetry;
extern Config_t config;
extern bool isWifiAlreadyInitialised;
bool stationConnected = false;

static void wifi_destroy(iface_info_t *info)
{
    esp_netif_action_disconnected(info->netif, 0, 0, 0);
    esp_netif_action_stop(info->netif, 0, 0, 0);
    esp_wifi_stop();
    esp_wifi_deinit();
    free(info);
}

void event_handler(void *event_handler_arg, esp_event_base_t event_base,
                   int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
        esp_wifi_connect();

        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");

        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        wifi_event_sta_disconnected_t *wifi_event_sta_disconnected = event_data;
        ESP_LOGW(TAG, "DISCONNECTED %d: %s", wifi_event_sta_disconnected->reason,
                 get_wifi_disconnection_string(wifi_event_sta_disconnected->reason));
        isWifiConnected = false;
        wifiRetry = false;
        if (wifi_event_sta_disconnected->reason != WIFI_REASON_NO_AP_FOUND)
        {
            wifiRetry = true;
        }
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
        xEventGroupSetBits(wifi_events, DISCONNECTED);
        break;
    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
        isWifiConnected = true;
        xEventGroupSetBits(wifi_events, CONNECTED);
        break;

    case WIFI_EVENT_AP_START:
        ESP_LOGI(TAG, "WIFI_EVENT_AP_START");
        break;
    case WIFI_EVENT_AP_STOP:
        ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP");
        break;
    case WIFI_EVENT_AP_STACONNECTED:
        stationConnected = true;
        ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED");
        // connect_handler(event_handler_arg, event_base, event_id, event_data);
        break;
    case WIFI_EVENT_AP_STADISCONNECTED:
        stationConnected = false;
        ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
        // disconnect_handler(event_handler_arg, event_base, event_id, event_data);
        break;
    default:
        break;
    }
}

bool WifiApInit(void)
{
    char ap_ssid[32] = "KISHORE";
    char pass[64] = "Amplify@5";
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    // Wifi Connect AP
    esp_netif = esp_netif_create_default_wifi_ap();
    esp_netif_get_ip_info(esp_netif, &if_info);
    // ESP_LOGI(TAG, "ESP32 IP:" IPSTR, IP2STR(&if_info.ip));

    uint8_t mac[6] = {0};
    esp_efuse_mac_get_default(mac);
    memset(ap_ssid, '\0', sizeof(ap_ssid));
    if (memcmp(config.serialNumber, "Amplify", strlen("Amplify")) == 0)
    {
        memcpy(ap_ssid, config.macAddress, strlen(config.macAddress));
    }
    else
    {
        memcpy(ap_ssid, config.serialNumber, strlen(config.serialNumber));
    }
    // sprintf(ap_ssid, "%s_%02x:%02x:%02x:%02x:%02x:%02x", config.serialNumber, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Please Connect Your Device to this AP, SSID: %s PSW: %s\r\n", ap_ssid, pass);
    ESP_LOGI(TAG, "Open Your Browser & Access the Webpage: " IPSTR "/config\r\n", IP2STR(&if_info.ip));

    wifi_config_t wifi_config = {};
    memcpy(wifi_config.ap.ssid, ap_ssid, strlen(ap_ssid));
    memcpy(wifi_config.ap.password, pass, strlen(pass));

    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK; // WIFI_AUTH_WPA2_PSK
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.beacon_interval = 100;
    wifi_config.ap.channel = 1;
    wifi_config.ap.pmf_cfg.required = false;

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
    return true;
}

bool isStationConnected(void)
{
    return stationConnected;
}

bool WifiApDeInit(void)
{
    esp_wifi_stop();
    esp_netif_destroy(esp_netif);
    esp_netif = NULL;
    return true;
}

iface_info_t *wifi_initialise(int prio, uint8_t NumOfInterfaces)
{
    iface_info_t *wifi_info = malloc(sizeof(iface_info_t));
    assert(wifi_info);
    wifi_info->destroy = wifi_destroy;
    wifi_info->name = "WiFi";
    wifi_events = xEventGroupCreate();

    // if (NumOfInterfaces == 1)
    if (NumOfInterfaces <= 3)
    {
        esp_netif_create_default_wifi_sta();
    }
    else
    {
        esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
        esp_netif_config.route_prio = prio;
        wifi_info->netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
        esp_wifi_set_default_wifi_sta_handlers();
    }

    if (isWifiAlreadyInitialised == false)
    {
        wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&wifi_init_config);
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL);
        esp_wifi_set_storage(WIFI_STORAGE_RAM);
    }

    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, config.wifiSSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, config.wifiPassword, sizeof(wifi_config.sta.password) - 1);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
    esp_wifi_start();

    EventBits_t result = xEventGroupWaitBits(wifi_events, CONNECTED | DISCONNECTED, true, false, pdMS_TO_TICKS(10000));

    return wifi_info;
}

char *get_wifi_disconnection_string(wifi_err_reason_t wifi_err_reason)
{
    switch (wifi_err_reason)
    {
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
        return "WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY";
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
        return "WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD";
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
        return "WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD";
    case WIFI_REASON_UNSPECIFIED:
        return "WIFI_REASON_UNSPECIFIED";
    case WIFI_REASON_AUTH_EXPIRE:
        return "WIFI_REASON_AUTH_EXPIRE";
    case WIFI_REASON_AUTH_LEAVE:
        return "WIFI_REASON_AUTH_LEAVE";
    case WIFI_REASON_ASSOC_EXPIRE:
        return "WIFI_REASON_ASSOC_EXPIRE";
    case WIFI_REASON_ASSOC_TOOMANY:
        return "WIFI_REASON_ASSOC_TOOMANY";
    case WIFI_REASON_NOT_AUTHED:
        return "WIFI_REASON_NOT_AUTHED";
    case WIFI_REASON_NOT_ASSOCED:
        return "WIFI_REASON_NOT_ASSOCED";
    case WIFI_REASON_ASSOC_LEAVE:
        return "WIFI_REASON_ASSOC_LEAVE";
    case WIFI_REASON_ASSOC_NOT_AUTHED:
        return "WIFI_REASON_ASSOC_NOT_AUTHED";
    case WIFI_REASON_DISASSOC_PWRCAP_BAD:
        return "WIFI_REASON_DISASSOC_PWRCAP_BAD";
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
        return "WIFI_REASON_DISASSOC_SUPCHAN_BAD";
    case WIFI_REASON_BSS_TRANSITION_DISASSOC:
        return "WIFI_REASON_BSS_TRANSITION_DISASSOC";
    case WIFI_REASON_IE_INVALID:
        return "WIFI_REASON_IE_INVALID";
    case WIFI_REASON_MIC_FAILURE:
        return "WIFI_REASON_MIC_FAILURE";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        return "WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
        return "WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:
        return "WIFI_REASON_IE_IN_4WAY_DIFFERS";
    case WIFI_REASON_GROUP_CIPHER_INVALID:
        return "WIFI_REASON_GROUP_CIPHER_INVALID";
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
        return "WIFI_REASON_PAIRWISE_CIPHER_INVALID";
    case WIFI_REASON_AKMP_INVALID:
        return "WIFI_REASON_AKMP_INVALID";
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
        return "WIFI_REASON_UNSUPP_RSN_IE_VERSION";
    case WIFI_REASON_INVALID_RSN_IE_CAP:
        return "WIFI_REASON_INVALID_RSN_IE_CAP";
    case WIFI_REASON_802_1X_AUTH_FAILED:
        return "WIFI_REASON_802_1X_AUTH_FAILED";
    case WIFI_REASON_CIPHER_SUITE_REJECTED:
        return "WIFI_REASON_CIPHER_SUITE_REJECTED";
    case WIFI_REASON_TDLS_PEER_UNREACHABLE:
        return "WIFI_REASON_TDLS_PEER_UNREACHABLE";
    case WIFI_REASON_TDLS_UNSPECIFIED:
        return "WIFI_REASON_TDLS_UNSPECIFIED";
    case WIFI_REASON_SSP_REQUESTED_DISASSOC:
        return "WIFI_REASON_SSP_REQUESTED_DISASSOC";
    case WIFI_REASON_NO_SSP_ROAMING_AGREEMENT:
        return "WIFI_REASON_NO_SSP_ROAMING_AGREEMENT";
    case WIFI_REASON_BAD_CIPHER_OR_AKM:
        return "WIFI_REASON_BAD_CIPHER_OR_AKM";
    case WIFI_REASON_NOT_AUTHORIZED_THIS_LOCATION:
        return "WIFI_REASON_NOT_AUTHORIZED_THIS_LOCATION";
    case WIFI_REASON_SERVICE_CHANGE_PERCLUDES_TS:
        return "WIFI_REASON_SERVICE_CHANGE_PERCLUDES_TS";
    case WIFI_REASON_UNSPECIFIED_QOS:
        return "WIFI_REASON_UNSPECIFIED_QOS";
    case WIFI_REASON_NOT_ENOUGH_BANDWIDTH:
        return "WIFI_REASON_NOT_ENOUGH_BANDWIDTH";
    case WIFI_REASON_MISSING_ACKS:
        return "WIFI_REASON_MISSING_ACKS";
    case WIFI_REASON_EXCEEDED_TXOP:
        return "WIFI_REASON_EXCEEDED_TXOP";
    case WIFI_REASON_STA_LEAVING:
        return "WIFI_REASON_STA_LEAVING";
    case WIFI_REASON_END_BA:
        return "WIFI_REASON_END_BA";
    case WIFI_REASON_UNKNOWN_BA:
        return "WIFI_REASON_UNKNOWN_BA";
    case WIFI_REASON_TIMEOUT:
        return "WIFI_REASON_TIMEOUT";
    case WIFI_REASON_PEER_INITIATED:
        return "WIFI_REASON_PEER_INITIATED";
    case WIFI_REASON_AP_INITIATED:
        return "WIFI_REASON_AP_INITIATED";
    case WIFI_REASON_INVALID_FT_ACTION_FRAME_COUNT:
        return "WIFI_REASON_INVALID_FT_ACTION_FRAME_COUNT";
    case WIFI_REASON_INVALID_PMKID:
        return "WIFI_REASON_INVALID_PMKID";
    case WIFI_REASON_INVALID_MDE:
        return "WIFI_REASON_INVALID_MDE";
    case WIFI_REASON_INVALID_FTE:
        return "WIFI_REASON_INVALID_FTE";
    case WIFI_REASON_TRANSMISSION_LINK_ESTABLISH_FAILED:
        return "WIFI_REASON_TRANSMISSION_LINK_ESTABLISH_FAILED";
    case WIFI_REASON_ALTERATIVE_CHANNEL_OCCUPIED:
        return "WIFI_REASON_ALTERATIVE_CHANNEL_OCCUPIED";
    case WIFI_REASON_BEACON_TIMEOUT:
        return "WIFI_REASON_BEACON_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND:
        return "WIFI_REASON_NO_AP_FOUND";
    case WIFI_REASON_AUTH_FAIL:
        return "WIFI_REASON_AUTH_FAIL";
    case WIFI_REASON_ASSOC_FAIL:
        return "WIFI_REASON_ASSOC_FAIL";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "WIFI_REASON_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_CONNECTION_FAIL:
        return "WIFI_REASON_CONNECTION_FAIL";
    case WIFI_REASON_AP_TSF_RESET:
        return "WIFI_REASON_AP_TSF_RESET";
    case WIFI_REASON_ROAMING:
        return "WIFI_REASON_ROAMING";
    case WIFI_REASON_ASSOC_COMEBACK_TIME_TOO_LONG:
        return "WIFI_REASON_ASSOC_COMEBACK_TIME_TOO_LONG";
    case WIFI_REASON_SA_QUERY_TIMEOUT:
        return "WIFI_REASON_SA_QUERY_TIMEOUT";
    }
    return "UNKNOWN";
}