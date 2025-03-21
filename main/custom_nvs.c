#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "ProjectSettings.h"
#include "custom_nvs.h"
#include "ocpp_task.h"
#include "ocpp.h"
#include "rtc_clock.h"

#define TAG_DEBUG "CN_TEST"
#define PARTITION_NAME "Mynvs"

#define WIFI_NAMESPACE "wifi"
#define WIFI_SSID "ssid"
#define WIFI_PASSWORD "password"
#define WIFI_DNS "dns"
#define WEBSOCKET_URL "wbUrl"
#define TIME_BUFFER "timeBuffer"

#define CONFIG_NAMESPACE "config"
#define CONFIG_DATA "configData"
#define RESTART_REASON "restartReason"
#define SLAVE_MAC0 "slaveMac0"
#define SLAVE_MAC1 "slaveMac1"
#define SLAVE_MAC2 "slaveMac2"
#define SLAVE_MAC3 "slaveMac3"
#define SLAVE_MAC4 "slaveMac4"
#define SLAVE_MAC5 "slaveMac5"

#define LOG_FILENO0 "LOG_FNO0"
#define LOG_FILENO1 "LOG_FNO1"
#define LOG_FILENO2 "LOG_FNO2"
#define LOG_FILENO3 "LOG_FNO3"

#define OCPP_NAMESPACE "OCPP"
#define CONFIG_DATA_OCPP "configDataOcpp"
#define LOCAL_LIST_OCPP "localListOcpp"
#define LOCAL_AUTH_LIST_OCPP "LocalAuthList"

#define CONNECTOR_DATA_NAMESPACE "connectorData"

#define CHARGING_PROGILE_NAMESPACE "chargProfile"
#define CHARGING_PROGILE_COUNT "cpcount"

#define OFFLINE_DATA_NAMESPACE "offlineData"
#define CONNECTOR1_OFFLINE_DATA_COUNT "OData1count"
#define CONNECTOR2_OFFLINE_DATA_COUNT "OData2count"
#define CONNECTOR3_OFFLINE_DATA_COUNT "OData3count"

#define MAX_OFFLINE_TRANSACTIONS_FOR_CONNECTOR 100
#define MAX_CHARGING_PROFILES 1000

#define TAG "CUSTOM_NVS"
#define TAG_SC "PROFILE"

extern uint32_t logFileNo;
extern Config_t config;
extern uint8_t esp_slave_mac[6];
extern ChargingProfiles_t ChargingProfile[NUM_OF_CONNECTORS];
extern ChargingProfiles_t ChargePointMaxChargingProfile[NUM_OF_CONNECTORS];
extern ConnectorPrivateData_t ConnectorData[NUM_OF_CONNECTORS];
extern ConnectorPrivateData_t ConnectorOfflineData[NUM_OF_CONNECTORS];
extern CMSSendLocalListRequest_t CMSSendLocalListRequest;
extern CPGetConfigurationResponse_t CPGetConfigurationResponse;
extern LocalAuthorizationList_t LocalAuthorizationList;
char *CONNECTOR_DATA[4];
extern uint32_t connectorsOfflineDataCount[NUM_OF_CONNECTORS];
extern bool updatingOfflineData[NUM_OF_CONNECTORS];
extern char time_buffer[50];
extern bool RtcTimeSet;
extern bool ChargingProfileAddedOrDeleted;

void logChargingProfile(ChargingProfiles_t ChargingProfile)
{
    ESP_LOGI(TAG_SC, "--------------------------------------------------");
    ESP_LOGI(TAG_SC, "Charging Profile");
    ESP_LOGI(TAG_SC, "--------------------------------------------------");
    ESP_LOGI(TAG_SC, "Connector Id %hhu", ChargingProfile.connectorId);
    ESP_LOGI(TAG_SC, "Profile Id %ld", ChargingProfile.csChargingProfiles.chargingProfileId);
    ESP_LOGI(TAG_SC, "Transaction Id %ld", ChargingProfile.csChargingProfiles.transactionId);
    ESP_LOGI(TAG_SC, "Stack Level %ld", ChargingProfile.csChargingProfiles.stackLevel);
    if (ChargingProfile.csChargingProfiles.chargingProfilePurpose == ChargePointMaxProfile)
    {
        ESP_LOGI(TAG_SC, "Purpose ChargePointMaxProfile");
    }
    else if (ChargingProfile.csChargingProfiles.chargingProfilePurpose == TxDefaultProfile)
    {
        ESP_LOGI(TAG_SC, "Purpose TxDefaultProfile");
    }
    else if (ChargingProfile.csChargingProfiles.chargingProfilePurpose == TxProfile)
    {
        ESP_LOGI(TAG_SC, "Purpose TxProfile");
    }
    else
    {
        ESP_LOGI(TAG_SC, "Purpose Unknown");
    }

    if (ChargingProfile.csChargingProfiles.chargingProfileKind == Absolute)
    {
        ESP_LOGI(TAG_SC, "Kind Absolute");
    }
    else if (ChargingProfile.csChargingProfiles.chargingProfileKind == Recurring)
    {
        ESP_LOGI(TAG_SC, "Kind Recurring");
        if (ChargingProfile.csChargingProfiles.recurrencyKind == Daily)
        {
            ESP_LOGI(TAG_SC, "Recurrency Daily");
        }
        else if (ChargingProfile.csChargingProfiles.recurrencyKind == Weekly)
        {
            ESP_LOGI(TAG_SC, "Recurrency Weekly");
        }
        else
        {
            ESP_LOGI(TAG_SC, "Recurrency Unknown");
        }
    }
    else if (ChargingProfile.csChargingProfiles.chargingProfileKind == Relative)
    {
        ESP_LOGI(TAG_SC, "Kind Relative");
    }
    else
    {
        ESP_LOGI(TAG_SC, "Kind Unknown");
    }

    if (ChargingProfile.csChargingProfiles.validFrom > 0)
    {
        struct timeval tv;
        struct tm timeinfo;
        char time_buffer[50];
        tv.tv_sec = ChargingProfile.csChargingProfiles.validFrom;
        localtime_r(&tv.tv_sec, &timeinfo);
        setNULL(time_buffer);
        snprintf(time_buffer, sizeof(time_buffer),
                 "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
                 (timeinfo.tm_year + 1900),
                 timeinfo.tm_mon + 1,
                 timeinfo.tm_mday,
                 timeinfo.tm_hour,
                 timeinfo.tm_min,
                 timeinfo.tm_sec,
                 tv.tv_usec / 1000);
        ESP_LOGI(TAG_SC, "Valid From %s", time_buffer);
    }

    if (ChargingProfile.csChargingProfiles.validTo > 0)
    {
        struct timeval tv;
        struct tm timeinfo;
        char time_buffer[50];
        tv.tv_sec = ChargingProfile.csChargingProfiles.validTo;
        localtime_r(&tv.tv_sec, &timeinfo);
        setNULL(time_buffer);
        snprintf(time_buffer, sizeof(time_buffer),
                 "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
                 (timeinfo.tm_year + 1900),
                 timeinfo.tm_mon + 1,
                 timeinfo.tm_mday,
                 timeinfo.tm_hour,
                 timeinfo.tm_min,
                 timeinfo.tm_sec,
                 tv.tv_usec / 1000);
        ESP_LOGI(TAG_SC, "Valid From %s", time_buffer);
    }

    if (ChargingProfile.csChargingProfiles.chargingSchedule.duration > 0)
        ESP_LOGI(TAG_SC, " Schedule Duration %ld", ChargingProfile.csChargingProfiles.chargingSchedule.duration);
    if (ChargingProfile.csChargingProfiles.chargingSchedule.startSchedule > 0)
    {
        struct timeval tv;
        struct tm timeinfo;
        char time_buffer[50];
        tv.tv_sec = ChargingProfile.csChargingProfiles.chargingSchedule.startSchedule;
        localtime_r(&tv.tv_sec, &timeinfo);
        setNULL(time_buffer);
        snprintf(time_buffer, sizeof(time_buffer),
                 "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
                 (timeinfo.tm_year + 1900),
                 timeinfo.tm_mon + 1,
                 timeinfo.tm_mday,
                 timeinfo.tm_hour,
                 timeinfo.tm_min,
                 timeinfo.tm_sec,
                 tv.tv_usec / 1000);
        ESP_LOGI(TAG_SC, "Schedule Start %s", time_buffer);
    }

    if (ChargingProfile.csChargingProfiles.chargingSchedule.chargingRateUnit == chargingRateUnitAmpere)
    {
        ESP_LOGI(TAG_SC, "Schedule Charging Rate Unit Ampere");
    }
    else if (ChargingProfile.csChargingProfiles.chargingSchedule.chargingRateUnit == chargingRateUnitWatt)
    {
        ESP_LOGI(TAG_SC, "Schedule Charging Rate Unit Watt");
    }
    else
    {
        ESP_LOGI(TAG_SC, "Schedule Charging Rate Unit Unknown");
    }

    if (ChargingProfile.csChargingProfiles.chargingSchedule.minChargingRate > 0)
        ESP_LOGI(TAG_SC, "Schedule Min Charging Rate %.2f", ChargingProfile.csChargingProfiles.chargingSchedule.minChargingRate);

    for (uint8_t i = 0; i < SchedulePeriodCount; i++)
    {
        if (ChargingProfile.csChargingProfiles.chargingSchedule.chargingSchedulePeriod[i].schedulePresent)
        {
            ESP_LOGI(TAG_SC, "Schedule Period %hhu", i);
            ESP_LOGI(TAG_SC, "Schedule Period Start %ld", ChargingProfile.csChargingProfiles.chargingSchedule.chargingSchedulePeriod[i].startPeriod);
            ESP_LOGI(TAG_SC, "Schedule Period Limit %.2f", ChargingProfile.csChargingProfiles.chargingSchedule.chargingSchedulePeriod[i].limit);
        }
    }

    ESP_LOGI(TAG_SC, "--------------------------------------------------");
}

void setSystemTime(void)
{
    struct tm timeRTC;
    struct tm timeNVS;
    time_t unix_time_RTC;
    time_t unix_time_NVS;

    custom_nvs_read_timeBuffer();
    if (config.vcharge_v5 || config.AC1)
    {
        sscanf(time_buffer, "%d-%d-%dT%d:%d:%d.%*3sZ",
               &timeNVS.tm_year, &timeNVS.tm_mon, &timeNVS.tm_mday,
               &timeNVS.tm_hour, &timeNVS.tm_min, &timeNVS.tm_sec);

        timeNVS.tm_year -= 1900; // Adjust year
        timeNVS.tm_mon -= 1;     // Adjust month

        unix_time_NVS = mktime(&timeNVS);

        RTC_Clock_init();
        timeRTC = RTC_Clock_getTime();
        unix_time_RTC = mktime(&timeRTC);

        if (unix_time_NVS < unix_time_RTC)
        {
            struct timeval tv;
            tv.tv_usec = 0;
            ESP_LOGI(TAG, "Setting RTC Time");
            RtcTimeSet = true;
            setNULL(time_buffer);
            snprintf(time_buffer, sizeof(time_buffer),
                     "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
                     (timeRTC.tm_year + 1900),
                     timeRTC.tm_mon + 1,
                     timeRTC.tm_mday,
                     timeRTC.tm_hour,
                     timeRTC.tm_min,
                     timeRTC.tm_sec,
                     tv.tv_usec / 1000);
        }
    }
    parse_timestamp(time_buffer);
}

void custom_nvs_init(void)
{

    CONNECTOR_DATA[0] = "connector0Data";
    CONNECTOR_DATA[1] = "connector1Data";
    CONNECTOR_DATA[2] = "connector2Data";
    CONNECTOR_DATA[3] = "connector3Data";
    nvs_flash_init_partition(PARTITION_NAME);
}

esp_err_t custom_nvs_read_localist_ocpp(void)
{
    ESP_LOGI(TAG, "Reading localListOcpp from namespace OCPP");
    nvs_handle handle;
    size_t localListDataSize = sizeof(CMSSendLocalListRequest_t);
    esp_err_t err = nvs_open_from_partition(PARTITION_NAME, OCPP_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open namespace OCPP, error = %d", err);
        nvs_close(handle);
        return err;
    }
    err = nvs_get_blob(handle, LOCAL_LIST_OCPP, (void *)&CMSSendLocalListRequest, &localListDataSize);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get localListOcpp, error = %d", err);
    }
    ESP_LOGI(TAG, "localListOcpp read successfully");
    nvs_close(handle);
    return err;
}

void custom_nvs_write_localist_ocpp(void)
{
    ESP_LOGI(TAG, "Writing LocalList_ocpp to NVS");
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(PARTITION_NAME, OCPP_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for writing LocalList_ocpp (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    err = nvs_set_blob(handle, LOCAL_LIST_OCPP, (void *)&CMSSendLocalListRequest, sizeof(CMSSendLocalListRequest_t));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write LocalList_ocpp to NVS (%s)", esp_err_to_name(err));
    }

    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit LocalList_ocpp to NVS (%s)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "LocalList_ocpp written to NVS");
    }
    nvs_close(handle);
}

esp_err_t custom_nvs_read_LocalAuthorizationList_ocpp(void)
{
    ESP_LOGI(TAG, "Reading LocalAuthorizationList_ocpp from NVS");
    nvs_handle handle;
    size_t localListDataSize = sizeof(LocalAuthorizationList_t);
    esp_err_t err = nvs_open_from_partition(PARTITION_NAME, OCPP_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for reading LocalAuthorizationList_ocpp (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_get_blob(handle, LOCAL_AUTH_LIST_OCPP, (void *)&LocalAuthorizationList, &localListDataSize);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read LocalAuthorizationList_ocpp from NVS (%s)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "LocalAuthorizationList_ocpp read from NVS, size = %d", localListDataSize);
    }

    nvs_close(handle);
    return err;
}

void custom_nvs_write_LocalAuthorizationList_ocpp(void)
{
    ESP_LOGI(TAG, "Writing LocalAuthorizationList_ocpp to NVS");
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(PARTITION_NAME, OCPP_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for writing LocalAuthorizationList_ocpp (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    err = nvs_set_blob(handle, LOCAL_AUTH_LIST_OCPP, (void *)&LocalAuthorizationList, sizeof(LocalAuthorizationList_t));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write LocalAuthorizationList_ocpp to NVS (%s)", esp_err_to_name(err));
    }

    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit LocalAuthorizationList_ocpp to NVS (%s)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "LocalAuthorizationList_ocpp written to NVS");
    }
    nvs_close(handle);
}

esp_err_t custom_nvs_read_config_ocpp(void)
{
    ESP_LOGI(TAG, "Reading ConfigData from namespace OCPP_NAMESPACE");
    nvs_handle handle;
    size_t ConfigDataSize = sizeof(CPGetConfigurationResponse_t);
    esp_err_t err = nvs_open_from_partition(PARTITION_NAME, OCPP_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for reading ConfigData (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_get_blob(handle, CONFIG_DATA_OCPP, (void *)&CPGetConfigurationResponse, &ConfigDataSize);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get ConfigData from NVS (%s)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "ConfigData read from NVS, size = %d", ConfigDataSize);
    }

    nvs_close(handle);
    return err;
}

void custom_nvs_write_config_ocpp(void)
{
    ESP_LOGI(TAG, "Writing ConfigData to namespace OCPP_NAMESPACE");
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(PARTITION_NAME, OCPP_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for writing ConfigData (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    err = nvs_set_blob(handle, CONFIG_DATA_OCPP, (void *)&CPGetConfigurationResponse, sizeof(CPGetConfigurationResponse_t));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write ConfigData to NVS (%s)", esp_err_to_name(err));
    }

    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit ConfigData to NVS (%s)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "ConfigData written to NVS");
    }
    nvs_close(handle);
}

void custom_nvs_read_config(void)
{
    ESP_LOGI(TAG, "Reading ConfigData from namespace CONFIG_NAMESPACE");
    nvs_handle handle;
    size_t ConfigDataSize = sizeof(Config_t);
    esp_err_t err = nvs_open_from_partition(PARTITION_NAME, CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for reading ConfigData (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    err = nvs_get_blob(handle, CONFIG_DATA, (void *)&config, &ConfigDataSize);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read ConfigData from NVS (%s)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "ConfigData read from NVS, size: %d", ConfigDataSize);
    }

    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit read ConfigData to NVS (%s)", esp_err_to_name(err));
    }
    nvs_close(handle);
}

void custom_nvs_write_config(void)
{
    nvs_handle handle;
    ESP_LOGI(TAG, "Writing ConfigData to namespace %s", CONFIG_NAMESPACE);
    // config.defaultConfig = true;
    nvs_open_from_partition(PARTITION_NAME, CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    ESP_LOGI(TAG, "Opened NVS handle for writing ConfigData");

    esp_err_t err = nvs_set_blob(handle, CONFIG_DATA, (void *)&config, sizeof(Config_t));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write ConfigData to NVS (%s)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "ConfigData written to NVS");
    }

    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit ConfigData to NVS (%s)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "ConfigData committed to NVS");
    }
    nvs_close(handle);
}

esp_err_t custom_nvs_read_slave_mac(void)
{
    ESP_LOGI(TAG, "Reading Slave Mac from NVS");
    nvs_handle handle;
    nvs_open_from_partition(PARTITION_NAME, CONFIG_NAMESPACE, NVS_READWRITE, &handle);

    esp_err_t result = nvs_get_u8(handle, SLAVE_MAC0, &esp_slave_mac[0]);
    if (result == ESP_OK)
    {
        ESP_LOGI(TAG, "Read Slave Mac byte 0 from NVS");
    }
    if (result == ESP_OK)
    {
        result = nvs_get_u8(handle, SLAVE_MAC1, &esp_slave_mac[1]);
        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "Read Slave Mac byte 1 from NVS");
        }
    }
    if (result == ESP_OK)
    {
        result = nvs_get_u8(handle, SLAVE_MAC2, &esp_slave_mac[2]);
        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "Read Slave Mac byte 2 from NVS");
        }
    }
    if (result == ESP_OK)
    {
        result = nvs_get_u8(handle, SLAVE_MAC3, &esp_slave_mac[3]);
        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "Read Slave Mac byte 3 from NVS");
        }
    }
    if (result == ESP_OK)
    {
        result = nvs_get_u8(handle, SLAVE_MAC4, &esp_slave_mac[4]);
        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "Read Slave Mac byte 4 from NVS");
        }
    }
    if (result == ESP_OK)
    {
        result = nvs_get_u8(handle, SLAVE_MAC5, &esp_slave_mac[5]);
        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "Read Slave Mac byte 5 from NVS");
        }
    }
    ESP_LOGI(TAG, "Read Slave Mac from NVS, result: %s", esp_err_to_name(result));
    nvs_close(handle);
    return result;
}

void custom_nvs_write_slave_mac(void)
{
    ESP_LOGI(TAG, "Writing Slave Mac to NVS");
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(PARTITION_NAME, CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for writing Slave Mac (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    ESP_LOGI(TAG, "Writing Slave Mac byte 0 to NVS");
    err = nvs_set_u8(handle, SLAVE_MAC0, esp_slave_mac[0]);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write Slave Mac byte 0 to NVS (%s)", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Writing Slave Mac byte 1 to NVS");
    err = nvs_set_u8(handle, SLAVE_MAC1, esp_slave_mac[1]);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write Slave Mac byte 1 to NVS (%s)", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Writing Slave Mac byte 2 to NVS");
    err = nvs_set_u8(handle, SLAVE_MAC2, esp_slave_mac[2]);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write Slave Mac byte 2 to NVS (%s)", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Writing Slave Mac byte 3 to NVS");
    err = nvs_set_u8(handle, SLAVE_MAC3, esp_slave_mac[3]);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write Slave Mac byte 3 to NVS (%s)", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Writing Slave Mac byte 4 to NVS");
    err = nvs_set_u8(handle, SLAVE_MAC4, esp_slave_mac[4]);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write Slave Mac byte 4 to NVS (%s)", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Writing Slave Mac byte 5 to NVS");
    err = nvs_set_u8(handle, SLAVE_MAC5, esp_slave_mac[5]);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write Slave Mac byte 5 to NVS (%s)", esp_err_to_name(err));
    }

    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit Slave Mac to NVS (%s)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Slave Mac written to NVS");
    }
    nvs_close(handle);
}

void custom_nvs_write_restart_reason(uint8_t restart_reason)
{
    ESP_LOGI(TAG, "Writing Restart Reason to NVS");
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(PARTITION_NAME, CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for writing Restart Reason (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    ESP_LOGI(TAG, "Writing Restart Reason byte 0 to NVS");
    err = nvs_set_u8(handle, RESTART_REASON, restart_reason);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write Restart Reason byte 0 to NVS (%s)", esp_err_to_name(err));
    }

    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit Restart Reason to NVS (%s)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Restart Reason written to NVS");
    }
    nvs_close(handle);
}

esp_err_t custom_nvs_read_restart_reason(uint8_t *restart_reason)
{
    ESP_LOGI(TAG, "Reading Restart Reason from NVS");
    nvs_handle handle;
    nvs_open_from_partition(PARTITION_NAME, CONFIG_NAMESPACE, NVS_READWRITE, &handle);

    esp_err_t result = nvs_get_u8(handle, RESTART_REASON, restart_reason);
    if (result == ESP_OK)
    {
        ESP_LOGI(TAG, "Read Restart Reason byte 0 from NVS");
    }

    nvs_close(handle);
    return result;
}

esp_err_t custom_nvs_read_logFileNo(void)
{
    ESP_LOGD(TAG, "Reading Log File Number from NVS");
    nvs_handle handle;
    uint8_t log_file_no[4];

    nvs_open_from_partition(PARTITION_NAME, CONFIG_NAMESPACE, NVS_READWRITE, &handle);

    esp_err_t result = nvs_get_u8(handle, LOG_FILENO0, &log_file_no[0]);
    if (result == ESP_OK)
    {
        ESP_LOGD(TAG, "Read Log File Number byte 0 from NVS");
    }
    if (result == ESP_OK)
    {
        result = nvs_get_u8(handle, LOG_FILENO1, &log_file_no[1]);
        if (result == ESP_OK)
        {
            ESP_LOGD(TAG, "Read Log File Number byte 1 from NVS");
        }
    }
    if (result == ESP_OK)
    {
        result = nvs_get_u8(handle, LOG_FILENO2, &log_file_no[2]);
        if (result == ESP_OK)
        {
            ESP_LOGD(TAG, "Read Log File Number byte 2 from NVS");
        }
    }
    if (result == ESP_OK)
    {
        result = nvs_get_u8(handle, LOG_FILENO3, &log_file_no[3]);
        if (result == ESP_OK)
        {
            ESP_LOGD(TAG, "Read Log File Number byte 3 from NVS");
        }
    }

    if (result != ESP_OK)
    {
        logFileNo = 0;
        custom_nvs_write_logFileNo();
    }
    else
    {
        logFileNo = log_file_no[0] | (log_file_no[1] << 8) | (log_file_no[2] << 16) | (log_file_no[3] << 24);
    }

    ESP_LOGD(TAG, "Read Log File Number from NVS, result: %s", esp_err_to_name(result));
    nvs_close(handle);
    return result;
}

void custom_nvs_write_logFileNo(void)
{
    ESP_LOGD(TAG, "Writing Log File Number to NVS");
    nvs_handle handle;
    uint8_t log_file_no[4];
    log_file_no[0] = logFileNo & 0xFF;
    log_file_no[1] = (logFileNo >> 8) & 0xFF;
    log_file_no[2] = (logFileNo >> 16) & 0xFF;
    log_file_no[3] = (logFileNo >> 24) & 0xFF;
    esp_err_t err = nvs_open_from_partition(PARTITION_NAME, CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for writing Log File Number (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    ESP_LOGD(TAG, "Writing Log File Number byte 0 to NVS");
    err = nvs_set_u8(handle, LOG_FILENO0, log_file_no[0]);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write Log File Number byte 0 to NVS (%s)", esp_err_to_name(err));
    }

    ESP_LOGD(TAG, "Writing Log File Number byte 1 to NVS");
    err = nvs_set_u8(handle, LOG_FILENO1, log_file_no[1]);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write Log File Number byte 1 to NVS (%s)", esp_err_to_name(err));
    }

    ESP_LOGD(TAG, "Writing Log File Number byte 2 to NVS");
    err = nvs_set_u8(handle, LOG_FILENO2, log_file_no[2]);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write Log File Number byte 2 to NVS (%s)", esp_err_to_name(err));
    }

    ESP_LOGD(TAG, "Writing Log File Number byte 3 to NVS");
    err = nvs_set_u8(handle, LOG_FILENO3, log_file_no[3]);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write Log File Number byte 3 to NVS (%s)", esp_err_to_name(err));
    }

    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit Log File Number to NVS (%s)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGD(TAG, "Log File Number written to NVS");
    }
    nvs_close(handle);
}

uint32_t custome_nvs_read_chargingProfilesCount(void)
{
    nvs_handle handle;
    esp_err_t ret = ESP_OK;
    uint32_t chargingProfilesCount = 0;
    nvs_open_from_partition(PARTITION_NAME, CHARGING_PROGILE_NAMESPACE, NVS_READWRITE, &handle);
    ret = nvs_get_u32(handle, CHARGING_PROGILE_COUNT, &chargingProfilesCount);
    if (ret != ESP_OK)
    {
        chargingProfilesCount = 0;
    }
    nvs_close(handle);

    return chargingProfilesCount;
}

void custome_nvs_write_chargingProfilesCount(uint32_t chargingProfilesCount)
{
    nvs_handle handle;
    esp_err_t ret = ESP_OK;
    nvs_open_from_partition(PARTITION_NAME, CHARGING_PROGILE_NAMESPACE, NVS_READWRITE, &handle);
    ret = nvs_set_u32(handle, CHARGING_PROGILE_COUNT, chargingProfilesCount);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Charging Profile Count Error (%s) writing NVS handle!\n", esp_err_to_name(ret));
    }
    nvs_commit(handle);
    nvs_close(handle);
}

void custom_nvs_write_chargingProgile(ChargingProfiles_t ChargingProfile)
{
    nvs_handle handle;
    esp_err_t ret = ESP_OK;
    char chargingprofilesavingkey[50];
    setNULL(chargingprofilesavingkey);
    uint32_t chargingProfilesCount = custome_nvs_read_chargingProfilesCount();
    size_t chargingProfilesSize = sizeof(ChargingProfiles_t);
    nvs_open_from_partition(PARTITION_NAME, CHARGING_PROGILE_NAMESPACE, NVS_READWRITE, &handle);

    if (chargingProfilesCount >= MAX_CHARGING_PROFILES)
    {
        for (uint32_t i = 1; i < chargingProfilesCount; i++)
        {
            sprintf(chargingprofilesavingkey, "cpcount%ld", i + (uint32_t)1);
            nvs_get_blob(handle, chargingprofilesavingkey, (void *)&ChargingProfile, &chargingProfilesSize);
            sprintf(chargingprofilesavingkey, "cpcount%ld", i);
            nvs_set_blob(handle, chargingprofilesavingkey, (void *)&ChargingProfile, sizeof(ChargingProfiles_t));
        }
    }
    else
    {
        chargingProfilesCount++;
    }
    sprintf(chargingprofilesavingkey, "cpcount%ld", chargingProfilesCount);
    ESP_LOGI(TAG, "write key: %s", chargingprofilesavingkey);
    ret = nvs_set_blob(handle, chargingprofilesavingkey, (void *)&ChargingProfile, sizeof(ChargingProfiles_t));
    if (ret != ESP_OK)
    {
        ESP_LOGI(TAG, "Charging Profile Error (%s) writing NVS handle!\n", esp_err_to_name(ret));
    }
    ret = nvs_set_u32(handle, CHARGING_PROGILE_COUNT, chargingProfilesCount);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Charging Profile Count Error (%s) writing NVS handle!\n", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "charging profiles count %ld", chargingProfilesCount);
    nvs_commit(handle);
    nvs_close(handle);
    ChargingProfileAddedOrDeleted = true;
}

ChargingProfiles_t custom_nvs_read_charging_profile(uint32_t chargingProfilesCount)
{
    ChargingProfiles_t ChargingProfile;
    nvs_handle handle;
    esp_err_t ret = ESP_OK;
    char chargingprofilesavingkey[50];
    setNULL(chargingprofilesavingkey);
    size_t chargingProfilesSize = sizeof(ChargingProfiles_t);
    nvs_open_from_partition(PARTITION_NAME, CHARGING_PROGILE_NAMESPACE, NVS_READWRITE, &handle);
    sprintf(chargingprofilesavingkey, "cpcount%ld", chargingProfilesCount);
    nvs_get_blob(handle, chargingprofilesavingkey, (void *)&ChargingProfile, &chargingProfilesSize);
    nvs_close(handle);
    return ChargingProfile;
}

ChargingProfiles_t custom_nvs_get_max_stack_level_charging_profile(uint8_t connId)
{
    ChargingProfiles_t ChargingProfile;
    memset(&ChargingProfile, 0, sizeof(ChargingProfiles_t));
    uint32_t chargingProfilesCount = custome_nvs_read_chargingProfilesCount();
    uint32_t MaxStackLevel = 0;
    uint32_t MaxStackLevelChargingProfileCount = 0;
    for (uint32_t i = 1; i <= chargingProfilesCount; i++)
    {
        ChargingProfile = custom_nvs_read_charging_profile(i);
        if ((ChargingProfile.connectorId == connId) || (ChargingProfile.connectorId == 0))
        {
            if (ChargingProfile.csChargingProfiles.stackLevel > MaxStackLevel)
            {
                MaxStackLevel = ChargingProfile.csChargingProfiles.stackLevel;
                MaxStackLevelChargingProfileCount = i;
            }
        }
    }
    memset(&ChargingProfile, 0, sizeof(ChargingProfiles_t));
    if (MaxStackLevelChargingProfileCount > 0)
        ChargingProfile = custom_nvs_read_charging_profile(MaxStackLevelChargingProfileCount);

    return ChargingProfile;
}

ChargingProfiles_t custom_nvs_get_max_stack_level_TxDefault_charging_profile(uint8_t connId)
{
    ChargingProfiles_t ChargingProfile;
    memset(&ChargingProfile, 0, sizeof(ChargingProfiles_t));
    uint32_t chargingProfilesCount = custome_nvs_read_chargingProfilesCount();
    uint32_t MaxStackLevel = 0;
    uint32_t MaxStackLevelChargingProfileCount = 0;
    for (uint32_t i = 1; i <= chargingProfilesCount; i++)
    {
        ChargingProfile = custom_nvs_read_charging_profile(i);
        if (((ChargingProfile.connectorId == connId) || (ChargingProfile.connectorId == 0)) &&
            (ChargingProfile.csChargingProfiles.chargingProfilePurpose == TxDefaultProfile))
        {
            if (ChargingProfile.csChargingProfiles.stackLevel > MaxStackLevel)
            {
                MaxStackLevel = ChargingProfile.csChargingProfiles.stackLevel;
                MaxStackLevelChargingProfileCount = i;
            }
        }
    }
    memset(&ChargingProfile, 0, sizeof(ChargingProfiles_t));
    if (MaxStackLevelChargingProfileCount > 0)
        ChargingProfile = custom_nvs_read_charging_profile(MaxStackLevelChargingProfileCount);

    return ChargingProfile;
}

ChargingProfiles_t custom_nvs_get_max_stack_level_ChargePointMax_charging_profile(uint8_t connId)
{
    ChargingProfiles_t ChargingProfile;
    memset(&ChargingProfile, 0, sizeof(ChargingProfiles_t));
    uint32_t chargingProfilesCount = custome_nvs_read_chargingProfilesCount();
    uint32_t MaxStackLevel = 0;
    uint32_t MaxStackLevelChargingProfileCount = 0;
    for (uint32_t i = 1; i <= chargingProfilesCount; i++)
    {
        ChargingProfile = custom_nvs_read_charging_profile(i);
        if (((ChargingProfile.connectorId == connId) || (ChargingProfile.connectorId == 0)) &&
            (ChargingProfile.csChargingProfiles.chargingProfilePurpose == ChargePointMaxProfile))
        {
            if (ChargingProfile.csChargingProfiles.stackLevel > MaxStackLevel)
            {
                MaxStackLevel = ChargingProfile.csChargingProfiles.stackLevel;
                MaxStackLevelChargingProfileCount = i;
            }
        }
    }
    memset(&ChargingProfile, 0, sizeof(ChargingProfiles_t));
    if (MaxStackLevelChargingProfileCount > 0)
        ChargingProfile = custom_nvs_read_charging_profile(MaxStackLevelChargingProfileCount);

    return ChargingProfile;
}

void custom_nvs_clear_chargingProfile(uint32_t id)
{
    ChargingProfiles_t ChargingProfile;
    memset(&ChargingProfile, 0, sizeof(ChargingProfiles_t));
    uint32_t chargingProfilesCount = custome_nvs_read_chargingProfilesCount();
    for (uint32_t i = 1; i <= chargingProfilesCount; i++)
    {
        ChargingProfile = custom_nvs_read_charging_profile(i);
        if (ChargingProfile.csChargingProfiles.chargingProfileId == id)
        {
            size_t chargingProfilesSize = sizeof(ChargingProfiles_t);
            char chargingprofilesavingkey[50];
            setNULL(chargingprofilesavingkey);
            nvs_handle handle;
            nvs_open_from_partition(PARTITION_NAME, CHARGING_PROGILE_NAMESPACE, NVS_READWRITE, &handle);
            for (uint32_t j = i; j < chargingProfilesCount; j++)
            {
                sprintf(chargingprofilesavingkey, "cpcount%ld", j + (uint32_t)1);
                nvs_get_blob(handle, chargingprofilesavingkey, (void *)&ChargingProfile, &chargingProfilesSize);
                sprintf(chargingprofilesavingkey, "cpcount%ld", j);
                nvs_set_blob(handle, chargingprofilesavingkey, (void *)&ChargingProfile, sizeof(ChargingProfiles_t));
            }
            nvs_commit(handle);
            nvs_close(handle);
            chargingProfilesCount = chargingProfilesCount - 1;
            custome_nvs_write_chargingProfilesCount(chargingProfilesCount);
            ChargingProfileAddedOrDeleted = true;
        }
    }
}

void set_ChargingProfiles(void)
{
    for (uint8_t connId = 1; connId < NUM_OF_CONNECTORS; connId++)
    {
        ChargingProfiles_t MaxStackLevelProfile = custom_nvs_get_max_stack_level_charging_profile(connId);
        ChargingProfiles_t MaxStackLevelTxDefaultProfile = custom_nvs_get_max_stack_level_TxDefault_charging_profile(connId);
        ChargingProfiles_t MaxStackLevelChargePointMaxProfile = custom_nvs_get_max_stack_level_ChargePointMax_charging_profile(connId);

        if ((MaxStackLevelProfile.csChargingProfiles.chargingProfilePurpose == TxProfile) &&
            (MaxStackLevelProfile.csChargingProfiles.transactionId == ConnectorData[connId].transactionId))
        {
            ChargingProfile[connId] = MaxStackLevelProfile;
        }
        else if ((MaxStackLevelProfile.csChargingProfiles.chargingProfilePurpose == TxProfile) &&
                 (MaxStackLevelProfile.csChargingProfiles.transactionId != ConnectorData[connId].transactionId))
        {
            if (MaxStackLevelTxDefaultProfile.csChargingProfiles.stackLevel >= MaxStackLevelChargePointMaxProfile.csChargingProfiles.stackLevel)
                ChargingProfile[connId] = MaxStackLevelTxDefaultProfile;
            else
                ChargingProfile[connId] = MaxStackLevelChargePointMaxProfile;
        }
        else
        {
            ChargingProfile[connId] = MaxStackLevelProfile;
        }
    }
    ChargingProfileAddedOrDeleted = false;
}

void set_ChargePointMaxChargingProfiles(void)
{
    for (uint8_t connId = 1; connId < NUM_OF_CONNECTORS; connId++)
    {
        ChargePointMaxChargingProfile[connId] = custom_nvs_get_max_stack_level_ChargePointMax_charging_profile(connId);
    }
}

void custom_nvs_write_connector_data(uint8_t connId)
{
    ESP_LOGI(TAG, "Writing Connector data to NVS for connector %d", connId);

    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(PARTITION_NAME, CONNECTOR_DATA_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for writing connector data (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    ESP_LOGI(TAG, "Setting blob with size %d", sizeof(ConnectorPrivateData_t));
    err = nvs_set_blob(handle, CONNECTOR_DATA[connId], (void *)&ConnectorData[connId], sizeof(ConnectorPrivateData_t));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set blob for connector data (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    ESP_LOGI(TAG, "Committing NVS handle with connector data");
    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS handle with connector data (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    ESP_LOGI(TAG, "Connector data written to NVS for connector %d", connId);
    nvs_close(handle);
}

void custom_nvs_read_connector_data(uint8_t connId)
{
    ESP_LOGI(TAG, "Reading Connector data from NVS for connector %d", connId);

    nvs_handle handle;
    size_t ConnectorPrivateDataSize = sizeof(ConnectorPrivateData_t);
    esp_err_t err = nvs_open_from_partition(PARTITION_NAME, CONNECTOR_DATA_NAMESPACE, NVS_READWRITE, &handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for reading connector data (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    err = nvs_get_blob(handle, CONNECTOR_DATA[connId], (void *)&ConnectorData[connId], &ConnectorPrivateDataSize);

    if (err != ESP_OK)
    {
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            ConnectorData[connId].CmsAvailable = true;
        }
        ESP_LOGE(TAG, "Failed to get blob for connector data (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    ESP_LOGI(TAG, "Connector data read successfully from NVS for connector %d", connId);
    nvs_close(handle);
}

esp_err_t custom_nvs_read_connectors_offline_data_count(void)
{
    nvs_handle handle;
    esp_err_t ret;
    nvs_open_from_partition(PARTITION_NAME, OFFLINE_DATA_NAMESPACE, NVS_READWRITE, &handle);
    ret = nvs_get_u32(handle, CONNECTOR1_OFFLINE_DATA_COUNT, &connectorsOfflineDataCount[1]);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Connector 1 offline data count: %ld", connectorsOfflineDataCount[1]);
        updatingOfflineData[1] = (connectorsOfflineDataCount[1] > 0) ? true : false;
    }
    else if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_set_u32(handle, CONNECTOR1_OFFLINE_DATA_COUNT, 0);
    }
    else
    {
        ESP_LOGE(TAG, "Connector 1 offline data count Error (%s) reading NVS handle!\n", esp_err_to_name(ret));
    }

    ret = nvs_get_u32(handle, CONNECTOR2_OFFLINE_DATA_COUNT, &connectorsOfflineDataCount[2]);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Connector 2 offline data count: %ld", connectorsOfflineDataCount[2]);
        updatingOfflineData[2] = (connectorsOfflineDataCount[2] > 0) ? true : false;
    }
    else if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_set_u32(handle, CONNECTOR2_OFFLINE_DATA_COUNT, 0);
    }
    else
    {
        ESP_LOGE(TAG, "Connector 2 offline data count Error (%s) reading NVS handle!\n", esp_err_to_name(ret));
    }

    ret = nvs_get_u32(handle, CONNECTOR3_OFFLINE_DATA_COUNT, &connectorsOfflineDataCount[3]);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Connector 3 offline data count: %ld", connectorsOfflineDataCount[3]);
        updatingOfflineData[3] = (connectorsOfflineDataCount[3] > 0) ? true : false;
    }
    else if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_set_u32(handle, CONNECTOR3_OFFLINE_DATA_COUNT, 0);
    }
    else
    {
        ESP_LOGE(TAG, "Connector 3 offline data count Error (%s) reading NVS handle!\n", esp_err_to_name(ret));
    }

    nvs_close(handle);
    return ret;
}

void custom_nvs_write_connectors_offline_data(uint8_t connId)
{
    nvs_handle handle;
    esp_err_t ret = ESP_OK;
    char connectorDataOfflineKey[50];
    setNULL(connectorDataOfflineKey);
    nvs_open_from_partition(PARTITION_NAME, OFFLINE_DATA_NAMESPACE, NVS_READWRITE, &handle);
    if (connId == 1)
        ret = nvs_get_u32(handle, CONNECTOR1_OFFLINE_DATA_COUNT, &connectorsOfflineDataCount[connId]);
    else if (connId == 2)
        ret = nvs_get_u32(handle, CONNECTOR2_OFFLINE_DATA_COUNT, &connectorsOfflineDataCount[connId]);
    else if (connId == 3)
        ret = nvs_get_u32(handle, CONNECTOR3_OFFLINE_DATA_COUNT, &connectorsOfflineDataCount[connId]);

    if (ret != ESP_OK)
    {
        connectorsOfflineDataCount[connId] = 0;
    }
    if (connectorsOfflineDataCount[connId] >= MAX_OFFLINE_TRANSACTIONS_FOR_CONNECTOR)
    {
        custom_nvs_read_connectors_offline_data(connId);
        if (connId == 1)
            ret = nvs_get_u32(handle, CONNECTOR1_OFFLINE_DATA_COUNT, &connectorsOfflineDataCount[connId]);
        else if (connId == 2)
            ret = nvs_get_u32(handle, CONNECTOR2_OFFLINE_DATA_COUNT, &connectorsOfflineDataCount[connId]);
        else if (connId == 3)
            ret = nvs_get_u32(handle, CONNECTOR3_OFFLINE_DATA_COUNT, &connectorsOfflineDataCount[connId]);
    }
    connectorsOfflineDataCount[connId]++;
    sprintf(connectorDataOfflineKey, "Con%dOffData%ld", connId, connectorsOfflineDataCount[connId]);
    ESP_LOGI(TAG, "write key: %s", connectorDataOfflineKey);
    ret = nvs_set_blob(handle, connectorDataOfflineKey, (void *)&ConnectorData[connId], sizeof(ConnectorPrivateData_t));
    if (ret != ESP_OK)
    {
        ESP_LOGI(TAG, "ConnectorData Error (%s) writing NVS handle!\n", esp_err_to_name(ret));
    }
    if (connId == 1)
        ret = nvs_set_u32(handle, CONNECTOR1_OFFLINE_DATA_COUNT, connectorsOfflineDataCount[connId]);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "connector 1 OfflineDataCount Error (%s) writing NVS handle!\n", esp_err_to_name(ret));
    }
    if (connId == 2)
        nvs_set_u32(handle, CONNECTOR2_OFFLINE_DATA_COUNT, connectorsOfflineDataCount[connId]);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "connector 2 OfflineDataCount Error (%s) writing NVS handle!\n", esp_err_to_name(ret));
    }
    if (connId == 3)
        nvs_set_u32(handle, CONNECTOR3_OFFLINE_DATA_COUNT, connectorsOfflineDataCount[connId]);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "connector 3 OfflineDataCount Error (%s) writing NVS handle!\n", esp_err_to_name(ret));
    }
    ESP_LOGE(TAG, "connector %hhu offline data count %ld", connId, connectorsOfflineDataCount[connId]);
    nvs_commit(handle);
    nvs_close(handle);
}

void custom_nvs_read_connectors_offline_data(uint8_t connId)
{
    nvs_handle handle;
    char connectorDataOfflineKey[50];
    size_t ConnectorPrivateDataSize = sizeof(ConnectorPrivateData_t);
    ConnectorPrivateData_t ConnectorOfflineData_temp;
    setNULL(connectorDataOfflineKey);
    nvs_open_from_partition(PARTITION_NAME, OFFLINE_DATA_NAMESPACE, NVS_READWRITE, &handle);
    if (connId == 1)
        nvs_get_u32(handle, CONNECTOR1_OFFLINE_DATA_COUNT, &connectorsOfflineDataCount[connId]);
    if (connId == 2)
        nvs_get_u32(handle, CONNECTOR2_OFFLINE_DATA_COUNT, &connectorsOfflineDataCount[connId]);
    if (connId == 3)
        nvs_get_u32(handle, CONNECTOR3_OFFLINE_DATA_COUNT, &connectorsOfflineDataCount[connId]);

    if (connectorsOfflineDataCount[connId] > 0)
    {
        sprintf(connectorDataOfflineKey, "Con%dOffData%ld", connId, (uint32_t)1);
        ESP_LOGI(TAG, "read key: %s", connectorDataOfflineKey);
        esp_err_t ret = nvs_get_blob(handle, connectorDataOfflineKey, (void *)&ConnectorOfflineData_temp, &ConnectorPrivateDataSize);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "ConnectorOfflineData read Error (%s) reading NVS handle!\n", esp_err_to_name(ret));
        }
        for (uint32_t i = 1; i < connectorsOfflineDataCount[connId]; i++)
        {
            sprintf(connectorDataOfflineKey, "Con%dOffData%ld", connId, i + (uint32_t)1);
            nvs_get_blob(handle, connectorDataOfflineKey, (void *)&ConnectorOfflineData[connId], &ConnectorPrivateDataSize);
            sprintf(connectorDataOfflineKey, "Con%dOffData%ld", connId, i);
            nvs_set_blob(handle, connectorDataOfflineKey, (void *)&ConnectorOfflineData[connId], sizeof(ConnectorPrivateData_t));
        }
        connectorsOfflineDataCount[connId]--;
        if (connId == 1)
            nvs_set_u32(handle, CONNECTOR1_OFFLINE_DATA_COUNT, connectorsOfflineDataCount[connId]);
        if (connId == 2)
            nvs_set_u32(handle, CONNECTOR2_OFFLINE_DATA_COUNT, connectorsOfflineDataCount[connId]);
        if (connId == 3)
            nvs_set_u32(handle, CONNECTOR3_OFFLINE_DATA_COUNT, connectorsOfflineDataCount[connId]);
        memcpy(&ConnectorOfflineData[connId], &ConnectorOfflineData_temp, sizeof(ConnectorPrivateData_t));
    }
    nvs_close(handle);
}

void custom_nvs_write_timeBuffer(void)
{
    ESP_LOGI(TAG, "Writing time_buffer to NVS");
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(PARTITION_NAME, WIFI_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for writing time_buffer (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    err = nvs_set_str(handle, TIME_BUFFER, time_buffer);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write time_buffer to NVS (%s)", esp_err_to_name(err));
    }

    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit time_buffer to NVS (%s)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "time_buffer written to NVS : %s", time_buffer);
    }
    nvs_close(handle);
}

void custom_nvs_read_timeBuffer(void)
{
    ESP_LOGI(TAG, "Reading time_buffer from NVS");
    nvs_handle handle;
    size_t len = sizeof(time_buffer);
    esp_err_t err = nvs_open_from_partition(PARTITION_NAME, WIFI_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for reading time_buffer (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }
    setNULL(time_buffer);
    err = nvs_get_str(handle, TIME_BUFFER, time_buffer, &len);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            setNULL(time_buffer);
            memcpy(time_buffer, "2024-03-26T1:409.519Z", strlen("2024-03-26T1:409.519Z"));
        }
        ESP_LOGE(TAG, "Failed to read time_buffer from NVS (%s)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "time_buffer read from NVS %s", time_buffer);
    }
    nvs_close(handle);
}

void custom_nvs_test_offline_data(void)
{
    custom_nvs_read_connectors_offline_data_count();
    ConnectorData[1].isTransactionOngoing = false;
    ConnectorData[1].offlineStarted = false;
    ConnectorData[1].isReserved = false;
    ConnectorData[1].CmsAvailableScheduled = false;
    ConnectorData[1].CmsAvailable = false;
    ConnectorData[1].CmsAvailableChanged = false;
    setNULL(ConnectorData[1].idTag);
    memcpy(ConnectorData[1].idTag, "w10", strlen("w10"));
    ConnectorData[1].state = 0;
    ConnectorData[1].stopReason = 0;
    ConnectorData[1].transactionId = 0;
    ConnectorData[1].meterStart = 10;
    ConnectorData[1].meterStop = 20;
    ConnectorData[1].meterValue.temp = 0;
    ConnectorData[1].meterValue.voltage[1] = 0;
    ConnectorData[1].meterValue.current[1] = 0;
    ConnectorData[1].meterValue.power = 0;
    ConnectorData[1].Energy = 0;
    setNULL(ConnectorData[1].meterStart_time);
    memcpy(ConnectorData[1].meterStart_time, "2021-11-11T1:409.519Z", strlen("2021-11-11T1:409.519Z"));
    setNULL(ConnectorData[1].meterStop_time);
    memcpy(ConnectorData[1].meterStop_time, "2021-11-11T1:409.519Z", strlen("2021-11-11T1:409.519Z"));
    setNULL(ConnectorData[1].ReservedData.idTag);
    memcpy(ConnectorData[1].ReservedData.idTag, "CP0MV84D7N", strlen("CP0MV84D7N"));
    setNULL(ConnectorData[1].ReservedData.parentidTag);
    memcpy(ConnectorData[1].ReservedData.parentidTag, "CP0MV84D7N", strlen("CP0MV84D7N"));
    setNULL(ConnectorData[1].ReservedData.expiryDate);
    memcpy(ConnectorData[1].ReservedData.expiryDate, "2021-11-11", strlen("2021-11-11"));
    ConnectorData[1].ReservedData.reservationId = 0;

    ConnectorData[2].isTransactionOngoing = false;
    ConnectorData[2].offlineStarted = false;
    ConnectorData[2].isReserved = false;
    ConnectorData[2].CmsAvailableScheduled = false;
    ConnectorData[2].CmsAvailable = false;
    ConnectorData[2].CmsAvailableChanged = false;
    setNULL(ConnectorData[2].idTag);
    memcpy(ConnectorData[2].idTag, "w20ir9j6veg96ele24aa", strlen("w20ir9j6veg96ele24aa"));
    ConnectorData[2].state = 0;
    ConnectorData[2].stopReason = 0;
    ConnectorData[2].transactionId = 0;
    ConnectorData[2].meterStart = 30;
    ConnectorData[2].meterStop = 40;
    ConnectorData[2].meterValue.temp = 0;
    ConnectorData[2].meterValue.voltage[2] = 0;
    ConnectorData[2].meterValue.current[2] = 0;
    ConnectorData[2].meterValue.power = 0;
    ConnectorData[2].Energy = 0;
    setNULL(ConnectorData[2].meterStart_time);
    memcpy(ConnectorData[2].meterStart_time, "2021-11-11T1:409.519Z", strlen("2021-11-11T1:409.519Z"));
    setNULL(ConnectorData[2].meterStop_time);
    memcpy(ConnectorData[2].meterStop_time, "2021-11-11T1:409.519Z", strlen("2021-11-11T1:409.519Z"));
    setNULL(ConnectorData[2].ReservedData.idTag);
    memcpy(ConnectorData[2].ReservedData.idTag, "CP0MV84D7N", strlen("CP0MV84D7N"));
    setNULL(ConnectorData[2].ReservedData.parentidTag);
    memcpy(ConnectorData[2].ReservedData.parentidTag, "CP0MV84D7N", strlen("CP0MV84D7N"));
    setNULL(ConnectorData[2].ReservedData.expiryDate);
    memcpy(ConnectorData[2].ReservedData.expiryDate, "2021-11-11", strlen("2021-11-11"));
    ConnectorData[2].ReservedData.reservationId = 0;

    ConnectorData[3].isTransactionOngoing = false;
    ConnectorData[3].offlineStarted = false;
    ConnectorData[3].isReserved = false;
    ConnectorData[3].CmsAvailableScheduled = false;
    ConnectorData[3].CmsAvailable = false;
    ConnectorData[3].CmsAvailableChanged = false;
    setNULL(ConnectorData[3].idTag);
    memcpy(ConnectorData[3].idTag, "w30", strlen("w30"));
    ConnectorData[3].state = 0;
    ConnectorData[3].stopReason = 0;
    ConnectorData[3].transactionId = 0;
    ConnectorData[3].meterStart = 50;
    ConnectorData[3].meterStop = 60;
    ConnectorData[3].meterValue.temp = 0;
    ConnectorData[3].meterValue.voltage[3] = 0;
    ConnectorData[3].meterValue.current[3] = 0;
    ConnectorData[3].meterValue.power = 0;
    ConnectorData[3].Energy = 0;
    setNULL(ConnectorData[3].meterStart_time);
    memcpy(ConnectorData[3].meterStart_time, "2021-11-11T1:409.519Z", strlen("2021-11-11T1:409.519Z"));
    setNULL(ConnectorData[3].meterStop_time);
    memcpy(ConnectorData[3].meterStop_time, "2021-11-11T1:409.519Z", strlen("2021-11-11T1:409.519Z"));
    setNULL(ConnectorData[3].ReservedData.idTag);
    memcpy(ConnectorData[3].ReservedData.idTag, "CP0MV84D7N", strlen("CP0MV84D7N"));
    setNULL(ConnectorData[3].ReservedData.parentidTag);
    memcpy(ConnectorData[3].ReservedData.parentidTag, "CP0MV84D7N", strlen("CP0MV84D7N"));
    setNULL(ConnectorData[3].ReservedData.expiryDate);
    memcpy(ConnectorData[3].ReservedData.expiryDate, "2021-11-11", strlen("2021-11-11"));
    ConnectorData[3].ReservedData.reservationId = 0;

    custom_nvs_write_connectors_offline_data(1);
    custom_nvs_write_connectors_offline_data(2);
    custom_nvs_write_connectors_offline_data(3);
    ConnectorData[1].isTransactionOngoing = true;
    ConnectorData[1].offlineStarted = true;
    ConnectorData[1].isReserved = true;
    ConnectorData[1].CmsAvailableScheduled = true;
    ConnectorData[1].CmsAvailable = true;
    ConnectorData[1].CmsAvailableChanged = true;
    setNULL(ConnectorData[1].idTag);
    memcpy(ConnectorData[1].idTag, "CPvsv4D7N", strlen("CPvsv4D7N"));
    ConnectorData[1].state = 0;
    ConnectorData[1].stopReason = 0;
    ConnectorData[1].transactionId = 0;
    ConnectorData[1].meterStart = 0;
    ConnectorData[1].meterStop = 0;
    ConnectorData[1].meterValue.temp = 0;
    ConnectorData[1].meterValue.voltage[1] = 0;
    ConnectorData[1].meterValue.current[1] = 0;
    ConnectorData[1].meterValue.power = 0;
    ConnectorData[1].Energy = 0;
    setNULL(ConnectorData[1].ReservedData.idTag);
    memcpy(ConnectorData[1].ReservedData.idTag, "CPvsv4D7N", strlen("CPvsv4D7N"));
    setNULL(ConnectorData[1].ReservedData.parentidTag);
    memcpy(ConnectorData[1].ReservedData.parentidTag, "CPvsv4D7N", strlen("CPvsv4D7N"));
    setNULL(ConnectorData[1].ReservedData.expiryDate);
    memcpy(ConnectorData[1].ReservedData.expiryDate, "2022-22-22", strlen("2022-22-22"));
    ConnectorData[1].ReservedData.reservationId = 0;
    custom_nvs_write_connectors_offline_data(1);

    custom_nvs_read_connectors_offline_data_count();

    for (uint8_t connId = 1; connId < NUM_OF_CONNECTORS; connId++)
    {
        ESP_LOGI(TAG, "Printing Connector %hhu offline data", connId);
        while (updatingOfflineData[connId])
        {
            custom_nvs_read_connectors_offline_data(connId);
            ESP_LOGI(TAG_DEBUG, "-----------------------------------------------------------------------");
            ESP_LOGI(TAG_DEBUG, "Connector %d offline Transaction details", connId);
            ESP_LOGI(TAG_DEBUG, "-----------------------------------------------------------------------");
            ESP_LOGI(TAG_DEBUG, "isTransactionOngoing %d", ConnectorOfflineData[connId].isTransactionOngoing);
            ESP_LOGI(TAG_DEBUG, "offlineStarted %d", ConnectorOfflineData[connId].offlineStarted);
            ESP_LOGI(TAG_DEBUG, "isReserved %d", ConnectorOfflineData[connId].isReserved);
            ESP_LOGI(TAG_DEBUG, "CmsAvailableScheduled %d", ConnectorOfflineData[connId].CmsAvailableScheduled);
            ESP_LOGI(TAG_DEBUG, "CmsAvailable %d", ConnectorOfflineData[connId].CmsAvailable);
            ESP_LOGI(TAG_DEBUG, "CmsAvailableChanged %d", ConnectorOfflineData[connId].CmsAvailableChanged);
            ESP_LOGI(TAG_DEBUG, "UnkownId %d", ConnectorOfflineData[connId].UnkownId);
            ESP_LOGI(TAG_DEBUG, "InvalidId %d", ConnectorOfflineData[connId].InvalidId);
            ESP_LOGI(TAG_DEBUG, "idTag %s", ConnectorOfflineData[connId].idTag);
            ESP_LOGI(TAG_DEBUG, "state %d", ConnectorOfflineData[connId].state);
            ESP_LOGI(TAG_DEBUG, "stopReason %d", ConnectorOfflineData[connId].stopReason);
            ESP_LOGI(TAG_DEBUG, "transactionId %ld", ConnectorOfflineData[connId].transactionId);
            ESP_LOGI(TAG_DEBUG, "meterStart %f", ConnectorOfflineData[connId].meterStart);
            ESP_LOGI(TAG_DEBUG, "meterStop %f", ConnectorOfflineData[connId].meterStop);
            ESP_LOGI(TAG_DEBUG, "meterValue.temp %d", ConnectorOfflineData[connId].meterValue.temp);
            ESP_LOGI(TAG_DEBUG, "meterValue.voltage[%hhu] %f", connId, ConnectorOfflineData[connId].meterValue.voltage[connId]);
            ESP_LOGI(TAG_DEBUG, "meterValue.current[%hhu] %f", connId, ConnectorOfflineData[connId].meterValue.current[connId]);
            ESP_LOGI(TAG_DEBUG, "meterValue.power %f", ConnectorOfflineData[connId].meterValue.power);
            ESP_LOGI(TAG_DEBUG, "Energy %f", ConnectorOfflineData[connId].Energy);
            ESP_LOGI(TAG_DEBUG, "ReservedData.idTag %s", ConnectorOfflineData[connId].ReservedData.idTag);
            ESP_LOGI(TAG_DEBUG, "ReservedData.parentidTag %s", ConnectorOfflineData[connId].ReservedData.parentidTag);
            ESP_LOGI(TAG_DEBUG, "ReservedData.expiryDate %s", ConnectorOfflineData[connId].ReservedData.expiryDate);
            ESP_LOGI(TAG_DEBUG, "ReservedData.reservationId %d", ConnectorOfflineData[connId].ReservedData.reservationId);
            ESP_LOGI(TAG_DEBUG, "-----------------------------------------------------------------------");

            custom_nvs_read_connectors_offline_data_count();
        }
    }
}