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
endif

MODULE_PATH?=$(abspath $(CURDIR)/thirdparty)
USB_PATH?=$(shell realpath --relative-to $(CURDIR) $(MODULE_PATH)/usb)
CORE_PATH?=core
BUILD_PATH?=$(abspath $(CURDIR)/build)

NAME?=main.cpp
_NAME?=$(basename $(NAME))

# -----------------------------------------------------------------------------
# Tools
CC=$(ARM_GCC_PATH)gcc
CXX=$(ARM_GCC_PATH)g++
OBJCOPY=$(ARM_GCC_PATH)objcopy
NM=$(ARM_GCC_PATH)nm
SIZE=$(ARM_GCC_PATH)size

# -----------------------------------------------------------------------------
# Compiler options
CFLAGS_EXTRA=-DF_CPU=48000000L -D__SAMD21G18A__
CFLAGS_EXTRA+=-DUSB_VID=0x2341 -DUSB_PID=0x804d -DUSBCON -DUSB_MANUFACTURER='"Arduino LLC"' -DUSB_PRODUCT='"Arduino Zero"'
CXXFLAGS=-mcpu=cortex-m0plus -mthumb -Wall -c -g -Os -std=gnu++11 -ffunction-sections -fdata-sections
CXXFLAGS+=-fno-threadsafe-statics -nostdlib --param max-inline-insns-single=500 -fno-rtti -fno-exceptions -MMD
CFLAGS=-mcpu=cortex-m0plus -mthumb -Wall -c -g -Os -std=gnu11 -ffunction-sections -fdata-sections -nostdlib --param max-inline-insns-single=500 -MMD

ELF=$(_NAME).elf
BIN=$(_NAME).bin
HEX=$(_NAME).hex

INCLUDES=-I"$(MODULE_PATH)/CMSIS-4.5.0/CMSIS/Include/" -I"$(MODULE_PATH)/CMSIS-Atmel/CMSIS/Device/ATMEL/" -I"$(USB_PATH)" -I"$(CORE_PATH)"

# -----------------------------------------------------------------------------
# Linker options
LDFLAGS=-mthumb -mcpu=cortex-m0plus -Wall -Wl,--cref -Wl,--check-sections -Wl,--gc-sections -Wl,--unresolved-symbols=report-all
LDFLAGS+=-Wl,--warn-common -Wl,--warn-section-align -Wl,--warn-unresolved-symbols --specs=nano.specs --specs=nosys.specs
LDFLAGS+=-L$(MODULE_PATH)/CMSIS-4.5.0/CMSIS/Lib/GCC/ -larm_cortexM0l_math

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

LD_SCRIPT=core/linker_scripts/flash_with_bootloader.ld

all: print_info $(SOURCES) $(BIN) $(HEX)

$(ELF): Makefile $(BUILD_PATH) $(OBJECTS)
	@echo ----------------------------------------------------------
	@echo Creating ELF binary
	"$(CXX)" -L. -L$(BUILD_PATH) $(LDFLAGS) -Os -Wl,--gc-sections -save-temps -T$(LD_SCRIPT) -Wl,-Map,"$(BUILD_PATH)/$(_NAME).map" -o "$(BUILD_PATH)/$(ELF)" -Wl,--start-group $(OBJECTS) -lm -Wl,--end-group
	"$(NM)" "$(BUILD_PATH)/$(ELF)" >"$(BUILD_PATH)/$(_NAME)_symbols.txt"
	"$(SIZE)" --format=sysv -t -x $(BUILD_PATH)/$(ELF)

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
	-mkdir -p $(BUILD_PATH) $(BUILD_PATH)/$(CORE_PATH)
	-mkdir -p $(BUILD_PATH)/$(USB_PATH)/samd

print_info:
	@echo ----------------------------------------------------------
	@echo Compiling bootloader using
	@echo BASE   PATH = $(MODULE_PATH)
	@echo GCC    PATH = $(ARM_GCC_PATH)
	@echo SOURCE LIST = $(SOURCES)

clean:
	@echo ----------------------------------------------------------
	@echo Cleaning project
	-$(RM) $(BIN)
	-$(RM) $(HEX)
	-$(RM) -r $(BUILD_PATH)

.phony: print_info $(BUILD_PATH)
