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
const uint32_t CTU_CAN_2_BASE = 0x43C10000u;

const char*    identifier_1   = "CTU CAN NODE 1";
const char*    identifier_2   = "CTU CAN NODE 2";

const uint8_t payload_len = 32u;
const uint8_t txt_buffer  = 1u;

void print_frame(const can_frame_t* frame);

int main()
{
    init_platform();

    ctu_can_bittiming_t ctu_can_nominal = CTU_CAN_BT_1M;
    ctu_can_bittiming_t ctu_can_data = CTU_CAN_BT_FD_5M;
    uint32_t opt = CTU_CAN_OPT_FD;
    
    ctu_can_config_t ctu_can_config = {.nominal = ctu_can_nominal, .data = ctu_can_data, .opts = opt};

    ctu_can_t ctu_can_1;
    ctu_can_init(&ctu_can_1, identifier_1, CTU_CAN_1_BASE, &ctu_can_config);

    ctu_can_t ctu_can_2;
    ctu_can_init(&ctu_can_2, identifier_2, CTU_CAN_2_BASE, &ctu_can_config);

    uint8_t flag = CAN_FLAG_FD | CAN_FLAG_BRS;
    can_frame_t frame = {.id = 0x123u, .len = payload_len, .flags = flag};
    
    /* Min between clasical CAN payload length and current payload length */
    for(uint32_t byte = 0; byte < 32; ++byte){
        frame.data[byte] = 0x1F;
    }

    
    while(true){
        ctu_can_send_txt(&ctu_can_1, 1u, &frame);
        usleep(CAN_SLEEP_PERIOD);

        uint32_t tx_status = reg_read(&ctu_can_1, TX_STATUS) >> (4 * (txt_buffer-1)) & 0xF;
        xil_printf("Status: 0x%x %s\r\n", tx_status, txt_state(tx_status));

        can_frame_t rcv_frame;
        int rcv_result = ctu_can_recv(&ctu_can_2, &rcv_frame);

        if(rcv_result == 1){
            xil_printf("Frame successfully transmitted on the Fabric AND-Bus\r\n");   
            print_frame(&rcv_frame);
        }
        else{
            xil_printf("Error receiving frame\r\n");
        }
        usleep(CAN_SLEEP_PERIOD * 10);
    }
    
   
    cleanup_platform();
    return 0;
}

void print_frame(const can_frame_t* frame){
    if(!frame){
        return;
    }

    xil_printf("ID:        0x%x\r\n", frame->id);
    xil_printf("Length:    %u bytes\r\n", frame->len);
    xil_printf("Flags:     0x%x\r\n", frame->flags);
    xil_printf("Timestamp: 0x%08x%08x\r\n", (uint32_t)(frame->timestamp >> 32), (uint32_t)frame->timestamp);

    for(uint32_t idx = 0; idx < frame->len; ++idx){
        xil_printf("Byte: %d\tValue: 0x%02x\r\n", idx, frame->data[idx]);
    }
}