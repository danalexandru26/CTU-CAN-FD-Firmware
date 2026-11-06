#ifndef CAN_UTILITY
#define CAN_UTILITY

#include <stdint.h>
#include "xil_io.h"
#include "xil_printf.h"

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


// CAN Utility Functions

int CAN_Loopback();
const char* txt_state (uint32_t s);

#endif