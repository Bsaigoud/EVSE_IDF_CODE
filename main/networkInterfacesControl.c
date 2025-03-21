#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <lwip/dns.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "custom_nvs.h"
#include "ProjectSettings.h"
#include "wifi_connect.h"
#include "esp_now.h"
#include "ppp_connect.h"
#include "networkInterfacesControl.h"
#include "websocket_connect.h"
#include "display.h"
#include "slave.h"
#include "ocpp_task.h"
#include "OTA.h"

#define TAG "INTERFACE"

#define HOST "www.espressif.com"

bool FirmwareUpdatingForOfflineCharger = false;
bool WifiInitialised = false;
bool isWifiConnected = false;
bool isModemConnected = false;
bool isEthernetConnected = false;
bool isWebsocketConnected = false;
bool WebSocketClientNotStarted = false;
bool WebSocketClientNotConnected = false;
bool WifiConnectedButConnectivityLost = false;
bool EthernetConnectedButConnectivityLost = false;
bool PPPConnectedButConnectivityLost = false;
bool wifiRetry = false;
bool websocketConnectStarted = false;
bool emergencyReceivedFromSlave = false;
uint8_t websocketConnectStartedCount = 0;
extern uint8_t emergencyButton;
extern bool FirmwareUpdateStarted;
extern uint8_t slavePushButton[4];
extern uint8_t taskState[NUM_OF_CONNECTORS];
extern bool connectorEnabled[NUM_OF_CONNECTORS];
bool isWifiAlreadyInitialised = false;
extern Config_t config;
extern bool taskInitialized;
uint8_t HighPriorityNetwork = 0;
int Wifi_Interface_Id = -1;
int ETH_Interface_Id = -1;
int PPP_Interface_Id = -1;
uint32_t loopCount = 0;
iface_info_t *iface_info[3];
iface_info_t *Wifi_iface;
iface_info_t *wifi_initialise(int prio, uint8_t NumOfInterfaces);
void restartEthernet(void);

// iface_info_t *wifi_initialise(int priority);

iface_info_t *ethernet_initialise(int priority);
// void ethernetStart(iface_info_t *iface_info);
// void ethernetStop(iface_info_t *iface_info);

esp_err_t check_connectivity(const char *host);
// void ppp_create_context(iface_info_t *iface_info);
// void ppp_destroy_context(iface_info_t *iface_info);

// void toggle_netif(iface_info_t *iface, int interfaceId, uint8_t interface, bool enable)
// {

//     if (enable)
//     {
//         if (interface == WIFI_INTERFACE)
//         {
//             esp_wifi_start();
//             ESP_LOGI("NETIF", "Wifi Network interface Created");
//         }
//         if (interface == PPP_INTERFACE)
//         {
//             ppp_create_context(iface);
//             ESP_LOGI("NETIF", "PPP Network interface Created");
//         }
//         if (interface == ETH_INTERFACE)
//         {
//             ethernetStart(iface);
//         }
//     }
//     else
//     {
//         if (interface == WIFI_INTERFACE)
//         {
//             ESP_LOGI("NETIF", "Wifi Network interface destroyed");
//             esp_wifi_stop();
//         }
//         if (interface == PPP_INTERFACE)
//         {
//             ESP_LOGI("NETIF", "PPP Network interface destroyed");
//             ppp_destroy_context(iface);
//         }
//         if (interface == ETH_INTERFACE)
//         {
//             ESP_LOGI("NETIF", "Ethernet Network interface destroyed");
//             ethernetStop(iface);
//         }
//     }
// }

void updateHighPriorityNetwork(void)
{
    bool connectingToEthernet = false;
    bool connectingToWifi = false;
    bool connectingToGsm = false;

    if (config.ethernetAvailability && config.ethernetEnable)
    {
        if (config.wifiAvailability && config.wifiEnable && config.gsmAvailability && config.gsmEnable)
        {
            if ((config.ethernetPriority > config.gsmPriority) && (config.ethernetPriority > config.wifiPriority))
            {
                connectingToEthernet = true;
            }
        }
        else if (config.wifiAvailability && config.wifiEnable)
        {
            if (config.ethernetPriority > config.wifiPriority)
            {
                connectingToEthernet = true;
            }
        }
        else if (config.gsmAvailability && config.gsmEnable)
        {
            if (config.ethernetPriority > config.gsmPriority)
            {
                connectingToEthernet = true;
            }
        }
        else
        {
            connectingToEthernet = true;
        }
    }
    if (config.wifiAvailability && config.wifiEnable)
    {
        if (config.gsmAvailability && config.gsmEnable && config.ethernetAvailability && config.ethernetEnable)
        {
            if ((config.wifiPriority > config.gsmPriority) && (config.wifiPriority > config.ethernetPriority))
            {
                connectingToWifi = true;
            }
        }
        else if (config.gsmAvailability && config.gsmEnable)
        {
            if (config.wifiPriority > config.gsmPriority)
            {
                connectingToWifi = true;
            }
        }
        else if (config.ethernetAvailability && config.ethernetEnable)
        {
            if (config.wifiPriority > config.ethernetPriority)
            {
                connectingToWifi = true;
            }
        }
        else
        {
            connectingToWifi = true;
        }
    }
    if (config.gsmAvailability && config.gsmEnable)
    {
        if (config.ethernetAvailability && config.ethernetEnable && config.wifiAvailability && config.wifiEnable)
        {
            if ((config.gsmPriority > config.ethernetPriority) && (config.gsmPriority > config.wifiPriority))
            {
                connectingToGsm = true;
            }
        }
        else if (config.ethernetAvailability && config.ethernetEnable)
        {
            if (config.gsmPriority > config.ethernetPriority)
            {
                connectingToGsm = true;
            }
        }
        else if (config.wifiAvailability && config.wifiEnable)
        {
            if (config.gsmPriority > config.wifiPriority)
            {
                connectingToGsm = true;
            }
        }
        else
        {
            connectingToGsm = true;
        }
    }

    if (connectingToEthernet)
    {
        HighPriorityNetwork = 3;
    }
    else if (connectingToWifi)
    {
        HighPriorityNetwork = 1;
    }
    else if (connectingToGsm)
    {
        HighPriorityNetwork = 2;
    }
    else
    {
        HighPriorityNetwork = 0;
    }
}

static void Assign_netifs(void)
{
    esp_netif_t *netif = NULL;
}

static int get_default(iface_info_t *list[], size_t num)
{
    esp_netif_t *default_netif = esp_netif_get_default_netif();
    if (default_netif == NULL)
    {
        ESP_LOGE(TAG, "default netif is NULL!");
        return -1;
    }
    ESP_LOGI(TAG, "Default netif: %s", esp_netif_get_desc(default_netif));

    for (int i = 0; i < num; ++i)
    {
        if (list[i] && list[i]->netif == default_netif)
        {
            ESP_LOGI(TAG, "Default interface: %s , %d", list[i]->name, i);
            return i;
        }
    }
    // not found
    return -2;
}

void wifi_connect(iface_info_t *list[], size_t num)
{
    uint8_t i = 0;
    if (config.wifiAvailability & config.wifiEnable)
    {
        esp_wifi_connect();
        // vTaskDelay(pdMS_TO_TICKS(2000));
        // while (wifiRetry)
        // {
        //     vTaskDelay(pdMS_TO_TICKS(5000));
        //     esp_wifi_connect();
        //     i++;
        //     if (i > 5)
        //     {
        //         break;
        //     }
        // }
    }
}

bool wifiStartPowerOn(void)
{
    if (config.defaultConfig == false)
    {
        isWifiAlreadyInitialised = true;
        WifiApInit();
    }
    else
    {
        if (config.vcharge_lite_1_4)
        {
            uint16_t count = 0;
            bool receivedEmergencyButton = false;
            while ((count < 10) && (receivedEmergencyButton == false))
            {
                if (SlaveGetPushButtonState(PUSH_BUTTON4))
                {
                    emergencyButton = slavePushButton[3];
                    receivedEmergencyButton = true;
                }
                count++;
            }
        }
        if (emergencyButton)
        {
            isWifiAlreadyInitialised = true;
            WifiApInit();
        }
    }

    return isWifiAlreadyInitialised;
}

void wifiStopPowerOn(void)
{
    WifiApDeInit();
}

void interfaceTask(void *params)
{
    uint8_t available_interfaces = 0;
    uint8_t interface_index = 0;
    uint32_t WebsocketOffCount = 0;
    uint32_t WebsocketOffWithNetworkCount = 0;
    uint32_t EthernetInternetDisconnectTime = 0;
    bool isWebsocketConnected_old = false;
    bool networkSwitchRequired = false;

    int Current_Interface_Id = -2;
    int Current_Interface_Id_old = -2;
    int Current_Interface_Id_priority = 0;
    if (config.wifiAvailability & config.wifiEnable)
    {
        available_interfaces++;
    }
    if (config.gsmAvailability & config.gsmEnable)
    {
        available_interfaces++;
    }
    if (config.ethernetAvailability & config.ethernetEnable)
    {
        available_interfaces++;
    }
    iface_info_t *ifaces[available_interfaces];

    if (config.wifiAvailability & config.wifiEnable)
    {
        ifaces[interface_index] = malloc(sizeof(iface_info_t));
        // ifaces[interface_index] = isWifiAlreadyInitialised ? Wifi_iface : wifi_initialise(config.wifiPriority);
        ifaces[interface_index] = wifi_initialise(config.wifiPriority, available_interfaces);
        Wifi_Interface_Id = interface_index;
        interface_index++;
    }
    WifiInitialised = true;
    if (config.gsmAvailability & config.gsmEnable)
    {
        ifaces[interface_index] = malloc(sizeof(iface_info_t));
        ifaces[interface_index] = modem_initialise(config.gsmPriority, available_interfaces);
        PPP_Interface_Id = interface_index;
        interface_index++;
    }
    if (config.ethernetAvailability & config.ethernetEnable)
    {
        ifaces[interface_index] = malloc(sizeof(iface_info_t));
        ifaces[interface_index] = ethernet_initialise(config.ethernetPriority);
        if (ifaces[interface_index] == NULL)
        {
            available_interfaces = available_interfaces - 1;
        }
        else
        {
            ETH_Interface_Id = interface_index;
            interface_index++;
        }
    }
    esp_netif_t *iter = NULL;

    // Get the list of network interfaces
    while ((iter = esp_netif_next(iter)) != NULL)
    {
        const char *if_desc = esp_netif_get_desc(iter);
        if (if_desc && strcmp(if_desc, "sta") == 0)
        {
            ifaces[Wifi_Interface_Id]->netif = iter;
        }
        else if (if_desc && strcmp(if_desc, "ppp") == 0)
        {
            ifaces[PPP_Interface_Id]->netif = iter;
        }
        else if (if_desc && strcmp(if_desc, "eth") == 0)
        {
            ifaces[ETH_Interface_Id]->netif = iter;
        }
    }
    updateHighPriorityNetwork();
    if (HighPriorityNetwork == 1)
    {
        if (available_interfaces > 1)
            esp_netif_set_default_netif(ifaces[Wifi_Interface_Id]->netif);
        Current_Interface_Id = Wifi_Interface_Id;
    }
    else if (HighPriorityNetwork == 2)
    {
        if (available_interfaces > 1)
            esp_netif_set_default_netif(ifaces[PPP_Interface_Id]->netif);
        Current_Interface_Id = PPP_Interface_Id;
    }
    else if (HighPriorityNetwork == 3)
    {
        if (available_interfaces > 1)
            esp_netif_set_default_netif(ifaces[ETH_Interface_Id]->netif);
        Current_Interface_Id = ETH_Interface_Id;
    }

    ESP_LOGI(TAG, "Wifi Interface Id: %d", Wifi_Interface_Id);
    ESP_LOGI(TAG, "GSM Interface Id: %d", PPP_Interface_Id);
    ESP_LOGI(TAG, "Ethernet Interface Id: %d", ETH_Interface_Id);
    uint8_t num_of_ifaces = sizeof(ifaces) / sizeof(ifaces[0]);
    ESP_LOGI(TAG, "num of ifaces %d", num_of_ifaces);
    if (HighPriorityNetwork == 1)
    {
        esp_wifi_connect();
        // wifi_connect(ifaces, num_of_ifaces);
    }
    uint32_t TastStartedTime = 0;
    // if (available_interfaces == 1)
    // {
    //     while (true)
    //     {
    //         if (config.wifiAvailability & config.wifiEnable)
    //         {
    //             if ((WebsocketConnectedisConnected() == false) && isWifiConnected)
    //             {
    //                 websocket_app_start();
    //             }
    //             else if (isWifiConnected == false)
    //             {
    //                 wifi_connect(ifaces, num_of_ifaces);
    //                 vTaskDelay(1000 / portTICK_PERIOD_MS);
    //             }
    //             else
    //             {
    //                 vTaskDelay(1000 / portTICK_PERIOD_MS);
    //             }
    //         }
    //         else if (config.gsmAvailability & config.gsmEnable)
    //         {
    //             if ((WebsocketConnectedisConnected() == false) && isModemConnected)
    //             {
    //                 websocket_app_start();
    //             }
    //             else if (isModemConnected == false)
    //             {
    //                 // modem_connect(ifaces, num_of_ifaces);
    //                 vTaskDelay(1000 / portTICK_PERIOD_MS);
    //             }
    //             else
    //             {
    //                 vTaskDelay(1000 / portTICK_PERIOD_MS);
    //             }
    //         }
    //     }
    // }
    // websocket_app_start();
    while (1)
    {
        dns_clear_cache();
        if (loopCount % 10 == 0)
        {
            esp_netif_t *default_netif = esp_netif_get_default_netif();
            if (default_netif != NULL)
            {
                if (strcmp(esp_netif_get_desc(default_netif), "ppp") == 0)
                {
                    ESP_LOGI(TAG, "PPP_Interface_Id %d", PPP_Interface_Id);
                    Current_Interface_Id = PPP_Interface_Id;
                }
                else if (strcmp(esp_netif_get_desc(default_netif), "sta") == 0)
                {
                    ESP_LOGI(TAG, "Wifi_Interface_Id %d", Wifi_Interface_Id);
                    Current_Interface_Id = Wifi_Interface_Id;
                }
                else if (strcmp(esp_netif_get_desc(default_netif), "eth") == 0)
                {
                    ESP_LOGI(TAG, "ETH_Interface_Id %d", ETH_Interface_Id);
                    Current_Interface_Id = ETH_Interface_Id;
                }
            }
            if ((Current_Interface_Id_old != Current_Interface_Id) &&
                (Current_Interface_Id_old >= 0) &&
                (Current_Interface_Id >= 0))
            {
                networkSwitchedUpdateDisplay();
            }

            isWebsocketConnected = WebsocketConnectedisConnected();
            if (isWebsocketConnected)
            {
                WebsocketOffWithNetworkCount = 0;
            }
            if (config.gsmAvailability & config.gsmEnable)
            {
                if (ifaces[PPP_Interface_Id]->connected)
                {
                    isModemConnected = true;
                }
                else
                {
                    isModemConnected = false;
                }
            }
            if ((WebsocketConnectedisConnected() == false) && (isWifiConnected || isModemConnected))
            {
                WebsocketOffWithNetworkCount++;
            }
            ESP_LOGI(TAG, "WebsocketOffWithNetworkCount %ld", WebsocketOffWithNetworkCount);
            if (WebsocketOffWithNetworkCount > 5)
            {
                if (config.gsmAvailability & config.gsmEnable)
                {
                    if ((ifaces[PPP_Interface_Id]->connected) & (Current_Interface_Id == PPP_Interface_Id))
                    {
                        ifaces[PPP_Interface_Id]->connected = false;
                    }
                }
                WebsocketOffWithNetworkCount = 0;
            }
            ESP_LOGI(TAG, "isWebsocketConnected %s", isWebsocketConnected ? "True" : "False");
            if (config.wifiAvailability & config.wifiEnable)
                ESP_LOGI(TAG, "wifi connected : %s", isWifiConnected ? "True" : "False");
            if (config.gsmAvailability & config.gsmEnable)
                ESP_LOGI(TAG, "GSM connected : %s", ifaces[PPP_Interface_Id]->connected ? "True" : "False");
            if (config.ethernetAvailability & config.ethernetEnable)
                ESP_LOGI(TAG, "Ethernet connected : %s", ifaces[ETH_Interface_Id]->connected ? "True" : "False");

            ESP_LOGI(TAG, "loopCount %ld", loopCount);
            if (Current_Interface_Id == PPP_Interface_Id)
            {
                ESP_LOGI(TAG, "Current_Interface_Id : PPP_Interface_Id");
            }
            else if (Current_Interface_Id == Wifi_Interface_Id)
            {
                ESP_LOGI(TAG, "Current_Interface_Id : Wifi_Interface_Id");
                if ((isWifiConnected == false) && (FirmwareUpdateStarted == false))
                    wifi_connect(ifaces, num_of_ifaces);
            }
            else if (Current_Interface_Id == ETH_Interface_Id)
            {
                ESP_LOGI(TAG, "Current_Interface_Id : ETH_Interface_Id");
            }
            /*
                        if ((loopCount % 10 == 0) && networkSwitchRequired && WebsocketConnectedisConnected())
                        {
                            if (available_interfaces == 3)
                            {
                                if (Current_Interface_Id == Wifi_Interface_Id)
                                {
                                    if ((config.ethernetPriority > config.wifiPriority) | (config.gsmPriority > config.wifiPriority))
                                    {
                                        if ((config.ethernetPriority > config.wifiPriority) & (ifaces[ETH_Interface_Id]->connected))
                                        {
                                            esp_netif_set_default_netif(ifaces[ETH_Interface_Id]->netif);
                                        }
                                        else if ((config.gsmPriority > config.wifiPriority) & (ifaces[PPP_Interface_Id]->connected))
                                        {
                                            esp_netif_set_default_netif(ifaces[PPP_Interface_Id]->netif);
                                        }
                                    }
                                }
                                else if (Current_Interface_Id == PPP_Interface_Id)
                                {
                                    if ((config.ethernetPriority > config.gsmPriority) | (config.wifiPriority > config.gsmPriority))
                                    {
                                        if ((config.ethernetPriority > config.gsmPriority) & (ifaces[ETH_Interface_Id]->connected))
                                        {
                                            esp_netif_set_default_netif(ifaces[ETH_Interface_Id]->netif);
                                        }
                                        else if (config.wifiPriority > config.gsmPriority)
                                        {
                                            if (isWifiConnected == false)
                                                wifi_connect(ifaces, num_of_ifaces);
                                            if (isWifiConnected)
                                            {
                                                esp_netif_set_default_netif(ifaces[Wifi_Interface_Id]->netif);
                                            }
                                            else
                                            {
                                                esp_netif_set_default_netif(ifaces[PPP_Interface_Id]->netif);
                                            }
                                        }
                                    }
                                }
                                else if (Current_Interface_Id == ETH_Interface_Id)
                                {
                                    if ((config.wifiPriority > config.ethernetPriority) | (config.gsmPriority > config.ethernetPriority))
                                    {
                                        if ((config.gsmPriority > config.ethernetPriority) & (isWifiConnected))
                                        {
                                            esp_netif_set_default_netif(ifaces[Wifi_Interface_Id]->netif);
                                        }
                                        else if (config.wifiPriority > config.ethernetPriority)
                                        {
                                            if (isWifiConnected == false)
                                                wifi_connect(ifaces, num_of_ifaces);
                                            if (isWifiConnected)
                                            {
                                                esp_netif_set_default_netif(ifaces[Wifi_Interface_Id]->netif);
                                            }
                                            else
                                            {
                                                esp_netif_set_default_netif(ifaces[ETH_Interface_Id]->netif);
                                            }
                                        }
                                    }
                                }
                            }
                            else if (available_interfaces == 2)
                            {
                                if (Current_Interface_Id == Wifi_Interface_Id)
                                {
                                    if (config.gsmAvailability & config.gsmEnable & (config.gsmPriority > config.wifiPriority))
                                    {
                                        if (ifaces[PPP_Interface_Id]->connected)
                                        {
                                            esp_netif_set_default_netif(ifaces[PPP_Interface_Id]->netif);
                                        }
                                    }
                                    else if (config.ethernetAvailability & config.ethernetEnable & (config.ethernetPriority > config.wifiPriority))
                                    {
                                        if (ifaces[ETH_Interface_Id]->connected)
                                        {
                                            esp_netif_set_default_netif(ifaces[ETH_Interface_Id]->netif);
                                        }
                                    }
                                }
                                else if (Current_Interface_Id == PPP_Interface_Id)
                                {
                                    if (config.wifiAvailability & config.wifiEnable & (config.wifiPriority > config.gsmPriority))
                                    {
                                        if (isWifiConnected == false)
                                            wifi_connect(ifaces, num_of_ifaces);
                                        if (isWifiConnected)
                                        {
                                            esp_netif_set_default_netif(ifaces[Wifi_Interface_Id]->netif);
                                        }
                                        else
                                        {
                                            esp_netif_set_default_netif(ifaces[PPP_Interface_Id]->netif);
                                        }
                                    }
                                    else if (config.ethernetAvailability & config.ethernetEnable & (config.ethernetPriority > config.gsmPriority))
                                    {
                                        if (ifaces[ETH_Interface_Id]->connected)
                                        {
                                            esp_netif_set_default_netif(ifaces[ETH_Interface_Id]->netif);
                                        }
                                    }
                                }
                                else if (Current_Interface_Id == ETH_Interface_Id)
                                {
                                    if (config.gsmAvailability & config.gsmEnable & (config.gsmPriority > config.ethernetPriority))
                                    {
                                        if (ifaces[PPP_Interface_Id]->connected)
                                        {
                                            esp_netif_set_default_netif(ifaces[PPP_Interface_Id]->netif);
                                        }
                                    }
                                    else if (config.wifiAvailability & config.wifiEnable & (config.wifiPriority > config.ethernetPriority))
                                    {
                                        if (isWifiConnected == false)
                                            wifi_connect(ifaces, num_of_ifaces);
                                        if (isWifiConnected)
                                        {
                                            esp_netif_set_default_netif(ifaces[Wifi_Interface_Id]->netif);
                                        }
                                        else
                                        {
                                            esp_netif_set_default_netif(ifaces[ETH_Interface_Id]->netif);
                                        }
                                    }
                                }
                            }
                        }
            */

            if ((WebsocketConnectedisConnected() == false) && (default_netif != NULL))
            {
                uint8_t network = 0;
                if (config.wifiAvailability & config.wifiEnable)
                {
                    ifaces[Wifi_Interface_Id]->connected = isWifiConnected;
                }
                if (ifaces[Current_Interface_Id]->connected == true)
                {
                    if (Current_Interface_Id == PPP_Interface_Id)
                    {
                        ESP_LOGE(TAG, "Trying PPP");
                        network = 2;
                        // if (config.wifiAvailability & config.wifiEnable)
                        // {
                        //     ESP_LOGW("NETIF", "Disabling WIFI_INTERFACE");
                        //     toggle_netif(ifaces[Wifi_Interface_Id], Wifi_Interface_Id, WIFI_INTERFACE, false);
                        // }
                        // if (config.ethernetAvailability & config.ethernetEnable)
                        // {
                        //     ESP_LOGW("NETIF", "Disabling ETH_INTERFACE");
                        //     toggle_netif(ifaces[ETH_Interface_Id], ETH_Interface_Id, ETH_INTERFACE, false);
                        // }
                    }
                    else if (Current_Interface_Id == Wifi_Interface_Id)
                    {
                        ESP_LOGE(TAG, "Trying Wifi");
                        network = 1;
                        // if (config.gsmAvailability & config.gsmEnable)
                        // {
                        //     ESP_LOGW("NETIF", "Disabling PPP_INTERFACE");
                        //     toggle_netif(ifaces[PPP_Interface_Id], PPP_Interface_Id, PPP_INTERFACE, false);
                        // }
                        // if (config.ethernetAvailability & config.ethernetEnable)
                        // {
                        //     ESP_LOGW("NETIF", "Disabling ETH_INTERFACE");
                        //     toggle_netif(ifaces[ETH_Interface_Id], ETH_Interface_Id, ETH_INTERFACE, false);
                        // }
                    }
                    else if (Current_Interface_Id == ETH_Interface_Id)
                    {
                        ESP_LOGE(TAG, "Trying ETH");
                        EthernetInternetDisconnectTime++;
                        network = 3;
                        // if (config.wifiAvailability & config.wifiEnable)
                        // {
                        //     ESP_LOGW("NETIF", "Disabling WIFI_INTERFACE");
                        //     toggle_netif(ifaces[Wifi_Interface_Id], Wifi_Interface_Id, WIFI_INTERFACE, false);
                        // }
                        // if (config.gsmAvailability & config.gsmEnable)
                        // {
                        //     ESP_LOGW("NETIF", "Disabling PPP_INTERFACE");
                        //     toggle_netif(ifaces[PPP_Interface_Id], PPP_Interface_Id, PPP_INTERFACE, false);
                        // }
                    }

                    if (isWifiConnected || isModemConnected || isEthernetConnected)
                        websocket_app_start(network);

                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    if (WebsocketConnectedisConnected() == false)
                    {
                        check_connectivity(HOST);
                    }
                    // if (Current_Interface_Id == PPP_Interface_Id)
                    // {
                    //     if (config.wifiAvailability & config.wifiEnable)
                    //     {
                    //         ESP_LOGE(TAG, "Enable Wifi");
                    //         toggle_netif(ifaces[Wifi_Interface_Id], Wifi_Interface_Id, WIFI_INTERFACE, true);
                    //     }
                    //     if (config.ethernetAvailability & config.ethernetEnable)
                    //     {
                    //         ESP_LOGE(TAG, "Enable ETH");
                    //         toggle_netif(ifaces[ETH_Interface_Id], ETH_Interface_Id, ETH_INTERFACE, true);
                    //     }
                    // }
                    // else if (Current_Interface_Id == Wifi_Interface_Id)
                    // {
                    //     ESP_LOGE(TAG, "Enable PPP");
                    //     if (config.gsmAvailability & config.gsmEnable)
                    //         toggle_netif(ifaces[PPP_Interface_Id], PPP_Interface_Id, PPP_INTERFACE, true);
                    //     ESP_LOGE(TAG, "Enable ETH");
                    //     if (config.ethernetAvailability & config.ethernetEnable)
                    //         toggle_netif(ifaces[ETH_Interface_Id], ETH_Interface_Id, ETH_INTERFACE, true);
                    // }
                    // else if (Current_Interface_Id == ETH_Interface_Id)
                    // {
                    //     ESP_LOGE(TAG, "Enable ETH");
                    //     if (config.wifiAvailability & config.wifiEnable)
                    //         toggle_netif(ifaces[Wifi_Interface_Id], Wifi_Interface_Id, WIFI_INTERFACE, true);
                    //     ESP_LOGE(TAG, "Enable PPP");
                    //     if (config.gsmAvailability & config.gsmEnable)
                    //         toggle_netif(ifaces[PPP_Interface_Id], PPP_Interface_Id, PPP_INTERFACE, true);
                    // }
                    // vTaskDelay(60000 / portTICK_PERIOD_MS);
                }
            }
            if ((isWifiConnected == false) && config.wifiAvailability && config.wifiEnable)
            {
                if (config.wifiAvailability &&
                    config.wifiEnable &&
                    ((HighPriorityNetwork == 1) || (TastStartedTime > 10)))
                {
                    if ((isWifiConnected == false) && (FirmwareUpdateStarted == false))
                        wifi_connect(ifaces, num_of_ifaces);
                }
            }

            if ((WebsocketConnectedisConnected() == false) && (TastStartedTime > 10))
            {
                if (Current_Interface_Id == Wifi_Interface_Id)
                {
                    if (isWifiConnected)
                    {
                        WifiConnectedButConnectivityLost = true;
                    }
                    if (available_interfaces == 3)
                    {
                        if (config.gsmAvailability & config.gsmEnable & ifaces[ETH_Interface_Id]->connected &
                            config.ethernetAvailability & config.ethernetEnable & ifaces[PPP_Interface_Id]->connected)
                        {
                            if (config.ethernetPriority > config.gsmPriority)
                            {
                                esp_netif_set_default_netif(ifaces[ETH_Interface_Id]->netif);
                            }
                            else
                            {
                                esp_netif_set_default_netif(ifaces[PPP_Interface_Id]->netif);
                            }
                        }
                        else if (config.gsmAvailability & config.gsmEnable & ifaces[PPP_Interface_Id]->connected)
                        {
                            esp_netif_set_default_netif(ifaces[PPP_Interface_Id]->netif);
                        }
                        else if (config.ethernetAvailability & config.ethernetEnable & ifaces[ETH_Interface_Id]->connected)
                        {
                            esp_netif_set_default_netif(ifaces[ETH_Interface_Id]->netif);
                        }
                    }
                    else if (available_interfaces == 2)
                    {
                        if (config.gsmAvailability & config.gsmEnable)
                        {
                            if (ifaces[PPP_Interface_Id]->connected)
                            {
                                esp_netif_set_default_netif(ifaces[PPP_Interface_Id]->netif);
                            }
                        }
                        else if (config.ethernetAvailability & config.ethernetEnable)
                        {
                            if (ifaces[ETH_Interface_Id]->connected)
                            {
                                esp_netif_set_default_netif(ifaces[ETH_Interface_Id]->netif);
                            }
                        }
                    }
                }
                else if (Current_Interface_Id == ETH_Interface_Id)
                {
                    if (ifaces[ETH_Interface_Id]->connected)
                    {
                        EthernetConnectedButConnectivityLost = true;
                    }
                    if (available_interfaces == 3)
                    {
                        if (isWifiConnected == false)
                            wifi_connect(ifaces, num_of_ifaces);
                        if (config.gsmAvailability & config.gsmEnable & ifaces[PPP_Interface_Id]->connected &
                            config.wifiAvailability & config.wifiEnable & isWifiConnected)
                        {
                            if (config.wifiPriority > config.gsmPriority)
                            {
                                esp_netif_set_default_netif(ifaces[Wifi_Interface_Id]->netif);
                            }
                            else
                            {
                                esp_netif_set_default_netif(ifaces[PPP_Interface_Id]->netif);
                            }
                        }
                        else if (config.gsmAvailability & config.gsmEnable & ifaces[PPP_Interface_Id]->connected)
                        {
                            esp_netif_set_default_netif(ifaces[PPP_Interface_Id]->netif);
                        }
                        else if (config.wifiAvailability & config.wifiEnable & isWifiConnected)
                        {
                            esp_netif_set_default_netif(ifaces[Wifi_Interface_Id]->netif);
                        }
                    }
                    else if (available_interfaces == 2)
                    {
                        if (config.gsmAvailability & config.gsmEnable)
                        {
                            if (ifaces[PPP_Interface_Id]->connected)
                            {
                                esp_netif_set_default_netif(ifaces[PPP_Interface_Id]->netif);
                            }
                        }
                        else if (config.wifiAvailability & config.wifiEnable)
                        {
                            if (isWifiConnected == false)
                                wifi_connect(ifaces, num_of_ifaces);
                            if (isWifiConnected)
                            {
                                esp_netif_set_default_netif(ifaces[Wifi_Interface_Id]->netif);
                            }
                        }
                    }
                }
                else if (Current_Interface_Id == PPP_Interface_Id)
                {
                    if (ifaces[PPP_Interface_Id]->connected)
                    {
                        PPPConnectedButConnectivityLost = true;
                    }
                    if (available_interfaces == 3)
                    {
                        if (isWifiConnected == false)
                            wifi_connect(ifaces, num_of_ifaces);
                        if (config.wifiAvailability & config.wifiEnable & isWifiConnected &
                            config.ethernetAvailability & config.ethernetEnable & ifaces[ETH_Interface_Id]->connected)
                        {
                            if (config.ethernetPriority > config.wifiPriority)
                            {
                                esp_netif_set_default_netif(ifaces[ETH_Interface_Id]->netif);
                            }
                            else
                            {
                                esp_netif_set_default_netif(ifaces[Wifi_Interface_Id]->netif);
                            }
                        }
                        else if (config.wifiAvailability & config.wifiEnable)
                        {
                            if (isWifiConnected)
                            {
                                esp_netif_set_default_netif(ifaces[Wifi_Interface_Id]->netif);
                            }
                        }
                        else if (config.ethernetAvailability & config.ethernetEnable)
                        {
                            if (ifaces[ETH_Interface_Id]->connected)
                            {
                                esp_netif_set_default_netif(ifaces[ETH_Interface_Id]->netif);
                            }
                        }
                    }
                    else if (available_interfaces == 2)
                    {
                        if (isWifiConnected == false)
                            wifi_connect(ifaces, num_of_ifaces);
                        if (config.wifiAvailability & config.wifiEnable & isWifiConnected)
                        {
                            esp_netif_set_default_netif(ifaces[Wifi_Interface_Id]->netif);
                        }
                        else if (config.ethernetAvailability & config.ethernetEnable)
                        {
                            if (ifaces[ETH_Interface_Id]->connected)
                            {
                                esp_netif_set_default_netif(ifaces[ETH_Interface_Id]->netif);
                            }
                        }
                    }
                }
            }
        }
        if ((WebsocketConnectedisConnected() == false) && (isWifiConnected || isModemConnected || isEthernetConnected))
        {
            WebsocketOffCount++;
            if (WebsocketOffCount >= 600)
            {
                WebsocketOffCount = 0;
                if (config.vcharge_lite_1_4)
                {
                    SlaveSendID(RESTART_ID);
                    vTaskDelay(2000 / portTICK_PERIOD_MS);
                }
                esp_restart();
            }
        }
        if (WebsocketConnectedisConnected())
        {
            EthernetInternetDisconnectTime = 0;
            WebsocketOffCount = 0;
        }

        if (EthernetInternetDisconnectTime > 2)
        {
            EthernetInternetDisconnectTime = 0;
            restartEthernet();
        }
        if (TastStartedTime > 10)
            loopCount++;
        else
        {
            loopCount = 0;
            TastStartedTime++;
        }

        Current_Interface_Id_old = Current_Interface_Id;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void initialise_interfaces(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    if (isWifiAlreadyInitialised == false)
    {
        ESP_ERROR_CHECK(esp_event_loop_create_default());
    }
    xTaskCreate(&interfaceTask, "interfaceTask", 6 * 1024, "interfaceTask", 2, NULL);
}