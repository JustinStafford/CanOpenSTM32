# Configure path to the CANopenNode project
set(CANOPENNODE_PATH "../../CANopenNode")

# Configure path to CubeMX generated files like Inc/main.h and Src/syscalls.c
set(CUBEMXGEN_PATH "../../../Core")

# Configure path to STM32 HAL. Paths can usually be found in cmake/stm32cubemx/CMakeLists.txt
set(HAL_PATH
  "../../../Drivers/STM32F4xx_HAL_Driver/Inc"
  "../../../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy"
  "../../../Drivers/CMSIS/Device/ST/STM32F4xx/Include"
  "../../../Drivers/CMSIS/Include"
)

# Configure compile definitions for STM32 HAL. Can usually be found in cmake/stm32cubemx/CMakeLists.txt
set(HAL_DEFINES
  "USE_HAL_DRIVER"
  "STM32F413xx"
)

####
# CANopenNode_STM32 is dependant on the HAL, so can't be built as a standalone library.
# CANopenNode_STM32 and CANopenNode are co-dependant, and unfortunately CANopenNode also depends
# on the HAL. Eg. "CANopenNode/301/CO_HBconsumer.c" includes "CO_driver_target.h" which includes
# "main.h". However CANopenNode doesn't need the HAL source, so can still be built as a library
# as long as we provide the right includes and defines. Doing so follows a similar convention to
# that in FreeRTOS, where the core source is added as a library, configured by a header file.
# This enables things like multiple targets with different configurations, as discussed here:
#   https://forums.freertos.org/t/freertos-as-a-cmake-interface-library/23239
# But as soon as you depend on things in the wider application (like the HAL) the transient
# dependency puts burden on the user, who has to reapply the relevant paths and defines from
# the wider application to the library. Hence all the variables above. If this is deemed net
# negative, then the library approach can be abandoned and the source added to the wider
# application target instead. To do so, remove "add_library" and the canopennode specific
# includes and definitions, and change the remaining "canopennode" to ${CMAKE_PROJECT_NAME}.
####

# First add canopennode as a library

add_library(canopennode STATIC)

target_include_directories(canopennode
  PRIVATE
    ${CUBEMXGEN_PATH}/Inc # CANopenNode includes CANopenNode_STM32 which includes "main.h".
    ${HAL_PATH}           # And "main.h" includes the HAL.
    ${CANOPENNODE_PATH}   # Includes within CANopenNode are relative to the root dir.
  PUBLIC
    .                     # CANopenNode includes CANopenNode_STM32. Make public to save including the dir again.
)

target_compile_definitions(canopennode
  PRIVATE
    ${HAL_DEFINES}
)

target_sources(canopennode
  PRIVATE
    ${CANOPENNODE_PATH}/301/CO_ODinterface.c
    ${CANOPENNODE_PATH}/301/CO_NMT_Heartbeat.c
    ${CANOPENNODE_PATH}/301/CO_HBconsumer.c
    ${CANOPENNODE_PATH}/301/CO_Emergency.c
    ${CANOPENNODE_PATH}/301/CO_SDOserver.c
    ${CANOPENNODE_PATH}/301/CO_TIME.c
    ${CANOPENNODE_PATH}/301/CO_SYNC.c
    ${CANOPENNODE_PATH}/301/CO_PDO.c
    ${CANOPENNODE_PATH}/303/CO_LEDs.c
    ${CANOPENNODE_PATH}/305/CO_LSSslave.c
    ${CANOPENNODE_PATH}/storage/CO_storage.c
    ${CANOPENNODE_PATH}/CANopen.c
)


# Second, add CANopenNode_STM32 as source

target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM
  PRIVATE
    ${CANOPENNODE_PATH}
    # Everything else we need has already added by the application or by the CANopenNode library
)

target_sources(${CMAKE_PROJECT_NAME}
  PRIVATE
    CO_app_STM32.c
    CO_driver_STM32.c
    CO_storageBlank.c
    OD.c
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    canopennode
)
