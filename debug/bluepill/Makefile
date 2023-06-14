.DEFAULT_GOAL := all

PROJECT_ROOT := $(CURDIR)

APP_NAME := debug

C_SRCS := $(PROJECT_ROOT)/src/system.c \
					$(PROJECT_ROOT)/src/main.c \
					$(PROJECT_ROOT)/src/bc80_hw.c \
					$(PROJECT_ROOT)/src/stm32_hw.c \
					$(PROJECT_ROOT)/src/usb/usb.c \
					$(PROJECT_ROOT)/src/usb/usb_lib.c \
					$(PROJECT_ROOT)/src/usb/usbhw.c \
					$(PROJECT_ROOT)/src/usb/ringbuffer.c

ASM_SRCS :=	$(PROJECT_ROOT)/src/startup_stm32f103x6.s

INCLUDES := $(PROJECT_ROOT)/include $(PROJECT_ROOT)/src $(PROJECT_ROOT)/src/usb $(PROJECT_ROOT)/..

#########################################################

TOOLS_PATH := ../tools

CC := $(TOOLS_PATH)/bin/arm-none-eabi-gcc
AS := $(TOOLS_PATH)/bin/arm-none-eabi-gcc
LD := $(TOOLS_PATH)/bin/arm-none-eabi-gcc
CPP := $(TOOLS_PATH)/bin/arm-none-eabi-cpp
OBJCOPY := $(TOOLS_PATH)/bin/arm-none-eabi-objcopy
GDB := $(TOOLS_PATH)/bin/arm-none-eabi-gdb
SIZE := $(TOOLS_PATH)/bin/arm-none-eabi-size

RM := rm -f
CP := cp

EXE_FLASH := $(PROJECT_ROOT)/$(APP_NAME)_flash.elf
BIN_FLASH := $(PROJECT_ROOT)/$(APP_NAME)_flash.bin
MAP_FLASH := $(PROJECT_ROOT)/$(APP_NAME)_flash.map

ASFLAGS += -mthumb -mcpu=cortex-m3 -g3 -c -x assembler-with-cpp --specs=nano.specs -mfloat-abi=soft

CFLAGS += -std=gnu11 \
          -O2 \
          -ggdb \
          -fno-omit-frame-pointer \
          -DDEBUG -DSTM32F103x6 \
          -Wall \
          -ffunction-sections -fdata-sections -fstack-usage \
          --specs=nano.specs \
          -mthumb \
          -mcpu=cortex-m3 \
          -mfloat-abi=soft

LD_SCRIPT_FLASH := $(PROJECT_ROOT)/src/STM32F103X6_FLASH.ld

LDFLAGS_FLASH := -mcpu=cortex-m3 -T "$(LD_SCRIPT_FLASH)" --specs=nosys.specs -Wl,--gc-sections -static --specs=nano.specs -Wl,--start-group -lc -lm -Wl,--end-group

OBJDIR := .obj
OBJECTS := $(addprefix $(OBJDIR)/, $(addsuffix .o,$(C_SRCS)))
OBJECTS += $(addprefix $(OBJDIR)/, $(addsuffix .o,$(ASM_SRCS)))
DEPS := $(addprefix $(OBJDIR)/, $(addsuffix .d,$(C_SRCS)))

$(EXE_FLASH): $(OBJECTS)
			$(LD) $(OBJECTS) -o $@ $(LDFLAGS_FLASH)

$(BIN_FLASH): $(EXE_FLASH)
			$(OBJCOPY) -O binary -S -g $< $@
			$(SIZE) $<

$(OBJDIR)/%.c.o: %.c
			@mkdir -p $(dir $@)
			$(CC) $(CFLAGS) $(addprefix -I, $(INCLUDES)) -c $< -o $@

$(OBJDIR)/%.s.o: %.s
			@mkdir -p $(dir $@)
			$(AS) $(ASFLAGS) $(addprefix -I, $(INCLUDES)) -c $< -o $@

all: $(BIN_FLASH)

clean:
			$(RM) $(EXE_FLASH) $(BIN_FLASH)
			$(RM) -r $(OBJDIR)

flash:
			st-flash write $(BIN_FLASH) 0x8000000
# 			../tools/bin/STM32_Programmer_CLI -c port=swd reset=HWrst -w $(BIN_FLASH) 0x8000000 -g 0x8000000

.PHONY: all clean $(EXE_FLASH) $(BIN_FLASH) flash

-include $(DEPS)