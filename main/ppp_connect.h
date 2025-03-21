#ifndef PPP_CONNECT_H
#define PPP_CONNECT_H

#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "networkInterfacesControl.h"

typedef struct
{
    iface_info_t parent;
    void *context;
    bool stop_task;
} ppp_info_t;

void ppp_task(void *args);
iface_info_t *modem_initialise(int prio, uint8_t NumOfInterfaces);
// iface_info_t *modem_initialise(int prio);

void ppp_destroy_context(ppp_info_t *ppp_info);

#endif