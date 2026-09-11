cmake_minimum_required(VERSION 3.22)

project(saframe_hello_world C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(HAL_SRC ${CMAKE_SOURCE_DIR}/Drivers/STM32N6xx_HAL_Driver/Src)
set(EXTMEM ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_ExtMem_Manager)

# ---------------------------------------------------------------- FSBL target
add_executable(saframe_hello_world_FSBL
    ${CMAKE_SOURCE_DIR}/FSBL/Core/Src/extmem_manager.c
    ${CMAKE_SOURCE_DIR}/FSBL/Core/Src/main.c
    ${CMAKE_SOURCE_DIR}/FSBL/Core/Src/stm32n6xx_hal_msp.c
    ${CMAKE_SOURCE_DIR}/FSBL/Core/Src/stm32n6xx_it.c
    ${CMAKE_SOURCE_DIR}/FSBL/Core/Src/syscalls.c
    ${CMAKE_SOURCE_DIR}/FSBL/Core/Src/sysmem.c
    ${CMAKE_SOURCE_DIR}/FSBL/Core/Src/system_stm32n6xx_fsbl.c
    ${CMAKE_SOURCE_DIR}/FSBL/Core/Startup/startup_stm32n657x0hxq_fsbl.s
    ${HAL_SRC}/stm32n6xx_hal.c
    ${HAL_SRC}/stm32n6xx_hal_cortex.c
    ${HAL_SRC}/stm32n6xx_hal_dma.c
    ${HAL_SRC}/stm32n6xx_hal_dma_ex.c
    ${HAL_SRC}/stm32n6xx_hal_exti.c
    ${HAL_SRC}/stm32n6xx_hal_gpio.c
    ${HAL_SRC}/stm32n6xx_hal_hcd.c
    ${HAL_SRC}/stm32n6xx_hal_i2c.c
    ${HAL_SRC}/stm32n6xx_hal_i2c_ex.c
    ${HAL_SRC}/stm32n6xx_hal_mdf.c
    ${HAL_SRC}/stm32n6xx_hal_pwr.c
    ${HAL_SRC}/stm32n6xx_hal_pwr_ex.c
    ${HAL_SRC}/stm32n6xx_hal_rcc.c
    ${HAL_SRC}/stm32n6xx_hal_rcc_ex.c
    ${HAL_SRC}/stm32n6xx_hal_sai.c
    ${HAL_SRC}/stm32n6xx_hal_sai_ex.c
    ${HAL_SRC}/stm32n6xx_hal_uart.c
    ${HAL_SRC}/stm32n6xx_hal_uart_ex.c
    ${HAL_SRC}/stm32n6xx_hal_xspi.c
    ${HAL_SRC}/stm32n6xx_ll_dma.c
    ${HAL_SRC}/stm32n6xx_ll_exti.c
    ${HAL_SRC}/stm32n6xx_ll_gpio.c
    ${HAL_SRC}/stm32n6xx_ll_rcc.c
    ${HAL_SRC}/stm32n6xx_ll_ucpd.c
    ${HAL_SRC}/stm32n6xx_ll_usb.c
    ${HAL_SRC}/stm32n6xx_ll_utils.c
    ${EXTMEM}/boot/stm32_boot_lrun.c
    ${EXTMEM}/stm32_extmem.c
    ${EXTMEM}/psram/stm32_psram_driver.c
    ${EXTMEM}/sal/stm32_sal_sd.c
    ${EXTMEM}/sal/stm32_sal_xspi.c
    ${EXTMEM}/sdcard/stm32_sdcard_driver.c
    ${EXTMEM}/nor_sfdp/stm32_sfdp_data.c
    ${EXTMEM}/nor_sfdp/stm32_sfdp_driver.c
    ${EXTMEM}/user/stm32_user_driver.c
)

target_compile_definitions(saframe_hello_world_FSBL PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:DEBUG;USE_HAL_DRIVER;STM32N657xx;USE_FULL_LL_DRIVER>"
    "$<$<COMPILE_LANGUAGE:ASM>:DEBUG>"
)

target_include_directories(saframe_hello_world_FSBL PRIVATE
    ${CMAKE_SOURCE_DIR}/FSBL/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32N6xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32N6xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/STM32N6xx_HAL_Driver/Inc/Legacy
    ${EXTMEM}
    ${EXTMEM}/boot
    ${EXTMEM}/sal
    ${EXTMEM}/nor_sfdp
    ${EXTMEM}/psram
    ${EXTMEM}/sdcard
    ${EXTMEM}/user
    ${HAL_SRC}
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)

target_link_options(saframe_hello_world_FSBL PRIVATE
    -T${CMAKE_SOURCE_DIR}/FSBL/STM32N657X0HXQ_AXISRAM2_fsbl.ld
    -Wl,-Map=${CMAKE_BINARY_DIR}/saframe_hello_world_FSBL.map
    -Wl,--no-warn-rwx-segments
)
set_target_properties(saframe_hello_world_FSBL PROPERTIES
    LINK_DEPENDS ${CMAKE_SOURCE_DIR}/FSBL/STM32N657X0HXQ_AXISRAM2_fsbl.ld
)
target_link_libraries(saframe_hello_world_FSBL PRIVATE ${TOOLCHAIN_LINK_LIBRARIES})

add_custom_command(TARGET saframe_hello_world_FSBL POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:saframe_hello_world_FSBL>
)

# --------------------------------------------------------------- Appli target
file(GLOB_RECURSE MTK3_SRC CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/Appli/mtk3_bsp2/*.c
    ${CMAKE_SOURCE_DIR}/Appli/mtk3_bsp2/*.S
)

add_executable(saframe_hello_world_Appli
    ${CMAKE_SOURCE_DIR}/Appli/Core/Src/main.c
    ${CMAKE_SOURCE_DIR}/Appli/Core/Src/secure_nsc.c
    ${CMAKE_SOURCE_DIR}/Appli/Core/Src/stm32n6xx_hal_msp.c
    ${CMAKE_SOURCE_DIR}/Appli/Core/Src/stm32n6xx_it.c
    ${CMAKE_SOURCE_DIR}/Appli/Core/Src/syscalls.c
    ${CMAKE_SOURCE_DIR}/Appli/Core/Src/sysmem.c
    ${CMAKE_SOURCE_DIR}/Appli/Core/Src/system_stm32n6xx_s.c
    ${CMAKE_SOURCE_DIR}/Appli/Core/Startup/startup_stm32n657x0hxq.s
    ${CMAKE_SOURCE_DIR}/Appli/Application/usermain.c
    ${MTK3_SRC}
    ${HAL_SRC}/stm32n6xx_hal.c
    ${HAL_SRC}/stm32n6xx_hal_adc.c
    ${HAL_SRC}/stm32n6xx_hal_adc_ex.c
    ${HAL_SRC}/stm32n6xx_hal_cortex.c
    ${HAL_SRC}/stm32n6xx_hal_dma.c
    ${HAL_SRC}/stm32n6xx_hal_dma_ex.c
    ${HAL_SRC}/stm32n6xx_hal_exti.c
    ${HAL_SRC}/stm32n6xx_hal_gpio.c
    ${HAL_SRC}/stm32n6xx_hal_i2c.c
    ${HAL_SRC}/stm32n6xx_hal_i2c_ex.c
    ${HAL_SRC}/stm32n6xx_hal_pwr.c
    ${HAL_SRC}/stm32n6xx_hal_pwr_ex.c
    ${HAL_SRC}/stm32n6xx_hal_rcc.c
    ${HAL_SRC}/stm32n6xx_hal_rcc_ex.c
    ${HAL_SRC}/stm32n6xx_ll_exti.c
    ${HAL_SRC}/stm32n6xx_ll_rcc.c
    ${HAL_SRC}/stm32n6xx_ll_utils.c
)

target_compile_definitions(saframe_hello_world_Appli PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:DEBUG;USE_HAL_DRIVER;STM32N657xx;_STM32CUBE_DISCOVERY_N657_>"
    "$<$<COMPILE_LANGUAGE:ASM>:DEBUG;_STM32CUBE_DISCOVERY_N657_>"
)

target_include_directories(saframe_hello_world_Appli PRIVATE
    ${CMAKE_SOURCE_DIR}/Appli/Core/Inc
    ${CMAKE_SOURCE_DIR}/Secure_nsclib
    ${CMAKE_SOURCE_DIR}/Drivers/STM32N6xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32N6xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/STM32N6xx_HAL_Driver/Inc/Legacy
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/Appli/mtk3_bsp2
    ${CMAKE_SOURCE_DIR}/Appli/mtk3_bsp2/config
    ${CMAKE_SOURCE_DIR}/Appli/mtk3_bsp2/include
    ${CMAKE_SOURCE_DIR}/Appli/mtk3_bsp2/mtkernel/kernel/knlinc
)

target_link_options(saframe_hello_world_Appli PRIVATE
    -T${CMAKE_SOURCE_DIR}/Appli/STM32N657X0HXQ_LRUN.ld
    -Wl,-Map=${CMAKE_BINARY_DIR}/saframe_hello_world_Appli.map
    -Wl,--no-warn-rwx-segments
)
set_target_properties(saframe_hello_world_Appli PROPERTIES
    LINK_DEPENDS ${CMAKE_SOURCE_DIR}/Appli/STM32N657X0HXQ_LRUN.ld
)
target_link_libraries(saframe_hello_world_Appli PRIVATE ${TOOLCHAIN_LINK_LIBRARIES})

add_custom_command(TARGET saframe_hello_world_Appli POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:saframe_hello_world_Appli>
)
