#include "../include/ctu_can.h"
#include <stdint.h>

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

int ctu_can_send(ctu_can_t* dev, const can_frame_t* frame){
    if(!dev || !frame) return -1;

    uint8_t txt_buffer = dev->next_txt;
    int ret = ctu_can_send_txt(dev, txt_buffer, frame);
    if(ret < 0) return ret;

    /* Round-robin TXT buffer selection */
    dev->next_txt++;
    if(dev->next_txt > dev->txt_count) dev->next_txt = 1;

    return txt_buffer;
}

int ctu_can_send_txt(ctu_can_t* dev, const uint8_t txt_buffer, const can_frame_t* frame){
    if(!dev || !frame) return -1;

    if(txt_buffer < 1 || txt_buffer > dev->txt_count){
        xil_printf("%s: Invalid TXT buffer %u\r\n", dev->name, txt_buffer);
        return -1;
    } 

    if(dev->opts & CTU_CAN_OPT_FD){
        /* Handle CAN FD frame transmission */
        reg_write(dev, (uint32_t)txt_buffer * 0x100u + FFW, ctu_can_len_to_dlc(frame->len) | FFW_FDF | FFW_BRS);
        reg_write(dev, (uint32_t)txt_buffer * 0x100u + TX_IDW, (frame->id & 0x7FF) << 18);
    }

    else{
        /* Handle classic CAN frame transmission */
        reg_write(dev, (uint32_t)txt_buffer * 0x100u + FFW, frame->len & 0x1F); // DLC = len;
        reg_write(dev, (uint32_t)txt_buffer * 0x100u + IDW, (frame->id & 0x7FF) << 18); 
    }

    /* Handle frame data transmission */
    uint32_t words = (frame->len + 3) / 4;

    for(uint32_t w = 0; w < words; ++w){
        uint32_t word = 0x0u;
        for(uint32_t byte = 0; byte < 4; ++byte){
            uint32_t idx = w * 4 + byte;
            if(idx < frame->len){
                word |= (uint32_t)frame->data[idx] << (8 * byte);
            }
        }
        reg_write(dev, (uint32_t)txt_buffer * 0x100u + TX_DATA + 4 * w, word);
    }

    reg_write(dev, TX_COMMAND, TXCR | (1u << (7 + txt_buffer))); // Apply Ready command to selected TXT buffer)

    return 0;
}

int ctu_can_recv(ctu_can_t *dev, can_frame_t *frame){
    if(!dev){
        return -1;
    }

    if(reg_read(dev, RX_STATUS) & 1u) {
        return -1;
    }

    /* Read Frame Format Word from Queue*/
    uint32_t ffw = reg_read(dev, RX_DATA);
    uint32_t rwcnt = (ffw >> 11) & 0x1F; // Words left after this one
    uint8_t dlc = ffw & 0xF;

    /* Set frame flags*/
    frame->flags = 0;
    if(ffw & FFW_IDE) frame->flags |= CAN_FLAG_EXT;
    if(ffw & FFW_FDF) frame->flags |= CAN_FLAG_FD;
    if(ffw & FFW_BRS) frame-> flags |= CAN_FLAG_BRS;

    if (ffw & FFW_FDF) {
    /* CAN FD: */
    frame->len = ctu_can_dlc_to_len(dlc);
    } else {
        /* Classic CAN length*/
        if (dlc > 8)
            frame->len = 8;
        else
            frame->len = dlc;
    }
    
    /* Read frame identifier from queue */
    uint32_t idw = reg_read(dev, RX_DATA);
    if(ffw & FFW_IDE) {
        frame->id = idw & 0x1FFFFFFF;
    }
    else{
        frame->id = (idw >> 18) & 0x7FF;
    }

    /* CTU CAN Controller Timestamp*/
    uint32_t timestamp_low = reg_read(dev, RX_DATA);
    uint32_t timestamp_high = reg_read(dev, RX_DATA);

    frame->timestamp = ((uint64_t)timestamp_high << 32) | timestamp_low;
    
    /* Read frame data fields*/
    for(uint32_t w = 0; w < rwcnt - 3; ++w){
        uint32_t word = reg_read(dev, RX_DATA);
        for(uint8_t b = 0; b < 4; ++b){
            frame->data[w * 4 + b] = (word >> (b * 8)) & 0xFF;
        }
    }

    return 1;
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

const char* txt_state (uint32_t s){
    switch (s){
        case 0x1: return "ready";
        case 0x2: return "transmitting";
        case 0x4: return "sent OK";
        case 0x6: return "error";
        case 0x7: return "aborted";
        case 0x8: return "empty";
        default:  return "?";
    }
}