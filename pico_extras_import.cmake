

if (NOT DEFINED ENV{PICO_EXTRAS_PATH})
    set(PICO_EXTRAS_PATH ${PICO_SDK_PATH}/../pico_extras)
else()
    set (PICO_EXTRAS_PATH $ENV{PICO_EXTRAS_PATH})
endif()

set(LWIP_PATH ${PICO_SDK_PATH}/lib/lwip)

if (NOT EXISTS ${LWIP_PATH})
    message(FATAL_ERROR "lwIP submodule has not been initialized; TCP/IP support will be unavailable
         hint: try 'git submodule update --init' from " $ENV{PICO_SDK_PATH} "/lib")
endif()

get_filename_component(PICO_EXTRAS_PATH "${PICO_EXTRAS_PATH}" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}")

if (NOT EXISTS ${PICO_EXTRAS_PATH}/CMakeLists.txt)
    message("File '${PICO_EXTRAS_PATH}/CMakeLists.txt' not found")
    message(FATAL_ERROR "       hint: try run the preparePico-extras.sh script to get it installed")
endif ()

set(PICO_EXTRAS_PATH ${PICO_EXTRAS_PATH} CACHE PATH "Path to the PICO EXTRAS" FORCE)

add_subdirectory(${PICO_EXTRAS_PATH} pico_extras)

message("Pico extras available at " ${PICO_EXTRAS_PATH})
