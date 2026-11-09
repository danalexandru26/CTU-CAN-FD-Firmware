#include "ctu_can.h"
#include <stdint.h>

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

const ctu_can_bittiming_t CTU_CAN_BT_125K  = { .brp = 40, .prop = 7, .ph1 = 8, .ph2 = 4, .sjw = 4 };
const ctu_can_bittiming_t CTU_CAN_BT_250K  = { .brp = 20, .prop = 7, .ph1 = 8, .ph2 = 4, .sjw = 4 };
const ctu_can_bittiming_t CTU_CAN_BT_500K  = { .brp = 10, .prop = 7, .ph1 = 8, .ph2 = 4, .sjw = 4 };
const ctu_can_bittiming_t CTU_CAN_BT_1M    = { .brp = 5,  .prop = 7, .ph1 = 8, .ph2 = 4, .sjw = 4 };

const ctu_can_bittiming_t CTU_CAN_BT_FD_1M  = { .brp = 4,  .prop = 9, .ph1 = 10, .ph2 = 5, .sjw = 5 };
const ctu_can_bittiming_t CTU_CAN_BT_FD_2M  = { .brp = 2,  .prop = 9, .ph1 = 10, .ph2 = 5, .sjw = 2 };
const ctu_can_bittiming_t CTU_CAN_BT_FD_4M  = { .brp = 1,  .prop = 9, .ph1 = 10, .ph2 = 5, .sjw = 5 };
const ctu_can_bittiming_t CTU_CAN_BT_FD_5M  = { .brp = 1,  .prop = 7, .ph1 = 8, .ph2 = 4, .sjw = 4 };
const ctu_can_bittiming_t CTU_CAN_BT_FD_10M = { .brp = 1,  .prop = 3, .ph1 = 4, .ph2 = 2, .sjw = 2 };

uint32_t reg_read(const ctu_can_t* dev, uint32_t offset){
    return Xil_In32(dev->base + offset);
}

void reg_write(const ctu_can_t* dev, uint32_t offset, uint32_t val){
    Xil_Out32(dev->base + offset, val);
}

int ctu_can_init(ctu_can_t* dev, const char* name, uintptr_t base, const ctu_can_config_t* cfg){
    if(!cfg){
        xil_printf("%s: NULL config pointer\r\n", name);
        return 1;
    }

    if(!dev){
        xil_printf("%s: NULL device pointer\r\n", name);
        return 1;
    }

    dev->base = base;
    dev->name = name;
    dev->txt_count = 2; // Hardcoded for now. CTU CAN FD core is synthesized with 2 TXT buffers.

    dev->opts = cfg->opts;
    dev->next_txt = 1; // Start with TXT buffer 1
    memset(&dev->stats, 0, sizeof(dev->stats));

    /* Is there really a CTU CAN FD at this address? */
    uint32_t id = reg_read(dev, DEVICE_ID);
    if ((id & 0xFFFF) != 0xCAFD) {
        xil_printf("%s: DEVICE_ID = 0x%08x, wrong base?\r\n", name, id);
        return 1;
    }

    reg_write(dev, MODE_SETTINGS, MODE_RST);

    reg_write(dev, BTR, ctu_can_btr_pack(&cfg->nominal));
    reg_write(dev, BTR_FD, ctu_can_btr_pack(&cfg->data));

    uint32_t ms = reg_read(dev, MODE_SETTINGS);
    ms &= ~(MODE_FDE | MODE_STM | MODE_BMM | SET_ILBP | SET_ENA);

    if(cfg->opts & CTU_CAN_OPT_FD) ms |= MODE_FDE;
    if(cfg->opts & CTU_CAN_OPT_SELF_TEST) ms |= MODE_STM;
    if(cfg->opts & CTU_CAN_OPT_LOOPBACK)  ms |= SET_ILBP;
    if(cfg->opts & CTU_CAN_OPT_BUS_MONITOR) ms |= MODE_BMM;

    reg_write(dev, MODE_SETTINGS, ms);  
    
    /* Enable CTU CAN FD core */
    reg_write(dev, MODE_SETTINGS, ms | SET_ENA);
    usleep(1000);

    /* Flush RX buffer */
    reg_write(dev, COMMAND, CMD_RRB);

    /* Verify Initialization Values*/
   xil_printf("%s @ 0x%08x: MODE = 0x%08x\r\n", name, (unsigned)base, reg_read(dev, MODE_SETTINGS));

   return 0;
}

void ctu_can_disable(ctu_can_t* dev){
    if(!dev) return;

    uint32_t ms = reg_read(dev, MODE_SETTINGS);
    ms &= ~(MODE_FDE | MODE_STM | MODE_BMM | SET_ILBP | SET_ENA);
    reg_write(dev, MODE_SETTINGS, ms);
}

uint32_t ctu_can_btr_pack(const ctu_can_bittiming_t *bt){
    return ((uint32_t)bt->prop << 0) | ((uint32_t)bt->ph1 << 7)  | 
           ((uint32_t)bt->ph2 << 13) | ((uint32_t)bt->brp << 19) | 
           ((uint32_t)bt->sjw << 27);
}

uint8_t ctu_can_len_to_dlc(uint8_t len){
    if (len <= 8)  return len;
    if (len <= 12) return 9;
    if (len <= 16) return 10;
    if (len <= 20) return 11;
    if (len <= 24) return 12;
    if (len <= 32) return 13;
    if (len <= 48) return 14;
    return 15;   
}

uint8_t ctu_can_dlc_to_len(uint8_t dlc){
    if (dlc <= 8)  return dlc;
    if (dlc == 9)  return 12;
    if (dlc == 10) return 16;
    if (dlc == 11) return 20;
    if (dlc == 12) return 24;
    if (dlc == 13) return 32;
    if (dlc == 14) return 48;
    return 64;   
}



// void CAN_init(){
//     /* TO DO */
// }

// void CAN_bitTiming(uint32_t prop, uint32_t ph1, uint32_t ph2, uint32_t brp, uint32_t sjw){
//     wr(BTR, btr_val(prop, ph1, ph2, brp, sjw));
//     xil_printf("BTR    = 0x%08x (wrote 0x%08x)\r\n", rd(BTR), btr_val(prop, ph1, ph2, brp, sjw));
// }

// void CAN_modeSettings(uint32_t mode_settings){
//     uint32_t ms = rd(MODE_SETTINGS) | mode_settings;

//     // Disable FD Mode. Classic CAN only
//     ms &= ~MODE_FDE;

//     wr(MODE_SETTINGS, ms);

//     //Enable CTU CAN FD core
//     wr(MODE_SETTINGS, ms | SET_ENA);
//     usleep(1000);

//     xil_printf("MODE = 0x%08x\r\n", rd(MODE_SETTINGS));
// }

// void CAN_selfTest(){
//     CAN_modeSettings(MODE_STM | SET_ILBP);
// }

// void CAN_send(uint32_t txt_buffer, uint32_t id, const uint8_t* data, uint8_t len){
//     uint32_t DATA_W_1 = 0x0u;
//     uint32_t DATA_W_2 = 0x0u;
    
//     wr(TXT_BUFFER(txt_buffer) + FFW, len & 0x1F); // DLC = len;
//     wr(TXT_BUFFER(txt_buffer) + IDW, (id & 0x7FF) << 18);

//     // Collect data bytes into 32-bit words
//     for(int i = 0; i < len; ++i){
//         if(i < 4){
//             DATA_W_1 |= data[i];
//         }
//         else{
//             DATA_W_2 |= data[i];
//         }
//     }
    
//     wr(TXT_BUFFER(txt_buffer) + DATA_1_4, DATA_W_1);
//     wr(TXT_BUFFER(txt_buffer) + DATA_5_8, DATA_W_2);
//     usleep(CAN_SLEEP_PERIOD);

//     // Apply Ready command to selected TXT buffer
//     wr(TX_COMMAND, TXCR | TX_B(txt_buffer));
// }