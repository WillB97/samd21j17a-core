#!/bin/bash -ex

# populate thirdparty
cd thirdparty
if [ ! -d CMSIS-4.5.0 ]; then
    wget https://github.com/ARM-software/CMSIS/archive/refs/tags/v4.5.0.tar.gz -O - | tar -xz
fi

if [ ! -d CMSIS-Atmel ]; then
    wget https://github.com/adafruit/ArduinoModule-CMSIS-Atmel/releases/download/v1.2.1/CMSIS-Atmel-1.2.1.tar.bz2 -O - | tar -xj
fi

cd ..
