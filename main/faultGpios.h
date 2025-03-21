#ifndef FAULT_GPIOS_H
#define FAULT_GPIOS_H

void fault_gpios_init(void);
void updateRelayState(uint8_t relay, uint8_t state);
void togglewatchdog(void);

#endif