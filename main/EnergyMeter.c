#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "rom/ets_sys.h"
#include <esp_log.h>
#include "EnergyMeter.h"
#include "ocpp_task.h"
#include "ControlPilotAdc.h"
#include "ProjectSettings.h"

SemaphoreHandle_t mutexSpiBus;
extern uint8_t relay_state;
extern uint32_t relayOnTime;
extern uint32_t relayOffTime;
extern uint8_t relayWeldDetectionButton;
extern bool earthDisconnectCircuit;
extern uint8_t earthDisconnectButton;
extern Config_t config;
extern ConnectorPrivateData_t ConnectorData[NUM_OF_CONNECTORS];
static const char *TAG = "Energy Meter";
spi_device_handle_t spi_EM;

double voltageMultificationFactor = 0.989649057;
double voltageOffset = 0.315538775;

// Function to initialize SPI bus
void EnergyMeter_init(void)
{
    mutexSpiBus = xSemaphoreCreateMutex();
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << EM_ISO_EN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(EM_ISO_EN_PIN, 1);
    spi_bus_config_t bus_config = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    spi_device_interface_config_t dev_config = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0,
        .duty_cycle_pos = 128,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = 1000000, // Adjust as needed
        .input_delay_ns = 0,
        .spics_io_num = PIN_NUM_EM_CS,
        .flags = 0,
        .queue_size = 1,
        .pre_cb = NULL,
        .post_cb = NULL,
    };

    esp_err_t ret = spi_bus_initialize(SPI3_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize spi bus, error = %x\n", ret);
    }
    ret = spi_bus_add_device(SPI3_HOST, &dev_config, &spi_EM);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add SPI device, error = %x\n", ret);
    }

    if (xSemaphoreTake(mutexSpiBus, portMAX_DELAY))
    {
        ATM90E36_Initialise();
        xSemaphoreGive(mutexSpiBus);
    }
}

void readEnrgyMeterValues(void)
{
    if (xSemaphoreTake(mutexSpiBus, portMAX_DELAY))
    {
        uint16_t temp = CommEnergyIC(spi_EM, ATM90E36A_READ, Temp, 0);     // Temp Register
        uint16_t voltage = CommEnergyIC(spi_EM, ATM90E36A_READ, UrmsA, 0); // Temp Register
        uint16_t current = CommEnergyIC(spi_EM, ATM90E36A_READ, IrmsA, 0); // Temp Register
        uint16_t power = CommEnergyIC(spi_EM, ATM90E36A_READ, PmeanAF, 0); // Temp Register
        xSemaphoreGive(mutexSpiBus);
        ESP_LOGI(TAG, "temp: %u \n", temp);
        ESP_LOGI(TAG, "Voltage: %f V\n", (voltage * 0.01 * voltageMultificationFactor + voltageOffset));
        ESP_LOGI(TAG, "current: %f A\n", current * 0.001 * config.CurrentGain1);
        ESP_LOGI(TAG, "Power Calculated: %f A\n", voltage * 0.01 * current * 0.001 * config.CurrentGain1);
        ESP_LOGI(TAG, "Power Fundamental: %f A\n", power * 1.0);
    }
}

MeterFaults_t readMeterFaults(uint8_t connId)
{
    MeterValues_t MeterValues;
    MeterFaults_t MeterFaults;
    if (config.Ac11s || config.Ac22s || config.Ac44s)
    {
        if (xSemaphoreTake(mutexSpiBus, portMAX_DELAY))
        {
            MeterValues.temp = CommEnergyIC(spi_EM, ATM90E36A_READ, Temp, 0);
            MeterValues.voltage[PHASE_A] = CommEnergyIC(spi_EM, ATM90E36A_READ, UrmsA, 0) * 0.01;
            MeterValues.current[PHASE_A] = CommEnergyIC(spi_EM, ATM90E36A_READ, IrmsA, 0) * 0.001;
            MeterValues.voltage[PHASE_B] = CommEnergyIC(spi_EM, ATM90E36A_READ, UrmsB, 0) * 0.01;
            MeterValues.current[PHASE_B] = CommEnergyIC(spi_EM, ATM90E36A_READ, IrmsB, 0) * 0.001;
            MeterValues.voltage[PHASE_C] = CommEnergyIC(spi_EM, ATM90E36A_READ, UrmsC, 0) * 0.01;
            MeterValues.current[PHASE_C] = CommEnergyIC(spi_EM, ATM90E36A_READ, IrmsC, 0) * 0.001;
            xSemaphoreGive(mutexSpiBus);
            // PHASE A SCALING
            MeterValues.voltage[PHASE_A] = MeterValues.voltage[PHASE_A] * voltageMultificationFactor + voltageOffset;
            if (MeterValues.current[PHASE_A] > 8.5)
            {
                MeterValues.current[PHASE_A] = MeterValues.current[PHASE_A] * config.CurrentGain2;
            }
            else
            {
                MeterValues.current[PHASE_A] = MeterValues.current[PHASE_A] * config.CurrentGain1;
            }
            MeterValues.current[PHASE_A] = MeterValues.current[PHASE_A] * config.CurrentGain - config.CurrentOffset;

            // PHASE B SCALING
            MeterValues.voltage[PHASE_B] = MeterValues.voltage[PHASE_B] * voltageMultificationFactor + voltageOffset;
            if (MeterValues.current[PHASE_B] > 8.5)
            {
                MeterValues.current[PHASE_B] = MeterValues.current[PHASE_B] * config.CurrentGain2;
            }
            else
            {
                MeterValues.current[PHASE_B] = MeterValues.current[PHASE_B] * config.CurrentGain1;
            }
            MeterValues.current[PHASE_B] = MeterValues.current[PHASE_B] * config.CurrentGain - config.CurrentOffset;

            // PHASE C SCALING
            MeterValues.voltage[PHASE_C] = MeterValues.voltage[PHASE_C] * voltageMultificationFactor + voltageOffset;
            if (MeterValues.current[PHASE_C] > 8.5)
            {
                MeterValues.current[PHASE_C] = MeterValues.current[PHASE_C] * config.CurrentGain2;
            }
            else
            {
                MeterValues.current[PHASE_C] = MeterValues.current[PHASE_C] * config.CurrentGain1;
            }
            MeterValues.current[PHASE_C] = MeterValues.current[PHASE_C] * config.CurrentGain - config.CurrentOffset;
        }
        if (MeterValues.voltage[PHASE_A] < 1.0)
            MeterValues.voltage[PHASE_A] = 0.0;
        if (MeterValues.current[PHASE_A] < 0.1)
            MeterValues.current[PHASE_A] = 0.0;

        if (MeterValues.voltage[PHASE_B] < 1.0)
            MeterValues.voltage[PHASE_B] = 0.0;
        if (MeterValues.current[PHASE_B] < 0.1)
            MeterValues.current[PHASE_B] = 0.0;

        if (MeterValues.voltage[PHASE_C] < 1.0)
            MeterValues.voltage[PHASE_C] = 0.0;
        if (MeterValues.current[PHASE_C] < 0.1)
            MeterValues.current[PHASE_C] = 0.0;

        if ((MeterValues.voltage[PHASE_A] > config.overVoltageThreshold) ||
            (MeterValues.voltage[PHASE_B] > config.overVoltageThreshold) ||
            (MeterValues.voltage[PHASE_C] > config.overVoltageThreshold))
        {
            MeterFaults.MeterOverVoltage = true;
        }
        else
        {
            MeterFaults.MeterOverVoltage = false;
        }

        if ((MeterValues.current[PHASE_A] > config.overCurrentThreshold[connId]) ||
            (MeterValues.current[PHASE_B] > config.overCurrentThreshold[connId]) ||
            (MeterValues.current[PHASE_C] > config.overCurrentThreshold[connId]))
        {
            MeterFaults.MeterOverCurrent = true;
        }
        else
        {
            MeterFaults.MeterOverCurrent = false;
        }

        if (MeterValues.temp > config.overTemperatureThreshold)
        {
            MeterFaults.MeterHighTemp = true;
        }
        else
        {
            MeterFaults.MeterHighTemp = false;
        }

        if ((MeterValues.voltage[PHASE_A] > (600.0)) ||
            (MeterValues.voltage[PHASE_B] > (600.0)) ||
            (MeterValues.voltage[PHASE_C] > (600.0)))
        {
            MeterFaults.MeterFailure = true;
        }
        else
        {
            MeterFaults.MeterFailure = false;
        }

        if (earthDisconnectCircuit)
        {
            if ((MeterValues.voltage[PHASE_A] < config.underVoltageThreshold) ||
                (MeterValues.voltage[PHASE_B] < config.underVoltageThreshold) ||
                (MeterValues.voltage[PHASE_C] < config.underVoltageThreshold))
            {
                MeterFaults.MeterUnderVoltage = true;
            }
            else
            {
                MeterFaults.MeterUnderVoltage = false;
            }

            if ((MeterValues.voltage[PHASE_A] < 50.0) ||
                (MeterValues.voltage[PHASE_B] < 50.0) ||
                (MeterValues.voltage[PHASE_C] < 50.0))
            {
                MeterFaults.MeterPowerLoss = true;
            }
            else
            {
                MeterFaults.MeterPowerLoss = false;
            }
        }
        else
        {
            if ((MeterValues.voltage[PHASE_A] < 20.0) ||
                (MeterValues.voltage[PHASE_B] < 20.0) ||
                (MeterValues.voltage[PHASE_C] < 20.0))
            {
                MeterFaults.MeterPowerLoss = true;
            }
            else
            {
                MeterFaults.MeterPowerLoss = false;
            }

            if (((MeterValues.voltage[PHASE_A] >= 20.0) && (MeterValues.voltage[PHASE_A] < 130.0)) ||
                ((MeterValues.voltage[PHASE_B] >= 20.0) && (MeterValues.voltage[PHASE_B] < 130.0)) ||
                ((MeterValues.voltage[PHASE_C] >= 20.0) && (MeterValues.voltage[PHASE_C] < 130.0)))
            {
                earthDisconnectButton = true;
            }
            else
            {
                earthDisconnectButton = false;
            }

            if (((MeterValues.voltage[PHASE_A] >= 130.0) && (MeterValues.voltage[PHASE_A] < config.underVoltageThreshold)) ||
                ((MeterValues.voltage[PHASE_B] >= 130.0) && (MeterValues.voltage[PHASE_B] < config.underVoltageThreshold)) ||
                ((MeterValues.voltage[PHASE_C] >= 130.0) && (MeterValues.voltage[PHASE_C] < config.underVoltageThreshold)))
            {
                MeterFaults.MeterUnderVoltage = true;
            }
            else
            {
                MeterFaults.MeterUnderVoltage = false;
            }
        }

        if (MeterFaults.MeterOverVoltage)
        {
            if ((MeterValues.voltage[PHASE_A] > MeterValues.voltage[PHASE_B]) && (MeterValues.voltage[PHASE_A] > MeterValues.voltage[PHASE_C]))
            {
                ConnectorData[connId].meterValueFault.voltage[PHASE_NA] = MeterValues.voltage[PHASE_A];
            }
            else if ((MeterValues.voltage[PHASE_B] > MeterValues.voltage[PHASE_A]) && (MeterValues.voltage[PHASE_B] > MeterValues.voltage[PHASE_C]))
            {
                ConnectorData[connId].meterValueFault.voltage[PHASE_NA] = MeterValues.voltage[PHASE_B];
            }
            else
            {
                ConnectorData[connId].meterValueFault.voltage[PHASE_NA] = MeterValues.voltage[PHASE_C];
            }
        }
        if (MeterFaults.MeterUnderVoltage)
        {
            if ((MeterValues.voltage[PHASE_A] < MeterValues.voltage[PHASE_B]) && (MeterValues.voltage[PHASE_A] < MeterValues.voltage[PHASE_C]))
            {
                ConnectorData[connId].meterValueFault.voltage[PHASE_NA] = MeterValues.voltage[PHASE_A];
            }
            else if ((MeterValues.voltage[PHASE_B] < MeterValues.voltage[PHASE_A]) && (MeterValues.voltage[PHASE_B] < MeterValues.voltage[PHASE_C]))
            {
                ConnectorData[connId].meterValueFault.voltage[PHASE_NA] = MeterValues.voltage[PHASE_B];
            }
            else
            {
                ConnectorData[connId].meterValueFault.voltage[PHASE_NA] = MeterValues.voltage[PHASE_C];
            }
        }
        if (MeterFaults.MeterOverCurrent)
        {
            if ((MeterValues.current[PHASE_A] > MeterValues.current[PHASE_B]) && (MeterValues.current[PHASE_A] > MeterValues.current[PHASE_C]))
            {
                ConnectorData[connId].meterValueFault.current[PHASE_NA] = MeterValues.current[PHASE_A];
            }
            else if ((MeterValues.current[PHASE_B] > MeterValues.current[PHASE_A]) && (MeterValues.current[PHASE_B] > MeterValues.current[PHASE_C]))
            {
                ConnectorData[connId].meterValueFault.current[PHASE_NA] = MeterValues.current[PHASE_B];
            }
            else
            {
                ConnectorData[connId].meterValueFault.current[PHASE_NA] = MeterValues.current[PHASE_C];
            }
        }
        if (MeterFaults.MeterHighTemp)
        {
            ConnectorData[connId].meterValueFault.temp = MeterValues.temp;
        }
    }
    else
    {
        if (xSemaphoreTake(mutexSpiBus, portMAX_DELAY))
        {
            MeterValues.temp = CommEnergyIC(spi_EM, ATM90E36A_READ, Temp, 0);
            if (connId == 1)
            {
                MeterValues.voltage[PHASE_A] = CommEnergyIC(spi_EM, ATM90E36A_READ, UrmsA, 0) * 0.01;
                MeterValues.current[PHASE_A] = CommEnergyIC(spi_EM, ATM90E36A_READ, IrmsA, 0) * 0.001;
                MeterValues.voltage[PHASE_NA] = MeterValues.voltage[PHASE_A];
                MeterValues.current[PHASE_NA] = MeterValues.current[PHASE_A];
                if (config.AC1)
                {
                    // for relay weld detection
                    MeterValues.voltage[PHASE_B] = CommEnergyIC(spi_EM, ATM90E36A_READ, UrmsB, 0) * 0.01;
                    MeterValues.voltage[PHASE_B] = MeterValues.voltage[PHASE_B] * voltageMultificationFactor + voltageOffset;
                }
            }
            else if (connId == 2)
            {
                MeterValues.voltage[PHASE_B] = CommEnergyIC(spi_EM, ATM90E36A_READ, UrmsB, 0) * 0.01;
                MeterValues.current[PHASE_B] = CommEnergyIC(spi_EM, ATM90E36A_READ, IrmsB, 0) * 0.001;
                MeterValues.voltage[PHASE_NA] = MeterValues.voltage[PHASE_B];
                MeterValues.current[PHASE_NA] = MeterValues.current[PHASE_B];
            }
            else if (connId == 3)
            {
                MeterValues.voltage[PHASE_C] = CommEnergyIC(spi_EM, ATM90E36A_READ, UrmsC, 0) * 0.01;
                MeterValues.current[PHASE_C] = CommEnergyIC(spi_EM, ATM90E36A_READ, IrmsC, 0) * 0.001;
                MeterValues.voltage[PHASE_NA] = MeterValues.voltage[PHASE_C];
                MeterValues.current[PHASE_NA] = MeterValues.current[PHASE_C];
            }
            xSemaphoreGive(mutexSpiBus);
            MeterValues.voltage[PHASE_NA] = MeterValues.voltage[PHASE_NA] * voltageMultificationFactor + voltageOffset;
            if (MeterValues.current[PHASE_NA] > 8.5)
            {
                MeterValues.current[PHASE_NA] = MeterValues.current[PHASE_NA] * config.CurrentGain2;
            }
            else
            {
                MeterValues.current[PHASE_NA] = MeterValues.current[PHASE_NA] * config.CurrentGain1;
            }
            MeterValues.current[PHASE_NA] = MeterValues.current[PHASE_NA] * config.CurrentGain - config.CurrentOffset;
        }
        if (MeterValues.voltage[PHASE_NA] < 1.0)
            MeterValues.voltage[PHASE_NA] = 0.0;
        if (MeterValues.current[PHASE_NA] < 0.1)
            MeterValues.current[PHASE_NA] = 0.0;
        MeterValues.power = MeterValues.voltage[PHASE_NA] * MeterValues.current[PHASE_NA];
        MeterFaults.MeterOverVoltage = (MeterValues.voltage[PHASE_NA] > config.overVoltageThreshold) ? true : false;
        MeterFaults.MeterOverCurrent = (MeterValues.current[PHASE_NA] > config.overCurrentThreshold[connId]) ? true : false;
        MeterFaults.MeterHighTemp = (MeterValues.temp > config.overTemperatureThreshold) ? true : false;
        MeterFaults.MeterFailure = (MeterValues.voltage[PHASE_NA] > (600.0)) ? true : false;
        if (earthDisconnectCircuit)
        {
            MeterFaults.MeterPowerLoss = (MeterValues.voltage[PHASE_NA] < 50.0) ? true : false;
            MeterFaults.MeterUnderVoltage = (MeterValues.voltage[PHASE_NA] < config.underVoltageThreshold) ? true : false;
        }
        else
        {
            MeterFaults.MeterPowerLoss = (MeterValues.voltage[PHASE_NA] < 20.0) ? true : false;
            earthDisconnectButton = ((MeterValues.voltage[PHASE_NA] >= 20.0) && (MeterValues.voltage[PHASE_NA] < 130.0)) ? true : false;
            MeterFaults.MeterUnderVoltage = ((MeterValues.voltage[PHASE_NA] >= 130.0) && (MeterValues.voltage[PHASE_NA] < config.underVoltageThreshold)) ? true : false;
        }

        if (MeterFaults.MeterOverVoltage || MeterFaults.MeterUnderVoltage)
        {
            ConnectorData[connId].meterValueFault.voltage[PHASE_NA] = MeterValues.voltage[PHASE_NA];
        }
        if (MeterFaults.MeterOverCurrent)
        {
            ConnectorData[connId].meterValueFault.current[PHASE_NA] = MeterValues.current[PHASE_NA];
        }
        if (MeterFaults.MeterHighTemp)
        {
            ConnectorData[connId].meterValueFault.temp = MeterValues.temp;
        }
        if (config.AC1)
        {
            if ((relay_state == 0) &&
                (MeterValues.voltage[PHASE_B] > 10.0) &&
                (config.Ac11s == false) &&
                (config.Ac22s == false) &&
                (config.Ac44s == false) &&
                (relayOffTime > 50))
            {
                relayWeldDetectionButton = true;
            }
            else if ((relay_state == 1) &&
                     (MeterValues.voltage[PHASE_B] < 10.0) &&
                     (config.Ac11s == false) &&
                     (config.Ac22s == false) &&
                     (config.Ac44s == false) &&
                     (relayOnTime > 50))
            {
                relayWeldDetectionButton = true;
            }
            else
            {
                relayWeldDetectionButton = false;
            }
        }
    }

    return MeterFaults;
}

MeterValues_t readEnrgyMeter(uint8_t connId)
{
    MeterValues_t MeterValues;
    if (config.Ac11s || config.Ac22s || config.Ac44s)
    {
        if (xSemaphoreTake(mutexSpiBus, portMAX_DELAY))
        {
            MeterValues.temp = CommEnergyIC(spi_EM, ATM90E36A_READ, Temp, 0);

            MeterValues.voltage[PHASE_A] = CommEnergyIC(spi_EM, ATM90E36A_READ, UrmsA, 0) * 0.01;
            MeterValues.current[PHASE_A] = CommEnergyIC(spi_EM, ATM90E36A_READ, IrmsA, 0) * 0.001;

            MeterValues.voltage[PHASE_B] = CommEnergyIC(spi_EM, ATM90E36A_READ, UrmsB, 0) * 0.01;
            MeterValues.current[PHASE_B] = CommEnergyIC(spi_EM, ATM90E36A_READ, IrmsB, 0) * 0.001;

            MeterValues.voltage[PHASE_C] = CommEnergyIC(spi_EM, ATM90E36A_READ, UrmsC, 0) * 0.01;
            MeterValues.current[PHASE_C] = CommEnergyIC(spi_EM, ATM90E36A_READ, IrmsC, 0) * 0.001;
            xSemaphoreGive(mutexSpiBus);

            MeterValues.voltage[PHASE_A] = MeterValues.voltage[PHASE_A] * voltageMultificationFactor + voltageOffset;
            MeterValues.voltage[PHASE_B] = MeterValues.voltage[PHASE_B] * voltageMultificationFactor + voltageOffset;
            MeterValues.voltage[PHASE_C] = MeterValues.voltage[PHASE_C] * voltageMultificationFactor + voltageOffset;

            MeterValues.current[PHASE_A] = (MeterValues.current[PHASE_A] > 8.5) ? (MeterValues.current[PHASE_A] * config.CurrentGain2) : (MeterValues.current[PHASE_A] * config.CurrentGain1);
            MeterValues.current[PHASE_A] = MeterValues.current[PHASE_A] * config.CurrentGain - config.CurrentOffset;
            MeterValues.current[PHASE_B] = (MeterValues.current[PHASE_B] > 8.5) ? (MeterValues.current[PHASE_B] * config.CurrentGain2) : (MeterValues.current[PHASE_B] * config.CurrentGain1);
            MeterValues.current[PHASE_B] = MeterValues.current[PHASE_B] * config.CurrentGain - config.CurrentOffset;
            MeterValues.current[PHASE_C] = (MeterValues.current[PHASE_C] > 8.5) ? (MeterValues.current[PHASE_C] * config.CurrentGain2) : (MeterValues.current[PHASE_C] * config.CurrentGain1);
            MeterValues.current[PHASE_C] = MeterValues.current[PHASE_C] * config.CurrentGain - config.CurrentOffset;

            if (MeterValues.voltage[PHASE_A] < 1.0)
                MeterValues.voltage[PHASE_A] = 0.0;
            if (MeterValues.current[PHASE_A] < 0.1)
                MeterValues.current[PHASE_A] = 0.0;
            if (MeterValues.voltage[PHASE_B] < 1.0)
                MeterValues.voltage[PHASE_B] = 0.0;
            if (MeterValues.current[PHASE_B] < 0.1)
                MeterValues.current[PHASE_B] = 0.0;
            if (MeterValues.voltage[PHASE_C] < 1.0)
                MeterValues.voltage[PHASE_C] = 0.0;
            if (MeterValues.current[PHASE_C] < 0.1)
                MeterValues.current[PHASE_C] = 0.0;
        }

        MeterValues.power = MeterValues.voltage[PHASE_A] * MeterValues.current[PHASE_A];
        MeterValues.power = MeterValues.power + MeterValues.voltage[PHASE_B] * MeterValues.current[PHASE_B];
        MeterValues.power = MeterValues.power + MeterValues.voltage[PHASE_C] * MeterValues.current[PHASE_C];
    }
    else
    {
        if (xSemaphoreTake(mutexSpiBus, portMAX_DELAY))
        {
            MeterValues.temp = CommEnergyIC(spi_EM, ATM90E36A_READ, Temp, 0);
            MeterValues.voltage[connId] = CommEnergyIC(spi_EM, ATM90E36A_READ, (UrmsA + connId - 1), 0) * 0.01;
            MeterValues.current[connId] = CommEnergyIC(spi_EM, ATM90E36A_READ, (IrmsA + connId - 1), 0) * 0.001;
            xSemaphoreGive(mutexSpiBus);

            MeterValues.voltage[connId] = MeterValues.voltage[connId] * voltageMultificationFactor + voltageOffset;

            MeterValues.current[connId] = (MeterValues.current[connId] > 8.5) ? (MeterValues.current[connId] * config.CurrentGain2) : (MeterValues.current[connId] * config.CurrentGain1);
            MeterValues.current[connId] = MeterValues.current[connId] * config.CurrentGain - config.CurrentOffset;
        }

        if (MeterValues.voltage[connId] < 1.0)
            MeterValues.voltage[connId] = 0.0;
        if (MeterValues.current[connId] < 0.1)
            MeterValues.current[connId] = 0.0;

        MeterValues.power = MeterValues.voltage[connId] * MeterValues.current[connId];
    }

    return MeterValues;
}

uint16_t CommEnergyIC(spi_device_handle_t spi_EM, uint16_t RW, uint16_t address, uint16_t val)
{
    uint16_t readCommand = address | RW;
    uint8_t tx_data[4] = {(readCommand >> 8) & 0xFF, readCommand & 0xFF, (val >> 8) & 0xFF, val & 0xFF};
    uint8_t rx_data[4] = {0, 0, 0, 0};
    gpio_set_level(EM_ISO_EN_PIN, 1);
    ets_delay_us(10);

    spi_transaction_t transaction = {
        .cmd = 0,
        .addr = 0,
        .length = 32,   // 2 bytes (16 bits)
        .rxlength = 32, // 2 bytes (16 bits)
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };

    esp_err_t ret = spi_device_transmit(spi_EM, &transaction);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI transaction failed, error = %x\n", ret);
    }

    ets_delay_us(10);
    gpio_set_level(EM_ISO_EN_PIN, 0);
    return ((rx_data[2] << 8) | rx_data[3]);
}

void ATM90E36_Initialise(void)
{
    if (config.AC1)
    {
        voltageMultificationFactor = 1.539249378;
        voltageOffset = 0.051283407;
        config.CurrentGain1 = 1.0;
        config.CurrentGain2 = 1.0;
        config.CurrentGain = 0.98042;
        config.CurrentOffset = -0.02957;
    }
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, SoftReset, 0x789A); // Perform soft reset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, FuncEn0, 0x0000);   // Voltage sag
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, FuncEn1, 0x0000);   // Voltage sag
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, SagTh, 0x0001);     // Voltage sag threshold

    /* SagTh = Vth * 100 * sqrt(2) / (2 * Ugain / 32768) */

    // Set metering config values (CONFIG)
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, ConfigStart, 0x5678); // Metering calibration startup
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, PLconstH, 0x0861);    // PL Constant MSB (default)
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, PLconstL, 0xC468);    // PL Constant LSB (default)
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, MMode0, 0x1087);      // Mode Config (60 Hz, 3P4W)
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, MMode1, 0x1500);      // 0x5555 (x2) // 0x0000 (1x)
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, PStartTh, 0x0000);    // Active Startup Power Threshold
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, QStartTh, 0x0000);    // Reactive Startup Power Threshold
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, SStartTh, 0x0000);    // Apparent Startup Power Threshold
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, PPhaseTh, 0x0000);    // Active Phase Threshold
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, QPhaseTh, 0x0000);    // Reactive Phase Threshold
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, SPhaseTh, 0x0000);    // Apparent  Phase Threshold
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, CSZero, 0x4741);      // Checksum 0

    // Set metering calibration values (CALIBRATION)
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, CalStart, 0x5678); // Metering calibration startup
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, GainA, 0x0000);    // Line calibration gain
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, PhiA, 0x0000);     // Line calibration angle
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, GainB, 0x0000);    // Line calibration gain
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, PhiB, 0x0000);     // Line calibration angle
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, GainC, 0x0000);    // Line calibration gain
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, PhiC, 0x0000);     // Line calibration angle
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, PoffsetA, 0x0000); // A line active power offset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, QoffsetA, 0x0000); // A line reactive power offset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, PoffsetB, 0x0000); // B line active power offset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, QoffsetB, 0x0000); // B line reactive power offset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, PoffsetC, 0x0000); // C line active power offset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, QoffsetC, 0x0000); // C line reactive power offset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, CSOne, 0x0000);    // Checksum 1

    // Set metering calibration values (HARMONIC)
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, HarmStart, 0x5678); // Metering calibration startup
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, POffsetAF, 0x0000); // A Fund. active power offset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, POffsetBF, 0x0000); // B Fund. active power offset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, POffsetCF, 0x0000); // C Fund. active power offset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, PGainAF, 0x0000);   // A Fund. active power gain
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, PGainBF, 0x0000);   // B Fund. active power gain
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, PGainCF, 0x0000);   // C Fund. active power gain
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, CSTwo, 0x0000);     // Checksum 2

    // Set measurement calibration values (ADJUST)
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, AdjStart, 0x5678); // Measurement calibration
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, UgainA, 0x0002);   // A SVoltage rms gain
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, IgainA, 0xFD7F);   // A line current gain
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, UoffsetA, 0x0000); // A Voltage offset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, IoffsetA, 0x0000); // A line current offset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, UgainB, 0x0002);   // B Voltage rms gain
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, IgainB, 0xFD7F);   // B line current gain
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, UoffsetB, 0x0000); // B Voltage offset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, IoffsetB, 0x0000); // B line current offset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, UgainC, 0x0002);   // C Voltage rms gain
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, IgainC, 0xFD7F);   // C line current gain
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, UoffsetC, 0x0000); // C Voltage offset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, IoffsetC, 0x0000); // C line current offset
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, IgainN, 0xFD7F);   // C line current gain
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, CSThree, 0x02F6);  // Checksum 3

    // Done with the configuration
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, ConfigStart, 0x5678);
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, CalStart, 0x5678);  // 0x6886 //0x5678 //8765);
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, HarmStart, 0x5678); // 0x6886 //0x5678 //8765);
    CommEnergyIC(spi_EM, ATM90E36A_WRITE, AdjStart, 0x5678);  // 0x6886 //0x5678 //8765);

    CommEnergyIC(spi_EM, ATM90E36A_WRITE, SoftReset, 0x789A); // Perform soft reset
}