#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "ControlPilotState.h"
#include "custom_nvs.h"
#include "ocpp_task.h"
#include "slave.h"

#define TAG "SLAVE"

#define SLAVE_BUFF_SIZE 1024
const uart_port_t SLAVE_UART = UART_NUM_2;
SemaphoreHandle_t mutexSlave;

ledColors_t ledColors;
extern Config_t config;
extern bool relayState[NUM_OF_CONNECTORS];
extern uint8_t cpState[NUM_OF_CONNECTORS];
extern bool slaveFirmwareStatusReceived;
UART_t slave_uart;
uint8_t slavePushButton[4] = {0, 0, 0, 0};
uint8_t PushButton[4] = {0, 0, 0, 0};

bool SlaveSendResponse(uint8_t id)
{
    bool sent = false;
    if (config.vcharge_lite_1_4)
    {
        uint16_t messageStart = 0xAA55;
        uint16_t messageSize = 12;
        uint16_t messageDataLength = messageSize - 4;
        uint16_t messageId = id;
        uint16_t spare = 0;
        uint8_t data[messageSize];
        memset(data, 0, messageSize);

        memcpy(data, &messageStart, 2);
        memcpy(&data[2], &messageDataLength, 2);
        memcpy(&data[4], &messageId, 2);
        memcpy(&data[6], &id, 1);
        memcpy(&data[7], &spare, 1);
        memcpy(&data[8], &spare, 2);

        uint16_t checksum = 0;
        for (uint16_t i = 0; i < messageSize - 2; i++)
        {
            checksum += data[i];
        }
        memcpy(&data[messageSize - 2], &checksum, 2);
        xSemaphoreTake(mutexSlave, portMAX_DELAY);
        uart_write_bytes(SLAVE_UART, (const uint8_t *)data, messageSize);
        uart_wait_tx_done(SLAVE_UART, 1000 / portTICK_PERIOD_MS);

        vTaskDelay(100 / portTICK_PERIOD_MS);
        xSemaphoreGive(mutexSlave);

        sent = true;
    }

    return sent;
}

void updatePushButtonStateReceivedFromSlave(uint8_t pb, uint8_t state)
{
    if (pb == PUSH_BUTTON1)
    {
        if (state == 0)
        {
            slavePushButton[0] = false;
            ESP_LOGI(TAG, "PUSH BUTTON 1 Released");
        }
        else if (state == 1)
        {
            slavePushButton[0] = true;
            ESP_LOGI(TAG, "PUSH BUTTON 1 Pressed");
        }
    }
    else if (pb == PUSH_BUTTON2)
    {
        if (state == 0)
        {
            slavePushButton[1] = false;
            ESP_LOGI(TAG, "PUSH BUTTON 2 Released");
        }
        else if (state == 1)
        {
            slavePushButton[1] = true;
            ESP_LOGI(TAG, "PUSH BUTTON 2 Pressed");
        }
    }
    else if (pb == PUSH_BUTTON3)
    {
        if (state == 0)
        {
            slavePushButton[2] = false;
            ESP_LOGI(TAG, "PUSH BUTTON 3 Released");
        }
        else if (state == 1)
        {
            slavePushButton[2] = true;
            ESP_LOGI(TAG, "PUSH BUTTON 3 Pressed");
        }
    }
    else if (pb == PUSH_BUTTON4)
    {
        if (state == 0)
        {
            slavePushButton[3] = false;
            ESP_LOGE(TAG, "Emergency Released");
        }
        else if (state == 1)
        {
            slavePushButton[3] = true;
            ESP_LOGE(TAG, "Emergency Pressed");
        }
    }

    PushButton[1] = slavePushButton[0];
    PushButton[2] = slavePushButton[1];
    PushButton[3] = slavePushButton[2];
}

void slaveTask(void *pvParameters)
{
    uart_event_t event;
    uint8_t slave_data[SLAVE_BUFF_SIZE];
    uint8_t receivedByte = 0;
    memset(&slave_uart, 0, sizeof(slave_uart));

    while (1)
    {
        int len = uart_read_bytes(SLAVE_UART, &receivedByte, 1, portMAX_DELAY);
        if (len > 0)
        {
            if (slave_uart.messageStart == false)
            {
                if (receivedByte == 0x55)
                    slave_data[0] = 0x55;
                else if (receivedByte == 0xAA && slave_data[0] == 0x55)
                {
                    slave_uart.messageStart = true;
                    slave_data[1] = 0xAA;
                    slave_uart.messagechecksumValue = 255; /*Integer value of 0xAA55 is 43605*/
                }
                else
                {
                    slave_data[0] = 0;
                    slave_uart.messagechecksumValue = 0;
                }
            }
            else if (slave_uart.messageLengthFirstByteStatus == false)
            {
                slave_data[2] = receivedByte;
                slave_uart.messageLengthFirstByte = receivedByte;
                slave_uart.messageLengthFirstByteStatus = true;
                slave_uart.messagechecksumValue += receivedByte;
            }
            else if (slave_uart.messageLengthSecondByteStatus == false)
            {
                slave_data[3] = receivedByte;
                slave_uart.messageLengthSecondByte = receivedByte;
                slave_uart.messageLength = (slave_uart.messageLengthSecondByte * 256 + slave_uart.messageLengthFirstByte);
                slave_uart.messageReceiveCounter = 0;
                slave_uart.messageLengthSecondByteStatus = true;
                slave_uart.messagechecksumValue += receivedByte;
            }
            else
            {
                if (slave_uart.messageReceiveCounter < (slave_uart.messageLength))
                {
                    slave_data[4 + slave_uart.messageReceiveCounter] = receivedByte;
                    if (slave_uart.messageReceiveCounter < (slave_uart.messageLength - 2))
                    {
                        slave_uart.messagechecksumValue += receivedByte;
                    }
                    else
                    {
                        if (slave_uart.messageReceiveCounter < (slave_uart.messageLength - 1))
                            slave_uart.checksumValue = receivedByte;
                        else
                        {
                            slave_uart.checksumValue += 256 * (receivedByte);
                            slave_uart.messageStart = false;
                            slave_uart.messageLengthFirstByteStatus = false;
                            slave_uart.messageLengthSecondByteStatus = false;
                            if (slave_uart.messagechecksumValue == slave_uart.checksumValue)
                            {
                                slave_uart.CommunicationFlag = 0;
                                slave_uart.messageTimeOutCount = 0;
                                slave_uart.messageCompletedStatus = true;
                                slave_uart.messagechecksumValue = 0;
                                slave_uart.checksumValue = 0;
                                slave_uart.errorMessage = false;
                                if (slave_data[4] == LED_ID)
                                {
#if SERIAL_FEEDBACK
                                    slave_uart.LED_ID_RECEIVED = true;
#endif
                                }
                                else if (slave_data[4] == RELAY_ID)
                                {
#if SERIAL_FEEDBACK
                                    slave_uart.RELAY_ID_RECEIVED = true;
#endif
                                }
                                else if (slave_data[4] == PUSHBUTTON_ID)
                                {
                                    if (slave_data[5] == 1)
                                    {
                                        updatePushButtonStateReceivedFromSlave(slave_data[6], slave_data[7]);
                                        SlaveSendResponse(slave_data[4]);
                                    }

#if SERIAL_FEEDBACK
                                    slave_uart.PUSHBUTTON_ID_RECEIVED = true;
#endif
                                }
                                else if (slave_data[4] == CONTROL_PILOT_SET_ID)
                                {
#if SERIAL_FEEDBACK
                                    slave_uart.CONTROL_PILOT_SET_ID_RECEIVED = true;
#endif
                                }
                                else if (slave_data[4] == CONTROL_PILOT_READ_ID)
                                {
#if SERIAL_FEEDBACK
                                    slave_uart.CONTROL_PILOT_READ_ID_RECEIVED = true;
#endif
                                }
                                else if (slave_data[4] == CP_CONFIG_ID)
                                {
#if SERIAL_FEEDBACK
                                    slave_uart.CP_CONFIG_ID_RECEIVED = true;
#endif
                                }
                                else if (slave_data[4] == CP_STATE_ID)
                                {
                                    if (slave_data[6] == CP1)
                                    {
                                        cpState[1] = slave_data[7];
                                    }
                                    else if (slave_data[6] == CP2)
                                    {
                                        cpState[2] = slave_data[7];
                                    }
                                    else
                                    {
                                    }
                                    if (slave_data[5] == 1)
                                    {
                                        SlaveSendResponse(slave_data[4]);
                                    }
#if SERIAL_FEEDBACK
                                    slave_uart.CP_STATE_ID_RECEIVED = true;
#endif
                                }
                                else if (slave_data[4] == FIRMWARE_VERSION_ID)
                                {
                                    setNULL(config.slavefirmwareVersion);
                                    memcpy(config.slavefirmwareVersion, &slave_data[6], slave_uart.messageLength - 4);
#if SERIAL_FEEDBACK
                                    slave_uart.FIRMWARE_VERSION_ID_RECEIVED = true;
#endif
                                }
                                else if (slave_data[4] == FIRMWARE_UPDATE_START_ID)
                                {
#if SERIAL_FEEDBACK
                                    slave_uart.FIRMWARE_UPDATE_START_ID_RECEIVED = true;
#endif
                                }
                                else if (slave_data[4] == FIRMWARE_DATA_ID)
                                {
#if SERIAL_FEEDBACK
                                    slave_uart.FIRMWARE_DATA_ID_RECEIVED = true;
#endif
                                }
                                else if (slave_data[4] == FIRMWARE_UPDATE_END_ID)
                                {
#if SERIAL_FEEDBACK
                                    slave_uart.FIRMWARE_UPDATE_END_ID_RECEIVED = true;
#endif
                                }
                                else if (slave_data[4] == FIRMWARE_UPDATE_COMPLETED_ID)
                                {
#if SERIAL_FEEDBACK
                                    slave_uart.FIRMWARE_UPDATE_COMPLETED_ID_RECEIVED = true;
#endif
                                }
                                else if (slave_data[4] == RESTART_ID)
                                {
#if SERIAL_FEEDBACK
                                    slave_uart.RESTART_ID_RECEIVED = true;
#endif
                                }
                                else if (slave_data[4] == FIRMWARE_STATUS_ID)
                                {
#if SERIAL_FEEDBACK
                                    slave_uart.FIRMWARE_STATUS_ID_RECEIVED = true;
#endif
                                }
                                else if (slave_data[4] == FIRMWARE_UPDATE_SUCCEESS_ID)
                                {
                                    slaveFirmwareStatusReceived = true;
#if SERIAL_FEEDBACK
                                    slave_uart.FIRMWARE_UPDATE_SUCCEESS_ID_RECEIVED = true;
#endif
                                }
                                else if (slave_data[4] == FIRMWARE_UPDATE_FAIL_ID)
                                {
                                    slaveFirmwareStatusReceived = true;
#if SERIAL_FEEDBACK
                                    slave_uart.FIRMWARE_UPDATE_FAIL_ID_RECEIVED = true;
#endif
                                }

                                else
                                    slave_uart.errorMessage = true;
                            }
                            else
                            {
                                slave_uart.errorMessage = true;
                                slave_uart.messagechecksumValue = 0;
                                slave_uart.checksumValue = 0;
                            }
                        }
                    }
                }
                slave_uart.messageReceiveCounter++;
            }
        }
    }
}

bool SlaveSendControlPilotConfig(SlaveCpConfig_t SlaveCpConfig)
{
    bool sent = false;
    if (config.vcharge_lite_1_4)
    {
        xSemaphoreTake(mutexSlave, portMAX_DELAY);
        uint16_t messageStart = 0xAA55;
        uint16_t messageSize = sizeof(SlaveCpConfig_t) + 8;
        uint16_t messageDataLength = messageSize - 4;
        uint16_t messageId = CP_CONFIG_ID + SLAVE_REQUEST;
        uint16_t spare = 0;
        uint8_t data[messageSize];
        memset(data, 0, messageSize);

        memcpy(data, &messageStart, 2);
        memcpy(&data[2], &messageDataLength, 2);
        memcpy(&data[4], &messageId, 2);
        memcpy(&data[6], &SlaveCpConfig, sizeof(SlaveCpConfig_t));

        uint16_t checksum = 0;
        for (uint16_t i = 0; i < messageSize - 2; i++)
        {
            checksum += data[i];
        }
        memcpy(&data[messageSize - 2], &checksum, 2);
        uart_write_bytes(SLAVE_UART, (const uint8_t *)data, messageSize);
        uart_wait_tx_done(SLAVE_UART, 1000 / portTICK_PERIOD_MS);

        slave_uart.CP_CONFIG_ID_RECEIVED = false;
        vTaskDelay(100 / portTICK_PERIOD_MS);
        xSemaphoreGive(mutexSlave);

#if SERIAL_FEEDBACK
        uint16_t count = 0;
        while ((slave_uart.CP_CONFIG_ID_RECEIVED == false) && (count < SERIAL_FEEDBACK_TIME))
        {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            count++;
        }
        if (slave_uart.CP_CONFIG_ID_RECEIVED == true)
        {
            sent = true;
        }
#else
        sent = true;
#endif
    }

    return sent;
}

bool sendFirmwareData(uint8_t *dataToSend, uint32_t length, uint32_t packetNo)
{
    bool sent = false;
    if (config.vcharge_lite_1_4)
    {
        xSemaphoreTake(mutexSlave, portMAX_DELAY);
        uint16_t messageStart = 0xAA55;
        uint16_t messageSize = length + 12;
        uint16_t messageDataLength = messageSize - 4;
        uint16_t messageId = FIRMWARE_DATA_ID + SLAVE_REQUEST;
        uint16_t spare = 0;
        uint8_t data[messageSize];
        memset(data, 0, messageSize);

        memcpy(data, &messageStart, 2);
        memcpy(&data[2], &messageDataLength, 2);
        memcpy(&data[4], &messageId, 2);
        memcpy(&data[6], &packetNo, 4);
        memcpy(&data[10], dataToSend, length);

        uint16_t checksum = 0;
        for (uint16_t i = 0; i < messageSize - 2; i++)
        {
            checksum += data[i];
        }
        memcpy(&data[messageSize - 2], &checksum, 2);
        uart_write_bytes(SLAVE_UART, (const uint8_t *)data, messageSize);
        uart_wait_tx_done(SLAVE_UART, 1000 / portTICK_PERIOD_MS);

        slave_uart.FIRMWARE_DATA_ID_RECEIVED = false;
        xSemaphoreGive(mutexSlave);

        ESP_LOGI(TAG, "Sent Packet No: %ld", packetNo);
#if SERIAL_FEEDBACK
        uint16_t count = 0;
        while ((slave_uart.FIRMWARE_DATA_ID_RECEIVED == false) && (count < (SERIAL_FEEDBACK_TIME * 50)))
        {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            count++;
        }
        // xSemaphoreGive(mutexSlave);
        if (slave_uart.FIRMWARE_DATA_ID_RECEIVED == true)
        {
            sent = true;
        }
#else
        sent = true;
#endif
    }

    return sent;
}

bool SlaveSendID(uint8_t id)
{
    bool sent = false;
    if (config.vcharge_lite_1_4)
    {
        uint16_t messageStart = 0xAA55;
        uint16_t messageSize = 12;
        uint16_t messageDataLength = messageSize - 4;
        uint16_t messageId = id + SLAVE_REQUEST;
        uint16_t spare = 0;
        uint8_t data[messageSize];
        memset(data, 0, messageSize);

        memcpy(data, &messageStart, 2);
        memcpy(&data[2], &messageDataLength, 2);
        memcpy(&data[4], &messageId, 2);
        memcpy(&data[6], &id, 1);
        memcpy(&data[7], &spare, 1);
        memcpy(&data[8], &spare, 2);

        uint16_t checksum = 0;
        for (uint16_t i = 0; i < messageSize - 2; i++)
        {
            checksum += data[i];
        }
        memcpy(&data[messageSize - 2], &checksum, 2);
        xSemaphoreTake(mutexSlave, portMAX_DELAY);
        uart_write_bytes(SLAVE_UART, (const uint8_t *)data, messageSize);
        uart_wait_tx_done(SLAVE_UART, 1000 / portTICK_PERIOD_MS);

        if (id == FIRMWARE_VERSION_ID)
        {
            slave_uart.FIRMWARE_VERSION_ID_RECEIVED = false;
        }
        else if (id == FIRMWARE_UPDATE_START_ID)
        {
            slave_uart.FIRMWARE_UPDATE_START_ID_RECEIVED = false;
        }
        else if (id == FIRMWARE_UPDATE_END_ID)
        {
            slave_uart.FIRMWARE_UPDATE_END_ID_RECEIVED = false;
        }
        else if (id == FIRMWARE_UPDATE_COMPLETED_ID)
        {
            slave_uart.FIRMWARE_UPDATE_COMPLETED_ID_RECEIVED = false;
        }
        else if (id == RESTART_ID)
        {
            slave_uart.RESTART_ID_RECEIVED = false;
        }
        else if (id == FIRMWARE_STATUS_ID)
        {
            slave_uart.FIRMWARE_STATUS_ID_RECEIVED = false;
        }
        else if (id == FIRMWARE_UPDATE_SUCCEESS_ID)
        {
            slave_uart.FIRMWARE_UPDATE_SUCCEESS_ID_RECEIVED = false;
        }
        else if (id == FIRMWARE_UPDATE_FAIL_ID)
        {
            slave_uart.FIRMWARE_UPDATE_FAIL_ID_RECEIVED = false;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
        xSemaphoreGive(mutexSlave);

#if SERIAL_FEEDBACK
        uint16_t count = 0;
        if (id == FIRMWARE_VERSION_ID)
        {
            // slave_uart.FIRMWARE_VERSION_ID_RECEIVED = false;
            while ((slave_uart.FIRMWARE_VERSION_ID_RECEIVED == false) && (count < SERIAL_FEEDBACK_TIME))
            {
                vTaskDelay(50 / portTICK_PERIOD_MS);
                count++;
            }
            if (slave_uart.FIRMWARE_VERSION_ID_RECEIVED == true)
            {
                sent = true;
            }
        }
        else if (id == FIRMWARE_UPDATE_START_ID)
        {
            // slave_uart.FIRMWARE_UPDATE_START_ID_RECEIVED = false;
            while ((slave_uart.FIRMWARE_UPDATE_START_ID_RECEIVED == false) && (count < SERIAL_FEEDBACK_TIME))
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
                count++;
            }
            if (slave_uart.FIRMWARE_UPDATE_START_ID_RECEIVED == true)
            {
                sent = true;
            }
        }
        else if (id == FIRMWARE_UPDATE_END_ID)
        {
            // slave_uart.FIRMWARE_UPDATE_END_ID_RECEIVED = false;
            while ((slave_uart.FIRMWARE_UPDATE_END_ID_RECEIVED == false) && (count < SERIAL_FEEDBACK_TIME))
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
                count++;
            }
            if (slave_uart.FIRMWARE_UPDATE_END_ID_RECEIVED == true)
            {
                sent = true;
            }
        }
        else if (id == FIRMWARE_UPDATE_COMPLETED_ID)
        {
            // slave_uart.FIRMWARE_UPDATE_COMPLETED_ID_RECEIVED = false;
            while ((slave_uart.FIRMWARE_UPDATE_COMPLETED_ID_RECEIVED == false) && (count < SERIAL_FEEDBACK_TIME))
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
                count++;
            }
            if (slave_uart.FIRMWARE_UPDATE_COMPLETED_ID_RECEIVED == true)
            {
                sent = true;
            }
        }
        else if (id == RESTART_ID)
        {
            // slave_uart.RESTART_ID_RECEIVED = false;
            while ((slave_uart.RESTART_ID_RECEIVED == false) && (count < SERIAL_FEEDBACK_TIME))
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
                count++;
            }
            if (slave_uart.RESTART_ID_RECEIVED == true)
            {
                sent = true;
            }
        }
        else if (id == FIRMWARE_STATUS_ID)
        {
            // slave_uart.FIRMWARE_STATUS_ID_RECEIVED = false;
            while ((slave_uart.FIRMWARE_STATUS_ID_RECEIVED == false) && (count < SERIAL_FEEDBACK_TIME))
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
                count++;
            }
            if (slave_uart.FIRMWARE_STATUS_ID_RECEIVED == true)
            {
                sent = true;
            }
        }
        else if (id == FIRMWARE_UPDATE_SUCCEESS_ID)
        {
            // slave_uart.FIRMWARE_UPDATE_SUCCEESS_ID_RECEIVED = false;
            while ((slave_uart.FIRMWARE_UPDATE_SUCCEESS_ID_RECEIVED == false) && (count < SERIAL_FEEDBACK_TIME))
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
                count++;
            }
            if (slave_uart.FIRMWARE_UPDATE_SUCCEESS_ID_RECEIVED == true)
            {
                sent = true;
            }
        }
        else if (id == FIRMWARE_UPDATE_FAIL_ID)
        {
            // slave_uart.FIRMWARE_UPDATE_FAIL_ID_RECEIVED = false;
            while ((slave_uart.FIRMWARE_UPDATE_FAIL_ID_RECEIVED == false) && (count < SERIAL_FEEDBACK_TIME))
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
                count++;
            }
            if (slave_uart.FIRMWARE_UPDATE_FAIL_ID_RECEIVED == true)
            {
                sent = true;
            }
        }
#else
        sent = true;
#endif
    }

    return sent;
}

bool SlaveGetState(uint8_t cp)
{
    bool sent = false;
    if (config.vcharge_lite_1_4)
    {
        xSemaphoreTake(mutexSlave, portMAX_DELAY);
        uint16_t messageStart = 0xAA55;
        uint16_t messageSize = 12;
        uint16_t messageDataLength = messageSize - 4;
        uint16_t messageId = CP_STATE_ID + SLAVE_REQUEST;
        uint16_t spare = 0;
        uint8_t data[messageSize];
        memset(data, 0, messageSize);

        memcpy(data, &messageStart, 2);
        memcpy(&data[2], &messageDataLength, 2);
        memcpy(&data[4], &messageId, 2);
        memcpy(&data[6], &cp, 1);
        memcpy(&data[7], &spare, 1);
        memcpy(&data[8], &spare, 2);

        uint16_t checksum = 0;
        for (uint16_t i = 0; i < messageSize - 2; i++)
        {
            checksum += data[i];
        }
        memcpy(&data[messageSize - 2], &checksum, 2);
        uart_write_bytes(SLAVE_UART, (const uint8_t *)data, messageSize);
        uart_wait_tx_done(SLAVE_UART, 1000 / portTICK_PERIOD_MS);

        slave_uart.CP_STATE_ID_RECEIVED = false;
        vTaskDelay(100 / portTICK_PERIOD_MS);
        xSemaphoreGive(mutexSlave);
#if SERIAL_FEEDBACK
        uint16_t count = 0;
        while ((slave_uart.CP_STATE_ID_RECEIVED == false) && (count < SERIAL_FEEDBACK_TIME))
        {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            count++;
        }
        if (slave_uart.CP_STATE_ID_RECEIVED == true)
        {
            sent = true;
        }
#else
        sent = true;
#endif
    }

    return sent;
}

bool SlaveGetPushButtonState(uint8_t pb)
{
    bool sent = false;
    if (config.vcharge_lite_1_4)
    {
        xSemaphoreTake(mutexSlave, portMAX_DELAY);
        uint16_t messageStart = 0xAA55;
        uint16_t messageSize = 12;
        uint16_t messageDataLength = messageSize - 4;
        uint16_t messageId = PUSHBUTTON_ID + SLAVE_REQUEST;
        uint16_t spare = 0;
        uint8_t data[messageSize];
        memset(data, 0, messageSize);

        memcpy(data, &messageStart, 2);
        memcpy(&data[2], &messageDataLength, 2);
        memcpy(&data[4], &messageId, 2);
        memcpy(&data[6], &pb, 1);
        memcpy(&data[7], &spare, 1);
        memcpy(&data[8], &spare, 2);

        uint16_t checksum = 0;
        for (uint16_t i = 0; i < messageSize - 2; i++)
        {
            checksum += data[i];
        }
        memcpy(&data[messageSize - 2], &checksum, 2);
        uart_write_bytes(SLAVE_UART, (const uint8_t *)data, messageSize);
        uart_wait_tx_done(SLAVE_UART, 1000 / portTICK_PERIOD_MS);

        slave_uart.PUSHBUTTON_ID_RECEIVED = false;
        vTaskDelay(100 / portTICK_PERIOD_MS);
        xSemaphoreGive(mutexSlave);

#if SERIAL_FEEDBACK
        uint16_t count = 0;
        while ((slave_uart.PUSHBUTTON_ID_RECEIVED == false) && (count < SERIAL_FEEDBACK_TIME))
        {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            count++;
        }
        if (slave_uart.PUSHBUTTON_ID_RECEIVED == true)
        {
            sent = true;
        }
#else
        sent = true;
#endif
    }

    return sent;
}

bool SlaveSetLedColor(uint8_t led, uint8_t color)
{
    bool sent = false;
    if (config.vcharge_lite_1_4)
    {
        xSemaphoreTake(mutexSlave, portMAX_DELAY);
        uint16_t messageStart = 0xAA55;
        uint16_t messageSize = 12;
        uint16_t messageDataLength = messageSize - 4;
        uint16_t messageId = LED_ID + SLAVE_REQUEST;
        uint16_t spare = 0;
        uint8_t data[messageSize];
        memset(data, 0, messageSize);

        memcpy(data, &messageStart, 2);
        memcpy(&data[2], &messageDataLength, 2);
        memcpy(&data[4], &messageId, 2);
        memcpy(&data[6], &led, 1);
        memcpy(&data[7], &color, 1);
        memcpy(&data[8], &spare, 2);

        uint16_t checksum = 0;
        for (uint16_t i = 0; i < messageSize - 2; i++)
        {
            checksum += data[i];
        }
        memcpy(&data[messageSize - 2], &checksum, 2);
        uart_write_bytes(SLAVE_UART, (const uint8_t *)data, messageSize);
        uart_wait_tx_done(SLAVE_UART, 1000 / portTICK_PERIOD_MS);

        slave_uart.LED_ID_RECEIVED = false;
        vTaskDelay(100 / portTICK_PERIOD_MS);
        xSemaphoreGive(mutexSlave);

#if SERIAL_FEEDBACK
        uint16_t count = 0;
        while ((slave_uart.LED_ID_RECEIVED == false) && (count < SERIAL_FEEDBACK_TIME))
        {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            count++;
        }
        if (slave_uart.LED_ID_RECEIVED == true)
        {
            sent = true;
        }
#else
        sent = true;
#endif
    }

    return sent;
}

bool SlaveSetControlPilotDuty(uint8_t cp, uint8_t duty)
{
    bool sent = false;
    if (config.vcharge_lite_1_4)
    {
        xSemaphoreTake(mutexSlave, portMAX_DELAY);
        uint16_t messageStart = 0xAA55;
        uint16_t messageSize = 12;
        uint16_t messageDataLength = messageSize - 4;
        uint16_t messageId = CONTROL_PILOT_SET_ID + SLAVE_REQUEST;
        uint16_t spare = 0;
        uint8_t data[messageSize];
        memset(data, 0, messageSize);

        memcpy(data, &messageStart, 2);
        memcpy(&data[2], &messageDataLength, 2);
        memcpy(&data[4], &messageId, 2);
        memcpy(&data[6], &cp, 1);
        memcpy(&data[7], &duty, 1);
        memcpy(&data[8], &spare, 2);

        uint16_t checksum = 0;
        for (uint16_t i = 0; i < messageSize - 2; i++)
        {
            checksum += data[i];
        }
        memcpy(&data[messageSize - 2], &checksum, 2);
        uart_write_bytes(SLAVE_UART, (const uint8_t *)data, messageSize);
        uart_wait_tx_done(SLAVE_UART, 1000 / portTICK_PERIOD_MS);

        slave_uart.CONTROL_PILOT_SET_ID_RECEIVED = false;
        vTaskDelay(100 / portTICK_PERIOD_MS);
        xSemaphoreGive(mutexSlave);

#if SERIAL_FEEDBACK
        uint16_t count = 0;
        while ((slave_uart.CONTROL_PILOT_SET_ID_RECEIVED == false) && (count < SERIAL_FEEDBACK_TIME))
        {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            count++;
        }
        if (slave_uart.CONTROL_PILOT_SET_ID_RECEIVED == true)
        {
            sent = true;
        }
#else
        sent = true;
#endif
    }

    return sent;
}

bool SlaveUpdateRelay(uint8_t relay, uint8_t state)
{
    bool sent = false;
    if (config.vcharge_lite_1_4)
    {
        if (relayState[relay] != state)
        {
            xSemaphoreTake(mutexSlave, portMAX_DELAY);
            uint16_t messageStart = 0xAA55;
            uint16_t messageSize = 12;
            uint16_t messageDataLength = messageSize - 4;
            uint16_t messageId = RELAY_ID + SLAVE_REQUEST;
            uint16_t spare = 0;
            uint8_t data[messageSize];
            memset(data, 0, messageSize);

            memcpy(data, &messageStart, 2);
            memcpy(&data[2], &messageDataLength, 2);
            memcpy(&data[4], &messageId, 2);
            memcpy(&data[6], &relay, 1);
            memcpy(&data[7], &state, 1);
            memcpy(&data[8], &spare, 2);

            uint16_t checksum = 0;
            for (uint16_t i = 0; i < messageSize - 2; i++)
            {
                checksum += data[i];
            }
            memcpy(&data[messageSize - 2], &checksum, 2);
            uart_write_bytes(SLAVE_UART, (const uint8_t *)data, messageSize);
            uart_wait_tx_done(SLAVE_UART, 1000 / portTICK_PERIOD_MS);

            slave_uart.RELAY_ID_RECEIVED = false;
            vTaskDelay(100 / portTICK_PERIOD_MS);
            xSemaphoreGive(mutexSlave);

#if SERIAL_FEEDBACK
            uint16_t count = 0;
            while ((slave_uart.RELAY_ID_RECEIVED == false) && (count < SERIAL_FEEDBACK_TIME))
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
                count++;
            }
            if (slave_uart.RELAY_ID_RECEIVED == true)
            {
                relayState[relay] = state;
                sent = true;
            }
#else
            relayState[relay] = state;
            sent = true;
#endif
        }
        else
        {
            sent = true;
        }
    }

    return sent;
}

void slave_serial_init(void)
{
    mutexSlave = xSemaphoreCreateMutex();

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(SLAVE_UART, &uart_config);
    uart_set_pin(SLAVE_UART, SLAVE_TX_PIN, SLAVE_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(SLAVE_UART, 2048, 8192, 0, NULL, 0);
    xTaskCreate(&slaveTask, "slaveTask", 4096, NULL, 2, NULL);
    SlaveSendID(RESTART_ID);
    vTaskDelay(pdMS_TO_TICKS(1000));
}