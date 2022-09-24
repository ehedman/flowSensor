#!/bin/bash

if [ -z "${PICO_SDK_PATH}" ]; then
    echo "Missing PICO_SDK_PATH in environment"
    exit 1
fi

if [ ! -f "pico_extras_import.cmake" ]; then
    echo "pico_extras_import.cmake file is missing or you are not at the project root directory"
    exit 1
fi

cd "${PICO_SDK_PATH}"/../

if [ -d "pico_extras" ] || [ -L "pico-extras" ]; then
    echo "$(pwd)/pico_extras appears to be installed already"
    exit 0
fi

echo "Cloning from https://github.com/raspberrypi/pico-extras.git"
git clone https://github.com/raspberrypi/pico-extras.git --branch sdk-1.3.1 --single-branch pico_extras &>/dev/null

if [ "$?" -ne 0 ]; then
    echo "Operation clone failed"
    exit 1
fi

ln -s pico_extras pico-extra
