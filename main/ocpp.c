#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <stdbool.h>
#include <esp_system.h>
#include <time.h>
#include "cJSON.h"
#include "ocpp.h"
#include "OTA.h"
#include "ProjectSettings.h"
#include "custom_nvs.h"

#define TAG "OCPP_CORE"
static const char *TAG_BOOT = "BootNotification";
static const char *TAG_STATUS = "StatusNotification";
static const char *TAG_AUTH = "Authorization";
static const char *TAG_CANRES = "CancelReservation";
static const char *TAG_CHANGEAVAILREQ = "ChangeAvailabilityRequest";
static const char *TAG_CHANGECONFREQ = "ChangeConfigurationRequest";
static const char *TAG_FIRMSTATUSNOTIREQ = "FirmwareStatusNotificationRequest";
static const char *TAG_TRIMSGREQ = "TriggerMessageRequest";
static const char *TAG_STRARTTRANSREQ = "StartTransactionRequest";
static const char *TAG_RMSTOPTRANSREQ = "RemoteStopTransactionRequest";
static const char *TAG_RESNOWREQ = "ReserveNowRequest";
static const char *TAG_RESETRREQ = "ResetRequest";
static const char *TAG_UPDATEFIRMREQ = "UpdateFirmwareRequest";
static const char *TAG_STOPTRANSREQ = "StopTransactionRequest";
static const char *TAG_SENDLOCALLISTREQ = "SendLocalListRequest";
static const char *TAG_REMOTESTARTREQ = "RemoteStartTransactionRequest";
static const char *TAG_METERVALREQ = "MeterValuesRequest";
static const char *TAG_CLEARCACHE = "Clearcache";
static const char *TAG_GETLOCALLISTVER = "GetLocalListVersion";
static const char *TAG_HBRES = "Heartbeat";

char ClearCacheReqUniqId[16];
char GetLocalListVersionReqUniqId[16];
char HeartbeatReqUniqId[16];

CPBootNotificationRequest_t CPBootNotificationRequest;
CPHeartbeatRequest_t CPHeartbeatRequest;
CPStatusNotificationRequest_t CPStatusNotificationRequest[NUM_OF_CONNECTORS];
CPAuthorizeRequest_t CPAuthorizeRequest[NUM_OF_CONNECTORS];
CPStartTransactionRequest_t CPStartTransactionRequest[NUM_OF_CONNECTORS];
CPMeterValuesRequest_t CPMeterValuesRequest[NUM_OF_CONNECTORS];
CPStopTransactionRequest_t CPStopTransactionRequest[NUM_OF_CONNECTORS];
CPFirmwareStatusNotificationRequest_t CPFirmwareStatusNotificationRequest;
CPDiagnosticsStatusNotificationRequest_t CPDiagnosticsStatusNotificationRequest;

CMSRemoteStartTransactionRequest_t CMSRemoteStartTransactionRequest[NUM_OF_CONNECTORS];
CMSRemoteStopTransactionRequest_t CMSRemoteStopTransactionRequest;
CMSCancelReservationRequest_t CMSCancelReservationRequest;
CMSChangeAvailabilityRequest_t CMSChangeAvailabilityRequest[NUM_OF_CONNECTORS];
CMSChangeConfigurationRequest_t CMSChangeConfigurationRequest;
CMSTriggerMessageRequest_t CMSTriggerMessageRequest;
CMSReserveNowRequest_t CMSReserveNowRequest[NUM_OF_CONNECTORS];
CMSResetRequest_t CMSResetRequest;
CMSUpdateFirmwareRequest_t CMSUpdateFirmwareRequest;
CMSSendLocalListRequest_t CMSSendLocalListRequest;
CMSClearCacheRequest_t CMSClearCacheRequest;
CMSGetLocalListVersionRequest_t CMSGetLocalListVersionRequest;
CMSGetConfigurationRequest_t CMSGetConfigurationRequest;
CMSClearChargingProfileRequest_t CMSClearChargingProfileRequest;
CMSDataTransferRequest_t CMSDataTransferRequest;
CMSGetCompositeScheduleRequest_t CMSGetCompositeScheduleRequest;
CMSGetDiagnosticsRequest_t CMSGetDiagnosticsRequest;
CMSSetChargingProfileRequest_t CMSSetChargingProfileRequest;
CMSUnlockConnectorRequest_t CMSUnlockConnectorRequest;

CPRemoteStartTransactionResponse_t CPRemoteStartTransactionResponse[NUM_OF_CONNECTORS];
CPRemoteStopTransactionResponse_t CPRemoteStopTransactionResponse;
CPReserveNowResponse_t CPReserveNowResponse;
CPResetResponse_t CPResetResponse;
CPSendLocalListResponse_t CPSendLocalListResponse;
CPTriggerMessageResponse_t CPTriggerMessageResponse;
CPCancelReservationResponse_t CPCancelReservationResponse;
CPChangeAvailabilityResponse_t CPChangeAvailabilityResponse;
CPChangeConfigurationResponse_t CPChangeConfigurationResponse;
CPClearCacheResponse_t CPClearCacheResponse;
CPGetLocalListVersionResponse_t CPGetLocalListVersionResponse;
CPUpdateFirmwareResponse_t CPUpdateFirmwareResponse;
CPGetDiagnosticsResponse_t CPGetDiagnosticsResponse;
CPGetConfigurationResponse_t CPGetConfigurationResponse;

CMSFirmwareStatusNotificationResponse_t CMSFirmwareStatusNotificationResponse;
CMSDiagnosticsStatusNotificationResponse_t CMSDiagnosticsStatusNotificationResponse;
CMSHeartbeatResponse_t CMSHeartbeatResponse;
CMSBootNotificationResponse_t CMSBootNotificationResponse;
CMSStatusNotificationResponse_t CMSStatusNotificationResponse[NUM_OF_CONNECTORS];
CMSMeterValuesResponse_t CMSMeterValuesResponse[NUM_OF_CONNECTORS];
CMSAuthorizeResponse_t CMSAuthorizeResponse[NUM_OF_CONNECTORS];
CMSStartTransactionResponse_t CMSStartTransactionResponse[NUM_OF_CONNECTORS];
CMSStopTransactionResponse_t CMSStopTransactionResponse[NUM_OF_CONNECTORS];

LocalAuthorizationList_t LocalAuthorizationList;

QueueHandle_t OcppMsgQueue;

uint64_t randomNumber = 100;
extern Config_t config;

bool WriteChargingProfileToFlash(ChargingProfiles_t ChargingProfile, cJSON *csChargingProfiles)
{
    bool iscsChargingProfilesReceived = false;
    if (csChargingProfiles != NULL)
    {
        bool ischargingProfileIdReceived = false;
        bool istransactionIdReceived = false;
        bool isstackLevelReceived = false;
        bool ischargingProfilePurposeReceived = false;
        bool ischargingProfileKindReceived = false;
        bool isrecurrencyKindReceived = false;
        bool isvalidFromReceived = false;
        bool isvalidToReceived = false;
        bool ischargingScheduleReceived = false;

        cJSON *chargingProfileId = cJSON_GetObjectItem(csChargingProfiles, "chargingProfileId");
        cJSON *transactionId = cJSON_GetObjectItem(csChargingProfiles, "transactionId");
        cJSON *stackLevel = cJSON_GetObjectItem(csChargingProfiles, "stackLevel");
        cJSON *chargingProfilePurpose = cJSON_GetObjectItem(csChargingProfiles, "chargingProfilePurpose");
        cJSON *chargingProfileKind = cJSON_GetObjectItem(csChargingProfiles, "chargingProfileKind");
        cJSON *recurrencyKind = cJSON_GetObjectItem(csChargingProfiles, "recurrencyKind");
        cJSON *validFrom = cJSON_GetObjectItem(csChargingProfiles, "validFrom");
        cJSON *validTo = cJSON_GetObjectItem(csChargingProfiles, "validTo");
        cJSON *chargingSchedule = cJSON_GetObjectItem(csChargingProfiles, "chargingSchedule");

        if (cJSON_IsNumber(chargingProfileId))
        {
            ischargingProfileIdReceived = true;
            ChargingProfile.csChargingProfiles.chargingProfileId = chargingProfileId->valueint;
        }
        if (cJSON_IsNumber(transactionId))
        {
            istransactionIdReceived = true;
            ChargingProfile.csChargingProfiles.transactionId = transactionId->valueint;
        }
        if (cJSON_IsNumber(stackLevel))
        {
            isstackLevelReceived = true;
            ChargingProfile.csChargingProfiles.stackLevel = stackLevel->valueint;
        }
        if (cJSON_IsString(chargingProfilePurpose))
        {
            if (memcmp(chargingProfilePurpose->valuestring, "ChargePointMaxProfile", strlen(chargingProfilePurpose->valuestring)) == 0)
            {
                ischargingProfilePurposeReceived = true;
                ChargingProfile.csChargingProfiles.chargingProfilePurpose = ChargePointMaxProfile;
            }
            else if (memcmp(chargingProfilePurpose->valuestring, "TxProfile", strlen(chargingProfilePurpose->valuestring)) == 0)
            {
                ischargingProfilePurposeReceived = true;
                ChargingProfile.csChargingProfiles.chargingProfilePurpose = TxProfile;
            }
            else if (memcmp(chargingProfilePurpose->valuestring, "TxDefaultProfile", strlen(chargingProfilePurpose->valuestring)) == 0)
            {
                ischargingProfilePurposeReceived = true;
                ChargingProfile.csChargingProfiles.chargingProfilePurpose = TxDefaultProfile;
            }
            else
            {
                ischargingProfilePurposeReceived = false;
                ChargingProfile.csChargingProfiles.chargingProfilePurpose = 0;
            }
        }
        if (cJSON_IsString(chargingProfileKind))
        {
            if (memcmp(chargingProfileKind->valuestring, "Absolute", strlen(chargingProfileKind->valuestring)) == 0)
            {
                ischargingProfileKindReceived = true;
                ChargingProfile.csChargingProfiles.chargingProfileKind = Absolute;
            }
            else if (memcmp(chargingProfileKind->valuestring, "Recurring", strlen(chargingProfileKind->valuestring)) == 0)
            {
                ischargingProfileKindReceived = true;
                ChargingProfile.csChargingProfiles.chargingProfileKind = Recurring;
            }
            else if (memcmp(chargingProfileKind->valuestring, "Relative", strlen(chargingProfileKind->valuestring)) == 0)
            {
                ischargingProfileKindReceived = true;
                ChargingProfile.csChargingProfiles.chargingProfileKind = Relative;
            }
            else
            {
                ischargingProfileKindReceived = false;
                ChargingProfile.csChargingProfiles.chargingProfileKind = 0;
            }
        }
        if (cJSON_IsString(recurrencyKind))
        {
            if (memcmp(recurrencyKind->valuestring, "Daily", strlen(recurrencyKind->valuestring)) == 0)
            {
                isrecurrencyKindReceived = true;
                ChargingProfile.csChargingProfiles.recurrencyKind = Daily;
            }
            else if (memcmp(recurrencyKind->valuestring, "Weekly", strlen(recurrencyKind->valuestring)) == 0)
            {
                isrecurrencyKindReceived = true;
                ChargingProfile.csChargingProfiles.recurrencyKind = Weekly;
            }
            else
            {
                isrecurrencyKindReceived = false;
                ChargingProfile.csChargingProfiles.recurrencyKind = 0;
            }
        }
        if (cJSON_IsString(validFrom))
        {
            struct tm tm_time = {0};
            struct timeval timeval_time;
            time_t unix_time;
            char timestamp[30];
            setNULL(timestamp);
            sscanf(timestamp, "%d-%d-%dT%d:%d:%d.%*3sZ",
                   &tm_time.tm_year, &tm_time.tm_mon, &tm_time.tm_mday,
                   &tm_time.tm_hour, &tm_time.tm_min, &tm_time.tm_sec);

            tm_time.tm_year -= 1900; // Adjust year
            tm_time.tm_mon -= 1;     // Adjust month

            unix_time = mktime(&tm_time);
            timeval_time.tv_sec = unix_time;

            isvalidFromReceived = true;
            ChargingProfile.csChargingProfiles.validFrom = timeval_time.tv_sec;
        }
        if (cJSON_IsString(validTo))
        {
            struct tm tm_time = {0};
            struct timeval timeval_time;
            time_t unix_time;
            char timestamp[30];
            setNULL(timestamp);
            sscanf(timestamp, "%d-%d-%dT%d:%d:%d.%*3sZ",
                   &tm_time.tm_year, &tm_time.tm_mon, &tm_time.tm_mday,
                   &tm_time.tm_hour, &tm_time.tm_min, &tm_time.tm_sec);

            tm_time.tm_year -= 1900; // Adjust year
            tm_time.tm_mon -= 1;     // Adjust month

            unix_time = mktime(&tm_time);
            timeval_time.tv_sec = unix_time;

            isvalidToReceived = true;
            ChargingProfile.csChargingProfiles.validTo = timeval_time.tv_sec;
        }
        if (cJSON_IsObject(chargingSchedule))
        {
            bool isdurationReceived = false;
            bool isstartScheduleReceived = false;
            bool ischargingRateUnitReceived = false;
            bool ischargingSchedulePeriodReceived = false;
            bool isminChargingRateReceived = false;

            cJSON *duration = cJSON_GetObjectItem(chargingSchedule, "duration");
            cJSON *startSchedule = cJSON_GetObjectItem(chargingSchedule, "startSchedule");
            cJSON *chargingRateUnit = cJSON_GetObjectItem(chargingSchedule, "chargingRateUnit");
            cJSON *chargingSchedulePeriod = cJSON_GetObjectItem(chargingSchedule, "chargingSchedulePeriod");
            cJSON *minChargingRate = cJSON_GetObjectItem(chargingSchedule, "minChargingRate");

            if (cJSON_IsNumber(duration))
            {
                isdurationReceived = true;
                ChargingProfile.csChargingProfiles.chargingSchedule.duration = duration->valueint;
            }
            if (cJSON_IsString(startSchedule))
            {
                struct tm tm_time = {0};
                struct timeval timeval_time;
                time_t unix_time;
                char timestamp[30];
                setNULL(timestamp);
                sscanf(timestamp, "%d-%d-%dT%d:%d:%d.%*3sZ",
                       &tm_time.tm_year, &tm_time.tm_mon, &tm_time.tm_mday,
                       &tm_time.tm_hour, &tm_time.tm_min, &tm_time.tm_sec);

                tm_time.tm_year -= 1900; // Adjust year
                tm_time.tm_mon -= 1;     // Adjust month

                unix_time = mktime(&tm_time);
                timeval_time.tv_sec = unix_time;

                isstartScheduleReceived = true;
                ChargingProfile.csChargingProfiles.chargingSchedule.startSchedule = timeval_time.tv_sec;
            }
            if (cJSON_IsString(chargingRateUnit))
            {
                if (memcmp(chargingRateUnit->valuestring, "A", strlen(chargingRateUnit->valuestring)) == 0)
                {
                    ischargingRateUnitReceived = true;
                    ChargingProfile.csChargingProfiles.chargingSchedule.chargingRateUnit = chargingRateUnitAmpere;
                }
                else if (memcmp(chargingRateUnit->valuestring, "W", strlen(chargingRateUnit->valuestring)) == 0)
                {
                    ischargingRateUnitReceived = true;
                    ChargingProfile.csChargingProfiles.chargingSchedule.chargingRateUnit = chargingRateUnitWatt;
                }
                else
                {
                    ischargingRateUnitReceived = false;
                    ChargingProfile.csChargingProfiles.chargingSchedule.chargingRateUnit = 0;
                }
            }
            if (cJSON_IsArray(chargingSchedulePeriod))
            {
                ischargingSchedulePeriodReceived = true;
                int numberOfItems = cJSON_GetArraySize(chargingSchedulePeriod);
                if (numberOfItems > SchedulePeriodCount)
                {
                    ESP_LOGW(TAG, "Number of items in chargingSchedulePeriod is more than the maximum supported. Truncating the items to %d", SchedulePeriodCount);
                    numberOfItems = SchedulePeriodCount;
                }
                int i = 0;
                cJSON *item;
                cJSON_ArrayForEach(item, chargingSchedulePeriod)
                {
                    bool isstartPeriodReceived = false;
                    bool islimitReceived = false;
                    bool isnumberPhasesReceived = false;

                    cJSON *startPeriod = cJSON_GetObjectItem(item, "startPeriod");
                    cJSON *limit = cJSON_GetObjectItem(item, "limit");
                    cJSON *numberPhases = cJSON_GetObjectItem(item, "numberPhases");
                    if (i < SchedulePeriodCount)
                    {
                        if (cJSON_IsNumber(startPeriod))
                        {
                            isstartPeriodReceived = true;
                            ChargingProfile.csChargingProfiles.chargingSchedule.chargingSchedulePeriod[i].startPeriod = startPeriod->valueint;
                        }
                        if (cJSON_IsNumber(limit))
                        {
                            islimitReceived = true;
                            ChargingProfile.csChargingProfiles.chargingSchedule.chargingSchedulePeriod[i].limit = limit->valuedouble;
                        }
                        if (cJSON_IsNumber(numberPhases))
                        {
                            isnumberPhasesReceived = true;
                            ChargingProfile.csChargingProfiles.chargingSchedule.chargingSchedulePeriod[i].numberPhases = numberPhases->valueint;
                        }
                        if (isstartPeriodReceived && islimitReceived && ischargingSchedulePeriodReceived)
                        {
                            ChargingProfile.csChargingProfiles.chargingSchedule.chargingSchedulePeriod[i].schedulePresent = true;
                            ischargingSchedulePeriodReceived = true;
                        }
                        else
                        {
                            ChargingProfile.csChargingProfiles.chargingSchedule.chargingSchedulePeriod[i].schedulePresent = false;
                            ischargingSchedulePeriodReceived = false;
                        }
                    }
                    i++;
                }
                for (int k = i; k < SchedulePeriodCount; k++)
                {
                    ChargingProfile.csChargingProfiles.chargingSchedule.chargingSchedulePeriod[k].schedulePresent = false;
                }
            }
            if (cJSON_IsNumber(minChargingRate))
            {
                isminChargingRateReceived = true;
                ChargingProfile.csChargingProfiles.chargingSchedule.minChargingRate = minChargingRate->valuedouble;
            }
            ischargingScheduleReceived = ischargingRateUnitReceived &&
                                         ischargingSchedulePeriodReceived;
        }

        iscsChargingProfilesReceived = ischargingProfileIdReceived &&
                                       isstackLevelReceived &&
                                       ischargingProfilePurposeReceived &&
                                       ischargingProfileKindReceived &&
                                       ischargingScheduleReceived;
        if (iscsChargingProfilesReceived)
            custom_nvs_write_chargingProgile(ChargingProfile);
    }
    return iscsChargingProfilesReceived;
}
void ocpp_struct_init(void)
{
    OcppMsgQueue = xQueueCreate(3, OCPP_MSG_SIZE);

    ESP_LOGV(TAG, "Size of CPHeartbeatRequest is : %d ", sizeof(CPHeartbeatRequest));
    ESP_LOGV(TAG, "Size of CPBootNotificationRequest is : %d ", sizeof(CPBootNotificationRequest));
    ESP_LOGV(TAG, "Size of CPHeartbeatRequest is : %d ", sizeof(CPHeartbeatRequest));
    ESP_LOGV(TAG, "Size of CPStatusNotificationRequest is : %d ", sizeof(CPStatusNotificationRequest));
    ESP_LOGV(TAG, "Size of CPAuthorizeRequest is : %d ", sizeof(CPAuthorizeRequest));
    ESP_LOGV(TAG, "Size of CPStartTransactionRequest is : %d ", sizeof(CPStartTransactionRequest));
    ESP_LOGV(TAG, "Size of CPMeterValuesRequest is : %d ", sizeof(CPMeterValuesRequest));
    ESP_LOGV(TAG, "Size of CPStopTransactionRequest is : %d ", sizeof(CPStopTransactionRequest));
    ESP_LOGV(TAG, "Size of CPFirmwareStatusNotificationRequest is : %d ", sizeof(CPFirmwareStatusNotificationRequest));
    ESP_LOGV(TAG, "Size of CPDiagnosticsStatusNotificationRequest is : %d ", sizeof(CPDiagnosticsStatusNotificationRequest));
    ESP_LOGV(TAG, "Size of CMSRemoteStartTransactionRequest is : %d ", sizeof(CMSRemoteStartTransactionRequest));
    ESP_LOGV(TAG, "Size of CMSRemoteStopTransactionRequest is : %d ", sizeof(CMSRemoteStopTransactionRequest));
    ESP_LOGV(TAG, "Size of CMSCancelReservationRequest is : %d ", sizeof(CMSCancelReservationRequest));
    ESP_LOGV(TAG, "Size of CMSChangeAvailabilityRequest is : %d ", sizeof(CMSChangeAvailabilityRequest));
    ESP_LOGV(TAG, "Size of CMSChangeConfigurationRequest is : %d ", sizeof(CMSChangeConfigurationRequest));
    ESP_LOGV(TAG, "Size of CMSTriggerMessageRequest is : %d ", sizeof(CMSTriggerMessageRequest));
    ESP_LOGV(TAG, "Size of CMSReserveNowRequest is : %d ", sizeof(CMSReserveNowRequest));
    ESP_LOGV(TAG, "Size of CMSResetRequest is : %d ", sizeof(CMSResetRequest));
    ESP_LOGV(TAG, "Size of CMSUpdateFirmwareRequest is : %d ", sizeof(CMSUpdateFirmwareRequest));
    ESP_LOGV(TAG, "Size of CMSSendLocalListRequest is : %d ", sizeof(CMSSendLocalListRequest));
    ESP_LOGV(TAG, "Size of CMSClearCacheRequest is : %d ", sizeof(CMSClearCacheRequest));
    ESP_LOGV(TAG, "Size of CMSGetLocalListVersionRequest is : %d ", sizeof(CMSGetLocalListVersionRequest));
    ESP_LOGV(TAG, "Size of CMSGetConfigurationRequest is : %d ", sizeof(CMSGetConfigurationRequest));
    ESP_LOGV(TAG, "Size of CMSClearChargingProfileRequest is : %d ", sizeof(CMSClearChargingProfileRequest));
    ESP_LOGV(TAG, "Size of CMSDataTransferRequest is : %d ", sizeof(CMSDataTransferRequest));
    ESP_LOGV(TAG, "Size of CMSGetCompositeScheduleRequest is : %d ", sizeof(CMSGetCompositeScheduleRequest));
    ESP_LOGV(TAG, "Size of CMSGetDiagnosticsRequest is : %d ", sizeof(CMSGetDiagnosticsRequest));
    ESP_LOGV(TAG, "Size of CMSSetChargingProfileRequest is : %d ", sizeof(CMSSetChargingProfileRequest));
    ESP_LOGV(TAG, "Size of CMSUnlockConnectorRequest is : %d ", sizeof(CMSUnlockConnectorRequest));
    ESP_LOGV(TAG, "Size of CPRemoteStartTransactionResponse is : %d ", sizeof(CPRemoteStartTransactionResponse));
    ESP_LOGV(TAG, "Size of CPRemoteStopTransactionResponse is : %d ", sizeof(CPRemoteStopTransactionResponse));
    ESP_LOGV(TAG, "Size of CPReserveNowResponse is : %d ", sizeof(CPReserveNowResponse));
    ESP_LOGV(TAG, "Size of CPResetResponse is : %d ", sizeof(CPResetResponse));
    ESP_LOGV(TAG, "Size of CPSendLocalListResponse is : %d ", sizeof(CPSendLocalListResponse));
    ESP_LOGV(TAG, "Size of CPTriggerMessageResponse is : %d ", sizeof(CPTriggerMessageResponse));
    ESP_LOGV(TAG, "Size of CPCancelReservationResponse is : %d ", sizeof(CPCancelReservationResponse));
    ESP_LOGV(TAG, "Size of CPChangeAvailabilityResponse is : %d ", sizeof(CPChangeAvailabilityResponse));
    ESP_LOGV(TAG, "Size of CPChangeConfigurationResponse is : %d ", sizeof(CPChangeConfigurationResponse));
    ESP_LOGV(TAG, "Size of CPClearCacheResponse is : %d ", sizeof(CPClearCacheResponse));
    ESP_LOGV(TAG, "Size of CPGetLocalListVersionResponse is : %d ", sizeof(CPGetLocalListVersionResponse));
    ESP_LOGV(TAG, "Size of CPUpdateFirmwareResponse is : %d ", sizeof(CPUpdateFirmwareResponse));
    ESP_LOGV(TAG, "Size of CPGetDiagnosticsResponse is : %d ", sizeof(CPGetDiagnosticsResponse));
    ESP_LOGV(TAG, "Size of CPGetConfigurationResponse is : %d ", sizeof(CPGetConfigurationResponse));
    ESP_LOGV(TAG, "Size of CMSFirmwareStatusNotificationResponse is : %d ", sizeof(CMSFirmwareStatusNotificationResponse));
    ESP_LOGV(TAG, "Size of CMSDiagnosticsStatusNotificationResponse is : %d ", sizeof(CMSDiagnosticsStatusNotificationResponse));
    ESP_LOGV(TAG, "Size of CMSHeartbeatResponse is : %d ", sizeof(CMSHeartbeatResponse));
    ESP_LOGV(TAG, "Size of CMSBootNotificationResponse is : %d ", sizeof(CMSBootNotificationResponse));
    ESP_LOGV(TAG, "Size of CMSStatusNotificationResponse is : %d ", sizeof(CMSStatusNotificationResponse));
    ESP_LOGV(TAG, "Size of CMSMeterValuesResponse is : %d ", sizeof(CMSMeterValuesResponse));
    ESP_LOGV(TAG, "Size of CMSAuthorizeResponse is : %d ", sizeof(CMSAuthorizeResponse));
    ESP_LOGV(TAG, "Size of CMSStartTransactionResponse is : %d ", sizeof(CMSStartTransactionResponse));
    ESP_LOGV(TAG, "Size of CMSStopTransactionResponse is : %d ", sizeof(CMSStopTransactionResponse));
    ESP_LOGV(TAG, "Size of LocalAuthorizationList is : %d ", sizeof(LocalAuthorizationList));

    char *last_occurence;
    char *delimiter = "/";
    last_occurence = strrchr(config.webSocketURL, '/');

    if (last_occurence != NULL)
    {
        last_occurence += strlen(delimiter);
        setNULL(CPBootNotificationRequest.chargePointSerialNumber);
        memcpy(CPBootNotificationRequest.chargePointSerialNumber, last_occurence, strlen(last_occurence));
    }

    memcpy(CPBootNotificationRequest.chargePointVendor, config.chargePointVendor, strlen(config.chargePointVendor));
    memcpy(CPBootNotificationRequest.chargePointModel, config.chargePointModel, strlen(config.chargePointModel));
    memcpy(CPBootNotificationRequest.chargeBoxSerialNumber, config.serialNumber, strlen(config.serialNumber));

    if (config.vcharge_lite_1_4)
    {
        bool slaveVersionReceived = getSlaveFirmwareVersion();
        if (slaveVersionReceived)
        {
            setNULL(CPBootNotificationRequest.firmwareVersion);
            snprintf(CPBootNotificationRequest.firmwareVersion, sizeof(CPBootNotificationRequest.firmwareVersion), "%s & %s", config.firmwareVersion, config.slavefirmwareVersion);
        }
        else
        {
            memcpy(CPBootNotificationRequest.firmwareVersion, config.firmwareVersion, strlen(config.firmwareVersion));
        }
    }
    else
    {
        memcpy(CPBootNotificationRequest.firmwareVersion, config.firmwareVersion, strlen(config.firmwareVersion));
    }
    memcpy(CPBootNotificationRequest.iccid, config.simIMEINumber, strlen(config.simIMEINumber));
    memcpy(CPBootNotificationRequest.imsi, config.simIMSINumber, strlen(config.simIMSINumber));
    setNULL(CPDiagnosticsStatusNotificationRequest.status);
    memcpy(CPDiagnosticsStatusNotificationRequest.status, "Idle", strlen("Idle"));

    // memcpy(CPBootNotificationRequest.meterType, METER_TYPE, strlen(METER_TYPE));
    // memcpy(CPBootNotificationRequest.meterSerialNumber, METER_SERIAL_NUMBER, strlen(METER_SERIAL_NUMBER));

    for (uint8_t i = 0; i < NUM_OF_CONNECTORS; i++)
    {
        CPStatusNotificationRequest[i].connectorId = i;
        memcpy(CPStatusNotificationRequest[i].status, "Available", strlen("Available"));
        memcpy(CPStatusNotificationRequest[i].errorCode, "NoError", strlen("NoError"));

        memcpy(CPAuthorizeRequest[i].idTag, "12345678", strlen("12345678"));

        CMSCancelReservationRequest.reservationId = 560;

        CMSChangeAvailabilityRequest[i].connectorId = 3;
        memcpy(CMSChangeAvailabilityRequest[i].type, "operative", strlen("operative"));

        memcpy(CMSChangeConfigurationRequest.key, "1234567", strlen("1234567"));
        memcpy(CMSChangeConfigurationRequest.value, "987654", strlen("987654"));

        memcpy(CPFirmwareStatusNotificationRequest.status, "Idle", strlen("Idle"));

        memcpy(CMSTriggerMessageRequest.requestedMessage, "DiagnosticsStatus", strlen("DiagnosticsStatus"));
        CMSTriggerMessageRequest.connectorId = 0;

        CPStartTransactionRequest[i].connectorId = i;
        memcpy(CPStartTransactionRequest[i].idTag, "ABC123", strlen("ABC123"));
        CPStartTransactionRequest[i].meterStart = 0;
        CPStartTransactionRequest[i].reservationId = 0;
        memcpy(CPStartTransactionRequest[i].timestamp, "2024-01-31T14:30:000Z", strlen("2024-01-31T14:30:000Z3"));

        CMSRemoteStopTransactionRequest.transactionId = 8765;

        CMSReserveNowRequest[i].connectorId = i;
        memcpy(CMSReserveNowRequest[i].expiryDate, "2024-02-15T18:30:00Z", strlen("2024-02-15T18:30:00Z"));
        memcpy(CMSReserveNowRequest[i].idTag, "ABC123", strlen("ABC123"));
        memcpy(CMSReserveNowRequest[i].parentidTag, "DEF456", strlen("DEF456"));
        CMSReserveNowRequest[i].reservationId = 56789;

        memcpy(CMSResetRequest.type, "SOFT", strlen("SOFT"));

        memcpy(CMSUpdateFirmwareRequest.location, "http://example.com/firmware-update", strlen("http://example.com/firmware-update"));
        CMSUpdateFirmwareRequest.retries = 3;
        memcpy(CMSUpdateFirmwareRequest.retrieveDate, "2024-02-01T10:30:00Z", strlen("2024-02-01T10:30:00Z"));
        CMSUpdateFirmwareRequest.retryInterval = 3600;

        memcpy(CPStopTransactionRequest[i].idTag, "ABC12345", strlen("ABC12345"));
        CPStopTransactionRequest[i].meterStop = 1500;
        memcpy(CPStopTransactionRequest[i].timestamp, "2024-02-01T15:45:00Z", strlen("2024-02-01T15:45:00Z"));
        CPStopTransactionRequest[i].transactionId = 67890;
        memcpy(CPStopTransactionRequest[i].reason, "EVDisconnected", strlen("EVDisconnected"));

        memcpy(CPStopTransactionRequest[i].transactionData->timestamp, "2024-02-01T15:40:00Z", strlen("2024-02-01T15:40:00Z"));

        memcpy(CPStopTransactionRequest[i].transactionData->sampledValue->value, "20.5", strlen("20.5"));
        memcpy(CPStopTransactionRequest[i].transactionData->sampledValue->context, "Sample.Periodic", strlen("Sample.Periodic"));
        memcpy(CPStopTransactionRequest[i].transactionData->sampledValue->format, "RAW", strlen("RAW"));
        memcpy(CPStopTransactionRequest[i].transactionData->sampledValue->measurand, "Power.Active.Import", strlen("Power.Active.Import"));
        memcpy(CPStopTransactionRequest[i].transactionData->sampledValue->phase, "L2", strlen("L2"));
        memcpy(CPStopTransactionRequest[i].transactionData->sampledValue->location, "EV", strlen("EV"));
        memcpy(CPStopTransactionRequest[i].transactionData->sampledValue->unit, "KW", strlen("KW"));

        CMSSendLocalListRequest.listVersion = 1;
        memcpy(CMSSendLocalListRequest.updateType, "FULL", strlen("FULL"));
        memcpy(CMSSendLocalListRequest.localAuthorizationList->idTag, "ABCD1234", strlen("ABCD1234"));
        memcpy(CMSSendLocalListRequest.localAuthorizationList->idTagInfo.expiryDate, "2024-02-15T18:30:00Z", strlen("2024-02-15T18:30:00Z"));
        memcpy(CMSSendLocalListRequest.localAuthorizationList->idTagInfo.parentidTag, "DEF456", strlen("DEF456"));
        memcpy(CMSSendLocalListRequest.localAuthorizationList->idTagInfo.status, "Accepted", strlen("Accepted"));

        CMSRemoteStartTransactionRequest[i].connectorId = 3;
        memcpy(CMSRemoteStartTransactionRequest[i].idTag, "GHIJ1234", strlen("GHIJ1234"));
        CMSRemoteStartTransactionRequest[i].chargingProfile.chargingProfileId = 1001;
        CMSRemoteStartTransactionRequest[i].chargingProfile.transactionId = 123456;
        CMSRemoteStartTransactionRequest[i].chargingProfile.stackLevel = 1;
        memcpy(CMSRemoteStartTransactionRequest[i].chargingProfile.chargingProfilePurpose, "ChargePointMaxProfile", strlen("ChargePointMaxProfile"));
        memcpy(CMSRemoteStartTransactionRequest[i].chargingProfile.chargingProfileKind, "Absolute", strlen("Absolute"));
        memcpy(CMSRemoteStartTransactionRequest[i].chargingProfile.recurrencyKind, "Daily", strlen("Daily"));
        memcpy(CMSRemoteStartTransactionRequest[i].chargingProfile.validFrom, "2024-01-31T12:00:00Z", strlen("2024-01-31T12:00:00Z"));
        memcpy(CMSRemoteStartTransactionRequest[i].chargingProfile.validTo, "2024-02-01T12:00:00Z", strlen("2024-02-01T12:00:00Z"));

        CMSRemoteStartTransactionRequest[i].chargingProfile.chargingSchedule.duration = 120;
        memcpy(CMSRemoteStartTransactionRequest[i].chargingProfile.chargingSchedule.startSchedule, "2024-01-31T14:00:00Z", strlen("2024-01-31T14:00:00Z"));
        memcpy(CMSRemoteStartTransactionRequest[i].chargingProfile.chargingSchedule.chargingRateUnit, "W", strlen("W"));
        CMSRemoteStartTransactionRequest[i].chargingProfile.chargingSchedule.minChargingRate = 5.4567;

        CMSRemoteStartTransactionRequest[i].chargingProfile.chargingSchedule.chargingSchedulePeriod->startPeriod = 60;
        CMSRemoteStartTransactionRequest[i].chargingProfile.chargingSchedule.chargingSchedulePeriod->limit = 15.1234;
        CMSRemoteStartTransactionRequest[i].chargingProfile.chargingSchedule.chargingSchedulePeriod->numberPhases = 3;

        CPMeterValuesRequest[i].connectorId = i;
        CPMeterValuesRequest[i].transactionId = 123456;
        memcpy(CPMeterValuesRequest[i].meterValue.timestamp, "2024-02-01T15:30:00", strlen("2024-02-01T15:30:00"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[0].context, "Sample.Periodic", strlen("Sample.Periodic"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[0].format, "Raw", strlen("Raw"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[0].location, "Cable", strlen("Cable"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[0].measurand, "Energy.Active.Import.Register", strlen("Energy.Active.Import.Register"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[0].phase, "L1", strlen("L1"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[0].unit, "Wh", strlen("Wh"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[0].value, "0", strlen("0"));

        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[1].context, "Sample.Periodic", strlen("Sample.Periodic"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[1].format, "Raw", strlen("Raw"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[1].location, "Cable", strlen("Cable"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[1].measurand, "Power.Active.Import", strlen("Power.Active.Import"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[1].phase, "L1", strlen("L1"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[1].unit, "kW", strlen("kW"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[1].value, "0", strlen("0"));

        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[2].context, "Sample.Periodic", strlen("Sample.Periodic"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[2].format, "Raw", strlen("Raw"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[2].location, "Cable", strlen("Cable"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[2].measurand, "Voltage", strlen("Voltage"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[2].phase, "L1", strlen("L1"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[2].unit, "V", strlen("V"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[2].value, "0", strlen("0"));

        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[3].context, "Sample.Periodic", strlen("Sample.Periodic"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[3].format, "Raw", strlen("Raw"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[3].location, "Cable", strlen("Cable"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[3].measurand, "Voltage", strlen("Voltage"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[3].phase, "L2", strlen("L2"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[3].unit, "V", strlen("V"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[3].value, "0", strlen("0"));

        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[4].context, "Sample.Periodic", strlen("Sample.Periodic"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[4].format, "Raw", strlen("Raw"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[4].location, "Cable", strlen("Cable"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[4].measurand, "Voltage", strlen("Voltage"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[4].phase, "L3", strlen("L3"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[4].unit, "V", strlen("V"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[4].value, "0", strlen("0"));

        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[5].context, "Sample.Periodic", strlen("Sample.Periodic"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[5].format, "Raw", strlen("Raw"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[5].location, "Cable", strlen("Cable"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[5].measurand, "Current.Import", strlen("Current.Import"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[5].phase, "L1", strlen("L1"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[5].unit, "A", strlen("A"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[5].value, "0", strlen("0"));

        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[6].context, "Sample.Periodic", strlen("Sample.Periodic"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[6].format, "Raw", strlen("Raw"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[6].location, "Cable", strlen("Cable"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[6].measurand, "Current.Import", strlen("Current.Import"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[6].phase, "L2", strlen("L2"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[6].unit, "A", strlen("A"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[6].value, "0", strlen("0"));

        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[7].context, "Sample.Periodic", strlen("Sample.Periodic"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[7].format, "Raw", strlen("Raw"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[7].location, "Cable", strlen("Cable"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[7].measurand, "Current.Import", strlen("Current.Import"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[7].phase, "L3", strlen("L3"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[7].unit, "A", strlen("A"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[7].value, "0", strlen("0"));

        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[8].context, "Sample.Periodic", strlen("Sample.Periodic"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[8].format, "Raw", strlen("Raw"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[8].location, "Cable", strlen("Cable"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[8].measurand, "Temperature", strlen("Temperature"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[8].phase, "L1", strlen("L1"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[8].unit, "Celsius", strlen("Celsius"));
        memcpy(CPMeterValuesRequest[i].meterValue.sampledValue[8].value, "0", strlen("0"));
    }
}

void ocpp_response(const char *jsonString)
{
    // Parse the JSON-formatted string
    cJSON *jsonArray = cJSON_Parse(jsonString);
    if (jsonArray != NULL)
    {
        cJSON *response = cJSON_GetArrayItem(jsonArray, 0);
        if (cJSON_IsNumber(response))
        {
            if (response->valueint == OCPP_CALL)
            {
                cJSON *ResponseMsgId = cJSON_GetArrayItem(jsonArray, 1);
                cJSON *ResponseMsg = cJSON_GetArrayItem(jsonArray, 2);
                cJSON *Payload = cJSON_GetArrayItem(jsonArray, 3);

                if (ResponseMsg != NULL && cJSON_IsString(ResponseMsg))
                {
                    const char *ResponseMsgStr = cJSON_GetStringValue(ResponseMsg);
                    if (strcmp(ResponseMsgStr, "CancelReservation") == 0)
                    {
                        CMSCancelReservationRequestProcess(jsonArray);
                    }
                    else if (strcmp(ResponseMsgStr, "ChangeAvailability") == 0)
                    {
                        CMSChangeAvailabilityRequestProcess(jsonArray);
                    }
                    else if (strcmp(ResponseMsgStr, "ChangeConfiguration") == 0)
                    {
                        CMSChangeConfigurationRequestProcess(jsonArray);
                    }
                    else if (strcmp(ResponseMsgStr, "ClearCache") == 0)
                    {
                        setNULL(CMSClearCacheRequest.UniqId);
                        memcpy(CMSClearCacheRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
                        CMSClearCacheRequest.Received = true;
                    }
                    else if (strcmp(ResponseMsgStr, "GetConfiguration") == 0)
                    {
                        setNULL(CMSGetConfigurationRequest.UniqId);
                        memcpy(CMSGetConfigurationRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
                        CMSGetConfigurationRequest.Received = true;
                    }
                    else if (strcmp(ResponseMsgStr, "RemoteStartTransaction") == 0)
                    {
                        CMSRemoteStartTransactionRequestProcess(jsonArray);
                    }
                    else if (strcmp(ResponseMsgStr, "RemoteStopTransaction") == 0)
                    {
                        CMSRemoteStopTransactionRequestProcess(jsonArray);
                    }
                    else if (strcmp(ResponseMsgStr, "Reset") == 0)
                    {
                        CMSResetRequestProcess(jsonArray);
                    }
                    else if (strcmp(ResponseMsgStr, "TriggerMessage") == 0)
                    {
                        CMSTriggerMessageRequestProcess(jsonArray);
                    }
                    else if (strcmp(ResponseMsgStr, "GetLocalListVersion") == 0)
                    {
                        setNULL(CMSGetLocalListVersionRequest.UniqId);
                        memcpy(CMSGetLocalListVersionRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
                        CMSGetLocalListVersionRequest.Received = true;
                    }
                    else if (strcmp(ResponseMsgStr, "SendLocalList") == 0)
                    {
                        CMSSendLocalListRequestProcess(jsonArray);
                    }
                    else if (strcmp(ResponseMsgStr, "ReserveNow") == 0)
                    {
                        CMSReserveNowRequestProcess(jsonArray);
                    }
                    else if (strcmp(ResponseMsgStr, "UpdateFirmware") == 0)
                    {
                        CMSUpdateFirmwareRequestProcess(jsonArray);
                    }
                    else if (strcmp(ResponseMsgStr, "ClearChargingProfile") == 0)
                    {
                        CMSClearChargingProfileRequestProcess(jsonArray);
                    }
                    else if (strcmp(ResponseMsgStr, "DataTransfer") == 0)
                    {
                        setNULL(CMSDataTransferRequest.UniqId);
                        memcpy(CMSDataTransferRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
                        CMSDataTransferRequest.Received = true;
                    }
                    else if (strcmp(ResponseMsgStr, "GetCompositeSchedule") == 0)
                    {
                        setNULL(CMSGetCompositeScheduleRequest.UniqId);
                        memcpy(CMSGetCompositeScheduleRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
                        CMSGetCompositeScheduleRequest.Received = true;
                    }
                    else if (strcmp(ResponseMsgStr, "GetDiagnostics") == 0)
                    {
                        CMSGetDiagnosticsRequestProcess(jsonArray);
                    }
                    else if (strcmp(ResponseMsgStr, "SetChargingProfile") == 0)
                    {
                        CMSSetChargingProfileRequestProcess(jsonArray);
                    }
                    else if (strcmp(ResponseMsgStr, "UnlockConnector") == 0)
                    {
                        setNULL(CMSUnlockConnectorRequest.UniqId);
                        memcpy(CMSUnlockConnectorRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
                        CMSUnlockConnectorRequest.Received = true;
                    }
                }
            }
            else if (response->valueint == OCPP_RECEIVE)
            {
                // memcpy(bootNotificationReqUniqId,"0123456789ABCDEF",16);
                cJSON *ResponseId = cJSON_GetArrayItem(jsonArray, 1);
                cJSON *Payload = cJSON_GetArrayItem(jsonArray, 2);
                if (strcmp(ResponseId->valuestring, CPHeartbeatRequest.UniqId) == 0)
                {
                    HeartbeatResponseProcess(Payload);
                }
                if (strcmp(ResponseId->valuestring, CPBootNotificationRequest.UniqId) == 0)
                {
                    BootNotificationResponseProcess(Payload);
                }
                else if (strcmp(ResponseId->valuestring, CPStatusNotificationRequest[0].UniqId) == 0)
                {
                    CMSStatusNotificationResponse[0].Received = true;
                }
                else if (strcmp(ResponseId->valuestring, CPStatusNotificationRequest[1].UniqId) == 0)
                {
                    CMSStatusNotificationResponse[1].Received = true;
                }
                else if (strcmp(ResponseId->valuestring, CPStatusNotificationRequest[2].UniqId) == 0)
                {
                    CMSStatusNotificationResponse[2].Received = true;
                }
                else if (strcmp(ResponseId->valuestring, CPStatusNotificationRequest[3].UniqId) == 0)
                {
                    CMSStatusNotificationResponse[3].Received = true;
                }
                else if (strcmp(ResponseId->valuestring, CPMeterValuesRequest[0].UniqId) == 0)
                {
                    CMSMeterValuesResponse[0].Received = true;
                }
                else if (strcmp(ResponseId->valuestring, CPMeterValuesRequest[1].UniqId) == 0)
                {
                    CMSMeterValuesResponse[1].Received = true;
                }
                else if (strcmp(ResponseId->valuestring, CPMeterValuesRequest[2].UniqId) == 0)
                {
                    CMSMeterValuesResponse[2].Received = true;
                }
                else if (strcmp(ResponseId->valuestring, CPMeterValuesRequest[3].UniqId) == 0)
                {
                    CMSMeterValuesResponse[3].Received = true;
                }
                else if (strcmp(ResponseId->valuestring, CPAuthorizeRequest[0].UniqId) == 0)
                {
                    AuthorizationResponseProcess(Payload, 0);
                }
                else if (strcmp(ResponseId->valuestring, CPAuthorizeRequest[1].UniqId) == 0)
                {
                    AuthorizationResponseProcess(Payload, 1);
                }
                else if (strcmp(ResponseId->valuestring, CPAuthorizeRequest[2].UniqId) == 0)
                {
                    AuthorizationResponseProcess(Payload, 2);
                }
                else if (strcmp(ResponseId->valuestring, CPAuthorizeRequest[3].UniqId) == 0)
                {
                    AuthorizationResponseProcess(Payload, 3);
                }
                else if (strcmp(ResponseId->valuestring, CPStartTransactionRequest[0].UniqId) == 0)
                {
                    StartTransactionResponseProcess(Payload, 0);
                }
                else if (strcmp(ResponseId->valuestring, CPStartTransactionRequest[1].UniqId) == 0)
                {
                    StartTransactionResponseProcess(Payload, 1);
                }
                else if (strcmp(ResponseId->valuestring, CPStartTransactionRequest[2].UniqId) == 0)
                {
                    StartTransactionResponseProcess(Payload, 2);
                }
                else if (strcmp(ResponseId->valuestring, CPStartTransactionRequest[3].UniqId) == 0)
                {
                    StartTransactionResponseProcess(Payload, 3);
                }
                else if (strcmp(ResponseId->valuestring, CPStopTransactionRequest[0].UniqId) == 0)
                {
                    StopTransactionResponseProcess(Payload, 0);
                }
                else if (strcmp(ResponseId->valuestring, CPStopTransactionRequest[1].UniqId) == 0)
                {
                    StopTransactionResponseProcess(Payload, 1);
                }
                else if (strcmp(ResponseId->valuestring, CPStopTransactionRequest[2].UniqId) == 0)
                {
                    StopTransactionResponseProcess(Payload, 2);
                }
                else if (strcmp(ResponseId->valuestring, CPStopTransactionRequest[3].UniqId) == 0)
                {
                    StopTransactionResponseProcess(Payload, 3);
                }
                else if (strcmp(ResponseId->valuestring, CPFirmwareStatusNotificationRequest.UniqId) == 0)
                {
                    CMSFirmwareStatusNotificationResponse.Received = true;
                }
            }
            else if (response->valueint == OCPP_ERROR)
            {
            }
            else
            {
            }
        }
    }
    else
    {
        ESP_LOGW(TAG, "Error string Cant parse json");
    }

    cJSON_Delete(jsonArray);
}

void CMSChangeAvailabilityRequestProcess(cJSON *jsonArray)
{
    uint8_t connId = 0;
    bool isTypeReceived = false;
    bool isConnectorIdReceived = false;
    cJSON *ResponseMsgId = cJSON_GetArrayItem(jsonArray, 1);
    cJSON *Payload = cJSON_GetArrayItem(jsonArray, 3);
    cJSON *connectorID = cJSON_GetObjectItem(Payload, "connectorId");
    cJSON *type = cJSON_GetObjectItem(Payload, "type");
    if (cJSON_IsNumber(connectorID))
    {
        isConnectorIdReceived = true;
        connId = (uint8_t)connectorID->valueint;
        setNULL(CMSChangeAvailabilityRequest[connId].UniqId);
        memcpy(CMSChangeAvailabilityRequest[connId].UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
        if (cJSON_IsString(type))
        {
            isTypeReceived = true;
            setNULL(CMSChangeAvailabilityRequest[connId].type);
            memcpy(CMSChangeAvailabilityRequest[connId].type, type->valuestring, strlen(type->valuestring));
            CMSChangeAvailabilityRequest[connId].Received = isConnectorIdReceived & isTypeReceived;
        }
        else
        {
            CMSChangeAvailabilityRequest[connId].Received = false;
        }
    }
}

void CMSChangeConfigurationRequestProcess(cJSON *jsonArray)
{
    bool iskeyReceived = false;
    bool isvalueReceived = false;
    cJSON *ResponseMsgId = cJSON_GetArrayItem(jsonArray, 1);
    cJSON *Payload = cJSON_GetArrayItem(jsonArray, 3);
    cJSON *key = cJSON_GetObjectItem(Payload, "key");
    cJSON *value = cJSON_GetObjectItem(Payload, "value");
    if (cJSON_IsString(key))
    {
        iskeyReceived = true;
        setNULL(CMSChangeConfigurationRequest.UniqId);
        memcpy(CMSChangeConfigurationRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
        setNULL(CMSChangeConfigurationRequest.key);
        memcpy(CMSChangeConfigurationRequest.key, key->valuestring, strlen(key->valuestring));
        if (cJSON_IsString(value))
        {
            isvalueReceived = true;
            setNULL(CMSChangeConfigurationRequest.value);
            memcpy(CMSChangeConfigurationRequest.value, value->valuestring, strlen(value->valuestring));
            CMSChangeConfigurationRequest.Received = iskeyReceived & isvalueReceived;
        }
        else
        {
            CMSChangeConfigurationRequest.Received = false;
        }
    }
}

void CMSCancelReservationRequestProcess(cJSON *jsonArray)
{
    cJSON *ResponseMsgId = cJSON_GetArrayItem(jsonArray, 1);
    cJSON *Payload = cJSON_GetArrayItem(jsonArray, 3);
    cJSON *reservationId = cJSON_GetObjectItem(Payload, "reservationId");
    if (cJSON_IsNumber(reservationId))
    {
        setNULL(CMSCancelReservationRequest.UniqId);
        memcpy(CMSCancelReservationRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
        CMSCancelReservationRequest.reservationId = reservationId->valueint;
        CMSCancelReservationRequest.Received = true;
    }
    else
    {
        CMSCancelReservationRequest.Received = false;
    }
}

void CMSTriggerMessageRequestProcess(cJSON *jsonArray)
{
    cJSON *ResponseMsgId = cJSON_GetArrayItem(jsonArray, 1);
    cJSON *Payload = cJSON_GetArrayItem(jsonArray, 3);
    cJSON *requestedMessage = cJSON_GetObjectItem(Payload, "requestedMessage");
    cJSON *connectorId = cJSON_GetObjectItem(Payload, "connectorId");

    if (cJSON_IsString(requestedMessage))
    {
        setNULL(CMSTriggerMessageRequest.UniqId);
        memcpy(CMSTriggerMessageRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
        setNULL(CMSTriggerMessageRequest.requestedMessage);
        memcpy(CMSTriggerMessageRequest.requestedMessage, requestedMessage->valuestring, strlen(requestedMessage->valuestring));
        if (strcmp(requestedMessage->valuestring, "BootNotification") == 0 ||
            strcmp(requestedMessage->valuestring, "DiagnosticsStatusNotification") == 0 ||
            strcmp(requestedMessage->valuestring, "FirmwareStatusNotification") == 0 ||
            strcmp(requestedMessage->valuestring, "Heartbeat") == 0)
        {
            CMSTriggerMessageRequest.Received = true;
        }
        else if (strcmp(requestedMessage->valuestring, "MeterValues") == 0 || strcmp(requestedMessage->valuestring, "StatusNotification") == 0)
        {
            if (cJSON_IsNumber(connectorId))
            {
                CMSTriggerMessageRequest.Received = true;
                CMSTriggerMessageRequest.connectorId = (uint8_t)connectorId->valueint;
            }
            else
            {
                CMSTriggerMessageRequest.Received = false;
            }
        }
        else
        {
            CMSTriggerMessageRequest.Received = false;
        }
    }
}

void CMSSendLocalListRequestProcess(cJSON *jsonArray)
{
    bool isSendLocalListMessageReceived = false;
    bool isListVersionReceived = false;
    bool isUpdateTypeReceived = false;
    bool isidTagInfoidTagReceived = true;
    cJSON *ResponseMsgId = cJSON_GetArrayItem(jsonArray, 1);
    cJSON *Payload = cJSON_GetArrayItem(jsonArray, 3);
    cJSON *listVersionJson = cJSON_GetObjectItem(Payload, "listVersion");
    cJSON *localAuthorizationListJson = cJSON_GetObjectItem(Payload, "localAuthorizationList");
    cJSON *updateTypeJson = cJSON_GetObjectItem(Payload, "updateType");
    ESP_LOGI(TAG, "Local list reuqest received");
    if (cJSON_IsString(ResponseMsgId))
    {
        ESP_LOGI(TAG, "Received UniqId from SendLocalListRequest: %s", ResponseMsgId->valuestring);
        setNULL(CMSSendLocalListRequest.UniqId);
        memcpy(CMSSendLocalListRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
    }
    if (cJSON_IsNumber(listVersionJson))
    {
        ESP_LOGI(TAG, "Received listVersion from SendLocalListRequest: %d", listVersionJson->valueint);
        isListVersionReceived = true;
        CMSSendLocalListRequest.listVersion = listVersionJson->valueint;
    }
    if (cJSON_IsString(updateTypeJson))
    {
        ESP_LOGI(TAG, "Received updateType from SendLocalListRequest: %s", updateTypeJson->valuestring);
        isUpdateTypeReceived = true;
        setNULL(CMSSendLocalListRequest.updateType);
        memcpy(CMSSendLocalListRequest.updateType, updateTypeJson->valuestring, strlen(updateTypeJson->valuestring));
    }
    if (memcmp(CMSSendLocalListRequest.updateType, "Full", strlen("Full")) == 0)
    {
        ESP_LOGI(TAG, "Received updateType as Full from SendLocalListRequest");
        if (cJSON_IsArray(localAuthorizationListJson))
        {
            ESP_LOGI(TAG, "Received localAuthorizationList as array from SendLocalListRequest");
            int numberOfItems = cJSON_GetArraySize(localAuthorizationListJson);
            if (numberOfItems > LOCAL_LIST_COUNT)
            {
                ESP_LOGW(TAG, "Number of items in localAuthorizationList is more than the maximum supported. Truncating the items to %d", LOCAL_LIST_COUNT);
                numberOfItems = LOCAL_LIST_COUNT;
            }
            ESP_LOGI(TAG, "Number of items in localAuthorizationList: %d", numberOfItems);
            int i = 0;
            cJSON *item;
            cJSON_ArrayForEach(item, localAuthorizationListJson)
            {
                ESP_LOGI(TAG, "Item number: %d", i);
                cJSON *idTag = cJSON_GetObjectItem(item, "idTag");
                cJSON *idTagInfo = cJSON_GetObjectItem(item, "idTagInfo");
                cJSON *expiryDate = cJSON_GetObjectItem(idTagInfo, "expiryDate");
                cJSON *parentidTag = cJSON_GetObjectItem(idTagInfo, "parentidTag");
                cJSON *status = cJSON_GetObjectItem(idTagInfo, "status");

                if (i < LOCAL_LIST_COUNT)
                {
                    if (cJSON_IsString(idTag))
                    {
                        isidTagInfoidTagReceived = isidTagInfoidTagReceived && true;
                        CMSSendLocalListRequest.localAuthorizationList[i].idTagPresent = true;
                        setNULL(CMSSendLocalListRequest.localAuthorizationList[i].idTag);
                        setNULL(CMSSendLocalListRequest.localAuthorizationList[i].idTagInfo.expiryDate);
                        setNULL(CMSSendLocalListRequest.localAuthorizationList[i].idTagInfo.parentidTag);
                        setNULL(CMSSendLocalListRequest.localAuthorizationList[i].idTagInfo.status);
                        memcpy(CMSSendLocalListRequest.localAuthorizationList[i].idTag, idTag->valuestring, strlen(idTag->valuestring));
                        ESP_LOGI(TAG, "Received idTag[%d] from SendLocalListRequest: %s", i, CMSSendLocalListRequest.localAuthorizationList[i].idTag);
                    }
                    else
                    {
                        isidTagInfoidTagReceived = false;
                        ESP_LOGW(TAG, "Received idTag[%d] from SendLocalListRequest is not a string", i);
                    }
                    if (cJSON_IsString(expiryDate))
                    {
                        memcpy(CMSSendLocalListRequest.localAuthorizationList[i].idTagInfo.expiryDate, expiryDate->valuestring, strlen(expiryDate->valuestring));
                        ESP_LOGI(TAG, "Received expiryDate[%d] from SendLocalListRequest: %s", i, CMSSendLocalListRequest.localAuthorizationList[i].idTagInfo.expiryDate);
                    }
                    if (cJSON_IsString(parentidTag))
                    {
                        memcpy(CMSSendLocalListRequest.localAuthorizationList[i].idTagInfo.parentidTag, parentidTag->valuestring, strlen(parentidTag->valuestring));
                        ESP_LOGI(TAG, "Received parentidTag[%d] from SendLocalListRequest: %s", i, CMSSendLocalListRequest.localAuthorizationList[i].idTagInfo.parentidTag);
                    }
                    if (cJSON_IsString(status))
                    {
                        memcpy(CMSSendLocalListRequest.localAuthorizationList[i].idTagInfo.status, status->valuestring, strlen(status->valuestring));
                        ESP_LOGI(TAG, "Received status[%d] from SendLocalListRequest: %s", i, CMSSendLocalListRequest.localAuthorizationList[i].idTagInfo.status);
                    }
                }
                i++;
            }
            ESP_LOGI(TAG, "localAuthorizationList size = %d", i);
            for (int k = i; k < LOCAL_LIST_COUNT; k++)
            {
                CMSSendLocalListRequest.localAuthorizationList[k].idTagPresent = false;
            }
            isSendLocalListMessageReceived = isListVersionReceived & isUpdateTypeReceived & isidTagInfoidTagReceived;
        }
        else
        {
            ESP_LOGI(TAG, "Received localAuthorizationList is not a JSON Array");
            isSendLocalListMessageReceived = isListVersionReceived & isUpdateTypeReceived;
            if (isSendLocalListMessageReceived)
            {
                for (uint8_t index = 0; index < LOCAL_LIST_COUNT; index++)
                {
                    CMSSendLocalListRequest.localAuthorizationList[index].idTagPresent = false;
                    setNULL(CMSSendLocalListRequest.localAuthorizationList[index].idTag);
                    setNULL(CMSSendLocalListRequest.localAuthorizationList[index].idTagInfo.expiryDate);
                    setNULL(CMSSendLocalListRequest.localAuthorizationList[index].idTagInfo.parentidTag);
                    setNULL(CMSSendLocalListRequest.localAuthorizationList[index].idTagInfo.status);
                }
                CMSSendLocalListRequest.Received = true;
            }
            else
            {
                ESP_LOGE(TAG, "Failed to receive SendLocalListRequest, isListVersionReceived = %d, isUpdateTypeReceived = %d", isListVersionReceived, isUpdateTypeReceived);
            }
        }
    }
    else if (memcmp(CMSSendLocalListRequest.updateType, "Differential", strlen("Differential")) == 0)
    {
        if (cJSON_IsArray(localAuthorizationListJson))
        {
            int numberOfItems = cJSON_GetArraySize(localAuthorizationListJson);
            int existingItems = 0;
            for (int j = 0; j < LOCAL_LIST_COUNT; j++)
            {
                if (CMSSendLocalListRequest.localAuthorizationList[j].idTagPresent)
                {
                    existingItems++;
                }
            }
            if (numberOfItems > (LOCAL_LIST_COUNT - existingItems))
            {
                numberOfItems = (LOCAL_LIST_COUNT - existingItems);
            }
            int i = existingItems;
            cJSON *item;
            cJSON_ArrayForEach(item, localAuthorizationListJson)
            {
                cJSON *idTag = cJSON_GetObjectItem(item, "idTag");
                cJSON *idTagInfo = cJSON_GetObjectItem(item, "idTagInfo");
                cJSON *expiryDate = cJSON_GetObjectItem(idTagInfo, "expiryDate");
                cJSON *parentidTag = cJSON_GetObjectItem(idTagInfo, "parentidTag");
                cJSON *status = cJSON_GetObjectItem(idTagInfo, "status");

                if (i < (LOCAL_LIST_COUNT - existingItems))
                {
                    if (cJSON_IsString(idTag))
                    {
                        isidTagInfoidTagReceived = isidTagInfoidTagReceived && true;
                        CMSSendLocalListRequest.localAuthorizationList[i].idTagPresent = true;
                        setNULL(CMSSendLocalListRequest.localAuthorizationList[i].idTag);
                        setNULL(CMSSendLocalListRequest.localAuthorizationList[i].idTagInfo.expiryDate);
                        setNULL(CMSSendLocalListRequest.localAuthorizationList[i].idTagInfo.parentidTag);
                        setNULL(CMSSendLocalListRequest.localAuthorizationList[i].idTagInfo.status);
                        memcpy(CMSSendLocalListRequest.localAuthorizationList[i].idTag, idTag->valuestring, strlen(idTag->valuestring));
                    }
                    else
                    {
                        isidTagInfoidTagReceived = false;
                    }
                    if (cJSON_IsString(expiryDate))
                    {
                        memcpy(CMSSendLocalListRequest.localAuthorizationList[i].idTagInfo.expiryDate, expiryDate->valuestring, strlen(expiryDate->valuestring));
                    }
                    if (cJSON_IsString(parentidTag))
                    {
                        memcpy(CMSSendLocalListRequest.localAuthorizationList[i].idTagInfo.parentidTag, parentidTag->valuestring, strlen(parentidTag->valuestring));
                    }
                    if (cJSON_IsString(status))
                    {
                        memcpy(CMSSendLocalListRequest.localAuthorizationList[i].idTagInfo.status, status->valuestring, strlen(status->valuestring));
                    }
                }
                i++;
            }
            for (int k = i; k < LOCAL_LIST_COUNT; k++)
            {
                CMSSendLocalListRequest.localAuthorizationList[k].idTagPresent = false;
            }
            isSendLocalListMessageReceived = isListVersionReceived & isUpdateTypeReceived & isidTagInfoidTagReceived;
        }
        else
        {
            isSendLocalListMessageReceived = isListVersionReceived & isUpdateTypeReceived;
            if (isSendLocalListMessageReceived)
            {
                for (uint8_t index = 0; index < LOCAL_LIST_COUNT; index++)
                {
                    CMSSendLocalListRequest.localAuthorizationList[index].idTagPresent = false;
                    setNULL(CMSSendLocalListRequest.localAuthorizationList[index].idTag);
                    setNULL(CMSSendLocalListRequest.localAuthorizationList[index].idTagInfo.expiryDate);
                    setNULL(CMSSendLocalListRequest.localAuthorizationList[index].idTagInfo.parentidTag);
                    setNULL(CMSSendLocalListRequest.localAuthorizationList[index].idTagInfo.status);
                }
                CMSSendLocalListRequest.Received = true;
            }
        }
    }
    CMSSendLocalListRequest.Received = isSendLocalListMessageReceived;
    ESP_LOGI(TAG, "Send Local List Response : %d", CMSSendLocalListRequest.Received);
}

void CMSRemoteStartTransactionRequestProcess(cJSON *jsonArray)
{
    uint8_t connId = 0;
    bool isidTagReceived = false;
    bool iscsChargingProfileReceived = false;
    cJSON *ResponseMsgId = cJSON_GetArrayItem(jsonArray, 1);
    cJSON *Payload = cJSON_GetArrayItem(jsonArray, 3);
    cJSON *connectorID = cJSON_GetObjectItem(Payload, "connectorId");
    cJSON *idTag = cJSON_GetObjectItem(Payload, "idTag");
    cJSON *csChargingProfiles = cJSON_GetObjectItem(Payload, "chargingProfile");

    if (cJSON_IsNumber(connectorID))
    {
        if (connectorID->valueint < NUM_OF_CONNECTORS)
        {
            connId = (uint8_t)connectorID->valueint;
            CMSRemoteStartTransactionRequest[connectorID->valueint].isConnectorIdReceived = true;
            CMSRemoteStartTransactionRequest[connectorID->valueint].connectorId = connectorID->valueint;
            if (cJSON_IsObject(csChargingProfiles))
            {
                ChargingProfiles_t ChargingProfile;
                memset(&ChargingProfile, 0, sizeof(ChargingProfile));
                ChargingProfile.connectorId = connectorID->valueint;
                iscsChargingProfileReceived = WriteChargingProfileToFlash(ChargingProfile, csChargingProfiles);
            }
            else
            {
                iscsChargingProfileReceived = true;
            }
        }
    }
    else
    {
        connId = 0;
        CMSRemoteStartTransactionRequest[0].isConnectorIdReceived = true;
    }
    setNULL(CMSRemoteStartTransactionRequest[connId].UniqId);
    if (cJSON_IsString(ResponseMsgId))
    {
        memcpy(CMSRemoteStartTransactionRequest[connId].UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
        if (cJSON_IsString(idTag))
        {
            isidTagReceived = true;
            setNULL(CMSRemoteStartTransactionRequest[connId].idTag);
            memcpy(CMSRemoteStartTransactionRequest[connId].idTag, idTag->valuestring, strlen(idTag->valuestring));
            CMSRemoteStartTransactionRequest[connId].Received = isidTagReceived && iscsChargingProfileReceived;
        }
        else
        {
            CMSRemoteStartTransactionRequest[connId].Received = false;
        }
    }
}

void CMSRemoteStopTransactionRequestProcess(cJSON *jsonArray)
{
    bool istransactionIdReceived = false;
    cJSON *ResponseMsgId = cJSON_GetArrayItem(jsonArray, 1);
    cJSON *Payload = cJSON_GetArrayItem(jsonArray, 3);
    cJSON *transactionId = cJSON_GetObjectItem(Payload, "transactionId");
    if (cJSON_IsNumber(transactionId))
    {
        istransactionIdReceived = true;
        setNULL(CMSRemoteStopTransactionRequest.UniqId);
        memcpy(CMSRemoteStopTransactionRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
        CMSRemoteStopTransactionRequest.transactionId = transactionId->valueint;
        CMSRemoteStopTransactionRequest.Received = istransactionIdReceived;
    }
    else
    {
        CMSRemoteStopTransactionRequest.Received = false;
    }
}

void CMSReserveNowRequestProcess(cJSON *jsonArray)
{
    uint8_t connId = 0;
    bool isidTagReceived = false;
    bool isConnectorIdReceived = false;
    bool isexpiryDateReceived = false;
    bool isreservationIdReceived = false;
    cJSON *ResponseMsgId = cJSON_GetArrayItem(jsonArray, 1);
    cJSON *Payload = cJSON_GetArrayItem(jsonArray, 3);
    cJSON *connectorID = cJSON_GetObjectItem(Payload, "connectorId");
    cJSON *expiryDate = cJSON_GetObjectItem(Payload, "expiryDate");
    cJSON *idTag = cJSON_GetObjectItem(Payload, "idTag");
    cJSON *parentidTag = cJSON_GetObjectItem(Payload, "parentidTag");
    cJSON *reservationId = cJSON_GetObjectItem(Payload, "reservationId");
    if (cJSON_IsNumber(connectorID))
    {
        isConnectorIdReceived = true;
        connId = (uint8_t)connectorID->valueint;
        CMSReserveNowRequest[connId].connectorId = connectorID->valueint;
        setNULL(CMSReserveNowRequest[connId].UniqId);
        memcpy(CMSReserveNowRequest[connId].UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
        if (cJSON_IsString(expiryDate))
        {
            isexpiryDateReceived = true;
            setNULL(CMSReserveNowRequest[connId].expiryDate);
            memcpy(CMSReserveNowRequest[connId].expiryDate, expiryDate->valuestring, strlen(expiryDate->valuestring));
        }
        if (cJSON_IsString(idTag))
        {
            isidTagReceived = true;
            setNULL(CMSReserveNowRequest[connId].idTag);
            memcpy(CMSReserveNowRequest[connId].idTag, idTag->valuestring, strlen(idTag->valuestring));
        }
        if (cJSON_IsString(parentidTag))
        {
            CMSReserveNowRequest[connId].isparentidTagReceived = true;
            setNULL(CMSReserveNowRequest[connId].parentidTag);
            memcpy(CMSReserveNowRequest[connId].parentidTag, parentidTag->valuestring, strlen(parentidTag->valuestring));
        }
        else
        {
            CMSReserveNowRequest[connId].isparentidTagReceived = false;
            setNULL(CMSReserveNowRequest[connId].parentidTag);
        }
        if (cJSON_IsNumber(reservationId))
        {
            isreservationIdReceived = true;
            CMSReserveNowRequest[connId].reservationId = reservationId->valueint;
        }
        CMSReserveNowRequest[connId].Received = isConnectorIdReceived & isexpiryDateReceived & isidTagReceived & isreservationIdReceived;
    }
}

void CMSResetRequestProcess(cJSON *jsonArray)
{
    bool istypeReceived = false;
    cJSON *ResponseMsgId = cJSON_GetArrayItem(jsonArray, 1);
    cJSON *Payload = cJSON_GetArrayItem(jsonArray, 3);
    cJSON *type = cJSON_GetObjectItem(Payload, "type");
    if (cJSON_IsString(type))
    {
        istypeReceived = true;
        setNULL(CMSResetRequest.UniqId);
        memcpy(CMSResetRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
        setNULL(CMSResetRequest.type);
        memcpy(CMSResetRequest.type, type->valuestring, strlen(type->valuestring));
        CMSResetRequest.Received = istypeReceived;
    }
    else
    {
        CMSResetRequest.Received = false;
    }
}

void CMSUpdateFirmwareRequestProcess(cJSON *jsonArray)
{
    bool islocationReceived = false;
    bool isretrieveDateReceived = false;
    cJSON *ResponseMsgId = cJSON_GetArrayItem(jsonArray, 1);
    cJSON *Payload = cJSON_GetArrayItem(jsonArray, 3);
    cJSON *location = cJSON_GetObjectItem(Payload, "location");
    cJSON *retrieveDate = cJSON_GetObjectItem(Payload, "retrieveDate");
    if (cJSON_IsString(location))
    {
        islocationReceived = true;
    }
    if (cJSON_IsString(retrieveDate))
    {
        isretrieveDateReceived = true;
    }
    if (cJSON_IsString(ResponseMsgId))
    {
        setNULL(CMSUpdateFirmwareRequest.UniqId);
        memcpy(CMSUpdateFirmwareRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
        CMSUpdateFirmwareRequest.Received = islocationReceived & isretrieveDateReceived;
    }
    else
    {
        CMSUpdateFirmwareRequest.Received = false;
    }
}

void CMSGetDiagnosticsRequestProcess(cJSON *jsonArray)
{
    bool islocationReceived = false;
    cJSON *ResponseMsgId = cJSON_GetArrayItem(jsonArray, 1);
    cJSON *Payload = cJSON_GetArrayItem(jsonArray, 3);
    cJSON *location = cJSON_GetObjectItem(Payload, "location");
    if (cJSON_IsString(location))
    {
        islocationReceived = true;
        setNULL(CMSGetDiagnosticsRequest.location);
        memcpy(CMSGetDiagnosticsRequest.location, location->valuestring, strlen(location->valuestring));
    }
    if (cJSON_IsString(ResponseMsgId))
    {
        setNULL(CMSGetDiagnosticsRequest.UniqId);
        memcpy(CMSGetDiagnosticsRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
        CMSGetDiagnosticsRequest.Received = islocationReceived;
    }
}

void CMSClearChargingProfileRequestProcess(cJSON *jsonArray)
{
    cJSON *ResponseMsgId = cJSON_GetArrayItem(jsonArray, 1);
    cJSON *Payload = cJSON_GetArrayItem(jsonArray, 3);
    cJSON *id = cJSON_GetObjectItem(Payload, "id");
    cJSON *connectorId = cJSON_GetObjectItem(Payload, "connectorId");
    cJSON *chargingProfilePurpose = cJSON_GetObjectItem(Payload, "chargingProfilePurpose");
    cJSON *stackLevel = cJSON_GetObjectItem(Payload, "stackLevel");

    if (cJSON_IsNumber(id))
    {
        CMSClearChargingProfileRequest.id = id->valueint;
    }
    if (cJSON_IsNumber(connectorId))
    {
        CMSClearChargingProfileRequest.connectorId = connectorId->valueint;
    }
    if (cJSON_IsString(chargingProfilePurpose))
    {
        if (memcmp(chargingProfilePurpose->valuestring, "ChargePointMaxProfile", strlen(chargingProfilePurpose->valuestring)) == 0)
        {
            CMSClearChargingProfileRequest.chargingProfilePurpose = ChargePointMaxProfile;
        }
        else if (memcmp(chargingProfilePurpose->valuestring, "TxProfile", strlen(chargingProfilePurpose->valuestring)) == 0)
        {
            CMSClearChargingProfileRequest.chargingProfilePurpose = TxProfile;
        }
        else if (memcmp(chargingProfilePurpose->valuestring, "TxDefaultProfile", strlen(chargingProfilePurpose->valuestring)) == 0)
        {
            CMSClearChargingProfileRequest.chargingProfilePurpose = TxDefaultProfile;
        }
        else
        {
            CMSClearChargingProfileRequest.chargingProfilePurpose = 0;
        }
    }
    if (cJSON_IsNumber(stackLevel))
    {
        CMSClearChargingProfileRequest.stackLevel = stackLevel->valueint;
    }

    if (cJSON_IsString(ResponseMsgId))
    {
        setNULL(CMSClearChargingProfileRequest.UniqId);
        memcpy(CMSClearChargingProfileRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
        CMSClearChargingProfileRequest.Received = true;
    }
}

void CMSSetChargingProfileRequestProcess(cJSON *jsonArray)
{
    ChargingProfiles_t ChargingProfile;
    memset(&ChargingProfile, 0, sizeof(ChargingProfile));
    bool isconnectorIdReceived = false;
    bool iscsChargingProfilesReceived = false;
    cJSON *ResponseMsgId = cJSON_GetArrayItem(jsonArray, 1);
    cJSON *Payload = cJSON_GetArrayItem(jsonArray, 3);
    cJSON *connectorId = cJSON_GetObjectItem(Payload, "connectorId");
    cJSON *csChargingProfiles = cJSON_GetObjectItem(Payload, "csChargingProfiles");

    if (cJSON_IsString(ResponseMsgId))
    {
        setNULL(CMSSetChargingProfileRequest.UniqId);
        memcpy(CMSSetChargingProfileRequest.UniqId, ResponseMsgId->valuestring, strlen(ResponseMsgId->valuestring));
        if (cJSON_IsNumber(connectorId))
        {
            isconnectorIdReceived = true;
            ChargingProfile.connectorId = connectorId->valueint;
            iscsChargingProfilesReceived = WriteChargingProfileToFlash(ChargingProfile, csChargingProfiles);
        }
        CMSSetChargingProfileRequest.Received = isconnectorIdReceived && iscsChargingProfilesReceived;
    }
}

/********************************************REQUEST START****************************************************************/
void sendHeartbeatRequest(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();
    // Generate Random unique Id for boot notification
    sprintf(CPHeartbeatRequest.UniqId, "%llu", randomNumber++);
    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_CALL));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CPHeartbeatRequest.UniqId));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString("Heartbeat"));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPHeartbeatRequest.Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendBootNotificationRequest(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();
    // Generate Random unique Id for boot notification

    sprintf(CPBootNotificationRequest.UniqId, "%llu", randomNumber++);
    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_CALL));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CPBootNotificationRequest.UniqId));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString("BootNotification"));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "chargePointVendor", CPBootNotificationRequest.chargePointVendor);
    cJSON_AddStringToObject(object, "chargePointModel", CPBootNotificationRequest.chargePointModel);
    cJSON_AddStringToObject(object, "chargePointSerialNumber", CPBootNotificationRequest.chargePointSerialNumber);
    cJSON_AddStringToObject(object, "chargeBoxSerialNumber", CPBootNotificationRequest.chargeBoxSerialNumber);
    cJSON_AddStringToObject(object, "firmwareVersion", CPBootNotificationRequest.firmwareVersion);
    if (config.gsmAvailability && config.gsmEnable)
    {
        cJSON_AddStringToObject(object, "iccid", CPBootNotificationRequest.iccid);
        cJSON_AddStringToObject(object, "imsi", CPBootNotificationRequest.imsi);
    }
    // cJSON_AddStringToObject(object, "meterType", CPBootNotificationRequest.meterType);
    // cJSON_AddStringToObject(object, "meterSerialNumber", CPBootNotificationRequest.meterSerialNumber);
    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPBootNotificationRequest.Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendStatusNotificationRequest(uint8_t connId)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();
    // Generate Random unique Id for boot notification
    sprintf(CPStatusNotificationRequest[connId].UniqId, "%llu", randomNumber++);
    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_CALL));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CPStatusNotificationRequest[connId].UniqId));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString("StatusNotification"));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddNumberToObject(object, "connectorId", CPStatusNotificationRequest[connId].connectorId);
    cJSON_AddStringToObject(object, "errorCode", CPStatusNotificationRequest[connId].errorCode);
    cJSON_AddStringToObject(object, "status", CPStatusNotificationRequest[connId].status);

    cJSON_AddStringToObject(object, "timestamp", CPStatusNotificationRequest[connId].timestamp);

    if (memcmp(CPStatusNotificationRequest[connId].errorCode, "OtherError", strlen("OtherError")) == 0)
    {
        cJSON_AddStringToObject(object, "vendorErrorCode", CPStatusNotificationRequest[connId].vendorErrorCode);
    }

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_STATUS, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPStatusNotificationRequest[connId].Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_STATUS, "Failed to serialize JSON");
    }

    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendAuthorizationRequest(uint8_t connId)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();
    // Generate Random unique Id for boot notification

    sprintf(CPAuthorizeRequest[connId].UniqId, "%llu", randomNumber++);
    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_CALL));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CPAuthorizeRequest[connId].UniqId));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString("Authorize"));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "idTag", CPAuthorizeRequest[connId].idTag);

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_AUTH, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPAuthorizeRequest[connId].Sent = true;
        CMSAuthorizeResponse[connId].Received = false;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_AUTH, "Failed to serialize JSON");
    }

    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendFirmwareStatusNotificationRequest(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();
    // Generate Random unique Id for boot notification

    sprintf(CPFirmwareStatusNotificationRequest.UniqId, "%llu", randomNumber++);
    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_CALL));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CPFirmwareStatusNotificationRequest.UniqId));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString("FirmwareStatusNotification"));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "status", CPFirmwareStatusNotificationRequest.status);

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_AUTH, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPFirmwareStatusNotificationRequest.Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_AUTH, "Failed to serialize JSON");
    }

    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendDiagnosticsStatusNotificationRequest(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();
    // Generate Random unique Id for boot notification

    sprintf(CPDiagnosticsStatusNotificationRequest.UniqId, "%llu", randomNumber++);
    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_CALL));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CPDiagnosticsStatusNotificationRequest.UniqId));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString("DiagnosticsStatusNotification"));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "status", CPDiagnosticsStatusNotificationRequest.status);

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_AUTH, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPDiagnosticsStatusNotificationRequest.Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_AUTH, "Failed to serialize JSON");
    }

    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendStartTransactionRequest(uint8_t connId)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();
    // Generate Random unique Id for boot notification

    sprintf(CPStartTransactionRequest[connId].UniqId, "%llu", randomNumber++);
    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_CALL));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CPStartTransactionRequest[connId].UniqId));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString("StartTransaction"));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddNumberToObject(object, "connectorId", CPStartTransactionRequest[connId].connectorId);
    cJSON_AddStringToObject(object, "idTag", CPStartTransactionRequest[connId].idTag);
    cJSON_AddNumberToObject(object, "meterStart", CPStartTransactionRequest[connId].meterStart);
    if (CPStartTransactionRequest[connId].reservationIdPresent)
    {
        cJSON_AddNumberToObject(object, "reservationId", CPStartTransactionRequest[connId].reservationId);
    }
    cJSON_AddStringToObject(object, "timestamp", CPStartTransactionRequest[connId].timestamp);

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPStartTransactionRequest[connId].Sent = true;
        CMSStartTransactionResponse[connId].Received = false;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendStopTransactionRequest(uint8_t connId, uint8_t context, bool Aligned, bool offline)
{
    char ContextValuestr[20];
    setNULL(ContextValuestr);

    switch (context)
    {
    case TransactionBegin:
        memcpy(ContextValuestr, "Transaction.Begin", strlen("Transaction.Begin"));
        break;
    case TransactionEnd:
        memcpy(ContextValuestr, "Transaction.End", strlen("Transaction.End"));
        break;
    case SampleClock:
        memcpy(ContextValuestr, "Sample.Clock", strlen("Sample.Clock"));
        break;
    case SamplePeriodic:
        memcpy(ContextValuestr, "Sample.Periodic", strlen("Sample.Periodic"));
        break;
    case InterruptionBegin:
        memcpy(ContextValuestr, "Interruption.Begin", strlen("Interruption.Begin"));
        break;
    case InterruptionEnd:
        memcpy(ContextValuestr, "Interruption.End", strlen("Interruption.End"));
        break;
    case Trigger:
        memcpy(ContextValuestr, "Trigger", strlen("Trigger"));
        break;
    default:
        memcpy(ContextValuestr, "Other", strlen("Other"));
        break;
    }

    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();
    // Generate Random unique Id for boot notification

    sprintf(CPStopTransactionRequest[connId].UniqId, "%llu", randomNumber++);
    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_CALL));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CPStopTransactionRequest[connId].UniqId));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString("StopTransaction"));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "idTag", CPStopTransactionRequest[connId].idTag);
    cJSON_AddNumberToObject(object, "meterStop", CPStopTransactionRequest[connId].meterStop);
    cJSON_AddStringToObject(object, "timestamp", CPStopTransactionRequest[connId].timestamp);
    cJSON_AddNumberToObject(object, "transactionId", CPStopTransactionRequest[connId].transactionId);
    cJSON_AddStringToObject(object, "reason", CPStopTransactionRequest[connId].reason);
    if (offline == false)
    {
        cJSON *transactionDataArray = cJSON_CreateArray();
        cJSON *transactionDataObject = cJSON_CreateObject();
        cJSON_AddStringToObject(transactionDataObject, "timestamp", CPStopTransactionRequest[connId].timestamp);

        cJSON *sampledValueArray = cJSON_CreateArray();
        if ((Aligned && CPGetConfigurationResponse.StopTxnAlignedData.EnergyActiveImportRegister) ||
            ((Aligned == false) && CPGetConfigurationResponse.StopTxnSampledData.EnergyActiveImportRegister))
        {
            cJSON *sampledValueObject0 = cJSON_CreateObject();
            cJSON_AddStringToObject(sampledValueObject0, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[0].value);
            cJSON_AddStringToObject(sampledValueObject0, "context", ContextValuestr);
            // cJSON_AddStringToObject(sampledValueObject0, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[0].format);
            cJSON_AddStringToObject(sampledValueObject0, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[0].measurand);
            // cJSON_AddStringToObject(sampledValueObject0, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[0].phase);
            // cJSON_AddStringToObject(sampledValueObject0, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[0].location);
            // cJSON_AddStringToObject(sampledValueObject0, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[0].unit);
            cJSON_AddItemToArray(sampledValueArray, sampledValueObject0);
        }
        if ((Aligned && CPGetConfigurationResponse.StopTxnAlignedData.PowerActiveImport) ||
            ((Aligned == false) && CPGetConfigurationResponse.StopTxnSampledData.PowerActiveImport))
        {
            cJSON *sampledValueObject1 = cJSON_CreateObject();
            cJSON_AddStringToObject(sampledValueObject1, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[1].value);
            cJSON_AddStringToObject(sampledValueObject1, "context", ContextValuestr);
            // cJSON_AddStringToObject(sampledValueObject1, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[1].format);
            cJSON_AddStringToObject(sampledValueObject1, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[1].measurand);
            // cJSON_AddStringToObject(sampledValueObject1, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[1].phase);
            // cJSON_AddStringToObject(sampledValueObject1, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[1].location);
            // cJSON_AddStringToObject(sampledValueObject1, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[1].unit);
            cJSON_AddItemToArray(sampledValueArray, sampledValueObject1);
        }
        if (((Aligned && CPGetConfigurationResponse.MeterValuesAlignedData.Voltage) ||
             ((Aligned == false) && CPGetConfigurationResponse.MeterValuesSampledData.Voltage)) &&
            ((config.Ac11s || config.Ac22s || config.Ac44s) || (connId == 1)))
        {
            cJSON *sampledValueObject2 = cJSON_CreateObject();
            cJSON_AddStringToObject(sampledValueObject2, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[2].value);
            cJSON_AddStringToObject(sampledValueObject2, "context", ContextValuestr);
            // cJSON_AddStringToObject(sampledValueObject2, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[2].format);
            cJSON_AddStringToObject(sampledValueObject2, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[2].measurand);
            cJSON_AddStringToObject(sampledValueObject2, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[2].phase);
            // cJSON_AddStringToObject(sampledValueObject2, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[2].location);
            // cJSON_AddStringToObject(sampledValueObject2, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[2].unit);
            cJSON_AddItemToArray(sampledValueArray, sampledValueObject2);
        }
        if (((Aligned && CPGetConfigurationResponse.StopTxnAlignedData.Voltage) ||
             ((Aligned == false) && CPGetConfigurationResponse.StopTxnSampledData.Voltage)) &&
            ((config.Ac11s || config.Ac22s || config.Ac44s) || (connId == 2)))
        {
            cJSON *sampledValueObject3 = cJSON_CreateObject();
            cJSON_AddStringToObject(sampledValueObject3, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[3].value);
            cJSON_AddStringToObject(sampledValueObject3, "context", ContextValuestr);
            // cJSON_AddStringToObject(sampledValueObject3, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[3].format);
            cJSON_AddStringToObject(sampledValueObject3, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[3].measurand);
            cJSON_AddStringToObject(sampledValueObject3, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[3].phase);
            // cJSON_AddStringToObject(sampledValueObject3, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[3].location);
            // cJSON_AddStringToObject(sampledValueObject3, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[3].unit);
            cJSON_AddItemToArray(sampledValueArray, sampledValueObject3);
        }
        if (((Aligned && CPGetConfigurationResponse.StopTxnAlignedData.Voltage) ||
             ((Aligned == false) && CPGetConfigurationResponse.StopTxnSampledData.Voltage)) &&
            ((config.Ac11s || config.Ac22s || config.Ac44s) || (connId == 3)))
        {
            cJSON *sampledValueObject4 = cJSON_CreateObject();
            cJSON_AddStringToObject(sampledValueObject4, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[4].value);
            cJSON_AddStringToObject(sampledValueObject4, "context", ContextValuestr);
            // cJSON_AddStringToObject(sampledValueObject4, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[4].format);
            cJSON_AddStringToObject(sampledValueObject4, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[4].measurand);
            cJSON_AddStringToObject(sampledValueObject4, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[4].phase);
            // cJSON_AddStringToObject(sampledValueObject4, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[4].location);
            // cJSON_AddStringToObject(sampledValueObject4, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[4].unit);
            cJSON_AddItemToArray(sampledValueArray, sampledValueObject4);
        }
        if (((Aligned && CPGetConfigurationResponse.StopTxnAlignedData.CurrentImport) ||
             ((Aligned == false) && CPGetConfigurationResponse.StopTxnSampledData.CurrentImport)) &&
            ((config.Ac11s || config.Ac22s || config.Ac44s) || (connId == 1)))
        {
            cJSON *sampledValueObject5 = cJSON_CreateObject();
            cJSON_AddStringToObject(sampledValueObject5, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[5].value);
            cJSON_AddStringToObject(sampledValueObject5, "context", ContextValuestr);
            // cJSON_AddStringToObject(sampledValueObject5, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[5].format);
            cJSON_AddStringToObject(sampledValueObject5, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[5].measurand);
            cJSON_AddStringToObject(sampledValueObject5, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[5].phase);
            // cJSON_AddStringToObject(sampledValueObject5, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[5].location);
            // cJSON_AddStringToObject(sampledValueObject5, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[5].unit);
            cJSON_AddItemToArray(sampledValueArray, sampledValueObject5);
        }
        if (((Aligned && CPGetConfigurationResponse.StopTxnAlignedData.CurrentImport) ||
             ((Aligned == false) && CPGetConfigurationResponse.StopTxnSampledData.CurrentImport)) &&
            ((config.Ac11s || config.Ac22s || config.Ac44s) || (connId == 2)))
        {
            cJSON *sampledValueObject6 = cJSON_CreateObject();
            cJSON_AddStringToObject(sampledValueObject6, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[6].value);
            cJSON_AddStringToObject(sampledValueObject6, "context", ContextValuestr);
            // cJSON_AddStringToObject(sampledValueObject6, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[6].format);
            cJSON_AddStringToObject(sampledValueObject6, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[6].measurand);
            cJSON_AddStringToObject(sampledValueObject6, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[6].phase);
            // cJSON_AddStringToObject(sampledValueObject6, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[6].location);
            // cJSON_AddStringToObject(sampledValueObject6, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[6].unit);
            cJSON_AddItemToArray(sampledValueArray, sampledValueObject6);
        }
        if (((Aligned && CPGetConfigurationResponse.StopTxnAlignedData.CurrentImport) ||
             ((Aligned == false) && CPGetConfigurationResponse.StopTxnSampledData.CurrentImport)) &&
            ((config.Ac11s || config.Ac22s || config.Ac44s) || (connId == 3)))
        {
            cJSON *sampledValueObject7 = cJSON_CreateObject();
            cJSON_AddStringToObject(sampledValueObject7, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[7].value);
            cJSON_AddStringToObject(sampledValueObject7, "context", ContextValuestr);
            // cJSON_AddStringToObject(sampledValueObject7, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[7].format);
            cJSON_AddStringToObject(sampledValueObject7, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[7].measurand);
            cJSON_AddStringToObject(sampledValueObject7, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[7].phase);
            // cJSON_AddStringToObject(sampledValueObject7, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[7].location);
            // cJSON_AddStringToObject(sampledValueObject7, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[7].unit);
            cJSON_AddItemToArray(sampledValueArray, sampledValueObject7);
        }
        if ((Aligned && CPGetConfigurationResponse.StopTxnAlignedData.Temperature) ||
            ((Aligned == false) && CPGetConfigurationResponse.StopTxnSampledData.Temperature))
        {
            cJSON *sampledValueObject8 = cJSON_CreateObject();
            cJSON_AddStringToObject(sampledValueObject8, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[8].value);
            cJSON_AddStringToObject(sampledValueObject8, "context", ContextValuestr);
            // cJSON_AddStringToObject(sampledValueObject8, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[8].format);
            cJSON_AddStringToObject(sampledValueObject8, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[8].measurand);
            // cJSON_AddStringToObject(sampledValueObject8, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[8].unit);
            cJSON_AddItemToArray(sampledValueArray, sampledValueObject8);
        }

        cJSON_AddItemToObject(transactionDataObject, "sampledValue", sampledValueArray);
        cJSON_AddItemToArray(transactionDataArray, transactionDataObject);

        cJSON_AddItemToObject(object, "transactionData", transactionDataArray);
    }
    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPStopTransactionRequest[connId].Sent = true;
        CMSStopTransactionResponse[connId].Received = false;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendMeterValuesRequest(uint8_t connId, uint8_t context, bool Aligned)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();
    // Generate Random unique Id for boot notification
    char ContextValuestr[20];
    setNULL(ContextValuestr);

    switch (context)
    {
    case TransactionBegin:
        memcpy(ContextValuestr, "Transaction.Begin", strlen("Transaction.Begin"));
        break;
    case TransactionEnd:
        memcpy(ContextValuestr, "Transaction.End", strlen("Transaction.End"));
        break;
    case SampleClock:
        memcpy(ContextValuestr, "Sample.Clock", strlen("Sample.Clock"));
        break;
    case SamplePeriodic:
        memcpy(ContextValuestr, "Sample.Periodic", strlen("Sample.Periodic"));
        break;
    case InterruptionBegin:
        memcpy(ContextValuestr, "Interruption.Begin", strlen("Interruption.Begin"));
        break;
    case InterruptionEnd:
        memcpy(ContextValuestr, "Interruption.End", strlen("Interruption.End"));
        break;
    case Trigger:
        memcpy(ContextValuestr, "Trigger", strlen("Trigger"));
        break;
    default:
        memcpy(ContextValuestr, "Other", strlen("Other"));
        break;
    }

    sprintf(CPMeterValuesRequest[connId].UniqId, "%llu", randomNumber++);
    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_CALL));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CPMeterValuesRequest[connId].UniqId));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString("MeterValues"));

    // Create an object and add it to the cJSON array
    cJSON *meterValuesRequest = cJSON_CreateObject();

    cJSON_AddNumberToObject(meterValuesRequest, "connectorId", CPMeterValuesRequest[connId].connectorId);
    cJSON_AddNumberToObject(meterValuesRequest, "transactionId", CPMeterValuesRequest[connId].transactionId);

    cJSON *meterValueArray = cJSON_CreateArray();
    cJSON *meterValueObject = cJSON_CreateObject();
    cJSON_AddStringToObject(meterValueObject, "timestamp", CPMeterValuesRequest[connId].meterValue.timestamp);

    cJSON *sampledValueArray = cJSON_CreateArray();
    if ((Aligned && CPGetConfigurationResponse.MeterValuesAlignedData.EnergyActiveImportRegister) ||
        ((Aligned == false) && CPGetConfigurationResponse.MeterValuesSampledData.EnergyActiveImportRegister))
    {
        cJSON *sampledValueObject0 = cJSON_CreateObject();
        cJSON_AddStringToObject(sampledValueObject0, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[0].value);
        cJSON_AddStringToObject(sampledValueObject0, "context", ContextValuestr);
        // cJSON_AddStringToObject(sampledValueObject0, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[0].format);
        cJSON_AddStringToObject(sampledValueObject0, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[0].measurand);
        // cJSON_AddStringToObject(sampledValueObject0, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[0].phase);
        // cJSON_AddStringToObject(sampledValueObject0, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[0].location);
        // cJSON_AddStringToObject(sampledValueObject0, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[0].unit);
        cJSON_AddItemToArray(sampledValueArray, sampledValueObject0);
    }
    if ((Aligned && CPGetConfigurationResponse.MeterValuesAlignedData.PowerActiveImport) ||
        ((Aligned == false) && CPGetConfigurationResponse.MeterValuesSampledData.PowerActiveImport))
    {
        cJSON *sampledValueObject1 = cJSON_CreateObject();
        cJSON_AddStringToObject(sampledValueObject1, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[1].value);
        cJSON_AddStringToObject(sampledValueObject1, "context", ContextValuestr);
        // cJSON_AddStringToObject(sampledValueObject1, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[1].format);
        cJSON_AddStringToObject(sampledValueObject1, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[1].measurand);
        // cJSON_AddStringToObject(sampledValueObject1, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[1].phase);
        // cJSON_AddStringToObject(sampledValueObject1, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[1].location);
        // cJSON_AddStringToObject(sampledValueObject1, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[1].unit);
        cJSON_AddItemToArray(sampledValueArray, sampledValueObject1);
    }
    if (((Aligned && CPGetConfigurationResponse.MeterValuesAlignedData.Voltage) ||
         ((Aligned == false) && CPGetConfigurationResponse.MeterValuesSampledData.Voltage)) &&
        ((config.Ac11s || config.Ac22s || config.Ac44s) || (connId == 1)))
    {
        cJSON *sampledValueObject2 = cJSON_CreateObject();
        cJSON_AddStringToObject(sampledValueObject2, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[2].value);
        cJSON_AddStringToObject(sampledValueObject2, "context", ContextValuestr);
        // cJSON_AddStringToObject(sampledValueObject2, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[2].format);
        cJSON_AddStringToObject(sampledValueObject2, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[2].measurand);
        cJSON_AddStringToObject(sampledValueObject2, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[2].phase);
        // cJSON_AddStringToObject(sampledValueObject2, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[2].location);
        // cJSON_AddStringToObject(sampledValueObject2, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[2].unit);
        cJSON_AddItemToArray(sampledValueArray, sampledValueObject2);
    }
    if (((Aligned && CPGetConfigurationResponse.MeterValuesAlignedData.Voltage) ||
         ((Aligned == false) && CPGetConfigurationResponse.MeterValuesSampledData.Voltage)) &&
        ((config.Ac11s || config.Ac22s || config.Ac44s) || (connId == 2)))
    {
        cJSON *sampledValueObject3 = cJSON_CreateObject();
        cJSON_AddStringToObject(sampledValueObject3, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[3].value);
        cJSON_AddStringToObject(sampledValueObject3, "context", ContextValuestr);
        // cJSON_AddStringToObject(sampledValueObject3, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[3].format);
        cJSON_AddStringToObject(sampledValueObject3, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[3].measurand);
        cJSON_AddStringToObject(sampledValueObject3, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[3].phase);
        // cJSON_AddStringToObject(sampledValueObject3, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[3].location);
        // cJSON_AddStringToObject(sampledValueObject3, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[3].unit);
        cJSON_AddItemToArray(sampledValueArray, sampledValueObject3);
    }
    if (((Aligned && CPGetConfigurationResponse.MeterValuesAlignedData.Voltage) ||
         ((Aligned == false) && CPGetConfigurationResponse.MeterValuesSampledData.Voltage)) &&
        ((config.Ac11s || config.Ac22s || config.Ac44s) || (connId == 3)))
    {
        cJSON *sampledValueObject4 = cJSON_CreateObject();
        cJSON_AddStringToObject(sampledValueObject4, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[4].value);
        cJSON_AddStringToObject(sampledValueObject4, "context", ContextValuestr);
        // cJSON_AddStringToObject(sampledValueObject4, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[4].format);
        cJSON_AddStringToObject(sampledValueObject4, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[4].measurand);
        cJSON_AddStringToObject(sampledValueObject4, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[4].phase);
        // cJSON_AddStringToObject(sampledValueObject4, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[4].location);
        // cJSON_AddStringToObject(sampledValueObject4, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[4].unit);
        cJSON_AddItemToArray(sampledValueArray, sampledValueObject4);
    }
    if (((Aligned && CPGetConfigurationResponse.MeterValuesAlignedData.CurrentImport) ||
         ((Aligned == false) && CPGetConfigurationResponse.MeterValuesSampledData.CurrentImport)) &&
        ((config.Ac11s || config.Ac22s || config.Ac44s) || (connId == 1)))
    {
        cJSON *sampledValueObject5 = cJSON_CreateObject();
        cJSON_AddStringToObject(sampledValueObject5, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[5].value);
        cJSON_AddStringToObject(sampledValueObject5, "context", ContextValuestr);
        // cJSON_AddStringToObject(sampledValueObject5, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[5].format);
        cJSON_AddStringToObject(sampledValueObject5, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[5].measurand);
        cJSON_AddStringToObject(sampledValueObject5, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[5].phase);
        // cJSON_AddStringToObject(sampledValueObject5, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[5].location);
        // cJSON_AddStringToObject(sampledValueObject5, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[5].unit);
        cJSON_AddItemToArray(sampledValueArray, sampledValueObject5);
    }
    if (((Aligned && CPGetConfigurationResponse.MeterValuesAlignedData.CurrentImport) ||
         ((Aligned == false) && CPGetConfigurationResponse.MeterValuesSampledData.CurrentImport)) &&
        ((config.Ac11s || config.Ac22s || config.Ac44s) || (connId == 2)))
    {
        cJSON *sampledValueObject6 = cJSON_CreateObject();
        cJSON_AddStringToObject(sampledValueObject6, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[6].value);
        cJSON_AddStringToObject(sampledValueObject6, "context", ContextValuestr);
        // cJSON_AddStringToObject(sampledValueObject6, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[6].format);
        cJSON_AddStringToObject(sampledValueObject6, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[6].measurand);
        cJSON_AddStringToObject(sampledValueObject6, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[6].phase);
        // cJSON_AddStringToObject(sampledValueObject6, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[6].location);
        // cJSON_AddStringToObject(sampledValueObject6, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[6].unit);
        cJSON_AddItemToArray(sampledValueArray, sampledValueObject6);
    }
    if (((Aligned && CPGetConfigurationResponse.MeterValuesAlignedData.CurrentImport) ||
         ((Aligned == false) && CPGetConfigurationResponse.MeterValuesSampledData.CurrentImport)) &&
        ((config.Ac11s || config.Ac22s || config.Ac44s) || (connId == 3)))
    {
        cJSON *sampledValueObject7 = cJSON_CreateObject();
        cJSON_AddStringToObject(sampledValueObject7, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[7].value);
        cJSON_AddStringToObject(sampledValueObject7, "context", ContextValuestr);
        // cJSON_AddStringToObject(sampledValueObject7, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[7].format);
        cJSON_AddStringToObject(sampledValueObject7, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[7].measurand);
        cJSON_AddStringToObject(sampledValueObject7, "phase", CPMeterValuesRequest[connId].meterValue.sampledValue[7].phase);
        // cJSON_AddStringToObject(sampledValueObject7, "location", CPMeterValuesRequest[connId].meterValue.sampledValue[7].location);
        // cJSON_AddStringToObject(sampledValueObject7, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[7].unit);
        cJSON_AddItemToArray(sampledValueArray, sampledValueObject7);
    }
    if ((Aligned && CPGetConfigurationResponse.MeterValuesAlignedData.Temperature) ||
        ((Aligned == false) && CPGetConfigurationResponse.MeterValuesSampledData.Temperature))
    {
        cJSON *sampledValueObject8 = cJSON_CreateObject();
        cJSON_AddStringToObject(sampledValueObject8, "value", CPMeterValuesRequest[connId].meterValue.sampledValue[8].value);
        cJSON_AddStringToObject(sampledValueObject8, "context", ContextValuestr);
        // cJSON_AddStringToObject(sampledValueObject8, "format", CPMeterValuesRequest[connId].meterValue.sampledValue[8].format);
        cJSON_AddStringToObject(sampledValueObject8, "measurand", CPMeterValuesRequest[connId].meterValue.sampledValue[8].measurand);
        // cJSON_AddStringToObject(sampledValueObject8, "unit", CPMeterValuesRequest[connId].meterValue.sampledValue[8].unit);
        cJSON_AddItemToArray(sampledValueArray, sampledValueObject8);
    }

    cJSON_AddItemToObject(meterValueObject, "sampledValue", sampledValueArray);
    cJSON_AddItemToArray(meterValueArray, meterValueObject);
    cJSON_AddItemToObject(meterValuesRequest, "meterValue", meterValueArray);

    cJSON_AddItemToArray(jsonArray, meterValuesRequest);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPMeterValuesRequest[connId].Sent = true;
        CMSMeterValuesResponse[connId].Received = false;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

/*****************************************REQUEST END**********************************************************/
/*****************************************RESPONSES START******************************************************************/

void sendCancelReservationResponse(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSCancelReservationRequest.UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "status", CPCancelReservationResponse.status);

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPCancelReservationResponse.Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendClearChargingProfileResponse(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSClearChargingProfileRequest.UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    if (config.smartCharging)
        cJSON_AddStringToObject(object, "status", "Accepted");
    else
        cJSON_AddStringToObject(object, "status", "NotSupported");

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendDataTransferResponse(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSDataTransferRequest.UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "status", "Rejected");

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendGetCompositeScheduleResponse(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSGetCompositeScheduleRequest.UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "status", "Rejected");

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendGetDiagnosticsResponse(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSGetDiagnosticsRequest.UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();
    cJSON_AddStringToObject(object, "fileName", CPGetDiagnosticsResponse.fileName);

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
    CPGetDiagnosticsResponse.Sent = true;
}

void sendSetChargingProfileResponse(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSSetChargingProfileRequest.UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    if (config.smartCharging)
        cJSON_AddStringToObject(object, "status", "Accepted");
    else
        cJSON_AddStringToObject(object, "status", "NotSupported");

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendUnlockConnectorResponse(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSUnlockConnectorRequest.UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "status", "NotSupported");

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendChangeAvailabilityResponse(uint8_t connId)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSChangeAvailabilityRequest[connId].UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "status", CPChangeAvailabilityResponse.status);

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPChangeAvailabilityResponse.Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendChangeConfigurationResponse(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSChangeConfigurationRequest.UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "status", CPChangeConfigurationResponse.status);

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPChangeConfigurationResponse.Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendClearCacheResponse(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSClearCacheRequest.UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "status", CPClearCacheResponse.status);

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPGetLocalListVersionResponse.Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendUpdateFirmwareResponse(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSUpdateFirmwareRequest.UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPUpdateFirmwareResponse.Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendGetLocalListVersionResponse(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSGetLocalListVersionRequest.UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddNumberToObject(object, "listVersion", CMSSendLocalListRequest.listVersion);

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPGetLocalListVersionResponse.Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendRemoteStartTransactionResponse(uint8_t connId)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSRemoteStartTransactionRequest[connId].UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "status", CPRemoteStartTransactionResponse[connId].status);

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPRemoteStartTransactionResponse[connId].Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendRemoteStopTransactionResponse(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSRemoteStopTransactionRequest.UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "status", "Accepted");

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPRemoteStopTransactionResponse.Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendReserveNowResponse(uint8_t connId)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSReserveNowRequest[connId].UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "status", CPReserveNowResponse.status);

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPReserveNowResponse.Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendResetResponse(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSResetRequest.UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "status", CPResetResponse.status);

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPResetResponse.Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void SendLocalListResponse(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSSendLocalListRequest.UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "status", "Accepted");

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPSendLocalListResponse.Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void SendTriggerMessageResponse(void)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSTriggerMessageRequest.UniqId));

    // Create an object and add it to the cJSON array
    cJSON *object = cJSON_CreateObject();

    cJSON_AddStringToObject(object, "status", CPTriggerMessageResponse.status);

    cJSON_AddItemToArray(jsonArray, object);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        CPTriggerMessageResponse.Sent = true;
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendGetConfigurationResponseKeys(uint8_t count)
{
    // Create a cJSON array
    cJSON *jsonArray = cJSON_CreateArray();

    // Add elements to the cJSON array
    cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(OCPP_RECEIVE));
    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(CMSGetConfigurationRequest.UniqId));

    cJSON *configurationJson = cJSON_CreateObject();
    cJSON *configurationKeyJsonArray = cJSON_CreateArray();
    if (count == 1)
    {
        //    Add AuthorizeRemoteTxRequests to configurationKeyJsonArray
        cJSON *AuthorizeRemoteTxRequestsJson = cJSON_CreateObject();
        cJSON_AddStringToObject(AuthorizeRemoteTxRequestsJson, "key", "AuthorizeRemoteTxRequests");
        cJSON_AddBoolToObject(AuthorizeRemoteTxRequestsJson, "readonly", CPGetConfigurationResponse.AuthorizeRemoteTxRequestsReadOnly ? true : false);
        cJSON_AddStringToObject(AuthorizeRemoteTxRequestsJson, "value", CPGetConfigurationResponse.AuthorizeRemoteTxRequestsValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, AuthorizeRemoteTxRequestsJson);

        //    Add ClockAlignedDataInterval to configurationKeyJsonArray
        cJSON *ClockAlignedDataIntervalJson = cJSON_CreateObject();
        cJSON_AddStringToObject(ClockAlignedDataIntervalJson, "key", "ClockAlignedDataInterval");
        cJSON_AddBoolToObject(ClockAlignedDataIntervalJson, "readonly", CPGetConfigurationResponse.ClockAlignedDataIntervalReadOnly ? true : false);
        cJSON_AddStringToObject(ClockAlignedDataIntervalJson, "value", CPGetConfigurationResponse.ClockAlignedDataIntervalValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, ClockAlignedDataIntervalJson);

        //    Add ConnectionTimeOut to configurationKeyJsonArray
        cJSON *ConnectionTimeOutJson = cJSON_CreateObject();
        cJSON_AddStringToObject(ConnectionTimeOutJson, "key", "ConnectionTimeOut");
        cJSON_AddBoolToObject(ConnectionTimeOutJson, "readonly", CPGetConfigurationResponse.ConnectionTimeOutReadOnly ? true : false);
        cJSON_AddStringToObject(ConnectionTimeOutJson, "value", CPGetConfigurationResponse.ConnectionTimeOutValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, ConnectionTimeOutJson);

        //    Add GetConfigurationMaxKeys to configurationKeyJsonArray
        cJSON *GetConfigurationMaxKeysJson = cJSON_CreateObject();
        cJSON_AddStringToObject(GetConfigurationMaxKeysJson, "key", "GetConfigurationMaxKeys");
        cJSON_AddBoolToObject(GetConfigurationMaxKeysJson, "readonly", CPGetConfigurationResponse.GetConfigurationMaxKeysReadOnly ? true : false);
        cJSON_AddStringToObject(GetConfigurationMaxKeysJson, "value", CPGetConfigurationResponse.GetConfigurationMaxKeysValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, GetConfigurationMaxKeysJson);

        //    Add HeartbeatInterval to configurationKeyJsonArray
        cJSON *HeartbeatIntervalJson = cJSON_CreateObject();
        cJSON_AddStringToObject(HeartbeatIntervalJson, "key", "HeartbeatInterval");
        cJSON_AddBoolToObject(HeartbeatIntervalJson, "readonly", CPGetConfigurationResponse.HeartbeatIntervalReadOnly ? true : false);
        cJSON_AddStringToObject(HeartbeatIntervalJson, "value", CPGetConfigurationResponse.HeartbeatIntervalValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, HeartbeatIntervalJson);

        //    Add LocalAuthorizeOffline to configurationKeyJsonArray
        cJSON *LocalAuthorizeOfflineJson = cJSON_CreateObject();
        cJSON_AddStringToObject(LocalAuthorizeOfflineJson, "key", "LocalAuthorizeOffline");
        cJSON_AddBoolToObject(LocalAuthorizeOfflineJson, "readonly", CPGetConfigurationResponse.LocalAuthorizeOfflineReadOnly ? true : false);
        cJSON_AddStringToObject(LocalAuthorizeOfflineJson, "value", CPGetConfigurationResponse.LocalAuthorizeOfflineValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, LocalAuthorizeOfflineJson);

        //    Add LocalPreAuthorize to configurationKeyJsonArray
        cJSON *LocalPreAuthorizeJson = cJSON_CreateObject();
        cJSON_AddStringToObject(LocalPreAuthorizeJson, "key", "LocalPreAuthorize");
        cJSON_AddBoolToObject(LocalPreAuthorizeJson, "readonly", CPGetConfigurationResponse.LocalPreAuthorizeReadOnly ? true : false);
        cJSON_AddStringToObject(LocalPreAuthorizeJson, "value", CPGetConfigurationResponse.LocalPreAuthorizeValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, LocalPreAuthorizeJson);

        //    Add MeterValuesAlignedData to configurationKeyJsonArray
        cJSON *MeterValuesAlignedDataJson = cJSON_CreateObject();
        cJSON_AddStringToObject(MeterValuesAlignedDataJson, "key", "MeterValuesAlignedData");
        cJSON_AddBoolToObject(MeterValuesAlignedDataJson, "readonly", CPGetConfigurationResponse.MeterValuesAlignedDataReadOnly ? true : false);
        cJSON_AddStringToObject(MeterValuesAlignedDataJson, "value", CPGetConfigurationResponse.MeterValuesAlignedDataValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, MeterValuesAlignedDataJson);

        //    Add MeterValuesSampledData to configurationKeyJsonArray
        cJSON *MeterValuesSampledDataJson = cJSON_CreateObject();
        cJSON_AddStringToObject(MeterValuesSampledDataJson, "key", "MeterValuesSampledData");
        cJSON_AddBoolToObject(MeterValuesSampledDataJson, "readonly", CPGetConfigurationResponse.MeterValuesSampledDataReadOnly ? true : false);
        cJSON_AddStringToObject(MeterValuesSampledDataJson, "value", CPGetConfigurationResponse.MeterValuesSampledDataValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, MeterValuesSampledDataJson);

        //    Add MeterValueSampleInterval to configurationKeyJsonArray
        cJSON *MeterValueSampleIntervalJson = cJSON_CreateObject();
        cJSON_AddStringToObject(MeterValueSampleIntervalJson, "key", "MeterValueSampleInterval");
        cJSON_AddBoolToObject(MeterValueSampleIntervalJson, "readonly", CPGetConfigurationResponse.MeterValueSampleIntervalReadOnly ? true : false);
        cJSON_AddStringToObject(MeterValueSampleIntervalJson, "value", CPGetConfigurationResponse.MeterValueSampleIntervalValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, MeterValueSampleIntervalJson);

        //    Add NumberOfConnectors to configurationKeyJsonArray
        cJSON *NumberOfConnectorsJson = cJSON_CreateObject();
        cJSON_AddStringToObject(NumberOfConnectorsJson, "key", "NumberOfConnectors");
        cJSON_AddBoolToObject(NumberOfConnectorsJson, "readonly", CPGetConfigurationResponse.NumberOfConnectorsReadOnly ? true : false);
        cJSON_AddStringToObject(NumberOfConnectorsJson, "value", CPGetConfigurationResponse.NumberOfConnectorsValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, NumberOfConnectorsJson);

        //    Add ResetRetries to configurationKeyJsonArray
        cJSON *ResetRetriesJson = cJSON_CreateObject();
        cJSON_AddStringToObject(ResetRetriesJson, "key", "ResetRetries");
        cJSON_AddBoolToObject(ResetRetriesJson, "readonly", CPGetConfigurationResponse.ResetRetriesReadOnly ? true : false);
        cJSON_AddStringToObject(ResetRetriesJson, "value", CPGetConfigurationResponse.ResetRetriesValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, ResetRetriesJson);

        //    Add ConnectorPhaseRotation to configurationKeyJsonArray
        cJSON *ConnectorPhaseRotationJson = cJSON_CreateObject();
        cJSON_AddStringToObject(ConnectorPhaseRotationJson, "key", "ConnectorPhaseRotation");
        cJSON_AddBoolToObject(ConnectorPhaseRotationJson, "readonly", CPGetConfigurationResponse.ConnectorPhaseRotationReadOnly ? true : false);
        cJSON_AddStringToObject(ConnectorPhaseRotationJson, "value", CPGetConfigurationResponse.ConnectorPhaseRotationValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, ConnectorPhaseRotationJson);

        //    Add StopTransactionOnEVSideDisconnect to configurationKeyJsonArray
        cJSON *StopTransactionOnEVSideDisconnectJson = cJSON_CreateObject();
        cJSON_AddStringToObject(StopTransactionOnEVSideDisconnectJson, "key", "StopTransactionOnEVSideDisconnect");
        cJSON_AddBoolToObject(StopTransactionOnEVSideDisconnectJson, "readonly", CPGetConfigurationResponse.StopTransactionOnEVSideDisconnectReadOnly ? true : false);
        cJSON_AddStringToObject(StopTransactionOnEVSideDisconnectJson, "value", CPGetConfigurationResponse.StopTransactionOnEVSideDisconnectValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, StopTransactionOnEVSideDisconnectJson);

        //    Add StopTransactionOnInvalidId to configurationKeyJsonArray
        cJSON *StopTransactionOnInvalidIdJson = cJSON_CreateObject();
        cJSON_AddStringToObject(StopTransactionOnInvalidIdJson, "key", "StopTransactionOnInvalidId");
        cJSON_AddBoolToObject(StopTransactionOnInvalidIdJson, "readonly", CPGetConfigurationResponse.StopTransactionOnInvalidIdReadOnly ? true : false);
        cJSON_AddStringToObject(StopTransactionOnInvalidIdJson, "value", CPGetConfigurationResponse.StopTransactionOnInvalidIdValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, StopTransactionOnInvalidIdJson);
    }
    else if (count == 2)
    {
        //    Add StopTxnAlignedData to configurationKeyJsonArray
        cJSON *StopTxnAlignedDataJson = cJSON_CreateObject();
        cJSON_AddStringToObject(StopTxnAlignedDataJson, "key", "StopTxnAlignedData");
        cJSON_AddBoolToObject(StopTxnAlignedDataJson, "readonly", CPGetConfigurationResponse.StopTxnAlignedDataReadOnly ? true : false);
        cJSON_AddStringToObject(StopTxnAlignedDataJson, "value", CPGetConfigurationResponse.StopTxnAlignedDataValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, StopTxnAlignedDataJson);

        //    Add StopTxnSampledData to configurationKeyJsonArray
        cJSON *StopTxnSampledDataJson = cJSON_CreateObject();
        cJSON_AddStringToObject(StopTxnSampledDataJson, "key", "StopTxnSampledData");
        cJSON_AddBoolToObject(StopTxnSampledDataJson, "readonly", CPGetConfigurationResponse.StopTxnSampledDataReadOnly ? true : false);
        cJSON_AddStringToObject(StopTxnSampledDataJson, "value", CPGetConfigurationResponse.StopTxnSampledDataValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, StopTxnSampledDataJson);

        //    Add SupportedFeatureProfiles to configurationKeyJsonArray
        cJSON *SupportedFeatureProfilesJson = cJSON_CreateObject();
        cJSON_AddStringToObject(SupportedFeatureProfilesJson, "key", "SupportedFeatureProfiles");
        cJSON_AddBoolToObject(SupportedFeatureProfilesJson, "readonly", CPGetConfigurationResponse.SupportedFeatureProfilesReadOnly ? true : false);
        cJSON_AddStringToObject(SupportedFeatureProfilesJson, "value", CPGetConfigurationResponse.SupportedFeatureProfilesValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, SupportedFeatureProfilesJson);

        //    Add TransactionMessageAttempts to configurationKeyJsonArray
        cJSON *TransactionMessageAttemptsJson = cJSON_CreateObject();
        cJSON_AddStringToObject(TransactionMessageAttemptsJson, "key", "TransactionMessageAttempts");
        cJSON_AddBoolToObject(TransactionMessageAttemptsJson, "readonly", CPGetConfigurationResponse.TransactionMessageAttemptsReadOnly ? true : false);
        cJSON_AddStringToObject(TransactionMessageAttemptsJson, "value", CPGetConfigurationResponse.TransactionMessageAttemptsValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, TransactionMessageAttemptsJson);

        //    Add TransactionMessageRetryInterval to configurationKeyJsonArray
        cJSON *TransactionMessageRetryIntervalJson = cJSON_CreateObject();
        cJSON_AddStringToObject(TransactionMessageRetryIntervalJson, "key", "TransactionMessageRetryInterval");
        cJSON_AddBoolToObject(TransactionMessageRetryIntervalJson, "readonly", CPGetConfigurationResponse.TransactionMessageRetryIntervalReadOnly ? true : false);
        cJSON_AddStringToObject(TransactionMessageRetryIntervalJson, "value", CPGetConfigurationResponse.TransactionMessageRetryIntervalValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, TransactionMessageRetryIntervalJson);

        //    Add UnlockConnectorOnEVSideDisconnect to configurationKeyJsonArray
        cJSON *UnlockConnectorOnEVSideDisconnectJson = cJSON_CreateObject();
        cJSON_AddStringToObject(UnlockConnectorOnEVSideDisconnectJson, "key", "UnlockConnectorOnEVSideDisconnect");
        cJSON_AddBoolToObject(UnlockConnectorOnEVSideDisconnectJson, "readonly", CPGetConfigurationResponse.UnlockConnectorOnEVSideDisconnectReadOnly ? true : false);
        cJSON_AddStringToObject(UnlockConnectorOnEVSideDisconnectJson, "value", CPGetConfigurationResponse.UnlockConnectorOnEVSideDisconnectValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, UnlockConnectorOnEVSideDisconnectJson);

        //    Add AllowOfflineTxForUnknownId to configurationKeyJsonArray
        cJSON *AllowOfflineTxForUnknownIdJson = cJSON_CreateObject();
        cJSON_AddStringToObject(AllowOfflineTxForUnknownIdJson, "key", "AllowOfflineTxForUnknownId");
        cJSON_AddBoolToObject(AllowOfflineTxForUnknownIdJson, "readonly", CPGetConfigurationResponse.AllowOfflineTxForUnknownIdReadOnly ? true : false);
        cJSON_AddStringToObject(AllowOfflineTxForUnknownIdJson, "value", CPGetConfigurationResponse.AllowOfflineTxForUnknownIdValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, AllowOfflineTxForUnknownIdJson);

        //    Add AuthorizationCacheEnabled to configurationKeyJsonArray
        cJSON *AuthorizationCacheEnabledJson = cJSON_CreateObject();
        cJSON_AddStringToObject(AuthorizationCacheEnabledJson, "key", "AuthorizationCacheEnabled");
        cJSON_AddBoolToObject(AuthorizationCacheEnabledJson, "readonly", CPGetConfigurationResponse.AuthorizationCacheEnabledReadOnly ? true : false);
        cJSON_AddStringToObject(AuthorizationCacheEnabledJson, "value", CPGetConfigurationResponse.AuthorizationCacheEnabledValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, AuthorizationCacheEnabledJson);

        //    Add BlinkRepeat to configurationKeyJsonArray
        cJSON *BlinkRepeatJson = cJSON_CreateObject();
        cJSON_AddStringToObject(BlinkRepeatJson, "key", "BlinkRepeat");
        cJSON_AddBoolToObject(BlinkRepeatJson, "readonly", CPGetConfigurationResponse.BlinkRepeatReadOnly ? true : false);
        cJSON_AddStringToObject(BlinkRepeatJson, "value", CPGetConfigurationResponse.BlinkRepeatValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, BlinkRepeatJson);

        //    Add LightIntensity to configurationKeyJsonArray
        cJSON *LightIntensityJson = cJSON_CreateObject();
        cJSON_AddStringToObject(LightIntensityJson, "key", "LightIntensity");
        cJSON_AddBoolToObject(LightIntensityJson, "readonly", CPGetConfigurationResponse.LightIntensityReadOnly ? true : false);
        cJSON_AddStringToObject(LightIntensityJson, "value", CPGetConfigurationResponse.LightIntensityValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, LightIntensityJson);

        //    Add MaxEnergyOnInvalidId to configurationKeyJsonArray
        cJSON *MaxEnergyOnInvalidIdJson = cJSON_CreateObject();
        cJSON_AddStringToObject(MaxEnergyOnInvalidIdJson, "key", "MaxEnergyOnInvalidId");
        cJSON_AddBoolToObject(MaxEnergyOnInvalidIdJson, "readonly", CPGetConfigurationResponse.MaxEnergyOnInvalidIdReadOnly ? true : false);
        cJSON_AddStringToObject(MaxEnergyOnInvalidIdJson, "value", CPGetConfigurationResponse.MaxEnergyOnInvalidIdValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, MaxEnergyOnInvalidIdJson);

        //    Add MeterValuesAlignedDataMaxLength to configurationKeyJsonArray
        cJSON *MeterValuesAlignedDataMaxLengthJson = cJSON_CreateObject();
        cJSON_AddStringToObject(MeterValuesAlignedDataMaxLengthJson, "key", "MeterValuesAlignedDataMaxLength");
        cJSON_AddBoolToObject(MeterValuesAlignedDataMaxLengthJson, "readonly", CPGetConfigurationResponse.MeterValuesAlignedDataMaxLengthReadOnly ? true : false);
        cJSON_AddStringToObject(MeterValuesAlignedDataMaxLengthJson, "value", CPGetConfigurationResponse.MeterValuesAlignedDataMaxLengthValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, MeterValuesAlignedDataMaxLengthJson);

        //    Add MeterValuesSampledDataMaxLength to configurationKeyJsonArray
        cJSON *MeterValuesSampledDataMaxLengthJson = cJSON_CreateObject();
        cJSON_AddStringToObject(MeterValuesSampledDataMaxLengthJson, "key", "MeterValuesSampledDataMaxLength");
        cJSON_AddBoolToObject(MeterValuesSampledDataMaxLengthJson, "readonly", CPGetConfigurationResponse.MeterValuesSampledDataMaxLengthReadOnly ? true : false);
        cJSON_AddStringToObject(MeterValuesSampledDataMaxLengthJson, "value", CPGetConfigurationResponse.MeterValuesSampledDataMaxLengthValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, MeterValuesSampledDataMaxLengthJson);

        //    Add MinimumStatusDuration to configurationKeyJsonArray
        cJSON *MinimumStatusDurationJson = cJSON_CreateObject();
        cJSON_AddStringToObject(MinimumStatusDurationJson, "key", "MinimumStatusDuration");
        cJSON_AddBoolToObject(MinimumStatusDurationJson, "readonly", CPGetConfigurationResponse.MinimumStatusDurationReadOnly ? true : false);
        cJSON_AddStringToObject(MinimumStatusDurationJson, "value", CPGetConfigurationResponse.MinimumStatusDurationValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, MinimumStatusDurationJson);

        //    Add ConnectorPhaseRotationMaxLength to configurationKeyJsonArray
        cJSON *ConnectorPhaseRotationMaxLengthJson = cJSON_CreateObject();
        cJSON_AddStringToObject(ConnectorPhaseRotationMaxLengthJson, "key", "ConnectorPhaseRotationMaxLength");
        cJSON_AddBoolToObject(ConnectorPhaseRotationMaxLengthJson, "readonly", CPGetConfigurationResponse.ConnectorPhaseRotationMaxLengthReadOnly ? true : false);
        cJSON_AddStringToObject(ConnectorPhaseRotationMaxLengthJson, "value", CPGetConfigurationResponse.ConnectorPhaseRotationMaxLengthValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, ConnectorPhaseRotationMaxLengthJson);
    }
    else if (count == 3)
    {
        //    Add StopTxnAlignedDataMaxLength to configurationKeyJsonArray
        cJSON *StopTxnAlignedDataMaxLengthJson = cJSON_CreateObject();
        cJSON_AddStringToObject(StopTxnAlignedDataMaxLengthJson, "key", "StopTxnAlignedDataMaxLength");
        cJSON_AddBoolToObject(StopTxnAlignedDataMaxLengthJson, "readonly", CPGetConfigurationResponse.StopTxnAlignedDataMaxLengthReadOnly ? true : false);
        cJSON_AddStringToObject(StopTxnAlignedDataMaxLengthJson, "value", CPGetConfigurationResponse.StopTxnAlignedDataMaxLengthValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, StopTxnAlignedDataMaxLengthJson);

        //    Add StopTxnSampledDataMaxLength to configurationKeyJsonArray
        cJSON *StopTxnSampledDataMaxLengthJson = cJSON_CreateObject();
        cJSON_AddStringToObject(StopTxnSampledDataMaxLengthJson, "key", "StopTxnSampledDataMaxLength");
        cJSON_AddBoolToObject(StopTxnSampledDataMaxLengthJson, "readonly", CPGetConfigurationResponse.StopTxnSampledDataMaxLengthReadOnly ? true : false);
        cJSON_AddStringToObject(StopTxnSampledDataMaxLengthJson, "value", CPGetConfigurationResponse.StopTxnSampledDataMaxLengthValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, StopTxnSampledDataMaxLengthJson);

        //    Add SupportedFeatureProfilesMaxLength to configurationKeyJsonArray
        cJSON *SupportedFeatureProfilesMaxLengthJson = cJSON_CreateObject();
        cJSON_AddStringToObject(SupportedFeatureProfilesMaxLengthJson, "key", "SupportedFeatureProfilesMaxLength");
        cJSON_AddBoolToObject(SupportedFeatureProfilesMaxLengthJson, "readonly", CPGetConfigurationResponse.SupportedFeatureProfilesMaxLengthReadOnly ? true : false);
        cJSON_AddStringToObject(SupportedFeatureProfilesMaxLengthJson, "value", CPGetConfigurationResponse.SupportedFeatureProfilesMaxLengthValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, SupportedFeatureProfilesMaxLengthJson);

        //    Add WebSocketPingInterval to configurationKeyJsonArray
        cJSON *WebSocketPingIntervalJson = cJSON_CreateObject();
        cJSON_AddStringToObject(WebSocketPingIntervalJson, "key", "WebSocketPingInterval");
        cJSON_AddBoolToObject(WebSocketPingIntervalJson, "readonly", CPGetConfigurationResponse.WebSocketPingIntervalReadOnly ? true : false);
        cJSON_AddStringToObject(WebSocketPingIntervalJson, "value", CPGetConfigurationResponse.WebSocketPingIntervalValue);
        cJSON_AddItemToArray(configurationKeyJsonArray, WebSocketPingIntervalJson);
        if (CPGetConfigurationResponse.SupportedFeatureProfiles.LocalAuthListManagement)
        {
            //    Add LocalAuthListEnabled to configurationKeyJsonArray
            cJSON *LocalAuthListEnabledJson = cJSON_CreateObject();
            cJSON_AddStringToObject(LocalAuthListEnabledJson, "key", "LocalAuthListEnabled");
            cJSON_AddBoolToObject(LocalAuthListEnabledJson, "readonly", CPGetConfigurationResponse.LocalAuthListEnabledReadOnly ? true : false);
            cJSON_AddStringToObject(LocalAuthListEnabledJson, "value", CPGetConfigurationResponse.LocalAuthListEnabledValue);
            cJSON_AddItemToArray(configurationKeyJsonArray, LocalAuthListEnabledJson);

            //    Add LocalAuthListMaxLength to configurationKeyJsonArray
            cJSON *LocalAuthListMaxLengthJson = cJSON_CreateObject();
            cJSON_AddStringToObject(LocalAuthListMaxLengthJson, "key", "LocalAuthListMaxLength");
            cJSON_AddBoolToObject(LocalAuthListMaxLengthJson, "readonly", CPGetConfigurationResponse.LocalAuthListMaxLengthReadOnly ? true : false);
            cJSON_AddStringToObject(LocalAuthListMaxLengthJson, "value", CPGetConfigurationResponse.LocalAuthListMaxLengthValue);
            cJSON_AddItemToArray(configurationKeyJsonArray, LocalAuthListMaxLengthJson);

            //    Add SendLocalListMaxLength to configurationKeyJsonArray
            cJSON *SendLocalListMaxLengthJson = cJSON_CreateObject();
            cJSON_AddStringToObject(SendLocalListMaxLengthJson, "key", "SendLocalListMaxLength");
            cJSON_AddBoolToObject(SendLocalListMaxLengthJson, "readonly", CPGetConfigurationResponse.SendLocalListMaxLengthReadOnly ? true : false);
            cJSON_AddStringToObject(SendLocalListMaxLengthJson, "value", CPGetConfigurationResponse.SendLocalListMaxLengthValue);
            cJSON_AddItemToArray(configurationKeyJsonArray, SendLocalListMaxLengthJson);
        }
        if (CPGetConfigurationResponse.SupportedFeatureProfiles.Reservation)
        {
            //    Add ReserveConnectorZeroSupported to configurationKeyJsonArray
            cJSON *ReserveConnectorZeroSupportedJson = cJSON_CreateObject();
            cJSON_AddStringToObject(ReserveConnectorZeroSupportedJson, "key", "ReserveConnectorZeroSupported");
            cJSON_AddBoolToObject(ReserveConnectorZeroSupportedJson, "readonly", CPGetConfigurationResponse.ReserveConnectorZeroSupportedReadOnly ? true : false);
            cJSON_AddStringToObject(ReserveConnectorZeroSupportedJson, "value", CPGetConfigurationResponse.ReserveConnectorZeroSupportedValue);
            cJSON_AddItemToArray(configurationKeyJsonArray, ReserveConnectorZeroSupportedJson);
        }
        if (CPGetConfigurationResponse.SupportedFeatureProfiles.SmartCharging)
        {
            //    Add ChargeProfileMaxStackLevel to configurationKeyJsonArray
            cJSON *ChargeProfileMaxStackLevelJson = cJSON_CreateObject();
            cJSON_AddStringToObject(ChargeProfileMaxStackLevelJson, "key", "ChargeProfileMaxStackLevel");
            cJSON_AddBoolToObject(ChargeProfileMaxStackLevelJson, "readonly", CPGetConfigurationResponse.ChargeProfileMaxStackLevelReadOnly ? true : false);
            cJSON_AddStringToObject(ChargeProfileMaxStackLevelJson, "value", CPGetConfigurationResponse.ChargeProfileMaxStackLevelValue);
            cJSON_AddItemToArray(configurationKeyJsonArray, ChargeProfileMaxStackLevelJson);

            //    Add ChargingScheduleAllowedChargingRateUnit to configurationKeyJsonArray
            cJSON *ChargingScheduleAllowedChargingRateUnitJson = cJSON_CreateObject();
            cJSON_AddStringToObject(ChargingScheduleAllowedChargingRateUnitJson, "key", "ChargingScheduleAllowedChargingRateUnit");
            cJSON_AddBoolToObject(ChargingScheduleAllowedChargingRateUnitJson, "readonly", CPGetConfigurationResponse.ChargingScheduleAllowedChargingRateUnitReadOnly ? true : false);
            cJSON_AddStringToObject(ChargingScheduleAllowedChargingRateUnitJson, "value", CPGetConfigurationResponse.ChargingScheduleAllowedChargingRateUnitValue);
            cJSON_AddItemToArray(configurationKeyJsonArray, ChargingScheduleAllowedChargingRateUnitJson);

            //    Add ChargingScheduleMaxPeriods to configurationKeyJsonArray
            cJSON *ChargingScheduleMaxPeriodsJson = cJSON_CreateObject();
            cJSON_AddStringToObject(ChargingScheduleMaxPeriodsJson, "key", "ChargingScheduleMaxPeriods");
            cJSON_AddBoolToObject(ChargingScheduleMaxPeriodsJson, "readonly", CPGetConfigurationResponse.ChargingScheduleMaxPeriodsReadOnly ? true : false);
            cJSON_AddStringToObject(ChargingScheduleMaxPeriodsJson, "value", CPGetConfigurationResponse.ChargingScheduleMaxPeriodsValue);
            cJSON_AddItemToArray(configurationKeyJsonArray, ChargingScheduleMaxPeriodsJson);

            //    Add ConnectorSwitch3to1PhaseSupported to configurationKeyJsonArray
            cJSON *ConnectorSwitch3to1PhaseSupportedJson = cJSON_CreateObject();
            cJSON_AddStringToObject(ConnectorSwitch3to1PhaseSupportedJson, "key", "ConnectorSwitch3to1PhaseSupported");
            cJSON_AddBoolToObject(ConnectorSwitch3to1PhaseSupportedJson, "readonly", CPGetConfigurationResponse.ConnectorSwitch3to1PhaseSupportedReadOnly ? true : false);
            cJSON_AddStringToObject(ConnectorSwitch3to1PhaseSupportedJson, "value", CPGetConfigurationResponse.ConnectorSwitch3to1PhaseSupportedValue);
            cJSON_AddItemToArray(configurationKeyJsonArray, ConnectorSwitch3to1PhaseSupportedJson);

            //    Add MaxChargingProfilesInstalled to configurationKeyJsonArray
            cJSON *MaxChargingProfilesInstalledJson = cJSON_CreateObject();
            cJSON_AddStringToObject(MaxChargingProfilesInstalledJson, "key", "MaxChargingProfilesInstalled");
            cJSON_AddBoolToObject(MaxChargingProfilesInstalledJson, "readonly", CPGetConfigurationResponse.MaxChargingProfilesInstalledReadOnly ? true : false);
            cJSON_AddStringToObject(MaxChargingProfilesInstalledJson, "value", CPGetConfigurationResponse.MaxChargingProfilesInstalledValue);
            cJSON_AddItemToArray(configurationKeyJsonArray, MaxChargingProfilesInstalledJson);
        }
    }

    cJSON_AddItemToObject(configurationJson, "configurationKey", configurationKeyJsonArray);

    cJSON *unknownKeyJsonArray = cJSON_CreateArray();
    cJSON_AddItemToObject(configurationJson, "unknownKey", unknownKeyJsonArray);

    cJSON_AddItemToArray(jsonArray, configurationJson);

    // Convert cJSON array to a JSON-formatted string
    char *json_string = cJSON_Print(jsonArray);

    if (json_string != NULL)
    {
        char json_output[strlen(json_string) + 1];
        memset(json_output, '\0', strlen(json_string) + 1);
        memcpy(json_output, json_string, strlen(json_string));
        // ESP_LOGI(TAG_BOOT, "Serialized JSON: %s", json_string);
        xQueueSend(OcppMsgQueue, &json_output, 1000 / portTICK_PERIOD_MS);
        free(json_string); // Free the allocated memory for the JSON string
    }
    else
    {
        ESP_LOGE(TAG_BOOT, "Failed to serialize JSON");
    }
    // Delete the cJSON array
    cJSON_Delete(jsonArray);
}

void sendGetConfigurationResponse(void)
{
    sendGetConfigurationResponseKeys(1);
    sendGetConfigurationResponseKeys(2);
    sendGetConfigurationResponseKeys(3);
}

/********************************************RESPONSES END***************************************************************/
/******************************************RESPONSES PROCESS START********************************************/

void BootNotificationResponseProcess(cJSON *bootResponse)
{
    bool isBootStatusReceived = false;
    bool isBootcurrentTimeReceived = false;
    bool isBootintervalReceived = false;
    // bootNotificationResponseReceived = true;
    cJSON *status = cJSON_GetObjectItem(bootResponse, "status");
    if (cJSON_IsString(status))
    {
        isBootStatusReceived = true;
        setNULL(CMSBootNotificationResponse.status);
        memcpy(CMSBootNotificationResponse.status, status->valuestring, strlen(status->valuestring));
        ESP_LOGE(TAG_BOOT, "Status : %s", CMSBootNotificationResponse.status);
    }
    cJSON *currentTime = cJSON_GetObjectItem(bootResponse, "currentTime");
    if (cJSON_IsString(currentTime))
    {
        isBootcurrentTimeReceived = true;
        setNULL(CMSBootNotificationResponse.currentTime);
        memcpy(CMSBootNotificationResponse.currentTime, currentTime->valuestring, strlen(currentTime->valuestring));
        ESP_LOGE(TAG_BOOT, "currentTime : %s", CMSBootNotificationResponse.currentTime);
    }
    cJSON *interval = cJSON_GetObjectItem(bootResponse, "interval");
    if (cJSON_IsNumber(interval))
    {
        isBootintervalReceived = true;
        CMSBootNotificationResponse.interval = interval->valueint;
        ESP_LOGE(TAG_BOOT, "interval : %lu", CMSBootNotificationResponse.interval);
    }

    CMSBootNotificationResponse.Received = isBootStatusReceived & isBootcurrentTimeReceived & isBootintervalReceived;
    if (CMSBootNotificationResponse.Received)
    {
        CPGetConfigurationResponse.HeartbeatInterval = CMSBootNotificationResponse.interval;
        setNULL(CPGetConfigurationResponse.HeartbeatIntervalValue);
        sprintf(CPGetConfigurationResponse.HeartbeatIntervalValue, "%ld", CPGetConfigurationResponse.HeartbeatInterval);
    }
}

void AuthorizationResponseProcess(cJSON *authorizationResponse, uint8_t connId)
{
    bool isidTagInfoReceived = false;
    bool isidTagInfoStatusReceived = false;

    cJSON *idTagInfo = cJSON_GetObjectItem(authorizationResponse, "idTagInfo");
    if (cJSON_IsObject(idTagInfo))
    {
        isidTagInfoReceived = true;
    }
    cJSON *status = cJSON_GetObjectItem(idTagInfo, "status");
    if (cJSON_IsString(status))
    {
        isidTagInfoStatusReceived = true;
        setNULL(CMSAuthorizeResponse[connId].idtaginfo.status);
        memcpy(CMSAuthorizeResponse[connId].idtaginfo.status, status->valuestring, strlen(status->valuestring));
        ESP_LOGI(TAG_AUTH, "status : %s", CMSAuthorizeResponse[connId].idtaginfo.status);
    }
    cJSON *expiryDate = cJSON_GetObjectItem(idTagInfo, "expiryDate");
    if (cJSON_IsString(expiryDate))
    {
        CMSAuthorizeResponse[connId].idtaginfo.expiryDateReceived = true;
        setNULL(CMSAuthorizeResponse[connId].idtaginfo.expiryDate);
        memcpy(CMSAuthorizeResponse[connId].idtaginfo.expiryDate, expiryDate->valuestring, strlen(expiryDate->valuestring));
        ESP_LOGI(TAG_AUTH, "expiryDate : %s", CMSAuthorizeResponse[connId].idtaginfo.expiryDate);
    }
    else
    {
        CMSAuthorizeResponse[connId].idtaginfo.expiryDateReceived = false;
    }
    cJSON *parentidTag = cJSON_GetObjectItem(idTagInfo, "parentidTag");
    if (cJSON_IsString(parentidTag))
    {
        CMSAuthorizeResponse[connId].idtaginfo.parentidTagReceived = true;
        setNULL(CMSAuthorizeResponse[connId].idtaginfo.parentidTag);
        memcpy(CMSAuthorizeResponse[connId].idtaginfo.parentidTag, parentidTag->valuestring, strlen(parentidTag->valuestring));
        ESP_LOGI(TAG_AUTH, "parentidTag : %s", CMSAuthorizeResponse[connId].idtaginfo.parentidTag);
    }
    else
    {
        CMSAuthorizeResponse[connId].idtaginfo.parentidTagReceived = false;
    }

    CMSAuthorizeResponse[connId].Received = isidTagInfoReceived & isidTagInfoStatusReceived;
}

void CancelReservationResponseProcess(cJSON *CancelReservationResponseV)
{
    bool isCancelReservationStatusReceived = false;
    cJSON *status = cJSON_GetObjectItem(CancelReservationResponseV, "status");
    if (cJSON_IsString(status))
    {
        isCancelReservationStatusReceived = true;
        setNULL(CPCancelReservationResponse.status);
        memcpy(CPCancelReservationResponse.status, status->valuestring, strlen(status->valuestring));
        ESP_LOGE(TAG_CANRES, "status : %s", CPCancelReservationResponse.status);
    }

    CPCancelReservationResponse.Sent = isCancelReservationStatusReceived;
}

void ChangeConfigurationResponseProcess(cJSON *ChangeConfigurationResponseV)
{
    bool isChangeConfigurationStatusReceived = false;

    cJSON *status = cJSON_GetObjectItem(ChangeConfigurationResponseV, "status");
    if (cJSON_IsString(status))
    {
        isChangeConfigurationStatusReceived = true;
        setNULL(CPChangeConfigurationResponse.status);
        memcpy(CPChangeConfigurationResponse.status, status->valuestring, strlen(status->valuestring));
        ESP_LOGE(TAG_CHANGECONFREQ, "status : %s", CPChangeConfigurationResponse.status);
    }
    CPChangeConfigurationResponse.Sent = isChangeConfigurationStatusReceived;
}

void ClearCacheResponseProcess(cJSON *ClearCacheResponseV)
{
    bool isClearCacheResponseStatusReceived = false;

    cJSON *status = cJSON_GetObjectItem(ClearCacheResponseV, "status");
    if (cJSON_IsString(status))
    {
        isClearCacheResponseStatusReceived = true;
        setNULL(CPClearCacheResponse.status);
        memcpy(CPClearCacheResponse.status, status->valuestring, strlen(status->valuestring));
        ESP_LOGE(TAG_CLEARCACHE, "status : %s", CPClearCacheResponse.status);
    }
    CPClearCacheResponse.Sent = isClearCacheResponseStatusReceived;
}

void HeartbeatResponseProcess(cJSON *HeartbeatResponseV)
{
    bool isHeartbeatResponseStatusReceived = false;

    cJSON *currentTime = cJSON_GetObjectItem(HeartbeatResponseV, "currentTime");
    if (cJSON_IsString(currentTime))
    {
        isHeartbeatResponseStatusReceived = true;
        setNULL(CMSHeartbeatResponse.currentTime);
        memcpy(CMSHeartbeatResponse.currentTime, currentTime->valuestring, strlen(currentTime->valuestring));
        ESP_LOGI(TAG_HBRES, "currentTime : %s", CMSHeartbeatResponse.currentTime);
    }
    CMSHeartbeatResponse.Received = isHeartbeatResponseStatusReceived;
}

void ResetResponseProcess(cJSON *ResetResponseV)
{
    bool isResetResponseStatusReceived = false;

    cJSON *status = cJSON_GetObjectItem(ResetResponseV, "status");
    if (cJSON_IsString(status))
    {
        isResetResponseStatusReceived = true;
        setNULL(CPResetResponse.status);
        memcpy(CPResetResponse.status, status->valuestring, strlen(status->valuestring));
        ESP_LOGE(TAG_RESETRREQ, "status : %s", CPResetResponse.status);
    }
    CPResetResponse.Sent = isResetResponseStatusReceived;
}

void StartTransactionResponseProcess(cJSON *StartTransactionResponseV, uint8_t connId)
{
    bool isidTagInfoStartTransReceived = false;
    bool isidTagInfoStartTransStatusReceived = false;
    bool isStartTranstransactionIdReceived = false;

    cJSON *idTagInfo = cJSON_GetObjectItem(StartTransactionResponseV, "idTagInfo");
    if (cJSON_IsObject(idTagInfo))
    {
        isidTagInfoStartTransReceived = true;
    }

    cJSON *status = cJSON_GetObjectItem(idTagInfo, "status");
    if (cJSON_IsString(status))
    {
        isidTagInfoStartTransStatusReceived = true;
        setNULL(CMSStartTransactionResponse[connId].idtaginfo.status);
        memcpy(CMSStartTransactionResponse[connId].idtaginfo.status, status->valuestring, strlen(status->valuestring));
        ESP_LOGE(TAG_STRARTTRANSREQ, "status : %s", CMSStartTransactionResponse[connId].idtaginfo.status);
    }
    cJSON *expiryDate = cJSON_GetObjectItem(idTagInfo, "expiryDate");
    if (cJSON_IsString(expiryDate))
    {
        CMSStartTransactionResponse[connId].idtaginfo.expiryDateReceived = true;
        setNULL(CMSStartTransactionResponse[connId].idtaginfo.expiryDate);
        memcpy(CMSStartTransactionResponse[connId].idtaginfo.expiryDate, expiryDate->valuestring, strlen(expiryDate->valuestring));
        ESP_LOGE(TAG_STRARTTRANSREQ, "expiryDate : %s", CMSStartTransactionResponse[connId].idtaginfo.expiryDate);
    }
    else
    {
        CMSStartTransactionResponse[connId].idtaginfo.expiryDateReceived = false;
    }
    cJSON *parentidTag = cJSON_GetObjectItem(idTagInfo, "parentidTag");
    if (cJSON_IsString(parentidTag))
    {
        CMSStartTransactionResponse[connId].idtaginfo.parentidTagReceived = true;
        setNULL(CMSStartTransactionResponse[connId].idtaginfo.parentidTag);
        memcpy(CMSStartTransactionResponse[connId].idtaginfo.parentidTag, parentidTag->valuestring, strlen(parentidTag->valuestring));
        ESP_LOGE(TAG_STRARTTRANSREQ, "parentidTag : %s", CMSStartTransactionResponse[connId].idtaginfo.parentidTag);
    }
    else
    {
        CMSStartTransactionResponse[connId].idtaginfo.parentidTagReceived = false;
    }
    cJSON *transactionId = cJSON_GetObjectItem(StartTransactionResponseV, "transactionId");
    if (cJSON_IsNumber(transactionId))
    {
        isStartTranstransactionIdReceived = true;
        CMSStartTransactionResponse[connId].transactionId = transactionId->valueint;
        ESP_LOGE(TAG_STRARTTRANSREQ, "transactionId : %lu", CMSStartTransactionResponse[connId].transactionId);
    }

    CMSStartTransactionResponse[connId].Received = isidTagInfoStartTransReceived && isidTagInfoStartTransStatusReceived && isStartTranstransactionIdReceived;
}

void StopTransactionResponseProcess(cJSON *StopTransactionResponseV, uint8_t connId)
{
    bool isidTagInfoStopTReceived = false;
    bool isidTagInfoStopTStatusReceived = false;

    cJSON *idTagInfo = cJSON_GetObjectItem(StopTransactionResponseV, "idTagInfo");
    if (cJSON_IsObject(idTagInfo))
    {
        isidTagInfoStopTReceived = true;
    }
    cJSON *status = cJSON_GetObjectItem(idTagInfo, "status");
    if (cJSON_IsString(status))
    {
        isidTagInfoStopTStatusReceived = true;
        setNULL(CMSStopTransactionResponse[connId].idtaginfo.status);
        memcpy(CMSStopTransactionResponse[connId].idtaginfo.status, status->valuestring, strlen(status->valuestring));
        ESP_LOGE(TAG_STOPTRANSREQ, "status : %s", CMSStopTransactionResponse[connId].idtaginfo.status);
    }
    cJSON *expiryDate = cJSON_GetObjectItem(idTagInfo, "expiryDate");
    if (cJSON_IsString(expiryDate))
    {
        CMSStopTransactionResponse[connId].idtaginfo.expiryDateReceived = true;
        setNULL(CMSStopTransactionResponse[connId].idtaginfo.expiryDate);
        memcpy(CMSStopTransactionResponse[connId].idtaginfo.expiryDate, expiryDate->valuestring, strlen(expiryDate->valuestring));
        ESP_LOGE(TAG_STOPTRANSREQ, "expiryDate : %s", CMSStopTransactionResponse[connId].idtaginfo.expiryDate);
    }
    else
    {
        CMSStopTransactionResponse[connId].idtaginfo.expiryDateReceived = false;
    }
    cJSON *parentidTag = cJSON_GetObjectItem(idTagInfo, "parentidTag");
    if (cJSON_IsString(parentidTag))
    {
        CMSStopTransactionResponse[connId].idtaginfo.parentidTagReceived = true;
        setNULL(CMSStopTransactionResponse[connId].idtaginfo.parentidTag);
        memcpy(CMSStopTransactionResponse[connId].idtaginfo.parentidTag, parentidTag->valuestring, strlen(parentidTag->valuestring));
        ESP_LOGE(TAG_STOPTRANSREQ, "parentidTag : %s", CMSStopTransactionResponse[connId].idtaginfo.parentidTag);
    }
    else
    {
        CMSStopTransactionResponse[connId].idtaginfo.parentidTagReceived = false;
    }

    CMSStopTransactionResponse[connId].Received = isidTagInfoStopTReceived & isidTagInfoStopTStatusReceived;
}

/*********************************************RESPONSE PROCESS END ******************************************/