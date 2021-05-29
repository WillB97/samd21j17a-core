# include per-user options
include config.mk
SHELL:=/bin/bash

# -----------------------------------------------------------------------------
# Paths
ifeq ($(OS),Windows_NT)
  # Are we using mingw/msys/msys2/cygwin?
  ifeq ($(TERM),xterm)
    T=$(shell cygpath -u $(LOCALAPPDATA))
    ARM_GCC_PATH?=$(T)/Arduino15/packages/arduino/tools/arm-none-eabi-gcc/7-2017q4/bin/arm-none-eabi-
    RM=rm
    SEP=/
  else
    ARM_GCC_PATH?=$(LOCALAPPDATA)/Arduino15/packages/arduino/tools/arm-none-eabi-gcc/7-2017q4/bin/arm-none-eabi-
    RM=rm
    SEP=\\
  endif
  PYTHON?=python
else
  UNAME_S := $(shell uname -s)

  ifeq ($(UNAME_S),Linux)
    ARM_GCC_PATH?=/usr/bin/arm-none-eabi-
    RM=rm
    SEP=/
  endif

  ifeq ($(UNAME_S),Darwin)
    ARM_GCC_PATH?=/usr/local/bin/arm-none-eabi-
    RM=rm
    SEP=/
  endif
  PYTHON?=python3
endif

MODULE_PATH?=$(abspath $(CURDIR)/thirdparty)
USB_PATH?=$(shell realpath --relative-to $(CURDIR) $(MODULE_PATH)/usb)
CORE_PATH?=core
BUILD_PATH?=$(abspath $(CURDIR)/build)

NAME?=main.cpp
_NAME?=$(basename $(NAME))
TOTAL_FLASH?=$$((120*1024))  # 8kB lost to bootloader
TOTAL_RAM?=$$((16*1024))

CHIPNAME?=samd21g18
CHIPNAME_U=$(shell echo $(CHIPNAME)|tr 'a-z' 'A-Z')

# -----------------------------------------------------------------------------
# Tools
CC=$(ARM_GCC_PATH)gcc
CXX=$(ARM_GCC_PATH)g++
OBJCOPY=$(ARM_GCC_PATH)objcopy
NM=$(ARM_GCC_PATH)nm
SIZE=$(ARM_GCC_PATH)size
BOSSAC?=bossac

# -----------------------------------------------------------------------------
# Compiler options
CFLAGS_EXTRA=-DF_CPU=48000000L -D__$(CHIPNAME_U)A__
CFLAGS_EXTRA+=-DUSB_VID=0x2341 -DUSB_PID=0x804d -DUSBCON -DUSB_MANUFACTURER='"Arduino LLC"' -DUSB_PRODUCT='"Arduino Zero"'
CXXFLAGS=-mcpu=cortex-m0plus -mthumb -Wall -c -g -Os -std=gnu++11 -ffunction-sections -fdata-sections
CXXFLAGS+=-fno-threadsafe-statics -nostdlib --param max-inline-insns-single=500 -fno-rtti -fno-exceptions -MMD
CFLAGS=-mcpu=cortex-m0plus -mthumb -Wall -c -g -Os -std=gnu11 -ffunction-sections -fdata-sections -nostdlib --param max-inline-insns-single=500 -MMD

ELF=$(_NAME).elf
BIN=$(_NAME).bin
HEX=$(_NAME).hex

INCLUDES=-I"$(MODULE_PATH)/CMSIS-4.5.0/CMSIS/Include/" -I"$(MODULE_PATH)/CMSIS-Atmel/CMSIS-Atmel/CMSIS/Device/ATMEL/" -I"$(USB_PATH)" -I"$(CORE_PATH)"

# -----------------------------------------------------------------------------
# Linker options
LDFLAGS=-mthumb -mcpu=cortex-m0plus -Wall -Wl,--cref -Wl,--check-sections -Wl,--gc-sections -Wl,--unresolved-symbols=report-all
LDFLAGS+=-Wl,--warn-common -Wl,--warn-section-align -Wl,--warn-unresolved-symbols --specs=nano.specs --specs=nosys.specs
LDFLAGS+=-L$(MODULE_PATH)/CMSIS-4.5.0/CMSIS/Lib/GCC/ -larm_cortexM0l_math

# -----------------------------------------------------------------------------
# Programmer options
BOSSAC_ARGS?=--info --erase --write --verify --reset
BOSSA_VERSION=$(shell $(BOSSAC) --help | awk '/Version/{print $$NF}')

ifeq ($(shell [[ ! "$(BOSSA_VERSION)" < "1.9.0" ]]; echo $$?),0)
  BOSSAC_OFFSET = -o 0x2000
endif

PROGRAMMER?=jlink
BOOT_BIN?=bootloader/out/$(CHIPNAME)_sam_ba
BOOT_SERNUM_BIN?=bootloader/out/boot_sernum.bin
ADALINK_ARGS=samd21 -p $(PROGRAMMER) -V $(CHIPNAME)

# -----------------------------------------------------------------------------
# Source files and objects
SOURCES= \
  $(CORE_PATH)/cortex_handlers.c \
  $(CORE_PATH)/delay.c \
  $(CORE_PATH)/startup.c \
  $(CORE_PATH)/Reset.cpp \
  $(CORE_PATH)/USB-CDC.c \
  $(CORE_PATH)/USBserial.cpp \
  $(USB_PATH)/samd/usb_samd.c \
  $(USB_PATH)/usb_requests.c \
  $(NAME)

OBJECTS=$(addprefix $(BUILD_PATH)/, $(patsubst %.cpp,%.o,$(SOURCES:.c=.o)))
DEPS=$(addprefix $(BUILD_PATH)/, $(SOURCES:.c=.d))
BUILD_PATHS=$(sort $(addprefix $(BUILD_PATH)/, $(dir $(SOURCES))))

LD_SCRIPT=core/linker_scripts/flash_with_bootloader.ld

all: print_info $(SOURCES) $(BIN) $(HEX)

$(ELF): Makefile $(BUILD_PATH) $(OBJECTS)
	@echo ----------------------------------------------------------
	@echo Creating ELF binary
	"$(CXX)" -L. -L$(BUILD_PATH) $(LDFLAGS) -Os -Wl,--gc-sections -save-temps -T$(LD_SCRIPT) -Wl,-Map,"$(BUILD_PATH)/$(_NAME).map" -o "$(BUILD_PATH)/$(ELF)" -Wl,--start-group $(OBJECTS) -lm -Wl,--end-group
	"$(NM)" "$(BUILD_PATH)/$(ELF)" >"$(BUILD_PATH)/$(_NAME)_symbols.txt"
	@"$(SIZE)" $(BUILD_PATH)/$(ELF) | awk -v maxflash=$(TOTAL_FLASH) -v maxram=$(TOTAL_RAM) '(NR==2){ \
		flash=$$1+$$2; \
		ram=$$2+$$3; \
		print $$6; \
		printf "Flash used: %d / %d (%0.2f%%)\n", flash, maxflash, flash / maxflash * 100; \
		printf "RAM used: %d / %d (%0.2f%%)\n", ram, maxram, ram / maxram * 100 \
	}'


$(BIN): $(ELF)
	@echo ----------------------------------------------------------
	@echo Creating flash binary
	"$(OBJCOPY)" -O binary $(BUILD_PATH)/$< $@

$(HEX): $(ELF)
	@echo ----------------------------------------------------------
	@echo Creating flash binary
	"$(OBJCOPY)" -O ihex -R .eeprom $(BUILD_PATH)/$< $@

$(BUILD_PATH)/%.o: %.c
	@echo ----------------------------------------------------------
	@echo Compiling $< to $@
	"$(CC)" $(CFLAGS) $(CFLAGS_EXTRA) $(INCLUDES) $< -o $@
	@echo ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

$(BUILD_PATH)/%.o: %.cpp
	@echo ----------------------------------------------------------
	@echo Compiling $< to $@
	"$(CXX)" $(CXXFLAGS) $(CFLAGS_EXTRA) $(INCLUDES) $< -o $@
	@echo ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

$(BUILD_PATH):
	@echo ----------------------------------------------------------
	@echo Creating build folder
	-mkdir -p $(BUILD_PATHS)

print_info:
	@echo ----------------------------------------------------------
	@echo Compiling using
	@echo BASE   PATH = $(MODULE_PATH)
	@echo GCC    PATH = $(ARM_GCC_PATH)
	@echo SOURCE LIST = $(SOURCES)
	@echo BOSSAC PATH = $(BOSSAC)
	@echo BOSSAC VERSION = $(BOSSA_VERSION)

usb_reset:
	@echo ----------------------------------------------------------
	@echo Performing force reset over serial on $(SERIAL_PORT)
	$(PYTHON) reset.py $(SERIAL_PORT)

usb_flash:
	@echo ----------------------------------------------------------
	@echo Flashing over USB
	$(BOSSAC) $(BOSSAC_OFFSET) $(BOSSAC_ARGS) $(BIN)

init:
	@echo ----------------------------------------------------------
	@echo Initialising submodules
	git submodule update --init

boot:
	@echo ----------------------------------------------------------
	@echo Building bootloader
	CHIPNAME=$(CHIPNAME) MODULE_PATH=$(MODULE_PATH) $(MAKE) -C bootloader all size clean

flash_all:
	@echo ----------------------------------------------------------
	@echo Flashing bootloader and program using $(PROGRAMMER)
ifeq ($(SERNUM),)  # Don't insert a blank serial number
	$(PYTHON) thirdparty/adalink/adalink.py $(ADALINK_ARGS) \
		-b $(BOOT_BIN) 0x0000 -b $(BIN) 0x2000
else
	$(PYTHON) bootloader/insert_serial.py $(SERNUM) $(BOOT_BIN) $(BOOT_SERNUM_BIN)
	$(PYTHON) thirdparty/adalink/adalink.py $(ADALINK_ARGS) \
		-b $(BOOT_SERNUM_BIN) 0x0000 -b $(BIN) 0x2000
	-$(RM) $(BOOT_SERNUM_BIN)
endif

SCRIPTS:
%.py: SCRIPTS
	$(PYTHON) $@ $(SERIAL_PORT)

clean:
	@echo ----------------------------------------------------------
	@echo Cleaning project
	-$(RM) $(BIN)
	-$(RM) $(HEX)
	-$(RM) -r $(BUILD_PATH)

.phony: print_info usb_reset usb_flash init boot flash_all $(BUILD_PATH)
