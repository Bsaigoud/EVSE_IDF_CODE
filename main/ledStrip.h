#ifndef LED_STRIP_H
#define LED_STRIP_H

#include <stdint.h>
#include "ProjectSettings.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define BLINK_TIME 500
void LED_Strip(void);

#endif