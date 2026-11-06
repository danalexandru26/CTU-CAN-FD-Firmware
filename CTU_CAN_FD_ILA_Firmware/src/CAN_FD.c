#include "CAN_FD.h"
#include <stdint.h>
#include<math.h>

void CAN_FD_init();

void CAN_FD_nominal_timing(uint32_t prop, uint32_t ph1, uint32_t ph2, uint32_t brp, uint32_t sjw){
    wr(BTR, btr_val(prop, ph1, ph2, brp, sjw));
    xil_printf("BTR    = 0x%08x (wrote 0x%08x)\r\n", rd(BTR), btr_val(prop, ph1, ph2, brp, sjw));
}

void CAN_FD_data_timing(uint32_t prop, uint32_t ph1, uint32_t ph2, uint32_t brp, uint32_t sjw){
    wr(BTR_FD, btr_val(prop, ph1, ph2, brp, sjw));
    xil_printf("BTR    = 0x%08x (wrote 0x%08x)\r\n", rd(BTR_FD), btr_val(prop, ph1, ph2, brp, sjw));
}

void CAN_FD_modeSettings(uint32_t mode_settings){
    uint32_t ms = rd(MODE_SETTINGS) | mode_settings | MODE_FDE; // Explicitly enable CAN FD

    wr(MODE_SETTINGS, ms);

    //Enable CTU CAN FD core
    wr(MODE_SETTINGS, ms | SET_ENA);
    usleep(1000);

    xil_printf("MODE = 0x%08x\r\n", rd(MODE_SETTINGS));
}

void CAN_FD_selfTest();

void CAN_FD_send(uint32_t txt_buffer, uint32_t id, const uint8_t* data, uint8_t len){
    static const uint8_t dlc_bytes[16] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};

    if (len > 64) len = 64;
    
    wr(TXT_BUFFER(txt_buffer) + FFW, len_to_dlc(len) | FFW_FDF | FFW_BRS);
    wr(TXT_BUFFER(txt_buffer) + TX_IDW, (id & 0x7FF) << 18);

    uint32_t words = (dlc_bytes[len_to_dlc(len)] + 3 ) / 4;

    for(uint32_t w = 0; w < words; ++w){
        uint32_t value = 0u;
        for(uint32_t b = 0; b < 4; ++b){
            uint32_t idx = w * 4 + b;
            if(idx < len){
                value |= (uint32_t)data[idx] << (8 * b);    
            }
        }
        wr(TXT_BUFFER(txt_buffer) + TX_DATA + 4 * w, value);
    }

    // Apply Ready command to selected TXT buffer
    wr(TX_COMMAND, TXCR | TX_B(txt_buffer));
}