#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include "esp_wifi.h"
#include "networkInterfacesControl.h"

#define WIFI_RECONNECT_TIME 5000
iface_info_t *wifi_initialise(int prio, uint8_t NumOfInterfaces);
// iface_info_t *wifi_initialise(int prio);
esp_err_t wifi_connect_sta(char *ssid, char *pass, int timeout);
char *get_wifi_disconnection_string(wifi_err_reason_t wifi_err_reason);
bool WifiApInit(void);
bool isStationConnected(void);
bool WifiApDeInit(void);

#endif