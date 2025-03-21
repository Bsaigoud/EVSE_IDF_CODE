
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "ocpp.h"
#include "ocpp_task.h"
#include "EnergyMeter.h"
#include "ProjectSettings.h"
#include "ControlPilotState.h"
#include "ControlPilotPwm.h"
#include "slave.h"
#include "custom_nvs.h"
#include "faultGpios.h"
#include "OTA.h"
#include "local_http_server.h"
#include "display.h"
#include "websocket_connect.h"
#include "rtc_clock.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "logsToSpiffs.h"

// void SW_Serial_write_wifiChannel(uint8_t channel);

#define TAG "OCPP_TASK"
#define TAG_DEBUG "OCPP_TASK_DEBUG"
#define TAG_STATUS "STATUS_MSG"
#define TAG_LED "LED"

extern CPHeartbeatRequest_t CPHeartbeatRequest;
extern CPBootNotificationRequest_t CPBootNotificationRequest;
extern CPHeartbeatRequest_t CPHeartbeatRequest;
extern CPStatusNotificationRequest_t CPStatusNotificationRequest[NUM_OF_CONNECTORS];
extern CPAuthorizeRequest_t CPAuthorizeRequest[NUM_OF_CONNECTORS];
extern CPStartTransactionRequest_t CPStartTransactionRequest[NUM_OF_CONNECTORS];
extern CPMeterValuesRequest_t CPMeterValuesRequest[NUM_OF_CONNECTORS];
extern CPStopTransactionRequest_t CPStopTransactionRequest[NUM_OF_CONNECTORS];
extern CPFirmwareStatusNotificationRequest_t CPFirmwareStatusNotificationRequest;
extern CPDiagnosticsStatusNotificationRequest_t CPDiagnosticsStatusNotificationRequest;

extern CMSRemoteStartTransactionRequest_t CMSRemoteStartTransactionRequest[NUM_OF_CONNECTORS];
extern CMSRemoteStopTransactionRequest_t CMSRemoteStopTransactionRequest;
extern CMSCancelReservationRequest_t CMSCancelReservationRequest;
extern CMSChangeAvailabilityRequest_t CMSChangeAvailabilityRequest[NUM_OF_CONNECTORS];
extern CMSChangeConfigurationRequest_t CMSChangeConfigurationRequest;
extern CMSTriggerMessageRequest_t CMSTriggerMessageRequest;
extern CMSReserveNowRequest_t CMSReserveNowRequest[NUM_OF_CONNECTORS];
extern CMSResetRequest_t CMSResetRequest;
extern CMSUpdateFirmwareRequest_t CMSUpdateFirmwareRequest;
extern CMSSendLocalListRequest_t CMSSendLocalListRequest;
extern CMSClearCacheRequest_t CMSClearCacheRequest;
extern CMSGetLocalListVersionRequest_t CMSGetLocalListVersionRequest;
extern CMSGetConfigurationRequest_t CMSGetConfigurationRequest;
extern CMSClearChargingProfileRequest_t CMSClearChargingProfileRequest;
extern CMSDataTransferRequest_t CMSDataTransferRequest;
extern CMSGetCompositeScheduleRequest_t CMSGetCompositeScheduleRequest;
extern CMSGetDiagnosticsRequest_t CMSGetDiagnosticsRequest;
extern CMSSetChargingProfileRequest_t CMSSetChargingProfileRequest;
extern CMSUnlockConnectorRequest_t CMSUnlockConnectorRequest;

extern CPRemoteStartTransactionResponse_t CPRemoteStartTransactionResponse[NUM_OF_CONNECTORS];
extern CPRemoteStopTransactionResponse_t CPRemoteStopTransactionResponse;
extern CPReserveNowResponse_t CPReserveNowResponse;
extern CPResetResponse_t CPResetResponse;
extern CPSendLocalListResponse_t CPSendLocalListResponse;
extern CPTriggerMessageResponse_t CPTriggerMessageResponse;
extern CPCancelReservationResponse_t CPCancelReservationResponse;
extern CPChangeAvailabilityResponse_t CPChangeAvailabilityResponse;
extern CPChangeConfigurationResponse_t CPChangeConfigurationResponse;
extern CPClearCacheResponse_t CPClearCacheResponse;
extern CPGetLocalListVersionResponse_t CPGetLocalListVersionResponse;
extern CPUpdateFirmwareResponse_t CPUpdateFirmwareResponse;
extern CPGetDiagnosticsResponse_t CPGetDiagnosticsResponse;
extern CPGetConfigurationResponse_t CPGetConfigurationResponse;

extern CMSFirmwareStatusNotificationResponse_t CMSFirmwareStatusNotificationResponse;
extern CMSDiagnosticsStatusNotificationResponse_t CMSDiagnosticsStatusNotificationResponse;
extern CMSHeartbeatResponse_t CMSHeartbeatResponse;
extern CMSBootNotificationResponse_t CMSBootNotificationResponse;
extern CMSStatusNotificationResponse_t CMSStatusNotificationResponse[NUM_OF_CONNECTORS];
extern CMSMeterValuesResponse_t CMSMeterValuesResponse[NUM_OF_CONNECTORS];
extern CMSAuthorizeResponse_t CMSAuthorizeResponse[NUM_OF_CONNECTORS];
extern CMSStartTransactionResponse_t CMSStartTransactionResponse[NUM_OF_CONNECTORS];
extern CMSStopTransactionResponse_t CMSStopTransactionResponse[NUM_OF_CONNECTORS];

extern LocalAuthorizationList_t LocalAuthorizationList;

ledColors_t ledStateColor[NUM_OF_CONNECTORS];
extern QueueHandle_t OcppMsgQueue;
extern bool isWebsocketConnected;
extern bool isWifiConnected;
extern uint8_t gfciButton;
extern uint8_t earthDisconnectButton;
extern uint8_t emergencyButton;
extern uint8_t relayWeldDetectionButton;
extern uint8_t slavePushButton[4];
extern uint8_t PushButton[4];
bool PushButtonEnabled[4] = {false, false, false, false};
extern uint64_t rfidTagNumber;
extern bool newRfidRead;
extern Config_t config;
extern EVSE_states_enum_t EVSE_states_enum;

bool RtcTimeSet = false;
bool ChargingProfileAddedOrDeleted = false;
bool controlPilotActive = false;
bool isWebsocketBooted = false;
bool bootedNow = false;
bool reservedStatusSent[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
bool finishingStatusSent[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
bool finishingStatusPending[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
bool connectorEnabled[NUM_OF_CONNECTORS] = {false, false, false, false};
uint8_t connectorFault[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
uint8_t connectorFault_old[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
MeterFaults_t MeterFaults[NUM_OF_CONNECTORS];
bool connectorChargingStatusSent[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
uint8_t taskState[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
uint8_t taskState_old[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
uint8_t StatusState[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
uint8_t cpState_old[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
uint8_t cpState[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
uint8_t slaveLed[NUM_OF_CONNECTORS] = {LED0, LED1, LED2, LED3};

bool WebsocketRestart = false;
stopReasons_t stopReasons;
ledStates_t ledState[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
ledStates_t ledState_old[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
ledStates_t ledState_Before_Rfid[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
bool isRfidLedProcessed[NUM_OF_CONNECTORS] = {false, false, false, false};
bool isCpStateChanged = false;
bool isEmergencyStateChanged = false;
bool isPoweredOn = true;
bool AllTasksBusy = false;
bool processRfidForConnector[NUM_OF_CONNECTORS] = {false, false, false, false};
bool connectorsOfflineDataSent[NUM_OF_CONNECTORS] = {false, false, false, false};
uint8_t gfciButton_old[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
uint8_t earthDisconnectButton_old[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
uint8_t emergencyButton_old[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
uint8_t relayWeldDetectionButton_old[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
MeterFaults_t MeterFaults_old[NUM_OF_CONNECTORS];
ConnectorPrivateData_t ConnectorData[NUM_OF_CONNECTORS];
uint16_t ChargingSuspendedTimeCount[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
uint16_t ChargingFaultTimeCount[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
bool isFaultRecovered[NUM_OF_CONNECTORS] = {false, false, false, false};
bool isConnectorCharging[NUM_OF_CONNECTORS] = {false, false, false, false};
bool relayState[NUM_OF_CONNECTORS] = {false, false, false, false};
bool rfidTappedSentToDisplay = false;
char rfidTagNumberbuf[20];
char RemoteTagNumberbuf[20];
char AlprTagNumberbuf[20];

struct tm timeNow = {0};
time_t unix_time;
struct timeval tv;
char time_buffer[50];

uint32_t meterStop[NUM_OF_CONNECTORS];
uint8_t connectorTaskId[NUM_OF_CONNECTORS] = {0, 1, 2, 3};

ChargingProfiles_t ChargingProfile[NUM_OF_CONNECTORS];
ChargingProfiles_t ChargePointMaxChargingProfile[NUM_OF_CONNECTORS];
ConnectorPrivateData_t ConnectorOfflineData[NUM_OF_CONNECTORS];
MeterValueAlignedData_t MeterValueAlignedData[NUM_OF_CONNECTORS];
uint32_t ClockAlignedDataCount[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
bool ClockAlignedDataTime = false;

uint32_t connectorsOfflineDataCount[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
bool cpEnable[NUM_OF_CONNECTORS] = {false, false, false, false};
bool updatingOfflineData[NUM_OF_CONNECTORS] = {false, false, false, false};
bool updatingOfflineDataPending[NUM_OF_CONNECTORS] = {false, false, false, false};
bool updatingDataToCms[NUM_OF_CONNECTORS] = {false, false, false, false};
bool updateStatusAfterBoot[NUM_OF_CONNECTORS] = {false, false, false, false};
bool FirmwareUpdateStarted = false;
bool FirmwareUpdateFailed = false;
bool rfidMatchingWithLocalList = false;
bool rfidReceivedFromRemote[NUM_OF_CONNECTORS] = {false, false, false, false};
bool rfidAuthenticated = false;
bool RemoteAuthenticated = false;
bool AlprAuthenticated = false;
bool rfidTappedBlinkLed[NUM_OF_CONNECTORS] = {false, false, false, false};
bool newRfidRead_old = false;
bool rfidTappedStateProcessed = false;
bool updatingFirmwarePending = false;
bool isRfidTappedFirst = false;
bool isEvPluggedFirst = false;
bool RfidTappedFirstProcessed = false;
bool updateDisplayAfterRejected = false;
bool RemoteAuthorize = false;
bool AuthenticationProcessed = false;
bool uploadingLogs = false;
uint8_t RemoteAuthorizeConnId = 0;
bool AlprAuthorize = false;
uint8_t AlprAuthorizeConnId = 0;
uint8_t cpDutyCycleToSet[NUM_OF_CONNECTORS] = {53, 53, 53, 53};

SemaphoreHandle_t mutexControlPilot;

void changeDutyCycle(uint8_t connId)
{
    ESP_LOGI(TAG, "Current duty cycle to %d", config.cpDuty[connId]);
    ESP_LOGI(TAG, "Changing duty cycle to %d", cpDutyCycleToSet[connId]);

    // if (config.vcharge_lite_1_4)
    // {
    //     SlaveSetControlPilotDuty(connId, 100);
    // }
    // else
    // {
    //     updateRelayState(connId, 0);
    //     xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
    //     SetControlPilotDuty(100);
    //     xSemaphoreGive(mutexControlPilot);
    // }
    // ESP_LOGI(TAG, "Waiting For cpState to be B1 or E2");
    // while ((cpState[connId] != STATE_B1) && (cpState[connId] != STATE_E2))
    // {
    //     vTaskDelay(40 / portTICK_PERIOD_MS);
    // }
    ESP_LOGI(TAG, "Updating cpConfig");
    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
    config.cpDuty[connId] = cpDutyCycleToSet[connId];
    updateCpConfig();
    SetControlPilotDuty(config.cpDuty[connId]);
    xSemaphoreGive(mutexControlPilot);
    ESP_LOGI(TAG, "Updating cpConfig Done");
}

void updateSlaveLeds(void)
{
    for (uint8_t connId = 1; connId <= config.NumberOfConnectors; connId++)
    {
        custom_nvs_read_connector_data(connId);
        if (config.onlineOffline || config.offline)
        {
            if (ConnectorData[connId].CmsAvailable == false)
            {
                ledState[connId] = OFFLINE_UNAVAILABLE_LED_STATE;
                SlaveSetLedColor(slaveLed[connId], OFFLINE_UNAVAILABLE_LED);
                ESP_LOGI(TAG_LED, "Connector %hhu OFFLINE_UNAVAILABLE_LED_STATE", connId);
            }
            else
            {
                ledState[connId] = OFFLINE_AVAILABLE_LED_STATE;
                SlaveSetLedColor(slaveLed[connId], OFFLINE_AVAILABLE_LED);
                ESP_LOGI(TAG_LED, "Connector %hhu OFFLINE_AVAILABLE_LED_STATE", connId);
            }
        }
        else
        {
            ledState[connId] = ONLINE_ONLY_OFFLINE_UNAVAILABLE_LED_STATE;
            SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_OFFLINE_UNAVAILABLE_LED);
            ESP_LOGI(TAG_LED, "Connector %hhu ONLINE_ONLY_OFFLINE_UNAVAILABLE_LED_STATE", connId);
        }
    }

    for (uint8_t connId = 1; connId <= config.NumberOfConnectors; connId++)
    {
        custom_nvs_read_connector_data(connId);
        if (config.onlineOffline || config.offline)
        {
            if (ConnectorData[connId].CmsAvailable == false)
            {
                ledState[connId] = OFFLINE_UNAVAILABLE_LED_STATE;
                SlaveSetLedColor(slaveLed[connId], OFFLINE_UNAVAILABLE_LED);
                ESP_LOGI(TAG_LED, "Connector %hhu OFFLINE_UNAVAILABLE_LED_STATE", connId);
            }
            else
            {
                ledState[connId] = OFFLINE_AVAILABLE_LED_STATE;
                SlaveSetLedColor(slaveLed[connId], OFFLINE_AVAILABLE_LED);
                ESP_LOGI(TAG_LED, "Connector %hhu OFFLINE_AVAILABLE_LED_STATE", connId);
            }
        }
        else
        {
            ledState[connId] = ONLINE_ONLY_OFFLINE_UNAVAILABLE_LED_STATE;
            SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_OFFLINE_UNAVAILABLE_LED);
            ESP_LOGI(TAG_LED, "Connector %hhu ONLINE_ONLY_OFFLINE_UNAVAILABLE_LED_STATE", connId);
        }
    }
}

void clearSendLocalList(void)
{
    memset(&CMSSendLocalListRequest, 0, sizeof(CMSSendLocalListRequest));
}

void clearLocalAuthenticatinCache(void)
{
    memset(&LocalAuthorizationList, 0, sizeof(LocalAuthorizationList));
}

void updateDiagnosticsfileName(void)
{
    struct timeval tv;
    struct tm timeinfo;
    char time[50];
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &timeinfo);
    setNULL(time);
    snprintf(time, sizeof(time),
             "_%04d_%02d_%02d_%02d_%02d_%02d",
             (timeinfo.tm_year + 1900),
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
    setNULL(CPGetDiagnosticsResponse.fileName);
    strcat(CPGetDiagnosticsResponse.fileName, config.serialNumber);
    strcat(CPGetDiagnosticsResponse.fileName, time);
}

void updateDisplayOnAcceptanceFailed(uint8_t connId)
{
    for (uint8_t i = 0; i <= config.NumberOfConnectors; i++)
        processRfidForConnector[i] = false;

    if (connectorFault[connId])
    {
        setChargerFaultBit(connId);
    }
    else if (ConnectorData[connId].CmsAvailable == false)
    {
        SetChargerUnAvailableBit(connId);
    }
    else if (ConnectorData[connId].isReserved)
    {
        SetChargerReservedBit(connId);
    }
    else if ((cpEnable[connId] && (cpState[connId] == STATE_A)) || (cpEnable[connId] == false))
    {
        SetChargerAvailableBit(connId);
    }
    else if (cpEnable[connId] && (cpState[connId] == STATE_A))
    {
        setChargerEvPluggedTapRfidBit(connId);
    }
    else if (cpEnable[connId] && ((cpState[connId] == STATE_B1) || (cpState[connId] == STATE_E2)))
    {
        isRfidTappedFirst = false;
        setChargerEvPluggedTapRfidBit(connId);
    }
}

void updateStopStansactionReason(uint8_t connId)
{
    ConnectorPrivateData_t ConnectorData_temp;
    if (updatingDataToCms[connId])
        memcpy(&ConnectorData_temp, &ConnectorOfflineData[connId], sizeof(ConnectorPrivateData_t));
    else
        memcpy(&ConnectorData_temp, &ConnectorData[connId], sizeof(ConnectorPrivateData_t));

    if (ConnectorData_temp.stopReason == Stop_DeAuthorized)
    {
        memcpy(CPStopTransactionRequest[connId].reason, "DeAuthorized", strlen("DeAuthorized"));
    }
    else if (ConnectorData_temp.stopReason == Stop_EVDisconnected)
    {
        memcpy(CPStopTransactionRequest[connId].reason, "EVDisconnected", strlen("EVDisconnected"));
    }
    else if (ConnectorData_temp.stopReason == Stop_HardReset)
    {
        memcpy(CPStopTransactionRequest[connId].reason, "HardReset", strlen("HardReset"));
    }
    else if (ConnectorData_temp.stopReason == Stop_Local)
    {
        memcpy(CPStopTransactionRequest[connId].reason, "Local", strlen("Local"));
    }
    else if (ConnectorData_temp.stopReason == Stop_Other)
    {
        memcpy(CPStopTransactionRequest[connId].reason, "Other", strlen("Other"));
    }
    else if (ConnectorData_temp.stopReason == Stop_PowerLoss)
    {
        memcpy(CPStopTransactionRequest[connId].reason, "PowerLoss", strlen("PowerLoss"));
    }
    else if (ConnectorData_temp.stopReason == Stop_Reboot)
    {
        memcpy(CPStopTransactionRequest[connId].reason, "Reboot", strlen("Reboot"));
    }
    else if (ConnectorData_temp.stopReason == Stop_Remote)
    {
        memcpy(CPStopTransactionRequest[connId].reason, "Remote", strlen("Remote"));
    }
    else if (ConnectorData_temp.stopReason == Stop_SoftReset)
    {
        memcpy(CPStopTransactionRequest[connId].reason, "SoftReset", strlen("SoftReset"));
    }
    else if (ConnectorData_temp.stopReason == Stop_UnlockCommand)
    {
        memcpy(CPStopTransactionRequest[connId].reason, "UnlockCommand", strlen("UnlockCommand"));
    }
    else if (ConnectorData_temp.stopReason == Stop_EmergencyStop)
    {
        memcpy(CPStopTransactionRequest[connId].reason, "EmergencyStop", strlen("EmergencyStop"));
    }
}

void updateStopReason(uint8_t connId)
{
    if (emergencyButton == 1)
        ConnectorData[connId].stopReason = Stop_EmergencyStop;
    else if (MeterFaults[connId].MeterPowerLoss == 1)
        ConnectorData[connId].stopReason = Stop_PowerLoss;
    else if (earthDisconnectButton == 1)
        ConnectorData[connId].stopReason = Stop_Other;
    else if (gfciButton == 1)
        ConnectorData[connId].stopReason = Stop_Other;
    else if (MeterFaults[connId].MeterHighTemp == 1)
        ConnectorData[connId].stopReason = Stop_Other;
    else if (MeterFaults[connId].MeterOverVoltage == 1)
        ConnectorData[connId].stopReason = Stop_Other;
    else if (MeterFaults[connId].MeterUnderVoltage == 1)
        ConnectorData[connId].stopReason = Stop_Other;
    else if (MeterFaults[connId].MeterOverCurrent == 1)
        ConnectorData[connId].stopReason = Stop_Other;
    else if (relayWeldDetectionButton == 1)
        ConnectorData[connId].stopReason = Stop_Other;
    else
        ConnectorData[connId].stopReason = Stop_Other;
}

void logConnectorData(uint8_t connId)
{
    ESP_LOGI(TAG_DEBUG, "Connector %d isTransactionOngoing %d", connId, ConnectorData[connId].isTransactionOngoing);
    ESP_LOGI(TAG_DEBUG, "Connector %d offlineStarted %d", connId, ConnectorData[connId].offlineStarted);
    ESP_LOGI(TAG_DEBUG, "Connector %d isReserved %d", connId, ConnectorData[connId].isReserved);
    ESP_LOGI(TAG_DEBUG, "Connector %d CmsAvailableScheduled %d", connId, ConnectorData[connId].CmsAvailableScheduled);
    ESP_LOGI(TAG_DEBUG, "Connector %d CmsAvailable %d", connId, ConnectorData[connId].CmsAvailable);
    ESP_LOGI(TAG_DEBUG, "Connector %d CmsAvailableChanged %d", connId, ConnectorData[connId].CmsAvailableChanged);
    ESP_LOGI(TAG_DEBUG, "Connector %d UnkownId %d", connId, ConnectorData[connId].UnkownId);
    ESP_LOGI(TAG_DEBUG, "Connector %d InvalidId %d", connId, ConnectorData[connId].InvalidId);
    ESP_LOGI(TAG_DEBUG, "Connector %d idTag %s", connId, ConnectorData[connId].idTag);
    ESP_LOGI(TAG_DEBUG, "Connector %d state %d", connId, ConnectorData[connId].state);
    ESP_LOGI(TAG_DEBUG, "Connector %d stopReason %d", connId, ConnectorData[connId].stopReason);
    ESP_LOGI(TAG_DEBUG, "Connector %d transactionId %ld", connId, ConnectorData[connId].transactionId);
    ESP_LOGI(TAG_DEBUG, "Connector %d meterStart %f", connId, ConnectorData[connId].meterStart);
    ESP_LOGI(TAG_DEBUG, "Connector %d meterStop %f", connId, ConnectorData[connId].meterStop);
    ESP_LOGI(TAG_DEBUG, "Connector %d meterValue.temp %d", connId, ConnectorData[connId].meterValue.temp);
    ESP_LOGI(TAG_DEBUG, "Connector %d meterValue.voltage[%hhu] %f", connId, connId, ConnectorData[connId].meterValue.voltage[connId]);
    ESP_LOGI(TAG_DEBUG, "Connector %d meterValue.current[%hhu] %f", connId, connId, ConnectorData[connId].meterValue.current[connId]);
    ESP_LOGI(TAG_DEBUG, "Connector %d meterValue.power %f", connId, ConnectorData[connId].meterValue.power);
    ESP_LOGI(TAG_DEBUG, "Connector %d Energy %f", connId, ConnectorData[connId].Energy);
    ESP_LOGI(TAG_DEBUG, "Connector %d ReservedData.idTag %s", connId, ConnectorData[connId].ReservedData.idTag);
    ESP_LOGI(TAG_DEBUG, "Connector %d ReservedData.parentidTag %s", connId, ConnectorData[connId].ReservedData.parentidTag);
    ESP_LOGI(TAG_DEBUG, "Connector %d ReservedData.expiryDate %s", connId, ConnectorData[connId].ReservedData.expiryDate);
    ESP_LOGI(TAG_DEBUG, "Connector %d ReservedData.reservationId %d", connId, ConnectorData[connId].ReservedData.reservationId);
}

void saveTagToLocalAuthenticationList(void)
{
    uint32_t i = 0;
    bool isTagAlreadyPresent = false;
    for (i = 0; i < LOCAL_LIST_COUNT; i++)
    {
        if ((strcmp(LocalAuthorizationList.idTag[i], rfidTagNumberbuf) == 0) && (LocalAuthorizationList.idTagPresent[i] == true))
        {
            isTagAlreadyPresent = true;
            i = LOCAL_LIST_COUNT;
        }
    }
    if (isTagAlreadyPresent == false)
    {
        for (i = 0; i < LOCAL_LIST_COUNT; i++)
        {
            if (LocalAuthorizationList.idTagPresent[i] == false)
            {
                setNULL(LocalAuthorizationList.idTag[i]);
                memcpy(LocalAuthorizationList.idTag[i], rfidTagNumberbuf, strlen(rfidTagNumberbuf));
                LocalAuthorizationList.idTagPresent[i] = true;
                i = LOCAL_LIST_COUNT;
            }
        }
    }
    custom_nvs_write_LocalAuthorizationList_ocpp();
    ESP_LOGI(TAG_DEBUG, "Printing Local Authentication list");
    // log localAuthentication list
    for (i = 0; i < LOCAL_LIST_COUNT; i++)
    {
        if (LocalAuthorizationList.idTagPresent[i])
        {
            ESP_LOGI(TAG_DEBUG, "Local Authentication Tag%ld : %s", i, LocalAuthorizationList.idTag[i]);
        }
    }
}

// void SlaveUpdateState(uint8_t connId, uint8_t state)
// {
//     if (connId == 1)
//     {
//         SlaveSetState(CP1, state);
//     }
//     else if (connId == 2)
//     {
//         SlaveSetState(CP2, state);
//     }
// }

void SetDefaultOcppConfig(void)
{
    esp_err_t result = custom_nvs_read_config_ocpp();
    switch (result)
    {
    case ESP_ERR_NOT_FOUND:
    case ESP_ERR_NVS_NOT_FOUND:
        memset(&CPGetConfigurationResponse, 0, sizeof(CPGetConfigurationResponse_t));
        setNULL(CPGetConfigurationResponse.AuthorizeRemoteTxRequestsValue);
        setNULL(CPGetConfigurationResponse.ClockAlignedDataIntervalValue);
        setNULL(CPGetConfigurationResponse.ConnectionTimeOutValue);
        setNULL(CPGetConfigurationResponse.GetConfigurationMaxKeysValue);
        setNULL(CPGetConfigurationResponse.HeartbeatIntervalValue);
        setNULL(CPGetConfigurationResponse.LocalAuthorizeOfflineValue);
        setNULL(CPGetConfigurationResponse.LocalPreAuthorizeValue);
        setNULL(CPGetConfigurationResponse.MeterValuesAlignedDataValue);
        setNULL(CPGetConfigurationResponse.MeterValuesSampledDataValue);
        setNULL(CPGetConfigurationResponse.MeterValueSampleIntervalValue);
        setNULL(CPGetConfigurationResponse.NumberOfConnectorsValue);
        setNULL(CPGetConfigurationResponse.ResetRetriesValue);
        setNULL(CPGetConfigurationResponse.ConnectorPhaseRotationValue);
        setNULL(CPGetConfigurationResponse.StopTransactionOnEVSideDisconnectValue);
        setNULL(CPGetConfigurationResponse.StopTransactionOnInvalidIdValue);
        setNULL(CPGetConfigurationResponse.StopTxnAlignedDataValue);
        setNULL(CPGetConfigurationResponse.StopTxnSampledDataValue);
        setNULL(CPGetConfigurationResponse.SupportedFeatureProfilesValue);
        setNULL(CPGetConfigurationResponse.TransactionMessageAttemptsValue);
        setNULL(CPGetConfigurationResponse.TransactionMessageRetryIntervalValue);
        setNULL(CPGetConfigurationResponse.UnlockConnectorOnEVSideDisconnectValue);
        setNULL(CPGetConfigurationResponse.AllowOfflineTxForUnknownIdValue);
        setNULL(CPGetConfigurationResponse.AuthorizationCacheEnabledValue);
        setNULL(CPGetConfigurationResponse.BlinkRepeatValue);
        setNULL(CPGetConfigurationResponse.LightIntensityValue);
        setNULL(CPGetConfigurationResponse.MaxEnergyOnInvalidIdValue);
        setNULL(CPGetConfigurationResponse.MeterValuesAlignedDataMaxLengthValue);
        setNULL(CPGetConfigurationResponse.MeterValuesSampledDataMaxLengthValue);
        setNULL(CPGetConfigurationResponse.MinimumStatusDurationValue);
        setNULL(CPGetConfigurationResponse.ConnectorPhaseRotationMaxLengthValue);
        setNULL(CPGetConfigurationResponse.StopTxnAlignedDataMaxLengthValue);
        setNULL(CPGetConfigurationResponse.StopTxnSampledDataMaxLengthValue);
        setNULL(CPGetConfigurationResponse.SupportedFeatureProfilesMaxLengthValue);
        setNULL(CPGetConfigurationResponse.WebSocketPingIntervalValue);
        setNULL(CPGetConfigurationResponse.LocalAuthListEnabledValue);
        setNULL(CPGetConfigurationResponse.LocalAuthListMaxLengthValue);
        setNULL(CPGetConfigurationResponse.SendLocalListMaxLengthValue);
        setNULL(CPGetConfigurationResponse.ReserveConnectorZeroSupportedValue);
        setNULL(CPGetConfigurationResponse.ChargeProfileMaxStackLevelValue);
        setNULL(CPGetConfigurationResponse.ChargingScheduleAllowedChargingRateUnitValue);
        setNULL(CPGetConfigurationResponse.ChargingScheduleMaxPeriodsValue);
        setNULL(CPGetConfigurationResponse.ConnectorSwitch3to1PhaseSupportedValue);
        setNULL(CPGetConfigurationResponse.MaxChargingProfilesInstalledValue);

        CPGetConfigurationResponse.AuthorizeRemoteTxRequestsReadOnly = false;
        CPGetConfigurationResponse.ClockAlignedDataIntervalReadOnly = false;
        CPGetConfigurationResponse.ConnectionTimeOutReadOnly = false;
        CPGetConfigurationResponse.GetConfigurationMaxKeysReadOnly = true;
        CPGetConfigurationResponse.HeartbeatIntervalReadOnly = false;
        CPGetConfigurationResponse.LocalAuthorizeOfflineReadOnly = false;
        CPGetConfigurationResponse.LocalPreAuthorizeReadOnly = false;
        CPGetConfigurationResponse.MeterValuesAlignedDataReadOnly = false;
        CPGetConfigurationResponse.MeterValuesSampledDataReadOnly = false;
        CPGetConfigurationResponse.MeterValueSampleIntervalReadOnly = false;
        CPGetConfigurationResponse.NumberOfConnectorsReadOnly = true;
        CPGetConfigurationResponse.ResetRetriesReadOnly = false;
        CPGetConfigurationResponse.ConnectorPhaseRotationReadOnly = false;
        CPGetConfigurationResponse.StopTransactionOnEVSideDisconnectReadOnly = false;
        CPGetConfigurationResponse.StopTransactionOnInvalidIdReadOnly = false;
        CPGetConfigurationResponse.StopTxnAlignedDataReadOnly = false;
        CPGetConfigurationResponse.StopTxnSampledDataReadOnly = false;
        CPGetConfigurationResponse.SupportedFeatureProfilesReadOnly = true;
        CPGetConfigurationResponse.TransactionMessageAttemptsReadOnly = false;
        CPGetConfigurationResponse.TransactionMessageRetryIntervalReadOnly = false;
        CPGetConfigurationResponse.UnlockConnectorOnEVSideDisconnectReadOnly = false;
        CPGetConfigurationResponse.AllowOfflineTxForUnknownIdReadOnly = false;
        CPGetConfigurationResponse.AuthorizationCacheEnabledReadOnly = false;
        CPGetConfigurationResponse.BlinkRepeatReadOnly = false;
        CPGetConfigurationResponse.LightIntensityReadOnly = false;
        CPGetConfigurationResponse.MaxEnergyOnInvalidIdReadOnly = false;
        CPGetConfigurationResponse.MeterValuesAlignedDataMaxLengthReadOnly = true;
        CPGetConfigurationResponse.MeterValuesSampledDataMaxLengthReadOnly = true;
        CPGetConfigurationResponse.MinimumStatusDurationReadOnly = false;
        CPGetConfigurationResponse.ConnectorPhaseRotationMaxLengthReadOnly = true;
        CPGetConfigurationResponse.StopTxnAlignedDataMaxLengthReadOnly = true;
        CPGetConfigurationResponse.StopTxnSampledDataMaxLengthReadOnly = true;
        CPGetConfigurationResponse.SupportedFeatureProfilesMaxLengthReadOnly = true;
        CPGetConfigurationResponse.WebSocketPingIntervalReadOnly = false;
        CPGetConfigurationResponse.LocalAuthListEnabledReadOnly = false;
        CPGetConfigurationResponse.LocalAuthListMaxLengthReadOnly = true;
        CPGetConfigurationResponse.SendLocalListMaxLengthReadOnly = true;
        CPGetConfigurationResponse.ReserveConnectorZeroSupportedReadOnly = true;
        CPGetConfigurationResponse.ChargeProfileMaxStackLevelReadOnly = true;
        CPGetConfigurationResponse.ChargingScheduleAllowedChargingRateUnitReadOnly = false;
        CPGetConfigurationResponse.ChargingScheduleMaxPeriodsReadOnly = true;
        CPGetConfigurationResponse.ConnectorSwitch3to1PhaseSupportedReadOnly = true;
        CPGetConfigurationResponse.MaxChargingProfilesInstalledReadOnly = true;

        memcpy(CPGetConfigurationResponse.AuthorizeRemoteTxRequestsValue, "false", strlen("false"));
        memcpy(CPGetConfigurationResponse.ClockAlignedDataIntervalValue, "600", strlen("600"));
        memcpy(CPGetConfigurationResponse.ConnectionTimeOutValue, "120", strlen("120"));
        memcpy(CPGetConfigurationResponse.GetConfigurationMaxKeysValue, "50", strlen("50"));
        memcpy(CPGetConfigurationResponse.HeartbeatIntervalValue, "30", strlen("30"));
        memcpy(CPGetConfigurationResponse.LocalAuthorizeOfflineValue, "true", strlen("true"));
        memcpy(CPGetConfigurationResponse.LocalPreAuthorizeValue, "true", strlen("true"));
        memcpy(CPGetConfigurationResponse.MeterValuesAlignedDataValue,
               "Energy.Active.Import.Register, Power.Active.Import, Voltage, Current.Import, Temperature",
               strlen("Energy.Active.Import.Register, Power.Active.Import, Voltage, Current.Import, Temperature"));
        memcpy(CPGetConfigurationResponse.MeterValuesSampledDataValue,
               "Energy.Active.Import.Register, Power.Active.Import, Voltage, Current.Import, Temperature",
               strlen("Energy.Active.Import.Register, Power.Active.Import, Voltage, Current.Import, Temperature"));
        memcpy(CPGetConfigurationResponse.MeterValueSampleIntervalValue, "30", strlen("30"));
        if (config.NumberOfConnectors == 1)
        {
            memcpy(CPGetConfigurationResponse.NumberOfConnectorsValue, "1", strlen("1"));
        }
        else if (config.NumberOfConnectors == 2)
        {
            memcpy(CPGetConfigurationResponse.NumberOfConnectorsValue, "2", strlen("2"));
        }
        else
        {
            memcpy(CPGetConfigurationResponse.NumberOfConnectorsValue, "3", strlen("3"));
        }
        memcpy(CPGetConfigurationResponse.ResetRetriesValue, "3", strlen("3"));
        memcpy(CPGetConfigurationResponse.ConnectorPhaseRotationValue, "NotApplicable", strlen("NotApplicable"));
        memcpy(CPGetConfigurationResponse.StopTransactionOnEVSideDisconnectValue, "true", strlen("true"));
        memcpy(CPGetConfigurationResponse.StopTransactionOnInvalidIdValue, "true", strlen("true"));
        memcpy(CPGetConfigurationResponse.StopTxnAlignedDataValue,
               "Energy.Active.Import.Register, Power.Active.Import, Voltage, Current.Import, Temperature",
               strlen("Energy.Active.Import.Register, Power.Active.Import, Voltage, Current.Import, Temperature"));
        memcpy(CPGetConfigurationResponse.StopTxnSampledDataValue,
               "Energy.Active.Import.Register, Power.Active.Import, Voltage, Current.Import, Temperature",
               strlen("Energy.Active.Import.Register, Power.Active.Import, Voltage, Current.Import, Temperature"));
        memcpy(CPGetConfigurationResponse.SupportedFeatureProfilesValue,
               "Core,LocalAuthListManagement,Reservation,RemoteTrigger,FirmwareManagement,SmartCharging",
               strlen("Core,LocalAuthListManagement,Reservation,RemoteTrigger,FirmwareManagement,SmartCharging"));
        memcpy(CPGetConfigurationResponse.TransactionMessageAttemptsValue, "2", strlen("2"));
        memcpy(CPGetConfigurationResponse.TransactionMessageRetryIntervalValue, "60", strlen("60"));
        memcpy(CPGetConfigurationResponse.UnlockConnectorOnEVSideDisconnectValue, "true", strlen("true"));
        memcpy(CPGetConfigurationResponse.AllowOfflineTxForUnknownIdValue, "false", strlen("false"));
        memcpy(CPGetConfigurationResponse.AuthorizationCacheEnabledValue, "true", strlen("true"));
        memcpy(CPGetConfigurationResponse.BlinkRepeatValue, "500", strlen("500"));
        memcpy(CPGetConfigurationResponse.LightIntensityValue, "100", strlen("100"));
        memcpy(CPGetConfigurationResponse.MaxEnergyOnInvalidIdValue, "0", strlen("0"));
        memcpy(CPGetConfigurationResponse.MeterValuesAlignedDataMaxLengthValue, "5", strlen("5"));
        memcpy(CPGetConfigurationResponse.MeterValuesSampledDataMaxLengthValue, "5", strlen("5"));
        memcpy(CPGetConfigurationResponse.MinimumStatusDurationValue, "900", strlen("900"));
        memcpy(CPGetConfigurationResponse.ConnectorPhaseRotationMaxLengthValue, "1", strlen("1"));
        memcpy(CPGetConfigurationResponse.StopTxnAlignedDataMaxLengthValue, "5", strlen("5"));
        memcpy(CPGetConfigurationResponse.StopTxnSampledDataMaxLengthValue, "5", strlen("5"));
        memcpy(CPGetConfigurationResponse.SupportedFeatureProfilesMaxLengthValue, "6", strlen("6"));
        memcpy(CPGetConfigurationResponse.WebSocketPingIntervalValue, "30", strlen("30"));
        memcpy(CPGetConfigurationResponse.LocalAuthListEnabledValue, "true", strlen("true"));
        memcpy(CPGetConfigurationResponse.LocalAuthListMaxLengthValue, "10", strlen("10"));
        memcpy(CPGetConfigurationResponse.SendLocalListMaxLengthValue, "10", strlen("10"));
        memcpy(CPGetConfigurationResponse.ReserveConnectorZeroSupportedValue, "false", strlen("false"));
        memcpy(CPGetConfigurationResponse.ChargeProfileMaxStackLevelValue, "100", strlen("100"));
        memcpy(CPGetConfigurationResponse.ChargingScheduleAllowedChargingRateUnitValue, "A", strlen("A"));
        memcpy(CPGetConfigurationResponse.ChargingScheduleMaxPeriodsValue, "20", strlen("20"));
        memcpy(CPGetConfigurationResponse.ConnectorSwitch3to1PhaseSupportedValue, "false", strlen("false"));
        memcpy(CPGetConfigurationResponse.MaxChargingProfilesInstalledValue, "100", strlen("100"));

        CPGetConfigurationResponse.AuthorizeRemoteTxRequests = false;
        CPGetConfigurationResponse.ClockAlignedDataInterval = 600;
        CPGetConfigurationResponse.ConnectionTimeOut = 120;
        CPGetConfigurationResponse.GetConfigurationMaxKeys = 50;
        CPGetConfigurationResponse.HeartbeatInterval = 30;
        CPGetConfigurationResponse.LocalAuthorizeOffline = true;
        CPGetConfigurationResponse.LocalPreAuthorize = true;
        CPGetConfigurationResponse.MeterValuesAlignedData.EnergyActiveImportRegister = true;
        CPGetConfigurationResponse.MeterValuesAlignedData.PowerActiveImport = true;
        CPGetConfigurationResponse.MeterValuesAlignedData.Voltage = true;
        CPGetConfigurationResponse.MeterValuesAlignedData.CurrentImport = true;
        CPGetConfigurationResponse.MeterValuesAlignedData.Temperature = true;
        CPGetConfigurationResponse.MeterValuesSampledData.EnergyActiveImportRegister = true;
        CPGetConfigurationResponse.MeterValuesSampledData.PowerActiveImport = true;
        CPGetConfigurationResponse.MeterValuesSampledData.Voltage = true;
        CPGetConfigurationResponse.MeterValuesSampledData.CurrentImport = true;
        CPGetConfigurationResponse.MeterValuesSampledData.Temperature = true;
        CPGetConfigurationResponse.MeterValueSampleInterval = 30;
        CPGetConfigurationResponse.NumberOfConnectors = config.NumberOfConnectors;
        CPGetConfigurationResponse.ResetRetries = 3;
        CPGetConfigurationResponse.StopTransactionOnEVSideDisconnect = true;
        CPGetConfigurationResponse.StopTransactionOnInvalidId = true;
        CPGetConfigurationResponse.StopTxnAlignedData.EnergyActiveImportRegister = true;
        CPGetConfigurationResponse.StopTxnAlignedData.PowerActiveImport = true;
        CPGetConfigurationResponse.StopTxnAlignedData.Voltage = true;
        CPGetConfigurationResponse.StopTxnAlignedData.CurrentImport = true;
        CPGetConfigurationResponse.StopTxnAlignedData.Temperature = true;
        CPGetConfigurationResponse.StopTxnSampledData.EnergyActiveImportRegister = true;
        CPGetConfigurationResponse.StopTxnSampledData.PowerActiveImport = true;
        CPGetConfigurationResponse.StopTxnSampledData.Voltage = true;
        CPGetConfigurationResponse.StopTxnSampledData.CurrentImport = true;
        CPGetConfigurationResponse.StopTxnSampledData.Temperature = true;
        CPGetConfigurationResponse.SupportedFeatureProfiles.Core = true;
        CPGetConfigurationResponse.SupportedFeatureProfiles.LocalAuthListManagement = true;
        CPGetConfigurationResponse.SupportedFeatureProfiles.Reservation = true;
        CPGetConfigurationResponse.SupportedFeatureProfiles.RemoteTrigger = true;
        CPGetConfigurationResponse.SupportedFeatureProfiles.FirmwareManagement = true;
        CPGetConfigurationResponse.SupportedFeatureProfiles.SmartCharging = false;
        CPGetConfigurationResponse.TransactionMessageAttempts = 2;
        CPGetConfigurationResponse.TransactionMessageRetryInterval = 60;
        CPGetConfigurationResponse.UnlockConnectorOnEVSideDisconnect = true;
        CPGetConfigurationResponse.AllowOfflineTxForUnknownId = false;
        CPGetConfigurationResponse.AuthorizationCacheEnabled = true;
        CPGetConfigurationResponse.BlinkRepeat = 500;
        CPGetConfigurationResponse.LightIntensity = 100;
        CPGetConfigurationResponse.MaxEnergyOnInvalidId = 0;
        CPGetConfigurationResponse.MeterValuesAlignedDataMaxLength = 5;
        CPGetConfigurationResponse.MeterValuesSampledDataMaxLength = 5;
        CPGetConfigurationResponse.MinimumStatusDuration = 900;
        CPGetConfigurationResponse.ConnectorPhaseRotationMaxLength = 1;
        CPGetConfigurationResponse.StopTxnAlignedDataMaxLength = 5;
        CPGetConfigurationResponse.StopTxnSampledDataMaxLength = 5;
        CPGetConfigurationResponse.SupportedFeatureProfilesMaxLength = 6;
        CPGetConfigurationResponse.WebSocketPingInterval = 30;
        CPGetConfigurationResponse.LocalAuthListEnabled = true;
        CPGetConfigurationResponse.LocalAuthListMaxLength = 10;
        CPGetConfigurationResponse.SendLocalListMaxLength = 10;
        CPGetConfigurationResponse.ReserveConnectorZeroSupported = false;
        CPGetConfigurationResponse.ChargeProfileMaxStackLevel = 100;
        CPGetConfigurationResponse.ChargingScheduleMaxPeriods = 20;
        CPGetConfigurationResponse.ConnectorSwitch3to1PhaseSupported = false;
        CPGetConfigurationResponse.MaxChargingProfilesInstalled = 100;

        custom_nvs_write_config_ocpp();
        break;
    case ESP_OK:
        if (config.NumberOfConnectors == 1)
        {
            memcpy(CPGetConfigurationResponse.NumberOfConnectorsValue, "1", strlen("1"));
        }
        else if (config.NumberOfConnectors == 2)
        {
            memcpy(CPGetConfigurationResponse.NumberOfConnectorsValue, "2", strlen("2"));
        }
        else
        {
            memcpy(CPGetConfigurationResponse.NumberOfConnectorsValue, "3", strlen("3"));
        }

        break;
    default:
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(result));
        break;
    }
    ESP_LOGI(TAG, "----------------------------------------------------------------------");
    ESP_LOGI(TAG, "OCPP Configuration Parameters");
    ESP_LOGI(TAG, "----------------------------------------------------------------------");

    ESP_LOGI(TAG, "AuthorizeRemoteTxRequestsValue : %s", CPGetConfigurationResponse.AuthorizeRemoteTxRequestsValue);
    ESP_LOGI(TAG, "ClockAlignedDataIntervalValue : %s", CPGetConfigurationResponse.ClockAlignedDataIntervalValue);
    ESP_LOGI(TAG, "ConnectionTimeOutValue : %s", CPGetConfigurationResponse.ConnectionTimeOutValue);
    ESP_LOGI(TAG, "GetConfigurationMaxKeysValue : %s", CPGetConfigurationResponse.GetConfigurationMaxKeysValue);
    ESP_LOGI(TAG, "HeartbeatIntervalValue : %s", CPGetConfigurationResponse.HeartbeatIntervalValue);
    ESP_LOGI(TAG, "LocalAuthorizeOfflineValue : %s", CPGetConfigurationResponse.LocalAuthorizeOfflineValue);
    ESP_LOGI(TAG, "LocalPreAuthorizeValue : %s", CPGetConfigurationResponse.LocalPreAuthorizeValue);
    ESP_LOGI(TAG, "MeterValuesAlignedDataValue : %s", CPGetConfigurationResponse.MeterValuesAlignedDataValue);
    ESP_LOGI(TAG, "MeterValuesSampledDataValue : %s", CPGetConfigurationResponse.MeterValuesSampledDataValue);
    ESP_LOGI(TAG, "MeterValueSampleIntervalValue : %s", CPGetConfigurationResponse.MeterValueSampleIntervalValue);
    ESP_LOGI(TAG, "NumberOfConnectorsValue : %s", CPGetConfigurationResponse.NumberOfConnectorsValue);
    ESP_LOGI(TAG, "ResetRetriesValue : %s", CPGetConfigurationResponse.ResetRetriesValue);
    ESP_LOGI(TAG, "ConnectorPhaseRotationValue : %s", CPGetConfigurationResponse.ConnectorPhaseRotationValue);
    ESP_LOGI(TAG, "StopTransactionOnEVSideDisconnectValue : %s", CPGetConfigurationResponse.StopTransactionOnEVSideDisconnectValue);
    ESP_LOGI(TAG, "StopTransactionOnInvalidIdValue : %s", CPGetConfigurationResponse.StopTransactionOnInvalidIdValue);
    ESP_LOGI(TAG, "StopTxnAlignedDataValue : %s", CPGetConfigurationResponse.StopTxnAlignedDataValue);
    ESP_LOGI(TAG, "StopTxnSampledDataValue : %s", CPGetConfigurationResponse.StopTxnSampledDataValue);
    ESP_LOGI(TAG, "SupportedFeatureProfilesValue : %s", CPGetConfigurationResponse.SupportedFeatureProfilesValue);
    ESP_LOGI(TAG, "TransactionMessageAttemptsValue : %s", CPGetConfigurationResponse.TransactionMessageAttemptsValue);
    ESP_LOGI(TAG, "TransactionMessageRetryIntervalValue : %s", CPGetConfigurationResponse.TransactionMessageRetryIntervalValue);
    ESP_LOGI(TAG, "UnlockConnectorOnEVSideDisconnectValue : %s", CPGetConfigurationResponse.UnlockConnectorOnEVSideDisconnectValue);
    ESP_LOGI(TAG, "AllowOfflineTxForUnknownIdValue : %s", CPGetConfigurationResponse.AllowOfflineTxForUnknownIdValue);
    ESP_LOGI(TAG, "AuthorizationCacheEnabledValue : %s", CPGetConfigurationResponse.AuthorizationCacheEnabledValue);
    ESP_LOGI(TAG, "BlinkRepeatValue : %s", CPGetConfigurationResponse.BlinkRepeatValue);
    ESP_LOGI(TAG, "LightIntensityValue : %s", CPGetConfigurationResponse.LightIntensityValue);
    ESP_LOGI(TAG, "MaxEnergyOnInvalidIdValue : %s", CPGetConfigurationResponse.MaxEnergyOnInvalidIdValue);
    ESP_LOGI(TAG, "MeterValuesAlignedDataMaxLengthValue : %s", CPGetConfigurationResponse.MeterValuesAlignedDataMaxLengthValue);
    ESP_LOGI(TAG, "MeterValuesSampledDataMaxLengthValue : %s", CPGetConfigurationResponse.MeterValuesSampledDataMaxLengthValue);
    ESP_LOGI(TAG, "MinimumStatusDurationValue : %s", CPGetConfigurationResponse.MinimumStatusDurationValue);
    ESP_LOGI(TAG, "ConnectorPhaseRotationMaxLengthValue : %s", CPGetConfigurationResponse.ConnectorPhaseRotationMaxLengthValue);
    ESP_LOGI(TAG, "StopTxnAlignedDataMaxLengthValue : %s", CPGetConfigurationResponse.StopTxnAlignedDataMaxLengthValue);
    ESP_LOGI(TAG, "StopTxnSampledDataMaxLengthValue : %s", CPGetConfigurationResponse.StopTxnSampledDataMaxLengthValue);
    ESP_LOGI(TAG, "SupportedFeatureProfilesMaxLengthValue : %s", CPGetConfigurationResponse.SupportedFeatureProfilesMaxLengthValue);
    ESP_LOGI(TAG, "WebSocketPingIntervalValue : %s", CPGetConfigurationResponse.WebSocketPingIntervalValue);
    ESP_LOGI(TAG, "LocalAuthListEnabledValue : %s", CPGetConfigurationResponse.LocalAuthListEnabledValue);
    ESP_LOGI(TAG, "LocalAuthListMaxLengthValue : %s", CPGetConfigurationResponse.LocalAuthListMaxLengthValue);
    ESP_LOGI(TAG, "SendLocalListMaxLengthValue : %s", CPGetConfigurationResponse.SendLocalListMaxLengthValue);
    ESP_LOGI(TAG, "ReserveConnectorZeroSupportedValue : %s", CPGetConfigurationResponse.ReserveConnectorZeroSupportedValue);
    ESP_LOGI(TAG, "ChargeProfileMaxStackLevelValue : %s", CPGetConfigurationResponse.ChargeProfileMaxStackLevelValue);
    ESP_LOGI(TAG, "ChargingScheduleAllowedChargingRateUnitValue : %s", CPGetConfigurationResponse.ChargingScheduleAllowedChargingRateUnitValue);
    ESP_LOGI(TAG, "ChargingScheduleMaxPeriodsValue : %s", CPGetConfigurationResponse.ChargingScheduleMaxPeriodsValue);
    ESP_LOGI(TAG, "ConnectorSwitch3to1PhaseSupportedValue : %s", CPGetConfigurationResponse.ConnectorSwitch3to1PhaseSupportedValue);
    ESP_LOGI(TAG, "MaxChargingProfilesInstalledValue : %s", CPGetConfigurationResponse.MaxChargingProfilesInstalledValue);
    ESP_LOGI(TAG, "----------------------------------------------------------------------");
}

void getTimeInOcppFormat(void)
{
    struct timeval tv;
    struct tm timeinfo;

    gettimeofday(&tv, NULL);
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
}

bool rfidMatchingLocalList(void)
{
    bool matching = false;
    uint8_t i = 0;
    for (i = 0; i < LOCAL_LIST_COUNT; i++)
    {
        if ((strncmp(rfidTagNumberbuf, CMSSendLocalListRequest.localAuthorizationList[i].idTag, strlen(rfidTagNumberbuf)) == 0) && (CMSSendLocalListRequest.localAuthorizationList[i].idTagPresent))
        {
            i = LOCAL_LIST_COUNT;
            matching = true;
        }
    }
    return matching;
}

bool rfidMatchingAuthorizationCache(void)
{
    bool matching = false;
    uint8_t i = 0;
    for (i = 0; i < LOCAL_LIST_COUNT; i++)
    {
        if ((strncmp(rfidTagNumberbuf, LocalAuthorizationList.idTag[i], strlen(rfidTagNumberbuf)) == 0) && (LocalAuthorizationList.idTagPresent[i]))
        {
            i = LOCAL_LIST_COUNT;
            matching = true;
        }
    }
    return matching;
}

void updateMeterValues(uint8_t connId, bool Aligned)
{
    char EnergyValuestr[20];
    char PowerValuestr[20];
    char VoltageValuestr[4][20];
    char CurrentValuestr[4][20];
    char TempValuestr[20];

    setNULL(EnergyValuestr);
    setNULL(PowerValuestr);
    setNULL(VoltageValuestr);
    setNULL(CurrentValuestr);
    setNULL(TempValuestr);
    if (Aligned)
    {
        memcpy(CPMeterValuesRequest[connId].meterValue.sampledValue[1].context, "Sample.Clock", strlen("Sample.Clock"));
        snprintf(EnergyValuestr, sizeof(EnergyValuestr), "%.2lf", ConnectorData[connId].meterStop);
        snprintf(PowerValuestr, sizeof(PowerValuestr), "%.2lf", MeterValueAlignedData[connId].power / 1000);
        if (config.Ac11s || config.Ac22s || config.Ac44s)
        {
            snprintf(VoltageValuestr[PHASE_A], sizeof(VoltageValuestr[PHASE_A]), "%.2lf", MeterValueAlignedData[connId].voltage[PHASE_A]);
            snprintf(CurrentValuestr[PHASE_A], sizeof(CurrentValuestr[PHASE_A]), "%.2lf", MeterValueAlignedData[connId].current[PHASE_A]);
            snprintf(VoltageValuestr[PHASE_B], sizeof(VoltageValuestr[PHASE_B]), "%.2lf", MeterValueAlignedData[connId].voltage[PHASE_B]);
            snprintf(CurrentValuestr[PHASE_B], sizeof(CurrentValuestr[PHASE_B]), "%.2lf", MeterValueAlignedData[connId].current[PHASE_B]);
            snprintf(VoltageValuestr[PHASE_C], sizeof(VoltageValuestr[PHASE_C]), "%.2lf", MeterValueAlignedData[connId].voltage[PHASE_C]);
            snprintf(CurrentValuestr[PHASE_C], sizeof(CurrentValuestr[PHASE_C]), "%.2lf", MeterValueAlignedData[connId].current[PHASE_C]);
        }
        else
        {
            snprintf(VoltageValuestr[connId], sizeof(VoltageValuestr[connId]), "%.2lf", MeterValueAlignedData[connId].voltage[connId]);
            snprintf(CurrentValuestr[connId], sizeof(CurrentValuestr[connId]), "%.2lf", MeterValueAlignedData[connId].current[connId]);
        }
        snprintf(TempValuestr, sizeof(TempValuestr), "%u", MeterValueAlignedData[connId].temp);
        CPMeterValuesRequest[connId].connectorId = connId;
        CPMeterValuesRequest[connId].transactionId = ConnectorData[connId].transactionId;
    }
    else
    {
        memcpy(CPMeterValuesRequest[connId].meterValue.sampledValue[1].context, "Sample.Periodic", strlen("Sample.Periodic"));
        snprintf(EnergyValuestr, sizeof(EnergyValuestr), "%.2lf", ConnectorData[connId].meterStop);
        snprintf(PowerValuestr, sizeof(PowerValuestr), "%.2lf", ConnectorData[connId].meterValue.power / 1000);
        if (config.Ac11s || config.Ac22s || config.Ac44s)
        {
            snprintf(VoltageValuestr[PHASE_A], sizeof(VoltageValuestr[PHASE_A]), "%.2lf", ConnectorData[connId].meterValue.voltage[PHASE_A]);
            snprintf(CurrentValuestr[PHASE_A], sizeof(CurrentValuestr[PHASE_A]), "%.2lf", ConnectorData[connId].meterValue.current[PHASE_A]);
            snprintf(VoltageValuestr[PHASE_B], sizeof(VoltageValuestr[PHASE_B]), "%.2lf", ConnectorData[connId].meterValue.voltage[PHASE_B]);
            snprintf(CurrentValuestr[PHASE_B], sizeof(CurrentValuestr[PHASE_B]), "%.2lf", ConnectorData[connId].meterValue.current[PHASE_B]);
            snprintf(VoltageValuestr[PHASE_C], sizeof(VoltageValuestr[PHASE_C]), "%.2lf", ConnectorData[connId].meterValue.voltage[PHASE_C]);
            snprintf(CurrentValuestr[PHASE_C], sizeof(CurrentValuestr[PHASE_C]), "%.2lf", ConnectorData[connId].meterValue.current[PHASE_C]);
        }
        else
        {
            snprintf(VoltageValuestr[connId], sizeof(VoltageValuestr[connId]), "%.2lf", ConnectorData[connId].meterValue.voltage[connId]);
            snprintf(CurrentValuestr[connId], sizeof(CurrentValuestr[connId]), "%.2lf", ConnectorData[connId].meterValue.current[connId]);
        }
        snprintf(TempValuestr, sizeof(TempValuestr), "%u", ConnectorData[connId].meterValue.temp);
        CPMeterValuesRequest[connId].connectorId = connId;
        CPMeterValuesRequest[connId].transactionId = ConnectorData[connId].transactionId;
    }

    setNULL(time_buffer);
    getTimeInOcppFormat();
    setNULL(CPMeterValuesRequest[connId].meterValue.timestamp);
    setNULL(CPMeterValuesRequest[connId].meterValue.sampledValue[0].value);
    setNULL(CPMeterValuesRequest[connId].meterValue.sampledValue[1].value);
    setNULL(CPMeterValuesRequest[connId].meterValue.sampledValue[2].value);
    setNULL(CPMeterValuesRequest[connId].meterValue.sampledValue[3].value);
    setNULL(CPMeterValuesRequest[connId].meterValue.sampledValue[4].value);
    setNULL(CPMeterValuesRequest[connId].meterValue.sampledValue[5].value);
    setNULL(CPMeterValuesRequest[connId].meterValue.sampledValue[6].value);
    setNULL(CPMeterValuesRequest[connId].meterValue.sampledValue[7].value);
    setNULL(CPMeterValuesRequest[connId].meterValue.sampledValue[8].value);
    memcpy(CPMeterValuesRequest[connId].meterValue.timestamp, time_buffer, strlen(time_buffer));
    memcpy(CPMeterValuesRequest[connId].meterValue.sampledValue[0].value, EnergyValuestr, strlen(EnergyValuestr));
    memcpy(CPMeterValuesRequest[connId].meterValue.sampledValue[1].value, PowerValuestr, strlen(PowerValuestr));
    if (config.Ac11s || config.Ac22s || config.Ac44s)
    {
        memcpy(CPMeterValuesRequest[connId].meterValue.sampledValue[2].value, VoltageValuestr[PHASE_A], strlen(VoltageValuestr[PHASE_A]));
        memcpy(CPMeterValuesRequest[connId].meterValue.sampledValue[3].value, VoltageValuestr[PHASE_B], strlen(VoltageValuestr[PHASE_B]));
        memcpy(CPMeterValuesRequest[connId].meterValue.sampledValue[4].value, VoltageValuestr[PHASE_C], strlen(VoltageValuestr[PHASE_C]));
        memcpy(CPMeterValuesRequest[connId].meterValue.sampledValue[5].value, CurrentValuestr[PHASE_A], strlen(CurrentValuestr[PHASE_A]));
        memcpy(CPMeterValuesRequest[connId].meterValue.sampledValue[6].value, CurrentValuestr[PHASE_B], strlen(CurrentValuestr[PHASE_B]));
        memcpy(CPMeterValuesRequest[connId].meterValue.sampledValue[7].value, CurrentValuestr[PHASE_C], strlen(CurrentValuestr[PHASE_C]));
    }
    else
    {
        memcpy(CPMeterValuesRequest[connId].meterValue.sampledValue[connId + 1].value, VoltageValuestr[connId], strlen(VoltageValuestr[connId]));
        memcpy(CPMeterValuesRequest[connId].meterValue.sampledValue[connId + 4].value, CurrentValuestr[connId], strlen(CurrentValuestr[connId]));
    }

    memcpy(CPMeterValuesRequest[connId].meterValue.sampledValue[8].value, TempValuestr, strlen(TempValuestr));
}

void parse_timestamp(const char *timestamp)
{
    struct tm tm_time = {0};
    sscanf(timestamp, "%d-%d-%dT%d:%d:%d.%*3sZ",
           &tm_time.tm_year, &tm_time.tm_mon, &tm_time.tm_mday,
           &tm_time.tm_hour, &tm_time.tm_min, &tm_time.tm_sec);

    tm_time.tm_year -= 1900; // Adjust year
    tm_time.tm_mon -= 1;     // Adjust month

    unix_time = mktime(&tm_time);
    tv.tv_sec = unix_time;
    settimeofday(&tv, NULL);
}

uint32_t getTimeInSecondsFromTimestamp(const char *timestamp)
{
    struct tm tm_time = {0};
    time_t local_unix_time;
    sscanf(timestamp, "%d-%d-%dT%d:%d:%d.%*3sZ",
           &tm_time.tm_year, &tm_time.tm_mon, &tm_time.tm_mday,
           &tm_time.tm_hour, &tm_time.tm_min, &tm_time.tm_sec);

    tm_time.tm_year -= 1900; // Adjust year
    tm_time.tm_mon -= 1;     // Adjust month

    local_unix_time = mktime(&tm_time);
    return local_unix_time;
}

void processBootedState(uint8_t connId)
{
    static uint32_t timeout[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
    if (CPStatusNotificationRequest[connId].Sent == false)
    {
        setNULL(CPStatusNotificationRequest[connId].errorCode);
        setNULL(CPStatusNotificationRequest[connId].status);
        if ((slavePushButton[3] && config.vcharge_lite_1_4) ||
            (gfciButton && config.vcharge_lite_1_4) ||
            (earthDisconnectButton && config.vcharge_lite_1_4) ||
            (emergencyButton && (config.vcharge_v5 || config.AC1)) ||
            (gfciButton && (config.vcharge_v5 || config.AC1)) ||
            (earthDisconnectButton && (config.vcharge_v5 || config.AC1)))
        {
            CPStatusNotificationRequest[connId].connectorId = connId;
            taskState[connId] = flag_EVSE_is_Booted;
            if (gfciButton)
            {
                memcpy(CPStatusNotificationRequest[connId].errorCode, GroundFailure, strlen(GroundFailure));
            }
            else
            {
                memcpy(CPStatusNotificationRequest[connId].errorCode, OtherError, strlen(OtherError));
            }
            memcpy(CPStatusNotificationRequest[connId].status, Faulted, strlen(Faulted));
        }
        else if ((CMSChangeAvailabilityRequest[connId].Received) && (memcmp(CMSChangeAvailabilityRequest[connId].type, Inoperative, strlen(Inoperative)) == 0))
        {
            taskState[connId] = flag_EVSE_Read_Id_Tag;
            CPStatusNotificationRequest[connId].connectorId = connId;
            memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
            memcpy(CPStatusNotificationRequest[connId].status, Unavailable, strlen(Unavailable));
        }
        else if ((CMSChangeAvailabilityRequest[connId].Received) && (memcmp(CMSChangeAvailabilityRequest[connId].type, Operative, strlen(Operative)) == 0))
        {
            taskState[connId] = flag_EVSE_Read_Id_Tag;
            if (ConnectorData[connId].isReserved)
            {
                CPStatusNotificationRequest[connId].connectorId = connId;
                memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
                memcpy(CPStatusNotificationRequest[connId].status, Reserved, strlen(Reserved));
            }
            else
            {
                CPStatusNotificationRequest[connId].connectorId = connId;
                memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
                if (cpState[connId] == STATE_A)
                {
                    taskState[connId] = flag_EVSE_Read_Id_Tag;
                    memcpy(CPStatusNotificationRequest[connId].status, Available, strlen(Available));
                }
                else if (cpState[connId] == STATE_B1)
                {
                    taskState[connId] = flag_EVSE_Read_Id_Tag;
                    memcpy(CPStatusNotificationRequest[connId].status, Preparing, strlen(Preparing));
                }
            }
        }
        else if (ConnectorData[connId].isReserved)
        {
            taskState[connId] = flag_EVSE_Read_Id_Tag;
            CPStatusNotificationRequest[connId].connectorId = connId;
            memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
            memcpy(CPStatusNotificationRequest[connId].status, Reserved, strlen(Reserved));
        }
        else
        {
            taskState[connId] = flag_EVSE_Read_Id_Tag;
            CPStatusNotificationRequest[connId].connectorId = connId;
            memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
            if (cpState[connId] == STATE_A)
            {
                memcpy(CPStatusNotificationRequest[connId].status, Available, strlen(Available));
            }
            else if (cpState[connId] == STATE_B1)
            {
                memcpy(CPStatusNotificationRequest[connId].status, Preparing, strlen(Preparing));
            }
        }
        if (isWebsocketConnected)
        {
            setNULL(time_buffer);
            getTimeInOcppFormat();
            memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
            sendStatusNotificationRequest(connId);
        }
    }
    if (CMSStatusNotificationResponse[connId].Received == false)
    {
        timeout[connId]++;
    }
    else
    {
        taskState[connId] = flag_EVSE_Read_Id_Tag;
        timeout[connId] = 0;
    }
    if (timeout[connId] > STATUS_TIMEOUT)
    {
        timeout[connId] = 0;
        setNULL(time_buffer);
        getTimeInOcppFormat();
        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
        sendStatusNotificationRequest(connId);
    }
}

void processAuthenticationState(uint8_t connId)
{
    static uint32_t AuthenticationTimeout[NUM_OF_CONNECTORS] = {0, 0, 0, 0};

    if ((cpEnable[connId] == false) && (PushButtonEnabled[connId] == false) && (AuthenticationProcessed == false))
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    if (AuthenticationProcessed)
    {
        updateDisplayOnAcceptanceFailed(connId);
        taskState[connId] = flag_EVSE_Read_Id_Tag;
    }
    if (connectorFault[connId] == true)
    {
        updateDisplayOnAcceptanceFailed(connId);
        taskState[connId] = flag_EVSE_Read_Id_Tag;
    }
    else if ((rfidReceivedFromRemote[connId] ||
              rfidMatchingWithLocalList ||
              rfidAuthenticated ||
              ((RemoteAuthorizeConnId == connId) && RemoteAuthenticated) ||
              ((AlprAuthorizeConnId == connId) && AlprAuthenticated)) &&
             (taskState[connId] == falg_EVSE_Authentication))
    {
        if ((((cpState[connId] == STATE_B1) || (cpState[connId] == STATE_E2)) && (cpEnable[connId] == true)) ||
            ((cpEnable[connId] == false) && (PushButtonEnabled[connId] == false)) ||
            ((PushButtonEnabled[connId] == true) && (PushButton[connId] == true)))
        {
            if (rfidReceivedFromRemote[connId])
                ESP_LOGD(TAG_DEBUG, "RemoteStartTransactionRequested");
            else if (rfidMatchingWithLocalList)
                ESP_LOGD(TAG_DEBUG, "RFID matching with local list");
            else if (rfidAuthenticated)
            {
                CPAuthorizeRequest[connId].Sent = false;
                ESP_LOGD(TAG_DEBUG, "RFID Authenticated");
            }
            AuthenticationTimeout[connId] = 0;
            CPStartTransactionRequest[connId].Sent = false;
            isRfidTappedFirst = false;
            if (rfidMatchingWithLocalList || rfidAuthenticated)
            {
                setNULL(ConnectorData[connId].idTag);
                memcpy(ConnectorData[connId].idTag, ConnectorData[0].idTag, strlen(ConnectorData[0].idTag));
                if (rfidMatchingWithLocalList)
                {
                    ConnectorData[connId].UnkownId = ConnectorData[0].UnkownId;
                    ConnectorData[0].UnkownId = false;
                }
            }
            custom_nvs_write_connector_data(connId);
            PushButton[connId] = false;
            rfidReceivedFromRemote[connId] = false;
            rfidMatchingWithLocalList = false;
            rfidAuthenticated = false;
            RemoteAuthenticated = false;
            AlprAuthenticated = false;
            AuthenticationProcessed = true;
            for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
            {
                processRfidForConnector[i] = false;
            }
            taskState[connId] = flag_EVSE_Start_Transaction;
        }
        else
        {
            AuthenticationTimeout[connId]++;
            isRfidTappedFirst = false;
        }

        if (AuthenticationTimeout[connId] > CPGetConfigurationResponse.ConnectionTimeOut)
        {
            rfidReceivedFromRemote[connId] = false;
            rfidMatchingWithLocalList = false;
            rfidAuthenticated = false;
            RemoteAuthenticated = false;
            AlprAuthenticated = false;
            AuthenticationTimeout[connId] = 0;
            CPRemoteStartTransactionResponse[connId].Sent = false;
            CMSAuthorizeResponse[connId].Received = false;
            CPAuthorizeRequest[connId].Sent = false;
            updateDisplayOnAcceptanceFailed(connId);
            taskState[connId] = flag_EVSE_Read_Id_Tag;
        }
    }
    else
    {
        CPAuthorizeRequest[connId].Sent = false;
        AuthenticationTimeout[connId] = 0;
        updateDisplayOnAcceptanceFailed(connId);
        taskState[connId] = flag_EVSE_Read_Id_Tag;
    }
}

void processRfidTappedState(void)
{
    static uint32_t timeout = 0;
    static uint8_t loopCount = 0;
    uint8_t connId = 0;
    bool OngoingTransaction = false;
    ESP_LOGI(TAG, "RFID TAPPED STATE");
    if ((newRfidRead || RemoteAuthorize || AlprAuthorize) && isWebsocketConnected && isWebsocketBooted)
    {
        OngoingTransaction = false;
        for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
        {
            if (taskState[i] == flag_EVSE_Request_Charge)
            {
                OngoingTransaction = true;
            }
        }
        if ((OngoingTransaction == true) && (config.Ac10DA))
        {
            newRfidRead = false;
            loopCount = 0;
        }
        if (loopCount >= 2)
        {
            if (newRfidRead)
            {
                AllTasksBusy = true;
                for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
                {
                    if (taskState[i] == flag_EVSE_Read_Id_Tag)
                        AllTasksBusy = false;
                }
                if (AllTasksBusy && (config.Ac10t == false))
                    newRfidRead = false;
            }
        }
        if ((OngoingTransaction == true) && (loopCount < 3))
        {
            loopCount++;
        }
        else if (RemoteAuthorize || AlprAuthorize)
        {
            connId = 0;
            loopCount = 0;
            setNULL(CPAuthorizeRequest[connId].idTag);
            setNULL(ConnectorData[connId].idTag);
            if (AlprAuthorize)
            {
                memcpy(CPAuthorizeRequest[connId].idTag, AlprTagNumberbuf, strlen(AlprTagNumberbuf));
                memcpy(ConnectorData[connId].idTag, AlprTagNumberbuf, strlen(AlprTagNumberbuf));
            }
            else
            {
                memcpy(CPAuthorizeRequest[connId].idTag, RemoteTagNumberbuf, strlen(RemoteTagNumberbuf));
                memcpy(ConnectorData[connId].idTag, RemoteTagNumberbuf, strlen(RemoteTagNumberbuf));
            }
            custom_nvs_write_connector_data(connId);
            sendAuthorizationRequest(connId);
            CPRemoteStartTransactionResponse[connId].Sent = false;
            newRfidRead = false;
            RemoteAuthorize = false;
            AlprAuthorize = false;
            timeout = 0;
        }
        else if (((rfidMatchingLocalList() && CPGetConfigurationResponse.LocalAuthListEnabled) ||
                  (rfidMatchingAuthorizationCache() && CPGetConfigurationResponse.AuthorizationCacheEnabled)) &&
                 CPGetConfigurationResponse.LocalPreAuthorize)
        {
            connId = 0;
            CPAuthorizeRequest[connId].Sent = false;
            newRfidRead = false;
            loopCount = 0;
            setNULL(ConnectorData[connId].idTag);
            memcpy(ConnectorData[connId].idTag, rfidTagNumberbuf, strlen(rfidTagNumberbuf));
            ESP_LOGD(TAG, "RFID Matching Setting Auth Success");
            if (isRfidTappedFirst)
                SetChargerAuthSuccessPlugEvBit();
            else
                SetChargerAuthSuccessBit();
            rfidTappedStateProcessed = true;
            rfidMatchingWithLocalList = true;
            for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
            {
                if ((taskState[i] == flag_EVSE_Read_Id_Tag) ||
                    ((cpEnable[i] == false) && PushButtonEnabled[i] && (taskState[i] == flag_EVSE_Request_Charge)))
                {
                    processRfidForConnector[i] = true;
                }
            }
            connId = 0;
            timeout = 0;
        }
        else if (CPAuthorizeRequest[connId].Sent == false)
        {
            connId = 0;
            CPRemoteStartTransactionResponse[connId].Sent = false;
            newRfidRead = false;
            loopCount = 0;
            setNULL(CPAuthorizeRequest[connId].idTag);
            setNULL(ConnectorData[connId].idTag);
            memcpy(CPAuthorizeRequest[connId].idTag, rfidTagNumberbuf, strlen(rfidTagNumberbuf));
            memcpy(ConnectorData[connId].idTag, rfidTagNumberbuf, strlen(rfidTagNumberbuf));
            custom_nvs_write_connector_data(connId);
            sendAuthorizationRequest(connId);
            for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
            {
                if ((taskState[i] == flag_EVSE_Read_Id_Tag) ||
                    ((cpEnable[i] == false) && PushButtonEnabled[i] && (taskState[i] == flag_EVSE_Request_Charge)))
                {
                    processRfidForConnector[i] = true;
                }
            }
            timeout = 0;
        }
    }
    if (isWebsocketConnected && isWebsocketBooted && (CPAuthorizeRequest[connId].Sent == true))
    {
        connId = 0;
        if ((CMSAuthorizeResponse[connId].Received == false) && (CPAuthorizeRequest[connId].Sent == true))
        {
            timeout++;
        }
        else if (memcmp(CMSAuthorizeResponse[connId].idtaginfo.status, ACCEPTED, strlen(ACCEPTED)) == 0)
        {
            timeout = 0;
            ConnectorData[connId].UnkownId = false;
            if (CPGetConfigurationResponse.AuthorizationCacheEnabled &&
                (memcmp(CPAuthorizeRequest[connId].idTag, rfidTagNumberbuf, strlen(rfidTagNumberbuf)) == 0))
                saveTagToLocalAuthenticationList();
            CMSAuthorizeResponse[connId].Received = false;
            CPAuthorizeRequest[connId].Sent = false;
            if (memcmp(CPAuthorizeRequest[connId].idTag, RemoteTagNumberbuf, strlen(RemoteTagNumberbuf)) == 0)
                RemoteAuthenticated = true;
            else if (memcmp(CPAuthorizeRequest[connId].idTag, AlprTagNumberbuf, strlen(AlprTagNumberbuf)) == 0)
                AlprAuthenticated = true;
            else if (memcmp(CPAuthorizeRequest[connId].idTag, rfidTagNumberbuf, strlen(rfidTagNumberbuf)) == 0)
            {
                rfidAuthenticated = true;
                rfidTappedStateProcessed = true;
            }
            if (isRfidTappedFirst)
                SetChargerAuthSuccessPlugEvBit();
            else
                SetChargerAuthSuccessBit();
            connId = 0;
        }
        else
        {
            timeout = 0;
            isRfidTappedFirst = false;
            SetChargerAuthFailedBit();
            vTaskDelay(100 / portTICK_PERIOD_MS);
            updateDisplayOnAcceptanceFailed(1);
            CMSAuthorizeResponse[connId].Received = false;
            CPAuthorizeRequest[connId].Sent = false;
        }
        if (timeout > AUTH_TIMEOUT)
        {
            timeout = 0;
            sendAuthorizationRequest(connId);
        }
    }
    if ((config.onlineOffline || config.offline) && (isWebsocketConnected == false))
    {
        for (uint8_t i = 1; i < (config.NumberOfConnectors + 1); i++)
        {
            if (ConnectorData[i].isTransactionOngoing)
            {
                OngoingTransaction = true;
            }
        }
        if ((OngoingTransaction == true) && (loopCount < 2))
        {
            loopCount++;
        }
        else if (newRfidRead)
        {
            connId = 0;
            CPAuthorizeRequest[connId].Sent = false;
            if (config.infra_charger)
            {
                newRfidRead = false;
                loopCount = 0;
                setNULL(ConnectorData[connId].idTag);
                memcpy(ConnectorData[connId].idTag, rfidTagNumberbuf, strlen(rfidTagNumberbuf));
                custom_nvs_write_connector_data(connId);
                rfidTappedStateProcessed = true;
                if (isRfidTappedFirst)
                    SetChargerAuthSuccessPlugEvBit();
                else
                    SetChargerAuthSuccessBit();
                for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
                {
                    if ((taskState[i] == flag_EVSE_Read_Id_Tag) ||
                        ((cpEnable[i] == false) && PushButtonEnabled[i] && (taskState[i] == flag_EVSE_Request_Charge)))
                    {
                        processRfidForConnector[i] = true;
                    }
                }
                rfidMatchingWithLocalList = true;
                connId = 0;
            }
            if (config.sales_charger)
            {
                if (((rfidMatchingLocalList() && CPGetConfigurationResponse.LocalAuthListEnabled) ||
                     (rfidMatchingAuthorizationCache() && CPGetConfigurationResponse.AuthorizationCacheEnabled)) &&
                    CPGetConfigurationResponse.LocalAuthorizeOffline)
                {
                    newRfidRead = false;
                    loopCount = 0;
                    ConnectorData[connId].UnkownId = false;
                    setNULL(ConnectorData[connId].idTag);
                    memcpy(ConnectorData[connId].idTag, rfidTagNumberbuf, strlen(rfidTagNumberbuf));
                    custom_nvs_write_connector_data(connId);
                    rfidTappedStateProcessed = true;
                    if (isRfidTappedFirst)
                        SetChargerAuthSuccessPlugEvBit();
                    else
                        SetChargerAuthSuccessBit();
                    for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
                    {
                        if ((taskState[i] == flag_EVSE_Read_Id_Tag) ||
                            ((cpEnable[i] == false) && PushButtonEnabled[i] && (taskState[i] == flag_EVSE_Request_Charge)))
                        {
                            processRfidForConnector[i] = true;
                        }
                    }
                    rfidMatchingWithLocalList = true;
                    connId = 0;
                }
                else if (CPGetConfigurationResponse.AllowOfflineTxForUnknownId)
                {
                    newRfidRead = false;
                    loopCount = 0;
                    ConnectorData[connId].UnkownId = true;
                    setNULL(ConnectorData[connId].idTag);
                    memcpy(ConnectorData[connId].idTag, rfidTagNumberbuf, strlen(rfidTagNumberbuf));
                    custom_nvs_write_connector_data(connId);
                    rfidTappedStateProcessed = true;
                    if (isRfidTappedFirst)
                        SetChargerAuthSuccessPlugEvBit();
                    else
                        SetChargerAuthSuccessBit();
                    for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
                    {
                        if (taskState[i] == flag_EVSE_Read_Id_Tag)
                        {
                            processRfidForConnector[i] = true;
                        }
                    }
                    rfidMatchingWithLocalList = true;
                    connId = 0;
                }
                else
                {
                    ConnectorData[connId].UnkownId = false;
                    rfidTappedStateProcessed = true;
                    isRfidTappedFirst = false;
                    newRfidRead = false;
                    SetChargerAuthFailedBit();
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    updateDisplayOnAcceptanceFailed(1);
                }
            }
        }
    }
}

void processRfidState(uint8_t connId)
{
    if (rfidReceivedFromRemote[connId] || ((rfidMatchingWithLocalList || rfidAuthenticated) && (processRfidForConnector[connId])) || ((RemoteAuthorizeConnId == connId) && RemoteAuthenticated) ||
        ((AlprAuthorizeConnId == connId) && AlprAuthenticated))
    {
        if (rfidMatchingWithLocalList || rfidAuthenticated)
            processRfidForConnector[connId] = false;

        if (ConnectorData[connId].CmsAvailable == false)
        {
            updateDisplayOnAcceptanceFailed(connId);
        }
        else if ((ConnectorData[connId].isReserved) &&
                 (strncmp(ConnectorData[connId].ReservedData.idTag, rfidTagNumberbuf, strlen(rfidTagNumberbuf)) == 0))
        {
            ConnectorData[connId].isReserved = false;
            AuthenticationProcessed = false;
            taskState[connId] = falg_EVSE_Authentication;
        }
        else if ((ConnectorData[connId].isReserved) &
                 (strncmp(ConnectorData[connId].ReservedData.idTag, rfidTagNumberbuf, strlen(rfidTagNumberbuf)) != 0))
        {
            updateDisplayOnAcceptanceFailed(connId);
            ESP_LOGI(TAG, "Connector %d Reserved for Others", connId);
        }
        else
        {
            CPAuthorizeRequest[connId].Sent = false;
            AuthenticationProcessed = false;
            taskState[connId] = falg_EVSE_Authentication;
        }
    }
}

void processStartTransactionState(uint8_t connId)
{
    static uint32_t timeout[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
    static uint32_t retryCount[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
    if (isWebsocketConnected && isWebsocketBooted)
    {
        if (CPStartTransactionRequest[connId].Sent == false)
        {
            CPStartTransactionRequest[connId].connectorId = connId;
            memcpy(CPStartTransactionRequest[connId].idTag, ConnectorData[connId].idTag, sizeof(ConnectorData[connId].idTag));
            CPStartTransactionRequest[connId].meterStart = (int)(ConnectorData[connId].meterStop);
            CPStartTransactionRequest[connId].reservationIdPresent = false;
            setNULL(time_buffer);
            getTimeInOcppFormat();
            memcpy(CPStartTransactionRequest[connId].timestamp, time_buffer, strlen(time_buffer));
            memcpy(ConnectorData[connId].meterStart_time, time_buffer, strlen(time_buffer));
            sendStartTransactionRequest(connId);
            retryCount[connId] = 0;
            timeout[connId] = 0;
            ChargingFaultTimeCount[connId] = 0;
        }
    }
    else
    {
        setNULL(ConnectorData[connId].meterStart_time);
        setNULL(time_buffer);
        getTimeInOcppFormat();
        memcpy(ConnectorData[connId].meterStart_time, time_buffer, strlen(time_buffer));
        ChargingFaultTimeCount[connId] = 0;
        taskState[connId] = flag_EVSE_Request_Charge;
        ConnectorData[connId].meterStart = ConnectorData[connId].meterStop;
        CMSStartTransactionResponse[connId].Received = false;
        CPRemoteStartTransactionResponse[connId].Sent = false;
        CPStartTransactionRequest[connId].Sent = false;

        if (ConnectorData[connId].isTransactionOngoing == false)
        {
            ConnectorData[connId].offlineStarted = (isWebsocketConnected & isWebsocketBooted) ? false : true;
            ConnectorData[connId].isTransactionOngoing = true;
            ConnectorData[connId].chargingDuration = 0;
            ConnectorData[connId].meterStart = ConnectorData[connId].meterStop;
            ConnectorData[connId].Energy = 0;
            custom_nvs_write_connector_data(connId);
            memset(&MeterValueAlignedData[connId], 0, sizeof(MeterValueAlignedData[connId]));
        }
    }

    if (isWebsocketConnected & isWebsocketBooted)
    {
        if ((CMSStartTransactionResponse[connId].Received == false) && (CPStartTransactionRequest[connId].Sent == true))
        {
            timeout[connId]++;
        }
        else if (memcmp(CMSStartTransactionResponse[connId].idtaginfo.status, ACCEPTED, strlen(ACCEPTED)) == 0)
        {
            ConnectorData[connId].transactionId = CMSStartTransactionResponse[connId].transactionId;
            timeout[connId] = 0;
            CMSStartTransactionResponse[connId].Received = false;
            CPStartTransactionRequest[connId].Sent = false;
            ConnectorData[connId].meterStart = ConnectorData[connId].meterStop;
            taskState[connId] = flag_EVSE_Request_Charge;

            if (ConnectorData[connId].isTransactionOngoing == false)
            {
                ConnectorData[connId].offlineStarted = (isWebsocketConnected & isWebsocketBooted) ? false : true;
                ConnectorData[connId].isTransactionOngoing = true;
                ConnectorData[connId].chargingDuration = 0;
                ConnectorData[connId].meterStart = ConnectorData[connId].meterStop;
                ConnectorData[connId].Energy = 0;
                custom_nvs_write_connector_data(connId);
                memset(&MeterValueAlignedData[connId], 0, sizeof(MeterValueAlignedData[connId]));
            }
        }
        else
        {
            timeout[connId] = 0;
            CMSStartTransactionResponse[connId].Received = false;
            CPStartTransactionRequest[connId].Sent = false;
            CPRemoteStartTransactionResponse[connId].Sent = false;
            CPAuthorizeRequest[connId].Sent = false;
            taskState[connId] = flag_EVSE_Read_Id_Tag;
        }

        if (timeout[connId] > CPGetConfigurationResponse.TransactionMessageRetryInterval)
        {
            timeout[connId] = 0;
            retryCount[connId]++;
            if (retryCount[connId] > CPGetConfigurationResponse.TransactionMessageAttempts)
            {
                taskState[connId] = flag_EVSE_Read_Id_Tag;
            }
            CMSStartTransactionResponse[connId].Received = false;
            CPStartTransactionRequest[connId].Sent = false;
            CPRemoteStartTransactionResponse[connId].Sent = false;
            CPAuthorizeRequest[connId].Sent = false;
        }
    }
}

void processStopTransactionState(uint8_t connId)
{
    static uint32_t timeout[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
    static uint32_t retryCount[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
    isConnectorCharging[connId] = false;
    ConnectorData[connId].isTransactionOngoing = false;
    if (isWebsocketConnected & isWebsocketBooted)
    {
        if (CPStopTransactionRequest[connId].Sent == false)
        {
            setNULL(CPStopTransactionRequest[connId].idTag);
            setNULL(time_buffer);
            setNULL(CPStopTransactionRequest[connId].reason);
            getTimeInOcppFormat();
            memcpy(CPStopTransactionRequest[connId].idTag, ConnectorData[connId].idTag, sizeof(ConnectorData[connId].idTag));
            memcpy(ConnectorData[connId].meterStop_time, time_buffer, strlen(time_buffer));
            CPStopTransactionRequest[connId].meterStop = (int)ConnectorData[connId].meterStop;
            memcpy(CPStopTransactionRequest[connId].timestamp, time_buffer, strlen(time_buffer));
            CPStopTransactionRequest[connId].transactionId = (int)ConnectorData[connId].transactionId;
            updateStopStansactionReason(connId);
            sendStopTransactionRequest(connId, TransactionEnd, SAMPLED_DATA, ONLINE_TRANSACTION);
            retryCount[connId] = 0;
            if (connectorFault[connId] == 0)
                finishingStatusSent[connId] = true;
            else
                finishingStatusPending[connId] = true;
            timeout[connId] = 0;
        }
    }
    else
    {
        setNULL(ConnectorData[connId].meterStop_time);
        setNULL(time_buffer);
        getTimeInOcppFormat();
        memcpy(ConnectorData[connId].meterStop_time, time_buffer, strlen(time_buffer));
        custom_nvs_write_connectors_offline_data(connId);
        if (connectorFault[connId] == 0)
        {
            finishingStatusSent[connId] = true;
            SetChargerFinishingdBit(connId);
            if (config.onlineOffline)
                ledState[connId] = OFFLINE_FINISHING_LED_STATE;
            else if (config.offline)
                ledState[connId] = OFFLINE_ONLY_FINISHING_LED_STATE;
            else
                ledState[connId] = ONLINE_ONLY_OFFLINE_FINISHING_LED_STATE;
        }
        else
            finishingStatusPending[connId] = true;

        ESP_LOGI(TAG_STATUS, "Connector %d Finishing", connId);
        taskState[connId] = flag_EVSE_Read_Id_Tag;
        logConnectorData(connId);
        ConnectorData[connId].InvalidId = false;
        ConnectorData[connId].UnkownId = false;
        if (ConnectorData[connId].meterStop > METER_VALUE_RESET)
        {
            ConnectorData[connId].meterStop = 0;
            custom_nvs_write_connector_data(connId);
        }
        else
        {
            custom_nvs_write_connector_data(connId);
        }
    }
    if (isWebsocketConnected & isWebsocketBooted)
    {
        if ((CMSStopTransactionResponse[connId].Received == false) && (CPStopTransactionRequest[connId].Sent == true))
        {
            timeout[connId]++;
        }
        else
        {
            ConnectorData[connId].isTransactionOngoing = false;
            custom_nvs_write_connector_data(connId);
            timeout[connId] = 0;
            CMSStopTransactionResponse[connId].Received = false;
            CPStopTransactionRequest[connId].Sent = false;
            ConnectorData[connId].InvalidId = false;
            ConnectorData[connId].UnkownId = false;
            taskState[connId] = flag_EVSE_Read_Id_Tag;
            logConnectorData(connId);
            if (ConnectorData[connId].meterStop > METER_VALUE_RESET)
            {
                ConnectorData[connId].meterStop = 0;
                custom_nvs_write_connector_data(connId);
            }
            else
            {
                custom_nvs_write_connector_data(connId);
            }
            if (connectorFault[connId] == 0)
            {
                finishingStatusSent[connId] = true;
                CPStatusNotificationRequest[connId].connectorId = connId;
                setNULL(CPStatusNotificationRequest[connId].errorCode);
                setNULL(CPStatusNotificationRequest[connId].status);
                memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
                memcpy(CPStatusNotificationRequest[connId].status, Finishing, strlen(Finishing));
                setNULL(time_buffer);
                getTimeInOcppFormat();
                memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                sendStatusNotificationRequest(connId);
                SetChargerFinishingdBit(connId);
                if (config.onlineOffline)
                    ledState[connId] = FINISHING_LED_STATE;
                else
                    ledState[connId] = ONLINE_ONLY_FINISHING_LED_STATE;
            }
            else
                finishingStatusPending[connId] = true;

            ESP_LOGI(TAG_STATUS, "Connector %d Finishing", connId);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
        if (timeout[connId] > CPGetConfigurationResponse.TransactionMessageRetryInterval)
        {
            timeout[connId] = 0;
            retryCount[connId]++;
            if (retryCount[connId] > CPGetConfigurationResponse.TransactionMessageAttempts)
            {
                // taskState[connId] = flag_EVSE_Read_Id_Tag;
            }
            sendStopTransactionRequest(connId, TransactionEnd, SAMPLED_DATA, ONLINE_TRANSACTION);
        }
    }
}

void processChargeState(uint8_t connId)
{
    static uint32_t timeout[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
    static uint32_t loopCount[NUM_OF_CONNECTORS] = {0, 0, 0, 0};

    if ((cpEnable[connId] == false) & (relayState[connId] == false) && (connectorFault[connId] == 0))
    {
        if ((config.vcharge_v5 == false) && (config.AC1 == false))
        {
            SlaveUpdateRelay(connId, 1);
        }
    }

    if (((cpState[connId] == STATE_B1) || (cpState[connId] == STATE_E2)) && (cpEnable[connId] == true) && (connectorFault[connId] == 0))
    {
        if (config.vcharge_v5 || config.AC1)
        {
            xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
            SetControlPilotDuty(config.cpDuty[connId]);
            xSemaphoreGive(mutexControlPilot);
        }
        else
        {
            ESP_LOGI(TAG, "state b2 request sent");
            SlaveSetControlPilotDuty(connId, config.cpDuty[connId]);
        }
    }
    else if (((((cpState[connId] == STATE_B1) || (cpState[connId] == STATE_E2) || (cpState[connId] == STATE_A) || (cpState[connId] == STATE_DIS)) &&
               (cpEnable[connId] == true)) ||
              (cpEnable[connId] == false)) &&
             (connectorFault[connId] == 1))
    {
        isConnectorCharging[connId] = false;

        if ((config.restoreSession == true) && (MeterFaults[connId].MeterPowerLoss == 1))
        {
            ESP_LOGI(TAG, "Waiting for power to resume the session");
        }
        else if ((config.restoreSession == false) ||
                 (MeterFaults[connId].MeterOverCurrent == 1) ||
                 (gfciButton == 1))
        {
            updateStopReason(connId);
            taskState[connId] = flag_EVSE_Stop_Transaction;
        }
        else
        {
            ChargingFaultTimeCount[connId]++;
            ESP_LOGI(TAG, "Connector %hhu Charging Fault Time %d", connId, ChargingFaultTimeCount[connId]);
            if (ChargingFaultTimeCount[connId] > config.sessionRestoreTime)
            {
                updateStopReason(connId);
                taskState[connId] = flag_EVSE_Stop_Transaction;
            }
        }
    }
    else if (((cpState[connId] == STATE_A) || (cpState[connId] == STATE_DIS)) && (cpEnable[connId] == true) && (connectorFault[connId] == 0))
    {
        ConnectorData[connId].stopReason = Stop_EVDisconnected;
        taskState[connId] = flag_EVSE_Stop_Transaction;
    }
    else if ((((cpState[connId] == STATE_B2) && (cpEnable[connId] == true)) ||
              ((cpEnable[connId] == false) && (config.minLowCurrentThreshold > ConnectorData[connId].meterValue.current[connId]))) &&
             (ConnectorData[connId].isTransactionOngoing))
    {
        isConnectorCharging[connId] = true;
        ChargingSuspendedTimeCount[connId]++;
        if (((ChargingSuspendedTimeCount[connId] > (config.StopTransactionInSuspendedStateTime)) &&
             (cpEnable[connId] == true) &&
             config.StopTransactionInSuspendedState) ||
            ((ChargingSuspendedTimeCount[connId] > config.lowCurrentTime) &&
             (cpEnable[connId] == false)))
        {
            ChargingSuspendedTimeCount[connId] = 0;
            ConnectorData[connId].stopReason = Stop_EVDisconnected;
            taskState[connId] = flag_EVSE_Stop_Transaction;
        }
    }

    if ((ConnectorData[connId].UnkownId &&
         (CPGetConfigurationResponse.MaxEnergyOnInvalidId <= ((uint32_t)ConnectorData[connId].Energy)) &&
         ConnectorData[connId].isTransactionOngoing) ||
        (ConnectorData[connId].InvalidId && CPGetConfigurationResponse.StopTransactionOnInvalidId))
    {
        ConnectorData[connId].InvalidId = false;
        ConnectorData[connId].stopReason = Stop_DeAuthorized;
        taskState[connId] = flag_EVSE_Stop_Transaction;
    }

    if (((((cpState[connId] == STATE_C) || (cpState[connId] == STATE_D)) && (cpEnable[connId] == true)) || (cpEnable[connId] == false)) &&
        (connectorChargingStatusSent[connId] == true) &&
        (taskState[connId] == flag_EVSE_Request_Charge) &&
        (connectorFault[connId] == 0))
    {
        isConnectorCharging[connId] = true;
        ChargingFaultTimeCount[connId] = 0;
        if (cpEnable[connId] == true)
            ChargingSuspendedTimeCount[connId] = 0;
        ConnectorData[connId].chargingDuration++;
        ESP_LOGD(TAG, "Charging Time %ld", ConnectorData[connId].chargingDuration);
        if (config.smartCharging && (config.cpDuty[connId] != cpDutyCycleToSet[connId]))
        {
            changeDutyCycle(connId);
        }
        // if ((ConnectorData[connId].chargingDuration % 30 == 0) && (cpEnable[connId] == true))
        // {
        //     changeDutyCycle(connId);
        // }

        if (loopCount[connId] % MeterValueSampleTime == 0)
        {
            ConnectorData[connId].meterValue = readEnrgyMeter(connId);
            ConnectorData[connId].meterStop = ConnectorData[connId].meterStop + ((ConnectorData[connId].meterValue.power * MeterValueSampleTime) / 3600.0);
            ConnectorData[connId].Energy = ConnectorData[connId].meterStop - ConnectorData[connId].meterStart;
            custom_nvs_write_connector_data(connId);
            if (CPGetConfigurationResponse.ClockAlignedDataInterval != 0)
            {
                MeterValueAlignedData[connId].temp = (MeterValueAlignedData[connId].temp * ClockAlignedDataCount[connId] + ConnectorData[connId].meterValue.temp) / (ClockAlignedDataCount[connId] + 1);
                if (config.Ac11s || config.Ac22s || config.Ac44s)
                {
                    MeterValueAlignedData[connId].voltage[PHASE_A] = (MeterValueAlignedData[connId].voltage[PHASE_A] * ClockAlignedDataCount[connId] + ConnectorData[connId].meterValue.voltage[PHASE_A]) / (ClockAlignedDataCount[connId] + 1);
                    MeterValueAlignedData[connId].current[PHASE_A] = (MeterValueAlignedData[connId].current[PHASE_A] * ClockAlignedDataCount[connId] + ConnectorData[connId].meterValue.current[PHASE_A]) / (ClockAlignedDataCount[connId] + 1);
                    MeterValueAlignedData[connId].voltage[PHASE_B] = (MeterValueAlignedData[connId].voltage[PHASE_B] * ClockAlignedDataCount[connId] + ConnectorData[connId].meterValue.voltage[PHASE_B]) / (ClockAlignedDataCount[connId] + 1);
                    MeterValueAlignedData[connId].current[PHASE_B] = (MeterValueAlignedData[connId].current[PHASE_B] * ClockAlignedDataCount[connId] + ConnectorData[connId].meterValue.current[PHASE_B]) / (ClockAlignedDataCount[connId] + 1);
                    MeterValueAlignedData[connId].voltage[PHASE_C] = (MeterValueAlignedData[connId].voltage[PHASE_C] * ClockAlignedDataCount[connId] + ConnectorData[connId].meterValue.voltage[PHASE_C]) / (ClockAlignedDataCount[connId] + 1);
                    MeterValueAlignedData[connId].current[PHASE_C] = (MeterValueAlignedData[connId].current[PHASE_C] * ClockAlignedDataCount[connId] + ConnectorData[connId].meterValue.current[PHASE_C]) / (ClockAlignedDataCount[connId] + 1);
                }
                else
                {
                    MeterValueAlignedData[connId].voltage[connId] = (MeterValueAlignedData[connId].voltage[connId] * ClockAlignedDataCount[connId] + ConnectorData[connId].meterValue.voltage[connId]) / (ClockAlignedDataCount[connId] + 1);
                    MeterValueAlignedData[connId].current[connId] = (MeterValueAlignedData[connId].current[connId] * ClockAlignedDataCount[connId] + ConnectorData[connId].meterValue.current[connId]) / (ClockAlignedDataCount[connId] + 1);
                }
                MeterValueAlignedData[connId].power = (MeterValueAlignedData[connId].power * ClockAlignedDataCount[connId] + ConnectorData[connId].meterValue.power) / (ClockAlignedDataCount[connId] + 1);
                MeterValueAlignedData[connId].Energy = MeterValueAlignedData[connId].Energy + ConnectorData[connId].Energy;
                ClockAlignedDataCount[connId]++;
            }
        }
        if (isWebsocketConnected & isWebsocketBooted)
        {
            if (ClockAlignedDataTime)
            {
                updateMeterValues(connId, ALIGNED_DATA);
                if (updatingDataToCms[connId] == false)
                    sendMeterValuesRequest(connId, SampleClock, ALIGNED_DATA);
                ClockAlignedDataCount[connId] = 0;
            }
            if (CPMeterValuesRequest[connId].Sent == false)
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
                updateMeterValues(connId, SAMPLED_DATA);
                if (updatingDataToCms[connId] == false)
                    sendMeterValuesRequest(connId, SamplePeriodic, SAMPLED_DATA);
                timeout[connId] = 0;
            }
        }
        if (isWebsocketConnected & isWebsocketBooted)
        {
            if ((CMSMeterValuesResponse[connId].Received == false) && (CPMeterValuesRequest[connId].Sent == true))
            {
                timeout[connId]++;
            }
            else
            {
                timeout[connId] = 0;
                // taskState[connId] = flag_EVSE_Request_Charge;
            }
            if (timeout[connId] > START_TRANSACTION_TIMEOUT)
            {
                timeout[connId] = 0;
                vTaskDelay(100 / portTICK_PERIOD_MS);
                updateMeterValues(connId, SAMPLED_DATA);
                if (updatingDataToCms[connId] == false)
                    sendMeterValuesRequest(connId, SamplePeriodic, SAMPLED_DATA);
            }
        }
        if ((loopCount[connId] % 30 == 0) && (isWebsocketConnected == false))
        {
            ESP_LOGI(TAG, "Con-%hhu-> Energy %f, Power %f, Voltage %f, Current %f",
                     connId,
                     ConnectorData[connId].Energy,
                     ConnectorData[connId].meterValue.power,
                     ConnectorData[connId].meterValue.voltage[connId],
                     ConnectorData[connId].meterValue.current[connId]);
        }

        loopCount[connId]++;
        if (loopCount[connId] % CPGetConfigurationResponse.MeterValueSampleInterval == 0)
        {
            CPMeterValuesRequest[connId].Sent = false;
        }

        if ((newRfidRead || (processRfidForConnector[connId] && (cpEnable[connId] == false) && PushButtonEnabled[connId])) &&
            ((cpEnable[connId]) ||
             ((cpEnable[connId] == false) &&
              ((PushButtonEnabled[connId] == false) ||
               (PushButtonEnabled[connId] && PushButton[connId])))))
        {

            if (memcmp(rfidTagNumberbuf, ConnectorData[connId].idTag, strlen(rfidTagNumberbuf)) == 0)
            {
                if (processRfidForConnector[connId] && (cpEnable[connId] == false) && PushButtonEnabled[connId] && PushButton[connId])
                {
                    AuthenticationProcessed = true;
                    for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
                    {
                        processRfidForConnector[i] = false;
                    }
                }
                ConnectorData[connId].stopReason = Stop_Local;
                newRfidRead = false;
                CMSMeterValuesResponse[connId].Received = false;
                CPMeterValuesRequest[connId].Sent = false;
                taskState[connId] = flag_EVSE_Stop_Transaction;
            }
        }
    }
}

void sendOngoigTransactionsData(uint8_t connId)
{
    uint32_t timeout = 0;
    ESP_LOGI(TAG, "Sending Connector %hhu Ongoing transaction data", connId);
    if (ConnectorData[connId].offlineStarted)
    {
        // send start tansaction
        CPStartTransactionRequest[connId].Sent = false;
        CPStartTransactionRequest[connId].connectorId = connId;
        memcpy(CPStartTransactionRequest[connId].idTag, ConnectorData[connId].idTag, sizeof(ConnectorData[connId].idTag));
        CPStartTransactionRequest[connId].meterStart = (int)(ConnectorData[connId].meterStart);
        CPStartTransactionRequest[connId].reservationIdPresent = false;
        setNULL(CPStartTransactionRequest[connId].timestamp);
        memcpy(CPStartTransactionRequest[connId].timestamp, ConnectorData[connId].meterStart_time, strlen(ConnectorData[connId].meterStart_time));
        sendStartTransactionRequest(connId);
        timeout = 0;
        CMSStartTransactionResponse[connId].Received = false;
        while (CMSStartTransactionResponse[connId].Received == false)
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            timeout++;
            if (timeout > 100)
            {
                timeout = 0;
                sendStartTransactionRequest(connId);
            }
        }
        CMSStartTransactionResponse[connId].Received = false;
        CPStartTransactionRequest[connId].Sent = false;

        if (memcmp(CMSStartTransactionResponse[connId].idtaginfo.status, ACCEPTED, strlen(ACCEPTED)) == 0)
        {
            ConnectorData[connId].transactionId = CMSStartTransactionResponse[connId].transactionId;
            ConnectorData[connId].offlineStarted = false;
            custom_nvs_write_connector_data(connId);
            // send charging status

            if (((config.resumeSession == false) || (WebsocketRestart == true) || (((cpState[connId] != STATE_B1) || (cpState[connId] != STATE_E2)) && (cpEnable[connId] == true))) & (isPoweredOn == true))
            {
                CPStatusNotificationRequest[connId].connectorId = connId;
                setNULL(CPStatusNotificationRequest[connId].errorCode);
                setNULL(CPStatusNotificationRequest[connId].status);
                memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
                memcpy(CPStatusNotificationRequest[connId].status, Charging, strlen(Charging));
                setNULL(time_buffer);
                getTimeInOcppFormat();
                memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                sendStatusNotificationRequest(connId);

                // send metervalue
                updateMeterValues(connId, SAMPLED_DATA);
                sendMeterValuesRequest(connId, SamplePeriodic, SAMPLED_DATA);
                // send stop transaction
                setNULL(CPStopTransactionRequest[connId].idTag);
                setNULL(CPStopTransactionRequest[connId].timestamp);
                setNULL(CPStopTransactionRequest[connId].reason);
                memcpy(CPStopTransactionRequest[connId].idTag, ConnectorData[connId].idTag, sizeof(ConnectorData[connId].idTag));
                CPStopTransactionRequest[connId].meterStop = (int)ConnectorData[connId].meterStop;
                memcpy(CPStopTransactionRequest[connId].timestamp, ConnectorData[connId].meterStop_time, strlen(ConnectorData[connId].meterStop_time));
                if (ConnectorData[connId].stopReason != Stop_HardReset)
                {
                    if ((cpState[connId] != STATE_B1) && (cpEnable[connId] == true))
                    {
                        ConnectorData[connId].stopReason = Stop_EVDisconnected;
                    }
                    else if (isPoweredOn)
                    {
                        ConnectorData[connId].stopReason = Stop_PowerLoss;
                    }
                }
                updateStopStansactionReason(connId);
                CPStopTransactionRequest[connId].transactionId = (int)CMSStartTransactionResponse[connId].transactionId;
                sendStopTransactionRequest(connId, TransactionEnd, SAMPLED_DATA, ONLINE_TRANSACTION);
                timeout = 0;
                while (CMSStopTransactionResponse[connId].Received == false)
                {
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    timeout++;
                    if (timeout > 100)
                    {
                        timeout = 0;
                        sendStopTransactionRequest(connId, TransactionEnd, SAMPLED_DATA, ONLINE_TRANSACTION);
                    }
                }
                CMSStopTransactionResponse[connId].Received = false;
                CPStopTransactionRequest[connId].Sent = false;

                // send Finishing status
                CPStatusNotificationRequest[connId].connectorId = connId;
                setNULL(CPStatusNotificationRequest[connId].errorCode);
                setNULL(CPStatusNotificationRequest[connId].status);
                memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
                memcpy(CPStatusNotificationRequest[connId].status, Finishing, strlen(Finishing));
                setNULL(time_buffer);
                getTimeInOcppFormat();
                memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                sendStatusNotificationRequest(connId);

                ConnectorData[connId].isTransactionOngoing = false;
                custom_nvs_write_connector_data(connId);
            }
            else
            {
                if (isPoweredOn == true)
                    taskState[connId] = flag_EVSE_Request_Charge;
            }
        }
        else
        {
            ConnectorData[connId].InvalidId = true;
            ConnectorData[connId].isTransactionOngoing = false;
            custom_nvs_write_connector_data(connId);
        }
    }
    else
    {
        if (((config.resumeSession == false) || (WebsocketRestart == true) || (((cpState[connId] != STATE_B1) || (cpState[connId] != STATE_E2)) && (cpEnable[connId] == true))) && (isPoweredOn == true))
        {
            // send stop transaction
            setNULL(CPStopTransactionRequest[connId].idTag);
            setNULL(time_buffer);
            setNULL(CPStopTransactionRequest[connId].reason);
            getTimeInOcppFormat();
            memcpy(CPStopTransactionRequest[connId].idTag, ConnectorData[connId].idTag, sizeof(ConnectorData[connId].idTag));
            CPStopTransactionRequest[connId].meterStop = (int)ConnectorData[connId].meterStop;
            memcpy(CPStopTransactionRequest[connId].timestamp, time_buffer, strlen(time_buffer));
            if (ConnectorData[connId].stopReason != Stop_HardReset)
            {
                if (((cpState[connId] != STATE_B1) || (cpState[connId] != STATE_E2)) && (cpEnable[connId] == true))
                {
                    ConnectorData[connId].stopReason = Stop_EVDisconnected;
                }
                else if (isPoweredOn)
                {
                    ConnectorData[connId].stopReason = Stop_PowerLoss;
                }
            }

            updateStopStansactionReason(connId);
            updateMeterValues(connId, SAMPLED_DATA);

            CPStopTransactionRequest[connId].transactionId = (int)ConnectorData[connId].transactionId;
            sendStopTransactionRequest(connId, TransactionEnd, SAMPLED_DATA, ONLINE_TRANSACTION);
            timeout = 0;
            while (CMSStopTransactionResponse[connId].Received == false)
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
                timeout++;
                if (timeout > 100)
                {
                    timeout = 0;
                    sendStopTransactionRequest(connId, TransactionEnd, SAMPLED_DATA, ONLINE_TRANSACTION);
                }
            }
            CMSStopTransactionResponse[connId].Received = false;
            CPStopTransactionRequest[connId].Sent = false;

            // send Finishing status
            CPStatusNotificationRequest[connId].connectorId = connId;
            setNULL(CPStatusNotificationRequest[connId].errorCode);
            setNULL(CPStatusNotificationRequest[connId].status);
            memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
            memcpy(CPStatusNotificationRequest[connId].status, Finishing, strlen(Finishing));
            setNULL(time_buffer);
            getTimeInOcppFormat();
            memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
            sendStatusNotificationRequest(connId);

            ConnectorData[connId].isTransactionOngoing = false;
            custom_nvs_write_connector_data(connId);
        }
        else
        {
            if (isPoweredOn == true)
                taskState[connId] = flag_EVSE_Request_Charge;
        }
    }
}

void sendOfflineTransactionsData(uint8_t connId)
{
    uint32_t timeout = 0;
    updatingDataToCms[connId] = true;
    ESP_LOGI(TAG, "Sending Connector %hhu offline data", connId);
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
        if (ConnectorOfflineData[connId].offlineStarted)
        {
            // send start tansaction
            CPStartTransactionRequest[connId].Sent = false;
            CPStartTransactionRequest[connId].connectorId = connId;
            memcpy(CPStartTransactionRequest[connId].idTag, ConnectorOfflineData[connId].idTag, sizeof(ConnectorOfflineData[connId].idTag));
            CPStartTransactionRequest[connId].meterStart = (int)(ConnectorOfflineData[connId].meterStart);
            CPStartTransactionRequest[connId].reservationIdPresent = false;

            setNULL(CPStartTransactionRequest[connId].timestamp);
            memcpy(CPStartTransactionRequest[connId].timestamp, ConnectorOfflineData[connId].meterStart_time, strlen(ConnectorOfflineData[connId].meterStart_time));
            sendStartTransactionRequest(connId);
            timeout = 0;
            while (CMSStartTransactionResponse[connId].Received == false)
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
                timeout++;
                if (timeout > 1000)
                {
                    timeout = 0;
                    sendStartTransactionRequest(connId);
                }
            }
            CMSStartTransactionResponse[connId].Received = false;
            CPStartTransactionRequest[connId].Sent = false;

            if (memcmp(CMSStartTransactionResponse[connId].idtaginfo.status, ACCEPTED, strlen(ACCEPTED)) == 0)
            {
                ConnectorOfflineData[connId].transactionId = CMSStartTransactionResponse[connId].transactionId;

                // send stop transaction
                setNULL(CPStopTransactionRequest[connId].idTag);
                setNULL(CPStopTransactionRequest[connId].reason);
                setNULL(CPStopTransactionRequest[connId].timestamp);
                memcpy(CPStopTransactionRequest[connId].idTag, ConnectorOfflineData[connId].idTag, sizeof(ConnectorOfflineData[connId].idTag));
                CPStopTransactionRequest[connId].meterStop = (int)ConnectorOfflineData[connId].meterStop;
                memcpy(CPStopTransactionRequest[connId].timestamp, ConnectorOfflineData[connId].meterStop_time, strlen(ConnectorOfflineData[connId].meterStop_time));
                updateStopStansactionReason(connId);
                CPStopTransactionRequest[connId].transactionId = (int)ConnectorOfflineData[connId].transactionId;
                sendStopTransactionRequest(connId, TransactionEnd, SAMPLED_DATA, OFFLINE_TRANSACTION);
                timeout = 0;
                while (CMSStopTransactionResponse[connId].Received == false)
                {
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    timeout++;
                    if (timeout > 1000)
                    {
                        timeout = 0;
                        sendStopTransactionRequest(connId, TransactionEnd, SAMPLED_DATA, OFFLINE_TRANSACTION);
                    }
                }
                CMSStopTransactionResponse[connId].Received = false;
                CPStopTransactionRequest[connId].Sent = false;
            }
        }
        else
        {
            // send stop transaction
            setNULL(CPStopTransactionRequest[connId].idTag);
            setNULL(CPStopTransactionRequest[connId].reason);
            setNULL(CPStopTransactionRequest[connId].timestamp);
            memcpy(CPStopTransactionRequest[connId].idTag, ConnectorOfflineData[connId].idTag, sizeof(ConnectorOfflineData[connId].idTag));
            CPStopTransactionRequest[connId].meterStop = (int)ConnectorOfflineData[connId].meterStop;
            memcpy(CPStopTransactionRequest[connId].timestamp, ConnectorOfflineData[connId].meterStop_time, strlen(ConnectorOfflineData[connId].meterStop_time));
            updateStopStansactionReason(connId);
            CPStopTransactionRequest[connId].transactionId = (int)ConnectorOfflineData[connId].transactionId;
            sendStopTransactionRequest(connId, TransactionEnd, SAMPLED_DATA, OFFLINE_TRANSACTION);
            timeout = 0;
            while (CMSStopTransactionResponse[connId].Received == false)
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
                timeout++;
                if (timeout > 1000)
                {
                    timeout = 0;
                    sendStopTransactionRequest(connId, TransactionEnd, SAMPLED_DATA, OFFLINE_TRANSACTION);
                }
            }
            CMSStopTransactionResponse[connId].Received = false;
            CPStopTransactionRequest[connId].Sent = false;
        }
        custom_nvs_read_connectors_offline_data_count();
    }
    updatingDataToCms[connId] = false;
}

void updateAlprInfo(const uint8_t *AlprData, uint8_t length)
{
    ESP_LOGI(TAG, "Recevied Data From ESPNOW");
    AlprData_t AlprInfo;
    memcpy(&AlprInfo, AlprData, sizeof(AlprData_t));

    ESP_LOGI(TAG, "msgType : %d", AlprInfo.msgType);
    ESP_LOGI(TAG, "header : %d", AlprInfo.header);
    ESP_LOGI(TAG, "deviceID : %d", AlprInfo.deviceID);
    ESP_LOGI(TAG, "packTYPE : %d", AlprInfo.packTYPE);
    ESP_LOGI(TAG, "packLEN : %d", AlprInfo.packLEN);
    ESP_LOGI(TAG, "packDATA : %s", AlprInfo.packDATA);
    ESP_LOGI(TAG, "checksum : %d", AlprInfo.checksum);
    ESP_LOGI(TAG, "footer : %d", AlprInfo.footer);
    if ((AlprInfo.msgType == MESSAGE_TYPE_DATA) &&
        (AlprInfo.packTYPE == CARPALTENUM) &&
        (AlprInfo.header == 0x55) &&
        (AlprInfo.footer == 0x23) &&
        (AlprInfo.deviceID <= config.NumberOfConnectors))
    {
        if ((taskState[AlprInfo.deviceID] == flag_EVSE_Read_Id_Tag) && (newRfidRead == false))
        {
            AlprAuthorizeConnId = AlprInfo.deviceID;
            AlprAuthorize = true;
            RemoteAuthorize = false;
            newRfidRead = false;
            setNULL(AlprTagNumberbuf);
            setNULL(RemoteTagNumberbuf);
            setNULL(rfidTagNumberbuf);
            memcpy(AlprTagNumberbuf, AlprInfo.packDATA, strlen(AlprInfo.packDATA));

            setNULL(ConnectorData[AlprInfo.deviceID].idTag);
            memcpy(ConnectorData[AlprInfo.deviceID].idTag, AlprTagNumberbuf, sizeof(AlprTagNumberbuf));

            memset(rfidTagNumberbuf, 0xFF, 10);
            memset(RemoteTagNumberbuf, 0xFF, 10);
            processRfidTappedState();
        }
        else if (taskState[AlprInfo.deviceID] != flag_EVSE_Read_Id_Tag)
        {
            ESP_LOGI(TAG, "Connector %hhu is Busy", AlprInfo.deviceID);
        }
        else
        {
            ESP_LOGI(TAG, "RFID Authorization is in progress, Can not process ALPR Request");
        }
    }
}

void smartChargingTask(void *params)
{
    bool logProfileData = false;
    static uint32_t current_chargingProfileId[NUM_OF_CONNECTORS] = {0, 0, 0, 0};
    static uint32_t current_chargingSchedulePeriod[NUM_OF_CONNECTORS] = {100, 100, 100, 100};

    for (uint8_t connId = 1; connId <= config.NumberOfConnectors; connId++)
        cpDutyCycleToSet[connId] = config.cpDuty[connId];

    while ((RtcTimeSet == false) && (isWebsocketBooted == false))
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    set_ChargingProfiles();
    for (uint8_t connId = 1; connId <= config.NumberOfConnectors; connId++)
    {
        if (ChargingProfile[connId].csChargingProfiles.chargingProfileId == 0)
        {
            ChargingProfiles_t ChargePointMaxProfileTemp;
            ChargePointMaxProfileTemp.connectorId = connId;
            ChargePointMaxProfileTemp.csChargingProfiles.chargingProfileId = connId;
            ChargePointMaxProfileTemp.csChargingProfiles.stackLevel = 0;
            ChargePointMaxProfileTemp.csChargingProfiles.chargingProfilePurpose = ChargePointMaxProfile;
            ChargePointMaxProfileTemp.csChargingProfiles.chargingProfileKind = Relative;
            ChargePointMaxProfileTemp.csChargingProfiles.chargingSchedule.chargingRateUnit = chargingRateUnitAmpere;
            ChargePointMaxProfileTemp.csChargingProfiles.chargingSchedule.chargingSchedulePeriod[0].schedulePresent = true;
            ChargePointMaxProfileTemp.csChargingProfiles.chargingSchedule.chargingSchedulePeriod[0].startPeriod = 0;
            ChargePointMaxProfileTemp.csChargingProfiles.chargingSchedule.chargingSchedulePeriod[0].limit = 17.0;
            custom_nvs_write_chargingProgile(ChargePointMaxProfileTemp);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }
    for (uint8_t connId = 1; connId <= config.NumberOfConnectors; connId++)
    {
        logChargingProfile(ChargingProfile[connId]);
    }
    while (true)
    {
        if (ChargingProfileAddedOrDeleted)
        {
            set_ChargingProfiles();
            if (logProfileData)
            {
                for (uint8_t connId = 1; connId <= config.NumberOfConnectors; connId++)
                {
                    logChargingProfile(ChargingProfile[connId]);
                }
            }
        }

        logProfileData = true;
        uint32_t current_time_sec = 0;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        current_time_sec = tv.tv_sec;

        //
        for (uint8_t connId = 1; connId <= config.NumberOfConnectors; connId++)
        {
            ESP_LOGI(TAG, "Connector %hhu Current Profile is %ld", connId, ChargingProfile[connId].csChargingProfiles.chargingProfileId);
            if ((ChargingProfile[connId].csChargingProfiles.chargingProfileId > 0) &&
                (ChargingProfile[connId].csChargingProfiles.chargingProfileKind > 0))
            {
                uint32_t NumOfChargingSchedulePeriodsPresent = 0;
                uint32_t SchedulePeriodToBeUsed = 0;
                for (uint32_t i = 0; i < SchedulePeriodCount; i++)
                {
                    if (ChargingProfile[connId].csChargingProfiles.chargingSchedule.chargingSchedulePeriod[i].schedulePresent)
                    {
                        NumOfChargingSchedulePeriodsPresent++;
                    }
                    else
                    {
                        i = SchedulePeriodCount;
                    }
                }

                if ((ChargingProfile[connId].csChargingProfiles.validFrom > 0) &&
                    (ChargingProfile[connId].csChargingProfiles.validTo > 0))
                {
                    if (current_time_sec >= ChargingProfile[connId].csChargingProfiles.validTo)
                    {
                        custom_nvs_clear_chargingProfile(ChargingProfile[connId].csChargingProfiles.chargingProfileId);
                    }
                }
                if (ChargingProfileAddedOrDeleted == false)
                {
                    uint32_t timeElapsed = 0;
                    uint32_t startTime = 0;
                    if (ChargingProfile[connId].csChargingProfiles.chargingProfileKind == Absolute)
                    {
                        if (ChargingProfile[connId].csChargingProfiles.chargingSchedule.startSchedule > 0)
                            startTime = ChargingProfile[connId].csChargingProfiles.chargingSchedule.startSchedule;
                        else if (ConnectorData[connId].isTransactionOngoing == true)
                            startTime = getTimeInSecondsFromTimestamp(ConnectorData[connId].meterStart_time);
                        else
                            startTime = current_time_sec;

                        timeElapsed = current_time_sec - startTime;
                    }
                    else if (ChargingProfile[connId].csChargingProfiles.chargingProfileKind == Recurring)
                    {
                        timeElapsed = 0;

                        if (ChargingProfile[connId].csChargingProfiles.recurrencyKind == Daily)
                        {
                            timeElapsed = current_time_sec % 86400; // 24 * 60 * 60
                        }
                        else
                        {
                            timeElapsed = current_time_sec % 604800; // 7 * 24 * 60 * 60
                        }
                    }
                    else if (ConnectorData[connId].isTransactionOngoing == true)
                    {
                        startTime = getTimeInSecondsFromTimestamp(ConnectorData[connId].meterStart_time);
                        timeElapsed = current_time_sec - startTime;
                    }
                    else
                    {
                        timeElapsed = 0;
                    }

                    if (NumOfChargingSchedulePeriodsPresent == 1)
                        SchedulePeriodToBeUsed = 0;
                    else if (timeElapsed >= ChargingProfile[connId].csChargingProfiles.chargingSchedule.chargingSchedulePeriod[NumOfChargingSchedulePeriodsPresent - 1].startPeriod)
                    {
                        SchedulePeriodToBeUsed = NumOfChargingSchedulePeriodsPresent - 1;
                    }
                    else
                    {
                        for (uint32_t i = 0; i < (NumOfChargingSchedulePeriodsPresent - 1); i++)
                        {
                            if ((timeElapsed >= ChargingProfile[connId].csChargingProfiles.chargingSchedule.chargingSchedulePeriod[i].startPeriod) &&
                                (timeElapsed < ChargingProfile[connId].csChargingProfiles.chargingSchedule.chargingSchedulePeriod[i + 1].startPeriod))
                            {
                                SchedulePeriodToBeUsed = i;
                                break;
                            }
                        }
                    }
                    ESP_LOGD(TAG, "Connector %hhu Schedule Period to be used is %ld", connId, SchedulePeriodToBeUsed);
                    if ((ChargingProfile[connId].csChargingProfiles.chargingProfileId != current_chargingProfileId[connId]) ||
                        (SchedulePeriodToBeUsed != current_chargingSchedulePeriod[connId]))
                    {
                        if (ChargingProfile[connId].csChargingProfiles.chargingSchedule.chargingRateUnit == chargingRateUnitAmpere)
                        {
                            float currentLimit = ChargingProfile[connId].csChargingProfiles.chargingSchedule.chargingSchedulePeriod[SchedulePeriodToBeUsed].limit;
                            if (currentLimit >= 64.0)
                            {
                                cpDutyCycleToSet[connId] = 89;
                            }
                            else if (currentLimit >= 50.0)
                            {
                                cpDutyCycleToSet[connId] = 83;
                            }
                            else if (currentLimit >= 40.0)
                            {
                                cpDutyCycleToSet[connId] = 66;
                            }
                            else if (currentLimit >= 32.0)
                            {
                                cpDutyCycleToSet[connId] = 53;
                            }
                            else if (currentLimit >= 25.0)
                            {
                                cpDutyCycleToSet[connId] = 41;
                            }
                            else if (currentLimit >= 20.0)
                            {
                                cpDutyCycleToSet[connId] = 33;
                            }
                            else if (currentLimit >= 16.0)
                            {
                                cpDutyCycleToSet[connId] = 26;
                            }
                            else
                            {
                                cpDutyCycleToSet[connId] = 16;
                            }
                        }
                        else
                        {
                            float powerLimit = ChargingProfile[connId].csChargingProfiles.chargingSchedule.chargingSchedulePeriod[SchedulePeriodToBeUsed].limit;

                            if (config.Ac11s || config.Ac22s || config.Ac44s)
                            {
                                powerLimit = powerLimit / 3.0;
                            }
                            if (powerLimit >= 14719.0)
                            {
                                cpDutyCycleToSet[connId] = 53;
                            }
                            else if (powerLimit >= 11499.0)
                            {
                                cpDutyCycleToSet[connId] = 53;
                            }
                            else if (powerLimit >= 9199.0)
                            {
                                cpDutyCycleToSet[connId] = 53;
                            }
                            else if (powerLimit >= 7359.0)
                            {
                                cpDutyCycleToSet[connId] = 53;
                            }
                            else if (powerLimit >= 5749.0)
                            {
                                cpDutyCycleToSet[connId] = 41;
                            }
                            else if (powerLimit >= 4599.0)
                            {
                                cpDutyCycleToSet[connId] = 33;
                            }
                            else if (powerLimit >= 3679.0)
                            {
                                cpDutyCycleToSet[connId] = 26;
                            }
                            else
                            {
                                cpDutyCycleToSet[connId] = 16;
                            }
                        }
                        if ((ConnectorData[connId].isTransactionOngoing == false) && (config.cpDuty[connId] != cpDutyCycleToSet[connId]))
                        {
                            changeDutyCycle(connId);
                        }
                        ESP_LOGI(TAG, "Connector %hhu Duty Cycle to be set is %hhu", connId, cpDutyCycleToSet[connId]);
                    }
                }
            }
        }
        vTaskDelay(SmartChargingSchedulePeriodCheckTime * 1000 / portTICK_PERIOD_MS);
    }
}

void Connector0Task(void *params)
{
    uint8_t i = 0;
    uint32_t timeout = 0;
    uint32_t bootRetryTime = 0;
    uint32_t loopCount = 0;
    uint32_t loopCountlogs = 0;
    uint8_t connId_temp = 0;
    uint32_t rfidReadTimeCount = 0;
    uint32_t offlineTimeout = 0;
    uint32_t systemDaySeconds = 0;
    struct timeval tvReserved;
    struct timeval tvNow;
    struct timeval tvAlignedData;
    struct tm tm_time_Reserved = {0};
    struct tm tm_time_Now = {0};
    uint8_t emergencyTimeCount = 0;
    uint8_t emergencyTransitionCount = 0;
    uint8_t restartReason = 0;
    // togglewatchdog();

    bool OngoingTransaction = false;
    bool isBootRejected = false;
    bool isWebsocketConnected_Now = false;
    bool isWebsocketConnected_old = false;
    bool AllTasksAvailableState = false;
    bool checkForChargingState = false;
    memset(gfciButton_old, 0, sizeof(gfciButton_old));
    memset(earthDisconnectButton_old, 0, sizeof(earthDisconnectButton_old));
    memset(emergencyButton_old, 0, sizeof(emergencyButton_old));
    memset(relayWeldDetectionButton_old, 0, sizeof(relayWeldDetectionButton_old));
    memset(connectorFault, 0, sizeof(connectorFault));
    ledState[0] = OFFLINE_UNAVAILABLE_LED_STATE;
    ledState[1] = OFFLINE_UNAVAILABLE_LED_STATE;
    ledState[2] = OFFLINE_UNAVAILABLE_LED_STATE;
    ledState[3] = OFFLINE_UNAVAILABLE_LED_STATE;

    memset(MeterFaults, 0, sizeof(MeterFaults));

    memset(MeterFaults_old, 0, sizeof(MeterFaults));
    TickType_t xLastWakeTime = xTaskGetTickCount();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    if (CPGetConfigurationResponse.HeartbeatInterval < 10)
    {
        CPGetConfigurationResponse.HeartbeatInterval = 10;
    }
    while (controlPilotActive == false)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    if (custom_nvs_read_restart_reason(&restartReason) == ESP_OK)
    {
        WebsocketRestart = (restartReason == WEBSOCKET_RESTART) ? true : false;
        ESP_LOGI(TAG, "Restart Reason: %hhu", restartReason);
        ESP_LOGI(TAG, "Websocket Restart: %hhu", WebsocketRestart);
        if (WebsocketRestart == false)
        {
            custom_nvs_write_restart_reason(WEBSOCKET_RESTART_CLEAR);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read restart reason");
    }
    while (true)
    {
        // if (config.vcharge_lite_1_4)
        // {
        //     uint8_t primary_channel;
        //     wifi_second_chan_t second_channel;

        //     esp_err_t err = esp_wifi_get_channel(&primary_channel, &second_channel);
        //     if (err == ESP_OK)
        //     {
        //         SW_Serial_write_wifiChannel(primary_channel);
        //     }
        //     else
        //     {
        //         ESP_LOGE("WiFi Channel", "Failed to get WiFi channel: %s", esp_err_to_name(err));
        //     }
        // }

        isWebsocketConnected_Now = isWebsocketConnected;
        if (isWebsocketConnected_Now & isWebsocketBooted)
        {
            if (CPGetConfigurationResponse.ClockAlignedDataInterval != 0)
            {
                gettimeofday(&tvAlignedData, NULL);
                systemDaySeconds = tvAlignedData.tv_sec % 86400;
                if (systemDaySeconds % CPGetConfigurationResponse.ClockAlignedDataInterval == 0)
                {
                    ClockAlignedDataTime = true;
                }
                else
                {
                    ClockAlignedDataTime = false;
                }
            }
        }
        if (config.vcharge_lite_1_4)
        {
            emergencyButton = slavePushButton[3];
        }
        if ((emergencyButton == true) && (emergencyButton_old[1] == false))
        {
            emergencyTransitionCount++;
        }
        if (emergencyTransitionCount > 0)
        {
            emergencyTimeCount++;
            if ((emergencyTransitionCount == 2) && (emergencyTimeCount == 10))
            {
                emergencyTransitionCount = 0;
                emergencyTimeCount = 0;
                esp_restart();
            }
            if ((emergencyTransitionCount == 3) && (emergencyTimeCount == 10))
            {
                emergencyTransitionCount = 0;
                emergencyTimeCount = 0;
                if ((config.redirectLogs) && isWebsocketConnected && isWebsocketBooted && (uploadingLogs == false))
                {
                    updateDiagnosticsfileName();
                    printf(CPGetDiagnosticsResponse.fileName);
                    sendLogsToCloud();
                    uploadingLogs = true;
                }
            }
        }
        if (emergencyTimeCount > 10)
        {
            emergencyTransitionCount = 0;
            emergencyTimeCount = 0;
        }
        switch (taskState[0])
        {
        case flag_EVSE_initialisation:
            if (isWebsocketConnected_Now == true)
            {
                if ((CPBootNotificationRequest.Sent == false) & (isBootRejected == false))
                {
                    isWebsocketBooted = false;
                    sendBootNotificationRequest();
                }
                if (CMSBootNotificationResponse.Received == false)
                {
                    timeout++;
                }
                else if (CMSBootNotificationResponse.Received &
                         (memcmp(CMSBootNotificationResponse.status, ACCEPTED, strlen(ACCEPTED)) == 0))
                {
                    offlineTimeout = 0;
                    timeout = 0;
                    isBootRejected = false;
                    taskState[0] = flag_EVSE_is_Booted;
                    setNULL(time_buffer);
                    memcpy(time_buffer, CMSBootNotificationResponse.currentTime, strlen(CMSBootNotificationResponse.currentTime));
                    parse_timestamp(time_buffer);
                    if (config.vcharge_v5 || config.AC1)
                    {
                        RTC_Clock_setTimeWithBuffer(time_buffer);
                    }
                    custom_nvs_write_timeBuffer();
                    for (i = 1; i <= config.NumberOfConnectors; i++)
                    {
                        if ((ConnectorData[i].isTransactionOngoing) & (ConnectorData[i].offlineStarted == false))
                        {
                            sendOngoigTransactionsData(i);
                            connectorsOfflineDataSent[i] = true;
                        }
                    }
                    custom_nvs_read_connectors_offline_data_count();
                    for (i = 1; i <= config.NumberOfConnectors; i++)
                    {
                        if (updatingOfflineData[i] & (ConnectorData[i].isTransactionOngoing == true) & (ConnectorData[i].offlineStarted == true))
                        {
                            connectorsOfflineDataSent[i] = true;
                            sendOfflineTransactionsData(i);
                        }
                        else if (updatingOfflineData[i] & (ConnectorData[i].isTransactionOngoing == false))
                        {
                            connectorsOfflineDataSent[i] = true;
                            sendOfflineTransactionsData(i);
                        }
                        else if (updatingOfflineData[i])
                        {
                            updatingOfflineDataPending[i] = true;
                        }
                        else
                        {
                            updatingOfflineDataPending[i] = false;
                        }

                        if ((ConnectorData[i].isTransactionOngoing) & (ConnectorData[i].offlineStarted == true))
                        {
                            connectorsOfflineDataSent[i] = true;
                            sendOngoigTransactionsData(i);
                        }
                    }
                    isWebsocketBooted = true;
                    isWebsocketConnected_old = false;
                    bootedNow = true;
                    for (i = 1; i <= config.NumberOfConnectors; i++)
                    {
                        if (taskState[i] < flag_EVSE_Read_Id_Tag)
                            taskState[i] = flag_EVSE_Read_Id_Tag;
                    }
                    isPoweredOn = false;
                    updateStatusAfterBoot[0] = true;
                    updateStatusAfterBoot[1] = true;
                    updateStatusAfterBoot[2] = true;
                    updateStatusAfterBoot[3] = true;
                    custom_nvs_write_restart_reason(WEBSOCKET_RESTART_CLEAR);
                }
                else if (CMSBootNotificationResponse.Received &
                         (memcmp(CMSBootNotificationResponse.status, ACCEPTED, strlen(ACCEPTED)) != 0))
                {
                    timeout = 0;
                    isBootRejected = true;
                    bootRetryTime = (CMSBootNotificationResponse.interval == 0) ? BOOT_RETRY_TIME : CMSBootNotificationResponse.interval;
                    CMSBootNotificationResponse.Received = false;
                }
                if (isBootRejected)
                {
                    if (timeout > bootRetryTime)
                    {
                        isBootRejected = false;
                        timeout = 0;
                        sendBootNotificationRequest();
                    }
                }
                else if (timeout > BOOT_TIMEOUT)
                {
                    timeout = 0;
                    sendBootNotificationRequest();
                }
            }
            else
            {
                offlineTimeout++;
                if ((offlineTimeout > BOOT_OFFLINE_TIMEOUT) || (config.offline == true))
                {
                    if (isPoweredOn)
                    {
                        custom_nvs_read_timeBuffer();
                        parse_timestamp(time_buffer);
                        for (i = 1; i <= config.NumberOfConnectors; i++)
                        {
                            if (ConnectorData[i].isTransactionOngoing)
                            {
                                if (((((cpState[i] == STATE_B1) || (cpState[i] == STATE_E2)) && (cpEnable[i] == true)) || (cpEnable[i] == false)) && (config.resumeSession || WebsocketRestart))
                                    taskState[i] = flag_EVSE_Request_Charge;
                                else
                                {
                                    ConnectorData[i].isTransactionOngoing = false;
                                    ConnectorData[i].stopReason = Stop_PowerLoss;
                                    custom_nvs_write_connector_data(i);
                                    custom_nvs_write_connectors_offline_data(i);
                                }
                            }
                        }
                    }
                    offlineTimeout = 0;
                    timeout = 0;
                    isPoweredOn = false;

                    if (taskState[1] < flag_EVSE_Read_Id_Tag)
                        taskState[1] = flag_EVSE_Read_Id_Tag;
                    if (taskState[2] < flag_EVSE_Read_Id_Tag)
                        taskState[2] = flag_EVSE_Read_Id_Tag;
                    if (taskState[3] < flag_EVSE_Read_Id_Tag)
                        taskState[3] = flag_EVSE_Read_Id_Tag;
                    custom_nvs_write_restart_reason(WEBSOCKET_RESTART_CLEAR);
                }
            }
            break;
        case flag_EVSE_is_Booted:
            isWebsocketBooted = true;
            if ((isWebsocketConnected_Now != isWebsocketConnected_old) & (isWebsocketConnected_Now == true))
            {
                custom_nvs_read_connectors_offline_data_count();
                for (i = 1; i <= config.NumberOfConnectors; i++)
                {
                    if (updatingOfflineData[i] & (ConnectorData[i].isTransactionOngoing == true) & (ConnectorData[i].offlineStarted == true))
                    {
                        connectorsOfflineDataSent[i] = true;
                        sendOfflineTransactionsData(i);
                    }
                    else if (updatingOfflineData[i] & (ConnectorData[i].isTransactionOngoing == false))
                    {
                        connectorsOfflineDataSent[i] = true;
                        sendOfflineTransactionsData(i);
                    }
                    else if (updatingOfflineData[i])
                    {
                        updatingOfflineDataPending[i] = true;
                    }
                    else
                    {
                        updatingOfflineDataPending[i] = false;
                    }

                    if ((ConnectorData[i].isTransactionOngoing) & (ConnectorData[i].offlineStarted == true))
                    {
                        connectorsOfflineDataSent[i] = true;
                        sendOngoigTransactionsData(i);
                    }
                }
            }
            break;
        default:
            break;
        }
        if (isWebsocketConnected_Now & isWebsocketBooted)
        {

            if (loopCount >= CPGetConfigurationResponse.HeartbeatInterval)
            {
                loopCount = 0;
#if 0
                if (((taskState[1] == flag_EVSE_Request_Charge) &&
                     ((((cpState[1] == STATE_C) || (cpState[1] == STATE_D)) && (cpEnable[1] == true)) || (cpEnable[1] == false))) ||
                    ((taskState[2] == flag_EVSE_Request_Charge) &&
                     ((((cpState[2] == STATE_C) || (cpState[2] == STATE_D)) && (cpEnable[2] == true)) || (cpEnable[2] == false))) ||
                    ((taskState[3] == flag_EVSE_Request_Charge) &&
                     ((((cpState[3] == STATE_C) || (cpState[3] == STATE_D)) && (cpEnable[2] == true)) || (cpEnable[2] == false))))
                {
                }
                else
                {
                    ESP_LOGD(TAG, "Sending Heart beat");
                    sendHeartbeatRequest();
                }
#else
                sendHeartbeatRequest();
#endif
            }

            for (i = 0; i <= config.NumberOfConnectors; i++)
            {
                if (CMSAuthorizeResponse[i].Received)
                {
                    processRfidTappedState();
                }
                if (CMSRemoteStartTransactionRequest[i].Received)
                {
                    CMSRemoteStartTransactionRequest[i].Received = false;
                    setNULL(CPRemoteStartTransactionResponse[i].status);
                    if ((connectorFault[i]) || (taskState[i] != flag_EVSE_Read_Id_Tag))
                    {
                        memcpy(CPRemoteStartTransactionResponse[i].status, REJECTED, strlen(REJECTED));
                    }
                    else if (CPGetConfigurationResponse.AuthorizeRemoteTxRequests)
                    {
                        RemoteAuthorize = true;
                        newRfidRead = false;
                        RemoteAuthorizeConnId = i;
                        if (PushButtonEnabled[i])
                            PushButton[i] = true;
                        setNULL(ConnectorData[i].idTag);
                        memcpy(ConnectorData[i].idTag, CMSRemoteStartTransactionRequest[i].idTag, sizeof(CMSRemoteStartTransactionRequest[i].idTag));
                        setNULL(RemoteTagNumberbuf);
                        setNULL(rfidTagNumberbuf);
                        memset(rfidTagNumberbuf, 0xFF, 10);
                        memcpy(RemoteTagNumberbuf, CMSRemoteStartTransactionRequest[i].idTag, sizeof(CMSRemoteStartTransactionRequest[i].idTag));
                        memcpy(CPRemoteStartTransactionResponse[i].status, ACCEPTED, strlen(ACCEPTED));
                        processRfidTappedState();
                    }
                    else
                    {
                        setNULL(ConnectorData[i].idTag);
                        memcpy(ConnectorData[i].idTag, CMSRemoteStartTransactionRequest[i].idTag, sizeof(CMSRemoteStartTransactionRequest[i].idTag));
                        setNULL(rfidTagNumberbuf);
                        memcpy(rfidTagNumberbuf, CMSRemoteStartTransactionRequest[i].idTag, sizeof(CMSRemoteStartTransactionRequest[i].idTag));
                        rfidReceivedFromRemote[i] = true;
                        if (PushButtonEnabled[i])
                            PushButton[i] = true;
                        memcpy(CPRemoteStartTransactionResponse[i].status, ACCEPTED, strlen(ACCEPTED));
                    }
                    sendRemoteStartTransactionResponse(i);
                }
                if (CMSChangeAvailabilityRequest[i].Received)
                {
                    ConnectorData[i].CmsAvailableChanged = false;
                    if (taskState[i] == flag_EVSE_Request_Charge)
                    {
                        ConnectorData[i].CmsAvailableScheduled = (memcmp(CMSChangeAvailabilityRequest[i].type, Inoperative, strlen(Inoperative)) == 0) ? true : false;
                        setNULL(CPChangeAvailabilityResponse.status);
                        memcpy(CPChangeAvailabilityResponse.status, SCHEDULED, strlen(SCHEDULED));
                        custom_nvs_write_connector_data(i);
                    }
                    else
                    {
                        ConnectorData[i].CmsAvailable = (memcmp(CMSChangeAvailabilityRequest[i].type, Inoperative, strlen(Inoperative)) == 0) ? false : true;
                        ConnectorData[i].CmsAvailableChanged = true;
                        setNULL(CPChangeAvailabilityResponse.status);
                        memcpy(CPChangeAvailabilityResponse.status, ACCEPTED, strlen(ACCEPTED));
                        custom_nvs_write_connector_data(i);
                    }
                    sendChangeAvailabilityResponse(i);
                    CMSChangeAvailabilityRequest[i].Received = false;
                }
                if ((taskState[i] == flag_EVSE_Read_Id_Tag) &&
                    (((cpState[i] == STATE_A) && (cpEnable[i] == true)) || ((cpEnable[i] == false) && (StatusState[i] == STATUS_AVAILABLE))) &&
                    (ConnectorData[i].CmsAvailableScheduled == true))
                {
                    ConnectorData[i].CmsAvailableScheduled = false;
                    ConnectorData[i].CmsAvailable = false;
                    ConnectorData[i].CmsAvailableChanged = true;
                    setNULL(CPChangeAvailabilityResponse.status);
                    memcpy(CPChangeAvailabilityResponse.status, ACCEPTED, strlen(ACCEPTED));
                    sendChangeAvailabilityResponse(i);
                    custom_nvs_write_connector_data(i);
                }
                if (CMSReserveNowRequest[i].Received)
                {
                    if (connectorFault[i] == 1)
                    {
                        setNULL(CPReserveNowResponse.status);
                        memcpy(CPReserveNowResponse.status, Faulted, strlen(Faulted));
                    }
                    else if (ConnectorData[i].CmsAvailable == false)
                    {
                        setNULL(CPReserveNowResponse.status);
                        memcpy(CPReserveNowResponse.status, Unavailable, strlen(Unavailable));
                    }
                    else if (taskState[i] == flag_EVSE_Request_Charge)
                    {
                        setNULL(CPReserveNowResponse.status);
                        memcpy(CPReserveNowResponse.status, OCCUPIED, strlen(OCCUPIED));
                    }
                    else
                    {
                        ConnectorData[i].isReserved = true;
                        ConnectorData[i].ReservedData.reservationId = CMSReserveNowRequest[i].reservationId;
                        setNULL(ConnectorData[i].ReservedData.idTag);
                        setNULL(ConnectorData[i].ReservedData.expiryDate);
                        setNULL(ConnectorData[i].ReservedData.parentidTag);
                        memcpy(ConnectorData[i].ReservedData.idTag, CMSReserveNowRequest[i].idTag, strlen(CMSReserveNowRequest[i].idTag));
                        memcpy(ConnectorData[i].ReservedData.expiryDate, CMSReserveNowRequest[i].expiryDate, strlen(CMSReserveNowRequest[i].expiryDate));
                        if (CMSReserveNowRequest[i].isparentidTagReceived)
                        {
                            memcpy(ConnectorData[i].ReservedData.parentidTag, CMSReserveNowRequest[i].parentidTag, strlen(CMSReserveNowRequest[i].parentidTag));
                        }
                        custom_nvs_write_connector_data(i);
                        setNULL(CPReserveNowResponse.status);
                        memcpy(CPReserveNowResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    sendReserveNowResponse(i);
                    CMSReserveNowRequest[i].Received = false;
                }
            }
            if (CMSHeartbeatResponse.Received)
            {
                CMSHeartbeatResponse.Received = false;
                setNULL(time_buffer);
                memcpy(time_buffer, CMSHeartbeatResponse.currentTime, strlen(CMSHeartbeatResponse.currentTime));
                parse_timestamp(time_buffer);
                custom_nvs_write_timeBuffer();
            }
            if (CMSRemoteStopTransactionRequest.Received)
            {
                if (CMSRemoteStopTransactionRequest.transactionId == ConnectorData[0].transactionId)
                {
                    ConnectorData[0].stopReason = Stop_Remote;
                    taskState[0] = flag_EVSE_Stop_Transaction;
                    sendRemoteStopTransactionResponse();
                    CMSRemoteStopTransactionRequest.Received = false;
                }
                else if (CMSRemoteStopTransactionRequest.transactionId == ConnectorData[1].transactionId)
                {
                    ConnectorData[1].stopReason = Stop_Remote;
                    taskState[1] = flag_EVSE_Stop_Transaction;
                    sendRemoteStopTransactionResponse();
                    CMSRemoteStopTransactionRequest.Received = false;
                }
                else if (CMSRemoteStopTransactionRequest.transactionId == ConnectorData[2].transactionId)
                {
                    ConnectorData[2].stopReason = Stop_Remote;
                    taskState[2] = flag_EVSE_Stop_Transaction;
                    sendRemoteStopTransactionResponse();
                    CMSRemoteStopTransactionRequest.Received = false;
                }
                else if (CMSRemoteStopTransactionRequest.transactionId == ConnectorData[3].transactionId)
                {
                    ConnectorData[3].stopReason = Stop_Remote;
                    taskState[3] = flag_EVSE_Stop_Transaction;
                    sendRemoteStopTransactionResponse();
                    CMSRemoteStopTransactionRequest.Received = false;
                }
            }
            if (CMSGetConfigurationRequest.Received)
            {
                sendGetConfigurationResponse();
                CMSGetConfigurationRequest.Received = false;
            }
            if (CMSSendLocalListRequest.Received)
            {
                SendLocalListResponse();
                custom_nvs_write_localist_ocpp();
                CMSSendLocalListRequest.Received = false;
                for (uint32_t i = 0; i < LOCAL_LIST_COUNT; i++)
                {
                    if (CMSSendLocalListRequest.localAuthorizationList[i].idTagPresent)
                    {
                        ESP_LOGI(TAG, "CMS Local List Tag%ld : %s", i, CMSSendLocalListRequest.localAuthorizationList[i].idTag);
                    }
                }
            }
            if (CMSTriggerMessageRequest.Received)
            {
                CMSTriggerMessageRequest.Received = false;
                setNULL(CPTriggerMessageResponse.status);
                if (memcmp(CMSTriggerMessageRequest.requestedMessage, "StatusNotification", strlen("StatusNotification")) == 0)
                {
                    memcpy(CPTriggerMessageResponse.status, ACCEPTED, strlen(ACCEPTED));
                    SendTriggerMessageResponse();
                    setNULL(time_buffer);
                    getTimeInOcppFormat();
                    memcpy(CPStatusNotificationRequest[CMSTriggerMessageRequest.connectorId].timestamp, time_buffer, strlen(time_buffer));
                    sendStatusNotificationRequest(CMSTriggerMessageRequest.connectorId);
                }
                else if (memcmp(CMSTriggerMessageRequest.requestedMessage, "MeterValues", strlen("MeterValues")) == 0)
                {
                    memcpy(CPTriggerMessageResponse.status, ACCEPTED, strlen(ACCEPTED));
                    SendTriggerMessageResponse();
                    connId_temp = CMSTriggerMessageRequest.connectorId;
                    ConnectorData[connId_temp].meterValue = readEnrgyMeter(connId_temp);
                    ConnectorData[connId_temp].meterStop = ConnectorData[connId_temp].meterStop + ((ConnectorData[connId_temp].meterValue.power * MeterValueSampleTime) / 3600.0);
                    ConnectorData[connId_temp].Energy = ConnectorData[connId_temp].meterStop - ConnectorData[connId_temp].meterStart;
                    updateMeterValues(connId_temp, SAMPLED_DATA);
                    sendMeterValuesRequest(connId_temp, Trigger, SAMPLED_DATA);
                }
                else if (memcmp(CMSTriggerMessageRequest.requestedMessage, "BootNotification", strlen("BootNotification")) == 0)
                {
                    memcpy(CPTriggerMessageResponse.status, ACCEPTED, strlen(ACCEPTED));
                    SendTriggerMessageResponse();
                    sendBootNotificationRequest();
                }
                else if (memcmp(CMSTriggerMessageRequest.requestedMessage, "DiagnosticsStatusNotification", strlen("DiagnosticsStatusNotification")) == 0)
                {
                    memcpy(CPTriggerMessageResponse.status, ACCEPTED, strlen(ACCEPTED));
                    SendTriggerMessageResponse();
                    sendDiagnosticsStatusNotificationRequest();
                }
                else if (memcmp(CMSTriggerMessageRequest.requestedMessage, "FirmwareStatusNotification", strlen("FirmwareStatusNotification")) == 0)
                {
                    memcpy(CPTriggerMessageResponse.status, ACCEPTED, strlen(ACCEPTED));
                    SendTriggerMessageResponse();
                    sendFirmwareStatusNotificationRequest();
                }
                else if (memcmp(CMSTriggerMessageRequest.requestedMessage, "Heartbeat", strlen("Heartbeat")) == 0)
                {
                    memcpy(CPTriggerMessageResponse.status, ACCEPTED, strlen(ACCEPTED));
                    SendTriggerMessageResponse();
                    sendHeartbeatRequest();
                }
            }
            if (CMSGetLocalListVersionRequest.Received)
            {
                sendGetLocalListVersionResponse();
                CMSGetLocalListVersionRequest.Received = false;
            }
            if (CMSClearCacheRequest.Received)
            {
                memset(&LocalAuthorizationList, 0, sizeof(LocalAuthorizationList_t));
                custom_nvs_write_LocalAuthorizationList_ocpp();
                setNULL(CPClearCacheResponse.status);
                memcpy(CPClearCacheResponse.status, ACCEPTED, strlen(ACCEPTED));
                sendClearCacheResponse();
                CMSClearCacheRequest.Received = false;
            }
            if (CMSCancelReservationRequest.Received)
            {
                CPCancelReservationResponse.Sent = false;
                for (i = 0; i <= config.NumberOfConnectors; i++)
                {
                    if ((CMSCancelReservationRequest.reservationId == ConnectorData[i].ReservedData.reservationId) && (ConnectorData[i].isReserved))
                    {
                        ConnectorData[i].isReserved = false;
                        setNULL(CPCancelReservationResponse.status);
                        memcpy(CPCancelReservationResponse.status, ACCEPTED, strlen(ACCEPTED));
                        sendCancelReservationResponse();
                    }
                }
                if (CPCancelReservationResponse.Sent == false)
                {
                    setNULL(CPCancelReservationResponse.status);
                    memcpy(CPCancelReservationResponse.status, REJECTED, strlen(REJECTED));
                    sendCancelReservationResponse();
                }
                CMSCancelReservationRequest.Received = false;
            }
            if (CMSChangeConfigurationRequest.Received)
            {
                setNULL(CPChangeConfigurationResponse.status);
                if (memcmp(CMSChangeConfigurationRequest.key, "AuthorizeRemoteTxRequests", strlen("AuthorizeRemoteTxRequests")) == 0)
                {
                    if (CPGetConfigurationResponse.AuthorizeRemoteTxRequestsReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.AuthorizeRemoteTxRequestsValue);
                        memcpy(CPGetConfigurationResponse.AuthorizeRemoteTxRequestsValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.AuthorizeRemoteTxRequests = (memcmp(CMSChangeConfigurationRequest.value, "true", strlen("true")) == 0) ? true : false;
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "ClockAlignedDataInterval", strlen("ClockAlignedDataInterval")) == 0)
                {
                    if (CPGetConfigurationResponse.ClockAlignedDataIntervalReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.ClockAlignedDataIntervalValue);
                        memcpy(CPGetConfigurationResponse.ClockAlignedDataIntervalValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.ClockAlignedDataInterval = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.ClockAlignedDataInterval > 0 && CPGetConfigurationResponse.ClockAlignedDataInterval < 30)
                        {
                            CPGetConfigurationResponse.ClockAlignedDataInterval = 30;
                            setNULL(CPGetConfigurationResponse.ClockAlignedDataIntervalValue);
                            memcpy(CPGetConfigurationResponse.ClockAlignedDataIntervalValue, "30", strlen("30"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "ConnectionTimeOut", strlen("ConnectionTimeOut")) == 0)
                {
                    if (CPGetConfigurationResponse.ConnectionTimeOutReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.ConnectionTimeOutValue);
                        memcpy(CPGetConfigurationResponse.ConnectionTimeOutValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.ConnectionTimeOut = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.ConnectionTimeOut == 0)
                        {
                            CPGetConfigurationResponse.ConnectionTimeOut = 120;
                            setNULL(CPGetConfigurationResponse.ConnectionTimeOutValue);
                            memcpy(CPGetConfigurationResponse.ConnectionTimeOutValue, "120", strlen("120"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "GetConfigurationMaxKeys", strlen("GetConfigurationMaxKeys")) == 0)
                {
                    if (CPGetConfigurationResponse.GetConfigurationMaxKeysReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.GetConfigurationMaxKeysValue);
                        memcpy(CPGetConfigurationResponse.GetConfigurationMaxKeysValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.GetConfigurationMaxKeys = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.GetConfigurationMaxKeys == 0)
                        {
                            CPGetConfigurationResponse.GetConfigurationMaxKeys = 60;
                            setNULL(CPGetConfigurationResponse.GetConfigurationMaxKeysValue);
                            memcpy(CPGetConfigurationResponse.GetConfigurationMaxKeysValue, "60", strlen("60"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "HeartbeatInterval", strlen("HeartbeatInterval")) == 0)
                {
                    if (CPGetConfigurationResponse.HeartbeatIntervalReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.HeartbeatIntervalValue);
                        memcpy(CPGetConfigurationResponse.HeartbeatIntervalValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.HeartbeatInterval = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.HeartbeatInterval == 0)
                        {
                            CPGetConfigurationResponse.HeartbeatInterval = 60;
                            setNULL(CPGetConfigurationResponse.HeartbeatIntervalValue);
                            memcpy(CPGetConfigurationResponse.HeartbeatIntervalValue, "60", strlen("60"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "LocalAuthorizeOffline", strlen("LocalAuthorizeOffline")) == 0)
                {
                    if (CPGetConfigurationResponse.LocalAuthorizeOfflineReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.LocalAuthorizeOfflineValue);
                        memcpy(CPGetConfigurationResponse.LocalAuthorizeOfflineValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.LocalAuthorizeOffline = (memcmp(CMSChangeConfigurationRequest.value, "true", strlen("true")) == 0) ? true : false;
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "LocalPreAuthorize", strlen("LocalPreAuthorize")) == 0)
                {
                    if (CPGetConfigurationResponse.LocalPreAuthorizeReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.LocalPreAuthorizeValue);
                        memcpy(CPGetConfigurationResponse.LocalPreAuthorizeValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.LocalPreAuthorize = (memcmp(CMSChangeConfigurationRequest.value, "true", strlen("true")) == 0) ? true : false;
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "MeterValuesAlignedData", strlen("MeterValuesAlignedData")) == 0)
                {
                    if (CPGetConfigurationResponse.MeterValuesAlignedDataReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.MeterValuesAlignedDataValue);
                        memcpy(CPGetConfigurationResponse.MeterValuesAlignedDataValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.MeterValuesAlignedData.EnergyActiveExportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Export.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.EnergyActiveImportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Import.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.EnergyReactiveExportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Export.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.EnergyReactiveImportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Import.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.EnergyActiveExportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Export.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.EnergyActiveImportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Import.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.EnergyReactiveExportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Export.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.EnergyReactiveImportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Import.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.PowerActiveExport = (strstr(CMSChangeConfigurationRequest.value, "Power.Active.Export") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.PowerActiveImport = (strstr(CMSChangeConfigurationRequest.value, "Power.Active.Import") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.PowerOffered = (strstr(CMSChangeConfigurationRequest.value, "Power.Offered") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.PowerReactiveExport = (strstr(CMSChangeConfigurationRequest.value, "Power.Reactive.Export") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.PowerReactiveImport = (strstr(CMSChangeConfigurationRequest.value, "Power.Reactive.Import") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.PowerFactor = (strstr(CMSChangeConfigurationRequest.value, "Power.Factor") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.CurrentExport = (strstr(CMSChangeConfigurationRequest.value, "Current.Export") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.CurrentImport = (strstr(CMSChangeConfigurationRequest.value, "Current.Import") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.CurrentOffered = (strstr(CMSChangeConfigurationRequest.value, "Current.Offered") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.Voltage = (strstr(CMSChangeConfigurationRequest.value, "Voltage") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.Temperature = (strstr(CMSChangeConfigurationRequest.value, "Temperature") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.SoC = (strstr(CMSChangeConfigurationRequest.value, "SoC") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesAlignedData.RPM = (strstr(CMSChangeConfigurationRequest.value, "RPM") != NULL) ? true : false;

                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "MeterValuesSampledData", strlen("MeterValuesSampledData")) == 0)
                {
                    if (CPGetConfigurationResponse.MeterValuesSampledDataReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.MeterValuesSampledDataValue);
                        memcpy(CPGetConfigurationResponse.MeterValuesSampledDataValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.MeterValuesSampledData.EnergyActiveExportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Export.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.EnergyActiveImportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Import.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.EnergyReactiveExportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Export.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.EnergyReactiveImportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Import.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.EnergyActiveExportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Export.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.EnergyActiveImportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Import.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.EnergyReactiveExportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Export.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.EnergyReactiveImportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Import.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.PowerActiveExport = (strstr(CMSChangeConfigurationRequest.value, "Power.Active.Export") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.PowerActiveImport = (strstr(CMSChangeConfigurationRequest.value, "Power.Active.Import") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.PowerOffered = (strstr(CMSChangeConfigurationRequest.value, "Power.Offered") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.PowerReactiveExport = (strstr(CMSChangeConfigurationRequest.value, "Power.Reactive.Export") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.PowerReactiveImport = (strstr(CMSChangeConfigurationRequest.value, "Power.Reactive.Import") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.PowerFactor = (strstr(CMSChangeConfigurationRequest.value, "Power.Factor") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.CurrentExport = (strstr(CMSChangeConfigurationRequest.value, "Current.Export") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.CurrentImport = (strstr(CMSChangeConfigurationRequest.value, "Current.Import") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.CurrentOffered = (strstr(CMSChangeConfigurationRequest.value, "Current.Offered") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.Voltage = (strstr(CMSChangeConfigurationRequest.value, "Voltage") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.Temperature = (strstr(CMSChangeConfigurationRequest.value, "Temperature") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.SoC = (strstr(CMSChangeConfigurationRequest.value, "SoC") != NULL) ? true : false;
                        CPGetConfigurationResponse.MeterValuesSampledData.RPM = (strstr(CMSChangeConfigurationRequest.value, "RPM") != NULL) ? true : false;

                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "MeterValueSampleInterval", strlen("MeterValueSampleInterval")) == 0)
                {
                    if (CPGetConfigurationResponse.MeterValueSampleIntervalReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.MeterValueSampleIntervalValue);
                        memcpy(CPGetConfigurationResponse.MeterValueSampleIntervalValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.MeterValueSampleInterval = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.MeterValueSampleInterval == 0)
                        {
                            CPGetConfigurationResponse.MeterValueSampleInterval = 30;
                            setNULL(CPGetConfigurationResponse.MeterValueSampleIntervalValue);
                            memcpy(CPGetConfigurationResponse.MeterValueSampleIntervalValue, "30", strlen("30"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "NumberOfConnectors", strlen("NumberOfConnectors")) == 0)
                {
                    if (CPGetConfigurationResponse.NumberOfConnectorsReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.NumberOfConnectorsValue);
                        memcpy(CPGetConfigurationResponse.NumberOfConnectorsValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.NumberOfConnectors = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.NumberOfConnectors == 0)
                        {
                            CPGetConfigurationResponse.NumberOfConnectors = 1;
                            setNULL(CPGetConfigurationResponse.NumberOfConnectorsValue);
                            memcpy(CPGetConfigurationResponse.NumberOfConnectorsValue, "1", strlen("1"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "ResetRetries", strlen("ResetRetries")) == 0)
                {
                    if (CPGetConfigurationResponse.ResetRetriesReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.ResetRetriesValue);
                        memcpy(CPGetConfigurationResponse.ResetRetriesValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.ResetRetries = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.ResetRetries == 0)
                        {
                            CPGetConfigurationResponse.ResetRetries = 3;
                            setNULL(CPGetConfigurationResponse.ResetRetriesValue);
                            memcpy(CPGetConfigurationResponse.ResetRetriesValue, "3", strlen("3"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "ConnectorPhaseRotation", strlen("ConnectorPhaseRotation")) == 0)
                {
                    if (CPGetConfigurationResponse.ConnectorPhaseRotationReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.ConnectorPhaseRotationValue);
                        memcpy(CPGetConfigurationResponse.ConnectorPhaseRotationValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "StopTransactionOnEVSideDisconnect", strlen("StopTransactionOnEVSideDisconnect")) == 0)
                {
                    if (CPGetConfigurationResponse.StopTransactionOnEVSideDisconnectReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.StopTransactionOnEVSideDisconnectValue);
                        memcpy(CPGetConfigurationResponse.StopTransactionOnEVSideDisconnectValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.StopTransactionOnEVSideDisconnect = (memcmp(CMSChangeConfigurationRequest.value, "true", strlen("true")) == 0) ? true : false;
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "StopTransactionOnInvalidId", strlen("StopTransactionOnInvalidId")) == 0)
                {
                    if (CPGetConfigurationResponse.StopTransactionOnInvalidIdReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.StopTransactionOnInvalidIdValue);
                        memcpy(CPGetConfigurationResponse.StopTransactionOnInvalidIdValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.StopTransactionOnInvalidId = (memcmp(CMSChangeConfigurationRequest.value, "true", strlen("true")) == 0) ? true : false;
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "StopTxnAlignedData", strlen("StopTxnAlignedData")) == 0)
                {
                    if (CPGetConfigurationResponse.StopTxnAlignedDataReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.StopTxnAlignedDataValue);
                        memcpy(CPGetConfigurationResponse.StopTxnAlignedDataValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.StopTxnAlignedData.EnergyActiveExportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Export.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.EnergyActiveImportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Import.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.EnergyReactiveExportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Export.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.EnergyReactiveImportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Import.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.EnergyActiveExportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Export.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.EnergyActiveImportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Import.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.EnergyReactiveExportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Export.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.EnergyReactiveImportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Import.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.PowerActiveExport = (strstr(CMSChangeConfigurationRequest.value, "Power.Active.Export") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.PowerActiveImport = (strstr(CMSChangeConfigurationRequest.value, "Power.Active.Import") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.PowerOffered = (strstr(CMSChangeConfigurationRequest.value, "Power.Offered") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.PowerReactiveExport = (strstr(CMSChangeConfigurationRequest.value, "Power.Reactive.Export") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.PowerReactiveImport = (strstr(CMSChangeConfigurationRequest.value, "Power.Reactive.Import") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.PowerFactor = (strstr(CMSChangeConfigurationRequest.value, "Power.Factor") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.CurrentExport = (strstr(CMSChangeConfigurationRequest.value, "Current.Export") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.CurrentImport = (strstr(CMSChangeConfigurationRequest.value, "Current.Import") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.CurrentOffered = (strstr(CMSChangeConfigurationRequest.value, "Current.Offered") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.Voltage = (strstr(CMSChangeConfigurationRequest.value, "Voltage") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.Temperature = (strstr(CMSChangeConfigurationRequest.value, "Temperature") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.SoC = (strstr(CMSChangeConfigurationRequest.value, "SoC") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnAlignedData.RPM = (strstr(CMSChangeConfigurationRequest.value, "RPM") != NULL) ? true : false;

                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "StopTxnSampledData", strlen("StopTxnSampledData")) == 0)
                {
                    if (CPGetConfigurationResponse.StopTxnSampledDataReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.StopTxnSampledDataValue);
                        memcpy(CPGetConfigurationResponse.StopTxnSampledDataValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.StopTxnSampledData.EnergyActiveExportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Export.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.EnergyActiveImportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Import.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.EnergyReactiveExportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Export.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.EnergyReactiveImportRegister = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Import.Register") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.EnergyActiveExportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Export.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.EnergyActiveImportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Active.Import.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.EnergyReactiveExportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Export.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.EnergyReactiveImportInterval = (strstr(CMSChangeConfigurationRequest.value, "Energy.Reactive.Import.Interval") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.PowerActiveExport = (strstr(CMSChangeConfigurationRequest.value, "Power.Active.Export") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.PowerActiveImport = (strstr(CMSChangeConfigurationRequest.value, "Power.Active.Import") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.PowerOffered = (strstr(CMSChangeConfigurationRequest.value, "Power.Offered") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.PowerReactiveExport = (strstr(CMSChangeConfigurationRequest.value, "Power.Reactive.Export") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.PowerReactiveImport = (strstr(CMSChangeConfigurationRequest.value, "Power.Reactive.Import") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.PowerFactor = (strstr(CMSChangeConfigurationRequest.value, "Power.Factor") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.CurrentExport = (strstr(CMSChangeConfigurationRequest.value, "Current.Export") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.CurrentImport = (strstr(CMSChangeConfigurationRequest.value, "Current.Import") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.CurrentOffered = (strstr(CMSChangeConfigurationRequest.value, "Current.Offered") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.Voltage = (strstr(CMSChangeConfigurationRequest.value, "Voltage") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.Temperature = (strstr(CMSChangeConfigurationRequest.value, "Temperature") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.SoC = (strstr(CMSChangeConfigurationRequest.value, "SoC") != NULL) ? true : false;
                        CPGetConfigurationResponse.StopTxnSampledData.RPM = (strstr(CMSChangeConfigurationRequest.value, "RPM") != NULL) ? true : false;

                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "SupportedFeatureProfiles", strlen("SupportedFeatureProfiles")) == 0)
                {
                    if (CPGetConfigurationResponse.SupportedFeatureProfilesReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.SupportedFeatureProfilesValue);
                        memcpy(CPGetConfigurationResponse.SupportedFeatureProfilesValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.SupportedFeatureProfiles.Core = (strstr(CMSChangeConfigurationRequest.value, "Core") != NULL) ? true : false;
                        CPGetConfigurationResponse.SupportedFeatureProfiles.LocalAuthListManagement = (strstr(CMSChangeConfigurationRequest.value, "LocalAuthListManagement") != NULL) ? true : false;
                        CPGetConfigurationResponse.SupportedFeatureProfiles.Reservation = (strstr(CMSChangeConfigurationRequest.value, "Reservation") != NULL) ? true : false;
                        CPGetConfigurationResponse.SupportedFeatureProfiles.RemoteTrigger = (strstr(CMSChangeConfigurationRequest.value, "RemoteTrigger") != NULL) ? true : false;
                        CPGetConfigurationResponse.SupportedFeatureProfiles.FirmwareManagement = (strstr(CMSChangeConfigurationRequest.value, "FirmwareManagement") != NULL) ? true : false;
                        CPGetConfigurationResponse.SupportedFeatureProfiles.SmartCharging = (strstr(CMSChangeConfigurationRequest.value, "SmartCharging") != NULL) ? true : false;

                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "TransactionMessageAttempts", strlen("TransactionMessageAttempts")) == 0)
                {
                    if (CPGetConfigurationResponse.TransactionMessageAttemptsReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.TransactionMessageAttemptsValue);
                        memcpy(CPGetConfigurationResponse.TransactionMessageAttemptsValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.TransactionMessageAttempts = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.TransactionMessageAttempts == 0)
                        {
                            CPGetConfigurationResponse.TransactionMessageAttempts = 1;
                            setNULL(CPGetConfigurationResponse.TransactionMessageAttemptsValue);
                            memcpy(CPGetConfigurationResponse.TransactionMessageAttemptsValue, "1", strlen("1"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "TransactionMessageRetryInterval", strlen("TransactionMessageRetryInterval")) == 0)
                {
                    if (CPGetConfigurationResponse.TransactionMessageRetryIntervalReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.TransactionMessageRetryIntervalValue);
                        memcpy(CPGetConfigurationResponse.TransactionMessageRetryIntervalValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.TransactionMessageRetryInterval = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.TransactionMessageRetryInterval == 0)
                        {
                            CPGetConfigurationResponse.TransactionMessageRetryInterval = 60;
                            setNULL(CPGetConfigurationResponse.TransactionMessageRetryIntervalValue);
                            memcpy(CPGetConfigurationResponse.TransactionMessageRetryIntervalValue, "60", strlen("60"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "UnlockConnectorOnEVSideDisconnect", strlen("UnlockConnectorOnEVSideDisconnect")) == 0)
                {
                    if (CPGetConfigurationResponse.UnlockConnectorOnEVSideDisconnectReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.UnlockConnectorOnEVSideDisconnectValue);
                        memcpy(CPGetConfigurationResponse.UnlockConnectorOnEVSideDisconnectValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.UnlockConnectorOnEVSideDisconnect = (memcmp(CMSChangeConfigurationRequest.value, "true", strlen("true")) == 0) ? true : false;
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "AllowOfflineTxForUnknownId", strlen("AllowOfflineTxForUnknownId")) == 0)
                {
                    if (CPGetConfigurationResponse.AllowOfflineTxForUnknownIdReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.AllowOfflineTxForUnknownIdValue);
                        memcpy(CPGetConfigurationResponse.AllowOfflineTxForUnknownIdValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.AllowOfflineTxForUnknownId = (memcmp(CMSChangeConfigurationRequest.value, "true", strlen("true")) == 0) ? true : false;
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "AuthorizationCacheEnabled", strlen("AuthorizationCacheEnabled")) == 0)
                {
                    if (CPGetConfigurationResponse.AuthorizationCacheEnabledReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.AuthorizationCacheEnabledValue);
                        memcpy(CPGetConfigurationResponse.AuthorizationCacheEnabledValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.AuthorizationCacheEnabled = (memcmp(CMSChangeConfigurationRequest.value, "true", strlen("true")) == 0) ? true : false;
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "BlinkRepeat", strlen("BlinkRepeat")) == 0)
                {
                    if (CPGetConfigurationResponse.BlinkRepeatReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.BlinkRepeatValue);
                        memcpy(CPGetConfigurationResponse.BlinkRepeatValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.BlinkRepeat = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.BlinkRepeat == 0)
                        {
                            CPGetConfigurationResponse.BlinkRepeat = 500;
                            setNULL(CPGetConfigurationResponse.BlinkRepeatValue);
                            memcpy(CPGetConfigurationResponse.BlinkRepeatValue, "500", strlen("500"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "LightIntensity", strlen("LightIntensity")) == 0)
                {
                    if (CPGetConfigurationResponse.LightIntensityReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.LightIntensityValue);
                        memcpy(CPGetConfigurationResponse.LightIntensityValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.LightIntensity = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.LightIntensity == 0)
                        {
                            CPGetConfigurationResponse.LightIntensity = 100;
                            setNULL(CPGetConfigurationResponse.LightIntensityValue);
                            memcpy(CPGetConfigurationResponse.LightIntensityValue, "100", strlen("100"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "MaxEnergyOnInvalidId", strlen("MaxEnergyOnInvalidId")) == 0)
                {
                    if (CPGetConfigurationResponse.MaxEnergyOnInvalidIdReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.MaxEnergyOnInvalidIdValue);
                        memcpy(CPGetConfigurationResponse.MaxEnergyOnInvalidIdValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.MaxEnergyOnInvalidId = atoi(CMSChangeConfigurationRequest.value);
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "MeterValuesAlignedDataMaxLength", strlen("MeterValuesAlignedDataMaxLength")) == 0)
                {
                    if (CPGetConfigurationResponse.MeterValuesAlignedDataMaxLengthReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.MeterValuesAlignedDataMaxLengthValue);
                        memcpy(CPGetConfigurationResponse.MeterValuesAlignedDataMaxLengthValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.MeterValuesAlignedDataMaxLength = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.MeterValuesAlignedDataMaxLength > 5)
                        {
                            CPGetConfigurationResponse.MeterValuesAlignedDataMaxLength = 5;
                            setNULL(CPGetConfigurationResponse.MeterValuesAlignedDataMaxLengthValue);
                            memcpy(CPGetConfigurationResponse.MeterValuesAlignedDataMaxLengthValue, "5", strlen("5"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "MeterValuesSampledDataMaxLength", strlen("MeterValuesSampledDataMaxLength")) == 0)
                {
                    if (CPGetConfigurationResponse.MeterValuesSampledDataMaxLengthReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.MeterValuesSampledDataMaxLengthValue);
                        memcpy(CPGetConfigurationResponse.MeterValuesSampledDataMaxLengthValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.MeterValuesSampledDataMaxLength = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.MeterValuesSampledDataMaxLength > 5)
                        {
                            CPGetConfigurationResponse.MeterValuesSampledDataMaxLength = 5;
                            setNULL(CPGetConfigurationResponse.MeterValuesSampledDataMaxLengthValue);
                            memcpy(CPGetConfigurationResponse.MeterValuesSampledDataMaxLengthValue, "5", strlen("5"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "MinimumStatusDuration", strlen("MinimumStatusDuration")) == 0)
                {
                    if (CPGetConfigurationResponse.MinimumStatusDurationReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.MinimumStatusDurationValue);
                        memcpy(CPGetConfigurationResponse.MinimumStatusDurationValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.MinimumStatusDuration = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.MinimumStatusDuration == 0)
                        {
                            CPGetConfigurationResponse.MinimumStatusDuration = 2;
                            setNULL(CPGetConfigurationResponse.MinimumStatusDurationValue);
                            memcpy(CPGetConfigurationResponse.MinimumStatusDurationValue, "2", strlen("2"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "ConnectorPhaseRotationMaxLength", strlen("ConnectorPhaseRotationMaxLength")) == 0)
                {
                    if (CPGetConfigurationResponse.ConnectorPhaseRotationMaxLengthReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.ConnectorPhaseRotationMaxLengthValue);
                        memcpy(CPGetConfigurationResponse.ConnectorPhaseRotationMaxLengthValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.ConnectorPhaseRotationMaxLength = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.ConnectorPhaseRotationMaxLength > 1)
                        {
                            CPGetConfigurationResponse.ConnectorPhaseRotationMaxLength = 1;
                            setNULL(CPGetConfigurationResponse.ConnectorPhaseRotationMaxLengthValue);
                            memcpy(CPGetConfigurationResponse.ConnectorPhaseRotationMaxLengthValue, "1", strlen("1"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "StopTxnAlignedDataMaxLength", strlen("StopTxnAlignedDataMaxLength")) == 0)
                {
                    if (CPGetConfigurationResponse.StopTxnAlignedDataMaxLengthReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.StopTxnAlignedDataMaxLengthValue);
                        memcpy(CPGetConfigurationResponse.StopTxnAlignedDataMaxLengthValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.StopTxnAlignedDataMaxLength = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.StopTxnAlignedDataMaxLength > 5)
                        {
                            CPGetConfigurationResponse.StopTxnAlignedDataMaxLength = 5;
                            setNULL(CPGetConfigurationResponse.StopTxnAlignedDataMaxLengthValue);
                            memcpy(CPGetConfigurationResponse.StopTxnAlignedDataMaxLengthValue, "5", strlen("5"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "StopTxnSampledDataMaxLength", strlen("StopTxnSampledDataMaxLength")) == 0)
                {
                    if (CPGetConfigurationResponse.StopTxnSampledDataMaxLengthReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.StopTxnSampledDataMaxLengthValue);
                        memcpy(CPGetConfigurationResponse.StopTxnSampledDataMaxLengthValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.StopTxnSampledDataMaxLength = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.StopTxnSampledDataMaxLength > 5)
                        {
                            CPGetConfigurationResponse.StopTxnSampledDataMaxLength = 5;
                            setNULL(CPGetConfigurationResponse.StopTxnSampledDataMaxLengthValue);
                            memcpy(CPGetConfigurationResponse.StopTxnSampledDataMaxLengthValue, "5", strlen("5"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "SupportedFeatureProfilesMaxLength", strlen("SupportedFeatureProfilesMaxLength")) == 0)
                {
                    if (CPGetConfigurationResponse.SupportedFeatureProfilesMaxLengthReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.SupportedFeatureProfilesMaxLengthValue);
                        memcpy(CPGetConfigurationResponse.SupportedFeatureProfilesMaxLengthValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.SupportedFeatureProfilesMaxLength = atoi(CMSChangeConfigurationRequest.value);
                        if (CPGetConfigurationResponse.SupportedFeatureProfilesMaxLength > 6)
                        {
                            CPGetConfigurationResponse.SupportedFeatureProfilesMaxLength = 6;
                            setNULL(CPGetConfigurationResponse.SupportedFeatureProfilesMaxLengthValue);
                            memcpy(CPGetConfigurationResponse.SupportedFeatureProfilesMaxLengthValue, "6", strlen("6"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "WebSocketPingInterval", strlen("WebSocketPingInterval")) == 0)
                {
                    if (CPGetConfigurationResponse.WebSocketPingIntervalReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.WebSocketPingIntervalValue);
                        memcpy(CPGetConfigurationResponse.WebSocketPingIntervalValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.WebSocketPingInterval = atoi(CMSChangeConfigurationRequest.value);
                        if ((CPGetConfigurationResponse.WebSocketPingInterval < 10) || (CPGetConfigurationResponse.WebSocketPingInterval > 180))
                        {
                            CPGetConfigurationResponse.WebSocketPingInterval = 30;
                            setNULL(CPGetConfigurationResponse.WebSocketPingIntervalValue);
                            memcpy(CPGetConfigurationResponse.WebSocketPingIntervalValue, "30", strlen("30"));
                        }
                        setWebsocketPingInterval();
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "LocalAuthListEnabled", strlen("LocalAuthListEnabled")) == 0)
                {
                    if (CPGetConfigurationResponse.LocalAuthListEnabledReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.LocalAuthListEnabledValue);
                        memcpy(CPGetConfigurationResponse.LocalAuthListEnabledValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.LocalAuthListEnabled = (memcmp(CMSChangeConfigurationRequest.value, "true", strlen("true")) == 0) ? true : false;
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "LocalAuthListMaxLength", strlen("LocalAuthListMaxLength")) == 0)
                {
                    if (CPGetConfigurationResponse.LocalAuthListMaxLengthReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.LocalAuthListMaxLengthValue);
                        memcpy(CPGetConfigurationResponse.LocalAuthListMaxLengthValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.LocalAuthListMaxLength = atoi(CMSChangeConfigurationRequest.value);
                        if ((CPGetConfigurationResponse.LocalAuthListMaxLength < 1) || (CPGetConfigurationResponse.LocalAuthListMaxLength > 20))
                        {
                            CPGetConfigurationResponse.LocalAuthListMaxLength = 10;
                            setNULL(CPGetConfigurationResponse.LocalAuthListMaxLengthValue);
                            memcpy(CPGetConfigurationResponse.LocalAuthListMaxLengthValue, "10", strlen("10"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "SendLocalListMaxLength", strlen("SendLocalListMaxLength")) == 0)
                {
                    if (CPGetConfigurationResponse.SendLocalListMaxLengthReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.SendLocalListMaxLengthValue);
                        memcpy(CPGetConfigurationResponse.SendLocalListMaxLengthValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.SendLocalListMaxLength = atoi(CMSChangeConfigurationRequest.value);
                        if ((CPGetConfigurationResponse.SendLocalListMaxLength < 1) || (CPGetConfigurationResponse.SendLocalListMaxLength > 20))
                        {
                            CPGetConfigurationResponse.SendLocalListMaxLength = 10;
                            setNULL(CPGetConfigurationResponse.SendLocalListMaxLengthValue);
                            memcpy(CPGetConfigurationResponse.SendLocalListMaxLengthValue, "10", strlen("10"));
                        }
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "ReserveConnectorZeroSupported", strlen("ReserveConnectorZeroSupported")) == 0)
                {
                    if (CPGetConfigurationResponse.ReserveConnectorZeroSupportedReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.ReserveConnectorZeroSupportedValue);
                        memcpy(CPGetConfigurationResponse.ReserveConnectorZeroSupportedValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.ReserveConnectorZeroSupported = (memcmp(CMSChangeConfigurationRequest.value, "true", strlen("true")) == 0) ? true : false;
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "ChargeProfileMaxStackLevel", strlen("ChargeProfileMaxStackLevel")) == 0)
                {
                    if (CPGetConfigurationResponse.ChargeProfileMaxStackLevelReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.ChargeProfileMaxStackLevelValue);
                        memcpy(CPGetConfigurationResponse.ChargeProfileMaxStackLevelValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.ChargeProfileMaxStackLevel = atoi(CMSChangeConfigurationRequest.value);
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "ConnectorSwitch3to1PhaseSupported", strlen("ConnectorSwitch3to1PhaseSupported")) == 0)
                {
                    if (CPGetConfigurationResponse.ConnectorSwitch3to1PhaseSupportedReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.ConnectorSwitch3to1PhaseSupportedValue);
                        memcpy(CPGetConfigurationResponse.ConnectorSwitch3to1PhaseSupportedValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.ConnectorSwitch3to1PhaseSupported = (memcmp(CMSChangeConfigurationRequest.value, "true", strlen("true")) == 0) ? true : false;
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "ChargingScheduleMaxPeriods", strlen("ChargingScheduleMaxPeriods")) == 0)
                {
                    if (CPGetConfigurationResponse.ChargingScheduleMaxPeriodsReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.ChargingScheduleMaxPeriodsValue);
                        memcpy(CPGetConfigurationResponse.ChargingScheduleMaxPeriodsValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.ChargingScheduleMaxPeriods = atoi(CMSChangeConfigurationRequest.value);
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else if (memcmp(CMSChangeConfigurationRequest.key, "MaxChargingProfilesInstalled", strlen("MaxChargingProfilesInstalled")) == 0)
                {
                    if (CPGetConfigurationResponse.MaxChargingProfilesInstalledReadOnly == false)
                    {
                        setNULL(CPGetConfigurationResponse.MaxChargingProfilesInstalledValue);
                        memcpy(CPGetConfigurationResponse.MaxChargingProfilesInstalledValue, CMSChangeConfigurationRequest.value, strlen(CMSChangeConfigurationRequest.value));
                        CPGetConfigurationResponse.MaxChargingProfilesInstalled = atoi(CMSChangeConfigurationRequest.value);
                        memcpy(CPChangeConfigurationResponse.status, ACCEPTED, strlen(ACCEPTED));
                    }
                    else
                    {
                        memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                    }
                }
                else
                {
                    memcpy(CPChangeConfigurationResponse.status, REJECTED, strlen(REJECTED));
                }
                sendChangeConfigurationResponse();
                custom_nvs_write_config_ocpp();
                CMSChangeConfigurationRequest.Received = false;
            }
            if (CMSResetRequest.Received)
            {
                if (memcmp(CMSResetRequest.type, Soft, strlen(Soft)) == 0)
                {
                    if ((((taskState[1] == flag_EVSE_Read_Id_Tag) && (connectorEnabled[1])) || ((connectorEnabled[1] == false))) &&
                        (((taskState[2] == flag_EVSE_Read_Id_Tag) && (connectorEnabled[2])) || ((connectorEnabled[2] == false))) &&
                        (((taskState[3] == flag_EVSE_Read_Id_Tag) && (connectorEnabled[3])) || ((connectorEnabled[3] == false))))
                    {
                        setNULL(CPResetResponse.status);
                        memcpy(CPResetResponse.status, ACCEPTED, strlen(ACCEPTED));
                        sendResetResponse();
                        CMSResetRequest.Received = false;
                        vTaskDelay(2000 / portTICK_PERIOD_MS);
                        esp_restart();
                    }
                    else
                    {
                        for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
                        {
                            if (taskState[1] == flag_EVSE_Request_Charge)
                            {
                                ConnectorData[i].stopReason = Stop_SoftReset;
                                taskState[i] = flag_EVSE_Stop_Transaction;
                            }
                        }
                    }
                }
                else if (memcmp(CMSResetRequest.type, Hard, strlen(Hard)) == 0)
                {
                    memcpy(CPResetResponse.status, ACCEPTED, strlen(ACCEPTED));
                    sendResetResponse();
                    for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
                    {
                        if (taskState[i] == flag_EVSE_Request_Charge)
                        {
                            if (cpEnable[i])
                                SlaveSetControlPilotDuty(i, 100);
                            else
                                SlaveUpdateRelay(i, 0);
                            ConnectorData[i].stopReason = Stop_HardReset;
                            custom_nvs_write_connector_data(i);
                        }
                    }
                    CMSResetRequest.Received = false;
                    vTaskDelay(2000 / portTICK_PERIOD_MS);
                    esp_restart();
                }
            }
            if (CMSUpdateFirmwareRequest.Received)
            {
                sendUpdateFirmwareResponse();
                CMSUpdateFirmwareRequest.Received = false;
                if (config.otaEnable)
                {
                    if (((taskState[1] == flag_EVSE_Read_Id_Tag) || (connectorEnabled[1] == false)) &&
                        ((taskState[2] == flag_EVSE_Read_Id_Tag) || (connectorEnabled[2] == false)) &&
                        ((taskState[3] == flag_EVSE_Read_Id_Tag) || (connectorEnabled[3] == false)))
                    {
                        UpdateFirmware();
                    }
                    else
                        updatingFirmwarePending = true;
                }
            }
            if (CMSClearChargingProfileRequest.Received)
            {
                sendClearChargingProfileResponse();
                custom_nvs_clear_chargingProfile(CMSClearChargingProfileRequest.id);
                CMSClearChargingProfileRequest.Received = false;
            }
            if (CMSDataTransferRequest.Received)
            {
                sendDataTransferResponse();
                CMSDataTransferRequest.Received = false;
            }
            if (CMSGetCompositeScheduleRequest.Received)
            {
                sendGetCompositeScheduleResponse();
                CMSGetCompositeScheduleRequest.Received = false;
            }
            if (CMSGetDiagnosticsRequest.Received)
            {
                updateDiagnosticsfileName();
                sendGetDiagnosticsResponse();
                sendLogsToCloud();
                CMSGetDiagnosticsRequest.Received = false;
            }
            if (CMSSetChargingProfileRequest.Received)
            {
                sendSetChargingProfileResponse();
                CMSSetChargingProfileRequest.Received = false;
            }
            if (CMSUnlockConnectorRequest.Received)
            {
                sendUnlockConnectorResponse();
                CMSUnlockConnectorRequest.Received = false;
            }
            if (updatingFirmwarePending)
            {
                if (((taskState[1] == flag_EVSE_Read_Id_Tag) || (connectorEnabled[1] == false)) &&
                    ((taskState[2] == flag_EVSE_Read_Id_Tag) || (connectorEnabled[2] == false)) &&
                    ((taskState[3] == flag_EVSE_Read_Id_Tag) || (connectorEnabled[3] == false)))
                {
                    updatingFirmwarePending = false;
                    UpdateFirmware();
                }
            }
        }
        loopCount++;
#if TEST
        loopCountlogs++;
        if ((loopCountlogs % 300 == 0) && (config.redirectLogs) && isWebsocketConnected && isWebsocketBooted)
        {
            updateDiagnosticsfileName();
            printf(CPGetDiagnosticsResponse.fileName);
            sendLogsToCloud();
        }
#endif
        if ((isWebsocketConnected_Now == false) &
            (isPoweredOn == false) &
            (loopCount % 10 == 0))
        {
            setNULL(time_buffer);
            getTimeInOcppFormat();
            custom_nvs_write_timeBuffer();
        }
        if (newRfidRead)
        {
            if ((newRfidRead != newRfidRead_old) && newRfidRead)
            {
                for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
                {
                    rfidTappedBlinkLed[i] = true;
                    ESP_LOGD(TAG, "Con %hhu RFID LED SET", i);
                }
            }

            if (!((isWebsocketConnected_Now && isWebsocketBooted && (config.onlineOffline || config.online)) ||
                  ((isWebsocketConnected_Now == false) && (config.onlineOffline || config.offline))))
                newRfidRead = false;
            for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
            {
                if (connectorFault[i] == 1)
                {
                    newRfidRead = false;
                    setNULL(rfidTagNumberbuf);
                    memset(rfidTagNumberbuf, 0xFF, 10);
                }
            }

            if ((newRfidRead) && (rfidTappedSentToDisplay == false))
            {
                checkForChargingState = false;
                for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
                {
                    if (taskState[i] == flag_EVSE_Request_Charge)
                    {
                        checkForChargingState = true;
                    }
                }
                if (checkForChargingState == true)
                {
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                }
            }
        }

        newRfidRead_old = newRfidRead;
        if (newRfidRead)
        {
            if (RfidTappedFirstProcessed == false)
            {
                RfidTappedFirstProcessed = true;
                for (uint8_t i = 1; i < (config.NumberOfConnectors + 1); i++)
                {
                    if ((cpState[i] == STATE_B1) || (cpState[i] == STATE_E2))
                    {
                        isEvPluggedFirst = true;
                    }
                }
                isRfidTappedFirst = !isEvPluggedFirst;
            }
            if (rfidTappedSentToDisplay == false)
            {
                SetChargerRfidTappedBit();
                rfidTappedSentToDisplay = true;
            }
            if (rfidTappedStateProcessed == false)
            {
                processRfidTappedState();
            }
            rfidReadTimeCount++;
            if (rfidReadTimeCount >= 120)
            {
                rfidTappedStateProcessed = false;
                rfidReceivedFromRemote[0] = false;
                rfidReceivedFromRemote[1] = false;
                rfidReceivedFromRemote[2] = false;
                rfidReceivedFromRemote[3] = false;
                rfidMatchingWithLocalList = false;
                rfidAuthenticated = false;
                RemoteAuthenticated = false;
                AlprAuthenticated = false;
                rfidReadTimeCount = 0;
                newRfidRead = false;
            }
        }
        else
        {
            OngoingTransaction = false;
            RfidTappedFirstProcessed = false;
            rfidTappedStateProcessed = false;
            rfidTappedSentToDisplay = false;
            isRfidLedProcessed[0] = false;
            isRfidLedProcessed[1] = false;
            isRfidLedProcessed[2] = false;
            isRfidLedProcessed[3] = false;
            rfidReadTimeCount = 0;
        }
        if (FirmwareUpdateFailed && FirmwareUpdateStarted)
        {
            ESP_LOGI(TAG, "Firmware Update Failed Reset");
            FirmwareUpdateFailed = false;
            FirmwareUpdateStarted = false;
            isWebsocketConnected_old = !isWebsocketConnected_Now;
        }
        for (uint8_t connId = 1; connId <= config.NumberOfConnectors; connId++)
        {
            if ((connectorEnabled[connId]) & (taskState[connId] >= flag_EVSE_is_Booted) & (FirmwareUpdateStarted == false))
            {
                if (ConnectorData[connId].isReserved & (loopCount % 10 == 0))
                {
                    sscanf(ConnectorData[connId].ReservedData.expiryDate, "%d-%d-%dT%d:%d:%d.%*3sZ",
                           &tm_time_Reserved.tm_year, &tm_time_Reserved.tm_mon, &tm_time_Reserved.tm_mday,
                           &tm_time_Reserved.tm_hour, &tm_time_Reserved.tm_min, &tm_time_Reserved.tm_sec);

                    tm_time_Reserved.tm_year -= 1900; // Adjust year
                    tm_time_Reserved.tm_mon -= 1;     // Adjust month

                    unix_time = mktime(&tm_time_Reserved);
                    tvReserved.tv_sec = unix_time;

                    setNULL(time_buffer);
                    getTimeInOcppFormat();

                    sscanf(time_buffer, "%d-%d-%dT%d:%d:%d.%*3sZ",
                           &tm_time_Now.tm_year, &tm_time_Now.tm_mon, &tm_time_Now.tm_mday,
                           &tm_time_Now.tm_hour, &tm_time_Now.tm_min, &tm_time_Now.tm_sec);

                    tm_time_Now.tm_year -= 1900; // Adjust year
                    tm_time_Now.tm_mon -= 1;     // Adjust month

                    unix_time = mktime(&tm_time_Now);
                    tvNow.tv_sec = unix_time;

                    if (tvReserved.tv_sec < tvNow.tv_sec)
                    {
                        ConnectorData[connId].isReserved = false;
                        custom_nvs_write_connector_data(connId);
                        connectorsOfflineDataSent[connId] = true;
                    }
                }
                if (connectorsOfflineDataSent[connId])
                {
                    emergencyButton_old[connId] = false;
                    relayWeldDetectionButton_old[connId] = false;
                    gfciButton_old[connId] = false;
                    earthDisconnectButton_old[connId] = false;
                    reservedStatusSent[connId] = false;
                    memset(&MeterFaults_old[connId], 0, sizeof(MeterFaults_t));
                    cpState_old[connId] = STARTUP;
                    connectorsOfflineDataSent[connId] = false;
                }
                if (updateStatusAfterBoot[connId])
                {
                    updateStatusAfterBoot[connId] = false;
                    emergencyButton_old[connId] = false;
                    relayWeldDetectionButton_old[connId] = false;
                    gfciButton_old[connId] = false;
                    earthDisconnectButton_old[connId] = false;
                    reservedStatusSent[connId] = false;
                    memset(&MeterFaults_old[connId], 0, sizeof(MeterFaults_t));
                    cpState_old[connId] = STARTUP;
                }
                if (bootedNow)
                {
                    updateStatusAfterBoot[connId] = false;
                    emergencyButton_old[connId] = false;
                    gfciButton_old[connId] = false;
                    earthDisconnectButton_old[connId] = false;
                    reservedStatusSent[connId] = false;
                    memset(&MeterFaults_old[connId], 0, sizeof(MeterFaults_t));
                    cpState_old[connId] = STARTUP;
                    StatusState[connId] = STATUS_STARTUP;
                }
                CPStatusNotificationRequest[connId].connectorId = connId;
                MeterFaults[connId] = readMeterFaults(connId);
                if ((emergencyButton == 1) ||
                    (relayWeldDetectionButton == 1) ||
                    (earthDisconnectButton == 1) ||
                    (gfciButton == 1) ||
                    (MeterFaults[connId].MeterPowerLoss == 1) ||
                    (MeterFaults[connId].MeterHighTemp == 1) ||
                    (MeterFaults[connId].MeterOverVoltage == 1) ||
                    (MeterFaults[connId].MeterUnderVoltage == 1) ||
                    (MeterFaults[connId].MeterOverCurrent == 1))
                {
                    // cpState[connId] = FAULT;
                    connectorFault[connId] = 1;
                    if ((MeterFaults[connId].MeterPowerLoss == 0) && (MeterFaults[connId].MeterPowerLoss != MeterFaults_old[connId].MeterPowerLoss))
                    {
                        MeterFaults_old[connId].MeterPowerLoss = 0;
                        emergencyButton_old[connId] = 0;
                        gfciButton_old[connId] = 0;
                        earthDisconnectButton_old[connId] = 0;
                        MeterFaults_old[connId].MeterOverVoltage = 0;
                        MeterFaults_old[connId].MeterUnderVoltage = 0;
                        MeterFaults_old[connId].MeterOverCurrent = 0;
                        MeterFaults_old[connId].MeterHighTemp = 0;
                        MeterFaults_old[connId].MeterFailure = 0;
                        relayWeldDetectionButton_old[connId] = 0;
                    }
                    else if ((emergencyButton == 0) && (emergencyButton != emergencyButton_old[connId]))
                    {
                        emergencyButton_old[connId] = 0;
                        gfciButton_old[connId] = 0;
                        earthDisconnectButton_old[connId] = 0;
                        MeterFaults_old[connId].MeterOverVoltage = 0;
                        MeterFaults_old[connId].MeterUnderVoltage = 0;
                        MeterFaults_old[connId].MeterOverCurrent = 0;
                        MeterFaults_old[connId].MeterHighTemp = 0;
                        MeterFaults_old[connId].MeterFailure = 0;
                        relayWeldDetectionButton_old[connId] = 0;
                    }
                    else if ((gfciButton == 0) && (gfciButton != gfciButton_old[connId]))
                    {
                        gfciButton_old[connId] = 0;
                        earthDisconnectButton_old[connId] = 0;
                        MeterFaults_old[connId].MeterOverVoltage = 0;
                        MeterFaults_old[connId].MeterUnderVoltage = 0;
                        MeterFaults_old[connId].MeterOverCurrent = 0;
                        MeterFaults_old[connId].MeterHighTemp = 0;
                        MeterFaults_old[connId].MeterFailure = 0;
                        relayWeldDetectionButton_old[connId] = 0;
                    }
                    else if ((earthDisconnectButton == 0) && (earthDisconnectButton != earthDisconnectButton_old[connId]))
                    {
                        earthDisconnectButton_old[connId] = 0;
                        MeterFaults_old[connId].MeterOverVoltage = 0;
                        MeterFaults_old[connId].MeterUnderVoltage = 0;
                        MeterFaults_old[connId].MeterOverCurrent = 0;
                        MeterFaults_old[connId].MeterHighTemp = 0;
                        MeterFaults_old[connId].MeterFailure = 0;
                        relayWeldDetectionButton_old[connId] = 0;
                    }
                    else if ((MeterFaults[connId].MeterOverVoltage == 0) && (MeterFaults[connId].MeterOverVoltage != MeterFaults_old[connId].MeterOverVoltage))
                    {
                        MeterFaults_old[connId].MeterOverVoltage = 0;
                        MeterFaults_old[connId].MeterUnderVoltage = 0;
                        MeterFaults_old[connId].MeterOverCurrent = 0;
                        MeterFaults_old[connId].MeterHighTemp = 0;
                        MeterFaults_old[connId].MeterFailure = 0;
                        relayWeldDetectionButton_old[connId] = 0;
                    }
                    else if ((MeterFaults[connId].MeterUnderVoltage == 0) && (MeterFaults[connId].MeterUnderVoltage != MeterFaults_old[connId].MeterUnderVoltage))
                    {
                        MeterFaults_old[connId].MeterUnderVoltage = 0;
                        MeterFaults_old[connId].MeterOverCurrent = 0;
                        MeterFaults_old[connId].MeterHighTemp = 0;
                        MeterFaults_old[connId].MeterFailure = 0;
                        relayWeldDetectionButton_old[connId] = 0;
                    }
                    else if ((MeterFaults[connId].MeterOverCurrent == 0) && (MeterFaults[connId].MeterOverCurrent != MeterFaults_old[connId].MeterOverCurrent))
                    {
                        MeterFaults_old[connId].MeterOverCurrent = 0;
                        MeterFaults_old[connId].MeterHighTemp = 0;
                        MeterFaults_old[connId].MeterFailure = 0;
                        relayWeldDetectionButton_old[connId] = 0;
                    }
                    else if ((MeterFaults[connId].MeterHighTemp == 0) && (MeterFaults[connId].MeterHighTemp != MeterFaults_old[connId].MeterHighTemp))
                    {
                        MeterFaults_old[connId].MeterHighTemp = 0;
                        MeterFaults_old[connId].MeterFailure = 0;
                        relayWeldDetectionButton_old[connId] = 0;
                    }
                    else if ((MeterFaults[connId].MeterFailure == 0) && (MeterFaults[connId].MeterFailure != MeterFaults_old[connId].MeterFailure))
                    {
                        MeterFaults_old[connId].MeterFailure = 0;
                        relayWeldDetectionButton_old[connId] = 0;
                    }
                    else if ((relayWeldDetectionButton == 0) && (relayWeldDetectionButton != relayWeldDetectionButton_old[connId]))
                    {
                        relayWeldDetectionButton_old[connId] = 0;
                    }

                    if (MeterFaults[connId].MeterPowerLoss == 1)
                    {
                        emergencyButton_old[connId] = emergencyButton;
                        gfciButton_old[connId] = gfciButton;
                        earthDisconnectButton_old[connId] = earthDisconnectButton;
                        MeterFaults_old[connId].MeterOverVoltage = MeterFaults[connId].MeterOverVoltage;
                        MeterFaults_old[connId].MeterUnderVoltage = MeterFaults[connId].MeterUnderVoltage;
                        MeterFaults_old[connId].MeterOverCurrent = MeterFaults[connId].MeterOverCurrent;
                        MeterFaults_old[connId].MeterHighTemp = MeterFaults[connId].MeterHighTemp;
                        MeterFaults_old[connId].MeterFailure = MeterFaults[connId].MeterFailure;
                        relayWeldDetectionButton_old[connId] = relayWeldDetectionButton;
                    }
                    else if (emergencyButton == 1)
                    {
                        gfciButton_old[connId] = gfciButton;
                        earthDisconnectButton_old[connId] = earthDisconnectButton;
                        MeterFaults_old[connId].MeterOverVoltage = MeterFaults[connId].MeterOverVoltage;
                        MeterFaults_old[connId].MeterUnderVoltage = MeterFaults[connId].MeterUnderVoltage;
                        MeterFaults_old[connId].MeterOverCurrent = MeterFaults[connId].MeterOverCurrent;
                        MeterFaults_old[connId].MeterHighTemp = MeterFaults[connId].MeterHighTemp;
                        MeterFaults_old[connId].MeterFailure = MeterFaults[connId].MeterFailure;
                        relayWeldDetectionButton_old[connId] = relayWeldDetectionButton;
                    }
                    else if (gfciButton == 1)
                    {
                        earthDisconnectButton_old[connId] = earthDisconnectButton;
                        MeterFaults_old[connId].MeterOverVoltage = MeterFaults[connId].MeterOverVoltage;
                        MeterFaults_old[connId].MeterUnderVoltage = MeterFaults[connId].MeterUnderVoltage;
                        MeterFaults_old[connId].MeterOverCurrent = MeterFaults[connId].MeterOverCurrent;
                        MeterFaults_old[connId].MeterHighTemp = MeterFaults[connId].MeterHighTemp;
                        MeterFaults_old[connId].MeterFailure = MeterFaults[connId].MeterFailure;
                        relayWeldDetectionButton_old[connId] = relayWeldDetectionButton;
                    }
                    else if (earthDisconnectButton == 1)
                    {
                        MeterFaults_old[connId].MeterOverVoltage = MeterFaults[connId].MeterOverVoltage;
                        MeterFaults_old[connId].MeterUnderVoltage = MeterFaults[connId].MeterUnderVoltage;
                        MeterFaults_old[connId].MeterOverCurrent = MeterFaults[connId].MeterOverCurrent;
                        MeterFaults_old[connId].MeterHighTemp = MeterFaults[connId].MeterHighTemp;
                        MeterFaults_old[connId].MeterFailure = MeterFaults[connId].MeterFailure;
                        relayWeldDetectionButton_old[connId] = relayWeldDetectionButton;
                    }
                    else if (MeterFaults[connId].MeterOverVoltage == 1)
                    {
                        MeterFaults_old[connId].MeterUnderVoltage = MeterFaults[connId].MeterUnderVoltage;
                        MeterFaults_old[connId].MeterOverCurrent = MeterFaults[connId].MeterOverCurrent;
                        MeterFaults_old[connId].MeterHighTemp = MeterFaults[connId].MeterHighTemp;
                        MeterFaults_old[connId].MeterFailure = MeterFaults[connId].MeterFailure;
                        relayWeldDetectionButton_old[connId] = relayWeldDetectionButton;
                    }
                    else if (MeterFaults[connId].MeterUnderVoltage == 1)
                    {
                        MeterFaults_old[connId].MeterOverCurrent = MeterFaults[connId].MeterOverCurrent;
                        MeterFaults_old[connId].MeterHighTemp = MeterFaults[connId].MeterHighTemp;
                        MeterFaults_old[connId].MeterFailure = MeterFaults[connId].MeterFailure;
                        relayWeldDetectionButton_old[connId] = relayWeldDetectionButton;
                    }
                    else if (MeterFaults[connId].MeterOverCurrent == 1)
                    {
                        MeterFaults_old[connId].MeterHighTemp = MeterFaults[connId].MeterHighTemp;
                        MeterFaults_old[connId].MeterFailure = MeterFaults[connId].MeterFailure;
                        relayWeldDetectionButton_old[connId] = relayWeldDetectionButton;
                    }
                    else if (MeterFaults[connId].MeterHighTemp == 1)
                    {
                        MeterFaults_old[connId].MeterFailure = MeterFaults[connId].MeterFailure;
                        relayWeldDetectionButton_old[connId] = relayWeldDetectionButton;
                    }
                    else if (MeterFaults[connId].MeterFailure == 1)
                    {
                        relayWeldDetectionButton_old[connId] = relayWeldDetectionButton;
                    }
                }
                else
                {
                    connectorFault[connId] = 0;
                }
                if ((connectorFault[connId] != connectorFault_old[connId]) && (connectorFault[connId] == 0))
                {
                    if ((connectorFault_old[connId] == 1) && (MeterFaults_old[connId].MeterPowerLoss == 1) && (connId == 1))
                    {
                        LCD_DisplayReinit();
                    }
                    if (taskState[connId] == flag_EVSE_Request_Charge)
                    {
                        isFaultRecovered[connId] = true;
                    }
                    if (cpEnable[connId])
                        SlaveSetControlPilotDuty(connId, 100);
                    else
                        SlaveUpdateRelay(connId, 0);
                    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                    SetControlPilotDuty(100);
                    xSemaphoreGive(mutexControlPilot);
                    // cpState[connId] = STATE_A;
                    cpState_old[connId] = STARTUP;
                    StatusState[connId] = STATUS_STARTUP;
                    finishingStatusSent[connId] = false;
                    if (ConnectorData[connId].CmsAvailable == false)
                    {
                        ConnectorData[connId].CmsAvailableChanged = true;
                    }
                    else if (ConnectorData[connId].isReserved)
                    {
                        reservedStatusSent[connId] = false;
                    }
                }
                else if ((connectorFault[connId] != connectorFault_old[connId]) && (connectorFault[connId] == 1))
                {

                    ESP_LOGE(TAG, "Connector %d FAULTED", connId);
                }

                if ((MeterFaults[connId].MeterPowerLoss == 1) && ((MeterFaults[connId].MeterPowerLoss != MeterFaults_old[connId].MeterPowerLoss) || (isWebsocketConnected_Now != isWebsocketConnected_old)))
                {
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        setNULL(CPStatusNotificationRequest[connId].vendorErrorCode);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, OtherError, strlen(OtherError));
                        memcpy(CPStatusNotificationRequest[connId].status, Faulted, strlen(Faulted));
                        memcpy(CPStatusNotificationRequest[connId].vendorErrorCode, PowerLoss, strlen(PowerLoss));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);
                        if (config.onlineOffline)
                            ledState[connId] = FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_FAULT_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline)
                            ledState[connId] = OFFLINE_FAULT_LED_STATE;
                        else if (config.offline)
                            ledState[connId] = OFFLINE_ONLY_FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_FAULT_LED_STATE;
                    }
                    if (cpEnable[connId])
                        SlaveSetControlPilotDuty(connId, 100);
                    else
                        SlaveUpdateRelay(connId, 0);
                    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                    SetControlPilotDuty(100);
                    xSemaphoreGive(mutexControlPilot);
                    setChargerFaultBit(connId);
                    ESP_LOGE(TAG_STATUS, "Connector %d Power Loss Fault", connId);
                }
                else if ((emergencyButton == 1) && ((emergencyButton != emergencyButton_old[connId]) || (isWebsocketConnected_Now != isWebsocketConnected_old)))
                {
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        setNULL(CPStatusNotificationRequest[connId].vendorErrorCode);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, OtherError, strlen(OtherError));
                        memcpy(CPStatusNotificationRequest[connId].status, Faulted, strlen(Faulted));
                        memcpy(CPStatusNotificationRequest[connId].vendorErrorCode, Emergency, strlen(Emergency));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);
                        if (config.onlineOffline)
                            ledState[connId] = EMERGENCY_FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_EMERGENCY_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline)
                            ledState[connId] = OFFLINE_EMERGENCY_FAULT_LED_STATE;
                        else if (config.offline)
                            ledState[connId] = OFFLINE_ONLY_EMERGENCY_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_EMERGENCY_LED_STATE;
                    }
                    if (cpEnable[connId])
                        SlaveSetControlPilotDuty(connId, 100);
                    else
                        SlaveUpdateRelay(connId, 0);
                    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                    SetControlPilotDuty(100);
                    xSemaphoreGive(mutexControlPilot);
                    setChargerFaultBit(connId);
                    ESP_LOGW(TAG_STATUS, "Connector %d Emergency State", connId);
                }
                else if ((gfciButton == 1) && ((gfciButton != gfciButton_old[connId]) || (isWebsocketConnected_Now != isWebsocketConnected_old)))
                {
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, GroundFailure, strlen(GroundFailure));
                        memcpy(CPStatusNotificationRequest[connId].status, Faulted, strlen(Faulted));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);
                        if (config.onlineOffline)
                            ledState[connId] = FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_FAULT_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline)
                            ledState[connId] = OFFLINE_FAULT_LED_STATE;
                        else if (config.offline)
                            ledState[connId] = OFFLINE_ONLY_FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_FAULT_LED_STATE;
                    }
                    if (cpEnable[connId])
                        SlaveSetControlPilotDuty(connId, 100);
                    else
                        SlaveUpdateRelay(connId, 0);
                    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                    SetControlPilotDuty(100);
                    xSemaphoreGive(mutexControlPilot);
                    setChargerFaultBit(connId);
                    ESP_LOGE(TAG_STATUS, "Connector %d GFCI Fault", connId);
                }
                else if ((earthDisconnectButton == 1) && ((earthDisconnectButton != earthDisconnectButton_old[connId]) || (isWebsocketConnected_Now != isWebsocketConnected_old)))
                {
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        setNULL(CPStatusNotificationRequest[connId].vendorErrorCode);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, OtherError, strlen(OtherError));
                        memcpy(CPStatusNotificationRequest[connId].status, Faulted, strlen(Faulted));
                        memcpy(CPStatusNotificationRequest[connId].vendorErrorCode, EarthDisconnect, strlen(EarthDisconnect));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);
                        if (config.onlineOffline)
                            ledState[connId] = FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_FAULT_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline)
                            ledState[connId] = OFFLINE_FAULT_LED_STATE;
                        else if (config.offline)
                            ledState[connId] = OFFLINE_ONLY_FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_FAULT_LED_STATE;
                    }
                    if (cpEnable[connId])
                        SlaveSetControlPilotDuty(connId, 100);
                    else
                        SlaveUpdateRelay(connId, 0);
                    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                    SetControlPilotDuty(100);
                    xSemaphoreGive(mutexControlPilot);
                    setChargerFaultBit(connId);
                    ESP_LOGE(TAG_STATUS, "Connector %d Earth Disconnect Fault", connId);
                }
                else if ((MeterFaults[connId].MeterOverVoltage == 1) && ((MeterFaults[connId].MeterOverVoltage != MeterFaults_old[connId].MeterOverVoltage) || (isWebsocketConnected_Now != isWebsocketConnected_old)))
                {
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, OverVoltage, strlen(OverVoltage));
                        memcpy(CPStatusNotificationRequest[connId].status, Faulted, strlen(Faulted));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);
                        if (config.onlineOffline)
                            ledState[connId] = FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_FAULT_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline)
                            ledState[connId] = OFFLINE_FAULT_LED_STATE;
                        else if (config.offline)
                            ledState[connId] = OFFLINE_ONLY_FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_FAULT_LED_STATE;
                    }
                    if (cpEnable[connId])
                        SlaveSetControlPilotDuty(connId, 100);
                    else
                        SlaveUpdateRelay(connId, 0);
                    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                    SetControlPilotDuty(100);
                    xSemaphoreGive(mutexControlPilot);
                    setChargerFaultBit(connId);
                    ESP_LOGE(TAG_STATUS, "Connector %d Over Voltage Fault", connId);
                }
                else if ((MeterFaults[connId].MeterUnderVoltage == 1) && ((MeterFaults[connId].MeterUnderVoltage != MeterFaults_old[connId].MeterUnderVoltage) || (isWebsocketConnected_Now != isWebsocketConnected_old)))
                {
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, UnderVoltage, strlen(UnderVoltage));
                        memcpy(CPStatusNotificationRequest[connId].status, Faulted, strlen(Faulted));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);
                        if (config.onlineOffline)
                            ledState[connId] = FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_FAULT_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline)
                            ledState[connId] = OFFLINE_FAULT_LED_STATE;
                        else if (config.offline)
                            ledState[connId] = OFFLINE_ONLY_FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_FAULT_LED_STATE;
                    }
                    if (cpEnable[connId])
                        SlaveSetControlPilotDuty(connId, 100);
                    else
                        SlaveUpdateRelay(connId, 0);
                    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                    SetControlPilotDuty(100);
                    xSemaphoreGive(mutexControlPilot);
                    setChargerFaultBit(connId);
                    ESP_LOGE(TAG_STATUS, "Connector %d Under Voltage Fault", connId);
                }
                else if ((MeterFaults[connId].MeterOverCurrent == 1) && ((MeterFaults[connId].MeterOverCurrent != MeterFaults_old[connId].MeterOverCurrent) || (isWebsocketConnected_Now != isWebsocketConnected_old)))
                {
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, OverCurrentFailure, strlen(OverCurrentFailure));
                        memcpy(CPStatusNotificationRequest[connId].status, Faulted, strlen(Faulted));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);
                        if (config.onlineOffline)
                            ledState[connId] = FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_FAULT_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline)
                            ledState[connId] = OFFLINE_FAULT_LED_STATE;
                        else if (config.offline)
                            ledState[connId] = OFFLINE_ONLY_FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_FAULT_LED_STATE;
                    }
                    if (cpEnable[connId])
                        SlaveSetControlPilotDuty(connId, 100);
                    else
                        SlaveUpdateRelay(connId, 0);
                    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                    SetControlPilotDuty(100);
                    xSemaphoreGive(mutexControlPilot);
                    setChargerFaultBit(connId);
                    ESP_LOGE(TAG_STATUS, "Connector %d Over Current Fault", connId);
                }
                else if ((MeterFaults[connId].MeterHighTemp == 1) && ((MeterFaults[connId].MeterHighTemp != MeterFaults_old[connId].MeterHighTemp) || (isWebsocketConnected_Now != isWebsocketConnected_old)))
                {
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, HighTemperature, strlen(HighTemperature));
                        memcpy(CPStatusNotificationRequest[connId].status, Faulted, strlen(Faulted));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);
                        if (config.onlineOffline)
                            ledState[connId] = FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_FAULT_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline)
                            ledState[connId] = OFFLINE_FAULT_LED_STATE;
                        else if (config.offline)
                            ledState[connId] = OFFLINE_ONLY_FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_FAULT_LED_STATE;
                    }
                    if (cpEnable[connId])
                        SlaveSetControlPilotDuty(connId, 100);
                    else
                        SlaveUpdateRelay(connId, 0);
                    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                    SetControlPilotDuty(100);
                    xSemaphoreGive(mutexControlPilot);
                    setChargerFaultBit(connId);
                    ESP_LOGE(TAG_STATUS, "Connector %d High Temperature Fault", connId);
                }
                else if ((MeterFaults[connId].MeterFailure == 1) && ((MeterFaults[connId].MeterFailure != MeterFaults_old[connId].MeterFailure) || (isWebsocketConnected_Now != isWebsocketConnected_old)))
                {
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, PowerMeterFailure, strlen(PowerMeterFailure));
                        memcpy(CPStatusNotificationRequest[connId].status, Faulted, strlen(Faulted));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);
                        if (config.onlineOffline)
                            ledState[connId] = FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_FAULT_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline)
                            ledState[connId] = OFFLINE_FAULT_LED_STATE;
                        else if (config.offline)
                            ledState[connId] = OFFLINE_ONLY_FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_FAULT_LED_STATE;
                    }
                    if (cpEnable[connId])
                        SlaveSetControlPilotDuty(connId, 100);
                    else
                        SlaveUpdateRelay(connId, 0);
                    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                    SetControlPilotDuty(100);
                    xSemaphoreGive(mutexControlPilot);
                    setChargerFaultBit(connId);
                    ESP_LOGE(TAG_STATUS, "Connector %d Power Meter Failure", connId);
                }
                else if ((relayWeldDetectionButton == 1) && ((relayWeldDetectionButton != relayWeldDetectionButton_old[connId]) || (isWebsocketConnected_Now != isWebsocketConnected_old)))
                {
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, OtherError, strlen(OtherError));
                        memcpy(CPStatusNotificationRequest[connId].status, Faulted, strlen(Faulted));
                        memcpy(CPStatusNotificationRequest[connId].vendorErrorCode, WeldDetect, strlen(WeldDetect));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);
                        if (config.onlineOffline)
                            ledState[connId] = FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_FAULT_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline)
                            ledState[connId] = OFFLINE_FAULT_LED_STATE;
                        else if (config.offline)
                            ledState[connId] = OFFLINE_ONLY_FAULT_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_FAULT_LED_STATE;
                    }
                    if (cpEnable[connId])
                        SlaveSetControlPilotDuty(connId, 100);
                    else
                        SlaveUpdateRelay(connId, 0);
                    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                    SetControlPilotDuty(100);
                    xSemaphoreGive(mutexControlPilot);
                    setChargerFaultBit(connId);
                    ESP_LOGE(TAG_STATUS, "Connector %d Relay Weld Detection Fault", connId);
                }
                else if ((ConnectorData[connId].CmsAvailable == false) &&
                         ((isWebsocketConnected_Now != isWebsocketConnected_old) ||
                          (ConnectorData[connId].CmsAvailableChanged == true)))
                {
                    ConnectorData[connId].CmsAvailableChanged = false;
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
                        memcpy(CPStatusNotificationRequest[connId].status, Unavailable, strlen(Unavailable));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);

                        if (config.onlineOffline)
                            ledState[connId] = UNAVAILABLE_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_UNAVAILABLE_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline || config.offline)
                            ledState[connId] = OFFLINE_UNAVAILABLE_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_UNAVAILABLE_LED_STATE;
                    }
                    SetChargerUnAvailableBit(connId);
                    if (cpEnable[connId])
                        SlaveSetControlPilotDuty(connId, 100);
                    else
                        SlaveUpdateRelay(connId, 0);
                    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                    SetControlPilotDuty(100);
                    xSemaphoreGive(mutexControlPilot);
                    ESP_LOGW(TAG_STATUS, "Connector %d Unavailable", connId);
                }
                else if (ConnectorData[connId].isReserved &&
                         (reservedStatusSent[connId] == false) &&
                         (ConnectorData[connId].CmsAvailable == true))
                {
                    reservedStatusSent[connId] = true;
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
                        memcpy(CPStatusNotificationRequest[connId].status, Reserved, strlen(Reserved));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);
                        if (config.onlineOffline)
                            ledState[connId] = RESERVATION_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_RESERVATION_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline)
                            ledState[connId] = OFFLINE_RESERVATION_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_RESERVATION_LED_STATE;
                    }
                    SetChargerReservedBit(connId);
                    ESP_LOGI(TAG_STATUS, "Connector %d Reserved", connId);
                    if (cpEnable[connId])
                        SlaveSetControlPilotDuty(connId, 100);
                    else
                        SlaveUpdateRelay(connId, 0);
                    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                    SetControlPilotDuty(100);
                    xSemaphoreGive(mutexControlPilot);
                }
                else if ((((cpState[connId] == STATE_A) && (cpEnable[connId] == true)) || (cpEnable[connId] == false)) &&
                         ((((cpState[connId] != cpState_old[connId]) ||
                            ((cpEnable[connId] == false) && (StatusState[connId] != STATUS_AVAILABLE)))) ||
                          ((finishingStatusSent[connId] == true) && (taskState[connId] != flag_EVSE_Stop_Transaction)) ||
                          (isWebsocketConnected_Now != isWebsocketConnected_old) ||
                          (ConnectorData[connId].CmsAvailableChanged == true)) &&
                         (ConnectorData[connId].isReserved == false) &&
                         (ConnectorData[connId].CmsAvailable == true) &&
                         (connectorFault[connId] == 0) &&
                         (taskState[connId] != flag_EVSE_Request_Charge) && (taskState[connId] != flag_EVSE_Stop_Transaction))
                {
                    ESP_LOGV(TAG, "cpState: %d", cpState[connId]);
                    ESP_LOGV(TAG, "cpState_old: %d", cpState_old[connId]);
                    ESP_LOGV(TAG, "taskState: %d", taskState[connId]);
                    ESP_LOGV(TAG, "isWebsocketConnected_Now: %s", isWebsocketConnected_Now ? "true" : "false");
                    ESP_LOGV(TAG, "isWebsocketConnected_old: %s", isWebsocketConnected_old ? "true" : "false");
                    ESP_LOGV(TAG, "ConnectorData[connId].CmsAvailableChanged: %s", ConnectorData[connId].CmsAvailableChanged ? "true" : "false");
                    ESP_LOGV(TAG, "ConnectorData[connId].CmsAvailable: %s", ConnectorData[connId].CmsAvailable ? "true" : "false");
                    ESP_LOGV(TAG, "ConnectorData[connId].isReserved: %s", ConnectorData[connId].isReserved ? "true" : "false");
                    StatusState[connId] = STATUS_AVAILABLE;
                    connectorChargingStatusSent[connId] = false;
                    finishingStatusSent[connId] = false;
                    finishingStatusPending[connId] = false;
                    ConnectorData[connId].CmsAvailableChanged = false;
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
                        memcpy(CPStatusNotificationRequest[connId].status, Available, strlen(Available));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);
                        if (config.onlineOffline)
                            ledState[connId] = AVAILABLE_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_AVAILABLE_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline)
                            ledState[connId] = OFFLINE_AVAILABLE_LED_STATE;
                        else if (config.offline)
                            ledState[connId] = OFFLINE_ONLY_AVAILABLE_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_AVAILABLE_LED_STATE;
                    }
                    if (((isWebsocketConnected_Now == false) || (isWebsocketBooted == false)) && config.online)
                        SetChargerUnAvailableBit(connId);
                    else
                        SetChargerAvailableBit(connId);
                    if (cpEnable[connId])
                        SlaveSetControlPilotDuty(connId, 100);
                    else
                        SlaveUpdateRelay(connId, 0);
                    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                    SetControlPilotDuty(100);
                    xSemaphoreGive(mutexControlPilot);
                    ESP_LOGI(TAG_STATUS, "Connector %d Available", connId);
                }
                else if (((cpState[connId] == STATE_B1) && (cpEnable[connId] == true)) &&
                         (((cpState[connId] != cpState_old[connId]) && (cpState_old[connId] != STATE_E2)) ||
                          (isWebsocketConnected_Now != isWebsocketConnected_old) ||
                          (ConnectorData[connId].CmsAvailableChanged == true)) &&
                         (ConnectorData[connId].isReserved == false) &&
                         (ConnectorData[connId].CmsAvailable == true) &&
                         (taskState[connId] != flag_EVSE_Request_Charge) &&
                         (connectorFault[connId] == 0))
                {
                    connectorChargingStatusSent[connId] = false;
                    ConnectorData[connId].CmsAvailableChanged = false;
                    reservedStatusSent[connId] = false;
                    if ((finishingStatusPending[connId] == true) || (finishingStatusSent[connId] == true))
                    {
                        finishingStatusPending[connId] = false;
                        if (isWebsocketConnected_Now & isWebsocketBooted)
                        {
                            setNULL(CPStatusNotificationRequest[connId].errorCode);
                            setNULL(CPStatusNotificationRequest[connId].status);
                            memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
                            memcpy(CPStatusNotificationRequest[connId].status, Finishing, strlen(Finishing));
                            setNULL(time_buffer);
                            getTimeInOcppFormat();
                            memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                            sendStatusNotificationRequest(connId);
                            if (config.onlineOffline)
                                ledState[connId] = FINISHING_LED_STATE;
                            else
                                ledState[connId] = ONLINE_ONLY_FINISHING_LED_STATE;
                        }
                        else
                        {
                            if (config.onlineOffline)
                                ledState[connId] = OFFLINE_FINISHING_LED_STATE;
                            else if (config.offline)
                                ledState[connId] = OFFLINE_ONLY_FINISHING_LED_STATE;
                            else
                                ledState[connId] = ONLINE_ONLY_OFFLINE_FINISHING_LED_STATE;
                        }
                        SetChargerFinishingdBit(connId);
                        ESP_LOGI(TAG_STATUS, "Connector %d Finishing", connId);
                    }
                    else if (finishingStatusSent[connId] == false)
                    {
                        if (isWebsocketConnected_Now & isWebsocketBooted)
                        {
                            setNULL(CPStatusNotificationRequest[connId].errorCode);
                            setNULL(CPStatusNotificationRequest[connId].status);
                            memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
                            memcpy(CPStatusNotificationRequest[connId].status, Preparing, strlen(Preparing));
                            setNULL(time_buffer);
                            getTimeInOcppFormat();
                            memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                            sendStatusNotificationRequest(connId);
                            if (config.onlineOffline)
                                ledState[connId] = PREPARING_LED_STATE;
                            else
                                ledState[connId] = ONLINE_ONLY_PREPARING_LED_STATE;
                        }
                        else
                        {
                            if (config.onlineOffline)
                                ledState[connId] = OFFLINE_PREPARING_LED_STATE;
                            else if (config.offline)
                            {
                                ledState[connId] = OFFLINE_ONLY_PREPARING_LED_STATE;
                                if (config.plugnplay && (taskState[connId] == flag_EVSE_Read_Id_Tag))
                                {
                                    ESP_LOGE(TAG, "Task switching to Plug & Play mode");
                                    ConnectorData[connId].UnkownId = false;
                                    setNULL(ConnectorData[0].idTag);
                                    memcpy(ConnectorData[0].idTag, "PLUGNPLAY", strlen("PLUGNPLAY"));
                                    setNULL(ConnectorData[connId].idTag);
                                    memcpy(ConnectorData[connId].idTag, "PLUGNPLAY", strlen("PLUGNPLAY"));
                                    custom_nvs_write_connector_data(connId);
                                    rfidMatchingWithLocalList = true;
                                    CPAuthorizeRequest[connId].Sent = false;
                                    AuthenticationProcessed = false;
                                    taskState[connId] = falg_EVSE_Authentication;
                                }
                            }
                            else
                                ledState[connId] = ONLINE_ONLY_OFFLINE_PREPARING_LED_STATE;
                        }

                        if ((isRfidTappedFirst == false) && (taskState[connId] == flag_EVSE_Read_Id_Tag))
                        {
                            setChargerEvPluggedTapRfidBit(connId);
                        }
                        ESP_LOGI(TAG_STATUS, "Connector %d Preparing", connId);
                    }
                }
                else if (((cpState[connId] == STATE_B2) &&
                          (cpEnable[connId] == true)) &&
                         ((cpState[connId] != cpState_old[connId]) ||
                          (isWebsocketConnected_Now != isWebsocketConnected_old)) &&
                         (connectorFault[connId] == 0))
                {
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
                        memcpy(CPStatusNotificationRequest[connId].status, SuspendedEV, strlen(SuspendedEV));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);
                        if (config.onlineOffline)
                            ledState[connId] = SUSPENDED_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_SUSPENDED_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline)
                            ledState[connId] = OFFLINE_SUSPENDED_LED_STATE;
                        else if (config.offline)
                            ledState[connId] = OFFLINE_ONLY_SUSPENDED_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_SUSPENDED_LED_STATE;
                    }
                    if ((taskState[connId] == flag_EVSE_Request_Charge) && (ConnectorData[connId].isTransactionOngoing == true))
                    {
                        SetChargerSuspendedBit(connId);
                    }
                    ESP_LOGI(TAG_STATUS, "Connector %d SuspendedEV", connId);
                }
                else if ((((cpState[connId] == STATE_C) && (cpEnable[connId] == true)) ||
                          (cpEnable[connId] == false)) &&
                         (((cpState[connId] != cpState_old[connId]) && (cpState_old[connId] != STATE_D)) ||
                          (isWebsocketConnected_Now != isWebsocketConnected_old) ||
                          ((cpEnable[connId] == false) && (taskState[connId] == flag_EVSE_Request_Charge) && (StatusState[connId] != STATUS_CHARGING)) ||
                          (isFaultRecovered[connId] == true)) &&
                         (connectorFault[connId] == 0))
                {
                    StatusState[connId] = STATUS_CHARGING;
                    if (isFaultRecovered[connId] == true)
                    {
                        isFaultRecovered[connId] = false;
                    }
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
                        memcpy(CPStatusNotificationRequest[connId].status, Charging, strlen(Charging));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);

                        if (config.onlineOffline)
                            ledState[connId] = CHARGING_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_CHARGING_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline)
                            ledState[connId] = OFFLINE_CHARGING_LED_STATE;
                        else if (config.offline)
                            ledState[connId] = OFFLINE_ONLY_CHARGING_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_CHARGING_LED_STATE;
                    }

                    ESP_LOGI(TAG_STATUS, "Connector %d Charging", connId);
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                    connectorChargingStatusSent[connId] = true;
                }
                else if ((((cpState[connId] == STATE_D) && (cpEnable[connId] == true)) ||
                          (cpEnable[connId] == false)) &&
                         (((cpState[connId] != cpState_old[connId]) && (cpState_old[connId] != STATE_C)) ||
                          (isWebsocketConnected_Now != isWebsocketConnected_old) ||
                          ((cpEnable[connId] == false) && (taskState[connId] == flag_EVSE_Request_Charge) && (StatusState[connId] != STATUS_CHARGING)) ||
                          (isFaultRecovered[connId] == true)) &&
                         (connectorFault[connId] == 0))
                {
                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
                        memcpy(CPStatusNotificationRequest[connId].status, Charging, strlen(Charging));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);

                        if (config.onlineOffline)
                            ledState[connId] = CHARGING_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_CHARGING_LED_STATE;
                    }
                    else
                    {
                        if (config.onlineOffline)
                            ledState[connId] = OFFLINE_CHARGING_LED_STATE;
                        else if (config.offline)
                            ledState[connId] = OFFLINE_ONLY_CHARGING_LED_STATE;
                        else
                            ledState[connId] = ONLINE_ONLY_OFFLINE_CHARGING_LED_STATE;
                    }
                    ESP_LOGI(TAG_STATUS, "Connector %d Charging", connId);
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                    connectorChargingStatusSent[connId] = true;
                }
                else if (((cpState[connId] == STATE_DIS) && (cpEnable[connId] == true)) && (cpState[connId] != cpState_old[connId]))
                {
                }
                else if ((((cpState[connId] == STATE_E1) && (cpEnable[connId] == true)) || (cpEnable[connId] == false)) &&
                         ((cpState[connId] != cpState_old[connId]) ||
                          ((finishingStatusSent[connId] == true) && (taskState[connId] != flag_EVSE_Stop_Transaction)) ||
                          (isWebsocketConnected_Now != isWebsocketConnected_old) ||
                          (ConnectorData[connId].CmsAvailableChanged == true)) &&
                         (ConnectorData[connId].isReserved == false) &&
                         (ConnectorData[connId].CmsAvailable == true) &&
                         (connectorFault[connId] == 0) &&
                         ((taskState[connId] != flag_EVSE_Request_Charge) && (taskState[connId] != flag_EVSE_Stop_Transaction)))
                {
                    ConnectorData[connId].CmsAvailableChanged = false;

                    if (isWebsocketConnected_Now & isWebsocketBooted)
                    {
                        setNULL(CPStatusNotificationRequest[connId].errorCode);
                        setNULL(CPStatusNotificationRequest[connId].status);
                        setNULL(CPStatusNotificationRequest[connId].vendorErrorCode);
                        memcpy(CPStatusNotificationRequest[connId].errorCode, EVCommunicationError, strlen(EVCommunicationError));
                        memcpy(CPStatusNotificationRequest[connId].status, Faulted, strlen(Faulted));
                        setNULL(time_buffer);
                        getTimeInOcppFormat();
                        memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                        sendStatusNotificationRequest(connId);
                    }
                    if (cpEnable[connId])
                        SlaveSetControlPilotDuty(connId, 100);
                    else
                        SlaveUpdateRelay(connId, 0);
                    xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                    SetControlPilotDuty(100);
                    xSemaphoreGive(mutexControlPilot);
                    ESP_LOGI(TAG_STATUS, "Connector %d CommunicationFailure", connId);
                }
                else if (((cpState[connId] == STATE_E2) && (cpEnable[connId] == true)) &&
                         (((cpState[connId] != cpState_old[connId]) && (cpState_old[connId] != STATE_B1)) ||
                          (isWebsocketConnected_Now != isWebsocketConnected_old) ||
                          (ConnectorData[connId].CmsAvailableChanged == true)) &&
                         (ConnectorData[connId].isReserved == false) &&
                         (ConnectorData[connId].CmsAvailable == true) &&
                         (taskState[connId] != flag_EVSE_Request_Charge) &&
                         (connectorFault[connId] == 0))
                {
                    connectorChargingStatusSent[connId] = false;
                    ConnectorData[connId].CmsAvailableChanged = false;
                    reservedStatusSent[connId] = false;
                    if ((finishingStatusPending[connId] == true) || (finishingStatusSent[connId] == true))
                    {
                        finishingStatusPending[connId] = false;
                        if (isWebsocketConnected_Now & isWebsocketBooted)
                        {
                            setNULL(CPStatusNotificationRequest[connId].errorCode);
                            setNULL(CPStatusNotificationRequest[connId].status);
                            memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
                            memcpy(CPStatusNotificationRequest[connId].status, Finishing, strlen(Finishing));
                            setNULL(time_buffer);
                            getTimeInOcppFormat();
                            memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                            sendStatusNotificationRequest(connId);
                            if (config.onlineOffline)
                                ledState[connId] = FINISHING_LED_STATE;
                            else
                                ledState[connId] = ONLINE_ONLY_FINISHING_LED_STATE;
                        }
                        else
                        {
                            if (config.onlineOffline)
                                ledState[connId] = OFFLINE_FINISHING_LED_STATE;
                            else if (config.offline)
                                ledState[connId] = OFFLINE_ONLY_FINISHING_LED_STATE;
                            else
                                ledState[connId] = ONLINE_ONLY_OFFLINE_FINISHING_LED_STATE;
                        }
                        SetChargerFinishingdBit(connId);
                        ESP_LOGI(TAG_STATUS, "Connector %d Finishing", connId);
                    }
                    else if (finishingStatusSent[connId] == false)
                    {
                        if (isWebsocketConnected_Now & isWebsocketBooted)
                        {
                            setNULL(CPStatusNotificationRequest[connId].errorCode);
                            setNULL(CPStatusNotificationRequest[connId].status);
                            memcpy(CPStatusNotificationRequest[connId].errorCode, NoError, strlen(NoError));
                            memcpy(CPStatusNotificationRequest[connId].status, Preparing, strlen(Preparing));
                            setNULL(time_buffer);
                            getTimeInOcppFormat();
                            memcpy(CPStatusNotificationRequest[connId].timestamp, time_buffer, strlen(time_buffer));
                            sendStatusNotificationRequest(connId);
                            if (config.onlineOffline)
                                ledState[connId] = PREPARING_LED_STATE;
                            else
                                ledState[connId] = ONLINE_ONLY_PREPARING_LED_STATE;
                        }
                        else
                        {
                            if (config.onlineOffline)
                                ledState[connId] = OFFLINE_PREPARING_LED_STATE;
                            else if (config.offline)
                                ledState[connId] = OFFLINE_ONLY_PREPARING_LED_STATE;
                            else
                                ledState[connId] = ONLINE_ONLY_OFFLINE_PREPARING_LED_STATE;
                        }
                        if ((isRfidTappedFirst == false) && (taskState[connId] == flag_EVSE_Read_Id_Tag))
                        {
                            setChargerEvPluggedTapRfidBit(connId);
                        }
                        ESP_LOGI(TAG_STATUS, "Connector %d Preparing", connId);
                    }
                }
                else if (((cpState[connId] == STATE_F) && (cpEnable[connId] == true)) && (cpState[connId] != cpState_old[connId]))
                {
                }
                gfciButton_old[connId] = gfciButton;
                earthDisconnectButton_old[connId] = earthDisconnectButton;
                emergencyButton_old[connId] = emergencyButton;
                relayWeldDetectionButton_old[connId] = relayWeldDetectionButton;
                cpState_old[connId] = cpState[connId];
                MeterFaults_old[connId] = MeterFaults[connId];

                connectorFault_old[connId] = connectorFault[connId];
                if (isWebsocketConnected_Now & isWebsocketBooted)
                {
                    if ((connectorFault[connId] == 1) && (memcmp(CPStatusNotificationRequest[connId].status, Faulted, strlen(Faulted)) != 0))
                    {
                        ESP_LOGE(TAG, "Connector %d Resetting fault old variables", connId);
                        gfciButton_old[connId] = 0;
                        earthDisconnectButton_old[connId] = 0;
                        emergencyButton_old[connId] = 0;
                        relayWeldDetectionButton_old[connId] = 0;
                        memset(&MeterFaults_old[connId], 0, sizeof(MeterFaults_t));
                    }
                    else if ((connectorFault[connId] == 0) && (memcmp(CPStatusNotificationRequest[connId].status, Faulted, strlen(Faulted)) == 0))
                    {
                        cpState_old[connId] = 0;
                    }
                }
            }
        }
        for (uint8_t connId = 1; connId <= config.NumberOfConnectors; connId++)
        {
            if ((ledState[connId] == EMERGENCY_FAULT_LED_STATE) || (ledState[connId] == EMERGENCY_LOGS_LED_STATE))
            {
                if (uploadingLogs)
                {
                    ledState[connId] = EMERGENCY_LOGS_LED_STATE;
                }
                else
                {
                    ledState[connId] = EMERGENCY_FAULT_LED_STATE;
                }
            }
            if (rfidTappedBlinkLed[connId] && (isRfidLedProcessed[connId] == false))
            {
                rfidTappedBlinkLed[connId] = false;
                ledState_Before_Rfid[connId] = ledState[connId];
                if (config.offline)
                    ledState[connId] = OFFLINE_ONLY_RFID_TAPPED_LED_STATE;
                else
                    ledState[connId] = RFID_TAPPED_LED_STATE;
                isRfidLedProcessed[connId] = true;
            }
            if (((ledState_old[connId] == RFID_TAPPED_LED_STATE) &&
                 (ledState[connId] == RFID_TAPPED_LED_STATE) &&
                 (config.offline == false)) ||
                ((ledState_old[connId] == OFFLINE_ONLY_RFID_TAPPED_LED_STATE) &&
                 (ledState[connId] == OFFLINE_ONLY_RFID_TAPPED_LED_STATE) &&
                 (config.offline)))
            {
                ledState[connId] = ledState_Before_Rfid[connId];
            }
            if (connectorEnabled[connId] && (ledState[connId] != ledState_old[connId]) && (taskState[connId] >= flag_EVSE_is_Booted))
            {
                switch (ledState[connId])
                {
                case POWERON_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], POWERON_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = POWERON_LED;
                    }
                    break;
                case OTA_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OTA_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OTA_LED;
                    }
                    break;
                case COMMISSIONING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], COMMISSIONING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = COMMISSIONING_LED;
                    }
                    break;
                case AVAILABLE_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], AVAILABLE_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = AVAILABLE_LED;
                    }
                    break;
                case OFFLINE_AVAILABLE_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_AVAILABLE_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_AVAILABLE_LED;
                    }
                    break;
                case PREPARING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], PREPARING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = PREPARING_LED;
                    }
                    break;
                case OFFLINE_PREPARING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_PREPARING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_PREPARING_LED;
                    }
                    break;
                case SUSPENDED_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], SUSPENDED_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = SUSPENDED_LED;
                    }
                    break;
                case OFFLINE_SUSPENDED_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_SUSPENDED_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_SUSPENDED_LED;
                    }
                    break;
                case CHARGING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], CHARGING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = CHARGING_LED;
                    }
                    break;
                case OFFLINE_CHARGING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_CHARGING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_CHARGING_LED;
                    }
                    break;
                case FINISHING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], FINISHING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = FINISHING_LED;
                    }
                    break;
                case OFFLINE_FINISHING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_FINISHING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_FINISHING_LED;
                    }
                    break;
                case EMERGENCY_FAULT_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], EMERGENCY_FAULT_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = EMERGENCY_FAULT_LED;
                    }
                    break;
                case OFFLINE_EMERGENCY_FAULT_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_EMERGENCY_FAULT_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_EMERGENCY_FAULT_LED;
                    }
                    break;
                case FAULT_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], FAULT_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = FAULT_LED;
                    }
                    break;
                case OFFLINE_FAULT_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_FAULT_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_FAULT_LED;
                    }
                    break;
                case RESERVATION_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], RESERVATION_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = RESERVATION_LED;
                    }
                    break;
                case OFFLINE_RESERVATION_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_RESERVATION_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_RESERVATION_LED;
                    }
                    break;
                case UNAVAILABLE_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], UNAVAILABLE_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = UNAVAILABLE_LED;
                    }
                    break;
                case OFFLINE_UNAVAILABLE_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_UNAVAILABLE_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_UNAVAILABLE_LED;
                    }
                    break;
                case RFID_TAPPED_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], RFID_TAPPED_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = RFID_TAPPED_LED;
                    }
                    break;
                case ONLINE_ONLY_AVAILABLE_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_AVAILABLE_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_AVAILABLE_LED;
                    }
                    break;
                case ONLINE_ONLY_OFFLINE_AVAILABLE_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_OFFLINE_AVAILABLE_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_OFFLINE_AVAILABLE_LED;
                    }
                    break;
                case ONLINE_ONLY_PREPARING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_PREPARING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_PREPARING_LED;
                    }
                    break;
                case ONLINE_ONLY_OFFLINE_PREPARING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_OFFLINE_PREPARING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_OFFLINE_PREPARING_LED;
                    }
                    break;
                case ONLINE_ONLY_SUSPENDED_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_SUSPENDED_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_SUSPENDED_LED;
                    }
                    break;
                case ONLINE_ONLY_OFFLINE_SUSPENDED_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_OFFLINE_SUSPENDED_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_OFFLINE_SUSPENDED_LED;
                    }
                    break;
                case ONLINE_ONLY_CHARGING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_CHARGING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_CHARGING_LED;
                    }
                    break;
                case ONLINE_ONLY_OFFLINE_CHARGING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_OFFLINE_CHARGING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_OFFLINE_CHARGING_LED;
                    }
                    break;
                case ONLINE_ONLY_FINISHING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_FINISHING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_FINISHING_LED;
                    }
                    break;
                case ONLINE_ONLY_OFFLINE_FINISHING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_OFFLINE_FINISHING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_OFFLINE_FINISHING_LED;
                    }
                    break;
                case ONLINE_ONLY_EMERGENCY_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_EMERGENCY_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_EMERGENCY_LED;
                    }
                    break;
                case ONLINE_ONLY_OFFLINE_EMERGENCY_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_OFFLINE_EMERGENCY_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_OFFLINE_EMERGENCY_LED;
                    }
                    break;
                case ONLINE_ONLY_FAULT_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_FAULT_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_FAULT_LED;
                    }
                    break;
                case ONLINE_ONLY_OFFLINE_FAULT_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_OFFLINE_FAULT_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_OFFLINE_FAULT_LED;
                    }
                    break;
                case ONLINE_ONLY_RESERVATION_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_RESERVATION_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_RESERVATION_LED;
                    }
                    break;
                case ONLINE_ONLY_OFFLINE_RESERVATION_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_OFFLINE_RESERVATION_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_OFFLINE_RESERVATION_LED;
                    }
                    break;
                case ONLINE_ONLY_UNAVAILABLE_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_UNAVAILABLE_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_UNAVAILABLE_LED;
                    }
                    break;
                case ONLINE_ONLY_OFFLINE_UNAVAILABLE_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], ONLINE_ONLY_OFFLINE_UNAVAILABLE_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = ONLINE_ONLY_OFFLINE_UNAVAILABLE_LED;
                    }
                    break;
                case OFFLINE_ONLY_AVAILABLE_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_ONLY_AVAILABLE_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_ONLY_AVAILABLE_LED;
                    }
                    break;
                case OFFLINE_ONLY_PREPARING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_ONLY_PREPARING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_ONLY_PREPARING_LED;
                    }
                    break;
                case OFFLINE_ONLY_SUSPENDED_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_ONLY_SUSPENDED_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_ONLY_SUSPENDED_LED;
                    }
                    break;
                case OFFLINE_ONLY_CHARGING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_ONLY_CHARGING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_ONLY_CHARGING_LED;
                    }
                    break;
                case OFFLINE_ONLY_FINISHING_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_ONLY_FINISHING_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_ONLY_FINISHING_LED;
                    }
                    break;
                case OFFLINE_ONLY_EMERGENCY_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_ONLY_EMERGENCY_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_ONLY_EMERGENCY_LED;
                    }
                    break;
                case OFFLINE_ONLY_FAULT_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_ONLY_FAULT_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_ONLY_FAULT_LED;
                    }
                    break;
                case OFFLINE_ONLY_RFID_TAPPED_LED_STATE:
                    if (config.vcharge_lite_1_4)
                    {
                        SlaveSetLedColor(slaveLed[connId], OFFLINE_ONLY_RFID_TAPPED_LED);
                    }
                    else
                    {
                        ledStateColor[connId] = OFFLINE_ONLY_RFID_TAPPED_LED;
                    }
                    break;
                default:
                    break;
                }
                ledState_old[connId] = ledState[connId];
            }
        }
        // AllTasksAvailableState = true;
        // for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
        // {
        //     if ((taskState[i] <= flag_EVSE_Read_Id_Tag) && (AllTasksAvailableState == true))
        //     {
        //         AllTasksAvailableState = true;
        //     }
        //     else
        //     {
        //         AllTasksAvailableState = false;
        //     }
        // }
        // if ((AllTasksAvailableState == true) && isWifiConnected)
        // {
        //     esp_netif_t *default_netif = esp_netif_get_default_netif();
        //     if (default_netif != NULL)
        //     {
        //         if (strcmp(esp_netif_get_desc(default_netif), "sta") == 0)
        //         {
        //             local_http_server_start();
        //         }
        //     }
        // }
        // else
        // {
        //     local_http_server_stop();
        // }
        isWebsocketConnected_old = isWebsocketConnected;
        if (bootedNow)
        {
            bootedNow = false;
        }
        for (uint8_t connId = 1; connId <= config.NumberOfConnectors; connId++)
        {
            if (connectorEnabled[connId] && (connectorFault[connId] == 0))
            {
                if (isConnectorCharging[connId])
                {
                    if (!((cpEnable[connId] == true) && (cpState[connId] == STATE_B2)))
                        SetChargerChargingBit(connId);
                }
            }
        }
        if (FirmwareUpdateStarted)
        {
            SetChargerFirmwareUpdateBit();
        }
        // vTaskDelayUntil(&xLastWakeTime, (TASK_DELAY_TIME / portTICK_PERIOD_MS));
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void ConnectorTask(void *params)
{
    uint8_t connId = *((uint8_t *)params);
    uint32_t loopCount = 0;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (true)
    {
        loopCount++;

        switch (taskState[connId])
        {
        case flag_EVSE_initialisation:
            break;
        case flag_EVSE_is_Booted:
            // if(isWebsocketConnected)
            // {
            //     processBootedState(connId);
            // }
            break;
        case flag_EVSE_Read_Id_Tag:
            if ((updatingDataToCms[connId] == false) && (FirmwareUpdateStarted == false))
                processRfidState(connId);

            break;
        case falg_EVSE_Authentication:
            if ((updatingDataToCms[connId] == false) && (FirmwareUpdateStarted == false))
                processAuthenticationState(connId);
            break;
        case flag_EVSE_Start_Transaction:
            if ((updatingDataToCms[connId] == false) && (FirmwareUpdateStarted == false))
                processStartTransactionState(connId);
            break;
        case flag_EVSE_Request_Charge:
            processChargeState(connId);
            break;
        case flag_EVSE_Stop_Transaction:
            if (config.vcharge_lite_1_4)
            {
                if (cpEnable[connId])
                    SlaveSetControlPilotDuty(connId, 100);
                else
                    SlaveUpdateRelay(connId, 0);
            }
            else
            {
                updateRelayState(connId, 0);
                xSemaphoreTake(mutexControlPilot, portMAX_DELAY);
                SetControlPilotDuty(100);
                xSemaphoreGive(mutexControlPilot);
            }
            if ((updatingDataToCms[connId] == false) && (FirmwareUpdateStarted == false))
                processStopTransactionState(connId);
            break;
        default:
            break;
        }
        if ((loopCount % 30 == 0) || (taskState[connId] != taskState_old[connId]))
        {
            // logConnectorData(connId);
            if (taskState[connId] == flag_EVSE_initialisation)
            {
                ESP_LOGI(TAG, "Connector %d STATE initialisation", connId);
            }
            else if (taskState[connId] == flag_EVSE_is_Booted)
            {
                ESP_LOGI(TAG, "Connector %d STATE Booted", connId);
            }
            else if (taskState[connId] == flag_EVSE_Read_Id_Tag)
            {
                ESP_LOGI(TAG, "Connector %d STATE Read_Id_Tag", connId);
            }
            else if (taskState[connId] == falg_EVSE_Authentication)
            {
                ESP_LOGI(TAG, "Connector %d STATE Authentication", connId);
            }
            else if (taskState[connId] == flag_EVSE_Start_Transaction)
            {
                ESP_LOGI(TAG, "Connector %d STATE Start_Transaction", connId);
            }
            else if (taskState[connId] == flag_EVSE_Request_Charge)
            {
                ESP_LOGI(TAG, "Connector %d STATE Request_Charge", connId);
            }
            else if (taskState[connId] == flag_EVSE_Stop_Transaction)
            {
                ESP_LOGI(TAG, "Connector %d STATE Stop_Transaction", connId);
            }
            else
            {
                ESP_LOGI(TAG, "Connector %d STATE UNKOWN", connId);
            }
        }
        taskState_old[connId] = taskState[connId];

        vTaskDelayUntil(&xLastWakeTime, (TASK_DELAY_TIME / portTICK_PERIOD_MS));
    }
}

void ocpp_task_init(void)
{
    // uint8_t restartReason;
    // ESP_LOGI(TAG, "Writing Websocket Restart Reason");
    // custom_nvs_write_restart_reason(WEBSOCKET_RESTART);
    // ESP_LOGI(TAG, "Reading Websocket Restart Reason");
    // if (custom_nvs_read_restart_reason(&restartReason) == ESP_OK)
    // {
    //     WebsocketRestart = (restartReason == WEBSOCKET_RESTART) ? true : false;
    //     ESP_LOGI(TAG, "Restart Reason: %hhu", restartReason);
    //     ESP_LOGI(TAG, "Websocket Restart: %hhu", WebsocketRestart);
    //     if (WebsocketRestart == false)
    //     {
    //         ESP_LOGI(TAG, "Clearing Websocket Restart Reason");
    //         custom_nvs_write_restart_reason(WEBSOCKET_RESTART_CLEAR);

    //         ESP_LOGI(TAG, "Reading Websocket Restart Reason");
    //         custom_nvs_read_restart_reason(&restartReason);
    //         ESP_LOGI(TAG, "Restart Reason: %hhu", restartReason);
    //     }
    // }
    // else
    // {
    //     ESP_LOGE(TAG, "Failed to read restart reason");
    // }
    mutexControlPilot = xSemaphoreCreateMutex();
    setenv("TZ", "UTC", 1);
    tzset();
    ocpp_struct_init();
    custom_nvs_read_localist_ocpp();
    custom_nvs_read_localist_ocpp();
    ESP_LOGI(TAG, "Printing Local List");
    // log localist
    for (uint32_t i = 0; i < LOCAL_LIST_COUNT; i++)
    {
        if (CMSSendLocalListRequest.localAuthorizationList[i].idTagPresent)
        {
            ESP_LOGI(TAG, "CMS Local List Tag%ld : %s", i, CMSSendLocalListRequest.localAuthorizationList[i].idTag);
        }
    }
    custom_nvs_read_LocalAuthorizationList_ocpp();
    custom_nvs_read_LocalAuthorizationList_ocpp();
    ESP_LOGI(TAG, "Printing Local Authorization List");
    // log localAuthentication list
    for (uint32_t i = 0; i < LOCAL_LIST_COUNT; i++)
    {
        if (LocalAuthorizationList.idTagPresent[i])
        {
            ESP_LOGI(TAG, "Local Authentication Tag%ld : %s", i, LocalAuthorizationList.idTag[i]);
        }
    }

    if (config.Ac7s || config.Ac11s || config.Ac22s || config.Ac44s || config.Ac14D || config.Ac10DA || config.Ac10DP || config.Ac14t || config.Ac18t)
    {
        cpEnable[1] = true;
    }
    if (config.Ac14D || config.Ac18t)
    {
        cpEnable[2] = true;
    }
    xTaskCreate(&Connector0Task, "Connector0Task", CONNECTORS_TASK_SIZE, NULL, 2, NULL);
    char taskName[50];
    for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
    {
        setNULL(taskName);
        sprintf(taskName, "ConnectorTask%hhu", i);

        if (connectorEnabled[i])
        {
            custom_nvs_read_connector_data(i);
            custom_nvs_read_connector_data(i);
            // logConnectorData(i);
            if (ConnectorData[i].CmsAvailable == false)
            {
                ConnectorData[i].CmsAvailableChanged = true;
            }
            xTaskCreate(&ConnectorTask, taskName, CONNECTORS_TASK_SIZE, &connectorTaskId[i], 2, NULL);
        }
    }
    if (config.Ac6D)
    {
        PushButtonEnabled[1] = true;
        PushButtonEnabled[2] = true;
    }
    else if (config.Ac10t)
    {
        PushButtonEnabled[1] = true;
        PushButtonEnabled[2] = true;
        PushButtonEnabled[3] = true;
    }
    else if (config.Ac14t)
    {
        PushButtonEnabled[2] = true;
        PushButtonEnabled[3] = true;
    }
    SetDefaultOcppConfig();
    Control_pilot_state_init();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    controlPilotActive = true;
    for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
    {
        logConnectorData(i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    if (config.smartCharging)
    {
        xTaskCreate(&smartChargingTask, "smartChargingTask", 1024 * 6, NULL, 2, NULL);
    }
}
