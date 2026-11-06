#include "CAN.h"
#include <stdint.h>

void CAN_bitTiming(uint32_t prop, uint32_t ph1, uint32_t ph2, uint32_t brp, uint32_t sjw){
    wr(BTR, btr_val(prop, ph1, ph2, brp, sjw));
    xil_printf("BTR    = 0x%08x (wrote 0x%08x)\r\n", rd(BTR), btr_val(prop, ph1, ph2, brp, sjw));
}

void CAN_modeSettings(uint32_t mode_settings){
    uint32_t ms = rd(MODE_SETTINGS) | mode_settings;

    // Disable FD Mode. Classic CAN only
    ms &= ~MODE_FDE;

    wr(MODE_SETTINGS, ms);

    //Enable CTU CAN FD core
    wr(MODE_SETTINGS, ms | SET_ENA);
    usleep(1000);

    xil_printf("MODE = 0x%08x\r\n", rd(MODE_SETTINGS));
}

void CAN_selfTest(){
    CAN_modeSettings(MODE_STM | SET_ILBP);
}

void CAN_send(uint32_t txt_buffer, uint32_t id, const uint8_t* data, uint8_t len){
    uint32_t DATA_W_1 = 0x0u;
    uint32_t DATA_W_2 = 0x0u;
    
    wr(TXT_BUFFER(txt_buffer) + FFW, len & 0x1F); // DLC = len;
    wr(TXT_BUFFER(txt_buffer) + IDW, (id & 0x7FF) << 18);

    // Collect data bytes into 32-bit words
    for(int i = 0; i < len; ++i){
        if(i < 4){
            DATA_W_1 |= data[i];
        }
        else{
            DATA_W_2 |= data[i];
        }
    }
    
    wr(TXT_BUFFER(txt_buffer) + DATA_1_4, DATA_W_1);
    wr(TXT_BUFFER(txt_buffer) + DATA_5_8, DATA_W_2);
    usleep(CAN_SLEEP_PERIOD);

    // Apply Ready command to selected TXT buffer
    wr(TX_COMMAND, TXCR | TX_B(txt_buffer));
}