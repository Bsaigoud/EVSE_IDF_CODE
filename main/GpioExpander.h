#ifndef GPIO_EXPANDER_H
#define GPIO_EXPANDER_H

bool GetBuzzerStatus(void);
bool GetBatteryStatus(void);
bool GetRelayDropStatus(void);
bool GetRfidResetStatus(void);
void BuzzerEnable(void);
void BuzzerDisable(void);
void BatteryEnable(void);
void BatteryDisable(void);
void RelayDropEnable(void);
void RelayDropDisable(void);
void RfidResetEnable(void);
void RfidResetDisable(void);
void GpioExpanderInit(void);
#endif