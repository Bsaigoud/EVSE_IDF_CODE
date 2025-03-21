#ifndef CONTROL_PILOT_STATE_H
#define CONTROL_PILOT_STATE_H

typedef enum
{
	STARTUP = 0,
	GFCI_CHECK_START,
	GFCI_CHECK_EXPECTED,
	GFCI_PASSED,
	FAULT,
	STATE_A,
	STATE_B1,
	STATE_B2,
	STATE_C,
	STATE_D,
	STATE_E1,
	STATE_E2,
	STATE_SUS,
	STATE_DIS,
	STATE_F
} EVSE_states_enum_t;

typedef struct
{
	bool cpEnable[4];
	uint8_t cpDuty[4];
	uint16_t cp_stateA_min[4];
	uint16_t cp_stateA_max[4];
	uint16_t cp_stateB1_min[4];
	uint16_t cp_stateB1_max[4];
	uint16_t cp_stateB2_min[4];
	uint16_t cp_stateB2_max[4];
	uint16_t cp_stateE2_min[4];
	uint16_t cp_stateE2_max[4];
	uint16_t cp_state_disc_min[4];
	uint16_t cp_state_disc_max[4];
	uint16_t cp_stateC_min[4];
	uint16_t cp_stateC_max[4];
	uint16_t cp_stateD_min[4];
	uint16_t cp_stateD_max[4];
	uint16_t cp_stateE1_min[4];
	uint16_t cp_stateE1_max[4];
} SlaveCpConfig_t;

void Control_pilot_state_init(void);
void updateCpConfig(void);

#endif