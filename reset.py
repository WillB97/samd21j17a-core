#! /usr/bin/python3
# https://stackoverflow.com/a/49384222

import sys
import serial

if len(sys.argv) < 2:
    exit("Please supply the serial port to perform the reset on")
com = serial.Serial(sys.argv[1], 1200)
com.dtr=False
com.close()
