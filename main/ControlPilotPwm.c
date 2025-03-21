#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "ControlPilotPwm.h"
#include "ProjectSettings.h"

extern Config_t config;

void ControlPilotPwmInit(void)
{
  ledc_timer_config_t timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = LEDC_TIMER_10_BIT,
      .timer_num = LEDC_TIMER_0,
      .freq_hz = 1000,
      .clk_cfg = LEDC_AUTO_CLK};

  ledc_timer_config(&timer);

  ledc_channel_config_t channel = {
      .gpio_num = CONTROL_PILOT_GPIO_V5_PIN, // same pin for AC1 also
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = LEDC_CHANNEL_0,
      .timer_sel = LEDC_TIMER_0,
      .duty = 50,
      .hpoint = 0};
  ledc_channel_config(&channel);

  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 1024);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void SetControlPilotDuty(uint32_t Percentage)
{
  if (config.vcharge_v5 || config.AC1)
  {
    uint32_t duty = 0;
    if ((Percentage <= 100) && (Percentage >= 0))
    {
      duty = ((Percentage * 1024) / 100);
    }
    else
    {
      duty = 1024;
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
  }
}
