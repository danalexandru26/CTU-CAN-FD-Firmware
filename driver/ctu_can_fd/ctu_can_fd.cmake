#driver/ctu_can_fd/ctu_can_fd.cmake
include(${CMAKE_CURRENT_LIST_DIR}/../common/common.cmake)   # CTU needs common

set(CTU_CAN_FD_DIR ${CMAKE_CURRENT_LIST_DIR})

set(CTU_CAN_FD_SOURCES ${CTU_CAN_FD_DIR}/src/ctu_can.c ${CTU_CAN_FD_DIR}/src/ctu_can_fd.c ${COMMON_SOURCES})

set(CTU_CAN_FD_INCLUDE_DIRS ${CTU_CAN_FD_DIR}/include ${CTU_CAN_FD_DIR}/src ${COMMON_INCLUDE_DIRS})
