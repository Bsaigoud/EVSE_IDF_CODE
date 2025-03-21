#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "ppp_connect.h"
#include "esp_event.h"
extern const esp_netif_driver_ifconfig_t *ppp_driver_cfg;

extern bool isModemConnected;
extern bool modemDiscoonectedFromPPPServer;
TaskHandle_t PPP_TaskHandle = NULL;
bool eventsRegisteredAlready = false;
static const int CONNECT_BIT = BIT0;
static const char *TAG = "pppos_connect";
static EventGroupHandle_t event_group = NULL;

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "PPP state changed event %" PRIu32, event_id);
    if (event_id == NETIF_PPP_ERRORUSER)
    {
        ppp_info_t *ppp_info = arg;
        esp_netif_t *netif = event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", netif);
        ppp_info->parent.connected = false;
    }
}

static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "IP event! %" PRIu32, event_id);
    ppp_info_t *ppp_info = arg;

    if (event_id == IP_EVENT_PPP_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        xEventGroupSetBits(event_group, CONNECT_BIT);
        ppp_info->parent.connected = true;
        isModemConnected = true;
        ESP_LOGI(TAG, "GOT ip event!!!");
    }
    else if (event_id == IP_EVENT_PPP_LOST_IP)
    {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
        isModemConnected = false;
        modemDiscoonectedFromPPPServer = true;
        ppp_info->parent.connected = false;
    }
    else if (event_id == IP_EVENT_GOT_IP6)
    {
        ESP_LOGI(TAG, "GOT IPv6 event!");
        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}

static void ppp_destroy(iface_info_t *info)
{
    ppp_info_t *ppp_info = __containerof(info, ppp_info_t, parent);
    // esp_netif_destroy(ppp_info->parent.netif);
    // esp_netif_action_disconnected(ppp_info->parent.netif, 0, 0, 0);
    // esp_netif_action_stop(ppp_info->parent.netif, 0, 0, 0);
    // vTaskDelete(PPP_TaskHandle);
    ppp_destroy_context(ppp_info);
    // vEventGroupDelete(event_group);
    // ppp_info->stop_task = true;
    // free(info);
}

iface_info_t *modem_initialise(int prio, uint8_t NumOfInterfaces)
{
    ppp_info_t *ppp_info = calloc(1, sizeof(ppp_info_t));
    assert(ppp_info);
    ppp_info->parent.destroy = ppp_destroy;
    ppp_info->parent.name = "Modem";
    event_group = xEventGroupCreate();

    if (eventsRegisteredAlready == false)
        esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, ppp_info);
    if (eventsRegisteredAlready == false)
        esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, ppp_info);

    // if (NumOfInterfaces == 1)
    if (NumOfInterfaces <= 3)
    {
        esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
        if (eventsRegisteredAlready == false)
            ppp_info->parent.netif = esp_netif_new(&netif_ppp_config);
    }
    else
    {
        esp_netif_inherent_config_t base_netif_cfg = ESP_NETIF_INHERENT_DEFAULT_PPP();
        base_netif_cfg.route_prio = prio;
        esp_netif_config_t netif_ppp_config = {.base = &base_netif_cfg,
                                               .driver = ppp_driver_cfg,
                                               .stack = ESP_NETIF_NETSTACK_DEFAULT_PPP};
        if (eventsRegisteredAlready == false)
            ppp_info->parent.netif = esp_netif_new(&netif_ppp_config);
    }

    if (ppp_info->parent.netif == NULL)
    {
        goto err;
    }
    if (xTaskCreate(ppp_task, "ppp_retry_task", 3072, ppp_info, 5, &PPP_TaskHandle) != pdTRUE)
    {
        goto err;
    }

    ESP_LOGI(TAG, "Waiting for IP address");
    if (eventsRegisteredAlready == false)
        xEventGroupWaitBits(event_group, CONNECT_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
    // eventsRegisteredAlready = true;

    return &ppp_info->parent;

err:

    ppp_destroy(&ppp_info->parent);
    return NULL;
}
