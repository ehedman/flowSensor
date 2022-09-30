
if (NOT DEFINED ENV{PICO_ONEWIRE_PATH})
    set(PICO_ONEWIRE_PATH ${PICO_SDK_PATH}/../pico-onewire)
else()
    set (PICO_ONEWIRE_PATH $ENV{PICO_ONEWIRE_PATH})
endif()

get_filename_component(PICO_ONEWIRE_PATH "${PICO_ONEWIRE_PATH}" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}")

if (NOT EXISTS ${PICO_ONEWIRE_PATH}/CMakeLists.txt)
    message("File '${PICO_ONEWIRE_PATH}/CMakeLists.txt' not found")
    message(FATAL_ERROR "       hint: try run the preparePico-onewire.sh script to get it installed")
endif ()

set(PICO_ONEWIRE_PATH ${PICO_ONEWIRE_PATH} CACHE PATH "Path to the PICO EXTRAS" FORCE)

add_subdirectory(${PICO_ONEWIRE_PATH} pico-onewire)

message("Pico onewire available at " ${PICO_ONEWIRE_PATH})
