#ifndef NETWORK_INTERFACES_CONTROL_H
#define NETWORK_INTERFACES_CONTROL_H

#include "esp_netif.h"

typedef struct
{
    esp_netif_t *netif;
    esp_netif_dns_info_t dns[2];
    void (*destroy)(struct iface_info_t *);
    const char *name;
    bool connected;
} iface_info_t;

bool wifiStartPowerOn(void);
void wifiStopPowerOn(void);
bool wifiStationStartOfflineOTA(void);

void initialise_interfaces(void);

#endif
