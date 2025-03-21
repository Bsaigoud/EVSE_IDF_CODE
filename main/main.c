#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"
#include "slave.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "wifi_connect.h"
#include "websocket_connect.h"
#include "ProjectSettings.h"
#include "ocpp.h"
#include "EnergyMeter.h"
#include "Rfid.h"
#include "ControlPilotAdc.h"
#include "ControlPilotPwm.h"
#include "ledStrip.h"
#include "ocpp_task.h"
#include "ControlPilotState.h"
#include "local_http_server.h"
#include "custom_nvs.h"
#include "esp_mac.h"
#include "OTA.h"
#include "esp_ota_ops.h"
#include "faultGpios.h"
#include "networkInterfacesControl.h"
#include "esp_netif.h"
#include "display.h"
#include "GpioExpander.h"
#include "logsToSpiffs.h"

#define TAG "MAIN"
Config_t config;
#define DELAY_TIME 10000

int free_heap_size;
extern bool WifiInitialised;
extern bool connectorEnabled[NUM_OF_CONNECTORS];

void init_config(void)
{
    custom_nvs_read_config();
    // config.defaultConfig = false;
    if (config.defaultConfig == false)
    {
        memset(&config, 0, sizeof(Config_t));

        memcpy(config.serialNumber, "Amplify", strlen("Amplify"));
        memcpy(config.chargerName, "Amplify", strlen("Amplify"));
        memcpy(config.chargePointVendor, "EVRE", strlen("EVRE"));
        memcpy(config.chargePointModel, "HALO", strlen("HALO"));
        memcpy(config.commissionedBy, "EVRE", strlen("EVRE"));
        memcpy(config.commissionedDate, "2024-07-16", strlen("2024-07-16"));
        memcpy(config.simIMEINumber, "864180056160042", strlen("864180056160042"));
        memcpy(config.simIMSINumber, "404920406446951", strlen("404920406446951"));
        memcpy(config.webSocketURL, "ws://evre.pulseenergy.io/ws/OCPP16J/1000/CPJ7OQXOEF", strlen("ws://evre.pulseenergy.io/ws/OCPP16J/1000/CPJ7OQXOEF"));
        config.wifiAvailability = true;
        config.wifiPriority = 3;
        config.gsmAvailability = true;
        config.gsmPriority = 2;
        config.ethernetAvailability = true;
        config.ethernetPriority = 1;
        config.wifiEnable = true;
        memcpy(config.wifiSSID, "EVRE Hub", strlen("EVRE Hub"));
        memcpy(config.wifiPassword, "Amplify@5", strlen("Amplify@5"));
        config.gsmEnable = true;
        memcpy(config.gsmAPN, "airtelgprs.com", strlen("airtelgprs.com"));
        config.ethernetEnable = false;
        config.onlineOffline = false;
        config.online = true;
        config.offline = false;
        config.plugnplay = true;
        config.ethernetConfig = false;
        memcpy(config.ipAddress, "192.168.0.1", strlen("192.168.0.1"));
        memcpy(config.gatewayAddress, "192.168.0.1", strlen("192.168.0.1"));
        memcpy(config.dnsAddress, "8.8.8.8", strlen("8.8.8.8"));
        memcpy(config.subnetMask, "255.255.255.0", strlen("255.255.255.0"));
        memcpy(config.macAddress, "10:97:bd:22:7e:00", strlen("10:97:bd:22:7e:00"));
        config.lowCurrentTime = 30;
        config.minLowCurrentThreshold = 0.25;
        config.resumeSession = true;
        config.otaEnable = true;
        config.redirectLogs = true;
        config.CurrentGain = 0.98042;
        config.CurrentOffset = -0.02957;
        config.CurrentGain1 = 2.202;
        config.CurrentGain2 = 2.21;
        config.overVoltageThreshold = 275;
        config.underVoltageThreshold = 175;
        config.overCurrentThreshold[1] = 35;
        config.overCurrentThreshold[2] = 35;
        config.overCurrentThreshold[3] = 17;
        config.overTemperatureThreshold = 80;
        config.restoreSession = true;
        config.sessionRestoreTime = 60;
        config.factoryReset = false;
        config.timestampSync = true;
        setNULL(config.otaURLConfig);
        memcpy(config.otaURLConfig, "http://34.100.138.28:8080/ota/", strlen("http://34.100.138.28:8080/ota/"));
        config.clearAuthCache = false;
        config.gfci = false;

        // config.defaultConfig = true;
        config.vcharge_v5 = false;
        config.vcharge_lite_1_4 = true;
        config.AC1 = false;
        config.AC2 = false;
        config.infra_charger = false;
        config.sales_charger = true;

        config.Ac3s = true;
        config.Ac7s = false;
        config.Ac11s = false;
        config.Ac22s = false;
        config.Ac44s = false;
        config.Ac14D = false;
        config.Ac10DP = false;
        config.Ac10DA = false;
        config.Ac10t = false;
        config.Ac14t = false;
        config.Ac18t = false;
        config.Ac6D = false;
        config.cpDuty[1] = 53;
        config.cpDuty[2] = 100;
        config.cpDuty[3] = 100;
        config.displayType = DISP_LED;
        config.NumofDisplays = 1;
        config.NumberOfConnectors = 1;
        config.MultiConnectorDisplay = false;
        config.smartCharging = false;
        config.BatteryBackup = false;
        config.DiagnosticServer = false;
        config.ALPR = false;
        config.StopTransactionInSuspendedState = true;
        config.StopTransactionInSuspendedStateTime = 480;
        setNULL(config.adminpassword);
        memcpy(config.adminpassword, "root", strlen("root"));
        setNULL(config.factorypassword);
        memcpy(config.factorypassword, "root", strlen("root"));
        setNULL(config.servicepassword);
        memcpy(config.servicepassword, "root", strlen("root"));
        setNULL(config.customerpassword);
        memcpy(config.customerpassword, "root", strlen("root"));
        custom_nvs_write_config();
    }
    if (memcmp(config.otaURLConfig, "http://34.100.138.28:8080/ota/v1/", strlen("http://34.100.138.28:8080/ota/v1/")) == 0)
    {
        setNULL(config.otaURLConfig);
        memcpy(config.otaURLConfig, "http://34.100.138.28:8080/ota/", strlen("http://34.100.138.28:8080/ota/"));
    }

    if (config.vcharge_v5 == true)
    {
        config.ethernetEnable = false;
    }
    // config.onlineOffline = true;
    if (config.CurrentGain > 5.0)
        config.CurrentGain = 1.0;
    if (config.CurrentGain1 > 5.0)
        config.CurrentGain1 = 2.0;
    if (config.CurrentGain2 > 5.0)
        config.CurrentGain2 = 2.0;
    if (config.CurrentOffset > 5.0)
        config.CurrentOffset = 0.0;
    if (config.Ac3s || config.Ac7s || config.Ac11s || config.Ac22s || config.Ac44s)
    {
        config.NumberOfConnectors = 1;
        connectorEnabled[1] = true;
    }
    if (config.Ac10DA || config.Ac10DP || config.Ac14D || config.Ac6D)
    {
        config.NumberOfConnectors = 2;
        connectorEnabled[1] = true;
        connectorEnabled[2] = true;
    }
    if (config.Ac10t || config.Ac14t || config.Ac18t)
    {
        config.NumberOfConnectors = 3;
        connectorEnabled[1] = true;
        connectorEnabled[2] = true;
        connectorEnabled[3] = true;
    }
    if (config.NumofDisplays > config.NumberOfConnectors)
    {
        config.NumofDisplays = config.NumberOfConnectors;
    }
    if ((config.NumofDisplays == 1) && (config.NumberOfConnectors > 1) && (config.displayType == DISP_CHAR))
    {
        config.MultiConnectorDisplay = true;
    }
    else
    {
        config.MultiConnectorDisplay = false;
    }
    for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
    {
        if ((config.serverSetCpDuty[i] != 16) ||
            (config.serverSetCpDuty[i] != 26) ||
            (config.serverSetCpDuty[i] != 36) ||
            (config.serverSetCpDuty[i] != 41) ||
            (config.serverSetCpDuty[i] != 53) ||
            (config.serverSetCpDuty[i] != 66) ||
            (config.serverSetCpDuty[i] != 83) ||
            (config.serverSetCpDuty[i] != 89))
        {
            config.serverSetCpDuty[i] = 53;
        }

        config.cpDuty[i] = config.serverSetCpDuty[i];
    }

    if ((config.smartCharging == true) &&
        ((config.vcharge_v5 == true) ||
         (config.vcharge_lite_1_4 == true)))
    {
        config.smartCharging = false;
    }
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    esp_app_desc_t running_partition_description;
    esp_ota_get_partition_description(running_partition, &running_partition_description);
    setNULL(config.firmwareVersion);
    memcpy(config.firmwareVersion, running_partition_description.version, strlen(running_partition_description.version));
    ESP_LOGI(TAG, "current firmware version is %s", running_partition_description.version);
    uint8_t my_mac[6];
    esp_efuse_mac_get_default(my_mac);
    setNULL(config.macAddress);
    sprintf(config.macAddress, MACSTR, MAC2STR(my_mac));
    custom_nvs_write_config();

    char sampleURL[100];
    memset(sampleURL, '\0', sizeof(sampleURL));
    strcat(sampleURL, config.otaURLConfig);
    strcat(sampleURL, "master/");
    strcat(sampleURL, config.serialNumber);
    ESP_LOGI(TAG, "Master OTA URL: %s", sampleURL);

    ESP_LOGI(TAG, "----------------------------------------------------------------------");
    ESP_LOGI(TAG, "Configuration Parameters");
    ESP_LOGI(TAG, "----------------------------------------------------------------------");
    ESP_LOGI(TAG, "serialNumber %s", config.serialNumber);
    ESP_LOGI(TAG, "chargerName %s", config.chargerName);
    ESP_LOGI(TAG, "chargePointVendor %s", config.chargePointVendor);
    ESP_LOGI(TAG, "chargePointModel %s", config.chargePointModel);
    ESP_LOGI(TAG, "commissionedBy %s", config.commissionedBy);
    ESP_LOGI(TAG, "commissionedDate %s", config.commissionedDate);
    ESP_LOGI(TAG, "webSocketURL %s", config.webSocketURL);
    ESP_LOGI(TAG, "wifiEnable %s", (config.wifiEnable) ? "Enabled" : "Disabled");
    if (config.wifiEnable)
    {
        ESP_LOGI(TAG, "wifiPriority %d", config.wifiPriority);
        ESP_LOGI(TAG, "wifiSSID %s", config.wifiSSID);
        ESP_LOGI(TAG, "wifiPassword %s", config.wifiPassword);
    }
    ESP_LOGI(TAG, "gsmEnable %s", (config.gsmEnable) ? "Enabled" : "Disabled");
    if (config.gsmEnable)
    {
        ESP_LOGI(TAG, "gsmPriority %d", config.gsmPriority);
        ESP_LOGI(TAG, "gsmAPN %s", config.gsmAPN);
        ESP_LOGI(TAG, "simIMEINumber %s", config.simIMEINumber);
        ESP_LOGI(TAG, "simIMSINumber %s", config.simIMSINumber);
    }
    ESP_LOGI(TAG, "ethernetEnable %s", (config.ethernetEnable) ? "Enabled" : "Disabled");
    if (config.ethernetEnable)
    {
        ESP_LOGI(TAG, "ethernetPriority %d", config.ethernetPriority);
        ESP_LOGI(TAG, "ethernetConfig %s", (config.ethernetConfig) ? "Enabled" : "Disabled");
        ESP_LOGI(TAG, "ipAddress %s", config.ipAddress);
        ESP_LOGI(TAG, "gatewayAddress %s", config.gatewayAddress);
        ESP_LOGI(TAG, "dnsAddress %s", config.dnsAddress);
        ESP_LOGI(TAG, "subnetMask %s", config.subnetMask);
        ESP_LOGI(TAG, "macAddress %s", config.macAddress);
    }
    if (config.onlineOffline)
    {
        ESP_LOGI(TAG, "onlineOffline Enabled");
    }
    else if (config.online)
    {
        ESP_LOGI(TAG, "online Enabled");
    }
    else if (config.offline)
    {
        ESP_LOGI(TAG, "offline Enabled");
        if (config.plugnplay)
        {
            ESP_LOGI(TAG, "plugnplay Enabled");
        }
    }
    else
    {
        ESP_LOGI(TAG, "plugnplay Enabled");
    }

    ESP_LOGI(TAG, "lowCurrentTime %d", config.lowCurrentTime);
    ESP_LOGI(TAG, "minLowCurrentThreshold %lf", config.minLowCurrentThreshold);
    ESP_LOGI(TAG, "resumeSession %s", (config.resumeSession) ? "Enabled" : "Disabled");
    ESP_LOGI(TAG, "CurrentGain %lf", config.CurrentGain);
    ESP_LOGI(TAG, "CurrentOffset %lf", config.CurrentOffset);
    ESP_LOGI(TAG, "CurrentGain1 (0-8.5A): %lf", config.CurrentGain1);
    ESP_LOGI(TAG, "CurrentGain2 (>8.5A): %lf", config.CurrentGain2);
    ESP_LOGI(TAG, "overVoltageThreshold %lf", config.overVoltageThreshold);
    ESP_LOGI(TAG, "underVoltageThreshold %lf", config.underVoltageThreshold);
    ESP_LOGI(TAG, "overCurrentThreshold %lf", config.overCurrentThreshold[1]);
    ESP_LOGI(TAG, "overCurrentThreshold %lf", config.overCurrentThreshold[2]);
    ESP_LOGI(TAG, "overCurrentThreshold %lf", config.overCurrentThreshold[3]);
    ESP_LOGI(TAG, "overTemperatureThreshold %lf", config.overTemperatureThreshold);
    ESP_LOGI(TAG, "restoreSession %s", (config.restoreSession) ? "Enabled" : "Disabled");
    ESP_LOGI(TAG, "sessionRestoreTime %d", config.sessionRestoreTime);
    ESP_LOGI(TAG, "factoryReset %s", (config.factoryReset) ? "Enabled" : "Disabled");
    ESP_LOGI(TAG, "otaURLConfig %s", sampleURL);
    ESP_LOGI(TAG, "clearAuthCache %s", (config.clearAuthCache) ? "Enabled" : "Disabled");
    ESP_LOGI(TAG, "gfci %s", (config.gfci) ? "Enabled" : "Disabled");
    if (config.vcharge_lite_1_4)
    {
        ESP_LOGI(TAG, "vcharge_lite_1_4 Enabled");
    }
    else if (config.vcharge_v5)
    {
        ESP_LOGI(TAG, "vcharge_v5 Enabled");
    }
    else if (config.AC1)
    {
        ESP_LOGI(TAG, "AC1 Enabled");
    }
    else // if (config.AC2)
    {
        ESP_LOGI(TAG, "AC2 Enabled");
    }
    // ESP_LOGI(TAG, "infra_charger %s", (config.infra_charger) ? "Enabled" : "Disabled");
    // ESP_LOGI(TAG, "sales_charger %s", (config.sales_charger) ? "Enabled" : "Disabled");

    if (config.Ac3s)
    {
        ESP_LOGI(TAG, "Ac3s Enabled");
    }
    else if (config.Ac7s)
    {
        ESP_LOGI(TAG, "Ac7s Enabled");
    }
    else if (config.Ac11s)
    {
        ESP_LOGI(TAG, "Ac11s Enabled");
    }
    else if (config.Ac22s)
    {
        ESP_LOGI(TAG, "Ac22s Enabled");
    }
    else if (config.Ac44s)
    {
        ESP_LOGI(TAG, "Ac44s Enabled");
    }
    else if (config.Ac14D)
    {
        ESP_LOGI(TAG, "Ac14D Enabled");
    }
    else if (config.Ac10DP)
    {
        ESP_LOGI(TAG, "Ac10DP Enabled");
    }
    else if (config.Ac10DA)
    {
        ESP_LOGI(TAG, "Ac10DA Enabled");
    }
    else if (config.Ac6D)
    {
        ESP_LOGI(TAG, "Ac6D Enabled");
    }
    else if (config.Ac10t)
    {
        ESP_LOGI(TAG, "Ac10t Enabled");
    }
    else if (config.Ac14t)
    {
        ESP_LOGI(TAG, "Ac14t Enabled");
    }
    else if (config.Ac18t)
    {
        ESP_LOGI(TAG, "Ac18t Enabled");
    }
    else
    {
        ESP_LOGI(TAG, "Charger Type Not Yet Selected");
    }

    if (config.displayType == DISP_LED)
    {
        ESP_LOGI(TAG, "Display Type LED");
    }
    else if (config.displayType == DISP_CHAR)
    {
        ESP_LOGI(TAG, "Display Type 20x4 Character");
    }
    else if (config.displayType == DISP_DWIN)
    {
        ESP_LOGI(TAG, "Display Type DWIN");
    }
    else
    {
        ESP_LOGI(TAG, "displayType Not Selected");
    }

    if (config.displayType == DISP_CHAR)
        ESP_LOGI(TAG, "Number of Displays %d", config.NumofDisplays);
    ESP_LOGI(TAG, "NumberOfConnectors %d", config.NumberOfConnectors);
    ESP_LOGI(TAG, "MultiConnectorDisplay %s", (config.MultiConnectorDisplay) ? "Enabled" : "Disabled");
    ESP_LOGI(TAG, "Connector 1 Control Pilot Duty %d", config.cpDuty[1]);
    ESP_LOGI(TAG, "Connector 2 Control Pilot Duty %d", config.cpDuty[2]);
    ESP_LOGI(TAG, "Connector 3 Control Pilot Duty %d", config.cpDuty[3]);

    ESP_LOGI(TAG, "Smart Charging %s", (config.smartCharging) ? "Enabled" : "Disabled");
    ESP_LOGI(TAG, "Battery Backup %s", (config.BatteryBackup) ? "Enabled" : "Disabled");
    ESP_LOGI(TAG, "Diagnostic Server %s", (config.DiagnosticServer) ? "Enabled" : "Disabled");
    if (config.DiagnosticServer)
        ESP_LOGI(TAG, "Daignostic Server URL: %s", config.DiagnosticServerUrl);
    ESP_LOGI(TAG, "Stop Transaction In Suspended State %s", (config.StopTransactionInSuspendedState) ? "Enabled" : "Disabled");
    if (config.StopTransactionInSuspendedState)
        ESP_LOGI(TAG, "Stop Transaction In Suspended State Time %d", config.StopTransactionInSuspendedStateTime);
    ESP_LOGI(TAG, "ALPR %s", (config.ALPR) ? "Enabled" : "Disabled");

    ESP_LOGI(TAG, "----------------------------------------------------------------------");
}

void app_main(void)
{
    custom_nvs_init();
    esp_log_level_set("CUSTOM_NVS", ESP_LOG_WARN);
    esp_log_level_set("ESP_NOW", ESP_LOG_WARN);
    log_spiffs_init();
    init_config();
    if (config.vcharge_lite_1_4)
    {
        slave_serial_init();
    }
    setSystemTime();
    Display_Init();
    if (config.AC1)
    {
        GpioExpanderInit();
    }
    if (config.vcharge_v5 || config.AC1)
    {
        LED_Strip();
    }
    fault_gpios_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }

    StartServerOnPowerOn();

    free_heap_size = esp_get_free_heap_size();
    ESP_LOGI(TAG, "Before Wifi init free heap size %d ", free_heap_size / 1024);
    if (config.offline == false)
    {
        initialise_interfaces();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    free_heap_size = esp_get_free_heap_size();

    ControlPilotAdcInit();
    if (config.vcharge_v5 || config.AC1)
    {
        ControlPilotPwmInit();
    }
    EnergyMeter_init();
    readEnrgyMeterValues();
    readEnrgyMeterValues();

    ocpp_task_init();
    RFID_Init();

    // esp_log_level_set("TRANSPORT", ESP_LOG_DEBUG);
    // esp_log_level_set("WEBSOCKET_CLIENT", ESP_LOG_DEBUG);
    esp_log_level_set("INTERFACE", ESP_LOG_WARN);
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("ppp_esp_modem", ESP_LOG_WARN);
    // esp_log_level_set("CP_ADC", ESP_LOG_WARN);
    esp_log_level_set("transport_base", ESP_LOG_WARN);
    esp_log_level_set("esp-tls", ESP_LOG_WARN);
    esp_log_level_set("transport_ws", ESP_LOG_WARN);
    esp_log_level_set("OCPP_CORE", ESP_LOG_WARN);
    esp_log_level_set("DISPLAY", ESP_LOG_WARN);
    esp_log_level_set("websocket_client", ESP_LOG_WARN);
    // esp_log_level_set("OCPP_TASK_DEBUG", ESP_LOG_WARN);
    free_heap_size = esp_get_free_heap_size();
    ESP_LOGI(TAG, "After all peripheral init free heap size %d kilobytes", free_heap_size / 1024);

#if TEST
    if (config.redirectLogs == false)
        sendLogsToCloud();
#endif
    while (true)
    {
        ESP_LOGI(TAG, "heap size %ld ", esp_get_free_heap_size());
        togglewatchdog();
        LCD_DisplayReinit();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        LCD_DisplayReinit();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        LCD_DisplayReinit();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        LCD_DisplayReinit();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        LCD_DisplayReinit();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        LCD_DisplayReinit();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
