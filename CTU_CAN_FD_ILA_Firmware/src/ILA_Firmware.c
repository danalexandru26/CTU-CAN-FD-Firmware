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
#include "CAN_FD.h"

#define CAN_NOMINAL_TIMING_1  0x18508388u  /* BTR:    PROP=8 PH1=7 PH2=4 BRP=10 SJW=3 -> 500 kbit/s */
#define CAN_NOMINAL_TIMING_2  0x21408407u  /* BTR:    PROP=7 PH1=8 PH2=4 BRP=40 SJW=4 -> 125 kbit/s */

#define CAN_DATA_TIMING_1     0x10284184u  /* BTR_FD: PROP=4 PH1=3 PH2=2 BRP=5 SJW=2 -> 2 Mbit/s */
#define CAN_DATA_TIMING_2     0x3020C489u  /* BTR_FD: PROP=9 PH1=9 PH2=6 BRP=4 SJW=6 -> 1 Mbit/s */



void CAN_Test();
void CAN_FD_Test(uint32_t nominal_timing, uint32_t data_timing, const uint32_t id, const uint32_t txt_buffer, const  uint8_t* data, uint32_t len);

int main()
{
    init_platform();
    xil_printf("CTU CAN FD ILA Firmware\r\n");


    {
        static const uint8_t tx_data[32] = {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
            0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
        };

        //CAN_FD_Test(CAN_NOMINAL_TIMING_1, CAN_DATA_TIMING_1, 0x123, 1, tx_data, 32);
        CAN_FD_Test(CAN_NOMINAL_TIMING_2, CAN_DATA_TIMING_2, 0x123, 1, tx_data, 32);

    }
  
    cleanup_platform();
    return 0;
}

void CAN_Test(){
      
    int status = CAN_Loopback();
    if(status == -1){
        xil_printf("CAN Lookback test failure\r\n");
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
}

void CAN_FD_Test(uint32_t nominal_timing, uint32_t data_timing, const uint32_t id, const uint32_t txt_buffer, const  uint8_t* data, uint32_t len){
    int status = CAN_Loopback();
    if(status == -1){
        xil_printf("CAN Lookback test failure\r\n");
    }

    // CAN FD nominal bit timing
    CAN_FD_nominal_timing(nominal_timing);
    
    // CAN FD data bit timing
    CAN_FD_data_timing(data_timing);

    // CAN FD mode & setting configuration
    CAN_FD_modeSettings(MODE_STM | SET_ILBP);

    usleep(CAN_SLEEP_PERIOD);
    xil_printf("STATUS = 0x%08x\r\n", rd(STATUS));

    // Start TX with empty RX buffer
    wr(COMMAND, CMD_RRB);

    while(1)
    {
        CAN_FD_send(txt_buffer, id, data, len); 
        
        for(int i = 0; i < 1000 && (rd(RX_STATUS) & 1u); ++i){
            usleep(CAN_SLEEP_PERIOD / 10);
        }

        // Verify Transmission Status
        uint32_t s = rd(TX_STATUS) & 0xF;
        xil_printf("TXT1: 0x%x (%s)\r\n", s, txt_state(s));

        // Verify Controller Status
        xil_printf("REC/TEC=0x%08x  ERR_CAPT=0x%08x  STATUS=0x%08x\r\n", rd(0x30), rd(0x7C), rd(STATUS));    
        usleep(CAN_SLEEP_PERIOD);
    }    
}