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
 
// 500 kbit/s at 100 MHz: BRP=10 -> tq = 100 ns, 1+PROP+PH1+PH2 = 20 tq,
// sample point at 80 %.
// BTR and BTR FD share same field positions
static uint32_t btr_val(uint32_t prop, uint32_t ph1, uint32_t ph2,
                        uint32_t brp, uint32_t sjw)
{
    return (prop << 0) | (ph1 << 7) | (ph2 << 13) | (brp << 19) | (sjw << 27);
}


void CAN_bitTiming();
void CAN_FD_bitTiming();

void modeConfig();
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

    // Bit Timing Configuration
    CAN_FD_bitTiming();

    // Controller Mode Configuration
    CAN_FD_modeConfig();

    // Start TX with empty RX buffer
    wr(COMMAND, CMD_RRB);

    // Check Node Status
    usleep(1000);
    xil_printf("STATUS: 0x%08x\r\n", rd(STATUS));
    
    CAN_FD_Send_Loop();

    // Classic CAN frame: ID 0x123, DLC 8    
    // // Wait for reception (RX_STATUS.RXE = 0 means not empty)
    // for (int i = 0; i < 1000 && (rd(RX_STATUS) & 1u); i++){
    //     usleep(100);
    // }
 
    // xil_printf("TX_STATUS = 0x%08x (TXT1 state 0x4 = sent OK)\r\n", rd(TX_STATUS));
 
    // if (rd(RX_STATUS) & 1u) {
    //     xil_printf("Nothing received. STATUS = 0x%08x\r\n", rd(STATUS));
    //     return -1;
    // }
 
    // uint32_t ffw  = rd(RX_DATA);
    // uint32_t idw  = rd(RX_DATA);
    // (void)rd(RX_DATA);
    // (void)rd(RX_DATA);
    // uint32_t d0   = rd(RX_DATA);
    // uint32_t d1   = rd(RX_DATA);
 
    // xil_printf("RX: ID=0x%03x DLC=%d data=%08x %08x\r\n",
    //            (idw >> 18) & 0x7FF, ffw & 0xF, d0, d1);

    cleanup_platform();
    return 0;
}

void CAN_bitTiming(){
    wr(BTR, btr_val(8, 7, 4, 10, 3));
    wr(BTR_FD, btr_val(8, 7, 4, 10, 3));
    xil_printf("BTR    = 0x%08x (wrote 0x%08x)\r\n", rd(BTR), btr_val(8, 7, 4, 10, 3));
}

void CAN_FD_bitTiming(){
    // Nominal Bit Timing: 100MHz, BRP 10 = 100 ns tq, 20 tq -> 500 kbit/s, SP 80
    wr(BTR, btr_val(8, 7, 4, 10, 3));

    // Data Bit Timing: 100 MHz / BRP 5  =  50 ns tq, 10 tq ->   2 Mbit/s, SP 80 %
    wr(BTR_FD, btr_val(4, 3, 2, 5, 2));
    xil_printf("BTR=0x%08x BTR_FD=0x%08x\r\n", rd(BTR), rd(BTR_FD));
}

void modeConfig(){
    uint32_t ms = rd(MODE_SETTINGS);
    xil_printf("MODE   = 0x%08x (reset value)\r\n", ms);
    
    ms |= MODE_STM | SET_ILBP;
    wr(MODE_SETTINGS, ms);
    wr(MODE_SETTINGS, ms | SET_ENA);
    
    xil_printf("MODE   = 0x%08x (after enable)\r\n", rd(MODE_SETTINGS)); 
}


void CAN_FD_modeConfig(){
    uint32_t ms = rd(MODE_SETTINGS) | MODE_FDE | MODE_STM | SET_ILBP;
    xil_printf("MODE   = 0x%08x (reset value)\r\n", ms);

    wr(MODE_SETTINGS, ms);
    wr(MODE_SETTINGS, ms | SET_ENA);
    usleep(1000);
}

void CAN_Send_Loop(){
    while(1)
    {
        wr(TXTB1 + FFW,      8);                 // DLC = 8
        wr(TXTB1 + IDW,      0x123u << 18);
        wr(TXTB1 + DATA_1_4, 0x44332211);
        wr(TXTB1 + DATA_5_8, 0x88776655);
        wr(TX_COMMAND, TXCR | TXB1);
        
        sleep(1);
        
        xil_printf("TX_STATUS=0x%08x  REC/TEC=0x%08x  ERR_CAPT=0x%08x  STATUS=0x%08x\r\n",
        rd(TX_STATUS), rd(0x30), rd(0x7C), rd(STATUS));
    }
}

void CAN_FD_Send_Loop(){
    // Flush RX Buffer 
    wr(COMMAND, CMD_RRB);
    
    while(1){
        CAN_FD_send_frame();
        sleep(1);

        // Verify Transmission Status
        uint32_t s = rd(TX_STATUS) & 0xF;
        xil_printf("TXT1: 0x%x (%s)\r\n", s, txt_state(s));

        // Verify Controller Status
        xil_printf("REC/TEC=0x%08x  ERR_CAPT=0x%08x  STATUS=0x%08x\r\n", rd(0x30), rd(0x7C), rd(STATUS));

        // Verify RX Buffer Size
        uint32_t RX_count = (rd(0x68) >> 4) & 0x7FF;
        xil_printf("RX Buffer Frame Count: %d\r\n", RX_count);

        if(rd(STATUS) & (1u << 1)){
            xil_printf("Buffer Overrun\r\n");
        }
    }
}

void CAN_FD_send_frame(){
    // Write DLC
    wr(TXTB1 + TX_FFW, FFW_DLC(DLC_64) | FFW_FDF | FFW_BRS);
    
    // Write FRAME ID
    wr(TXTB1 + TX_IDW, FRAME_ID << 18);

    // Write PAYLOAD_LEN bytes
    for(int i = 0; i < PAYLOAD_LEN / 4; ++i) {
        uint32_t bd = 4u * i;
        
        wr(TXTB1 + TX_DATA + 4 * i, (bd) | ((bd + 1) << 8) | ((bd + 2) << 16) | ((bd + 3) << 24));
    }

    // Ready transmission and collect bytes from TXT Buffer 1
    wr(TX_COMMAND, TXCR | TXB1);
    
}