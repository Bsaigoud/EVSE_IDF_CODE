#ifndef CUSTOM_NVS_H
#define CUSTOM_NVS_H
#include "ocpp.h"

#define WEBSOCKET_RESTART 0x01
#define WEBSOCKET_RESTART_CLEAR 0x00
void custom_nvs_init(void);
void setSystemTime(void);
void custom_nvs_read_config(void);
void custom_nvs_write_config(void);
esp_err_t custom_nvs_read_slave_mac(void);
void custom_nvs_write_slave_mac(void);
esp_err_t custom_nvs_read_logFileNo(void);
void custom_nvs_write_logFileNo(void);
void custom_nvs_write_connector_data(uint8_t connId);
void custom_nvs_read_connector_data(uint8_t connId);
esp_err_t custom_nvs_read_config_ocpp(void);
void custom_nvs_write_config_ocpp(void);
esp_err_t custom_nvs_read_localist_ocpp(void);
void custom_nvs_write_localist_ocpp(void);
esp_err_t custom_nvs_read_LocalAuthorizationList_ocpp(void);
void custom_nvs_write_LocalAuthorizationList_ocpp(void);
esp_err_t custom_nvs_read_connectors_offline_data_count(void);
void custom_nvs_write_connectors_offline_data(uint8_t connId);
void custom_nvs_read_connectors_offline_data(uint8_t connId);
void custom_nvs_write_timeBuffer(void);
void custom_nvs_read_timeBuffer(void);
void custom_nvs_test_offline_data(void);
void custom_nvs_write_restart_reason(uint8_t restart_reason);
esp_err_t custom_nvs_read_restart_reason(uint8_t *restart_reason);

// Smart charging APIs
void custom_nvs_write_chargingProgile(ChargingProfiles_t ChargingProfile);
void custom_nvs_clear_chargingProfile(uint32_t id);
void set_ChargingProfiles(void);
void set_ChargePointMaxChargingProfiles(void);
void logChargingProfile(ChargingProfiles_t ChargingProfile);

#endif