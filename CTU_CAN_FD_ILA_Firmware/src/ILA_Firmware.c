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

#define CAN_BASE        0x43C00000u     // Vivado AXI Address Editor
 
// Register offsets (from driver/ctucanfd_kregs.h)
#define COMMAND         0x0C
#define DEVICE_ID       0x00
#define MODE_SETTINGS   0x04            // MODE [15:0] + SETTINGS [31:16]
#define STATUS          0x08
#define BTR             0x24
#define BTR_FD          0x28
#define RX_STATUS       0x68
#define RX_DATA         0x6C
#define TX_STATUS       0x70
#define TX_COMMAND      0x74
#define TXTB1           0x100           // TXT buffer 1 base
 
// COMMAND
#define CMD_RRB         (1u << 2)       // Flush RX buffer
 
// MODE / SETTINGS bits
#define MODE_STM        (1u << 2)       // self test: missing ACK is not an error
#define MODE_FDE        (1u << 4)       // CAN FD enable (on by default)
#define SET_ILBP        (1u << 21)      // internal loopback: receive own frames 
#define SET_ENA         (1u << 22)      // enable core                             
 
// TX_COMMAND
#define TXCR            (1u << 1)       // "set ready" -> send 
#define TXB1            (1u << 8)       // apply command to TXT buffer 1 
 
// TXT buffer frame layout (from driver/ctucanfd_kframe.h) */
#define FFW             0x00            // frame format word: DLC[3:0], IDE, FDF...
#define IDW             0x04            // base ID in [28:18] 
#define DATA_1_4        0x10
#define DATA_5_8        0x14

// Frame format word
#define FFW_DLC(n)      ((n) & 0xFu)
#define FFW_IDE         (1u << 6)
#define FFW_FDF         (1u << 7)       // CAN FD frame          
#define FFW_LBPF        (1u << 8)       // RX: looped-back frame 
#define FFW_BRS         (1u << 9)       // bit-rate switch       
#define FFW_RWCNT(w)    (((w) >> 11) & 0x1Fu)

// CAN FD Frame Payload
#define TX_FFW          0x00
#define TX_IDW          0x04
#define TX_DATA         0x10

#define FRAME_ID        0x123u
#define PAYLOAD_LEN     64
#define DLC_64          15             // CAN FD DLC 15 -> 64 Bytes

 
#define rd(off)         Xil_In32(CAN_BASE + (off))
#define wr(off, v)      Xil_Out32(CAN_BASE + (off), (v))
 
// 500 kbit/s at 100 MHz: BRP=10 -> tq = 100 ns, 1+PROP+PH1+PH2 = 20 tq,
// sample point at 80 %.
// BTR and BTR FD share same field positions
static uint32_t btr_val(uint32_t prop, uint32_t ph1, uint32_t ph2,
                        uint32_t brp, uint32_t sjw)
{
    return (prop << 0) | (ph1 << 7) | (ph2 << 13) | (brp << 19) | (sjw << 27);
}

static const char* txt_state (uint32_t s);

int CANloopback();

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
    
    int status = CANloopback();
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

int CANloopback(){
    xil_printf("\r\nCTU CAN FD loopback test\r\n");
 
    uint32_t id = rd(DEVICE_ID);
    xil_printf("DEVICE_ID = 0x%08x (expect low half 0xCAFD)\r\n", id);
    if ((id & 0xFFFF) != 0xCAFD) {
        xil_printf("Wrong ID - check base address / AXI-APB path\r\n");
        return -1;
    }
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
    while(1){
        // Flush RX Buffer 
        wr(COMMAND, CMD_RRB);
        CAN_FD_send_frame();
        sleep(1);

        // Verify Transmission Status
        uint32_t s = rd(TX_STATUS) & 0xF;
        xil_printf("TXT1: 0x%x (%s)\r\n", s, txt_state(s));

        // Verify Controller Status
        xil_printf("REC/TEC=0x%08x  ERR_CAPT=0x%08x  STATUS=0x%08x\r\n", rd(0x30), rd(0x7C), rd(STATUS));
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

static const char* txt_state(uint32_t s){
    switch (s)
    {
        case 0x1: return "ready";
        case 0x2: return "transmitting";
        case 0x4: return "sent OK";
        case 0x6: return "error";
        case 0x7: return "aborted";
        case 0x8: return "empty";
        default:  return "?";
    }
}