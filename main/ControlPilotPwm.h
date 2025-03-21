#ifndef CONTROL_PILOT_PWM_H
#define CONTROL_PILOT_PWM_H

#include "ProjectSettings.h"

void ControlPilotPwmInit(void);
void SetControlPilotDuty(uint32_t Percentage);

#endif