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

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "sleep.h"

#define CAN_BASE        0x43C00000u     // Vivado AXI Address Editor
 
// Register offsets (from driver/ctucanfd_kregs.h)
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
 
// MODE / SETTINGS bits
#define MODE_STM        (1u << 2)       // self test: missing ACK is not an error
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
 
#define rd(off)         Xil_In32(CAN_BASE + (off))
#define wr(off, v)      Xil_Out32(CAN_BASE + (off), (v))
 
// 500 kbit/s at 100 MHz: BRP=10 -> tq = 100 ns, 1+PROP+PH1+PH2 = 20 tq,
// sample point at 80 %.
static uint32_t btr_val(uint32_t prop, uint32_t ph1, uint32_t ph2,
                        uint32_t brp, uint32_t sjw)
{
    return (prop << 0) | (ph1 << 7) | (ph2 << 13) | (brp << 19) | (sjw << 27);
}

int CANloopback();

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

    wr(BTR, btr_val(8, 7, 4, 10, 3));
    wr(BTR_FD, btr_val(8, 7, 4, 10, 3));
    xil_printf("BTR    = 0x%08x (wrote 0x%08x)\r\n", rd(BTR), btr_val(8, 7, 4, 10, 3));
    
    uint32_t ms = rd(MODE_SETTINGS);
    xil_printf("MODE   = 0x%08x (reset value)\r\n", ms);
    ms |= MODE_STM | SET_ILBP;
    wr(MODE_SETTINGS, ms);
    wr(MODE_SETTINGS, ms | SET_ENA);
    xil_printf("MODE   = 0x%08x (after enable)\r\n", rd(MODE_SETTINGS));

    usleep(1000);
    xil_printf("STATUS: 0x%08x\r\n", rd(STATUS));

    // Classic CAN frame: ID 0x123, DLC 8 
   
   while(1){
        wr(TXTB1 + FFW,      8);                 // DLC = 8
        wr(TXTB1 + IDW,      0x123u << 18);
        wr(TXTB1 + DATA_1_4, 0x44332211);
        wr(TXTB1 + DATA_5_8, 0x88776655);
        wr(TX_COMMAND, TXCR | TXB1);
        sleep(1);
        xil_printf("TX_STATUS=0x%08x  REC/TEC=0x%08x  ERR_CAPT=0x%08x  STATUS=0x%08x\r\n",
           rd(TX_STATUS), rd(0x30), rd(0x7C), rd(STATUS));
   }
   
    // Wait for reception (RX_STATUS.RXE = 0 means not empty)
    for (int i = 0; i < 1000 && (rd(RX_STATUS) & 1u); i++){
        usleep(100);
    }
 
    xil_printf("TX_STATUS = 0x%08x (TXT1 state 0x4 = sent OK)\r\n", rd(TX_STATUS));
 
    if (rd(RX_STATUS) & 1u) {
        xil_printf("Nothing received. STATUS = 0x%08x\r\n", rd(STATUS));
        return -1;
    }
 
    uint32_t ffw  = rd(RX_DATA);
    uint32_t idw  = rd(RX_DATA);
    (void)rd(RX_DATA);
    (void)rd(RX_DATA);
    uint32_t d0   = rd(RX_DATA);
    uint32_t d1   = rd(RX_DATA);
 
    xil_printf("RX: ID=0x%03x DLC=%d data=%08x %08x\r\n",
               (idw >> 18) & 0x7FF, ffw & 0xF, d0, d1);

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