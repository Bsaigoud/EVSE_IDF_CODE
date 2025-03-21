#ifndef PROJECT_SETTINGS_H
#define PROJECT_SETTINGS_H

#define NUM_OF_CONNECTORS 4
#define METER_VALUE_RESET 10000000.0

// common pins
#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK 14
#define PIN_NUM_EM_CS 5
#define RFID_CS_PIN 15
#define RFID_CS_AC1_PIN 0
#define EM_ISO_EN_PIN 4

#define GFCI_V14_PIN 36
#define GFCI_V5_PIN 34
#define GFCI_AC1_PIN 34

#define EARTH_DISCONNECT_V14_PIN 32 // vcharge_lite_1_4 i32 signal pin number 32
#define EARTH_DISCONNECT_V5_PIN 39

#define EMERGENCY_V5_PIN 36
#define MCP_23008_INT_PIN 36

#define RFID_RESET_PIN 22
#define SLAVE_TX_PIN 25
#define SLAVE_RX_PIN 33
#define DISPLAY_TX_PIN 26
#define DISPLAY_RX_PIN 27
#define MODEM_TX_PIN 17
#define MODEM_RX_PIN 16
#define ETH_MOSI_PIN 23
#define ETH_MISO_PIN 19
#define ETH_SCLK_PIN 18
#define ETH_CS_PIN 2

// #define RFID_RESET_PIN              23
#define CONTROL_PILOT_GPIO_V5_PIN 32
#define RMT_LED_STRIP_GPIO_V5_PIN 19
#define RMT_LED_STRIP_GPIO_AC1_PIN 25
#define EXAMPLE_LED_NUMBERS 10
#define EXAMPLE_CHASE_SPEED_MS 10
#define RELAY_V5_PIN 18
#define RELAY_AC1_PIN 33
#define WATCH_DOG_V5_PIN 2
#define WATCH_DOG_AC1_PIN 15

#define LIGHT_SENSOR_AC1_PIN 39

#define I2C1_SDA 21
#define I2C1_SCL 22

#define OCPP_MSG_SIZE 1700

#define DISP_LED 1
#define DISP_CHAR 2
#define DISP_DWIN 3

#define EXP_I2C_PORT 1
#define EXP_ADDR 0x27

#define LCD1_ADDR 0x27
#define LCD2_ADDR 0x26
#define LCD3_ADDR 0x27

#define LCD_COLS 20
#define LCD_ROWS 4

#define TEST 0
typedef struct
{
    char serialNumber[20];
    char chargerName[20];
    char chargePointVendor[20];
    char chargePointModel[20];
    char commissionedBy[20];
    char commissionedDate[20];
    char simIMEINumber[20];
    char simIMSINumber[20];
    char webSocketURL[100];
    bool wifiAvailability;
    uint8_t wifiPriority;
    bool gsmAvailability;
    uint8_t gsmPriority;
    bool ethernetAvailability;
    uint8_t ethernetPriority;
    bool wifiEnable;
    char wifiSSID[50];
    char wifiPassword[50];
    bool gsmEnable;
    char gsmAPN[50];
    bool ethernetEnable;
    bool onlineOffline;
    bool online;
    bool offline;
    bool ethernetConfig;
    char ipAddress[20];
    char gatewayAddress[20];
    char dnsAddress[20];
    char subnetMask[20];
    char macAddress[25];
    uint16_t lowCurrentTime;
    double minLowCurrentThreshold;
    bool resumeSession;
    bool otaEnable;
    bool redirectLogs;
    double CurrentGain;
    double CurrentOffset;
    double CurrentGain1;
    double CurrentGain2;
    uint16_t overVoltageThreshold_old;
    uint16_t underVoltageThreshold_old;
    uint8_t overCurrentThreshold_old;
    uint8_t overTemperatureThreshold_old;
    bool restoreSession;
    uint16_t sessionRestoreTime;
    bool factoryReset;
    bool plugnplay;
    bool gfci;
    bool timestampSync;
    char otaURLConfig[100];
    bool clearAuthCache;
    bool defaultConfig;
    bool vcharge_v5;
    bool vcharge_lite_1_4;
    bool vcharge_lite_1_3;
    bool infra_charger;
    bool sales_charger;
    bool Ac3s;
    bool Ac7s;
    bool Ac11s;
    bool Ac22s;
    bool Ac44s;
    bool Ac14D;
    bool Ac10DP;
    bool Ac10DA;
    bool Ac10t;
    bool Ac14t;
    bool Ac18t;
    uint8_t displayType;
    uint8_t NumofDisplays;
    bool MultiConnectorDisplay;
    uint8_t NumberOfConnectors;
    uint8_t cpDuty[4];
    char firmwareVersion[30];
    char adminpassword[20];
    char factorypassword[20];
    char servicepassword[20];
    char customerpassword[20];
    double overVoltageThreshold;
    double underVoltageThreshold;
    double overCurrentThreshold[4];
    double overTemperatureThreshold;
    char DiagnosticServerUrl[100];
    bool AC1;
    bool AC2;
    bool AC3;
    bool futureSpareBoardTypes[9];
    bool Ac6D;
    bool futureSpareChargerTypes[9];
    bool smartCharging;
    bool BatteryBackup;
    bool DiagnosticServer;
    bool StopTransactionInSuspendedState;
    uint16_t StopTransactionInSuspendedStateTime;
    bool ALPR;
    uint8_t NumOfAlprDevices;
    char slavefirmwareVersion[30];
    uint8_t macAddressofAlprDevice[4][6];
    uint8_t serverSetCpDuty[4];
    uint32_t spare[100];

} Config_t;

#endif