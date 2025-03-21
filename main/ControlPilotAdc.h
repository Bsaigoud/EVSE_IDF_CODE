#ifndef CONTROL_PILOT_ADC_H
#define CONTROL_PILOT_ADC_H

#include <stdint.h>
#include "driver/gptimer.h"

void delayMicroSeconds(gptimer_handle_t gptimer, uint32_t time);
void ControlPilotAdcInit(void);
void readAdcValue(void);

#endif