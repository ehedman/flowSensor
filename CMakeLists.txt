cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)
project(Pico_ePaper_Code)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

# Add a compilation subdirectory
add_subdirectory(lib/Config)
add_subdirectory(lib/Infrared)
add_subdirectory(lib/Icm20948)
add_subdirectory(lib/LCD)
add_subdirectory(lib/Fonts)
add_subdirectory(lib/GUI)
add_subdirectory(examples)


# Add a header directory
include_directories(examples)
include_directories(./lib/Config)
include_directories(./lib/GUI)
include_directories(./lib/Icm20948)
include_directories(./lib/LCD)
include_directories(./lib/Infrared)

if (PICO_CYW43_SUPPORTED) # set by BOARD=pico-w
    if (NOT TARGET pico_cyw43_arch)
        message("Skipping Pico W support is not available")
    else()
        message("including Pico W support")
    endif()
endif()

# create an executable file
add_executable(main
main.c
)

# enable usb output, disable uart output
pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 1)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(main)

target_link_libraries(main pico_cyw43_arch_lwip_threadsafe_background examples LCD Infrared Icm20948 GUI Fonts Config pico_stdlib hardware_spi pico_multicore hardware_flash hardware_sleep hardware_rtc pico_runtime)

target_include_directories(examples 
PRIVATE ${PICO_SDK_PATH}/src/host/pico_multicore/include
PRIVATE ${PICO_SDK_PATH}/src/rp2_common/pico_cyw43_arch/include
PRIVATE ${PICO_SDK_PATH}/src/rp2_common/pico_lwip/include
PRIVATE ${PICO_SDK_PATH}/lib/cyw43-driver/src
PRIVATE ${PICO_SDK_PATH}/lib/lwip/src/include
PRIVATE ${PICO_SDK_PATH}/src/rp2_common/hardware_rtc/include
PRIVATE ${PICO_SDK_PATH}/src/rp2_common/hardware_flash/include
PRIVATE ${PICO_SDK_PATH}/src/rp2_common/hardware_sync/include
PRIVATE ${PICO_EXTRAS_PATH}/src/rp2_common/pico_sleep/include
PRIVATE ${PICO_EXTRAS_PATH}/src/rp2_common/hardware_rosc/include
PRIVATE ${CMAKE_CURRENT_LIST_DIR}
)

