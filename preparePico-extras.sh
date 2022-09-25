#!/bin/bash

if [ -z "${PICO_SDK_PATH}" ]; then
    echo "Missing PICO_SDK_PATH in environment"
    exit 1
fi

if [ ! -f "pico_extras_import.cmake" ]; then
    echo "pico_extras_import.cmake file is missing or you are not at the project root directory"
    exit 1
fi

if [ -z "${PICO_EXTRAS_PATH}" ]; then
    PICO_EXTRAS_PATH="$(dirname "${PICO_SDK_PATH}")/pico_extras"
fi

if [ -d "${PICO_EXTRAS_PATH}" ]; then
    echo "${PICO_EXTRAS_PATH} appears to be installed already"
    exit 0
fi

if ! git clone https://github.com/raspberrypi/pico-extras.git --branch sdk-1.3.1 --single-branch "${PICO_EXTRAS_PATH}" &>/dev/null; then
    echo "Operation clone failed"
    exit 1
fi

echo "Pico extras installed at: ${PICO_EXTRAS_PATH}"
