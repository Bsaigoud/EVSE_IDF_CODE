#ifndef DISPLAY_H
#define DISPLAY_H

#ifndef BIT31
#define BIT31 0x80000000
#endif
#ifndef BIT30
#define BIT30 0x40000000
#endif
#ifndef BIT29
#define BIT29 0x20000000
#endif
#ifndef BIT28
#define BIT28 0x10000000
#endif
#ifndef BIT27
#define BIT27 0x08000000
#endif
#ifndef BIT26
#define BIT26 0x04000000
#endif
#ifndef BIT25
#define BIT25 0x02000000
#endif
#ifndef BIT24
#define BIT24 0x01000000
#endif
#ifndef BIT23
#define BIT23 0x00800000
#endif
#ifndef BIT22
#define BIT22 0x00400000
#endif
#ifndef BIT21
#define BIT21 0x00200000
#endif
#ifndef BIT20
#define BIT20 0x00100000
#endif
#ifndef BIT19
#define BIT19 0x00080000
#endif
#ifndef BIT18
#define BIT18 0x00040000
#endif
#ifndef BIT17
#define BIT17 0x00020000
#endif
#ifndef BIT16
#define BIT16 0x00010000
#endif
#ifndef BIT15
#define BIT15 0x00008000
#endif
#ifndef BIT14
#define BIT14 0x00004000
#endif
#ifndef BIT13
#define BIT13 0x00002000
#endif
#ifndef BIT12
#define BIT12 0x00001000
#endif
#ifndef BIT11
#define BIT11 0x00000800
#endif
#ifndef BIT10
#define BIT10 0x00000400
#endif
#ifndef BIT9
#define BIT9 0x00000200
#endif
#ifndef BIT8
#define BIT8 0x00000100
#endif
#ifndef BIT7
#define BIT7 0x00000080
#endif
#ifndef BIT6
#define BIT6 0x00000040
#endif
#ifndef BIT5
#define BIT5 0x00000020
#endif
#ifndef BIT4
#define BIT4 0x00000010
#endif
#ifndef BIT3
#define BIT3 0x00000008
#endif
#ifndef BIT2
#define BIT2 0x00000004
#endif
#ifndef BIT1
#define BIT1 0x00000002
#endif
#ifndef BIT0
#define BIT0 0x00000001
#endif

#ifndef BIT63
#define BIT63 (0x80000000ULL << 32)
#endif
#ifndef BIT62
#define BIT62 (0x40000000ULL << 32)
#endif
#ifndef BIT61
#define BIT61 (0x20000000ULL << 32)
#endif
#ifndef BIT60
#define BIT60 (0x10000000ULL << 32)
#endif
#ifndef BIT59
#define BIT59 (0x08000000ULL << 32)
#endif
#ifndef BIT58
#define BIT58 (0x04000000ULL << 32)
#endif
#ifndef BIT57
#define BIT57 (0x02000000ULL << 32)
#endif
#ifndef BIT56
#define BIT56 (0x01000000ULL << 32)
#endif
#ifndef BIT55
#define BIT55 (0x00800000ULL << 32)
#endif
#ifndef BIT54
#define BIT54 (0x00400000ULL << 32)
#endif
#ifndef BIT53
#define BIT53 (0x00200000ULL << 32)
#endif
#ifndef BIT52
#define BIT52 (0x00100000ULL << 32)
#endif
#ifndef BIT51
#define BIT51 (0x00080000ULL << 32)
#endif
#ifndef BIT50
#define BIT50 (0x00040000ULL << 32)
#endif
#ifndef BIT49
#define BIT49 (0x00020000ULL << 32)
#endif
#ifndef BIT48
#define BIT48 (0x00010000ULL << 32)
#endif
#ifndef BIT47
#define BIT47 (0x00008000ULL << 32)
#endif
#ifndef BIT46
#define BIT46 (0x00004000ULL << 32)
#endif
#ifndef BIT45
#define BIT45 (0x00002000ULL << 32)
#endif
#ifndef BIT44
#define BIT44 (0x00001000ULL << 32)
#endif
#ifndef BIT43
#define BIT43 (0x00000800ULL << 32)
#endif
#ifndef BIT42
#define BIT42 (0x00000400ULL << 32)
#endif
#ifndef BIT41
#define BIT41 (0x00000200ULL << 32)
#endif
#ifndef BIT40
#define BIT40 (0x00000100ULL << 32)
#endif
#ifndef BIT39
#define BIT39 (0x00000080ULL << 32)
#endif
#ifndef BIT38
#define BIT38 (0x00000040ULL << 32)
#endif
#ifndef BIT37
#define BIT37 (0x00000020ULL << 32)
#endif
#ifndef BIT36
#define BIT36 (0x00000010ULL << 32)
#endif
#ifndef BIT35
#define BIT35 (0x00000008ULL << 32)
#endif
#ifndef BIT34
#define BIT34 (0x00000004ULL << 32)
#endif
#ifndef BIT33
#define BIT33 (0x00000002ULL << 32)
#endif
#ifndef BIT32
#define BIT32 (0x00000001ULL << 32)
#endif

typedef enum
{
    DISPLAY_AVAILABLE = 0,
    DISPLAY_UNAVAILABLE,
    DISPLAY_CHARGING,
    DISPLAY_FINISHING,
    DISPLAY_EV_PLUGGED_TAP_RFID,
    DISPLAY_FAULT,
    DISPLAY_RESERVED,
    DISPLAY_SUSPENDED
} ConnectorDisplayState_t;

#define CHARGER_INITIALIZING_BIT BIT0
#define CHARGER_AVAILABLE1_BIT BIT1
#define CHARGER_AVAILABLE2_BIT BIT2
#define CHARGER_AVAILABLE3_BIT BIT3
#define CHARGER_UNAVAILABLE1_BIT BIT4
#define CHARGER_UNAVAILABLE2_BIT BIT5
#define CHARGER_UNAVAILABLE3_BIT BIT6
#define CHARGER_RFID_TAPPED_BIT BIT7
#define CHARGER_CHARGING1_BIT BIT8
#define CHARGER_CHARGING2_BIT BIT9
#define CHARGER_CHARGING3_BIT BIT10
#define CHARGER_AUTH_SUCCESS_PLUG_EV_BIT BIT11
#define CHARGER_AUTH_FAILED_BIT BIT12
#define CHARGER_FINISHING1_BIT BIT13
#define CHARGER_FINISHING2_BIT BIT14
#define CHARGER_FINISHING3_BIT BIT15
#define CHARGER_EV_PLUGGED_TAP_RFID1_BIT BIT16
#define CHARGER_EV_PLUGGED_TAP_RFID2_BIT BIT17
#define CHARGER_AUTH_SUCCESS_BIT BIT18
#define CHARGER_FAULT1_BIT BIT19
#define CHARGER_FAULT2_BIT BIT20
#define CHARGER_FAULT3_BIT BIT21
#define CHARGER_RESERVED1_BIT BIT22
#define CHARGER_RESERVED2_BIT BIT23
#define CHARGER_RESERVED3_BIT BIT24
#define CHARGER_FIRMWARE_UPDATE_BIT BIT25
#define CHARGER_COMMISSIONING_BIT BIT26
#define CHARGER_NETWORKSWITCH_BIT BIT27
#define CHARGER_SUSPENDED1_BIT BIT28
#define CHARGER_SUSPENDED2_BIT BIT29
#define CHARGER_SUSPENDED3_BIT BIT30
#define CHARGER_EV_PLUGGED_TAP_RFID3_BIT BIT31
#define CHARGER_ERROR_BIT BIT32

#define CONNECTOR_TYPE_3KW "3.3 kW"
#define CONNECTOR_TYPE_7KW "7.4 kW"

#define DWIN_TESTING (0)

#define EVSE_SET_ZERO (0x00)
#define EVSE_SET_ONE (0x01)
#define EVSE_SET_TWO (0x02)
#define EVSE_SET_THREE (0x03)
#define EVSE_SET_FOUR (0x04)
#define EVSE_SET_FIVE (0x05)
#define EVSE_SET_SIX (0x06)
#define EVSE_SET_SEVEN (0x07)
#define EVSE_SET_EIGHT (0x08)
#define EVSE_SET_NINE (0x09)
#define EVSE_SET_TEN (0x0A)
#define EVSE_SET_ELEVEN (0x0B)
#define EVSE_SET_TWELVE (0x0C)
#define EVSE_SET_THIRTEEN (0x0D)
#define EVSE_SET_FOURTEEN (0x0E)
#define EVSE_SET_FIFTEEN (0x0F)
#define EVSE_SET_SIXTEEN (0x10)
#define EVSE_SET_EIGHTEEN (0x12)
#define EVSE_SET_TWENTY (0x14)
#define EVSE_SET_TWENTYTWO (0x16)
#define EVSE_SET_TWENTYFOUR (0x18)
#define EVSE_SET_TWENTYSIX (0x1A)

#define EVSE_SET_THIRTYTWO (0x20)

/**************************************************************************************8*/

#define EVSE_SET_PAGE_ZERO EVSE_SET_ZERO
#define EVSE_SET_PAGE_ONE EVSE_SET_ONE
#define EVSE_SET_PAGE_TWO EVSE_SET_TWO
#define EVSE_SET_PAGE_THREE EVSE_SET_THREE
#define EVSE_SET_PAGE_FOUR EVSE_SET_FOUR
#define EVSE_SET_PAGE_FIVE EVSE_SET_FIVE

#define EVSE_PAGE_PACKET EVSE_SET_ONE
#define EVSE_DATA_PACKET EVSE_SET_TWO

#define EVSE_SET_PACKET_ONE EVSE_SET_ONE
#define EVSE_SET_PACKET_TWO EVSE_SET_TWO
#define EVSE_SET_PACKET_THREE EVSE_SET_THREE
#define EVSE_SET_PACKET_FOUR EVSE_SET_FOUR

/****************************4G display ****************************************/
#define EVSE_4G_CLEAR_LOGO EVSE_SET_ZERO
#define EVSE_4G_AVAILABLE_LOGO EVSE_SET_ONE
#define EVSE_4G_UNAVAILABLE_LOGO EVSE_SET_TWO

#define EVSE_4G_CLEAR_LOGO_ADDR_MSB EVSE_SET_SIXTEEN
#define EVSE_4G_AVAILABLE_LOGO_ADDR_LSB EVSE_SET_THREE

/*****************************WIFI**********************************************/
#define EVSE_WIFI_CLEAR_LOGO EVSE_SET_ZERO
#define EVSE_WIFI_AVAILABLE_LOGO EVSE_SET_ONE
#define EVSE_WIFI_UNAVAILABLE_LOGO EVSE_SET_TWO

#define EVSE_WIFI_CLEAR_LOGO_ADDR_MSB EVSE_SET_SIXTEEN
#define EVSE_WIFI_AVAILABLE_LOGO_ADDR_LSB EVSE_SET_SIX

/*******************************ocpp status************************************************/

#define EVSE_OCPP_STATUS_MSB EVSE_SET_SIXTEEN
#define EVSE_OCPP_STATUS_LSB EVSE_SET_TEN

#define EVSE_AVAILABLE_STATUS EVSE_SET_ZERO
#define EVSE_PREPARING_STATUS EVSE_SET_ONE
#define EVSE_CHARGING_STATUS EVSE_SET_TWO
#define EVSE_SUSPENDEDEVSE_STATUS EVSE_SET_THREE
#define EVSE_SUSPENDEDEV_STATUS EVSE_SET_FOUR
#define EVSE_FINISHING_STATUS EVSE_SET_FIVE
#define EVSE_RESERVED_STATUS EVSE_SET_SIX
#define EVSE_UNAVAILABLE_STATUS EVSE_SET_SEVEN
#define EVSE_FAULTED_STATUS EVSE_SET_EIGHT

#define EVSE_ERROR_CLEAR EVSE_SET_ZERO
#define EVSE_EARTH_DISCONNECT EVSE_SET_ONE
#define EVSE_EMERGANCY_STOP EVSE_SET_TWO
#define EVSE_POWER_LOSS EVSE_SET_THREE
#define EVSE_OTHER_ERROR EVSE_SET_FOUR
#define EVSE_OVER_VOLTAGE EVSE_SET_FIVE
#define EVSE_UNDER_VOLTAGE EVSE_SET_SIX
#define EVSE_OVER_CURRENT EVSE_SET_SEVEN
#define EVSE_OVER_TEMP EVSE_SET_EIGHT

#define EVSE_FAULT_MSB EVSE_SET_SIXTEEN
#define EVSE_FAULT_LSB EVSE_SET_TWELVE

#define EVSE_CLOUD_NULL EVSE_SET_ZERO
#define EVSE_CLOUD_ONLINE EVSE_SET_ONE
#define EVSE_CLOUD_OFFLINE EVSE_SET_TWO

#define EVSE_CLOUD_MSB EVSE_SET_SIXTEEN
#define EVSE_CLOUD_LSB EVSE_SET_FOURTEEN

#define EVSE_VOLTAGE_MSB EVSE_SET_SIXTEEN
#define EVSE_VOLTAGE_LSB EVSE_SET_SIXTEEN
#define EVSE_CURRENT_MSB EVSE_SET_SIXTEEN
#define EVSE_CURRENT_LSB EVSE_SET_EIGHTEEN

#define EVSE_ENERGY_MSB EVSE_SET_SIXTEEN
#define EVSE_ENERGY_LSB EVSE_SET_TWENTY

#define EVSE_TIME_MSB EVSE_SET_SIXTEEN
#define EVSE_TIME_HOURS_LSB EVSE_SET_TWENTYTWO
#define EVSE_TIME_MINUTES_LSB EVSE_SET_TWENTYFOUR
#define EVSE_TIME_SECONDS_LSB EVSE_SET_TWENTYSIX

void SetChargerReservedBit(uint8_t connId);
void SetChargerInitializingBit(void);
void SetChargerCommissioningBit(void);
void SetChargerAvailableBit(uint8_t connId);
void SetChargerSuspendedBit(uint8_t connId);
void SetChargerUnAvailableBit(uint8_t connId);
void SetChargerRfidTappedBit(void);
void SetChargerAuthSuccessPlugEvBit(void);
void SetChargerAuthFailedBit(void);
void SetChargerFinishingdBit(uint8_t connId);
void setChargerFaultBit(uint8_t connId);
void SetChargerChargingBit(uint8_t connId);
void SetChargerAuthSuccessBit(void);
void setChargerEvPluggedTapRfidBit(uint8_t connId);
void SetChargerFirmwareUpdateBit(void);

void LCD_DisplayReinit(void);
void Display_Init(void);
void networkSwitchedUpdateDisplay(void);

#endif