#! /usr/bin/python3
# https://stackoverflow.com/a/49384222

import sys
import serial

com = serial.Serial(sys.argv[1], 1200)
com.dtr=False
com.close()
