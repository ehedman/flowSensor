cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)
project(DigiFlow VERSION 1.1)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

include_regular_expression("^.*(h|cpp|inl|inc)$")

# Add a compilation subdirectory
add_subdirectory(lib/Config)
add_subdirectory(lib/LCD)
add_subdirectory(lib/Fonts)
add_subdirectory(lib/GUI)
add_subdirectory(examples)


# Add a header directory
include_directories(examples)
include_directories(./lib/Config)
include_directories(./lib/GUI)
include_directories(./lib/LCD)

configure_file(digiflow.h.in digiflow.h)
configure_file(custom.h.in rtc.def)

# Where the pico in BOOTSEL mode automounts on a (Mint) host
set(UF2DESTINATION /media/$ENV{LOGNAME}/RPI-RP2)

add_custom_target(dynDefines
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/custom.cmake
    )

add_custom_command( 
    TARGET dynDefines PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/custom.cmake
    )

# create an executable file
add_executable(main
    main.c
)

# Set the network hostname for this Pico w
set(HOSTNAME DigiFlow)

if (PICO_CYW43_SUPPORTED) # set by command 'PICO_BOARD=pico_w cmake  ..'
    if (NOT TARGET pico_cyw43_arch)
        message("Skipping Pico W support is not available")
    else()
        message("including Pico W support")

        add_dependencies(main dynDefines)

        add_compile_definitions(CYW43_HOST_NAME="${HOSTNAME}")
        target_compile_definitions(examples PRIVATE CYW43_HOST_NAME="${HOSTNAME}")
        target_compile_options(examples PRIVATE  -Wextra -Wunused -Wall)

        set(MAKE_FS_DATA_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/external/makefsdata)

        add_custom_command(OUTPUT fsdata.c
          COMMAND ${MAKE_FS_DATA_SCRIPT} fs
          COMMAND mv fsdata.c ../fs
          COMMAND touch lwipopts.h
          WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/examples 
          DEPENDS ${CMAKE_CURRENT_LIST_DIR}/examples/fs/ssi.shtml
          VERBATIM)
        add_custom_target(fsdata
                      DEPENDS fsdata.c)
        target_link_libraries(main
            examples GUI Fonts Config pico_stdlib hardware_spi pico_multicore hardware_flash 
            hardware_rtc hardware_clocks hardware_rosc hardware_sleep hardware_adc
            pico_cyw43_arch_lwip_threadsafe_background cyw43_driver_base pico_lwip_http)
    endif()
else()
    add_dependencies(main dynDefines)
    target_link_libraries(main
        examples LCD GUI Fonts Config pico_stdlib hardware_spi pico_multicore hardware_flash
        hardware_rtc hardware_clocks hardware_rosc hardware_sleep hardware_adc)
endif()

# enable usb output, disable uart output
pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 1)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(main)

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
PRIVATE ${PICO_SDK_PATH}/lib/lwip/src/apps
PRIVATE ${PROJECT_BINARY_DIR}
PRIVATE ${CMAKE_CURRENT_LIST_DIR}
)

install(FILES "${PROJECT_BINARY_DIR}/main.uf2" DESTINATION ${UF2DESTINATION})


