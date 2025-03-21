#ifndef SLAVE_H
#define SLAVE_H

#include <stdint.h>
#include "ProjectSettings.h"
#include "ControlPilotState.h"

#define SERIAL_FEEDBACK 1

#if SERIAL_FEEDBACK
#define SERIAL_FEEDBACK_TIME 10
#endif

#define SLAVE_REQUEST (256)
#define SLAVE_RESPONSE (0)

#define LED0 0
#define LED1 1
#define LED2 2
#define LED3 3

#define RELAY1 1
#define RELAY2 2
#define RELAY3 3

#define PUSH_BUTTON1 0
#define PUSH_BUTTON2 1
#define PUSH_BUTTON3 2
#define PUSH_BUTTON4 3

#define CP1 1
#define CP2 2

typedef enum
{
    SOLID_BLACK = 0,
    SOLID_WHITE,
    SOLID_ORANGE,
    SOLID_GREEN,
    SOLID_BLUE,
    SOLID_RED,
    BLINK_BLUE,
    BLINK_GREEN,
    BLINK_RED,
    BLINK_WHITE,
    SOLID_GREEN5_SOLID_WHITE1,
    BLINK_BLUE5_SOLID_WHITE1,
    SOLID_BLUE5_SOLID_WHITE1,
    BLINK_GREEN5_SOLID_WHITE1,
    SOLID_GREEN3_SOLID_BLUE1,
    SOLID_GREEN3_SOLID_BLUE1_SOLID_WHITE1,
    BLINK_RED5_SOLID_WHITE1,
    SOLID_RED5_SOLID_WHITE1,
    SOLID_BLUE5_BLINK_WHITE1,
} ledColors_t;

enum packetMsgId
{
    LED_ID = 1,
    RELAY_ID,
    PUSHBUTTON_ID,
    CONTROL_PILOT_SET_ID,
    CONTROL_PILOT_READ_ID,
    CP_CONFIG_ID,
    CP_STATE_ID,
    FIRMWARE_VERSION_ID,
    FIRMWARE_UPDATE_START_ID,
    FIRMWARE_DATA_ID,
    FIRMWARE_UPDATE_END_ID,
    FIRMWARE_UPDATE_COMPLETED_ID,
    RESTART_ID,
    FIRMWARE_STATUS_ID,
    FIRMWARE_UPDATE_SUCCEESS_ID,
    FIRMWARE_UPDATE_FAIL_ID
};

typedef struct
{

#if SERIAL_FEEDBACK
    bool LED_ID_RECEIVED;
    bool RELAY_ID_RECEIVED;
    bool PUSHBUTTON_ID_RECEIVED;
    bool CONTROL_PILOT_SET_ID_RECEIVED;
    bool CONTROL_PILOT_READ_ID_RECEIVED;
    bool CP_CONFIG_ID_RECEIVED;
    bool CP_STATE_ID_RECEIVED;
    bool FIRMWARE_VERSION_ID_RECEIVED;
    bool FIRMWARE_UPDATE_START_ID_RECEIVED;
    bool FIRMWARE_DATA_ID_RECEIVED;
    bool FIRMWARE_UPDATE_END_ID_RECEIVED;
    bool FIRMWARE_UPDATE_COMPLETED_ID_RECEIVED;
    bool RESTART_ID_RECEIVED;
    bool FIRMWARE_STATUS_ID_RECEIVED;
    bool FIRMWARE_UPDATE_SUCCEESS_ID_RECEIVED;
    bool FIRMWARE_UPDATE_FAIL_ID_RECEIVED;
#endif

    bool messageStart;
    bool errorStatus;
    bool writeStatus;
    bool errorMessage;
    bool messageCompletedStatus;
    bool messageLengthFirstByteStatus;
    bool messageLengthSecondByteStatus;

    uint8_t CommunicationFlag;
    uint16_t messageLengthFirstByte;
    uint16_t messageLengthSecondByte;
    uint16_t messageLength;
    uint16_t messageReceiveCounter;
    uint16_t messagechecksumValue;
    uint16_t checksumValue;
    uint16_t messageTimeOutCount;
} UART_t;

bool SlaveSendControlPilotConfig(SlaveCpConfig_t SlaveCpConfig);
bool sendFirmwareData(uint8_t *dataToSend, uint32_t length, uint32_t packetNo);
bool SlaveSendID(uint8_t id);
bool SlaveGetState(uint8_t cp);
bool SlaveGetPushButtonState(uint8_t pb);
bool SlaveSetLedColor(uint8_t led, uint8_t color);
bool SlaveSetControlPilotDuty(uint8_t cp, uint8_t duty);
bool SlaveUpdateRelay(uint8_t relay, uint8_t state);
void slave_serial_init(void);

#endif
