#ifndef CAN_UTILITY
#define CAN_UTILITY

#include <stdint.h>
#include "xil_io.h"
#include "xil_printf.h"

#define CAN_SLEEP_PERIOD 0x3E8          // CAN default millisecond sleep period
#define CAN_BASE         0x43C00000u     // Vivado AXI Address Editor
 
/* Register offsets (from driver/ctucanfd_kregs.h) */
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

#define TXT_BUFFER(n)   (0x100u * n)    // TXT buffer computation. TXT bufffer 1 is 0x100

/* COMMAND */
#define CMD_RRB         (1u << 2)       // Flush RX buffer
 
/* MODE / SETTINGS bits */
#define MODE_STM        (1u << 2)       // self test: missing ACK is not an error
#define MODE_FDE        (1u << 4)       // CAN FD enable (on by default)
#define SET_ILBP        (1u << 21)      // internal loopback: receive own frames 
#define SET_ENA         (1u << 22)      // enable core                             
 
/* TX_COMMAND */
#define TXCR            (1u << 1)       // "set ready" -> send 
#define TX_B(n)         (1u << (7+(n))) // apply command to the nth TXT buffer. TXT buffer 1 is (1u << 8)
#define TXB1            (1u << 8)       // apply command to TXT buffer 1 
#define TXB2            (1u << 9)       // apply command to TXT buffer 2
#define TXB3            (1u << 10)      // apply command to TXT buffer 3 
#define TXB4            (1u << 11)      // apply command to TXT buffer 1 
 
/* TXT buffer frame layout (from driver/ctucanfd_kframe.h) */
#define FFW             0x00            // frame format word: DLC[3:0], IDE, FDF...
#define IDW             0x04            // base ID in [28:18] 
#define DATA_1_4        0x10
#define DATA_5_8        0x14

/* Frame format word */
#define FFW_DLC(n)      ((n) & 0xFu)
#define FFW_IDE         (1u << 6)
#define FFW_FDF         (1u << 7)       // CAN FD frame          
#define FFW_LBPF        (1u << 8)       // RX: looped-back frame 
#define FFW_BRS         (1u << 9)       // bit-rate switch       
#define FFW_RWCNT(w)    (((w) >> 11) & 0x1Fu)

/* CAN FD Frame Payload */
#define TX_FFW          0x00
#define TX_IDW          0x04
#define TX_DATA         0x10

#define FRAME_ID        0x123u
#define PAYLOAD_LEN     64
#define DLC_64          15             // CAN FD DLC 15 -> 64 Bytes


/* CAN Utility Functions */
int CAN_Loopback();
const char* txt_state (uint32_t s);
uint32_t btr_val(uint32_t prop, uint32_t ph1, uint32_t ph2, uint32_t brp, uint32_t sjw);

uint8_t len_to_dlc(uint8_t len);

uint32_t RX_Buffer_Size();
uint32_t RX_Buffer_Overrun();

void rd(uint32_t base, uint32_t offset);
void wr(uint32_t base, uint32_t offset, uint32_t val);

#endif