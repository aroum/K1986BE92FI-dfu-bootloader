cmake_minimum_required(VERSION 3.20)

# Set path helpers
set(PLATFORM_DIR "${CMAKE_CURRENT_LIST_DIR}/platforms/milandr")
set(DEP_DIR "${PLATFORM_DIR}/dep")
set(COMMON_DIR "${DEP_DIR}/common")
set(CMAKE_DIR "${COMMON_DIR}/cmake")

# Always use MDR32F9Q2I.ld (startup file) but with bootloader configuration
set(LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/MDR32F9Q2I_bootloader.ld")

# Include toolchain settings
include("${CMAKE_DIR}/gcc-milandr.cmake")

project(milandr_dfu_bootloader ASM C)

# Add SDK drivers
if(NOT TARGET milandr_sdk)
    add_subdirectory(
        "${COMMON_DIR}/Drivers"
        "${CMAKE_BINARY_DIR}/milandr_bootloader_sdk"
    )
endif()

# Set include paths
include_directories(
    "${CMAKE_CURRENT_LIST_DIR}/src"
    "${COMMON_DIR}/inc"
    "${PLATFORM_DIR}/sdk"
    "${PLATFORM_DIR}/sdk/USB_Library"
)

# Sources
set(SOURCES
    src/main.c
    src/flash.c
    src/usb_dfu.c
    "${COMMON_DIR}/src/board_usb.c"
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Target compiler and linker flags
target_compile_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -Wall
    -Wextra
    -O2
    -ffunction-sections
    -fdata-sections
    -Wno-unused-parameter
    -Wno-unused-but-set-variable
    -Wno-deprecated
)

target_link_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    --specs=nano.specs
    -Wl,--no-warn-rwx-segments
)

target_link_libraries(${PROJECT_NAME} PRIVATE milandr_sdk)

# Post-build generation of HEX and BIN
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
    COMMENT "Building ${PROJECT_NAME}.hex and ${PROJECT_NAME}.bin"
)
