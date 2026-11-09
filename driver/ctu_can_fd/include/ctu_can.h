#ifndef CTU_CAN_H
#define CTU_CAN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "xil_io.h"
#include "xil_printf.h"

#define CAN_SLEEP_PERIOD 0x3E8          // CAN default millisecond sleep period
 
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

/* COMMAND */
#define CMD_RRB         (1u << 2)       // Flush RX buffer
 
/* MODE / SETTINGS bits */
#define MODE_STM        (1u << 2)       // self test: missing ACK is not an error
#define MODE_FDE        (1u << 4)       // CAN FD enable (on by default)
#define SET_ILBP        (1u << 21)      // internal loopback: receive own frames 
#define SET_ENA         (1u << 22)      // enable core        
#define MODE_RST        (1u << 0)   // soft reset
#define MODE_BMM        (1u << 1)   // bus monitoring                      
 
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

/* ------------------------------------------------------------------ */
/* Bit timing                                                          */
/* ------------------------------------------------------------------ */

/* BTR and BTR_FD share the same layout:
 * PROP[6:0] PH1[12:7] PH2[18:13] BRP[26:19] SJW[31:27]               */
typedef struct {
    uint8_t brp;    /* prescaler                         */
    uint8_t prop;   /* propagation segment (tq)          */
    uint8_t ph1;    /* phase segment 1 (tq)              */
    uint8_t ph2;    /* phase segment 2 (tq)              */
    uint8_t sjw;    /* sync jump width (tq)              */
} ctu_can_bittiming_t;

/* Presets for a 100 MHz core clock (same values as today's #defines) */
extern const ctu_can_bittiming_t CTU_CAN_BT_125K, CTU_CAN_BT_250K,
                                 CTU_CAN_BT_500K, CTU_CAN_BT_1M;
extern const ctu_can_bittiming_t CTU_CAN_BT_FD_1M, CTU_CAN_BT_FD_2M,
                                 CTU_CAN_BT_FD_4M, CTU_CAN_BT_FD_5M,
                                 CTU_CAN_BT_FD_10M;

/* ------------------------------------------------------------------ */
/* Configuration                                                      */
/* ------------------------------------------------------------------ */

typedef enum {
    CTU_CAN_OPT_FD          = 1u << 0,  /* MODE.FDE: accept/send FD frames     */
    CTU_CAN_OPT_SELF_TEST   = 1u << 1,  /* MODE.STM: missing ACK is not error  */
    CTU_CAN_OPT_LOOPBACK    = 1u << 2,  /* SETTINGS.ILBP: receive own frames   */
    CTU_CAN_OPT_BUS_MONITOR = 1u << 3,  /* MODE.BMM: listen only               */
} ctu_can_opt_t;

typedef struct {
    ctu_can_bittiming_t nominal;    /* arbitration phase                 */
    ctu_can_bittiming_t data;       /* FD data phase, ignored without FD */
    uint32_t            opts;       /* OR of ctu_can_opt_t               */
} ctu_can_config_t;

/* ------------------------------------------------------------------ */
/* Frame                                                              */
/* ------------------------------------------------------------------ */

#define CAN_MAX_DLEN    8u
#define CANFD_MAX_DLEN  64u

typedef enum {
    CAN_FLAG_EXT  = 1u << 0,    /* 29-bit identifier                  */
    CAN_FLAG_RTR  = 1u << 1,    /* remote frame (classic only)        */
    CAN_FLAG_FD   = 1u << 2,    /* FD frame (FDF)                     */
    CAN_FLAG_BRS  = 1u << 3,    /* bit-rate switch in data phase      */
    CAN_FLAG_ESI  = 1u << 4,    /* RX: sender was error passive       */
    CAN_FLAG_LBPF = 1u << 5,    /* RX: our own frame, looped back     */
} can_flag_t;

typedef struct {
    uint32_t id;                    /* 11 or 29 bit, right-aligned        */
    uint8_t  len;                   /* payload bytes: 0..8 or 0..64       */
    uint8_t  flags;                 /* OR of can_flag_t                   */
    uint64_t timestamp;             /* RX: core timestamp, TX: unused     */
    uint8_t  data[CANFD_MAX_DLEN] __attribute__((aligned(4)));
} can_frame_t;

/* ------------------------------------------------------------------ */
/* Device handle                                                      */
/* ------------------------------------------------------------------ */

typedef enum {                      /* TX_STATUS per-buffer state codes  */
    CTU_TXT_READY        = 0x1,
    CTU_TXT_TX_PROGRESS  = 0x2,
    CTU_TXT_ABORT_PROG   = 0x3,
    CTU_TXT_TX_OK        = 0x4,
    CTU_TXT_ERROR        = 0x6,
    CTU_TXT_ABORTED      = 0x7,
    CTU_TXT_EMPTY        = 0x8,
} ctu_txt_state_t;

typedef struct {
    uint32_t tx_frames;
    uint32_t rx_frames;
    uint32_t tx_busy;               /* send() found no free TXT buffer   */
    uint32_t rx_overruns;
} ctu_can_stats_t;

typedef struct {
    uintptr_t   base;               /* e.g. 0x43C00000 / 0x43C10000      */
    const char *name;               /* "CAN0", used in debug prints      */
    uint8_t     txt_count;          /* 2 in this HW (TXTB_INFO if the core has it) */
    uint32_t    opts;               /* copy of cfg->opts                 */

    uint8_t         next_txt;       /* round-robin TXT buffer pick (1..n) */
    ctu_can_stats_t stats;
} ctu_can_t;


/* CTU CAN FD Initialization */
int ctu_can_init(ctu_can_t* dev, const char* name, uintptr_t base, const ctu_can_config_t* cfg);
void ctu_can_disable(ctu_can_t* dev);

/* Transmit a frame. Pick up a free TXT buffer. Return the buffer used (1...n). */
int ctu_can_send(ctu_can_t* dev, const can_frame_t* frame);

/* Transmit a frame. Use a specific TXT buffer. */
int ctu_can_send_txt(ctu_can_t* dev, const uint8_t txt_buffer, const can_frame_t* frame);


/* Helper Functions */
uint32_t ctu_can_btr_pack(const ctu_can_bittiming_t *bt);
uint8_t ctu_can_len_to_dlc(uint8_t len);
uint8_t ctu_can_dlc_to_len(uint8_t dlc);

uint32_t reg_read(const ctu_can_t* dev, uint32_t offset);
void reg_write(const ctu_can_t* dev, uint32_t offset, uint32_t val);

/* Debug / Utility Functions */
const char* txt_state (uint32_t s);
#endif