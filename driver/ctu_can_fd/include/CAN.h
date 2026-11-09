#ifndef CAN
#define CAN

#include "Utility.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "sleep.h"
#include <stdbool.h>

void CAN_init();
void CAN_bitTiming(uint32_t prop, uint32_t ph1, uint32_t ph2, uint32_t brp, uint32_t sjw);
void CAN_modeSettings(uint32_t mode_settings);

void CAN_selfTest();

void CAN_send(uint32_t txt_buffer, uint32_t id, const uint8_t* data, uint8_t len);

#endif