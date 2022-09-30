#!/bin/bash

if [ -z "${PICO_SDK_PATH}" ]; then
    echo "Missing PICO_SDK_PATH in environment"
    exit 1
fi

if [ ! -f "pico_onewire_import.cmake" ]; then
    echo "pico_onewire_import.cmake file is missing or you are not at the project root directory"
    exit 1
fi

if [ -z "${PICO_ONEWIRE_PATH}" ]; then
    PICO_ONEWIRE_PATH="$(dirname "${PICO_SDK_PATH}")/pico-onewire"
fi

if [ -d "${PICO_ONEWIRE_PATH}" ]; then
    echo "${PICO_ONEWIRE_PATH} appears to be installed already"
    exit 0
fi

if ! git clone https://github.com/adamboardman/pico-onewire.git --single-branch "${PICO_ONEWIRE_PATH}" &>/dev/null; then
    echo "Operation clone failed"
    exit 1
fi

cd "${PICO_ONEWIRE_PATH}"

cat <<EOF > pico-onewire.patch
--- pico-onewire/source/one_wire.cpp.orig	2022-09-30 14:47:37.228170974 +0200
+++ pico-onewire/source/one_wire.cpp	2022-09-30 14:48:52.122077627 +0200
@@ -1,4 +1,5 @@
 #include "../api/one_wire.h"
+#include "one_wire_c.h"
 #include <cmath>
 #include <cstdio>
 #include <cstdlib>
EOF

if ! patch -p1 < pico-onewire.patch; then
    echo "Failed to patch pico-onewire/source/one_wire.cpp"
    exit 1
fi

echo "Pico  pico-onewire installed at: ${PICO_ONEWIRE_PATH}"

