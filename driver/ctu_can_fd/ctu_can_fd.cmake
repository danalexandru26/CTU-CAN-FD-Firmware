#driver/ctu_can_fd/ctu_can_fd.cmake  # CTU needs common
set(CTU_CAN_FD_DIR ${CMAKE_CURRENT_LIST_DIR})

set(CTU_CAN_FD_SOURCES ${CTU_CAN_FD_DIR}/src/ctu_can.c)

set(CTU_CAN_FD_INCLUDE_DIRS ${CTU_CAN_FD_DIR}/include ${CTU_CAN_FD_DIR}/src)
