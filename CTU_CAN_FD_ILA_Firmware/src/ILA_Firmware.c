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
#include "platform.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "sleep.h"

#include "Utility.h"
#include "CAN.h"

void CAN_FD_bitTiming();

void CAN_FD_modeConfig();

void CAN_Send_Loop();
void CAN_FD_Send_Loop();
void CAN_FD_send_frame();

int main()
{
    init_platform();
    xil_printf("CTU CAN FD ILA Firmware\r\n");
    
    int status = CAN_Loopback();
    if(status == -1){
        xil_printf("CAN Lookback test failure\r\n");
        return -1;
    }

    // Set CAN bit timing
    CAN_bitTiming(8, 7, 4, 10, 3);

    // CAN mode & settings configuration
    CAN_modeSettings(MODE_STM | SET_ILBP);

    usleep(CAN_SLEEP_PERIOD);
    xil_printf("STATUS = 0x%08x\r\n", rd(STATUS));

    // Start TX with empty RX buffer
    wr(COMMAND, CMD_RRB);

    {
        static const uint8_t d8[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};
        CAN_send(1, 0x123, d8, 8);

        for(int i = 0; i < 1000 && (rd(RX_STATUS) & 1u); ++i){
            usleep(CAN_SLEEP_PERIOD / 10);
        }

        // Verify Transmission Status
        uint32_t s = rd(TX_STATUS) & 0xF;
        xil_printf("TXT1: 0x%x (%s)\r\n", s, txt_state(s));

        // Verify Controller Status
        xil_printf("REC/TEC=0x%08x  ERR_CAPT=0x%08x  STATUS=0x%08x\r\n", rd(0x30), rd(0x7C), rd(STATUS));    
    }
    cleanup_platform();
    return 0;
}