#include <driver/i2c.h>
#include "driver/uart.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include <stdio.h>
#include <string.h>
#include <hd44780.h>
#include <pcf8574.h>
#include "esp_netif.h"
#include "rom/ets_sys.h"
#include "sdkconfig.h"
#include "display.h"
#include "websocket_connect.h"
#include "ProjectSettings.h"
#include "ocpp_task.h"
#include "EnergyMeter.h"

#define TAG "DISPLAY"
#define BUF_SIZE_DWIN 250
extern Config_t config;
extern ConnectorPrivateData_t ConnectorData[NUM_OF_CONNECTORS];
extern MeterFaults_t MeterFaults[NUM_OF_CONNECTORS];
extern uint8_t gfciButton;
extern uint8_t relayWeldDetectionButton;
extern uint8_t earthDisconnectButton;
extern uint8_t emergencyButton;
extern MeterFaults_t MeterFaults[NUM_OF_CONNECTORS];
extern bool FirmwareUpdateStarted;
extern bool cpEnable[NUM_OF_CONNECTORS];
extern stopReasons_t stopReasons;
extern double firmwareUpdateProgress;
ConnectorDisplayState_t ConnectorDisplayState;
SemaphoreHandle_t mutexDisplay;
SemaphoreHandle_t mutexDisplayTask;
EventGroupHandle_t evtGrpDisplay;
char LCDbuffer[NUM_OF_CONNECTORS][LCD_COLS * LCD_ROWS];
char updatedbuffer[LCD_COLS * LCD_ROWS];
char reasonForStop[NUM_OF_CONNECTORS][20];
bool dwin_checkResult = false;
uint8_t lcdAddr[NUM_OF_CONNECTORS] = {0x27, LCD1_ADDR, LCD2_ADDR, LCD3_ADDR};

uint8_t dwin_fault = 0xFF;
uint8_t dwin_page = 0xFF;
uint8_t dwin_status = 0xFF;
uint8_t dwin_4g = 0xFF;
uint8_t dwin_wifi = 0xFF;
uint8_t dwin_cloud = 0xFF;
uint16_t dwin_current = 0xFFFF;
uint16_t dwin_voltage = 0xFFFF;
uint16_t dwin_energy = 0xFFFF;

static i2c_dev_t pcf8574[NUM_OF_CONNECTORS];
static hd44780_t lcd[NUM_OF_CONNECTORS];

TaskHandle_t LcdDisplayTask_handle = NULL;

const uart_port_t UART_NUM = UART_NUM_2;

const uint8_t dwin_cmd[8] = {0x5A, 0xA5, 0x05, 0x82, 0x10, 0x03, 0x00, 0x00};
const uint8_t dwin_page_cmd[10] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x00};
uint8_t dwin_buf[8] = {0x5A, 0xA5, 0x05, 0x82, 0x10, 0x03, 0x00, 0x00};
uint8_t dwin_page_buf[10] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x00};

uint8_t displayState[NUM_OF_CONNECTORS] = {0, 0, 0, 0};

uint32_t ConnectorReservedBit[NUM_OF_CONNECTORS] = {0, CHARGER_RESERVED1_BIT, CHARGER_RESERVED2_BIT, CHARGER_RESERVED3_BIT};
uint32_t ConnectorAvailableBit[NUM_OF_CONNECTORS] = {0, CHARGER_AVAILABLE1_BIT, CHARGER_AVAILABLE2_BIT, CHARGER_AVAILABLE3_BIT};
uint32_t ConnectorSuspendedBit[NUM_OF_CONNECTORS] = {0, CHARGER_SUSPENDED1_BIT, CHARGER_SUSPENDED2_BIT, CHARGER_SUSPENDED3_BIT};
uint32_t ConnectorUnAvailableBit[NUM_OF_CONNECTORS] = {0, CHARGER_UNAVAILABLE1_BIT, CHARGER_UNAVAILABLE2_BIT, CHARGER_UNAVAILABLE3_BIT};
uint32_t ConnectorChargingBit[NUM_OF_CONNECTORS] = {0, CHARGER_CHARGING1_BIT, CHARGER_CHARGING2_BIT, CHARGER_CHARGING3_BIT};
uint32_t ConnectorFaultBit[NUM_OF_CONNECTORS] = {0, CHARGER_FAULT1_BIT, CHARGER_FAULT2_BIT, CHARGER_FAULT3_BIT};
uint32_t ConnectorEvPluggedTapRfidBit[NUM_OF_CONNECTORS] = {0, CHARGER_EV_PLUGGED_TAP_RFID1_BIT, CHARGER_EV_PLUGGED_TAP_RFID2_BIT, CHARGER_EV_PLUGGED_TAP_RFID3_BIT};
uint32_t ConnectorFinishingBit[NUM_OF_CONNECTORS] = {0, CHARGER_FINISHING1_BIT, CHARGER_FINISHING2_BIT, CHARGER_FINISHING3_BIT};
bool connectorPageEnabled[NUM_OF_CONNECTORS] = {true, false, false, false};
bool connectingLcdSet = false;
uint8_t connectorRating[NUM_OF_CONNECTORS] = {3, 3, 3, 3};

void updateConnectorType(uint8_t connId)
{
    if (cpEnable[connId])
    {
        if (connId == 1)
        {
            if (config.Ac11s)
            {
                connectorRating[connId] = 11;
            }
            else if (config.Ac22s)
            {
                connectorRating[connId] = 22;
            }
            else if (config.Ac44s)
            {
                connectorRating[connId] = 44;
            }
            else
            {
                connectorRating[connId] = 7;
            }
        }
        else
        {
            connectorRating[connId] = 7;
        }
    }
    else
    {
        connectorRating[connId] = 3;
    }
}

void getReasonForStop(uint8_t connId)
{
    memset(reasonForStop[connId], '\0', sizeof(reasonForStop[connId]));
    switch (ConnectorData[connId].stopReason)
    {
    case Stop_EmergencyStop:
        memcpy(reasonForStop[connId], "EMERGENCY", strlen("EMERGENCY"));
        break;
    case Stop_EVDisconnected:
        memcpy(reasonForStop[connId], "STOPPED BY EV", strlen("STOPPED BY EV"));
        break;
    case Stop_HardReset:
        memcpy(reasonForStop[connId], "POWER LOSS", strlen("POWER LOSS"));
        break;
    case Stop_Local:
        memcpy(reasonForStop[connId], "STOPPED BY USER", strlen("STOPPED BY USER"));
        break;
    case Stop_Other:
        if (earthDisconnectButton == 1)
            memcpy(reasonForStop[connId], "EARTH DISCONNECTED", strlen("EARTH DISCONNECTED"));
        else if (gfciButton == 1)
            memcpy(reasonForStop[connId], "LEAKAGE CURRENT", strlen("LEAKAGE CURRENT"));
        else if (MeterFaults[connId].MeterHighTemp == 1)
            snprintf(reasonForStop[connId], sizeof(reasonForStop[connId]), "OVER TEMP-%dC", (ConnectorData[connId].meterValueFault.temp));
        else if (MeterFaults[connId].MeterOverVoltage == 1)
            snprintf(reasonForStop[connId], sizeof(reasonForStop[connId]), "OVER VOLTAGE-%.1fV", ConnectorData[connId].meterValueFault.voltage[connId]);
        else if (MeterFaults[connId].MeterUnderVoltage == 1)
            snprintf(reasonForStop[connId], sizeof(reasonForStop[connId]), "UNDER VOLTAGE-%.1fV", ConnectorData[connId].meterValueFault.voltage[connId]);
        else if (MeterFaults[connId].MeterOverCurrent == 1)
            snprintf(reasonForStop[connId], sizeof(reasonForStop[connId]), "OVER CURRENT-%.1fA", ConnectorData[connId].meterValueFault.current[connId]);
        else
            memcpy(reasonForStop[connId], "OTHER", strlen("OTHER"));
        break;
    case Stop_PowerLoss:
        memcpy(reasonForStop[connId], "POWER LOSS", strlen("POWER LOSS"));
        break;
    case Stop_Reboot:
        memcpy(reasonForStop[connId], "POWER LOSS", strlen("POWER LOSS"));
        break;
    case Stop_Remote:
        memcpy(reasonForStop[connId], "STOPPED BY USER", strlen("STOPPED BY USER"));
        break;
    case Stop_SoftReset:
        memcpy(reasonForStop[connId], "POWER LOSS", strlen("POWER LOSS"));
        break;
    case Stop_UnlockCommand:
        memcpy(reasonForStop[connId], "EMERGENCY", strlen("EMERGENCY"));
        break;
    case Stop_DeAuthorized:
        memcpy(reasonForStop[connId], "EMERGENCY", strlen("EMERGENCY"));
        break;

    default:
        break;
    }
}

void convertSecondsToTime(int totalSeconds, char *buffer)
{
    int hours, minutes, seconds;

    hours = totalSeconds / 3600;
    totalSeconds = totalSeconds % 3600;
    minutes = totalSeconds / 60;
    seconds = totalSeconds % 60;

    sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);
}

void dwin_send_cmd(uint8_t pkt_type)
{
    switch (pkt_type)
    {
    case EVSE_PAGE_PACKET:
        uart_write_bytes(UART_NUM, (const char *)dwin_page_buf, 10);
        uart_wait_tx_done(UART_NUM, 100);
        break;

    case EVSE_DATA_PACKET:
        uart_write_bytes(UART_NUM, (const char *)dwin_buf, 8);
        uart_wait_tx_done(UART_NUM, 100);
        break;

    default:
        break;
    }

    // evse_dump_cmd(pkt_type);
    if (dwin_checkResult)
    {
        // evse_check_return();
    }
}

void evse_page_change(uint8_t page_num)
{
    if (dwin_page != page_num)
    {
        memcpy(dwin_page_buf, dwin_page_cmd, 10);
        dwin_page_buf[9] = page_num;
        dwin_send_cmd(EVSE_PAGE_PACKET);
        dwin_page = page_num;
    }
}

void evse_update_status(uint8_t status)
{
    if (dwin_status != status)
    {
        memcpy(dwin_buf, dwin_cmd, 8);
        dwin_buf[4] = EVSE_OCPP_STATUS_MSB;
        dwin_buf[5] = EVSE_OCPP_STATUS_LSB;
        dwin_buf[6] = EVSE_SET_ZERO;
        dwin_buf[7] = status;
        dwin_send_cmd(EVSE_DATA_PACKET);
        dwin_status = status;
    }
}

void evse_update_fault(uint8_t fault)
{
    if (dwin_fault != fault)
    {
        memcpy(dwin_buf, dwin_cmd, 8);
        dwin_buf[4] = EVSE_FAULT_MSB;
        dwin_buf[5] = EVSE_FAULT_LSB;
        dwin_buf[6] = EVSE_SET_ZERO;
        dwin_buf[7] = fault;
        dwin_send_cmd(EVSE_DATA_PACKET);
        dwin_fault = fault;
    }
}

void evse_update_4G_logo(uint8_t logo)
{
    if (dwin_4g != logo)
    {
        memcpy(dwin_buf, dwin_cmd, 8);
        dwin_buf[4] = EVSE_4G_CLEAR_LOGO_ADDR_MSB;
        dwin_buf[5] = EVSE_4G_AVAILABLE_LOGO_ADDR_LSB;
        dwin_buf[6] = EVSE_SET_ZERO;
        dwin_buf[7] = logo;
        dwin_send_cmd(EVSE_DATA_PACKET);
        dwin_4g = logo;
    }
}

void evse_update_wifi_logo(uint8_t logo)
{
    if (dwin_wifi != logo)
    {
        memcpy(dwin_buf, dwin_cmd, 8);
        dwin_buf[4] = EVSE_WIFI_CLEAR_LOGO_ADDR_MSB;
        dwin_buf[5] = EVSE_WIFI_AVAILABLE_LOGO_ADDR_LSB;
        dwin_buf[6] = EVSE_SET_ZERO;
        dwin_buf[7] = logo;
        dwin_send_cmd(EVSE_DATA_PACKET);
        dwin_wifi = logo;
    }
}

void evse_update_cloud_status(uint8_t status)
{
    if (dwin_cloud != status)
    {
        memcpy(dwin_buf, dwin_cmd, 8);
        dwin_buf[4] = EVSE_CLOUD_MSB;
        dwin_buf[5] = EVSE_CLOUD_LSB;
        dwin_buf[6] = EVSE_SET_ZERO;
        dwin_buf[7] = status;
        dwin_send_cmd(EVSE_DATA_PACKET);
        dwin_cloud = status;
    }
}

void evse_voltage_value(double voltage)
{
    uint16_t present_voltage = (uint16_t)(voltage * 10);
    if (dwin_voltage != present_voltage)
    {
        memcpy(dwin_buf, dwin_cmd, 8);
        dwin_buf[4] = EVSE_VOLTAGE_MSB;
        dwin_buf[5] = EVSE_VOLTAGE_LSB;
        dwin_buf[6] = (present_voltage >> 8);
        dwin_buf[7] = (present_voltage & 0xff);
        dwin_send_cmd(EVSE_DATA_PACKET);
        dwin_voltage = present_voltage;
    }
}

void evse_current_value(double current)
{
    uint16_t present_current = (uint16_t)(current * 100);
    if (dwin_current != present_current)
    {
        memcpy(dwin_buf, dwin_cmd, 8);
        dwin_buf[4] = EVSE_CURRENT_MSB;
        dwin_buf[5] = EVSE_CURRENT_LSB;
        dwin_buf[6] = (present_current >> 8);
        dwin_buf[7] = (present_current & 0xff);
        dwin_send_cmd(EVSE_DATA_PACKET);
        dwin_current = present_current;
    }
}

void evse_energy_value(double energy)
{
    // uint16_t dwin_energy = energy * 100;
    uint16_t present_energy = (uint16_t)(energy / 10); // for kwh caluclation
    if (dwin_energy != present_energy)
    {
        memcpy(dwin_buf, dwin_cmd, 8);
        dwin_buf[4] = EVSE_ENERGY_MSB;
        dwin_buf[5] = EVSE_ENERGY_LSB;
        dwin_buf[6] = (present_energy >> 8);
        dwin_buf[7] = (present_energy & 0xff);
        dwin_send_cmd(EVSE_DATA_PACKET);
        dwin_energy = present_energy;
    }
}

void evse_update_time(uint32_t time)
{
    uint8_t hours = time / 3600;
    uint8_t minutes = (time % 3600) / 60;
    uint8_t seconds = time % 60;
    memcpy(dwin_buf, dwin_cmd, 8);
    dwin_buf[4] = EVSE_TIME_MSB;
    dwin_buf[5] = EVSE_TIME_HOURS_LSB;
    dwin_buf[6] = EVSE_SET_ZERO;
    dwin_buf[7] = hours;
    dwin_send_cmd(EVSE_DATA_PACKET);

    dwin_buf[5] = EVSE_TIME_MINUTES_LSB;
    dwin_buf[7] = minutes;
    dwin_send_cmd(EVSE_DATA_PACKET);

    dwin_buf[5] = EVSE_TIME_SECONDS_LSB;
    dwin_buf[7] = seconds;
    dwin_send_cmd(EVSE_DATA_PACKET);
}

void UpdateLcdDisplay(uint8_t dispId)
{
    esp_err_t err;
    char buffer[81];

    memset(buffer, '\0', sizeof(buffer));
    memcpy(&buffer[0], &updatedbuffer[0], 20);
    memcpy(&buffer[20], &updatedbuffer[40], 20);
    memcpy(&buffer[40], &updatedbuffer[20], 20);
    memcpy(&buffer[60], &updatedbuffer[60], 20);
    memcpy(&LCDbuffer[dispId], &updatedbuffer[0], 80);

    uint8_t length = sizeof(updatedbuffer);
    hd44780_gotoxy(&lcd[dispId], 0, 0);
    hd44780_puts(&lcd[dispId], buffer);
    // err = LCD_setCursor(lcdAddr[dispId], 0, 0);
    // if (err != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "Failed to set cursor: %d", err);
    //     // LCD_DisplayReinit();
    // }
    // else
    // {
    //     for (uint8_t i = 0; i < length; i++)
    //     {
    //         err = LCD_writeChar(lcdAddr[dispId], buffer[i]);
    //         if (err != ESP_OK)
    //         {
    //             ESP_LOGE(TAG, "Failed to write char: %d", err);
    //             // LCD_DisplayReinit();
    //             break;
    //         }
    //     }
    // }
}

void SetChargerInitializingBit(void)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        xTaskNotify(LcdDisplayTask_handle, CHARGER_INITIALIZING_BIT, eSetBits);
        ESP_LOGD(TAG, "CHARGER_INITIALIZING_BIT Set");
    }
}

void SetChargerReservedBit(uint8_t connId)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        if (config.MultiConnectorDisplay == false)
            xTaskNotify(LcdDisplayTask_handle, ConnectorReservedBit[connId], eSetBits);
        else
            displayState[connId] = DISPLAY_RESERVED;
        ESP_LOGD(TAG, "Connector %hhu Reserved Bit Set", connId);
    }
}

void SetChargerCommissioningBit(void)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        xTaskNotify(LcdDisplayTask_handle, CHARGER_COMMISSIONING_BIT, eSetBits);
        ESP_LOGD(TAG, "CHARGER_COMMISSIONING_BIT Set");
    }
}

void setChargerFaultBit(uint8_t connId)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        if (config.MultiConnectorDisplay == false)
            xTaskNotify(LcdDisplayTask_handle, ConnectorFaultBit[connId], eSetBits);
        else
            displayState[connId] = DISPLAY_FAULT;
        ESP_LOGD(TAG, "Connector %hhu Fault Bit Set", connId);
    }
}

void SetChargerAvailableBit(uint8_t connId)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        if (config.MultiConnectorDisplay == false)
            xTaskNotify(LcdDisplayTask_handle, ConnectorAvailableBit[connId], eSetBits);
        else
            displayState[connId] = DISPLAY_AVAILABLE;
        ESP_LOGD(TAG, "Connector %hhu Available Bit Set", connId);
    }
}

void SetChargerSuspendedBit(uint8_t connId)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        if (config.MultiConnectorDisplay == false)
            xTaskNotify(LcdDisplayTask_handle, ConnectorSuspendedBit[connId], eSetBits);
        else
            displayState[connId] = DISPLAY_SUSPENDED;
        ESP_LOGD(TAG, "Connector %hhu Available Bit Set", connId);
    }
}

void SetChargerUnAvailableBit(uint8_t connId)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        if (config.MultiConnectorDisplay == false)
            xTaskNotify(LcdDisplayTask_handle, ConnectorUnAvailableBit[connId], eSetBits);
        else
            displayState[connId] = DISPLAY_UNAVAILABLE;
        ESP_LOGD(TAG, "Connector %hhu UnAvailable Bit Set", connId);
    }
}

void SetChargerRfidTappedBit(void)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        xTaskNotify(LcdDisplayTask_handle, CHARGER_RFID_TAPPED_BIT, eSetBits);
        ESP_LOGD(TAG, "CHARGER_RFID_TAPPED_BIT Set");
    }
}

void SetChargerAuthSuccessPlugEvBit(void)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        xTaskNotify(LcdDisplayTask_handle, CHARGER_AUTH_SUCCESS_PLUG_EV_BIT, eSetBits);
        ESP_LOGD(TAG, "CHARGER_AUTH_SUCCESS_PLUG_EV_BIT Set");
    }
}

void SetChargerAuthFailedBit(void)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        xTaskNotify(LcdDisplayTask_handle, CHARGER_AUTH_FAILED_BIT, eSetBits);
        ESP_LOGD(TAG, "CHARGER_AUTH_FAILED_BIT Set");
    }
}

void SetChargerAuthSuccessBit(void)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        xTaskNotify(LcdDisplayTask_handle, CHARGER_AUTH_SUCCESS_BIT, eSetBits);
        ESP_LOGD(TAG, "CHARGER_AUTH_SUCCESS_BIT Set");
    }
}

void SetChargerFirmwareUpdateBit(void)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        xTaskNotify(LcdDisplayTask_handle, CHARGER_FIRMWARE_UPDATE_BIT, eSetBits);
        ESP_LOGD(TAG, "CHARGER_FIRMWARE_UPDATE_BIT Set");
    }
}

void SetChargerFinishingdBit(uint8_t connId)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        if (config.MultiConnectorDisplay == false)
            xTaskNotify(LcdDisplayTask_handle, ConnectorFinishingBit[connId], eSetBits);
        else
            displayState[connId] = DISPLAY_FINISHING;
        ESP_LOGD(TAG, "Connector %hhu Finishing Bit Set", connId);
    }
}

void SetChargerChargingBit(uint8_t connId)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        if (config.MultiConnectorDisplay == false)
            xTaskNotify(LcdDisplayTask_handle, ConnectorChargingBit[connId], eSetBits);
        else
            displayState[connId] = DISPLAY_CHARGING;
        ESP_LOGD(TAG, "Connector %hhu Charging Bit Set", connId);
    }
}

void setChargerEvPluggedTapRfidBit(uint8_t connId)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        if (config.MultiConnectorDisplay == false)
            xTaskNotify(LcdDisplayTask_handle, ConnectorEvPluggedTapRfidBit[connId], eSetBits);
        else
            displayState[connId] = DISPLAY_EV_PLUGGED_TAP_RFID;
        ESP_LOGD(TAG, "Connector %hhu Ev Plugged Tap Rfid Bit Set", connId);
    }
}

void setLcdConnectorPageBit(uint8_t connId)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        switch (displayState[connId])
        {
        case DISPLAY_AVAILABLE:
            xTaskNotify(LcdDisplayTask_handle, ConnectorAvailableBit[connId], eSetBits);
            break;
        case DISPLAY_SUSPENDED:
            xTaskNotify(LcdDisplayTask_handle, ConnectorSuspendedBit[connId], eSetBits);
            break;
        case DISPLAY_UNAVAILABLE:
            xTaskNotify(LcdDisplayTask_handle, ConnectorUnAvailableBit[connId], eSetBits);
            break;
        case DISPLAY_FINISHING:
            xTaskNotify(LcdDisplayTask_handle, ConnectorFinishingBit[connId], eSetBits);
            break;
        case DISPLAY_CHARGING:
            xTaskNotify(LcdDisplayTask_handle, ConnectorChargingBit[connId], eSetBits);
            break;
        case DISPLAY_EV_PLUGGED_TAP_RFID:
            xTaskNotify(LcdDisplayTask_handle, ConnectorEvPluggedTapRfidBit[connId], eSetBits);
            break;
        case DISPLAY_RESERVED:
            xTaskNotify(LcdDisplayTask_handle, ConnectorReservedBit[connId], eSetBits);
            break;
        case DISPLAY_FAULT:
            xTaskNotify(LcdDisplayTask_handle, ConnectorFaultBit[connId], eSetBits);
        default:
            break;
        }
        ESP_LOGD(TAG, "Connector %hhu Page Bit Set", connId);
    }
}

void setLcdChargerReserved(uint8_t connId)
{
    bool isConnected = WebsocketConnectedisConnected();
    bool connectedToEthernet = false;
    bool connectedToWifi = false;
    bool connectedToGsm = false;
    if (isConnected)
    {
        esp_netif_t *default_netif = esp_netif_get_default_netif();
        if (default_netif != NULL)
        {
            if (strcmp(esp_netif_get_desc(default_netif), "ppp") == 0)
            {
                connectedToGsm = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "sta") == 0)
            {
                connectedToWifi = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "eth") == 0)
            {
                connectedToEthernet = true;
            }
        }
    }
    if (config.displayType == DISP_CHAR)
    {
        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        char lcdString[81];
        memset(lcdString, '\0', sizeof(lcdString));
        char *ptrlcdString =
            "                    "
            "                    "
            "                    "
            "                    ";
        if (connectedToEthernet)
        {
            ptrlcdString =
                "                 ETH"
                "                    "
                "RESERVED            "
                "                    ";
        }
        else if (connectedToWifi)
        {
            ptrlcdString =
                "                 WFI"
                "                    "
                "RESERVED            "
                "                    ";
        }
        else if (connectedToGsm)
        {
            ptrlcdString =
                "                 GSM"
                "                    "
                "RESERVED            "
                "                    ";
        }
        else
        {
            ptrlcdString =
                "                 OFL"
                "                    "
                "RESERVED            "
                "                    ";
        }

        updateConnectorType(connId);
        strncpy(lcdString, ptrlcdString, sizeof(lcdString));

        char outputType[21];
        snprintf(outputType, sizeof(outputType), "C-%hhu:%hhukW %s", connId, connectorRating[connId], (cpEnable[connId] ? "TYPE-2" : "SOCKET"));

        strncpy(lcdString, outputType, strlen(outputType));

        memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
        if (config.MultiConnectorDisplay == true)
            UpdateLcdDisplay(1);
        else
            UpdateLcdDisplay(connId);
        xSemaphoreGive(mutexDisplay);
    }
    else if (config.displayType == DISP_DWIN)
    {
        evse_page_change(EVSE_SET_PAGE_ONE);
        evse_update_status(EVSE_RESERVED_STATUS);
        evse_update_fault(EVSE_ERROR_CLEAR);
        if (connectedToGsm)
            evse_update_4G_logo(EVSE_4G_AVAILABLE_LOGO);
        else if (config.gsmAvailability && config.gsmEnable)
            evse_update_4G_logo(EVSE_4G_UNAVAILABLE_LOGO);
        else
            evse_update_4G_logo(EVSE_4G_CLEAR_LOGO);

        if (connectedToWifi)
            evse_update_wifi_logo(EVSE_WIFI_AVAILABLE_LOGO);
        else if (config.wifiAvailability && config.wifiEnable)
            evse_update_wifi_logo(EVSE_WIFI_UNAVAILABLE_LOGO);
        else
            evse_update_wifi_logo(EVSE_WIFI_CLEAR_LOGO);

        if (isConnected)
            evse_update_cloud_status(EVSE_CLOUD_ONLINE);
        else
            evse_update_cloud_status(EVSE_CLOUD_OFFLINE);
    }
}

void setLcdChargerFault(uint8_t connId)
{
    bool isConnected = WebsocketConnectedisConnected();
    bool connectedToEthernet = false;
    bool connectedToWifi = false;
    bool connectedToGsm = false;

    char FaultStr[21];
    uint8_t faultCode = EVSE_ERROR_CLEAR;
    memset(FaultStr, '\0', sizeof(FaultStr));
    if (emergencyButton == 1)
    {
        faultCode = EVSE_EMERGANCY_STOP;
        memcpy(FaultStr, "EMERGENCY PRESSED", strlen("EMERGENCY PRESSED"));
    }
    else if (gfciButton == 1)
    {
        faultCode = EVSE_OTHER_ERROR;
        memcpy(FaultStr, "LEAKAGE CURRENT", strlen("LEAKAGE CURRENT"));
    }
    else if (MeterFaults[connId].MeterPowerLoss == 1)
    {
        faultCode = EVSE_POWER_LOSS;
        memcpy(FaultStr, "POWER LOSS", strlen("POWER LOSS"));
    }
    else if (earthDisconnectButton == 1)
    {
        faultCode = EVSE_EARTH_DISCONNECT;
        memcpy(FaultStr, "EARTH DISCONNECTED", strlen("EARTH DISCONNECTED"));
    }
    else if (MeterFaults[connId].MeterOverVoltage == 1)
    {
        faultCode = EVSE_OVER_VOLTAGE;
        snprintf(FaultStr, sizeof(FaultStr), "OVER VOLTAGE-%.1fV", ConnectorData[connId].meterValueFault.voltage[connId]);
    }
    else if (MeterFaults[connId].MeterUnderVoltage == 1)
    {
        faultCode = EVSE_UNDER_VOLTAGE;
        snprintf(FaultStr, sizeof(FaultStr), "UNDER VOLTAGE-%.1fV", ConnectorData[connId].meterValueFault.voltage[connId]);
    }
    else if (MeterFaults[connId].MeterOverCurrent == 1)
    {
        faultCode = EVSE_OVER_CURRENT;
        snprintf(FaultStr, sizeof(FaultStr), "OVER CURRENT-%.1fA", ConnectorData[connId].meterValueFault.current[connId]);
    }
    else if (MeterFaults[connId].MeterHighTemp == 1)
    {
        faultCode = EVSE_OVER_TEMP;
        snprintf(FaultStr, sizeof(FaultStr), "OVER TEMP-%dC", ConnectorData[connId].meterValueFault.temp);
    }
    else if (relayWeldDetectionButton == 1)
    {
        faultCode = EVSE_OTHER_ERROR;
        memcpy(FaultStr, "RELAY WELD DETECTED", strlen("RELAY WELD DETECTED"));
    }
    else if (MeterFaults[connId].MeterFailure == 1)
    {
        faultCode = EVSE_OTHER_ERROR;
        memcpy(FaultStr, "INTERNAL FAULT", strlen("INTERNAL FAULT"));
    }
    else
    {
        faultCode = EVSE_OTHER_ERROR;
        memcpy(FaultStr, "INTERNAL FAULT", strlen("INTERNAL FAULT"));
    }

    if (isConnected)
    {
        esp_netif_t *default_netif = esp_netif_get_default_netif();
        if (default_netif != NULL)
        {
            if (strcmp(esp_netif_get_desc(default_netif), "ppp") == 0)
            {
                connectedToGsm = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "sta") == 0)
            {
                connectedToWifi = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "eth") == 0)
            {
                connectedToEthernet = true;
            }
        }
    }
    if (config.displayType == DISP_CHAR)
    {
        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        char lcdString[81];
        memset(lcdString, '\0', sizeof(lcdString));
        char *ptrlcdString =
            "                    "
            "                    "
            "                    "
            "                    ";
        if (connectedToEthernet)
        {
            ptrlcdString =
                "                 ETH"
                "                    "
                "FAULT:              "
                "                    ";
        }
        else if (connectedToWifi)
        {
            ptrlcdString =
                "                 WFI"
                "                    "
                "FAULT:              "
                "                    ";
        }
        else if (connectedToGsm)
        {
            ptrlcdString =
                "                 GSM"
                "                    "
                "FAULT:              "
                "                    ";
        }
        else
        {
            ptrlcdString =
                "                 OFL"
                "                    "
                "FAULT:              "
                "                    ";
        }

        updateConnectorType(connId);
        strncpy(lcdString, ptrlcdString, sizeof(lcdString));

        char outputType[21];
        memset(outputType, "\0", sizeof(outputType));
        snprintf(outputType, sizeof(outputType), "C-%hhu:%hhukW %s", connId, connectorRating[connId], (cpEnable[connId] ? "TYPE-2" : "SOCKET"));

        strncpy(lcdString, outputType, strlen(outputType));
        strncpy(lcdString + 61 - 1, FaultStr, strlen(FaultStr));

        memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
        if (config.MultiConnectorDisplay == true)
            UpdateLcdDisplay(1);
        else
            UpdateLcdDisplay(connId);
        xSemaphoreGive(mutexDisplay);
    }

    else if (config.displayType == DISP_DWIN)
    {
        if (ConnectorData[connId].isTransactionOngoing)
            evse_page_change(EVSE_SET_PAGE_THREE);
        else
            evse_page_change(EVSE_SET_PAGE_ONE);

        evse_update_status(EVSE_FAULTED_STATUS);
        evse_update_fault(faultCode);
        if (connectedToGsm)
            evse_update_4G_logo(EVSE_4G_AVAILABLE_LOGO);
        else if (config.gsmAvailability && config.gsmEnable)
            evse_update_4G_logo(EVSE_4G_UNAVAILABLE_LOGO);
        else
            evse_update_4G_logo(EVSE_4G_CLEAR_LOGO);

        if (connectedToWifi)
            evse_update_wifi_logo(EVSE_WIFI_AVAILABLE_LOGO);
        else if (config.wifiAvailability && config.wifiEnable)
            evse_update_wifi_logo(EVSE_WIFI_UNAVAILABLE_LOGO);
        else
            evse_update_wifi_logo(EVSE_WIFI_CLEAR_LOGO);

        if (isConnected)
            evse_update_cloud_status(EVSE_CLOUD_ONLINE);
        else
            evse_update_cloud_status(EVSE_CLOUD_OFFLINE);
    }
}

void setLcdChargerEvPluggedTapRfid(uint8_t connId)
{
    bool isConnected = WebsocketConnectedisConnected();
    bool connectedToEthernet = false;
    bool connectedToWifi = false;
    bool connectedToGsm = false;
    if (isConnected)
    {
        esp_netif_t *default_netif = esp_netif_get_default_netif();
        if (default_netif != NULL)
        {
            if (strcmp(esp_netif_get_desc(default_netif), "ppp") == 0)
            {
                connectedToGsm = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "sta") == 0)
            {
                connectedToWifi = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "eth") == 0)
            {
                connectedToEthernet = true;
            }
        }
    }
    if (config.displayType == DISP_CHAR)
    {
        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        char lcdString[81];
        memset(lcdString, '\0', sizeof(lcdString));
        char *ptrlcdString =
            "                    "
            "                    "
            "                    "
            "                    ";
        if (connectedToEthernet)
        {
            ptrlcdString =
                "                 ETH"
                "                    "
                "EV PLUGGED IN       "
                "                    ";
        }
        else if (connectedToWifi)
        {
            ptrlcdString =
                "                 WFI"
                "                    "
                "EV PLUGGED IN       "
                "                    ";
        }
        else if (connectedToGsm)
        {
            ptrlcdString =
                "                 GSM"
                "                    "
                "EV PLUGGED IN       "
                "                    ";
        }
        else
        {
            ptrlcdString =
                "                 OFL"
                "                    "
                "EV PLUGGED IN       "
                "                    ";
        }

        updateConnectorType(connId);
        strncpy(lcdString, ptrlcdString, sizeof(lcdString));
        char outputType[21];
        snprintf(outputType, sizeof(outputType), "C-%hhu:%hhukW %s", connId, connectorRating[connId], (cpEnable[connId] ? "TYPE-2" : "SOCKET"));
        strncpy(lcdString, outputType, strlen(outputType));

        memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
        if (config.MultiConnectorDisplay == true)
            UpdateLcdDisplay(1);
        else
            UpdateLcdDisplay(connId);
        xSemaphoreGive(mutexDisplay);
    }
    else if (config.displayType == DISP_DWIN)
    {
        evse_page_change(EVSE_SET_PAGE_ONE);
        evse_update_status(EVSE_PREPARING_STATUS);
        evse_update_fault(EVSE_ERROR_CLEAR);
        if (connectedToGsm)
            evse_update_4G_logo(EVSE_4G_AVAILABLE_LOGO);
        else if (config.gsmAvailability && config.gsmEnable)
            evse_update_4G_logo(EVSE_4G_UNAVAILABLE_LOGO);
        else
            evse_update_4G_logo(EVSE_4G_CLEAR_LOGO);

        if (connectedToWifi)
            evse_update_wifi_logo(EVSE_WIFI_AVAILABLE_LOGO);
        else if (config.wifiAvailability && config.wifiEnable)
            evse_update_wifi_logo(EVSE_WIFI_UNAVAILABLE_LOGO);
        else
            evse_update_wifi_logo(EVSE_WIFI_CLEAR_LOGO);

        if (isConnected)
            evse_update_cloud_status(EVSE_CLOUD_ONLINE);
        else
            evse_update_cloud_status(EVSE_CLOUD_OFFLINE);
    }
}

void setLcdChargerRfidTapped(void)
{
    if (config.displayType == DISP_CHAR)
    {
        char *lcdString =
            "                    "
            "RFID TAPPED         "
            "ATHENTICATING       "
            "                    ";
        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
        if (config.MultiConnectorDisplay == true)
            UpdateLcdDisplay(1);
        else
        {
            if (config.NumberOfConnectors == 1)
            {
                UpdateLcdDisplay(1);
            }
            else if (config.NumberOfConnectors == 2)
            {
                UpdateLcdDisplay(1);
                UpdateLcdDisplay(2);
            }
            else if (config.NumberOfConnectors == 3)
            {
                UpdateLcdDisplay(1);
                UpdateLcdDisplay(2);
                UpdateLcdDisplay(3);
            }
        }
        xSemaphoreGive(mutexDisplay);
    }
    else if (config.displayType == DISP_DWIN)
    {
        uint8_t current_page = dwin_page;
        evse_page_change(EVSE_SET_PAGE_FIVE);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        evse_page_change(current_page);
    }
}

void setLcdChargerAuthSuccessPlugEV(void)
{
    if (config.displayType == DISP_CHAR)
    {
        char *lcdString =
            "AUTHENTICATION      "
            "SUCCESSFUL          "
            "PLUG IN YOUR EV     "
            "                    ";
        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
        if (config.MultiConnectorDisplay == true)
            UpdateLcdDisplay(1);
        else
        {
            if (config.NumberOfConnectors == 1)
            {
                UpdateLcdDisplay(1);
            }
            else if (config.NumberOfConnectors == 2)
            {
                UpdateLcdDisplay(1);
                UpdateLcdDisplay(2);
            }
            else if (config.NumberOfConnectors == 3)
            {
                UpdateLcdDisplay(1);
                UpdateLcdDisplay(2);
                UpdateLcdDisplay(3);
            }
        }
        xSemaphoreGive(mutexDisplay);
    }
}

void setLcdChargerAuthSuccess(void)
{
    if (config.displayType == DISP_CHAR)
    {
        char *lcdString =
            "                    "
            "AUTHENTICATION      "
            "SUCCESSFUL          "
            "                    ";
        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
        if (config.MultiConnectorDisplay == true)
            UpdateLcdDisplay(1);
        else
        {
            if (config.NumberOfConnectors == 1)
            {
                UpdateLcdDisplay(1);
            }
            else if (config.NumberOfConnectors == 2)
            {
                UpdateLcdDisplay(1);
                UpdateLcdDisplay(2);
            }
            else if (config.NumberOfConnectors == 3)
            {
                UpdateLcdDisplay(1);
                UpdateLcdDisplay(2);
                UpdateLcdDisplay(3);
            }
        }
        xSemaphoreGive(mutexDisplay);
    }
}

void setLcdChargerAuthFailed(void)
{
    if (config.displayType == DISP_CHAR)
    {
        char *lcdString =
            "                    "
            "AUTHENTICATION      "
            "DENIED              "
            "                    ";
        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
        if (config.MultiConnectorDisplay == true)
            UpdateLcdDisplay(1);
        else
        {
            if (config.NumberOfConnectors == 1)
            {
                UpdateLcdDisplay(1);
            }
            else if (config.NumberOfConnectors == 2)
            {
                UpdateLcdDisplay(1);
                UpdateLcdDisplay(2);
            }
            else if (config.NumberOfConnectors == 3)
            {
                UpdateLcdDisplay(1);
                UpdateLcdDisplay(2);
                UpdateLcdDisplay(3);
            }
        }
        xSemaphoreGive(mutexDisplay);
    }
}

void setLcdChargerFinishing(uint8_t connId)
{
    bool isConnected = WebsocketConnectedisConnected();
    bool connectedToEthernet = false;
    bool connectedToWifi = false;
    bool connectedToGsm = false;
    if (isConnected)
    {
        esp_netif_t *default_netif = esp_netif_get_default_netif();
        if (default_netif != NULL)
        {
            if (strcmp(esp_netif_get_desc(default_netif), "ppp") == 0)
            {
                connectedToGsm = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "sta") == 0)
            {
                connectedToWifi = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "eth") == 0)
            {
                connectedToEthernet = true;
            }
        }
    }
    if (config.displayType == DISP_CHAR)
    {
        char lcdString[81];
        memset(lcdString, '\0', sizeof(lcdString));
        char *ptrlcdString =
            "                    "
            "                    "
            "                    "
            "                    ";
        if (connectedToEthernet)
        {
            ptrlcdString =
                "                 ETH"
                "CHARGING COMPLETE   "
                "E:    kWh T:        "
                "                    ";
        }
        else if (connectedToWifi)
        {
            ptrlcdString =
                "                 WFI"
                "CHARGING COMPLETE   "
                "E:    kWh T:        "
                "                    ";
        }
        else if (connectedToGsm)
        {
            ptrlcdString =
                "                 GSM"
                "CHARGING COMPLETE   "
                "E:    kWh T:        "
                "                    ";
        }
        else
        {
            ptrlcdString =
                "                 OFL"
                "CHARGING COMPLETE   "
                "E:    kWh T:        "
                "                    ";
        }
        strncpy(lcdString, ptrlcdString, sizeof(lcdString));
        updateConnectorType(connId);
        getReasonForStop(connId);
        char energy[10];
        memset(energy, '\0', sizeof(energy));
        snprintf(energy, sizeof(energy), "%.1f", (ConnectorData[connId].Energy / 1000.0));
        char timeBuffer[10];
        memset(timeBuffer, '\0', sizeof(timeBuffer));
        convertSecondsToTime(ConnectorData[connId].chargingDuration, timeBuffer);
        char outputType[21];
        snprintf(outputType, sizeof(outputType), "C-%hhu:%hhukW %s", connId, connectorRating[connId], (cpEnable[connId] ? "TYPE-2" : "SOCKET"));
        strncpy(lcdString, outputType, strlen(outputType));
        strncpy(lcdString + 43 - 1, energy, strlen(energy));
        strncpy(lcdString + 53 - 1, timeBuffer, strlen(timeBuffer));
        strncpy(lcdString + 61 - 1, reasonForStop[connId], strlen(reasonForStop[connId]));

        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
        if (config.MultiConnectorDisplay == true)
            UpdateLcdDisplay(1);
        else
            UpdateLcdDisplay(connId);
        xSemaphoreGive(mutexDisplay);
    }
    else if (config.displayType == DISP_DWIN)
    {
        evse_page_change(EVSE_SET_PAGE_FOUR);
        evse_update_status(EVSE_FINISHING_STATUS);
        evse_update_fault(EVSE_ERROR_CLEAR);
        if (connectedToGsm)
            evse_update_4G_logo(EVSE_4G_AVAILABLE_LOGO);
        else if (config.gsmAvailability && config.gsmEnable)
            evse_update_4G_logo(EVSE_4G_UNAVAILABLE_LOGO);
        else
            evse_update_4G_logo(EVSE_4G_CLEAR_LOGO);

        if (connectedToWifi)
            evse_update_wifi_logo(EVSE_WIFI_AVAILABLE_LOGO);
        else if (config.wifiAvailability && config.wifiEnable)
            evse_update_wifi_logo(EVSE_WIFI_UNAVAILABLE_LOGO);
        else
            evse_update_wifi_logo(EVSE_WIFI_CLEAR_LOGO);

        if (isConnected)
            evse_update_cloud_status(EVSE_CLOUD_ONLINE);
        else
            evse_update_cloud_status(EVSE_CLOUD_OFFLINE);

        evse_energy_value(ConnectorData[connId].Energy);
        evse_update_time(ConnectorData[connId].chargingDuration);
    }
}

void setLcdChargerCharging(uint8_t connId)
{
    bool isConnected = WebsocketConnectedisConnected();
    bool connectedToEthernet = false;
    bool connectedToWifi = false;
    bool connectedToGsm = false;
    if (isConnected)
    {
        esp_netif_t *default_netif = esp_netif_get_default_netif();
        if (default_netif != NULL)
        {
            if (strcmp(esp_netif_get_desc(default_netif), "ppp") == 0)
            {
                connectedToGsm = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "sta") == 0)
            {
                connectedToWifi = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "eth") == 0)
            {
                connectedToEthernet = true;
            }
        }
    }
    if (config.displayType == DISP_CHAR)
    {
        char lcdString[81];
        memset(lcdString, '\0', sizeof(lcdString));
        char *ptrlcdString =
            "                    "
            "                    "
            "                    "
            "                    ";
        if (connectedToEthernet)
        {
            ptrlcdString =
                "                 ETH"
                "CHARGING    P:    kW"
                "E:     kWh  V:     V"
                "T:          C:     A";
        }
        else if (connectedToWifi)
        {
            ptrlcdString =
                "                 WFI"
                "CHARGING    P:    kW"
                "E:     kWh  V:     V"
                "T:          C:     A";
        }
        else if (connectedToGsm)
        {
            ptrlcdString =
                "                 GSM"
                "CHARGING    P:    kW"
                "E:     kWh  V:     V"
                "T:          C:     A";
        }
        else
        {
            ptrlcdString =
                "                 OFL"
                "CHARGING    P:    kW"
                "E:     kWh  V:     V"
                "T:          C:     A";
        }
        updateConnectorType(connId);
        strncpy(lcdString, ptrlcdString, sizeof(lcdString));
        char power[10];
        memset(power, '\0', sizeof(power));
        if (ConnectorData[connId].meterValue.power >= 10.0)
        {
            snprintf(power, sizeof(power), "%.1f", (ConnectorData[connId].meterValue.power / 1000.0));
        }
        else
        {
            snprintf(power, sizeof(power), "%.2f", (ConnectorData[connId].meterValue.power / 1000.0));
        }
        char energy[10];
        memset(energy, '\0', sizeof(energy));
        snprintf(energy, sizeof(energy), "%.2f", (ConnectorData[connId].Energy / 1000.0));
        char voltage[10];
        memset(voltage, '\0', sizeof(voltage));
        snprintf(voltage, sizeof(voltage), "%.1f", (ConnectorData[connId].meterValue.voltage[connId]));
        char current[10];
        memset(current, '\0', sizeof(current));
        if (ConnectorData[connId].meterValue.current[connId] >= 10.0)
        {
            snprintf(current, sizeof(current), "%.1f", (ConnectorData[connId].meterValue.current[connId]));
        }
        else
        {
            snprintf(current, sizeof(current), "%.2f", (ConnectorData[connId].meterValue.current[connId]));
        }
        char timeBuffer[10];
        memset(timeBuffer, '\0', sizeof(timeBuffer));
        convertSecondsToTime(ConnectorData[connId].chargingDuration, timeBuffer);
        char outputType[21];
        snprintf(outputType, sizeof(outputType), "C-%hhu:%hhukW %s", connId, connectorRating[connId], (cpEnable[connId] ? "TYPE-2" : "SOCKET"));
        strncpy(lcdString, outputType, strlen(outputType));
        strncpy(lcdString + 35 - 1, power, strlen(power));
        strncpy(lcdString + 43 - 1, energy, strlen(energy));
        strncpy(lcdString + 55 - 1, voltage, strlen(voltage));
        strncpy(lcdString + 63 - 1, timeBuffer, strlen(timeBuffer));
        strncpy(lcdString + 75 - 1, current, strlen(current));

        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
        if (config.MultiConnectorDisplay == true)
            UpdateLcdDisplay(1);
        else
            UpdateLcdDisplay(connId);
        xSemaphoreGive(mutexDisplay);
    }
    else if (config.displayType == DISP_DWIN)
    {
        evse_page_change(EVSE_SET_PAGE_TWO);
        evse_update_status(EVSE_CHARGING_STATUS);
        evse_update_fault(EVSE_ERROR_CLEAR);
        if (connectedToGsm)
            evse_update_4G_logo(EVSE_4G_AVAILABLE_LOGO);
        else if (config.gsmAvailability && config.gsmEnable)
            evse_update_4G_logo(EVSE_4G_UNAVAILABLE_LOGO);
        else
            evse_update_4G_logo(EVSE_4G_CLEAR_LOGO);

        if (connectedToWifi)
            evse_update_wifi_logo(EVSE_WIFI_AVAILABLE_LOGO);
        else if (config.wifiAvailability && config.wifiEnable)
            evse_update_wifi_logo(EVSE_WIFI_UNAVAILABLE_LOGO);
        else
            evse_update_wifi_logo(EVSE_WIFI_CLEAR_LOGO);

        if (isConnected)
            evse_update_cloud_status(EVSE_CLOUD_ONLINE);
        else
            evse_update_cloud_status(EVSE_CLOUD_OFFLINE);

        evse_voltage_value(ConnectorData[connId].meterValue.voltage[connId]);
        evse_current_value(ConnectorData[connId].meterValue.current[connId]);
        evse_energy_value(ConnectorData[connId].Energy);
        evse_update_time(ConnectorData[connId].chargingDuration);
    }
}

void setLcdChargerUnAvailable(uint8_t connId)
{
    bool isConnected = WebsocketConnectedisConnected();
    bool connectedToEthernet = false;
    bool connectedToWifi = false;
    bool connectedToGsm = false;
    if (isConnected)
    {
        esp_netif_t *default_netif = esp_netif_get_default_netif();
        if (default_netif != NULL)
        {
            if (strcmp(esp_netif_get_desc(default_netif), "ppp") == 0)
            {
                connectedToGsm = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "sta") == 0)
            {
                connectedToWifi = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "eth") == 0)
            {
                connectedToEthernet = true;
            }
        }
    }
    if (config.displayType == DISP_CHAR)
    {
        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        char lcdString[81];
        memset(lcdString, '\0', sizeof(lcdString));
        char *ptrlcdString =
            "                    "
            "                    "
            "                    "
            "                    ";
        if (connectedToEthernet)
        {
            ptrlcdString =
                "                 ETH"
                "                    "
                "UNAVAILABLE         "
                "                    ";
        }
        else if (connectedToWifi)
        {
            ptrlcdString =
                "                 WFI"
                "                    "
                "UNAVAILABLE         "
                "                    ";
        }
        else if (connectedToGsm)
        {
            ptrlcdString =
                "                 GSM"
                "                    "
                "UNAVAILABLE         "
                "                    ";
        }
        else
        {
            ptrlcdString =
                "                 OFL"
                "                    "
                "UNAVAILABLE         "
                "                    ";
        }

        updateConnectorType(connId);
        strncpy(lcdString, ptrlcdString, sizeof(lcdString));

        char outputType[21];
        snprintf(outputType, sizeof(outputType), "C-%hhu:%hhukW %s", connId, connectorRating[connId], (cpEnable[connId] ? "TYPE-2" : "SOCKET"));
        strncpy(lcdString, outputType, strlen(outputType));

        memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
        if (config.MultiConnectorDisplay == true)
            UpdateLcdDisplay(1);
        else
            UpdateLcdDisplay(connId);
        xSemaphoreGive(mutexDisplay);
    }
    else if (config.displayType == DISP_DWIN)
    {
        evse_page_change(EVSE_SET_PAGE_ONE);
        evse_update_status(EVSE_UNAVAILABLE_STATUS);
        evse_update_fault(EVSE_ERROR_CLEAR);
        if (connectedToGsm)
            evse_update_4G_logo(EVSE_4G_AVAILABLE_LOGO);
        else if (config.gsmAvailability && config.gsmEnable)
            evse_update_4G_logo(EVSE_4G_UNAVAILABLE_LOGO);
        else
            evse_update_4G_logo(EVSE_4G_CLEAR_LOGO);

        if (connectedToWifi)
            evse_update_wifi_logo(EVSE_WIFI_AVAILABLE_LOGO);
        else if (config.wifiAvailability && config.wifiEnable)
            evse_update_wifi_logo(EVSE_WIFI_UNAVAILABLE_LOGO);
        else
            evse_update_wifi_logo(EVSE_WIFI_CLEAR_LOGO);

        if (isConnected)
            evse_update_cloud_status(EVSE_CLOUD_ONLINE);
        else
            evse_update_cloud_status(EVSE_CLOUD_OFFLINE);
    }
}

void setLcdChargerSuspended(uint8_t connId)
{
    bool isConnected = WebsocketConnectedisConnected();
    bool connectedToEthernet = false;
    bool connectedToWifi = false;
    bool connectedToGsm = false;
    if (isConnected)
    {
        esp_netif_t *default_netif = esp_netif_get_default_netif();
        if (default_netif != NULL)
        {
            if (strcmp(esp_netif_get_desc(default_netif), "ppp") == 0)
            {
                connectedToGsm = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "sta") == 0)
            {
                connectedToWifi = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "eth") == 0)
            {
                connectedToEthernet = true;
            }
        }
    }
    if (config.displayType == DISP_CHAR)
    {
        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        char lcdString[81];
        memset(lcdString, '\0', sizeof(lcdString));
        char *ptrlcdString =
            "                    "
            "                    "
            "                    "
            "                    ";
        if (connectedToEthernet)
        {
            ptrlcdString =
                "                 ETH"
                "                    "
                "READY TO CHARGE     "
                "                    ";
        }
        else if (connectedToWifi)
        {
            ptrlcdString =
                "                 WFI"
                "                    "
                "READY TO CHARGE     "
                "                    ";
        }
        else if (connectedToGsm)
        {
            ptrlcdString =
                "                 GSM"
                "                    "
                "READY TO CHARGE     "
                "                    ";
        }
        else
        {
            ptrlcdString =
                "                 OFL"
                "                    "
                "READY TO CHARGE     "
                "                    ";
        }

        updateConnectorType(connId);
        strncpy(lcdString, ptrlcdString, sizeof(lcdString));

        char outputType[21];
        snprintf(outputType, sizeof(outputType), "C-%hhu:%hhukW %s", connId, connectorRating[connId], (cpEnable[connId] ? "TYPE-2" : "SOCKET"));
        strncpy(lcdString, outputType, strlen(outputType));

        memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
        if (config.MultiConnectorDisplay == true)
            UpdateLcdDisplay(1);
        else
            UpdateLcdDisplay(connId);
        xSemaphoreGive(mutexDisplay);
    }
    else if (config.displayType == DISP_DWIN)
    {
        // evse_page_change(EVSE_SET_PAGE_ONE);
        // evse_update_status(EVSE_AVAILABLE_STATUS);
        // evse_update_fault(EVSE_ERROR_CLEAR);
        // if (connectedToGsm)
        //     evse_update_4G_logo(EVSE_4G_AVAILABLE_LOGO);
        // else if (config.gsmAvailability && config.gsmEnable)
        //     evse_update_4G_logo(EVSE_4G_UNAVAILABLE_LOGO);
        // else
        //     evse_update_4G_logo(EVSE_4G_CLEAR_LOGO);

        // if (connectedToWifi)
        //     evse_update_wifi_logo(EVSE_WIFI_AVAILABLE_LOGO);
        // else if (config.wifiAvailability && config.wifiEnable)
        //     evse_update_wifi_logo(EVSE_WIFI_UNAVAILABLE_LOGO);
        // else
        //     evse_update_wifi_logo(EVSE_WIFI_CLEAR_LOGO);

        // if (isConnected)
        //     evse_update_cloud_status(EVSE_CLOUD_ONLINE);
        // else
        //     evse_update_cloud_status(EVSE_CLOUD_OFFLINE);
    }
}

void setLcdChargerAvailable(uint8_t connId)
{
    bool isConnected = WebsocketConnectedisConnected();
    bool connectedToEthernet = false;
    bool connectedToWifi = false;
    bool connectedToGsm = false;
    if (isConnected)
    {
        esp_netif_t *default_netif = esp_netif_get_default_netif();
        if (default_netif != NULL)
        {
            if (strcmp(esp_netif_get_desc(default_netif), "ppp") == 0)
            {
                connectedToGsm = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "sta") == 0)
            {
                connectedToWifi = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "eth") == 0)
            {
                connectedToEthernet = true;
            }
        }
    }
    if (config.displayType == DISP_CHAR)
    {
        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        char lcdString[81];
        memset(lcdString, '\0', sizeof(lcdString));
        char *ptrlcdString =
            "                    "
            "                    "
            "                    "
            "                    ";
        if (connectedToEthernet)
        {
            ptrlcdString =
                "                 ETH"
                "                    "
                "AVAILABLE           "
                "                    ";
        }
        else if (connectedToWifi)
        {
            ptrlcdString =
                "                 WFI"
                "                    "
                "AVAILABLE           "
                "                    ";
        }
        else if (connectedToGsm)
        {
            ptrlcdString =
                "                 GSM"
                "                    "
                "AVAILABLE           "
                "                    ";
        }
        else
        {
            ptrlcdString =
                "                 OFL"
                "                    "
                "AVAILABLE           "
                "                    ";
        }

        // if (config.offline && config.plugnplay && cpEnable[connId])
        // {
        //     ptrlcdString =
        //         "                 OFL"
        //         " CHARGER AVAILABLE  "
        //         "  PLUG EV TO START  "
        //         "     CHARGING       ";
        // }
        updateConnectorType(connId);
        strncpy(lcdString, ptrlcdString, sizeof(lcdString));

        // if (config.MultiConnectorDisplay == true)
        // {
        //     char connectorBuffer[15];
        //     memset(connectorBuffer, '\0', sizeof(connectorBuffer));
        //     sprintf(connectorBuffer, "CON:%hhu", connId);
        //     strncpy(lcdString, connectorBuffer, strlen(connectorBuffer));
        // }
        char outputType[21];
        snprintf(outputType, sizeof(outputType), "C-%hhu:%hhukW %s", connId, connectorRating[connId], (cpEnable[connId] ? "TYPE-2" : "SOCKET"));

        strncpy(lcdString, outputType, strlen(outputType));

        memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
        if (config.MultiConnectorDisplay == true)
            UpdateLcdDisplay(1);
        else
            UpdateLcdDisplay(connId);
        xSemaphoreGive(mutexDisplay);
    }
    else if (config.displayType == DISP_DWIN)
    {
        evse_page_change(EVSE_SET_PAGE_ONE);
        evse_update_status(EVSE_AVAILABLE_STATUS);
        evse_update_fault(EVSE_ERROR_CLEAR);
        if (connectedToGsm)
            evse_update_4G_logo(EVSE_4G_AVAILABLE_LOGO);
        else if (config.gsmAvailability && config.gsmEnable)
            evse_update_4G_logo(EVSE_4G_UNAVAILABLE_LOGO);
        else
            evse_update_4G_logo(EVSE_4G_CLEAR_LOGO);

        if (connectedToWifi)
            evse_update_wifi_logo(EVSE_WIFI_AVAILABLE_LOGO);
        else if (config.wifiAvailability && config.wifiEnable)
            evse_update_wifi_logo(EVSE_WIFI_UNAVAILABLE_LOGO);
        else
            evse_update_wifi_logo(EVSE_WIFI_CLEAR_LOGO);

        if (isConnected)
            evse_update_cloud_status(EVSE_CLOUD_ONLINE);
        else
            evse_update_cloud_status(EVSE_CLOUD_OFFLINE);
    }
}

void setLcdChargerCommissioning(void)
{
    if (config.displayType == DISP_CHAR)
    {
        char *lcdString =
            "                    "
            "COMMISSIONING       "
            "IN PROGRESS         "
            "                    ";

        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
        if (config.NumofDisplays == config.NumberOfConnectors)
        {
            for (uint8_t connId = 1; connId <= config.NumberOfConnectors; connId++)
            {
                UpdateLcdDisplay(connId);
            }
        }
        else
        {
            UpdateLcdDisplay(1);
        }
        xSemaphoreGive(mutexDisplay);
    }
}

void setLcdChargerFirmwareUpdating(void)
{
    bool isConnected = WebsocketConnectedisConnected();
    bool connectedToEthernet = false;
    bool connectedToWifi = false;
    bool connectedToGsm = false;
    if (isConnected)
    {
        esp_netif_t *default_netif = esp_netif_get_default_netif();
        if (default_netif != NULL)
        {
            if (strcmp(esp_netif_get_desc(default_netif), "ppp") == 0)
            {
                connectedToGsm = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "sta") == 0)
            {
                connectedToWifi = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "eth") == 0)
            {
                connectedToEthernet = true;
            }
        }
    }
    if (config.displayType == DISP_CHAR)
    {
        char lcdString[81];
        memset(lcdString, '\0', sizeof(lcdString));
        char *ptrlcdString =
            "                    "
            "                    "
            "                    "
            "                    ";
        if (connectedToEthernet)
        {
            ptrlcdString =
                "                 ETH"
                "CHARGER UPDATING    "
                "DO NOT TURN OFF     "
                "PROGRESS:           ";
        }
        else if (connectedToWifi)
        {
            ptrlcdString =
                "                 WFI"
                "CHARGER UPDATING    "
                "DO NOT TURN OFF     "
                "PROGRESS:           ";
        }
        else if (connectedToGsm)
        {
            ptrlcdString =
                "                 GSM"
                "CHARGER UPDATING    "
                "DO NOT TURN OFF     "
                "PROGRESS:           ";
        }
        else
        {
            ptrlcdString =
                "                 OFL"
                "CHARGER UPDATING    "
                "DO NOT TURN OFF     "
                "PROGRESS:           ";
        }
        strncpy(lcdString, ptrlcdString, sizeof(lcdString));

        char downloadProgress[8];
        snprintf(downloadProgress, sizeof(downloadProgress), "%.2f%%", firmwareUpdateProgress);
        strncpy(lcdString + 70, downloadProgress, strlen(downloadProgress));
        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
        if (config.NumofDisplays == config.NumberOfConnectors)
        {
            for (uint8_t connId = 1; connId <= config.NumberOfConnectors; connId++)
            {
                UpdateLcdDisplay(connId);
            }
        }
        else
        {
            UpdateLcdDisplay(1);
        }
        xSemaphoreGive(mutexDisplay);
    }
    else if (config.displayType == DISP_DWIN)
    {
    }
}

void setLcdChargerInitializing(void)
{
    if (config.displayType == DISP_CHAR)
    {
        char *lcdString =
            "                 OFL"
            "                    "
            "POWERING ON         "
            "                    ";
        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
        if (config.MultiConnectorDisplay == true)
            UpdateLcdDisplay(1);
        else
        {
            if (config.NumberOfConnectors == 1)
            {
                UpdateLcdDisplay(1);
            }
            else if (config.NumberOfConnectors == 2)
            {
                UpdateLcdDisplay(1);
                UpdateLcdDisplay(2);
            }
            else if (config.NumberOfConnectors == 3)
            {
                UpdateLcdDisplay(1);
                UpdateLcdDisplay(2);
                UpdateLcdDisplay(3);
            }
        }

        xSemaphoreGive(mutexDisplay);
    }
    else if (config.displayType == DISP_DWIN)
    {
        evse_page_change(EVSE_SET_PAGE_ZERO);
    }
}

void setLcdNetWorkChange(void)
{
    bool isConnected = WebsocketConnectedisConnected();
    bool connectedToEthernet = false;
    bool connectedToWifi = false;
    bool connectedToGsm = false;
    if (isConnected)
    {
        esp_netif_t *default_netif = esp_netif_get_default_netif();
        if (default_netif != NULL)
        {
            if (strcmp(esp_netif_get_desc(default_netif), "ppp") == 0)
            {
                connectedToGsm = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "sta") == 0)
            {
                connectedToWifi = true;
            }
            else if (strcmp(esp_netif_get_desc(default_netif), "eth") == 0)
            {
                connectedToEthernet = true;
            }
        }
    }
    if (config.displayType == DISP_CHAR)
    {

        char lcdString[81];
        memset(lcdString, '\0', sizeof(lcdString));
        char *ptrlcdString =
            "                    ";
        if (connectedToEthernet)
        {
            ptrlcdString =
                "                 ETH";
        }
        else if (connectedToWifi)
        {
            ptrlcdString =
                "                 WFI";
        }
        else if (connectedToGsm)
        {
            ptrlcdString =
                "                 GSM";
        }
        else
        {
            ptrlcdString =
                "                 OFL";
        }
        for (uint8_t i = 1; i <= config.NumberOfConnectors; i++)
        {

            strncpy(lcdString, LCDbuffer[i], 80);
            strncpy(lcdString, ptrlcdString, 20);
            char outputType[21];
            snprintf(outputType, sizeof(outputType), "C-%hhu:%hhukW %s", i, connectorRating[i], (cpEnable[i] ? "TYPE-2" : "SOCKET"));

            strncpy(lcdString, outputType, strlen(outputType));
            xSemaphoreTake(mutexDisplay, portMAX_DELAY);
            memcpy(updatedbuffer, lcdString, sizeof(updatedbuffer));
            UpdateLcdDisplay(i);
            xSemaphoreGive(mutexDisplay);
        }
    }
}

void networkSwitchedUpdateDisplay(void)
{
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        if (config.MultiConnectorDisplay == false)
            xTaskNotify(LcdDisplayTask_handle, CHARGER_NETWORKSWITCH_BIT, eSetBits);
    }
}

void displayPageTask(void *pvParameters)
{
    uint8_t pageNum = 1;
    uint8_t TotalPages = 1;

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    while (1)
    {
        if (FirmwareUpdateStarted == false)
        {
            xSemaphoreTake(mutexDisplayTask, portMAX_DELAY);
            setLcdConnectorPageBit(pageNum);
            xSemaphoreGive(mutexDisplayTask);
            pageNum++;
            if (pageNum > config.NumberOfConnectors)
            {
                pageNum = 1;
            }
        }
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static esp_err_t write_lcd_data1(const hd44780_t *lcd, uint8_t data)
{
    return pcf8574_port_write(&pcf8574[1], data);
}

static esp_err_t write_lcd_data2(const hd44780_t *lcd, uint8_t data)
{
    return pcf8574_port_write(&pcf8574[2], data);
}

void LcdDisplayTask(void *pvParameters)
{
    esp_err_t err;
    if (config.displayType == DISP_CHAR)
    {
        memset(LCDbuffer, ' ', LCD_COLS * LCD_ROWS * NUM_OF_CONNECTORS);
        memset(updatedbuffer, ' ', LCD_COLS * LCD_ROWS);

        uint8_t char_data[] = {
            0x04, 0x0e, 0x0e, 0x0e, 0x1f, 0x00, 0x04, 0x00,
            0x1f, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x1f, 0x00};
        // i2cdev_init();
        xSemaphoreTake(mutexDisplay, portMAX_DELAY);

        lcd[1].write_cb = write_lcd_data1;
        lcd[1].font = HD44780_FONT_5X8;
        lcd[1].lines = 2;
        lcd[1].pins.rs = 0;
        lcd[1].pins.e = 2;
        lcd[1].pins.d4 = 4;
        lcd[1].pins.d5 = 5;
        lcd[1].pins.d6 = 6;
        lcd[1].pins.d7 = 7;
        lcd[1].pins.bl = 3;

        memset(&pcf8574[1], 0, sizeof(i2c_dev_t));
        pcf8574_init_desc(&pcf8574[1], lcdAddr[1], 0, DISPLAY_TX_PIN, DISPLAY_RX_PIN);

        hd44780_init(&lcd[1]);

        hd44780_switch_backlight(&lcd[1], true);

        hd44780_upload_character(&lcd[1], 0, char_data);
        hd44780_upload_character(&lcd[1], 1, char_data + 8);

        hd44780_gotoxy(&lcd[1], 0, 0);
        hd44780_puts(&lcd[1], "\x08 Hello world!");
        hd44780_gotoxy(&lcd[1], 0, 1);
        hd44780_puts(&lcd[1], "\x09 ");

        if ((config.MultiConnectorDisplay == false) && (config.NumofDisplays > 1))
        {
            lcd[2].write_cb = write_lcd_data1;
            lcd[2].font = HD44780_FONT_5X8;
            lcd[2].lines = 2;
            lcd[2].pins.rs = 0;
            lcd[2].pins.e = 2;
            lcd[2].pins.d4 = 4;
            lcd[2].pins.d5 = 5;
            lcd[2].pins.d6 = 6;
            lcd[2].pins.d7 = 7;
            lcd[2].pins.bl = 3;

            memset(&pcf8574[2], 0, sizeof(i2c_dev_t));
            pcf8574_init_desc(&pcf8574[2], lcdAddr[2], 0, DISPLAY_TX_PIN, DISPLAY_RX_PIN);

            hd44780_init(&lcd[2]);

            hd44780_switch_backlight(&lcd[2], true);
        }
        xSemaphoreGive(mutexDisplay);

        if (config.MultiConnectorDisplay == true)
            xTaskCreate(&displayPageTask, "taskDisplayPage", 4096, NULL, 2, NULL);
    }
    else if (config.displayType == DISP_DWIN)
    {
        xSemaphoreTake(mutexDisplay, portMAX_DELAY);
        uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
        uart_param_config(UART_NUM, &uart_config);
        uart_set_pin(UART_NUM, DISPLAY_TX_PIN, DISPLAY_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        uart_driver_install(UART_NUM, 1024, 1024, 0, NULL, 0);
        xSemaphoreGive(mutexDisplay);
    }
    SetChargerInitializingBit();

    uint32_t notify_value;
    while (1)
    {
        if (notify_value)
        {
            xTaskNotify(LcdDisplayTask_handle, 0, eNoAction);
        }
        if (xTaskNotifyWait(0, 0, &notify_value, portMAX_DELAY))
        {
            xSemaphoreTake(mutexDisplayTask, portMAX_DELAY);

            if (notify_value & CHARGER_INITIALIZING_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_INITIALIZING_BIT);
                setLcdChargerInitializing();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            else if (notify_value & CHARGER_COMMISSIONING_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_COMMISSIONING_BIT);
                setLcdChargerCommissioning();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            else if (notify_value & CHARGER_FIRMWARE_UPDATE_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_FIRMWARE_UPDATE_BIT);
                setLcdChargerFirmwareUpdating();
                vTaskDelay(10000 / portTICK_PERIOD_MS);
            }
            else if (notify_value & CHARGER_RFID_TAPPED_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_RFID_TAPPED_BIT);
                setLcdChargerRfidTapped();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            else if (notify_value & CHARGER_AUTH_FAILED_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_AUTH_FAILED_BIT);
                setLcdChargerAuthFailed();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            else if (notify_value & CHARGER_AUTH_SUCCESS_PLUG_EV_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_AUTH_SUCCESS_PLUG_EV_BIT);
                setLcdChargerAuthSuccessPlugEV();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            else if (notify_value & CHARGER_AUTH_SUCCESS_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_AUTH_SUCCESS_BIT);
                setLcdChargerAuthSuccess();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            else if (notify_value & CHARGER_RESERVED1_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_RESERVED1_BIT);
                setLcdChargerReserved(1);
            }
            else if (notify_value & CHARGER_RESERVED2_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_RESERVED2_BIT);
                setLcdChargerReserved(2);
            }
            else if (notify_value & CHARGER_RESERVED3_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_RESERVED3_BIT);
                setLcdChargerReserved(3);
            }
            else if (notify_value & CHARGER_AVAILABLE1_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_AVAILABLE1_BIT);
                setLcdChargerAvailable(1);
            }
            else if (notify_value & CHARGER_AVAILABLE2_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_AVAILABLE2_BIT);
                setLcdChargerAvailable(2);
            }
            else if (notify_value & CHARGER_AVAILABLE3_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_AVAILABLE3_BIT);
                setLcdChargerAvailable(3);
            }
            else if (notify_value & CHARGER_SUSPENDED1_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_SUSPENDED1_BIT);
                setLcdChargerSuspended(1);
            }
            else if (notify_value & CHARGER_SUSPENDED2_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_SUSPENDED2_BIT);
                setLcdChargerSuspended(2);
            }
            else if (notify_value & CHARGER_SUSPENDED3_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_SUSPENDED3_BIT);
                setLcdChargerSuspended(3);
            }
            else if (notify_value & CHARGER_UNAVAILABLE1_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_UNAVAILABLE1_BIT);
                setLcdChargerUnAvailable(1);
            }
            else if (notify_value & CHARGER_UNAVAILABLE2_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_UNAVAILABLE2_BIT);
                setLcdChargerUnAvailable(2);
            }
            else if (notify_value & CHARGER_UNAVAILABLE3_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_UNAVAILABLE3_BIT);
                setLcdChargerUnAvailable(3);
            }
            else if (notify_value & CHARGER_EV_PLUGGED_TAP_RFID1_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_EV_PLUGGED_TAP_RFID1_BIT);
                setLcdChargerEvPluggedTapRfid(1);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            else if (notify_value & CHARGER_EV_PLUGGED_TAP_RFID2_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_EV_PLUGGED_TAP_RFID2_BIT);
                setLcdChargerEvPluggedTapRfid(2);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            else if (notify_value & CHARGER_EV_PLUGGED_TAP_RFID3_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_EV_PLUGGED_TAP_RFID3_BIT);
                setLcdChargerEvPluggedTapRfid(3);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }

            else if (notify_value & CHARGER_FINISHING1_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_FINISHING1_BIT);
                setLcdChargerFinishing(1);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            else if (notify_value & CHARGER_FINISHING2_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_FINISHING2_BIT);
                setLcdChargerFinishing(2);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            else if (notify_value & CHARGER_FINISHING3_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_FINISHING3_BIT);
                setLcdChargerFinishing(3);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            else if (notify_value & CHARGER_ERROR_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_ERROR_BIT);
                setLcdChargerUnAvailable(1);
                setLcdChargerUnAvailable(2);
                setLcdChargerUnAvailable(3);
            }
            else if (notify_value & CHARGER_CHARGING1_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_CHARGING1_BIT);
                setLcdChargerCharging(1);
            }
            else if (notify_value & CHARGER_CHARGING2_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_CHARGING2_BIT);
                setLcdChargerCharging(2);
            }
            else if (notify_value & CHARGER_CHARGING3_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_CHARGING3_BIT);
                setLcdChargerCharging(3);
            }
            else if (notify_value & CHARGER_FAULT1_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_FAULT1_BIT);
                setLcdChargerFault(1);
            }
            else if (notify_value & CHARGER_FAULT2_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_FAULT2_BIT);
                setLcdChargerFault(2);
            }
            else if (notify_value & CHARGER_FAULT3_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_FAULT3_BIT);
                setLcdChargerFault(3);
            }
            else if (notify_value & CHARGER_NETWORKSWITCH_BIT)
            {
                ulTaskNotifyValueClear(LcdDisplayTask_handle, CHARGER_NETWORKSWITCH_BIT);
                setLcdNetWorkChange();
            }
            xSemaphoreGive(mutexDisplayTask);
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void LCD_DisplayReinit(void)
{
    if (config.displayType == DISP_CHAR)
    {
        hd44780_init(&lcd[1]);
        ets_delay_us(200);
        hd44780_switch_backlight(&lcd[1], true);
        ets_delay_us(200);
        hd44780_control(&lcd[1], true, false, false);
        ets_delay_us(200);
        hd44780_clear(&lcd[1]);
        ets_delay_us(200);
        UpdateLcdDisplay(1);
        if ((config.MultiConnectorDisplay == false) && (config.NumofDisplays > 1))
        {
            hd44780_init(&lcd[2]);
            ets_delay_us(200);
            hd44780_switch_backlight(&lcd[2], true);
            ets_delay_us(200);
            hd44780_control(&lcd[2], true, false, false);
            ets_delay_us(200);
            hd44780_clear(&lcd[2]);
            ets_delay_us(200);
            UpdateLcdDisplay(2);
        }
    }
}

void Display_Init(void)
{
    if (config.vcharge_lite_1_4)
    {
        i2cdev_init();
    }
    evtGrpDisplay = xEventGroupCreate();
    mutexDisplay = xSemaphoreCreateMutex();
    mutexDisplayTask = xSemaphoreCreateMutex();
    if ((config.displayType == DISP_CHAR) || (config.displayType == DISP_DWIN))
    {
        xTaskCreate(&LcdDisplayTask, "taskPRINT", 4096, NULL, 3, &LcdDisplayTask_handle);
    }
}
