#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "local_http_server.h"
#include "ProjectSettings.h"
#include "custom_nvs.h"
#include "cJSON.h"
#include "esp_ota_ops.h"
#include "slave.h"
#include "ocpp.h"
#include "wifi_connect.h"
#include "ocpp_task.h"
#include "display.h"
#include "networkInterfacesControl.h"

extern Config_t config;
extern ledColors_t ledStateColor[NUM_OF_CONNECTORS];
extern ConnectorPrivateData_t ConnectorData[NUM_OF_CONNECTORS];
extern uint8_t slaveLed[NUM_OF_CONNECTORS];

char dnsName[32] = "Amplify";
char buffer[2048];
httpd_handle_t httpd_server = NULL;
httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
bool isUrlHit = false;
static bool isServerStarted = false;
static const char *TAG = "HTTP_SERVER";
uint8_t CurrentUrlType = 0;

enum urlType
{
    ADMIN_URL = 1,
    FACTORY_URL,
    SERVICE_URL,
    CUSTOMER_URL
};

// To disable CORS in windows press WINDOWS+R and enter
// chrome.exe --user-data-dir="C://Chrome dev session" --disable-web-security
// http://Amplify.local/config/admin

static esp_err_t Set_Config(uint8_t url)
{
    cJSON *Payload = cJSON_Parse(buffer);
    if (Payload != NULL)
    {
        cJSON *serialNumberJson = cJSON_GetObjectItem(Payload, "serialNumber");
        cJSON *chargerNameJson = cJSON_GetObjectItem(Payload, "chargerName");
        cJSON *chargePointVendorJson = cJSON_GetObjectItem(Payload, "chargePointVendor");
        cJSON *chargePointModelJson = cJSON_GetObjectItem(Payload, "chargePointModel");
        cJSON *boardTypeJson = cJSON_GetObjectItem(Payload, "boardType");
        cJSON *chargerTypeJson = cJSON_GetObjectItem(Payload, "chargerType");
        cJSON *connector1PWMJson = cJSON_GetObjectItem(Payload, "connector1PWM");
        cJSON *connector2PWMJson = cJSON_GetObjectItem(Payload, "connector2PWM");
        cJSON *connector3PWMJson = cJSON_GetObjectItem(Payload, "connector3PWM");
        cJSON *chargerDisplayTypeJson = cJSON_GetObjectItem(Payload, "chargerDisplayType");
        cJSON *DisplayNumJson = cJSON_GetObjectItem(Payload, "DisplayNum");
        cJSON *commissionedByJson = cJSON_GetObjectItem(Payload, "commissionedBy");
        cJSON *commissionedDateJson = cJSON_GetObjectItem(Payload, "commissionedDate");
        cJSON *webSocketURLJson = cJSON_GetObjectItem(Payload, "webSocketURL");
        cJSON *wifiStatusJson = cJSON_GetObjectItem(Payload, "wifiStatus");
        cJSON *wifiPriorityJson = cJSON_GetObjectItem(Payload, "wifiPriority");
        cJSON *wifiSSIDJson = cJSON_GetObjectItem(Payload, "wifiSSID");
        cJSON *wifiPasswordJson = cJSON_GetObjectItem(Payload, "wifiPassword");
        cJSON *gsmStatusJson = cJSON_GetObjectItem(Payload, "gsmStatus");
        cJSON *gsmPriorityJson = cJSON_GetObjectItem(Payload, "gsmPriority");
        cJSON *gsmAPNJson = cJSON_GetObjectItem(Payload, "gsmAPN");
        cJSON *ethernetStatusJson = cJSON_GetObjectItem(Payload, "ethernetStatus");
        cJSON *ethernetPriorityJson = cJSON_GetObjectItem(Payload, "ethernetPriority");
        cJSON *ethernetConfigJson = cJSON_GetObjectItem(Payload, "ethernetConfig");
        cJSON *ipAddressJson = cJSON_GetObjectItem(Payload, "ipAddress");
        cJSON *gatewayAddressJson = cJSON_GetObjectItem(Payload, "gatewayAddress");
        cJSON *dnsAddressJson = cJSON_GetObjectItem(Payload, "dnsAddress");
        cJSON *subnetMaskJson = cJSON_GetObjectItem(Payload, "subnetMask");
        cJSON *OnlineOfflineStatusJson = cJSON_GetObjectItem(Payload, "OnlineOfflineStatus");
        cJSON *gfciJson = cJSON_GetObjectItem(Payload, "gfci");
        cJSON *lowCurrentTimeJson = cJSON_GetObjectItem(Payload, "lowCurrentTime");
        cJSON *minLowCurrentThresholdJson = cJSON_GetObjectItem(Payload, "minLowCurrentThreshold");
        cJSON *resumeSessionJson = cJSON_GetObjectItem(Payload, "resumeSession");
        cJSON *overVoltageThresholdJson = cJSON_GetObjectItem(Payload, "overVoltageThreshold");
        cJSON *underVoltageThresholdJson = cJSON_GetObjectItem(Payload, "underVoltageThreshold");
        cJSON *con1overCurrentThresholdJson = cJSON_GetObjectItem(Payload, "con1overCurrentThreshold");
        cJSON *con2overCurrentThresholdJson = cJSON_GetObjectItem(Payload, "con2overCurrentThreshold");
        cJSON *con3overCurrentThresholdJson = cJSON_GetObjectItem(Payload, "con3overCurrentThreshold");
        cJSON *overTemperatureThresholdJson = cJSON_GetObjectItem(Payload, "overTemperatureThreshold");
        cJSON *restoreSessionJson = cJSON_GetObjectItem(Payload, "restoreSession");
        cJSON *sessionRestoreTimeJson = cJSON_GetObjectItem(Payload, "sessionRestoreTime");
        cJSON *StopTransactionInSuspendedStateStatusJson = cJSON_GetObjectItem(Payload, "StopTransactionInSuspendedStateStatus");
        cJSON *StopTransactionInSuspendedStateTimeJson = cJSON_GetObjectItem(Payload, "StopTransactionInSuspendedStateTime");
        cJSON *smartChargingStatusJson = cJSON_GetObjectItem(Payload, "smartChargingStatus");
        cJSON *batteryBackupStatusJson = cJSON_GetObjectItem(Payload, "batteryBackupStatus");
        cJSON *diagnosticServerStatusJson = cJSON_GetObjectItem(Payload, "diagnosticServerStatus");
        cJSON *alprDeviceStatusJson = cJSON_GetObjectItem(Payload, "alprDeviceStatus");
        cJSON *clearAuthCacheJson = cJSON_GetObjectItem(Payload, "clearAuthCache");
        cJSON *factoryResetJson = cJSON_GetObjectItem(Payload, "factoryReset");
        cJSON *otaURLConfigJson = cJSON_GetObjectItem(Payload, "otaURLConfig");
        cJSON *localListTagsJson = cJSON_GetObjectItem(Payload, "localListTags");
        cJSON *adminpasswordJson = cJSON_GetObjectItem(Payload, "adminpassword");
        cJSON *factorypasswordJson = cJSON_GetObjectItem(Payload, "factorypassword");
        cJSON *servicepasswordJson = cJSON_GetObjectItem(Payload, "servicepassword");
        cJSON *customerpasswordJson = cJSON_GetObjectItem(Payload, "customerpassword");

        vTaskDelay(100 / portTICK_PERIOD_MS);

        if (cJSON_IsString(serialNumberJson))
        {
            setNULL(config.serialNumber);
            memcpy(config.serialNumber, serialNumberJson->valuestring, strlen(serialNumberJson->valuestring));
        }
        if (cJSON_IsString(chargerNameJson))
        {
            setNULL(config.chargerName);
            memcpy(config.chargerName, chargerNameJson->valuestring, strlen(chargerNameJson->valuestring));
        }
        if (cJSON_IsString(chargePointVendorJson))
        {
            setNULL(config.chargePointVendor);
            memcpy(config.chargePointVendor, chargePointVendorJson->valuestring, strlen(chargePointVendorJson->valuestring));
        }
        if (cJSON_IsString(chargePointModelJson))
        {
            setNULL(config.chargePointModel);
            memcpy(config.chargePointModel, chargePointModelJson->valuestring, strlen(chargePointModelJson->valuestring));
        }
        if (cJSON_IsString(boardTypeJson))
        {
            config.AC1 = false;
            config.AC2 = false;
            config.vcharge_lite_1_4 = false;
            config.vcharge_lite_1_3 = false;
            config.vcharge_v5 = false;
            if (strcmp(boardTypeJson->valuestring, "AC1") == 0)
            {
                config.AC1 = true;
            }
            else if (strcmp(boardTypeJson->valuestring, "AC2") == 0)
            {
                config.AC2 = true;
            }
            else if (strcmp(boardTypeJson->valuestring, "VCharge1.4") == 0)
            {
                config.vcharge_lite_1_4 = true;
            }
            else if (strcmp(boardTypeJson->valuestring, "VChargeV5") == 0)
            {
                config.vcharge_v5 = true;
            }
        }
        if (cJSON_IsString(chargerTypeJson))
        {
            config.Ac3s = false;
            config.Ac7s = false;
            config.Ac11s = false;
            config.Ac22s = false;
            config.Ac44s = false;
            config.Ac14D = false;
            config.Ac10DP = false;
            config.Ac10DA = false;
            config.Ac6D = false;
            config.Ac10t = false;
            config.Ac14t = false;
            config.Ac18t = false;
            if (strcmp(chargerTypeJson->valuestring, "Ac3s") == 0)
            {
                config.Ac3s = true;
            }
            else if (strcmp(chargerTypeJson->valuestring, "Ac7s") == 0)
            {
                config.Ac7s = true;
            }
            else if (strcmp(chargerTypeJson->valuestring, "Ac11s") == 0)
            {
                config.Ac11s = true;
            }
            else if (strcmp(chargerTypeJson->valuestring, "Ac22s") == 0)
            {
                config.Ac22s = true;
            }
            else if (strcmp(chargerTypeJson->valuestring, "Ac44s") == 0)
            {
                config.Ac44s = true;
            }
            else if (strcmp(chargerTypeJson->valuestring, "Ac14D") == 0)
            {
                config.Ac14D = true;
            }
            else if (strcmp(chargerTypeJson->valuestring, "Ac10DP") == 0)
            {
                config.Ac10DP = true;
            }
            else if (strcmp(chargerTypeJson->valuestring, "Ac10DA") == 0)
            {
                config.Ac10DA = true;
            }
            else if (strcmp(chargerTypeJson->valuestring, "Ac6D") == 0)
            {
                config.Ac6D = true;
            }
            else if (strcmp(chargerTypeJson->valuestring, "Ac10t") == 0)
            {
                config.Ac10t = true;
            }
            else if (strcmp(chargerTypeJson->valuestring, "Ac14t") == 0)
            {
                config.Ac14t = true;
            }
            else if (strcmp(chargerTypeJson->valuestring, "Ac18t") == 0)
            {
                config.Ac18t = true;
            }
        }
        if (cJSON_IsString(connector1PWMJson))
        {
            if (strcmp(connector1PWMJson->valuestring, "63") == 0)
            {
                config.serverSetCpDuty[1] = 89;
            }
            else if (strcmp(connector1PWMJson->valuestring, "50") == 0)
            {
                config.serverSetCpDuty[1] = 83;
            }
            else if (strcmp(connector1PWMJson->valuestring, "40") == 0)
            {
                config.serverSetCpDuty[1] = 66;
            }
            else if (strcmp(connector1PWMJson->valuestring, "32") == 0)
            {
                config.serverSetCpDuty[1] = 53;
            }
            else if (strcmp(connector1PWMJson->valuestring, "25") == 0)
            {
                config.serverSetCpDuty[1] = 41;
            }
            else if (strcmp(connector1PWMJson->valuestring, "20") == 0)
            {
                config.serverSetCpDuty[1] = 33;
            }
            else if (strcmp(connector1PWMJson->valuestring, "16") == 0)
            {
                config.serverSetCpDuty[1] = 26;
            }
            else if (strcmp(connector1PWMJson->valuestring, "10") == 0)
            {
                config.serverSetCpDuty[1] = 16;
            }
            else
            {
                config.serverSetCpDuty[1] = 100;
            }
            if ((config.serverSetCpDuty[1] > 53) && (config.Ac7s || config.Ac22s || config.Ac14D || config.Ac10DP || config.Ac10DA || config.Ac14t || config.Ac18t))
            {
                config.serverSetCpDuty[1] = 53;
            }
            if ((config.serverSetCpDuty[1] > 26) && (config.Ac6D || config.Ac10t || config.Ac3s || config.Ac11s))
            {
                config.serverSetCpDuty[1] = 26;
            }
        }
        if (cJSON_IsString(connector2PWMJson))
        {
            if (strcmp(connector2PWMJson->valuestring, "63") == 0)
            {
                config.serverSetCpDuty[2] = 89;
            }
            else if (strcmp(connector2PWMJson->valuestring, "50") == 0)
            {
                config.serverSetCpDuty[2] = 83;
            }
            else if (strcmp(connector2PWMJson->valuestring, "40") == 0)
            {
                config.serverSetCpDuty[2] = 66;
            }
            else if (strcmp(connector2PWMJson->valuestring, "32") == 0)
            {
                config.serverSetCpDuty[2] = 53;
            }
            else if (strcmp(connector2PWMJson->valuestring, "25") == 0)
            {
                config.serverSetCpDuty[2] = 41;
            }
            else if (strcmp(connector2PWMJson->valuestring, "20") == 0)
            {
                config.serverSetCpDuty[2] = 33;
            }
            else if (strcmp(connector2PWMJson->valuestring, "16") == 0)
            {
                config.serverSetCpDuty[2] = 26;
            }
            else if (strcmp(connector2PWMJson->valuestring, "10") == 0)
            {
                config.serverSetCpDuty[2] = 16;
            }
            else
            {
                config.serverSetCpDuty[2] = 100;
            }
            if ((config.serverSetCpDuty[2] > 53) && (config.Ac14D || config.Ac18t))
            {
                config.serverSetCpDuty[2] = 53;
            }
            if ((config.serverSetCpDuty[2] > 26) && (config.Ac6D || config.Ac10t || config.Ac14t || config.Ac10DP || config.Ac10DA))
            {
                config.serverSetCpDuty[2] = 26;
            }
        }
        if (cJSON_IsString(connector3PWMJson))
        {
            if (strcmp(connector3PWMJson->valuestring, "63") == 0)
            {
                config.serverSetCpDuty[3] = 89;
            }
            else if (strcmp(connector3PWMJson->valuestring, "50") == 0)
            {
                config.serverSetCpDuty[3] = 83;
            }
            else if (strcmp(connector3PWMJson->valuestring, "40") == 0)
            {
                config.serverSetCpDuty[3] = 66;
            }
            else if (strcmp(connector3PWMJson->valuestring, "32") == 0)
            {
                config.serverSetCpDuty[3] = 53;
            }
            else if (strcmp(connector3PWMJson->valuestring, "25") == 0)
            {
                config.serverSetCpDuty[3] = 41;
            }
            else if (strcmp(connector3PWMJson->valuestring, "20") == 0)
            {
                config.serverSetCpDuty[3] = 33;
            }
            else if (strcmp(connector3PWMJson->valuestring, "16") == 0)
            {
                config.serverSetCpDuty[3] = 26;
            }
            else if (strcmp(connector3PWMJson->valuestring, "10") == 0)
            {
                config.serverSetCpDuty[3] = 16;
            }
            else
            {
                config.serverSetCpDuty[3] = 100;
            }

            if ((config.serverSetCpDuty[3] > 26) && (config.Ac10t || config.Ac14t || config.Ac18t))
            {
                config.serverSetCpDuty[3] = 26;
            }
        }

        if (cJSON_IsString(chargerDisplayTypeJson))
        {
            if (strcmp(chargerDisplayTypeJson->valuestring, "LED") == 0)
            {
                config.displayType = 1;
            }
            else if (strcmp(chargerDisplayTypeJson->valuestring, "20x4") == 0)
            {
                config.displayType = 2;
            }
            else if (strcmp(chargerDisplayTypeJson->valuestring, "DWIN") == 0)
            {
                config.displayType = 3;
            }
        }
        if (cJSON_IsString(DisplayNumJson))
        {
            if (strcmp(DisplayNumJson->valuestring, "1") == 0)
            {
                config.NumofDisplays = 1;
            }
            else if (strcmp(DisplayNumJson->valuestring, "2") == 0)
            {
                config.NumofDisplays = 2;
            }
            else if (strcmp(DisplayNumJson->valuestring, "3") == 0)
            {
                config.NumofDisplays = 3;
            }
        }
        if (cJSON_IsString(commissionedByJson))
        {
            setNULL(config.commissionedBy);
            memcpy(config.commissionedBy, commissionedByJson->valuestring, strlen(commissionedByJson->valuestring));
        }
        if (cJSON_IsString(commissionedDateJson))
        {
            setNULL(config.commissionedDate);
            memcpy(config.commissionedDate, commissionedDateJson->valuestring, strlen(commissionedDateJson->valuestring));
        }
        if (cJSON_IsString(webSocketURLJson))
        {
            if (memcmp(webSocketURLJson->valuestring, config.webSocketURL, strlen(config.webSocketURL)) != 0)
            {
                clearSendLocalList();
                clearLocalAuthenticatinCache();
            }
            setNULL(config.webSocketURL);
            memcpy(config.webSocketURL, webSocketURLJson->valuestring, strlen(webSocketURLJson->valuestring));
        }
        if (cJSON_IsString(wifiStatusJson))
        {
            config.wifiEnable = false;
            if (strcmp(wifiStatusJson->valuestring, "Enable") == 0)
            {
                config.wifiEnable = true;
            }
            else if (strcmp(wifiStatusJson->valuestring, "Disable") == 0)
            {
                config.wifiEnable = false;
            }
        }
        if (cJSON_IsString(wifiPriorityJson))
        {
            config.wifiPriority = 1;
            if (strcmp(wifiPriorityJson->valuestring, "1") == 0)
            {
                config.wifiPriority = 3;
            }
            else if (strcmp(wifiPriorityJson->valuestring, "2") == 0)
            {
                config.wifiPriority = 2;
            }
            else if (strcmp(wifiPriorityJson->valuestring, "3") == 0)
            {
                config.wifiPriority = 1;
            }
        }
        if (cJSON_IsString(wifiSSIDJson))
        {
            setNULL(config.wifiSSID);
            memcpy(config.wifiSSID, wifiSSIDJson->valuestring, strlen(wifiSSIDJson->valuestring));
        }
        if (cJSON_IsString(wifiPasswordJson))
        {
            setNULL(config.wifiPassword);
            memcpy(config.wifiPassword, wifiPasswordJson->valuestring, strlen(wifiPasswordJson->valuestring));
        }
        if (cJSON_IsString(gsmStatusJson))
        {
            config.gsmEnable = false;
            if (strcmp(gsmStatusJson->valuestring, "Enable") == 0)
            {
                config.gsmEnable = true;
            }
            else if (strcmp(gsmStatusJson->valuestring, "Disable") == 0)
            {
                config.gsmEnable = false;
            }
        }
        if (cJSON_IsString(gsmPriorityJson))
        {
            config.gsmPriority = 2;
            if (strcmp(gsmPriorityJson->valuestring, "1") == 0)
            {
                config.gsmPriority = 3;
            }
            else if (strcmp(gsmPriorityJson->valuestring, "2") == 0)
            {
                config.gsmPriority = 2;
            }
            else if (strcmp(gsmPriorityJson->valuestring, "3") == 0)
            {
                config.gsmPriority = 1;
            }
        }
        if (cJSON_IsString(gsmAPNJson))
        {
            setNULL(config.gsmAPN);
            memcpy(config.gsmAPN, gsmAPNJson->valuestring, strlen(gsmAPNJson->valuestring));
        }
        if (cJSON_IsString(ethernetStatusJson))
        {
            config.ethernetEnable = false;
            if (strcmp(ethernetStatusJson->valuestring, "Enable") == 0)
            {
                config.ethernetEnable = true;
            }
            else if (strcmp(ethernetStatusJson->valuestring, "Disable") == 0)
            {
                config.ethernetEnable = false;
            }
        }
        if (cJSON_IsString(ethernetPriorityJson))
        {
            config.ethernetPriority = 3;
            if (strcmp(ethernetPriorityJson->valuestring, "1") == 0)
            {
                config.ethernetPriority = 3;
            }
            else if (strcmp(ethernetPriorityJson->valuestring, "2") == 0)
            {
                config.ethernetPriority = 2;
            }
            else if (strcmp(ethernetPriorityJson->valuestring, "3") == 0)
            {
                config.ethernetPriority = 1;
            }
        }
        if (cJSON_IsString(ethernetConfigJson))
        {
            config.ethernetConfig = false;
            if (strcmp(ethernetConfigJson->valuestring, "Static") == 0)
            {
                config.ethernetConfig = true;
            }
            else if (strcmp(ethernetConfigJson->valuestring, "Dynamic") == 0)
            {
                config.ethernetConfig = false;
            }
        }
        if (cJSON_IsString(ipAddressJson))
        {
            setNULL(config.ipAddress);
            memcpy(config.ipAddress, ipAddressJson->valuestring, strlen(ipAddressJson->valuestring));
        }
        if (cJSON_IsString(gatewayAddressJson))
        {
            setNULL(config.gatewayAddress);
            memcpy(config.gatewayAddress, gatewayAddressJson->valuestring, strlen(gatewayAddressJson->valuestring));
        }
        if (cJSON_IsString(dnsAddressJson))
        {
            setNULL(config.dnsAddress);
            memcpy(config.dnsAddress, dnsAddressJson->valuestring, strlen(dnsAddressJson->valuestring));
        }
        if (cJSON_IsString(subnetMaskJson))
        {
            setNULL(config.subnetMask);
            memcpy(config.subnetMask, subnetMaskJson->valuestring, strlen(subnetMaskJson->valuestring));
        }
        if (cJSON_IsString(OnlineOfflineStatusJson))
        {
            config.onlineOffline = false;
            config.online = false;
            config.offline = false;
            config.plugnplay = false;
            if (strcmp(OnlineOfflineStatusJson->valuestring, "onlineOffline") == 0)
            {
                config.onlineOffline = true;
            }
            else if (strcmp(OnlineOfflineStatusJson->valuestring, "online") == 0)
            {
                config.online = true;
            }
            else if (strcmp(OnlineOfflineStatusJson->valuestring, "offline") == 0)
            {
                config.offline = true;
            }
            else
            {
                config.offline = true;
                config.plugnplay = true;
            }
        }
        if (cJSON_IsString(gfciJson))
        {
            config.gfci = false;
            if (strcmp(gfciJson->valuestring, "Yes") == 0)
            {
                config.gfci = true;
            }
            else if (strcmp(gfciJson->valuestring, "No") == 0)
            {
                config.gfci = false;
            }
        }

        if (cJSON_IsString(lowCurrentTimeJson))
        {
            config.lowCurrentTime = 30;
            sscanf(lowCurrentTimeJson->valuestring, "%hd", &config.lowCurrentTime);
        }
        if (cJSON_IsString(minLowCurrentThresholdJson))
        {
            config.minLowCurrentThreshold = 0.1;
            sscanf(minLowCurrentThresholdJson->valuestring, "%lf", &config.minLowCurrentThreshold);
        }
        if (cJSON_IsString(resumeSessionJson))
        {
            config.resumeSession = false;
            if (strcmp(resumeSessionJson->valuestring, "Yes") == 0)
            {
                config.resumeSession = true;
            }
            else if (strcmp(resumeSessionJson->valuestring, "No") == 0)
            {
                config.resumeSession = false;
            }
        }
        if (cJSON_IsString(overVoltageThresholdJson))
        {
            config.overVoltageThreshold = 270;
            sscanf(overVoltageThresholdJson->valuestring, "%lf", &config.overVoltageThreshold);
        }
        if (cJSON_IsString(underVoltageThresholdJson))
        {
            config.underVoltageThreshold = 110;
            sscanf(underVoltageThresholdJson->valuestring, "%lf", &config.underVoltageThreshold);
        }
        if (cJSON_IsString(con1overCurrentThresholdJson))
        {
            config.overCurrentThreshold[1] = 20.0;
            sscanf(con1overCurrentThresholdJson->valuestring, "%lf", &config.overCurrentThreshold[1]);
        }
        if (cJSON_IsString(con2overCurrentThresholdJson))
        {
            config.overCurrentThreshold[2] = 20.0;
            sscanf(con2overCurrentThresholdJson->valuestring, "%lf", &config.overCurrentThreshold[2]);
        }
        if (cJSON_IsString(con3overCurrentThresholdJson))
        {
            config.overCurrentThreshold[3] = 20.0;
            sscanf(con3overCurrentThresholdJson->valuestring, "%lf", &config.overCurrentThreshold[3]);
        }
        if (cJSON_IsString(overTemperatureThresholdJson))
        {
            config.overTemperatureThreshold = 50;
            sscanf(overTemperatureThresholdJson->valuestring, "%lf", &config.overTemperatureThreshold);
        }
        if (cJSON_IsString(restoreSessionJson))
        {
            config.restoreSession = false;
            if (strcmp(restoreSessionJson->valuestring, "Yes") == 0)
            {
                config.restoreSession = true;
            }
            else if (strcmp(restoreSessionJson->valuestring, "No") == 0)
            {
                config.restoreSession = false;
            }
        }
        if (cJSON_IsString(sessionRestoreTimeJson))
        {
            config.sessionRestoreTime = 120;
            sscanf(sessionRestoreTimeJson->valuestring, "%hd", &config.sessionRestoreTime);
        }
        if (cJSON_IsString(StopTransactionInSuspendedStateStatusJson))
        {
            config.StopTransactionInSuspendedState = false;
            if (strcmp(StopTransactionInSuspendedStateStatusJson->valuestring, "Enable") == 0)
            {
                config.StopTransactionInSuspendedState = true;
            }
            else if (strcmp(StopTransactionInSuspendedStateStatusJson->valuestring, "Disable") == 0)
            {
                config.StopTransactionInSuspendedState = false;
            }
        }
        if (cJSON_IsString(StopTransactionInSuspendedStateTimeJson))
        {
            config.StopTransactionInSuspendedStateTime = 120;
            sscanf(StopTransactionInSuspendedStateTimeJson->valuestring, "%hd", &config.StopTransactionInSuspendedStateTime);
        }
        if (cJSON_IsString(smartChargingStatusJson))
        {
            config.smartCharging = false;
            if (strcmp(smartChargingStatusJson->valuestring, "Enable") == 0)
            {
                config.smartCharging = true;
            }
            else if (strcmp(smartChargingStatusJson->valuestring, "Disable") == 0)
            {
                config.smartCharging = false;
            }
        }
        if (cJSON_IsString(batteryBackupStatusJson))
        {
            config.BatteryBackup = false;
            if (strcmp(batteryBackupStatusJson->valuestring, "Enable") == 0)
            {
                config.BatteryBackup = true;
            }
            else if (strcmp(batteryBackupStatusJson->valuestring, "Disable") == 0)
            {
                config.BatteryBackup = false;
            }
        }
        if (cJSON_IsString(diagnosticServerStatusJson))
        {
            config.DiagnosticServer = false;
            if (strcmp(diagnosticServerStatusJson->valuestring, "Enable") == 0)
            {
                config.DiagnosticServer = true;
            }
            else if (strcmp(diagnosticServerStatusJson->valuestring, "Disable") == 0)
            {
                config.DiagnosticServer = false;
            }
        }
        if (cJSON_IsString(alprDeviceStatusJson))
        {
            config.ALPR = false;
            if (strcmp(alprDeviceStatusJson->valuestring, "Enable") == 0)
            {
                config.ALPR = true;
            }
            else if (strcmp(alprDeviceStatusJson->valuestring, "Disable") == 0)
            {
                config.ALPR = false;
            }
        }
        if (cJSON_IsString(clearAuthCacheJson))
        {
            config.clearAuthCache = false;
            if (strcmp(clearAuthCacheJson->valuestring, "Yes") == 0)
            {
                config.clearAuthCache = true;
            }
            else if (strcmp(clearAuthCacheJson->valuestring, "No") == 0)
            {
                config.clearAuthCache = false;
            }
        }
        if (cJSON_IsString(factoryResetJson))
        {
            config.factoryReset = false;
            if (strcmp(factoryResetJson->valuestring, "Yes") == 0)
            {
                config.factoryReset = true;
            }
            else if (strcmp(factoryResetJson->valuestring, "No") == 0)
            {
                config.factoryReset = false;
            }
        }
        if (cJSON_IsString(otaURLConfigJson))
        {
            setNULL(config.otaURLConfig);
            memcpy(config.otaURLConfig, otaURLConfigJson->valuestring, strlen(otaURLConfigJson->valuestring));
        }
        if (cJSON_IsString(localListTagsJson))
        {
            // setNULL(config.localListTags);
            // memcpy(config.localListTags, localListTagsJson->valuestring, strlen(localListTagsJson->valuestring));
        }
        if (cJSON_IsString(adminpasswordJson))
        {
            setNULL(config.adminpassword);
            memcpy(config.adminpassword, adminpasswordJson->valuestring, strlen(adminpasswordJson->valuestring));
        }
        if (cJSON_IsString(factorypasswordJson))
        {
            setNULL(config.factorypassword);
            memcpy(config.factorypassword, factorypasswordJson->valuestring, strlen(factorypasswordJson->valuestring));
        }
        if (cJSON_IsString(servicepasswordJson))
        {
            setNULL(config.servicepassword);
            memcpy(config.servicepassword, servicepasswordJson->valuestring, strlen(servicepasswordJson->valuestring));
        }
        if (cJSON_IsString(customerpasswordJson))
        {
            setNULL(config.customerpassword);
            memcpy(config.customerpassword, customerpasswordJson->valuestring, strlen(customerpasswordJson->valuestring));
        }

        config.defaultConfig = true;
        vTaskDelay(100 / portTICK_PERIOD_MS);

        custom_nvs_write_config();
        ESP_LOGI(TAG, "Config updated successfully");
    }

    cJSON_Delete(Payload);
    return ESP_OK;
}

static esp_err_t Get_Config(uint8_t url)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        printf("Failed to create cJSON object\n");
        return EXIT_FAILURE;
    }

    cJSON_AddStringToObject(root, "serialNumber", config.serialNumber);
    cJSON_AddStringToObject(root, "chargerName", config.chargerName);
    cJSON_AddStringToObject(root, "chargePointVendor", config.chargePointVendor);
    cJSON_AddStringToObject(root, "chargePointModel", config.chargePointModel);

    if (config.AC1)
    {
        cJSON_AddStringToObject(root, "boardType", "AC1");
    }
    else if (config.AC2)
    {
        cJSON_AddStringToObject(root, "boardType", "AC2");
    }
    else if (config.vcharge_lite_1_4)
    {
        cJSON_AddStringToObject(root, "boardType", "VCharge1.4");
    }
    else if (config.vcharge_v5)
    {
        cJSON_AddStringToObject(root, "boardType", "VChargeV5");
    }
    else
    {
        cJSON_AddStringToObject(root, "boardType", "NOT Defined");
    }

    if (config.Ac3s)
    {
        cJSON_AddStringToObject(root, "chargerType", "Ac3s");
    }
    else if (config.Ac7s)
    {
        cJSON_AddStringToObject(root, "chargerType", "Ac7s");
    }
    else if (config.Ac11s)
    {
        cJSON_AddStringToObject(root, "chargerType", "Ac11s");
    }
    else if (config.Ac22s)
    {
        cJSON_AddStringToObject(root, "chargerType", "Ac22s");
    }
    else if (config.Ac44s)
    {
        cJSON_AddStringToObject(root, "chargerType", "Ac44s");
    }
    else if (config.Ac14D)
    {
        cJSON_AddStringToObject(root, "chargerType", "Ac14D");
    }
    else if (config.Ac10DP)
    {
        cJSON_AddStringToObject(root, "chargerType", "Ac10DP");
    }
    else if (config.Ac10DA)
    {
        cJSON_AddStringToObject(root, "chargerType", "Ac10DA");
    }
    else if (config.Ac6D)
    {
        cJSON_AddStringToObject(root, "chargerType", "Ac6D");
    }
    else if (config.Ac10t)
    {
        cJSON_AddStringToObject(root, "chargerType", "Ac10t");
    }
    else if (config.Ac14t)
    {
        cJSON_AddStringToObject(root, "chargerType", "Ac14t");
    }
    else if (config.Ac18t)
    {
        cJSON_AddStringToObject(root, "chargerType", "Ac18t");
    }
    else
    {
        cJSON_AddStringToObject(root, "chargerType", "Not Defined");
    }

    if (config.serverSetCpDuty[1] == 89)
    {
        cJSON_AddStringToObject(root, "connector1PWM", "63");
    }
    else if (config.serverSetCpDuty[1] == 83)
    {
        cJSON_AddStringToObject(root, "connector1PWM", "50");
    }
    else if (config.serverSetCpDuty[1] == 66)
    {
        cJSON_AddStringToObject(root, "connector1PWM", "40");
    }
    else if (config.serverSetCpDuty[1] == 53)
    {
        cJSON_AddStringToObject(root, "connector1PWM", "32");
    }
    else if (config.serverSetCpDuty[1] == 41)
    {
        cJSON_AddStringToObject(root, "connector1PWM", "25");
    }
    else if (config.serverSetCpDuty[1] == 33)
    {
        cJSON_AddStringToObject(root, "connector1PWM", "20");
    }
    else if (config.serverSetCpDuty[1] == 26)
    {
        cJSON_AddStringToObject(root, "connector1PWM", "16");
    }
    else if (config.serverSetCpDuty[1] == 16)
    {
        cJSON_AddStringToObject(root, "connector1PWM", "10");
    }
    else
    {
        cJSON_AddStringToObject(root, "connector1PWM", "10");
    }

    if (config.serverSetCpDuty[2] == 89)
    {
        cJSON_AddStringToObject(root, "connector2PWM", "63");
    }
    else if (config.serverSetCpDuty[2] == 83)
    {
        cJSON_AddStringToObject(root, "connector2PWM", "50");
    }
    else if (config.serverSetCpDuty[2] == 66)
    {
        cJSON_AddStringToObject(root, "connector2PWM", "40");
    }
    else if (config.serverSetCpDuty[2] == 53)
    {
        cJSON_AddStringToObject(root, "connector2PWM", "32");
    }
    else if (config.serverSetCpDuty[2] == 41)
    {
        cJSON_AddStringToObject(root, "connector2PWM", "25");
    }
    else if (config.serverSetCpDuty[2] == 33)
    {
        cJSON_AddStringToObject(root, "connector2PWM", "20");
    }
    else if (config.serverSetCpDuty[2] == 26)
    {
        cJSON_AddStringToObject(root, "connector2PWM", "16");
    }
    else if (config.serverSetCpDuty[2] == 16)
    {
        cJSON_AddStringToObject(root, "connector2PWM", "10");
    }
    else
    {
        cJSON_AddStringToObject(root, "connector2PWM", "10");
    }

    if (config.serverSetCpDuty[3] == 89)
    {
        cJSON_AddStringToObject(root, "connector3PWM", "63");
    }
    else if (config.serverSetCpDuty[3] == 83)
    {
        cJSON_AddStringToObject(root, "connector3PWM", "50");
    }
    else if (config.serverSetCpDuty[3] == 66)
    {
        cJSON_AddStringToObject(root, "connector3PWM", "40");
    }
    else if (config.serverSetCpDuty[3] == 53)
    {
        cJSON_AddStringToObject(root, "connector3PWM", "32");
    }
    else if (config.serverSetCpDuty[3] == 41)
    {
        cJSON_AddStringToObject(root, "connector3PWM", "25");
    }
    else if (config.serverSetCpDuty[3] == 33)
    {
        cJSON_AddStringToObject(root, "connector3PWM", "20");
    }
    else if (config.serverSetCpDuty[3] == 26)
    {
        cJSON_AddStringToObject(root, "connector3PWM", "16");
    }
    else if (config.serverSetCpDuty[3] == 16)
    {
        cJSON_AddStringToObject(root, "connector3PWM", "10");
    }
    else
    {
        cJSON_AddStringToObject(root, "connector3PWM", "10");
    }

    if (config.displayType == 1)
    {
        cJSON_AddStringToObject(root, "chargerDisplayType", "LED");
    }
    else if (config.displayType == 2)
    {
        cJSON_AddStringToObject(root, "chargerDisplayType", "20x4");
    }
    else if (config.displayType == 3)
    {
        cJSON_AddStringToObject(root, "chargerDisplayType", "DWIN");
    }
    else
    {
        cJSON_AddStringToObject(root, "chargerDisplayType", "LED");
    }

    if (config.NumofDisplays == 1)
    {
        cJSON_AddStringToObject(root, "DisplayNum", "1");
    }
    else if (config.NumofDisplays == 2)
    {
        cJSON_AddStringToObject(root, "DisplayNum", "2");
    }
    else if (config.NumofDisplays == 3)
    {
        cJSON_AddStringToObject(root, "DisplayNum", "3");
    }
    else
    {
        cJSON_AddStringToObject(root, "DisplayNum", "1");
    }
    cJSON_AddStringToObject(root, "commissionedBy", config.commissionedBy);
    cJSON_AddStringToObject(root, "commissionedDate", config.commissionedDate);
    cJSON_AddStringToObject(root, "firmwareversion", config.firmwareVersion);
    cJSON_AddStringToObject(root, "slavefirmwareversion", config.slavefirmwareVersion);
    cJSON_AddStringToObject(root, "webSocketURL", config.webSocketURL);
    if (config.wifiEnable)
    {
        cJSON_AddStringToObject(root, "wifiStatus", "Enable");
    }
    else
    {
        cJSON_AddStringToObject(root, "wifiStatus", "Disable");
    }
    if (config.wifiPriority == 1)
    {
        cJSON_AddStringToObject(root, "wifiPriority", "3");
    }
    else if (config.wifiPriority == 2)
    {
        cJSON_AddStringToObject(root, "wifiPriority", "2");
    }
    else if (config.wifiPriority == 3)
    {
        cJSON_AddStringToObject(root, "wifiPriority", "1");
    }
    else
    {
        cJSON_AddStringToObject(root, "wifiPriority", "Not Defined");
    }
    cJSON_AddStringToObject(root, "wifiSSID", config.wifiSSID);
    cJSON_AddStringToObject(root, "wifiPassword", config.wifiPassword);
    if (config.gsmEnable)
    {
        cJSON_AddStringToObject(root, "gsmStatus", "Enable");
    }
    else
    {
        cJSON_AddStringToObject(root, "gsmStatus", "Disable");
    }
    if (config.gsmPriority == 1)
    {
        cJSON_AddStringToObject(root, "gsmPriority", "3");
    }
    else if (config.gsmPriority == 2)
    {
        cJSON_AddStringToObject(root, "gsmPriority", "2");
    }
    else if (config.gsmPriority == 3)
    {
        cJSON_AddStringToObject(root, "gsmPriority", "1");
    }
    else
    {
        cJSON_AddStringToObject(root, "gsmPriority", "Not Defined");
    }
    cJSON_AddStringToObject(root, "gsmAPN", config.gsmAPN);
    cJSON_AddStringToObject(root, "simIMEINumber", config.simIMEINumber);
    if (config.ethernetEnable)
    {
        cJSON_AddStringToObject(root, "ethernetStatus", "Enable");
    }
    else
    {
        cJSON_AddStringToObject(root, "ethernetStatus", "Disable");
    }
    if (config.ethernetPriority == 1)
    {
        cJSON_AddStringToObject(root, "ethernetPriority", "3");
    }
    else if (config.ethernetPriority == 2)
    {
        cJSON_AddStringToObject(root, "ethernetPriority", "2");
    }
    else if (config.ethernetPriority == 3)
    {
        cJSON_AddStringToObject(root, "ethernetPriority", "1");
    }
    else
    {
        cJSON_AddStringToObject(root, "ethernetPriority", "Not Defined");
    }
    if (config.ethernetConfig)
    {
        cJSON_AddStringToObject(root, "ethernetConfig", "Static");
    }
    else
    {
        cJSON_AddStringToObject(root, "ethernetConfig", "Dynamic");
    }
    cJSON_AddStringToObject(root, "ipAddress", config.ipAddress);
    cJSON_AddStringToObject(root, "gatewayAddress", config.gatewayAddress);
    cJSON_AddStringToObject(root, "dnsAddress", config.dnsAddress);
    cJSON_AddStringToObject(root, "subnetMask", config.subnetMask);
    if (config.onlineOffline)
    {
        cJSON_AddStringToObject(root, "OnlineOfflineStatus", "onlineOffline");
        // cJSON_AddStringToObject(root, "offline", "Yes");
    }
    else if (config.online)
    {
        cJSON_AddStringToObject(root, "OnlineOfflineStatus", "online");
        // cJSON_AddStringToObject(root, "offline", "No");
    }
    else if (config.offline && (config.plugnplay == false))
    {
        cJSON_AddStringToObject(root, "OnlineOfflineStatus", "offline");
        // cJSON_AddStringToObject(root, "offline", "Yes");
    }
    else
    {
        cJSON_AddStringToObject(root, "OnlineOfflineStatus", "plugnplay");
    }

    if (config.gfci)
    {
        cJSON_AddStringToObject(root, "gfci", "Yes");
    }
    else
    {
        cJSON_AddStringToObject(root, "gfci", "No");
    }

    char lowCurrentTime_str[16];
    setNULL(lowCurrentTime_str);
    sprintf(lowCurrentTime_str, "%d", config.lowCurrentTime);
    cJSON_AddStringToObject(root, "lowCurrentTime", lowCurrentTime_str);
    char minLowCurrentThreshold_str[16];
    setNULL(minLowCurrentThreshold_str);
    sprintf(minLowCurrentThreshold_str, "%lf", config.minLowCurrentThreshold);
    cJSON_AddStringToObject(root, "minLowCurrentThreshold", minLowCurrentThreshold_str);

    if (config.resumeSession)
    {
        cJSON_AddStringToObject(root, "resumeSession", "Yes");
    }
    else
    {
        cJSON_AddStringToObject(root, "resumeSession", "No");
    }
    char overVoltageThreshold_str[16];
    setNULL(overVoltageThreshold_str);
    sprintf(overVoltageThreshold_str, "%.2f", config.overVoltageThreshold);
    cJSON_AddStringToObject(root, "overVoltageThreshold", overVoltageThreshold_str);
    char underVoltageThreshold_str[16];
    setNULL(underVoltageThreshold_str);
    sprintf(underVoltageThreshold_str, "%.2f", config.underVoltageThreshold);
    cJSON_AddStringToObject(root, "underVoltageThreshold", underVoltageThreshold_str);
    char con1overCurrentThreshold_str[16];
    setNULL(con1overCurrentThreshold_str);
    sprintf(con1overCurrentThreshold_str, "%.2f", config.overCurrentThreshold[1]);
    cJSON_AddStringToObject(root, "con1overCurrentThreshold", con1overCurrentThreshold_str);
    char con2overCurrentThreshold_str[16];
    setNULL(con2overCurrentThreshold_str);
    sprintf(con2overCurrentThreshold_str, "%.2f", config.overCurrentThreshold[2]);
    cJSON_AddStringToObject(root, "con2overCurrentThreshold", con2overCurrentThreshold_str);
    char con3overCurrentThreshold_str[16];
    setNULL(con3overCurrentThreshold_str);
    sprintf(con3overCurrentThreshold_str, "%.2f", config.overCurrentThreshold[3]);
    cJSON_AddStringToObject(root, "con3overCurrentThreshold", con3overCurrentThreshold_str);
    char overTemperatureThreshold_str[16];
    setNULL(overTemperatureThreshold_str);
    sprintf(overTemperatureThreshold_str, "%.2f", config.overTemperatureThreshold);
    cJSON_AddStringToObject(root, "overTemperatureThreshold", overTemperatureThreshold_str);
    if (config.restoreSession)
    {
        cJSON_AddStringToObject(root, "restoreSession", "Yes");
    }
    else
    {
        cJSON_AddStringToObject(root, "restoreSession", "No");
    }
    char sessionRestoreTime_str[16];
    setNULL(sessionRestoreTime_str);
    sprintf(sessionRestoreTime_str, "%d", config.sessionRestoreTime);
    cJSON_AddStringToObject(root, "sessionRestoreTime", sessionRestoreTime_str);
    if (config.StopTransactionInSuspendedState)
    {
        cJSON_AddStringToObject(root, "StopTransactionInSuspendedStateStatus", "Enable");
    }
    else
    {
        cJSON_AddStringToObject(root, "StopTransactionInSuspendedStateStatus", "Disable");
    }

    char StopTransactionInSuspendedStateTime_str[16];
    setNULL(StopTransactionInSuspendedStateTime_str);
    sprintf(StopTransactionInSuspendedStateTime_str, "%d", config.StopTransactionInSuspendedStateTime);
    cJSON_AddStringToObject(root, "StopTransactionInSuspendedStateTime", StopTransactionInSuspendedStateTime_str);
    if (config.smartCharging)
    {
        cJSON_AddStringToObject(root, "smartChargingStatus", "Enable");
    }
    else
    {
        cJSON_AddStringToObject(root, "smartChargingStatus", "Disable");
    }
    if (config.BatteryBackup)
    {
        cJSON_AddStringToObject(root, "batteryBackupStatus", "Enable");
    }
    else
    {
        cJSON_AddStringToObject(root, "batteryBackupStatus", "Disable");
    }
    if (config.DiagnosticServer)
    {
        cJSON_AddStringToObject(root, "diagnosticServerStatus", "Enable");
    }
    else
    {
        cJSON_AddStringToObject(root, "diagnosticServerStatus", "Disable");
    }
    if (config.ALPR)
    {
        cJSON_AddStringToObject(root, "alprDeviceStatus", "Enable");
    }
    else
    {
        cJSON_AddStringToObject(root, "alprDeviceStatus", "Disable");
    }

    if (config.clearAuthCache)
    {
        cJSON_AddStringToObject(root, "clearAuthCache", "Yes");
    }
    else
    {
        cJSON_AddStringToObject(root, "clearAuthCache", "No");
    }
    if (config.factoryReset)
    {
        cJSON_AddStringToObject(root, "factoryReset", "Yes");
    }
    else
    {
        cJSON_AddStringToObject(root, "factoryReset", "No");
    }
    cJSON_AddStringToObject(root, "otaURLConfig", config.otaURLConfig);
    cJSON_AddStringToObject(root, "localListTags", "NULL");
    cJSON_AddStringToObject(root, "adminpassword", config.adminpassword);
    cJSON_AddStringToObject(root, "factorypassword", config.factorypassword);
    cJSON_AddStringToObject(root, "servicepassword", config.servicepassword);
    cJSON_AddStringToObject(root, "customerpassword", config.customerpassword);

    char *jsonString = cJSON_Print(root);
    if (jsonString == NULL)
    {
        printf("Failed to convert cJSON object to string\n");
        cJSON_Delete(root);
        return EXIT_FAILURE;
    }
    memset(&buffer, '\0', sizeof(buffer));
    memcpy(buffer, jsonString, strlen(jsonString));
    cJSON_Delete(root);
    free(jsonString);
    return ESP_OK;
}

static esp_err_t Url_Config(uint8_t url, httpd_req_t *req)
{
    bool isChargerConfiguringFirstTime = false;
    if (memcmp(config.chargerName, "Amplify", strlen(config.chargerName)) == 0)
    {
        isChargerConfiguringFirstTime = true;
    }

    char *htmlString =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "\t<head>\n"
        "\t\t<title>Login Page</title>\n"
        "\t\t<style>\n"
        "\t\t\tbody {\n"
        "\t\t\tfont-family: Arial, sans-serif;\n"
        "\t\t\tbackground-color: #87cefa;\n"
        "\t\t\tmargin: 0;\n"
        "\t\t\tpadding: 20px;\n"
        "\t\t\theight: 100vh;\n"
        "\t\t\tjustify-content: center;\n"
        "\t\t\talign-items: center;\n"
        "\t\t\tflex-direction: column;\n"
        "\t\t\t}\n"
        "\t\t\th1 {\n"
        "\t\t\tbackground-color: green;\n"
        "\t\t\tcolor: white;\n"
        "\t\t\tpadding: 10px;\n"
        "\t\t\tborder-radius: 8px;\n"
        "\t\t\ttext-align: center;\n"
        "\t\t\twidth: 100%;\n"
        "\t\t\tbox-sizing: border-box;\n"
        "\t\t\t}\n"
        "\t\t\th2 {\n"
        "\t\t\tcolor: #333;\n"
        "\t\t\ttext-align: center;\n"
        "\t\t\t}\n"
        "\t\t\t.form-container {\n"
        "\t\t\tbackground-color: #fff;\n"
        "\t\t\tpadding: 20px;\n"
        "\t\t\tborder-radius: 8px;\n"
        "\t\t\tbox-shadow: 0 0 10px rgba(0, 0, 0, 0.1);\n"
        "\t\t\t}\n"
        "\t\t\t.form-group {\n"
        "\t\t\tflex-direction: column;\n"
        "\t\t\tmargin-bottom: 15px;\n"
        "\t\t\t}\n"
        "\t\t\tlabel {\n"
        "\t\t\tmargin-bottom: 5px;\n"
        "\t\t\tfont-weight: bold;\n"
        "\t\t\t}\n"
        "\t\t\tinput[type=\"text\"], input[type=\"password\"], select {\n"
        "\t\t\tpadding: 10px;\n"
        "\t\t\tborder: 1px solid #ccc;\n"
        "\t\t\tborder-radius: 4px;\n"
        "\t\t\twidth: 100%;\n"
        "\t\t\tbox-sizing: border-box;\n"
        "\t\t\t}\n"
        "\t\t\tbutton {\n"
        "\t\t\tpadding: 10px 20px;\n"
        "\t\t\tbackground-color: green;\n"
        "\t\t\tcolor: white;\n"
        "\t\t\tborder: none;\n"
        "\t\t\tborder-radius: 4px;\n"
        "\t\t\tcursor: pointer;\n"
        "\t\t\tfont-size: 16px;\n"
        "\t\t\t}\n"
        "\t\t\tbutton:hover {\n"
        "\t\t\tbackground-color: darkgreen;\n"
        "\t\t\t}\n"
        "\t\t</style>\n"
        "\t\t</head>\n"
        "\t<body>\n"
        "\t\t<h1>EVRE</h1>\n"
        "\t\t<div class=\"form-container\">\n"
        "<h2>Charger Configuration</h2>\n"
        "\t\t\t\t<div class=\"form-group\">\n"

        "<label for=\"serialNumber\">Charger Box Serial Number:</label>\n"
        "<input type=\"text\" id=\"serialNumber\"><br><br>\n"

        "<label for=\"chargerName\">Charger Name:</label>\n"
        "<input type=\"text\" id=\"chargerName\"><br><br>\n"

        "<label for=\"chargePointVendor\">Charge Point Vendor:</label>\n"
        "<input type=\"text\" id=\"chargePointVendor\"><br><br>\n"

        "<label for=\"chargePointModel\">Charge Point Model:</label>\n"
        "<input type=\"text\" id=\"chargePointModel\"><br><br>\n"

        "<label for=\"boardType\">Board Type:</label>\n"
        "<select id=\"boardType\"";
    httpd_resp_send_chunk(req, htmlString, strlen(htmlString));
    if (isChargerConfiguringFirstTime == false)
    {
        htmlString = " disabled";
        httpd_resp_send_chunk(req, htmlString, strlen(htmlString));
    }
    htmlString =
        ">\n"
        "<option value=\"AC1\">AC1</option>\n"
        "<option value=\"AC2\">AC2</option>\n"
        "<option value=\"VChargeV5\">VChargeV5</option>\n"
        "<option value=\"VCharge1.4\">VCharge1.4</option>\n"
        "</select>\n"
        "<br><br>\n"

        "<label for=\"chargerType\">Charger Type:</label>\n"
        "<select id=\"chargerType\"";
    httpd_resp_send_chunk(req, htmlString, strlen(htmlString));
    if (isChargerConfiguringFirstTime == false)
    {
        htmlString = " disabled";
        httpd_resp_send_chunk(req, htmlString, strlen(htmlString));
    }
    htmlString =
        ">\n"
        "<option value=\"Ac3s\">Ac3s</option>\n"
        "<option value=\"Ac7s\">Ac7s</option>\n"
        "<option value=\"Ac11s\">Ac11s</option>\n"
        "<option value=\"Ac22s\">Ac22s</option>\n"
        "<option value=\"Ac44s\">Ac44s</option>\n"
        "<option value=\"Ac14D\">Ac14D</option>\n"
        "<option value=\"Ac10DP\">Ac10DP</option>\n"
        "<option value=\"Ac10DA\">Ac10DA</option>\n"
        "<option value=\"Ac6D\">Ac6D</option>\n"
        "<option value=\"Ac10t\">Ac10t</option>\n"
        "<option value=\"Ac14t\">Ac14t</option>\n"
        "<option value=\"Ac18t\">Ac18t</option>\n"
        "</select>\n"
        "<br><br>\n"

        "<label for=\"connector1PWM\">Connector-1 PWM:</label>\n"
        "<select id=\"connector1PWM\">\n"
        "<option value=\"63\">63</option>\n"
        "<option value=\"50\">50</option>\n"
        "<option value=\"40\">40</option>\n"
        "<option value=\"32\">32</option>\n"
        "<option value=\"25\">25</option>\n"
        "<option value=\"20\">20</option>\n"
        "<option value=\"16\">16</option>\n"
        "<option value=\"10\">10</option>\n"
        "</select><br><br>\n"

        "<label for=\"connector2PWM\">Connector-2 PWM:</label>\n"
        "<select id=\"connector2PWM\">\n"
        "<option value=\"32\">32</option>\n"
        "<option value=\"25\">25</option>\n"
        "<option value=\"20\">20</option>\n"
        "<option value=\"16\">16</option>\n"
        "<option value=\"10\">10</option>\n"
        "</select><br><br>\n"

        "<label for=\"connector3PWM\">Connector-3 PWM:</label>\n"
        "<select id=\"connector3PWM\">\n"
        "<option value=\"16\">16</option>\n"
        "<option value=\"10\">10</option>\n"
        "</select><br><br>\n"

        "<label for=\"chargerDisplayType\">Charger Display Type:</label>\n"
        "<select id=\"chargerDisplayType\"";
    httpd_resp_send_chunk(req, htmlString, strlen(htmlString));
    if (isChargerConfiguringFirstTime == false)
    {
        htmlString = " disabled";
        httpd_resp_send_chunk(req, htmlString, strlen(htmlString));
    }
    htmlString =
        ">\n"
        "<option value=\"LED\">LED</option>\n"
        "<option value=\"20x4\">20x4</option>\n"
        "<option value=\"DWIN\">DWIN</option>\n"
        "</select>\n"
        "<br><br>\n"

        "<label for=\"DisplayNum\">Number of Displays:</label>\n"
        "<select id=\"DisplayNum\"";
    httpd_resp_send_chunk(req, htmlString, strlen(htmlString));
    if (isChargerConfiguringFirstTime == false)
    {
        htmlString = " disabled";
        httpd_resp_send_chunk(req, htmlString, strlen(htmlString));
    }
    htmlString =
        ">\n"
        "<option value=\"1\">1</option>\n"
        "<option value=\"2\">2</option>\n"
        "<option value=\"3\">3</option>\n"
        "</select><br><br>\n"

        "<label for=\"commissionedBy\">Charger Commissioned by:</label>\n"
        "<input type=\"text\" id=\"commissionedBy\"><br><br>\n"

        "<label for=\"commissionedDate\">Charger Commissioned Date:</label>\n"
        "<input type=\"date\" id=\"commissionedDate\"><br><br>\n"

        "<label for=\"firmwareversion\">Firmware Version:</label>\n"
        "<input type=\"text\" id=\"firmwareversion\" disabled><br><br>\n"

        "<label for=\"slavefirmwareversion\">Slave Firmware Version:</label>\n"
        "<input type=\"text\" id=\"slavefirmwareversion\" disabled><br><br>\n"

        "<label for=\"webSocketURL\">WebSocket URL:</label>\n"
        "<input type=\"text\" id=\"webSocketURL\"><br><br>\n"

        "<label for=\"wifiStatus\">Wi-Fi Enable/Disable:</label>\n"
        "<select id=\"wifiStatus\">\n"
        "<option value=\"Enable\">Enable</option>\n"
        "<option value=\"Disable\">Disable</option>\n"
        "</select><br><br>\n"

        "<label for=\"wifiPriority\">Wi-Fi Priority:</label>\n"
        "<select id=\"wifiPriority\" onchange=\"disableSelectedOptions(this.id, this.value)\">\n"
        "<option value=\"1\">1</option>\n"
        "<option value=\"2\">2</option>\n"
        "<option value=\"3\">3</option>\n"
        "</select><br><br>\n"

        "<label for=\"wifiSSID\">Wi-Fi SSID:</label>\n"
        "<input type=\"text\" id=\"wifiSSID\"><br><br>\n"

        "<label for=\"wifiPassword\">Wi-Fi Password:</label>\n"
        "<input type=\"text\" id=\"wifiPassword\"><br><br>\n"

        "<label for=\"gsmStatus\">GSM Enable/Disable:</label>\n"
        "<select id=\"gsmStatus\">\n"
        "<option value=\"Enable\">Enable</option>\n"
        "<option value=\"Disable\">Disable</option>\n"
        "</select><br><br>\n"

        "<label for=\"gsmPriority\">GSM Priority:</label>\n"
        "<select id=\"gsmPriority\" onchange=\"disableSelectedOptions(this.id, this.value)\">\n"
        "<option value=\"1\">1</option>\n"
        "<option value=\"2\">2</option>\n"
        "<option value=\"3\">3</option>\n"
        "</select><br><br>\n"

        "<label for=\"gsmAPN\">GSM APN:</label>\n"
        "<input type=\"text\" id=\"gsmAPN\"><br><br>\n"

        "<label for=\"simIMEINumber\">SIM IMEI Number:</label>\n"
        "<input type=\"text\" id=\"simIMEINumber\" disabled><br><br>\n"

        "<label for=\"ethernetStatus\">Ethernet Enable/Disable:</label>\n"
        "<select id=\"ethernetStatus\">\n"
        "<option value=\"Enable\">Enable</option>\n"
        "<option value=\"Disable\">Disable</option>\n"
        "</select><br><br>\n"

        "<label for=\"ethernetPriority\">Ethernet Priority:</label>\n"
        "<select id=\"ethernetPriority\" onchange=\"disableSelectedOptions(this.id, this.value)\">\n"
        "<option value=\"1\">1</option>\n"
        "<option value=\"2\">2</option>\n"
        "<option value=\"3\">3</option>\n"
        "</select><br><br>\n"

        "<label for=\"ethernetConfig\">Ethernet Configuration Static/Dynamic Option:</label>\n"
        "<select id=\"ethernetConfig\">\n"
        "<option value=\"Static\">Static</option>\n"
        "<option value=\"Dynamic\">Dynamic</option>\n"
        "</select><br><br>\n"

        "<label for=\"ipAddress\">IP Address:</label>\n"
        "<input type=\"text\" id=\"ipAddress\"><br><br>\n"
        "<label for=\"gatewayAddress\">Gateway Address:</label>\n"
        "<input type=\"text\" id=\"gatewayAddress\"><br><br>\n"
        "<label for=\"dnsAddress\">DNS Address:</label>\n"
        "<input type=\"text\" id=\"dnsAddress\"><br><br>\n"
        "<label for=\"subnetMask\">Subnet Mask:</label>\n"
        "<input type=\"text\" id=\"subnetMask\"><br><br>\n"

        "<label for=\"OnlineOfflineStatus\">Offline-Online Enable/Disable Feature:</label>\n"
        "<select id=\"OnlineOfflineStatus\">\n"
        "<option value=\"onlineOffline\">onlineOffline</option>\n"
        "<option value=\"online\">online</option>\n"
        "<option value=\"offline\">offline</option>\n"
        "<option value=\"plugnplay\">plugnplay</option>\n"
        "</select><br><br>\n"

        "<label for=\"gfci\">GFCI Enable:</label>\n"
        "<select id=\"gfci\">\n"
        "<option value=\"Yes\">Yes</option>\n"
        "<option value=\"No\">No</option>\n"
        "</select><br><br>\n"

        "<label for=\"lowCurrentTime\">Low Current Time/Interval(Seconds):</label>\n"
        "<input type=\"text\" id=\"lowCurrentTime\"><br><br>\n"

        "<label for=\"minLowCurrentThreshold\">Minimum Low Current Threshold(Amps):</label>\n"
        "<input type=\"text\" id=\"minLowCurrentThreshold\"><br><br>\n"

        "<label for=\"resumeSession\">Resume Session after Power Recycle:</label>\n"
        "<select id=\"resumeSession\">\n"
        "<option value=\"Yes\">Yes</option>\n"
        "<option value=\"No\">No</option>\n"
        "</select><br><br>\n"

        "<label for=\"overVoltageThreshold\">Over Voltage Threshold:</label>\n"
        "<input type=\"text\" id=\"overVoltageThreshold\"><br><br>\n"
        "<label for=\"underVoltageThreshold\">Under Voltage Threshold:</label>\n"
        "<input type=\"text\" id=\"underVoltageThreshold\"><br><br>\n"
        "<label for=\"con1overCurrentThreshold\">Connector 1: Over Current Threshold:</label>\n"
        "<input type=\"text\" id=\"con1overCurrentThreshold\"><br><br>\n"
        "<label for=\"con2overCurrentThreshold\">Connector 2: Over Current Threshold:</label>\n"
        "<input type=\"text\" id=\"con2overCurrentThreshold\"><br><br>\n"
        "<label for=\"con3overCurrentThreshold\">Connector 3: Over Current Threshold:</label>\n"
        "<input type=\"text\" id=\"con3overCurrentThreshold\"><br><br>\n"
        "<label for=\"overTemperatureThreshold\">Over Temperature Threshold(Degree Celsius):</label>\n"
        "<input type=\"text\" id=\"overTemperatureThreshold\"><br><br>\n"

        "<label for=\"restoreSession\">Restore Session from Fault:</label>\n"
        "<select id=\"restoreSession\">\n"
        "<option value=\"Yes\">Yes</option>\n"
        "<option value=\"No\">No</option>\n"
        "</select><br><br>\n"

        "<label for=\"sessionRestoreTime\">Session Restore Fault Recovery Time(Seconds):</label>\n"
        "<input type=\"text\" id=\"sessionRestoreTime\"><br><br>\n"

        "<label for=\"StopTransactionInSuspendedStateStatus\">Stop Transaction In Suspended State Enable/Disable:</label>\n"
        "<select id=\"StopTransactionInSuspendedStateStatus\">\n"
        "<option value=\"Enable\">Enable</option>\n"
        "<option value=\"Disable\">Disable</option>\n"
        "</select><br><br>\n"

        "<label for=\"StopTransactionInSuspendedStateTime\">Stop Transaction In Suspended State Time(Seconds):</label>\n"
        "<input type=\"text\" id=\"StopTransactionInSuspendedStateTime\"><br><br>\n"

        "<label for=\"c\">Smart Charging Enable/Disable:</label>\n"
        "<select id=\"smartChargingStatus\">\n"
        "<option value=\"Enable\">Enable</option>\n"
        "<option value=\"Disable\">Disable</option>\n"
        "</select><br><br>\n"

        "<label for=\"batteryBackupStatus\">Battery Backup Enable/Disable:</label>\n"
        "<select id=\"batteryBackupStatus\">\n"
        "<option value=\"Enable\">Enable</option>\n"
        "<option value=\"Disable\">Disable</option>\n"
        "</select><br><br>\n"

        "<label for=\"diagnosticServerStatus\">Diagnostic Server Enable/Disable:</label>\n"
        "<select id=\"diagnosticServerStatus\">\n"
        "<option value=\"Enable\">Enable</option>\n"
        "<option value=\"Disable\">Disable</option>\n"
        "</select><br><br>\n"

        "<label for=\"alprDeviceStatus\">ALPR Device Enable/Disable:</label>\n"
        "<select id=\"alprDeviceStatus\">\n"
        "<option value=\"Enable\">Enable</option>\n"
        "<option value=\"Disable\">Disable</option>\n"
        "</select><br><br>\n"

        "<label for=\"clearAuthCache\">Clear Authorization Cache:</label>\n"
        "<select id=\"clearAuthCache\">\n"
        "<option value=\"Yes\">Yes</option>\n"
        "<option value=\"No\">No</option>\n"
        "</select><br><br>\n"

        "<label for=\"factoryReset\">Factory Reset:</label>\n"
        "<select id=\"factoryReset\">\n"
        "<option value=\"Yes\">Yes</option>\n"
        "<option value=\"No\">No</option>\n"
        "</select><br><br>\n"

        "<label for=\"otaURLConfig\">OTA URL Configuration:</label>\n"
        "<input type=\"text\" id=\"otaURLConfig\"><br><br>\n"

        "<label for=\"localListTags\">Local List Tags Upload:</label>\n"
        "<input type=\"text\" id=\"localListTags\"><br><br>\n"

        "<label for=\"adminpassword\">Admin User Password:</label>\n"
        "<input type=\"text\" id=\"adminpassword\"><br><br>\n"
        "<label for=\"factorypassword\">Factory User Password:</label>\n"
        "<input type=\"text\" id=\"factorypassword\"><br><br>\n"
        "<label for=\"servicepassword\">Service User Password:</label>\n"
        "<input type=\"text\" id=\"servicepassword\"><br><br>\n"
        "<label for=\"customerpassword\">Customer User Password:</label>\n"
        "<input type=\"text\" id=\"customerpassword\"><br><br>\n"

        "<button onclick=\"getConfig()\">Get Config</button>\n"
        "<button onclick=\"setConfig()\">Set Config</button>\n"
        "<p id=\"statusMessage\"></p>\n"
        "\t\t</div>\n"
        "\t\t</div>\n"
        "\n"

        "<script>\n"
        "function getConfig() {\n"
        "fetch('http://192.168.4.1/config/getConfig')\n"
        ".then(response => response.json())\n"
        ".then(data => {\n"
        "document.getElementById('serialNumber').value = data.serialNumber;\n"
        "document.getElementById('chargerName').value = data.chargerName;\n"
        "document.getElementById('chargePointVendor').value = data.chargePointVendor;\n"
        "document.getElementById('chargePointModel').value = data.chargePointModel;\n"
        "document.getElementById('boardType').value = data.boardType;\n"
        "document.getElementById('chargerType').value = data.chargerType;\n"
        "document.getElementById('connector1PWM').value = data.connector1PWM;\n"
        "document.getElementById('connector2PWM').value = data.connector2PWM;\n"
        "document.getElementById('connector3PWM').value = data.connector3PWM;\n"
        "document.getElementById('chargerDisplayType').value = data.chargerDisplayType;\n"
        "document.getElementById('DisplayNum').value = data.DisplayNum;\n"
        "document.getElementById('commissionedBy').value = data.commissionedBy;\n"
        "document.getElementById('commissionedDate').value = data.commissionedDate;\n"
        "document.getElementById('firmwareversion').value = data.firmwareversion;\n"
        "document.getElementById('slavefirmwareversion').value = data.slavefirmwareversion;\n"
        "document.getElementById('webSocketURL').value = data.webSocketURL;\n"
        "document.getElementById('wifiStatus').value = data.wifiStatus;\n"
        "document.getElementById('wifiPriority').value = data.wifiPriority;\n"
        "document.getElementById('wifiSSID').value = data.wifiSSID;\n"
        "document.getElementById('wifiPassword').value = data.wifiPassword;\n"
        "document.getElementById('gsmStatus').value = data.gsmStatus;\n"
        "document.getElementById('gsmPriority').value = data.gsmPriority;\n"
        "document.getElementById('gsmAPN').value = data.gsmAPN;\n"
        "document.getElementById('simIMEINumber').value = data.simIMEINumber;\n"
        "document.getElementById('ethernetStatus').value = data.ethernetStatus;\n"
        "document.getElementById('ethernetPriority').value = data.ethernetPriority;\n"
        "document.getElementById('ethernetConfig').value = data.ethernetConfig;\n"
        "document.getElementById('ipAddress').value = data.ipAddress;\n"
        "document.getElementById('gatewayAddress').value = data.gatewayAddress;\n"
        "document.getElementById('dnsAddress').value = data.dnsAddress;\n"
        "document.getElementById('subnetMask').value = data.subnetMask;\n"
        "document.getElementById('OnlineOfflineStatus').value = data.OnlineOfflineStatus;\n"
        "document.getElementById('gfci').value = data.gfci;\n"
        "document.getElementById('lowCurrentTime').value = data.lowCurrentTime;\n"
        "document.getElementById('minLowCurrentThreshold').value = data.minLowCurrentThreshold;\n"
        "document.getElementById('resumeSession').value = data.resumeSession;\n"
        "document.getElementById('overVoltageThreshold').value = data.overVoltageThreshold;\n"
        "document.getElementById('underVoltageThreshold').value = data.underVoltageThreshold;\n"
        "document.getElementById('con1overCurrentThreshold').value = data.con1overCurrentThreshold;\n"
        "document.getElementById('con2overCurrentThreshold').value = data.con2overCurrentThreshold;\n"
        "document.getElementById('con3overCurrentThreshold').value = data.con3overCurrentThreshold;\n"
        "document.getElementById('overTemperatureThreshold').value = data.overTemperatureThreshold;\n"
        "document.getElementById('restoreSession').value = data.restoreSession;\n"
        "document.getElementById('sessionRestoreTime').value = data.sessionRestoreTime;\n"
        "document.getElementById('StopTransactionInSuspendedStateStatus').value = data.StopTransactionInSuspendedStateStatus;\n"
        "document.getElementById('StopTransactionInSuspendedStateTime').value = data.StopTransactionInSuspendedStateTime;\n"
        "document.getElementById('smartChargingStatus').value = data.smartChargingStatus;\n"
        "document.getElementById('batteryBackupStatus').value = data.batteryBackupStatus;\n"
        "document.getElementById('diagnosticServerStatus').value = data.diagnosticServerStatus;\n"
        "document.getElementById('alprDeviceStatus').value = data.alprDeviceStatus;\n"
        "document.getElementById('clearAuthCache').value = data.clearAuthCache;\n"
        "document.getElementById('factoryReset').value = data.factoryReset;\n"
        "document.getElementById('otaURLConfig').value = data.otaURLConfig;\n"
        "document.getElementById('localListTags').value = data.localListTags;\n"
        "document.getElementById('adminpassword').value = data.adminpassword;\n"
        "document.getElementById('factorypassword').value = data.factorypassword;\n"
        "document.getElementById('servicepassword').value = data.servicepassword;\n"
        "document.getElementById('customerpassword').value = data.customerpassword;\n"

        "})\n"
        ".catch(error => {\n"
        "console.error('Error:', error);\n"
        "});\n"
        "}\n"

        "function setConfig() {\n"
        "const data = {\n"
        "serialNumber: document.getElementById('serialNumber').value,\n"
        "chargerName: document.getElementById('chargerName').value,\n"
        "chargePointVendor: document.getElementById('chargePointVendor').value,\n"
        "chargePointModel: document.getElementById('chargePointModel').value,\n"
        "boardType: document.getElementById('boardType').value,\n"
        "chargerType: document.getElementById('chargerType').value,\n"
        "connector1PWM: document.getElementById('connector1PWM').value,\n"
        "connector2PWM: document.getElementById('connector2PWM').value,\n"
        "connector3PWM: document.getElementById('connector3PWM').value,\n"
        "chargerDisplayType: document.getElementById('chargerDisplayType').value,\n"
        "DisplayNum: document.getElementById('DisplayNum').value,\n"
        "commissionedBy: document.getElementById('commissionedBy').value,\n"
        "commissionedDate: document.getElementById('commissionedDate').value,\n"
        "webSocketURL: document.getElementById('webSocketURL').value,\n"
        "wifiStatus: document.getElementById('wifiStatus').value,\n"
        "wifiPriority: document.getElementById('wifiPriority').value,\n"
        "wifiSSID: document.getElementById('wifiSSID').value,\n"
        "wifiPassword: document.getElementById('wifiPassword').value,\n"
        "gsmStatus: document.getElementById('gsmStatus').value,\n"
        "gsmPriority: document.getElementById('gsmPriority').value,\n"
        "gsmAPN: document.getElementById('gsmAPN').value,\n"
        "simIMEINumber: document.getElementById('simIMEINumber').value,\n"
        "ethernetStatus: document.getElementById('ethernetStatus').value,\n"
        "ethernetPriority: document.getElementById('ethernetPriority').value,\n"
        "ethernetConfig: document.getElementById('ethernetConfig').value,\n"
        "ipAddress: document.getElementById('ipAddress').value,\n"
        "gatewayAddress: document.getElementById('gatewayAddress').value,\n"
        "dnsAddress: document.getElementById('dnsAddress').value,\n"
        "subnetMask: document.getElementById('subnetMask').value,\n"
        "OnlineOfflineStatus: document.getElementById('OnlineOfflineStatus').value,\n"
        "gfci: document.getElementById('gfci').value,\n"
        "lowCurrentTime: document.getElementById('lowCurrentTime').value,\n"
        "minLowCurrentThreshold: document.getElementById('minLowCurrentThreshold').value,\n"
        "resumeSession: document.getElementById('resumeSession').value,\n"
        "overVoltageThreshold: document.getElementById('overVoltageThreshold').value,\n"
        "underVoltageThreshold: document.getElementById('underVoltageThreshold').value,\n"
        "con1overCurrentThreshold: document.getElementById('con1overCurrentThreshold').value,\n"
        "con2overCurrentThreshold: document.getElementById('con2overCurrentThreshold').value,\n"
        "con3overCurrentThreshold: document.getElementById('con3overCurrentThreshold').value,\n"
        "overTemperatureThreshold: document.getElementById('overTemperatureThreshold').value,\n"
        "restoreSession: document.getElementById('restoreSession').value,\n"
        "sessionRestoreTime: document.getElementById('sessionRestoreTime').value,\n"
        "StopTransactionInSuspendedStateStatus: document.getElementById('StopTransactionInSuspendedStateStatus').value,\n"
        "StopTransactionInSuspendedStateTime: document.getElementById('StopTransactionInSuspendedStateTime').value,\n"
        "smartChargingStatus: document.getElementById('smartChargingStatus').value,\n"
        "batteryBackupStatus: document.getElementById('batteryBackupStatus').value,\n"
        "diagnosticServerStatus: document.getElementById('diagnosticServerStatus').value,\n"
        "alprDeviceStatus: document.getElementById('alprDeviceStatus').value,\n"
        "clearAuthCache: document.getElementById('clearAuthCache').value,\n"
        "factoryReset: document.getElementById('factoryReset').value,\n"
        "otaURLConfig: document.getElementById('otaURLConfig').value,\n"
        "localListTags: document.getElementById('localListTags').value,\n"
        "adminpassword: document.getElementById('adminpassword').value,\n"
        "factorypassword: document.getElementById('factorypassword').value,\n"
        "servicepassword: document.getElementById('servicepassword').value,\n"
        "customerpassword: document.getElementById('customerpassword').value\n"
        "};\n"

        "fetch('http://192.168.4.1/config/setConfig', {\n"
        "method: 'POST',\n"
        "headers: {\n"
        "'Content-Type': 'application/json'\n"
        "},\n"
        "body: JSON.stringify(data)\n"
        "})\n"
        ".then(response => {\n"
        "if (!response.ok) {\n"
        "throw new Error('Network response was not ok');\n"
        "}\n"
        "return response.json();\n"
        "})\n"
        ".then(data => {\n"
        "document.getElementById('statusMessage').textContent = data.message;\n"
        "})\n"
        ".catch(error => {\n"
        "console.error('Error:', error);\n"
        "document.getElementById('statusMessage').textContent = 'Error: ' + error.message;\n"
        "});\n"
        "}\n"
        "</script>\n"
        "</body>\n"
        "</html>\n";
    httpd_resp_send_chunk(req, htmlString, strlen(htmlString));
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t Url_login(httpd_req_t *req)
{
    char *htmlString = "<!DOCTYPE html>\n"
                       "<html>\n"
                       "\t<head>\n"
                       "\t\t<title>Login Page</title>\n"
                       "\t\t<style>\n"
                       "\t\t\tbody {\n"
                       "\t\t\tfont-family: Arial, sans-serif;\n"
                       "\t\t\tbackground-color: #87cefa;\n"
                       "\t\t\tmargin: 0;\n"
                       "\t\t\tpadding: 20px;\n"
                       "\t\t\theight: 100vh;\n"
                       "\t\t\tdisplay: flex;\n"
                       "\t\t\tjustify-content: center;\n"
                       "\t\t\talign-items: center;\n"
                       "\t\t\tflex-direction: column;\n"
                       "\t\t\t}\n"
                       "\t\t\th1 {\n"
                       "\t\t\tbackground-color: green;\n"
                       "\t\t\tcolor: white;\n"
                       "\t\t\tpadding: 10px;\n"
                       "\t\t\tborder-radius: 8px;\n"
                       "\t\t\ttext-align: center;\n"
                       "\t\t\twidth: 100%;\n"
                       "\t\t\tbox-sizing: border-box;\n"
                       "\t\t\t}\n"
                       "\t\t\th2 {\n"
                       "\t\t\tcolor: #333;\n"
                       "\t\t\ttext-align: center;\n"
                       "\t\t\t}\n"
                       "\t\t\t.form-container {\n"
                       "\t\t\tbackground-color: #fff;\n"
                       "\t\t\tpadding: 20px;\n"
                       "\t\t\tborder-radius: 8px;\n"
                       "\t\t\tbox-shadow: 0 0 10px rgba(0, 0, 0, 0.1);\n"
                       "\t\t\t}\n"
                       "\t\t\t.form-group {\n"
                       "\t\t\tdisplay: flex;\n"
                       "\t\t\tflex-direction: column;\n"
                       "\t\t\tmargin-bottom: 15px;\n"
                       "\t\t\t}\n"
                       "\t\t\tlabel {\n"
                       "\t\t\tmargin-bottom: 5px;\n"
                       "\t\t\tfont-weight: bold;\n"
                       "\t\t\t}\n"
                       "\t\t\tinput[type=\"text\"], input[type=\"password\"], select {\n"
                       "\t\t\tpadding: 10px;\n"
                       "\t\t\tborder: 1px solid #ccc;\n"
                       "\t\t\tborder-radius: 4px;\n"
                       "\t\t\twidth: 100%;\n"
                       "\t\t\tbox-sizing: border-box;\n"
                       "\t\t\t}\n"
                       "\t\t\tbutton {\n"
                       "\t\t\tpadding: 10px 20px;\n"
                       "\t\t\tbackground-color: green;\n"
                       "\t\t\tcolor: white;\n"
                       "\t\t\tborder: none;\n"
                       "\t\t\tborder-radius: 4px;\n"
                       "\t\t\tcursor: pointer;\n"
                       "\t\t\tfont-size: 16px;\n"
                       "\t\t\t}\n"
                       "\t\t\tbutton:hover {\n"
                       "\t\t\tbackground-color: darkgreen;\n"
                       "\t\t\t}\n"
                       "\t\t</style>\n"
                       "\t<body>\n"
                       "\t\t<h1>EVRE</h1>\n"
                       "\t\t<div class=\"form-container\">\n"
                       "\t\t\t<h2>Login</h2>\n"
                       "\t\t\t<form>\n"
                       "\t\t\t\t<div class=\"form-group\">\n"
                       "\t\t\t\t\t<label for=\"username\">Username:</label>\n"
                       "\t\t\t\t\t<select id=\"username\">\n"
                       "\t\t\t\t\t\t<option value=\"Admin\">Admin</option>\n"
                       "\t\t\t\t\t\t<option value=\"Factory\">Factory</option>\n"
                       "\t\t\t\t\t\t<option value=\"Service\">Service</option>\n"
                       "\t\t\t\t\t\t<option value=\"Customer\">Customer</option>\n"
                       "\t\t\t\t\t</select>\n"
                       "\t\t\t\t</div>\n"
                       "\t\t\t\t<div class=\"form-group\">\n"
                       "\t\t\t\t\t<label for=\"password\">Password:</label>\n"
                       "\t\t\t\t\t<input type=\"text\" id=\"password\" name=\"password\">\n"
                       "\t\t\t\t</div>\n"
                       "\t\t\t\t<div class=\"form-group\">\n"
                       "\t\t\t\t\t<button type=\"button\" onclick=\"handleLogin()\">Login</button>\n"
                       "\t\t\t\t</div>\n"
                       "\t\t\t\t<div class=\"form-group\">\n"
                       "\t\t\t\t<p id=\\\"statusMessage\\\"></p>\n"
                       "\t\t\t\t</div>\n"
                       "\t\t\t</form>\n"
                       "\t\t</div>\n"
                       "\t</body>\n"
                       "\t<script>\n"
                       "\t\tfunction handleLogin() {\n"
                       "\t\t    const data = {\n"
                       "\t\t        username: document.getElementById('username').value,\n"
                       "\t\t        password: document.getElementById('password').value\n"
                       "\t\t    };\n"
                       "\t\t\n"
                       "\t\t    fetch('http://";
    httpd_resp_send_chunk(req, htmlString, strlen(htmlString));
    // httpd_resp_send_chunk(req, dnsName, strlen(dnsName));
    htmlString = "192.168.4.1/loginAuth', {\n"
                 "\t\t        method: 'POST',\n"
                 "\t\t        headers: {\n"
                 "\t\t            'Content-Type': 'application/json'\n"
                 "\t\t        },\n"
                 "\t\t        body: JSON.stringify(data)\n"
                 "\t\t    })\n"
                 "\t\t    .then(response => {\n"
                 "\t\t        if (!response.ok) {\n"
                 "\t\t            throw new Error('Network response was not ok');\n"
                 "\t\t        }\n"
                 "\t\t        return response.json();\n"
                 "\t\t    })\n"
                 "\t\t    .then(data => {\n"
                 "\t\t        if (data.message === 'Invalid password') {\n"
                 "\t\t            document.getElementById('statusMessage').textContent = 'Invalid password';\n"
                 "\t\t        } else {\n"
                 "\t\t            window.location.href = 'http://";
    httpd_resp_send_chunk(req, htmlString, strlen(htmlString));
    // httpd_resp_send_chunk(req, dnsName, strlen(dnsName));
    htmlString = "192.168.4.1/configuration'; // replace with your new page URL\n"
                 "\t\t        }\n"
                 "\t\t    })\n"
                 "\t\t    .catch(error => {\n"
                 "\t\t        console.error('Error:', error);\n"
                 "\t\t        document.getElementById('statusMessage').textContent = 'Error: ' + error.message;\n"
                 "\t\t    });\n"
                 "\t\t}\n"
                 "\t\t        \n"
                 "\t\t   \n"
                 "\t</script>\n"
                 "\t</head>\n"
                 "</html>";
    httpd_resp_send_chunk(req, htmlString, strlen(htmlString));
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t on_set_config_url(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    memset(&buffer, '\0', sizeof(buffer));
    ESP_LOGI(TAG, "SET CONFIG URL: %s", req->uri);
    httpd_req_recv(req, buffer, req->content_len);
    ESP_LOGI(TAG, "%s", buffer);
    Set_Config(CurrentUrlType);
    httpd_resp_set_status(req, "204 NO CONTENT");
    httpd_resp_send(req, NULL, 0);
    ESP_LOGI(TAG, "Configuration Updated. Restrating Now...................\n");
    esp_restart();
    return ESP_OK;
}

static esp_err_t on_UpdateFirmware_url(httpd_req_t *req)
{
    bool isReqBodyStarted = false;
    bool isFlashSuccessful = false;
    char otaBuffer[1024];
    int contentLength = req->content_len;
    int contentReceived = 0;
    int ReceivedLength = 0;
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    memset(&otaBuffer, '\0', sizeof(otaBuffer));
    ESP_LOGI(TAG, "SET UPDATE FIRMWARE URL: %s", req->uri);
    do
    {
        if ((ReceivedLength = httpd_req_recv(req, otaBuffer, sizeof(otaBuffer))) < 0)
        {
            if (ReceivedLength == HTTPD_SOCK_ERR_TIMEOUT)
            {
                ESP_LOGI(TAG, "HTTPD_SOCK_ERR_TIMEOUT");
                continue; // retry receiving if timeout occured
            }
            ESP_LOGE(TAG, "httpd_req_recv failed with error %d", ReceivedLength);
            return ESP_FAIL;
        }

        if (isReqBodyStarted)
        {
            esp_err_t err = esp_ota_write(update_handle, otaBuffer, ReceivedLength);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
                esp_ota_abort(update_handle);
                return ESP_FAIL;
            }
            contentReceived += ReceivedLength;
        }
        else
        {
            isReqBodyStarted = true;
            char *bodyPartStartPosition = strstr(otaBuffer, "\r\n\r\n") + 4;
            int bodyPartLength = ReceivedLength - (bodyPartStartPosition - otaBuffer);
            ESP_LOGI(TAG, "OTA File size: %d", contentLength);

            esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                esp_ota_abort(update_handle);
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%lx", update_partition->subtype, update_partition->address);
            err = esp_ota_write(update_handle, bodyPartStartPosition, bodyPartLength);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
                esp_ota_abort(update_handle);
                return ESP_FAIL;
            }
            contentReceived += bodyPartLength;
        }
    } while ((ReceivedLength > 0) && (contentReceived < contentLength));

    if (esp_ota_end(update_handle) == ESP_OK)
    {
        if (esp_ota_set_boot_partition(update_partition) == ESP_OK)
        {
            const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
            ESP_LOGI(TAG, "Boot partition %d at offset 0x%lx", boot_partition->subtype, boot_partition->address);
            isFlashSuccessful = true;
        }
        else
        {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition flash error");
        }
    }
    else
    {
        ESP_LOGE(TAG, "esp_ota_end failed");
        esp_ota_abort(update_handle);
        return ESP_FAIL;
    }
    // Set_Config(CurrentUrlType);
    httpd_resp_set_status(req, "204 NO CONTENT");
    httpd_resp_send(req, NULL, 0);
    ESP_LOGI(TAG, "Firmware Updated. Restrating Now...................\n");
    esp_restart();
    return ESP_OK;
}

static esp_err_t on_GetFirmwareVersion_url(httpd_req_t *req)
{
    custom_nvs_read_config();
    ESP_LOGI(TAG, "GET GET FRIMWARE VERSION URL: %s", req->uri);
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        printf("Failed to create cJSON object\n");
        return EXIT_FAILURE;
    }
    cJSON_AddStringToObject(root, "firmwareversion", config.firmwareVersion);
    char *jsonString = cJSON_Print(root);
    if (jsonString == NULL)
    {
        printf("Failed to convert cJSON object to string\n");
        cJSON_Delete(root);
        return EXIT_FAILURE;
    }
    memset(&buffer, '\0', sizeof(buffer));
    memcpy(buffer, jsonString, strlen(jsonString));
    cJSON_Delete(root);
    free(jsonString);

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, buffer);
    return ESP_OK;
}
static esp_err_t on_get_config_url(httpd_req_t *req)
{
    custom_nvs_read_config();
    ESP_LOGI(TAG, "GET CONFIG URL: %s", req->uri);
    Get_Config(CurrentUrlType);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, buffer);
    return ESP_OK;
}

static esp_err_t on_configuration_url(httpd_req_t *req)
{
    ESP_LOGI(TAG, "ADMIN URL: %s", req->uri);
    isUrlHit = true;

    Url_Config(CurrentUrlType, req);
    CurrentUrlType = 0;
    return ESP_OK;
}

static esp_err_t on_login_Auth_url(httpd_req_t *req)
{
    ESP_LOGI(TAG, "LOGIN AUTH URL: %s", req->uri);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    memset(&buffer, '\0', sizeof(buffer));
    httpd_req_recv(req, buffer, req->content_len);
    ESP_LOGI(TAG, "%s", buffer);
    cJSON *Payload = cJSON_Parse(buffer);
    if (Payload != NULL)
    {
        cJSON *usernameJson = cJSON_GetObjectItem(Payload, "username");
        cJSON *passwordJson = cJSON_GetObjectItem(Payload, "password");
        if ((cJSON_IsString(usernameJson)) && (cJSON_IsString(passwordJson)))
        {
            if (memcmp(usernameJson->valuestring, "Admin", strlen("Admin")) == 0 && memcmp(passwordJson->valuestring, config.adminpassword, strlen(config.adminpassword)) == 0)
            {
                CurrentUrlType = ADMIN_URL;
                // httpd_resp_set_status(req, "204 NO CONTENT");
                httpd_resp_send(req, "{ \"message\" : \"Valid password\" }", strlen("{ \"message\" : \"Valid password\" }"));
                cJSON_Delete(Payload);

                return ESP_OK;
            }
            else if (memcmp(usernameJson->valuestring, "Factory", strlen("Factory")) == 0 && memcmp(passwordJson->valuestring, config.factorypassword, strlen(config.factorypassword)) == 0)
            {
                CurrentUrlType = FACTORY_URL;
                // httpd_resp_set_status(req, "204 NO CONTENT");
                httpd_resp_send(req, "{ \"message\" : \"Valid password\" }", strlen("{ \"message\" : \"Valid password\" }"));
                cJSON_Delete(Payload);
                return ESP_OK;
            }
            else if (memcmp(usernameJson->valuestring, "Service", strlen("Service")) == 0 && memcmp(passwordJson->valuestring, config.servicepassword, strlen(config.servicepassword)) == 0)
            {
                CurrentUrlType = SERVICE_URL;
                // httpd_resp_set_status(req, "204 NO CONTENT");
                httpd_resp_send(req, "{ \"message\" : \"Valid password\" }", strlen("{ \"message\" : \"Valid password\" }"));
                cJSON_Delete(Payload);
                return ESP_OK;
            }
            else if (memcmp(usernameJson->valuestring, "Customer", strlen("Customer")) == 0 && memcmp(passwordJson->valuestring, config.customerpassword, strlen(config.customerpassword)) == 0)
            {
                CurrentUrlType = CUSTOMER_URL;
                // httpd_resp_set_status(req, "204 NO CONTENT");
                httpd_resp_send(req, "{ \"message\" : \"Valid password\" }", strlen("{ \"message\" : \"Valid password\" }"));
                cJSON_Delete(Payload);
                return ESP_OK;
            }
        }
    }
    httpd_resp_set_status(req, "400 BAD REQUEST");
    httpd_resp_send(req, "{ \"message\" : \"Invalid password\" }", strlen("{ \"message\" : \"Invalid password\" }"));
    return ESP_OK;
}

static esp_err_t on_login_url(httpd_req_t *req)
{
    ESP_LOGI(TAG, "LOGIN URL: %s", req->uri);
    Url_login(req);
    isUrlHit = true;
    for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
    {
        if (config.vcharge_lite_1_4)
        {
            SlaveSetLedColor(slaveLed[i], STATION_CONFIG_LED);
        }
        else
        {
            ledStateColor[i] = STATION_CONFIG_LED;
        }
    }
    return ESP_OK;
}

void local_http_server_start(void)
{
    if (isServerStarted == false)
    {
        ESP_LOGI(TAG, "Starting Server");

        ESP_ERROR_CHECK(httpd_start(&httpd_server, &httpd_config));

        httpd_uri_t GetFirmwareVersion_url = {
            .uri = "/GetFirmwareVersion",
            .method = HTTP_GET,
            .handler = on_GetFirmwareVersion_url};
        httpd_register_uri_handler(httpd_server, &GetFirmwareVersion_url);

        httpd_uri_t UpdateFirmware_url = {
            .uri = "/UpdateFirmware",
            .method = HTTP_POST,
            .handler = on_UpdateFirmware_url,
            .user_ctx = NULL};
        httpd_register_uri_handler(httpd_server, &UpdateFirmware_url);

        httpd_uri_t config_get_url = {
            .uri = "/config/getConfig",
            .method = HTTP_GET,
            .handler = on_get_config_url};
        httpd_register_uri_handler(httpd_server, &config_get_url);

        httpd_uri_t config_set_url = {
            .uri = "/config/setConfig",
            .method = HTTP_POST,
            .handler = on_set_config_url,
            .user_ctx = NULL};
        httpd_register_uri_handler(httpd_server, &config_set_url);

        httpd_uri_t configuration_url = {
            .uri = "/configuration",
            .method = HTTP_GET,
            .handler = on_configuration_url};
        httpd_register_uri_handler(httpd_server, &configuration_url);

        httpd_uri_t login_url = {
            .uri = "/login",
            .method = HTTP_GET,
            .handler = on_login_url};
        httpd_register_uri_handler(httpd_server, &login_url);

        httpd_uri_t login_Auth_url = {
            .uri = "/loginAuth",
            .method = HTTP_POST,
            .handler = on_login_Auth_url};
        httpd_register_uri_handler(httpd_server, &login_Auth_url);

        isServerStarted = true;
    }
}

void local_http_server_stop(void)
{
    if (isServerStarted == true)
    {
        ESP_LOGI(TAG, "Stoppig Server");
        httpd_stop(httpd_server);
        isServerStarted = false;
    }
}

void StartServerOnPowerOn(void)
{
    uint32_t timer = 0;
    uint32_t expiryTime = 120;
    for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
    {
        if (config.vcharge_lite_1_4 || config.AC2)
        {
            SlaveSetLedColor(slaveLed[i], POWERON_LED);
        }
        else
        {
            ledStateColor[i] = POWERON_LED;
        }
    }
    bool isWifiApStarted = wifiStartPowerOn();
    for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
        custom_nvs_read_connector_data(i);

    if (isWifiApStarted)
    {
        for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
        {
            if (config.vcharge_lite_1_4 || config.AC2)
            {
                SlaveSetLedColor(slaveLed[i], COMMISSIONING_LED);
            }
            else
            {
                ledStateColor[i] = COMMISSIONING_LED;
            }
        }
        while ((timer < expiryTime) && (isStationConnected() == false))
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            timer++;
        }

        if (isStationConnected())
        {
            for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
            {
                if (config.vcharge_lite_1_4 || config.AC2)
                {
                    SlaveSetLedColor(slaveLed[i], STATION_CONNECTED_LED);
                }
                else
                {
                    ledStateColor[i] = STATION_CONNECTED_LED;
                }
            }
            SetChargerCommissioningBit();
            local_http_server_start();
            while (timer < expiryTime)
            {
                if (isUrlHit)
                {
                    expiryTime = 600;
                }
                ESP_LOGI(TAG, "time : %ld\t expiryTime : %ld", timer, expiryTime);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                timer++;
            }
            local_http_server_stop();
            // for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
            // {
            //     if (ConnectorData[i].CmsAvailable)
            //     {
            //         if (config.online)
            //         {
            //             if (config.vcharge_lite_1_4)
            //                 SlaveSetLedColor(i, ONLINE_ONLY_OFFLINE_UNAVAILABLE_LED);
            //             else
            //                 ledStateColor[i] = ONLINE_ONLY_OFFLINE_UNAVAILABLE_LED;
            //         }
            //         else if (config.offline)
            //         {
            //             if (config.vcharge_lite_1_4)
            //                 SlaveSetLedColor(i, OFFLINE_ONLY_AVAILABLE_LED);
            //             else
            //                 ledStateColor[i] = OFFLINE_ONLY_AVAILABLE_LED;
            //         }
            //         else
            //         {
            //             if (config.vcharge_lite_1_4)
            //                 SlaveSetLedColor(i, OFFLINE_AVAILABLE_LED);
            //             else
            //                 ledStateColor[i] = OFFLINE_AVAILABLE_LED;
            //         }
            //     }
            //     else
            //     {
            //         if (config.online)
            //         {
            //             if (config.vcharge_lite_1_4)
            //                 SlaveSetLedColor(i, ONLINE_ONLY_UNAVAILABLE_LED);
            //             else
            //                 ledStateColor[i] = ONLINE_ONLY_UNAVAILABLE_LED;
            //         }
            //         else
            //         {
            //             if (config.vcharge_lite_1_4)
            //                 SlaveSetLedColor(i, OFFLINE_UNAVAILABLE_LED);
            //             else
            //                 ledStateColor[i] = OFFLINE_UNAVAILABLE_LED;
            //         }
            //     }
            // }
        }
        wifiStopPowerOn();
    }
}