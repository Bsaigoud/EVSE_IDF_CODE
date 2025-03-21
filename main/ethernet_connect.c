/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Ethernet Basic Initialization

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "esp_eth_driver.h"
#include <string.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_eth_mac.h"
#include "sdkconfig.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "networkInterfacesControl.h"
#include "ProjectSettings.h"

static const char *TAG = "ethernet_connect";
#define ETH_SPI_HOST SPI2_HOST
struct eth_info_t *eth_info;
extern Config_t config;
extern bool isEthernetConnected;

uint8_t local_mac_addr[6] = {0};
uint8_t eth_mac_addr[6] = {0};
struct eth_info_t
{
    iface_info_t parent;
    esp_eth_handle_t eth_handle;
    esp_eth_netif_glue_handle_t glue;
    esp_eth_mac_t *mac;
    esp_eth_phy_t *phy;
};

static void eth_event_handler(void *args, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    struct eth_info_t *eth_info = args;

    switch (event_id)
    {
    case ETHERNET_EVENT_CONNECTED:
        if (config.ethernetConfig)
        {
            esp_netif_ip_info_t ip_info;
            if (!ip4addr_aton(config.ipAddress, &ip_info.ip))
            {
                ESP_LOGI(TAG, "Invalid ipAddress");
            }
            if (!ip4addr_aton(config.gatewayAddress, &ip_info.gw))
            {
                ESP_LOGI(TAG, "Invalid gatewayAddress");
            }
            if (!ip4addr_aton(config.subnetMask, &ip_info.netmask))
            {
                ESP_LOGI(TAG, "Invalid subnetMask");
            }

            // IP4_ADDR(&ip_info.ip, 192, 168, 0, 173);
            // IP4_ADDR(&ip_info.gw, 192, 168, 0, 1);
            // IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

            esp_netif_dhcpc_stop(eth_info->parent.netif);
            esp_netif_set_ip_info(eth_info->parent.netif, &ip_info);

            esp_netif_dns_info_t dns_info;
            if (!ip4addr_aton(config.dnsAddress, &dns_info.ip.u_addr.ip4))
            {
                ESP_LOGI(TAG, "Invalid dnsAddress");
            }
            // IP4_ADDR(&dns_info.ip.u_addr.ip4, 192, 168, 0, 1); // Set DNS server address here
            dns_info.ip.type = ESP_NETIF_DNS_MAIN;
            esp_netif_set_dns_info(eth_info->parent.netif, ESP_NETIF_DNS_MAIN, &dns_info);
        }

        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        if (config.ethernetConfig)
        {
            eth_info->parent.connected = true;
            isEthernetConnected = true;
        }
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        eth_info->parent.connected = false;
        isEthernetConnected = false;
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

static void got_ip_event_handler(void *args, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;
    struct eth_info_t *eth_info = args;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "IP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "MASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "GW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    if (config.ethernetConfig == false)
    {
        eth_info->parent.connected = true;
        isEthernetConnected = true;
    }
    for (int i = 0; i < 2; ++i)
    {
        esp_netif_get_dns_info(eth_info->parent.netif, i, &eth_info->parent.dns[i]);
        ESP_LOGI(TAG, "DNS %i:" IPSTR, i, IP2STR(&eth_info->parent.dns[i].ip.u_addr.ip4));
    }
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

static void eth_destroy(iface_info_t *info)
{
    struct eth_info_t *eth_info = __containerof(info, struct eth_info_t, parent);

    esp_eth_stop(eth_info->eth_handle);
    esp_eth_del_netif_glue(eth_info->glue);
    esp_eth_driver_uninstall(eth_info->eth_handle);
    eth_info->phy->del(eth_info->phy);
    eth_info->mac->del(eth_info->mac);
    esp_netif_destroy(eth_info->parent.netif);
    free(eth_info);
}

uint8_t reverse_byte(uint8_t byte)
{
    uint8_t result = 0;
    for (int i = 0; i < 8; i++)
    {
        result <<= 1;         // Shift result left
        result |= (byte & 1); // Copy the least significant bit of byte to result
        byte >>= 1;           // Shift byte right
    }
    return result;
}

iface_info_t *ethernet_initialise(int prio)
{
    eth_info = malloc(sizeof(struct eth_info_t));
    assert(eth_info);
    eth_info->parent.destroy = eth_destroy;
    eth_info->parent.name = "Ethernet";
    esp_efuse_mac_get_default(local_mac_addr);

    spi_bus_config_t buscfg = {
        .miso_io_num = ETH_MISO_PIN,
        .mosi_io_num = ETH_MOSI_PIN,
        .sclk_io_num = ETH_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_bus_initialize(ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = 10000000,
        .queue_size = 20,
        .spics_io_num = ETH_CS_PIN,
    };

    // Init common MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    // mac_config.rx_task_stack_size = 2048;
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = -1;
    phy_config.phy_addr = 1;

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(ETH_SPI_HOST, &spi_devcfg);
    w5500_config.poll_period_ms = 20;
    w5500_config.int_gpio_num = -1;
    eth_info->mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    eth_info->phy = esp_eth_phy_new_w5500(&phy_config);

    // Init Ethernet driver to default and install it
    esp_eth_config_t eth_config_spi = ETH_DEFAULT_CONFIG(eth_info->mac, eth_info->phy);
    esp_err_t err = esp_eth_driver_install(&eth_config_spi, &eth_info->eth_handle);
    if (err == ESP_OK)
    {
        memcpy(eth_mac_addr, local_mac_addr, 6);
        // eth_mac_addr[0] = reverse_byte(eth_mac_addr[0]);
        // eth_mac_addr[1] = reverse_byte(eth_mac_addr[1]);
        // eth_mac_addr[2] = reverse_byte(eth_mac_addr[2]);
        eth_mac_addr[3] = reverse_byte(eth_mac_addr[3]);
        eth_mac_addr[4] = reverse_byte(eth_mac_addr[4]);
        eth_mac_addr[5] = reverse_byte(eth_mac_addr[5]);
        eth_info->mac->set_addr(eth_info->mac, eth_mac_addr);

        // Create an instance of esp-netif for Ethernet
        esp_netif_config_t base_netif_cfg = ESP_NETIF_DEFAULT_ETH();
        // base_netif_cfg.route_prio = prio;

        // esp_netif_config_t cfg = {
        //     .base = &base_netif_cfg,
        //     .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,
        // };
        eth_info->parent.netif = esp_netif_new(&base_netif_cfg);
        eth_info->glue = esp_eth_new_netif_glue(eth_info->eth_handle);
        // Attach Ethernet driver to TCP/IP stack
        esp_netif_attach(eth_info->parent.netif, eth_info->glue);

        // Register user defined event handers
        esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, eth_info);
        esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, eth_info);

        // Start Ethernet driver state machine
        esp_eth_start(eth_info->eth_handle);

        return &eth_info->parent;
    }
    free(eth_info);
    return NULL;
}

void restartEthernet(void)
{
    ESP_LOGE(TAG, "Restarting Ethernet");
    esp_eth_stop(eth_info->eth_handle);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_eth_start(eth_info->eth_handle);
}
