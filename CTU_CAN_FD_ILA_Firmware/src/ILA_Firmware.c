/******************************************************************************
* Copyright (C) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/
/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <stdbool.h>

#include "platform.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "sleep.h"

/* CTU CAN DRIVER */
#include "ctu_can.h"

const uint32_t CTU_CAN_1_BASE = 0x43C00000u;
const char*    identifier     = "CTU CAN NODE 1";

const uint8_t payload_len = 32u;
const uint8_t txt_buffer  = 1u;

void cleanup_memory_allocations(void** mem);

int main()
{
    init_platform();
    xil_printf("CTU CAN FD ILA Firmware\r\n");
    
    ctu_can_bittiming_t ctu_can_nominal = CTU_CAN_BT_1M;
    ctu_can_bittiming_t ctu_can_data = CTU_CAN_BT_FD_5M;
    uint32_t opt = CTU_CAN_OPT_FD | CTU_CAN_OPT_SELF_TEST | CTU_CAN_OPT_LOOPBACK;
    ctu_can_config_t ctu_can_config = {.nominal = ctu_can_nominal, .data = ctu_can_data, .opts = opt};
    
    ctu_can_t ctu_can;
    ctu_can_init(&ctu_can, identifier, CTU_CAN_1_BASE, &ctu_can_config);

    uint8_t flag = 0x0u;
    can_frame_t frame = {.id = 0x123u, .len = payload_len, .flags = flag};
    
    /* Min between clasical CAN payload length and current payload length */
    for(uint32_t byte = 0; byte < 32; ++byte){
        frame.data[byte] = 0xFF;
    }


    while(true){
        ctu_can_send_txt(&ctu_can, 1u, &frame);
        usleep(1000);

        uint32_t tx_status = reg_read(&ctu_can, TX_STATUS) >> (4 * (txt_buffer - 1)) & 0xF;
        uint32_t rx_status = reg_read(&ctu_can, RX_STATUS);
        
        bool empty = rx_status & 1u;
        uint32_t frames = (rx_status >> 4) & 0x7FF;

        xil_printf("Status: 0x%x\r\n RX Empty: %d \t RX Frames: 0x%x\r\n", tx_status, empty, frames);
        usleep(10000);
    }
    
    cleanup_platform();
    return 0;
}