set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(CMAKE_C_COMPILER_ID GNU)

# Locate arm-none-eabi-gcc: $ARM_TOOLCHAIN_DIR (CI), then PATH, then any STM32CubeCLT installation.
file(GLOB _clt_bins /opt/ST/STM32CubeCLT_*/GNU-tools-for-STM32/bin)
find_program(ARM_GCC arm-none-eabi-gcc HINTS $ENV{ARM_TOOLCHAIN_DIR} ${_clt_bins})
if(NOT ARM_GCC)
    message(FATAL_ERROR "arm-none-eabi-gcc not found. Set ARM_TOOLCHAIN_DIR, put STM32CubeCLT's GNU-tools-for-STM32/bin on PATH, or install STM32CubeCLT under /opt/ST.")
endif()
get_filename_component(TOOLCHAIN_BIN ${ARM_GCC} DIRECTORY)

set(CMAKE_C_COMPILER                ${ARM_GCC})
set(CMAKE_ASM_COMPILER              ${ARM_GCC})
set(CMAKE_OBJCOPY                   ${TOOLCHAIN_BIN}/arm-none-eabi-objcopy)
set(CMAKE_SIZE                      ${TOOLCHAIN_BIN}/arm-none-eabi-size)

execute_process(COMMAND ${ARM_GCC} -dumpfullversion OUTPUT_VARIABLE _gcc_ver OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _gcc_ver MATCHES "^14\\.3\\.1")
    message(WARNING "arm-none-eabi-gcc ${_gcc_ver}: reference toolchain is 14.3.1 (GNU Tools for STM32 14.3.rel1 / STM32CubeCLT 1.22.0).")
endif()

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Architecture flags common to C, ASM and link. CubeIDE puts -mcmse on the C
# compiler only, but ADR 0003 already accepts it on ASM and link as a tolerated
# difference (it does not change the generated image).
set(TARGET_FLAGS "-mcpu=cortex-m55 -mfpu=fpv5-d16 -mfloat-abi=hard -mcmse -mthumb")

# C-only flags, matching the CubeIDE compile line.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fdata-sections -ffunction-sections -fstack-usage")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")

# No -O here: FSBL Debug is -O0 and Appli Debug is -Os in .cproject, so each
# target sets its own via target_compile_options.
set(CMAKE_C_FLAGS_DEBUG "-g3")
# CMAKE_ASM_FLAGS is derived from CMAKE_C_FLAGS above, but per-config _DEBUG
# variables are not, so the assembler needs its own -g3.
set(CMAKE_ASM_FLAGS_DEBUG "-g3")

# --specs=nano.specs appears exactly once here. It must NOT go into
# CMAKE_C_FLAGS: CMake passes those to the link step too, and two
# --specs=nano.specs abort the link with
#   "attempt to rename spec 'link' to already defined spec 'nano_link'".
# The compile-side copy is added per target as a compile option instead.
set(CMAKE_EXE_LINKER_FLAGS "${TARGET_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-memory-usage")
set(TOOLCHAIN_LINK_LIBRARIES "m")
