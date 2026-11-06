#ifndef CAN_FD
#define CAN_FD

#include "Utility.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "sleep.h"
#include <stdbool.h>

void CAN_FD_init();

void CAN_FD_nominal_timing(uint32_t btr);
void CAN_FD_data_timing(uint32_t btr);

void CAN_FD_modeSettings(uint32_t mode_settings);

void CAN_FD_selfTest();

void CAN_FD_send(uint32_t txt_buffer, uint32_t id, const uint8_t* data, uint8_t len);

#endif