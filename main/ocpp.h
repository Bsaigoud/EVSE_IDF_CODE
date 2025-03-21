#ifndef OCPP_H
#define OCPP_H

#include "cJSON.h"

#define OCPP_CALL 2
#define OCPP_RECEIVE 3
#define OCPP_ERROR 4

#define Enable_chargePointSerialNumber 1
#define Enable_chargeBoxSerialNumber 1
#define Enable_firmwareVersion 1
#define Enable_iccid 0
#define Enable_imsi 0
#define Enable_meterType 0
#define Enable_meterSerialNumber 0

#define NUM_OF_CONNECTORS 4
#define LOCAL_LIST_COUNT 10
#define idTagLength 25           // 20 plus extra 5
#define OCPP_MESSAGEID_LENGTH 40 // 36 plus extra 4
#define setNULL(x) memset(x, '\0', sizeof(x))

enum context_t
{
  InterruptionBegin = 1,
  InterruptionEnd,
  SampleClock,
  SamplePeriodic,
  TransactionBegin,
  TransactionEnd,
  Trigger,
  Other,
};

#define CP_MESSAGEID_LENGTH 10

#define InterruptionBegin 1
#define InterruptionEnd 2
#define SampleClock 3
#define SamplePeriodic 4
#define TransactionBegin 5
#define TransactionEnd 6
#define Trigger 7
#define Other 8

#define ChargePointMaxProfile 1
#define TxDefaultProfile 2
#define TxProfile 3

#define Absolute 1
#define Recurring 2
#define Relative 3

#define Daily 1
#define Weekly 2

#define chargingRateUnitAmpere 1
#define chargingRateUnitWatt 2

#define SchedulePeriodCount 20

/********************************************START Request Structures****************************************/
typedef struct
{
  char chargePointVendor[20];
  char chargePointModel[20];
  char chargePointSerialNumber[25];
  char chargeBoxSerialNumber[25];
  char firmwareVersion[70];
  char iccid[20];
  char imsi[20];
  // char meterType[25];
  // char meterSerialNumber[25];
  char UniqId[CP_MESSAGEID_LENGTH];
  bool Sent;
} CPBootNotificationRequest_t;

typedef struct
{
  uint32_t connectorId;
  char errorCode[25];
  // char info[50];
  char status[15];
  char timestamp[25];
  // char vendorId[255];
  char vendorErrorCode[25];
  char UniqId[CP_MESSAGEID_LENGTH];
  bool Sent;
} CPStatusNotificationRequest_t;

typedef struct
{
  char idTag[idTagLength];
  char UniqId[CP_MESSAGEID_LENGTH];
  bool Sent;
} CPAuthorizeRequest_t;

typedef struct
{
  uint32_t reservationId;
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSCancelReservationRequest_t;

typedef struct
{
  uint8_t connectorId;
  char type[15];
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSChangeAvailabilityRequest_t;

typedef struct
{
  char key[50];
  char value[200];
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSChangeConfigurationRequest_t;

typedef struct
{
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSClearCacheRequest_t;

typedef struct
{
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSGetConfigurationRequest_t;

typedef struct
{
  char UniqId[OCPP_MESSAGEID_LENGTH];
  uint32_t id;
  uint8_t connectorId;
  uint8_t chargingProfilePurpose;
  uint32_t stackLevel;
  bool Received;
} CMSClearChargingProfileRequest_t;

typedef struct
{
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSDataTransferRequest_t;

typedef struct
{
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSGetCompositeScheduleRequest_t;

typedef struct
{
  char location[100];
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSGetDiagnosticsRequest_t;

typedef struct
{
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSSetChargingProfileRequest_t;

typedef struct
{
  uint8_t connectorId;
  struct
  {
    uint32_t chargingProfileId;
    uint32_t transactionId;
    uint32_t stackLevel;
    uint8_t chargingProfilePurpose;
    uint8_t chargingProfileKind;
    uint8_t recurrencyKind;
    uint32_t validFrom;
    uint32_t validTo;
    struct
    {
      uint32_t duration;
      uint32_t startSchedule;
      uint8_t chargingRateUnit;
      struct
      {
        bool schedulePresent;
        uint32_t startPeriod;
        float limit;
        uint8_t numberPhases;
      } chargingSchedulePeriod[SchedulePeriodCount];
      float minChargingRate;
    } chargingSchedule;
  } csChargingProfiles;
} ChargingProfiles_t;

typedef struct
{
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSUnlockConnectorRequest_t;

typedef struct
{
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSGetLocalListVersionRequest_t;

typedef struct
{
  char UniqId[CP_MESSAGEID_LENGTH];
  bool Sent;
} CPHeartbeatRequest_t;

typedef struct
{
  char status[20];
  char UniqId[CP_MESSAGEID_LENGTH];
  bool Sent;
} CPFirmwareStatusNotificationRequest_t;

typedef struct
{
  char status[15];
  char UniqId[CP_MESSAGEID_LENGTH];
  bool Sent;
} CPDiagnosticsStatusNotificationRequest_t;

typedef struct
{
  char requestedMessage[35];
  bool isConnectorIdReceived;
  uint8_t connectorId;
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSTriggerMessageRequest_t;

typedef struct
{
  bool reservationIdPresent;
  int connectorId;
  char idTag[idTagLength];
  int meterStart;
  int reservationId;
  char timestamp[25];
  char UniqId[CP_MESSAGEID_LENGTH];
  bool Sent;
} CPStartTransactionRequest_t;

typedef struct
{
  char idTag[idTagLength]; // Including null terminator
  int meterStop;
  char timestamp[25]; // Including null terminator
  int transactionId;
  char reason[15]; // Adjust size based on the longest enum value
  // Define a structure for sampledValue, you can adapt based on your needs
  // For simplicity, I'm using a fixed-size array
  struct
  {
    char timestamp[28];
    struct
    {
      char value[15];
      char context[20];   // Adjust size based on the longest enum value
      char format[10];    // Adjust size based on the longest enum value
      char measurand[32]; // Adjust size based on the longest enum value
      char phase[5];      // Adjust size based on the longest enum value
      char location[7];   // Adjust size based on the longest enum value
      char unit[11];      // Adjust size based on the longest enum value
    } sampledValue[2];
  } transactionData[2]; // Assuming there are two elements in the array
  char UniqId[CP_MESSAGEID_LENGTH];
  bool Sent;
} CPStopTransactionRequest_t;

typedef struct
{
  bool isConnectorIdReceived;
  int connectorId;
  char idTag[idTagLength]; // assuming a maximum length of 20 characters for idTag
  struct
  {
    int chargingProfileId;
    int transactionId;
    int stackLevel;
    char chargingProfilePurpose[50]; // adjust size accordingly
    char chargingProfileKind[20];    // adjust size accordingly
    char recurrencyKind[10];         // adjust size accordingly
    char validFrom[30];              // adjust size accordingly
    char validTo[30];                // adjust size accordingly
    struct
    {
      int duration;
      char startSchedule[30];   // adjust size accordingly
      char chargingRateUnit[2]; // adjust size accordingly
      struct
      {
        int startPeriod;
        float limit;
        int numberPhases;
      } chargingSchedulePeriod[SchedulePeriodCount]; // assuming a maximum of 2 periods
      float minChargingRate;
    } chargingSchedule;
  } chargingProfile;
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSRemoteStartTransactionRequest_t;

typedef struct
{
  int transactionId;
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSRemoteStopTransactionRequest_t;

typedef struct
{
  int connectorId;
  char expiryDate[25];
  char idTag[idTagLength];
  bool isparentidTagReceived;
  char parentidTag[idTagLength];
  int reservationId;
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSReserveNowRequest_t;

typedef struct
{
  char type[5];
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSResetRequest_t;

typedef struct
{
  char location[256];
  int retries;
  char retrieveDate[25];
  int retryInterval;
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSUpdateFirmwareRequest_t;

typedef struct
{
  int listVersion;
  struct
  {
    bool idTagPresent;
    char idTag[idTagLength];
    struct
    {
      char expiryDate[25];
      char parentidTag[idTagLength];
      char status[13];
    } idTagInfo;
  } localAuthorizationList[LOCAL_LIST_COUNT]; // Assuming an array of size 2
  char updateType[13];
  char UniqId[OCPP_MESSAGEID_LENGTH];
  bool Received;
} CMSSendLocalListRequest_t;

typedef struct
{
  char idTag[LOCAL_LIST_COUNT][20];
  bool idTagPresent[LOCAL_LIST_COUNT];
} LocalAuthorizationList_t;

typedef struct
{
  int connectorId;
  int transactionId;
  struct
  {
    char timestamp[25]; // Assuming a fixed size for simplicity
    struct
    {
      char value[15]; // Assuming a fixed size for simplicity
      char context[20];
      char format[10];
      char measurand[32];
      char phase[5];
      char location[7];
      char unit[11];
    } sampledValue[9]; // Assuming a fixed size array for simplicity
  } meterValue;        // Assuming a fixed size array for simplicity
  char UniqId[CP_MESSAGEID_LENGTH];
  bool Sent;
} CPMeterValuesRequest_t;
/********************************************END Request Structures******************************************/
/********************************************START Response Structures********************************************/
typedef struct
{
  char status[10];
  char currentTime[25];
  uint32_t interval;
  bool Received;
} CMSBootNotificationResponse_t;

typedef struct
{
  bool Received;
} CMSStatusNotificationResponse_t;

typedef struct
{
  struct
  {
    bool expiryDateReceived;
    char expiryDate[25];
    bool parentidTagReceived;
    char parentidTag[idTagLength];
    char status[10];
    /* data */
  } idtaginfo;
  bool Received;
} CMSAuthorizeResponse_t;

typedef struct
{
  char status[10];
  bool Sent;

} CPCancelReservationResponse_t;

typedef struct
{
  char status[10];
  bool Sent;

} CPChangeAvailabilityResponse_t;

typedef struct
{
  char status[10];
  bool Sent;

} CPChangeConfigurationResponse_t;

typedef struct
{
  char status[10];
  bool Sent;

} CPClearCacheResponse_t;

typedef struct
{
  bool Sent;
} CPGetLocalListVersionResponse_t;

typedef struct
{
  char currentTime[25];
  bool Received;

} CMSHeartbeatResponse_t;

typedef struct
{
  bool Sent;
  /* data */
} CPUpdateFirmwareResponse_t;

typedef struct
{
  char fileName[255];
  bool Sent;
  /* data */
} CPGetDiagnosticsResponse_t;

typedef struct
{
  bool Received;
  /* data */
} CMSFirmwareStatusNotificationResponse_t;

typedef struct
{
  bool Received;
  /* data */
} CMSDiagnosticsStatusNotificationResponse_t;

typedef struct
{
  char status[10];
  bool Sent;
} CPRemoteStartTransactionResponse_t;

typedef struct
{
  char status[10];
  bool Sent;
} CPRemoteStopTransactionResponse_t;

typedef struct
{
  char status[10];
  bool Sent;
  /* data */
} CPReserveNowResponse_t;

typedef struct
{
  char status[10];
  bool Sent;
  /* data */
} CPResetResponse_t;

typedef struct
{
  char status[10];
  bool Sent;
  /* data */
} CPSendLocalListResponse_t;

typedef struct
{
  bool EnergyActiveExportRegister;
  bool EnergyActiveImportRegister;
  bool EnergyReactiveExportRegister;
  bool EnergyReactiveImportRegister;
  bool EnergyActiveExportInterval;
  bool EnergyActiveImportInterval;
  bool EnergyReactiveExportInterval;
  bool EnergyReactiveImportInterval;
  bool PowerActiveExport;
  bool PowerActiveImport;
  bool PowerOffered;
  bool PowerReactiveExport;
  bool PowerReactiveImport;
  bool PowerFactor;
  bool CurrentImport;
  bool CurrentExport;
  bool CurrentOffered;
  bool Voltage;
  bool Frequency;
  bool Temperature;
  bool SoC;
  bool RPM;
} Measurand_t;

typedef struct
{
  bool Core;
  bool LocalAuthListManagement;
  bool Reservation;
  bool RemoteTrigger;
  bool FirmwareManagement;
  bool SmartCharging;
} SupportedFeatureProfiles_t;

typedef struct
{
  // core profile
  bool AuthorizeRemoteTxRequestsReadOnly;
  bool ClockAlignedDataIntervalReadOnly;
  bool ConnectionTimeOutReadOnly;
  bool GetConfigurationMaxKeysReadOnly;
  bool HeartbeatIntervalReadOnly;
  bool LocalAuthorizeOfflineReadOnly;
  bool LocalPreAuthorizeReadOnly;
  bool MeterValuesAlignedDataReadOnly;
  bool MeterValuesSampledDataReadOnly;
  bool MeterValueSampleIntervalReadOnly;
  bool NumberOfConnectorsReadOnly;
  bool ResetRetriesReadOnly;
  bool ConnectorPhaseRotationReadOnly;
  bool StopTransactionOnEVSideDisconnectReadOnly;
  bool StopTransactionOnInvalidIdReadOnly;
  bool StopTxnAlignedDataReadOnly;
  bool StopTxnSampledDataReadOnly;
  bool SupportedFeatureProfilesReadOnly;
  bool TransactionMessageAttemptsReadOnly;
  bool TransactionMessageRetryIntervalReadOnly;
  bool UnlockConnectorOnEVSideDisconnectReadOnly;
  bool AllowOfflineTxForUnknownIdReadOnly;
  bool AuthorizationCacheEnabledReadOnly;
  bool BlinkRepeatReadOnly;
  bool LightIntensityReadOnly;
  bool MaxEnergyOnInvalidIdReadOnly;
  bool MeterValuesAlignedDataMaxLengthReadOnly;
  bool MeterValuesSampledDataMaxLengthReadOnly;
  bool MinimumStatusDurationReadOnly;
  bool ConnectorPhaseRotationMaxLengthReadOnly;
  bool StopTxnAlignedDataMaxLengthReadOnly;
  bool StopTxnSampledDataMaxLengthReadOnly;
  bool SupportedFeatureProfilesMaxLengthReadOnly;
  bool WebSocketPingIntervalReadOnly;
  // Local Auth List Management Profile
  bool LocalAuthListEnabledReadOnly;
  bool LocalAuthListMaxLengthReadOnly;
  bool SendLocalListMaxLengthReadOnly;
  // Reservation Profile
  bool ReserveConnectorZeroSupportedReadOnly;
  // Smart Charging Profile
  bool ChargeProfileMaxStackLevelReadOnly;
  bool ChargingScheduleAllowedChargingRateUnitReadOnly;
  bool ChargingScheduleMaxPeriodsReadOnly;
  bool ConnectorSwitch3to1PhaseSupportedReadOnly;
  bool MaxChargingProfilesInstalledReadOnly;

  // core profile
  char AuthorizeRemoteTxRequestsValue[10];
  char ClockAlignedDataIntervalValue[10];
  char ConnectionTimeOutValue[10];
  char GetConfigurationMaxKeysValue[10];
  char HeartbeatIntervalValue[10];
  char LocalAuthorizeOfflineValue[10];
  char LocalPreAuthorizeValue[10];
  char MeterValuesAlignedDataValue[100];
  char MeterValuesSampledDataValue[100];
  char MeterValueSampleIntervalValue[10];
  char NumberOfConnectorsValue[10];
  char ResetRetriesValue[10];
  char ConnectorPhaseRotationValue[20];
  char StopTransactionOnEVSideDisconnectValue[10];
  char StopTransactionOnInvalidIdValue[10];
  char StopTxnAlignedDataValue[100];
  char StopTxnSampledDataValue[100];
  char SupportedFeatureProfilesValue[100];
  char TransactionMessageAttemptsValue[10];
  char TransactionMessageRetryIntervalValue[10];
  char UnlockConnectorOnEVSideDisconnectValue[10];
  char AllowOfflineTxForUnknownIdValue[10];
  char AuthorizationCacheEnabledValue[10];
  char BlinkRepeatValue[10];
  char LightIntensityValue[10];
  char MaxEnergyOnInvalidIdValue[10];
  char MeterValuesAlignedDataMaxLengthValue[10];
  char MeterValuesSampledDataMaxLengthValue[10];
  char MinimumStatusDurationValue[10];
  char ConnectorPhaseRotationMaxLengthValue[10];
  char StopTxnAlignedDataMaxLengthValue[10];
  char StopTxnSampledDataMaxLengthValue[10];
  char SupportedFeatureProfilesMaxLengthValue[10];
  char WebSocketPingIntervalValue[10];
  // Local Auth List Management Profile
  char LocalAuthListEnabledValue[10];
  char LocalAuthListMaxLengthValue[10];
  char SendLocalListMaxLengthValue[10];
  // Reservation Profile
  char ReserveConnectorZeroSupportedValue[10];
  // Smart Charging Profile
  char ChargeProfileMaxStackLevelValue[10];
  char ChargingScheduleAllowedChargingRateUnitValue[10];
  char ChargingScheduleMaxPeriodsValue[10];
  char ConnectorSwitch3to1PhaseSupportedValue[10];
  char MaxChargingProfilesInstalledValue[10];

  bool AuthorizeRemoteTxRequests;
  uint32_t ClockAlignedDataInterval;
  uint32_t ConnectionTimeOut;
  uint32_t GetConfigurationMaxKeys;
  uint32_t HeartbeatInterval;
  bool LocalAuthorizeOffline;
  bool LocalPreAuthorize;
  Measurand_t MeterValuesAlignedData;
  Measurand_t MeterValuesSampledData;
  uint32_t MeterValueSampleInterval;
  uint32_t NumberOfConnectors;
  uint32_t ResetRetries;
  bool StopTransactionOnEVSideDisconnect;
  bool StopTransactionOnInvalidId;
  Measurand_t StopTxnAlignedData;
  Measurand_t StopTxnSampledData;
  SupportedFeatureProfiles_t SupportedFeatureProfiles;
  uint32_t TransactionMessageAttempts;
  uint32_t TransactionMessageRetryInterval;
  bool UnlockConnectorOnEVSideDisconnect;
  bool AllowOfflineTxForUnknownId;
  bool AuthorizationCacheEnabled;
  uint32_t BlinkRepeat;
  uint32_t LightIntensity;
  uint32_t MaxEnergyOnInvalidId;
  uint32_t MeterValuesAlignedDataMaxLength;
  uint32_t MeterValuesSampledDataMaxLength;
  uint32_t MinimumStatusDuration;
  uint32_t ConnectorPhaseRotationMaxLength;
  uint32_t StopTxnAlignedDataMaxLength;
  uint32_t StopTxnSampledDataMaxLength;
  uint32_t SupportedFeatureProfilesMaxLength;
  size_t WebSocketPingInterval;
  bool LocalAuthListEnabled;
  uint32_t LocalAuthListMaxLength;
  uint32_t SendLocalListMaxLength;
  bool ReserveConnectorZeroSupported;
  uint32_t ChargeProfileMaxStackLevel;
  uint32_t ChargingScheduleMaxPeriods;
  bool ConnectorSwitch3to1PhaseSupported;
  uint32_t MaxChargingProfilesInstalled;
} CPGetConfigurationResponse_t;

typedef struct
{
  char status[10];
  bool Sent;
  /* data */
} CPTriggerMessageResponse_t;

typedef struct
{

  struct
  {
    bool expiryDateReceived;
    char expiryDate[25];
    bool parentidTagReceived;
    char parentidTag[idTagLength];
    char status[13];

  } idtaginfo;
  uint32_t transactionId;
  bool Received;
} CMSStartTransactionResponse_t;

typedef struct
{

  struct
  {
    bool expiryDateReceived;
    char expiryDate[25];
    bool parentidTagReceived;
    char parentidTag[idTagLength];
    char status[13];
    /* data */
  } idtaginfo;
  bool Received;
} CMSStopTransactionResponse_t;

typedef struct
{
  bool Received;
  /* data */
} CMSMeterValuesResponse_t;

/********************************************END Response Structures*****************************************/

void ocpp_response(const char *jsonString);
void ocpp_struct_init(void);
void sendHeartbeatRequest(void);
void sendBootNotificationRequest(void);
void sendStatusNotificationRequest(uint8_t connId);
void sendAuthorizationRequest(uint8_t connId);
void sendFirmwareStatusNotificationRequest(void);
void sendDiagnosticsStatusNotificationRequest(void);
void sendStartTransactionRequest(uint8_t connId);
void sendStopTransactionRequest(uint8_t connId, uint8_t context, bool Aligned, bool offline);
void sendMeterValuesRequest(uint8_t connId, uint8_t context, bool Aligned);
void sendRemoteStartTransactionRequest(void);

void CMSChangeAvailabilityRequestProcess(cJSON *jsonArray);
void CMSChangeConfigurationRequestProcess(cJSON *jsonArray);
void CMSCancelReservationRequestProcess(cJSON *jsonArray);
void CMSFirmwareStatusNotificationRequestProcess(cJSON *jsonArray);
void CMSTriggerMessageRequestProcess(cJSON *jsonArray);
void CMSSendLocalListRequestProcess(cJSON *jsonArray);
void CMSRemoteStartTransactionRequestProcess(cJSON *jsonArray);
void CMSRemoteStopTransactionRequestProcess(cJSON *jsonArray);
void CMSReserveNowRequestProcess(cJSON *jsonArray);
void CMSResetRequestProcess(cJSON *jsonArray);
void CMSUpdateFirmwareRequestProcess(cJSON *jsonArray);
void CMSGetDiagnosticsRequestProcess(cJSON *jsonArray);
void CMSSetChargingProfileRequestProcess(cJSON *jsonArray);
void CMSClearChargingProfileRequestProcess(cJSON *jsonArray);

void sendClearChargingProfileResponse(void);
void sendDataTransferResponse(void);
void sendGetCompositeScheduleResponse(void);
void sendGetDiagnosticsResponse(void);
void sendSetChargingProfileResponse(void);
void sendUnlockConnectorResponse(void);
void sendCancelReservationResponse(void);
void sendChangeAvailabilityResponse(uint8_t connId);
void sendChangeConfigurationResponse(void);
void sendClearCacheResponse(void);
void sendUpdateFirmwareResponse(void);
void sendGetLocalListVersionResponse(void);
void sendHeartbeatResponse(void);
void sendRemoteStartTransactionResponse(uint8_t connId);
void sendRemoteStopTransactionResponse(void);
void sendReserveNowResponse(uint8_t connId);
void sendResetResponse(void);
void SendLocalListResponse(void);
void SendTriggerMessageResponse(void);
void StartTransactionResponse(void);
void StopTransactionResponse(void);
void sendGetConfigurationResponse(void);

void BootNotificationResponseProcess(cJSON *bootResponse);
void AuthorizationResponseProcess(cJSON *authorizationResponse, uint8_t connId);
void CancelReservationResponseProcess(cJSON *CancelReservationResponseV);
void ChangeAvailabilityResponseProcess(cJSON *ChangeAvailabilityResponseV);
void ChangeConfigurationResponseProcess(cJSON *ChangeConfigurationResponseV);
void ClearCacheResponseProcess(cJSON *ClearCacheResponseV);
void HeartbeatResponseProcess(cJSON *HeartbeatResponseV);
void RemoteStartTransactionResponseProcess(cJSON *RemoteStartTransactionV);
void RemoteStopTransactionResponseProcess(cJSON *RemoteStartTransactionV);
void ReserveNowResponseProcess(cJSON *ReserveNowResponseV);
void ResetResponseProcess(cJSON *ResetResponseV);
void SendLocalListResponseProcess(cJSON *SendLocalListResponseV);
void TriggerMessageResponseProcess(cJSON *TriggerMessageResponseV);
void StartTransactionResponseProcess(cJSON *StartTransactionResponseV, uint8_t connId);
void StopTransactionResponseProcess(cJSON *StopTransactionResponseV, uint8_t connId);

#endif