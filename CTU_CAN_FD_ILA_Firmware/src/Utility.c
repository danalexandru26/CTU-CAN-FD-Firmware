#include "Utility.h"
#include <xil_printf.h>

int CAN_Loopback(){
    xil_printf("\r\nCTU CAN FD loopback test\r\n");

    uint32_t id = rd(DEVICE_ID);
    xil_printf("DEVICE_ID = 0x%08x (expected lower half 0xCAFD)\r\n", id);
    
    if((id & 0xFFFF) != 0xCAFD){
        xil_printf("Wrong ID. Check Base Address / AXI-APB Bridge path\r\n");
        return -1;
    }
    
    return 0;
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

uint32_t RX_Buffer_Size(){
    return (rd(RX_STATUS) >> 4) & 0x7FF;
    
}

uint32_t RX_Buffer_Overrun(){
    return rd(STATUS) & (1u << 1);
}