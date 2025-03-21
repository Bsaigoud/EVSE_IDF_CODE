#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"
#include <esp_log.h>
#include "ControlPilotPwm.h"
#include "ControlPilotAdc.h"
#include "EnergyMeter.h"
#include "ControlPilotState.h"
#include "slave.h"
#include "faultGpios.h"

EVSE_states_enum_t EVSE_states_enum;

#define TAG "CP_STATE"
#define TAG_DEBUG "CP_ADC"
extern uint8_t cpState[NUM_OF_CONNECTORS];
extern bool slave_adc_received;
extern uint16_t cp_adc_value[NUM_OF_CONNECTORS];
uint8_t cpState_local_old[NUM_OF_CONNECTORS];
extern uint8_t cpState[NUM_OF_CONNECTORS];
extern Config_t config;

uint8_t cpDutyCycleCurrent[8] = {10, 16, 20, 25, 32, 40, 50, 64};
uint8_t cpDutyCycle[8] = {16, 26, 33, 41, 53, 66, 83, 89};

uint16_t cp_stateA_max_rawvalue[8] = {4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096};
uint16_t cp_stateA_min_rawvalue[8] = {3702, 3702, 3702, 3702, 3500, 3702, 3702, 3766};
uint16_t cp_stateB1_max_rawvalue[8] = {3702, 3702, 3702, 3702, 3500, 3702, 3702, 3372};
uint16_t cp_stateB1_min_rawvalue[8] = {3031, 3031, 3031, 3031, 2800, 3031, 3234, 3031};
uint16_t cp_stateE2_max_rawvalue[8] = {3031, 3031, 3031, 3031, 2800, 3031, 2957, 3031};
uint16_t cp_stateE2_min_rawvalue[8] = {1658, 1849, 1984, 2138, 2100, 2616, 2726, 2830};
uint16_t cp_state_disc_max_rawvalue[8] = {1658, 1849, 1984, 2138, 2100, 2616, 3234, 3766};
uint16_t cp_state_disc_min_rawvalue[8] = {522, 878, 1129, 1416, 1800, 2309, 2957, 3372};
uint16_t cp_stateB2_max_rawvalue[8] = {522, 878, 1129, 1416, 1800, 2309, 2726, 2830};
uint16_t cp_stateB2_min_rawvalue[8] = {444, 749, 962, 1209, 1550, 1976, 2493, 2680};
uint16_t cp_stateC_max_rawvalue[8] = {444, 749, 962, 1209, 1550, 1976, 2493, 2680};
uint16_t cp_stateC_min_rawvalue[8] = {371, 627, 802, 1008, 1150, 1652, 2087, 2240};
uint16_t cp_stateD_max_rawvalue[8] = {371, 627, 802, 1008, 1150, 1652, 2087, 2240};
uint16_t cp_stateD_min_rawvalue[8] = {268, 458, 612, 804, 850, 1296, 1618, 1763};
uint16_t cp_stateE1_max_rawvalue[8] = {268, 458, 612, 804, 850, 1296, 1618, 1763};
uint16_t cp_stateE1_min_rawvalue[8] = {0, 0, 0, 0, 0, 0, 0, 0};

uint16_t cp_stateA_min[4] = {0, 0, 0, 0};
uint16_t cp_stateA_max[4] = {0, 0, 0, 0};
uint16_t cp_stateB1_min[4] = {0, 0, 0, 0};
uint16_t cp_stateB1_max[4] = {0, 0, 0, 0};
uint16_t cp_stateB2_min[4] = {0, 0, 0, 0};
uint16_t cp_stateB2_max[4] = {0, 0, 0, 0};
uint16_t cp_stateE2_min[4] = {0, 0, 0, 0};
uint16_t cp_stateE2_max[4] = {0, 0, 0, 0};
uint16_t cp_state_disc_min[4] = {0, 0, 0, 0};
uint16_t cp_state_disc_max[4] = {0, 0, 0, 0};
uint16_t cp_stateC_min[4] = {0, 0, 0, 0};
uint16_t cp_stateC_max[4] = {0, 0, 0, 0};
uint16_t cp_stateD_min[4] = {0, 0, 0, 0};
uint16_t cp_stateD_max[4] = {0, 0, 0, 0};
uint16_t cp_stateE1_min[4] = {0, 0, 0, 0};
uint16_t cp_stateE1_max[4] = {0, 0, 0, 0};

uint8_t taskCpId[NUM_OF_CONNECTORS] = {0, 1, 2, 3};

extern SemaphoreHandle_t mutexControlPilot;
extern bool cpEnable[NUM_OF_CONNECTORS];

void cpTask(void *params)
{
    uint8_t cpId = *((uint8_t *)params);
    int taskDelayTime = 500;
    cpState[cpId] = STATE_A;
    cpState_local_old[cpId] = cpState[cpId];
    uint32_t delayTimeToDisplayAdcValues = 0;
    SetControlPilotDuty(100);
    updateRelayState(cpId, 0);
    while (true)
    {
        xSemaphoreTake(mutexControlPilot, portMAX_DELAY);

        readAdcValue();
        if (delayTimeToDisplayAdcValues > 5000)
        {
            delayTimeToDisplayAdcValues = 0;
            ESP_LOGI(TAG_DEBUG, "cp %hhu adc value = %d", cpId, cp_adc_value[cpId]);
        }
        if ((cp_adc_value[cpId] >= cp_stateA_min[cpId]) & (cp_adc_value[cpId] < cp_stateA_max[cpId]))
        {
            updateRelayState(cpId, 0);
            SetControlPilotDuty(100);
            cpState[cpId] = STATE_A;
        }
        else if ((cp_adc_value[cpId] >= cp_stateB1_min[cpId]) & (cp_adc_value[cpId] < cp_stateB1_max[cpId]))
        {
            updateRelayState(cpId, 0);
            SetControlPilotDuty(100);
            cpState[cpId] = STATE_B1;
        }
        else if ((cp_adc_value[cpId] >= cp_stateB2_min[cpId]) & (cp_adc_value[cpId] < cp_stateB2_max[cpId]))
        {
            updateRelayState(cpId, 0);
            SetControlPilotDuty(config.cpDuty[1]);
            cpState[cpId] = STATE_B2;
        }
        else if ((cp_adc_value[cpId] >= cp_stateE2_min[cpId]) & (cp_adc_value[cpId] < cp_stateE2_max[cpId]))
        {
            SetControlPilotDuty(100);
            updateRelayState(cpId, 0);
            cpState[cpId] = STATE_E2;
        }
        else if ((cp_adc_value[cpId] >= cp_state_disc_min[cpId]) & (cp_adc_value[cpId] < cp_state_disc_max[cpId]))
        {
            updateRelayState(cpId, 0);
            SetControlPilotDuty(100);
            cpState[cpId] = STATE_DIS;
        }
        else if ((cp_adc_value[cpId] >= cp_stateC_min[cpId]) & (cp_adc_value[cpId] < cp_stateC_max[cpId]))
        {
            SetControlPilotDuty(config.cpDuty[1]);
            updateRelayState(cpId, 1);
            cpState[cpId] = STATE_C;
        }
        else if ((cp_adc_value[cpId] >= cp_stateD_min[cpId]) & (cp_adc_value[cpId] < cp_stateD_max[cpId]))
        {
            SetControlPilotDuty(config.cpDuty[1]);
            updateRelayState(cpId, 1);
            cpState[cpId] = STATE_D;
        }
        else if (cp_adc_value[cpId] < cp_stateE1_max[cpId])
        {
            SetControlPilotDuty(100);
            updateRelayState(cpId, 0);
            cpState[cpId] = STATE_E1;
        }
        else
        {
            updateRelayState(cpId, 0);
            SetControlPilotDuty(100);
        }
        xSemaphoreGive(mutexControlPilot);

        if (cpState[cpId] != cpState_local_old[cpId])
        {
            ESP_LOGI(TAG_DEBUG, "cp %hhu adc value = %d", cpId, cp_adc_value[cpId]);
            if (cpState[cpId] == STATE_A)
            {
                ESP_LOGI(TAG, "CP %hhu STATE A", cpId);
            }
            else if (cpState[cpId] == STATE_B1)
            {
                ESP_LOGI(TAG, "CP %hhu STATE B1", cpId);
            }
            else if (cpState[cpId] == STATE_B2)
            {
                ESP_LOGI(TAG, "CP %hhu STATE B2", cpId);
            }
            else if (cpState[cpId] == STATE_C)
            {
                ESP_LOGI(TAG, "CP %hhu STATE C", cpId);
            }
            else if (cpState[cpId] == STATE_D)
            {
                ESP_LOGI(TAG, "CP %hhu STATE D", cpId);
            }
            else if (cpState[cpId] == STATE_E1)
            {
                ESP_LOGI(TAG, "CP %hhu STATE E1", cpId);
            }
            else if (cpState[cpId] == STATE_DIS)
            {
                ESP_LOGI(TAG, "CP %hhu STATE DISCONNECTED", cpId);
            }
            else if (cpState[cpId] == STATE_E2)
            {
                ESP_LOGI(TAG, "CP %hhu STATE E2", cpId);
            }
            else if (cpState[cpId] == FAULT)
            {
                ESP_LOGI(TAG, "CP %hhu STATE FAULT", cpId);
            }
            else
            {
                ESP_LOGI(TAG, "CP %hhu STATE UNKOWN", cpId);
            }
        }
        cpState_local_old[cpId] = cpState[cpId];

        if ((cpState[cpId] == STATE_B2) || (cpState[cpId] == STATE_C) || (cpState[cpId] == STATE_DIS))
        {
            taskDelayTime = 40;
        }
        else
        {
            taskDelayTime = 500;
        }
        delayTimeToDisplayAdcValues = delayTimeToDisplayAdcValues + taskDelayTime;
        vTaskDelay(taskDelayTime / portTICK_PERIOD_MS);
    }
}

void updateCpConfig(void)
{
    if (config.Ac7s || config.Ac11s || config.Ac22s || config.Ac44s || config.Ac14D || config.Ac10DP || config.Ac10DA || config.Ac14t || config.Ac18t)
    {
        for (uint8_t i = 0; i < 8; i++)
        {
            if (config.cpDuty[1] == cpDutyCycle[i])
            {
                cp_stateA_min[1] = cp_stateA_min_rawvalue[i];
                cp_stateA_max[1] = cp_stateA_max_rawvalue[i];
                cp_stateB1_min[1] = cp_stateB1_min_rawvalue[i];
                cp_stateB1_max[1] = cp_stateB1_max_rawvalue[i];
                cp_stateB2_min[1] = cp_stateB2_min_rawvalue[i];
                cp_stateB2_max[1] = cp_stateB2_max_rawvalue[i];
                cp_stateE2_min[1] = cp_stateE2_min_rawvalue[i];
                cp_stateE2_max[1] = cp_stateE2_max_rawvalue[i];
                cp_state_disc_min[1] = cp_state_disc_min_rawvalue[i];
                cp_state_disc_max[1] = cp_state_disc_max_rawvalue[i];
                cp_stateC_min[1] = cp_stateC_min_rawvalue[i];
                cp_stateC_max[1] = cp_stateC_max_rawvalue[i];
                cp_stateD_min[1] = cp_stateD_min_rawvalue[i];
                cp_stateD_max[1] = cp_stateD_max_rawvalue[i];
                cp_stateE1_min[1] = cp_stateE1_min_rawvalue[i];
                cp_stateE1_max[1] = cp_stateE1_max_rawvalue[i];
                SetControlPilotDuty(config.cpDuty[1]);
            }
        }
    }
    if (config.Ac14D || config.Ac18t)
    {
        for (uint8_t i = 0; i < 8; i++)
        {
            if (config.cpDuty[2] == cpDutyCycle[i])
            {
                cp_stateA_min[2] = cp_stateA_min_rawvalue[i];
                cp_stateA_max[2] = cp_stateA_max_rawvalue[i];
                cp_stateB1_min[2] = cp_stateB1_min_rawvalue[i];
                cp_stateB1_max[2] = cp_stateB1_max_rawvalue[i];
                cp_stateB2_min[2] = cp_stateB2_min_rawvalue[i];
                cp_stateB2_max[2] = cp_stateB2_max_rawvalue[i];
                cp_stateE2_min[2] = cp_stateE2_min_rawvalue[i];
                cp_stateE2_max[2] = cp_stateE2_max_rawvalue[i];
                cp_state_disc_min[2] = cp_state_disc_min_rawvalue[i];
                cp_state_disc_max[2] = cp_state_disc_max_rawvalue[i];
                cp_stateC_min[2] = cp_stateC_min_rawvalue[i];
                cp_stateC_max[2] = cp_stateC_max_rawvalue[i];
                cp_stateD_min[2] = cp_stateD_min_rawvalue[i];
                cp_stateD_max[2] = cp_stateD_max_rawvalue[i];
                cp_stateE1_min[2] = cp_stateE1_min_rawvalue[i];
                cp_stateE1_max[2] = cp_stateE1_max_rawvalue[i];
                SetControlPilotDuty(config.cpDuty[1]);
            }
        }
    }
    ESP_LOGI(TAG, "--------------------------------------------------");
    ESP_LOGI(TAG, "CP ADC Values for Duty %hhu", config.cpDuty[1]);
    ESP_LOGI(TAG, "--------------------------------------------------");
    ESP_LOGI(TAG, "cp_stateA_max : %d", cp_stateA_max[1]);
    ESP_LOGI(TAG, "cp_stateA_min : %d", cp_stateA_min[1]);
    ESP_LOGI(TAG, "cp_stateB1_max : %d", cp_stateB1_max[1]);
    ESP_LOGI(TAG, "cp_stateB1_min : %d", cp_stateB1_min[1]);
    ESP_LOGI(TAG, "cp_stateE2_max : %d", cp_stateE2_max[1]);
    ESP_LOGI(TAG, "cp_stateE2_min : %d", cp_stateE2_min[1]);
    ESP_LOGI(TAG, "cp_state_disc_max : %d", cp_state_disc_max[1]);
    ESP_LOGI(TAG, "cp_state_disc_min : %d", cp_state_disc_min[1]);
    ESP_LOGI(TAG, "cp_stateB2_max : %d", cp_stateB2_max[1]);
    ESP_LOGI(TAG, "cp_stateB2_min : %d", cp_stateB2_min[1]);
    ESP_LOGI(TAG, "cp_stateC_max : %d", cp_stateC_max[1]);
    ESP_LOGI(TAG, "cp_stateC_min : %d", cp_stateC_min[1]);
    ESP_LOGI(TAG, "cp_stateD_max : %d", cp_stateD_max[1]);
    ESP_LOGI(TAG, "cp_stateD_min : %d", cp_stateD_min[1]);
    ESP_LOGI(TAG, "cp_stateE1_max : %d", cp_stateE1_max[1]);
    ESP_LOGI(TAG, "cp_stateE1_min : %d", cp_stateE1_min[1]);
    ESP_LOGI(TAG, "--------------------------------------------------");

    if (config.vcharge_lite_1_4)
    {
        SlaveCpConfig_t slaveCpConfig;
        slaveCpConfig.cpEnable[0] = cpEnable[0];
        slaveCpConfig.cpEnable[1] = cpEnable[1];
        slaveCpConfig.cpEnable[2] = cpEnable[2];
        slaveCpConfig.cpEnable[3] = cpEnable[3];
        slaveCpConfig.cpDuty[0] = config.cpDuty[0];
        slaveCpConfig.cpDuty[1] = config.cpDuty[1];
        slaveCpConfig.cpDuty[2] = config.cpDuty[2];
        slaveCpConfig.cpDuty[3] = config.cpDuty[3];
        for (int i = 0; i < NUM_OF_CONNECTORS; i++)
        {
            slaveCpConfig.cp_stateA_min[i] = cp_stateA_min[i];
            slaveCpConfig.cp_stateA_max[i] = cp_stateA_max[i];
            slaveCpConfig.cp_stateB1_min[i] = cp_stateB1_min[i];
            slaveCpConfig.cp_stateB1_max[i] = cp_stateB1_max[i];
            slaveCpConfig.cp_stateB2_min[i] = cp_stateB2_min[i];
            slaveCpConfig.cp_stateB2_max[i] = cp_stateB2_max[i];
            slaveCpConfig.cp_stateE2_min[i] = cp_stateE2_min[i];
            slaveCpConfig.cp_stateE2_max[i] = cp_stateE2_max[i];
            slaveCpConfig.cp_state_disc_min[i] = cp_state_disc_min[i];
            slaveCpConfig.cp_state_disc_max[i] = cp_state_disc_max[i];
            slaveCpConfig.cp_stateC_min[i] = cp_stateC_min[i];
            slaveCpConfig.cp_stateC_max[i] = cp_stateC_max[i];
            slaveCpConfig.cp_stateD_min[i] = cp_stateD_min[i];
            slaveCpConfig.cp_stateD_max[i] = cp_stateD_max[i];
            slaveCpConfig.cp_stateE1_min[i] = cp_stateE1_min[i];
            slaveCpConfig.cp_stateE1_max[i] = cp_stateE1_max[i];
        }
        SlaveSendControlPilotConfig(slaveCpConfig);
    }
}

void Control_pilot_state_init(void)
{
    if (config.AC1)
    {
        cp_stateA_max_rawvalue[0] = 4096;
        cp_stateA_min_rawvalue[0] = 3900;
        cp_stateB1_max_rawvalue[0] = 3600;
        cp_stateB1_min_rawvalue[0] = 3420;
        cp_stateE2_max_rawvalue[0] = 3000;
        cp_stateE2_min_rawvalue[0] = 2800;
        cp_state_disc_max_rawvalue[0] = 716;
        cp_state_disc_min_rawvalue[0] = 639;
        cp_stateB2_max_rawvalue[0] = 641;
        cp_stateB2_min_rawvalue[0] = 567;
        cp_stateC_max_rawvalue[0] = 566;
        cp_stateC_min_rawvalue[0] = 495;
        cp_stateD_max_rawvalue[0] = 495;
        cp_stateD_min_rawvalue[0] = 248;
        cp_stateE1_max_rawvalue[0] = 248;
        cp_stateE1_min_rawvalue[0] = 0;

        cp_stateA_max_rawvalue[1] = 4096;
        cp_stateA_min_rawvalue[1] = 3900;
        cp_stateB1_max_rawvalue[1] = 3600;
        cp_stateB1_min_rawvalue[1] = 3420;
        cp_stateE2_max_rawvalue[1] = 3000;
        cp_stateE2_min_rawvalue[1] = 2800;
        cp_state_disc_max_rawvalue[1] = 1104;
        cp_state_disc_min_rawvalue[1] = 1013;
        cp_stateB2_max_rawvalue[1] = 980;
        cp_stateB2_min_rawvalue[1] = 893;
        cp_stateC_max_rawvalue[1] = 858;
        cp_stateC_min_rawvalue[1] = 775;
        cp_stateD_max_rawvalue[1] = 775;
        cp_stateD_min_rawvalue[1] = 388;
        cp_stateE1_max_rawvalue[1] = 388;
        cp_stateE1_min_rawvalue[1] = 0;

        cp_stateA_max_rawvalue[2] = 4096;
        cp_stateA_min_rawvalue[2] = 3900;
        cp_stateB1_max_rawvalue[2] = 3600;
        cp_stateB1_min_rawvalue[2] = 3420;
        cp_stateE2_max_rawvalue[2] = 3000;
        cp_stateE2_min_rawvalue[2] = 2800;
        cp_state_disc_max_rawvalue[2] = 1372;
        cp_state_disc_min_rawvalue[2] = 1272;
        cp_stateB2_max_rawvalue[2] = 1214;
        cp_stateB2_min_rawvalue[2] = 1119;
        cp_stateC_max_rawvalue[2] = 1058;
        cp_stateC_min_rawvalue[2] = 969;
        cp_stateD_max_rawvalue[2] = 969;
        cp_stateD_min_rawvalue[2] = 485;
        cp_stateE1_max_rawvalue[2] = 485;
        cp_stateE1_min_rawvalue[2] = 0;

        cp_stateA_max_rawvalue[3] = 4096;
        cp_stateA_min_rawvalue[3] = 3900;
        cp_stateB1_max_rawvalue[3] = 3600;
        cp_stateB1_min_rawvalue[3] = 3420;
        cp_stateE2_max_rawvalue[3] = 3000;
        cp_stateE2_min_rawvalue[3] = 2800;
        cp_state_disc_max_rawvalue[3] = 1681;
        cp_state_disc_min_rawvalue[3] = 1571;
        cp_stateB2_max_rawvalue[3] = 1484;
        cp_stateB2_min_rawvalue[3] = 1380;
        cp_stateC_max_rawvalue[3] = 1290;
        cp_stateC_min_rawvalue[3] = 1193;
        cp_stateD_max_rawvalue[3] = 1193;
        cp_stateD_min_rawvalue[3] = 597;
        cp_stateE1_max_rawvalue[3] = 597;
        cp_stateE1_min_rawvalue[3] = 0;

        cp_stateA_max_rawvalue[4] = 4096;
        cp_stateA_min_rawvalue[4] = 3900;
        cp_stateB1_max_rawvalue[4] = 3600;
        cp_stateB1_min_rawvalue[4] = 3420;
        cp_stateE2_max_rawvalue[4] = 3000;
        cp_stateE2_min_rawvalue[4] = 2800;
        cp_state_disc_max_rawvalue[4] = 2145;
        cp_state_disc_min_rawvalue[4] = 2020;
        cp_stateB2_max_rawvalue[4] = 1889;
        cp_stateB2_min_rawvalue[4] = 1772;
        cp_stateC_max_rawvalue[4] = 1637;
        cp_stateC_min_rawvalue[4] = 1530;
        cp_stateD_max_rawvalue[4] = 1530;
        cp_stateD_min_rawvalue[4] = 765;
        cp_stateE1_max_rawvalue[4] = 765;
        cp_stateE1_min_rawvalue[4] = 0;

        cp_stateA_max_rawvalue[5] = 4096;
        cp_stateA_min_rawvalue[5] = 3900;
        cp_stateB1_max_rawvalue[5] = 3600;
        cp_stateB1_min_rawvalue[5] = 3420;
        cp_stateE2_max_rawvalue[5] = 3000;
        cp_stateE2_min_rawvalue[5] = 2800;
        cp_state_disc_max_rawvalue[5] = 2657;
        cp_state_disc_min_rawvalue[5] = 2517;
        cp_stateB2_max_rawvalue[5] = 2327;
        cp_stateB2_min_rawvalue[5] = 2195;
        cp_stateC_max_rawvalue[5] = 2012;
        cp_stateC_min_rawvalue[5] = 1892;
        cp_stateD_max_rawvalue[5] = 1892;
        cp_stateD_min_rawvalue[5] = 946;
        cp_stateE1_max_rawvalue[5] = 946;
        cp_stateE1_min_rawvalue[5] = 0;

        cp_stateA_max_rawvalue[6] = 4096;
        cp_stateA_min_rawvalue[6] = 3900;
        cp_stateB1_max_rawvalue[6] = 3600;
        cp_stateB1_min_rawvalue[6] = 3420;
        cp_stateE2_max_rawvalue[6] = 3000;
        cp_stateE2_min_rawvalue[6] = 2886;
        cp_state_disc_max_rawvalue[6] = 3410;
        cp_state_disc_min_rawvalue[6] = 3200;
        cp_stateB2_max_rawvalue[6] = 2886;
        cp_stateB2_min_rawvalue[6] = 2764;
        cp_stateC_max_rawvalue[6] = 2500;
        cp_stateC_min_rawvalue[6] = 2368;
        cp_stateD_max_rawvalue[6] = 2368;
        cp_stateD_min_rawvalue[6] = 1184;
        cp_stateE1_max_rawvalue[6] = 1184;
        cp_stateE1_min_rawvalue[6] = 0;

        cp_stateA_max_rawvalue[7] = 4096;
        cp_stateA_min_rawvalue[7] = 3900;
        cp_stateB1_max_rawvalue[7] = 3572;
        cp_stateB1_min_rawvalue[7] = 3420;
        cp_stateE2_max_rawvalue[7] = 2985;
        cp_stateE2_min_rawvalue[7] = 2800;
        cp_state_disc_max_rawvalue[7] = 3750;
        cp_state_disc_min_rawvalue[7] = 3573;
        cp_stateB2_max_rawvalue[7] = 3129;
        cp_stateB2_min_rawvalue[7] = 2988;
        cp_stateC_max_rawvalue[7] = 2675;
        cp_stateC_min_rawvalue[7] = 2540;
        cp_stateD_max_rawvalue[7] = 2540;
        cp_stateD_min_rawvalue[7] = 1270;
        cp_stateE1_max_rawvalue[7] = 1270;
        cp_stateE1_min_rawvalue[7] = 0;
    }
    if (config.Ac7s || config.Ac11s || config.Ac22s || config.Ac44s || config.Ac14D || config.Ac10DP || config.Ac10DA || config.Ac14t || config.Ac18t)
    {
        for (uint8_t i = 0; i < 8; i++)
        {
            if (config.cpDuty[1] == cpDutyCycle[i])
            {
                cp_stateA_min[1] = cp_stateA_min_rawvalue[i];
                cp_stateA_max[1] = cp_stateA_max_rawvalue[i];
                cp_stateB1_min[1] = cp_stateB1_min_rawvalue[i];
                cp_stateB1_max[1] = cp_stateB1_max_rawvalue[i];
                cp_stateB2_min[1] = cp_stateB2_min_rawvalue[i];
                cp_stateB2_max[1] = cp_stateB2_max_rawvalue[i];
                cp_stateE2_min[1] = cp_stateE2_min_rawvalue[i];
                cp_stateE2_max[1] = cp_stateE2_max_rawvalue[i];
                cp_state_disc_min[1] = cp_state_disc_min_rawvalue[i];
                cp_state_disc_max[1] = cp_state_disc_max_rawvalue[i];
                cp_stateC_min[1] = cp_stateC_min_rawvalue[i];
                cp_stateC_max[1] = cp_stateC_max_rawvalue[i];
                cp_stateD_min[1] = cp_stateD_min_rawvalue[i];
                cp_stateD_max[1] = cp_stateD_max_rawvalue[i];
                cp_stateE1_min[1] = cp_stateE1_min_rawvalue[i];
                cp_stateE1_max[1] = cp_stateE1_max_rawvalue[i];
            }
        }

        if (config.vcharge_v5 || config.AC1)
            xTaskCreate(&cpTask, "cpTask1", 3072, &taskCpId[1], 2, NULL);
    }
    if (config.Ac14D || config.Ac18t)
    {
        for (uint8_t i = 0; i < 8; i++)
        {
            if (config.cpDuty[2] == cpDutyCycle[i])
            {
                cp_stateA_min[2] = cp_stateA_min_rawvalue[i];
                cp_stateA_max[2] = cp_stateA_max_rawvalue[i];
                cp_stateB1_min[2] = cp_stateB1_min_rawvalue[i];
                cp_stateB1_max[2] = cp_stateB1_max_rawvalue[i];
                cp_stateB2_min[2] = cp_stateB2_min_rawvalue[i];
                cp_stateB2_max[2] = cp_stateB2_max_rawvalue[i];
                cp_stateE2_min[2] = cp_stateE2_min_rawvalue[i];
                cp_stateE2_max[2] = cp_stateE2_max_rawvalue[i];
                cp_state_disc_min[2] = cp_state_disc_min_rawvalue[i];
                cp_state_disc_max[2] = cp_state_disc_max_rawvalue[i];
                cp_stateC_min[2] = cp_stateC_min_rawvalue[i];
                cp_stateC_max[2] = cp_stateC_max_rawvalue[i];
                cp_stateD_min[2] = cp_stateD_min_rawvalue[i];
                cp_stateD_max[2] = cp_stateD_max_rawvalue[i];
                cp_stateE1_min[2] = cp_stateE1_min_rawvalue[i];
                cp_stateE1_max[2] = cp_stateE1_max_rawvalue[i];
            }
        }

        if (config.vcharge_v5 || config.AC1)
            xTaskCreate(&cpTask, "cp2Task", 3072, &taskCpId[2], 2, NULL);
    }

    if (config.vcharge_lite_1_4)
    {
        SlaveCpConfig_t slaveCpConfig;
        slaveCpConfig.cpEnable[0] = cpEnable[0];
        slaveCpConfig.cpEnable[1] = cpEnable[1];
        slaveCpConfig.cpEnable[2] = cpEnable[2];
        slaveCpConfig.cpEnable[3] = cpEnable[3];
        slaveCpConfig.cpDuty[0] = config.cpDuty[0];
        slaveCpConfig.cpDuty[1] = config.cpDuty[1];
        slaveCpConfig.cpDuty[2] = config.cpDuty[2];
        slaveCpConfig.cpDuty[3] = config.cpDuty[3];
        for (int i = 0; i < NUM_OF_CONNECTORS; i++)
        {
            slaveCpConfig.cp_stateA_min[i] = cp_stateA_min[i];
            slaveCpConfig.cp_stateA_max[i] = cp_stateA_max[i];
            slaveCpConfig.cp_stateB1_min[i] = cp_stateB1_min[i];
            slaveCpConfig.cp_stateB1_max[i] = cp_stateB1_max[i];
            slaveCpConfig.cp_stateB2_min[i] = cp_stateB2_min[i];
            slaveCpConfig.cp_stateB2_max[i] = cp_stateB2_max[i];
            slaveCpConfig.cp_stateE2_min[i] = cp_stateE2_min[i];
            slaveCpConfig.cp_stateE2_max[i] = cp_stateE2_max[i];
            slaveCpConfig.cp_state_disc_min[i] = cp_state_disc_min[i];
            slaveCpConfig.cp_state_disc_max[i] = cp_state_disc_max[i];
            slaveCpConfig.cp_stateC_min[i] = cp_stateC_min[i];
            slaveCpConfig.cp_stateC_max[i] = cp_stateC_max[i];
            slaveCpConfig.cp_stateD_min[i] = cp_stateD_min[i];
            slaveCpConfig.cp_stateD_max[i] = cp_stateD_max[i];
            slaveCpConfig.cp_stateE1_min[i] = cp_stateE1_min[i];
            slaveCpConfig.cp_stateE1_max[i] = cp_stateE1_max[i];
        }
        SlaveSendControlPilotConfig(slaveCpConfig);
    }
}
