#ifndef OCPP_TASK_H
#define OCPP_TASK_H

#include "EnergyMeter.h"
#include "slave.h"

#define TASK_DELAY_TIME 1000

#define POWERON_LED SOLID_WHITE
#define OTA_LED SOLID_ORANGE
#define COMMISSIONING_LED SOLID_ORANGE
#define STATION_CONNECTED_LED BLINK_WHITE
#define STATION_CONFIG_LED SOLID_WHITE

#define AVAILABLE_LED SOLID_GREEN
#define OFFLINE_AVAILABLE_LED SOLID_GREEN5_SOLID_WHITE1
#define PREPARING_LED BLINK_BLUE
#define OFFLINE_PREPARING_LED BLINK_BLUE5_SOLID_WHITE1
#define SUSPENDED_LED SOLID_BLUE
#define OFFLINE_SUSPENDED_LED SOLID_BLUE5_SOLID_WHITE1
#define CHARGING_LED BLINK_GREEN
#define OFFLINE_CHARGING_LED BLINK_GREEN5_SOLID_WHITE1
#define FINISHING_LED SOLID_GREEN3_SOLID_BLUE1
#define OFFLINE_FINISHING_LED SOLID_GREEN3_SOLID_BLUE1_SOLID_WHITE1
#define EMERGENCY_FAULT_LED BLINK_RED
#define OFFLINE_EMERGENCY_FAULT_LED BLINK_RED5_SOLID_WHITE1
#define FAULT_LED SOLID_RED
#define OFFLINE_FAULT_LED SOLID_RED5_SOLID_WHITE1
#define RESERVATION_LED SOLID_BLUE
#define OFFLINE_RESERVATION_LED SOLID_BLUE5_BLINK_WHITE1
#define UNAVAILABLE_LED SOLID_BLUE
#define OFFLINE_UNAVAILABLE_LED SOLID_BLUE5_BLINK_WHITE1
#define RFID_TAPPED_LED BLINK_WHITE

#define ONLINE_ONLY_AVAILABLE_LED SOLID_GREEN
#define ONLINE_ONLY_OFFLINE_AVAILABLE_LED BLINK_WHITE
#define ONLINE_ONLY_PREPARING_LED BLINK_BLUE
#define ONLINE_ONLY_OFFLINE_PREPARING_LED BLINK_WHITE
#define ONLINE_ONLY_SUSPENDED_LED SOLID_BLUE
#define ONLINE_ONLY_OFFLINE_SUSPENDED_LED SOLID_BLUE5_SOLID_WHITE1
#define ONLINE_ONLY_CHARGING_LED BLINK_GREEN
#define ONLINE_ONLY_OFFLINE_CHARGING_LED BLINK_GREEN5_SOLID_WHITE1
#define ONLINE_ONLY_FINISHING_LED SOLID_GREEN3_SOLID_BLUE1
#define ONLINE_ONLY_OFFLINE_FINISHING_LED SOLID_GREEN3_SOLID_BLUE1_SOLID_WHITE1
#define ONLINE_ONLY_EMERGENCY_LED BLINK_RED
#define ONLINE_ONLY_OFFLINE_EMERGENCY_LED BLINK_RED5_SOLID_WHITE1
#define ONLINE_ONLY_FAULT_LED SOLID_RED
#define ONLINE_ONLY_OFFLINE_FAULT_LED SOLID_RED5_SOLID_WHITE1
#define ONLINE_ONLY_RESERVATION_LED SOLID_BLUE
#define ONLINE_ONLY_OFFLINE_RESERVATION_LED SOLID_BLUE5_SOLID_WHITE1
#define ONLINE_ONLY_UNAVAILABLE_LED SOLID_BLUE
#define ONLINE_ONLY_OFFLINE_UNAVAILABLE_LED SOLID_BLUE5_SOLID_WHITE1

#define OFFLINE_ONLY_AVAILABLE_LED SOLID_GREEN
#define OFFLINE_ONLY_PREPARING_LED BLINK_BLUE
#define OFFLINE_ONLY_SUSPENDED_LED SOLID_BLUE
#define OFFLINE_ONLY_CHARGING_LED BLINK_GREEN
#define OFFLINE_ONLY_FINISHING_LED SOLID_GREEN3_SOLID_BLUE1
#define OFFLINE_ONLY_EMERGENCY_LED BLINK_RED
#define OFFLINE_ONLY_FAULT_LED SOLID_RED
#define OFFLINE_ONLY_RFID_TAPPED_LED SOLID_WHITE

#define idTagLength 21

typedef enum
{
    INIT_STATE = 0,
    POWERON_LED_STATE,
    OTA_LED_STATE,
    COMMISSIONING_LED_STATE,
    AVAILABLE_LED_STATE,
    OFFLINE_AVAILABLE_LED_STATE,
    PREPARING_LED_STATE,
    OFFLINE_PREPARING_LED_STATE,
    SUSPENDED_LED_STATE,
    OFFLINE_SUSPENDED_LED_STATE,
    CHARGING_LED_STATE,
    OFFLINE_CHARGING_LED_STATE,
    FINISHING_LED_STATE,
    OFFLINE_FINISHING_LED_STATE,
    EMERGENCY_FAULT_LED_STATE,
    OFFLINE_EMERGENCY_FAULT_LED_STATE,
    FAULT_LED_STATE,
    OFFLINE_FAULT_LED_STATE,
    RESERVATION_LED_STATE,
    OFFLINE_RESERVATION_LED_STATE,
    UNAVAILABLE_LED_STATE,
    OFFLINE_UNAVAILABLE_LED_STATE,
    RFID_TAPPED_LED_STATE,
    ONLINE_ONLY_AVAILABLE_LED_STATE,
    ONLINE_ONLY_OFFLINE_AVAILABLE_LED_STATE,
    ONLINE_ONLY_PREPARING_LED_STATE,
    ONLINE_ONLY_OFFLINE_PREPARING_LED_STATE,
    ONLINE_ONLY_SUSPENDED_LED_STATE,
    ONLINE_ONLY_OFFLINE_SUSPENDED_LED_STATE,
    ONLINE_ONLY_CHARGING_LED_STATE,
    ONLINE_ONLY_OFFLINE_CHARGING_LED_STATE,
    ONLINE_ONLY_FINISHING_LED_STATE,
    ONLINE_ONLY_OFFLINE_FINISHING_LED_STATE,
    ONLINE_ONLY_EMERGENCY_LED_STATE,
    ONLINE_ONLY_OFFLINE_EMERGENCY_LED_STATE,
    ONLINE_ONLY_FAULT_LED_STATE,
    ONLINE_ONLY_OFFLINE_FAULT_LED_STATE,
    ONLINE_ONLY_RESERVATION_LED_STATE,
    ONLINE_ONLY_OFFLINE_RESERVATION_LED_STATE,
    ONLINE_ONLY_UNAVAILABLE_LED_STATE,
    ONLINE_ONLY_OFFLINE_UNAVAILABLE_LED_STATE,
    OFFLINE_ONLY_AVAILABLE_LED_STATE,
    OFFLINE_ONLY_PREPARING_LED_STATE,
    OFFLINE_ONLY_SUSPENDED_LED_STATE,
    OFFLINE_ONLY_CHARGING_LED_STATE,
    OFFLINE_ONLY_FINISHING_LED_STATE,
    OFFLINE_ONLY_EMERGENCY_LED_STATE,
    OFFLINE_ONLY_FAULT_LED_STATE,
    OFFLINE_ONLY_RFID_TAPPED_LED_STATE,
    EMERGENCY_LOGS_LED_STATE
} ledStates_t;

typedef enum
{
    Stop_EmergencyStop = 0,
    Stop_EVDisconnected,
    Stop_HardReset,
    Stop_Local,
    Stop_Other,
    Stop_PowerLoss,
    Stop_Reboot,
    Stop_Remote,
    Stop_SoftReset,
    Stop_UnlockCommand,
    Stop_DeAuthorized,
} stopReasons_t;

#define ACCEPTED "Accepted"
#define PENDING "Pending"
#define REJECTED "Rejected"
#define SCHEDULED "Scheduled"
#define OCCUPIED "Occupied"
#define Inoperative "Inoperative"
#define Operative "Operative"

#define CONNECTORS_TASK_SIZE 3072
#define BOOT_RETRY_TIME 60
#define BOOT_TIMEOUT 60
#define STATUS_TIMEOUT 60
#define AUTH_TIMEOUT 60
#define START_TRANSACTION_TIMEOUT 120
#define BOOT_OFFLINE_TIMEOUT 10

#define Available "Available"
#define Preparing "Preparing"
#define Charging "Charging"
#define SuspendedEVSE "SuspendedEVSE"
#define SuspendedEV "SuspendedEV"
#define Finishing "Finishing"
#define Reserved "Reserved"
#define Unavailable "Unavailable"
#define Faulted "Faulted"

#define Soft "Soft"
#define Hard "Hard"

#define Idle "Idle"
#define Uploaded "Uploaded"
#define UploadFailed "UploadFailed"
#define Uploading "Uploading"

#define Downloaded "Downloaded"
#define DownloadFailed "DownloadFailed"
#define Downloading "Downloading"
#define Idle "Idle"
#define InstallationFailed "InstallationFailed"
#define Installing "Installing"
#define Installed "Installed"

#define ConnectorLockFailure "ConnectorLockFailure"
#define EVCommunicationError "EVCommunicationError"
#define GroundFailure "GroundFailure"
#define HighTemperature "HighTemperature"
#define InternalError "InternalError"
#define LocalListConflict "LocalListConflict"
#define NoError "NoError"
#define OtherError "OtherError"
#define OverCurrentFailure "OverCurrentFailure"
#define PowerMeterFailure "PowerMeterFailure"
#define PowerSwitchFailure "PowerSwitchFailure"
#define ReaderFailure "ReaderFailure"
#define ResetFailure "ResetFailure"
#define UnderVoltage "UnderVoltage"
#define OverVoltage "OverVoltage"
#define WeakSignal "WeakSignal"
#define Emergency "Emergency"
#define EarthDisconnect "EarthDisconnect"
#define PowerLoss "PowerLoss"
#define WeldDetect "WeldDetect"

#define MeterValueSampleTime 5
#define SmartChargingSchedulePeriodCheckTime 30

#define ALIGNED_DATA true
#define SAMPLED_DATA false

#define OFFLINE_TRANSACTION true
#define ONLINE_TRANSACTION false
typedef struct
{
    uint16_t temp;
    double voltage[4];
    double current[4];
    double power;
    double Energy;
    double meterStop;
} MeterValueAlignedData_t;

typedef struct
{
    char idTag[idTagLength];
    char expiryDate[25];
    char parentidTag[idTagLength];
    int reservationId;
} Reserved_t;

typedef struct
{
    bool isTransactionOngoing;
    bool offlineStarted;
    bool isReserved;
    bool CmsAvailableScheduled;
    bool CmsAvailable;
    bool CmsAvailableChanged;
    bool UnkownId;
    bool InvalidId;
    char idTag[idTagLength];
    uint8_t state;
    uint8_t stopReason;
    uint32_t transactionId;
    uint32_t chargingDuration;
    double meterStart;
    double meterStop;
    MeterValues_t meterValue;
    double Energy;
    Reserved_t ReservedData;
    char meterStart_time[50];
    char meterStop_time[50];
    MeterValues_t meterValueFault;
} ConnectorPrivateData_t;

enum EvseDevStatuse
{
    flag_EVSE_initialisation = 0,
    flag_EVSE_is_Booted,
    flag_EVSE_Read_Id_Tag,
    falg_EVSE_Authentication,
    flag_EVSE_Start_Transaction,
    flag_EVSE_Request_Charge,
    flag_EVSE_Stop_Transaction,
    flag_EVSE_UnAutharized,
    flag_EVSE_Reboot_Request,
    flag_EVSE_Reserve_Now,
    flag_EVSE_Cancle_Reservation,
    flag_EVSE_Local_Authantication
};

enum AC001States
{
    STATUS_STARTUP = 0,
    STATUS_AVAILABLE,
    STATUS_CHARGING
};

#define MESSAGE_TYPE_PAIRING 0
#define MESSAGE_TYPE_DATA 1

#define DEVICE_ID1 (0x01)
#define DEVICE_ID2 (0x02)
#define DEVICE_ID3 (0x03)

#define CAMERA_INFO (0x10)
#define CARAVAIL (0x20)
#define CARPALTENUM (0x21)
#define CARNOTAVAIL (0x22)

typedef struct
{
    uint8_t msgType;
    uint8_t header;
    uint8_t deviceID;
    uint8_t packTYPE;
    uint8_t packLEN;
    char packDATA[15];
    uint16_t checksum;
    uint8_t footer;
} AlprData_t;

void updateAlprInfo(const uint8_t *AlprData, uint8_t length);
void ocpp_task_init(void);
void updateSlaveLeds(void);
void parse_timestamp(const char *timestamp);
void sendOfflineTransactionsData(uint8_t connId);
void clearSendLocalList(void);
void clearLocalAuthenticatinCache(void);
#endif