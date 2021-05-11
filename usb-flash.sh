#!/bin/bash

if [ -z "$1" ]; then
    echo "$0 <bin-file> [<serial-port>]"
    exit
fi

BOSSAC="${BOSSAC:-bossac}"
BOSSAC_ARGS="--info --erase --write --verify --reset"

BOSSA_VERSION="$($BOSSAC --help | awk '/Version/{print $NF}')"

# bossa < v1.9 hardcodes offset of 0x2000, v1.9+ doesn't
# on v1.9+ add -o 0x2000 to avoid overwriting the bootloader
if [[ ! "$BOSSA_VERSION" < "1.9.0" ]]; then
    BOSSAC_ARGS="-o 0x2000 $BOSSAC_ARGS"
fi

# in many cases bossa can auto-detect the USB port, when this fails or
# multiple are connected specify the serial port explicitly as the second argument
if [ -n "$2" ]; then
    BOSSAC_SERIAL_ARGS="--port=$2 -U true"
fi

$BOSSAC $BOSSAC_ARGS $BOSSAC_SERIAL_ARGS $1
