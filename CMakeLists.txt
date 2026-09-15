# CLI / VS Code build of the STM32N6570-DK face-detection application.
#
# This mirrors the STM32CubeIDE managed build described by Appli/.cproject and
# FSBL/.cproject (ADR 0003). The two build systems are kept in sync by the
# parity check documented in that ADR: object set, symbol set and section sizes.
# Ground truth for every flag below is the literal gcc line CubeIDE generates in
# <project>/Debug/**/subdir.mk, not the .cproject XML (CubeIDE does not
# serialise option defaults).

cmake_minimum_required(VERSION 3.22)

project(mtk3bsp2_stm32n657 C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)          # -> -std=gnu11, as CubeIDE uses

set(HAL_SRC ${CMAKE_SOURCE_DIR}/Drivers/STM32N6xx_HAL_Driver/Src)
set(EXTMEM  ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_ExtMem_Manager)
set(FD      ${CMAKE_SOURCE_DIR}/Appli/FaceDetection)

# --specs=nano.specs must be a compile-only option. Putting it in CMAKE_C_FLAGS
# would also reach the link line, where the toolchain file already supplies it,
# and gcc aborts on the duplicate:
#   "attempt to rename spec 'link' to already defined spec 'nano_link'"
set(NANO_SPECS_COMPILE
    "$<$<COMPILE_LANGUAGE:C>:--specs=nano.specs>"
    "$<$<COMPILE_LANGUAGE:ASM>:--specs=nano.specs>")

# ---------------------------------------------------------------- FSBL target
# Sources: FSBL/.project linked resources (26 HAL/LL + 9 ExtMem) + Core + startup.
add_executable(mtk3bsp2_stm32n657_FSBL
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

# FSBL Debug has no optimisation level in .cproject, i.e. CubeIDE's -O0.
target_compile_options(mtk3bsp2_stm32n657_FSBL PRIVATE
    ${NANO_SPECS_COMPILE}
    "$<$<COMPILE_LANGUAGE:C>:-O0>"
)

target_compile_definitions(mtk3bsp2_stm32n657_FSBL PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:DEBUG;USE_HAL_DRIVER;STM32N657xx;USE_FULL_LL_DRIVER>"
    "$<$<COMPILE_LANGUAGE:ASM>:DEBUG>"
)

# 13 paths, from FSBL/.cproject. The assembler gets no include paths there.
target_include_directories(mtk3bsp2_stm32n657_FSBL PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:${CMAKE_SOURCE_DIR}/FSBL/Core/Inc>"
    "$<$<COMPILE_LANGUAGE:C>:${CMAKE_SOURCE_DIR}/Drivers/STM32N6xx_HAL_Driver/Inc>"
    "$<$<COMPILE_LANGUAGE:C>:${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32N6xx/Include>"
    "$<$<COMPILE_LANGUAGE:C>:${CMAKE_SOURCE_DIR}/Drivers/STM32N6xx_HAL_Driver/Inc/Legacy>"
    "$<$<COMPILE_LANGUAGE:C>:${EXTMEM}>"
    "$<$<COMPILE_LANGUAGE:C>:${EXTMEM}/boot>"
    "$<$<COMPILE_LANGUAGE:C>:${EXTMEM}/sal>"
    "$<$<COMPILE_LANGUAGE:C>:${EXTMEM}/nor_sfdp>"
    "$<$<COMPILE_LANGUAGE:C>:${EXTMEM}/psram>"
    "$<$<COMPILE_LANGUAGE:C>:${EXTMEM}/sdcard>"
    "$<$<COMPILE_LANGUAGE:C>:${EXTMEM}/user>"
    "$<$<COMPILE_LANGUAGE:C>:${HAL_SRC}>"
    "$<$<COMPILE_LANGUAGE:C>:${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include>"
)

target_link_options(mtk3bsp2_stm32n657_FSBL PRIVATE
    -T${CMAKE_SOURCE_DIR}/FSBL/STM32N657X0HXQ_AXISRAM2_fsbl.ld
    -Wl,-Map=${CMAKE_BINARY_DIR}/mtk3bsp2_stm32n657_FSBL.map
    -Wl,--no-warn-rwx-segments
)
set_target_properties(mtk3bsp2_stm32n657_FSBL PROPERTIES
    LINK_DEPENDS ${CMAKE_SOURCE_DIR}/FSBL/STM32N657X0HXQ_AXISRAM2_fsbl.ld
)
target_link_libraries(mtk3bsp2_stm32n657_FSBL PRIVATE ${TOOLCHAIN_LINK_LIBRARIES})

# CubeIDE emits a .bin for FSBL (convertbinary=true); ADR 0003 compares it byte
# for byte against this one.
add_custom_command(TARGET mtk3bsp2_stm32n657_FSBL POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:mtk3bsp2_stm32n657_FSBL>
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:mtk3bsp2_stm32n657_FSBL>
            ${CMAKE_BINARY_DIR}/mtk3bsp2_stm32n657_FSBL.bin
)

# --------------------------------------------------------------- Appli target
# ADR 0003: the .cproject compiles every file under mtk3_bsp2 with no exclusions;
# translation units for other boards collapse to nothing via the sys/machine.h
# guard at the top of each file. 241 files (223 .c + 18 .S).
file(GLOB_RECURSE MTK3_SRC CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/Appli/mtk3_bsp2/*.c
    ${CMAKE_SOURCE_DIR}/Appli/mtk3_bsp2/*.S
)

# Explicit list mirroring the sourceEntries of Appli/.cproject. Do not replace
# this with a glob: Vendor/Utilities/Fonts (5 files) and
# Vendor/Camera/ISP_Library/isp/Src/isp_conf_template.c are NOT source entries,
# and a glob would silently pull them in. 52 files.
set(FD_SRC
    ${FD}/Src/app_camerapipeline.c
    ${FD}/Src/crop_img.c
    ${FD}/Src/face_detection_app.c
    ${FD}/Src/face_detection_osal.c
    ${FD}/Src/privacy_filter.c
    ${FD}/Src/stm32_lcd_ex.c
    ${FD}/Model/network.c
    ${FD}/Model/stai_network.c
    ${FD}/Vendor/HAL/Src/stm32n6xx_hal_bsec.c
    ${FD}/Vendor/HAL/Src/stm32n6xx_hal_cacheaxi.c
    ${FD}/Vendor/HAL/Src/stm32n6xx_hal_dcmipp.c
    ${FD}/Vendor/HAL/Src/stm32n6xx_hal_dma2d.c
    ${FD}/Vendor/HAL/Src/stm32n6xx_hal_ltdc.c
    ${FD}/Vendor/HAL/Src/stm32n6xx_hal_ltdc_ex.c
    ${FD}/Vendor/HAL/Src/stm32n6xx_hal_ramcfg.c
    ${FD}/Vendor/HAL/Src/stm32n6xx_hal_rif.c
    ${FD}/Vendor/HAL/Src/stm32n6xx_hal_uart.c
    ${FD}/Vendor/HAL/Src/stm32n6xx_hal_xspi.c
    ${FD}/Vendor/BSP/Components/aps256xx/aps256xx.c
    ${FD}/Vendor/BSP/Components/mx66uw1g45g/mx66uw1g45g.c
    ${FD}/Vendor/BSP/STM32N6570-DK/stm32n6570_discovery.c
    ${FD}/Vendor/BSP/STM32N6570-DK/stm32n6570_discovery_bus.c
    ${FD}/Vendor/BSP/STM32N6570-DK/stm32n6570_discovery_lcd.c
    ${FD}/Vendor/BSP/STM32N6570-DK/stm32n6570_discovery_xspi.c
    ${FD}/Vendor/Camera/cmw_camera.c
    ${FD}/Vendor/Camera/cmw_utils.c
    ${FD}/Vendor/Camera/sensors/cmw_imx335.c
    ${FD}/Vendor/Camera/sensors/imx335/imx335.c
    ${FD}/Vendor/Camera/sensors/imx335/imx335_reg.c
    ${FD}/Vendor/Camera/ISP_Library/isp/Src/isp_ae_algo.c
    ${FD}/Vendor/Camera/ISP_Library/isp/Src/isp_algo.c
    ${FD}/Vendor/Camera/ISP_Library/isp/Src/isp_awb_algo.c
    ${FD}/Vendor/Camera/ISP_Library/isp/Src/isp_cmd_parser.c
    ${FD}/Vendor/Camera/ISP_Library/isp/Src/isp_core.c
    ${FD}/Vendor/Camera/ISP_Library/isp/Src/isp_services.c
    ${FD}/Vendor/Camera/ISP_Library/isp/Src/isp_tool_com.c
    ${FD}/Vendor/Postprocess/Src/app_postprocess_fd_blazeface_ui.c
    ${FD}/Vendor/Postprocess/Src/fd_pp_blazeface.c
    ${FD}/Vendor/Postprocess/Src/vision_models_pp.c
    ${FD}/Vendor/EdgeAI/Devices/STM32N6xx/mcu_cache.c
    ${FD}/Vendor/EdgeAI/Devices/STM32N6xx/npu_cache.c
    ${FD}/Vendor/EdgeAI/ll_aton/ll_aton.c
    ${FD}/Vendor/EdgeAI/ll_aton/ll_aton_cipher.c
    ${FD}/Vendor/EdgeAI/ll_aton/ll_aton_lib.c
    ${FD}/Vendor/EdgeAI/ll_aton/ll_aton_lib_sw_operators.c
    ${FD}/Vendor/EdgeAI/ll_aton/ll_aton_rt_main.c
    ${FD}/Vendor/EdgeAI/ll_aton/ll_aton_runtime.c
    ${FD}/Vendor/EdgeAI/ll_aton/ll_aton_stai_internal.c
    ${FD}/Vendor/EdgeAI/ll_aton/ll_aton_util.c
    ${FD}/Vendor/EdgeAI/ll_aton/ll_sw_float.c
    ${FD}/Vendor/EdgeAI/ll_aton/ll_sw_integer.c
    ${FD}/Vendor/Utilities/lcd/stm32_lcd.c
)

add_executable(mtk3bsp2_stm32n657_Appli
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
    ${FD_SRC}
)

# Appli Debug is -Os in .cproject, unlike FSBL.
target_compile_options(mtk3bsp2_stm32n657_Appli PRIVATE
    ${NANO_SPECS_COMPILE}
    "$<$<COMPILE_LANGUAGE:C>:-Os>"
)

# The C and ASM define sets differ. ADR 0003: without
# _STM32CUBE_DISCOVERY_N657_ on the assembler, dispatch.S compiles to an empty
# object and the kernel dispatcher goes unresolved.
target_compile_definitions(mtk3bsp2_stm32n657_Appli PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:DEBUG;USE_HAL_DRIVER;STM32N657xx;_STM32CUBE_DISCOVERY_N657_;USE_FULL_ASSERT;USE_FULL_LL_DRIVER;VECT_TAB_SRAM;LL_ATON_DUMP_DEBUG_API;LL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6;LL_ATON_OSAL=LL_ATON_OSAL_BARE_METAL;LL_ATON_RT_MODE=LL_ATON_RT_ASYNC;LL_ATON_SW_FALLBACK;LL_ATON_DBG_BUFFER_INFO_EXCLUDED=1>"
    "$<$<COMPILE_LANGUAGE:ASM>:DEBUG;_STM32CUBE_DISCOVERY_N657_>"
)

# The assembler gets only the four mtk3_bsp2 paths, as in Appli/.cproject.
set(APPLI_ASM_INC
    ${CMAKE_SOURCE_DIR}/Appli/mtk3_bsp2
    ${CMAKE_SOURCE_DIR}/Appli/mtk3_bsp2/config
    ${CMAKE_SOURCE_DIR}/Appli/mtk3_bsp2/include
    ${CMAKE_SOURCE_DIR}/Appli/mtk3_bsp2/mtkernel/kernel/knlinc
)
# 29 paths for C: 6 core/driver + 4 mtk3_bsp2 + 2 FaceDetection + 17 Vendor.
set(APPLI_C_INC
    ${CMAKE_SOURCE_DIR}/Appli/Core/Inc
    ${CMAKE_SOURCE_DIR}/Secure_nsclib
    ${CMAKE_SOURCE_DIR}/Drivers/STM32N6xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32N6xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/STM32N6xx_HAL_Driver/Inc/Legacy
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${APPLI_ASM_INC}
    ${FD}/Inc
    ${FD}/Model
    ${FD}/Vendor/HAL/Inc
    ${FD}/Vendor/BSP/STM32N6570-DK
    ${FD}/Vendor/BSP/Components/Common
    ${FD}/Vendor/BSP/Components/aps256xx
    ${FD}/Vendor/BSP/Components/mx66uw1g45g
    ${FD}/Vendor/Camera
    ${FD}/Vendor/Camera/sensors
    ${FD}/Vendor/Camera/sensors/imx335
    ${FD}/Vendor/Camera/ISP_Library/isp/Inc
    ${FD}/Vendor/Camera/ISP_Library/isp_param_conf
    ${FD}/Vendor/Postprocess/Inc
    ${FD}/Vendor/Postprocess/Src
    ${FD}/Vendor/EdgeAI/Inc
    ${FD}/Vendor/EdgeAI/ll_aton
    ${FD}/Vendor/EdgeAI/Devices/STM32N6xx
    ${FD}/Vendor/CMSIS/DSP/Include
    ${FD}/Vendor/Utilities/lcd
)
target_include_directories(mtk3bsp2_stm32n657_Appli PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:${APPLI_C_INC}>"
    "$<$<COMPILE_LANGUAGE:ASM>:${APPLI_ASM_INC}>"
)

target_link_options(mtk3bsp2_stm32n657_Appli PRIVATE
    -T${CMAKE_SOURCE_DIR}/Appli/STM32N657X0HXQ_LRUN_FACE_DETECTION.ld
    -Wl,-Map=${CMAKE_BINARY_DIR}/mtk3bsp2_stm32n657_Appli.map
    -Wl,--no-warn-rwx-segments
)
set_target_properties(mtk3bsp2_stm32n657_Appli PROPERTIES
    LINK_DEPENDS "${CMAKE_SOURCE_DIR}/Appli/STM32N657X0HXQ_LRUN_FACE_DETECTION.ld;${FD}/Vendor/Lib/NetworkRuntime1200_CM55_GCC.a"
)

# The Neural-ART runtime, passed by full path. Equivalent to the .cproject's
# "-L../FaceDetection/Vendor/Lib -l:NetworkRuntime1200_CM55_GCC.a", and placed
# before libm so the archive is searched in the same order.
target_link_libraries(mtk3bsp2_stm32n657_Appli PRIVATE
    ${FD}/Vendor/Lib/NetworkRuntime1200_CM55_GCC.a
    ${TOOLCHAIN_LINK_LIBRARIES}
)

# No .bin for Appli: the linker script has a loadable .flash_section at
# 0x71380000 which is empty today, but would make objcopy -O binary produce a
# ~1 GB file the moment anything lands in .flash_blob. dev-boot loads the ELF.
add_custom_command(TARGET mtk3bsp2_stm32n657_Appli POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:mtk3bsp2_stm32n657_Appli>
)
